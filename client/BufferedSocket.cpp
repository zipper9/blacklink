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
#include "BufferedSocket.h"
#include "ThrottleManager.h"
#include "ResourceManager.h"
#include "Streams.h"
#include "ZUtils.h"
#include "CryptoManager.h"
#include "SSLSocket.h"
#include "AutoDetectSocket.h"
#include "LogManager.h"

static const size_t INITIAL_CAPACITY = 8 * 1024;
static const size_t STREAM_BUF_SIZE = 256 * 1024;

static const int POLL_TIMEOUT = 250;
static const int LONG_TIMEOUT = 30000;
static const int SHORT_TIMEOUT = 1000;
static const int DISCONNECT_TIMEOUT = 10000;

#ifdef FLYLINKDC_USE_SOCKET_COUNTER
static std::atomic<int> socketCounter(0);
#endif

BufferedSocket::Buffer::Buffer()
{
	buf = nullptr;
	maxCapacity = 1024 * 1024;
	capacity = 0;
	writePtr = 0;
	readPtr = 0;
}

BufferedSocket::Buffer::~Buffer()
{
	delete[] buf;
}

void BufferedSocket::Buffer::append(const void* data, size_t size)
{
	if (writePtr + size <= capacity)
	{
		memcpy(buf + writePtr, data, size);
		writePtr += size;
		return;
	}
	size_t fill = writePtr - readPtr;
	if (fill + size <= capacity)
	{
		shift();
		memcpy(buf + writePtr, data, size);
		writePtr += size;
		return;
	}
	grow(fill + size);
	memcpy(buf + writePtr, data, size);
	writePtr += size;
}

void BufferedSocket::Buffer::remove(size_t size) noexcept
{
	readPtr += size;
	if (readPtr > writePtr) readPtr = writePtr;
	if (readPtr >= capacity / 2) shift();
}

void BufferedSocket::Buffer::grow(size_t newCapacity)
{
	if (newCapacity > maxCapacity) throw SocketException("Buffer capacity exceeded");
	dcassert(newCapacity > capacity);
	size_t doubleCapacity = min(capacity * 2, maxCapacity);
	if (doubleCapacity < INITIAL_CAPACITY) doubleCapacity = INITIAL_CAPACITY;
	if (doubleCapacity > newCapacity) newCapacity = doubleCapacity;
	uint8_t* newBuf = new uint8_t[newCapacity];
	size_t fill = min(writePtr - readPtr, newCapacity);
	memcpy(newBuf, buf + readPtr, fill);
	delete[] buf;
	buf = newBuf;
	capacity = newCapacity;
	readPtr = 0;
	writePtr = fill;
}

void BufferedSocket::Buffer::grow()
{
	if (capacity == maxCapacity) return;
	size_t doubleCapacity = min(capacity * 2, maxCapacity);
	if (doubleCapacity < INITIAL_CAPACITY) doubleCapacity = INITIAL_CAPACITY;
	uint8_t* newBuf = new uint8_t[doubleCapacity];
	size_t fill = writePtr - readPtr;
	memcpy(newBuf, buf + readPtr, fill);
	delete[] buf;
	buf = newBuf;
	capacity = doubleCapacity;
	readPtr = 0;
	writePtr = fill;
}

void BufferedSocket::Buffer::shift() noexcept
{
	dcassert(readPtr);
	size_t fill = writePtr - readPtr;
	memmove(buf, buf + readPtr, fill);
	readPtr = 0;
	writePtr = fill;
}

void BufferedSocket::Buffer::maybeShift() noexcept
{
	if (readPtr == writePtr) readPtr = writePtr = 0;
	else if (readPtr && readPtr > capacity / 2) shift();
}

void BufferedSocket::Buffer::clear() noexcept
{
	readPtr = writePtr = 0;
}

BufferedSocket::BufferedSocket(char separator, BufferedSocketListener* listener) :
	stopFlag(false), separator(separator), listener(listener)
{
	protocol = Socket::PROTO_DEFAULT;
	state = STARTING;
	pollMask = pollState = 0;
	mode = MODE_LINE;
	remainingSize = (size_t) -1;
	maxLineSize = SETTING(MAX_COMMAND_LENGTH);
	task = TASK_NONE;
	proxyStage = PROXY_STAGE_NONE;
	proxyAuthMethod = 0;
	connectInfo = nullptr;
	outStream = nullptr;
	updateSent = updateReceived = 0;
	gracefulDisconnectTimeout = 0;
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	++socketCounter;
#endif
}

