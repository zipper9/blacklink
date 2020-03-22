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

UserPtr ClientManager::g_me;
CID ClientManager::g_pid;
volatile bool g_isShutdown = false;
volatile bool g_isBeforeShutdown = false;
bool g_isStartupProcess = true;
bool ClientManager::g_isSpyFrame = false;
ClientManager::ClientList ClientManager::g_clients;
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
OnlineUserList ClientManager::g_UserUpdateQueue;
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csOnlineUsersUpdateQueue = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif

std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csClients = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csOnlineUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csUsers = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());

ClientManager::OnlineMap ClientManager::g_onlineUsers;
ClientManager::UserMap ClientManager::g_users;

ClientManager::ClientManager()
{
	if (SETTING(NICK).empty())
	{
		SET_SETTING(NICK, Util::getRandomNick(15));
	}
	dcassert(!SETTING(NICK).empty());
	createMe(SETTING(PRIVATE_ID), SETTING(NICK));
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
	Client* c;
	if (Util::isAdc(hubURL))
	{
		c = new AdcHub(hubURL, false);
	}
	else if (Util::isAdcS(hubURL))
	{
		c = new AdcHub(hubURL, true);
	}
	else if (Util::isNmdcS(hubURL))
	{
		c = new NmdcHub(hubURL, true);
	}
	else
	{
		c = new NmdcHub(hubURL, false);
	}
	
	{
		CFlyWriteLock(*g_csClients);
		g_clients.insert(make_pair(c->getHubUrl(), c));
	}
	
	c->addListener(this);
	
	return c;
}

#if 0 // Not Used
std::map<string, CFlyClientStatistic > ClientManager::getClientStat()
{
	std::map<string, CFlyClientStatistic> l_stat;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		CFlyClientStatistic l_item;
		if (i->second->isConnected())
		{
			l_item.m_count_user = i->second->getUserCount();
			l_item.m_share_size = i->second->getAvailableBytes();
			l_item.m_message_count = i->second->getMessagesCount();
			l_item.m_is_active = i->second->isActive();
			if (l_item.m_message_count)
			{
				i->second->clearMessagesCount();
			}
		}
		l_stat[i->first] = l_item;
	}
	return l_stat;
}
#endif

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

unsigned ClientManager::getTotalUsers()
{
	unsigned l_users = 0;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		l_users += i->second->getUserCount();
	}
	return l_users;
}
void ClientManager::setIPUser(const UserPtr& p_user, const string& p_ip, const uint16_t p_udpPort /* = 0 */)
{
	if (p_ip.empty())
		return;
		
	CFlyWriteLock(*g_csOnlineUsers);
	const auto p = g_onlineUsers.equal_range(p_user->getCID());
	for (auto i = p.first; i != p.second; ++i)
	{
#ifdef _DEBUG
//		const auto l_old_ip = i->second->getIdentity().getIpAsString();
//		if (l_old_ip != p_ip)
//		{
//			LogManager::message("ClientManager::setIPUser, p_user = " + p_user->getLastNick() + " old ip = " + l_old_ip + " ip = " + p_ip);
//		}
#endif
		i->second->getIdentity().setIp(p_ip);
		if (p_udpPort != 0)
		{
			i->second->getIdentity().setUdpPort(p_udpPort);
		}
	}
}

bool ClientManager::getUserParams(const UserPtr& user, UserParams& p_params)
{
	CFlyReadLock(*g_csOnlineUsers);
	const OnlineUserPtr u = getOnlineUserL(user);
	if (u)
	{
		// [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'u->getIdentity()' expression repeatedly. clientmanager.h 160
		const auto& i = u->getIdentity();
		p_params.m_bytesShared = i.getBytesShared();
		p_params.m_slots = i.getSlots();
		p_params.m_limit = i.getLimit();
		p_params.m_ip = i.getIpAsString();
		p_params.m_tag = i.getTag();
		p_params.m_nick = i.getNick();
		
		return true;
	}
	return false;
}

#ifndef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
void ClientManager::getConnectedHubUrls(StringList& p_hub_url)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
			p_hub_url.push_back(i->second->getHubUrl());
	}
}

