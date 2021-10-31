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
#include "ClientManager.h"
#include "ConnectionManager.h"
#include "Client.h"
#include "ShareManager.h"
#include "SearchManager.h"
#include "CryptoManager.h"
#include "SimpleXML.h"
#include "SearchResult.h"
#include "AdcHub.h"
#include "NmdcHub.h"
#include "QueueManager.h"
#include "ConnectivityManager.h"
#include "PortTest.h"
#include "BusyCounter.h"
#include "dht/DHT.h"

CID ClientManager::pid;
CID ClientManager::cid;
volatile bool g_isShutdown = false;
volatile bool g_isBeforeShutdown = false;
bool g_isStartupProcess = true;
bool ClientManager::g_isSpyFrame = false;
ClientManager::ClientMap ClientManager::g_clients;
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
OnlineUserList ClientManager::g_UserUpdateQueue;
std::unique_ptr<RWLock> ClientManager::g_csOnlineUsersUpdateQueue = std::unique_ptr<RWLock>(RWLock::create());
#endif

std::unique_ptr<RWLock> ClientManager::g_csClients = std::unique_ptr<RWLock>(RWLock::create());
std::unique_ptr<RWLock> ClientManager::g_csOnlineUsers = std::unique_ptr<RWLock> (RWLock::create());
std::unique_ptr<RWLock> ClientManager::g_csUsers = std::unique_ptr<RWLock>(RWLock::create());

ClientManager::OnlineMap ClientManager::g_onlineUsers;
ClientManager::UserMap ClientManager::g_users;

ClientManager::ClientManager()
{
	if (SETTING(NICK).empty())
		SET_SETTING(NICK, Util::getRandomNick(15));
	setMyPID(SETTING(PRIVATE_ID));
}

ClientManager::~ClientManager()
{
	dcassert(isShutdown());
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
	dcassert(g_UserUpdateQueue.empty());
#endif
}

ClientBasePtr ClientManager::getClient(const string& hubURL)
{
	dcassert(hubURL == Text::toLower(hubURL));
	string scheme, address, file, query, fragment;
	uint16_t port;
	Util::decodeUrl(hubURL, scheme, address, port, file, query, fragment);
	int protocol = Util::getHubProtocol(scheme);
	ClientBasePtr cb;
	if (protocol == Util::HUB_PROTOCOL_ADC)
	{
		cb = AdcHub::create(hubURL, address, port, false);
	}
	else if (protocol == Util::HUB_PROTOCOL_ADCS)
	{
		cb = AdcHub::create(hubURL, address, port, true);
	}
	else if (protocol == Util::HUB_PROTOCOL_NMDCS)
	{
		cb = NmdcHub::create(hubURL, address, port, true);
	}
	else
	{
		cb = NmdcHub::create(hubURL, address, port, false);
	}
	
	Client* c = static_cast<Client*>(cb.get());
	if (!query.empty())
	{
		string keyprint = Util::getQueryParam(query, "kp");
#ifdef _DEBUG
		LogManager::message("keyprint = " + keyprint);
#endif
		c->setKeyPrint(keyprint);
	}

	{
		WRITE_LOCK(*g_csClients);
		g_clients.insert(make_pair(c->getHubUrl(), cb));
	}
	
	c->addListener(this);
	c->initDefaultUsers();
	return cb;
}

void ClientManager::shutdown()
{
	dcassert(!isShutdown());
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	flushRatio();
#endif
	::g_isShutdown = true;
	::g_isBeforeShutdown = true; // Для надежности
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
	{
		WRITE_LOCK(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.clear();
	}
#endif
}

void ClientManager::before_shutdown()
{
	dcassert(!isBeforeShutdown());
	::g_isBeforeShutdown = true;
	TimerManager::getInstance()->setTicksDisabled(true);
}

void ClientManager::clear()
{
	{
		WRITE_LOCK(*g_csOnlineUsers);
		g_onlineUsers.clear();
	}
	{
		WRITE_LOCK(*g_csUsers);
		//LOCK(g_csUsers);
		g_users.clear();
	}
}

size_t ClientManager::getTotalUsers()
{
	size_t users = 0;
	READ_LOCK(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
		users += static_cast<const Client*>(i->second.get())->getUserCount();
	return users;
}

void ClientManager::setUserIP(const UserPtr& user, const IpAddress& ip)
{
	if (ip.type != AF_INET && ip.type != AF_INET6)
	{
		dcassert(0);
		return;
	}

	WRITE_LOCK(*g_csOnlineUsers);
	const auto p = g_onlineUsers.equal_range(user->getCID());
	for (auto i = p.first; i != p.second; ++i)
		if (ip.type == AF_INET)
			i->second->getIdentity().setIP4(ip.data.v4);
		else
			i->second->getIdentity().setIP6(ip.data.v6);
}

bool ClientManager::getUserParams(const UserPtr& user, UserParams& params)
{
	READ_LOCK(*g_csOnlineUsers);
	const OnlineUserPtr u = getOnlineUserL(user);
	if (u)
	{
		const auto& i = u->getIdentity();
		params.bytesShared = i.getBytesShared();
		params.slots = i.getSlots();
		params.limit = i.getLimit();
		params.ip4 = i.getIP4();
		params.ip6 = i.getIP6();
		params.tag = i.getTag();
		params.nick = i.getNick();
		return true;
	}
	return false;
}

void ClientManager::getConnectedHubs(vector<ClientBasePtr>& out)
{
	READ_LOCK(*g_csClients);
	out.reserve(g_clients.size());
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		const Client* c = static_cast<const Client*>(i->second.get());
		if (c->isConnected())
			out.push_back(i->second);
	}
}

void ClientManager::prepareClose()
{
	{
		WRITE_LOCK(*g_csClients);
		g_clients.clear();
	}
}

void ClientManager::putClient(const ClientBasePtr& cb)
{
	dcassert(cb.get());
	dcassert(cb->getType() != ClientBase::TYPE_DHT);
	Client* client = static_cast<Client*>(cb.get());
	client->removeListeners();
	{
		WRITE_LOCK(*g_csClients);
		g_clients.erase(client->getHubUrl());
	}
	if (!isBeforeShutdown()) // При закрытии не шлем уведомление (на него подписан только фрейм поиска)
	{
		fly_fire1(ClientManagerListener::ClientDisconnected(), client);
	}
	client->shutdown();
	client->clearDefaultUsers();
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl)
{
	bool isPrivate = FavoriteManager::getInstance()->isPrivateHub(hintUrl);
	return getHubs(cid, hintUrl, isPrivate);
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl)
{
	bool isPrivate = FavoriteManager::getInstance()->isPrivateHub(hintUrl);
	return getHubNames(cid, hintUrl, isPrivate);
}

StringList ClientManager::getNicks(const CID& cid, const string& hintUrl)
{
	bool isPrivate = FavoriteManager::getInstance()->isPrivateHub(hintUrl);
	return getNicks(cid, hintUrl, isPrivate);
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl, bool priv)
{
	StringList lst;
	if (!priv)
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase()->getHubUrl());
		}
	}
	else
	{
		READ_LOCK(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl);
		if (u)
		{
			lst.push_back(u->getClientBase()->getHubUrl());
		}
	}
	return lst;
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
	StringList lst;
	if (!priv)
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase()->getHubName());
		}
	}
	else
	{
		READ_LOCK(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl);
		if (u)
		{
			lst.push_back(u->getClientBase()->getHubName());
		}
	}
	return lst;
}

