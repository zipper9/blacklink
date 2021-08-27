#ifndef IP_ADDRESS_H_
#define IP_ADDRESS_H_

#include "Ip4Address.h"
#include "Ip6Address.h"
#include "SockDefs.h"

struct IpAddress
{
	int type; // 0 (empty), AF_INET, AF_INET6
	union
	{
		Ip4Address v4;
		Ip6Address v6;
	} data;
};

bool operator==(const IpAddress& a, const IpAddress& b);

int compare(const IpAddress& a, const IpAddress& b);

#ifdef _UNICODE
#define printIpAddressT printIpAddressW
#else
#define printIpAddressT printIpAddress
#endif

namespace Util
{
	bool parseIpAddress(IpAddress& result, const string& s) noexcept;

	bool isValidIp(const IpAddress& addr) noexcept;

	string printIpAddress(const IpAddress& addr, bool brackets = false) noexcept;
	wstring printIpAddressW(const IpAddress& addr, bool brackets = false) noexcept;
}

#endif // IP_ADDRESS_H_
