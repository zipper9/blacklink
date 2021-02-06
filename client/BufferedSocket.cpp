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
#include "CompatibilityManager.h"
#include "SettingsManager.h"
#include "CryptoManager.h"
#include "ZUtils.h"
#include "ThrottleManager.h"
#include "LogManager.h"
#include "IpGuard.h"
#include "ShareManager.h"
#include "DebugManager.h"
#include "SSLSocket.h"
#include "AutoDetectSocket.h"
#include <atomic>

// Polling is used for tasks...should be fixed...
static const uint64_t POLL_TIMEOUT = 250;

#ifdef FLYLINKDC_USE_SOCKET_COUNTER
static std::atomic<int> g_sockets(0);
#endif

BufferedSocket::BufferedSocket(char separator, BufferedSocketListener* listener) :
	listener(listener),
	protocol(Socket::PROTO_DEFAULT),
	separator(separator),
	mode(MODE_LINE),
	dataBytes(0),
	state(STARTING),
// [-] brain-ripper
// should be rewritten using ThrottleManager
//sleep(0), // !SMT!-S
	disconnecting(false)
{
	semaphore.create();
	start(64, "BufferedSocket");
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	++g_sockets;
#endif
}

BufferedSocket::~BufferedSocket()
{
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	--g_sockets;
#endif
}

void BufferedSocket::setMode(Modes newMode) noexcept
{
	if (mode == newMode)
	{
		dcdebug("WARNING: Re-entering mode %d\n", mode);
		return;
	}
	
	if (mode == MODE_ZPIPE && filterIn)
	{
		// delete the filter when going out of zpipe mode.
		filterIn.reset();
	}
	
	if (newMode == MODE_ZPIPE)
		filterIn = std::make_unique<UnZFilter>();

	mode = newMode;
}

void BufferedSocket::setSocket(std::unique_ptr<Socket>&& s)
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	if (sock.get())
	{
		if (doLog)
			LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Error - Socket already assigned", false);
	}
	dcassert(!sock.get());
	sock = move(s);
	if (doLog)
		LogManager::message("BufferedSocket " + Util::toHexString(this) +
			": Assigned socket " + Util::toHexString(sock.get() ? sock->sock : 0), false);
}

void BufferedSocket::resizeInBuf()
{
	int l_size = MAX_SOCKET_BUFFER_SIZE;
	inbuf.resize(l_size);
}

void BufferedSocket::addAcceptedSocket(unique_ptr<Socket> newSock, uint16_t port)
{
	setSocket(move(newSock));
	sock->setPort(port);
	setOptions();
	addTask(ACCEPTED, nullptr);	
}

void BufferedSocket::connect(const string& address, uint16_t port, bool secure, bool allowUntrusted, bool proxy, Socket::Protocol proto, const string& expKP /*= Util::emptyString*/)
{
	connect(address, port, 0, NAT_NONE, secure, allowUntrusted, proxy, proto, expKP);
}

void BufferedSocket::connect(const string& address, uint16_t port, uint16_t localPort, NatRoles natRole, bool secure, bool allowUntrusted, bool useProxy, Socket::Protocol proto, const string& expKP /*= Util::emptyString*/)
{
	dcdebug("BufferedSocket::connect() %p\n", (void*)this);

	Socket::ProxyConfig proxy;
	if (useProxy && !Socket::getProxyConfig(proxy))
		useProxy = false;

	std::unique_ptr<Socket> s(secure && !useProxy ? (natRole == NAT_SERVER ? 
		CryptoManager::getInstance()->getServerSocket(allowUntrusted) : 
		CryptoManager::getInstance()->getClientSocket(allowUntrusted, expKP, proto)) : new Socket);

	s->create();
	
	setSocket(move(s));
	sock->bind(localPort, SETTING(BIND_ADDRESS));
	
	disconnecting = false;
	protocol = proto;

	addTask(CONNECT, new ConnectInfo(address, port, localPort, natRole, secure, allowUntrusted, expKP, useProxy ? &proxy : nullptr));
}

static const uint16_t LONG_TIMEOUT = 30000;
static const uint16_t SHORT_TIMEOUT = 1000;

