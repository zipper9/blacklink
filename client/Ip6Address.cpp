#include "stdinc.h"
#include "Ip6Address.h"
#include "inet_compat.h"
#include "StrUtil.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/ip6.h>
#endif

static inline bool parseIpAddress(Ip6Address& result, const char* s) noexcept
{
	return inet_pton_compat(AF_INET6, s, &result) == 1;
}

static inline bool checkPrefix(uint16_t w, uint16_t prefix, int len) noexcept
{
	return (w & (0xFFFF << (16-len))) == prefix;
}

static inline bool isLinkScopedIp(const Ip6Address& ip) noexcept
{
	uint16_t w = ntohs(ip.data[0]);
	return checkPrefix(w, 0xFE80, 10);
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
		return ::parseIpAddress(result, s.c_str());
	return ::parseIpAddress(result, s.substr(start, len).c_str());
}

bool Util::parseIpAddress(Ip6AddressEx& result, const string& s, string::size_type start, string::size_type end) noexcept
{
	result.scopeId = 0;
	if (end - start > 2 && s[start] == '[' && s[end-1] == ']')
	{
		start++;
		end--;
	}
	string::size_type endPos = end;
	string::size_type scopePos = s.find('%', start);
	if (scopePos != string::npos && scopePos < end)
	{
		if (scopePos + 1 == end) return false;
		end = scopePos++;
	}
	size_t len = end - start;
	if (start == 0 && len == s.length())
		return ::parseIpAddress(result, s.c_str());
	if (!::parseIpAddress(result, s.substr(start, len).c_str())) return false;
	if (scopePos != string::npos && isLinkScopedIp(result))
	{
		result.scopeId = Util::stringToInt<uint32_t, char>(s.c_str(), scopePos);
		if (scopePos != endPos) return false;
	}
	return true;
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

string Util::printIpAddress(const Ip6AddressEx& ip, bool brackets) noexcept
{
	if (!ip.scopeId) return printIpAddress(static_cast<const Ip6Address&>(ip), brackets);
	string s = printIpAddress(static_cast<const Ip6Address&>(ip));
	char buf[64];
	int len = sprintf(buf, "%%%u", ip.scopeId);
	s.insert(brackets ? s.length() - 1 : s.length(), buf, len);
	return s;
}
