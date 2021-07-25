#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

int win32_inet_pton(int af, const char* src, void* dst)
{
	if (af != AF_INET && af != AF_INET6) return -1;

	struct sockaddr_storage ss;
	int size = sizeof(ss);
	int len = strlen(src);
	char* src_copy = (char*) _alloca(len + 1);
	memcpy(src_copy, src, len + 1);
	memset(&ss, 0, sizeof(ss));
	if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr*) &ss, &size))
		return 0;
	if (af == AF_INET)
		*(struct in_addr*) dst = ((struct sockaddr_in*) &ss)->sin_addr;
	else
		*(struct in6_addr*) dst = ((struct sockaddr_in6*) &ss)->sin6_addr;
	return 1;
}

#endif
