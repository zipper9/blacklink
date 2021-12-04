/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "Socket.h"
#include "TimerManager.h"
#include "IpGuard.h"
#include "LogManager.h"
#include "ResourceManager.h"
#include "SettingsManager.h"

#ifdef _WIN32
#include "CompatibilityManager.h"
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#define SHUT_RDWR SD_BOTH
#endif

/// @todo remove when MinGW has this
#ifdef __MINGW32__
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#endif
#endif

string Socket::g_udpServer;
uint16_t Socket::g_udpPort;

#ifdef _DEBUG

SocketException::SocketException(int error) noexcept :
	Exception("SocketException: " + errorToString(error))
{
	errorCode = error;
	dcdebug("Thrown: %s\n", what());
}

#else // _DEBUG

SocketException::SocketException(int error) noexcept :
	Exception(errorToString(error))
{
	errorCode = error;
}

#endif

Socket::Stats Socket::g_stats;

static const unsigned SOCKS_TIMEOUT = 30000;

void Socket::toSockAddr(sockaddr_u& sa, socklen_t& size, const IpAddress& ip, uint16_t port)
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

void Socket::toSockAddr(sockaddr_u& sa, socklen_t& size, const IpAddressEx& ip, uint16_t port)
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

void Socket::fromSockAddr(IpAddress& ip, uint16_t& port, const sockaddr_u& sa)
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

void Socket::fromSockAddr(IpAddressEx& ip, uint16_t& port, const sockaddr_u& sa)
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

static bool isAnyAddr(const sockaddr_u& sa)
{
	switch (((const sockaddr*) &sa)->sa_family)
	{
		case AF_INET:
			return sa.v4.sin_addr.s_addr == INADDR_ANY;

		case AF_INET6:
			return IN6_IS_ADDR_UNSPECIFIED(&sa.v6.sin6_addr);
	
		default:
			return false;
	}
}

static void setAnyAddr(sockaddr_u& sa)
{
	switch (((const sockaddr*) &sa)->sa_family)
	{
		case AF_INET:
			sa.v4.sin_addr.s_addr = INADDR_ANY;
			break;

		case AF_INET6:
			memset(&sa.v6.sin6_addr, 0, sizeof(sa.v6.sin6_addr));
			break;
	}
}

string SocketException::errorToString(int error) noexcept
{
	string msg = Util::translateError(error);
	if (msg.empty())
	{
		char tmp[64];
		tmp[0] = 0;
		snprintf(tmp, sizeof(tmp), CSTRING(UNKNOWN_ERROR), error);
		msg = tmp;
	}
	
	return msg;
}

socket_t Socket::getSock() const
{
	return sock;
}

void Socket::setIp(const IpAddressEx& ip)
{
	switch (ip.type)
	{
		case AF_INET:
			this->ip.data.v4 = ip.data.v4;
			this->ip.type = AF_INET;
			break;
		case AF_INET6:
			this->ip.data.v6 = ip.data.v6;
			this->ip.type = AF_INET6;
			break;
	}
}

void Socket::create(int af, SocketType type)
{
	if (sock != INVALID_SOCKET)
		disconnect();
		
	switch (type)
	{
		case TYPE_TCP:
			sock = checksocket(socket(af, SOCK_STREAM, IPPROTO_TCP));
			break;
		case TYPE_UDP:
			sock = checksocket(socket(af, SOCK_DGRAM, IPPROTO_UDP));
			break;
		default:
			dcassert(0);
	}
	this->type = type;
	setBlocking(false);
}

uint16_t Socket::accept(const Socket& listeningSocket)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	string sockName;

	if (sock != INVALID_SOCKET)
		disconnect();

	if (doLog) listeningSocket.printSockName(sockName);

	sockaddr_u sockAddr;
	socklen_t sockLen = sizeof(sockAddr);

#ifdef _WIN32
	sock = ::accept(listeningSocket.sock, (struct sockaddr*) &sockAddr, &sockLen);
#else
	do
	{
		sock = ::accept(listeningSocket.sock, (struct sockaddr*) &sockAddr, &sockLen);
	}
	while (sock == SOCKET_ERROR && getLastError() == EINTR);
