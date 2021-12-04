#include "stdinc.h"
#include "IpAddress.h"
#include "BaseUtil.h"

bool operator==(const IpAddress& a, const IpAddress& b) noexcept
{
	if (a.type != b.type) return false;
	if (a.type == AF_INET)
		return a.data.v4 == b.data.v4;
	if (a.type == AF_INET6)
		return a.data.v6 == b.data.v6;
	return true;
}

bool operator==(const IpAddressEx& a, const IpAddressEx& b) noexcept
{
	if (a.type != b.type) return false;
	if (a.type == AF_INET)
		return a.data.v4 == b.data.v4;
	if (a.type == AF_INET6)
		return a.data.v6 == b.data.v6;
	return true;
}

int compare(const IpAddress& a, const IpAddress& b) noexcept
{
	if (a.type != b.type) return compare(a.type, b.type);
	if (a.type == AF_INET)
		return compare(a.data.v4, b.data.v4);
	if (a.type == AF_INET6)
		return memcmp(a.data.v6.data, b.data.v6.data, sizeof(Ip6Address));
	return 0;
}

int compare(const IpAddressEx& a, const IpAddressEx& b) noexcept
{
	if (a.type != b.type) return compare(a.type, b.type);
	if (a.type == AF_INET)
		return compare(a.data.v4, b.data.v4);
	if (a.type == AF_INET6)
	{
		if (a.data.v6.scopeId != b.data.v6.scopeId)
			return compare(a.data.v6.scopeId, b.data.v6.scopeId);
		return memcmp(a.data.v6.data, b.data.v6.data, sizeof(Ip6Address));
	}
	return 0;
}

template<typename T, typename S> 
bool _parseIpAddress(T& result, const S& s) noexcept
{
	result.type = 0;
	if (s.empty()) return false;
	if (s.find(':') != string::npos)
	{
		if (!Util::parseIpAddress(result.data.v6, s)) return false;
		result.type = AF_INET6;
		return true;
	}
	if (!Util::parseIpAddress(result.data.v4, s)) return false;
	result.type = AF_INET;
	return true;
}

bool Util::parseIpAddress(IpAddress& result, const string& s) noexcept
{
	return _parseIpAddress(result, s);
}

bool Util::parseIpAddress(IpAddressEx& result, const string& s) noexcept
{
	return _parseIpAddress(result, s);
}

template<typename T>
string _printIpAddress(const T& addr, bool brackets) noexcept
{
	if (addr.type == AF_INET)
		return Util::printIpAddress(addr.data.v4);
	if (addr.type == AF_INET6)
		return Util::printIpAddress(addr.data.v6, brackets);
	return Util::emptyString;
}

string Util::printIpAddress(const IpAddress& addr, bool brackets) noexcept
{
	return _printIpAddress(addr, brackets);
}

string Util::printIpAddress(const IpAddressEx& addr, bool brackets) noexcept
{
	return _printIpAddress(addr, brackets);
}

template<typename T>
wstring _printIpAddressW(const T& addr, bool brackets) noexcept
{
	if (addr.type == AF_INET)
		return Util::printIpAddressW(addr.data.v4);
	if (addr.type == AF_INET6)
		return Util::printIpAddressW(addr.data.v6, brackets);
	return Util::emptyStringW;
}

wstring Util::printIpAddressW(const IpAddress& addr, bool brackets) noexcept
{
	return _printIpAddressW(addr, brackets);
}

wstring Util::printIpAddressW(const IpAddressEx& addr, bool brackets) noexcept
{
	return _printIpAddressW(addr, brackets);
}
