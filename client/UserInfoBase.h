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

#ifndef USERINFOBASE_H
#define USERINFOBASE_H

#include "HintedUser.h"

class UserInfoBase
{
	public:
		UserInfoBase() {}
		virtual ~UserInfoBase() {}

		UserInfoBase(const UserInfoBase&) = delete;
		UserInfoBase& operator= (const UserInfoBase&) = delete;

		void getList();
		void browseList();

		void getUserResponses();
		void matchQueue();

		void doReport();

		void pm();
		void pmText(const tstring& message);

		void grantSlotPeriod(uint64_t period);
		void ungrantSlot();
		void addFav();
		void delFav();
		void setUploadLimit(int limit);
		void setIgnorePM();
		void setFreePM();
		void setNormalPM();
		void ignoreOrUnignoreUserByName();
		void removeAll();
		void connect();

		virtual const UserPtr& getUser() const = 0;
		virtual const string& getHubHint() const = 0;

		bool isMe() const;
		HintedUser getHintedUser() const { return HintedUser(getUser(), getHubHint()); }
};

#endif // USERINFOBASE_H
