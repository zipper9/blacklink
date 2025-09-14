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
#include "ChatOptions.h"
#include "SettingsManager.h"
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
#include "FavoriteManager.h"
#include "DatabaseManager.h"
#include "GlobalState.h"
#include "PortTest.h"
#include "BusyCounter.h"
#include "Util.h"
#include "dht/DHT.h"
#include "ConfCore.h"

CID ClientManager::pid;
CID ClientManager::cid;
bool ClientManager::searchSpyEnabled = false;
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
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	string pid = ss->getString(Conf::PRIVATE_ID);
	if (ss->getString(Conf::NICK).empty())
		ss->setString(Conf::NICK, Util::getRandomNick(15));
	ss->unlockWrite();
	setMyPID(pid);
	ChatOptions::updateSettings();
}

ClientManager::~ClientManager()
{
	dcassert(GlobalState::isShutdown());
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
	dcassert(g_UserUpdateQueue.empty());
#endif
}

ClientBasePtr ClientManager::getClient(const string& hubURL)
{
	Util::ParsedUrl url;
	Util::decodeUrl(hubURL, url);
	// save parameters before they are cleared in Util::formatDchubUrl
	string query = std::move(url.query);
	uint16_t port = url.port;
	int protocol = Util::getHubProtocol(url.protocol);
	string formattedUrl = Util::formatDchubUrl(url);
	ClientBasePtr cb;
	if (protocol == Util::HUB_PROTOCOL_ADC)
	{
		cb = AdcHub::create(formattedUrl, url.host, port, false);
	}
	else if (protocol == Util::HUB_PROTOCOL_ADCS)
	{
		cb = AdcHub::create(formattedUrl, url.host, port, true);
	}
	else if (protocol == Util::HUB_PROTOCOL_NMDCS)
	{
		cb = NmdcHub::create(formattedUrl, url.host, port, true);
	}
	else
	{
		cb = NmdcHub::create(formattedUrl, url.host, port, false);
	}

	Client* c = static_cast<Client*>(cb.get());
	if (c->getType() == Client::TYPE_NMDC)
	{
		auto ss = SettingsManager::instance.getCoreSettings();
		ss->lockRead();
		bool detectEncoding = ss->getBool(Conf::NMDC_ENCODING_FROM_DOMAIN);
		ss->unlockRead();
		if (detectEncoding)
		{
			int encoding = NmdcHub::getEncodingFromDomain(url.host);
			if (encoding) c->setEncoding(encoding);
		}
	}
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
#ifdef BL_FEATURE_IP_DATABASE
	flushRatio();
#endif
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
	{
		WRITE_LOCK(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.clear();
	}
#endif
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

	OnlineUserList users;
	{
		READ_LOCK(*g_csOnlineUsers);
		const auto p = g_onlineUsers.equal_range(user->getCID());
		for (auto i = p.first; i != p.second; ++i)
			users.push_back(i->second);
	}
	for (OnlineUserPtr& ou : users)
		if (ip.type == AF_INET)
			ou->getIdentity().setIP4(ip.data.v4);
		else
			ou->getIdentity().setIP6(ip.data.v6);
}

bool ClientManager::getUserParams(const UserPtr& user, OnlineUserParams& params)
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
	if (!GlobalState::isShuttingDown())
		fire(ClientManagerListener::ClientDisconnected(), client);

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

void ClientManager::getOnlineUsers(const CID& cid, OnlineUserList& lst)
{
	READ_LOCK(*g_csOnlineUsers);
	const auto op = g_onlineUsers.equal_range(cid);
	for (auto i = op.first; i != op.second; ++i)
		lst.push_back(i->second);
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

string ClientManager::getNick(const UserPtr& user, const string& hintUrl)
{
	if (user->getFlags() & User::NMDC)
		return user->getLastNick();
	string result;
	if (!hintUrl.empty())
	{
		OnlineUserPtr u;
		{
			READ_LOCK(*g_csOnlineUsers);
			u = findOnlineUserHintL(user->getCID(), hintUrl);
		}
		if (u)
			result = u->getIdentity().getNick();
	}
	if (result.empty())
	{
		result = user->getLastNick();
		if (result.empty())
			result = '{' + user->getCID().toBase32() + '}';
	}
	return result;
}

string ClientManager::getNick(const HintedUser& hintedUser)
{
	return getNick(hintedUser.user, hintedUser.hint);
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

string ClientManager::getOnlineHubName(const string& hubUrl)
{
	READ_LOCK(*g_csClients);
	auto i = g_clients.find(hubUrl);
	if (i == g_clients.end()) return Util::emptyString;
	return i->second->getHubName();
}

bool ClientManager::getHubUserCommands(const string& hubUrl, vector<UserCommand>& cmd, int ctx)
{
	READ_LOCK(*g_csClients);
	auto i = g_clients.find(hubUrl);
	if (i == g_clients.end()) return false;
	const Client* c = static_cast<const Client*>(i->second.get());
	c->getUserCommands(cmd, ctx);
	return true;
}

bool ClientManager::isOnline(const UserPtr& user)
{
	READ_LOCK(*g_csOnlineUsers);
	return g_onlineUsers.find(user->getCID()) != g_onlineUsers.end();
}

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

void ClientManager::findOnlineUsers(const CID& cid, OnlineUserList& res, int clientType)
{
	res.clear();
	READ_LOCK(*g_csOnlineUsers);
	auto op = g_onlineUsers.equal_range(cid);
	for (auto i = op.first; i != op.second; ++i)
		if (!clientType || clientType == i->second->getClientBase()->getType())
			res.push_back(i->second);
}

OnlineUserPtr ClientManager::findDHTNode(const CID& cid)
{
	READ_LOCK(*g_csOnlineUsers);
	auto op = g_onlineUsers.equal_range(cid);
	for (auto i = op.first; i != op.second; ++i)
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

ClientBasePtr ClientManager::findClientForHbriConn(int id)
{
	ClientBasePtr result;
	READ_LOCK(*g_csClients);
	for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
	{
		const Client* c = static_cast<const Client*>(j->second.get());
		if (c->getType() != Client::TYPE_ADC) continue;
		if (static_cast<const AdcHub*>(c)->getHbriConnId() == id)
		{
			result = j->second;
			break;
		}
	}
	return result;
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
	
	g_csUsers->acquireExclusive();
	auto p = g_users.insert(make_pair(cid, std::make_shared<User>(cid, nick)));
	auto& user = p.first->second;
	user->setFlag(User::NMDC);
	g_csUsers->releaseExclusive();
#ifdef BL_FEATURE_IP_DATABASE
	user->addNick(nick, hubUrl);
#endif
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
#ifdef BL_FEATURE_IP_DATABASE
		user->addNick(nick, hubUrl);
#endif
		return user;
	}
	UserPtr user = std::make_shared<User>(cid, nick);
	p.first->second = user;
	g_csUsers->releaseExclusive();
#ifdef BL_FEATURE_IP_DATABASE
	user->addNick(nick, hubUrl);
#endif
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
	if (!GlobalState::isShuttingDown())
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
			fire(ClientManagerListener::UserConnected(), user);
		}
	}
}

void ClientManager::putOffline(const OnlineUserPtr& ou, bool disconnectFlag) noexcept
{
#ifdef BL_FEATURE_IP_DATABASE
	ou->getUser()->saveStats(DatabaseManager::getInstance()->getOptions());
#endif
	if (!GlobalState::isShuttingDown())
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
			fire(ClientManagerListener::UserDisconnected(), u);
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

OnlineUserPtr ClientManager::connect(const HintedUser& user, const string& token, bool forcePassive)
{
	dcassert(!token.empty());
	const bool priv = FavoriteManager::getInstance()->isPrivateHub(user.hint);

	READ_LOCK(*g_csOnlineUsers);
	OnlineUserPtr ou = findOnlineUserL(user, priv);
	if (ou)
	{
		if (forcePassive)
			ou->getClientBase()->resendMyINFO(false, true);
		ou->getClientBase()->connect(ou, token, forcePassive);
	}
	return ou;
}

int ClientManager::privateMessage(const HintedUser& user, const string& msg, int flags)
{
	const bool priv = FavoriteManager::getInstance()->isPrivateHub(user.hint);
	OnlineUserPtr u;
	{
		READ_LOCK(*g_csOnlineUsers);
		u = findOnlineUserL(user, priv);
	}
	if (!u) return PM_NO_USER;
	auto& cb = u->getClientBase();
	if (cb->getType() == ClientBase::TYPE_DHT) return PM_DISABLED;
	if ((flags & ClientBase::PM_FLAG_MAIN_CHAT) && !cb->isMcPmSupported()) return PM_DISABLED;
	if (cb->privateMessage(u, msg, flags)) return PM_OK;
	return PM_ERROR;
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

bool ClientManager::sendAdcCommand(AdcCommand& cmd, const CID& cid, const IpAddress& udpAddr, uint16_t udpPort, const void* sudpKey)
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
			if (cmd.getType() == AdcCommand::TYPE_UDP)
			{
				if (u->getIdentity().isUdpActive())
					sendUDP = u->getIdentity().getUdpAddress(ip, port);
				if (!sendUDP)
				{
					if (!(u->getUser()->getFlags() & User::NMDC))
					{
						cmd.setType(AdcCommand::TYPE_DIRECT);
						cmd.setTo(u->getIdentity().getSID());
						sendToClient = true;
					}
					else if (Util::isValidIp(udpAddr) && udpPort)
					{
						ip = udpAddr;
						port = udpPort;
						sendUDP = true;
					}
				}
			}
			else
			{
				if (u->getUser()->getFlags() & User::NMDC)
					return false;
				cmd.setTo(u->getIdentity().getSID());
				sendToClient = true;
			}
		}
	}
	if (sendToClient)
	{
		const ClientBasePtr& cb = u->getClientBase();
		if (cb->getType() != ClientBase::TYPE_DHT)
		{
			Client* client = static_cast<Client*>(cb.get());
			return client->send(cmd);
		}
		return false;
	}
	if (sendUDP)
	{
		string cmdStr = cmd.toString(getMyCID());
		uint16_t flags = sudpKey ? SearchManager::FLAG_ENC_KEY : 0;
		SearchManager::getInstance()->addToSendQueue(cmdStr, ip, port, flags, sudpKey);
		return true;
	}
	return false;
}

