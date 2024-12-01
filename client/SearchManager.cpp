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
#include "SettingsManager.h"
#include "SimpleStringTokenizer.h"
#include "FinishedManager.h"
#include "DebugManager.h"
#include "PortTest.h"
#include "ConnectivityManager.h"
#include "LogManager.h"
#include "NetworkUtil.h"
#include "NetworkDevices.h"
#include "SettingsUtil.h"
#include "ConfCore.h"
#include "dht/DHT.h"
#include "unaligned.h"
#include <openssl/evp.h>
#include <openssl/rand.h>

static const unsigned SUDP_KEY_EXPIRE_TIME = 15 * 60 * 1000;
static const unsigned SUDP_KEY_UPDATE_TIME = 10 * 60 * 1000;

uint16_t SearchManager::udpPort = 0;

#ifndef _WIN32
static void signalHandler(int sig, siginfo_t* inf, void* param)
{
	dcdebug("Got signal: %d\n", sig);
}
#endif

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

SearchManager::SearchManager(): stopFlag(false), failed{false, false}, options(0), decryptKeyLock(RWLock::create())
{
#ifdef _WIN32
	events[EVENT_COMMAND].create();
#else
	threadId = 0;
#endif
	lastDecryptState = -1;
	newKeyTime = 0;
	updateSettings();
}

uint16_t SearchManager::getSearchPort(int af)
{
	uint16_t port = getLocalPort();
	if (!port)
		return 0;
	int rport = ConnectivityManager::getInstance()->getReflectedPort(af, AppPorts::PORT_UDP);
	if (rport)
		port = rport;
	return port;
}

void SearchManager::listenUDP(int af)
{
	int index = af == AF_INET6 ? 1 : 0;
	Conf::IPSettings ips;
	Conf::getIPSettings(ips, af == AF_INET6);
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const bool autoDetectFlag = ss->getBool(ips.autoDetect);
	const int portSetting = ss->getInt(Conf::UDP_PORT);
	const int bindOptions = ss->getInt(ips.bindOptions);
	const string bindDev = ss->getString(ips.bindDevice);
	const string bindAddr = ss->getString(ips.bindAddress);
	ss->unlockRead();
#ifdef _WIN32
	events[index].create();
#endif
	try
	{
		sockets[index].reset(new Socket);
		sockets[index]->create(af, Socket::TYPE_UDP);
		sockets[index]->setBufSize(Socket::IN_BUFFER);

		IpAddressEx bindIp;
		memset(&bindIp, 0, sizeof(bindIp));
		if (autoDetectFlag)
		{
			bindIp.type = af;
			udpPort = sockets[index]->bind(static_cast<uint16_t>(portSetting), bindIp);
		}
		else
		{
			if (bindOptions & Conf::BIND_OPTION_USE_DEV)
			{
				if (!bindDev.empty() && !networkDevices.getDeviceAddress(af, bindDev, bindIp) && (bindOptions & Conf::BIND_OPTION_NO_FALLBACK))
					throw SocketException(STRING_F(NETWORK_DEVICE_NOT_FOUND, bindDev));
			}
			if (!bindIp.type)
				BufferedSocket::getBindAddress(bindIp, af, bindAddr);
			udpPort = sockets[index]->bind(static_cast<uint16_t>(portSetting), bindIp);
		}

		if (af == AF_INET)
		{
			IpAddress localIp = sockets[index]->getLocalIp();
			if (!Util::isValidIp(localIp))
			{
				auto ip = Util::getLocalIp(af);
				memcpy(&localIp, &ip, sizeof(IpAddress));
			}
			dht::DHT::getInstance()->updateLocalIP(localIp);
		}

#ifdef _WIN32
		socket_t nativeSocket = sockets[index]->getSock();
		WSAEventSelect(nativeSocket, events[index].getHandle(), FD_READ);
#else
		sockets[index]->setBlocking(false);
#endif
		ss->lockWrite();
		ss->setInt(Conf::UDP_PORT, udpPort);
		ss->unlockWrite();
		if (failed[index])
		{
			LogManager::message(STRING(SEARCH_ENABLED));
			failed[index] = false;
		}
	}
	catch (const Exception& e)
	{
		sockets[index].reset();
		if (!failed[index])
		{
			LogManager::message(STRING(SEARCH_DISABLED) + ": " + e.getError());
			failed[index] = true;
		}
		throw;
	}
}

