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

struct IpAddressEx
{
	int type; // 0 (empty), AF_INET, AF_INET6
	union
	{
		Ip4Address v4;
		Ip6AddressEx v6;
	} data;
};

bool operator==(const IpAddress& a, const IpAddress& b) noexcept;
bool operator==(const IpAddressEx& a, const IpAddressEx& b) noexcept;

inline bool operator!=(const IpAddress& a, const IpAddress& b) noexcept { return !(a == b); }
inline bool operator!=(const IpAddressEx& a, const IpAddressEx& b) noexcept { return !(a == b); }

int compare(const IpAddress& a, const IpAddress& b) noexcept;
int compare(const IpAddressEx& a, const IpAddressEx& b) noexcept;

#ifdef _UNICODE
#define printIpAddressT printIpAddressW
#else
#define printIpAddressT printIpAddress
#endif

namespace Util
{
	bool parseIpAddress(IpAddress& result, const string& s) noexcept;
	bool parseIpAddress(IpAddressEx& result, const string& s) noexcept;

	template<typename T>
	bool isValidIp(const T& addr) noexcept
	{
		if (addr.type == AF_INET)
			return isValidIp4(addr.data.v4);
		if (addr.type == AF_INET6)
			return isValidIp6(addr.data.v6);
		return false;
	}

	template<typename T>
	bool isEmpty(const T& addr) noexcept
	{
		if (!addr.type)
			return true;
		if (addr.type == AF_INET)
			return addr.data.v4 == 0;
		if (addr.type == AF_INET6)
			return isEmpty(addr.data.v6);
		return false;
	}

	string printIpAddress(const IpAddress& addr, bool brackets = false) noexcept;
	string printIpAddress(const IpAddressEx& addr, bool brackets = false) noexcept;

	wstring printIpAddressW(const IpAddress& addr, bool brackets = false) noexcept;
	wstring printIpAddressW(const IpAddressEx& addr, bool brackets = false) noexcept;
}

#endif // IP_ADDRESS_H_
