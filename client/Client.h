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

#ifndef DCPLUSPLUS_DCPP_CLIENT_H
#define DCPLUSPLUS_DCPP_CLIENT_H

#include <atomic>
#include <regex>
#include "ClientListener.h"
#include "DebugManager.h"
#include "SearchQueue.h"
#include "OnlineUser.h"
#include "BufferedSocket.h"
#include "ChatMessage.h"

class ClientBase
{
	public:
		ClientBase() {}
		virtual ~ClientBase() {}

		ClientBase(const ClientBase&) = delete;
		ClientBase& operator= (const ClientBase&) = delete;
		
	public:
		bool isActive() const;
		virtual bool resendMyINFO(bool alwaysSend, bool forcePassive) = 0;
		virtual const string getHubUrl() const = 0;
		virtual const string getHubName() const = 0;
		virtual bool isOp() const = 0;
		virtual void connect(const OnlineUser& user, const string& token, bool forcePassive) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false) = 0;
};

/** Yes, this should probably be called a Hub */
class Client : public ClientBase, public Speaker<ClientListener>, public BufferedSocketListener, protected TimerManagerListener
{
	protected:
		std::unique_ptr<webrtc::RWLockWrapper> m_cs;
		void fireUserListUpdated(const OnlineUserList& userList);
		void fireUserUpdated(const OnlineUserPtr& user);
		void decBytesShared(int64_t bytes);
		void changeBytesShared(Identity& id, int64_t bytes);
	
	public:
		int64_t getBytesShared() const
		{
			return bytesShared.load();
		}

	public:
		virtual void connect();
		virtual void disconnect(bool graceless);
		virtual void connect(const OnlineUser& user, const string& token, bool forcePassive) = 0;
		virtual void hubMessage(const string& aMessage, bool thirdPerson = false) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& message, bool thirdPerson = false) = 0;
		virtual void sendUserCmd(const UserCommand& command, const StringMap& params) = 0;
		
		uint64_t searchInternal(const SearchParamToken& sp);
		void cancelSearch(void* aOwner)
		{
			searchQueue.cancelSearch(aOwner);
		}
		virtual void password(const string& pwd) = 0;
		virtual void info(bool forceUpdate) = 0;
		
		virtual size_t getUserCount() const = 0;
		
		virtual void send(const AdcCommand& command) = 0;
		
		virtual string escape(const string& str) const noexcept = 0;
		virtual bool convertNick(string& nick, bool& suffixAppended) const noexcept
		{
			suffixAppended = false;
			return true;
		}

		void connectIfNetworkOk();

		bool isConnected() const
		{
			return state != STATE_DISCONNECTED;
		}
		
		bool isReady() const
		{
			return state != STATE_CONNECTING && state != STATE_DISCONNECTED;
		}
		
		bool isSecure() const;
		bool isTrusted() const;
		string getCipherName() const;
		vector<uint8_t> getKeyprint() const;
		
		bool isOp() const
		{
			return getMyIdentity().isOp();
		}
		
		bool isRegistered() const
		{
			return getMyIdentity().isRegistered();
		}
		
		virtual void refreshUserList(bool) = 0;
		virtual void getUserList(OnlineUserList& list) const = 0;
		virtual OnlineUserPtr findUser(const string& aNick) const = 0;
		
		uint16_t getPort() const
		{
			return port;
		}
		string getIpAsString() const
		{
			return ip.to_string();
		}
		const string& getAddress() const
		{
			return address;
		}
		boost::asio::ip::address_v4 getIp() const
		{
			return ip;
		}
		string getIpPort() const
		{
			return getIpAsString() + ':' + Util::toString(port);
		}
		string getHubUrlAndIP() const
		{
			return "[Hub: " + getHubUrl() + ", " + getIpPort() + "]";
		}
		string getLocalIp() const;
		
		static int getTotalCounts()
		{
			return g_counts[COUNT_NORMAL] + g_counts[COUNT_REGISTERED] + g_counts[COUNT_OP];
		}
		
		static void getCounts(unsigned& normal, unsigned& registered, unsigned& op);
		void getFakeCounts(unsigned& normal, unsigned& registered, unsigned& op) const;
		const string& getRawCommand(int command) const;
		bool isPrivateMessageAllowed(const ChatMessage& message);
		bool isChatMessageAllowed(const ChatMessage& message, const string& nick) const;
		
		void processingPassword();
		void escapeParams(StringMap& sm) const;
		void setSearchInterval(unsigned interval, bool fromRule);
		void setSearchIntervalPassive(unsigned interval, bool fromRule);
		
		uint32_t getSearchInterval() const
		{
			return searchQueue.interval;
		}
		uint32_t getSearchIntervalPassive() const
		{
			return searchQueue.intervalPassive;
		}
		
		void cheatMessage(const string& msg)
		{
			fly_fire1(ClientListener::CheatMessage(), msg);
		}
		
		void dumpUserInfo(const string& userReport)
		{
			fly_fire2(ClientListener::UserReport(), this, userReport);
		}
		void reconnect();
		void shutdown();
		bool getExcludeCheck() const
		{
			return exclChecks;
		}
		void send(const string& message)
		{
			send(message.c_str(), message.length());
		}
		void send(const char* message, size_t len);
		
		void setMyNick(const string& nick)
		{
			getMyIdentity().setNick(nick);
		}
		const string& getMyNick() const
		{
			return getMyIdentity().getNick();
		}
		const string getHubName() const
		{
			const string ni = getHubIdentity().getNick();
			return ni.empty() ? getHubUrl() : ni;
		}
		string getHubDescription() const
		{
			return getHubIdentity().getDescription();
		}
		virtual const string getHubUrl() const
		{
			return hubURL;
		}
		string fromUtf8(const string& str) const
		{
			return Text::fromUtf8(str, getEncoding());
		}
		string toUtf8(const string& str) const
		{
			return Text::toUtf8(str, getEncoding());
		}
		
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		uint32_t getHubID() const
		{
			return m_HubID;
		}
