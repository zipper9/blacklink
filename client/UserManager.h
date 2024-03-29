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

#ifndef DCPLUSPLUS_DCPP_USER_MANAGER_H
#define DCPLUSPLUS_DCPP_USER_MANAGER_H

#include "UserManagerListener.h"
#include "User.h"
#include "Singleton.h"
#include "Speaker.h"
#include <atomic>
#include <regex>

class ChatMessage;

class UserManager : public Singleton<UserManager>, public Speaker<UserManagerListener>
{
	public:
		enum IgnoreType
		{
			IGNORE_NICK,
			IGNORE_WILDCARD
		};

		struct IgnoreListItem
		{
			string data;
			IgnoreType type;
		};

		typedef boost::unordered_set<string> IgnoreSet;

		void outgoingPrivateMessage(const UserPtr& user, const string& hubHint, const tstring& message) noexcept
		{
			fire(UserManagerListener::OutgoingPrivateMessage(), user, hubHint, message);
		}
		void openUserUrl(const string& hub, const UserPtr& user) noexcept;

		enum PasswordStatus
		{
			FIRST,
			GRANTED,
			CHECKED,
			WAITING
		};

		void setPMOpen(const UserPtr& user, bool flag);
		bool checkPMOpen(const ChatMessage& pm, UserManager::PasswordStatus& passwordStatus);
		bool checkOutgoingPM(const UserPtr& user, bool automatic);
		void addPMPassword(const UserPtr& user, const string& password);

		void getIgnoreList(vector<IgnoreListItem>& ignoreList) const noexcept;
		string getIgnoreListAsString() const noexcept;
		bool addToIgnoreList(const IgnoreListItem& item);
		void removeFromIgnoreList(const IgnoreListItem& item);
		void removeFromIgnoreList(const vector<string>& items);
		void removeFromIgnoreList(const vector<IgnoreListItem>& items);
		bool isInIgnoreList(const string& nick, int* type = nullptr) const noexcept;
		void clearIgnoreList();
		void fireReservedSlotChanged(const UserPtr& user) noexcept;

	private:
		void loadIgnoreList();
		void saveIgnoreList();

		struct ParsedIgnoreListItem
		{
			IgnoreType type;
			std::regex re;
		};

		IgnoreSet ignoredNicks;
		boost::unordered_map<string, ParsedIgnoreListItem> ignoredExt;
		std::atomic_bool ignoreListEmpty;
		std::unique_ptr<RWLock> csIgnoreList;

		enum
		{
			FLAG_OPEN            = 1,
			FLAG_PW_ACTIVITY     = 2,
			FLAG_GRANTED         = 4,
			FLAG_SENDING_REQUEST = 8
		};

		struct PMInfo
		{
			int flags;
			string password;
		};

		boost::unordered_map<UserPtr, PMInfo> pmInfo;
		mutable FastCriticalSection csPM;

		friend class Singleton<UserManager>;
		UserManager();
};

#endif // !defined(DCPLUSPLUS_DCPP_USER_MANAGER_H)
