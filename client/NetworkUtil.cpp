#include "stdinc.h"
#include "NetworkUtil.h"
#include "inet_compat.h"
#include "SocketAddr.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "CompatibilityManager.h"
#else
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <ifaddrs.h>
#include <net/if.h>
extern "C"
{
 #include <natpmp/getgateway.h>
}
#endif

#ifdef OSVER_WIN_XP
static unsigned getPrefixLen(const IP_ADAPTER_ADDRESSES* adapter, const IpAddressEx& address, int family)
{
	unsigned prefixLen = 0;
	for (const IP_ADAPTER_PREFIX* prefix = adapter->FirstPrefix; prefix; prefix = prefix->Next)
	{
		if (prefix->Address.lpSockaddr->sa_family == family && prefix->PrefixLength > prefixLen)
		{
			IpAddressEx prefixAddr;
			uint16_t port;
			fromSockAddr(prefixAddr, port, *(const sockaddr_u*)(prefix->Address.lpSockaddr));
			if (prefixAddr != address && Util::isSameNetwork(prefixAddr, address, prefix->PrefixLength))
				prefixLen = prefix->PrefixLength;
		}
	}
	return prefixLen;
}
#endif

#ifndef _WIN32
static int getPrefix(const void* data, int size)
{
	const uint8_t* bytes = static_cast<const uint8_t*>(data);
	int i = 0;
	while (i < size && bytes[i] == 0xFF) i++;
	int prefix = 0;
	if (i < size)
	{
		uint8_t b = bytes[i];
		while (prefix < 8 && !(b & 1<<prefix)) prefix++;
		prefix = 8 - prefix;
	}
	return prefix + i*8;
}
#endif

void Util::getNetworkAdapters(int af, vector<AdapterInfo>& adapterInfos) noexcept
{
	adapterInfos.clear();
#ifdef _WIN32
	ULONG len = 15360;
	ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
#ifdef OSVER_WIN_XP
	if (!CompatibilityManager::isOsVistaPlus())
		flags |= GAA_FLAG_INCLUDE_PREFIX;
#endif
	for (int i = 0; i < 3; ++i)
	{
		uint8_t* infoBuf = new uint8_t[len];
		PIP_ADAPTER_ADDRESSES adapterInfo = (PIP_ADAPTER_ADDRESSES) infoBuf;
		ULONG ret = GetAdaptersAddresses(af, flags, nullptr, adapterInfo, &len);

		if (ret == ERROR_SUCCESS)
		{
			for (PIP_ADAPTER_ADDRESSES pAdapterInfo = adapterInfo; pAdapterInfo; pAdapterInfo = pAdapterInfo->Next)
			{
				// we want only enabled ethernet interfaces
				if (pAdapterInfo->OperStatus == IfOperStatusUp && pAdapterInfo->IfType != IF_TYPE_SOFTWARE_LOOPBACK)
				{
					for (PIP_ADAPTER_UNICAST_ADDRESS ua = pAdapterInfo->FirstUnicastAddress; ua; ua = ua->Next)
					{
						const IP_ADAPTER_UNICAST_ADDRESS_LH* ualh = (const IP_ADAPTER_UNICAST_ADDRESS_LH*) ua;
						IpAddressEx address;
						uint16_t port;
						fromSockAddr(address, port, *(const sockaddr_u*)(ua->Address.lpSockaddr));
						if (address.type)
						{
							unsigned prefixLen;
#ifdef OSVER_WIN_XP
							if (CompatibilityManager::isOsVistaPlus())
								prefixLen = ualh->OnLinkPrefixLength;
							else
								prefixLen = getPrefixLen(pAdapterInfo, address, af);
#else
							prefixLen = ualh->OnLinkPrefixLength;
#endif
							int index = af == AF_INET6 ? pAdapterInfo->Ipv6IfIndex : pAdapterInfo->IfIndex;
							adapterInfos.emplace_back(pAdapterInfo->AdapterName, pAdapterInfo->FriendlyName, address, prefixLen, index);
						}
					}
				}
			}
			delete[] infoBuf;
			return;
		}

		delete[] infoBuf;
		if (ret != ERROR_BUFFER_OVERFLOW)
			break;
	}
#else
	ifaddrs* addr;
	if (!getifaddrs(&addr))
	{
		for (ifaddrs* p = addr; p; p = p->ifa_next)
			if (p->ifa_name && p->ifa_addr && p->ifa_netmask && p->ifa_addr->sa_family == af)
			{
				IpAddressEx ip;
				ip.type = 0;
				uint16_t port;
				int prefix;
				if (af == AF_INET6)
				{
					fromSockAddr(ip, port, *(const sockaddr_u*) p->ifa_addr);
					prefix = getPrefix(&((const sockaddr_in6*) p->ifa_netmask)->sin6_addr, 16);
				}
				else if (af == AF_INET)
				{
					fromSockAddr(ip, port, *(const sockaddr_u*) p->ifa_addr);
					prefix = getPrefix(&((const sockaddr_in*) p->ifa_netmask)->sin_addr, 4);
				}
				if (ip.type)
				{
					string name(p->ifa_name);
					adapterInfos.emplace_back(name, name, ip, prefix, if_nametoindex(p->ifa_name));
				}
			}
		freeifaddrs(addr);
	}
#endif
}

