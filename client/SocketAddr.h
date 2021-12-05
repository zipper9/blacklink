#ifndef SOCKET_ADDR_H_
#define SOCKET_ADDR_H_

#include "IpAddress.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/ip6.h>
#endif

union sockaddr_u
{
	sockaddr_in v4;
	sockaddr_in6 v6;
};

static inline void toSockAddr(sockaddr_u& sa, socklen_t& size, const IpAddress& ip, uint16_t port)
{
	memset(&sa, 0, sizeof(sa));
	switch (ip.type)
	{
		case AF_INET:
			sa.v4.sin_family = AF_INET;
			sa.v4.sin_addr.s_addr = htonl(ip.data.v4);
			sa.v4.sin_port = htons(port);
			size = sizeof(sa.v4);
			break;

		case AF_INET6:
			sa.v6.sin6_family = AF_INET6;
			memcpy(&sa.v6.sin6_addr, &ip.data.v6.data, sizeof(in6_addr));
			sa.v6.sin6_port = htons(port);
			size = sizeof(sa.v6);
			break;

		default:
			size = 0;
	}
}

static inline void toSockAddr(sockaddr_u& sa, socklen_t& size, const IpAddressEx& ip, uint16_t port)
{
	memset(&sa, 0, sizeof(sa));
	switch (ip.type)
	{
		case AF_INET:
			sa.v4.sin_family = AF_INET;
			sa.v4.sin_addr.s_addr = htonl(ip.data.v4);
			sa.v4.sin_port = htons(port);
			size = sizeof(sa.v4);
			break;

		case AF_INET6:
			sa.v6.sin6_family = AF_INET6;
			memcpy(&sa.v6.sin6_addr, &ip.data.v6.data, sizeof(in6_addr));
			sa.v6.sin6_port = htons(port);
			sa.v6.sin6_scope_id = ip.data.v6.scopeId;
			size = sizeof(sa.v6);
			break;

		default:
			size = 0;
	}
}

static inline void fromSockAddr(IpAddress& ip, uint16_t& port, const sockaddr_u& sa)
{
	memset(&ip, 0, sizeof(ip));
	switch (((const sockaddr*) &sa)->sa_family)
	{
		case AF_INET:
			ip.type = AF_INET;
			ip.data.v4 = htonl(sa.v4.sin_addr.s_addr);
			port = ntohs(sa.v4.sin_port);
			break;

		case AF_INET6:
			ip.type = AF_INET6;
			memcpy(&ip.data.v6.data, &sa.v6.sin6_addr, sizeof(in6_addr));
			port = ntohs(sa.v6.sin6_port);
			break;
	}
}

static inline void fromSockAddr(IpAddressEx& ip, uint16_t& port, const sockaddr_u& sa)
{
	memset(&ip, 0, sizeof(ip));
	switch (((const sockaddr*) &sa)->sa_family)
	{
		case AF_INET:
			ip.type = AF_INET;
			ip.data.v4 = htonl(sa.v4.sin_addr.s_addr);
			port = ntohs(sa.v4.sin_port);
			break;

		case AF_INET6:
			ip.type = AF_INET6;
			memcpy(&ip.data.v6.data, &sa.v6.sin6_addr, sizeof(in6_addr));
			port = ntohs(sa.v6.sin6_port);
			ip.data.v6.scopeId = sa.v6.sin6_scope_id;
			break;
	}
}

#endif // SOCKET_ADDR_H_