void SearchManager::start()
{
	stopFlag = false;
	if (sockets[0].get() || sockets[1].get())
	{
#ifdef _WIN32
		events[EVENT_COMMAND].reset();
#endif
		Thread::start(64, "SearchManager");
	}
}

void SearchManager::shutdown()
{
	bool printMessage = false;
	stopFlag = true;
	sendNotif();
	join();
	for (int i = 0; i < 2; ++i)
	{
		if (sockets[i])
		{
			sockets[i]->disconnect();
			sockets[i].reset();
			printMessage = true;
		}
	}
	udpPort = 0;
	if (printMessage) LogManager::message("SearchManager: shutdown completed", false);
}

int SearchManager::run()
{
	int numEvents = 0;
#ifdef _WIN32
	HANDLE handle[3];
	int eventInfo[3];
	for (int i = 0; i < 3; ++i)
	{
		HANDLE h = events[i].getHandle();
		if (h)
		{
			handle[numEvents] = h;
			eventInfo[numEvents++] = i;
		}
	}
#else
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = signalHandler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &sa, nullptr);
	threadId = pthread_self();

	pollfd pfd[2];
	int eventInfo[2];
	for (int i = 0; i < 2; ++i)
		if (sockets[i])
		{
			pfd[numEvents].fd = sockets[i]->getSock();
			pfd[numEvents].events = POLLIN;
			pfd[numEvents].revents = 0;
			eventInfo[numEvents++] = i;
		}
#endif
	dcassert(numEvents);

	while (!isShutdown())
	{
#ifdef _WIN32
		int result = (int) WaitForMultipleObjects(numEvents, handle, FALSE, INFINITE) - (int) WAIT_OBJECT_0;
		if (result >= 0 && result < numEvents)
		{
			int index = eventInfo[result];
			if ((index == 0 || index == 1) && receivePackets(index)) goto terminate;
		}
#else
		poll(pfd, numEvents, -1);
		for (int i = 0; i < numEvents; ++i)
			if (pfd[i].revents & POLLIN)
			{
				if (receivePackets(eventInfo[i])) goto terminate;
			}
#endif
		processSendQueue();
	}

	terminate:
	g_portTest.resetState(1<<AppPorts::PORT_UDP);
	return 0;
}

bool SearchManager::receivePackets(int index)
{
	static const int BUFSIZE = 8192;
	char buf[BUFSIZE];
	IpAddress remoteIp;
	uint16_t remotePort;
	Socket& socket = *sockets[index].get();
	for (;;)
	{
		int len = socket.receivePacket(buf, BUFSIZE, remoteIp, remotePort);
		if (isShutdown())
			return true;
		if (len < 0 && Socket::getLastError() == SE_EWOULDBLOCK)
			break;
		if (len >= 4)
			onData(buf, len, remoteIp, remotePort);
	}
	return false;
}

static inline bool isText(char ch)
{
	return ch >= 0x20 && !(ch & 0x80);
}

static inline bool isText(const char* buf, int len)
{
	return len > 4 && isText(buf[0]) && isText(buf[1]) && isText(buf[2]) && isText(buf[3]);
}

void SearchManager::onData(const char* buf, int len, const IpAddress& remoteIp, uint16_t remotePort)
{
	if ((LogManager::getLogOptions() & LogManager::OPT_LOG_UDP_PACKETS) && isText(buf, len))
		LogManager::commandTrace(buf, len, LogManager::FLAG_IN | LogManager::FLAG_UDP,
			Util::printIpAddress(remoteIp, true), remotePort);

	if ((getOptions() & OPT_ENABLE_SUDP) && processSUDP(buf, len, remoteIp, remotePort)) return;
	if (processNMDC(buf, len, remoteIp, remotePort)) return;
	if (remoteIp.type == AF_INET &&
	    dht::DHT::getInstance()->processIncoming((const uint8_t *) buf, len, remoteIp.data.v4, remotePort)) return;
	if (processRES(buf, len, remoteIp)) return;
	if (processPSR(buf, len, remoteIp)) return;
	processPortTest(buf, len, remoteIp);
}