void BufferedSocket::threadConnect(const BufferedSocket::ConnectInfo* ci)
{
	dcassert(state == STARTING);
	
	if (listener) listener->onConnecting();
	const uint64_t endTime = GET_TICK() + LONG_TIMEOUT;
	state = RUNNING;
	do
	{
		if (socketIsDisconnecting())
			break;
			
		dcdebug("threadConnect attempt to addr \"%s\"\n", ci->addr.c_str());
		try
		{
			if (ci->useProxy)
			{
				sock->socksConnect(ci->proxy, ci->addr, ci->port, LONG_TIMEOUT);
				if (ci->secure)
				{
					SSLSocket* newSock = CryptoManager::getInstance()->getClientSocket(ci->allowUntrusted, ci->expKP, protocol);
					newSock->setIp4(sock->getIp4());
					newSock->setPort(sock->getPort());
					newSock->attachSock(sock->detachSock());
					sock.reset(newSock);
				}
			}
			else
				sock->connect(ci->addr, ci->port);
			
			setOptions();
			
			while (true)
			{
				if (sock->waitConnected(POLL_TIMEOUT))
				{
					if (!socketIsDisconnecting())
					{
						resizeInBuf();
						if (listener) listener->onConnected();
					}
					return;
				}
				if (endTime <= GET_TICK())
					break;
					
				if (socketIsDisconnecting())
					return;
			}
		}
		catch (const SSLSocketException&)
		{
			throw;
		}
		catch (const SocketException&)
		{
			if (ci->natRole == NAT_NONE)
				throw;
			sleep(SHORT_TIMEOUT);
		}
	}
	while (GET_TICK() < endTime);
	
	throw SocketException(STRING(CONNECTION_TIMEOUT));
}

void BufferedSocket::threadAccept()
{
	dcassert(state == STARTING);
	
	dcdebug("threadAccept\n");
	
	state = RUNNING;
	
	resizeInBuf();
	
	uint64_t startTime = GET_TICK();
	while (!sock->waitAccepted(POLL_TIMEOUT))
	{
		if (socketIsDisconnecting())
			return;
			
		if (startTime + LONG_TIMEOUT < GET_TICK())
			throw SocketException(STRING(CONNECTION_TIMEOUT));
	}

	if (sock->getSecureTransport() == Socket::SECURE_TRANSPORT_DETECT)
	{
		int type = static_cast<AutoDetectSocket*>(sock.get())->getType();
		if (type == AutoDetectSocket::TYPE_SSL)
		{
			SSLSocket* newSocket = new SSLSocket(CryptoManager::SSL_SERVER, true, Util::emptyString);
			newSocket->attachSock(sock->detachSock());
			newSocket->setIp4(sock->getIp4());
			newSocket->setPort(sock->getPort());
			sock.reset(newSocket);
			startTime = GET_TICK();
			while (!sock->waitAccepted(POLL_TIMEOUT))
			{
				if (socketIsDisconnecting())
					return;
			
				if (startTime + LONG_TIMEOUT < GET_TICK())
					throw SocketException(STRING(CONNECTION_TIMEOUT));
			}
			const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
			if (doLog)
				LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Upgraded to SSL", false);
			if (listener) listener->onUpgradedToSSL();
		}
	}
}

