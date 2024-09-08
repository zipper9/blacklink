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


#ifndef DCPLUSPLUS_DCPP_SEARCH_MANAGER_H
#define DCPLUSPLUS_DCPP_SEARCH_MANAGER_H

#include "Thread.h"
#include "StringSearch.h"
#include "SearchManagerListener.h"
#include "ClientManagerListener.h"
#include "AdcCommand.h"
#include "Socket.h"
#include "QueueItem.h"
#include "Speaker.h"
#include "Singleton.h"

#ifdef _WIN32
#include "WinEvent.h"
#endif

struct AdcSearchParam;

class SearchManager : public Speaker<SearchManagerListener>, public Singleton<SearchManager>, public Thread
{
	public:
		enum
		{
			OPT_INCOMING_SEARCH_TTH_ONLY       = 1,
			OPT_INCOMING_SEARCH_IGNORE_PASSIVE = 2,
			OPT_INCOMING_SEARCH_IGNORE_BOTS    = 4,
			OPT_ENABLE_SUDP                    = 8
		};

		static ResourceManager::Strings getTypeStr(int type);

		void searchAuto(const string& tth);

		ClientManagerListener::SearchReply respond(AdcSearchParam& param, const OnlineUserPtr& ou, const string& hubUrl, const IpAddress& hubIp, uint16_t hubPort);

		static uint16_t getUdpPort() { return udpPort; }

		void listenUDP(int af);
		void start();
		void shutdown();

		void onRES(const AdcCommand& cmd, bool skipCID, const UserPtr& from, const IpAddress& remoteIp);
		void onPSR(const AdcCommand& cmd, bool skipCID, UserPtr from, const IpAddress& remoteIp);
		static void toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, int af, const string& hubIpPort, const string& tth, const QueueItem::PartsInfo& partialInfo);
		void addEncryptionKey(AdcCommand& cmd) noexcept;

		void onSearchResult(const char* buf, size_t len, const IpAddress& ip)
		{
			onData(buf, static_cast<int>(len), ip, 0);
		}

		static const uint16_t FLAG_NO_TRACE = 1;
		static const uint16_t FLAG_ENC_KEY  = 2;

		void addToSendQueue(string& data, const IpAddress& address, uint16_t port, uint16_t flags = 0, const void* encKey = nullptr) noexcept;
		int getOptions() const noexcept { return options.load(); }
		void updateSettings() noexcept;
		void createNewDecryptKey(uint64_t tick) noexcept;

	private:
		friend class Singleton<SearchManager>;
		static const int MAX_SUDP_KEYS = 2;

#ifdef _WIN32
		enum
		{
			EVENT_SOCKET_V4,
			EVENT_SOCKET_V6,
			EVENT_COMMAND
		};
		WinEvent<FALSE> events[3];
#else
		pthread_t threadId;
#endif

		struct SendQueueItem
		{
			string data;
			IpAddress address;
			uint16_t port;
			uint16_t flags;
			uint8_t encKey[16];

			SendQueueItem(string& data, const IpAddress& address, uint16_t port, uint16_t flags, const void* encKey);
		};

		struct EncryptState
		{
			void* cipher;

			EncryptState() noexcept : cipher(nullptr) {}
			~EncryptState() noexcept { clearCipher(); }

			bool create() noexcept;
			bool encrypt(string& out, const string& in, const void* key) const noexcept;

		private:
			void clearCipher() noexcept;
		};

		struct DecryptState
		{
			string keyBase32;
			void* cipher;
			uint64_t expires;

			DecryptState() noexcept : cipher(nullptr), expires(0) {}
			~DecryptState() noexcept { clearCipher(); }

			bool create(uint64_t tick) noexcept;
			bool decrypt(string& out, const char* inBuf, int len, uint64_t tick) const noexcept;

		private:
			void clearCipher() noexcept;
		};

		unique_ptr<Socket> sockets[2];
		bool failed[2];
		static uint16_t udpPort;
		std::atomic_bool stopFlag;
		std::atomic_bool restartFlag;
		std::atomic_int options;
		vector<SendQueueItem> sendQueue;
		CriticalSection csSendQueue;

		// SUDP
		EncryptState encryptState;
		DecryptState decryptState[MAX_SUDP_KEYS];
		int lastDecryptState;
		uint64_t newKeyTime;
		std::unique_ptr<RWLock> decryptKeyLock;

		SearchManager();

		virtual int run() override;
		bool receivePackets(int index);

		void onData(const char* buf, int len, const IpAddress& address, uint16_t remotePort);
		bool processNMDC(const char* buf, int len, const IpAddress& address, uint16_t remotePort);
		bool processRES(const char* buf, int len, const IpAddress& address);
		bool processPSR(const char* buf, int len, const IpAddress& address);
		bool processSUDP(const char* buf, int len, const IpAddress& remoteIp, uint16_t remotePort);
		bool processPortTest(const char* buf, int len, const IpAddress& address);

		static string getPartsString(const QueueItem::PartsInfo& partsInfo);
		bool isShutdown() const;
		void processSendQueue() noexcept;
		void sendNotif();
};

#endif // !defined(SEARCH_MANAGER_H)
