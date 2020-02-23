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

#ifndef DCPLUSPLUS_DCPP_SEARCHRESULT_H
#define DCPLUSPLUS_DCPP_SEARCHRESULT_H

#include "forward.h"
#include "SearchQueue.h"
#include "HintedUser.h"
#include "StrUtil.h"
#include "Util.h"
#include <boost/asio/ip/address_v4.hpp>

class Client;
class AdcCommand;
class SearchManager;

class SearchResultCore
{
	public:
		enum Types
		{
			TYPE_FILE,
			TYPE_DIRECTORY,
			TYPE_TORRENT_MAGNET
		};
		SearchResultCore(): size(0), type(TYPE_FILE)
		{
		}
		
		SearchResultCore(Types type, int64_t size, const string& name, const TTHValue& tth);
		string toSR(const Client& c, unsigned freeSlots, unsigned slots) const;
		void toRES(AdcCommand& cmd, unsigned freeSlots) const;
		const string& getFile() const
		{
			return file;
		}
		void setFile(const string& file)
		{
			this->file = file;
		}
		string getSHA1() const
		{
			const auto pos = torrentMagnet.find("xt=urn:btih:");
			if (pos != string::npos && pos + 12 + 40 <= torrentMagnet.length())
				return torrentMagnet.substr(pos + 12, 40);
			return string();
		}
		const string& getTorrentMagnet() const
		{
			return torrentMagnet;
		}
		void setTorrentMagnet(const string& torrentMagnet)
		{
			this->torrentMagnet = torrentMagnet;
			type = TYPE_TORRENT_MAGNET;
		}
		int64_t getSize() const
		{
			return size;
		}
		const TTHValue& getTTH() const
		{
			return tth;
		}
		Types getType() const
		{
			return type;
		}
		string getPeersString() const
		{
			return Util::toString(peer) + '/' + Util::toString(seed);
		}
		// TODO унести свойства торрента в отдельный класс
		uint16_t peer = 0;
		uint16_t seed = 0;
		string torrentUrl;
		uint16_t m_group_index = 0;
		string   m_group_name;
		uint16_t m_comment = 0;
		string m_tracker;
		uint16_t m_tracker_index = 0;
		string m_date;
		int64_t size;

	protected:
		TTHValue tth;
		string file;
		string torrentMagnet;
		uint8_t slots;
		uint8_t freeSlots;
		Types type;
};

class SearchResult : public SearchResultCore
{
	public:
		enum
		{
			FLAG_STATUS_KNOWN      = 0x01,
			FLAG_SHARED            = 0x02,
			FLAG_QUEUED            = 0x04,
			FLAG_DOWNLOADED        = 0x08,
			FLAG_DOWNLOAD_CANCELED = 0x10
		};

		SearchResult() : flags(0), token(uint32_t (-1)), p2pGuardInit(false)
		{
		}
		SearchResult(Types type, int64_t size, const string& file, const TTHValue& tth, uint32_t token);
		
		SearchResult(const UserPtr& user, Types type, unsigned slots, unsigned freeSlots,
		             int64_t size, const string& file, const string& hubName,
		             const string& hubURL, boost::asio::ip::address_v4 ip4, const TTHValue& tth, uint32_t token);
		             
		string getFileName() const;
		string getFilePath() const;
		
		const UserPtr& getUser() const
		{
			return user;
		}
		HintedUser getHintedUser() const
		{
			return HintedUser(getUser(), getHubUrl());
		}
		const string& getHubUrl() const
		{
			return hubURL;
		}
		const string& getHubName() const
		{
			return hubName;
		}
		void calcHubName();
		
		string getIPAsString() const
		{
			if (!ip.is_unspecified())
				return ip.to_string();
			else
				return Util::emptyString;
		}
		boost::asio::ip::address_v4 getIP() const
		{
			return ip;
		}
		uint32_t getToken() const
		{
			return token;
		}
		
		int flags;
		unsigned m_torrent_page = 0;

		unsigned freeSlots;
		unsigned slots;
		
		const string& getP2PGuard() const
		{
			return p2pGuardText;
		}

		void checkTTH();
		void calcP2PGuard();

	private:
		friend class SearchManager;
		
		string hubName;
		string hubURL;
		uint32_t token;
		UserPtr user;
		boost::asio::ip::address_v4 ip;
		string p2pGuardText;
		bool p2pGuardInit;
};

#endif
