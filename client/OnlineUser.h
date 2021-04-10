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
#ifdef IRAINMAN_USE_HIDDEN_USERS
			CT_HIDDEN  = 0x40,
#endif
			CT_USE_IP6 = 0x80
		};
		
		Identity()
		{
			memset(&m_bits_info, 0, sizeof(m_bits_info));
			slots = 0;
			bytesShared = 0;
			p2pGuardInfoKnown = false;
			hasExtJson = false;
			changes = 0;
			ip = 0;
		}
		
		Identity(const UserPtr& ptr, uint32_t aSID) : user(ptr)
		{
			memset(&m_bits_info, 0, sizeof(m_bits_info));
			slots = 0;
			bytesShared = 0;
			p2pGuardInfoKnown = false;
			hasExtJson = false;
			changes = 0;
			ip = 0;
			setSID(aSID);
		}
		
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		enum FakeCard // [!] IRainman: The internal feature to the protocols it has nothing to do!
		{
			NOT_CHECKED = 0x01,
			CHECKED     = 0x02,
			BAD_CLIENT  = 0x04 //-V112
			, BAD_LIST    = 0x08
		};
#endif
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
		GSMC(IP6, "I6", 1<<COLUMN_IP)
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
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
			change(1<<COLUMN_UPLOAD_SPEED);
#endif
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
		void setBytesShared(const int64_t bytes) // "SS"
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
		
		void setIp(const string& ip);
		void setIp(Ip4Address ip);
		bool isPhantomIP() const;
		Ip4Address getIp() const
		{
			if (ip)
				return ip;
			else
				return getUser()->getIP();
		}
		bool isIPValid() const
		{
			return Util::isValidIp4(ip);
		}
		string getIpAsString() const;

	private:
		string nick;
		Ip4Address ip; // "I4"
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
#define GC_INC_UINT(bits, x)\
	uint##bits##_t  inc##x() { return ++m_bits_info.info_uint##bits[e_##x]; }

