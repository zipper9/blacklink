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

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
#include "CFlylinkDBManager.h"
#endif

std::atomic<uint32_t> Client::g_counts[COUNT_UNCOUNTED];

Client::Client(const string& hubURL, const string& address, uint16_t port, char separator, bool secure, Socket::Protocol proto) :
	reconnDelay(120),
	lastActivity(GET_TICK()),
	pendingUpdate(0),
	autoReconnect(false),
	encoding(Text::CHARSET_SYSTEM_DEFAULT),
	state(STATE_DISCONNECTED),
	clientSock(nullptr),
	hubURL(hubURL),
	address(address),
	port(port),
	separator(separator),
	secure(secure),
	countType(COUNT_UNCOUNTED),
	bytesShared(0),
	exclChecks(false),
	hideShare(false),
	overrideId(false),
	proto(proto),
	userListLoaded(false),
	suppressChatAndPM(false),
	isExclusiveHub(false)
{
	dcassert(hubURL == Text::toLower(hubURL));
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	hubID = CFlylinkDBManager::getInstance()->get_dic_hub_id(hubURL);
	dcassert(hubID != 0);
	const auto myUser = std::make_shared<User>(ClientManager::getMyCID(), Util::emptyString, hubID);
	const auto hubUser = std::make_shared<User>(CID(), Util::emptyString, hubID);
#else
	const auto myUser = std::make_shared<User>(ClientManager::getMyCID(), Util::emptyString);
	const auto hubUser = std::make_shared<User>(CID(), Util::emptyString);
#endif
	myUser->setFlag(User::MYSELF);
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

	myOnlineUser = std::make_shared<OnlineUser>(myUser, *this, 0);
	hubOnlineUser = std::make_shared<OnlineUser>(hubUser, *this, AdcCommand::HUB_SID);
	TimerManager::getInstance()->addListener(this);
}

Client::~Client()
{
	dcassert(!clientSock);
	FavoriteManager::getInstance()->removeHubUserCommands(UserCommand::CONTEXT_MASK, getHubUrl());
#ifdef _DEBUG
	if (!ClientManager::isBeforeShutdown())
		dcassert(FavoriteManager::getInstance()->countHubUserCommands(getHubUrl()) == 0);
#endif
	updateCounts(true);
}

void Client::resetSocket(BufferedSocket* bufferedSocket) noexcept
{
	if (bufferedSocket)
	{
		bufferedSocket->shutdown();
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
	BufferedSocket* prevSocket = nullptr;
	csState.lock();
	state = STATE_DISCONNECTED;
	prevSocket = clientSock;
	clientSock = nullptr;
	csState.unlock();

	TimerManager::getInstance()->removeListener(this);
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
			CFlyFastLock(csState);
			storedPassword = hub->getPassword();
		}
		
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		setHideShare(hub->getHideShare());
#endif
		setFavIp(hub->getIP());
		
		int hubEncoding = hub->getEncoding();
		if (hubEncoding)
			setEncoding(hubEncoding);
		
		int searchInterval = hub->getSearchInterval();
		if (searchInterval < 2)
			searchInterval = std::max(SETTING(MIN_SEARCH_INTERVAL), 2);
		setSearchInterval(searchInterval * 1000, false);

		searchInterval = hub->getSearchIntervalPassive();
		if (searchInterval < 2)
			searchInterval = std::max(SETTING(MIN_SEARCH_INTERVAL_PASSIVE), 2);
		setSearchIntervalPassive(searchInterval * 1000, false);

		opChat = hub->getOpChat();
		if (!Wildcards::regexFromPatternList(reOpChat, hub->getOpChat(), false)) opChat.clear();
		exclChecks = hub->getExclChecks();
		isExclusiveHub = hub->getExclusiveHub();
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
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		setHideShare(false);
#endif
		setFavIp(Util::emptyString);
		
		setSearchInterval(SETTING(MIN_SEARCH_INTERVAL) * 1000, false);
		setSearchIntervalPassive(SETTING(MIN_SEARCH_INTERVAL_PASSIVE) * 1000, false);
		
		opChat.clear();
		exclChecks = false;
		isExclusiveHub = false;
	}
	fm->releaseFavoriteHubEntryPtr(hub);
}

void Client::connect()
{
	BufferedSocket* prevSocket = nullptr;
	csState.lock();	
	state = STATE_CONNECTING;
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
	
	csState.lock();
	try
	{
		clientSock = BufferedSocket::getBufferedSocket(separator, this);
		clientSock->connect(address, port, secure, BOOLSETTING(ALLOW_UNTRUSTED_HUBS), true, proto);
		dcdebug("Client::connect() %p\n", this);
	}
	catch (const Exception& e)
	{
		state = STATE_DISCONNECTED;
		csState.unlock();
		fly_fire2(ClientListener::ClientFailed(), this, e.getError());
		return;
	}
	csState.unlock();
}

