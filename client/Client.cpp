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
#include "UploadManager.h"
#include "ThrottleManager.h"
#include "LogManager.h"
#include "CompatibilityManager.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "QueueManager.h"
#include "SearchManager.h"
#include "Wildcards.h"
#include "ParamExpander.h"
#include "Resolver.h"

std::atomic<uint32_t> Client::g_counts[COUNT_UNCOUNTED];

#ifdef _DEBUG
std::atomic_int Client::clientCount(0);
#endif

Client::Client(const string& hubURL, const string& address, uint16_t port, char separator, bool secure, Socket::Protocol proto) :
	reconnDelay(120),
	lastActivity(GET_TICK()),
	pendingUpdate(0),
	autoReconnect(false),
	state(STATE_DISCONNECTED),
	connSuccess(false),
	clientSock(nullptr),
	hubURL(hubURL),
	address(address),
	ip{0},
	port(port),
	separator(separator),
	secure(secure),
	countType(COUNT_UNCOUNTED),
	bytesShared(0),
	exclChecks(false),
	hideShare(false),
	overrideSearchInterval(false), overrideSearchIntervalPassive(false),
	overrideId(false),
	proto(proto),
	userListLoaded(false),
	suppressChatAndPM(false),
	fakeHubCount(false),
	fakeShareSize(-1),
	fakeShareFiles(-1),
	fakeClientStatus(0),
	favMode(0),
	preferIP6(false),
	csUserCommands(RWLock::create())
{
	dcassert(hubURL == Text::toLower(hubURL));
#ifdef _DEBUG
	++clientCount;
#endif
	encoding = Text::charsetFromString(SETTING(DEFAULT_CODEPAGE));
	const auto l_lower_url = Text::toLower(hubURL);
	if (!Util::isAdcHub(l_lower_url))
	{
		const auto l_pos_ru = l_lower_url.rfind(".ru");
		if (l_pos_ru != string::npos)
		{
			if (l_pos_ru == l_lower_url.size() - 3 ||
			        l_pos_ru < l_lower_url.size() - 4 && l_lower_url[l_pos_ru + 3] == ':')
			{
				encoding = 1251;
			}
		}
	}

	TimerManager::getInstance()->addListener(this);
}

Client::~Client()
{
#ifdef _DEBUG
	--clientCount;
#endif
	dcassert(!clientSock);
	updateCounts(true);
}

void Client::initDefaultUsers()
{
	const auto myUser = std::make_shared<User>(ClientManager::getMyCID(), Util::emptyString);
	const auto hubUser = std::make_shared<User>(CID(), Util::emptyString);
	myUser->setFlag(User::MYSELF);
	if (getType() == TYPE_NMDC)
	{
		myUser->setFlag(User::NMDC);
		hubUser->setFlag(User::NMDC);
	}
	auto clientPtr = getClientPtr();
	myOnlineUser = std::make_shared<OnlineUser>(myUser, clientPtr, 0);
	hubOnlineUser = std::make_shared<OnlineUser>(hubUser, clientPtr, AdcCommand::HUB_SID);
}

void Client::resetSocket(BufferedSocket* bufferedSocket) noexcept
{
	if (bufferedSocket)
	{
		bufferedSocket->disconnect(true);
		bufferedSocket->joinThread();
		BufferedSocket::destroyBufferedSocket(bufferedSocket);
	}
}

void Client::reconnect()
{
	disconnect(true);
	setAutoReconnect(true);
	setReconnDelay(0);
}

void Client::shutdown() noexcept
{
	TimerManager::getInstance()->removeListener(this);
	BufferedSocket* prevSocket = nullptr;
	csState.lock();
	state = STATE_DISCONNECTED;
	prevSocket = clientSock;
	clientSock = nullptr;
	csState.unlock();

	resetSocket(prevSocket);
}