StringList ClientManager::getNicks(const CID& cid, const string& hintUrl, bool priv, bool noBase32)
{
	StringSet ret;
	if (!priv)
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
			ret.insert(i->second->getIdentity().getNick());
	}
	else
	{
		READ_LOCK(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl);
		if (u)
			ret.insert(u->getIdentity().getNick());
	}
	if (ret.empty() && !noBase32)
		ret.insert('{' + cid.toBase32() + '}');
	if (ret.empty())
		return StringList();
	else
		return StringList(ret.begin(), ret.end());
}

StringList ClientManager::getNicks(const HintedUser& user)
{
	dcassert(user.user);
	if (user.user)
		return getNicks(user.user->getCID(), user.hint);
	else
		return StringList();
}

StringList ClientManager::getHubNames(const HintedUser& user)
{
	dcassert(user.user);
	if (user.user)
		return getHubNames(user.user->getCID(), user.hint);
	else
		return StringList();
}

bool ClientManager::isConnected(const string& hubUrl)
{
	READ_LOCK(*g_csClients);
	return g_clients.find(hubUrl) != g_clients.end();
}

string ClientManager::getHubName(const string& hubUrl)
{
	READ_LOCK(*g_csClients);
	auto i = g_clients.find(hubUrl);
	if (i == g_clients.end()) return Util::emptyString;
	return i->second->getHubName();
}

bool ClientManager::getHubUserCommands(const string& hubUrl, vector<UserCommand>& cmd)
{
	READ_LOCK(*g_csClients);
	auto i = g_clients.find(hubUrl);
	if (i == g_clients.end()) return false;
	const Client* c = static_cast<const Client*>(i->second.get());
	c->getUserCommands(cmd);
	return true;
}

#if 0
bool ClientManager::isOnline(const UserPtr& user)
{
	READ_LOCK(*g_csOnlineUsers);
	return g_onlineUsers.find(user->getCID()) != g_onlineUsers.end();
}
#endif

OnlineUserPtr ClientManager::findOnlineUserL(const HintedUser& user, bool priv)
{
	if (user.user)
		return findOnlineUserL(user.user->getCID(), user.hint, priv);
	else
		return OnlineUserPtr();
}

UserPtr ClientManager::findUser(const string& nick, const string& hubUrl)
{
	return findUser(makeCid(nick, hubUrl));
}

OnlineUserPtr ClientManager::findOnlineUserL(const CID& cid, const string& hintUrl, bool priv)
{
	OnlinePairC p;
	OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl, p);
	if (u) // found an exact match (CID + hint).
		return u;
		
	if (p.first == p.second) // no user found with the given CID.
		return nullptr;
		
	// if the hint hub is private, don't allow connecting to the same user from another hub.
	if (priv)
		return nullptr;
		
	// ok, hub not private, return a random user that matches the given CID but not the hint.
	return p.first->second;
}

OnlineUserPtr ClientManager::findOnlineUser(const CID& cid, const string& hintUrl, bool priv)
{
	READ_LOCK(*g_csOnlineUsers);
	return findOnlineUserL(cid, hintUrl, priv);
}

