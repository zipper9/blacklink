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

UserManager::CheckedUserSet UserManager::checkedPasswordUsers;
UserManager::WaitingUserMap UserManager::waitingPasswordUsers;

#ifdef _DEBUG
static bool ignoreListLoaded;
#endif

FastCriticalSection UserManager::g_csPsw;

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

UserManager::PasswordStatus UserManager::checkPrivateMessagePassword(const ChatMessage& pm)
{
	const UserPtr& user = pm.replyTo->getUser();
	LOCK(g_csPsw);
	if (checkedPasswordUsers.find(user) != checkedPasswordUsers.cend())
	{
		return FREE;
	}
	else if (pm.text == SETTING(PM_PASSWORD))
	{
		waitingPasswordUsers.erase(user);
		checkedPasswordUsers.insert(user);
		return CHECKED;
	}
	else if (waitingPasswordUsers.find(user) != waitingPasswordUsers.cend())
	{
		return WAITING;
	}
	else
	{
		waitingPasswordUsers.insert(make_pair(user, true));
		return FIRST;
	}
}

#ifdef IRAINMAN_INCLUDE_USER_CHECK
void UserManager::checkUser(const OnlineUserPtr& user) const
{
	if (BOOLSETTING(CHECK_NEW_USERS))
	{
		if (!ClientManager::getInstance()->isMe(user))
		{
			const Client& client = user->getClient();
			if (!client.getExcludeCheck() && client.isOp() && (client.isActive() || user->getIdentity().isTcpActive()))
			{
				if (!BOOLSETTING(DONT_BAN_FAVS) || FavoriteManager::getInstance()->isFavUserAndNotBanned(user->getUser()))
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
		fly_fire1(UserManagerListener::IgnoreListChanged(), userName);
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
	fly_fire1(UserManagerListener::IgnoreListChanged(), userName);
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
		fly_fire1(UserManagerListener::IgnoreListChanged(), *i);
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
	fly_fire(UserManagerListener::IgnoreListCleared());
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

bool UserManager::expectPasswordFromUser(const UserPtr& user)
{
	LOCK(g_csPsw);
	auto i = waitingPasswordUsers.find(user);
	if (i == waitingPasswordUsers.end())
	{
		return false;
	}
	else if (i->second)
	{
		i->second = false;
		return true;
	}
	else
	{
		waitingPasswordUsers.erase(user);
		checkedPasswordUsers.insert(user);
		return false;
	}
}

void UserManager::openUserUrl(const UserPtr& aUser)
{
	const string& url = FavoriteManager::getInstance()->getUserUrl(aUser);
	if (!url.empty())
	{
		fly_fire1(UserManagerListener::OpenHub(), url);
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
	fly_fire1(UserManagerListener::ReservedSlotChanged(), user);
}