bool SearchManager::processNMDC(const char* buf, int len, const IpAddress& remoteIp, uint16_t remotePort)
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
		const auto first05 = x.find(0x05, j);
		if (first05 == string::npos) return false;
		const auto second05 = x.find(0x05, first05 + 1);
		SearchResult::Types type = SearchResult::TYPE_FILE;
		string file;
		int64_t size = 0;

		if (first05 != string::npos && second05 == string::npos) // cnt == 1
		{
			// We have a directory...find the first space beyond the first 0x05 from the back
			// (dirs might contain spaces as well...clever protocol, eh?)
			type = SearchResult::TYPE_DIRECTORY;
			// Get past the hubname that might contain spaces
			j = first05;
			// Find the end of the directory info
			if ((j = x.rfind(' ', j - 1)) == string::npos) return false;
			if (j < i + 1) return false;
			file = x.substr(i, j - i) + '\\';
		}
		else if (first05 != string::npos && second05 != string::npos) // cnt == 2
		{
			j = first05;
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
		string url = ClientManager::findHub(hubIpPort, ClientBase::TYPE_NMDC); // check all connected hubs

		const bool isTTH = Util::isTTHBase32(hubNameOrTTH);
		const bool convertNick = !Text::isAscii(nick);
		const bool convertFile = !Text::isAscii(file);
		if (convertNick || convertFile)
		{
			int encoding = ClientManager::findHubEncoding(url);
			if (encoding != Text::CHARSET_UTF8)
			{
				if (convertNick) nick = Text::toUtf8(nick, encoding);
				if (convertFile) file = Text::toUtf8(file, encoding);
			}
		}
					
		UserPtr user;
		if (!url.empty())
			user = ClientManager::findUser(nick, url);
		else
		if (LogManager::getLogOptions() & LogManager::OPT_LOG_SYSTEM)
			LogManager::message("Unknown Hub IP: " + hubIpPort, false);
		if (!user)
		{
			// LogManager::message("Error ClientManager::findUser nick = " + nick + " url = " + url);
			// Could happen if hub has multiple URLs / IPs
			user = ClientManager::findLegacyUser(nick, url, &url);
			if (!user)
			{
				//LogManager::message("Error ClientManager::findLegacyUser nick = " + nick + " url = " + url);
				return true;
			}
		}
		if (remotePort && Util::isValidIp(remoteIp))
		{
			user->setIP(remoteIp);
#ifdef _DEBUG
			//ClientManager::setUserIP(user, remoteIp); // TODO - может не нужно тут?
#endif
			// “€жела€ операци€ по мапе юзеров - только чтобы показать IP в списке ?
		}
		string tth;
		if (isTTH)
			tth = hubNameOrTTH.substr(4);
		if (tth.empty() && type == SearchResult::TYPE_FILE)
			return true;

		SearchResult sr(user, type, slots, freeSlots, size, file, url, remoteIp, TTHValue(tth), 0);
		if (CMD_DEBUG_ENABLED())
			COMMAND_DEBUG("[Search-result] url = " + url + " remoteIp = " + Util::printIpAddress(remoteIp) + " file = " + file + " user = " + user->getLastNick(), DebugTask::CLIENT_IN, Util::printIpAddress(remoteIp));
		SearchManager::getInstance()->fire(SearchManagerListener::SR(), sr);
		return true;
	}
	return false;
}

#if BOOST_ENDIAN_BIG_BYTE
static const uint32_t MESSAGE_RES = 0x52455320;
static const uint32_t MESSAGE_PSR = 0x50535220;
#else
static const uint32_t MESSAGE_RES = 0x20534552;
static const uint32_t MESSAGE_PSR = 0x20525350;
#endif

static inline bool isRES(const char* buf, int len)
{
	return len >= 5 && loadUnaligned32(buf + 1) == MESSAGE_RES && buf[len - 1] == 0x0A;
}

static inline bool isPSR(const char* buf, int len)
{
	return len >= 5 && loadUnaligned32(buf + 1) == MESSAGE_PSR && buf[len - 1] == 0x0A;
}

bool SearchManager::processRES(const char* buf, int len, const IpAddress& remoteIp)
{
	if (isRES(buf, len))
	{
		AdcCommand c(0);
		int parseResult = c.parse(buf, len-1);
		if (parseResult != AdcCommand::PARSE_OK)
		{
#ifdef _DEBUG
			LogManager::message("[AdcCommand] Parse Error: " + Util::toString(parseResult) + " ip=" + Util::printIpAddress(remoteIp) + " data=[" + string(buf, len) + "]", false);
#endif
			return false;
		}
		if (c.getParameters().empty()) return false;
		const string& cid = c.getParam(0);
		if (cid.size() != 39) return false;
		UserPtr user = ClientManager::findUser(CID(cid));
		if (!user) return true;
		onRES(c, true, user, remoteIp);
		return true;
	}
	return false;
}

