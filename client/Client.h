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
		enum
		{
			TYPE_NMDC = 1,
			TYPE_ADC,
			TYPE_DHT
		};

		ClientBase() {}
		virtual ~ClientBase() {}

		ClientBase(const ClientBase&) = delete;
		ClientBase& operator= (const ClientBase&) = delete;
		
	public:
		virtual bool resendMyINFO(bool alwaysSend, bool forcePassive) = 0;
		virtual const string& getHubUrl() const = 0;
		virtual string getHubName() const = 0;
		virtual string getMyNick() const = 0;
		virtual bool isOp() const = 0;
		virtual void connect(const OnlineUserPtr& user, const string& token, bool forcePassive) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false) = 0;
		virtual int getType() const = 0;
		virtual void dumpUserInfo(const string& userReport) = 0;
};

/** Yes, this should probably be called a Hub */
class Client : public ClientBase,
               public Speaker<ClientListener>,
               public BufferedSocketListener,
               protected TimerManagerListener,
               public std::enable_shared_from_this<Client>
{
	protected:
		void fireUserListUpdated(const OnlineUserList& userList);
		void fireUserUpdated(const OnlineUserPtr& user);
		void decBytesShared(int64_t bytes);
		void changeBytesShared(Identity& id, int64_t bytes);

		static const int64_t averageFakeFileSize = 1536 * 1024;

	public:
		virtual ~Client();
		int64_t getBytesShared() const
		{
			return bytesShared.load();
		}

	public:
		bool isActive() const;
		virtual void connect();
		virtual void disconnect(bool graceless);
		virtual void hubMessage(const string& aMessage, bool thirdPerson = false) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& message, bool thirdPerson = false) = 0;
		virtual void sendUserCmd(const UserCommand& command, const StringMap& params) = 0;
		
		unsigned searchInternal(const SearchParamToken& sp);
		void cancelSearch(void* aOwner)
		{
			searchQueue.cancelSearch(aOwner);
		}
		virtual void password(const string& pwd, bool setPassword) = 0;
		virtual void info(bool forceUpdate) = 0;
		
		virtual size_t getUserCount() const = 0;
		
		virtual bool send(const AdcCommand& command) = 0;
		
		virtual string escape(const string& str) const noexcept = 0;
		virtual void checkNick(string& nick) const noexcept = 0;
		virtual bool convertNick(string& nick, bool& suffixAppended) const noexcept
		{
			suffixAppended = false;
			return true;
		}

		void connectIfNetworkOk();

		bool isConnected() const
		{
			LOCK(csState);
			return state != STATE_DISCONNECTED;
		}

		bool isReady() const
		{
			LOCK(csState);
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
			return Util::printIpAddress(ip);
		}
		const string& getAddress() const
		{
			return address;
		}
		const IpAddress& getIp() const
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
		void getLocalIp(Ip4Address& ip4, Ip6Address& ip6) const;
		
		static int getTotalCounts()
		{
			return g_counts[COUNT_NORMAL] + g_counts[COUNT_REGISTERED] + g_counts[COUNT_OP];
		}
		
		static void getCounts(unsigned& normal, unsigned& registered, unsigned& op);
		void getFakeCounts(unsigned& normal, unsigned& registered, unsigned& op) const;
		void setRawCommands(const string commands[]);
		const string& getRawCommand(int command) const;

		void escapeParams(StringMap& sm) const;
		void setSearchInterval(unsigned interval);
		void setSearchIntervalPassive(unsigned interval);
		
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
			fire(ClientListener::CheatMessage(), msg);
		}
		
		void dumpUserInfo(const string& userReport)
		{
			fire(ClientListener::UserReport(), this, userReport);
		}
		void reconnect();
		void shutdown() noexcept;
		bool getExcludeCheck() const
		{
			return exclChecks;
		}
		void send(const string& message)
		{
			send(message.c_str(), message.length());
		}
		void send(const char* message, size_t len);
		string getHubName() const
		{
			string ni = getHubIdentity().getNick();
			return ni.empty() ? getHubUrl() : ni;
		}
		string getHubDescription() const
		{
			return getHubIdentity().getDescription();
		}
		const string& getHubUrl() const
		{
			return hubURL;
		}
		string fromUtf8(const string& str) const
		{
			return Text::fromUtf8(str, encoding);
		}
		string toUtf8(const string& str) const
		{
			return Text::toUtf8(str, encoding);
		}

	protected:
		bool isPrivateMessageAllowed(const ChatMessage& message, string* response);
		bool isChatMessageAllowed(const ChatMessage& message, const string& nick) const;
		void logPM(const ChatMessage& message) const;
		void processIncomingPM(std::unique_ptr<ChatMessage>& message);

	private:
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
		string rawCommands[5];

	public:
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
		
		const string getCurrentDescription() const
		{
			return getMyIdentity().getDescription();
		}
		void setCurrentDescription(const string& descr)
		{
			getMyIdentity().setDescription(descr);
		}
		GETSET(string, name, Name)
		GETSET(string, favIp, FavIp);
		GETSET(int, favMode, FavMode);
		GETSET(uint32_t, reconnDelay, ReconnDelay);
		GETSET(bool, suppressChatAndPM, SuppressChatAndPM);
		
		bool isUserListLoaded() const { return userListLoaded; }

		GETSET(int, encoding, Encoding);

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

		void getStoredLoginParams(string& nick, string& pwd) const
		{
			LOCK(csState);
			nick = myNick;
			pwd = storedPassword;
		}

		virtual string getMyNick() const override
		{
			LOCK(csState);
			return myNick;
		}

		void setMyNick(const string& nick, bool setRandomNick)
		{
			csState.lock();
			myNick = nick;
			if (setRandomNick) randomTempNick = nick;
			csState.unlock();
			getMyIdentity().setNick(nick);
		}

		bool hasRandomTempNick() const
		{
			LOCK(csState);
			return !randomTempNick.empty();
		}

		string getRandomTempNick() const
		{
			LOCK(csState);
			return randomTempNick;
		}

		bool getHideShare() const { return hideShare; }
		const CID& getShareGroup() const { return shareGroup; }
		void getUserCommands(vector<UserCommand>& result) const;

		std::shared_ptr<Client> getClientPtr()
		{
			return shared_from_this();
		}
		void clearDefaultUsers()
		{
			myOnlineUser.reset();
			hubOnlineUser.reset();
		}
		void initDefaultUsers();

	private:
		bool overrideId;
		string clientName;
		string clientVersion;

	protected:
		void setClientId(bool overrideId, const string& name, const string& version);
		const string& getClientName() const { return clientName; }
		const string& getClientVersion() const { return clientVersion; }
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		virtual bool slotsReported() const = 0;
#endif

		friend class ClientManager;
		friend class User;

		Client(const string& hubURL, const string& address, uint16_t port, char separator, bool secure, Socket::Protocol proto);

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
		mutable FastCriticalSection csState;
		bool connSuccess;
		string storedPassword;
		string randomTempNick;
		string myNick;
		uint64_t pendingUpdate;
		
		std::atomic<int64_t> bytesShared;

		// set by reloadSettings
		bool fakeHubCount;
		bool hideShare;
		bool overrideSearchInterval;
		bool overrideSearchIntervalPassive;
		int64_t fakeShareSize;
		int64_t fakeShareFiles;
		int fakeClientStatus;
		CID shareGroup;
		
		void updateCounts(bool remove);
		
		void processPasswordRequest(const string& pwd);

		/** Reload details from favmanager or settings */
		void reloadSettings(bool updateNick);
		
		virtual void searchToken(const SearchParamToken& sp) = 0;
		
		// TimerManagerListener
		virtual void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
		
		// BufferedSocketListener
		virtual void onConnecting() noexcept override
		{
			fire(ClientListener::Connecting(), this);
		}
		virtual void onConnected() noexcept override;
		virtual void onDataLine(const string&) noexcept override;
		virtual void onFailed(const string&) noexcept override;
		
		const string& getOpChat() const { return opChat; }
		void setKeyPrint(const string& keyprint) { this->keyprint = keyprint; }

		void clearUserCommands(int ctx);
		void addUserCommand(const UserCommand& uc);
		void removeUserCommand(const string& name);

	private:
		const string hubURL;
		const string address;
		const uint16_t port;
		IpAddress ip;
		uint64_t lastActivity;
		
		string keyprint;
		string opChat;
		std::regex reOpChat;
		bool exclChecks;
		
		const char separator;
		const Socket::Protocol proto;

		const bool secure;
		CountType countType;

		vector<UserCommand> userCommands;
		mutable std::unique_ptr<RWLock> csUserCommands;

		static void resetSocket(BufferedSocket* bufferedSocket) noexcept;
		void updateActivityL()
		{
			lastActivity = GET_TICK();
		}

	public:
		bool isSecureConnect() const { return secure; }
		bool isInOperatorList(const string& userName) const;
		Socket::Protocol getProtocol() const { return proto; }

		static bool removeRandomSuffix(string& nick);
		static void appendRandomSuffix(string& nick);

#ifdef _DEBUG
		static std::atomic_int clientCount;
#endif
};

#endif // !defined(CLIENT_H)
