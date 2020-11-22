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
#include "SearchManager.h"
#include "UploadManager.h"
#include "ShareManager.h"
#include "SearchResult.h"
#include "QueueManager.h"
#include "SimpleStringTokenizer.h"
#include "FinishedManager.h"
#include "DebugManager.h"
#include "PortTest.h"
#include "ConnectivityManager.h"
#include "LogManager.h"
#include "dht/DHT.h"

uint16_t SearchManager::g_search_port = 0;

ResourceManager::Strings SearchManager::getTypeStr(int type)
{
	static ResourceManager::Strings types[NUMBER_OF_FILE_TYPES] =
	{
		ResourceManager::ANY,
		ResourceManager::AUDIO,
		ResourceManager::COMPRESSED,
		ResourceManager::DOCUMENT,
		ResourceManager::EXECUTABLE,
		ResourceManager::PICTURE,
		ResourceManager::VIDEO_AND_SUBTITLES,
		ResourceManager::DIRECTORY,
		ResourceManager::TTH,
		ResourceManager::CD_DVD_IMAGES,
		ResourceManager::COMICS,
		ResourceManager::BOOK,
	};
	return types[type];
}

SearchManager::SearchManager(): stopFlag(false), failed(false)
{
	events[EVENT_SOCKET].create();
	events[EVENT_COMMAND].create();
}

void SearchManager::start()
{
	stopFlag = false;
	events[EVENT_COMMAND].reset();
	try
	{
		socket.reset(new Socket);
		socket->create(Socket::TYPE_UDP);
		socket->setInBufSize();
		if (BOOLSETTING(AUTO_DETECT_CONNECTION))
		{
			g_search_port = socket->bind(0, Util::emptyString);
		}
		else
		{
			string ip = SETTING(BIND_ADDRESS);
			int port = SETTING(UDP_PORT);
			g_search_port = socket->bind(static_cast<uint16_t>(port), ip);
		}
		socket_t nativeSocket = socket->getSock();
		WSAEventSelect(nativeSocket, events[EVENT_SOCKET].getHandle(), FD_READ);
		SET_SETTING(UDP_PORT, g_search_port);
		Thread::start(64, "SearchManager");
		if (failed)
		{
			LogManager::message(STRING(SEARCH_ENABLED));
			failed = false;
		}

#if 0 // superseded by ConnectivityManager::testPorts
		int unusedPort;
		if (g_portTest.getState(PortTest::PORT_UDP, unusedPort, nullptr) == PortTest::STATE_UNKNOWN)
		{
			g_portTest.setPort(PortTest::PORT_UDP, g_search_port);
			g_portTest.runTest(1<<PortTest::PORT_UDP);
		}
#endif
	}
	catch (const Exception& e)
	{
		socket.reset();
		if (!failed)
		{
			LogManager::message(STRING(SEARCH_DISABLED) + ": " + e.getError());
			failed = true;
		}
	}
}

void SearchManager::shutdown()
{
	stopFlag = true;
	events[EVENT_COMMAND].notify();
	join();
	if (socket.get())
	{
		socket->disconnect();
		socket.reset();
	}
	g_search_port = 0;
	LogManager::message("SearchManager: shutdown completed", false);
}

int SearchManager::run()
{
	static const int BUFSIZE = 8192;
	char buf[BUFSIZE];

	while (!isShutdown())
	{
		socket_t nativeSocket = socket->getSock();
		WinEvent<FALSE>::waitMultiple(events, 2);
		for (;;)
		{
			sockaddr_in remoteAddr;
			socklen_t addrLen = sizeof(remoteAddr);
			int len = ::recvfrom(nativeSocket, buf, BUFSIZE, 0, (struct sockaddr*) &remoteAddr, &addrLen);
			if (isShutdown()) return 0;
			if (len < 0)
			{
				if (Socket::getLastError() == WSAEWOULDBLOCK)
				{
					processSendQueue();
					break;
				}
			}
			if (len >= 4)
			{
				const boost::asio::ip::address_v4 ip4(ntohl(remoteAddr.sin_addr.s_addr));
				onData(buf, len, ip4, ntohs(remoteAddr.sin_port));
			}
		}
	}
	return 0;
}

