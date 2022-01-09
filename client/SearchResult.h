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
#include "BaseUtil.h"
#include "HashValue.h"
#include "IPInfo.h"
#include "IpAddress.h"

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

		static const uint16_t SLOTS_UNKNOWN = 0xFFFF;
		
		SearchResultCore(): size(0), type(TYPE_FILE) {}
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

	protected:
		TTHValue tth;
		string file;
		string torrentMagnet;
		int64_t size;
		uint16_t slots;
		uint16_t freeSlots;
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

		SearchResult() : flags(0), token(uint32_t (-1)), ip{0}
		{
		}

		SearchResult(const UserPtr& user, Types type, unsigned slots, unsigned freeSlots,
		             int64_t size, const string& file, const string& hubURL,
					 const IpAddress& ip, const TTHValue& tth, uint32_t token);
		             
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
		
		string getIPAsString() const
		{
			return Util::printIpAddress(ip);
		}
		const IpAddress& getIP() const
		{
			return ip;
		}
		uint32_t getToken() const
		{
			return token;
		}
		
		int flags;
		unsigned freeSlots;
		unsigned slots;
		
		void checkTTH();
		void loadLocation();
		void loadP2PGuard();
		const IPInfo& getIpInfo() const { return ipInfo; }

	private:
		friend class SearchManager;
		
		const string hubURL;
		const uint32_t token;
		const UserPtr user;
		const IpAddress ip;
		IPInfo ipInfo;
};

#endif
