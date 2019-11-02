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
#include "CompatibilityManager.h"
#include "BufferedSocket.h"
#include "TimerManager.h"
#include "SettingsManager.h"
#include "Streams.h"
#include "CryptoManager.h"
#include "ZUtils.h"
#include "ThrottleManager.h"
#include "LogManager.h"
#include "ResourceManager.h"
#include "IpGuard.h"
#include "ClientManager.h"
#include "Util.h"
#include "ShareManager.h"
#include "DebugManager.h"
#include "SSLSocket.h"
#include "UserConnection.h"
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
	m_rollback(0),
	state(STARTING),
	m_count_search_ddos(0),
// [-] brain-ripper
// should be rewritten using ThrottleManager
//sleep(0), // !SMT!-S
	m_is_disconnecting(false),
	myInfoCount(0),
	myInfoLoaded(false),
	m_is_hide_share(false)
{
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

void BufferedSocket::setMode(Modes newMode, size_t aRollback)
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
	
	switch (newMode)
	{
		case MODE_LINE:
			m_rollback = aRollback;
			break;
		case MODE_ZPIPE:
			filterIn = std::make_unique<UnZFilter>();
			break;
		case MODE_DATA:
			break;
	}
	mode = newMode;
}

void BufferedSocket::setSocket(std::unique_ptr<Socket>&& s)
{
	bool doLog = BOOLSETTING(LOG_SYSTEM);
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

#ifndef FLYLINKDC_HE
void BufferedSocket::resizeInBuf()
{
	int l_size = MAX_SOCKET_BUFFER_SIZE;
	inbuf.resize(l_size);
}
#endif // FLYLINKDC_HE

void BufferedSocket::addAcceptedSocket(unique_ptr<Socket> newSock)
{
	setSocket(move(newSock));
	setOptions();
	addTask(ACCEPTED, nullptr);	
}

void BufferedSocket::connect(const string& address, uint16_t port, bool secure, bool allowUntrusted, bool proxy,
    Socket::Protocol proto, const string& expKP /*= Util::emptyString*/)
{
	connect(address, port, 0, NAT_NONE, secure, allowUntrusted, proxy, proto, expKP);
}

void BufferedSocket::connect(const string& address, uint16_t port, uint16_t localPort, NatRoles natRole, bool secure, bool allowUntrusted, bool proxy, Socket::Protocol proto, const string& expKP /*= Util::emptyString*/)
{
	dcdebug("BufferedSocket::connect() %p\n", (void*)this);
//	unique_ptr<Socket> s(secure ? new SSLSocket(natRole == NAT_SERVER ? CryptoManager::SSL_SERVER : CryptoManager::SSL_CLIENT_ALPN, allowUntrusted, p_proto, expKP) 
//             : new Socket(/*Socket::TYPE_TCP*/));
    std::unique_ptr<Socket> s(secure ? (natRole == NAT_SERVER ? 
		CryptoManager::getInstance()->getServerSocket(allowUntrusted) : 
		CryptoManager::getInstance()->getClientSocket(allowUntrusted, proto)) : new Socket);

	s->create(); // в AirDC++ нет такой херни... разобраться
	
	setSocket(move(s));
	sock->bind(localPort, SETTING(BIND_ADDRESS));
	
	m_is_disconnecting = false;
	myInfoLoaded = proto == Socket::PROTO_HTTP;
	myInfoCount = 0;
	protocol = proto;

	addTask(CONNECT, new ConnectInfo(address, port, localPort, natRole, proxy && (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)));
}

static const uint16_t LONG_TIMEOUT = 30000;
static const uint16_t SHORT_TIMEOUT = 1000;

void BufferedSocket::threadConnect(const string& aAddr, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool proxy)
{
	m_count_search_ddos = 0;
	dcassert(state == STARTING);
	
	if (listener) listener->onConnecting();
	const uint64_t endTime = GET_TICK() + LONG_TIMEOUT;
	state = RUNNING;
	do
	{
		if (socketIsDisconnecting())
			break;
			
		dcdebug("threadConnect attempt to addr \"%s\"\n", aAddr.c_str());
		try
		{
			if (proxy)
			{
				sock->socksConnect(aAddr, aPort, LONG_TIMEOUT);
			}
			else
			{
				sock->connect(aAddr, aPort); // https://www.box.net/shared/l08o2vdekthrrp319m8n + http://www.flylinkdc.ru/2012/10/ashampoo-firewall.html
			}
			
			setOptions();
			
			while (true)
			{
#ifndef FLYLINKDC_HE
				if (ClientManager::isBeforeShutdown())
				{
					throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
				}
#endif
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
			if (natRole == NAT_NONE)
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
	
	const uint64_t startTime = GET_TICK();
	while (!sock->waitAccepted(POLL_TIMEOUT))
	{
		if (socketIsDisconnecting())
			return;
			
		if ((startTime + LONG_TIMEOUT) < GET_TICK())
		{
			throw SocketException(STRING(CONNECTION_TIMEOUT));
		}
	}
}

bool BufferedSocket::parseSearch(const string::size_type posNextSeparator, const string& line,
                                 CFlySearchArrayTTH& listTTH, CFlySearchArrayFile& listFile, bool doTrace)
{
	if (doTrace)
		LogManager::commandTrace(line.substr(0, posNextSeparator), true, getServerAndPort());
	if (m_is_disconnecting)
		return false;
	if (line.length() < 8)
		return false;
	if (line[0] != '$' || line[1] != 'S')
		return false;
	if (protocol == Socket::PROTO_HTTP)
		return false;
	if (ShareManager::g_is_initial)
	{
#ifdef _DEBUG
		LogManager::message("[ShareManager::g_is_initial] BufferedSocket::filterSearch line = " + line);
#endif
		return true;
	}
	if (m_is_hide_share || ClientManager::isStartup())
	{
		return true;
	}
	if (line.compare(2, 6, "earch ", 6) == 0)
	{
		const string lineItem = line.substr(0, posNextSeparator);
		auto pos = lineItem.find("?0?9?TTH:");
		// TODO научиться обрабатывать лимит по размеру вида
		// "x.x.x.x:yyy T?F?57671680?9?TTH:A3VSWSWKCVC4N6EP2GX47OEMGT5ZL52BOS2LAHA"
		if (pos != string::npos && pos > 5 &&
		    lineItem[pos - 4] == ' ' &&
		    lineItem.length() >= pos + 9 + 39
		   ) // Поправка на полную команду  F?T?0?9?TTH: или F?F?0?9?TTH: или T?T?0?9?TTH:
		{
			pos -= 4;
			const TTHValue tth(lineItem.c_str() + pos + 13, 39);
			//dcassert(l_tth == l_tth_orig);
			if (!ShareManager::isUnknownTTH(tth))
			{
				string searchStr = lineItem.substr(8, pos - 8);
				dcassert(searchStr.length() > 4);
				if (searchStr.length() > 4)
				{
					dcassert(searchStr.find('|') == string::npos && searchStr.find('$') == string::npos);
					listTTH.emplace_back(tth, searchStr);
				}
			}
			else
			{
				COMMAND_DEBUG("[TTH][FastSkip]" + lineItem, DebugTask::HUB_IN, getServerAndPort());
			}
		}
		else
		{
			if (!Util::isValidSearch(lineItem))
			{
				if (!m_count_search_ddos)
				{
					const string error = "[" + Util::formatDigitalDate() + "] BufferedSocket::all_search_parser DDoS $Search command: " + lineItem + " Hub IP = " + getIp();
					LogManager::message(error);
					if (!m_count_search_ddos)
					{
						if (listener) listener->onDDoSSearchDetect(error);
					}
					m_count_search_ddos++;
				}
				COMMAND_DEBUG("[DDoS] " + lineItem, DebugTask::HUB_IN, getServerAndPort());
				return true;
			}
#if 0
			auto l_marker_file = l_line_item.find(' ', 8);
			if (l_marker_file == string::npos || l_line_item.size() <= 12)
			{
				const string l_error = "BufferedSocket::all_search_parser error format $Search command: " + l_line_item + " Hub IP = " + getIp();
				CFlyServerJSON::pushError(19, l_error);
				LogManager::message(l_error);
				return true;
			}
#endif
#ifdef _DEBUG
//            LogManager::message("BufferedSocket::all_search_parser Skip unknown file = " + aString);
#endif
			CFlySearchItemFile item;
			string str = lineItem.substr(8);
			int errorLevel;
			if (item.parseNMDCSearch(str, errorLevel))
			{
				if (ShareManager::getCountSearchBot(item) > 1)
				{
					COMMAND_DEBUG("[File][SearchBot-BAN]" + lineItem, DebugTask::HUB_IN, getServerAndPort());
				}
				else if (ShareManager::isUnknownFile(item.getRawQuery()))
				{
					COMMAND_DEBUG("[File][FastSkip]" + lineItem, DebugTask::HUB_IN, getServerAndPort());
				}
				else
				{
					listFile.push_back(item);
				}
			}
			else
			{
#ifdef _DEBUG
				LogManager::message("BufferedSocket::all_search_parser error is_parse_nmdc_search = " + item.m_raw_search + " m_error_level = " + Util::toString(errorLevel));
#endif
			}
		}
		return true;
	}
	if (line.length() >= 45 && line[3] == ' ' && (line[2] == 'P' || line[2] == 'A') && line[43] == ' ')
	{
		const TTHValue tth(line.c_str() + 4, 39);
		if (!ShareManager::isUnknownTTH(tth))
		{
			auto endPos = line.find('|', 43);
			if (endPos != string::npos)
			{
				string searchStr = line.substr(44, endPos - 44);
				if (line[2] == 'P')
					searchStr = "Hub:" + searchStr;
				dcassert(searchStr.length() > 4);
				if (searchStr.length() > 4)
				{
					listTTH.emplace_back(tth, searchStr);
				}
			}
		}
		else
		{
			string lineItem = line.substr(0, posNextSeparator);
			COMMAND_DEBUG("[TTHS][FastSkip]" + lineItem, DebugTask::HUB_IN, getServerAndPort());
		}
		return true;
	}
	return false;
}

void BufferedSocket::parseMyInfo(const string::size_type posNextSeparator, const string& line, StringList& listMyInfo, bool isZOn)
{
	const bool processMyInfo = !myInfoLoaded && line.compare(0, 8, "$MyINFO ", 8) == 0;
	const string lineItem = processMyInfo ? line.substr(8, posNextSeparator - 8) : line.substr(0, posNextSeparator);
	if (!myInfoLoaded)
	{
		if (processMyInfo)
		{
			dcassert(lineItem.compare(0, 5, "$ALL ", 5) == 0);
			if (!lineItem.empty())
			{
				++myInfoCount;
				listMyInfo.push_back(lineItem);
			}
		}
		else if (myInfoCount)
		{
			if (!listMyInfo.empty() && !m_is_disconnecting && listener)
				listener->onMyInfoArray(listMyInfo);
			myInfoLoaded = true;
		}
	}
	if (listMyInfo.empty())
	{
		if (!isZOn && lineItem.compare(0, 4, "$ZOn", 4) == 0)
		{
			setMode(MODE_ZPIPE);
		}
		else
		{
			dcassert(mode == MODE_LINE || mode == MODE_ZPIPE);
#ifdef _DEBUG
			if (!lineItem.empty() && ClientManager::isBeforeShutdown())
			{
				if (!(lineItem[0] == '<' || lineItem[0] == '$' || lineItem[lineItem.length() - 1] == '|'))
				{
					LogManager::message("OnLine: " + lineItem);
				}
			}
#endif
			if (!ClientManager::isBeforeShutdown())
			{
				//dcassert(m_is_disconnecting == false)
				if (!m_is_disconnecting && listener)
					listener->onDataLine(lineItem);
			}
		}
	}
}

/*
#ifdef FLYLINKDC_EMULATOR_4000_USERS
                                    static bool g_is_test = false;
                                    const int l_count_guest = 4000;
                                    if (!g_is_test)
                                    {
                                        g_is_test = true;
                                        for (int i = 0; i < l_count_guest; ++i)
                                        {
                                            char bbb[200];
                                            snprintf(bbb, sizeof(bbb), "$ALL Guest%d <<Peers V:(r622),M:P,H:1/0/0,S:15,C:Кемерово>$ $%c$$3171624055$", i, 5);
                                            listMyInfo.push_back(bbb);
                                        }
                                    }
#endif // FLYLINKDC_EMULATOR_4000_USERS
#ifdef FLYLINKDC_EMULATOR_4000_USERS
// Генерируем случайные IP-адреса
                                    for (int i = 0; i < l_count_guest; ++i)
                                    {
                                        char bbb[200];
                                        boost::system::error_code ec;
                                        const auto l_start = boost::asio::ip::address_v4::from_string("200.23.17.18", ec);
                                        const auto l_stop = boost::asio::ip::address_v4::from_string("240.200.17.18", ec);
                                        boost::asio::ip::address_v4 l_rnd_ip(Util::rand(l_start.to_ulong(), l_stop.to_ulong()));
                                        snprintf(bbb, sizeof(bbb), "$UserIP Guest%d %s$$", i, l_rnd_ip.to_string().c_str());
                                        fly_fire1(BufferedSocketListener::Line(), bbb);
                                    }
#endif

*/

void BufferedSocket::giveMyInfo(StringList& myInfoList)
{
	if (m_is_disconnecting) return;	
	if (!myInfoList.empty() && listener)
		listener->onMyInfoArray(myInfoList);
}

void BufferedSocket::giveSearch(CFlySearchArrayTTH& listTTH, CFlySearchArrayFile& listFile)
{
	if (m_is_disconnecting) return;
	if (!listTTH.empty() && listener)
		listener->onSearchArrayTTH(listTTH);
	if (!listFile.empty() && listener)
		listener->onSearchArrayFile(listFile);
}

void BufferedSocket::threadRead()
{
	if (state != RUNNING)
		return;
	try
	{
		bool doTrace = BOOLSETTING(LOG_COMMAND_TRACE);
		int l_left = mode == MODE_DATA ? ThrottleManager::getInstance()->read(sock.get(), &inbuf[0], (int)inbuf.size()) : sock->read(&inbuf[0], (int)inbuf.size());
		if (l_left == -1)
		{
			// EWOULDBLOCK, no data received...
			return;
		}
		else if (l_left == 0)
		{
			// This socket has been closed...
			throw SocketException(STRING(CONNECTION_CLOSED));
		}
		
		string::size_type l_pos = 0;
		// always uncompressed data
		string l;
		int l_bufpos = 0;
		int l_total = l_left;
		while (l_left > 0)
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
					while (l_left)
					{
						size_t in = BUF_SIZE;
						size_t used = l_left;
						bool ret = (*filterIn)(&inbuf[0] + l_total - l_left, used, buffer, in);
						l_left -= used;
						l.append(buffer, in);
						// if the stream ends before the data runs out, keep remainder of data in inbuf
						if (!ret)
						{
							l_bufpos = l_total - l_left;
							setMode(MODE_LINE, m_rollback);
							break;
						}
					}
					// process all lines
#define FLYLINKDC_USE_MYINFO_ARRAY
#ifdef FLYLINKDC_USE_MYINFO_ARRAY
#ifdef _DEBUG
					// LogManager::message("BufferedSocket::threadRead[MODE_ZPIPE] = " + l);
#endif
					//
					if (!ClientManager::isBeforeShutdown())
					{
						StringList listMyInfo;
						CFlySearchArrayTTH listTTH;
						CFlySearchArrayFile listFile;
						while ((zpos = l.find(separator)) != string::npos)
						{
							if (zpos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
							{
								if (!parseSearch(zpos, l, listTTH, listFile, doTrace))
									parseMyInfo(zpos, l, listMyInfo, true);
							}
							l.erase(0, zpos + 1 /* separator char */); //[3] https://www.box.net/shared/74efa5b96079301f7194
						}
						giveMyInfo(listMyInfo);
						giveSearch(listTTH, listFile);
#else
						// process all lines
						while ((pos = l.find(separator)) != string::npos)
						{
							if (pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
								fly_fire1(__FUNCTION__, BufferedSocketListener::Line(), l.substr(0, pos));
							l.erase(0, pos + 1 /* separator char */); // TODO не эффективно
						}
#endif
					}
					else
					{
#ifdef _DEBUG
						const string l_log = "Skip MODE_LINE [after ZLIB] - FlylinkDC++ Destroy... l = " + l;
						LogManager::message(l_log);
#endif
						throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
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
					//======================================================================
					// TODO - вставить быструю обработку поиска по TTH без вызова листенеров
					// Если пасив - отвечаем в буфер сразу
					// Если актив - кидаем отсылку UDP (тоже через очередь?)
					//======================================================================
					l = m_line + string((char*)& inbuf[l_bufpos], l_left);
					//dcassert(isalnum(l[0]) || isalpha(l[0]) || isascii(l[0]));
#if 0
					int l_count_separator = 0;
#endif
#ifdef _DEBUG
					//LogManager::message("MODE_LINE . m_line = " + m_line);
					//LogManager::message("MODE_LINE = " + l);
#endif
					if (!ClientManager::isBeforeShutdown())
					{
						StringList listMyInfo;
						CFlySearchArrayTTH listTTH;
						CFlySearchArrayFile listFile;
						while ((l_pos = l.find(separator)) != string::npos)
						{
#if 0
							if (l_count_separator++ && l.length() > 0 && BOOLSETTING(LOG_PROTOCOL_MESSAGES))
							{
								StringMap params;
								const string l_log = "MODE_LINE l_count_separator = " + Util::toString(l_count_separator) + " l_left = " + Util::toString(l_left) + " l.length()=" + Util::toString(l.length()) + " l = " + l;
								LogManager::message(l_log);
							}
#endif
							if (ClientManager::isBeforeShutdown())
							{
								m_line.clear();
								throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
							}
							if (l_pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
							{
								if (!parseSearch(l_pos, l, listTTH, listFile, doTrace))
									parseMyInfo(l_pos, l, listMyInfo, false);
							}
							l.erase(0, l_pos + 1 /* separator char */);
							// TODO - erase не эффективно.
							if (l.length() < (size_t)l_left)
							{
								l_left = l.length();
							}
							//dcassert(mode == MODE_LINE);
							if (mode != MODE_LINE)
							{
								// dcassert(mode == MODE_LINE);
								// TOOD ? m_myInfoStop = true;
								// we changed mode; remainder of l is invalid.
								l.clear();
								l_bufpos = l_total - l_left;
								break;
							}
						}
						giveMyInfo(listMyInfo);
						giveSearch(listTTH, listFile);
					}
					else
					{
#ifdef _DEBUG
						const string l_log = "Skip MODE_LINE [normal] - FlylinkDC++ Destroy... l = " + l;
						LogManager::message(l_log);
#endif
						l.clear();
						l_bufpos = l_total - l_left;
						l_left = 0;
						l_pos = string::npos;
						m_line.clear();
						throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
					}
					
					if (l_pos == string::npos)
					{
						l_left = 0;
					}
					m_line = l;
					break;
				}
				case MODE_DATA:
					while (l_left > 0)
					{
						if (dataBytes == -1)
						{							
							if (listener)
								listener->onData(&inbuf[l_bufpos], l_left);
							l_bufpos += (l_left - m_rollback);
							l_left = m_rollback;
							m_rollback = 0;
						}
						else
						{
							const int high = (int)min(dataBytes, (int64_t)l_left);
							//dcassert(high != 0);
							if (high != 0) // [+] IRainman fix.
							{
								if (listener)
									listener->onData(&inbuf[l_bufpos], high);
								l_bufpos += high;
								l_left -= high;								
								dataBytes -= high;
							}
							if (dataBytes == 0)
							{
								mode = MODE_LINE;
#ifdef _DEBUG
								LogManager::message("BufferedSocket:: = MODE_LINE [1]");
#endif;
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
		m_is_disconnecting = true;
	myInfoLoaded = false;
	addTask(DISCONNECT, nullptr);
}

boost::asio::ip::address_v4 BufferedSocket::getIp4() const
{
	if (hasSocket())
	{
		boost::system::error_code ec;
		const auto l_ip = boost::asio::ip::address_v4::from_string(sock->getIp(), ec); // TODO - конвертнуть IP и в сокетах
		dcassert(!ec);
		return l_ip;
	}
	else
	{
		return boost::asio::ip::address_v4();
	}
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
	if (BOOLSETTING(LOG_COMMAND_TRACE))
	{
		if (len > 512)
		{
			string truncatedMsg(buf, 512 - 11);
			truncatedMsg.append("<TRUNCATED>", 11);
			LogManager::commandTrace(truncatedMsg, false, getServerAndPort());
		}
		else
			LogManager::commandTrace(string(buf, len), false, getServerAndPort());
	}
	CFlyFastLock(cs);
	if (writeBuf.empty())
	{
		addTaskL(SEND_DATA, nullptr);
	}
#ifdef _DEBUG
	if (len > 1)
	{
		dcassert(!(buf[len - 1] == '|' && buf[len - 2] == '|'));
	}
#endif
	writeBuf.reserve(writeBuf.size() + len);
	writeBuf.insert(writeBuf.end(), buf, buf + len); // [1] std::bad_alloc nomem https://www.box.net/shared/nmobw6wofukhcdr7lx4h
}

void BufferedSocket::threadSendData()
{
	if (state != RUNNING)
		return;
	ByteVector l_sendBuf;
	{
		CFlyFastLock(cs);
		if (writeBuf.empty())
		{
			dcassert(!writeBuf.empty());
			return;
		}
		writeBuf.swap(l_sendBuf);
	}
	
	size_t left = l_sendBuf.size();
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
			const int n = sock->write(&l_sendBuf[done], left); // adguard - https://www.box.net/shared/9201edaa1fa1b83a8d3c
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
	while (state == RUNNING ? socketSemaphore.wait(0) : socketSemaphore.wait())
	{
		//dcassert(!ClientManager::isShutdown());
		pair<Tasks, std::unique_ptr<TaskData>> p;
		{
			CFlyFastLock(cs);
			if (!tasks.empty())
			{
				p = std::move(tasks.front());
				tasks.pop_front();
			}
			else
			{
				dcassert(!tasks.empty());
				return false;
			}
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
				threadSendFile(static_cast<SendFileInfo*>(p.second.get())->m_stream);
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
				threadConnect(ci->addr, ci->port, ci->localPort, ci->natRole, ci->proxy);
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
	bool doLog = BOOLSETTING(LOG_SYSTEM);
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
		// dcassert(!ClientManager::isBeforeShutdown());
		// fix https://drdump.com/Problem.aspx?ProblemID=112938
		// fix https://drdump.com/Problem.aspx?ProblemID=112262
		// fix https://drdump.com/Problem.aspx?ProblemID=112195
		// Нельзя - вешаемся if (!ClientManager::isBeforeShutdown())
		{
			if (listener) listener->onFailed(aError);
		}
	}
}

void BufferedSocket::shutdown()
{
	m_is_disconnecting = true;
	addTask(SHUTDOWN, nullptr);
}

void BufferedSocket::addTask(Tasks task, TaskData* data)
{
	CFlyFastLock(cs);
	addTaskL(task, data);
}

void BufferedSocket::addTaskL(Tasks task, TaskData* data)
{
	dcassert(task == DISCONNECT || task == SHUTDOWN || task == UPDATED || sock.get());
	if (task == DISCONNECT && !tasks.empty())
	{
		if (tasks.back().first == DISCONNECT)
			return;
	}
	if (task == SHUTDOWN && !tasks.empty())
	{
		if (tasks.back().first == SHUTDOWN)
			return;
	}
#ifdef _DEBUG
	if (task == UPDATED && !tasks.empty())
	{
		if (tasks.back().first == UPDATED)
		{
			dcassert(0);
			return;
		}
	}
	if (task == SEND_DATA && !tasks.empty())
	{
		if (tasks.back().first == SEND_DATA)
		{
			dcassert(0);
			return;
		}
	}
	if (task == ACCEPTED && !tasks.empty())
	{
		if (tasks.back().first == ACCEPTED)
		{
			dcassert(0);
			return;
		}
	}
#endif
	
	tasks.push_back(std::make_pair(task, std::unique_ptr<TaskData>(data)));
	socketSemaphore.signal();
}