bool SearchManager::processPSR(const char* buf, int len, const IpAddress& remoteIp)
{
	if (isPSR(buf, len))
	{
		AdcCommand c(0);
		int parseResult = c.parse(buf, len-1);
		if (parseResult != AdcCommand::PARSE_OK)
		{
#ifdef _DEBUG
			LogManager::message("[AdcCommand] Parse Error: " + Util::toString(parseResult) + " ip=" + Util::printIpAddress(remoteIp) + " data=[" + string(buf, len) + "]", false);
#endif
			return false;
		}
		if (c.getParameters().empty()) return false;
		const string& cid = c.getParam(0);
		if (cid.size() != 39) return false;
		UserPtr user;
		OnlineUserPtr ou = ClientManager::findOnlineUser(CID(cid), Util::emptyString, false);
		if (ou) user = ou->getUser();
		// when user == NULL then it is probably NMDC user, check it later
		onPSR(c, true, user, remoteIp);
		return true;
	}
	return false;
}

bool SearchManager::processSUDP(const char* buf, int len, const IpAddress& remoteIp, uint16_t remotePort)
{
	if (len < 32 || (len & 15)) return false;
	uint64_t tick = Util::getTick();
	bool result = false;
	string data;
	decryptKeyLock->acquireShared();
	if (lastDecryptState != -1)
	{
		int index = lastDecryptState;
		while (true)
		{
			if (decryptState[index].decrypt(data, buf, len, tick) && isRES(data.data(), (int) data.length()))
			{
				result = true;
				break;
			}
			index = (index + 1) % MAX_SUDP_KEYS;
			if (index == lastDecryptState) break;
		}
	}
	decryptKeyLock->releaseShared();
	if (result)
	{
		if (LogManager::getLogOptions() & LogManager::OPT_LOG_UDP_PACKETS)
			LogManager::commandTrace(data.data(), data.length(), LogManager::FLAG_IN | LogManager::FLAG_UDP,
				Util::printIpAddress(remoteIp, true), remotePort);
		processRES(data.data(), (int) data.length(), remoteIp);
	}
	return result;
}

bool SearchManager::processPortTest(const char* buf, int len, const IpAddress& address)
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
		if (g_portTest.processInfo(AppPorts::PORT_UDP, AppPorts::PORT_UDP, 0, reflectedAddress, string(buf + 15, 39)))
			ConnectivityManager::getInstance()->processPortTestResult();
		return true;
	}
	return false;
}

void SearchManager::searchAuto(const string& tth)
{
	dcassert(tth.size() == 39);
	SearchParam sp;
	sp.fileType = FILE_TYPE_TTH;
	sp.filter = tth;
	sp.generateToken(true);
	ClientManager::search(sp);
}

void SearchManager::onRES(const AdcCommand& cmd, bool skipCID, const UserPtr& from, const IpAddress& remoteIp)
{
	uint16_t freeSlots = SearchResult::SLOTS_UNKNOWN;
	int64_t size = -1;
	string file;
	string tth;
	uint32_t token = 0;
	
	const auto& params = cmd.getParameters();
	for (StringList::size_type i = skipCID ? 1 : 0; i < params.size(); ++i)
	{
		const string& str = params[i];
		if (str.length() <= 2) continue;
		switch (AdcCommand::toCode(str.c_str()))
		{
			case TAG('F', 'N'):
				file = Util::toNmdcFile(str.c_str() + 2);
				break;
			case TAG('S', 'L'):
				freeSlots = Util::toInt(str.c_str() + 2);
				break;
			case TAG('S', 'I'):
				size = Util::toInt64(str.c_str() + 2);
				break;
			case TAG('T', 'R'):
				tth = str.substr(2);
				break;
			case TAG('T', 'O'):
				token = Util::toUInt32(str.c_str() + 2);
		}
	}

	if (!file.empty() && freeSlots != SearchResult::SLOTS_UNKNOWN && size != -1)
	{
		/// @todo get the hub this was sent from, to be passed as a hint? (eg by using the token?)
		const StringList hubs = ClientManager::getHubs(from->getCID(), Util::emptyString);
		const string hub = hubs.empty() ? Util::emptyString : hubs[0];

		const SearchResult::Types type = (file[file.length() - 1] == '\\' ? SearchResult::TYPE_DIRECTORY : SearchResult::TYPE_FILE);
		if (type == SearchResult::TYPE_FILE && tth.empty())
			return;

		uint16_t slots = SearchResult::SLOTS_UNKNOWN;
		ClientManager::getSlots(from->getCID(), slots);
		SearchResult sr(from, type, slots, freeSlots, size, file, hub, remoteIp, TTHValue(tth), token);
		fire(SearchManagerListener::SR(), sr);
	}
}

