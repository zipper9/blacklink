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

#ifndef DCPLUSPLUS_DCPP_USER_H
#define DCPLUSPLUS_DCPP_USER_H

#include <boost/asio/ip/address_v4.hpp>
#include "RWLock.h"
#include "Util.h"
#include "CID.h"
#include "Flags.h"
#include "forward.h"
#include "Thread.h"

#ifdef _DEBUG
#include <atomic>
#endif

class ClientBase;
class Client;

#define TAG(x,y) (x + (y << 8)) // TODO static_assert

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
#include "IPStat.h"
#endif

/** A user connected to one or more hubs. */
class User final
{
	public:
		typedef uint32_t MaskType;

		enum Bits
		{
			ONLINE_BIT,
			NMDC_FILES_PASSIVE_BIT,
			NMDC_SEARCH_PASSIVE_BIT,
			TCP4_BIT = NMDC_FILES_PASSIVE_BIT,
			UDP4_BIT = NMDC_SEARCH_PASSIVE_BIT,
			TCP6_BIT,
			UDP6_BIT,
			NMDC_BIT,
			PASSIVE_BIT, // DHT only
			TLS_BIT,
			AWAY_BIT,
			SERVER_BIT,
			FIREBALL_BIT,
			
			NO_ADC_1_0_PROTOCOL_BIT,
			NO_ADCS_0_10_PROTOCOL_BIT,
			NAT_BIT,
#ifdef FLYLINKDC_USE_IPFILTER
			PG_IPGUARD_BLOCK_BIT,
			PG_IPTRUST_BLOCK_BIT,
			PG_P2PGUARD_BLOCK_BIT,
#endif
			DHT_BIT,
			MYSELF_BIT,
			LAST_IP_CHANGED_BIT,
			IP_STAT_LOADED_BIT,
			IP_STAT_CHANGED_BIT,
			USER_STAT_LOADED_BIT,
			SAVE_USER_STAT_BIT,
			ATTRIBS_CHANGED_BIT,
			LAST_BIT // for static_assert (32 bit)
		};
		
		/** Each flag is set if it's true in at least one hub */
		enum UserFlags
		{
			ONLINE = 1 << ONLINE_BIT,
			NMDC_FILES_PASSIVE = 1 << NMDC_FILES_PASSIVE_BIT,
			NMDC_SEARCH_PASSIVE = 1 << NMDC_SEARCH_PASSIVE_BIT,
			TCP4 = 1 << TCP4_BIT,
			UDP4 = 1 << UDP4_BIT,
			TCP6 = 1 << TCP6_BIT,
			UDP6 = 1 << UDP6_BIT,
			NMDC = 1 << NMDC_BIT,
			PASSIVE = 1 << PASSIVE_BIT,
			TLS = 1 << TLS_BIT,             //< Client supports TLS
			ADCS = TLS,                     //< Client supports TLS
			AWAY = 1 << AWAY_BIT,
			SERVER = 1 << SERVER_BIT,
			FIREBALL = 1 << FIREBALL_BIT,
			
			NO_ADC_1_0_PROTOCOL = 1 << NO_ADC_1_0_PROTOCOL_BIT,
			NO_ADCS_0_10_PROTOCOL = 1 << NO_ADCS_0_10_PROTOCOL_BIT,
			
			NAT0 = 1 << NAT_BIT,
			
#ifdef FLYLINKDC_USE_IPFILTER
			PG_IPGUARD_BLOCK = 1 << PG_IPGUARD_BLOCK_BIT,
			PG_IPTRUST_BLOCK = 1 << PG_IPTRUST_BLOCK_BIT,
			PG_P2PGUARD_BLOCK = 1 << PG_P2PGUARD_BLOCK_BIT,
#endif
			DHT = 1 << DHT_BIT,
			MYSELF = 1 << MYSELF_BIT,
			LAST_IP_CHANGED = 1 << LAST_IP_CHANGED_BIT,
			IP_STAT_LOADED = 1 << IP_STAT_LOADED_BIT,
			IP_STAT_CHANGED = 1 << IP_STAT_CHANGED_BIT,
			USER_STAT_LOADED = 1 << USER_STAT_LOADED_BIT,
			SAVE_USER_STAT = 1 << SAVE_USER_STAT_BIT,
			ATTRIBS_CHANGED = 1 << ATTRIBS_CHANGED_BIT
		};
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		enum DefinedAutoBanFlags
		{
			BAN_NONE          = 0x00,
			BAN_BY_MIN_SLOT   = 0x01,
			BAN_BY_MAX_SLOT   = 0x02,
			BAN_BY_SHARE      = 0x04,
			BAN_BY_LIMIT      = 0x08
		};
		
