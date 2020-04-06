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
#include "AdcCommand.h"
#include "ClientManager.h"
#include "WinEvent.h"
#include "Socket.h"

struct AdcSearchParam;

class SearchManager : public Speaker<SearchManagerListener>, public Singleton<SearchManager>, public Thread
{
	public:
		static const char* getTypeStr(int type);
		
		void searchAuto(const string& tth);
		
		ClientManagerListener::SearchReply respond(AdcSearchParam& param, const CID& cid, const string& hubIpPort);
		
		static bool isSearchPortValid()
		{
			return g_search_port != 0;
		}
		static uint16_t getSearchPortUint()
		{
			dcassert(g_search_port);
			return g_search_port;
		}
		static string getSearchPort()
		{
			return Util::toString(getSearchPortUint());
		}

		void start();
		void shutdown();

		void onRES(const AdcCommand& cmd, bool skipCID, const UserPtr& from, boost::asio::ip::address_v4 remoteIp);
		void onPSR(const AdcCommand& cmd, bool skipCID, UserPtr from, boost::asio::ip::address_v4 remoteIp);
		static void toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo);

		void onSearchResult(const string& line)
		{
			onData(line.c_str(), static_cast<int>(line.length()), boost::asio::ip::address_v4());
		}

		void addToSendQueue(string& data, boost::asio::ip::address_v4 address, uint16_t port) noexcept;
		
	private:
		friend class Singleton<SearchManager>;

		enum
		{
			EVENT_SOCKET,
			EVENT_COMMAND
		};

		struct SendQueueItem
		{
			string data;
			boost::asio::ip::address_v4 address;
			uint16_t port;

			SendQueueItem(string& data, boost::asio::ip::address_v4 address, uint16_t port):
				data(std::move(data)), address(address), port(port) {}
		};

		unique_ptr<Socket> socket;
		static uint16_t g_search_port;
		std::atomic_bool stopFlag;
		std::atomic_bool restartFlag;
		WinEvent<FALSE> events[2];
		bool failed;
		vector<SendQueueItem> sendQueue;
		CriticalSection csSendQueue;
		
		SearchManager();
		
		virtual int run() override;
		
		void onData(const char* buf, int len, boost::asio::ip::address_v4 address);
		
		static string getPartsString(const PartsInfo& partsInfo);
		bool isShutdown() const;
		void processSendQueue() noexcept;
};

#endif // !defined(SEARCH_MANAGER_H)
