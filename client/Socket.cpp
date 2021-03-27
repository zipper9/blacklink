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
#include "CompatibilityManager.h"
#include "SettingsManager.h"
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

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

const unsigned SOCKS_TIMEOUT = 30000;

string SocketException::errorToString(int aError) noexcept
{
	string msg = Util::translateError(aError);
	if (msg.empty())
	{
		char tmp[64];
		tmp[0] = 0;
		_snprintf(tmp, _countof(tmp), CSTRING(UNKNOWN_ERROR), aError);
		msg = tmp;
	}
	
	return msg;
}

socket_t Socket::getSock() const
{
	return sock;
}

void Socket::create(SocketType type /* = TYPE_TCP */)
{
	if (sock != INVALID_SOCKET)
		disconnect();
		
	switch (type)
	{
		case TYPE_TCP:
			sock = checksocket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
			break;
		case TYPE_UDP:
			sock = checksocket(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
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
	
	if (sock != INVALID_SOCKET)
	{
		disconnect();
	}
	
	sockaddr_in sockAddr;
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
			LogManager::message("Socket " + Util::toHexString(listeningSocket.sock) +
				": Accept error #" + Util::toString(getLastError()), false);
		throw SocketException(getLastError());
	}
	
	const Ip4Address remoteIp = ntohl(sockAddr.sin_addr.s_addr);
	if (BOOLSETTING(ENABLE_IPGUARD) && ipGuard.isBlocked(ntohl(sockAddr.sin_addr.s_addr)))		
		throw SocketException(STRING_F(IP_BLOCKED, "IPGuard" % Util::printIpAddress(remoteIp)));

	// Make sure we disable any inherited windows message things for this socket.
	::WSAAsyncSelect(sock, NULL, 0, 0);
	
	type = TYPE_TCP;
	
	uint16_t port = ntohs(sockAddr.sin_port);
	if (doLog)
		LogManager::message("Socket " + Util::toHexString(listeningSocket.sock) +
			": Accepted connection from " + Util::printIpAddress(remoteIp) + ":" + Util::toString(port), false);
	
	// remote IP
	setIp4(remoteIp);
	setBlocking(false);
	
	// return the remote port
	return port;
}

uint16_t Socket::bind(uint16_t port, const string& address /* = 0.0.0.0 */)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);

	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(port);
	sockAddr.sin_addr.s_addr = address.empty() ? INADDR_ANY : inet_addr(address.c_str());

	if (::bind(sock, (sockaddr *) &sockAddr, sizeof(sockAddr)) == SOCKET_ERROR)
	{
		if (sockAddr.sin_addr.s_addr == INADDR_ANY)
		{
			if (doLog)
				LogManager::message("Socket " + Util::toHexString(sock) +
					": Error #" + Util::toString(getLastError()) +
					" binding to " + address + ":" + Util::toString(port), false);
			throw SocketException(getLastError());
		}
		if (doLog)
			LogManager::message("Socket " + Util::toHexString(sock) +
				": Error #" + Util::toString(getLastError()) +
				" binding to " + address + ":" + Util::toString(port) + ", retrying with INADDR_ANY", false);
		sockAddr.sin_addr.s_addr = INADDR_ANY;
		if (::bind(sock, (sockaddr *) &sockAddr, sizeof(sockAddr)) == SOCKET_ERROR)
		{
			if (doLog)
				LogManager::message("Socket " + Util::toHexString(sock) +
					": Error #" + Util::toString(getLastError()) +
					" binding to 0.0.0.0:" + Util::toString(port), false);
			throw SocketException(getLastError());
		}
	}

	uint16_t localPort = getLocalPort();
	if (doLog)
		LogManager::message("Socket " + Util::toHexString(sock) +
			": Bound to " + address + ":" + Util::toString(localPort) +
			", type=" + Util::toString(type) + ", secureTransport=" + Util::toString(getSecureTransport()), false);
	return localPort;
}

void Socket::listen()
{
	check(::listen(sock, 20));
}

