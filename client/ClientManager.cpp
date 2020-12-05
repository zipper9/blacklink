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
#include "dht/DHT.h"

CID ClientManager::pid;
CID ClientManager::cid;
volatile bool g_isShutdown = false;
volatile bool g_isBeforeShutdown = false;
bool g_isStartupProcess = true;
bool ClientManager::g_isSpyFrame = false;
ClientManager::ClientList ClientManager::g_clients;
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

Client* ClientManager::getClient(const string& hubURL)
{
	dcassert(hubURL == Text::toLower(hubURL));
	string scheme, address, file, query, fragment;
	uint16_t port;
	Util::decodeUrl(hubURL, scheme, address, port, file, query, fragment);
	int protocol = Util::getHubProtocol(scheme);
	Client* c;
	if (protocol == Util::HUB_PROTOCOL_ADC)
	{
		c = new AdcHub(hubURL, address, port, false);
	}
	else if (protocol == Util::HUB_PROTOCOL_ADCS)
	{
		c = new AdcHub(hubURL, address, port, true);
	}
	else if (protocol == Util::HUB_PROTOCOL_NMDCS)
	{
		c = new NmdcHub(hubURL, address, port, true);
	}
	else
	{
		c = new NmdcHub(hubURL, address, port, false);
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
		CFlyWriteLock(*g_csClients);
		g_clients.insert(make_pair(c->getHubUrl(), c));
	}
	
	c->addListener(this);
	
	return c;
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
		CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
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
		CFlyWriteLock(*g_csOnlineUsers);
		g_onlineUsers.clear();
	}
	{
		CFlyWriteLock(*g_csUsers);
		//CFlyLock(g_csUsers);
		g_users.clear();
	}
}

size_t ClientManager::getTotalUsers()
{
	size_t users = 0;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
		users += i->second->getUserCount();
	return users;
}

void ClientManager::setUserIP(const UserPtr& user, const string& ip)
{
	if (ip.empty())
		return;

	CFlyWriteLock(*g_csOnlineUsers);
	const auto p = g_onlineUsers.equal_range(user->getCID());
	for (auto i = p.first; i != p.second; ++i)
		i->second->getIdentity().setIp(ip);
}

bool ClientManager::getUserParams(const UserPtr& user, UserParams& params)
{
	CFlyReadLock(*g_csOnlineUsers);
	const OnlineUserPtr u = getOnlineUserL(user);
	if (u)
	{
		const auto& i = u->getIdentity();
		params.bytesShared = i.getBytesShared();
		params.slots = i.getSlots();
		params.limit = i.getLimit();
		params.ip = i.getIpAsString();
		params.tag = i.getTag();
		params.nick = i.getNick();
		return true;
	}
	return false;
}

void ClientManager::getConnectedHubUrls(StringList& out)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		const Client* c = i->second;
		if (c->isConnected())
			out.push_back(c->getHubUrl());
	}
}

void ClientManager::getConnectedHubInfo(HubInfoArray& out)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		const Client* c = i->second;
		if (c->isConnected())
			out.push_back(HubInfo{c->getHubUrl(), c->getHubName(), c->getMyIdentity().isOp()});
	}
}

void ClientManager::prepareClose()
{
	// http://www.flickr.com/photos/96019675@N02/11475592005/
	/*
	{
	    CFlyReadLock(*g_csClients);
	    for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	    {
	        i->second->removeListeners();
	    }
	}
	*/
	{
		CFlyWriteLock(*g_csClients);
		g_clients.clear();
	}
}

void ClientManager::putClient(Client* client)
{
	client->removeListeners();
	{
		CFlyWriteLock(*g_csClients);
		g_clients.erase(client->getHubUrl());
	}
	if (!isBeforeShutdown()) // При закрытии не шлем уведомление (на него подписан только фрейм поиска)
	{
		fly_fire1(ClientManagerListener::ClientDisconnected(), client);
	}
	client->shutdown();
	delete client;
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
		CFlyReadLock(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubUrl());
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl);
		if (u)
		{
			lst.push_back(u->getClientBase().getHubUrl());
		}
	}
	return lst;
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
	StringList lst;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubName());
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl);
		if (u)
		{
			lst.push_back(u->getClientBase().getHubName());
		}
	}
	return lst;
}