void BufferedSocket::threadRead()
{
	if (state != RUNNING)
		return;
	try
	{
		bool doTrace = BOOLSETTING(LOG_TCP_MESSAGES);
		int left = mode == MODE_DATA ? ThrottleManager::getInstance()->read(sock.get(), &inbuf[0], (int)inbuf.size()) : sock->read(&inbuf[0], (int)inbuf.size());
		if (left == -1)
		{
			// EWOULDBLOCK, no data received...
			return;
		}
		if (ClientManager::isBeforeShutdown())
			return;
		if (left == 0)
		{
			// This socket has been closed...
			throw SocketException(STRING(CONNECTION_CLOSED));
		}
		
		string::size_type pos = 0;
		// always uncompressed data
		string l;
		int bufpos = 0;
		int total = left;
		while (left > 0)
		{
			switch (mode)
			{
				case MODE_ZPIPE:
				{
					const int BUF_SIZE = 1024;
					char buffer[BUF_SIZE];
					// Special to autodetect nmdc connections...
					string::size_type zpos = 0;
					l = m_line;
					// decompress all input data and store in l.
					while (left)
					{
						size_t in = BUF_SIZE;
						size_t used = left;
						bool ret = (*filterIn)(&inbuf[0] + total - left, used, buffer, in);
						left -= used;
						l.append(buffer, in);
						// if the stream ends before the data runs out, keep remainder of data in inbuf
						if (!ret)
						{
							bufpos = total - left;
							setMode(MODE_LINE);
							break;
						}
					}
					// process all lines
					while ((zpos = l.find(separator)) != string::npos)
					{
						if (zpos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
						{
							if (!disconnecting && listener)
							{
								currentLine = l.substr(0, zpos);
								if (doTrace)
									LogManager::commandTrace(currentLine, LogManager::FLAG_IN, getRemoteIpPort());
								listener->onDataLine(currentLine);
							}
						}
						l.erase(0, zpos + 1 /* separator char */); //[3] https://www.box.net/shared/74efa5b96079301f7194
					}
					// store remainder
					m_line = l;
					break;
				}
				case MODE_LINE:
				{
					// Special to autodetect nmdc connections...
					if (separator == 0)
					{
						if (inbuf[0] == '$')
							separator = '|';
						else
							separator = '\n';
					}
					l = m_line + string((char*)& inbuf[bufpos], left);
					//dcassert(isalnum(l[0]) || isalpha(l[0]) || isascii(l[0]));
#if 0
					int l_count_separator = 0;
#endif
#ifdef _DEBUG
					//LogManager::message("MODE_LINE . m_line = " + m_line);
					//LogManager::message("MODE_LINE = " + l);
#endif
					while ((pos = l.find(separator)) != string::npos)
					{
						if (pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
						{
							if (!disconnecting && listener)
							{
								currentLine = l.substr(0, pos);
								if (doTrace)
									LogManager::commandTrace(currentLine, LogManager::FLAG_IN, getRemoteIpPort());
								listener->onDataLine(currentLine);
							}
						}
						l.erase(0, pos + 1 /* separator char */);
						// TODO - erase не эффективно.
						if (l.length() < (size_t)left)
						{
							left = l.length();
						}
						//dcassert(mode == MODE_LINE);
						if (mode != MODE_LINE)
						{
							// we changed mode; remainder of l is invalid.
							l.clear();
							bufpos = total - left;
							break;
						}
					}					
					if (pos == string::npos)
						left = 0;
					m_line = l;
					break;
				}
				case MODE_DATA:
					while (left > 0)
					{
						if (dataBytes == -1)
						{							
							if (listener)
								listener->onData(&inbuf[bufpos], left);
							bufpos += left;
							left = 0;
						}
						else
						{
							const int high = (int)min(dataBytes, (int64_t)left);
							//dcassert(high != 0);
							if (high != 0) // [+] IRainman fix.
							{
								if (listener)
									listener->onData(&inbuf[bufpos], high);
								bufpos += high;
								left -= high;								
								dataBytes -= high;
							}
							if (dataBytes == 0)
							{
								mode = MODE_LINE;
#ifdef _DEBUG
								LogManager::message("BufferedSocket:: = MODE_LINE [1]");
#endif
								if (listener) listener->onModeChange();
								break; // [DC++] break loop, in case setDataMode is called with less than read buffer size
							}
						}
					}
					break;
			}
		}
		if (mode == MODE_LINE && m_line.size() > static_cast<size_t>(SETTING(MAX_COMMAND_LENGTH)))
		{
			throw SocketException(STRING(COMMAND_TOO_LONG));
		}
	}
	catch (const std::bad_alloc&)
	{
		throw SocketException(STRING(BAD_ALLOC));
	}
}

void BufferedSocket::disconnect(bool graceless /*= false */)
{
	if (graceless)
		disconnecting = true;
	addTask(DISCONNECT, nullptr);
}

Ip4Address BufferedSocket::getIp4() const
{
	if (hasSocket())
		return sock->getIp4();
	return 0;
}

#ifdef FLYLINKDC_USE_SOCKET_COUNTER
void BufferedSocket::waitShutdown()
{
	while (g_sockets > 0)
	{
		sleep(10);
	}
}
#endif

void BufferedSocket::threadSendFile(InputStream* file)
{
	if (state != RUNNING)
		return;

	if (socketIsDisconnecting())
		return;
	dcassert(file);
	
	const size_t l_sockSize = MAX_SOCKET_BUFFER_SIZE; // тормозит отдача size_t(sock->getSocketOptInt(SO_SNDBUF));
	static size_t g_bufSize = 0;
	if (g_bufSize == 0)
	{
		g_bufSize = std::max(l_sockSize, size_t(MAX_SOCKET_BUFFER_SIZE));
	}
	
	ByteVector tmpReadBuf; // TODO заменить на - не пишет буфера 0-ями std::unique_ptr<uint8_t[]> buf(new uint8_t[BUFSIZE]);
	ByteVector tmpWriteBuf;
	tmpReadBuf.resize(g_bufSize);
	tmpWriteBuf.resize(g_bufSize);
	
	size_t readPos = 0;
	bool readDone = false;
	dcdebug("Starting threadSend\n");
	while (!socketIsDisconnecting())
	{
		if (!readDone && tmpReadBuf.size() > readPos)
		{
			size_t bytesRead = tmpReadBuf.size() - readPos;
			size_t actual = file->read(&tmpReadBuf[readPos], bytesRead); // TODO можно узнать что считали последний кусок в файл
			if (actual == 0)
			{
				readDone = true;
			}
			else
			{
				if (listener) listener->onBytesSent(actual, 0);
				readPos += actual;
			}
		}
		if (readDone && readPos == 0)
		{
			if (listener) listener->onTransmitDone();
			return;
		}
		tmpReadBuf.swap(tmpWriteBuf);
		tmpReadBuf.resize(g_bufSize);
		tmpWriteBuf.resize(readPos); // TODO - tmpWriteBuf опустить ниже и заказывать сколько нужно.
		readPos = 0;
		
		size_t writePos = 0, writeSize = 0;
		int written = 0;
		
		while (writePos < tmpWriteBuf.size())
		{
			if (socketIsDisconnecting())
				return;
				
			if (written == -1)
			{
				// workaround for OpenSSL (crashes when previous write failed and now retrying with different writeSize)
				written = sock->write(&tmpWriteBuf[writePos], writeSize);
			}
			else
			{
				writeSize = std::min(l_sockSize / 2, tmpWriteBuf.size() - writePos);
				written = ThrottleManager::getInstance()->write(sock.get(), &tmpWriteBuf[writePos], writeSize);
			}
			
			if (written > 0)
			{
				writePos += written;
				if (listener) listener->onBytesSent(0, written);
			}
			else if (written == -1)
			{
				if (!readDone && readPos < tmpReadBuf.size())
				{
					size_t bytesRead = min(tmpReadBuf.size() - readPos, tmpReadBuf.size() / 2);
					size_t actual = file->read(&tmpReadBuf[readPos], bytesRead);
					if (actual == 0)
					{
						readDone = true;
					}
					else
					{
						if (listener) listener->onBytesSent(actual, 0);
						readPos += actual;
					}
				}
				else
				{
					while (!socketIsDisconnecting())
					{
						const int w = sock->wait(POLL_TIMEOUT, Socket::WAIT_WRITE | Socket::WAIT_READ);
						if (w & Socket::WAIT_READ)
						{
							threadRead();
						}
						if (w & Socket::WAIT_WRITE)
						{
							break;
						}
					}
				}
			}
		}
	}
}

void BufferedSocket::write(const char* buf, size_t len)
{
	if (!hasSocket())
	{
		dcassert(0);
		return;
	}
	if (BOOLSETTING(LOG_TCP_MESSAGES))
	{
		if (len > 512)
		{
			string truncatedMsg(buf, 512 - 11);
			truncatedMsg.append("<TRUNCATED>", 11);
			LogManager::commandTrace(truncatedMsg, 0, getRemoteIpPort());
		}
		else
			LogManager::commandTrace(string(buf, len), 0, getRemoteIpPort());
	}
	
	bool sendSignal = false;
	cs.lock();
	if (writeBuf.empty())
		sendSignal = addTaskL(SEND_DATA, nullptr);
#ifdef _DEBUG
	if (len > 1)
		dcassert(!(buf[len - 1] == '|' && buf[len - 2] == '|'));
#endif
	// TODO: limit size of writeBuf
	writeBuf.reserve(writeBuf.size() + len);
	writeBuf.insert(writeBuf.end(), buf, buf + len);
	cs.unlock();
	if (sendSignal) semaphore.notify();
}

void BufferedSocket::threadSendData()
{
	if (state != RUNNING)
		return;
	ByteVector sendBuf;
	{
		LOCK(cs);
		if (writeBuf.empty())
		{
			dcassert(!writeBuf.empty());
			return;
		}
		writeBuf.swap(sendBuf);
	}
	
	size_t left = sendBuf.size();
	size_t done = 0;
	while (left > 0)
	{
		if (socketIsDisconnecting())
		{
			return;
		}
		
		const int w = sock->wait(POLL_TIMEOUT, Socket::WAIT_READ | Socket::WAIT_WRITE);
		
		if (w & Socket::WAIT_READ)
		{
			threadRead();
		}
		
		if (w & Socket::WAIT_WRITE)
		{
			// TODO - find ("||")
			const int n = sock->write(&sendBuf[done], left); // adguard - https://www.box.net/shared/9201edaa1fa1b83a8d3c
			if (n > 0)
			{
				left -= n;
				done += n;
			}
		}
	}
}

bool BufferedSocket::checkEvents()
{
	while (true)
	{
		//dcassert(!ClientManager::isShutdown());
		pair<Tasks, std::unique_ptr<TaskData>> p;
		{
			cs.lock();
			if (tasks.empty())
			{
				if (state == RUNNING)
				{
					cs.unlock();
					break;
				}
				cs.unlock();
				semaphore.wait();
				continue;
			}
			p = std::move(tasks.front());
			tasks.pop_front();
			cs.unlock();
		}
		if (state == RUNNING)
		{
			if (p.first == UPDATED)
			{
				if (listener) listener->onUpdated();
				continue;
			}
			else if (p.first == SEND_DATA)
			{
				threadSendData();
			}
			else if (p.first == SEND_FILE)
			{
				threadSendFile(static_cast<SendFileInfo*>(p.second.get())->stream);
				break;
			}
			else if (p.first == DISCONNECT)
			{
				fail(STRING(DISCONNECTED));
			}
			else if (p.first == SHUTDOWN)
			{
				return false;
			}
			else
			{
				dcdebug("%d unexpected in RUNNING state\n", p.first);
			}
		}
		else if (state == STARTING)
		{
			if (p.first == CONNECT)
			{
				ConnectInfo* ci = static_cast<ConnectInfo*>(p.second.get());
				threadConnect(ci);
			}
			else if (p.first == ACCEPTED)
			{
				threadAccept();
			}
			else if (p.first == SHUTDOWN)
			{
				return false;
			}
			else
			{
				dcdebug("%d unexpected in STARTING state\n", p.first);
			}
		}
		else
		{
			if (p.first == SHUTDOWN)
			{
				return false;
			}
			else
			{
				dcdebug("%d unexpected in FAILED state\n", p.first);
			}
		}
	}
	return true;
}

void BufferedSocket::checkSocket()
{
	if (hasSocket())
	{
		int waitFor = sock->wait(POLL_TIMEOUT, Socket::WAIT_READ);
		if (waitFor & Socket::WAIT_READ)
		{
			threadRead();
		}
	}
}

/**
 * Main task dispatcher for the buffered socket abstraction.
 * @todo Fix the polling...
 */
int BufferedSocket::run()
{
	const bool doLog = BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM);
	if (doLog)
		LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Thread started", false);

	while (true)
	{
		try
		{
			if (!checkEvents()) break;
			if (state == RUNNING) checkSocket();
		}
		catch (const Exception& e)
		{
			if (doLog)
				LogManager::message("Socket " + Util::toHexString(hasSocket() ? sock->sock : 0) + ": " + e.getError(), false);
			fail(e.getError());
		}
	}
	if (hasSocket())
		sock->close();
	if (doLog)
		LogManager::message("BufferedSocket " + Util::toHexString(this) + ": Thread stopped", false);
	return 0;
}

void BufferedSocket::fail(const string& aError)
{
	if (hasSocket())
	{
		sock->disconnect();
	}
	
	if (state == RUNNING)
	{
		state = FAILED;
		if (listener) listener->onFailed(aError);
	}
}

void BufferedSocket::shutdown()
{
	disconnecting = true;
	addTask(SHUTDOWN, nullptr);
}

void BufferedSocket::addTask(Tasks task, TaskData* data) noexcept
{
	cs.lock();
	bool result = addTaskL(task, data);
	cs.unlock();
	if (result) semaphore.notify();
}

bool BufferedSocket::addTaskL(Tasks task, TaskData* data) noexcept
{
	dcassert(task == DISCONNECT || task == SHUTDOWN || task == UPDATED || sock.get());
	if (task == DISCONNECT && !tasks.empty())
	{
		if (tasks.back().first == DISCONNECT)
			return false;
	}
	if (task == SHUTDOWN && !tasks.empty())
	{
		if (tasks.back().first == SHUTDOWN)
			return false;
	}
#ifdef _DEBUG
	if (task == UPDATED && !tasks.empty())
	{
		if (tasks.back().first == UPDATED)
		{
			dcassert(0);
			return false;
		}
	}
	if (task == SEND_DATA && !tasks.empty())
	{
		if (tasks.back().first == SEND_DATA)
		{
			dcassert(0);
			return false;
		}
	}
	if (task == ACCEPTED && !tasks.empty())
	{
		if (tasks.back().first == ACCEPTED)
		{
			dcassert(0);
			return false;
		}
	}
#endif
	
	bool result = tasks.empty();
	tasks.push_back(std::make_pair(task, std::unique_ptr<TaskData>(data)));
	return result;
}
