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

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
#include "CFlylinkDBManager.h"
#endif

std::atomic<uint32_t> Client::g_counts[COUNT_UNCOUNTED];

Client::Client(const string& hubURL, char separator, bool secure, Socket::Protocol proto) :
	m_cs(std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock())),
	reconnDelay(120),
	lastActivity(GET_TICK()),
	autoReconnect(false),
	encoding(Text::CHARSET_SYSTEM_DEFAULT),
	state(STATE_DISCONNECTED),
	clientSock(nullptr),
	hubURL(hubURL),
	port(0),
	separator(separator),
	secure(secure),
	countType(COUNT_UNCOUNTED),
	bytesShared(0),
	exclChecks(false),
	hideShare(false),
	overrideId(false),
	proto(proto),
	userListLoaded(false),
	suppressChatAndPM(false)
{
	dcassert(hubURL == Text::toLower(hubURL));
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	m_HubID = CFlylinkDBManager::getInstance()->get_dic_hub_id(hubURL);
	dcassert(m_HubID != 0);
#endif
	const auto myUser = std::make_shared<User>(ClientManager::getMyCID(), "", m_HubID);
	const auto hubUser = std::make_shared<User>(CID(), "", m_HubID);
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
	
	string file, scheme, query, fragment;
	Util::decodeUrl(getHubUrl(), scheme, address, port, file, query, fragment);
	if (!query.empty())
	{
		keyprint = Util::getQueryParam(query, "kp");
#ifdef _DEBUG
		LogManager::message("keyprint = " + keyprint);
#endif
	}
	TimerManager::getInstance()->addListener(this);
}

Client::~Client()
{
	dcassert(!clientSock);
	FavoriteManager::removeHubUserCommands(UserCommand::CONTEXT_MASK, getHubUrl());
#ifdef _DEBUG
	if (!ClientManager::isBeforeShutdown())
		dcassert(FavoriteManager::countHubUserCommands(getHubUrl()) == 0);
#endif
	updateCounts(true);
}

void Client::resetSocket() noexcept
{
	if (clientSock)
	{
		clientSock->shutdown();
		clientSock->joinThread();
		BufferedSocket::destroyBufferedSocket(clientSock);
		clientSock = nullptr;
	}
}

void Client::reconnect()
{
	disconnect(true);
	setAutoReconnect(true);
	setReconnDelay(0);
}

void Client::shutdown()
{
	state = STATE_DISCONNECTED;
	TimerManager::getInstance()->removeListener(this);
	resetSocket();
}

const FavoriteHubEntry* Client::reloadSettings(bool updateNick)
{
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	string speedDescription;
#endif
	const FavoriteHubEntry* hub = FavoriteManager::getFavoriteHubEntry(getHubUrl());
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
			if (!getRandomTempNick().empty()) // сгенерили _Rxxx?
				nick = getRandomTempNick();
			checkNick(nick);
			setMyNick(nick);
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
			setPassword(hub->getPassword());
		}
		
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		setHideShare(hub->getHideShare());
#endif
		setFavIp(hub->getIP());
		
		int hubEncoding = hub->getEncoding();
		if (hubEncoding)
			setEncoding(hubEncoding);
		
		if (hub->getSearchInterval() < 2) // FlylinkDC changed 10 to 2
		{
			dcassert(SETTING(MIN_SEARCH_INTERVAL) > 2);
			const auto newInterval = std::max(SETTING(MIN_SEARCH_INTERVAL), 2);
			const auto newIntervalPassive = std::max(SETTING(MIN_SEARCH_INTERVAL_PASSIVE), 2);
			setSearchInterval(newInterval * 1000, false);
			setSearchIntervalPassive(newIntervalPassive * 1000, false);
		}
		else
		{
			setSearchInterval(hub->getSearchInterval() * 1000, false);
			setSearchIntervalPassive(hub->getSearchIntervalPassive() * 1000, false);
		}
		
		opChat = hub->getOpChat();
		if (!Wildcards::regexFromPatternList(reOpChat, hub->getOpChat(), false)) opChat.clear();
		exclChecks = hub->getExclChecks();
	}
	else
	{
		if (updateNick)
		{
			string nick = SETTING(NICK);
			checkNick(nick);
			setMyNick(nick);
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
	}
	return hub;
}

