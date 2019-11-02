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

SocketException::SocketException(DWORD aError) noexcept
	:
	Exception("SocketException: " + errorToString(aError))
{
	m_error_code = aError;
	dcdebug("Thrown: %s\n", what()); //-V111
}

#else // _DEBUG

SocketException::SocketException(DWORD aError) noexcept :
	Exception(errorToString(aError))
{
	m_error_code = aError;
}

#endif

Socket::Stats Socket::g_stats;

const uint64_t SOCKS_TIMEOUT = 30000;

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

void Socket::create(SocketType aType /* = TYPE_TCP */)
{
	if (sock != INVALID_SOCKET)
		disconnect();
		
	switch (aType)
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
	type = aType;
	setBlocking(false);
}

uint16_t Socket::accept(const Socket& listeningSocket)
{
	bool doLog = BOOLSETTING(LOG_SYSTEM);
	
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
	
	const string remoteIp = inet_ntoa(sockAddr.sin_addr);
	IpGuard::check_ip_str(remoteIp, this);
	// Make sure we disable any inherited windows message things for this socket.
	::WSAAsyncSelect(sock, NULL, 0, 0);
	
	type = TYPE_TCP;
	
	uint16_t port = ntohs(sockAddr.sin_port);
	if (doLog)
		LogManager::message("Socket " + Util::toHexString(listeningSocket.sock) +
			": Accepted connection from " + remoteIp + ":" + Util::toString(port), false);
	
	// remote IP
	setIp(remoteIp);
	connected = true;
	setBlocking(false);
	
	// return the remote port
	return port;
}

uint16_t Socket::bind(uint16_t port, const string& address /* = 0.0.0.0 */)
{
	bool doLog = BOOLSETTING(LOG_SYSTEM);

	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(port);
	sockAddr.sin_addr.s_addr = inet_addr(address.c_str());

	if (::bind(sock, (sockaddr *) &sockAddr, sizeof(sockAddr)) == SOCKET_ERROR)
	{
		if (doLog)
			LogManager::message("Socket " + Util::toHexString(sock) +
				": Error #" + Util::toString(getLastError()) +
				" binding to " + address + ":" + Util::toString(port) + ", retrying with INADDR_ANY", false);
		sockAddr.sin_port = 0;
		sockAddr.sin_addr.s_addr = INADDR_ANY;
		if (::bind(sock, (sockaddr *) &sockAddr, sizeof(sockAddr)) == SOCKET_ERROR)
		{
			if (doLog)
				LogManager::message("Socket " + Util::toHexString(sock) +
					": Error #" + Util::toString(getLastError()) +
					" binding to " + address + ":0", false);
			throw SocketException(getLastError());
		}
	}

	uint16_t localPort = getLocalPort();
	if (doLog)
		LogManager::message("Socket " + Util::toHexString(sock) +
			": Bound to " + address + ":" + Util::toString(localPort) +
			", type=" + Util::toString(type) + ", secure=" + Util::toString(isSecure()), false);
	return localPort;
}

void Socket::listen()
{
	check(::listen(sock, 20));
	connected = true;
}