#define GSUINTBIT(bits,x)\
	bool get##x##Bit(const uint##bits##_t p_bit_mask) const\
	{\
		return (get_uint##bits(e_##x) & p_bit_mask) != 0;\
	}\
	void set##x##Bit(const uint##bits##_t p_bit_mask, bool p_is_set)\
	{\
		auto& bits_info = get_uint##bits(e_##x);\
		if (p_is_set)\
		{\
			bits_info |= p_bit_mask;\
		}\
		else\
		{\
			bits_info &= ~p_bit_mask;\
		}\
	}

#define GSUINTBITS(bits)\
	const uint##bits##_t& get_uint##bits(eTypeUint##bits##Attr p_attr_index) const\
	{\
		return m_bits_info.info_uint##bits[p_attr_index];\
	}\
	uint##bits##_t& get_uint##bits(eTypeUint##bits##Attr p_attr_index)\
	{\
		return m_bits_info.info_uint##bits[p_attr_index];\
	}\
	void set_uint##bits(eTypeUint##bits##Attr p_attr_index, const uint##bits##_t& p_val)\
	{\
		m_bits_info.info_uint##bits[p_attr_index] = p_val;\
	}

#define GSINTC(bits, x)\
	int##bits##_t get##x() const  { return get_int##bits(e_##x); }\
	void set##x(int##bits##_t val) { set_int##bits(e_##x, val); change(c); }

#define GSINT(bits, x)\
	int##bits##_t get##x() const  { return get_int##bits(e_##x); }\
	void set##x(int##bits##_t val) { set_int##bits(e_##x, val); }

#define GSINTBITS(bits)\
	const int##bits##_t& get_int##bits(eTypeInt##bits##Attr p_attr_index) const\
	{\
		return m_bits_info.info_int##bits[p_attr_index];\
	}\
	int##bits##_t& get_int##bits(eTypeInt##bits##Attr p_attr_index)\
	{\
		return m_bits_info.info_int##bits[p_attr_index];\
	}\
	void set_int##bits(eTypeInt##bits##Attr p_attr_index, const int##bits##_t& p_val)\
	{\
		m_bits_info.info_int##bits[p_attr_index] = p_val;\
	}

	private:
		enum eTypeUint8Attr
		{
			e_ClientType, // 7 бит
#ifdef FLYLINKDC_USE_DETECT_CHEATING
			e_FakeCard,   // 6 бит
#endif
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
		GSUINT(8, ConnectionTimeouts); // "TO"
		GC_INC_UINT(8, ConnectionTimeouts); // "TO"
		GSUINT(8, FileListDisconnects); // "FD"
		GC_INC_UINT(8, FileListDisconnects); // "FD"
		GSUINT(8, ClientType); // "CT"
		GSUINT(8, KnownSupports); // "SU"
		GSUINT(8, KnownUcSupports); // "SU"
		
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
			dcassert(getClientTypeBit(CT_BOT | CT_HUB) == isBot() || isHub());
			return getClientTypeBit(CT_BOT | CT_HUB);
		}
		
		void setUseIP6() // "CT"
		{
			return setClientTypeBit(CT_USE_IP6, true);
		}
		bool isUseIP6() const // "CT"
		{
			return getClientTypeBit(CT_USE_IP6);
		}
		
		void setOp(bool op) // "CT"
		{
			return setClientTypeBit(CT_OP, op);
		}
		bool isOp() const  // "CT"
		{
			// TODO: please fix me.
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
#ifdef IRAINMAN_USE_HIDDEN_USERS
		void setHidden() // "CT"
		{
			return setClientTypeBit(CT_HIDDEN, true);
		}
		bool isHidden() const // "CT"
		{
			return getClientTypeBit(CT_HIDDEN);
		}
#endif
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		GSUINT(8, FakeCard);
		GSUINTBIT(8, FakeCard);
#endif
		
		GSUINTBIT(8, NotEmptyString);
		GSUINT(8, NotEmptyString);
		
//////////////////// uint16 ///////////////////
	private:
		enum eTypeUint16Attr
		{
			e_UdpPort,
			e_Udp6Port,
			e_FreeSlots,
			e_TypeUInt16AttrLast
		};
		GSUINTBITS(16);

	public:
		GSUINT(16, UdpPort); // "U4"
		GSUINT(16, Udp6Port); // "U6"
		GSUINT(16, FreeSlots); // "FS"
		
//////////////////// uint32 ///////////////////
	private:
		enum eTypeUint32Attr
		{
			e_SID,
			e_HubNormalRegOper, // 30 bit.
			e_InfoBitMap,
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
			//e_ExtJSONGDI,
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
		string getSIDString() const;
		
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
		GSUINTC(32, DownloadSpeed, 1<<COLUMN_CONNECTION); // "DS", "CO" (unofficial)
#else
		GSUINT(32, DownloadSpeed);
#endif
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
		
#ifdef FLYLINKDC_USE_EXT_JSON
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
		tstring getGenderTypeAsString(int p_index) const
		{
			switch (p_index)
			{
				case 1:
					return TSTRING(FLY_GENDER_NONE);
				case 2:
					return TSTRING(FLY_GENDER_MALE);
				case 3:
					return TSTRING(FLY_GENDER_FEMALE);
				case 4:
					return TSTRING(FLY_GENDER_ASEXUAL);
			}
			return Util::emptyStringT;
		}
		string getExtJSONSupportInfo() const
		{
			return getStringParamExtJSON("F5");
		}
		void setExtJSONSupportInfo(const string& p_value)
		{
			setStringParam("F5", p_value);
		}
		string getExtJSONHubRamAsText() const
		{
			string result;
			if (hasExtJson)
			{
				if (getExtJSONRAMWorkingSet())
				{
					result = Util::formatBytes(int64_t(getExtJSONRAMWorkingSet()) << 20);
				}
				if (getExtJSONRAMPeakWorkingSet() != getExtJSONRAMWorkingSet())
				{
					result += " [Max: " + Util::formatBytes(int64_t(getExtJSONRAMPeakWorkingSet()) << 20) + "]";
				}
				if (getExtJSONRAMFree())
				{
					result += " [Free: " + Util::formatBytes(int64_t(getExtJSONRAMFree()) >> 20) + "]";
				}
			}
			return result;
		}
		
		string getExtJSONCountFilesAsText() const
		{
			if (hasExtJson && getExtJSONCountFiles())
				return Util::toString(getExtJSONCountFiles());
			else
				return Util::emptyString;
		}
		string getExtJSONLastSharedDateAsText() const
		{
			if (hasExtJson && getExtJSONLastSharedDate())
				return Util::formatTime(getExtJSONLastSharedDate());
			else
				return Util::emptyString;
		}
		
		string getExtJSONSQLiteDBSizeAsText() const
		{
			string result;
			if (hasExtJson)
			{
				if (getExtJSONSQLiteDBSize())
				{
					result = Util::formatBytes(int64_t(getExtJSONSQLiteDBSize()) << 20);
				}
				if (getExtJSONSQLiteDBSizeFree())
				{
					result += " [Free: " + Util::formatBytes(int64_t(getExtJSONSQLiteDBSizeFree()) << 20) + "]";
				}
				if (getExtJSONlevelDBHistSize())
				{
					result += " [LevelDB: " + Util::formatBytes(int64_t(getExtJSONlevelDBHistSize()) << 20) + "]";
				}
			}
			return result;
		}
		string getExtJSONQueueFilesText() const
		{
			string result;
			if (hasExtJson)
			{
				if (getExtJSONQueueFiles())
				{
					result = "[Files: " + Util::toString(getExtJSONQueueFiles()) + "]";
				}
				if (getExtJSONQueueSrc())
				{
					result += " [Sources: " + Util::toString(getExtJSONQueueSrc()) + "]";
				}
			}
			return result;
		}
		string getExtJSONTimesStartCoreText() const
		{
			string result;
			if (hasExtJson)
			{
				if (getExtJSONTimesStartCore())
				{
					result = "[Start core: " + Util::toString(getExtJSONTimesStartCore()) + "]";
				}
				if (getExtJSONTimesStartGUI())
				{
					result += " [Start GUI: " + Util::toString(getExtJSONTimesStartGUI()) + "]";
				}
			}
			return result;
		}
		
		
#endif // EXT_JSON
		
		static string formatShareBytes(uint64_t bytes);
		static string formatIpString(const string& value);
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
		
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		string setCheat(const ClientBase& c, const string& aCheatDescription, bool aBadClient);
#endif
		void getReport(string& report);
		void updateClientType(const OnlineUser& ou)
		{
			setStringParam("CS", Util::emptyString);
#ifdef FLYLINKDC_USE_DETECT_CHEATING
			setFakeCardBit(BAD_CLIENT, false);
#endif
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
		} m_bits_info;
#pragma pack(pop)

		const string& getStringParamL(const char* name) const;
};

class OnlineUser :  public UserInfoBase
{
	public:
		enum
		{
			COLUMN_FIRST,
			COLUMN_NICK = COLUMN_FIRST,
			COLUMN_SHARED,
			COLUMN_EXACT_SHARED,
			COLUMN_P2P_GUARD,
			COLUMN_DESCRIPTION,
			COLUMN_CONNECTION,
			COLUMN_IP,
			COLUMN_LAST_IP,
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
			COLUMN_UPLOAD,
			COLUMN_DOWNLOAD,
			COLUMN_MESSAGES,
#endif
			COLUMN_EMAIL,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
			COLUMN_VERSION,
			COLUMN_MODE,
#endif
			COLUMN_HUBS,
			COLUMN_SLOTS,
			COLUMN_CID,
			COLUMN_TAG,
			COLUMN_LAST
		};
		
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
		
		OnlineUser(const UserPtr& user, ClientBase& client, uint32_t sid)
			: identity(user, sid), client(client)
		{
#ifdef _DEBUG
			++g_online_user_counts;
#endif
		}
		
		virtual ~OnlineUser() noexcept
		{
#ifdef _DEBUG
			--g_online_user_counts;
#endif
		}
#ifdef _DEBUG
		static std::atomic_int g_online_user_counts;
#endif
		
		operator UserPtr&()
		{
			return getUser();
		}
		operator const UserPtr&() const
		{
			return getUser();
		}
		UserPtr& getUser() // TODO
		{
			return identity.getUser();
		}
		const UserPtr& getUser() const // TODO
		{
			return identity.getUser();
		}
		Identity& getIdentity() // TODO
		{
			return identity;
		}
		const Identity& getIdentity() const // TODO
		{
			return identity;
		}
		Client& getClient()
		{
			return (Client&) client;
		}
		const Client& getClient() const
		{
			return (const Client&) client;
		}
		ClientBase& getClientBase()
		{
			return client;
		}
		const ClientBase& getClientBase() const
		{
			return client;
		}
		
		uint8_t getImageIndex() const
		{
			return UserInfoBase::getImage(*this);
		}
		bool isHub() const
		{
			return identity.isHub();
		}
#ifdef IRAINMAN_USE_HIDDEN_USERS
		bool isHidden() const
		{
			return identity.isHidden();
		}
#endif
	private:
		Identity identity;
		ClientBase& client;
};

#endif /* ONLINEUSER_H_ */