#endif
	
	if (sock == INVALID_SOCKET)
	{
		if (doLog)
			LogManager::message(sockName +
				": Accept error #" + Util::toString(getLastError()), false);
		throw SocketException(getLastError());
	}
	
	IpAddress remoteIp;
	uint16_t port;
	fromSockAddr(remoteIp, port, sockAddr);
	if (BOOLSETTING(ENABLE_IPGUARD) && remoteIp.type == AF_INET && ipGuard.isBlocked(remoteIp.data.v4))
		throw SocketException(STRING_F(IP_BLOCKED, "IPGuard" % Util::printIpAddress(remoteIp.data.v4)));

#ifdef _WIN32
	// Make sure we disable any inherited windows message things for this socket.
	::WSAAsyncSelect(sock, NULL, 0, 0);
#endif

	type = TYPE_TCP;

	if (doLog)
	{
		string newSockName;
		printSockName(newSockName);
		LogManager::message(sockName +
			": Accepted (" + newSockName + ") from " +
			Util::printIpAddress(remoteIp, true) + ":" + Util::toString(port), false);
	}

	// remote IP
	setIp(remoteIp);
	setBlocking(false);

	// return the remote port
	return port;
}

uint16_t Socket::bind(uint16_t port, const IpAddressEx& addr)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	string sockName;
	if (doLog) printSockName(sockName);

	sockaddr_u sockAddr;
	socklen_t sockAddrSize;
	toSockAddr(sockAddr, sockAddrSize, addr, port);

#ifdef OSVER_WIN_XP
	if (CompatibilityManager::isOsVistaPlus())
#endif
	if (addr.type == AF_INET6)
		setSocketOpt(IPPROTO_IPV6, IPV6_V6ONLY, 1);

	if (::bind(sock, (sockaddr*) &sockAddr, sockAddrSize) == SOCKET_ERROR)
	{
		if (isAnyAddr(sockAddr))
		{
			if (doLog)
				LogManager::message(sockName +
					": Error #" + Util::toString(getLastError()) +
					" binding to :" + Util::toString(port), false);
			throw SocketException(getLastError());
		}
		if (doLog)
			LogManager::message(sockName +
				": Error #" + Util::toString(getLastError()) +
				" binding to " + Util::printIpAddress(addr, true) + ":" + Util::toString(port) + ", retrying with " +
				string(addr.type == AF_INET6 ? "INADDR6_ANY" : "INADDR_ANY"), false);
		setAnyAddr(sockAddr);
		if (::bind(sock, (sockaddr*) &sockAddr, sockAddrSize) == SOCKET_ERROR)
		{
			if (doLog)
				LogManager::message(sockName +
					": Error #" + Util::toString(getLastError()) +
					" binding to :" + Util::toString(port), false);
			throw SocketException(getLastError());
		}
	}

	uint16_t localPort = getLocalPort();
	if (doLog)
		LogManager::message(sockName +
			": Bound to " + Util::printIpAddress(addr, true) + ":" + Util::toString(localPort) +
			(type == TYPE_TCP ? " (TCP)" : " (UDP)"), false);
	return localPort;
}

void Socket::listen()
{
	check(::listen(sock, 20));
}

void Socket::connect(const string& host, uint16_t port)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	IpAddressEx ip;
	bool isNumeric;
	if (!resolveHost(ip, 0, host, &isNumeric))
	{
		if (doLog)
			LogManager::message("Error resolving " + host, false);
		throw SocketException(STRING(RESOLVE_FAILED));
	}

	if (sock == INVALID_SOCKET)
		create(ip.type, TYPE_TCP);

	if (doLog && !isNumeric)
	{
		string sockName;
		printSockName(sockName);
		LogManager::message(sockName + ": Host " + host + " resolved to " + Util::printIpAddress(ip, true), false);
	}

	connect(ip, port, isNumeric ? Util::emptyString : host);
}