void ClientManager::infoUpdated(Client* client)
{
	if (!client) return;
	READ_LOCK(*g_csClients);
	if (client->isConnected())
		client->info(false);
}

void ClientManager::infoUpdated(bool forceUpdate /* = false*/)
{
	READ_LOCK(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = static_cast<Client*>(i->second.get());
		if (c->isConnected())
			c->info(forceUpdate);
	}
}

void ClientManager::fireIncomingSearch(int protocol, const string& seeker, const string& hub, const string& filter, ClientManagerListener::SearchReply reply)
{
	if (searchSpyEnabled)
		Speaker<ClientManagerListener>::fire(ClientManagerListener::IncomingSearch(), protocol, seeker, hub, filter, reply);
}

static void getShareGroup(const OnlineUserPtr& ou, bool& hideShare, CID& shareGroup)
{
	hideShare = false;
	auto fm = FavoriteManager::getInstance();
	FavoriteUser::MaskType flags;
	int uploadLimit;
	if (fm->getFavUserParam(ou->getUser(), flags, uploadLimit, shareGroup))
	{
		if (flags & FavoriteUser::FLAG_HIDE_SHARE)
		{
			hideShare = true;
			return;
		}
		if (!shareGroup.isZero()) return;
	}
	const ClientBasePtr& cb = ou->getClientBase();
	if (cb->getType() == ClientBase::TYPE_DHT)
	{
		shareGroup.init();
		return;
	}
	Client* client = static_cast<Client*>(cb.get());
	hideShare = client->getHideShare();
	shareGroup = client->getShareGroup();
}

