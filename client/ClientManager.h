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

#ifndef DCPLUSPLUS_DCPP_CLIENT_MANAGER_H
#define DCPLUSPLUS_DCPP_CLIENT_MANAGER_H

#include "ClientListener.h"
#include "ClientManagerListener.h"
#include "OnlineUser.h"
#include "HintedUser.h"
#include "SearchParam.h"
#include "Speaker.h"
#include "Singleton.h"
#include "ConnectionStatus.h"
#include "NoCaseHash.h"

class UserCommand;

class ClientManager : public Speaker<ClientManagerListener>,
	private ClientListener, public Singleton<ClientManager>
{
	public:
		enum
		{
			PM_OK,
			PM_NO_USER,
			PM_DISABLED,
			PM_ERROR
		};

		enum
		{
			CHAT_OPTION_SHOW_IP            = 0x0001,
			CHAT_OPTION_SHOW_COUNTRY       = 0x0002,
			CHAT_OPTION_SHOW_ISP           = 0x0004,
			CHAT_OPTION_SEND_GRANT_MSG     = 0x0008,
			CHAT_OPTION_SUPPRESS_MAIN_CHAT = 0x0010,
			CHAT_OPTION_SUPPRESS_PM        = 0x0020,
			CHAT_OPTION_IGNORE_ME          = 0x0040,
			CHAT_OPTION_IGNORE_HUB_PMS     = 0x0080,
			CHAT_OPTION_IGNORE_BOT_PMS     = 0x0100,
			CHAT_OPTION_PROTECT_PRIVATE    = 0x0200,
			CHAT_OPTION_LOG_PRIVATE_CHAT   = 0x0400,
			CHAT_OPTION_LOG_SUPPRESSED     = 0x0800,
			CHAT_OPTION_LOG_MAIN_CHAT      = 0x1000,
			CHAT_OPTION_FILTER_KICK        = 0x2000
		};

		ClientBasePtr getClient(const string& hubURL);
		void putClient(const ClientBasePtr& cb);
		static void prepareClose();
		static void getOnlineUsers(const CID& cid, OnlineUserList& lst);
		static StringList getHubs(const CID& cid, const string& hintUrl);
		static StringList getHubNames(const CID& cid, const string& hintUrl);
		static StringList getNicks(const CID& cid, const string& hintUrl);
		static void getConnectedHubs(vector<ClientBasePtr>& out);
		static size_t getTotalUsers();
		static StringList getHubs(const CID& cid, const string& hintUrl, bool priv);
		static StringList getHubNames(const CID& cid, const string& hintUrl, bool priv);
		static StringList getNicks(const CID& cid, const string& hintUrl, bool priv, bool noBase32 = false);
		static string getNick(const UserPtr& user, const string& hintUrl);
		static string getNick(const HintedUser& hintedUser);
		static string getStringField(const CID& cid, const string& hintUrl, const char* field);
		static StringList getNicks(const HintedUser& user);
		static StringList getHubNames(const HintedUser& user);
		static string getOnlineHubName(const string& hubUrl);
		static bool getHubUserCommands(const string& hubUrl, vector<UserCommand>& cmd);
		static bool isConnected(const string& hubUrl);
		static bool getSlots(const CID& cid, uint16_t& slots);
		static void search(const SearchParam& sp);
		static unsigned multiSearch(const SearchParam& sp, vector<SearchClientItem>& clients);
		static void cancelSearch(uint64_t owner);
		static void infoUpdated(bool forceUpdate = false);
		static void infoUpdated(Client* client);
		static void updateSettings();
		static string getDefaultNick();
		static bool isNickEmpty();
		static int getChatOptions() { return chatOptions; }

		static UserPtr getUser(const string& nick, const string& hubUrl);
		static UserPtr createUser(const CID& cid, const string& nick, const string& hubUrl);
		static string findHub(const string& ipPort, int type);
		static int findHubEncoding(const string& url);

		/**
		* @param priv discard any user that doesn't match the hint.
		* @return OnlineUser* found by CID and hint; might be only by CID if priv is false.
		*/
		static UserPtr findUser(const string& nick, const string& hubUrl);
		static UserPtr findUser(const CID& cid);
		static UserPtr findLegacyUser(const string& nick, const string& hubUrl, string* foundHubUrl = nullptr);
		static OnlineUserPtr findOnlineUser(const CID& cid, const string& hintUrl, bool priv);
		static void findOnlineUsers(const CID& cid, OnlineUserList& res, int clientType);
		static OnlineUserPtr findDHTNode(const CID& cid);
		static bool isOnline(const UserPtr& user);

		static string findMyNick(const string& hubUrl);

		struct UserParams
		{
			int64_t bytesShared;
			int slots;
			int limit;
			Ip4Address ip4;
			Ip6Address ip6;
			std::string tag;
			std::string nick;
		};
		
		static bool getUserParams(const UserPtr& user, UserParams& params);
		
#define CREATE_LOCK_INSTANCE_CM(data, CS)\
		class LockInstance##CS \
		{\
			public:\
				LockInstance##CS() \
				{\
					ClientManager::g_cs##CS->acquireShared();\
				}\
				~LockInstance##CS()\
				{\
					ClientManager::g_cs##CS->releaseShared();\
				}\
				const auto& getData() const\
				{\
					return ClientManager::data;\
				}\
		}
		CREATE_LOCK_INSTANCE_CM(g_clients, Clients);
		CREATE_LOCK_INSTANCE_CM(g_onlineUsers, OnlineUsers);
#undef CREATE_LOCK_INSTANCE_CM

		static void setUserIP(const UserPtr& user, const IpAddress& ip);

		static StringList getNicksByIp(const IpAddress& ip);
		static OnlineUserPtr getOnlineUserL(const UserPtr& p);
		static bool isOp(const string& hubUrl);
		/** Constructs a synthetic, hopefully unique CID */
		static CID makeCid(const string& nick, const string& hubUrl);
		
		void putOnline(const OnlineUserPtr& ou, bool fireFlag) noexcept;
		void putOffline(const OnlineUserPtr& ou, bool disconnectFlag = false) noexcept;
		static void removeOnlineUser(const OnlineUserPtr& ou) noexcept;

		static void getOnlineClients(StringSet& onlineClients) noexcept;
		static void getClientStatus(boost::unordered_map<string, ConnectionStatus::Status>& result) noexcept;

		static bool searchSpyEnabled;

	private:
		static void getUserCommandParams(const OnlineUserPtr& ou, const UserCommand& uc, StringMap& params, bool compatibility);

		static OnlineUserPtr findOnlineUserL(const HintedUser& user, bool priv);
		static OnlineUserPtr findOnlineUserL(const CID& cid, const string& hintUrl, bool priv);

	public:
		static bool sendAdcCommand(AdcCommand& c, const CID& to, const IpAddress& udpAddr, uint16_t udpPort, const void* sudpKey);
		static void resendMyInfo();
		OnlineUserPtr connect(const HintedUser& user, const string& token, bool forcePassive);
		static int privateMessage(const HintedUser& user, const string& msg, int flags);
		static void userCommand(const HintedUser& user, const UserCommand& uc, StringMap& params, bool compatibility);
		
		static bool isActiveMode(int af, int favHubMode, bool udp);
		static const CID& getMyCID();
		static const CID& getMyPID();
		static void setMyPID(const string& pid);

		static void setSupports(const UserPtr& user, uint8_t knownUcSupports);
		static void setUnknownCommand(const UserPtr& user, const string& unknownCommand);
		static void dumpUserInfo(const HintedUser& user);

		static void shutdown();
		static void beforeShutdown();
		static void clear();
		static bool isShutdown()
		{
			extern volatile bool g_isShutdown;
			return g_isShutdown;
		}
		static bool isBeforeShutdown()
		{
			extern volatile bool g_isBeforeShutdown;
			return g_isBeforeShutdown;
		}
		static bool isStartup()
		{
			extern bool g_isStartupProcess;
			return g_isStartupProcess;
		}
		static void stopStartup()
		{
			extern bool g_isStartupProcess;
			g_isStartupProcess = false;
		}
#ifdef BL_FEATURE_IP_DATABASE
		static void flushRatio();
#endif
		static void usersCleanup();
	
		void updateUser(const OnlineUserPtr& ou);

	private:	
		typedef std::unordered_map<string, ClientBasePtr, NoCaseStringHash, NoCaseStringEq> ClientMap;
		static ClientMap g_clients;
		static std::unique_ptr<RWLock> g_csClients;
		
		typedef boost::unordered_map<CID, UserPtr> UserMap;
		
		static UserMap g_users;
		
		static std::unique_ptr<RWLock> g_csUsers;
		typedef std::multimap<CID, OnlineUserPtr> OnlineMap;
		typedef OnlineMap::iterator OnlineIter;
		typedef OnlineMap::const_iterator OnlineIterC;
		typedef pair<OnlineIter, OnlineIter> OnlinePair;
		typedef pair<OnlineIterC, OnlineIterC> OnlinePairC;
		
		static OnlineMap g_onlineUsers;
		static std::unique_ptr<RWLock> g_csOnlineUsers;
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
		static OnlineUserList g_UserUpdateQueue;
		static std::unique_ptr<RWLock> g_csOnlineUsersUpdateQueue;
		void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
#endif

		void addAsyncOnlineUserUpdated(const OnlineUserPtr& ou);
		static CID cid;
		static CID pid;

		static std::atomic_int chatOptions;

		friend class Singleton<ClientManager>;
		friend class NmdcHub;
		
		ClientManager();
		~ClientManager();
		
		static void updateNick(const OnlineUserPtr& ou);
		
		static OnlineUserPtr findOnlineUserHintL(const CID& cid, const string& hintUrl)
		{
			OnlinePairC p;
			return findOnlineUserHintL(cid, hintUrl, p);
		}

		/**
		* @param p OnlinePair of all the users found by CID, even those who don't match the hint.
		* @return OnlineUserPtr found by CID and hint; discard any user that doesn't match the hint.
		*/
		static OnlineUserPtr findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p);
		
		void fireIncomingSearch(int protocol, const string& seeker, const string& hub, const string& filter, ClientManagerListener::SearchReply reply);

		// ClientListener
		void on(Connecting, const Client* c) noexcept override;
		void on(Connected, const Client* c) noexcept override;
		void on(UserUpdated, const OnlineUserPtr& user) noexcept override;
		void on(UserListUpdated, const ClientBase* c, const OnlineUserList&) noexcept override;
		void on(ClientFailed, const Client*, const string&) noexcept override;
		void on(HubUpdated, const Client* c) noexcept override;
		void on(AdcSearch, const Client* c, const AdcCommand& adc, const OnlineUserPtr& ou) noexcept override;
};

#endif // !defined(CLIENT_MANAGER_H)
