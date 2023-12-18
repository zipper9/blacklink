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

		void onSearchResult(const string& line, const IpAddress& ip)
		{
			onData(line.c_str(), static_cast<int>(line.length()), ip, 0);
		}

		static const uint16_t FLAG_NO_TRACE = 1;
		void addToSendQueue(string& data, const IpAddress& address, uint16_t port, uint16_t flags = 0) noexcept;

	private:
		friend class Singleton<SearchManager>;

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

			SendQueueItem(string& data, const IpAddress& address, uint16_t port, uint16_t flags):
				data(std::move(data)), address(address), port(port), flags(flags) {}
		};

		unique_ptr<Socket> sockets[2];
		bool failed[2];
		static uint16_t udpPort;
		std::atomic_bool stopFlag;
		std::atomic_bool restartFlag;
		vector<SendQueueItem> sendQueue;
		CriticalSection csSendQueue;

		SearchManager();

		virtual int run() override;
		bool receivePackets(int index);

		void onData(const char* buf, int len, const IpAddress& address, uint16_t remotePort);
		bool processNMDC(const char* buf, int len, const IpAddress& address, uint16_t remotePort);
		bool processRES(const char* buf, int len, const IpAddress& address);
		bool processPSR(const char* buf, int len, const IpAddress& address);
		bool processPortTest(const char* buf, int len, const IpAddress& address);

		static string getPartsString(const QueueItem::PartsInfo& partsInfo);
		bool isShutdown() const;
		void processSendQueue() noexcept;
		void sendNotif();
};

#endif // !defined(SEARCH_MANAGER_H)
