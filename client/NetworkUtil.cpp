#include "stdinc.h"
#include "NetworkUtil.h"
#include "Util.h"
#include "Ip4Address.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <ifaddrs.h>
extern "C"
{
 #include <natpmp/getgateway.h>
}
#endif

#ifdef FLYLINKDC_SUPPORT_WIN_XP
static unsigned getPrefixLen(const IP_ADAPTER_ADDRESSES* adapter, const string& address, bool v6)
{
	char buf[512];
	unsigned prefixLen = 0;
	int family = v6 ? AF_INET6 : AF_INET;
	for (const IP_ADAPTER_PREFIX* prefix = adapter->FirstPrefix; prefix; prefix = prefix->Next)
	{
		if (prefix->Address.lpSockaddr->sa_family == family &&
			prefix->PrefixLength > prefixLen &&
			!getnameinfo(prefix->Address.lpSockaddr, prefix->Address.iSockaddrLength, buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST))
		{
			string prefixAddr(buf);
			if (prefixAddr != address && Util::isSameNetwork(prefixAddr, address, prefix->PrefixLength, v6))
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

void Util::getNetworkAdapters(bool v6, vector<AdapterInfo>& adapterInfos) noexcept
{
	adapterInfos.clear();
#ifdef _WIN32
	ULONG len = 15360;
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	const ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_PREFIX;
#else
	const ULONG flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
#endif
	for (int i = 0; i < 3; ++i)
	{
		uint8_t* infoBuf = new uint8_t[len];
		PIP_ADAPTER_ADDRESSES adapterInfo = (PIP_ADAPTER_ADDRESSES) infoBuf;
		ULONG ret = GetAdaptersAddresses(v6 ? AF_INET6 : AF_INET, flags, nullptr, adapterInfo, &len);

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
#ifdef FLYLINKDC_SUPPORT_WIN_XP
							unsigned prefixLen = getPrefixLen(pAdapterInfo, address, v6);
#else
							unsigned prefixLen = ua->OnLinkPrefixLength;
#endif
							adapterInfos.emplace_back(pAdapterInfo->FriendlyName, address, prefixLen);
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
		int type = v6 ? AF_INET6 : AF_INET;
		char buf[256];
		for (ifaddrs* p = addr; p; p = p->ifa_next)
			if (p->ifa_name && p->ifa_addr && p->ifa_netmask && p->ifa_addr->sa_family == type)
			{
				int prefix;
				const char *res;
				if (v6)
				{
					res = inet_ntop(AF_INET6, &((const sockaddr_in6*) p->ifa_addr)->sin6_addr, buf, sizeof(buf));
					prefix = getPrefix(&((const sockaddr_in6*) p->ifa_netmask)->sin6_addr, 16);
				}
				else
				{
					res = inet_ntop(AF_INET, &((const sockaddr_in*) p->ifa_addr)->sin_addr, buf, sizeof(buf));
					prefix = getPrefix(&((const sockaddr_in*) p->ifa_netmask)->sin_addr, 4);
				}
				if (res)
					adapterInfos.emplace_back(p->ifa_name, buf, prefix);
			}
		freeifaddrs(addr);
	}
#endif
}

string Util::getDefaultGateway()
{
#ifdef _WIN32
	in_addr addr = {};
	MIB_IPFORWARDROW ip_forward = {0};
	memset(&ip_forward, 0, sizeof(ip_forward));
	if (GetBestRoute(inet_addr("0.0.0.0"), 0, &ip_forward) == NO_ERROR)
	{
		addr = *(in_addr*) &ip_forward.dwForwardNextHop;
		return inet_ntoa(addr);
	}
	return string();
#else
	in_addr addr = {};
	if (getdefaultgateway(&addr.s_addr) == 0)
		return inet_ntoa(addr);
	return string();
#endif
}

string Util::getLocalIp()
{
	vector<AdapterInfo> adapters;
	getNetworkAdapters(false, adapters);
	if (adapters.empty()) return "0.0.0.0";
	string defRoute = getDefaultGateway();
	if (!defRoute.empty())
	{
		for (const AdapterInfo& ai : adapters)
			if (isSameNetwork(ai.ip, defRoute, ai.prefix, false))
				return ai.ip;
	}	
	// fallback
	return adapters[0].ip;
}

bool Util::isPrivateIp(const string& ip)
{
	dcassert(!ip.empty());
	struct in_addr addr = {0};
	addr.s_addr = inet_addr(ip.c_str());
	if (addr.s_addr != INADDR_NONE)
	{
		const uint32_t haddr = ntohl(addr.s_addr);
		return isPrivateIp(haddr);
	}
	return false;
}

#ifdef FLYLINKDC_SUPPORT_WIN_XP
static int inet_pton_compat(int af, const char* src, void* dst)
{
	if (af != AF_INET && af != AF_INET6) return -1;

	sockaddr_storage ss;
	int size = sizeof(ss);
	int len = strlen(src);
	char* src_copy = (char*) _alloca(len + 1);
	memcpy(src_copy, src, len + 1);
	memset(&ss, 0, sizeof(ss));
	if (WSAStringToAddressA(src_copy, af, nullptr, (struct sockaddr*) &ss, &size))
		return 0;
	if (af == AF_INET)
		*(in_addr*) dst = ((sockaddr_in*) &ss)->sin_addr;
	else
		*(in6_addr*) dst = ((sockaddr_in6*) &ss)->sin6_addr;
	return 1;
}
#else
#define inet_pton_compat inet_pton
#endif

bool Util::isSameNetwork(const string& addr1, const string& addr2, unsigned prefix, bool v6)
{
	if (!v6)
	{
		uint32_t ip1, ip2;
		if (!Util::parseIpAddress(ip1, addr1) || !Util::parseIpAddress(ip2, addr2)) return false;
		uint32_t mask = ~0u << (32 - prefix);
		return (ip1 & mask) == (ip2 & mask);
	}
	else
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
}
