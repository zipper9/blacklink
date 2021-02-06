#include "stdinc.h"
#include "Ip4Address.h"

template<typename string_type>
bool parseIpAddress(Ip4Address& result, const string_type& s, typename string_type::size_type start, typename string_type::size_type end)
{
	uint32_t byte = 0;
	uint32_t bytes = 0;
	bool digitFound = false;
	int dotCount = 0;
	result = 0;
	while (start < end)
	{
		if (s[start] >= '0' && s[start] <= '9')
		{
			byte = byte * 10 + s[start] - '0';
			if (byte > 255) return false;
			digitFound = true;
		}
		else
		if (s[start] == '.')
		{
			if (!digitFound || ++dotCount == 4) return false;
			bytes = bytes << 8 | byte;
			byte = 0;
			digitFound = false;
		}
		else return false;
		++start;
	}
	if (dotCount != 3 || !digitFound) return false;
	result = bytes << 8 | byte;
	return true;
}

bool Util::parseIpAddress(Ip4Address& result, const string& s, string::size_type start, string::size_type end)
{
	return ::parseIpAddress(result, s, start, end);
}

bool Util::parseIpAddress(Ip4Address& result, const wstring& s, wstring::size_type start, wstring::size_type end)
{
	return ::parseIpAddress(result, s, start, end);
}

bool Util::isValidIp4(const string& ip)
{
	Ip4Address result;
	return parseIpAddress(result, ip, 0, ip.length()) && result && result != 0xFFFFFFFF;
}

bool Util::isValidIp4(const wstring& ip)
{
	Ip4Address result;
	return parseIpAddress(result, ip, 0, ip.length()) && result && result != 0xFFFFFFFF;
}

string Util::printIpAddress(Ip4Address addr)
{
	char buf[64];
	sprintf(buf, "%d.%d.%d.%d", addr >> 24, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF);
	return string(buf);
}

wstring Util::printIpAddressW(Ip4Address addr)
{
	wchar_t buf[64];
	swprintf(buf, L"%d.%d.%d.%d", addr >> 24, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF);
	return wstring(buf);
}
