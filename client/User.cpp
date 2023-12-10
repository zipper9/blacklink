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
	lastIp6{},
	lastCheckTime(0)
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

bool User::setIP4(Ip4Address ip)
{
	if (!Util::isValidIp4(ip))
		return false;
	LOCK(cs);
	if (lastIp4 == ip)
		return false;
	lastIp4 = ip;
	flags |= LAST_IP_CHANGED;
#ifdef BL_FEATURE_IP_DATABASE
	userStat.setIP(Util::printIpAddress(ip));
	static const auto MASK = UserStatItem::FLAG_INSERTED | UserStatItem::FLAG_LAST_IP_CHANGED;
	if ((userStat.flags & MASK) == MASK)
	{
		dcassert(flags & USER_STAT_LOADED);
		flags |= SAVE_USER_STAT;
	}
#endif
	return true;
}

bool User::setIP6(const Ip6Address& ip)
{
	if (!Util::isValidIp6(ip))
		return false;
	LOCK(cs);
	if (lastIp6 == ip)
		return false;
	lastIp6 = ip;
	flags |= LAST_IP_CHANGED;
#ifdef BL_FEATURE_IP_DATABASE
	userStat.setIP(Util::printIpAddress(ip));
	static const auto MASK = UserStatItem::FLAG_INSERTED | UserStatItem::FLAG_LAST_IP_CHANGED;
	if ((userStat.flags & MASK) == MASK)
	{
		dcassert(flags & USER_STAT_LOADED);
		flags |= SAVE_USER_STAT;
	}
#endif
	return true;
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
	string key = Util::printIpAddress(ip);
	LOCK(cs);
	if (!ipStat) ipStat = new IPStatMap;
	IPStatItem& item = ipStat->data[key];
	item.upload += size;
	item.flags |= IPStatItem::FLAG_CHANGED;
	ipStat->totalUploaded += size;
	flags |= IP_STAT_CHANGED;
}

void User::addBytesDownloaded(const IpAddress& ip, uint64_t size)
{
	string key = Util::printIpAddress(ip);
	LOCK(cs);
	if (!ipStat) ipStat = new IPStatMap;
	IPStatItem& item = ipStat->data[key];
	item.download += size;
	item.flags |= IPStatItem::FLAG_CHANGED;
	ipStat->totalDownloaded += size;
	flags |= IP_STAT_CHANGED;
}

void User::addLoadedData(IPStatMap* newIpStat)
{
	cs.lock();
	if (newIpStat)
	{
		if (ipStat)
		{
			for (auto i : newIpStat->data)
			{
				auto j = ipStat->data.find(i.first);
				if (j == ipStat->data.end())
					ipStat->data.insert(i);
				else
				{
					const auto& d1 = i.second;
					auto& d2 = j->second;
					d2.download += d1.download;
					d2.upload += d1.upload;
					d2.flags |= IPStatItem::FLAG_INSERTED;
				}
			}
			ipStat->totalDownloaded += newIpStat->totalDownloaded;
			ipStat->totalUploaded += newIpStat->totalUploaded;
		}
		else
		{
			ipStat = newIpStat;
			newIpStat = nullptr;
		}
	}
	flags = (flags & ~IP_STAT_LOADING) | IP_STAT_LOADED;
	cs.unlock();
	delete newIpStat;
}