static inline bool isText(char ch)
{
	return ch >= 0x20 && !(ch & 0x80);
}

static inline bool isText(const char* buf, int len)
{
	return len > 4 && isText(buf[0]) && isText(buf[1]) && isText(buf[2]) && isText(buf[3]);
}

void SearchManager::onData(const char* buf, int len, boost::asio::ip::address_v4 remoteIp, uint16_t remotePort)
{
	if (BOOLSETTING(LOG_UDP_PACKETS) && isText(buf, len))
		LogManager::commandTrace(string(buf, len), LogManager::FLAG_IN | LogManager::FLAG_UDP,
			remoteIp.to_string() + ':' + Util::toString(remotePort));

	if (processNMDC(buf, len, remoteIp)) return;
	if (dht::DHT::getInstance()->processIncoming((const uint8_t *) buf, len, remoteIp, remotePort)) return;
	if (processRES(buf, len, remoteIp)) return;
	if (processPSR(buf, len, remoteIp)) return;
	processPortTest(buf, len, remoteIp);
}

bool SearchManager::processNMDC(const char* buf, int len, boost::asio::ip::address_v4 remoteIp)
{
	if (!memcmp(buf, "$SR ", 4))
	{
		string x(buf, len);
		string::size_type i = 4;
		string::size_type j;
		// Directories: $SR <nick><0x20><directory><0x20><free slots>/<total slots><0x05><Hubname><0x20>(<Hubip:port>)
		// Files:       $SR <nick><0x20><filename><0x05><filesize><0x20><free slots>/<total slots><0x05><Hubname><0x20>(<Hubip:port>)
		if ((j = x.find(' ', i)) == string::npos) return false;
		string nick = x.substr(i, j - i);
		i = j + 1;
				
		// A file has 2 0x05, a directory only one
		const auto fist05 = x.find(0x05, j);
		dcassert(fist05 != string::npos);
		if (fist05 == string::npos) return false;
		const auto second05 = x.find(0x05, fist05 + 1);
		SearchResult::Types type = SearchResult::TYPE_FILE;
		string file;
		int64_t size = 0;
				
		if (fist05 != string::npos && second05 == string::npos) // cnt == 1
		{
			// We have a directory...find the first space beyond the first 0x05 from the back
			// (dirs might contain spaces as well...clever protocol, eh?)
			type = SearchResult::TYPE_DIRECTORY;
			// Get past the hubname that might contain spaces
			j = fist05;
			// Find the end of the directory info
			if ((j = x.rfind(' ', j - 1)) == string::npos) return false;
			if (j < i + 1) return false;
			file = x.substr(i, j - i) + '\\';
		}
		else if (fist05 != string::npos && second05 != string::npos) // cnt == 2
		{
			j = fist05;
			file = x.substr(i, j - i);
			i = j + 1;
			if ((j = x.find(' ', i)) == string::npos) return false;
			size = Util::toInt64(x.substr(i, j - i));
		}
		i = j + 1;
				
		if ((j = x.find('/', i)) == string::npos) return false;
		int freeSlots = Util::toInt(x.substr(i, j - i));
		i = j + 1;
		if ((j = x.find((char)5, i)) == string::npos) return false;
		int slots = Util::toInt(x.substr(i, j - i));
		if (slots < 0) return false;
		i = j + 1;
		if ((j = x.rfind(" (")) == string::npos) return false;
		string hubNameOrTTH = x.substr(i, j - i);
		i = j + 2;
		if ((j = x.rfind(')')) == string::npos) return false;

		if (freeSlots < 0 || slots < 0) return true;
				
		const string hubIpPort = x.substr(i, j - i);
		const string url = ClientManager::findHub(hubIpPort); // TODO - внутри линейный поиск. оптимизнуть
		// Иногда вместо IP приходит домен "$SR chen video\multfilm\Ну, погоди!\Ну, погоди! 2.avi33492992 5/5TTH:B4O5M74UPKZ7I23CH36NA3SZOUZTJLWNVEIJMTQ (dc.a-galaxy.com:411)|"
		// Это не обрабатывается в функции - поправить.
		// для dc.dly-server.ru - возвращается его IP-шник "31.186.103.125:411"
		// url оказывается пустым https://www.box.net/shared/ayirspvdjk2boix4oetr
		// падаем на dcassert в следующем вызове findHubEncoding.
		// [!] IRainman fix: не падаем!!!! Это диагностическое предупреждение!!!
		// [-] string encoding;
		// [-] if (!url.empty())
			
		const bool isTTH = isTTHBase32(hubNameOrTTH);
		int encoding = ClientManager::findHubEncoding(url);
		if (encoding != Text::CHARSET_UTF8)
		{
			nick = Text::toUtf8(nick, encoding);
			file = Text::toUtf8(file, encoding);
			if (!isTTH)
				hubNameOrTTH = Text::toUtf8(hubNameOrTTH, encoding);
		}
					
		UserPtr user = ClientManager::findUser(nick, url); // TODO оптимизнуть makeCID
		// не находим юзера "$SR snooper-06 Фильмы\Прошлой ночью в Нью-Йорке.avi1565253632 15/15TTH:LUWOOXBE2H77TUV4S4HNZQTVDXLPEYC757OUMLY (31.186.103.125:411)"
		// при пустом url - можно не звать ClientManager::findUser - не найдем.
		// сразу нужно переходить на ClientManager::findLegacyUser
		// url не педедается при коннекте к хабу через SOCKS5
		// TODO - если хаб только один - пытаться подставлять его?
		if (!user)
		{
			// LogManager::message("Error ClientManager::findUser nick = " + nick + " url = " + url);
			// Could happen if hub has multiple URLs / IPs
			user = ClientManager::findLegacyUser(nick, url);
			if (!user)
			{
				//LogManager::message("Error ClientManager::findLegacyUser nick = " + nick + " url = " + url);
				return true;
			}
		}
		if (!remoteIp.is_unspecified())
		{
			user->setIP(remoteIp);
#ifdef _DEBUG
			//ClientManager::setIPUser(user, remoteIp); // TODO - может не нужно тут?
#endif
			// Тяжелая операция по мапе юзеров - только чтобы показать IP в списке ?
		}
		string tth;
		if (isTTH)
		{
			tth = hubNameOrTTH.substr(4);
		}
		if (tth.empty() && type == SearchResult::TYPE_FILE)
		{
			dcassert(tth.empty() && type == SearchResult::TYPE_FILE);
			return true;
		}
				
		SearchResult sr(user, type, slots, freeSlots, size, file, Util::emptyString, url, remoteIp, TTHValue(tth), 0);
		if (CMD_DEBUG_ENABLED())
			COMMAND_DEBUG("[Search-result] url = " + url + " remoteIp = " + remoteIp.to_string() + " file = " + file + " user = " + user->getLastNick(), DebugTask::CLIENT_IN, remoteIp.to_string());
		SearchManager::getInstance()->fly_fire1(SearchManagerListener::SR(), sr);
		return true;
	}
	return false;
}

