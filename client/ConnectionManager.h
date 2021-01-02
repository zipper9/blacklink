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

#include <boost/asio/ip/address_v4.hpp>
#include "UserConnection.h"
#include "Singleton.h"
#include "ConnectionManagerListener.h"
#include "HintedUser.h"
#include <atomic>

#define USING_IDLERS_IN_CONNECTION_MANAGER

class TokenManager
{
	public:
		string makeToken() noexcept;
		bool addToken(const string& token) noexcept;
		bool isToken(const string& token) const noexcept;
		void removeToken(const string& token) noexcept;
		size_t getTokenCount() const noexcept;
		string toString() const noexcept;

	private:
		StringSet tokens;
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

class ExpectedMap
{
	public:
		/** Nick -> myNick, hubUrl for expected NMDC incoming connections */
		struct NickHubPair
		{
			const string nick;
			const string hubUrl;
			
			NickHubPair(const string& nick, const string& hubUrl): nick(nick), hubUrl(hubUrl) {}
		};

		void add(const string& nick, const string& myNick, const string& hubUrl)
		{
			LOCK(cs);
			expectedConnections.insert(make_pair(nick, NickHubPair(myNick, hubUrl)));
		}
		
		NickHubPair remove(const string& nick)
		{
			LOCK(cs);
			const auto i = expectedConnections.find(nick);
			if (i == expectedConnections.end())
				return NickHubPair(Util::emptyString, Util::emptyString);
				                  
			const NickHubPair tmp = i->second;
			expectedConnections.erase(i);
			return tmp;
		}

#ifdef _DEBUG
		size_t getCount() const
		{
			LOCK(cs);
			return expectedConnections.size();
		}
#endif
		