void Socket::connect(const IpAddressEx& ip, uint16_t port, const string& host)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);

	if (sock == INVALID_SOCKET)
		create(ip.type, TYPE_TCP);

	if (doLog)
	{
		string sockName;
		printSockName(sockName);
		LogManager::message(sockName + ": Connecting to " +
			(host.empty() ? Util::printIpAddress(ip, true) : host) +
			":" + Util::toString(port) + ", secureTransport=" + Util::toString(getSecureTransport()), false);
	}

	if (BOOLSETTING(ENABLE_IPGUARD) && ip.type == AF_INET && ipGuard.isBlocked(ip.data.v4))
	{
		string error = STRING_F(IP_BLOCKED, "IPGuard" % Util::printIpAddress(ip.data.v4));
		if (!host.empty()) error += " (" + host + ")";
		throw SocketException(error);
	}

	sockaddr_u sockAddr;
	socklen_t sockAddrSize;
	toSockAddr(sockAddr, sockAddrSize, ip, port);

	int result;
#ifdef _WIN32
	result = ::connect(sock, (struct sockaddr*) &sockAddr, sockAddrSize);
#else
	do
	{
		result = ::connect(sock, (struct sockaddr*) &sockAddr, sockAddrSize);
	}
	while (result < 0 && getLastError() == EINTR);
#endif
	check(result, true);
	
	setIp(ip);
	setPort(port);
}

int Socket::getSocketOptInt(int level, int option) const
{
	int val = 0;
	socklen_t len = sizeof(val);
	check(::getsockopt(sock, level, option, (char*)&val, &len));
	return val;
}