StringList ClientManager::getNicks(const CID& cid, const string& hintUrl, bool priv, bool noBase32)
{
	StringSet ret;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
			ret.insert(i->second->getIdentity().getNick());
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers);
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
	CFlyReadLock(*g_csClients);
	return g_clients.find(hubUrl) != g_clients.end();
}

bool ClientManager::isOnline(const UserPtr& user)
{
	CFlyReadLock(*g_csOnlineUsers);
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
	CFlyReadLock(*g_csOnlineUsers);
	return findOnlineUserL(cid, hintUrl, priv);
}

OnlineUserPtr ClientManager::findDHTNode(const CID& cid)
{
	CFlyReadLock(*g_csOnlineUsers);	
	OnlinePairC op = g_onlineUsers.equal_range(cid);
	for (OnlineIterC i = op.first; i != op.second; ++i)
	{
		const OnlineUserPtr& ou = i->second;

		// user not in DHT, so don't bother with other hubs
		if (!(ou->getUser()->getFlags() & User::DHT))
			break;

		if (ou->getClientBase().getType() == ClientBase::TYPE_DHT)
			return ou;
	}
	return OnlineUserPtr();
}

string ClientManager::getStringField(const CID& cid, const string& hint, const char* field)
{
	CFlyReadLock(*g_csOnlineUsers);
	
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
	CFlyReadLock(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		slots = i->second->getIdentity().getSlots();
		return true;
	}
	return false;
}

string ClientManager::findHub(const string& ipPort)
{
	if (ipPort.empty())
		return Util::emptyString;
		
	string ipOrHost;
	uint16_t port = 411;
	Util::parseIpPort(ipPort, ipOrHost, port);
	string url;
	boost::system::error_code ec;
	const auto ip = boost::asio::ip::address_v4::from_string(ipOrHost, ec);
	CFlyReadLock(*g_csClients);
	for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
	{
		const Client* c = j->second;
		if (c->getPort() == port)
		{
			// If exact match is found, return it
			if (ec)
			{
				if (c->getAddress() == ipOrHost)
				{
					url = c->getHubUrl();
					break;
				}
			}
			else if (c->getIp() == ip)
			{
				url = c->getHubUrl();
				break;
			}
		}
	}
	return url;
}

int ClientManager::findHubEncoding(const string& url)
{
	if (!url.empty())
	{
		CFlyReadLock(*g_csClients);
		const auto& i = g_clients.find(url);
		if (i != g_clients.end())
			return i->second->getEncoding();
	}
	return Util::isAdcHub(url) ? Text::CHARSET_UTF8 : Text::CHARSET_SYSTEM_DEFAULT;
}

UserPtr ClientManager::findLegacyUser(const string& nick, const string& hubUrl)
{
	dcassert(!nick.empty());
	if (!nick.empty())
	{
		CFlyReadLock(*g_csClients);
		if (!hubUrl.empty())
		{
			const auto& i = g_clients.find(hubUrl);
			if (i != g_clients.end())
			{
				const auto& ou = i->second->findUser(nick);
				if (ou)
					return ou->getUser();
			}
			return UserPtr();
		}
		for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
		{
			const auto& ou = j->second->findUser(nick);
			if (ou)
				return ou->getUser();
		}
	}
	return UserPtr();
}

UserPtr ClientManager::getUser(const string& nick, const string& hubUrl)
{
	dcassert(!nick.empty());
	const CID cid = makeCid(nick, hubUrl);
	
	CFlyWriteLock(*g_csUsers);
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
	CFlyReadLock(*g_csUsers);
	//CFlyLock(g_csUsers);
	const auto& ui = g_users.find(cid);
	if (ui != g_users.end())
	{
		return ui->second;
	}
	return UserPtr();
}