void Client::reloadSettings(bool updateNick)
{
	auto fm = FavoriteManager::getInstance();
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	string speedDescription;
#endif
	const FavoriteHubEntry* hub = fm->getFavoriteHubEntryPtr(getHubUrl());
	string clientName, clientVersion;
	bool overrideClientId = false;
	if (hub && hub->getOverrideId())
	{
		clientName = hub->getClientName();
		clientVersion = hub->getClientVersion();
		overrideClientId = !clientName.empty();
	}
	if (!overrideClientId && BOOLSETTING(OVERRIDE_CLIENT_ID))
	{
		FavoriteManager::splitClientId(SETTING(CLIENT_ID), clientName, clientVersion);
		overrideClientId = !clientName.empty();
	}
	setClientId(overrideClientId, clientName, clientVersion);
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	if (!overrideId && BOOLSETTING(ADD_TO_DESCRIPTION))
	{
		if (BOOLSETTING(ADD_DESCRIPTION_SLOTS))
			speedDescription += '[' + Util::toString(UploadManager::getFreeSlots()) + ']';
		if (BOOLSETTING(ADD_DESCRIPTION_LIMIT) && BOOLSETTING(THROTTLE_ENABLE) && ThrottleManager::getInstance()->getUploadLimitInKBytes() != 0)
			speedDescription += "[L:" + Util::toString(ThrottleManager::getInstance()->getUploadLimitInKBytes()) + "KB]";
	}
#endif
	if (hub)
	{
		if (updateNick)
		{
			string nick = hub->getNick(true);
			csState.lock();
			if (!randomTempNick.empty())
				nick = randomTempNick;
			checkNick(nick);
			myNick = nick;
			csState.unlock();
			myOnlineUser->getIdentity().setNick(nick);
		}

		if (!hub->getUserDescription().empty())
		{
			setCurrentDescription(
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
			    speedDescription +
#endif
			    hub->getUserDescription());
		}
		else
		{
		
			setCurrentDescription(
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
			    speedDescription +
#endif
			    SETTING(DESCRIPTION));
		}
		
		if (!hub->getEmail().empty())
		{
			setCurrentEmail(hub->getEmail());
		}
		else
		{
			setCurrentEmail(SETTING(EMAIL));
		}

		if (!hub->getPassword().empty())
		{
			LOCK(csState);
			storedPassword = hub->getPassword();
		}

		preferIP6 = hub->getPreferIP6();

		hideShare = hub->getHideShare();
		shareGroup = hub->getShareGroup();

		string ip = hub->getIP();
		boost::algorithm::trim(ip);
		setFavIp(ip);
		favMode = hub->getMode();

		int hubEncoding = hub->getEncoding();
		if (hubEncoding)
			setEncoding(hubEncoding);
		
		int searchInterval = hub->getSearchInterval();
		if (searchInterval < 2)
			searchInterval = SETTING(MIN_SEARCH_INTERVAL);
		else
			overrideSearchInterval = true;
		setSearchInterval(searchInterval * 1000);

		searchInterval = hub->getSearchIntervalPassive();
		if (searchInterval < 2)
			searchInterval = SETTING(MIN_SEARCH_INTERVAL_PASSIVE);
		else
			overrideSearchIntervalPassive = true;
		setSearchIntervalPassive(searchInterval * 1000);

		csOpChat.lock();
		opChat = hub->getOpChat();
		if (!Wildcards::regexFromPatternList(reOpChat, hub->getOpChat(), false)) opChat.clear();
		csOpChat.unlock();

		exclChecks = hub->getExclChecks();
		fakeHubCount = hub->getExclusiveHub();
		fakeClientStatus = hub->getFakeClientStatus();

		const string& fakeShare = hub->getFakeShare();
		if (!fakeShare.empty())
		{
			fakeShareSize = static_cast<int64_t>(FavoriteHubEntry::parseSizeString(fakeShare));
			fakeShareFiles = hub->getFakeFileCount();
		}
		else
		{
			fakeShareSize = -1;
			fakeShareFiles = -1;
		}

		const string& kp = hub->getKeyPrint();
		if (!kp.empty()) setKeyPrint(kp);
	}
	else
	{
		if (updateNick)
		{
			string nick = SETTING(NICK);
			checkNick(nick);
			csState.lock();
			myNick = nick;
			csState.unlock();
			myOnlineUser->getIdentity().setNick(nick);
		}
		setCurrentDescription(
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
		    speedDescription +
#endif
		    SETTING(DESCRIPTION));
		setCurrentEmail(SETTING(EMAIL));
		hideShare = false;
		shareGroup.init();
		setFavIp(Util::emptyString);
		favMode = 0;

		setSearchInterval(SETTING(MIN_SEARCH_INTERVAL) * 1000);
		setSearchIntervalPassive(SETTING(MIN_SEARCH_INTERVAL_PASSIVE) * 1000);
		overrideSearchInterval = overrideSearchIntervalPassive = false;

		opChat.clear();
		exclChecks = false;
		fakeHubCount = false;
		fakeClientStatus = 0;
		fakeShareSize = -1;
		fakeShareFiles = -1;
	}
	fm->releaseFavoriteHubEntryPtr(hub);
}

