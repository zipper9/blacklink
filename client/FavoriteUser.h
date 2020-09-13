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

#ifndef DCPLUSPLUS_DCPP_FAVORITE_USER_H
#define DCPLUSPLUS_DCPP_FAVORITE_USER_H

#include "User.h"
#include "ResourceManager.h"

class FavoriteUser : public Flags
{
	public:
		enum
		{
			UL_SU  = -2,
			UL_BAN = -1,
			UL_NONE = 0
		};
		
		FavoriteUser(const UserPtr& user, const string& nick, const string& hubUrl) : user(user), nick(nick), url(hubUrl), uploadLimit(UL_NONE), lastSeen(0) { }
		explicit FavoriteUser(const UserPtr& user) : user(user), uploadLimit(UL_NONE), lastSeen(0) {}		
		FavoriteUser() : uploadLimit(UL_NONE), lastSeen(0) { }
		
		enum Flags
		{
			FLAG_NONE           = 0,
			FLAG_GRANT_SLOT     = 1,
			FLAG_IGNORE_PRIVATE = 2,
			FLAG_FREE_PM_ACCESS = 4
		};
		
		static string getSpeedLimitText(int lim)
		{
			switch (lim)
			{
				case UL_SU:
					return STRING(SPEED_SUPER_USER);
				case UL_BAN:
					return "BAN";
			}
			if (lim > 0)
				return Util::formatBytes(int64_t(lim) << 10) + '/' + STRING(S);
				
			return Util::emptyString;
		}

		void update(const OnlineUser& info);
		
		UserPtr user;
		string nick;
		string url;
		time_t lastSeen;
		string description;
		int uploadLimit;
};

#endif // !defined(FAVORITE_USER_H)