#endif
	private:
		uint32_t m_message_count;
		
		struct CFlyFloodCommand
		{
			std::vector<std::pair<string, int64_t>> m_flood_command;
			int64_t  m_start_tick;
			int64_t  m_tick;
			uint32_t m_count;
			bool m_is_ban;
			CFlyFloodCommand() : m_start_tick(0), m_tick(0), m_count(0), m_is_ban(false)
			{
			}
		};
		typedef boost::unordered_map<string, CFlyFloodCommand> CFlyFloodCommandMap;
		CFlyFloodCommandMap m_flood_detect;

	protected:
		bool isFloodCommand(const string& command, const string& line);
		
		OnlineUserPtr myOnlineUser;
		OnlineUserPtr hubOnlineUser;

		std::atomic_bool userListLoaded;

	public:
		bool isMeCheck(const OnlineUserPtr& ou) const;
		bool isMe(const OnlineUserPtr& ou) const
		{
			return ou == getMyOnlineUser();
		}
		const OnlineUserPtr& getMyOnlineUser() const
		{
			return myOnlineUser;
		}
		const Identity& getMyIdentity() const
		{
			return getMyOnlineUser()->getIdentity();
		}
		Identity& getMyIdentity()
		{
			return getMyOnlineUser()->getIdentity();
		}
		const OnlineUserPtr& getHubOnlineUser() const
		{
			return hubOnlineUser;
		}
		Identity& getHubIdentity()
		{
			return getHubOnlineUser()->getIdentity();
		}
		const Identity& getHubIdentity() const
		{
			return getHubOnlineUser()->getIdentity();
		}
		
		GETSET(string, defpassword, Password);
		const string getCurrentDescription() const
		{
			return getMyIdentity().getDescription();
		}
		void setCurrentDescription(const string& descr)
		{
			getMyIdentity().setDescription(descr);
		}
		GETSET(string, randomTempNick, RandomTempNick)
		GETSET(string, name, Name)
		GETSET(string, rawOne, RawOne);
		GETSET(string, rawTwo, RawTwo);
		GETSET(string, rawThree, RawThree);
		GETSET(string, rawFour, RawFour);
		GETSET(string, rawFive, RawFive);
		GETSET(string, favIp, FavIp);
		GETM(uint64_t, lastActivity, LastActivity);
		GETSET(uint32_t, reconnDelay, ReconnDelay);
		GETSET(bool, suppressChatAndPM, SuppressChatAndPM);
		uint32_t getMessagesCount() const
		{
			return m_message_count;
		}
		void incMessagesCount()
		{
			++m_message_count;
		}
		void clearMessagesCount()
		{
			m_message_count = 0;
		}
		
		void setClientId(bool overrideId, const string& name, const string& version);
		const string& getClientName() const { return clientName; }
		const string& getClientVersion() const { return clientVersion; }

		bool isUserListLoaded() const { return userListLoaded; }

		GETSET(string, m_encoding, Encoding);

		void setRegistered()
		{
			getMyIdentity().setRegistered(true);
		}
		void resetRegistered()
		{
			getMyIdentity().setRegistered(false);
		}
		void resetOp()
		{
			getMyIdentity().setOp(false);
		}
		
		GETSET(bool, autoReconnect, AutoReconnect);
		string getCurrentEmail() const
		{
			return getMyIdentity().getEmail();
		}
		void setCurrentEmail(const string& email)
		{
			getMyIdentity().setEmail(email);
		}