void Client::connect()
{
	BufferedSocket* prevSocket = nullptr;
	csState.lock();	
	state = STATE_CONNECTING;
	connSuccess = false;
	updateActivityL();
	prevSocket = clientSock;
	clientSock = nullptr;
	csState.unlock();
	
	resetSocket(prevSocket);
	bytesShared.store(0);
	setAutoReconnect(true);
	setReconnDelay(Util::rand(10, 30));
	reloadSettings(true);
	resetRegistered();
	resetOp();

	bool hasIP6 = ConnectivityManager::getInstance()->hasIP6();
	csState.lock();
	try
	{
		clientSock = BufferedSocket::getBufferedSocket(separator, this);
		if (hasIP6)
		{
			if (preferIP6) clientSock->setIpVersion(AF_INET6);
		}
		else
			clientSock->setIpVersion(AF_INET | Resolver::RESOLVE_TYPE_EXACT);
		clientSock->connect(address, port, secure, BOOLSETTING(ALLOW_UNTRUSTED_HUBS), true, proto);
		clientSock->start();
		dcdebug("Client::connect() %p\n", this);
	}
	catch (const Exception& e)
	{
		state = STATE_DISCONNECTED;
		csState.unlock();
		fire(ClientListener::ClientFailed(), this, e.getError());
		return;
	}
	csState.unlock();
}

void Client::connectIfNetworkOk()
{
	{
		LOCK(csState);
		if (state != STATE_DISCONNECTED && state != STATE_WAIT_PORT_TEST) return;
	}
	if (ConnectivityManager::getInstance()->isSetupInProgress())	
	{
		bool sendStatusMessage = false;
		csState.lock();
		if (state != STATE_WAIT_PORT_TEST)
		{
			state = STATE_WAIT_PORT_TEST;
			sendStatusMessage = true;
		}
		csState.unlock();
		if (sendStatusMessage)
			fire(ClientListener::StatusMessage(), this, CSTRING(WAITING_NETWORK_CONFIG));
		return;
	}
	connect();
}

bool Client::isActive() const
{
	extern bool g_DisableTestPort;
	if (!g_DisableTestPort)
		return ClientManager::isActive(ip.type, favMode);
	return true; // Manual active
}

void Client::send(const char* message, size_t len)
{
	{
		LOCK(csState);
		if (state == STATE_CONNECTING || state == STATE_DISCONNECTED)
		{
			dcdebug("Send message failed, hub is disconnected!");
			return;
		}
		updateActivityL();
		clientSock->write(message, len);
	}	
	if (CMD_DEBUG_ENABLED()) COMMAND_DEBUG(toUtf8(string(message, len)), DebugTask::HUB_OUT, getIpPort());
}

void Client::onConnected() noexcept
{
	clearUserCommands(UserCommand::CONTEXT_MASK);
	csState.lock();
	updateActivityL();
	ip = clientSock->getIp();
	if (clientSock->isSecure() && keyprint.compare(0, 7, "SHA256/", 7) == 0)
	{
		const auto kp = clientSock->getKeyprint();
		if (!kp.empty())
		{
			vector<uint8_t> kp2v(kp.size());
			Encoder::fromBase32(keyprint.c_str() + 7, &kp2v[0], kp2v.size());
			if (!std::equal(kp.begin(), kp.end(), kp2v.begin()))
			{
				state = STATE_DISCONNECTED;
				csState.unlock();
				fire(ClientListener::ClientFailed(), this, STRING(KEYPRINT_MISMATCH));
				return;
			}
		}
	}
	state = STATE_PROTOCOL;
	csState.unlock();
	auto fm = FavoriteManager::getInstance();
	fm->changeConnectionStatus(getHubUrl(), ConnectionStatus::SUCCESS);
	fire(ClientListener::Connected(), this);
}