OnlineUserPtr ClientManager::findDHTNode(const CID& cid)
{
	READ_LOCK(*g_csOnlineUsers);	
	OnlinePairC op = g_onlineUsers.equal_range(cid);
	for (OnlineIterC i = op.first; i != op.second; ++i)
	{
		const OnlineUserPtr& ou = i->second;

		// user not in DHT, so don't bother with other hubs
		if (!(ou->getUser()->getFlags() & User::DHT))
			break;

		if (ou->getClientBase()->getType() == ClientBase::TYPE_DHT)
			return ou;
	}
	return OnlineUserPtr();
}

string ClientManager::getStringField(const CID& cid, const string& hint, const char* field)
{
	READ_LOCK(*g_csOnlineUsers);
	
	OnlinePairC p;
	const auto u = findOnlineUserHintL(cid, hint, p);
	if (u)
	{
		auto value = u->getIdentity().getStringParam(field);
		if (!value.empty())
		{
			return value;
		}
	}
	
	for (auto i = p.first; i != p.second; ++i)
	{
		auto value = i->second->getIdentity().getStringParam(field);
		if (!value.empty())
		{
			return value;
		}
	}
	return Util::emptyString;
}

bool ClientManager::getSlots(const CID& cid, uint16_t& slots)
{
	READ_LOCK(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		slots = i->second->getIdentity().getSlots();
		return true;
	}
	return false;
}

string ClientManager::findHub(const string& ipPort, int type)
{
	if (ipPort.empty())
		return Util::emptyString;
		
	string ipOrHost;
	uint16_t port = 411;
	Util::parseIpPort(ipPort, ipOrHost, port);
	string url, fallbackUrl;
	IpAddress ip;
	bool parseResult = Util::parseIpAddress(ip, ipOrHost);
	READ_LOCK(*g_csClients);
	for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
	{
		const Client* c = static_cast<const Client*>(j->second.get());
		if (type && c->getType() != type) continue;
		if (!parseResult) // hostname
		{
			if (c->getAddress() == ipOrHost)
			{
				if (c->getPort() == port)
				{
					url = c->getHubUrl();
					break;
				}
				fallbackUrl = c->getHubUrl();
			}
		}
		else if (c->getIp() == ip)
		{
			if (c->getPort() == port)
			{
				url = c->getHubUrl();
				break;
			}
			fallbackUrl = c->getHubUrl();
		}
	}
	return url.empty() ? fallbackUrl : url;
}

int ClientManager::findHubEncoding(const string& url)
{
	if (!url.empty())
	{
		READ_LOCK(*g_csClients);
		const auto& i = g_clients.find(url);
		if (i != g_clients.end())
		{
			const Client* c = static_cast<const Client*>(i->second.get());
			return c->getEncoding();
		}
	}
	return Util::isAdcHub(url) ? Text::CHARSET_UTF8 : Text::CHARSET_SYSTEM_DEFAULT;
}

UserPtr ClientManager::findLegacyUser(const string& nick, const string& hubUrl, string* foundHubUrl)
{
	dcassert(!nick.empty());
	if (!nick.empty())
	{
		READ_LOCK(*g_csClients);
		if (!hubUrl.empty())
		{
			const auto& i = g_clients.find(hubUrl);
			if (i != g_clients.end())
			{
				const Client* c = static_cast<const Client*>(i->second.get());
				const auto& ou = c->findUser(nick);
				if (ou)
					return ou->getUser();
			}
			return UserPtr();
		}
		for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
		{
			const Client* c = static_cast<const Client*>(j->second.get());
			const auto& ou = c->findUser(nick);
			if (ou)
			{
				if (foundHubUrl) *foundHubUrl = j->first;
				return ou->getUser();
			}
		}
	}
	return UserPtr();
}

UserPtr ClientManager::getUser(const string& nick, const string& hubUrl)
{
	dcassert(!nick.empty());
	const CID cid = makeCid(nick, hubUrl);
	
	WRITE_LOCK(*g_csUsers);
	auto p = g_users.insert(make_pair(cid, std::make_shared<User>(cid, nick)));
	auto& user = p.first->second;
	user->setFlag(User::NMDC);
	user->addNick(nick, hubUrl);
	return user;
}

UserPtr ClientManager::createUser(const CID& cid, const string& nick, const string& hubUrl)
{
	g_csUsers->acquireExclusive();
	auto p = g_users.insert(make_pair(cid, UserPtr()));
	if (!p.second)
	{
		UserPtr user = p.first->second;
		g_csUsers->releaseExclusive();
		if (!nick.empty()) user->updateNick(nick);
		user->addNick(nick, hubUrl);
		return user;
	}
	UserPtr user = std::make_shared<User>(cid, nick);
	p.first->second = user;
	g_csUsers->releaseExclusive();
	user->addNick(nick, hubUrl);
	return user;
}

UserPtr ClientManager::findUser(const CID& cid)
{
	READ_LOCK(*g_csUsers);
	//LOCK(g_csUsers);
	const auto& ui = g_users.find(cid);
	if (ui != g_users.end())
	{
		return ui->second;
	}
	return UserPtr();
}

bool ClientManager::isOp(const string& hubUrl)
{
	READ_LOCK(*g_csClients);
	auto i = g_clients.find(hubUrl);
	if (i == g_clients.cend()) return false;
	const Client* c = static_cast<const Client*>(i->second.get());
	return c->isConnected() && c->isOp();
}

CID ClientManager::makeCid(const string& nick, const string& hubUrl)
{
	TigerHash th;
	th.update(nick.c_str(), nick.length());
	th.update(hubUrl.c_str(), hubUrl.length());
	// Construct hybrid CID from the bits of the tiger hash - should be
	// fairly random, and hopefully low-collision
	return CID(th.finalize());
}