#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		bool getHideShare() const
		{
			return m_is_hide_share;
		}
		void setHideShare(bool p_is_hide_share)
		{
			m_is_hide_share = p_is_hide_share;
			if (clientSock)
				clientSock->set_is_hide_share(p_is_hide_share);
		}
	private:
		bool m_is_hide_share;
		
#endif
	private:
		bool overrideId;
		string clientName;
		string clientVersion;
		string clientVersionFull;

	protected:
		const string& getFullClientVersion() const { return clientVersionFull; }
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		virtual bool hubIsNotSupportSlot() const = 0;// [+]IRainman
#endif // IRAINMAN_ENABLE_AUTO_BAN
//[~]FlylinkDC

		friend class ClientManager;
		friend class User;
		Client(const string& hubURL, char separator, bool secure, Socket::Protocol proto);
		virtual ~Client();
		
		enum CountType
		{
			COUNT_NORMAL,
			COUNT_REGISTERED,
			COUNT_OP,
			COUNT_UNCOUNTED,
		};
		
		static std::atomic<uint32_t> g_counts[COUNT_UNCOUNTED];
		
		enum States
		{
			STATE_CONNECTING,    // Waiting for socket to connect
			STATE_PROTOCOL,      // Protocol setup
			STATE_IDENTIFY,      // Nick setup
			STATE_VERIFY,        // Checking password
			STATE_NORMAL,        // Running
			STATE_DISCONNECTED,  // Idle
			STATE_WAIT_PORT_TEST // Waiting for port test to complete
		} state;
		
		SearchQueue searchQueue;
		BufferedSocket* clientSock;
		void resetSocket() noexcept;
		
		std::atomic<int64_t> bytesShared;
		
		void updateCounts(bool remove);
		void updateActivity()
		{
			lastActivity = GET_TICK();
		}
		
		/** Reload details from favmanager or settings */
		const FavoriteHubEntry* reloadSettings(bool updateNick);
		
		virtual void checkNick(string& nick) = 0;
		virtual void searchToken(const SearchParamToken& sp) = 0;
		
		// TimerManagerListener
		virtual void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
		virtual void on(TimerManagerListener::Minute, uint64_t aTick) noexcept override;
		
		// BufferedSocketListener
		virtual void onConnecting() noexcept override
		{
			fly_fire1(ClientListener::Connecting(), this);
		}
		virtual void onConnected() noexcept override;
		virtual void onDataLine(const string&) noexcept override;
		virtual void onFailed(const string&) noexcept override;
		
		const string& getOpChat() const { return opChat; }

	private:
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		uint32_t m_HubID;
#endif
		const string hubURL;
		string address;
		boost::asio::ip::address_v4 ip;
		uint16_t port;
		
		string keyprint;
		string opChat;
		std::regex reOpChat;
		bool exclChecks;
		
		const char separator;
		Socket::Protocol proto;

		const bool secure;
		CountType countType;

	public:
		bool isSecureConnect() const { return secure; }
		bool isInOperatorList(const string& userName) const;
		Socket::Protocol getProtocol() const { return proto; }

		static bool removeRandomSuffix(string& nick);
		static void appendRandomSuffix(string& nick);
};

#endif // !defined(CLIENT_H)