BufferedSocket::~BufferedSocket()
{
	delete connectInfo;
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	--socketCounter;
#endif
}

void BufferedSocket::start()
{
	Thread::start(64, "BufferedSocket");
}

int BufferedSocket::run()
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	if (doLog)
		LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Thread started", false);

	while (!stopFlag)
	{
		try
		{
			if (state == RUNNING || state == CONNECT_PROXY)
			{
				int waitMask = pollState ^ (Socket::WAIT_READ | Socket::WAIT_WRITE);
				if (waitMask)
				{
					int timeout = mode == MODE_DATA ? POLL_TIMEOUT : DISCONNECT_TIMEOUT;
					int waitResult = sock->wait(timeout, waitMask | Socket::WAIT_CONTROL);
					if (waitResult & Socket::WAIT_WRITE)
						pollState |= Socket::WAIT_WRITE;
					if (waitResult & Socket::WAIT_READ)
						pollState |= Socket::WAIT_READ;

#if 0
					string dbg;
					if (waitResult & Socket::WAIT_CONTROL) dbg += " control";
					if (waitResult & Socket::WAIT_READ) dbg += " read";
					if (waitResult & Socket::WAIT_WRITE) dbg += " write";
					if (!dbg.empty()) dcdebug("%p wait:%s\n", this, dbg.c_str());
#endif
				}
				processTask();
				if (pollState & Socket::WAIT_WRITE)
					writeData();
				if (pollState & Socket::WAIT_READ)
					readData();
			} else
			if (!processTask())
				Thread::sleep(POLL_TIMEOUT);
		}
		catch (const Exception& e)
		{
			if (doLog)
			{
				string sockName;
				printSockName(sockName);
				LogManager::message(sockName + ": " + e.getError(), false);
			}
			if (sock)
				sock->disconnect();
			if (state != FAILED)
			{
				state = FAILED;
				if (listener) listener->onFailed(e.getError());
			}
			break;
		}
	}
	if (sock)
		sock->close();
	if (state != FAILED)
	{
		state = FAILED;
		if (listener) listener->onFailed(STRING(DISCONNECTED));
	}
	if (doLog)
		LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Thread stopped", false);
	return 0;
}

void BufferedSocket::readData()
{
	while (!stopFlag)
	{
		if (rb.writePtr == rb.capacity) rb.grow();
		size_t readSize = rb.capacity - rb.writePtr;
		uint8_t* readBuf = rb.buf + rb.writePtr;
		int result;
		if (mode == MODE_DATA)
		{
			if (remainingSize != -1 && readSize > remainingSize) readSize = remainingSize;
			if (state == CONNECT_PROXY)
				result = sock->read(readBuf, readSize);
			else	
				result = ThrottleManager::getInstance()->read(sock.get(), readBuf, readSize);
		}
		else
			result = sock->read(readBuf, readSize);
		if (result == 0)
			throw SocketException(STRING(CONNECTION_CLOSED));
		if (result < 0)
			pollState &= ~Socket::WAIT_READ; // EWOULDBLOCK
		else
		{
			rb.writePtr += result;
			if (!separator && state != CONNECT_PROXY)
				separator = *readBuf == '$' ? '|' : '\n';
		}

		bool resizeFlag = rb.writePtr == rb.capacity;
		while (rb.readPtr < rb.writePtr)
		{
			int prevMode = mode;
			switch (mode)
			{
				case MODE_LINE:
					parseData(rb);
					break;
				case MODE_ZPIPE:
					decompress();
					break;
				default:
					dcassert(mode == MODE_DATA);
					if (state == CONNECT_PROXY)
					{
						checkSocksReply();
						prevMode = mode;
					}
					else
						consumeData();
			}
			if (mode == prevMode) break;
		}
		if (resizeFlag) rb.grow();
		rb.maybeShift();
		if (!(pollState & Socket::WAIT_READ)) break;
	}
}