bool SearchManager::processRES(const char* buf, int len, boost::asio::ip::address_v4 remoteIp)
{
	if (len >= 5 && !memcmp(buf + 1, "RES ", 4) && buf[len - 1] == 0x0a)
	{
		AdcCommand c(0);
		int parseResult = c.parse(string(buf, len-1));
		if (parseResult != AdcCommand::PARSE_OK)
		{
#ifdef _DEBUG
			LogManager::message("[AdcCommand] Parse Error: " + Util::toString(parseResult) + " ip=" + remoteIp.to_string() + " data=[" + string(buf, len) + "]", false);
#endif
			return false;
		}
		if (c.getParameters().empty()) return false;
		const string& cid = c.getParam(0);
		if (cid.size() != 39) return false;
		UserPtr user = ClientManager::findUser(CID(cid));
		if (!user) return true;
		SearchManager::getInstance()->onRES(c, true, user, remoteIp);
		return true;
	}
	return false;
}

bool SearchManager::processPSR(const char* buf, int len, boost::asio::ip::address_v4 remoteIp)
{
	if (len >= 5 && !memcmp(buf + 1, "PSR ", 4) && buf[len - 1] == 0x0a)
	{
		AdcCommand c(0);
		int parseResult = c.parse(string(buf, len-1));
		if (parseResult != AdcCommand::PARSE_OK)
		{
#ifdef _DEBUG
			LogManager::message("[AdcCommand] Parse Error: " + Util::toString(parseResult) + " ip=" + remoteIp.to_string() + " data=[" + string(buf, len) + "]", false);
#endif
			return false;
		}
		if (c.getParameters().empty()) return false;
		const string& cid = c.getParam(0);
		if (cid.size() != 39) return false;
		const UserPtr user = ClientManager::findUser(CID(cid));
		// when user == NULL then it is probably NMDC user, check it later
		SearchManager::getInstance()->onPSR(c, true, user, remoteIp);
		return true;
	}
	return false;
}

