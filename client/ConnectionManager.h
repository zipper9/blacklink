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

#ifndef DCPLUSPLUS_DCPP_CONNECTION_MANAGER_H
#define DCPLUSPLUS_DCPP_CONNECTION_MANAGER_H

#include "UserConnection.h"
#include "Singleton.h"
#include "ConnectionManagerListener.h"
#include "HintedUser.h"
#include <atomic>

#define USING_IDLERS_IN_CONNECTION_MANAGER

class TokenManager
{
	public:
		enum
		{
			TYPE_DOWNLOAD,
			TYPE_UPLOAD
		};

		string makeToken(int type, uint64_t expires) noexcept;
		bool addToken(const string& token, int type, uint64_t expires) noexcept;
		bool isToken(const string& token) const noexcept;
		void removeToken(const string& token) noexcept;
		size_t getTokenCount() const noexcept;
		string getInfo() const noexcept;
		void removeExpired(uint64_t now) noexcept;

	private:
		struct TokenData
		{
			int type;
			uint64_t expires;
		};
		boost::unordered_map<string, TokenData> tokens;
		mutable FastCriticalSection cs;
};

class ConnectionQueueItem
{
	public:
		enum State
		{
			CONNECTING,                 // Recently sent request to connect
			WAITING,                    // Waiting to send request to connect
			NO_DOWNLOAD_SLOTS,          // Not needed right now
			ACTIVE                      // In one up/downmanager
		};
		
		ConnectionQueueItem(const HintedUser& hintedUser, bool download, const string& token) :
			token(token), lastAttempt(0),
			errors(0), state(WAITING), download(download), hintedUser(hintedUser)
		{
		}

		ConnectionQueueItem(const ConnectionQueueItem&) = delete;
		ConnectionQueueItem& operator= (const ConnectionQueueItem&) = delete;

		GETSET(uint64_t, lastAttempt, LastAttempt);
		GETSET(int, errors, Errors); // Number of connection errors, or -1 after a protocol error
		GETSET(State, state, State);

		const string& getConnectionQueueToken() const { return token; }
		bool isDownload() const { return download; }
		const UserPtr& getUser() const { return hintedUser.user; }
		const HintedUser& getHintedUser() const { return hintedUser; }
		
	private:
		const string token;
		const HintedUser hintedUser;
		const bool download;
};

typedef std::shared_ptr<ConnectionQueueItem> ConnectionQueueItemPtr;

class ExpectedNmdcMap
{
	public:
		struct ExpectedData
		{
			string myNick;
			string hubUrl;
			string token;
			int encoding;
			uint64_t id;
			uint64_t expires;
			bool waiting;
		};

		struct NextConnectionInfo
		{
			string nick;
			string hubUrl;
			string token;
		};

		bool add(const string& nick, const string& myNick, const string& hubUrl, const string& token, int encoding, uint64_t expires) noexcept;
		bool remove(const string& nick, ExpectedData& res, NextConnectionInfo& nci) noexcept;
		bool removeToken(const string& token, NextConnectionInfo& nci) noexcept;
		string getInfo() const noexcept;
		void removeExpired(uint64_t now, vector<NextConnectionInfo>& vnci) noexcept;

	private:
		// Nick -> <myNick, hubUrl, encoding> for expected NMDC incoming connections
		boost::unordered_multimap<string, ExpectedData> expectedConnections;
		uint64_t id = 0;
		mutable FastCriticalSection cs;
};

class ExpectedAdcMap
{
	public:
		struct ExpectedData
		{
			CID cid;
			string hubUrl;
			uint64_t expires;
		};

		void add(const string& token, const CID& cid, const string& hubUrl, uint64_t expires) noexcept;
		bool remove(const string& token, ExpectedData& res) noexcept;
		bool removeToken(const string& token) noexcept;
		string getInfo() const noexcept;
		void removeExpired(uint64_t now) noexcept;

	private:
		// token -> <CID, hubUrl> for expected ADC incoming connections
		boost::unordered_map<string, ExpectedData> expectedConnections;
		mutable FastCriticalSection cs;
};

// Comparing with a user...
inline bool operator==(const ConnectionQueueItemPtr& ptr, const UserPtr& user)
{
	return ptr->getUser() == user;
}
// With a token
inline bool operator==(const ConnectionQueueItemPtr& ptr, const string& token)
{
	return ptr->getConnectionQueueToken() == token;
}