void ClientManager::putOnline(const OnlineUserPtr& ou, bool fireFlag) noexcept
{
	if (!isBeforeShutdown())
	{
		const auto& user = ou->getUser();
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!user->getCID().isZero());
		{
			WRITE_LOCK(*g_csOnlineUsers);
			const auto l_res = g_onlineUsers.insert(make_pair(user->getCID(), ou));
			dcassert(l_res->second);
		}
		
		if (!(user->setFlagEx(User::ONLINE) & User::ONLINE) && fireFlag)
		{
			fly_fire1(ClientManagerListener::UserConnected(), user);	
		}
	}
}

void ClientManager::putOffline(const OnlineUserPtr& ou, bool disconnectFlag) noexcept
{
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	bool ipStat = BOOLSETTING(ENABLE_RATIO_USER_LIST);
	bool userStat = BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER);
	ou->getUser()->saveStats(ipStat, userStat);
#endif
	if (!isBeforeShutdown())
	{
		// [!] IRainman fix: don't put any hub to online or offline! Any hubs as user is always offline!
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!ou->getUser()->getCID().isZero());
		// [~] IRainman fix.
		OnlineIter::difference_type diff = 0;
		{
			WRITE_LOCK(*g_csOnlineUsers);
			auto op = g_onlineUsers.equal_range(ou->getUser()->getCID()); // Ищется по одном - научиться убивать сразу массив.
			// [-] dcassert(op.first != op.second); [!] L: this is normal and means that the user is offline.
			for (auto i = op.first; i != op.second; ++i)
			{
				if (ou == i->second)
				{
					diff = distance(op.first, op.second);
					g_onlineUsers.erase(i);
					break;
				}
			}
		}
		
		if (diff == 1) //last user
		{
			UserPtr& u = ou->getUser();
			u->unsetFlag(User::ONLINE);
			if (disconnectFlag)
				ConnectionManager::getInstance()->disconnect(u);
			fly_fire1(ClientManagerListener::UserDisconnected(), u);
		}
		else if (diff > 1)
		{
			addAsyncOnlineUserUpdated(ou);
		}
	}
}

void ClientManager::removeOnlineUser(const OnlineUserPtr& ou) noexcept
{
	WRITE_LOCK(*g_csOnlineUsers);
	auto op = g_onlineUsers.equal_range(ou->getUser()->getCID());
	for (auto i = op.first; i != op.second; ++i)
	{
		if (ou == i->second)
		{
			g_onlineUsers.erase(i);
			break;
		}
	}
}

OnlineUserPtr ClientManager::findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p)
{
	p = g_onlineUsers.equal_range(cid);
	
	if (p.first == p.second) // no user found with the given CID.
		return nullptr;
		
	if (!hintUrl.empty())
	{
		for (auto i = p.first; i != p.second; ++i)
		{
			const OnlineUserPtr u = i->second;
			if (u->getClientBase()->getHubUrl() == hintUrl)
			{
				return u;
			}
		}
	}
	return nullptr;
}

void ClientManager::resendMyInfo()
{
	READ_LOCK(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = static_cast<Client*>(i->second.get());
		if (c->isConnected())
			c->resendMyINFO(true, false);
	}
}

void ClientManager::connect(const HintedUser& user, const string& token, bool forcePassive)
{
	dcassert(!token.empty());
	dcassert(!isBeforeShutdown());
	if (!isBeforeShutdown())
	{
		const bool priv = FavoriteManager::getInstance()->isPrivateHub(user.hint);
		
		READ_LOCK(*g_csOnlineUsers);
		OnlineUserPtr u = findOnlineUserL(user, priv);
		
		if (u)
		{
			if (forcePassive)
			{
				u->getClientBase()->resendMyINFO(false, true);
			}
			u->getClientBase()->connect(u, token, forcePassive);
		}
	}
}

void ClientManager::privateMessage(const HintedUser& user, const string& msg, bool thirdPerson)
{
	const bool priv = FavoriteManager::getInstance()->isPrivateHub(user.hint);
	OnlineUserPtr u;
	{
		// # u->getClientBase().privateMessage Нельзя выполнять под локом - там внутри есть fire
		// Есть дампы от Mikhail Korbakov где вешаемся в дедлоке.
		// http://www.flickr.com/photos/96019675@N02/11424193335/
		READ_LOCK(*g_csOnlineUsers);
		u = findOnlineUserL(user, priv);
	}
	if (u)
	{
		u->getClientBase()->privateMessage(u, msg, thirdPerson);
	}
}
void ClientManager::userCommand(const HintedUser& hintedUser, const UserCommand& uc, StringMap& params, bool compatibility)
{
	OnlineUserPtr ou;
	{
		READ_LOCK(*g_csOnlineUsers);
		/** @todo we allow wrong hints for now ("false" param of findOnlineUser) because users
		 * extracted from search results don't always have a correct hint; see
		 * SearchManager::onRES(const AdcCommand& cmd, ...). when that is done, and SearchResults are
		 * switched to storing only reliable HintedUsers (found with the token of the ADC command),
		 * change this call to findOnlineUserHint. */
		ou = findOnlineUserL(hintedUser.user->getCID(), hintedUser.hint.empty() ? uc.getHub() : hintedUser.hint, false);
		if (!ou) return;
		getUserCommandParams(ou, uc, params, compatibility);
	}
	ClientBasePtr& cb = ou->getClientBase();
	if (cb->getType() != ClientBase::TYPE_DHT)
	{
		Client* client = static_cast<Client*>(cb.get());
		client->sendUserCmd(uc, params);
	}
}