void Client::connect()
{
	resetSocket();
	bytesShared.store(0);
	setAutoReconnect(true);
	setReconnDelay(Util::rand(10, 30));
	reloadSettings(true);
	resetRegistered();
	resetOp();
	
	state = STATE_CONNECTING;
	
	try
	{
		clientSock = BufferedSocket::getBufferedSocket(separator, this);
		clientSock->connect(address, port, secure, BOOLSETTING(ALLOW_UNTRUSTED_HUBS), true, proto);
		dcdebug("Client::connect() %p\n", this);
	}
	catch (const Exception& e)
	{
		state = STATE_DISCONNECTED;
		fly_fire2(ClientListener::ClientFailed(), this, e.getError());
	}
	updateActivity();
}

void Client::connectIfNetworkOk()
{
	if (state != STATE_DISCONNECTED && state != STATE_WAIT_PORT_TEST) return;
	if (ConnectivityManager::getInstance()->isSetupInProgress())
	{
		if (state != STATE_WAIT_PORT_TEST)
		{
			state = STATE_WAIT_PORT_TEST;
			fly_fire2(ClientListener::StatusMessage(), this, CSTRING(WAITING_NETWORK_CONFIG));
		}
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
		const FavoriteHubEntry* fe = FavoriteManager::getFavoriteHubEntry(getHubUrl());
		return ClientManager::isActive(fe);
	}
	return true; // Manual active
}

void Client::send(const char* message, size_t len)
{
	if (!isReady())
	{
		dcdebug("Send message failed, hub is disconnected!");//[+] IRainman
		//dcassert(isReady()); // ѕод отладкой падаем тут. найти причину.
		return;
	}
	updateActivity();
	clientSock->write(message, len);
	
	if (CMD_DEBUG_ENABLED()) COMMAND_DEBUG(toUtf8(string(message, len)), DebugTask::HUB_OUT, getIpPort());
}

void Client::onConnected() noexcept
{
	updateActivity();
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
				fly_fire2(ClientListener::ClientFailed(), this, "Keyprint mismatch");
				return;
			}
		}
	}
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	FavoriteManager::changeConnectionStatus(getHubUrl(), ConnectionStatus::SUCCES);
#endif
	fly_fire1(ClientListener::Connected(), this);
	state = STATE_PROTOCOL;
}

void Client::onFailed(const string& line) noexcept
{
	// although failed consider initialized
	state = STATE_DISCONNECTED;
	FavoriteManager::removeHubUserCommands(UserCommand::CONTEXT_MASK, getHubUrl());
	if (!ClientManager::isBeforeShutdown())
	{
		updateActivity();
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		FavoriteManager::changeConnectionStatus(getHubUrl(), ConnectionStatus::CONNECTION_FAILURE);
#endif
	}
	fly_fire2(ClientListener::ClientFailed(), this, line);
}

void Client::disconnect(bool graceless)
{
	state = STATE_DISCONNECTED;
	FavoriteManager::removeHubUserCommands(UserCommand::CONTEXT_MASK, getHubUrl());
	if (clientSock)
		clientSock->disconnect(graceless);
}

bool Client::isSecure() const
{
	return clientSock && clientSock->isSecure();
}

bool Client::isTrusted() const
{
	return clientSock && clientSock->isTrusted();
}

string Client::getCipherName() const
{
	return clientSock ? clientSock->getCipherName() : Util::emptyString;
}