void SearchManager::onPSR(const AdcCommand& cmd, bool skipCID, UserPtr from, const IpAddress& remoteIp)
{
	uint16_t udp4Port = 0;
	uint16_t udp6Port = 0;
	uint32_t partialCount = 0;
	string tth;
	string hubIpPort;
	string nick;
	QueueItem::PartsInfo partialInfo;
	
	const auto& params = cmd.getParameters();
	for (StringList::size_type i = skipCID ? 1 : 0; i < params.size(); ++i)
	{
		const string& str = params[i];
		if (str.length() <= 2) continue;
		switch (AdcCommand::toCode(str.c_str()))
		{
			case TAG('U', '4'):
				udp4Port = static_cast<uint16_t>(Util::toInt(str.c_str() + 2));
				break;
			case TAG('U', '6'):
				udp6Port = static_cast<uint16_t>(Util::toInt(str.c_str() + 2));
				break;
			case TAG('N', 'I'):
				nick = str.substr(2);
				break;
			case TAG('H', 'I'):
				hubIpPort = str.substr(2);
				break;
			case TAG('T', 'R'):
				tth = str.substr(2);
				break;
			case TAG('P', 'C'):
				partialCount = Util::toUInt32(str.c_str() + 2) * 2;
				break;
			case TAG('P', 'I'):
			{
				SimpleStringTokenizer<char> st(str, ',', 2);
				string tok;
				while (st.getNextNonEmptyToken(tok))
					partialInfo.push_back((uint16_t) Util::toInt(tok));
			}
		}
	}

	if (partialInfo.size() != partialCount || !partialCount)
		return; // Malformed command

	string url = ClientManager::findHub(hubIpPort, 0);
	if (!from || from->isMe())
	{
		// for NMDC support
		if (nick.empty() || hubIpPort.empty())
			return;
		if (!url.empty())
			from = ClientManager::findUser(nick, url);
		else
		if (LogManager::getLogOptions() & LogManager::OPT_LOG_SYSTEM)
			LogManager::message("Unknown Hub IP: " + hubIpPort, false);
		if (!from)
		{
			// Could happen if hub has multiple URLs / IPs
			from = ClientManager::findLegacyUser(nick, url, &url);
			if (!from)
			{
				dcdebug("Search result from unknown user");
				if (LogManager::getLogOptions() & LogManager::OPT_LOG_PSR)
					LOG(PSR_TRACE, "Unknown user " + (nick.empty() ? "<empty>" : nick) + " (" + url + ')');
				return;
			}
			else
			{
				dcdebug("Search result from valid user");
				if (LogManager::getLogOptions() & LogManager::OPT_LOG_PSR)
					LOG(PSR_TRACE, "Found user " + (nick.empty() ? "<empty>" : nick) + " (" + url + ')');
			}
		}
	}

	uint16_t udpPort = remoteIp.type == AF_INET6 ? udp6Port : udp4Port;
	QueueItem::PartsInfo outParts;
	QueueItem::PartialSource ps((from->getFlags() & User::NMDC) ? ClientManager::findMyNick(url) : Util::emptyString, hubIpPort, remoteIp, udpPort, 0);
	ps.setParts(partialInfo);

	QueueManager::getInstance()->handlePartialResult(from, TTHValue(tth), ps, outParts);

	if (udpPort && !outParts.empty())
	{
		try
		{
			AdcCommand reply(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			toPSR(reply, false, ps.getMyNick(), remoteIp.type, hubIpPort, tth, outParts);
			bool result = ClientManager::sendAdcCommand(reply, from->getCID(), remoteIp, udpPort, nullptr);
			if (LogManager::getLogOptions() & LogManager::OPT_LOG_PSR)
			{
				string msg = tth + ": sending PSR response to ";
				msg += from->getCID().toBase32();
				string nick = from->getLastNick();
				if (!nick.empty()) msg += ", " + nick;
				if (!hubIpPort.empty()) msg += ", hub " + hubIpPort;
				msg += ", we have " + Util::toString(QueueItem::countParts(outParts)) + '*' + Util::toString(ps.getBlockSize());
				LOG(PSR_TRACE, msg);
				if (!result)
					LOG(PSR_TRACE, "Unable to send PSR response");
			}
		}
		catch (const Exception& e)
		{
			if (LogManager::getLogOptions() & LogManager::OPT_LOG_PSR)
				LOG(PSR_TRACE, "Error sending response packet - " + e.getError());
		}
	}
	
}

ClientManagerListener::SearchReply SearchManager::respond(AdcSearchParam& param, const OnlineUserPtr& ou, const string& hubUrl, const IpAddress& hubIp, uint16_t hubPort)
{
	// Filter own searches
	const CID& from = ou->getUser()->getCID();
	if (from == ClientManager::getMyCID())
		return ClientManagerListener::SEARCH_MISS;
	
	const UserPtr user = ClientManager::findUser(from);
	if (!user)
		return ClientManagerListener::SEARCH_MISS;
	
	vector<SearchResultCore> searchResults;
	ShareManager::getInstance()->search(searchResults, param);
	
	ClientManagerListener::SearchReply sr = ClientManagerListener::SEARCH_MISS;
	
	// TODO: don't send replies to passive users
	if (searchResults.empty())
	{
		if (!param.hasRoot)
			return sr;

		QueueItem::PartsInfo outParts;
		uint64_t blockSize;
		if (QueueManager::handlePartialSearch(param.root, outParts, blockSize))
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			string tth = param.root.toBase32();
			string hubIpPort = Util::printIpAddress(hubIp, true) + ":" + Util::toString(hubPort);
			toPSR(cmd, true, Util::emptyString, hubIp.type, hubIpPort, tth, outParts);
			bool result = ClientManager::sendAdcCommand(cmd, from, IpAddress{0}, 0, nullptr);
			if (LogManager::getLogOptions() & LogManager::OPT_LOG_PSR)
			{
				string msg = tth + ": sending PSR search result to ";
				msg += from.toBase32();
				string nick = user->getLastNick();
				if (!nick.empty()) msg += ", " + nick;
				if (!hubIpPort.empty()) msg += ", hub " + hubIpPort;
				msg += ", we have " + Util::toString(QueueItem::countParts(outParts)) + '*' + Util::toString(blockSize);
				LOG(PSR_TRACE, msg);
				if (!result)
					LOG(PSR_TRACE, "Unable to send PSR request");
			}
			sr = ClientManagerListener::SEARCH_PARTIAL_HIT;
		}
	}
	else
	{
		if (LogManager::getLogOptions() & LogManager::OPT_LOG_SEARCH)
		{
			string description = param.getDescription();
			string nick = ou->getIdentity().getNick();
			string message;
			if (searchResults.size() > 1)
			{
				string found = STRING_F(SEARCH_HIT_MULTIPLE, Util::toString(searchResults.size()) % searchResults[0].getFile());
				message = STRING_F(SEARCH_HIT_INFO, nick % hubUrl % description % found);
			}
			else
				message = STRING_F(SEARCH_HIT_INFO, nick % hubUrl % description % searchResults[0].getFile());
			LOG(SEARCH_TRACE, message);
		}
		uint8_t sudpKeyBuf[16];
		const void* sudpKey = nullptr;
		if (param.sudpKey.length() == 26)
		{
			bool error;
			Util::fromBase32(param.sudpKey.c_str(), sudpKeyBuf, sizeof(sudpKeyBuf), &error);
			if (!error) sudpKey = sudpKeyBuf;
		}
		for (auto i = searchResults.cbegin(); i != searchResults.cend(); ++i)
		{
			AdcCommand cmd(AdcCommand::CMD_RES, AdcCommand::TYPE_UDP);
			i->toRES(cmd, UploadManager::getFreeSlots());
			if (!param.token.empty())
				cmd.addParam(TAG('T', 'O'), param.token);
			ClientManager::sendAdcCommand(cmd, from, IpAddress{0}, 0, sudpKey);
		}
		sr = ClientManagerListener::SEARCH_HIT;
	}
	return sr;
}