void BufferedSocket::writeData()
{
	InputStream* stream;
	{
		LOCK(cs);
		while (wb.readPtr < wb.writePtr)
		{
			int result = sock->write(wb.buf + wb.readPtr, wb.writePtr - wb.readPtr);
			if (result < 0)
			{
				// EWOULDBLOCK
				pollState &= ~Socket::WAIT_WRITE;
				return;
			}
			wb.readPtr += result;
			if (stopFlag) return;
		}
		wb.clear();
		if (state == CONNECT_PROXY && mode != MODE_DATA)
			mode = MODE_DATA;
		if (gracefulDisconnectTimeout)
		{
			gracefulDisconnectTimeout = 0;
			stopFlag = true;
			return;
		}
		stream = outStream;
	}
	bool transmitDone = false;
	do
	{
		if (stream)
		{
			if (stopFlag) return;
			if (!sb.capacity) sb.grow(STREAM_BUF_SIZE);
			size_t readSize = sb.capacity - sb.writePtr;
			if (readSize)
			{
				size_t actual = stream->read(sb.buf + sb.writePtr, readSize);
				if (actual)
				{
					if (listener) listener->onBytesSent(actual, 0);
					sb.writePtr += actual;
				}
				else
				{
					stream = nullptr;
					transmitDone = true;
					LOCK(cs);
					outStream = nullptr;
				}
			}
		}
		if (sb.readPtr < sb.writePtr)
		{
			transmitDone = true;
			while (sb.readPtr < sb.writePtr)
			{
				size_t writeSize = sb.writePtr - sb.readPtr;
				int result = ThrottleManager::getInstance()->write(sock.get(), sb.buf + sb.readPtr, writeSize);
				if (result < 0)
				{
					// EWOULDBLOCK
					pollState &= ~Socket::WAIT_WRITE;
					return;
				}
				if (result)
				{
					sb.readPtr += result;
					if (listener) listener->onBytesSent(0, result);
				}
				if (stopFlag) return;
			}
			sb.clear();
		}
	} while (stream);
	if (listener && !stream && transmitDone)
		listener->onTransmitDone();
}

void BufferedSocket::parseData(Buffer& b)
{
	bool doTrace = BOOLSETTING(LOG_TCP_MESSAGES);
	Modes prevMode = mode;
	while (b.readPtr < b.writePtr)
	{
		const uint8_t* buf = b.buf + b.readPtr;
		const uint8_t* ptr = static_cast<const uint8_t*>(memchr(buf, separator, b.writePtr - b.readPtr));
		if (ptr)
		{
			size_t len = ptr - buf;
			if (listener)
			{
				if (len > maxLineSize)
					throw SocketException(STRING(COMMAND_TOO_LONG));
				lineBuf.assign((const char*) buf, len);
				if (doTrace)
					LogManager::commandTrace(lineBuf, LogManager::FLAG_IN, getRemoteIpAsString(), getPort());
				listener->onDataLine(lineBuf);
			}
			b.readPtr += len + 1;
			if (stopFlag || mode != prevMode) break;
		}
		else
		{
			if (b.writePtr - b.readPtr > maxLineSize)
				throw SocketException(STRING(COMMAND_TOO_LONG));
			break;
		}
	}
}

void BufferedSocket::consumeData()
{
	while (rb.readPtr < rb.writePtr)
	{
		if (!remainingSize)
		{
			mode = MODE_LINE;
			if (listener) listener->onModeChange();
			break;
		}
		size_t size = rb.writePtr - rb.readPtr;
		if (remainingSize != -1 && remainingSize < size) size = remainingSize;
		if (listener) listener->onData(rb.buf + rb.readPtr, size);
		if (remainingSize != -1) remainingSize -= size;
		rb.readPtr += size;
		if (mode != MODE_DATA) break;
	}
}

void BufferedSocket::decompress()
{
	size_t inSize = rb.writePtr - rb.readPtr;
	if (!inSize) return;
	size_t outSizeEstimate = inSize * 2;
	size_t newCapacity = min(zb.writePtr - zb.readPtr + outSizeEstimate, zb.maxCapacity);
	if (newCapacity > zb.capacity)
		zb.grow(newCapacity);
	else
		zb.maybeShift();
	while (rb.readPtr < rb.writePtr)
	{
		size_t outSize = zb.capacity - zb.writePtr;
		if (!outSize)
		{
			parseData(zb);
			if (mode != MODE_ZPIPE)
			{
				dcassert(0);
				break;
			}
			zb.maybeShift();
			outSize = zb.capacity - zb.writePtr;
			if (!outSize)
			{
				if (zb.readPtr)
				{
					zb.shift();
					continue;
				}
				throw SocketException("Buffer capacity exceeded");
			}
		}
		inSize = rb.writePtr - rb.readPtr;
		bool result = (*zfilter)(rb.buf + rb.readPtr, inSize, zb.buf + zb.writePtr, outSize);
		rb.readPtr += inSize;
		zb.writePtr += outSize;
		if (!result)
		{
			parseData(zb);
			zb.maybeShift();
			mode = MODE_LINE;
			break;
		}
	}
}