void Client::onFailed(const string& line) noexcept
{
	csState.lock();
	bool connected = connSuccess;
	state = STATE_DISCONNECTED;
	updateActivityL();
	csState.unlock();

	if (!ClientManager::isBeforeShutdown() && !connected)
		FavoriteManager::getInstance()->changeConnectionStatus(getHubUrl(), ConnectionStatus::FAILURE);
	fire(ClientListener::ClientFailed(), this, line);
}

void Client::disconnect(bool graceless)
{
	csState.lock();
	state = STATE_DISCONNECTED;
	if (clientSock)
		clientSock->disconnect(graceless);
	csState.unlock();
}

bool Client::isSecure() const
{
	LOCK(csState);
	return clientSock && clientSock->isSecure();
}

bool Client::isTrusted() const
{
	LOCK(csState);
	return clientSock && clientSock->isTrusted();
}

string Client::getCipherName() const
{
	LOCK(csState);
	return clientSock ? clientSock->getCipherName() : Util::emptyString;
}

vector<uint8_t> Client::getCertificateHash() const
{
	LOCK(csState);
	return clientSock ? clientSock->getKeyprint() : Util::emptyByteVector;
}

void Client::updateCounts(bool remove)
{
	// We always remove the count and then add the correct one if requested...
	if (countType != COUNT_UNCOUNTED)
	{
		--g_counts[countType];
		countType = COUNT_UNCOUNTED;
	}
	
	if (!remove)
	{
		if (isOp())
		{
			countType = COUNT_OP;
		}
		else if (isRegistered())
		{
			countType = COUNT_REGISTERED;
		}
		else
		{
			countType = COUNT_NORMAL;
		}
		++g_counts[countType];
	}
}

void Client::decBytesShared(int64_t bytes)
{
	dcassert(bytes >= 0);
	bytesShared -= bytes;
}

void Client::changeBytesShared(Identity& id, int64_t bytes)
{
	dcassert(bytes >= 0);
	int64_t old = id.getBytesShared();
	id.setBytesShared(bytes);
	bytesShared += bytes - old;
}

void Client::fireUserListUpdated(const OnlineUserList& userList)
{
	if (!userList.empty() && !ClientManager::isBeforeShutdown())
	{
		fire(ClientListener::UserListUpdated(), this, userList);
	}
}

void Client::fireUserUpdated(const OnlineUserPtr& aUser)
{
	if (!ClientManager::isBeforeShutdown())
	{
		fire(ClientListener::UserUpdated(), aUser);
	}
}

void Client::getLocalIp(Ip4Address& ip4, Ip6Address& ip6) const
{
	if (!getFavIp().empty())
	{
		IpAddress ip;
		if (Util::parseIpAddress(ip, getFavIp()))
		{
			if (ip.type == AF_INET)
			{
				ip4 = ip.data.v4;
				memset(&ip6, 0, sizeof(ip6));
				return;
			}
			if (ip.type == AF_INET6)
			{
				ip6 = ip.data.v6;
				ip4 = 0;
				return;
			}
		}
	}

	const Identity& id = getMyIdentity();
	ip4 = id.getIP4();
	ip6 = id.getIP6();

	if (ip4 == 0)
	{
		string externalIP;
		if (BOOLSETTING(WAN_IP_MANUAL))
		{
			externalIP = SETTING(EXTERNAL_IP);
			if (!Util::isValidIp4(externalIP)) externalIP.clear();
		}
		if (externalIP.empty() || !BOOLSETTING(NO_IP_OVERRIDE))
		{
			IpAddress ip = ConnectivityManager::getInstance()->getReflectedIP(AF_INET);
			if (!Util::isEmpty(ip)) externalIP = Util::printIpAddress(ip);
		}
		if (externalIP.empty())
		{
			IpAddress ip = ConnectivityManager::getInstance()->getLocalIP(AF_INET);
			ip4 = ip.data.v4;
		}
		else
			Util::parseIpAddress(ip4, externalIP);
	}

	if (Util::isEmpty(ip6))
	{
		string externalIP;
		if (BOOLSETTING(WAN_IP_MANUAL6))
		{
			externalIP = SETTING(EXTERNAL_IP6);
			if (!Util::isValidIp6(externalIP)) externalIP.clear();
		}
		if (externalIP.empty() || !BOOLSETTING(NO_IP_OVERRIDE6))
		{
			IpAddress ip = ConnectivityManager::getInstance()->getReflectedIP(AF_INET6);
			if (!Util::isEmpty(ip)) externalIP = Util::printIpAddress(ip);
		}
		if (externalIP.empty())
		{
			IpAddress ip = ConnectivityManager::getInstance()->getLocalIP(AF_INET6);
			ip6 = ip.data.v6;
		}
		else
			Util::parseIpAddress(ip6, externalIP);
	}
}

