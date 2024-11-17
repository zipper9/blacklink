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
#include <boost/algorithm/string/trim.hpp>

#include "SettingsManager.h"
#include "UploadManager.h"
#include "ThrottleManager.h"
#include "LogManager.h"
#include "CompatibilityManager.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "QueueManager.h"
#include "SearchManager.h"
#include "UserManager.h"
#include "FavoriteManager.h"
#include "DebugManager.h"
#include "Wildcards.h"
#include "ParamExpander.h"
#include "Resolver.h"
#include "Random.h"
#include "Util.h"
#include "ConfCore.h"

static const unsigned USER_CHECK_INTERVAL = 60000;
static const unsigned USER_CHECK_VALIDITY = 60 * 60000;

std::atomic<uint32_t> Client::g_counts[COUNT_UNCOUNTED];

#ifdef _DEBUG
std::atomic_int Client::clientCount(0);
#endif

Client::Client(const string& hubURL, const string& address, uint16_t port, char separator, bool secure, Socket::Protocol proto) :
	reconnDelay(120),
	lastActivity(GET_TICK()),
	pendingUpdate(0),
	nextUserCheck(0),
	autoReconnect(false),
	state(STATE_DISCONNECTED),
	connMode(0),
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
	fakeSlots(-1),
	favMode(0),
	preferIP6(false),
	favoriteId(0),
	csUserCommands(RWLock::create())
{
	dcassert(hubURL == Text::toLower(hubURL));
#ifdef _DEBUG
	++clientCount;
#endif
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	encoding = Text::charsetFromString(ss->getString(Conf::DEFAULT_CODEPAGE));
	ss->unlockRead();
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
	auto ss = SettingsManager::instance.getCoreSettings();
	auto fm = FavoriteManager::getInstance();
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	string speedDescription;
#endif
	const FavoriteHubEntry* hub = favoriteId ? fm->getFavoriteHubEntryPtr(favoriteId) : fm->getFavoriteHubEntryPtr(getHubUrl());
	string clientName, clientVersion;
	bool overrideClientId = false;
	if (hub && hub->getOverrideId())
	{
		clientName = hub->getClientName();
		clientVersion = hub->getClientVersion();
		overrideClientId = !clientName.empty();
	}
	
	ss->lockRead();
	if (!overrideClientId && ss->getBool(Conf::OVERRIDE_CLIENT_ID))
	{
		FavoriteManager::splitClientId(ss->getString(Conf::CLIENT_ID), clientName, clientVersion);
		overrideClientId = !clientName.empty();
	}
	const string description = ss->getString(Conf::DESCRIPTION);
	const string email = ss->getString(Conf::EMAIL);
	string nick = ss->getString(Conf::NICK);
	const int minSearchInterval = ss->getInt(Conf::MIN_SEARCH_INTERVAL);
	const int minSearchIntervalPassive = ss->getInt(Conf::MIN_SEARCH_INTERVAL_PASSIVE);
	ss->unlockRead();

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
			nick = hub->getNick(true);
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
			    description);
		}
		
		if (!hub->getEmail().empty())
		{
			setCurrentEmail(hub->getEmail());
		}
		else
		{
			setCurrentEmail(email);
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
			searchInterval = minSearchInterval;
		else
			overrideSearchInterval = true;
		setSearchInterval(searchInterval * 1000);

		searchInterval = hub->getSearchIntervalPassive();
		if (searchInterval < 2)
			searchInterval = minSearchIntervalPassive;
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

		fakeSlots = hub->getFakeSlots();
		const string& kp = hub->getKeyPrint();
		if (!kp.empty()) setKeyPrint(kp);
		setSuppressChatAndPM(hub->getSuppressChatAndPM());
	}
	else
	{
		if (updateNick)
		{
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
		    description);
		setCurrentEmail(email);
		hideShare = false;
		shareGroup.init();
		setFavIp(Util::emptyString);
		favMode = 0;

		setSearchInterval(minSearchInterval * 1000);
		setSearchIntervalPassive(minSearchIntervalPassive * 1000);
		overrideSearchInterval = overrideSearchIntervalPassive = false;

		opChat.clear();
		exclChecks = false;
		fakeHubCount = false;
		fakeClientStatus = 0;
		fakeShareSize = -1;
		fakeShareFiles = -1;
		fakeSlots = -1;
		setSuppressChatAndPM(false);
	}
	fm->releaseFavoriteHubEntryPtr(hub);
	fire(ClientListener::SettingsLoaded(), this);
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
	bool hasTempNick = !randomTempNick.empty();
	csState.unlock();

	resetSocket(prevSocket);
	bytesShared.store(0);
	setAutoReconnect(true);
	setReconnDelay(Util::rand(10, 30));
	reloadSettings(!hasTempNick);
	resetRegistered();
	resetOp();
	updateConnectionStatus(ConnectionStatus::CONNECTING);

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool allowUntrusted = ss->getBool(Conf::ALLOW_UNTRUSTED_HUBS);
	ss->unlockRead();

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
		clientSock->connect(address, port, secure, allowUntrusted, true, proto);
		clientSock->start();
		dcdebug("Client::connect() %p\n", this);
	}
	catch (const Exception& e)
	{
		state = STATE_DISCONNECTED;
		csState.unlock();
		fire(ClientListener::ClientFailed(), this, e.getError());
		updateConnectionStatus(ConnectionStatus::FAILURE);
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
	csState.lock();
	bool result = (connMode & CONN_MODE_TCP_ACTIVE) != 0;
	csState.unlock();
	return result;
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
	int af = ip.type;
	if (clientSock->isSecure() && keyprint.compare(0, 7, "SHA256/", 7) == 0)
	{
		const auto kp = clientSock->getKeyprint();
		if (!kp.empty())
		{
			vector<uint8_t> kp2v(kp.size());
			Util::fromBase32(keyprint.c_str() + 7, &kp2v[0], kp2v.size());
			if (!std::equal(kp.begin(), kp.end(), kp2v.begin()))
			{
				state = STATE_DISCONNECTED;
				csState.unlock();
				fire(ClientListener::ClientFailed(), this, STRING(KEYPRINT_MISMATCH));
				return;
			}
		}
	}
	csState.unlock();
	
	uint16_t newConnMode = 0;
	if (ClientManager::isActiveMode(af, favMode, false))
		newConnMode |= CONN_MODE_TCP_ACTIVE;
	if (ClientManager::isActiveMode(af, favMode, true))
		newConnMode |= CONN_MODE_UDP_ACTIVE;
	csState.lock();
	connMode = newConnMode;
	state = STATE_PROTOCOL;
	csState.unlock();

	fire(ClientListener::Connected(), this);
}