	private:
		/** Nick -> myNick, hubUrl for expected NMDC incoming connections */
		boost::unordered_map<string, NickHubPair> expectedConnections;		
		mutable FastCriticalSection cs;
};

// Comparing with a user...
inline bool operator==(const ConnectionQueueItemPtr& ptr, const UserPtr& user)
{
	return ptr->getUser() == user;
}

class ConnectionManager :
	public Speaker<ConnectionManagerListener>,
	private UserConnectionListener,
	private ClientManagerListener,
	private TimerManagerListener,
	public Singleton<ConnectionManager>
{
	public:
		static TokenManager g_tokens_manager;
		void nmdcExpect(const string& nick, const string& myNick, const string& hubUrl)
		{
			expectedConnections.add(nick, myNick, hubUrl);
		}
		
		void nmdcConnect(const string& address, uint16_t port, const string& myNick, const string& hubUrl, int encoding, bool secure);
		void nmdcConnect(const string& address, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& myNick, const string& hubUrl, int encoding, bool secure);
		void adcConnect(const OnlineUser& user, uint16_t port, const string& token, bool secure);
		void adcConnect(const OnlineUser& user, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& token, bool secure);
		
		void getDownloadConnection(const UserPtr& user);
		void force(const UserPtr& user);
		static void setUploadLimit(const UserPtr& user, int lim);
		
		static void disconnect(const UserPtr& user);
		static void disconnect(const UserPtr& user, bool isDownload);
		
		void shutdown();
		static bool isShuttingDown()
		{
			return g_shuttingDown;
		}
		
		/** Find a suitable port to listen on, and start doing it */
		void startListen();
		void disconnect();
		
		uint16_t getPort() const
		{
			return server ? server->getServerPort() : 0;
		}
		uint16_t getSecurePort() const
		{
			if (server && server->getType() == Server::TYPE_AUTO_DETECT)
				return server->getServerPort();
			return secureServer ? secureServer->getServerPort() : 0;
		}
		
		static string getUserConnectionInfo();

		static uint16_t g_ConnToMeCount;
		
	private:		
		class Server : public Thread
		{
			public:
				enum
				{
					TYPE_TCP,
					TYPE_SSL,
					TYPE_AUTO_DETECT
				};

				Server(int type, uint16_t port, const string& ipAddr = "0.0.0.0");
				uint16_t getServerPort() const
				{
					dcassert(serverPort);
					return serverPort;
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
				string serverIp;
				int type;
		};

		static std::unique_ptr<RWLock> g_csConnection;
		static std::unique_ptr<RWLock> g_csDownloads;
		//static std::unique_ptr<RWLock> g_csUploads;
		static CriticalSection g_csUploads;
		static FastCriticalSection g_csDdosCheck;
		static std::unique_ptr<RWLock> g_csDdosCTM2HUBCheck;
		static std::unique_ptr<RWLock> g_csTTHFilter;
		static std::unique_ptr<RWLock> g_csFileFilter;
		
		/** All ConnectionQueueItems */
		static std::set<ConnectionQueueItemPtr> g_downloads;
		static std::set<ConnectionQueueItemPtr> g_uploads;
		
		/** All active connections */
		static boost::unordered_set<UserConnection*> g_userConnections;
		
		struct CFlyDDOSkey
		{
			std::string m_server;
			boost::asio::ip::address_v4 m_ip;
			CFlyDDOSkey(const std::string& p_server, boost::asio::ip::address_v4 p_ip) : m_server(p_server), m_ip(p_ip)
			{
			}
			bool operator < (const CFlyDDOSkey& p_val) const
			{
				return (m_ip < p_val.m_ip) || (m_ip == p_val.m_ip && m_server < p_val.m_server);
			}
		};
		class CFlyTickDetect
		{
			public:
				uint64_t m_first_tick;
				uint64_t m_last_tick;
				uint16_t m_count_connect;
				uint16_t m_block_id;
				CFlyTickDetect(): m_first_tick(0), m_last_tick(0), m_count_connect(0), m_block_id(0)
				{
				}
		};
		class CFlyTickTTH : public CFlyTickDetect
		{
		};
		class CFlyTickFile : public CFlyTickDetect
		{
		};
		class CFlyDDoSTick : public CFlyTickDetect
		{
			public:
				std::string m_type_block;
				boost::unordered_set<uint16_t> m_ports;
				boost::unordered_map<std::string, uint32_t> m_original_query_for_debug;
				CFlyDDoSTick()
				{
				}
				string getPorts() const
				{
					string l_ports;
					string l_sep;
					for (auto i = m_ports.cbegin(); i != m_ports.cend(); ++i)
					{
						l_ports += l_sep;
						l_ports += Util::toString(*i);
						l_sep = ",";
					}
					return " Port: " + l_ports;
				}
		};

		static std::map<CFlyDDOSkey, CFlyDDoSTick> g_ddos_map;
		static boost::unordered_set<string> g_ddos_ctm2hub; // $Error CTM2HUB

	public:
		static void addCTM2HUB(const string& serverAddr, const HintedUser& hintedUser);
		void fireUploadError(const HintedUser& hintedUser, const string& reason, const string& token) noexcept;

	private:
		static boost::unordered_map<string, CFlyTickTTH> g_duplicate_search_tth;
		static boost::unordered_map<string, CFlyTickFile> g_duplicate_search_file;
		
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		UserList checkIdle;
#endif
		
		StringList nmdcFeatures;
		StringList adcFeatures;
		
		static FastCriticalSection g_cs_update;
		static UserSet g_users_for_update;
		void addUpdatedUser(const UserPtr& user);
		void flushUpdatedUsers();
		
		ExpectedMap expectedConnections;
		
		uint64_t m_floodCounter;
		
		Server* server;
		Server* secureServer;
		
		static bool g_shuttingDown;
		
		SpeedCalc<32> uploadSpeed;
		SpeedCalc<32> downloadSpeed;

		friend class Singleton<ConnectionManager>;
		ConnectionManager();
		
		~ConnectionManager();
		
		static void setIP(UserConnection* conn, const ConnectionQueueItemPtr& qi);
		UserConnection* getConnection(bool nmdc, bool secure) noexcept;
		void putConnection(UserConnection* conn);
		void deleteConnection(UserConnection* conn);
		
		static void removeUnusedConnections();
#ifdef DEBUG_USER_CONNECTION
		static void dumpUserConnections();
#endif
		void addUploadConnection(UserConnection* conn);
		void addDownloadConnection(UserConnection* conn);
		void updateAverageSpeed(uint64_t tick);
		
		ConnectionQueueItemPtr getCQI_L(const HintedUser& hintedUser, bool download);
		void putCQI_L(ConnectionQueueItemPtr& cqi);
		
		void accept(const Socket& sock, int type, Server* server) noexcept;
		
		bool checkKeyprint(UserConnection *source);
		
		void failed(UserConnection* source, const string& error, bool protocolError);
		
	public:
		//static bool getCipherNameAndIP(UserConnection* p_conn, string& p_chiper_name, string& p_ip);
		
		static bool checkIpFlood(const string& address, uint16_t port, const boost::asio::ip::address_v4& p_ip_hub, const string& userInfo, const string& p_HubInfo);
		static bool checkDuplicateSearchTTH(const string& p_search_command, const TTHValue& p_tth);
		static bool checkDuplicateSearchFile(const string& p_search_command);

	private:	
		static void cleanupDuplicateSearchTTH(const uint64_t p_tick);
		static void cleanupDuplicateSearchFile(const uint64_t p_tick);
		static void cleanupIpFlood(const uint64_t p_tick);
		
		// UserConnectionListener
		void on(Connected, UserConnection*) noexcept override;
		void on(Failed, UserConnection*, const string&) noexcept override;
		void on(ProtocolError, UserConnection*, const string&) noexcept override;
		void on(CLock, UserConnection*, const string&) noexcept override;
		void on(Key, UserConnection*, const string&) noexcept override;
		void on(Direction, UserConnection*, const string&, const string&) noexcept override;
		void on(MyNick, UserConnection*, const string&) noexcept override;
		void on(Supports, UserConnection*, StringList &) noexcept override;
		
		void on(AdcCommand::SUP, UserConnection*, const AdcCommand&) noexcept override;
		void on(AdcCommand::INF, UserConnection*, const AdcCommand&) noexcept override;
		void on(AdcCommand::STA, UserConnection*, const AdcCommand&) noexcept override;
		
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
		void on(TimerManagerListener::Minute, uint64_t tick) noexcept override;

		// ClientManagerListener
		void on(ClientManagerListener::UserConnected, const UserPtr& user) noexcept override;
		void on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept override;
		
		void onUserUpdated(const UserPtr& user);
};

#endif // !defined(CONNECTION_MANAGER_H)