unsigned Client::searchInternal(const SearchParamToken& sp)
{
	//dcdebug("Queue search %s\n", sp.m_filter.c_str());
	
	if (searchQueue.interval)
	{
		Search s;
		s.searchMode = sp.searchMode;
		if (s.searchMode == SearchParamBase::MODE_DEFAULT)
			s.searchMode = isActive() ? SearchParamBase::MODE_ACTIVE : SearchParamBase::MODE_PASSIVE;
		s.fileType = sp.fileType;
		s.size = sp.size;
		s.filter = sp.filter;
		s.filterExclude = sp.filterExclude;
		s.sizeMode = sp.sizeMode;
		s.token = sp.token;
		s.extList = sp.extList;
		s.owners.insert(sp.owner);
		
		searchQueue.add(s);
		
		const uint64_t now = GET_TICK();
		uint64_t st = searchQueue.getSearchTime(sp.owner, now);
		return st ? unsigned(st - now) : 0;
	}
	searchToken(sp);
	return 0;
}

void Client::onDataLine(const string& aLine) noexcept
{
	updateActivityL();
	if (CMD_DEBUG_ENABLED()) COMMAND_DEBUG(aLine, DebugTask::HUB_IN, getIpPort());
}

void Client::on(Second, uint64_t tick) noexcept
{
	csState.lock();
	const States state = this->state;
	const uint64_t lastActivity = this->lastActivity;
	const uint64_t pendingUpdate = this->pendingUpdate;
	csState.unlock();
	
	if (state == STATE_WAIT_PORT_TEST)
	{
		connectIfNetworkOk();
		return;
	}
	if (state == STATE_DISCONNECTED && getAutoReconnect() && tick > lastActivity + getReconnDelay() * 1000)
	{
		// Try to reconnect...
		connect();
	}
	else if (state == STATE_IDENTIFY && lastActivity + 30000 < tick)
	{
		csState.lock();
		if (clientSock)
			clientSock->disconnect(false);
		csState.unlock();
	}
	else if ((state == STATE_CONNECTING || state == STATE_PROTOCOL) && lastActivity + 60000 < tick)
	{
		reconnect();
	}
	else if (state == STATE_NORMAL)
	{
		if (tick >= lastActivity + 118 * 1000)
			send(&separator, 1);
		if (pendingUpdate && tick >= pendingUpdate)
			info(false);
	}

	if (searchQueue.interval == 0)
	{
		//dcassert(0);
		return;
	}
	
	if (state == STATE_NORMAL)
	{
		Search s;
		if (searchQueue.pop(s, tick))
		{
			SearchParamToken sp;
			sp.token = s.token;
			sp.sizeMode = s.sizeMode;
			sp.fileType = s.fileType;
			sp.size = s.size;
			sp.filter = std::move(s.filter);
			sp.filterExclude = std::move(s.filterExclude);
			sp.searchMode = s.searchMode;
			sp.extList = std::move(s.extList);
			sp.owner = nullptr;
			searchToken(sp);
		}
	}
}

