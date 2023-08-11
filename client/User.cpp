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
#include "Client.h"
#include "SettingsManager.h"
#include "DatabaseManager.h"

#ifdef _DEBUG
std::atomic_int User::g_user_counts(0);
#endif

User::User(const CID& cid, const string& nick) : cid(cid),
	nick(nick),
	flags(0),
	bytesShared(0),
	limit(0),
	slots(0),
	uploadCount(0),
	lastIp4(0),
	lastIp6{}
#ifdef BL_FEATURE_IP_DATABASE
	, ipStat(nullptr)
#endif
{
	BOOST_STATIC_ASSERT(LAST_BIT < 32);
#ifdef _DEBUG
	++g_user_counts;
#endif
}

User::~User()
{
#ifdef _DEBUG
	--g_user_counts;
#endif
#ifdef BL_FEATURE_IP_DATABASE
	delete ipStat;
#endif
}

string User::getLastNick() const
{
	LOCK(cs);
	return nick;
}

bool User::hasNick() const
{
	LOCK(cs);
	return !nick.empty();
}

void User::setLastNick(const string& newNick)
{
	dcassert(!newNick.empty());
	LOCK(cs);
	nick = newNick;
}

void User::updateNick(const string& newNick)
{
	dcassert(!newNick.empty());
	LOCK(cs);
	if (nick.empty())
		nick = newNick;
}

void User::setIP4(Ip4Address ip)
{
	if (!Util::isValidIp4(ip))
		return;
	LOCK(cs);
	if (lastIp4 == ip)
		return;
	lastIp4 = ip;
	flags |= LAST_IP_CHANGED;
#ifdef BL_FEATURE_IP_DATABASE
	userStat.setIP(Util::printIpAddress(ip));
	if ((userStat.flags & (UserStatItem::FLAG_LOADED | UserStatItem::FLAG_CHANGED)) == (UserStatItem::FLAG_LOADED | UserStatItem::FLAG_CHANGED))
	{
		dcassert(flags & USER_STAT_LOADED);
		flags |= SAVE_USER_STAT;
	}
#endif
}

void User::setIP6(const Ip6Address& ip)
{
	if (!Util::isValidIp6(ip))
		return;
	LOCK(cs);
	if (lastIp6 == ip)
		return;
	lastIp6 = ip;
	flags |= LAST_IP_CHANGED;
#ifdef BL_FEATURE_IP_DATABASE
	userStat.setIP(Util::printIpAddress(ip));
	if ((userStat.flags & (UserStatItem::FLAG_LOADED | UserStatItem::FLAG_CHANGED)) == (UserStatItem::FLAG_LOADED | UserStatItem::FLAG_CHANGED))
	{
		dcassert(flags & USER_STAT_LOADED);
		flags |= SAVE_USER_STAT;
	}
#endif
}

void User::setIP(const IpAddress& ip)
{
	if (ip.type == AF_INET)
		setIP4(ip.data.v4);
	else if (ip.type == AF_INET6)
		setIP6(ip.data.v6);
}

Ip4Address User::getIP4() const
{
	LOCK(cs);
	return lastIp4;
}

Ip6Address User::getIP6() const
{
	LOCK(cs);
	return lastIp6;
}

void User::getInfo(string& nick, Ip4Address& ip4, Ip6Address& ip6, int64_t& bytesShared, int& slots) const
{
	LOCK(cs);
	nick = this->nick;
	ip4 = lastIp4;
	ip6 = lastIp6;
	bytesShared = this->bytesShared;
	slots = this->slots;
}

#ifdef BL_FEATURE_IP_DATABASE
unsigned User::getMessageCount() const
{
	LOCK(cs);
	return userStat.messageCount;
}

uint64_t User::getBytesUploaded() const
{
	LOCK(cs);
	return ipStat ? ipStat->totalUploaded : 0;
}

uint64_t User::getBytesDownloaded() const
{
	LOCK(cs);
	return ipStat ? ipStat->totalDownloaded : 0;
}

void User::getBytesTransfered(uint64_t out[]) const
{
	LOCK(cs);
	if (ipStat)
	{
		out[0] = ipStat->totalDownloaded;
		out[1] = ipStat->totalUploaded;
	}
	else
		out[0] = out[1] = 0;
}

void User::addBytesUploaded(const IpAddress& ip, uint64_t size)
{
	LOCK(cs);
	if (flags & IP_STAT_LOADED)
	{
		if (!ipStat) ipStat = new IPStatMap;
		IPStatItem& item = ipStat->data[Util::printIpAddress(ip)];
		item.upload += size;
		item.flags |= IPStatItem::FLAG_CHANGED;
		ipStat->totalUploaded += size;
		flags |= IP_STAT_CHANGED;
	}
}

void User::addBytesDownloaded(const IpAddress& ip, uint64_t size)
{
	LOCK(cs);
	if (flags & IP_STAT_LOADED)
	{
		if (!ipStat) ipStat = new IPStatMap;
		IPStatItem& item = ipStat->data[Util::printIpAddress(ip)];
		item.download += size;
		item.flags |= IPStatItem::FLAG_CHANGED;
		ipStat->totalDownloaded += size;
		flags |= IP_STAT_CHANGED;
	}
}