void ClientManager::on(AdcSearch, const Client* c, const AdcCommand& adc, const OnlineUserPtr& ou) noexcept
{
	bool isUdpActive = ou->getIdentity().isUdpActive();
	const IpAddress hubIp = c->getIp();
	int hubPort = c->getPort();
	bool hideShare;
	CID shareGroup;
	getShareGroup(ou, hideShare, shareGroup);
	AdcSearchParam param(adc.getParameters(), isUdpActive ? SearchParamBase::MAX_RESULTS_ACTIVE : SearchParamBase::MAX_RESULTS_PASSIVE, shareGroup);
	ClientManagerListener::SearchReply re;
	auto sm = SearchManager::getInstance();
	int options = sm->getOptions();
	if (hideShare ||
	    (!param.hasRoot && (options & SearchManager::OPT_INCOMING_SEARCH_TTH_ONLY)) ||
	    (!isUdpActive && (options & SearchManager::OPT_INCOMING_SEARCH_IGNORE_PASSIVE)))
		re = ClientManagerListener::SEARCH_MISS;
	else
		re = sm->respond(param, ou, c->getHubUrl(), hubIp, hubPort);
	if (searchSpyEnabled)
	{
		string description = param.getDescription();
		Speaker<ClientManagerListener>::fire(ClientManagerListener::IncomingSearch(), ClientBase::TYPE_ADC, "Hub:" + ou->getIdentity().getNick(), c->getHubUrl(), description, re);
	}
}