bool Client::isFloodCommand(const string& p_command, const string& p_line)
{
#if 0
	if (is_all_my_info_loaded() && CFlyServerConfig::g_interval_flood_command)
	{
		if (!CFlyServerConfig::isIgnoreFloodCommand(p_command))
		{
			CFlyFloodCommand l_item;
			l_item.m_start_tick = GET_TICK();
			l_item.m_tick = l_item.m_start_tick;
			auto l_flood_find = m_flood_detect.insert(std::make_pair(p_command, l_item));
			if (l_flood_find.second == false)
			{
				auto& l_result = l_flood_find.first->second;
				l_result.m_count++;
				l_result.m_tick = l_item.m_tick;
				if (BOOLSETTING(LOG_FLOOD_TRACE))
				{
					l_result.m_flood_command.push_back(make_pair(p_line, GET_TICK() - l_result.m_start_tick));
				}
				const auto l_delta = l_result.m_tick - l_result.m_start_tick;
				if (l_delta > CFlyServerConfig::g_interval_flood_command * 1000)
				{
					// ѕрошла секунда и команд пришло больше 100 (CFlyServerConfig::g_max_flood_command)
					// логируем счетчик и баним на 20 секунд данные команды (CFlyServerConfig::g_ban_flood_command)
					if (l_result.m_count > CFlyServerConfig::g_max_flood_command)  // в секунду больше чем 20
					{
						if (l_result.m_is_ban == false) // ¬ лог кидаем первую мессагу
						{
							if (BOOLSETTING(LOG_FLOOD_TRACE))
							{
								const string l_msg = "[Start flood][" + hubURL + "] command = " + l_flood_find.first->first
								                     + " count = " + Util::toString(l_result.m_count);
								LogManager::flood_message(l_msg);
								unsigned l_index = 0;
								std::string l_last_message;
								unsigned l_count_dup = 0;
								for (auto i = l_result.m_flood_command.cbegin(); i != l_result.m_flood_command.cend(); ++i)
								{
									if (l_last_message == i->first)
									{
										l_count_dup++;
									}
									else
									{
										LogManager::flood_message("[DeltaTime:" + Util::toString(i->second) + "][Index = " +
										                          Util::toString(l_index++) + "][Message = " + i->first + "][dup=" + Util::toString(l_count_dup) + "]");
										l_last_message = i->first;
										l_count_dup = 0;
									}
								}
								if (l_count_dup)
								{
									LogManager::flood_message("[Message = " + l_last_message + "][dup=" + Util::toString(l_count_dup) + "]");
								}
								l_result.m_flood_command.clear();
							}
							l_result.m_is_ban = true;
						}
						if (l_delta > CFlyServerConfig::g_ban_flood_command * 1000) // 20 секунд данные команды в бане!
						{
							if (BOOLSETTING(LOG_FLOOD_TRACE))
							{
								const string l_msg = "[Stop flood][" + hubURL + "] command = " + l_flood_find.first->first +
								                     " count = " + Util::toString(l_result.m_count);
								LogManager::flood_message(l_msg);
								l_result.m_flood_command.clear();
							}
							l_result.m_is_ban = false;
							l_result.m_count = 0;
							l_result.m_start_tick = l_result.m_tick = GET_TICK();
							
						}
						return l_result.m_is_ban;
					}
					else
					{
						//  оманд прибежало мало - зачищаемс€
						l_result.m_is_ban = false;
						l_result.m_count = 0;
						l_result.m_start_tick = l_result.m_tick = GET_TICK();
						if (BOOLSETTING(LOG_FLOOD_TRACE))
						{
							l_result.m_flood_command.clear();
						}
					}
				}
			}
		}
	}
#endif
	return false;
}

#if 0 // Not used
OnlineUserPtr Client::getUser(const UserPtr& aUser)
{
	// for generic client, use ClientManager, but it does not correctly handle ClientManager::me
	ClientManager::LockInstanceOnlineUsers lockedInstance;
	return lockedInstance->getOnlineUserL(aUser);
}
#endif

