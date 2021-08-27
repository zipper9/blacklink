#ifndef IP6_ADDRESS_H_
#define IP6_ADDRESS_H_

#include "typedefs.h"
#include <string.h>

struct Ip6Address
{
	uint16_t data[8];
};

static inline bool operator==(const Ip6Address& a, const Ip6Address& b)
{
	return memcmp(&a, &b, sizeof(Ip6Address)) == 0;
}

namespace Util
{
	bool parseIpAddress(Ip6Address& result, const string& s, string::size_type start, string::size_type end) noexcept;
	inline bool parseIpAddress(Ip6Address& result, const string& s)
	{
		return parseIpAddress(result, s, 0, s.length());
	}
	bool parseIpAddress(Ip6Address& result, const wstring& s, wstring::size_type start, wstring::size_type end) noexcept;
	inline bool parseIpAddress(Ip6Address& result, const wstring& s)
	{
		return parseIpAddress(result, s, 0, s.length());
	}

	bool isValidIp6(const Ip6Address& ip) noexcept;
	inline bool isValidIp6(const string& ip)
	{
		Ip6Address addr;
		return parseIpAddress(addr, ip) && isValidIp6(addr);
	}
	inline bool isValidIp6(const wstring& ip)
	{
		Ip6Address addr;
		return parseIpAddress(addr, ip) && isValidIp6(addr);
	}

	bool isEmpty(const Ip6Address& ip) noexcept;

	string printIpAddress(const Ip6Address& ip, bool brackets = false) noexcept;
	wstring printIpAddressW(const Ip6Address& ip, bool brackets = false) noexcept;
}

#endif // IP6_ADDRESS_H_