void Client::connectIfNetworkOk()
{
	{
		CFlyFastLock(csState);
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
			fly_fire2(ClientListener::StatusMessage(), this, CSTRING(WAITING_NETWORK_CONFIG));
		return;
	}
	connect();
}

bool ClientBase::isActive() const
{
	if (SETTING(FORCE_PASSIVE_INCOMING_CONNECTIONS))
		return false;

	extern bool g_DisableTestPort;
	if (!g_DisableTestPort)
	{
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(getHubUrl());
		int favHubMode = fhe ? fhe->getMode() : 0;
		fm->releaseFavoriteHubEntryPtr(fhe);
		return ClientManager::isActive(favHubMode);
	}
	return true; // Manual active
}

void Client::send(const char* message, size_t len)
{
	{
		CFlyFastLock(csState);
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
	csState.lock();
	updateActivityL();
	boost::system::error_code ec;
	ip = boost::asio::ip::address_v4::from_string(clientSock->getIp(), ec);
	dcassert(!ec);
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
				fly_fire2(ClientListener::ClientFailed(), this, "Keyprint mismatch");
				return;
			}
		}
	}
	state = STATE_PROTOCOL;
	csState.unlock();
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	auto fm = FavoriteManager::getInstance();
	fm->changeConnectionStatus(getHubUrl(), ConnectionStatus::SUCCES);
#endif
	fly_fire1(ClientListener::Connected(), this);
}

void Client::onFailed(const string& line) noexcept
{
	csState.lock();
	state = STATE_DISCONNECTED;
	updateActivityL();
	csState.unlock();

	auto fm = FavoriteManager::getInstance();
	fm->removeHubUserCommands(UserCommand::CONTEXT_MASK, getHubUrl());
	if (!ClientManager::isBeforeShutdown())
	{
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		fm->changeConnectionStatus(getHubUrl(), ConnectionStatus::CONNECTION_FAILURE);
#endif
	}
	fly_fire2(ClientListener::ClientFailed(), this, line);
}

void Client::disconnect(bool graceless)
{
	csState.lock();
	state = STATE_DISCONNECTED;
	if (clientSock)
		clientSock->disconnect(graceless);
	csState.unlock();

	FavoriteManager::getInstance()->removeHubUserCommands(UserCommand::CONTEXT_MASK, getHubUrl());
}

bool Client::isSecure() const
{
	CFlyFastLock(csState);
	return clientSock && clientSock->isSecure();
}

bool Client::isTrusted() const
{
	CFlyFastLock(csState);
	return clientSock && clientSock->isTrusted();
}

string Client::getCipherName() const
{
	CFlyFastLock(csState);
	return clientSock ? clientSock->getCipherName() : Util::emptyString;
}

vector<uint8_t> Client::getKeyprint() const
{
	CFlyFastLock(csState);
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

void Client::changeBytesShared(Identity& id, const int64_t bytes)
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
		fly_fire2(ClientListener::UserListUpdated(), this, userList);
	}
}

void Client::fireUserUpdated(const OnlineUserPtr& aUser)
{
	if (!ClientManager::isBeforeShutdown())
	{
		fly_fire1(ClientListener::UserUpdated(), aUser);
	}
}

