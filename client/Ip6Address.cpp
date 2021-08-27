#include "stdinc.h"
#include "Ip6Address.h"
#include "inet_compat.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/ip6.h>
#endif

static bool parseIpAddress(Ip6Address& result, const char* s) noexcept
{
	return inet_pton_compat(AF_INET6, s, &result) == 1;
}

bool Util::parseIpAddress(Ip6Address& result, const string& s, string::size_type start, string::size_type end) noexcept
{
	if (end - start > 2 && s[start] == '[' && s[end-1] == ']')
	{
		start++;
		end--;
	}
	string::size_type pos = s.find('%', start);
	if (pos != string::npos && pos < end) end = pos;
	size_t len = end - start;
	if (start == 0 && len == s.length())
		return parseIpAddress(result, s.c_str());
	return parseIpAddress(result, s.substr(start, len).c_str());
}

bool Util::parseIpAddress(Ip6Address& result, const wstring& s, wstring::size_type start, wstring::size_type end) noexcept
{
	if (end - start > 2 && s[start] == L'[' && s[end-1] == L']')
	{
		start++;
		end--;
	}
	wstring::size_type pos = s.find(L'%', start);
	if (pos != wstring::npos && pos < end) end = pos;
	size_t len = end - start;
	string tmp;
	tmp.resize(len);
	for (size_t i = 0; i < len; ++i)
		tmp[i] = (char) s[start + i];
	return parseIpAddress(result, tmp);
}

bool Util::isEmpty(const Ip6Address& ip) noexcept
{
	return ip.data[0] == 0 && ip.data[1] == 0 && ip.data[2] == 0 && ip.data[3] == 0 &&
	       ip.data[4] == 0 && ip.data[5] == 0 && ip.data[6] == 0 && ip.data[7] == 0;
}

bool Util::isValidIp6(const Ip6Address& ip) noexcept
{
	if (isEmpty(ip)) return false;
	if (ip.data[0] == 0xFFFF && ip.data[1] == 0xFFFF && ip.data[2] == 0xFFFF && ip.data[3] == 0xFFFF &&
	    ip.data[4] == 0xFFFF && ip.data[5] == 0xFFFF && ip.data[6] == 0xFFFF && ip.data[7] == 0xFFFF) return false;
	return true;
}

string Util::printIpAddress(const Ip6Address& ip, bool brackets) noexcept
{
	char buf[64];
	char* out = brackets ? buf + 1 : buf;
	if (!inet_ntop_compat(AF_INET6, &ip, out, sizeof(buf)-2)) return string();
	size_t len = strlen(out);
	if (brackets)
	{
		buf[0] = '[';
		out[len] = ']';
		len += 2;
	}
	return string(buf, len);
}

wstring Util::printIpAddressW(const Ip6Address& ip, bool brackets) noexcept
{
	string tmp = printIpAddress(ip, brackets);
	wstring result;
	size_t len = tmp.length();
	result.resize(len);
	for (size_t i = 0; i < len; ++i)
		result[i] = tmp[i];
	return result;
}