void ClientManager::getUserCommandParams(const OnlineUserPtr& ou, const UserCommand& uc, StringMap& params, bool compatibility)
{
	const ClientBasePtr& cb = ou->getClientBase();
	const Client* client = nullptr;
	if (cb->getType() != ClientBase::TYPE_DHT)
	{
		client = static_cast<const Client*>(cb.get());
		const string& opChat = client->getOpChat();
		if (opChat.find('*') == string::npos && opChat.find('?') == string::npos)
			params["opchat"] = opChat;
	}
		
	ou->getIdentity().getParams(params, "user", compatibility);
	if (client)
	{
		client->getHubIdentity().getParams(params, "hub", false);
		client->getMyIdentity().getParams(params, "my", compatibility);
		if (uc.isRaw()) client->escapeParams(params);
	}
}

void ClientManager::sendAdcCommand(AdcCommand& cmd, const CID& cid)
{
	IpAddress ip;
	uint16_t port = 0;
	bool sendToClient = false;
	bool sendUDP = false;
	OnlineUserPtr u;
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(cid);
		if (i != g_onlineUsers.end())
		{
			u = i->second;
			if (cmd.getType() == AdcCommand::TYPE_UDP && !u->getIdentity().isUdpActive())
			{
				if (u->getUser()->getFlags() & User::NMDC)
					return;
					
				cmd.setType(AdcCommand::TYPE_DIRECT);
				cmd.setTo(u->getIdentity().getSID());
				sendToClient = true;
			}
			else
				sendUDP = u->getIdentity().getUdpAddress(ip, port);
		}
	}
	if (sendToClient)
	{
		const ClientBasePtr& cb = u->getClientBase();
		if (cb->getType() != ClientBase::TYPE_DHT)
		{
			Client* client = static_cast<Client*>(cb.get());
			client->send(cmd);
		}
		return;
	}
	if (sendUDP)
	{
		string cmdStr = cmd.toString(getMyCID());
		SearchManager::getInstance()->addToSendQueue(cmdStr, ip, port);
	}
}

void ClientManager::infoUpdated(Client* client)
{
	if (!client) return;
	if (!ClientManager::isBeforeShutdown())
	{
		READ_LOCK(*g_csClients);
		if (client->isConnected())
			client->info(false);
	}
}

void ClientManager::infoUpdated(bool forceUpdate /* = false*/)
{
	if (ClientManager::isBeforeShutdown())
		return;
	READ_LOCK(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = static_cast<Client*>(i->second.get());
		if (!ClientManager::isBeforeShutdown())
		{
			if (c->isConnected())
				c->info(forceUpdate);
		}
	}
}

void ClientManager::fireIncomingSearch(int protocol, const string& seeker, const string& hub, const string& filter, ClientManagerListener::SearchReply reply)
{
	if (g_isSpyFrame)
		Speaker<ClientManagerListener>::fly_fire5(ClientManagerListener::IncomingSearch(), protocol, seeker, hub, filter, reply);
}

static void getShareGroup(const OnlineUserPtr& ou, CID& shareGroup)
{
	auto fm = FavoriteManager::getInstance();
	FavoriteUser::MaskType flags;
	int uploadLimit;
	if (fm->getFavUserParam(ou->getUser(), flags, uploadLimit, shareGroup) && !shareGroup.isZero())
		return;
	const ClientBasePtr& cb = ou->getClientBase();
	if (cb->getType() == ClientBase::TYPE_DHT)
	{
		shareGroup.init();
		return;
	}
	Client* client = static_cast<Client*>(cb.get());
	shareGroup = client->getShareGroup();
}

void ClientManager::on(AdcSearch, const Client* c, const AdcCommand& adc, const OnlineUserPtr& ou) noexcept
{
	bool isUdpActive = ou->getIdentity().isUdpActive();
	const IpAddress hubIp = c->getIp();
	int hubPort = c->getPort();
	CID shareGroup;
	getShareGroup(ou, shareGroup);
	AdcSearchParam param(adc.getParameters(), isUdpActive ? SearchParamBase::MAX_RESULTS_ACTIVE : SearchParamBase::MAX_RESULTS_PASSIVE, shareGroup);
	ClientManagerListener::SearchReply re;
	if (!param.hasRoot && BOOLSETTING(INCOMING_SEARCH_TTH_ONLY))
		re = ClientManagerListener::SEARCH_MISS;
	else
		re = SearchManager::getInstance()->respond(param, ou, c->getHubUrl(), hubIp, hubPort);
	if (g_isSpyFrame)
	{
		string description = param.getDescription();
		Speaker<ClientManagerListener>::fly_fire5(ClientManagerListener::IncomingSearch(), ClientBase::TYPE_ADC, "Hub:" + ou->getIdentity().getNick(), c->getHubUrl(), description, re);
	}
}

void ClientManager::search(const SearchParamToken& sp)
{
	{
		READ_LOCK(*g_csClients);
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
		{
			Client* c = static_cast<Client*>(i->second.get());
			if (c->isConnected())
				c->searchInternal(sp);
		}
	}
	if (sp.fileType == FILE_TYPE_TTH)
		dht::DHT::getInstance()->findFile(sp.filter, sp.token, sp.owner);
}

