#include "stdinc.h"
#include "Resolver.h"

#ifdef _WIN32

#include <ws2tcpip.h>

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#endif

// af = AF_INET / AF_INET6 - return only IPv4 or IPv6
// af = 0 - return both
int Resolver::resolveHost(Ip4Address* v4, Ip6AddressEx* v6, int af, const string& host, bool* isNumeric) noexcept
{
	if (v4) *v4 = 0;
	if (v6) memset(v6, 0, sizeof(*v6));
	if (host.find(':') != string::npos)
	{
		if (!(af == 0 || af == AF_INET6)) return 0;
		Ip6AddressEx ip6;
		if (Util::parseIpAddress(ip6, host))
		{
			if (v6) *v6 = ip6;
			if (isNumeric) *isNumeric = true;
			return RESOLVE_RESULT_V6;
		}
	}
	else
	{
		Ip4Address ip4;
		if (Util::parseIpAddress(ip4, host))
		{
			if (!(af == 0 || af == AF_INET)) return 0;
			if (v4) *v4 = ip4;
			if (isNumeric) *isNumeric = true;
			return RESOLVE_RESULT_V4;
		}
	}
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	if (!af) hints.ai_flags = AI_V4MAPPED | AI_ALL;
	addrinfo* result = nullptr;
	if (getaddrinfo(host.c_str(), nullptr, &hints, &result))
		return 0;
	const addrinfo* ai = result;
	const sockaddr* v4Result = nullptr;
	const sockaddr* v6Result = nullptr;
	int outFlags = 0;
	while (ai)
	{
		if (ai->ai_family == AF_INET)
		{
			if ((af == 0 || af == AF_INET) && !v4Result)
				v4Result = ai->ai_addr;
		}
		else if (ai->ai_family == AF_INET6)
		{
			const sockaddr_in6* sa = (const sockaddr_in6*) ai->ai_addr;
			if (IN6_IS_ADDR_V4MAPPED(&sa->sin6_addr))
			{
				if ((af == 0 || af == AF_INET) && !v4Result)
					v4Result = ai->ai_addr;
			}
			else
			{
				if ((af == 0 || af == AF_INET6) && !v6Result)
					v6Result = ai->ai_addr;
			}
		}
		ai = ai->ai_next;
	}
	if (v4Result)
	{
		outFlags |= RESOLVE_RESULT_V4;
		if (v4)
		{
			if (v4Result->sa_family == AF_INET6)
			{
				const sockaddr_in6* sa = (const sockaddr_in6*) v4Result;
				uint32_t val = *(const uint32_t *) (((const uint8_t *) &sa->sin6_addr) + 12);
				*v4 = ntohl(val);
			}
			else
			{
				const sockaddr_in* sa = (const sockaddr_in*) v4Result;
				*v4 = ntohl(sa->sin_addr.s_addr);
			}
		}
	}
	if (v6Result)
	{
		outFlags |= RESOLVE_RESULT_V6;
		if (v6)
		{
			const sockaddr_in6* sa = (const sockaddr_in6*) v6Result;
			memcpy(v6, &sa->sin6_addr, sizeof(*v6));
		}
	}
	freeaddrinfo(result);
	return outFlags;
}

bool Resolver::resolveHost(IpAddressEx& addr, int type, const string& host, bool* isNumeric) noexcept
{
	int af = type & ~RESOLVE_TYPE_EXACT;
	Ip4Address v4;
	Ip6AddressEx v6;
	int result = resolveHost(&v4, &v6, (type & RESOLVE_TYPE_EXACT) ? af : 0, host, isNumeric);
	if (!result) return false;
	unsigned flag[2];
	if (af == AF_INET6)
	{
		flag[0] = RESOLVE_RESULT_V6;
		flag[1] = RESOLVE_RESULT_V4;
	}
	else
	{
		flag[0] = RESOLVE_RESULT_V4;
		flag[1] = RESOLVE_RESULT_V6;
	}
	if (type & RESOLVE_TYPE_EXACT)
		result &= ~flag[1];
	for (int i = 0; i < 2; i++)
		if (result & flag[i])
		{
			if (flag[i] == RESOLVE_RESULT_V4)
			{
				addr.type = AF_INET;
				addr.data.v4 = v4;
			}
			else
			{
				addr.type = AF_INET6;
				addr.data.v6 = v6;
			}
			return true;
		}
	return false;
}