void ClientManager::search(const SearchParam& sp)
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

unsigned ClientManager::multiSearch(const SearchParam& sp, vector<SearchClientItem>& clients)
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

void ClientManager::getOnlineClients(StringSet& onlineClients) noexcept
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

void ClientManager::getClientStatus(boost::unordered_map<string, ConnectionStatus::Status>& result) noexcept
{
	READ_LOCK(*g_csClients);
	result.clear();
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = static_cast<Client*>(i->second.get());
		ConnectionStatus::Status status;
		switch (c->getState())
		{
			case Client::STATE_CONNECTING:
			case Client::STATE_PROTOCOL:
			case Client::STATE_IDENTIFY:
			case Client::STATE_VERIFY:
			case Client::STATE_WAIT_PORT_TEST:
				status = ConnectionStatus::CONNECTING;
				break;
			case Client::STATE_NORMAL:
				status = ConnectionStatus::SUCCESS;
				break;
			default:
				status = ConnectionStatus::FAILURE;
		}
		auto p = result.insert(make_pair(c->getHubUrl(), status));
		if (!p.second && status == ConnectionStatus::SUCCESS)
			p.first->second = status;
	}
}

void ClientManager::addAsyncOnlineUserUpdated(const OnlineUserPtr& ou)
{
	if (!GlobalState::isShuttingDown())
	{
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
		WRITE_LOCK(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.push_back(ou);
#else
		fire(ClientManagerListener::UserUpdated(), ou);
#endif
	}
}

#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
void ClientManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (!GlobalState::isShuttingDown())
	{
		READ_LOCK(*g_csOnlineUsersUpdateQueue);
		for (auto i = g_UserUpdateQueue.cbegin(); i != g_UserUpdateQueue.cend(); ++i)
		{
			fire(ClientManagerListener::UserUpdated(), *i);
		}
	}
	WRITE_LOCK(*g_csOnlineUsersUpdateQueue);
	g_UserUpdateQueue.clear();
}
#endif

#ifdef BL_FEATURE_IP_DATABASE
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
				if (i->second->shouldSaveStats())
					usersToFlush.push_back(i->second);
			}
		}
		if (!usersToFlush.empty())
		{
			int options = DatabaseManager::getInstance()->getOptions();
			for (auto& user : usersToFlush)
				user->saveStats(options);
		}
	}
}
#endif