bool Client::isPrivateMessageAllowed(const ChatMessage& message, string* response, bool automatic)
{
	if (isMe(message.replyTo))
	{
		if (!UserManager::getInstance()->checkOutgoingPM(message.to->getUser(), automatic)
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		        || UploadManager::isBanReply(message.to->getUser())
#endif
		   )
			return false;
		return true;
	}
	UserManager::PasswordStatus passwordStatus;
	bool isOpen = UserManager::getInstance()->checkPMOpen(message, passwordStatus);
	if (passwordStatus == UserManager::CHECKED && response)
		*response = SETTING(PM_PASSWORD_OK_HINT);
	if (isOpen)
		return true;
	if (message.thirdPerson && BOOLSETTING(IGNORE_ME))
		return false;
	if (UserManager::getInstance()->isInIgnoreList(message.replyTo->getIdentity().getNick()))
		return false;
	if (BOOLSETTING(SUPPRESS_PMS))
	{
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		if (UploadManager::isBanReply(message.replyTo->getUser()))
			return false;
#endif
		FavoriteUser::MaskType flags;
		int uploadLimit;
		bool isFav = FavoriteManager::getInstance()->getFavUserParam(message.replyTo->getUser(), flags, uploadLimit);
		if (!isFav || (flags & FavoriteUser::FLAG_IGNORE_PRIVATE))
			return false;
	}
	if (message.replyTo->getIdentity().isHub())
	{
		if (BOOLSETTING(IGNORE_HUB_PMS) && !isInOperatorList(message.replyTo->getIdentity().getNick()))
		{
			fire(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.text);
			return false;
		}
		return !FavoriteManager::getInstance()->hasIgnorePM(message.replyTo->getUser());
	}
	if (message.replyTo->getIdentity().isBot())
	{
		if (BOOLSETTING(IGNORE_BOT_PMS) && !isInOperatorList(message.replyTo->getIdentity().getNick()))
		{
			fire(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.text);
			return false;
		}
		return !FavoriteManager::getInstance()->hasIgnorePM(message.replyTo->getUser());
	}
	auto pmFlags = FavoriteManager::getInstance()->getFlags(message.replyTo->getUser());
	if (BOOLSETTING(PROTECT_PRIVATE) && !(pmFlags & (FavoriteUser::FLAG_FREE_PM_ACCESS | FavoriteUser::FLAG_IGNORE_PRIVATE)))
	{
		switch (passwordStatus)
		{
			case UserManager::GRANTED:
				return true;
			case UserManager::FIRST:
			{
				const string& password = SETTING(PM_PASSWORD);
				UserManager::getInstance()->addPMPassword(message.replyTo->getUser(), password);
				if (response)
				{
					StringMap params;
					params["pm_pass"] = password;
					*response = Util::formatParams(SETTING(PM_PASSWORD_HINT), params, false);
				}
				if (BOOLSETTING(PROTECT_PRIVATE_SAY))
				{
					fire(ClientListener::StatusMessage(), this, STRING(REJECTED_PRIVATE_MESSAGE_FROM) + " " + message.replyTo->getIdentity().getNick());
				}
				return false;
			}
			default: // WAITING, CHECKED
				return false;
		}
	}
	else
	{
		if ((pmFlags & FavoriteUser::FLAG_IGNORE_PRIVATE)
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		        || UploadManager::isBanReply(message.replyTo->getUser())
#endif
		   )
			return false;
		return true;
	}
}

bool Client::isChatMessageAllowed(const ChatMessage& message, const string& nick) const
{
	if (!message.from)
		return nick.empty() || !UserManager::getInstance()->isInIgnoreList(nick);
	if (isMe(message.from))
		return true;
	if (message.thirdPerson && BOOLSETTING(IGNORE_ME))
		return false;
	if (BOOLSETTING(SUPPRESS_MAIN_CHAT) && !isOp())
		return false;
	if (UserManager::getInstance()->isInIgnoreList(message.from->getIdentity().getNick()))
		return false;
	return true;
}

void Client::logPM(const ChatMessage& message) const
{
	if (BOOLSETTING(LOG_PRIVATE_CHAT) && message.from && message.from->getUser())
	{
		const Identity& from = message.from->getIdentity();
		bool myMessage = from.getUser()->isMe();
		StringMap params;
		message.getUserParams(params, hubURL, myMessage);
		params["message"] = ChatMessage::formatNick(from.getNick(), message.thirdPerson) + message.text;
		const OnlineUserPtr& peer = myMessage ? message.to : message.replyTo;
		string extra = ChatMessage::getExtra(peer->getIdentity());
		if (!extra.empty())
			params["extra"] = " | " + extra;
		LOG(PM, params);
	}
}

void Client::processIncomingPM(std::unique_ptr<ChatMessage>& message, string& response)
{
	if (isPrivateMessageAllowed(*message, &response, false))
		fire(ClientListener::Message(), this, message);
	else
	{
		logPM(*message);
		if (BOOLSETTING(LOG_IF_SUPPRESS_PMS) && response.empty())
		{
			string hubName = getHubName();
			const string& hubUrl = getHubUrl();
			if (hubName != hubUrl)
			{
				hubName += " (";
				hubName += hubUrl;
				hubName += ')';
			}
			LogManager::message(CSTRING_F(PM_IGNORED, message->replyTo->getIdentity().getNick() % hubName));
		}
	}
}

void Client::fireOutgoingPM(const OnlineUserPtr& user, const string& message, bool thirdPerson, bool automatic)
{
	const OnlineUserPtr& me = getMyOnlineUser();

	unique_ptr<ChatMessage> chatMessage(new ChatMessage(message, me, user, me, thirdPerson));
	if (!isPrivateMessageAllowed(*chatMessage, nullptr, automatic))
	{
		logPM(*chatMessage);
		return;
	}

	fire(ClientListener::Message(), this, chatMessage);
}