string SearchManager::getPartsString(const QueueItem::PartsInfo& partsInfo)
{
	string ret;	
	for (size_t i = 0; i + 2 <= partsInfo.size(); i += 2)
	{
		if (!ret.empty()) ret += ',';
		ret += Util::toString(partsInfo[i]);
		ret += ',';
		ret += Util::toString(partsInfo[i + 1]);
	}
	return ret;
}

void SearchManager::toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, int af, const string& hubIpPort, const string& tth, const QueueItem::PartsInfo& partialInfo)
{
	cmd.getParameters().reserve(6);
	if (!myNick.empty())
		cmd.addParam(TAG('N', 'I'), myNick);

	cmd.addParam(TAG('H', 'I'), hubIpPort);
	cmd.addParam(af == AF_INET6 ? TAG('U', '6') : TAG('U', '4'), Util::toString(wantResponse ? getSearchPort(af) : 0));
	cmd.addParam(TAG('T', 'R'), tth);
	cmd.addParam(TAG('P', 'C'), Util::toString(partialInfo.size() / 2));
	cmd.addParam(TAG('P', 'I'), getPartsString(partialInfo));
}

bool SearchManager::isShutdown() const
{
	if (stopFlag.load()) return true;
	extern volatile bool g_isBeforeShutdown;
	return g_isBeforeShutdown;
}