void Client::onFailed(const string& line) noexcept
{
	csState.lock();
	bool connected = connSuccess;
	state = STATE_DISCONNECTED;
	updateActivityL();
	csState.unlock();

	if (!connected) updateConnectionStatus(ConnectionStatus::FAILURE);
	clearUserCheckList();
	fire(ClientListener::ClientFailed(), this, line);
}

void Client::disconnect(bool graceless)
{
	csState.lock();
	state = STATE_DISCONNECTED;
	if (clientSock)
		clientSock->disconnect(graceless);
	csState.unlock();
	clearUserCheckList();
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

void Client::fireUserUpdated(const OnlineUserPtr& user)
{
	if (!ClientManager::isBeforeShutdown())
		fire(ClientListener::UserUpdated(), user);
}

int Client::getLocalIp(Ip4Address& ip4, Ip6Address& ip6) const
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
				return GLIP_FLAG_MANUAL_IPV4;
			}
			if (ip.type == AF_INET6)
			{
				ip6 = ip.data.v6;
				ip4 = 0;
				return GLIP_FLAG_MANUAL_IPV6;
			}
		}
	}

	const Identity& id = getMyIdentity();
	ip4 = id.getIP4();
	ip6 = id.getIP6();

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool wanIpManual4 = ss->getBool(Conf::WAN_IP_MANUAL);
	string externalIp4 = wanIpManual4 ? ss->getString(Conf::EXTERNAL_IP) : string();
	const bool noIpOverride4 = ss->getBool(Conf::NO_IP_OVERRIDE);

	bool wanIpManual6 = ss->getBool(Conf::WAN_IP_MANUAL6);
	string externalIp6 = wanIpManual6 ? ss->getString(Conf::EXTERNAL_IP6) : string();
	const bool noIpOverride6 = ss->getBool(Conf::NO_IP_OVERRIDE6);
	ss->unlockRead();

	if (ip4 == 0)
	{
		if (wanIpManual4 && !Util::isValidIp4(externalIp4))
		{
			externalIp4.clear();
			wanIpManual4 = false;
		}
		if (externalIp4.empty() || !noIpOverride4)
		{
			IpAddress ip = ConnectivityManager::getInstance()->getReflectedIP(AF_INET);
			if (!Util::isEmpty(ip)) externalIp4 = Util::printIpAddress(ip);
		}
		if (externalIp4.empty())
		{
			IpAddress ip = ConnectivityManager::getInstance()->getLocalIP(AF_INET);
			ip4 = ip.data.v4;
		}
		else
			Util::parseIpAddress(ip4, externalIp4);
	}

	if (Util::isEmpty(ip6))
	{
		if (wanIpManual6 && !Util::isValidIp6(externalIp6))
		{
			externalIp6.clear();
			wanIpManual6 = false;
		}
		if (externalIp6.empty() || !noIpOverride6)
		{
			IpAddress ip = ConnectivityManager::getInstance()->getReflectedIP(AF_INET6);
			if (!Util::isEmpty(ip)) externalIp6 = Util::printIpAddress(ip);
		}
		if (externalIp6.empty())
		{
			IpAddress ip = ConnectivityManager::getInstance()->getLocalIP(AF_INET6);
			ip6 = ip.data.v6;
		}
		else
			Util::parseIpAddress(ip6, externalIp6);
	}
	return (wanIpManual4 ? GLIP_FLAG_MANUAL_IPV4 : 0) | (wanIpManual6 ? GLIP_FLAG_MANUAL_IPV6 : 0);
}