bool SearchManager::processPortTest(const char* buf, int len, boost::asio::ip::address_v4 address)
{
	if (len >= 15 + 39 && !memcmp(buf, "$FLY-TEST-PORT ", 15))
	{
		string reflectedAddress(buf + 15 + 39, len - (15 + 39));
		if (!reflectedAddress.empty() && reflectedAddress.back() == '|')
		{
			reflectedAddress.erase(reflectedAddress.length()-1);
			string ip;
			uint16_t port = 0;
			Util::parseIpPort(reflectedAddress, ip, port);
			if (!(port && Util::isValidIp4(ip)))
				reflectedAddress.clear();
		}
		if (g_portTest.processInfo(PortTest::PORT_UDP, PortTest::PORT_UDP, 0, reflectedAddress, string(buf + 15, 39)))
			ConnectivityManager::getInstance()->processPortTestResult();
		return true;
	}
	return false;
}

void SearchManager::searchAuto(const string& tth)
{
	dcassert(tth.size() == 39);
	SearchParamToken sp;
	sp.fileType = FILE_TYPE_TTH;
	sp.filter = tth;
	sp.generateToken(true);
	ClientManager::search(sp);
}

void SearchManager::onRES(const AdcCommand& cmd, bool skipCID, const UserPtr& from, boost::asio::ip::address_v4 remoteIp)
{
	int freeSlots = -1;
	int64_t size = -1;
	string file;
	string tth;
	uint32_t token = 0;
	
	const auto& params = cmd.getParameters();
	for (StringList::size_type i = skipCID ? 1 : 0; i < params.size(); ++i)
	{
		const string& str = params[i];
		if (str.compare(0, 2, "FN", 2) == 0)
		{
			file = Util::toNmdcFile(str.c_str() + 2);
		}
		else if (str.compare(0, 2, "SL", 2) == 0)
		{
			freeSlots = Util::toInt(str.c_str() + 2);
		}
		else if (str.compare(0, 2, "SI", 2) == 0)
		{
			size = Util::toInt64(str.c_str() + 2);
		}
		else if (str.compare(0, 2, "TR", 2) == 0)
		{
			tth = str.substr(2);
		}
		else if (str.compare(0, 2, "TO", 2) == 0)
		{
			token = Util::toUInt32(str.c_str() + 2);
		}
	}
	
	if (!file.empty() && freeSlots != -1 && size != -1)
	{
		/// @todo get the hub this was sent from, to be passed as a hint? (eg by using the token?)
		const StringList names = ClientManager::getHubNames(from->getCID(), Util::emptyString);
		const string hubName = names.empty() ? STRING(OFFLINE) : Util::toString(names);
		const StringList hubs = ClientManager::getHubs(from->getCID(), Util::emptyString);
		const string hub = hubs.empty() ? STRING(OFFLINE) : Util::toString(hubs);
		
		const SearchResult::Types type = (file[file.length() - 1] == '\\' ? SearchResult::TYPE_DIRECTORY : SearchResult::TYPE_FILE);
		if (type == SearchResult::TYPE_FILE && tth.empty())
			return;
			
		const uint8_t slots = ClientManager::getSlots(from->getCID());
		SearchResult sr(from, type, slots, freeSlots, size, file, hubName, hub, remoteIp, TTHValue(tth), token);
		fly_fire1(SearchManagerListener::SR(), sr);
	}
}

