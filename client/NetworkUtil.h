#ifndef NETWORK_UTIL_H_
#define NETWORK_UTIL_H_

#include "typedefs.h"
#include "IpAddress.h"

namespace Util
{
	struct AdapterInfo
	{
		AdapterInfo(const string& name, const tstring& description, const IpAddressEx& ip, int prefix, int index) : name(name), description(description), ip(ip), prefix(prefix), index(index) { }
		string name;
		tstring description;
		IpAddressEx ip;
		int prefix;
		int index;
	};

	void getNetworkAdapters(int af, std::vector<AdapterInfo>& adapterInfos) noexcept;
	bool getDeviceAddress(int af, const string& name, IpAddressEx& ip) noexcept;
	IpAddressEx getDefaultGateway(int af, const std::vector<AdapterInfo>* cachedAdapterInfos = nullptr);
	IpAddressEx getLocalIp(int af);
	inline bool isPrivateIp(Ip4Address ip)
	{
		return ((ip & 0xff000000) == 0x0a000000 || // 10.0.0.0/8
		        (ip & 0xffff0000) == 0xa9fe0000 || // 169.254.0.0/16
		        (ip & 0xfff00000) == 0xac100000 || // 172.16.0.0/12
		        (ip & 0xffff0000) == 0xc0a80000);  // 192.168.0.0/16
	}
	inline bool isPublicIp(Ip4Address ip)
	{
		return !(isPrivateIp(ip) ||
		        (ip & 0xff000000) == 0x7f000000); // 127.0.0.0/8
	}
	bool isPrivateIp(const Ip6Address& ip);
	bool isPublicIp(const Ip6Address& ip);
	bool isLinkScopedIp(const Ip6Address& ip);
	bool isPrivateIp(const IpAddress& ip);
	bool isPublicIp(const IpAddress& ip);
	bool isReservedIp(const Ip6Address& ip);
	bool isSameNetwork(const IpAddressEx& addr1, const IpAddressEx& addr2, unsigned prefix);
}

#endif // NETWORK_UTIL_H_