void User::addLoadedData(UserStatItem& dbUserStat)
{
	cs.lock();
	if (dbUserStat.flags & UserStatItem::FLAG_INSERTED)
	{
		userStat.flags = UserStatItem::FLAG_INSERTED;
		for (const string& item : userStat.nickList)
			dbUserStat.addNick(item);
		if (dbUserStat.flags & UserStatItem::FLAG_NICK_LIST_CHANGED)
		{
			userStat.nickList = std::move(dbUserStat.nickList);
			userStat.flags |= UserStatItem::FLAG_NICK_LIST_CHANGED;
			flags |= SAVE_USER_STAT;
		}
		if (userStat.lastIp.empty())
		{
			userStat.lastIp = dbUserStat.lastIp;
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
		else if (userStat.lastIp != dbUserStat.lastIp)
		{
			userStat.flags |= UserStatItem::FLAG_LAST_IP_CHANGED;
			flags |= SAVE_USER_STAT;
		}
		userStat.messageCount += dbUserStat.messageCount;
		if (userStat.messageCount != dbUserStat.messageCount)
		{
			userStat.flags |= UserStatItem::FLAG_MSG_COUNT_CHANGED;
			flags |= SAVE_USER_STAT;
		}
	}
	else if ((userStat.flags & UserStatItem::FLAG_MSG_COUNT_CHANGED) || (ipStat && ipStat->totalDownloaded + ipStat->totalUploaded))
		flags |= SAVE_USER_STAT;

	flags = (flags & ~USER_STAT_LOADING) | USER_STAT_LOADED;
	cs.unlock();
}

void User::loadIPStatFromDB(const UserPtr& user)
{
	{
		LOCK(user->cs);
		if (user->flags & (IP_STAT_LOADING | IP_STAT_LOADED)) return;
		user->flags |= IP_STAT_LOADING;
	}

	if (!BOOLSETTING(ENABLE_RATIO_USER_LIST))
	{
		LOCK(user->cs);
		user->flags = (user->flags & ~IP_STAT_LOADING) | IP_STAT_LOADED;
		return;
	}

	DatabaseManager::getInstance()->loadIPStatAsync(user);
}

void User::loadUserStatFromDB(const UserPtr& user)
{
	{
		LOCK(user->cs);
		if (user->flags & (USER_STAT_LOADING | USER_STAT_LOADED)) return;
		user->flags |= USER_STAT_LOADING;
	}

	if (!BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER))
	{
		LOCK(user->cs);
		user->flags = (user->flags & ~USER_STAT_LOADING) | USER_STAT_LOADED;
		return;
	}

	DatabaseManager::getInstance()->loadUserStatAsync(user);
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
	if (userStat.nickList.empty())
	{
		cs.unlock();
		dcassert(0);
		return;
	}
	userStat.flags &= ~(UserStatItem::FLAG_NICK_LIST_CHANGED | UserStatItem::FLAG_LAST_IP_CHANGED | UserStatItem::FLAG_MSG_COUNT_CHANGED);
	UserStatItem dbStat = userStat;
	userStat.flags |= UserStatItem::FLAG_INSERTED;
	cs.unlock();

	DatabaseManager::getInstance()->saveUserStat(getCID(), dbStat);
}

void User::saveIPStat()
{
	static const auto MASK = IP_STAT_LOADED | IP_STAT_CHANGED;
	cs.lock();
	if ((flags & MASK) != MASK || !ipStat)
	{
		cs.unlock();
		return;
	}
	vector<IPStatVecItem> items;
	for (auto& i : ipStat->data)
	{
		IPStatItem& item = i.second;
		if (item.flags & IPStatItem::FLAG_CHANGED)
		{
			item.flags &= ~IPStatItem::FLAG_CHANGED;
			items.emplace_back(IPStatVecItem{i.first, item});
			item.flags |= IPStatItem::FLAG_INSERTED;
		}
	}
	flags &= ~IP_STAT_CHANGED;
	if (!items.empty() && (flags & USER_STAT_LOADED) && !(userStat.flags & UserStatItem::FLAG_INSERTED))
		flags |= SAVE_USER_STAT;
	cs.unlock();

	if (!items.empty())
		DatabaseManager::getInstance()->saveIPStat(getCID(), items);
}

void User::incMessageCount()
{
	LOCK(cs);
	userStat.messageCount++;
	userStat.flags |= UserStatItem::FLAG_MSG_COUNT_CHANGED;
	if (flags & USER_STAT_LOADED)
		flags |= SAVE_USER_STAT;
}

void User::saveStats(bool ipStat, bool userStat)
{
	if (ipStat) saveIPStat();
	if (userStat) saveUserStat();
}

bool User::shouldSaveStats() const
{
	LOCK(cs);
	return (flags & (SAVE_USER_STAT | IP_STAT_CHANGED)) != 0;
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

void User::addNick(const string& nick, const string& hub)
{
	if (nick.empty() || hub.empty()) return;
	LOCK(cs);
	userStat.addNick(nick, hub);
	static const auto MASK = UserStatItem::FLAG_INSERTED | UserStatItem::FLAG_NICK_LIST_CHANGED;
	if ((userStat.flags & MASK) == MASK)
		flags |= SAVE_USER_STAT;
}

#if 0
void User::reportSaving()
{
	if (!(flags & SAVE_USER_STAT))
		LogManager::message("User " + nick + "/" + cid.toBase32() + " must be saved: flags=0x"
			+ Util::toHexString(flags) + ", userStatFlags=" + Util::toString(userStat.flags), false);
}
#endif

#endif // BL_FEATURE_IP_DATABASE
