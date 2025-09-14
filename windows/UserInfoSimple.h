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

#ifndef USERINFOSIMPLE_H
#define USERINFOSIMPLE_H

#include "UserInfo.h"
#include "../client/OnlineUserParams.h"

class UserInfoSimple: public UserInfoBase
{
	public:
		explicit UserInfoSimple(const HintedUser& hintedUser) : hintedUser(hintedUser)
		{
		}
		explicit UserInfoSimple(const UserPtr& user, const string& hubHint) : hintedUser(user, hubHint)
		{
		}
		explicit UserInfoSimple(const OnlineUserPtr& user, const string& hubHint) : hintedUser(user->getUser(), hubHint)
		{
			hintedUser.user->setLastNick(user->getIdentity().getNick());
		}

		static tstring getBroadcastPrivateMessage();
		static uint64_t inputSlotTime();
		static tstring getTagIP(const string& tag, Ip4Address ip4, const Ip6Address& ip6);
		static tstring getTagIP(const OnlineUserParams& params);

		const UserPtr& getUser() const override { return hintedUser.user; }
		const string& getHubHint() const override { return hintedUser.hint; }

		const HintedUser hintedUser;
};

#endif