void ClientManager::getConnectedHubInfo(HubInfoArray& p_hub_info)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
		{
			HubInfo l_info;
			l_info.m_hub_url  = i->second->getHubUrl();
			l_info.m_hub_name = i->second->getHubName();
			l_info.m_is_op = i->second->getMyIdentity().isOp();
			p_hub_info.push_back(l_info);
		}
	}
}
#endif // IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER

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
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubUrl());
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
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
#ifdef _DEBUG
	//LogManager::message("[!!!!!!!] ClientManager::getHubNames cid = " + cid.toBase32() + " hintUrl = " + hintUrl + " priv = " + Util::toString(priv));
#endif
	StringList lst;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubName()); // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=114958
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
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

bool ClientManager::isConnected(const string& aUrl)
{
	CFlyReadLock(*g_csClients);
	return g_clients.find(aUrl) != g_clients.end();
}

bool ClientManager::isOnline(const UserPtr& aUser)
{
	CFlyReadLock(*g_csOnlineUsers);
	return g_onlineUsers.find(aUser->getCID()) != g_onlineUsers.end();
}

OnlineUserPtr ClientManager::findOnlineUserL(const HintedUser& user, bool priv)
{
	if (user.user)
		return findOnlineUserL(user.user->getCID(), user.hint, priv);
	else
		return OnlineUserPtr();
}

UserPtr ClientManager::findUser(const string& aNick, const string& aHubUrl)
{
	return findUser(makeCid(aNick, aHubUrl));
}

