/*
 * Copyright (C) 2011-2013 Alexey Solomin, a.rainman on gmail point com
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
#include "UserManager.h"
#include "DatabaseManager.h"
#include "QueueManager.h"
#include "Client.h"
#include "FavoriteManager.h"
#include "Wildcards.h"
#include "SettingsManager.h"

#ifdef _DEBUG
static bool ignoreListLoaded;
#endif

UserManager::UserManager()
{
	csIgnoreList = std::unique_ptr<RWLock>(RWLock::create());
	ignoreListEmpty = true;
	loadIgnoreList();
}

void UserManager::setPMOpen(const UserPtr& user, bool flag)
{
	LOCK(csPM);
	auto i = pmInfo.find(user);
	if (i != pmInfo.end())
	{
		auto& flags = i->second.flags;
		if (flag)
			flags |= FLAG_OPEN;
		else
		{
			flags &= ~FLAG_OPEN;
			if (!flags) pmInfo.erase(i);
		}
	}
	else if (flag)
		pmInfo.insert(make_pair(user, PMInfo{FLAG_OPEN, string()}));
}

bool UserManager::checkPMOpen(const ChatMessage& pm, UserManager::PasswordStatus& passwordStatus)
{
	passwordStatus = FIRST;
	LOCK(csPM);
	auto i = pmInfo.find(pm.replyTo->getUser());
	if (i == pmInfo.end())
		return false;
	auto& pmi = i->second;
	if (pmi.flags & FLAG_PW_ACTIVITY)
	{
		if (pmi.flags & FLAG_GRANTED)
			passwordStatus = GRANTED;
		else if (pm.text.find(pmi.password) != string::npos)
		{
			pmi.flags |= FLAG_GRANTED;
			passwordStatus = CHECKED;
		}
		else
			passwordStatus = WAITING;
	}
	return (pmi.flags & FLAG_OPEN) != 0;
}

void UserManager::addPMPassword(const UserPtr& user, const string& password)
{
	LOCK(csPM);
	auto i = pmInfo.find(user);
	if (i != pmInfo.end())
	{
		auto& pmi = i->second;
		dcassert(!(pmi.flags & FLAG_OPEN));
		pmi.password = password;
		pmi.flags |= FLAG_SENDING_REQUEST | FLAG_PW_ACTIVITY;
	}
	else
		pmInfo.insert(make_pair(user, PMInfo{FLAG_SENDING_REQUEST | FLAG_PW_ACTIVITY, password}));
}

bool UserManager::checkOutgoingPM(const UserPtr& user, bool automatic)
{
	LOCK(csPM);
	auto i = pmInfo.find(user);
	if (i == pmInfo.end())
		return !automatic;

	auto& flags = i->second.flags;
	if (flags & FLAG_PW_ACTIVITY)
	{
		if (flags & FLAG_SENDING_REQUEST)
		{
			flags ^= FLAG_SENDING_REQUEST;
			return (flags & FLAG_OPEN) != 0;
		}
		if (!automatic) flags |= FLAG_GRANTED;
	}
	return !automatic || (flags & FLAG_OPEN) != 0;
}

#ifdef IRAINMAN_INCLUDE_USER_CHECK
void UserManager::checkUser(const OnlineUserPtr& user) const
{
	if (BOOLSETTING(CHECK_NEW_USERS))
	{
		if (!user->getUser()->isMe())
		{
			const Client& client = user->getClient();
			if (!client.getExcludeCheck() && client.isOp() && (client.isActive() || user->getIdentity().isTcpActive()))
			{
				if (!BOOLSETTING(DONT_BAN_FAVS) || (user->getUser()->getFlags() & (User::FAVORITE | User::BANNED)) == User::FAVORITE)
				{
					if (!isInProtectedUserList(user->getIdentity().getNick()))
					{
						try
						{
							QueueManager::getInstance()->addList(HintedUser(user->getUser(), client.getHubUrl()), QueueItem::FLAG_USER_CHECK);
						}
						catch (const Exception& e)
						{
							LogManager::message(e.getError());
						}
					}
				}
			}
		}
	}
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void UserManager::getIgnoreList(vector<IgnoreListItem>& ignoreList) const noexcept
{
	dcassert(ignoreListLoaded);
	ignoreList.clear();
	READ_LOCK(*csIgnoreList);
	for (const string& nick : ignoredNicks)
		ignoreList.emplace_back(IgnoreListItem{nick, IGNORE_NICK});
	for (const auto& ext : ignoredExt)
		ignoreList.emplace_back(IgnoreListItem{ext.first, ext.second.type});
}

string UserManager::getIgnoreListAsString() const noexcept
{
	string result;
	READ_LOCK(*csIgnoreList);
	for (const string& nick : ignoredNicks)
	{
		result += ' ';
		result += nick;
	}
	for (const auto& ext : ignoredExt)
	{
		result += ' ';
		result += ext.first;
	}
	return result;
}

bool UserManager::addToIgnoreList(const IgnoreListItem& item)
{
	std::regex re;
	if (item.type == IGNORE_WILDCARD && !Wildcards::regexFromPatternList(re, item.data, false))
		return false;
	bool result = false;
	{
		dcassert(ignoreListLoaded);
		WRITE_LOCK(*csIgnoreList);
		if (item.type == IGNORE_NICK)
		{
			if (ignoredExt.find(item.data) == ignoredExt.end())
				result = ignoredNicks.insert(item.data).second;
		}
		else if (item.type == IGNORE_WILDCARD)
		{
			if (ignoredNicks.find(item.data) == ignoredNicks.end())
			{
				auto r = ignoredExt.insert(make_pair(item.data, ParsedIgnoreListItem{}));
				if (r.second)
				{
					auto& parsed = r.first->second;
					parsed.re = std::move(re);
					parsed.type = IGNORE_WILDCARD;
					result = true;
				}
			}
		}
	}
	if (result)
	{
		saveIgnoreList();
		fire(UserManagerListener::IgnoreListChanged());
	}
	return result;
}

void UserManager::removeFromIgnoreList(const IgnoreListItem& item)
{
	{
		dcassert(ignoreListLoaded);
		WRITE_LOCK(*csIgnoreList);
		if (item.type == IGNORE_NICK)
			ignoredNicks.erase(item.data);
		else
		{
			auto i = ignoredExt.find(item.data);
			if (i != ignoredExt.end()) ignoredExt.erase(i);
		}
	}
	saveIgnoreList();
	fire(UserManagerListener::IgnoreListChanged());
}

void UserManager::removeFromIgnoreList(const vector<string>& items)
{
	bool changed = false;
	{
		dcassert(ignoreListLoaded);
		WRITE_LOCK(*csIgnoreList);
		for (const string& item : items)
		{
			auto i = ignoredNicks.find(item);
			if (i != ignoredNicks.end())
			{
				ignoredNicks.erase(i);
				changed = true;
			}
			auto j = ignoredExt.find(item);
			if (j != ignoredExt.end())
			{
				ignoredExt.erase(j);
				changed = true;
			}
		}
	}
	if (changed)
	{
		saveIgnoreList();
		fire(UserManagerListener::IgnoreListChanged());
	}
}

void UserManager::removeFromIgnoreList(const vector<IgnoreListItem>& items)
{
	{
		dcassert(ignoreListLoaded);
		WRITE_LOCK(*csIgnoreList);
		for (const auto& item : items)
			if (item.type == IGNORE_NICK)
				ignoredNicks.erase(item.data);
			else
			{
				auto i = ignoredExt.find(item.data);
				if (i != ignoredExt.end()) ignoredExt.erase(i);
			}
	}
	saveIgnoreList();
	fire(UserManagerListener::IgnoreListChanged());
}

bool UserManager::isInIgnoreList(const string& nick, int* type) const noexcept
{
	if (!nick.empty() && !ignoreListEmpty)
	{
		dcassert(ignoreListLoaded);
		READ_LOCK(*csIgnoreList);
		if (ignoredNicks.find(nick) != ignoredNicks.end())
		{
			if (type) *type = IGNORE_NICK;
			return true;
		}
		for (const auto& i : ignoredExt)
			if (std::regex_match(nick, i.second.re))
			{
				if (type) *type = i.second.type;
				return true;
			}
	}
	return false;
}

void UserManager::clearIgnoreList()
{
	bool changed = false;
	{
		WRITE_LOCK(*csIgnoreList);
		if (!ignoredNicks.empty())
		{
			changed = true;
			ignoredNicks.clear();
		}
		if (!ignoredExt.empty())
		{
			changed = true;
			ignoredExt.clear();
		}
	}
	if (changed)
	{
		saveIgnoreList();
		fire(UserManagerListener::IgnoreListCleared());
	}
}

void UserManager::loadIgnoreList()
{
	vector<DBIgnoreListItem> items;
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	if (conn)
	{
		conn->loadIgnoredUsers(items);
		dm->putConnection(conn);
	}

	vector<pair<string, std::regex>> parsedReg;
	std::regex re;
	for (const auto& item : items)
		if (item.type == IGNORE_WILDCARD && Wildcards::regexFromPatternList(re, item.data, false))
			parsedReg.emplace_back(make_pair(item.data, std::move(re)));

	WRITE_LOCK(*csIgnoreList);
	{
		ignoredNicks.clear();
		ignoredExt.clear();
		for (const auto& item : items)
			if (item.type == IGNORE_NICK)
				ignoredNicks.insert(item.data);
		for (auto& item : parsedReg)
		{
			if (ignoredNicks.find(item.first) != ignoredNicks.end()) continue;
			auto& parsed = ignoredExt[item.first];
			parsed.re = std::move(item.second);
			parsed.type = IGNORE_WILDCARD;
		}
		ignoreListEmpty = ignoredNicks.empty() && ignoredExt.empty();
	}
	dcdrun(ignoreListLoaded = true);
}

void UserManager::saveIgnoreList()
{
	vector<DBIgnoreListItem> items;
	{
		dcassert(ignoreListLoaded);
		READ_LOCK(*csIgnoreList);
		for (const string& nick : ignoredNicks)
			items.emplace_back(DBIgnoreListItem{nick, IGNORE_NICK});
		for (const auto& ext : ignoredExt)
			items.emplace_back(DBIgnoreListItem{ext.first, ext.second.type});
		ignoreListEmpty = items.empty();
	}
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	if (conn)
	{
		conn->saveIgnoredUsers(items);
		dm->putConnection(conn);
	}
}

void UserManager::openUserUrl(const string& url, const UserPtr& user) noexcept
{
	fire(UserManagerListener::OpenHub(), url, user);
}

void UserManager::fireReservedSlotChanged(const UserPtr& user) noexcept
{
	fire(UserManagerListener::ReservedSlotChanged(), user);
}