void BufferedSocket::setMode(Modes newMode) noexcept
{
	if (mode == newMode)
	{
		dcdebug("WARNING: Re-entering mode %d\n", mode);
		return;
	}
	
	if (mode == MODE_ZPIPE)
		zfilter.reset();

	if (newMode == MODE_ZPIPE)
	{
		zfilter = std::make_unique<UnZFilter>();
		zb.clear();
	}

	mode = newMode;
}

void BufferedSocket::write(const void* data, size_t size)
{
	if (BOOLSETTING(LOG_TCP_MESSAGES))
	{
		if (size > 512)
		{
			string truncatedMsg((const char *) data, 512 - 11);
			truncatedMsg.append("<TRUNCATED>", 11);
			LogManager::commandTrace(truncatedMsg, 0, getRemoteIpAsString(), getPort());
		}
		else
			LogManager::commandTrace(string((const char *) data, size), 0, getRemoteIpAsString(), getPort());
	}

	bool notify = false;
	{
		LOCK(cs);
		notify = wb.readPtr == wb.writePtr;
		wb.append(data, size);
	}
	if (notify && sock) sock->signalControlEvent();
}

void BufferedSocket::transmitFile(InputStream* stream)
{
	{
		LOCK(cs);
		outStream = stream;
	}
	if (sock) sock->signalControlEvent();
}

string BufferedSocket::getRemoteIpAsString(bool brackets) const
{
	return sock ? Util::printIpAddress(sock->getIp(), brackets) : Util::emptyString;
}

string BufferedSocket::getRemoteIpPort() const
{
	string s = getRemoteIpAsString(true);
	if (s.empty()) return s;
	if (sock)
	{
		s += ':';
		s += Util::toString(sock->getPort());
	}
	return s;
}

#ifdef FLYLINKDC_USE_SOCKET_COUNTER
void BufferedSocket::waitShutdown()
{
	while (socketCounter > 0)
	{
		sleep(10);
	}
}
#endif

void BufferedSocket::setSocket(std::unique_ptr<Socket>&& s)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	if (sock.get())
	{
		if (doLog)
			LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Error - Socket already assigned", false);
		throw SocketException("Socket already assigned");
	}
	dcassert(!sock.get());
	sock = move(s);
	if (doLog)
		LogManager::message("BufferedSocket " + Util::toHexString(this) +
			": Assigned socket " + Util::toHexString(sock.get() ? sock->getSock() : 0), false);
}

void BufferedSocket::addAcceptedSocket(unique_ptr<Socket> newSock, uint16_t port)
{
	if (state != STARTING)
		throw SocketException("Bad state for accept");

	setSocket(move(newSock));
	sock->setPort(port);
	setOptions();

	{
		LOCK(cs);
		task = TASK_ACCEPT;
	}
}

void BufferedSocket::connect(const string& address, uint16_t port, bool secure, bool allowUntrusted, bool proxy, Socket::Protocol proto, const string& expKP /*= Util::emptyString*/)
{
	connect(address, port, 0, NAT_NONE, secure, allowUntrusted, proxy, proto, expKP);
}

void BufferedSocket::connect(const string& address, uint16_t port, uint16_t localPort, NatRoles natRole, bool secure, bool allowUntrusted, bool useProxy, Socket::Protocol proto, const string& expKP /*= Util::emptyString*/)
{
	if (state != STARTING)
		throw SocketException("Bad state for connect");

	Socket::ProxyConfig proxy;
	if (useProxy && !Socket::getProxyConfig(proxy))
		useProxy = false;

	protocol = proto;

	{
		LOCK(cs);
		dcassert(!connectInfo);
		connectInfo = new ConnectInfo(address, port, localPort, natRole, secure, allowUntrusted, expKP, useProxy ? &proxy : nullptr);
		task = TASK_CONNECT;
	}
}

