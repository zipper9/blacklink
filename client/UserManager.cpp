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

#ifdef _DEBUG
static bool ignoreListLoaded;
#endif

UserManager::UserManager()
{
	csIgnoreList = std::unique_ptr<RWLock>(RWLock::create());
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	csProtectedUsers = std::unique_ptr<RWLock>(RWLock::create());
	hasProtectedUsers = false;
#endif
	ignoreListEmpty = true;
	loadIgnoreList();
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	reloadProtectedUsers();
#endif
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

void UserManager::getIgnoreList(StringSet& result) const
{
	dcassert(ignoreListLoaded);
	READ_LOCK(*csIgnoreList);
	result = ignoreList;
}

tstring UserManager::getIgnoreListAsString() const
{
	tstring result;
	READ_LOCK(*csIgnoreList);
	for (auto i = ignoreList.cbegin(); i != ignoreList.cend(); ++i)
	{
		result += _T(' ');
		result += Text::toT((*i));
	}
	return result;
}

bool UserManager::addToIgnoreList(const string& userName)
{
	bool result;
	{
		dcassert(ignoreListLoaded);
		WRITE_LOCK(*csIgnoreList);
		result = ignoreList.insert(userName).second;
	}
	if (result)
	{
		saveIgnoreList();
		fire(UserManagerListener::IgnoreListChanged(), userName);
	}
	return result;
}

void UserManager::removeFromIgnoreList(const string& userName)
{
	{
		dcassert(ignoreListLoaded);
		WRITE_LOCK(*csIgnoreList);
		ignoreList.erase(userName);
	}
	saveIgnoreList();
	fire(UserManagerListener::IgnoreListChanged(), userName);
}

void UserManager::removeFromIgnoreList(const vector<string>& userNames)
{
	{
		dcassert(ignoreListLoaded);
		WRITE_LOCK(*csIgnoreList);
		for (auto i = userNames.cbegin(); i != userNames.cend(); ++i)
			ignoreList.erase(*i);
	}
	saveIgnoreList();
	for (auto i = userNames.cbegin(); i != userNames.cend(); ++i)
		fire(UserManagerListener::IgnoreListChanged(), *i);
}

bool UserManager::isInIgnoreList(const string& nick) const
{
	if (!nick.empty() && !ignoreListEmpty)
	{
		dcassert(ignoreListLoaded);
		READ_LOCK(*csIgnoreList);
		return ignoreList.find(nick) != ignoreList.cend();
	}
	return false;
}

void UserManager::clearIgnoreList()
{
	{
		WRITE_LOCK(*csIgnoreList);
		if (ignoreList.empty()) return;
		ignoreList.clear();
	}
	saveIgnoreList();
	fire(UserManagerListener::IgnoreListCleared());
}

void UserManager::loadIgnoreList()
{	
	WRITE_LOCK(*csIgnoreList);
	{
		DatabaseManager::getInstance()->loadIgnoredUsers(ignoreList);
		ignoreListEmpty = ignoreList.empty();
	}
	dcdrun(ignoreListLoaded = true);
}

void UserManager::saveIgnoreList()
{
	{	
		dcassert(ignoreListLoaded);
		READ_LOCK(*csIgnoreList);
		DatabaseManager::getInstance()->saveIgnoredUsers(ignoreList);
		ignoreListEmpty = ignoreList.empty();
	}
}

#ifdef IRAINMAN_ENABLE_AUTO_BAN
void UserManager::reloadProtectedUsers()
{
	std::regex re;
	bool result = Wildcards::regexFromPatternList(re, SETTING(DONT_BAN_PATTERN), true);
	WRITE_LOCK(*csProtectedUsers);
	reProtectedUsers = std::move(re);
	hasProtectedUsers = result;
}
#endif

void UserManager::openUserUrl(const UserPtr& aUser)
{
	const string& url = FavoriteManager::getInstance()->getUserUrl(aUser);
	if (!url.empty())
	{
		fire(UserManagerListener::OpenHub(), url);
	}
}

#ifdef IRAINMAN_ENABLE_AUTO_BAN
bool UserManager::isInProtectedUserList(const string& userName) const
{
	READ_LOCK(*csProtectedUsers);
	return hasProtectedUsers && std::regex_match(userName, reProtectedUsers);
}
#endif

void UserManager::fireReservedSlotChanged(const UserPtr& user)
{
	fire(UserManagerListener::ReservedSlotChanged(), user);
}
