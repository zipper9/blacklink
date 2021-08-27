#include "stdinc.h"
#include "IpAddress.h"
#include "BaseUtil.h"

bool operator==(const IpAddress& a, const IpAddress& b)
{
	if (a.type != b.type) return false;
	if (a.type == AF_INET)
		return a.data.v4 == b.data.v4;
	if (a.type == AF_INET6)
		return a.data.v6 == b.data.v6;
	return true;
}

int compare(const IpAddress& a, const IpAddress& b)
{
	if (a.type != b.type) return compare(a.type, b.type);
	if (a.type == AF_INET)
		return compare(a.data.v4, b.data.v4);
	if (a.type == AF_INET6)
		return memcmp(&a.data.v6, &b.data.v6, sizeof(Ip6Address));
	return 0;
}

bool Util::parseIpAddress(IpAddress& result, const string& s) noexcept
{
	result.type = 0;
	if (s.empty()) return false;
	if (s.find(':') != string::npos)
	{
		if (!parseIpAddress(result.data.v6, s)) return false;
		result.type = AF_INET6;
		return true;
	}
	if (!parseIpAddress(result.data.v4, s)) return false;
	result.type = AF_INET;
	return true;
}

bool Util::isValidIp(const IpAddress& addr) noexcept
{
	if (addr.type == AF_INET)
		return isValidIp4(addr.data.v4);
	if (addr.type == AF_INET6)
		return isValidIp6(addr.data.v6);
	return false;
}

string Util::printIpAddress(const IpAddress& addr, bool brackets) noexcept
{
	if (addr.type == AF_INET)
		return printIpAddress(addr.data.v4);
	if (addr.type == AF_INET6)
		return printIpAddress(addr.data.v6, brackets);
	return Util::emptyString;
}

wstring Util::printIpAddressW(const IpAddress& addr, bool brackets) noexcept
{
	if (addr.type == AF_INET)
		return printIpAddressW(addr.data.v4);
	if (addr.type == AF_INET6)
		return printIpAddressW(addr.data.v6, brackets);
	return Util::emptyStringW;
}