void SearchManager::onPSR(const AdcCommand& cmd, bool skipCID, UserPtr from, boost::asio::ip::address_v4 remoteIp)
{
	uint16_t udpPort = 0;
	uint32_t partialCount = 0;
	string tth;
	string hubIpPort;
	string nick;
	PartsInfo partialInfo;
	
	const auto& params = cmd.getParameters();
	for (StringList::size_type i = skipCID ? 1 : 0; i < params.size(); ++i)
	{
		const string& str = params[i];
		if (str.compare(0, 2, "U4", 2) == 0)
		{
			udpPort = static_cast<uint16_t>(Util::toInt(str.substr(2)));
		}
		else if (str.compare(0, 2, "NI", 2) == 0)
		{
			nick = str.substr(2);
		}
		else if (str.compare(0, 2, "HI", 2) == 0)
		{
			hubIpPort = str.substr(2);
		}
		else if (str.compare(0, 2, "TR", 2) == 0)
		{
			tth = str.substr(2);
		}
		else if (str.compare(0, 2, "PC", 2) == 0)
		{
			partialCount = Util::toUInt32(str.c_str() + 2) * 2;
		}
		else if (str.compare(0, 2, "PI", 2) == 0)
		{
			SimpleStringTokenizer<char> st(str, ',', 2);
			string tok;
			while (st.getNextNonEmptyToken(tok))
				partialInfo.push_back((uint16_t) Util::toInt(tok));
		}
	}

	if (partialInfo.size() != partialCount)
		return; // Malformed command

	const string url = ClientManager::findHub(hubIpPort);
	if (!from || from->isMe())
	{
		// for NMDC support
		
		if (nick.empty() || hubIpPort.empty())
		{
			return;
		}
		from = ClientManager::findUser(nick, url); // TODO оптмизнуть makeCID
		if (!from)
		{
			// Could happen if hub has multiple URLs / IPs
			from = ClientManager::findLegacyUser(nick, url);
			if (!from)
			{
				dcdebug("Search result from unknown user");
				LogManager::psr_message("Error SearchManager::onPSR & ClientManager::findLegacyUser nick = " + nick + " url = " + url);
				return;
			}
			else
			{
				dcdebug("Search result from valid user");
				LogManager::psr_message("OK SearchManager::onPSR & ClientManager::findLegacyUser nick = " + nick + " url = " + url);
			}
		}
	}
	
#ifdef _DEBUG
	// ClientManager::setIPUser(from, remoteIp, udpPort);
#endif
	// TODO Ищем в OnlineUser а чуть выше ищем в UserPtr може тожно схлопнуть в один поиск для апдейта IP
	
	PartsInfo outPartialInfo;
	QueueItem::PartialSource ps((from->getFlags() & User::NMDC) ? ClientManager::findMyNick(url) : Util::emptyString, hubIpPort, remoteIp, udpPort, 0);
	ps.setPartialInfo(partialInfo);
	
	QueueManager::getInstance()->handlePartialResult(from, TTHValue(tth), ps, outPartialInfo);
	
	if (udpPort > 0 && !outPartialInfo.empty())
	{
		try
		{
			AdcCommand reply(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			toPSR(reply, false, ps.getMyNick(), hubIpPort, tth, outPartialInfo);
			ClientManager::sendAdcCommand(reply, from->getCID());
			LogManager::psr_message(
			    "[SearchManager::respond] hubIpPort = " + hubIpPort +
			    " ps.getMyNick() = " + ps.getMyNick() +
			    " tth = " + tth +
			    " outPartialInfo.size() = " + Util::toString(outPartialInfo.size())
			);
			
		}
		catch (const Exception& e)
		{
			dcdebug("Partial search caught error\n");
			LogManager::psr_message("Partial search caught error = " + e.getError());
		}
	}
	
}

ClientManagerListener::SearchReply SearchManager::respond(AdcSearchParam& param, const CID& from, const string& hubIpPort)
{
	// Filter own searches
	if (from == ClientManager::getMyCID())
		return ClientManagerListener::SEARCH_MISS;
	
	const UserPtr p = ClientManager::findUser(from);
	if (!p)
		return ClientManagerListener::SEARCH_MISS;
	
	vector<SearchResultCore> searchResults;
	ShareManager::getInstance()->search(searchResults, param);
	
	ClientManagerListener::SearchReply sr = ClientManagerListener::SEARCH_MISS;
	
	// TODO: don't send replies to passive users
	if (searchResults.empty())
	{
		if (!param.hasRoot)
			return sr;
			
		PartsInfo partialInfo;
		if (QueueManager::handlePartialSearch(param.root, partialInfo))
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			string tth = param.root.toBase32();
			toPSR(cmd, true, Util::emptyString, hubIpPort, tth, partialInfo);
			ClientManager::sendAdcCommand(cmd, from);
			sr = ClientManagerListener::SEARCH_PARTIAL_HIT;
			LogManager::psr_message(
			    "[SearchManager::respond] hubIpPort = " + hubIpPort +
			    " tth = " + tth +
			    " partialInfo.size() = " + Util::toString(partialInfo.size())
			);
		}
	}
	else
	{
		for (auto i = searchResults.cbegin(); i != searchResults.cend(); ++i)
		{
			AdcCommand cmd(AdcCommand::CMD_RES, AdcCommand::TYPE_UDP);
			i->toRES(cmd, UploadManager::getFreeSlots());
			if (!param.token.empty())
				cmd.addParam("TO", param.token);
			ClientManager::sendAdcCommand(cmd, from);
		}
		sr = ClientManagerListener::SEARCH_HIT;
	}
	return sr;
}