void Socket::setInBufSize()
{
#ifdef _WIN32
	if (!CompatibilityManager::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
#endif
	{
		const int sockInBuf = SETTING(SOCKET_IN_BUFFER);
		if (sockInBuf > 0)
			setSocketOpt(SOL_SOCKET, SO_RCVBUF, sockInBuf);
	}
}

void Socket::setOutBufSize()
{
#ifdef _WIN32
	if (!CompatibilityManager::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
#endif
	{
		const int sockOutBuf = SETTING(SOCKET_OUT_BUFFER);
		if (sockOutBuf > 0)
			setSocketOpt(SOL_SOCKET, SO_SNDBUF, sockOutBuf);
	}
}

void Socket::setSocketOpt(int level, int option, int val)
{
	int len = sizeof(val);
	check(::setsockopt(sock, level, option, (char*)&val, len));
}

int Socket::read(void* aBuffer, int aBufLen)
{
	int len = 0;
	
	dcassert(type == TYPE_TCP || type == TYPE_UDP);
	dcassert(sock != INVALID_SOCKET);
	
	if (type == TYPE_TCP)
	{
#ifdef _WIN32
		lastWaitResult &= ~WAIT_READ;
		len = ::recv(sock, (char*)aBuffer, aBufLen, 0);
#else
		do
		{
			len = ::recv(sock, (char*)aBuffer, aBufLen, 0);
		}
		while (len < 0 && getLastError() == EINTR);
#endif
	}
	else
	{
#ifdef _WIN32
		len = ::recvfrom(sock, (char*)aBuffer, aBufLen, 0, NULL, NULL);
#else
		do
		{
			len = ::recvfrom(sock, (char*)aBuffer, aBufLen, 0, NULL, NULL);
		}
		while (len < 0 && getLastError() == EINTR);
#endif
	}
	
	check(len, true);
	if (len > 0)
	{
		if (type == TYPE_UDP)
			g_stats.udp.downloaded += len;
		else
			g_stats.tcp.downloaded += len;
	}
	return len;
}

int Socket::readAll(void* aBuffer, int aBufLen, unsigned timeout)
{
	uint8_t* buf = (uint8_t*)aBuffer;
	int i = 0;
	while (i < aBufLen)
	{
		const int j = read(buf + static_cast<size_t>(i), aBufLen - i); // [!] PVS V104 Implicit conversion of 'i' to memsize type in an arithmetic expression: buf + i socket.cpp 436
		if (j == 0)
		{
			return i;
		}
		else if (j == -1)
		{
			if (wait(timeout, WAIT_READ) != WAIT_READ)
			{
				return i;
			}
			continue;
		}
		dcassert(j > 0); // [+] IRainman fix.
		i += j;
	}
	return i;
}

int Socket::writeAll(const void* aBuffer, int aLen, unsigned timeout)
{
	const uint8_t* buf = (const uint8_t*)aBuffer;
	int pos = 0;
	// No use sending more than this at a time...
#if 0
	const int sendSize = getSocketOptInt(SOL_SOCKET, SO_SNDBUF);
#else
	const int sendSize = MAX_SOCKET_BUFFER_SIZE;
#endif
	
	while (pos < aLen)
	{
		const int i = write(buf + static_cast<size_t>(pos), (int)min(aLen - pos, sendSize)); // [!] PVS V104 Implicit conversion of 'pos' to memsize type in an arithmetic expression: buf + pos socket.cpp 464
		if (i == -1)
		{
			wait(timeout, WAIT_WRITE);
		}
		else
		{
			dcassert(i >= 0); // [+] IRainman fix.
			pos += i;
			// [-] IRainman fix: please see Socket::write
			// [-] g_stats.totalUp += i;
		}
	}
	return pos;
}

int Socket::write(const void* buffer, int len)
{
	dcassert(sock != INVALID_SOCKET);
	
	int sent = 0;
#ifdef _WIN32
	sent = ::send(sock, (const char*)buffer, len, 0);
#else
	do
	{
		sent = ::send(sock, (const char*)buffer, len, 0);
	}
	while (sent < 0 && getLastError() == EINTR);
#endif

	check(sent, true);
	if (sent > 0)
	{
		if (type == TYPE_UDP)
			g_stats.udp.uploaded += sent;
		else
			g_stats.tcp.uploaded += sent;
	}
#ifdef _WIN32
	if (sent < 0)
		lastWaitResult &= ~WAIT_WRITE;
#endif
	return sent;
}

int Socket::sendPacket(const void* buffer, int bufLen, const IpAddress& ip, uint16_t port) noexcept
{
	dcassert(type == TYPE_UDP);
	sockaddr_u sockAddr;
	socklen_t sockLen;
	toSockAddr(sockAddr, sockLen, ip, port);
	int res = sendto(sock, static_cast<const char*>(buffer), bufLen, 0, (const sockaddr*) &sockAddr, sockLen);
	if (res > 0) g_stats.udp.uploaded += res;
	return res;
}

int Socket::receivePacket(void* buffer, int bufLen, IpAddress& ip, uint16_t& port) noexcept
{
	dcassert(type == TYPE_UDP);
	sockaddr_u sockAddr;	
	socklen_t sockLen = sizeof(sockAddr);
	int res = recvfrom(sock, static_cast<char*>(buffer), bufLen, 0, (sockaddr*) &sockAddr, &sockLen);
	if (res > 0)
	{
		g_stats.udp.downloaded += res;
		fromSockAddr(ip, port, sockAddr);
	}
	return res;
}

/**
 * Blocks until timeout is reached one of the specified conditions have been fulfilled
 * @param millis Max milliseconds to block.
 * @param waitFor WAIT_*** flags that set what we're waiting for, set to the combination of flags that
 *                triggered the wait stop on return (==WAIT_NONE on timeout)
 * @return WAIT_*** ored together of the current state.
 * @throw SocketException Select or the connection attempt failed.
 */
int Socket::wait(int millis, int waitFor)
{
	dcassert(sock != INVALID_SOCKET);
#ifdef _WIN32
	DWORD waitResult;
	unsigned mask = 0;
	if (waitFor & WAIT_CONNECT) mask |= FD_CONNECT;
	if (waitFor & WAIT_READ) mask |= FD_READ | FD_CLOSE;
	if (waitFor & WAIT_WRITE) mask |= FD_WRITE;
	if (waitFor & WAIT_ACCEPT) mask |= FD_ACCEPT;
	if (waitFor & lastWaitResult)
		return waitFor & lastWaitResult;
	if (!mask)
	{
		if (!controlEvent.empty() && (waitFor & WAIT_CONTROL))
		{
			waitResult = WaitForSingleObject(controlEvent.getHandle(), millis < 0 ? INFINITE : static_cast<DWORD>(millis));
			if (waitResult == WAIT_OBJECT_0)
			{
				controlEvent.reset();
				return WAIT_CONTROL;
			}
			return 0;
		}
		if (millis > 0) Sleep(millis);
		return 0;
	}
	event.create();
	if (currentMask != mask)
	{
		currentMask = mask;
		//dcdebug("WSAEventSelect: sock=%p, mask=%u\n", sock, currentMask);
		int ret = WSAEventSelect(sock, event.getHandle(), currentMask);
		check(ret);
	}

	if (!controlEvent.empty() && (waitFor & WAIT_CONTROL))
	{
		HANDLE handles[] = { controlEvent.getHandle(), event.getHandle() };
		waitResult = WaitForMultipleObjects(2, handles, FALSE, millis < 0 ? INFINITE : static_cast<DWORD>(millis));
		if (waitResult == WAIT_OBJECT_0)
		{
			controlEvent.reset();
			return WAIT_CONTROL;
		}
		if (waitResult == WAIT_OBJECT_0 + 1) waitResult--;
	}
	else
	{
		waitResult = WaitForSingleObject(event.getHandle(), millis < 0 ? INFINITE : static_cast<DWORD>(millis));
	}

	if (waitResult == WAIT_TIMEOUT)
		return 0;
	if (waitResult == WAIT_OBJECT_0)
	{
		WSANETWORKEVENTS ne;
		int ret = WSAEnumNetworkEvents(sock, event.getHandle(), &ne);
		check(ret);
		waitFor = WAIT_NONE;
		//dcdebug("WSAEnumNetworkEvents: 0x%X\n", ne.lNetworkEvents);
		if (ne.lNetworkEvents & FD_CONNECT)
		{
			if (ne.iErrorCode[FD_CONNECT_BIT]) throw SocketException(ne.iErrorCode[FD_CONNECT_BIT]);
			waitFor |= WAIT_CONNECT;
			connected = true;
		}
		if (ne.lNetworkEvents & FD_READ)
		{
			if (ne.iErrorCode[FD_READ_BIT]) throw SocketException(ne.iErrorCode[FD_READ_BIT]);
			waitFor |= WAIT_READ;
			lastWaitResult |= WAIT_READ;
		}
		if (ne.lNetworkEvents & FD_CLOSE)
		{
			if (ne.iErrorCode[FD_CLOSE_BIT]) throw SocketException(ne.iErrorCode[FD_CLOSE_BIT]);
			waitFor |= WAIT_READ;
			lastWaitResult |= WAIT_READ;
		}
		if (ne.lNetworkEvents & FD_WRITE)
		{
			if (ne.iErrorCode[FD_WRITE_BIT]) throw SocketException(ne.iErrorCode[FD_WRITE_BIT]);
			waitFor |= WAIT_WRITE;
			lastWaitResult |= WAIT_WRITE;
		}
		if (ne.lNetworkEvents & FD_ACCEPT)
		{
			if (ne.iErrorCode[FD_ACCEPT_BIT]) throw SocketException(ne.iErrorCode[FD_ACCEPT_BIT]);
			waitFor |= WAIT_ACCEPT;
		}
		return waitFor;
	}
	throw SocketException(getLastError());
#else
	pollfd pfd[2];
	int count = 0;
	if (waitFor & WAIT_CONTROL)
	{
		pfd[0].fd = controlEvent.getHandle();
		pfd[0].events = POLLIN;
		pfd[0].revents = 0;
		count++;
	}
	if (waitFor & ~WAIT_CONTROL)
	{
		pfd[count].fd = sock;
		pfd[count].events = 0;
		pfd[count].revents = 0;
		if (waitFor & WAIT_CONNECT)
		{
			dcassert(!(waitFor & (WAIT_READ | WAIT_WRITE)));
			pfd[count].events |= POLLOUT;
		}
		else if (waitFor & WAIT_ACCEPT)
		{
			dcassert(!(waitFor & (WAIT_READ | WAIT_WRITE)));
			pfd[count].events |= POLLIN;
		}
		else
		{
			if (waitFor & WAIT_READ) pfd[count].events |= POLLIN;
			if (waitFor & WAIT_WRITE) pfd[count].events |= POLLOUT;
		}
		count++;
	}
	int result;
	do
	{
		result = poll(pfd, count, millis);
	} while (result == -1 && errno == EINTR);
	check(result);
	if ((waitFor & WAIT_CONTROL) && (pfd[0].revents & POLLIN))
	{
		controlEvent.reset();
		return WAIT_CONTROL;
	}
	result = 0;
	if (waitFor & ~WAIT_CONTROL)
	{
		if (pfd[count-1].revents & POLLOUT)
		{
			if (waitFor & WAIT_CONNECT)
			{
				int error = 0;
				socklen_t optlen = sizeof(error);
				check(getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*) &error, &optlen));
				if (error) throw SocketException(error);
				connected = true;
				return WAIT_CONNECT;
			}
			result |= WAIT_WRITE;
		}
		if (pfd[count-1].revents & POLLIN)
		{
			if (waitFor & WAIT_ACCEPT) return WAIT_ACCEPT;
			result |= WAIT_READ;
		}
	}
	return result;
#endif
}