bool Client::checkIpType(int type) const
{
	if (type != AF_INET && type != AF_INET6)
		return false;
	Ip4Address ip4;
	Ip6Address ip6;
	getLocalIp(ip4, ip6);
	return type == AF_INET ? Util::isValidIp4(ip4) : Util::isValidIp6(ip6);
}

bool Client::allowNatTraversal()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool enabled = ss->getBool(Conf::ALLOW_NAT_TRAVERSAL);
	int outgoingMode = ss->getInt(Conf::OUTGOING_CONNECTIONS);
	ss->unlockRead();
	return enabled && outgoingMode == Conf::OUTGOING_DIRECT;
}

unsigned Client::searchInternal(const SearchParam& sp)
{
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

void Client::onDataLine(const char* buf, size_t len) noexcept
{
	updateActivityL();
	if (CMD_DEBUG_ENABLED())
		COMMAND_DEBUG(string(buf, len), DebugTask::HUB_IN, getIpPort());
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
		if (tick >= nextUserCheck)
			checkUsers(tick);
		onTimer(tick);
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
			SearchParam sp;
			sp.token = s.token;
			sp.sizeMode = s.sizeMode;
			sp.fileType = s.fileType;
			sp.size = s.size;
			sp.filter = std::move(s.filter);
			sp.filterExclude = std::move(s.filterExclude);
			sp.searchMode = s.searchMode;
			sp.extList = std::move(s.extList);
			sp.owner = 0;
			searchToken(sp);
		}
	}
}

#if 0 // Not used
OnlineUserPtr Client::getUser(const UserPtr& user)
{
	// for generic client, use ClientManager, but it does not correctly handle ClientManager::me
	ClientManager::LockInstanceOnlineUsers lockedInstance;
	return lockedInstance->getOnlineUserL(user);
}
#endif