void ClientManager::usersCleanup()
{
	WRITE_LOCK(*g_csUsers);
	auto i = g_users.begin();
	while (i != g_users.end())
	{
		if (i->second.unique())
			g_users.erase(i++);
		else
			++i;
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

string ClientManager::findMyNick(const string& hubUrl)
{
	READ_LOCK(*g_csClients);
	const auto& i = g_clients.find(hubUrl);
	if (i != g_clients.end())
		return i->second->getMyNick();
	return Util::emptyString;
}

bool ClientManager::isActiveMode(int af, int favHubMode, bool udp)
{
	int whatPort;
	if (udp)
	{
		if (SearchManager::getLocalPort() == 0) return false;
		whatPort = AppPorts::PORT_UDP;
	}
	else
	{
		if (ConnectionManager::getInstance()->getPort() == 0) return false;
		whatPort = AppPorts::PORT_TCP;
	}
	switch (favHubMode)
	{
		case 1: return true;
		case 2: return false;
	}

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const int type = ss->getInt(af == AF_INET6 ? Conf::INCOMING_CONNECTIONS6 : Conf::INCOMING_CONNECTIONS);
	const int ougoingConnections = ss->getInt(Conf::OUTGOING_CONNECTIONS);
	ss->unlockRead();

	if (ougoingConnections != Conf::OUTGOING_DIRECT)
		return false;

	int portTestState = PortTest::STATE_UNKNOWN;
	if (af == AF_INET)
	{
		int unused;
		portTestState = g_portTest.getState(whatPort, unused, nullptr);
		if (portTestState == PortTest::STATE_FAILURE)
			return false;
	}
	if (type == Conf::INCOMING_FIREWALL_UPNP &&
	    portTestState != PortTest::STATE_SUCCESS &&
		ConnectivityManager::getInstance()->getMapper(af).getState(whatPort) == MappingManager::STATE_FAILURE)
		return false;
	return type != Conf::INCOMING_FIREWALL_PASSIVE;
}

void ClientManager::cancelSearch(uint64_t owner)
{
	READ_LOCK(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = static_cast<Client*>(i->second.get());
		c->cancelSearch(owner);
	}
}

void ClientManager::on(Connecting, const Client* c) noexcept
{
	fire(ClientManagerListener::ClientConnecting(), c);
}

void ClientManager::on(Connected, const Client* c) noexcept
{
	fire(ClientManagerListener::ClientConnected(), c);
}

void ClientManager::on(UserUpdated, const OnlineUserPtr& ou) noexcept
{
	addAsyncOnlineUserUpdated(ou);
}

void ClientManager::on(UserListUpdated, const ClientBase* client, const OnlineUserList& l) noexcept
{
	for (auto i = l.cbegin(); i != l.cend(); ++i)
	{
		updateNick(*i); // TODO проверить что меняется именно ник - иначе не звать. или разбить UsersUpdated на UsersUpdated + UsersUpdatedNick
#ifdef _DEBUG
		//      LogManager::message("ClientManager::on(UsersUpdated nick = " + (*i)->getUser()->getLastNick());
#endif
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
	fire(ClientManagerListener::ClientUpdated(), c);
}

void ClientManager::on(ClientFailed, const Client* client, const string&) noexcept
{
	fire(ClientManagerListener::ClientDisconnected(), client);
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

void ClientManager::setSupports(const UserPtr& user, uint8_t knownUcSupports)
{
	OnlineUserPtr ou;
	{
		READ_LOCK(*g_csOnlineUsers);
		auto i = g_onlineUsers.find(user->getCID());
		if (i != g_onlineUsers.end()) ou = i->second;
	}
	if (ou) ou->getIdentity().setKnownUcSupports(knownUcSupports);
}

void ClientManager::setUnknownCommand(const UserPtr& user, const string& unknownCommand)
{
	OnlineUserPtr ou;
	{
		READ_LOCK(*g_csOnlineUsers);
		auto i = g_onlineUsers.find(user->getCID());
		if (i != g_onlineUsers.end()) ou = i->second;
	}
	if (ou) ou->getIdentity().setStringParam("UC", unknownCommand);
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

string ClientManager::getDefaultNick()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string nick = ss->getString(Conf::NICK);
	ss->unlockRead();
	return nick;
}

bool ClientManager::isNickEmpty()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool result = ss->getString(Conf::NICK).empty();
	ss->unlockRead();
	return result;
}