bool Socket::waitConnected(unsigned millis)
{
	if (connected) return true;
	return wait(millis, Socket::WAIT_CONNECT) == WAIT_CONNECT;
}

bool Socket::waitAccepted(unsigned /*millis*/)
{
	// Normal sockets are always connected after a call to accept
	return true;
}

int Socket::resolveHost(Ip4Address* v4, Ip6AddressEx* v6, int af, const string& host, bool* isNumeric) noexcept
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
			if (!(af == 0 || af == AF_INET)) return false;
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

bool Socket::resolveHost(IpAddressEx& addr, int af, const string& host, bool* isNumeric) noexcept
{
	Ip4Address v4;
	Ip6AddressEx v6;
	int result = resolveHost(&v4, &v6, af, host, isNumeric);
	if (!result) return false;
	// Prefer IPv4
	if (result & RESOLVE_RESULT_V4)
	{
		addr.type = AF_INET;
		addr.data.v4 = v4;
		return true;
	}
	if (result & RESOLVE_RESULT_V6)
	{
		addr.type = AF_INET6;
		addr.data.v6 = v6;
		return true;
	}
	return false;
}

uint16_t Socket::getLocalPort() const
{
	if (sock == INVALID_SOCKET)
	{
		dcassert(sock != INVALID_SOCKET);
		return 0;
	}
	sockaddr_u sockAddr;
	socklen_t len = sizeof(sockAddr);
	if (getsockname(sock, (struct sockaddr*) &sockAddr, &len) == 0)
	{
		switch (((const sockaddr*) &sockAddr)->sa_family)
		{
			case AF_INET:
				return ntohs(((const sockaddr_in*) &sockAddr)->sin_port);

			case AF_INET6:
				return ntohs(((const sockaddr_in6*) &sockAddr)->sin6_port);
		}
	}
	return 0;
}

