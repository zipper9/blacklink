#include "stdinc.h"
#include "NetworkUtil.h"
#include "inet_compat.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
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
static unsigned getPrefixLen(const IP_ADAPTER_ADDRESSES* adapter, const string& address, int family)
{
	char buf[512];
	unsigned prefixLen = 0;
	for (const IP_ADAPTER_PREFIX* prefix = adapter->FirstPrefix; prefix; prefix = prefix->Next)
	{
		if (prefix->Address.lpSockaddr->sa_family == family &&
			prefix->PrefixLength > prefixLen &&
			!getnameinfo(prefix->Address.lpSockaddr, prefix->Address.iSockaddrLength, buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST))
		{
			string prefixAddr(buf);
			if (prefixAddr != address && Util::isSameNetwork(prefixAddr, address, prefix->PrefixLength, family))
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
#ifdef OSVER_WIN_XP
	const ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_PREFIX;
#else
	const ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
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
						// convert IP address to string
						char buf[512];
						memset(buf, 0, sizeof(buf));
						if (!getnameinfo(ua->Address.lpSockaddr, ua->Address.iSockaddrLength, buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST))
						{
							string address(buf);
#ifdef OSVER_WIN_XP
							unsigned prefixLen = getPrefixLen(pAdapterInfo, address, af);
#else
							unsigned prefixLen = ua->OnLinkPrefixLength;
#endif
							int index = af == AF_INET6 ? pAdapterInfo->Ipv6IfIndex : pAdapterInfo->IfIndex;
							adapterInfos.emplace_back(pAdapterInfo->FriendlyName, address, prefixLen, index);
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
		char buf[256];
		for (ifaddrs* p = addr; p; p = p->ifa_next)
			if (p->ifa_name && p->ifa_addr && p->ifa_netmask && p->ifa_addr->sa_family == af)
			{
				int prefix;
				const char *res = nullptr;
				if (af == AF_INET6)
				{
					res = inet_ntop(AF_INET6, &((const sockaddr_in6*) p->ifa_addr)->sin6_addr, buf, sizeof(buf));
					prefix = getPrefix(&((const sockaddr_in6*) p->ifa_netmask)->sin6_addr, 16);
				}
				else if (af == AF_INET)
				{
					res = inet_ntop(AF_INET, &((const sockaddr_in*) p->ifa_addr)->sin_addr, buf, sizeof(buf));
					prefix = getPrefix(&((const sockaddr_in*) p->ifa_netmask)->sin_addr, 4);
				}
				if (res)
					adapterInfos.emplace_back(p->ifa_name, buf, prefix, if_nametoindex(p->ifa_name));
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

string Util::getDefaultGateway(int af, const vector<AdapterInfo>* cachedAdapterInfos)
{
#ifdef _WIN32
	if (af == AF_INET)
	{
		MIB_IPFORWARDROW ip_forward;
		memset(&ip_forward, 0, sizeof(ip_forward));
		if (GetBestRoute(0, 0, &ip_forward) == NO_ERROR)
		{
			in_addr addr = *(in_addr*) &ip_forward.dwForwardNextHop;
			return inet_ntoa(addr);
		}
		return string();
	}
	if (af == AF_INET6)
	{
		string result;
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
					result = ai.ip;
					break;
				}
			delete tempAdapaterInfos;
		}
		return result;
	}
	return string();
#else
	in_addr addr = {};
	if (getdefaultgateway(&addr.s_addr) == 0)
		return inet_ntoa(addr);
	return string();
#endif
}

string Util::getLocalIp(int af)
{
	vector<AdapterInfo> adapters;
	getNetworkAdapters(af, adapters);
	if (adapters.empty())
		return af == AF_INET6 ? "::" : "0.0.0.0";
	string defRoute = getDefaultGateway(af, &adapters);
	if (!defRoute.empty())
	{
		for (const AdapterInfo& ai : adapters)
			if (isSameNetwork(ai.ip, defRoute, ai.prefix, af))
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

bool Util::isSameNetwork(const string& addr1, const string& addr2, unsigned prefix, int af)
{
	if (af == AF_INET)
	{
		uint32_t ip1, ip2;
		if (!Util::parseIpAddress(ip1, addr1) || !Util::parseIpAddress(ip2, addr2)) return false;
		uint32_t mask = ~0u << (32 - prefix);
		return (ip1 & mask) == (ip2 & mask);
	}
	if (af == AF_INET6)
	{
		in6_addr ip1, ip2;

		auto p = addr1.find('%');
		inet_pton_compat(AF_INET6, (p != string::npos ? addr1.substr(0, p) : addr1).c_str(), &ip1);

		p = addr2.find('%');
		inet_pton_compat(AF_INET6, (p != string::npos ? addr2.substr(0, p) : addr2).c_str(), &ip2);

		unsigned bytes = prefix / 8;
		if (bytes && memcmp(ip1.s6_addr, ip2.s6_addr, bytes)) return false;
		prefix &= 7;
		if (prefix)
		{
			uint8_t mask = 0xFF << (8 - prefix);
			if ((ip1.s6_addr[bytes] & mask) != (ip2.s6_addr[bytes] & mask)) return false;
		}
		return true;
	}
	return false;
}
