#ifndef IP6_ADDRESS_H_
#define IP6_ADDRESS_H_

#include "typedefs.h"
#include <string.h>

struct Ip6Address
{
	uint16_t data[8];
};

struct Ip6AddressEx : public Ip6Address
{
	uint32_t scopeId;
};

static inline bool operator==(const Ip6Address& a, const Ip6Address& b)
{
	return memcmp(&a, &b, sizeof(Ip6Address)) == 0;
}

static inline bool operator==(const Ip6AddressEx& a, const Ip6AddressEx& b)
{
	return memcmp(&a, &b, sizeof(Ip6Address)) == 0 && a.scopeId == b.scopeId;
}

namespace Util
{
	bool parseIpAddress(Ip6Address& result, const string& s, string::size_type start, string::size_type end) noexcept;
	bool parseIpAddress(Ip6AddressEx& result, const string& s, string::size_type start, string::size_type end) noexcept;

	template<typename T>
	bool parseIpAddress(T& result, const wstring& s, wstring::size_type start, wstring::size_type end) noexcept
	{
		size_t len = end - start;
		string tmp;
		tmp.resize(len);
		for (size_t i = 0; i < len; ++i)
			tmp[i] = (char) s[start + i];
		return parseIpAddress(result, tmp, 0, len);
	}

	template<typename T, typename S>
	bool parseIpAddress(T& result, const S& s) noexcept
	{
		return parseIpAddress(result, s, 0, s.length());
	}

	bool isValidIp6(const Ip6Address& ip) noexcept;
	inline bool isValidIp6(const string& ip) noexcept
	{
		Ip6Address addr;
		return parseIpAddress(addr, ip) && isValidIp6(addr);
	}
	inline bool isValidIp6(const wstring& ip) noexcept
	{
		Ip6Address addr;
		return parseIpAddress(addr, ip) && isValidIp6(addr);
	}

	bool isEmpty(const Ip6Address& ip) noexcept;

	string printIpAddress(const Ip6Address& ip, bool brackets = false) noexcept;
	string printIpAddress(const Ip6AddressEx& ip, bool brackets = false) noexcept;

	template<typename T>
	wstring printIpAddressW(const T& ip, bool brackets = false) noexcept
	{
		string tmp = printIpAddress(ip, brackets);
		wstring result;
		size_t len = tmp.length();
		result.resize(len);
		for (size_t i = 0; i < len; ++i)
			result[i] = tmp[i];
		return result;
	}
}

#endif // IP6_ADDRESS_H_