void User::loadIPStatFromDB()
{
	if (!BOOLSETTING(ENABLE_RATIO_USER_LIST))
		return;

	IPStatMap* dbStat = nullptr;
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	if (conn)
	{
		dbStat = conn->loadIPStat(getCID());
		dm->putConnection(conn);
	}
	LOCK(cs);
	flags |= IP_STAT_LOADED;
	delete ipStat;
	ipStat = dbStat;
}

void User::loadIPStat()
{
	if (!(getFlags() & IP_STAT_LOADED))
		loadIPStatFromDB();	
}

void User::loadUserStatFromDB()
{
	if (!BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER))
		return;
	
	UserStatItem dbStat;
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	if (conn)
	{
		conn->loadUserStat(getCID(), dbStat);
		dm->putConnection(conn);
	}

	LOCK(cs);
	flags |= USER_STAT_LOADED;
	for (const auto& nick : userStat.nickList)
		dbStat.addNick(nick);
	if (!userStat.lastIp.empty())
		dbStat.setIP(userStat.lastIp);
	userStat = std::move(dbStat);
	IpAddress ip;
	if (Util::parseIpAddress(ip, userStat.lastIp) && Util::isValidIp(ip))
	{
		if (ip.type == AF_INET)
		{
			if (lastIp4 == 0)
			{
				lastIp4 = ip.data.v4;
				flags |= LAST_IP_CHANGED;
			}
		}
		else
		{
			if (Util::isEmpty(lastIp6))
			{
				lastIp6 = ip.data.v6;
				flags |= LAST_IP_CHANGED;
			}
		}
	}
}

void User::loadUserStat()
{
	if (!(getFlags() & USER_STAT_LOADED))
		loadUserStatFromDB();
}

void User::saveUserStat()
{
	cs.lock();
	if (!(flags & SAVE_USER_STAT))
	{
		cs.unlock();
		return;
	}
	flags &= ~SAVE_USER_STAT;
	dcassert(flags & USER_STAT_LOADED);
	if (userStat.nickList.empty())
	{
		cs.unlock();
		dcassert(0);
		return;
	}
	userStat.flags &= ~UserStatItem::FLAG_CHANGED;
	UserStatItem dbStat = userStat;
	userStat.flags |= UserStatItem::FLAG_LOADED;
	cs.unlock();

	DatabaseManager::getInstance()->saveUserStat(getCID(), dbStat);
}

void User::saveIPStat()
{
	vector<IPStatVecItem> items;
	bool loadUserStat = false;
	cs.lock();
	if ((flags & (IP_STAT_LOADED | IP_STAT_CHANGED)) != (IP_STAT_LOADED | IP_STAT_CHANGED) || !ipStat)
	{
		cs.unlock();
		return;
	}
	for (auto& i : ipStat->data)
	{
		IPStatItem& item = i.second;
		if (item.flags & IPStatItem::FLAG_CHANGED)
		{
			item.flags &= ~IPStatItem::FLAG_CHANGED;
			items.emplace_back(IPStatVecItem{i.first, item});
			item.flags |= IPStatItem::FLAG_LOADED;
		}
	}
	flags &= ~IP_STAT_CHANGED;
	if (!items.empty() && !(userStat.flags & UserStatItem::FLAG_LOADED))
	{
		flags |= SAVE_USER_STAT;
		if (!(flags & USER_STAT_LOADED)) loadUserStat = true;
	}
	cs.unlock();

	DatabaseManager::getInstance()->saveIPStat(getCID(), items);
	if (loadUserStat) loadUserStatFromDB();
}

void User::incMessageCount()
{
	cs.lock();
	if (!(flags & USER_STAT_LOADED))
	{
		cs.unlock();
		loadUserStatFromDB();
		cs.lock();
	}
	userStat.messageCount++;
	userStat.flags |= UserStatItem::FLAG_CHANGED;
	flags |= SAVE_USER_STAT;
	cs.unlock();
}

void User::saveStats(bool ipStat, bool userStat)
{
	if (ipStat) saveIPStat();
	if (userStat) saveUserStat();
}

bool User::statsChanged() const
{
	LOCK(cs);
	if (flags & SAVE_USER_STAT) return true;
	if ((flags & (IP_STAT_LOADED | IP_STAT_CHANGED)) == (IP_STAT_LOADED | IP_STAT_CHANGED) && ipStat) return true;
	return false;
}

bool User::getLastNickAndHub(string& nick, string& hub) const
{
	LOCK(cs);
	if (userStat.nickList.empty()) return false;
	const string& val = userStat.nickList.back();
	auto pos = val.find('\t');
	if (pos == string::npos) return false;
	nick = val.substr(0, pos);
	hub = val.substr(pos + 1);
	return true;
}
#endif

void User::addNick(const string& nick, const string& hub)
{
#ifdef BL_FEATURE_IP_DATABASE
	if (nick.empty() || hub.empty()) return;
	LOCK(cs);
	userStat.addNick(nick, hub);
	if (!(flags & USER_STAT_LOADED))
		userStat.flags &= ~UserStatItem::FLAG_CHANGED;
	else if (userStat.flags & UserStatItem::FLAG_CHANGED)
		flags |= SAVE_USER_STAT;
#endif
}