string Client::getLocalIp() const
{
	// [!] IRainman fix:
	// [!] If possible, always return the hub that IP, which he identified with us when you connect.
	// [!] This saves the user from a variety of configuration problems.
	if (getMyIdentity().isIPValid())
	{
		const string myUserIp = getMyIdentity().getIpAsString();
		if (!myUserIp.empty())
		{
			return myUserIp; // [!] Best case - the server detected it.
		}
	}
	// Favorite hub Ip
	if (!getFavIp().empty())
	{
		auto addr = Socket::resolveHost(getFavIp());
		if (!addr.is_unspecified()) return addr.to_string();
	}
	string externalIp = BOOLSETTING(WAN_IP_MANUAL) ? SETTING(EXTERNAL_IP) : Util::emptyString;
	if (!externalIp.empty() && BOOLSETTING(NO_IP_OVERRIDE))
	{
		auto addr = Socket::resolveHost(externalIp);
		if (!addr.is_unspecified()) return addr.to_string();
	}
	string ip = ConnectivityManager::getInstance()->getReflectedIP();
	if (!ip.empty()) return ip;
	if (!externalIp.empty())
	{
		auto addr = Socket::resolveHost(externalIp);
		if (!addr.is_unspecified()) return addr.to_string();
	}
	return Util::getLocalOrBindIp(false);
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
	
	if (state != STATE_DISCONNECTED)
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

bool Client::isPrivateMessageAllowed(const ChatMessage& message)
{
	if (isMe(message.replyTo))
	{
		if (UserManager::expectPasswordFromUser(message.to->getUser())
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		        || UploadManager::isBanReply(message.to->getUser())
#endif
		   )
			return false;
		return true;
	}
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
		bool isFav = FavoriteManager::getFavUserParam(message.replyTo->getUser(), flags, uploadLimit);
		if (!isFav || (flags & FavoriteUser::FLAG_IGNORE_PRIVATE))
		{
			if (BOOLSETTING(LOG_IF_SUPPRESS_PMS))
			{
				char buf[1024];
				_snprintf(buf, sizeof(buf), CSTRING(LOG_IF_SUPPRESS_PMS), message.replyTo->getIdentity().getNick().c_str(), getHubName().c_str(), getHubUrl().c_str());
				LogManager::message(buf);
			}
			return false;
		}
		return true;
	}
	if (message.replyTo->getIdentity().isHub())
	{
		if (BOOLSETTING(IGNORE_HUB_PMS) && !isInOperatorList(message.replyTo->getIdentity().getNick()))
		{
			fly_fire2(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.text);
			return false;
		}
		return !FavoriteManager::getInstance()->hasIgnorePM(message.replyTo->getUser());
	}
	if (message.replyTo->getIdentity().isBot())
	{
		if (BOOLSETTING(IGNORE_BOT_PMS) && !isInOperatorList(message.replyTo->getIdentity().getNick()))
		{
			fly_fire2(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.text);
			return false;
		}
		return !FavoriteManager::getInstance()->hasIgnorePM(message.replyTo->getUser());
	}
	if (BOOLSETTING(PROTECT_PRIVATE) && !FavoriteManager::hasFreePM(message.replyTo->getUser()))
	{
		switch (UserManager::checkPrivateMessagePassword(message))
		{
			case UserManager::FREE:
				return true;
			case UserManager::WAITING:
				return false;
			case UserManager::FIRST:
			{
				StringMap params;
				params["pm_pass"] = SETTING(PM_PASSWORD);
				privateMessage(message.replyTo, Util::formatParams(SETTING(PM_PASSWORD_HINT), params, false), false);
				if (BOOLSETTING(PROTECT_PRIVATE_SAY))
				{
					fly_fire2(ClientListener::StatusMessage(), this, STRING(REJECTED_PRIVATE_MESSAGE_FROM) + ": " + message.replyTo->getIdentity().getNick());
				}
				return false;
			}
			case UserManager::CHECKED:
			{
				privateMessage(message.replyTo, SETTING(PM_PASSWORD_OK_HINT), true);
				
				// TODO needs?
				// const tstring passwordOKMessage = _T('<') + message.replyTo->getUser()->getLastNickT() + _T("> ") + TSTRING(PRIVATE_CHAT_PASSWORD_OK_STARTED);
				// PrivateFrame::gotMessage(from, to, replyTo, passwordOKMessage, getHubHint(), myPM, pm.thirdPerson); // !SMT!-S
				
				return true;
			}
			default: // Only for compiler.
			{
				dcassert(0);
				return false;
			}
		}
	}
	else
	{
		if (FavoriteManager::getInstance()->hasIgnorePM(message.replyTo->getUser())
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

bool Client::isInOperatorList(const string& userName) const
{
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

const string& Client::getRawCommand(int command) const
{
	switch (command)
	{
		case 1:
			return rawOne;
		case 2:
			return rawTwo;
		case 3:
			return rawThree;
		case 4:
			return rawFour;
		case 5:
			return rawFive;
	}
	return Util::emptyString;
}

void Client::processPasswordRequest(const string& pwd)
{
	if (!pwd.empty())
	{
		password(pwd, false);
		fly_fire2(ClientListener::StatusMessage(), this, STRING(STORED_PASSWORD_SENT));
	}
	else
	{
		fly_fire1(ClientListener::GetPassword(), this);
	}
}

void Client::escapeParams(StringMap& sm) const
{
	for (auto i = sm.begin(); i != sm.end(); ++i)
	{
		i->second = escape(i->second);
	}
}

void Client::setSearchInterval(unsigned interval, bool fromRule)
{
	// min interval is 2 seconds in FlylinkDC
	if (interval < 2000) interval = 2000;
	if (!fromRule && interval > 120000) interval = 120000;
	searchQueue.interval = interval;
}

void Client::setSearchIntervalPassive(unsigned interval, bool fromRule)
{
	// min interval is 2 seconds in FlylinkDC
	if (interval < 2000) interval = 2000;
	if (!fromRule && interval > 120000) interval = 120000;
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
