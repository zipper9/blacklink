#include "stdafx.h"
#include "UserTypeColors.h"
#include "../client/SettingsManager.h"
#include "../client/UserManager.h"

COLORREF UserTypeColors::getColor(unsigned short& flags, const OnlineUserPtr& onlineUser)
{
	const UserPtr& user = onlineUser->getUser();
	dcassert(user);
	const auto userFlags = user->getFlags();
	const auto statusFlags = onlineUser->getIdentity().getStatus();
	if ((flags & IS_IGNORED_USER) == IS_IGNORED_USER)
	{
		flags &= ~IS_IGNORED_USER;
		if (UserManager::getInstance()->isInIgnoreList(onlineUser->getIdentity().getNick()))
			flags |= IS_IGNORED_USER_ON;
	}
	if ((flags & IS_RESERVED_SLOT) == IS_RESERVED_SLOT)
	{
		flags &= ~IS_RESERVED_SLOT;
		if (userFlags & User::RESERVED_SLOT)
			flags |= IS_RESERVED_SLOT_ON;
	}
	if ((flags & IS_FAVORITE) == IS_FAVORITE || (flags & IS_BAN) == IS_BAN)
	{
		flags &= ~(IS_FAVORITE | IS_BAN);
		if (userFlags & User::FAVORITE)
		{
			flags |= IS_FAVORITE_ON;
			if (userFlags & User::BANNED)
				flags |= IS_BAN_ON;
		}
	}

	if (flags & IS_RESERVED_SLOT)
		return SETTING(RESERVED_SLOT_COLOR);
	if (flags & IS_FAVORITE_ON)
		return (flags & IS_BAN_ON) ? SETTING(FAV_BANNED_COLOR) : SETTING(FAVORITE_COLOR);
	if (onlineUser->getIdentity().isOp())
		return SETTING(OP_COLOR);
	if (flags & IS_IGNORED_USER)
		return SETTING(IGNORED_COLOR);
	if (BOOLSETTING(SHOW_CHECKED_USERS) && user->getLastCheckTime())
	{
		if (userFlags & User::USER_CHECK_FAILED)
			return SETTING(CHECKED_FAIL_COLOR);
		if (!(userFlags & User::USER_CHECK_RUNNING))
			return SETTING(CHECKED_COLOR);
	}
	if (statusFlags & Identity::SF_FIREBALL)
		return SETTING(FIREBALL_COLOR);
	if (statusFlags & Identity::SF_SERVER)
		return SETTING(SERVER_COLOR);
	if (statusFlags & Identity::SF_PASSIVE)
		return SETTING(PASSIVE_COLOR);
	return SETTING(NORMAL_COLOR);
}

COLORREF UserTypeColors::getColor(unsigned short& flags, const UserPtr& user)
{
	dcassert(user);
	const auto userFlags = user->getFlags();
	if ((flags & IS_IGNORED_USER) == IS_IGNORED_USER)
	{
		flags &= ~IS_IGNORED_USER;
		if (UserManager::getInstance()->isInIgnoreList(user->getLastNick()))
			flags |= IS_IGNORED_USER_ON;
	}
	if ((flags & IS_RESERVED_SLOT) == IS_RESERVED_SLOT)
	{
		flags &= ~IS_RESERVED_SLOT;
		if (userFlags & User::RESERVED_SLOT)
			flags |= IS_RESERVED_SLOT_ON;
	}
	if ((flags & IS_FAVORITE) == IS_FAVORITE || (flags & IS_BAN) == IS_BAN)
	{
		flags &= ~(IS_FAVORITE | IS_BAN);
		if (userFlags & User::FAVORITE)
		{
			flags |= IS_FAVORITE_ON;
			if (userFlags & User::BANNED)
				flags |= IS_BAN_ON;
		}
	}

	if (flags & IS_RESERVED_SLOT)
		return SETTING(RESERVED_SLOT_COLOR);
	if (flags & IS_FAVORITE_ON)
		return (flags & IS_BAN_ON) ? SETTING(FAV_BANNED_COLOR) : SETTING(FAVORITE_COLOR);
	if (flags & IS_IGNORED_USER)
		return SETTING(IGNORED_COLOR);
	return SETTING(NORMAL_COLOR);
}