bool BufferedSocket::processTask()
{
	bool result = false;
	bool updated = false;
	int task = TASK_NONE;
	{
		LOCK(cs);
		if (gracefulDisconnectTimeout && GET_TICK() > gracefulDisconnectTimeout)
		{
			gracefulDisconnectTimeout = 0;
			stopFlag = true;
			return true;
		}
		std::swap(task, this->task);
		if (updateReceived != updateSent)
		{
			updateReceived = updateSent;
			updated = true;
		}
	}
	switch (task)
	{
		case TASK_CONNECT:
			doConnect(connectInfo, false);
			result = true;
			break;
		case TASK_ACCEPT:
			doAccept();
			result = true;
	}
	if (updated && listener) 
		listener->onUpdated();
	return result;
}

void BufferedSocket::createSocksMessage(const BufferedSocket::ConnectInfo* ci)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	const bool resolveNames = ci->proxy.resolveNames && ci->addr.length() <= 255;
	size_t userLen = std::min<size_t>(255, ci->proxy.user.length());
	size_t passwordLen = std::min<size_t>(255, ci->proxy.password.length());
	size_t size;
	switch (proxyStage)
	{
		case PROXY_STAGE_NEGOTIATE:
			size = 3;
			if (userLen || passwordLen)
				proxyAuthMethod = 2;
			else
				proxyAuthMethod = 0;
			break;
		case PROXY_STAGE_AUTH:
			size = 3 + userLen + passwordLen;
			break;
		case PROXY_STAGE_CONNECT:
			size = resolveNames ? ci->addr.length() + 7 : 22;
			break;
		default:
			return;
	}
	if (wb.capacity < size) wb.grow(size);
	wb.writePtr = 0;
	wb.readPtr = 0;
	switch (proxyStage)
	{
		case PROXY_STAGE_NEGOTIATE:
			wb.buf[0] = 5; // SOCKSv5
			wb.buf[1] = 1; // Number of methods
			wb.buf[2] = proxyAuthMethod;
			break;
		case PROXY_STAGE_AUTH:
			wb.buf[0] = 1;
			wb.buf[1] = static_cast<uint8_t>(userLen);
			size = 2;
			memcpy(wb.buf + size, ci->proxy.user.data(), userLen);
			size += userLen;
			wb.buf[size++] = static_cast<uint8_t>(passwordLen);
			memcpy(wb.buf + size, ci->proxy.password.data(), passwordLen);
			size += passwordLen;
			break;
		case PROXY_STAGE_CONNECT:
			wb.buf[0] = 5;
			wb.buf[1] = 1; // Connect
			wb.buf[2] = 0; // Reserved
			if (resolveNames)
			{
				wb.buf[3] = 3; // Address type: domain name
				wb.buf[4] = static_cast<uint8_t>(ci->addr.length());
				size = 5;
				memcpy(wb.buf + 5, ci->addr.data(), ci->addr.length());
				size += ci->addr.length();
			}
			else
			{
				IpAddress addr;
				bool isNumeric;
				if (!Socket::resolveHost(addr, getIp().type, ci->addr, &isNumeric)) // FIXME
				{
					if (doLog)
						LogManager::message("Error resolving " + ci->addr, false);
					throw SocketException(STRING(RESOLVE_FAILED));
				}
				else if (doLog)
				{
					string sockName;
					printSockName(sockName);
					LogManager::message(sockName + ": Host " + ci->addr + " resolved to " + Util::printIpAddress(addr, true), false);
				}
				if (addr.type == AF_INET)
				{
					uint32_t address = addr.data.v4;
					wb.buf[3] = 1;
					wb.buf[4] = (uint8_t) (address >> 24);
					wb.buf[5] = (uint8_t) (address >> 16);
					wb.buf[6] = (uint8_t) (address >> 8);
					wb.buf[7] = (uint8_t) address;
					size = 8;
				}
				else
				{
					wb.buf[3] = 4;
					memcpy(wb.buf + 4, &addr.data.v6, 16);
					size = 20;
				}
			}
			wb.buf[size++] = ci->port >> 8;
			wb.buf[size++] = ci->port & 0xFF;
	}
	wb.writePtr = size;
}

