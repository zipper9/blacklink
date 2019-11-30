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
		
		virtual void on(OutgoingPrivateMessage, const UserPtr&, const string&, const tstring&) noexcept { }
		virtual void on(OpenHub, const string&) noexcept { }
		virtual void on(CollectSummaryInfo, const UserPtr&, const string& hubHint) noexcept { }
		virtual void on(IgnoreListChanged) noexcept { }
};

class UserManager : public Singleton<UserManager>, public Speaker<UserManagerListener>
{
	public:
		void outgoingPrivateMessage(const UserPtr& user, const string& hubHint, const tstring& message)
		{
			fly_fire3(UserManagerListener::OutgoingPrivateMessage(), user, hubHint, message);
		}
		void openUserUrl(const UserPtr& user);
		void collectSummaryInfo(const UserPtr user, const string hubHint)
		{
			fly_fire2(UserManagerListener::CollectSummaryInfo(), user, hubHint);
		}
		enum PasswordStatus
		{
			FIRST = -1,
			FREE = 0,
			CHECKED = 1,
			WAITING = 2,
		};
		static bool expectPasswordFromUser(const UserPtr& user);
		static PasswordStatus checkPrivateMessagePassword(const ChatMessage& pm); // !SMT!-PSW
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		void checkUser(const OnlineUserPtr& user) const;
#endif
		typedef StringSet IgnoreMap;
		
		void getIgnoreList(StringSet& ignoreList) const;
		tstring getIgnoreListAsString() const;
		bool addToIgnoreList(const string& userName);
		void removeFromIgnoreList(const string& userName);
		void removeFromIgnoreList(const vector<string>& userNames);
		bool isInIgnoreList(const string& nick) const;
		void setIgnoreList(IgnoreMap& newlist);
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		void reloadProtectedUsers();
		bool isInProtectedUserList(const string& userName) const;
#endif
		
	private:
		void loadIgnoreList();
		void saveIgnoreList();
		IgnoreMap ignoreList;
		std::atomic_bool ignoreListEmpty;
		std::unique_ptr<webrtc::RWLockWrapper> csIgnoreList;
	
		typedef boost::unordered_set<UserPtr, User::Hash> CheckedUserSet;
		typedef boost::unordered_map<UserPtr, bool, User::Hash> WaitingUserMap;
		
		static CheckedUserSet checkedPasswordUsers;
		static WaitingUserMap waitingPasswordUsers;
		
		static FastCriticalSection g_csPsw;
		
		friend class Singleton<UserManager>;
		UserManager();
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool hasProtectedUsers;
		std::regex reProtectedUsers;
		std::unique_ptr<webrtc::RWLockWrapper> csProtectedUsers;
#endif
};

#endif // !defined(DCPLUSPLUS_DCPP_USER_MANAGER_H)