void Socket::connect(const string& host, uint16_t port)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	if (sock == INVALID_SOCKET)
		create(TYPE_TCP);

	if (doLog)
		LogManager::message("Socket " + Util::toHexString(sock) + ": Connecting to " +
			host + ":" + Util::toString(port) + ", secureTransport=" + Util::toString(getSecureTransport()), false);
	
	bool isNumeric;
	Ip4Address address = resolveHost(host, &isNumeric);
	if (doLog && !isNumeric)
		if (!address)
			LogManager::message("Socket " + Util::toHexString(sock) + ": Error resolving " + host, false);
		else
			LogManager::message("Socket " + Util::toHexString(sock) + ": Host " + host + " resolved to " + Util::printIpAddress(address), false);

	if (!address)
		throw SocketException(STRING(RESOLVE_FAILED));

	if (BOOLSETTING(ENABLE_IPGUARD) && ipGuard.isBlocked(address))
	{
		string error = STRING_F(IP_BLOCKED, "IPGuard" % Util::printIpAddress(address));
		if (!isNumeric) error += " (" + host + ")";
		throw SocketException(error);
	}

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address);

	int result;
#ifdef _WIN32
	result = ::connect(sock, (struct sockaddr*) &addr, sizeof(addr));
#else
	do
	{
		result = ::connect(sock, (struct sockaddr*) &addr, sizeof(addr));
	}
	while (result < 0 && getLastError() == EINTR);
#endif
	check(result, true);
	
	setIp4(address);
	setPort(port);
}

static uint64_t timeLeft(uint64_t start, uint64_t timeout)
{
	if (timeout == 0)
	{
		return 0;
	}
	uint64_t now = GET_TICK();
	if (start + timeout < now)
		throw SocketException(STRING(CONNECTION_TIMEOUT));
	return start + timeout - now;
}

void Socket::socksConnect(const ProxyConfig& proxy, const string& host, uint16_t port, unsigned timeout)
{
	if (proxy.host.empty() || proxy.port == 0)
		throw SocketException(STRING(SOCKS_FAILED));
	
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	bool resolved = false;
	Ip4Address address = 0;
	if (BOOLSETTING(ENABLE_IPGUARD))
	{
		bool isNumeric;
		address = resolveHost(host, &isNumeric);
		if (doLog && !isNumeric)
			if (!address)
				LogManager::message("Socket " + Util::toHexString(sock) + ": Error resolving " + host, false);
			else
				LogManager::message("Socket " + Util::toHexString(sock) + ": Host " + host + " resolved to " + Util::printIpAddress(address), false);
		if (!address)
			throw SocketException(STRING(RESOLVE_FAILED));
		if (ipGuard.isBlocked(address))
		{
			string error = STRING_F(IP_BLOCKED, "IPGuard" % Util::printIpAddress(address));
			if (!isNumeric) error += " (" + host + ")";
			throw SocketException(error);
		}
		resolved = true;
	}

	uint64_t start = GET_TICK();
	connect(proxy.host, proxy.port);
	if (wait(timeLeft(start, timeout), WAIT_CONNECT) != WAIT_CONNECT)
		throw SocketException(STRING(SOCKS_FAILED));
	
	socksAuth(proxy, timeLeft(start, timeout));
	
	ByteVector connStr;
	connStr.push_back(5); // SOCKSv5
	connStr.push_back(1); // Connect
	connStr.push_back(0); // Reserved
	
	if (proxy.resolveNames && !resolved && host.length() <= 255)
	{
		connStr.push_back(3); // Address type: domain name
		connStr.push_back((uint8_t) host.length());
		connStr.insert(connStr.end(), host.begin(), host.end());
	}
	else
	{
		if (!resolved)
		{
			bool isNumeric;
			address = resolveHost(host, &isNumeric);
			if (doLog && !isNumeric)
				if (!address)
					LogManager::message("Socket " + Util::toHexString(sock) + ": Error resolving " + host, false);
				else
					LogManager::message("Socket " + Util::toHexString(sock) + ": Host " + host + " resolved to " + Util::printIpAddress(address), false);
			if (!address)
				throw SocketException(STRING(RESOLVE_FAILED));
		}
		connStr.push_back(1); // Address type: IPv4
		connStr.push_back((uint8_t) (address >> 24));
		connStr.push_back((uint8_t) (address >> 16));
		connStr.push_back((uint8_t) (address >> 8));
		connStr.push_back((uint8_t) address);
	}
	
	connStr.push_back(port >> 8);
	connStr.push_back(port & 0xFF);
	
	writeAll(&connStr[0], static_cast<int>(connStr.size()), timeLeft(start, timeout));
	
	// We assume we'll get a ipv4 address back...therefore, 10 bytes...
	/// @todo add support for ipv6
	if (readAll(&connStr[0], 10, timeLeft(start, timeout)) != 10)
		throw SocketException(STRING(SOCKS_FAILED));
	
	if (connStr[0] != 5 || connStr[1] != 0)
		throw SocketException(STRING(SOCKS_FAILED));
	
	setIp4(address);
	setPort(port);
}