unsigned ClientManager::multiSearch(const SearchParamToken& sp, vector<SearchClientItem>& clients)
{
	unsigned maxWaitTime = 0;
	bool useDHT = false;
	SearchClientItem* dhtItem = nullptr;
	if (clients.empty())
	{
		useDHT = true;
		READ_LOCK(*g_csClients);
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
		{
			Client* c = static_cast<Client*>(i->second.get());
			if (c->isConnected())
			{
				unsigned waitTime = c->searchInternal(sp);
				if (waitTime > maxWaitTime) maxWaitTime = waitTime;
			}
		}
	}
	else
	{
		READ_LOCK(*g_csClients);
		for (SearchClientItem& client : clients)
		{
			if (client.url == dht::NetworkName)
			{
				useDHT = true;
				dhtItem = &client;
				continue;
			}
			const auto& i = g_clients.find(client.url);
			if (i != g_clients.end())
			{
				Client* c = static_cast<Client*>(i->second.get());
				if (c->isConnected())
				{
					client.waitTime = c->searchInternal(sp);
					if (client.waitTime > maxWaitTime) maxWaitTime = client.waitTime;
				}
			}
		}
	}
	if (useDHT && sp.fileType == FILE_TYPE_TTH)
	{
		dht::DHT* d = dht::DHT::getInstance();
		if (d->isConnected())
		{
			unsigned waitTime = d->findFile(sp.filter, sp.token, sp.owner);
			if (waitTime > maxWaitTime) maxWaitTime = waitTime;
			if (dhtItem) dhtItem->waitTime = waitTime;
		}
	}
	return maxWaitTime;
}

void ClientManager::getOnlineClients(StringSet& onlineClients)
{
	READ_LOCK(*g_csClients);
	onlineClients.clear();
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = static_cast<Client*>(i->second.get());
		if (c->isConnected())
			onlineClients.insert(c->getHubUrl());
	}
}

void ClientManager::addAsyncOnlineUserUpdated(const OnlineUserPtr& ou)
{
	if (!isBeforeShutdown())
	{
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
		WRITE_LOCK(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.push_back(ou);
#else
		fly_fire1(ClientManagerListener::UserUpdated(), ou);
#endif
	}
}

#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
void ClientManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (!isBeforeShutdown())
	{
		READ_LOCK(*g_csOnlineUsersUpdateQueue);
		for (auto i = g_UserUpdateQueue.cbegin(); i != g_UserUpdateQueue.cend(); ++i)
		{
			fly_fire1(ClientManagerListener::UserUpdated(), *i);
		}
	}
	WRITE_LOCK(*g_csOnlineUsersUpdateQueue);
	g_UserUpdateQueue.clear();
}
#endif

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
void ClientManager::flushRatio()
{
	static bool isBusy = false;
	if (!isBusy)
	{
		BusyCounter<bool> busy(isBusy);
		std::vector<UserPtr> usersToFlush;
		{
			READ_LOCK(*g_csUsers);
			for (auto i = g_users.cbegin(); i != g_users.cend(); ++i)
			{
				if (i->second->statsChanged())
					usersToFlush.push_back(i->second);
			}
		}
		if (!usersToFlush.empty())
		{
			bool ipStat = BOOLSETTING(ENABLE_RATIO_USER_LIST);
			bool userStat = BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER);
			for (auto& user : usersToFlush)
				user->saveStats(ipStat, userStat);
		}
	}
}
#endif

void ClientManager::usersCleanup()
{
	WRITE_LOCK(*g_csUsers);
	auto i = g_users.begin();
	while (i != g_users.end() && !isBeforeShutdown())
	{
		if (i->second.unique())
		{
#ifdef _DEBUG
			//LogManager::message("g_users.erase(i++); - Nick = " + i->second->getLastNick());
#endif
			g_users.erase(i++);
		}
		else
		{
			++i;
		}
	}
}

void ClientManager::setMyPID(const string& pid)
{
	ClientManager::pid = CID(pid);
	TigerHash tiger;
	tiger.update(ClientManager::pid.data(), CID::SIZE);
	ClientManager::cid = CID(tiger.finalize());
}

const CID& ClientManager::getMyCID()
{
	return cid;
}

const CID& ClientManager::getMyPID()
{
	dcassert(!pid.isZero());
	return pid;
}

const string ClientManager::findMyNick(const string& hubUrl)
{
	READ_LOCK(*g_csClients);
	const auto& i = g_clients.find(hubUrl);
	if (i != g_clients.end())
		return i->second->getMyNick();
	return Util::emptyString;
}

int ClientManager::getConnectivityMode(int af, int favHubMode)
{
	switch (favHubMode)
	{
		case 1: return SettingsManager::INCOMING_DIRECT;
		case 2: return SettingsManager::INCOMING_FIREWALL_PASSIVE;
	}
	int portTestState = PortTest::STATE_UNKNOWN;
	int type;
	if (af == AF_INET6)
	{
		type = SETTING(INCOMING_CONNECTIONS6);
	}
	else
	{
		type = SETTING(INCOMING_CONNECTIONS);
		int unused;
		portTestState = g_portTest.getState(PortTest::PORT_TCP, unused, nullptr);
		if (portTestState == PortTest::STATE_FAILURE)
			return SettingsManager::INCOMING_FIREWALL_PASSIVE;
	}
	if (type == SettingsManager::INCOMING_FIREWALL_UPNP &&
	    portTestState != PortTest::STATE_SUCCESS &&
		ConnectivityManager::getInstance()->getMapper(af).getState(MappingManager::PORT_TCP) == MappingManager::STATE_FAILURE)
		return SettingsManager::INCOMING_FIREWALL_PASSIVE;
	return type;
}