string SearchManager::getPartsString(const PartsInfo& partsInfo)
{
	string ret;
	
	for (auto i = partsInfo.cbegin(); i < partsInfo.cend(); i += 2)
	{
		ret += Util::toString(*i) + "," + Util::toString(*(i + 1)) + ",";
	}
	
	return ret.substr(0, ret.size() - 1);
}

void SearchManager::toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo)
{
	cmd.getParameters().reserve(6);
	if (!myNick.empty())
	{
		cmd.addParam("NI", Text::utf8ToAcp(myNick));
	}
	
	cmd.addParam("HI", hubIpPort);
	cmd.addParam("U4", Util::toString(wantResponse ? getSearchPortUint() : 0)); // Сюда по ошибке подався не урл к хабу. && ClientManager::isActive(hubIpPort)
	cmd.addParam("TR", tth);
	cmd.addParam("PC", Util::toString(partialInfo.size() / 2));
	cmd.addParam("PI", getPartsString(partialInfo));
}

bool SearchManager::isShutdown() const
{
	if (stopFlag.load()) return true;
	extern volatile bool g_isBeforeShutdown;
	return g_isBeforeShutdown;
}

void SearchManager::addToSendQueue(string& data, boost::asio::ip::address_v4 address, uint16_t port, uint16_t flags) noexcept
{
	csSendQueue.lock();
	sendQueue.emplace_back(data, address, port, flags);
	csSendQueue.unlock();
	events[EVENT_COMMAND].notify();
}

void SearchManager::processSendQueue() noexcept
{
	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	csSendQueue.lock();
	for (auto i = sendQueue.cbegin(); i != sendQueue.cend(); ++i)
	{
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_port = htons(i->port);
		sockAddr.sin_addr.s_addr = htonl(i->address.to_ulong());
		const string& data = i->data;
		::sendto(socket->getSock(), data.data(), data.length(), 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
		if (BOOLSETTING(LOG_UDP_PACKETS) && !(i->flags & FLAG_NO_TRACE))
			LogManager::commandTrace(data, LogManager::FLAG_UDP, i->address.to_string() + ':' + Util::toString(i->port));
	}
	sendQueue.clear();
	csSendQueue.unlock();
}