IpAddress Socket::getLocalIp() const
{
	IpAddress ip{0};
	if (sock == INVALID_SOCKET)
	{
		dcassert(sock != INVALID_SOCKET);
		return ip;
	}
	sockaddr_u sockAddr;
	socklen_t len = sizeof(sockAddr);
	if (getsockname(sock, (struct sockaddr*) &sockAddr, &len) == 0)
	{
		switch (((const sockaddr*) &sockAddr)->sa_family)
		{
			case AF_INET:
				ip.type = AF_INET;
				ip.data.v4 = ntohl(((const sockaddr_in*) &sockAddr)->sin_addr.s_addr);
				break;

			case AF_INET6:
				ip.type = AF_INET6;
				memcpy(&ip.data.v6, &((const sockaddr_in6*) &sockAddr)->sin6_addr, sizeof(ip.data.v6));
				break;
		}
	}
	return ip;
}

void Socket::socksUpdated(const ProxyConfig* proxy)
{
#if 0 // FIXME
	g_udpServer.clear();
	g_udpPort = 0;
	
	if (proxy)
	{
		try
		{
			Socket s;
			s.setBlocking(false);
			s.connect(proxy->host, proxy->port);
			s.socksAuth(*proxy, SOCKS_TIMEOUT);
			
			uint8_t connStr[10];
			connStr[0] = 5; // SOCKSv5
			connStr[1] = 3; // UDP Associate
			connStr[2] = 0; // Reserved
			connStr[3] = 1; // Address type: IPv4;
			*(uint32_t*) &connStr[4] = 0; // No specific outgoing UDP address
			*(uint16_t*) &connStr[8] = 0; // No specific port...
			
			s.writeAll(connStr, 10, SOCKS_TIMEOUT);
			
			// We assume we'll get a ipv4 address back...therefore, 10 bytes...if not, things
			// will break, but hey...noone's perfect (and I'm tired...)...
			if (s.readAll(connStr, 10, SOCKS_TIMEOUT) != 10)
			{
				return;
			}
			
			if (connStr[0] != 5 || connStr[1] != 0)
			{
				return;
			}
			
			g_udpPort = static_cast<uint16_t>(ntohs(*((uint16_t*)(&connStr[8]))));
			
			in_addr addr;
			addr.s_addr = *(unsigned long *) &connStr[4];
			g_udpServer = inet_ntoa(addr);
		}
		catch (const SocketException&)
		{
			dcdebug("Socket: Failed to register with socks server\n");
		}
	}
#endif
}