void Socket::connect(const string& host, uint16_t port)
{
	bool doLog = BOOLSETTING(LOG_SYSTEM);
	if (sock == INVALID_SOCKET)
		create(TYPE_TCP);

	if (doLog)
		LogManager::message("Socket " + Util::toHexString(sock) + ": Connecting to " +
			host + ":" + Util::toString(port) + ", secure=" + Util::toString(isSecure()), false);
	
	const string resolvedAddress = resolve(host);
	if (doLog && resolvedAddress != host)
		if (resolvedAddress.empty())
			LogManager::message("Socket " + Util::toHexString(sock) + ": Error resolving " + host, false);
		else
			LogManager::message("Socket " + Util::toHexString(sock) + ": Host " + host + " resolved to " + resolvedAddress, false);

	if (resolvedAddress.empty())
		throw SocketException(STRING(RESOLVE_FAILED));
	
	string reason;
	if (IpGuard::check_ip_str(resolvedAddress, reason))
		throw SocketException(STRING(IPGUARD_BLOCK_LIST) + ": (" + host + ") :" + reason);

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(resolvedAddress.c_str());

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
	
	connected = true;
	setIp(resolvedAddress);
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

void Socket::socksConnect(const string& aAddr, uint16_t aPort, uint64_t timeout)
{
	if (SETTING(SOCKS_SERVER).empty() || SETTING(SOCKS_PORT) == 0)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
	
	// FIXME: resolve called!
	string l_reason;
	if (IpGuard::check_ip_str(resolve(aAddr), l_reason))
	{
		throw SocketException(STRING(IPGUARD_BLOCK_LIST) + ": (" + aAddr + ") :" + l_reason);
	}
	
	uint64_t start = GET_TICK();
	
	connect(SETTING(SOCKS_SERVER), static_cast<uint16_t>(SETTING(SOCKS_PORT)));
	
	if (wait(timeLeft(start, timeout), WAIT_CONNECT) != WAIT_CONNECT)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
	
	socksAuth(timeLeft(start, timeout));
	
	ByteVector connStr;
	
	// Authenticated, let's get on with it...
	connStr.push_back(5);           // SOCKSv5
	connStr.push_back(1);           // Connect
	connStr.push_back(0);           // Reserved
	
	if (BOOLSETTING(SOCKS_RESOLVE))
	{
		connStr.push_back(3);       // Address type: domain name
		connStr.push_back((uint8_t)aAddr.size());
		connStr.insert(connStr.end(), aAddr.begin(), aAddr.end());
	}
	else
	{
		connStr.push_back(1);       // Address type: IPv4;
		const unsigned long addr = inet_addr(resolve(aAddr).c_str());
		uint8_t* paddr = (uint8_t*) & addr;
		connStr.insert(connStr.end(), paddr, paddr + 4); //-V112
	}
	
	uint16_t l_port = htons(aPort);
	uint8_t* pport = (uint8_t*) & l_port;
	connStr.push_back(pport[0]);
	connStr.push_back(pport[1]);
	
	writeAll(&connStr[0], static_cast<int>(connStr.size()), timeLeft(start, timeout)); // [!] PVS V107 Implicit type conversion second argument 'connStr.size()' of function 'writeAll' to 32-bit type. socket.cpp 254
	
	// We assume we'll get a ipv4 address back...therefore, 10 bytes...
	/// @todo add support for ipv6
	if (readAll(&connStr[0], 10, timeLeft(start, timeout)) != 10)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
	
	if (connStr[0] != 5 || connStr[1] != 0)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
	
	in_addr sock_addr;
	
	memzero(&sock_addr, sizeof(sock_addr));
	sock_addr.s_addr = *((unsigned long*) & connStr[4]);
	setIp(inet_ntoa(sock_addr));
}

void Socket::socksAuth(uint64_t timeout)
{
	vector<uint8_t> connStr;
	
	uint64_t start = GET_TICK();
	
	if (SETTING(SOCKS_USER).empty() && SETTING(SOCKS_PASSWORD).empty())
	{
		// No username and pw, easier...=)
		connStr.push_back(5);           // SOCKSv5
		connStr.push_back(1);           // 1 method
		connStr.push_back(0);           // Method 0: No auth...
		
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
		
		connStr.push_back(5);           // SOCKSv5
		connStr.push_back(1);           // 1 method
		connStr.push_back(2);           // Method 2: Name/Password...
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
		connStr.push_back((uint8_t)SETTING(SOCKS_USER).length());
		connStr.insert(connStr.end(), SETTING(SOCKS_USER).begin(), SETTING(SOCKS_USER).end());
		connStr.push_back((uint8_t)SETTING(SOCKS_PASSWORD).length());
		connStr.insert(connStr.end(), SETTING(SOCKS_PASSWORD).begin(), SETTING(SOCKS_PASSWORD).end());
		
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
		const int l_sockInBuf = SETTING(SOCKET_IN_BUFFER);
		if (l_sockInBuf > 0)
			setSocketOpt(SO_RCVBUF, l_sockInBuf);
	}
}

void Socket::setOutBufSize()
{
	if (!CompatibilityManager::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
	{
		const int l_sockOutBuf = SETTING(SOCKET_OUT_BUFFER);
		if (l_sockOutBuf > 0)
			setSocketOpt(SO_SNDBUF, l_sockOutBuf);
	}
}

void Socket::setSocketOpt(int option, int val)
{
	dcassert(val > 0);
	int len = sizeof(val); // x64 - x86 int разный размер
	check(::setsockopt(sock, SOL_SOCKET, option, (char*)&val, len)); // [2] https://www.box.net/shared/57976d5de875f5b33516
}

int Socket::read(void* aBuffer, int aBufLen)
{
	int len = 0;
	
	dcassert(type == TYPE_TCP || type == TYPE_UDP);
	dcassert(sock != INVALID_SOCKET);
	
	if (type == TYPE_TCP)
	{
#ifdef _WIN32
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
			g_stats.m_udp.totalDown += len;
		else
			g_stats.m_tcp.totalDown += len;
	}
	
	return len;
}

int Socket::read(void* aBuffer, int aBufLen, sockaddr_in &remote)
{
	dcassert(type == TYPE_UDP);
	dcassert(sock != INVALID_SOCKET);
	
	sockaddr_in remote_addr = { 0 };
	socklen_t addr_length = sizeof(remote_addr);
	
	int len = 0;
#ifdef _WIN32
	len = ::recvfrom(sock, (char*)aBuffer, aBufLen, 0, (struct sockaddr*) & remote_addr, &addr_length);
#else
	do
	{
		len = ::recvfrom(sock, (char*)aBuffer, aBufLen, 0, (struct sockaddr*) & remote_addr, &addr_length);
	}
	while (len < 0 && getLastError() == EINTR);
#endif
	
	check(len, true);
	if (len > 0)
	{
		if (type == TYPE_UDP)
			g_stats.m_udp.totalDown += len;
		else
			g_stats.m_tcp.totalDown += len;
	}
	remote = remote_addr;
	
	return len;
}

int Socket::readAll(void* aBuffer, int aBufLen, uint64_t timeout)
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

int Socket::writeAll(const void* aBuffer, int aLen, uint64_t timeout)
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
			g_stats.m_udp.totalUp += sent;
		else
			g_stats.m_tcp.totalUp += sent;
	}
	return sent;
}