#ifdef _WIN32
bool getDefRouteInterfaceV6(DWORD* index)
{
	static const uint16_t w[3] = { 0, 0x2000, 0x3000 };
	sockaddr_in6 addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	for (int i = 0; i < 3; ++i)
	{
		*((uint16_t*) &addr.sin6_addr) = ntohs(w[i]);
		int result = GetBestInterfaceEx((sockaddr*) &addr, index);
		if (result == NO_ERROR) return true;
	}
	return false;
}
#endif

IpAddressEx Util::getDefaultGateway(int af, const vector<AdapterInfo>* cachedAdapterInfos)
{
	IpAddressEx ip;
	memset(&ip, 0, sizeof(ip));
#ifdef _WIN32
	if (af == AF_INET)
	{
		MIB_IPFORWARDROW ip_forward;
		memset(&ip_forward, 0, sizeof(ip_forward));
		if (GetBestRoute(0, 0, &ip_forward) == NO_ERROR)
		{
			ip.data.v4 = ntohl(((in_addr*) &ip_forward.dwForwardNextHop)->s_addr);
			ip.type = AF_INET;
		}
		return ip;
	}
	if (af == AF_INET6)
	{
		DWORD index;
		if (getDefRouteInterfaceV6(&index))
		{
			const vector<AdapterInfo>* adapterInfos = cachedAdapterInfos;
			vector<AdapterInfo>* tempAdapaterInfos = nullptr;
			if (!adapterInfos)
			{
				tempAdapaterInfos = new vector<AdapterInfo>;
				getNetworkAdapters(af, *tempAdapaterInfos);
				adapterInfos = tempAdapaterInfos;
			}
			for (const AdapterInfo& ai : *adapterInfos)
				if (ai.index == (int) index)
				{
					ip = ai.ip;
					break;
				}
			delete tempAdapaterInfos;
		}
		return ip;
	}
	return ip;
#else
	in_addr_t addr = 0;
	if (af == AF_INET && getdefaultgateway(&addr) == 0)
	{
		ip.type = AF_INET;
		ip.data.v4 = ntohl(addr);
	}
	return ip;
#endif
}

IpAddressEx Util::getLocalIp(int af)
{
	vector<AdapterInfo> adapters;
	getNetworkAdapters(af, adapters);
	if (adapters.empty())
	{
		IpAddressEx ip;
		memset(&ip, 0, sizeof(ip));
		return ip;
	}
	IpAddressEx defRoute = getDefaultGateway(af, &adapters);
	if (defRoute.type)
	{
		for (const AdapterInfo& ai : adapters)
			if (isSameNetwork(ai.ip, defRoute, ai.prefix))
				return ai.ip;
	}
	// fallback
	return adapters[0].ip;
}

static inline bool checkPrefix(uint16_t w, uint16_t prefix, int len)
{
	return (w & (0xFFFF << (16-len))) == prefix;
}

bool Util::isPrivateIp(const Ip6Address& ip)
{
	uint16_t w = ntohs(ip.data[0]);
	return checkPrefix(w, 0xFC00, 7) || // Unique Local Unicast
	       checkPrefix(w, 0xFE80, 10); // Link-Scoped Unicast
}

bool Util::isPublicIp(const Ip6Address& ip)
{
	uint16_t w = ntohs(ip.data[0]);
	return checkPrefix(w, 0x2000, 3); // Global Unicast
}

bool Util::isLinkScopedIp(const Ip6Address& ip)
{
	uint16_t w = ntohs(ip.data[0]);
	return checkPrefix(w, 0xFE80, 10);
}

bool Util::isReservedIp(const Ip6Address& ip)
{
	uint16_t w = ntohs(ip.data[0]);
	return checkPrefix(w, 0, 8);
}

bool Util::isPrivateIp(const IpAddress& ip)
{
	if (ip.type == AF_INET) return isPrivateIp(ip.data.v4);
	if (ip.type == AF_INET6) return isPrivateIp(ip.data.v6);
	return false;
}

bool Util::isPublicIp(const IpAddress& ip)
{
	if (ip.type == AF_INET) return isPublicIp(ip.data.v4);
	if (ip.type == AF_INET6) return isPublicIp(ip.data.v6);
	return false;
}

bool Util::isSameNetwork(const IpAddressEx& addr1, const IpAddressEx& addr2, unsigned prefix)
{
	if (addr1.type != addr2.type) return false;
	if (addr1.type == AF_INET)
	{
		uint32_t mask = ~0u << (32 - prefix);
		return (addr1.data.v4 & mask) == (addr2.data.v4 & mask);
	}
	if (addr1.type == AF_INET6)
	{
		const Ip6AddressEx& ip1 = addr1.data.v6;
		const Ip6AddressEx& ip2 = addr2.data.v6;
		if (ip1.scopeId && ip2.scopeId && ip1.scopeId != ip2.scopeId) return false;
		unsigned words = prefix / 16;
		if (words && memcmp(ip1.data, ip2.data, words * 2)) return false;
		prefix &= 15;
		if (!prefix) return true;
		uint16_t mask = 0xFFFF << (16 - prefix);
		uint16_t w1 = ntohs(ip1.data[words]);
		uint16_t w2 = ntohs(ip2.data[words]);
		return (w1 & mask) == (w2 & mask);
	}
	return false;
}