void Socket::socksAuth(const ProxyConfig& proxy, unsigned timeout)
{
	vector<uint8_t> connStr;
	
	uint64_t start = GET_TICK();
	
	if (proxy.user.empty() && proxy.password.empty())
	{
		// No username and pw, easier...=)
		connStr.push_back(5); // SOCKSv5
		connStr.push_back(1); // 1 method
		connStr.push_back(0); // Method 0: No auth...
		
		writeAll(&connStr[0], 3, timeLeft(start, timeout));
		
		if (readAll(&connStr[0], 2, timeLeft(start, timeout)) != 2)
		{
			throw SocketException(STRING(SOCKS_FAILED));
		}
		
		if (connStr[1] != 0)
		{
			throw SocketException(STRING(SOCKS_NEEDS_AUTH));
		}
	}
	else
	{
		// We try the username and password auth type (no, we don't support gssapi)
		
		connStr.push_back(5); // SOCKSv5
		connStr.push_back(1); // 1 method
		connStr.push_back(2); // Method 2: Name/Password...
		writeAll(&connStr[0], 3, timeLeft(start, timeout));
		
		if (readAll(&connStr[0], 2, timeLeft(start, timeout)) != 2)
		{
			throw SocketException(STRING(SOCKS_FAILED));
		}
		if (connStr[1] != 2)
		{
			throw SocketException(STRING(SOCKS_AUTH_UNSUPPORTED));
		}
		
		connStr.clear();
		// Now we send the username / pw...
		connStr.push_back(1);
		connStr.push_back((uint8_t) proxy.user.length());
		connStr.insert(connStr.end(), proxy.user.begin(), proxy.user.end());
		connStr.push_back((uint8_t) proxy.password.length());
		connStr.insert(connStr.end(), proxy.password.begin(), proxy.password.end());
		
		writeAll(&connStr[0], static_cast<int>(connStr.size()), timeLeft(start, timeout));
		
		if (readAll(&connStr[0], 2, timeLeft(start, timeout)) != 2 || connStr[1] != 0)
		{
			throw SocketException(STRING(SOCKS_AUTH_FAILED));
		}
	}
}

int Socket::getSocketOptInt(int option) const
{
	int val = 0;
	socklen_t len = sizeof(val);
	check(::getsockopt(sock, SOL_SOCKET, option, (char*)&val, &len)); // [2] https://www.box.net/shared/3ad49dfa7f44028a7467
	/* [-] IRainman fix:
	Please read http://msdn.microsoft.com/en-us/library/windows/desktop/ms740532(v=vs.85).aspx
	and explain on what basis to audit  has been added to the magic check l_val <= 0,
	and the error in the log is sent to another condition l_val == 0.
	Just ask them to explain on what basis we do not trust the system,
	and why the system could restore us to waste in these places, but the api does not contain the test function of range.
	Once again I ask, please - if that's where you fell, you have to find real bugs in our code and not to mask them is not clear what the basis of checks.
	  [-] if (l_val <= 0)
	  [-]    throw SocketException("[Error] getSocketOptInt() <= 0");
	  [~] IRainman fix */
	return val;
}

