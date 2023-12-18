
/*
 * ApexDC speedmod (c) SMT 2007
 */

#include "stdinc.h"
#include "UserInfoBase.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "UploadManager.h"
#include "UserManager.h"
#include "FavoriteManager.h"
#include "ClientManager.h"
#include "LogManager.h"

void UserInfoBase::matchQueue()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addList(getUser(), 0, QueueItem::XFLAG_MATCH_QUEUE);
		}
		catch (const Exception& e)
		{
			LogManager::message(e.getError());
		}
	}
}

void UserInfoBase::getUserResponses()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->userCheckStart(getUser());
		}
		catch (const Exception& e)
		{
			LogManager::message(e.getError());
		}
	}
}

void UserInfoBase::doReport(const string& hubHint)
{
	if (getUser())
		ClientManager::dumpUserInfo(HintedUser(getUser(), hubHint));
}

void UserInfoBase::getList()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addList(getUser(), 0, QueueItem::XFLAG_CLIENT_VIEW);
		}
		catch (const Exception& e)
		{
			LogManager::message(e.getError());
		}
	}
}

void UserInfoBase::browseList()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addList(getUser(), QueueItem::FLAG_PARTIAL_LIST, QueueItem::XFLAG_CLIENT_VIEW);
		}
		catch (const Exception& e)
		{
			dcassert(0);
			LogManager::message(e.getError());
		}
	}
}

void UserInfoBase::addFav()
{
	if (getUser())
		FavoriteManager::getInstance()->addFavoriteUser(getUser());
}

void UserInfoBase::setIgnorePM()
{
	if (getUser())
		FavoriteManager::getInstance()->setFlags(getUser(), FavoriteUser::FLAG_IGNORE_PRIVATE, FavoriteUser::PM_FLAGS_MASK);
}

void UserInfoBase::setFreePM()
{
	if (getUser())
		FavoriteManager::getInstance()->setFlags(getUser(), FavoriteUser::FLAG_FREE_PM_ACCESS, FavoriteUser::PM_FLAGS_MASK);
}

void UserInfoBase::setNormalPM()
{
	if (getUser())
		FavoriteManager::getInstance()->setFlags(getUser(), FavoriteUser::FLAG_NONE, FavoriteUser::PM_FLAGS_MASK);
}

void UserInfoBase::setUploadLimit(const int limit)
{
	if (getUser())
		FavoriteManager::getInstance()->setUploadLimit(getUser(), limit);
}

void UserInfoBase::delFav()
{
	if (getUser())
		FavoriteManager::getInstance()->removeFavoriteUser(getUser());
}

void UserInfoBase::ignoreOrUnignoreUserByName()
{
	if (getUser())
	{
		UserManager::IgnoreListItem item;
		item.data = getUser()->getLastNick();
		item.type = UserManager::IGNORE_NICK;
		UserManager* userManager = UserManager::getInstance();
		int type;
		if (userManager->isInIgnoreList(item.data, &type))
		{
			if (type == UserManager::IGNORE_NICK)
				userManager->removeFromIgnoreList(item);
		}
		else
			userManager->addToIgnoreList(item);
	}
}

void UserInfoBase::pm(const string& hubHint)
{
	if (getUser()
#ifndef _DEBUG
	&& !getUser()->isMe()
#endif
	)
	{
		UserManager::getInstance()->outgoingPrivateMessage(getUser(), hubHint, Util::emptyStringT);
	}
}

void UserInfoBase::pmText(const string& hubHint, const tstring& message)
{
	if (!message.empty())
		UserManager::getInstance()->outgoingPrivateMessage(getUser(), hubHint, message);
}

void UserInfoBase::removeAll()
{
	if (getUser())
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
}

void UserInfoBase::connect(const string& hubHint)
{
	if (getUser() && !hubHint.empty())
		UserManager::getInstance()->openUserUrl(hubHint, getUser());
}

void UserInfoBase::grantSlotPeriod(const string& hubHint, const uint64_t period)
{
	if (period && getUser())
		UploadManager::getInstance()->reserveSlot(HintedUser(getUser(), hubHint), period);
}

void UserInfoBase::ungrantSlot(const string& hubHint)
{
	if (getUser())
		UploadManager::getInstance()->unreserveSlot(HintedUser(getUser(), hubHint));
}

bool UserInfoBase::isMe() const
{
	const UserPtr& user = getUser();
	if (!user) return false;
	return user->isMe();
}
