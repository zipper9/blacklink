#ifndef IP4_ADDRESS_H_
#define IP4_ADDRESS_H_

#include "typedefs.h"

typedef uint32_t Ip4Address;

namespace Util
{
	bool parseIpAddress(Ip4Address& result, const string& s, string::size_type start, string::size_type end) noexcept;
	bool parseIpAddress(Ip4Address& result, const wstring& s, wstring::size_type start, wstring::size_type end) noexcept;
	inline bool parseIpAddress(Ip4Address& result, const string& s)
	{
		return parseIpAddress(result, s, 0, s.length());
	}
	inline bool parseIpAddress(Ip4Address& result, const wstring& s)
	{
		return parseIpAddress(result, s, 0, s.length());
	}

	inline bool isValidIp4(Ip4Address ip) { return ip != 0 && ip != 0xFFFFFFFF; }
	bool isValidIp4(const string& ip) noexcept;
	bool isValidIp4(const wstring& ip) noexcept;

	string printIpAddress(Ip4Address addr) noexcept;
	wstring printIpAddressW(Ip4Address addr) noexcept;
}

#endif // IP4_ADDRESS_H_