void SearchManager::addToSendQueue(string& data, const IpAddress& address, uint16_t port, uint16_t flags, const void* encKey) noexcept
{
	csSendQueue.lock();
	sendQueue.emplace_back(data, address, port, flags, encKey);
	csSendQueue.unlock();
	sendNotif();
}

void SearchManager::processSendQueue() noexcept
{
	string tmp;
	csSendQueue.lock();
	for (SendQueueItem& item : sendQueue)
	{
		int index = item.address.type == AF_INET6 ? 1 : 0;
		if (sockets[index])
		{
			const string& data = item.data;
			if ((LogManager::getLogOptions() & LogManager::OPT_LOG_UDP_PACKETS) && !(item.flags & FLAG_NO_TRACE))
				LogManager::commandTrace(data.data(), data.length(), LogManager::FLAG_UDP, Util::printIpAddress(item.address, true), item.port);
			if (item.flags & FLAG_ENC_KEY)
			{
				encryptState.encrypt(tmp, data, item.encKey);
				sockets[index]->sendPacket(tmp.data(), tmp.length(), item.address, item.port);
			}
			else
				sockets[index]->sendPacket(data.data(), data.length(), item.address, item.port);
		}
	}
	sendQueue.clear();
	csSendQueue.unlock();
}

void SearchManager::sendNotif()
{
#ifdef _WIN32
	events[EVENT_COMMAND].notify();
#else
	if (threadId) pthread_kill(threadId, SIGUSR1);
#endif
}

void SearchManager::updateSettings() noexcept
{
	int newOptions = 0;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	if (ss->getBool(Conf::INCOMING_SEARCH_TTH_ONLY))
		newOptions |= OPT_INCOMING_SEARCH_TTH_ONLY;
	if (ss->getBool(Conf::INCOMING_SEARCH_IGNORE_PASSIVE))
		newOptions |= OPT_INCOMING_SEARCH_IGNORE_PASSIVE;
	if (ss->getBool(Conf::INCOMING_SEARCH_IGNORE_BOTS))
		newOptions |= OPT_INCOMING_SEARCH_IGNORE_BOTS;
	if (ss->getBool(Conf::USE_SUDP))
		newOptions |= OPT_ENABLE_SUDP;
	ss->unlockRead();
	if (newOptions & OPT_ENABLE_SUDP)
	{
		encryptState.create();
		createNewDecryptKey(Util::getTick());
	}
	options = newOptions;
}

void SearchManager::createNewDecryptKey(uint64_t tick) noexcept
{
	WRITE_LOCK(*decryptKeyLock);
	if (tick >= newKeyTime)
	{
		newKeyTime = tick + SUDP_KEY_UPDATE_TIME;
		lastDecryptState = (lastDecryptState + 1) % MAX_SUDP_KEYS;
		decryptState[lastDecryptState].create(tick);
	}
}

void SearchManager::addEncryptionKey(AdcCommand& cmd) noexcept
{
	if (!(getOptions() & OPT_ENABLE_SUDP)) return;
	uint64_t tick = Util::getTick();
	string key;
	{
		READ_LOCK(*decryptKeyLock);
		if (lastDecryptState != -1 && tick < decryptState[lastDecryptState].expires)
			key = decryptState[lastDecryptState].keyBase32;
	}
	if (!key.empty())
		cmd.addParam(TAG('K', 'Y'), key);
}