vector<uint8_t> Client::getKeyprint() const
{
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
		const string& myUserIp = getMyIdentity().getIpAsString(); // [!] opt, and fix done: [4] https://www.box.net/shared/c497f50da28f3dfcc60a
		if (!myUserIp.empty())
		{
			return myUserIp; // [!] Best case - the server detected it.
		}
	}
	string ip;
	// Favorite hub Ip
	if (!getFavIp().empty())
	{
		ip = Socket::resolve(getFavIp());
		if (!ip.empty()) return ip;
	}
	string externalIp = BOOLSETTING(WAN_IP_MANUAL) ? SETTING(EXTERNAL_IP) : Util::emptyString;
	if (!externalIp.empty() && BOOLSETTING(NO_IP_OVERRIDE))
	{
		ip = Socket::resolve(externalIp);
		if (!ip.empty()) return ip;
	}
	ip = ConnectivityManager::getInstance()->getReflectedIP();
	if (!ip.empty()) return ip;
	if (!externalIp.empty())
	{
		ip = Socket::resolve(externalIp);
		if (!ip.empty()) return ip;
	}
	return Util::getLocalOrBindIp(false);
}

uint64_t Client::searchInternal(const SearchParamToken& sp)
{
	//dcdebug("Queue search %s\n", sp.m_filter.c_str());
	
	if (searchQueue.interval)
	{
		Search s;
		s.forcePassive = sp.forcePassive;
		s.fileTypesBitmap = sp.fileType; // FIXME ?!
		s.size = sp.size;
		s.filter = sp.filter;
		s.filterExclude = sp.filterExclude;
		s.sizeMode = sp.sizeMode;
		s.token = sp.token;
		s.extList = sp.extList;
		s.owners.insert(sp.owner);
		
		searchQueue.add(s);
		
		const uint64_t now = GET_TICK();
		return searchQueue.getSearchTime(sp.owner, now) - now;
	}
	searchToken(sp);
	return 0;
}

void Client::onDataLine(const string& aLine) noexcept
{
	updateActivity();
	if (CMD_DEBUG_ENABLED()) COMMAND_DEBUG(aLine, DebugTask::HUB_IN, getIpPort());
}

void Client::on(Minute, uint64_t aTick) noexcept
{
	if (state == STATE_NORMAL && (aTick >= (getLastActivity() + 118 * 1000)))
	{
		send(&separator, 1);
	}
}

void Client::on(Second, uint64_t aTick) noexcept
{
	if (state == STATE_WAIT_PORT_TEST)
	{
		connectIfNetworkOk();
		return;
	}
	if (state == STATE_DISCONNECTED && getAutoReconnect() && aTick > getLastActivity() + getReconnDelay() * 1000)
	{
		// Try to reconnect...
		connect();
	}
	else if (state == STATE_IDENTIFY && getLastActivity() + 30000 < aTick)
	{
		if (clientSock)
			clientSock->disconnect(false);
	}
	else if ((state == STATE_CONNECTING || state == STATE_PROTOCOL) && getLastActivity() + 60000 < aTick)
	{
		reconnect();
	}
	if (searchQueue.interval == 0)
	{
		//dcassert(0);
		return;
	}
	
	if (isConnected())
	{
		Search s;
		if (searchQueue.pop(s, aTick, !isActive()))
		{
			// TODO - пробежатьс€ по битовой маске?
			// ≈сли она там есть
			SearchParamToken sp;
			sp.token = s.token;
			sp.sizeMode = s.sizeMode;
			sp.fileType = s.fileTypesBitmap;
			sp.size = s.size;
			sp.filter = s.filter;
			sp.filterExclude = s.filterExclude;
			sp.forcePassive = s.forcePassive;
			sp.extList = s.extList;
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

bool Client::isMeCheck(const OnlineUserPtr& ou) const
{
	return !ou || ou->getUser() == ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks();
}

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

void Client::processingPassword()
{
	if (!getPassword().empty())
	{
		password(getPassword());
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
		clientVersionFull = clientVersion = version;		
	} else
	{
		clientName = getFlylinkDCAppCaption();
		clientVersionFull = clientVersion = A_SHORT_VERSIONSTRING;
		clientVersionFull += "-" A_REVISION_NUM_STR;
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