bool Client::isPrivateMessageAllowed(const ChatMessage& message, string* response, bool automatic)
{
	if (isMe(message.replyTo))
	{
		if (!UserManager::getInstance()->checkOutgoingPM(message.to->getUser(), automatic))
			return false;
		return true;
	}
	int chatOptions = ClientManager::getChatOptions();
	UserManager::PasswordStatus passwordStatus;
	bool isOpen = UserManager::getInstance()->checkPMOpen(message, passwordStatus);
	if (passwordStatus == UserManager::CHECKED && response)
	{
		auto ss = SettingsManager::instance.getCoreSettings();
		ss->lockRead();
		*response = ss->getString(Conf::PM_PASSWORD_OK_HINT);
		ss->unlockRead();
	}
	if (isOpen)
		return true;
	if (suppressChatAndPM)
		return false;
	if (message.thirdPerson && (chatOptions & ClientManager::CHAT_OPTION_IGNORE_ME))
		return false;
	if (UserManager::getInstance()->isInIgnoreList(message.replyTo->getIdentity().getNick()))
		return false;
	if (chatOptions & ClientManager::CHAT_OPTION_SUPPRESS_PM)
	{
		FavoriteUser::MaskType flags;
		int uploadLimit;
		bool isFav = FavoriteManager::getInstance()->getFavUserParam(message.replyTo->getUser(), flags, uploadLimit);
		if (!isFav || (flags & FavoriteUser::FLAG_IGNORE_PRIVATE))
			return false;
	}
	if (message.replyTo->getIdentity().isHub())
	{
		if ((chatOptions & ClientManager::CHAT_OPTION_IGNORE_HUB_PMS) && !isInOperatorList(message.replyTo->getIdentity().getNick()))
		{
			fire(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.text);
			return false;
		}
		return !FavoriteManager::getInstance()->hasIgnorePM(message.replyTo->getUser());
	}
	if (message.replyTo->getIdentity().isBot())
	{
		if ((chatOptions & ClientManager::CHAT_OPTION_IGNORE_BOT_PMS) && !isInOperatorList(message.replyTo->getIdentity().getNick()))
		{
			fire(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.text);
			return false;
		}
		return !FavoriteManager::getInstance()->hasIgnorePM(message.replyTo->getUser());
	}
	auto pmFlags = FavoriteManager::getInstance()->getFlags(message.replyTo->getUser());
	if ((chatOptions & ClientManager::CHAT_OPTION_PROTECT_PRIVATE) &&
	    !(pmFlags & (FavoriteUser::FLAG_FREE_PM_ACCESS | FavoriteUser::FLAG_IGNORE_PRIVATE)))
	{
		switch (passwordStatus)
		{
			case UserManager::GRANTED:
				return true;
			case UserManager::FIRST:
			{
				auto ss = SettingsManager::instance.getCoreSettings();
				ss->lockRead();
				const string password = ss->getString(Conf::PM_PASSWORD);
				const string passwordHint = ss->getString(Conf::PM_PASSWORD_HINT);
				bool protectPrivateSay = ss->getBool(Conf::PROTECT_PRIVATE_SAY);
				ss->unlockRead();
				UserManager::getInstance()->addPMPassword(message.replyTo->getUser(), password);
				if (response)
				{
					StringMap params;
					params["pm_pass"] = password;
					*response = Util::formatParams(passwordHint, params, false);
				}
				if (protectPrivateSay)
					fire(ClientListener::StatusMessage(), this, STRING(REJECTED_PRIVATE_MESSAGE_FROM) + " " + message.replyTo->getIdentity().getNick());
				return false;
			}
			default: // WAITING, CHECKED
				return false;
		}
	}
	else
	{
		if ((pmFlags & FavoriteUser::FLAG_IGNORE_PRIVATE))
			return false;
		return true;
	}
}

bool Client::isChatMessageAllowed(const ChatMessage& message, const string& nick) const
{
	if (suppressChatAndPM)
		return false;
	if (!message.from)
		return nick.empty() || !UserManager::getInstance()->isInIgnoreList(nick);
	if (isMe(message.from))
		return true;
	int options = ClientManager::getChatOptions();
	if (message.thirdPerson && (options & ClientManager::CHAT_OPTION_IGNORE_ME))
		return false;
	if ((options & ClientManager::CHAT_OPTION_SUPPRESS_MAIN_CHAT) && !isOp())
		return false;
	if (UserManager::getInstance()->isInIgnoreList(message.from->getIdentity().getNick()))
		return false;
	return true;
}

void Client::logPM(const ChatMessage& message) const
{
	if ((ClientManager::getChatOptions() & ClientManager::CHAT_OPTION_LOG_PRIVATE_CHAT) &&
	    message.from && message.from->getUser())
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
		if (response.empty() && (ClientManager::getChatOptions() & ClientManager::CHAT_OPTION_LOG_SUPPRESSED))
		{
			string hubName = getHubName();
			const string& hubUrl = getHubUrl();
			if (hubName != hubUrl)
			{
				hubName += " (";
				hubName += hubUrl;
				hubName += ')';
			}
			string nick = message->replyTo->getIdentity().getNick();
			LogManager::message(CSTRING_F(PM_IGNORED, nick % hubName));
			fire(ClientListener::StatusMessage(), this, CSTRING_F(PM_IGNORED2, nick));
		}
	}
}