SearchManager::SendQueueItem::SendQueueItem(string& data, const IpAddress& address, uint16_t port, uint16_t flags, const void* encKey) :
	data(std::move(data)), address(address), port(port), flags(flags)
{
	if (flags & FLAG_ENC_KEY)
		memcpy(this->encKey, encKey, 16);
}

static const unsigned char zeroIV[16] = {};

bool SearchManager::EncryptState::encrypt(string& out, const string& in, const void* key) const noexcept
{
	int len = (int) in.length();
	int pad = 16 - (len & 15);
	int outLen = len + pad + 16;
	out.resize(outLen);
	unsigned char* outBuf = (unsigned char*) &out[0];
	RAND_bytes(outBuf, 16);
	memcpy(outBuf + 16, in.data(), len);
	memset(outBuf + 16 + len, pad, pad);

	bool result = false;
	if (cipher)
	{
		EVP_CIPHER_CTX* ctx = static_cast<EVP_CIPHER_CTX*>(cipher);
		if (EVP_CipherInit_ex(ctx, nullptr, nullptr, (const unsigned char*) key, zeroIV, 1))
		{
			EVP_CIPHER_CTX_set_padding(ctx, 0);
			int outl;
			result = EVP_EncryptUpdate(ctx, outBuf, &outl, outBuf, outLen) != 0 && outl == outLen;
		}
	}
	return result;
}

bool SearchManager::EncryptState::create() noexcept
{
	if (cipher) return false;
	EVP_CIPHER_CTX* newCipher = EVP_CIPHER_CTX_new();
	if (!newCipher) return false;
	if (!EVP_CipherInit_ex(newCipher, EVP_aes_128_cbc(), nullptr, nullptr, nullptr, 1))
	{
		EVP_CIPHER_CTX_free(newCipher);
		return false;
	}
	cipher = newCipher;
	return true;
}

void SearchManager::EncryptState::clearCipher() noexcept
{
	if (cipher)
	{
		EVP_CIPHER_CTX_free(static_cast<EVP_CIPHER_CTX*>(cipher));
		cipher = nullptr;
	}
}

void SearchManager::DecryptState::clearCipher() noexcept
{
	if (cipher)
	{
		EVP_CIPHER_CTX_free(static_cast<EVP_CIPHER_CTX*>(cipher));
		cipher = nullptr;
	}
}

bool SearchManager::DecryptState::create(uint64_t tick) noexcept
{
	unsigned char key[16];
	RAND_bytes(key, sizeof(key));
	EVP_CIPHER_CTX* newCipher = EVP_CIPHER_CTX_new();
	if (!newCipher) return false;
	if (!EVP_CipherInit_ex(newCipher, EVP_aes_128_cbc(), nullptr, key, zeroIV, 0))
	{
		EVP_CIPHER_CTX_free(newCipher);
		return false;
	}
	clearCipher();
	cipher = newCipher;
	keyBase32 = Util::toBase32(key, sizeof(key));
	expires = tick + SUDP_KEY_EXPIRE_TIME;
	return true;
}

bool SearchManager::DecryptState::decrypt(string& out, const char* inBuf, int len, uint64_t tick) const noexcept
{
	if (len < 32 || (len & 15)) return false;
	bool result = false;
	if (cipher && tick < expires)
	{
		out.resize(len);
		unsigned char* outBuf = (unsigned char *) &out[0];
		EVP_CIPHER_CTX* ctx = static_cast<EVP_CIPHER_CTX*>(cipher);
		EVP_CipherInit_ex(ctx, nullptr, nullptr, nullptr, zeroIV, 0);
		EVP_CIPHER_CTX_set_padding(ctx, 0);
		int outl;
		if (EVP_DecryptUpdate(ctx, outBuf, &outl, (const unsigned char *) inBuf, len))
		{
			int pad = outBuf[len-1];
			if (pad > 0 && pad <= 16)
			{
				int i = 0;
				while (i < pad)
				{
					if (outBuf[len-1-i] != pad) break;
					++i;
				}
				if (i == pad)
				{
					len -= pad + 16;
					memmove(outBuf, outBuf + 16, len);
					out.resize(len);
					result = true;
				}
			}
		}
	}
	return result;
}
