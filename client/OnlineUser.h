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

#ifndef DCPLUSPLUS_DCPP_ONLINEUSER_H_
#define DCPLUSPLUS_DCPP_ONLINEUSER_H_

#include <boost/unordered/unordered_map.hpp>
#include "User.h"
#include "UserInfoBase.h"
#include "UserInfoColumns.h"
#include "StrUtil.h"

#ifdef _DEBUG
#include <atomic>
#endif

class ClientBase;

/** One of possibly many identities of a user, mainly for UI purposes */
class Identity
{
	public:
		enum ClientTypeFlag
		{
			CT_BOT     = 0x01,
			CT_REGGED  = 0x02,
			CT_OP      = 0x04,
			CT_SU      = 0x08,
			CT_OWNER   = 0x10,
			CT_HUB     = 0x20,
			CT_HIDDEN  = 0x40
		};

		enum StatusFlag
		{
			SF_AWAY     = 0x01,
			SF_SERVER   = 0x02,
			SF_FIREBALL = 0x04,
			SF_PASSIVE  = 0x08,
			SF_SOCKS    = 0x10
		};

		Identity()
		{
			memset(&values, 0, sizeof(values));
			slots = 0;
			bytesShared = 0;
			p2pGuardInfoKnown = false;
			hasExtJson = false;
			changes = 0;
			ip4 = 0;
			memset(&ip6, 0, sizeof(ip6));
		}
		
		Identity(const UserPtr& ptr, uint32_t aSID) : user(ptr)
		{
			memset(&values, 0, sizeof(values));
			slots = 0;
			bytesShared = 0;
			p2pGuardInfoKnown = false;
			hasExtJson = false;
			changes = 0;
			ip4 = 0;
			memset(&ip6, 0, sizeof(ip6));
			setSID(aSID);
		}

		enum NotEmptyString
		{
			EM = 0x01,
			DE = 0x02
		};
		
		Identity(const Identity&) = delete;
		Identity& operator= (const Identity&) = delete;
		
#define GSMC(n, x, c)\
		string get##n() const { return getStringParam(x); }\
		void set##n(const string& v) { setStringParam(x, v); change(c); }

#define GSM(n, x)\
		string get##n() const { return getStringParam(x); }\
		void set##n(const string& v) { setStringParam(x, v); }

		GSMC(Description, "DE", 1<<COLUMN_DESCRIPTION)
		GSMC(Email, "EM", 1<<COLUMN_EMAIL)
		GSMC(P2PGuard, "P2", 1<<COLUMN_DESCRIPTION)
		void setNick(const string& newNick) // "NI"
		{
			dcassert(!newNick.empty());
			cs.lock();
			if (nick != newNick)
			{
				nick = newNick;
				change(1<<COLUMN_NICK);
			}
			cs.unlock();
			getUser()->setLastNick(newNick);
		}
		string getNick() const
		{
			LOCK(cs);
			return nick;
		}
		bool hasNick() const
		{
			LOCK(cs);
			return !nick.empty();
		}

	public:
		string getSupports() const; // "SU"
		void setLimit(uint32_t lim) // "US"
		{
			getUser()->setLimit(lim);
		}
		uint32_t getLimit() const // "US"
		{
			return getUser()->getLimit();
		}
		void setSlots(uint16_t slots) // "SL"
		{
			this->slots = slots;
			getUser()->setSlots(slots);
			change(1<<COLUMN_SLOTS);
		}
		uint16_t getSlots() const // "SL"
		{
			return slots;
		}
		void setBytesShared(int64_t bytes) // "SS"
		{
			dcassert(bytes >= 0);
			getUser()->setBytesShared(bytes);
			bytesShared = bytes;
			change(1<<COLUMN_SHARED | 1<<COLUMN_EXACT_SHARED);
		}
		int64_t getBytesShared() const // "SS"
		{
			return bytesShared;
		}

		void setIP4(Ip4Address ip) noexcept;
		void setIP6(const Ip6Address& ip) noexcept;
		Ip4Address getIP4() const noexcept;
		Ip6Address getIP6() const noexcept;
		IpAddress getConnectIP() const;
		bool getUdpAddress(IpAddress& ip, uint16_t& port) const;
		bool isIPCached(int af) const;

	private:
		string nick;
		Ip4Address ip4; // "I4"
		Ip6Address ip6; // "I6"
		int64_t bytesShared;
		uint16_t slots;

