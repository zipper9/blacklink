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
#include "Resolver.h"
#include "IpGuard.h"
#include "LogManager.h"
#include "ResourceManager.h"
#include "SettingsManager.h"

#ifdef _WIN32
#include "SysVersion.h"
#define SHUT_RDWR SD_BOTH
#endif

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
		int error = getLastError();
		if (doLog)
			LogManager::message(sockName +
				": Accept error #" + Util::toString(error), false);
		throw SocketException(error);
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

static inline int getBindOptions(int af)
{
	return SettingsManager::get(af == AF_INET6 ? SettingsManager::BIND_OPTIONS6 : SettingsManager::BIND_OPTIONS);
}

uint16_t Socket::bind(uint16_t port, const IpAddressEx& addr)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	string sockName;
	if (doLog) printSockName(sockName);

	sockaddr_u sockAddr;
	socklen_t sockAddrSize;
	toSockAddr(sockAddr, sockAddrSize, addr, port);

	if (addr.type == AF_INET6
#ifdef OSVER_WIN_XP
	    && SysVersion::isOsVistaPlus()
#endif
	    )
		setSocketOpt(IPPROTO_IPV6, IPV6_V6ONLY, 1);

	if (::bind(sock, (sockaddr*) &sockAddr, sockAddrSize) == SOCKET_ERROR)
	{
		int error = getLastError();
		bool anyAddr = isAnyAddr(sockAddr);
		if (anyAddr || (getBindOptions(addr.type) & SettingsManager::BIND_OPTION_NO_FALLBACK) != 0)
		{
			if (doLog)
				LogManager::message(sockName +
					": Error #" + Util::toString(error) +
					" binding to " + (anyAddr ? Util::emptyString : Util::printIpAddress(addr, true)) + ":" + Util::toString(port), false);
			throw SocketException(error);
		}
		if (doLog)
			LogManager::message(sockName +
				": Error #" + Util::toString(error) +
				" binding to " + Util::printIpAddress(addr, true) + ":" + Util::toString(port) + ", retrying with " +
				string(addr.type == AF_INET6 ? "INADDR6_ANY" : "INADDR_ANY"), false);
		setAnyAddr(sockAddr);
		if (::bind(sock, (sockaddr*) &sockAddr, sockAddrSize) == SOCKET_ERROR)
		{
			error = getLastError();
			if (doLog)
				LogManager::message(sockName +
					": Error #" + Util::toString(error) +
					" binding to :" + Util::toString(port), false);
			throw SocketException(error);
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
	check(::listen(sock, SOMAXCONN));
}

void Socket::connect(const string& host, uint16_t port)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	IpAddressEx ip;
	bool isNumeric;
	if (!Resolver::resolveHost(ip, 0, host, &isNumeric))
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
	if (!SysVersion::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
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
	if (!SysVersion::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
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

int Socket::read(void* buffer, int bufLen)
{
	int len = 0;

	dcassert(type == TYPE_TCP || type == TYPE_UDP);
	dcassert(sock != INVALID_SOCKET);

	if (type == TYPE_TCP)
	{
#ifdef _WIN32
		lastWaitResult &= ~WAIT_READ;
		len = ::recv(sock, (char*)buffer, bufLen, 0);
#else
		do
		{
			len = ::recv(sock, (char*)buffer, bufLen, 0);
		}
		while (len < 0 && getLastError() == EINTR);
#endif
	}
	else
	{
#ifdef _WIN32
		len = ::recvfrom(sock, (char*)buffer, bufLen, 0, NULL, NULL);
#else
		do
		{
			len = ::recvfrom(sock, (char*)buffer, bufLen, 0, NULL, NULL);
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