bool ClientManager::isOp(const string& hubUrl)
{
	CFlyReadLock(*g_csClients);
	auto i = g_clients.find(hubUrl);
	if (i == g_clients.cend()) return false;
	const Client* c = i->second;
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
			CFlyWriteLock(*g_csOnlineUsers);
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
			CFlyWriteLock(*g_csOnlineUsers);
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
				ConnectionManager::disconnect(u);
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
	CFlyWriteLock(*g_csOnlineUsers);
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
			if (u->getClientBase().getHubUrl() == hintUrl)
			{
				return u;
			}
		}
	}
	return nullptr;
}

void ClientManager::resend_ext_json()
{
	NmdcHub::inc_version_fly_info();
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
		{
			i->second->resendMyINFO(true, false);
		}
	}
}

void ClientManager::connect(const HintedUser& user, const string& token, bool forcePassive, bool& activeClient)
{
	activeClient = false;
	dcassert(!isBeforeShutdown());
	if (!isBeforeShutdown())
	{
		const bool priv = FavoriteManager::getInstance()->isPrivateHub(user.hint);
		
		CFlyReadLock(*g_csOnlineUsers);
		OnlineUserPtr u = findOnlineUserL(user, priv);
		
		if (u)
		{
			if (forcePassive)
			{
				(&u->getClientBase())->resendMyINFO(false, true);
			}
			u->getClientBase().connect(u, token, forcePassive);
			activeClient = u->getClientBase().isActive();
#if 0
			if (activeClient && forcePassive)
			{
				// (&u->getClientBase())->resendMyINFO(false,false); // Вернем активный режим
				// Не делаем это - флуд получается
			}
#endif
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
		CFlyReadLock(*g_csOnlineUsers);
		u = findOnlineUserL(user, priv);
	}
	if (u)
	{
		u->getClientBase().privateMessage(u, msg, thirdPerson);
	}
}
void ClientManager::userCommand(const HintedUser& hintedUser, const UserCommand& uc, StringMap& params, bool compatibility)
{
	CFlyReadLock(*g_csOnlineUsers);
	userCommandL(hintedUser, uc, params, compatibility);
}

void ClientManager::userCommandL(const HintedUser& hintedUser, const UserCommand& uc, StringMap& params, bool compatibility)
{
	/** @todo we allow wrong hints for now ("false" param of findOnlineUser) because users
	 * extracted from search results don't always have a correct hint; see
	 * SearchManager::onRES(const AdcCommand& cmd, ...). when that is done, and SearchResults are
	 * switched to storing only reliable HintedUsers (found with the token of the ADC command),
	 * change this call to findOnlineUserHint. */
	if (hintedUser.user)
	{
		OnlineUserPtr ou = findOnlineUserL(hintedUser.user->getCID(), hintedUser.hint.empty() ? uc.getHub() : hintedUser.hint, false);
		if (!ou)
			return;
			
		auto& client = ou->getClient();
		const string& opChat = client.getOpChat();
		if (opChat.find('*') == string::npos && opChat.find('?') == string::npos)
		{
			params["opchat"] = opChat;
		}
		
		ou->getIdentity().getParams(params, "user", compatibility);
		client.getHubIdentity().getParams(params, "hub", false);
		client.getMyIdentity().getParams(params, "my", compatibility);
		client.escapeParams(params);
		client.sendUserCmd(uc, params); // TODO - сеть зовем под Lock-ом
	}
}

void ClientManager::sendAdcCommand(AdcCommand& cmd, const CID& cid)
{
	boost::asio::ip::address_v4 ip;
	uint16_t port = 0;
	bool sendToClient = false;
	OnlineUserPtr u;
	{
		CFlyReadLock(*g_csOnlineUsers);
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
			{
				ip = u->getIdentity().getIp();
				port = u->getIdentity().getUdpPort();
			}
		}
	}
	if (sendToClient)
	{
		u->getClient().send(cmd);
		return;
	}
	if (port && !ip.is_unspecified())
	{
		string cmdStr = cmd.toString(getMyCID());
		SearchManager::getInstance()->addToSendQueue(cmdStr, ip, port);
	}
}

