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

#include "typedefs.h"
#include "forward.h"

class UserInfoBase
{
	public:
		UserInfoBase() { }
		virtual ~UserInfoBase() { }

		UserInfoBase(const UserInfoBase&) = delete;
		UserInfoBase& operator= (const UserInfoBase&) = delete;
		
		void getList();
		void browseList();
		
		void getUserResponses();
		void matchQueue();

		void doReport(const string& hubHint);

		void pm(const string& hubHint);
		void pmText(const string& hubHint, const tstring& message);

		void grantSlotPeriod(const string& hubHint, const uint64_t period);
		void ungrantSlot(const string& hubHint);
		void addFav();
		void delFav();
		void setUploadLimit(const int limit);
		void setIgnorePM();
		void setFreePM();
		void setNormalPM();
		void ignoreOrUnignoreUserByName();
		void removeAll();
		void connect(const string& hubHint);
		
		virtual const UserPtr& getUser() const = 0;
		bool isMe() const;
};

#endif // USERINFOBASE_H
