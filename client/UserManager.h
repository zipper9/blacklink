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

#include "SettingsManager.h"
#include "User.h"
#include <atomic>
#include <regex>

class ChatMessage;

class UserManagerListener
{
	public:
		virtual ~UserManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> OutgoingPrivateMessage;
		typedef X<1> OpenHub;
		typedef X<2> CollectSummaryInfo;
		typedef X<3> IgnoreListChanged;
		typedef X<4> IgnoreListCleared;
		typedef X<5> ReservedSlotChanged;
		
		virtual void on(OutgoingPrivateMessage, const UserPtr&, const string&, const tstring&) noexcept { }
		virtual void on(OpenHub, const string&, const UserPtr&) noexcept { }
		virtual void on(CollectSummaryInfo, const UserPtr&, const string& hubHint) noexcept { }
		virtual void on(IgnoreListChanged) noexcept { }
		virtual void on(IgnoreListCleared) noexcept { }
		virtual void on(ReservedSlotChanged, const UserPtr&) noexcept { }
};

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
		void collectSummaryInfo(const UserPtr& user, const string& hubHint) noexcept
		{
			fire(UserManagerListener::CollectSummaryInfo(), user, hubHint);
		}

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

#ifdef IRAINMAN_INCLUDE_USER_CHECK
		void checkUser(const OnlineUserPtr& user) const;
#endif

		void getIgnoreList(vector<IgnoreListItem>& ignoreList) const noexcept;
		string getIgnoreListAsString() const noexcept;
		bool addToIgnoreList(const IgnoreListItem& item);
		void removeFromIgnoreList(const IgnoreListItem& item);
		void removeFromIgnoreList(const vector<string>& items);
		void removeFromIgnoreList(const vector<IgnoreListItem>& items);
		bool isInIgnoreList(const string& nick, int* type = nullptr) const noexcept;
		void clearIgnoreList();
		void fireReservedSlotChanged(const UserPtr& user) noexcept;

#ifdef IRAINMAN_ENABLE_AUTO_BAN
		void reloadProtectedUsers() noexcept;
		bool isInProtectedUserList(const string& userName) const noexcept;
#endif

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

		boost::unordered_map<UserPtr, PMInfo, User::Hash> pmInfo;
		mutable FastCriticalSection csPM;

		friend class Singleton<UserManager>;
		UserManager();

#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool hasProtectedUsers;
		std::regex reProtectedUsers;
		std::unique_ptr<RWLock> csProtectedUsers;
#endif
};

#endif // !defined(DCPLUSPLUS_DCPP_USER_MANAGER_H)