OnlineUserPtr ClientManager::findOnlineUserL(const CID& cid, const string& hintUrl, bool priv)
{
	// [!] IRainman: This function need to external lock.
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

string ClientManager::getStringField(const CID& cid, const string& hint, const char* field) // [!] IRainman fix.
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

uint8_t ClientManager::getSlots(const CID& cid)
{
	CFlyReadLock(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		return i->second->getIdentity().getSlots();
	}
	return 0;
}

Client* ClientManager::findClient(const string& p_url)
{
	dcassert(!p_url.empty());
	CFlyReadLock(*g_csClients);
	const auto& i = g_clients.find(p_url);
	if (i != g_clients.end())
	{
		return i->second;
	}
	return nullptr;
}

string ClientManager::findHub(const string& ipPort)
{
	if (ipPort.empty()) //[+]FlylinkDC++ Team
		return Util::emptyString;
		
	// [-] CFlyLock(cs); IRainman opt.
	
	string ip_or_host;
	uint16_t port = 411;
	Util::parseIpPort(ipPort, ip_or_host, port);
	string url;
	boost::system::error_code ec;
	const auto l_ip = boost::asio::ip::address_v4::from_string(ip_or_host, ec);
	//dcassert(!ec);
	CFlyReadLock(*g_csClients); // [+] IRainman opt.
	for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
	{
		const Client* c = j->second;
		if (c->getPort() == port) // [!] IRainman opt.
		{
			// If exact match is found, return it
			if (ec)
			{
				if (c->getAddress() == ip_or_host)
				{
					url = c->getHubUrl();
					break;
				}
			}
			else if (c->getIp() == l_ip)
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

UserPtr ClientManager::findLegacyUser(const string& aNick, const string& aHubUrl)
{
	dcassert(!aNick.empty());
	if (!aNick.empty())
	{
		CFlyReadLock(*g_csClients);
		if (!aHubUrl.empty())
		{
			const auto& i = g_clients.find(aHubUrl);
			if (i != g_clients.end())
			{
				const auto& ou = i->second->findUser(aNick);
				if (ou)
				{
					return ou->getUser();
				}
			}
		}
		for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
		{
			const auto& ou = j->second->findUser(aNick);
			if (ou)
			{
				return ou->getUser();
			}
		}
	}
	return UserPtr();
}

UserPtr ClientManager::getUser(const string& nick, const string& hubURL, uint32_t hubID)
{
	dcassert(!nick.empty());
	const CID cid = makeCid(nick, hubURL);
	
	CFlyWriteLock(*g_csUsers);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	auto p = g_users.insert(make_pair(cid, std::make_shared<User>(cid, nick, hubID)));
#else
	auto p = g_users.insert(make_pair(cid, std::make_shared<User>(cid, nick)));
#endif
	if (!p.second)
	{
		const auto& user = p.first->second;
		user->setLastNick(nick);
		user->setFlag(User::NMDC);
		// TODO-2 зачем второй раз прописывать флаг на NMDC
/*
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		if (!l_user->getHubID() && p_HubID)
			l_user->setHubID(p_HubID); // TODO-3 а это зачем повторно. оно разве может поменяться?
#endif
*/
		return user;
	}
	p.first->second->setFlag(User::NMDC);
	return p.first->second;
}

UserPtr ClientManager::createUser(const CID& cid, const string& nick, uint32_t hubID)
{
	CFlyWriteLock(*g_csUsers);
	auto p = g_users.insert(make_pair(cid, UserPtr()));
	if (!p.second)
		return p.first->second;
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	p.first->second = std::make_shared<User>(cid, nick, hubID);
#else
	p.first->second = std::make_shared<User>(cid, nick);
#endif
	return p.first->second;
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

// deprecated
bool ClientManager::isOp(const UserPtr& user, const string& aHubUrl)
{
	CFlyReadLock(*g_csOnlineUsers);
	const auto p = g_onlineUsers.equal_range(user->getCID());
	for (auto i = p.first; i != p.second; ++i)
	{
		const auto& l_hub = i->second->getClient().getHubUrl();
		if (l_hub == aHubUrl)
			return i->second->getIdentity().isOp();
	}
	return false;
}

CID ClientManager::makeCid(const string& aNick, const string& aHubUrl)
{
	TigerHash th;
	th.update(aNick.c_str(), aNick.length());
	th.update(aHubUrl.c_str(), aHubUrl.length());
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
	ou->getUser()->flushRatio();
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
	// [!] IRainman fix: This function need to external lock.
	p = g_onlineUsers.equal_range(cid);
	
	if (p.first == p.second) // no user found with the given CID.
		return nullptr;
		
	if (!hintUrl.empty()) // [+] IRainman fix.
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
			u->getClientBase().connect(*u, token, forcePassive);
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

void ClientManager::fireIncomingSearch(const string& aSeeker, const string& aString, ClientManagerListener::SearchReply p_re)
{
	if (g_isSpyFrame)
		Speaker<ClientManagerListener>::fly_fire3(ClientManagerListener::IncomingSearch(), aSeeker, aString, p_re);
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
	AdcSearchParam param(adc.getParameters(), isUdpActive ? 10 : 5);
	const ClientManagerListener::SearchReply re = SearchManager::getInstance()->respond(param, from, seeker);
	if (g_isSpyFrame)
		for (auto i = param.include.cbegin(); i != param.include.cend(); ++i)
			Speaker<ClientManagerListener>::fly_fire3(ClientManagerListener::IncomingSearch(), seeker, i->getPattern(), re);
}

void ClientManager::search(const SearchParamToken& sp)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (c->isConnected())
		{
			/*
			SearchParamToken l_search_param_token;
			l_search_param_token.m_token = p_search_param.m_token;
			l_search_param_token.m_is_force_passive_searh = p_search_param.m_is_force_passive_searh;
			l_search_param_token.m_size_mode = p_search_param.m_size_mode;
			l_search_param_token.m_size = p_search_param.m_size;
			l_search_param_token.m_file_type = p_search_param.m_file_type;
			l_search_param_token.m_filter = p_search_param.m_filter;
			l_search_param_token.m_owner  = p_search_param.m_owner;
			l_search_param_token.m_ext_list.clear();
			c->search_internal(l_search_param_token);
			*/
			c->searchInternal(sp);
		}
	}
}

uint64_t ClientManager::multiSearch(const SearchParamTokenMultiClient& sp)
{
	uint64_t estimateSearchSpan = 0;
	if (sp.clients.empty())
	{
		CFlyReadLock(*g_csClients);
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
			if (i->second->isConnected())
			{
				const uint64_t ret = i->second->searchInternal(sp);
				estimateSearchSpan = max(estimateSearchSpan, ret);
			}
	}
	else
	{
		CFlyReadLock(*g_csClients);
		for (auto it = sp.clients.cbegin(); it != sp.clients.cend(); ++it)
		{
			const string& client = *it;
			
			const auto& i = g_clients.find(client);
			if (i != g_clients.end() && i->second->isConnected())
			{
				const uint64_t ret = i->second->searchInternal(sp);
				estimateSearchSpan = max(estimateSearchSpan, ret);
			}
		}
	}
	return estimateSearchSpan;
}

void ClientManager::getOnlineClients(StringSet& p_onlineClients)
{
	CFlyReadLock(*g_csClients);
	p_onlineClients.clear();
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
			p_onlineClients.insert(i->second->getHubUrl());
	}
}
void ClientManager::addAsyncOnlineUserUpdated(const OnlineUserPtr& p_ou)
{
	if (!isBeforeShutdown())
	{
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
		CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.push_back(p_ou);
#else
		fly_fire1(ClientManagerListener::UserUpdated(), p_ou);
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
			bool enableMessageCounter = BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER);
			CFlyReadLock(*g_csUsers);
			for (auto i = g_users.cbegin(); i != g_users.cend(); ++i)
			{
				if (i->second->isDirty(enableMessageCounter))
					usersToFlush.push_back(i->second);
			}
		}
		for (auto& user : usersToFlush)
		{
			user->flushRatio();
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

/*
void ClientManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{
    usersCleanup();
}
*/

void ClientManager::createMe(const string& pid, const string& nick)
{
	dcassert(!g_me);
	dcassert(g_pid.isZero());
	
	g_pid = CID(pid);
	
	TigerHash tiger;
	tiger.update(g_pid.data(), CID::SIZE);
	const CID myCID = CID(tiger.finalize());

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	g_me = std::make_shared<User>(myCID, nick, 0);
#else
	g_me = std::make_shared<User>(myCID, nick);
#endif
	
	{
		CFlyWriteLock(*g_csUsers);
		//CFlyLock(g_csUsers);
		g_users.insert(make_pair(g_me->getCID(), g_me));
	}
}

void ClientManager::changeMyPID(const string& pid)
{
	dcassert(g_me);
	CID oldCID = g_me->getCID();

	g_pid = CID(pid);
	
	TigerHash tiger;
	tiger.update(g_pid.data(), CID::SIZE);
	//g_me->setCID(CID(tiger.finalize()));

	{
		CFlyWriteLock(*g_csUsers);
		g_users.erase(oldCID);
		g_users.insert(make_pair(g_me->getCID(), g_me));
	}
}

const CID& ClientManager::getMyCID()
{
	dcassert(g_me);
	if (g_me)
		return g_me->getCID();
	else
	{
		static CID g_CID;
		return g_CID;
	}
}

const CID& ClientManager::getMyPID()
{
	dcassert(!g_pid.isZero());
	return g_pid;
}

const string ClientManager::findMyNick(const string& hubUrl)
{
	CFlyReadLock(*g_csClients);
	const auto& i = g_clients.find(hubUrl);
	if (i != g_clients.end())
		return i->second->getMyNick(); // [!] IRainman opt.
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

void ClientManager::updateNick(const OnlineUserPtr& p_online_user)
{
	if (p_online_user->getUser()->getLastNick().empty())
	{
		const string& l_nick_from_identity = p_online_user->getIdentity().getNick();
		p_online_user->getUser()->setLastNick(l_nick_from_identity);
		dcassert(p_online_user->getUser()->getLastNick() != l_nick_from_identity); // TODO поймать когда это бывает?
	}
	else
	{
#ifdef _DEBUG
		//dcassert(0);
		//const string& l_nick_from_identity = p_online_user->getIdentity().getNick();
		//LogManager::message("[DUP] updateNick(const OnlineUserPtr& p_online_user) ! nick==nick == "
		//                    + l_nick_from_identity + " p_online_user->getUser()->getLastNick() = " + p_online_user->getUser()->getLastNick());
#endif
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

////////////////////
/**
 * This file is a part of client manager.
 * It has been divided but shouldn't be used anywhere else.
 */
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

void ClientManager::cheatMessage(Client* p_client, const string& p_report)
{
	if (p_client && !p_report.empty() && BOOLSETTING(DISPLAY_CHEATS_IN_MAIN_CHAT))
	{
		p_client->cheatMessage(p_report);
	}
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
			auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou.getIdentity()' expression repeatedly. cheatmanager.h 80
			
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
	const bool priv = FavoriteManager::getInstance()->isPrivateHub(user.hint);
	string report;
	Client* client = nullptr;
	if (user.user)
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineUserPtr ou = findOnlineUserL(user.user->getCID(), user.hint, priv);
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