/**
* Sends data, will block until all data has been sent or an exception occurs
* @param aBuffer Buffer with data
* @param aLen Data length
* @throw SocketExcpetion Send failed.
*/
int Socket::writeTo(const string& aAddr, uint16_t aPort, const void* aBuffer, int aLen, bool proxy)
{
	if (aLen <= 0)
	{
		dcassert(0);
		return 0;
	}
	
	uint8_t* buf = (uint8_t*)aBuffer;
	if (sock == INVALID_SOCKET)
	{
		create(TYPE_UDP);
		setSocketOpt(SO_SNDTIMEO, 250);
	}
	dcassert(type == TYPE_UDP);
	
	if (aAddr.empty() || aPort == 0)
	{
		//dcassert(0);
		throw SocketException(EADDRNOTAVAIL);
	}
	
	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	
	int sent;
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5 && proxy)
	{
		if (g_udpServer.empty() || g_udpPort == 0)
		{
			static bool g_is_first = false;
			if (!g_is_first)
			{
				// TODO g_is_first = true;
				Socket::socksUpdated();
			}
		}
		if (g_udpServer.empty() || g_udpPort == 0)
		{
			throw SocketException(STRING(SOCKS_SETUP_ERROR));
		}
		
		sockAddr.sin_port = htons(g_udpPort);
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = inet_addr(g_udpServer.c_str());		
		
		vector<uint8_t> connStr;
		unsigned long addr;
		connStr.reserve(24 + static_cast<size_t>(aLen)); // [!] PVS V106 Implicit type conversion first argument '20 + aLen' of function 'reserve' to memsize type. socket.cpp 570
		
		connStr.push_back(0);       // Reserved
		connStr.push_back(0);       // Reserved
		connStr.push_back(0);       // Fragment number, always 0 in our case...
		
		if (BOOLSETTING(SOCKS_RESOLVE))
		{
			connStr.push_back(3);
			connStr.push_back((uint8_t)aAddr.size());
			connStr.insert(connStr.end(), aAddr.begin(), aAddr.end());
		}
		else
		{
			connStr.push_back(1);       // Address type: IPv4;
			addr = inet_addr(resolve(aAddr).c_str());
			uint8_t* paddr = (uint8_t*) & addr;
			connStr.insert(connStr.end(), paddr, paddr + 4);
		}
		
		connStr.insert(connStr.end(), buf, buf + static_cast<size_t>(aLen));
		
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
		sockAddr.sin_port = htons(aPort);
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = inet_addr(resolve(aAddr).c_str());
#ifdef _WIN32
		sent = ::sendto(sock, (const char*)aBuffer, aLen, 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
#else
		do
		{
			sent = ::sendto(sock, (const char*)aBuffer, aLen, 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
		}
		while (sent < 0 && getLastError() == EINTR);
#endif
	}
	
	check(sent);
	if (type == TYPE_UDP)
		g_stats.m_udp.totalUp += sent;
	else
		g_stats.m_tcp.totalUp += sent;
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
int Socket::wait(uint64_t millis, int waitFor)
{
	timeval tv;
	fd_set rfd, wfd, efd;
	fd_set *rfdp = nullptr, *wfdp = nullptr;
	tv.tv_sec = static_cast<long>(millis / 1000);// [!] IRainman fix this fild in timeval is a long type (PVS TODO)
	tv.tv_usec = static_cast<long>((millis % 1000) * 1000);// [!] IRainman fix this fild in timeval is a long type (PVS TODO)
	
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
		if (waitFor & WAIT_READ)
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
		
		result = select((int)(sock + 1), rfdp, wfdp, NULL, &tv); //[1] https://www.box.net/shared/03ae4d0b4586cea0a305
	}
	while (result < 0 && getLastError() == EINTR);
	check(result);
	
	waitFor = WAIT_NONE;
	
	//dcassert(sock != INVALID_SOCKET); // https://github.com/eiskaltdcpp/eiskaltdcpp/commit/b031715
	if (sock != INVALID_SOCKET)
	{
		if (rfdp && FD_ISSET(sock, rfdp)) // https://www.box.net/shared/t3apqdurqxzicy4bg1h0
		{
			waitFor |= WAIT_READ;
		}
		if (wfdp && FD_ISSET(sock, wfdp))
		{
			waitFor |= WAIT_WRITE;
		}
	}
	return waitFor;
}

bool Socket::waitConnected(uint64_t millis)
{
	return wait(millis, Socket::WAIT_CONNECT) == WAIT_CONNECT;
}

bool Socket::waitAccepted(uint64_t /*millis*/)
{
	// Normal sockets are always connected after a call to accept
	return true;
}

// FIXME: must return a uint32_t
string Socket::resolve(const string& host) noexcept
{
	unsigned long address = inet_addr(host.c_str());
	if (address != INADDR_NONE) return host;
	const hostent* he = gethostbyname(host.c_str());
	if (!(he && he->h_addr)) return string();
	in_addr addr = *(const in_addr *) he->h_addr;
	return inet_ntoa(addr);
}

string Socket::getDefaultGateWay(boost::logic::tribool& p_is_wifi_router)
{
	p_is_wifi_router = false;
	in_addr l_addr = {0};
	MIB_IPFORWARDROW ip_forward = {0};
	memset(&ip_forward, 0, sizeof(ip_forward));
	if (GetBestRoute(inet_addr("0.0.0.0"), 0, &ip_forward) == NO_ERROR)
	{
		l_addr = *(in_addr*)&ip_forward.dwForwardNextHop;
		const string l_ip_gateway = inet_ntoa(l_addr);
		if (l_ip_gateway == "192.168.1.1" ||
		        l_ip_gateway == "192.168.0.1" ||
		        l_ip_gateway == "192.168.88.1" || // http://www.lan23.ru/FAQ-Mikrotik-RouterOS-part1.html
		        l_ip_gateway == "192.168.33.1" || // http://www.speedguide.net/routers/huawei-ws323-300mbps-dual-band-wireless-range-extender-2951&print=friendly
		        l_ip_gateway == "192.168.0.50"    // http://nastroisam.ru/dap-1360-d1/#more-7437
		   )
		{
			p_is_wifi_router = true;
		}
		return l_ip_gateway;
	}
	return "";
	
#if 0
	// Get local host name
	char szHostName[128] = {0};
	
	if (gethostname(szHostName, sizeof(szHostName)))
	{
		// Error handling -> call 'WSAGetLastError()'
	}
	
	SOCKADDR_IN socketAddress;
	hostent *pHost        = 0;
	
	// Try to get the host ent
	pHost = gethostbyname(szHostName);
	if (!pHost)
	{
		// Error handling -> call 'WSAGetLastError()'
	}
	
	char ppszIPAddresses[10][16]; // maximum of ten IP addresses
	for (int iCnt = 0; (pHost->h_addr_list[iCnt]) && (iCnt < 10); ++iCnt)
	{
		memcpy(&socketAddress.sin_addr, pHost->h_addr_list[iCnt], pHost->h_length);
		strcpy(ppszIPAddresses[iCnt], inet_ntoa(socketAddress.sin_addr));
		
		//printf("Found interface address: %s\n", ppszIPAddresses[iCnt]);
	}
	return "";
#endif
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
	uint16_t p_port = 0;
	string p_ip;
	getLocalIPPort(p_port, p_ip, false);
	return p_port;
}

void Socket::socksUpdated()
{
	g_udpServer.clear();
	g_udpPort = 0;
	
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)
	{
		try
		{
			Socket s;
			s.setBlocking(false);
			s.connect(SETTING(SOCKS_SERVER), static_cast<uint16_t>(SETTING(SOCKS_PORT)));
			s.socksAuth(SOCKS_TIMEOUT);
			
			char connStr[10];
			connStr[0] = 5;         // SOCKSv5
			connStr[1] = 3;         // UDP Associate
			connStr[2] = 0;         // Reserved
			connStr[3] = 1;         // Address type: IPv4;
			*(unsigned long*) &connStr[4] = 0;  // No specific outgoing UDP address // [!] IRainman fix. this value unsigned!
			*(uint16_t*) &connStr[8] = 0;    // No specific port...
			
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
	//dcassert(sock != INVALID_SOCKET);
	if (sock != INVALID_SOCKET)
	{
		::shutdown(sock, SD_BOTH); // !DC++!
	}
}

void Socket::close() noexcept
{
	//dcassert(sock != INVALID_SOCKET);
	if (sock != INVALID_SOCKET)
	{
		::closesocket(sock);
		connected = false;
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
	
	hostent *h = gethostbyaddr(reinterpret_cast<const char *>(&addr), 4, AF_INET); //-V112
	dcassert(h);
	if (h == nullptr)
	{
		return Util::emptyString;
	}
	else
	{
		return h->h_name;
	}
}