		DefinedAutoBanFlags hasAutoBan(const Client *client, bool isFavorite);
#endif // IRAINMAN_ENABLE_AUTO_BAN

		// TODO
		struct Hash
		{
			size_t operator()(const UserPtr& x) const
			{
				size_t cidHash = 0;
				boost::hash_combine(cidHash, x);
				return cidHash;
			}
		};
//		bool operator==(const UserPtr & x) const
//		{
//			return m_cid == x->m_cid;
//		}
//#define ENABLE_DEBUG_LOG_IN_USER_CLASS

		User(const CID& aCID, const string& nick);
		User(const User&) = delete;
		User& operator= (const User&) = delete;
		~User();
		
#ifdef _DEBUG
		static std::atomic_int g_user_counts;
#endif
		
		const CID& getCID() const
		{
			return cid;
		}

#if 0 // FIXME: temporarily removed. Only used when processing AdcCommand::ERROR_CID_TAKEN
		void setCID(const CID& cid)
		{
			this->cid = cid;
		}
#endif

		string getLastNick() const;
		void setLastNick(const string& nick);
		void updateNick(const string& nick);
		void setIP(const string& ipStr);
		void setIP(boost::asio::ip::address_v4 ip);

		void setBytesShared(int64_t bytesShared)
		{
			CFlyFastLock(cs);
			this->bytesShared = bytesShared;
		}

		int64_t getBytesShared() const
		{
			CFlyFastLock(cs);
			return bytesShared;
		}

		void setLimit(uint32_t limit)
		{
			CFlyFastLock(cs);
			this->limit = limit;
		}

		uint32_t getLimit() const
		{
			CFlyFastLock(cs);
			return limit;
		}

		void setSlots(int slots)
		{
			CFlyFastLock(cs);
			this->slots = slots;
		}

		int getSlots() const
		{
			CFlyFastLock(cs);
			return slots;
		}
		
		MaskType getFlags() const
		{
			CFlyFastLock(cs);
			return flags;
		}

		void setFlag(MaskType flag)
		{
			CFlyFastLock(cs);
			flags |= flag;
		}

		MaskType setFlagEx(MaskType flag)
		{
			CFlyFastLock(cs);
			MaskType prev = flags;
			flags |= flag;
			return prev;
		}

		void unsetFlag(MaskType flag)
		{
			CFlyFastLock(cs);
			flags &= ~flag;		
		}
		
		void changeFlags(MaskType setFlags, MaskType unsetFlags)
		{
			CFlyFastLock(cs);
			flags |= setFlags;
			flags &= ~unsetFlags;
		}

		bool testAndClearFlag(MaskType flag)
		{
			CFlyFastLock(cs);
			if (flags & flag)
			{
				flags &= ~flag;
				return true;
			}
			return false;
		}

		bool isOnline() const
		{
			return (getFlags() & ONLINE) != 0;
		}

		bool isMe() const
		{
			return (getFlags() & MYSELF) != 0;
		}

		boost::asio::ip::address_v4 getIP() const;
		void getInfo(string& nick, boost::asio::ip::address_v4& ip, int64_t& bytesShared, int& slots) const;
		void addNick(const string& nick, const string& hub);

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		uint64_t getBytesUploaded() const;
		uint64_t getBytesDownloaded() const;
		void getBytesTransfered(uint64_t out[]) const;
		void addBytesUploaded(boost::asio::ip::address_v4 ip, uint64_t size);
		void addBytesDownloaded(boost::asio::ip::address_v4 ip, uint64_t size);
		void incMessageCount();
		unsigned getMessageCount() const;
		void loadUserStat();
		void saveUserStat();
		void loadIPStat();
		void saveIPStat(); // must be called before saveUserStat
		void saveStats(bool ipStat, bool userStat);
		bool statsChanged() const;
		bool getLastNickAndHub(string& nick, string& hub) const;
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

	private:
		mutable FastCriticalSection cs;
		const CID cid;
		string nick;
		MaskType flags;
		int64_t bytesShared;
		uint32_t limit;
		int slots;
		boost::asio::ip::address_v4 lastIp;
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		UserStatItem userStat;
		IPStatMap* ipStat;

		void loadIPStatFromDB();
		void loadUserStatFromDB();
#endif
};

// TODO - для буста это пока не цепляется
// http://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key
namespace std
{
template <>
struct hash<UserPtr>
{
	size_t operator()(const UserPtr & x) const
	{
		return ((size_t)(&(*x))) / sizeof(User);
	}
};
}

#endif // !defined(USER_H)