class ConnectionManager :
	public Speaker<ConnectionManagerListener>,
	private ClientManagerListener,
	private TimerManagerListener,
	public Singleton<ConnectionManager>
{
	public:
		enum
		{
			SERVER_TYPE_TCP,
			SERVER_TYPE_SSL,
			SERVER_TYPE_AUTO_DETECT
		};

		static TokenManager tokenManager;
		bool nmdcExpect(const string& nick, const string& myNick, const string& hubUrl, const string& token, int encoding, uint64_t expires)
		{
			return expectedNmdc.add(nick, myNick, hubUrl, token, encoding, expires);
		}
		void adcExpect(const string& token, const CID& cid, const string& hubUrl, uint64_t expires)
		{
			expectedAdc.add(token, cid, hubUrl, expires);
		}
		
		void nmdcConnect(const string& address, uint16_t port, const string& myNick, const string& hubUrl, int encoding, bool secure);
		void nmdcConnect(const string& address, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& myNick, const string& hubUrl, int encoding, bool secure);
		void adcConnect(const OnlineUser& user, uint16_t port, const string& token, bool secure);
		void adcConnect(const OnlineUser& user, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& token, bool secure);
		
		void getDownloadConnection(const UserPtr& user);
		void force(const UserPtr& user);
		void setUploadLimit(const UserPtr& user, int lim);
		
		void disconnect(const UserPtr& user);
		void disconnect(const UserPtr& user, bool isDownload);
		
		void shutdown();

		void startListen(int type);
		void updateLocalIp();
		void disconnect() noexcept;
		void fireListenerStarted() noexcept;
		void fireListenerFailed(const char* type, int errorCode) noexcept;
		
		uint16_t getPort() const
		{
			return server ? server->getServerPort() : 0;
		}
		uint16_t getSecurePort() const
		{
			if (server && server->getType() == SERVER_TYPE_AUTO_DETECT)
				return server->getServerPort();
			return secureServer ? secureServer->getServerPort() : 0;
		}
		
		string getUserConnectionInfo() const;
		string getExpectedInfo() const;

		static uint16_t g_ConnToMeCount;

		void putConnection(UserConnection* conn);
		void processMyNick(UserConnection* source, const string& nick) noexcept;
		void processKey(UserConnection* source) noexcept;
		void processINF(UserConnection* source, const AdcCommand& cmd) noexcept;
		void fireUploadError(const HintedUser& hintedUser, const string& reason, const string& token) noexcept;
		void failed(UserConnection* source, const string& error, bool protocolError);

		StringList getNmdcFeatures() const;
		StringList getAdcFeatures() const;

	private:		
		class Server : public Thread
		{
			public:
				Server(int type, uint16_t port, const string& ipAddr);
				uint16_t getServerPort() const
				{
					dcassert(serverPort);
					return serverPort;
				}
				string getServerIP() const
				{
					string ip;
					uint16_t port;
					sock.getLocalIPPort(port, ip, true);
					return ip;
				}
				~Server()
				{
					stopFlag.store(true);
					join();
				}
				int getType() const { return type; }

			private:
				int run() noexcept;
				std::atomic_bool stopFlag;

				Socket sock;
				uint16_t serverPort;
				const string bindIp;
				const int type;
		};

		mutable std::unique_ptr<RWLock> csConnections;
		mutable std::unique_ptr<RWLock> csDownloads;
		mutable CriticalSection csUploads;
		
		/** All ConnectionQueueItems */
		std::set<ConnectionQueueItemPtr> downloads;
		std::set<ConnectionQueueItemPtr> uploads;
		
		/** All active connections */
		boost::unordered_set<UserConnection*> userConnections;
		
	private:
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		UserList checkIdle;
#endif
		
		StringList nmdcFeatures;
		StringList adcFeatures;
		
		FastCriticalSection csUpdatedUsers;
		UserSet updatedUsers;
		void addUpdatedUser(const UserPtr& user);
		void flushUpdatedUsers();
		
		ExpectedNmdcMap expectedNmdc;
		ExpectedAdcMap expectedAdc;
		
		uint64_t m_floodCounter;
		
		Server* server;
		Server* secureServer;
		
		bool shuttingDown;
		
		SpeedCalc<32> uploadSpeed;
		SpeedCalc<32> downloadSpeed;

		friend class Singleton<ConnectionManager>;
		ConnectionManager();
		
		~ConnectionManager();
		
		static void setIP(UserConnection* conn, const ConnectionQueueItemPtr& qi);
		UserConnection* getConnection(bool nmdc, bool secure) noexcept;
		void deleteConnection(UserConnection* conn);
		void connectNextNmdcUser(const ExpectedNmdcMap::NextConnectionInfo& nci);
		void removeExpectedToken(const string& token);
		
		void removeUnusedConnections();
#ifdef DEBUG_USER_CONNECTION
		void dumpUserConnections();
#endif
		void addUploadConnection(UserConnection* conn);
		void addDownloadConnection(UserConnection* conn);
		void updateAverageSpeed(uint64_t tick);
		
		void putCQI_L(ConnectionQueueItemPtr& cqi);
		
		void accept(const Socket& sock, int type, Server* server) noexcept;
		
		bool checkKeyprint(UserConnection *source);
		
	public:
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
		void on(TimerManagerListener::Minute, uint64_t tick) noexcept override;

		// ClientManagerListener
		void on(ClientManagerListener::UserConnected, const UserPtr& user) noexcept override;
		void on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept override;
		
		void onUserUpdated(const UserPtr& user);
};

#endif // !defined(CONNECTION_MANAGER_H)