bool ClientManager::isActive(int af, int favHubMode)
{
	return getConnectivityMode(af, favHubMode) != SettingsManager::INCOMING_FIREWALL_PASSIVE;
}

void ClientManager::cancelSearch(void* owner)
{
	READ_LOCK(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = static_cast<Client*>(i->second.get());
		c->cancelSearch(owner);
	}
}

void ClientManager::on(Connected, const Client* c) noexcept
{
	fly_fire1(ClientManagerListener::ClientConnected(), c);
}

void ClientManager::on(UserUpdated, const OnlineUserPtr& ou) noexcept
{
	addAsyncOnlineUserUpdated(ou);
}

void ClientManager::on(UserListUpdated, const ClientBase* client, const OnlineUserList& l) noexcept
{
	dcassert(!isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		for (auto i = l.cbegin(); i != l.cend(); ++i)
		{
			updateNick(*i); // TODO проверить что меняется именно ник - иначе не звать. или разбить UsersUpdated на UsersUpdated + UsersUpdatedNick
#ifdef _DEBUG
			//      LogManager::message("ClientManager::on(UsersUpdated nick = " + (*i)->getUser()->getLastNick());
#endif
		}
	}
}

void ClientManager::updateNick(const OnlineUserPtr& ou)
{
	if (ou->getUser()->getLastNick().empty())
	{
		string nickFromIdentity = ou->getIdentity().getNick();
		ou->getUser()->setLastNick(nickFromIdentity);
		dcassert(ou->getUser()->getLastNick() != nickFromIdentity);
	}
}

void ClientManager::on(HubUpdated, const Client* c) noexcept
{
	dcassert(!isBeforeShutdown());
	fly_fire1(ClientManagerListener::ClientUpdated(), c);
}

void ClientManager::on(ClientFailed, const Client* client, const string&) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		fly_fire1(ClientManagerListener::ClientDisconnected(), client);
	}
}

OnlineUserPtr ClientManager::getOnlineUserL(const UserPtr& p)
{
	if (p == nullptr)
		return nullptr;
		
	const auto i = g_onlineUsers.find(p->getCID());
	if (i == g_onlineUsers.end())
		return OnlineUserPtr();
		
	return i->second;
}

void ClientManager::sendRawCommand(const OnlineUserPtr& ou, int commandIndex)
{
	const ClientBasePtr& cb = ou->getClientBase();
	if (cb->getType() == ClientBase::TYPE_DHT)
		return;
	Client* client = static_cast<Client*>(cb.get());
	const string rawCommand = client->getRawCommand(commandIndex);
	if (!rawCommand.empty())
	{
		StringMap params;
		const UserCommand uc = UserCommand(0, 0, 0, 0, "", rawCommand, "", "");
		getUserCommandParams(ou, uc, params, true);
		client->sendUserCmd(uc, params);
	}
}

#if 0
void ClientManager::setListLength(const UserPtr& p, const string& listLen)
{
	if (!p) return;
	WRITE_LOCK(*g_csOnlineUsers); // TODO Write
	const auto i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		i->second->getIdentity().setStringParam("LL", listLen);
	}
}
#endif

void ClientManager::cheatMessage(Client* client, const string& report)
{
	if (client && !report.empty() && BOOLSETTING(DISPLAY_CHEATS_IN_MAIN_CHAT))
		client->cheatMessage(report);
}

#ifdef IRAINMAN_INCLUDE_USER_CHECK
void ClientManager::fileListDisconnected(const UserPtr& p)
{
	string report;
	Client* c = nullptr;
	OnlineUserPtr sendCmd;
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end())
		{
			OnlineUserPtr ou = i->second;
			auto& id = ou->getIdentity();
			
			auto fileListDisconnects = id.incFileListDisconnects(); // 8 бит не мало?
			
			int maxDisconnects = SETTING(AUTOBAN_MAX_DISCONNECTS);
			if (maxDisconnects == 0)
				return;
				
			if (fileListDisconnects == maxDisconnects)
			{
				c = &ou->getClient();
				report = id.setCheat(ou->getClientBase(), "Disconnected file list " + Util::toString(fileListDisconnects) + " times", false);
				sendCmd = ou;
			}
		}
	}
	if (sendCmd) sendRawCommand(sendCmd, SETTING(AUTOBAN_CMD_DISCONNECTS));
	cheatMessage(c, report);
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void ClientManager::connectionTimeout(const UserPtr& p)
{
	string report;
	Client* c = nullptr;
	OnlineUserPtr sendCmd;
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end())
		{
			OnlineUserPtr ou = i->second;
			auto& id = ou->getIdentity();
			
			auto connectionTimeouts = id.incConnectionTimeouts(); // 8 бит не мало?
			
			int maxTimeouts = SETTING(AUTOBAN_MAX_TIMEOUTS);
			if (maxTimeouts == 0)
				return;
				
			if (connectionTimeouts == maxTimeouts)
			{
				const ClientBasePtr& cb = ou->getClientBase();
				if (cb->getType() != ClientBase::TYPE_DHT)
				{
					c = static_cast<Client*>(cb.get());
#ifdef FLYLINKDC_USE_DETECT_CHEATING
					report = id.setCheat(ou->getClientBase(), "Connection timeout " + Util::toString(connectionTimeouts) + " times", false);
#else
					report = "Connection timeout " + Util::toString(connectionTimeouts) + " times";
#endif
					sendCmd = ou;
				}
			}
		}
	}
	if (sendCmd) sendRawCommand(sendCmd, SETTING(AUTOBAN_CMD_TIMEOUTS));
	cheatMessage(c, report);
}