void Client::fireOutgoingPM(const OnlineUserPtr& user, const string& message, int flags)
{
	const OnlineUserPtr& me = getMyOnlineUser();

	unique_ptr<ChatMessage> chatMessage(new ChatMessage(message, me, user, me, (flags & PM_FLAG_THIRD_PERSON) != 0));
	if (!isPrivateMessageAllowed(*chatMessage, nullptr, (flags & PM_FLAG_AUTOMATIC) != 0))
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

int Client::getSlots() const
{
	return fakeSlots < 0 ? UploadManager::getSlots() : fakeSlots;
}

int Client::getFreeSlots() const
{
	return std::max(getSlots() - UploadManager::getRunningCount(), 0);
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
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	size_t maxCommands = ss->getInt(Conf::MAX_HUB_USER_COMMANDS);
	ss->unlockRead();
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

void Client::getUserCommands(vector<UserCommand>& result, int ctx) const
{
	READ_LOCK(*csUserCommands);
	for (const UserCommand& uc : userCommands)
		if (uc.getCtx() & ctx)
			result.push_back(uc);
}

string Client::getOpChat() const noexcept
{
	LOCK(csOpChat);
	return opChat;
}

void Client::updateConnectionStatus(ConnectionStatus::Status status)
{
	if (favoriteId)
		FavoriteManager::getInstance()->changeConnectionStatus(favoriteId, status);
	else
		FavoriteManager::getInstance()->changeConnectionStatus(getHubUrl(), status);
}

void Client::checkUsers(uint64_t tick)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool performChecks = ss->getBool(getType() == TYPE_NMDC ? Conf::CHECK_USERS_NMDC : Conf::CHECK_USERS_ADC);
	const unsigned maxUsersToCheck = ss->getInt(Conf::USER_CHECK_BATCH);
	ss->unlockRead();

	nextUserCheck = tick + USER_CHECK_INTERVAL;
	bool autoChecks = !exclChecks && performChecks;
	if (!autoChecks && !hasUserCheckList())
		return;

	unsigned numActive = 0;
	UserList usersToCheck;
	{
		LOCK(csCheckUsers);
		auto i = checkUsersList.begin();
		while (i != checkUsersList.end())
		{
			UserPtr& user = *i;
			if (!user->isOnline())
			{
				i = checkUsersList.erase(i);
				continue;
			}
			if (user->getFlags() & User::USER_CHECK_RUNNING)
			{
				++numActive;
				++i;
				continue;
			}
			uint64_t lastCheckTime = user->getLastCheckTime();
			if (lastCheckTime && lastCheckTime + USER_CHECK_VALIDITY > tick)
			{
				i = checkUsersList.erase(i);
				continue;
			}
			if (usersToCheck.size() < maxUsersToCheck)
				usersToCheck.push_back(user);
			++i;
		}
	}
	unsigned numUsers = numActive + (unsigned) usersToCheck.size();
	if (autoChecks && numUsers < maxUsersToCheck)
	{
		// Get more users
		UserList newUsers;
		getUsersToCheck(newUsers, tick, USER_CHECK_VALIDITY);
		unsigned count = maxUsersToCheck - numUsers;
		if (count > newUsers.size()) count = newUsers.size();
		if (count)
		{
			LOCK(csCheckUsers);
			for (unsigned i = 0; i < count; i++)
			{
				unsigned pos = Util::rand(i, newUsers.size());
				if (pos != i) std::swap(newUsers[pos], newUsers[i]);
				usersToCheck.push_back(newUsers[i]);
				checkUsersList.push_back(newUsers[i]);
			}
		}
	}
	if (usersToCheck.empty()) return;
	HintedUser hintedUser;
	hintedUser.hint = getHubUrl();
	auto qm = QueueManager::getInstance();
	for (UserPtr& user : usersToCheck)
	{
		if (numActive >= maxUsersToCheck) break;
		LogManager::message("User check started: " + user->getLastNick(), false);
		hintedUser.user = user;
		qm->userCheckStart(hintedUser);
		++numActive;
	}
}

void Client::updateUserCheckTime() noexcept
{
	nextUserCheck = Util::getTick() + USER_CHECK_INTERVAL;
}

void Client::clearUserCheckList() noexcept
{
	LOCK(csCheckUsers);
	checkUsersList.clear();
}

bool Client::hasUserCheckList() const noexcept
{
	LOCK(csCheckUsers);
	return !checkUsersList.empty();
}
