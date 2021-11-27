#ifndef NETWORK_UTIL_H_
#define NETWORK_UTIL_H_

#include "typedefs.h"
#include "IpAddress.h"

namespace Util
{
	struct AdapterInfo
	{
		AdapterInfo(const tstring& name, const string& ip, int prefix, int index) : adapterName(name), ip(ip), prefix(prefix), index(index) { }
		tstring adapterName;
		string ip;
		int prefix;
		int index;
	};

	void getNetworkAdapters(int af, std::vector<AdapterInfo>& adapterInfos) noexcept;
	string getDefaultGateway(int af, const std::vector<AdapterInfo>* cachedAdapterInfos = nullptr);
	string getLocalIp(int af);
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
	bool isSameNetwork(const string& addr1, const string& addr2, unsigned prefix, int af);
}

#endif // NETWORK_UTIL_H_