#ifdef FLYLINKDC_USE_DETECT_CHEATING
void ClientManager::checkCheating(const UserPtr& p, DirectoryListing* dl)
{
	Client* client;
	string report;
	OnlineUserPtr ou;
	OnlineUserPtr sendCmd;
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
			return;
			
		ou = i->second;
		auto& id = ou->getIdentity();
		
		const int64_t statedSize = id.getBytesShared();
		const int64_t realSize = dl->getRoot()->getTotalSize();
		
		const double multiplier = (100 + SETTING(AUTOBAN_FAKE_SHARE_PERCENT)) / 100.0;
		const int64_t sizeTolerated = (int64_t)(realSize * multiplier);
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		id.setRealBytesShared(realSize);
#endif
		
		if (statedSize > sizeTolerated)
		{
			id.setFakeCardBit(Identity::BAD_LIST | Identity::CHECKED, true);
			string detectString = STRING(CHECK_MISMATCHED_SHARE_SIZE) + " - ";
			if (realSize == 0)
			{
				detectString += STRING(CHECK_0BYTE_SHARE);
			}
			else
			{
				const double qwe = double(statedSize) / double(realSize);
				char buf[128];
				buf[0] = 0;
				_snprintf(buf, _countof(buf), CSTRING(CHECK_INFLATED), Util::toString(qwe).c_str()); //-V111
				detectString += buf;
			}
			detectString += STRING(CHECK_SHOW_REAL_SHARE);
			
			report = id.setCheat(ou->getClientBase(), detectString, false);
			sendCmd = ou;
		}
		else
		{
			id.setFakeCardBit(Identity::CHECKED, true);
		}
		id.updateClientType(*ou);
		
		client = &(ou->getClient());
	}
	if (sendCmd) sendRawCommand(sendCmd, SETTING(AUTOBAN_CMD_FAKESHARE));
	//client->updatedMyINFO(ou); // тут тоже не нужна нотификация всем подписчикам
	cheatMessage(client, report);
}
#endif // FLYLINKDC_USE_DETECT_CHEATING

#ifdef IRAINMAN_INCLUDE_USER_CHECK
void ClientManager::setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient)
{
	Client* client;
	OnlineUserPtr ou;
	OnlineUserPtr sendCmd;
	string report;
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
			return;
			
		ou = i->second;
		ou->getIdentity().updateClientType(*ou);
		if (!aCheatString.empty())
			report += ou->getIdentity().setCheat(ou->getClientBase(), aCheatString, aBadClient);
		if (aRawCommand != -1)
			sendCmd = ou;

		client = &(ou->getClient());
	}
	if (sendCmd) sendRawCommand(sendCmd, aRawCommand);
	//client->updatedMyINFO(ou); // Не шлем обновку подписчикам!
	cheatMessage(client, report);
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void ClientManager::setSupports(const UserPtr& p, const StringList & aSupports, const uint8_t knownUcSupports)
{
	WRITE_LOCK(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		auto& id = i->second->getIdentity();
		id.setKnownUcSupports(knownUcSupports);
		{
			AdcSupports::setSupports(id, aSupports);
		}
	}
}
void ClientManager::setUnknownCommand(const UserPtr& p, const string& aUnknownCommand)
{
	WRITE_LOCK(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		i->second->getIdentity().setStringParam("UC", aUnknownCommand);
	}
}

void ClientManager::dumpUserInfo(const HintedUser& user)
{
	string report;
	ClientBasePtr cb;
	if (user.user)
	{
		READ_LOCK(*g_csOnlineUsers);
		OnlineUserPtr ou = findOnlineUserL(user.user->getCID(), user.hint, true);
		if (!ou)
			return;
			
		ou->getIdentity().getReport(report);
		cb = ou->getClientBase();
	}
	if (cb)
		cb->dumpUserInfo(report);
}

StringList ClientManager::getNicksByIp(const IpAddress& ip)
{
	std::unordered_set<string> nicks;
	{
		READ_LOCK(*g_csOnlineUsers);
		for (auto i = g_onlineUsers.cbegin(); i != g_onlineUsers.cend(); ++i)
		{
			const auto& user = i->second->getUser();
			if (!user) continue;
			string nick;
			if (ip.type == AF_INET)
			{
				if (user->getIP4() == ip.data.v4)
					nick = user->getLastNick();
			}
			else if (ip.type == AF_INET6)
			{
				if (user->getIP6() == ip.data.v6)
					nick = user->getLastNick();
			}
			if (!nick.empty())
				nicks.insert(nick);
		}
	}
	StringList result;
	result.reserve(nicks.size());
	for (const auto& nick : nicks)
		result.push_back(nick);
	return result;
}

void ClientManager::updateUser(const OnlineUserPtr& ou)
{
	addAsyncOnlineUserUpdated(ou);
}
