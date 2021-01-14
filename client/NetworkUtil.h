#ifndef NETWORK_UTIL_H_
#define NETWORK_UTIL_H_

#include "typedefs.h"

namespace Util
{
	struct AdapterInfo
	{
		AdapterInfo(const tstring& name, const string& ip, int prefix) : adapterName(name), ip(ip), prefix(prefix) { }
		tstring adapterName;
		string ip;
		int prefix;
	};

	void getNetworkAdapters(bool v6, vector<AdapterInfo>& adapterInfos) noexcept;
	string getDefaultGateway();
	string getLocalIp();
	bool isPrivateIp(const string& ip);
	inline bool isPrivateIp(uint32_t ip)
	{
		return ((ip & 0xff000000) == 0x0a000000 || // 10.0.0.0/8
		        (ip & 0xff000000) == 0x7f000000 || // 127.0.0.0/8
		        (ip & 0xffff0000) == 0xa9fe0000 || // 169.254.0.0/16
		        (ip & 0xfff00000) == 0xac100000 || // 172.16.0.0/12
		        (ip & 0xffff0000) == 0xc0a80000);  // 192.168.0.0/16
	}		
	bool isSameNetwork(const string& addr1, const string& addr2, unsigned prefix, bool v6);
}

#endif // NETWORK_UTIL_H_
