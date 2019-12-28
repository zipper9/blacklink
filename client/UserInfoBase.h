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

#include "UserManager.h"
#include "FavoriteManager.h"

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
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		void checkList();
#endif
		void matchQueue();
		
		void doReport(const string& hubHint);
		
		void pm(const string& hubHint);
		void pmText(const string& hubHint, const tstring& message);
		
		void createSummaryInfo(const string& p_selectedHint);
		
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
		void connectFav();
		
		virtual const UserPtr& getUser() const = 0;
		static uint8_t getImage(const OnlineUser& ou);
		static uint8_t getStateImageIndex() { return 0; }
};
#endif