void BufferedSocket::checkSocksReply()
{
	size_t fill = rb.writePtr - rb.readPtr;
	size_t responseSize = proxyStage == PROXY_STAGE_CONNECT ? 10 : 2;
	if (fill < responseSize) return;
	const uint8_t* resp = rb.buf + rb.readPtr;
	switch (proxyStage)
	{
		case PROXY_STAGE_NEGOTIATE:
			if (resp[1] != proxyAuthMethod)
				throw SocketException(proxyAuthMethod ? STRING(SOCKS_AUTH_UNSUPPORTED) : STRING(SOCKS_NEEDS_AUTH));
			rb.clear();
			proxyStage = proxyAuthMethod ? PROXY_STAGE_AUTH : PROXY_STAGE_CONNECT;
			createSocksMessage(connectInfo);
			return;
		case PROXY_STAGE_AUTH:
			if (resp[1] != 0)
				throw SocketException(STRING(SOCKS_AUTH_FAILED));
			rb.clear();
			proxyStage = PROXY_STAGE_CONNECT;
			createSocksMessage(connectInfo);
			return;
	}
	if (resp[0] != 5 || resp[1] != 0)
		throw SocketException(STRING(SOCKS_FAILED));
	rb.clear();
	proxyStage = PROXY_STAGE_NONE;
	mode = MODE_LINE;
	IpAddress ip;
	Socket::resolveHost(ip, getIp().type, connectInfo->addr);
	sock->setIp(ip);
	sock->setPort(connectInfo->port);
	if (connectInfo->secure)
	{
		SSLSocket* newSock = CryptoManager::getInstance()->getClientSocket(connectInfo->allowUntrusted, connectInfo->expKP, protocol);
		newSock->setIp(sock->getIp());
		newSock->setPort(sock->getPort());
		newSock->attachSock(sock->detachSock());
		newSock->setConnected();
		sock.reset(newSock);
		doConnect(connectInfo, true);
	}
	else
	{
		state = RUNNING;
		pollState = Socket::WAIT_READ | Socket::WAIT_WRITE;
		if (listener) listener->onConnected();
	}
	delete connectInfo;
	connectInfo = nullptr;
}

void BufferedSocket::doConnect(const BufferedSocket::ConnectInfo* ci, bool sslSocks)
{
	if (!sslSocks)
	{
		dcassert(state == STARTING);
		if (listener) listener->onConnecting();
	}
	const uint64_t endTime = GET_TICK() + LONG_TIMEOUT;
	do
	{
		if (stopFlag)
			return;
			
		try
		{
			if (!sslSocks)
			{
				const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
				const string* host;
				uint16_t port;
				if (ci->useProxy)
				{
					if (ci->proxy.host.empty() || ci->proxy.port == 0)
						throw SocketException(STRING(SOCKS_FAILED));

					proxyStage = PROXY_STAGE_NEGOTIATE;
					createSocksMessage(ci);
					state = CONNECT_PROXY;
					
					host = &ci->proxy.host;
					port = ci->proxy.port;
				}
				else
				{
					host = &ci->addr;
					port = ci->port;
				}
				IpAddress ip;
				bool isNumeric;
				if (!Socket::resolveHost(ip, 0, *host, &isNumeric))
				{
					if (doLog)
						LogManager::message("Error resolving " + *host, false);
					throw SocketException(STRING(RESOLVE_FAILED));
				}
				if (!sock)
				{
					std::unique_ptr<Socket> s(ci->secure && !ci->useProxy ? (ci->natRole == NAT_SERVER ? 
						CryptoManager::getInstance()->getServerSocket(ci->allowUntrusted) : 
						CryptoManager::getInstance()->getClientSocket(ci->allowUntrusted, ci->expKP, protocol)) : new Socket);
					s->create(ip.type, Socket::TYPE_TCP);
					setSocket(move(s));
					IpAddress bindAddr;
					getBindAddress(bindAddr, ip.type);
					sock->bind(ci->localPort, bindAddr);
				}
				if (doLog && !isNumeric)
				{
					string sockName;
					printSockName(sockName);
					LogManager::message(sockName + ": Host " + *host + " resolved to " + Util::printIpAddress(ip, true), false);
				}
				sock->connect(ip, port, isNumeric ? Util::emptyString : *host);
				setOptions();
			}
			while (true)
			{
				if (sock->waitConnected(POLL_TIMEOUT))
				{
					pollState = Socket::WAIT_READ | Socket::WAIT_WRITE;
					sock->createControlEvent();
					if (state != CONNECT_PROXY)
					{
						state = RUNNING;
						if (listener) listener->onConnected();
						delete connectInfo;
						connectInfo = nullptr;
					}
					return;
				}
				if (endTime <= GET_TICK())
					break;

				if (stopFlag)
					return;
			}
		}
		catch (const SSLSocketException&)
		{
			throw;
		}
		catch (const SocketException&)
		{
			if (state == CONNECT_PROXY)
				throw SocketException(STRING(SOCKS_CONN_FAILED));
			if (ci->natRole == NAT_NONE)
				throw;
			sleep(SHORT_TIMEOUT);
		}
	}
	while (GET_TICK() < endTime);
	
	throw SocketException(state == CONNECT_PROXY ? STRING(SOCKS_CONN_FAILED) : STRING(CONNECTION_TIMEOUT));
}

