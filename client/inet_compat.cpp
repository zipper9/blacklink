#include "inet_compat.h"
#include "stdinc.h"

#ifdef OSVER_WIN_XP
#include <ws2tcpip.h>

int inet_pton_compat(int af, const char* src, void* dst)
{
	if (af != AF_INET && af != AF_INET6) return -1;

	sockaddr_storage ss;
	int size = sizeof(ss);
	int len = strlen(src);
	char* src_copy = (char*) _alloca(len + 1);
	memcpy(src_copy, src, len + 1);
	memset(&ss, 0, sizeof(ss));
	if (WSAStringToAddressA(src_copy, af, nullptr, (struct sockaddr*) &ss, &size))
		return 0;
	if (af == AF_INET)
		*(in_addr*) dst = ((sockaddr_in*) &ss)->sin_addr;
	else
		*(in6_addr*) dst = ((sockaddr_in6*) &ss)->sin6_addr;
	return 1;
}

const char* inet_ntop_compat(int af, const void* src, char* dst, int size)
{
	if (af != AF_INET && af != AF_INET6) return nullptr;

	sockaddr_storage ss;
	int addrSize;
	if (af == AF_INET)
	{
		sockaddr_in* sa = (sockaddr_in*) &ss;
		memset(sa, 0, sizeof(*sa));
		sa->sin_family = AF_INET;
		sa->sin_addr = *(const in_addr *) src;
		addrSize = sizeof(*sa);
	}
	else
	{
		sockaddr_in6* sa = (sockaddr_in6*) &ss;
		memset(sa, 0, sizeof(*sa));
		sa->sin6_family = AF_INET6;
		sa->sin6_addr = *(const in6_addr *) src;
		addrSize = sizeof(*sa);
	}

	DWORD dstSize = size;
	return WSAAddressToStringA((sockaddr *) &ss, addrSize, nullptr, dst, &dstSize) ? nullptr : dst;
}
#endif