	public:
		bool p2pGuardInfoKnown;
		void loadP2PGuard();
		
// Нужна ли тут блокировка?
// L: с одной стороны надо блокировать такие операции,
// а с другой эти данные всегда изменяются в одном потоке,
// в потоке хаба, т.е. в потоке сокета.
// А вот читать их могут одновременно многие,
// однако, поскольку, чтение происходит побитно,
// то можно считать данную операцию атомарной, и не блокировать,
// но при этом куски, которые больше чем int для платформы надо блокировать обязательно.

#define GSUINTC(bits, x, c)\
	uint##bits##_t get##x() const  { return get_uint##bits(e_##x); }\
	void set##x(uint##bits##_t val) { set_uint##bits(e_##x, val); change(c); }

#define GSUINT(bits, x)\
	uint##bits##_t get##x() const  { return get_uint##bits(e_##x); }\
	void set##x(uint##bits##_t val) { set_uint##bits(e_##x, val); }

#define GSUINTBIT(bits, x)\
	bool get##x##Bit(uint##bits##_t mask) const\
	{\
		return (get_uint##bits(e_##x) & mask) != 0;\
	}\
	void set##x##Bit(uint##bits##_t mask, bool enable)\
	{\
		auto& val = get_uint##bits(e_##x);\
		if (enable) val |= mask; else val &= ~mask;\
	}\
	void set##x##Bits(uint##bits##_t mask, uint##bits##_t changeMask)\
	{\
		auto& val = get_uint##bits(e_##x);\
		val = (val & ~changeMask) | mask;\
	}

#define GSUINTBITS(bits)\
	const uint##bits##_t& get_uint##bits(eTypeUint##bits##Attr index) const\
	{\
		return values.info_uint##bits[index];\
	}\
	uint##bits##_t& get_uint##bits(eTypeUint##bits##Attr index)\
	{\
		return values.info_uint##bits[index];\
	}\
	void set_uint##bits(eTypeUint##bits##Attr index, const uint##bits##_t& value)\
	{\
		values.info_uint##bits[index] = value;\
	}

#define GSINTC(bits, x)\
	int##bits##_t get##x() const  { return get_int##bits(e_##x); }\
	void set##x(int##bits##_t val) { set_int##bits(e_##x, val); change(c); }

#define GSINT(bits, x)\
	int##bits##_t get##x() const  { return get_int##bits(e_##x); }\
	void set##x(int##bits##_t val) { set_int##bits(e_##x, val); }

#define GSINTBITS(bits)\
	const int##bits##_t& get_int##bits(eTypeInt##bits##Attr index) const\
	{\
		return values.info_int##bits[index];\
	}\
	int##bits##_t& get_int##bits(eTypeInt##bits##Attr index)\
	{\
		return values.info_int##bits[index];\
	}\
	void set_int##bits(eTypeInt##bits##Attr index, const int##bits##_t& value)\
	{\
		values.info_int##bits[index] = value;\
	}

	private:
		enum eTypeUint8Attr
		{
			e_ClientType, // 7 бит
			e_Status,
			e_ConnectionTimeouts,
			e_FileListDisconnects,
			e_KnownSupports, // 1 бит для ADC, 0 для NMDC
			e_KnownUcSupports, // 7 бит вперемешку.
			e_NotEmptyString, // 2 бита
			e_TypeUInt8AttrLast
		};
		GSUINTBITS(8);
		GSUINTBIT(8, ClientType);

	public:
		GSUINTBIT(8, Status);
		GSUINT(8, Status);
		GSUINT(8, ClientType); // "CT"
		GSUINT(8, KnownSupports); // "SU"
		GSUINT(8, KnownUcSupports); // "SU"
		GSUINTBIT(8, NotEmptyString);
		GSUINT(8, NotEmptyString);

		void setHub() // "CT"
		{
			return setClientTypeBit(CT_HUB, true);
		}
		bool isHub() const // "CT"
		{
			return getClientTypeBit(CT_HUB);
		}
		void setBot() // "CT"
		{
			return setClientTypeBit(CT_BOT, true);
		}
		bool isBot() const // "CT"
		{
			return getClientTypeBit(CT_BOT);
		}
		bool isBotOrHub() const // "CT"
		{
			return getClientTypeBit(CT_BOT | CT_HUB);
		}
		void setOp(bool op) // "CT"
		{
			return setClientTypeBit(CT_OP, op);
		}
		bool isOp() const  // "CT"
		{
			return getClientTypeBit(CT_OP /*| CT_SU*/ | CT_OWNER);
		}
		void setRegistered(bool reg) // "CT"
		{
			return setClientTypeBit(CT_REGGED, reg);
		}
		bool isRegistered() const // "CT"
		{
			return getClientTypeBit(CT_REGGED);
		}
		void setHidden() // "CT"
		{
			return setClientTypeBit(CT_HIDDEN, true);
		}
		bool isHidden() const // "CT"
		{
			return getClientTypeBit(CT_HIDDEN);
		}
		
//////////////////// uint16 ///////////////////
	private:
		enum eTypeUint16Attr
		{
			e_Udp4Port,
			e_Udp6Port,
			e_FreeSlots,
			e_TypeUInt16AttrLast
		};
		GSUINTBITS(16);

	public:
		GSUINT(16, Udp4Port); // "U4"
		GSUINT(16, Udp6Port); // "U6"
		GSUINT(16, FreeSlots); // "FS"
		
//////////////////// uint32 ///////////////////
	private:
		enum eTypeUint32Attr
		{
			e_SID,
			e_HubNormalRegOper, // 30 bit.
			e_DownloadSpeed,
			e_SharedFiles,
			e_ExtJSONRAMWorkingSet,
			e_ExtJSONRAMPeakWorkingSet,
			e_ExtJSONRAMFree,
			e_ExtJSONlevelDBHistSize,
			e_ExtJSONSQLiteDBSize,
			e_ExtJSONSQLiteDBSizeFree,
			e_ExtJSONCountFiles,
			e_ExtJSONQueueFiles,
			e_ExtJSONQueueSrc,
			e_ExtJSONTimesStartCore,
			e_ExtJSONTimesStartGUI,
			e_TypeUInt32AttrLast
		};
		GSUINTBITS(32);

	private:
		std::atomic<uint32_t> changes;

		void change(const uint32_t flag)
		{
			BOOST_STATIC_ASSERT(COLUMN_LAST - 1 < 32);
			changes |= flag;
		}

	public:
		uint32_t getChanges()
		{
			return changes.exchange(0);
		}
		
	public:
		GSUINT(32, SID); // "SI"
		static string getSIDString(uint32_t sid);
		string getSIDString() const { return getSIDString(getSID()); }

		GSUINT(32, DownloadSpeed); // "DS", "CO" (unofficial)
		GSUINT(32, SharedFiles); // "SF"

		GSUINT(32, ExtJSONRAMWorkingSet);
		GSUINT(32, ExtJSONRAMPeakWorkingSet);
		GSUINT(32, ExtJSONRAMFree);
		GSUINT(32, ExtJSONCountFiles);
		GSUINT(32, ExtJSONSQLiteDBSize);
		GSUINT(32, ExtJSONlevelDBHistSize);
		GSUINT(32, ExtJSONSQLiteDBSizeFree);
		GSUINT(32, ExtJSONQueueFiles);
		GSUINT(32, ExtJSONQueueSrc);
		GSUINT(32, ExtJSONTimesStartCore);
		GSUINT(32, ExtJSONTimesStartGUI);

		GSUINTC(32, HubNormalRegOper, 1<<COLUMN_HUBS); // "HN"/"HR"/"HO" - packed into a single 32-bit value (10 bits each)
		uint16_t getHubsNormal() const // "HN"
		{
			return (getHubNormalRegOper() >> 20) & MAX_HUBS;
		}
		uint16_t getHubsRegistered() const // "HR"
		{
			return (getHubNormalRegOper() >> 10) & MAX_HUBS;
		}
		uint16_t getHubsOperator() const // "HO"
		{
			return getHubNormalRegOper() & MAX_HUBS;
		}
		void setHubsNormal(unsigned val) // "HN"
		{
			if (val > MAX_HUBS) val = MAX_HUBS;
			uint32_t& res = get_uint32(e_HubNormalRegOper);
			res = (res & ~(MAX_HUBS << 20)) | (val << 20);
			change(1<<COLUMN_HUBS);
		}
		void setHubsRegistered(unsigned val) // "HR"
		{
			if (val > MAX_HUBS) val = MAX_HUBS;
			uint32_t& res = get_uint32(e_HubNormalRegOper);
			res = (res & ~(MAX_HUBS << 10)) | (val << 10);
			change(1<<COLUMN_HUBS);
		}
		void setHubsOperator(unsigned val) // "HO"
		{
			if (val > MAX_HUBS) val = MAX_HUBS;
			uint32_t& res = get_uint32(e_HubNormalRegOper);
			res = (res & ~MAX_HUBS) | val;
			change(1<<COLUMN_HUBS);
		}
		
//////////////////// int64 ///////////////////
	private:
		static const unsigned MAX_HUBS = 0x3FF;
	
		enum eTypeInt64Attr
		{
			e_ExtJSONLastSharedDate = 0,
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
			e_RealBytesShared = 1,
#endif
			e_TypeInt64AttrLast
		};
		GSINTBITS(64);
	public:
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		GSINT(64, RealBytesShared) // "RS"
#endif
		GSINT(64, ExtJSONLastSharedDate)
//////////////////////////////////
	public:
		string getCID() const
		{
			return getUser()->getCID().toBase32();
		}
		string getTag() const;
		string getApplication() const;
		
#ifdef BL_FEATURE_NMDC_EXT_JSON
	private:
		bool hasExtJson;

	public:
		int getGenderType() const
		{
			return Util::toInt(getStringParamExtJSON("F4"));
		}
		tstring getGenderTypeAsString() const
		{
			return getGenderTypeAsString(getGenderType());
		}
		tstring getGenderTypeAsString(int index) const;
		string getExtJSONSupportInfo() const
		{
			return getStringParamExtJSON("F5");
		}
		void setExtJSONSupportInfo(const string& value)
		{
			setStringParam("F5", value);
		}
		string getExtJSONHubRamAsText() const;
		string getExtJSONCountFilesAsText() const;
		string getExtJSONLastSharedDateAsText() const;
		string getExtJSONSQLiteDBSizeAsText() const;
		string getExtJSONQueueFilesText() const;
		string getExtJSONTimesStartCoreText() const;

#endif // EXT_JSON
		
		static string formatShareBytes(uint64_t bytes);
		static string formatIpString(const IpAddress& ip);
		static string formatSpeedLimit(const uint32_t limit);
		
		//bool isTcpActive(const Client* client) const;
		bool isTcpActive() const;
		//bool isUdpActive(const Client* client) const;
		bool isUdpActive() const;
		
		string getStringParam(const char* name) const;
		string getStringParamExtJSON(const char* name) const
		{
			if (hasExtJson)
				return getStringParam(name);
			else
				return Util::emptyString;
		}
		void setStringParam(const char* name, const string& val);
		
		void getReport(string& report);
		void updateClientType(const OnlineUser& ou)
		{
			setStringParam("CS", Util::emptyString);
		}
		
		void getParams(StringMap& map, const string& prefix, bool compatibility, bool dht = false) const;
		UserPtr& getUser()
		{
			return user;
		}
		GETSET(UserPtr, user, User);
		bool isExtJSON() const
		{
			return hasExtJson;
		}
		void setExtJSON();
		
	private:
		mutable FastCriticalSection cs;
		boost::unordered_map<uint16_t, string> stringInfo;
	
#pragma pack(push,1)
		struct
		{
			int64_t  info_int64 [e_TypeInt64AttrLast];
			uint32_t info_uint32[e_TypeUInt32AttrLast];
			uint16_t info_uint16[e_TypeUInt16AttrLast];
			uint8_t  info_uint8 [e_TypeUInt8AttrLast];
		} values;
#pragma pack(pop)

		const string& getStringParamL(uint16_t tag) const;
};

class OnlineUser :  public UserInfoBase
{
	public:
		struct Hash
		{
			size_t operator()(const OnlineUserPtr& x) const
			{
				// return ((size_t)(&(*x))) / sizeof(OnlineUser);
				size_t cidHash = 0;
				boost::hash_combine(cidHash, x);
				return cidHash;
			}
		};
		
		OnlineUser(const UserPtr& user, const ClientBasePtr& cb, uint32_t sid)
			: identity(user, sid), cb(cb)
		{
#ifdef _DEBUG
			++onlineUserCount;
#endif
		}
		
		virtual ~OnlineUser() noexcept
		{
#ifdef _DEBUG
			--onlineUserCount;
#endif
		}
#ifdef _DEBUG
		static std::atomic_int onlineUserCount;
#endif

		const UserPtr& getUser() const override { return identity.getUser(); }
		const string& getHubHint() const override;

		UserPtr& getUser() { return identity.getUser(); }
		Identity& getIdentity() { return identity; }
		const Identity& getIdentity() const { return identity; }
		ClientBasePtr& getClientBase() { return cb; }
		const ClientBasePtr& getClientBase() const { return cb; }

	private:
		Identity identity;
		ClientBasePtr cb;
};

#endif /* ONLINEUSER_H_ */