void ClientManager::infoUpdated(Client* client)
{
	if (!client) return;
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		CFlyReadLock(*g_csClients);
		if (client->isConnected())
			client->info(false);
	}
}

void ClientManager::infoUpdated(bool forceUpdate /* = false*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
		return;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (!ClientManager::isBeforeShutdown())
		{
			if (c->isConnected())
				c->info(forceUpdate);
		}
	}
}

void ClientManager::fireIncomingSearch(const string& seeker, const string& filter, ClientManagerListener::SearchReply reply)
{
	if (g_isSpyFrame)
		Speaker<ClientManagerListener>::fly_fire3(ClientManagerListener::IncomingSearch(), seeker, filter, reply);
}

void ClientManager::on(AdcSearch, const Client* c, const AdcCommand& adc, const CID& from) noexcept
{
	bool isUdpActive = false;
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range((from));
		for (auto i = op.first; i != op.second; ++i)
		{
			const OnlineUserPtr& u = i->second;
			if (&u->getClient() == c)
			{
				isUdpActive = u->getIdentity().isUdpActive();
				break;
			}
		}
	}

	const string seeker = c->getIpPort();
	AdcSearchParam param(adc.getParameters(), isUdpActive ? SearchParamBase::MAX_RESULTS_ACTIVE : SearchParamBase::MAX_RESULTS_PASSIVE);
	ClientManagerListener::SearchReply re;
	if (!param.hasRoot && BOOLSETTING(INCOMING_SEARCH_TTH_ONLY))
		re = ClientManagerListener::SEARCH_MISS;
	else
		re = SearchManager::getInstance()->respond(param, from, seeker);
	if (g_isSpyFrame)
		for (auto i = param.include.cbegin(); i != param.include.cend(); ++i)
			Speaker<ClientManagerListener>::fly_fire3(ClientManagerListener::IncomingSearch(), seeker, i->getPattern(), re);
}

void ClientManager::search(const SearchParamToken& sp)
{
	{
		CFlyReadLock(*g_csClients);
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
		{
			Client* c = i->second;
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
		CFlyReadLock(*g_csClients);
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
			if (i->second->isConnected())
			{
				unsigned waitTime = i->second->searchInternal(sp);
				if (waitTime > maxWaitTime) maxWaitTime = waitTime;
			}
	}
	else
	{
		CFlyReadLock(*g_csClients);
		for (SearchClientItem& client : clients)
		{
			if (client.url == dht::NetworkName)
			{
				useDHT = true;
				dhtItem = &client;
				continue;
			}
			const auto& i = g_clients.find(client.url);
			if (i != g_clients.end() && i->second->isConnected())
			{
				client.waitTime = i->second->searchInternal(sp);
				if (client.waitTime > maxWaitTime) maxWaitTime = client.waitTime;
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
	CFlyReadLock(*g_csClients);
	onlineClients.clear();
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
			onlineClients.insert(i->second->getHubUrl());
	}
}

void ClientManager::addAsyncOnlineUserUpdated(const OnlineUserPtr& ou)
{
	if (!isBeforeShutdown())
	{
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
		CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
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
		CFlyReadLock(*g_csOnlineUsersUpdateQueue);
		for (auto i = g_UserUpdateQueue.cbegin(); i != g_UserUpdateQueue.cend(); ++i)
		{
			fly_fire1(ClientManagerListener::UserUpdated(), *i);
		}
	}
	CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
	g_UserUpdateQueue.clear();
}
#endif

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
void ClientManager::flushRatio()
{
	static bool g_isBusy = false;
	if (!g_isBusy)
	{
		CFlyBusyBool busy(g_isBusy);
		std::vector<UserPtr> usersToFlush;
		{
			CFlyReadLock(*g_csUsers);
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
	//CFlyLog l_log("[ClientManager::usersCleanup]");
	CFlyWriteLock(*g_csUsers);
	//CFlyLock(g_csUsers);
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
	CFlyReadLock(*g_csClients);
	const auto& i = g_clients.find(hubUrl);
	if (i != g_clients.end())
		return i->second->getMyNick();
	return Util::emptyString;
}

int ClientManager::getMode(int favHubMode)
{
	switch (favHubMode)
	{
		case 1: return SettingsManager::INCOMING_DIRECT;
		case 2: return SettingsManager::INCOMING_FIREWALL_PASSIVE;
	}
	int unused;
	if (g_portTest.getState(PortTest::PORT_TCP, unused, nullptr) == PortTest::STATE_FAILURE)
		return SettingsManager::INCOMING_FIREWALL_PASSIVE;
	int type = SETTING(INCOMING_CONNECTIONS);
	if (type == SettingsManager::INCOMING_FIREWALL_UPNP &&
	    ConnectivityManager::getInstance()->getMapperV4().getState(MappingManager::PORT_TCP) == MappingManager::STATE_FAILURE)
		return SettingsManager::INCOMING_FIREWALL_PASSIVE;
	return type;
}

bool ClientManager::isActive(int favHubMode)
{
	return getMode(favHubMode) != SettingsManager::INCOMING_FIREWALL_PASSIVE;
}

void ClientManager::cancelSearch(void* aOwner)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		i->second->cancelSearch(aOwner);
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

void ClientManager::on(UserListUpdated, const Client* client, const OnlineUserList& l) noexcept
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

void ClientManager::on(HubUserCommand, const Client* client, int type, int ctx, const string& name, const string& command) noexcept
{
	if (BOOLSETTING(HUB_USER_COMMANDS))
	{
		auto fm = FavoriteManager::getInstance();
		if (type == UserCommand::TYPE_REMOVE)
		{
			// FIXME: add FavoriteManager::removeUserCommand
			const int cmd = fm->findUserCommand(name, client->getHubUrl());
			if (cmd != -1)
				fm->removeUserCommandCID(cmd);
		}
		else if (type == UserCommand::TYPE_CLEAR)
		{
			fm->removeHubUserCommands(ctx, client->getHubUrl());
		}
		else
		{			
			int flags = UserCommand::FLAG_NOSAVE;
			if (client->getProtocol() == Socket::PROTO_ADC) flags |= UserCommand::FLAG_FROM_ADC_HUB;
			fm->addUserCommand(type, ctx, flags, name, command, Util::emptyString, client->getHubUrl());
		}
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

void ClientManager::sendRawCommandL(const OnlineUser& ou, const int aRawCommand)
{
	const string rawCommand = ou.getClient().getRawCommand(aRawCommand);
	if (!rawCommand.empty())
	{
		StringMap ucParams;
		
		const UserCommand uc = UserCommand(0, 0, 0, 0, "", rawCommand, "", "");
		userCommandL(HintedUser(ou.getUser(), ou.getClient().getHubUrl()), uc, ucParams, true);
	}
}

void ClientManager::setListLength(const UserPtr& p, const string& listLen)
{
	CFlyWriteLock(*g_csOnlineUsers); // TODO Write
	const auto i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		i->second->getIdentity().setStringParam("LL", listLen);
	}
}

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
	{
		CFlyReadLock(*g_csOnlineUsers);
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
				sendRawCommandL(*ou, SETTING(AUTOBAN_CMD_DISCONNECTS));
			}
		}
	}
	cheatMessage(c, report);
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void ClientManager::connectionTimeout(const UserPtr& p)
{
	string report;
	bool remove = false;
	Client* c = nullptr;
	{
		CFlyReadLock(*g_csOnlineUsers);
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
				c = &ou->getClient();
#ifdef FLYLINKDC_USE_DETECT_CHEATING
				report = id.setCheat(ou.getClientBase(), "Connection timeout " + Util::toString(connectionTimeouts) + " times", false);
#else
				report = "Connection timeout " + Util::toString(connectionTimeouts) + " times";
#endif
				remove = true;
				sendRawCommandL(*ou, SETTING(AUTOBAN_CMD_TIMEOUTS));
			}
		}
	}
	cheatMessage(c, report);
}

#ifdef FLYLINKDC_USE_DETECT_CHEATING
void ClientManager::checkCheating(const UserPtr& p, DirectoryListing* dl)
{
	Client* client;
	string report;
	OnlineUserPtr ou;
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
			return;
			
		ou = i->second;
		auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getIdentity()' expression repeatedly. cheatmanager.h 127
		
		const int64_t l_statedSize = id.getBytesShared();
		const int64_t l_realSize = dl->getTotalSize();
		
		const double l_multiplier = (100 + double(SETTING(PERCENT_FAKE_SHARE_TOLERATED))) / 100;
		const int64_t l_sizeTolerated = (int64_t)(l_realSize * l_multiplier);
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		id.setRealBytesShared(realSize);
#endif
		
		if (l_statedSize > l_sizeTolerated)
		{
			id.setFakeCardBit(Identity::BAD_LIST | Identity::CHECKED, true);
			string detectString = STRING(CHECK_MISMATCHED_SHARE_SIZE) + " - ";
			if (l_realSize == 0)
			{
				detectString += STRING(CHECK_0BYTE_SHARE);
			}
			else
			{
				const double qwe = double(l_statedSize) / double(l_realSize);
				char buf[128];
				buf[0] = 0;
				_snprintf(buf, _countof(buf), CSTRING(CHECK_INFLATED), Util::toString(qwe).c_str()); //-V111
				detectString += buf;
			}
			detectString += STRING(CHECK_SHOW_REAL_SHARE);
			
			report = id.setCheat(ou->getClientBase(), detectString, false);
			sendRawCommandL(*ou, SETTING(AUTOBAN_CMD_FAKESHARE));
		}
		else
		{
			id.setFakeCardBit(Identity::CHECKED, true);
		}
		id.updateClientType(*ou);
		
		client = &(ou->getClient());
	}
	//client->updatedMyINFO(ou); // тут тоже не нужна нотификация всем подписчикам
	cheatMessage(client, report);
}
#endif // FLYLINKDC_USE_DETECT_CHEATING

#ifdef IRAINMAN_INCLUDE_USER_CHECK
void ClientManager::setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient)
{
	Client* client;
	OnlineUserPtr ou;
	string report;
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
			return;
			
		ou = i->second;
		ou->getIdentity().updateClientType(*ou);
		if (!aCheatString.empty())
		{
			report += ou->getIdentity().setCheat(ou->getClientBase(), aCheatString, aBadClient);
		}
		if (aRawCommand != -1)
		{
			sendRawCommandL(*ou, aRawCommand);
		}
		
		client = &(ou->getClient());
	}
	//client->updatedMyINFO(ou); // Не шлем обновку подписчикам!
	cheatMessage(client, report);
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void ClientManager::setSupports(const UserPtr& p, const StringList & aSupports, const uint8_t knownUcSupports)
{
	CFlyWriteLock(*g_csOnlineUsers);
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
	CFlyWriteLock(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		i->second->getIdentity().setStringParam("UC", aUnknownCommand);
	}
}

void ClientManager::dumpUserInfo(const HintedUser& user)
{
	string report;
	Client* client = nullptr;
	if (user.user)
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineUserPtr ou = findOnlineUserL(user.user->getCID(), user.hint, true);
		if (!ou)
			return;
			
		ou->getIdentity().getReport(report);
		client = &(ou->getClient());
	}
	if (client)
		client->dumpUserInfo(report);
}

StringList ClientManager::getNicksByIp(boost::asio::ip::address_v4 ip)
{
	std::unordered_set<string> nicks;
	{
		CFlyReadLock(*g_csOnlineUsers);
		for (auto i = g_onlineUsers.cbegin(); i != g_onlineUsers.cend(); ++i)
		{
			const auto& user = i->second->getUser();
			if (user && user->getIP() == ip)
			{
				const string nick = user->getLastNick();
				if (!nick.empty())
					nicks.insert(nick);
			}
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