bool Client::isInOperatorList(const string& userName) const
{
	LOCK(csOpChat);
	return !opChat.empty() && std::regex_match(userName, reOpChat);
}

void Client::getCounts(unsigned& normal, unsigned& registered, unsigned& op)
{
	normal = g_counts[COUNT_NORMAL];
	registered = g_counts[COUNT_REGISTERED];
	op = g_counts[COUNT_OP];
}

void Client::getFakeCounts(unsigned& normal, unsigned& registered, unsigned& op) const
{
	// Exclusive hub, send H:1/0/0 or similar
	normal = registered = op = 0;
	if (isOp())
		op = 1;
	else if (isRegistered())
		registered = 1;
	else
		normal = 1;
}

void Client::setRawCommands(const string commands[])
{
	for (int i = 0; i < 5; ++i)
		rawCommands[i] = commands[i];
}

const string& Client::getRawCommand(int command) const
{
	if (command < 0 || command >= 5)
		return Util::emptyString;
	return rawCommands[command];
}

void Client::processPasswordRequest(const string& pwd)
{
	if (!pwd.empty())
	{
		password(pwd, false);
		fire(ClientListener::StatusMessage(), this, STRING(STORED_PASSWORD_SENT));
	}
	else
	{
		fire(ClientListener::GetPassword(), this);
	}
}

void Client::escapeParams(StringMap& sm) const
{
	for (auto i = sm.begin(); i != sm.end(); ++i)
	{
		i->second = escape(i->second);
	}
}

void Client::setSearchInterval(unsigned interval)
{
	// min interval is 2 seconds in FlylinkDC
	if (interval < 2000) interval = 2000;
	else if (interval > 120000) interval = 120000;
	searchQueue.interval = interval;
}

void Client::setSearchIntervalPassive(unsigned interval)
{
	// min interval is 2 seconds in FlylinkDC
	if (interval < 2000) interval = 2000;
	else if (interval > 120000) interval = 120000;
	searchQueue.intervalPassive = interval;
}

void Client::setClientId(bool overrideId, const string& name, const string& version)
{
	this->overrideId = overrideId;
	if (overrideId)
	{
		clientName = name;
		clientVersion = version;
	} else
	{
		clientName = getAppName();
		clientVersion = getAppVersion();
	}
}

bool Client::removeRandomSuffix(string& nick)
{
	int digits = 0;
	for (string::size_type i = nick.length()-1; i >= 2; i--)
	{
		if (isdigit(nick[i]))
		{
			digits++;
			continue;
		}
		if (nick[i] != 'R' || nick[i-1] != '_' || !digits)
			return false;
		nick.erase(i-1);
		return true;
	}
	return false;
}

void Client::appendRandomSuffix(string& nick)
{
	char buf[8];
	sprintf(buf, "_R%03u", Util::rand(1000));
	nick.append(buf, 5);
}

void Client::clearUserCommands(int ctx)
{
	WRITE_LOCK(*csUserCommands);
	if (ctx != UserCommand::CONTEXT_MASK)
	{
		vector<UserCommand> newCommands;
		for (const UserCommand& uc : userCommands)
			if (!(uc.getCtx() & ctx))
				newCommands.push_back(uc);
		userCommands = std::move(newCommands);
	}
	else
		userCommands.clear();
}

void Client::addUserCommand(const UserCommand& uc)
{
	size_t maxCommands = SETTING(MAX_HUB_USER_COMMANDS);
	WRITE_LOCK(*csUserCommands);
	if (userCommands.size() < maxCommands)
		userCommands.push_back(uc);
}

void Client::removeUserCommand(const string& name)
{
	WRITE_LOCK(*csUserCommands);
	for (vector<UserCommand>::iterator i = userCommands.begin(); i != userCommands.end(); ++i)
		if (i->getName() == name)
		{
			userCommands.erase(i);
			break;
		}
}

void Client::getUserCommands(vector<UserCommand>& result) const
{
	READ_LOCK(*csUserCommands);
	for (const UserCommand& uc : userCommands)
		result.push_back(uc);
}

string Client::getOpChat() const
{
	LOCK(csOpChat);
	return opChat;
}