void Socket::shutdown() noexcept
{
	if (sock != INVALID_SOCKET)
		::shutdown(sock, SHUT_RDWR);
}

void Socket::close() noexcept
{
	if (sock != INVALID_SOCKET)
	{
#ifdef _WIN32
		::closesocket(sock);
#else
		::close(sock);
#endif
		sock = INVALID_SOCKET;
	}
}

void Socket::disconnect() noexcept
{
	shutdown();
	close();
}

string Socket::getRemoteHost(const IpAddress& ip)
{
	uint32_t ip4;
	const char* paddr;
	int addrSize;
	switch (ip.type)
	{
		case AF_INET:
			ip4 = htonl(ip.data.v4);
			paddr = reinterpret_cast<const char*>(&ip4);
			addrSize = sizeof(ip4);
			break;

		case AF_INET6:
			paddr = reinterpret_cast<const char*>(&ip.data.v6);
			addrSize = sizeof(ip.data.v6);
			break;

		default:
			return Util::emptyString;
	}

	hostent* h = gethostbyaddr(paddr, addrSize, ip.type);
	return h ? h->h_name : Util::emptyString;
}

bool Socket::getProxyConfig(Socket::ProxyConfig& proxy)
{
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)
	{
		proxy.host = SETTING(SOCKS_SERVER);
		proxy.port = SETTING(SOCKS_PORT);
		proxy.user = SETTING(SOCKS_USER);
		proxy.password = SETTING(SOCKS_PASSWORD);
		proxy.resolveNames = BOOLSETTING(SOCKS_RESOLVE);
		return true;
	}
	proxy.host.clear();
	proxy.port = 0;
	proxy.user.clear();
	proxy.password.clear();
	proxy.resolveNames = false;
	return false;
}

void Socket::createControlEvent()
{
	controlEvent.create();
}

void Socket::signalControlEvent()
{
	if (!controlEvent.empty())
		controlEvent.notify();
}

void Socket::setBlocking(bool block) noexcept
{
#ifdef _WIN32
	u_long b = block ? 0 : 1;
	ioctlsocket(sock, FIONBIO, &b);
#else
	int newFlags, flags = fcntl(sock, F_GETFL, 0);
	if (block)
		newFlags = flags & ~O_NONBLOCK;
	else
		newFlags = flags | O_NONBLOCK;
	if (newFlags != flags)
		fcntl(sock, F_SETFL, newFlags);
#endif
}

void Socket::printSockName(string& s) const
{
	s = "Socket " + Util::toHexString(sock);
}