void BufferedSocket::doAccept()
{
	dcassert(state == STARTING);
	dcdebug("doAccept\n");

	uint64_t startTime = GET_TICK();
	while (!sock->waitAccepted(POLL_TIMEOUT))
	{
		if (stopFlag)
			return;
			
		if (startTime + LONG_TIMEOUT < GET_TICK())
			throw SocketException(STRING(CONNECTION_TIMEOUT));
	}

	state = RUNNING;
	pollState = Socket::WAIT_READ | Socket::WAIT_WRITE;
	sock->createControlEvent();

	if (sock->getSecureTransport() == Socket::SECURE_TRANSPORT_DETECT)
	{
		int type = static_cast<AutoDetectSocket*>(sock.get())->getType();
		if (type == AutoDetectSocket::TYPE_SSL)
		{
			SSLSocket* newSocket = new SSLSocket(CryptoManager::SSL_SERVER, true, Util::emptyString);
			newSocket->attachSock(sock->detachSock());
			newSocket->setIp(sock->getIp());
			newSocket->setPort(sock->getPort());
			sock.reset(newSocket);
			setOptions();
			startTime = GET_TICK();
			while (!sock->waitAccepted(POLL_TIMEOUT))
			{
				if (stopFlag)
					return;

				if (startTime + LONG_TIMEOUT < GET_TICK())
					throw SocketException(STRING(CONNECTION_TIMEOUT));
			}
			sock->createControlEvent();
			const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
			if (doLog)
				LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Upgraded to SSL", false);
			if (listener) listener->onUpgradedToSSL();
		}
	}
}

void BufferedSocket::setOptions()
{
	dcassert(sock);
	sock->setInBufSize();
	sock->setOutBufSize();
}

void BufferedSocket::disconnect(bool graceless /*= false */)
{
	if (graceless)
	{
		stopFlag = true;
		if (sock) sock->signalControlEvent();
	}
	else
	{
		uint64_t timeout = GET_TICK() + DISCONNECT_TIMEOUT;
		{
			LOCK(cs);
			if (wb.readPtr == wb.writePtr)
			{
				stopFlag = true;
				gracefulDisconnectTimeout = 0;
			}
			else
			if (!gracefulDisconnectTimeout)
				gracefulDisconnectTimeout = timeout;
		}
		if (sock && stopFlag) sock->signalControlEvent();
	}
}

void BufferedSocket::updated()
{
	LOCK(cs);
	updateSent++;
	if (sock) sock->signalControlEvent();
}

const IpAddress& BufferedSocket::getIp() const
{
	static IpAddress empty{0};
	if (sock) return sock->getIp();
	return empty;
}

void BufferedSocket::printSockName(string& sockName) const
{
	if (sock)
		sock->printSockName(sockName);
	else
		sockName = "Socket 0";
}

void BufferedSocket::getBindAddress(IpAddress& ip, int af, const string& s)
{
	if (af == AF_INET6)
	{
		Ip6Address v6;
		if (!s.empty() && Util::parseIpAddress(v6, s))
			ip.data.v6 = v6;
		else
			memset(&ip.data.v6, 0, sizeof(ip.data.v6));
		ip.type = AF_INET6;
	}
	else
	{
		Ip4Address v4;
		if (!s.empty() && Util::parseIpAddress(v4, s))
			ip.data.v4 = v4;
		else
			ip.data.v4 = 0;
		ip.type = AF_INET;
	}
}

void BufferedSocket::getBindAddress(IpAddress& ip, int af)
{
	getBindAddress(ip, af, af == AF_INET6 ? SETTING(BIND_ADDRESS6) : SETTING(BIND_ADDRESS));
}