void Socket::setInBufSize()
{
	if (!CompatibilityManager::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
	{
		const int sockInBuf = SETTING(SOCKET_IN_BUFFER);
		if (sockInBuf > 0)
			setSocketOpt(SO_RCVBUF, sockInBuf);
	}
}

void Socket::setOutBufSize()
{
	if (!CompatibilityManager::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
	{
		const int sockOutBuf = SETTING(SOCKET_OUT_BUFFER);
		if (sockOutBuf > 0)
			setSocketOpt(SO_SNDBUF, sockOutBuf);
	}
}

void Socket::setSocketOpt(int option, int val)
{
	dcassert(val > 0);
	int len = sizeof(val);
	check(::setsockopt(sock, SOL_SOCKET, option, (char*)&val, len));
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

int Socket::readPacket(void* aBuffer, int aBufLen, sockaddr_in &remote)
{
	dcassert(type == TYPE_UDP);
	dcassert(sock != INVALID_SOCKET);
	
	sockaddr_in remote_addr = { 0 };
	socklen_t addr_length = sizeof(remote_addr);
	
	int len = 0;
#ifdef _WIN32
	lastWaitResult &= ~WAIT_READ;
	len = ::recvfrom(sock, (char*)aBuffer, aBufLen, 0, (struct sockaddr*) &remote_addr, &addr_length);
#else
	do
	{
		len = ::recvfrom(sock, (char*)aBuffer, aBufLen, 0, (struct sockaddr*) &remote_addr, &addr_length);
	}
	while (len < 0 && getLastError() == EINTR);
#endif
	
	check(len, true);
	if (len > 0)
		g_stats.udp.downloaded += len;
	remote = remote_addr;
	
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
	const int sendSize = getSocketOptInt(SO_SNDBUF);
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

int Socket::write(const void* aBuffer, int aLen)
{
	dcassert(sock != INVALID_SOCKET);
	
	int sent = 0;
#ifdef _WIN32
	sent = ::send(sock, (const char*)aBuffer, aLen, 0);
#else
	do
	{
		sent = ::send(sock, (const char*)aBuffer, aLen, 0);
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

int Socket::writeTo(const string& host, uint16_t port, const void* buffer, int len, bool useProxy)
{
	if (len <= 0)
	{
		dcassert(0);
		return 0;
	}
	
	const uint8_t* buf = static_cast<const uint8_t*>(buffer);
	if (sock == INVALID_SOCKET)
	{
		create(TYPE_UDP);
		setSocketOpt(SO_SNDTIMEO, 250);
	}
	dcassert(type == TYPE_UDP);
	
	if (host.empty() || port == 0)
		throw SocketException(EADDRNOTAVAIL);

	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	
	int sent;
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5 && useProxy)
	{
		ProxyConfig proxy;
		getProxyConfig(proxy);
		if (g_udpServer.empty() || g_udpPort == 0)
		{
			Socket::socksUpdated(&proxy);
		}
		if (g_udpServer.empty() || g_udpPort == 0)
		{
			throw SocketException(STRING(SOCKS_SETUP_ERROR));
		}
		
		sockAddr.sin_port = htons(g_udpPort);
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = inet_addr(g_udpServer.c_str());		
		
		vector<uint8_t> connStr;
		connStr.reserve(24 + len);
		
		connStr.push_back(0); // Reserved
		connStr.push_back(0); // Reserved
		connStr.push_back(0); // Fragment number, always 0 in our case...
		
		if (proxy.resolveNames && host.length() <= 255)
		{
			connStr.push_back(3);
			connStr.push_back((uint8_t) host.length());
			connStr.insert(connStr.end(), host.begin(), host.end());
		}
		else
		{
			Ip4Address address = resolveHost(host);
			if (!address)
				throw SocketException(STRING(RESOLVE_FAILED));
			connStr.push_back(1); // Address type: IPv4
			connStr.push_back((uint8_t) (address >> 24));
			connStr.push_back((uint8_t) (address >> 16));
			connStr.push_back((uint8_t) (address >> 8));
			connStr.push_back((uint8_t) address);
		}
		
		connStr.insert(connStr.end(), buf, buf + len);
		
#ifdef _WIN32
		sent = ::sendto(sock, (const char*) &connStr[0], static_cast<int>(connStr.size()), 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
#else
		do
		{
			sent = ::sendto(sock, (const char*) &connStr[0], static_cast<int>(connStr.size()), 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
		}
		while (sent < 0 && getLastError() == EINTR);
#endif
	}
	else
	{
		Ip4Address address = resolveHost(host);
		if (!address)
			throw SocketException(STRING(RESOLVE_FAILED));
		sockAddr.sin_port = htons(port);
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = htonl(address);
#ifdef _WIN32
		sent = ::sendto(sock, (const char*) buffer, len, 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
#else
		do
		{
			sent = ::sendto(sock, (const char*) buffer, len, 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
		}
		while (sent < 0 && getLastError() == EINTR);
#endif
	}
	
	check(sent);
	if (type == TYPE_UDP)
		g_stats.udp.uploaded += sent;
	else
		g_stats.tcp.uploaded += sent;
	return sent;
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
			return waitResult == WAIT_OBJECT_0 ? WAIT_CONTROL : 0;
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
	timeval tv;
	fd_set rfd, wfd, efd;
	fd_set *rfdp = nullptr, *wfdp = nullptr;
	tv.tv_sec = static_cast<long>(millis / 1000);
	tv.tv_usec = static_cast<long>(millis % 1000) * 1000;
	
	if (waitFor & WAIT_CONNECT)
	{
		dcassert(!(waitFor & WAIT_READ) && !(waitFor & WAIT_WRITE));
		
		int result = -1;
		do
		{
			FD_ZERO(&wfd);
			FD_ZERO(&efd);
			
			FD_SET(sock, &wfd);
			FD_SET(sock, &efd);
			result = select((int)(sock + 1), 0, &wfd, &efd, &tv);
		}
		while (result < 0 && getLastError() == EINTR);
		check(result);
		
		if (FD_ISSET(sock, &wfd))
		{
			return WAIT_CONNECT;
		}
		
		if (FD_ISSET(sock, &efd))
		{
			int y = 0;
			socklen_t z = sizeof(y);
			check(getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&y, &z));
			
			if (y != 0)
				throw SocketException(y);
			// No errors! We're connected (?)...
			return WAIT_CONNECT;
		}
		return 0;
	}
	
	int result = -1;
	do
	{
		if (waitFor & (WAIT_READ | WAIT_ACCEPT))
		{
			dcassert(!(waitFor & WAIT_CONNECT));
			rfdp = &rfd;
			FD_ZERO(rfdp);
			FD_SET(sock, rfdp);
		}
		if (waitFor & WAIT_WRITE)
		{
			dcassert(!(waitFor & WAIT_CONNECT));
			wfdp = &wfd;
			FD_ZERO(wfdp);
			FD_SET(sock, wfdp);
		}
		
		result = select((int)(sock + 1), rfdp, wfdp, NULL, &tv);
	}
	while (result < 0 && getLastError() == EINTR);
	check(result);

	result = WAIT_NONE;
	if (rfdp && FD_ISSET(sock, rfdp))
		result |= (waitFor & WAIT_ACCEPT) ? WAIT_ACCEPT : WAIT_READ;
	if (wfdp && FD_ISSET(sock, wfdp))
		result |= WAIT_WRITE;
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

Ip4Address Socket::resolveHost(const string& host, bool* isNumeric) noexcept
{
	Ip4Address result;
	if (Util::parseIpAddress(result, host))
	{
		if (isNumeric) *isNumeric = true;
		return result;
	}
	if (isNumeric) *isNumeric = false;
	const hostent* he = gethostbyname(host.c_str());
	if (!(he && he->h_addr)) return 0;
	return ntohl(((in_addr*) he->h_addr)->s_addr);
}

bool Socket::getLocalIPPort(uint16_t& port, string& ip, bool getIp) const
{
	port = 0;
	ip.clear();
	if (sock == INVALID_SOCKET)
	{
		dcassert(sock != INVALID_SOCKET);
		return false;
	}
	sockaddr_in sock_addr = { { 0 } };
	socklen_t len = sizeof(sock_addr);
	if (getsockname(sock, (struct sockaddr*)&sock_addr, &len) == 0)
	{
		if (getIp)
		{
			ip = inet_ntoa(sock_addr.sin_addr);
			dcassert(!ip.empty());
		}
		else
		{
			port = ntohs(sock_addr.sin_port);
			dcassert(port);
		}
		return true;
	}
	else
	{
		const string error = "Socket::getLocalIPPort() error = " + Util::toString(getLastError());
		LogManager::message(error, false);
	}
	dcassert(0);
	return false;
}

uint16_t Socket::getLocalPort() const
{
	uint16_t port;
	string unused;
	getLocalIPPort(port, unused, false);
	return port;
}

void Socket::socksUpdated(const ProxyConfig* proxy)
{
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
}

void Socket::shutdown() noexcept
{
	if (sock != INVALID_SOCKET)
		::shutdown(sock, SD_BOTH);
}

void Socket::close() noexcept
{
	if (sock != INVALID_SOCKET)
	{
		::closesocket(sock);
		sock = INVALID_SOCKET;
	}
}

void Socket::disconnect() noexcept
{
	shutdown();
	close();
}

string Socket::getRemoteHost(const string& aIp)
{
	dcassert(!aIp.empty());
	if (aIp.empty())
		return Util::emptyString;
		
	const unsigned long addr = inet_addr(aIp.c_str());
	
	hostent *h = gethostbyaddr(reinterpret_cast<const char *>(&addr), 4, AF_INET);
	if (h) return h->h_name;
	return Util::emptyString;
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
