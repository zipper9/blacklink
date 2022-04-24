/*
  Blacklink's database manager (c) 2020
  Based on CFlylinkDBManager
 */

//-----------------------------------------------------------------------------
//(c) 2007-2017 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#ifndef DatabaseManager_H
#define DatabaseManager_H

#define FLYLINKDC_USE_LMDB

#include "Singleton.h"
#include "Locks.h"
#include "LruCache.h"
#include "LogManager.h"
#include "BaseUtil.h"
#include "IPInfo.h"
#include "CID.h"
#include "IpAddress.h"
#include "HttpClientListener.h"
#include "sqlite/sqlite3x.hpp"

#ifdef FLYLINKDC_USE_LMDB
#include "HashDatabaseLMDB.h"
#endif

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
#include "IPStat.h"
#endif

using sqlite3x::sqlite3_connection;
using sqlite3x::sqlite3_command;
using sqlite3x::sqlite3_reader;
using sqlite3x::sqlite3_transaction;
using sqlite3x::database_error;

enum eTypeTransfer
{
	e_TransferDownload = 0, // must match e_Download
	e_TransferUpload = 1    // must match e_Upload
};

struct IPAddressRange
{
	uint32_t startIp;
	uint32_t endIp;
	IPAddressRange() {}
	IPAddressRange(uint32_t startIp, uint32_t endIp): startIp(startIp), endIp(endIp) {}
};

struct P2PGuardData
{
	uint32_t startIp;
	uint32_t endIp;
	string note;
	P2PGuardData(const string& note, uint32_t startIp, uint32_t endIp) :
		startIp(startIp), endIp(endIp), note(note) {}
};

struct P2PGuardBlockedIP
{
	uint32_t ip;
	string note;
	P2PGuardBlockedIP() {}
	P2PGuardBlockedIP(uint32_t ip, const string& note) :
		ip(ip), note(note) {}
};

struct LocationInfo
{
	uint32_t startIp;
	uint32_t endIp;
	string location;
	int imageIndex;

	LocationInfo() : startIp(0), endIp(0), imageIndex(-1) {}
	LocationInfo(const string& location, uint32_t startIp, uint32_t endIp, int imageIndex) :
		location(location), startIp(startIp), endIp(endIp), imageIndex(imageIndex) {}
};

struct TransferHistorySummary
{
	time_t date;
	unsigned count;
	unsigned dateAsInt;
	uint64_t actual;
	uint64_t fileSize;
	TransferHistorySummary() : count(0), dateAsInt(0), actual(0), fileSize(0) {}
};

struct IpKey
{	
	union
	{
		uint8_t b[16];
		uint32_t dw[4];
	} u;
	
	bool operator< (const IpKey& x) const
	{
		for (int i = 0; i < 4; i++)
			if (u.dw[i] != x.u.dw[i])
				return u.dw[i] < x.u.dw[i];
		return false;
	}
	bool operator== (const IpKey& x) const
	{
		return u.dw[0] == x.u.dw[0] && u.dw[1] == x.u.dw[1] &&
		       u.dw[2] == x.u.dw[2] && u.dw[3] == x.u.dw[3];
	}
	void setIP(Ip4Address ip);
	void setIP(const Ip6Address& ip);
	uint32_t getHash() const
	{
		return u.dw[0] ^ u.dw[1] ^ u.dw[2] ^ u.dw[3];
	}
};

namespace boost
{
	template<> struct hash<IpKey>
	{
		size_t operator()(const IpKey& x) const
		{
			return x.getHash();
		}
	};
}

enum DBRegistryType
{
	e_ExtraSlot = 1,
	e_RecentHub = 2,
	e_SearchHistory = 3,
	e_CMDDebugFilterState = 6,
	e_TimeStampGeoIP = 7,
	e_TimeStampCustomLocation = 8,
	e_IncopatibleSoftwareList = 10,
	e_TimeStampIBlockListCom = 17,
	e_TimeStampP2PGuard = 18,
	e_FileListSearchHistory = 73
};

struct DBRegistryValue
{
	string sval;
	int64_t ival;
	explicit DBRegistryValue(int64_t ival = 0) : ival(ival)
	{
	}
	DBRegistryValue(const string& sval, int64_t ival = 0) : sval(sval), ival(ival)
	{
	}
	operator bool() const
	{
		return ival != 0;
	}
};

typedef std::unordered_map<string, DBRegistryValue> DBRegistryMap;

class DatabaseManager : public Singleton<DatabaseManager>, public HttpClientListener
{
	public:
		typedef void (*ErrorCallback)(const string& message,  bool forceExit);

		DatabaseManager() noexcept;
		~DatabaseManager();
		static void shutdown();

		string getDBInfo();
		int64_t getDBSize() const { return dbSize; }
		enum
		{
			FLAG_SHARED            = 1,
			FLAG_DOWNLOADED        = 2,
			FLAG_DOWNLOAD_CANCELED = 4
		};		

		void init(ErrorCallback errorCallback);
		bool getFileInfo(const TTHValue &tth, unsigned &flags, string *path, size_t *treeSize);
		bool setFileInfoDownloaded(const TTHValue &tth, uint64_t fileSize, const string &path);
		bool setFileInfoCanceled(const TTHValue &tth, uint64_t fileSize);
		bool addTree(const TigerTree &tree);
		bool getTree(const TTHValue &tth, TigerTree &tree);

#if 0
	private:
		void inc_hitL(const string& p_Path, const string& p_FileName);
#endif

	public:
		void loadTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out);
		void loadTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out);
		void addTransfer(eTypeTransfer type, const FinishedItemPtr& item);
		void deleteTransferHistory(const vector<int64_t>& id);

	private:
		void deleteOldTransferHistoryL();

	public:
		void errorDB(const string& text, int errorCode = SQLITE_ERROR);
		void vacuum();

	private:
		void clearRegistryL(DBRegistryType type, int64_t tick);
		int64_t getRandValForRegistry();

	public:
		void loadIgnoredUsers(StringSet& users);
		void saveIgnoredUsers(const StringSet& users);
		void loadRegistry(DBRegistryMap& values, DBRegistryType type);
		void saveRegistry(const DBRegistryMap& values, DBRegistryType type, bool clearOldValues);
		void clearRegistry(DBRegistryType type, int64_t tick);

		void setRegistryVarInt(DBRegistryType type, int64_t value);
		int64_t getRegistryVarInt(DBRegistryType type);
		void setRegistryVarString(DBRegistryType type, const string& value);
		string getRegistryVarString(DBRegistryType type);

		enum
		{
			PG_DATA_P2P_GUARD_INI  = 2,
			PG_DATA_IBLOCKLIST_COM = 3,
			PG_DATA_MANUAL         = 64
		};

		void saveP2PGuardData(const vector<P2PGuardData>& data, int type, bool removeOld);
		void loadManuallyBlockedIPs(vector<P2PGuardBlockedIP>& result);
		void removeManuallyBlockedIP(Ip4Address ip);
		void getIPInfo(const IpAddress& ip, IPInfo& result, int what, bool onlyCached);
		void clearCachedP2PGuardData(Ip4Address ip);
		void clearIpCache();
		void downloadGeoIPDatabase(uint64_t timestamp, bool force) noexcept;
		bool isDownloading() const noexcept;

	private:
		void loadLocation(Ip4Address ip, IPInfo& result);
		void loadP2PGuard(Ip4Address ip, IPInfo& result);

	public:	
		void saveLocation(const vector<LocationInfo>& data);

	private:
		bool openGeoIPDatabaseL() noexcept;
		void closeGeoIPDatabaseL() noexcept;
		bool loadGeoIPInfo(const IpAddress& ip, IPInfo& result) noexcept;
		void processDownloadResult(uint64_t reqId, const string& text, bool isError) noexcept;
		void clearDownloadRequest(bool isError) noexcept;
		void getGeoIPTimestamp() noexcept;

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	private:
		bool convertStatTables(bool hasRatioTable, bool hasUserTable);
		void saveIPStatL(const CID& cid, const string& ip, const IPStatItem& item, int batchSize, int& count, sqlite3_transaction& trans);
		void saveUserStatL(const CID& cid, UserStatItem& stat, int batchSize, int& count, sqlite3_transaction& trans);
		
	public:
		struct GlobalRatio
		{
			uint64_t download;
			uint64_t upload;
		};

		bool loadUserStat(const CID& cid, UserStatItem& stat);
		void saveUserStat(const CID& cid, UserStatItem& stat);
		IPStatMap* loadIPStat(const CID& cid);
		void saveIPStat(const CID& cid, const vector<IPStatVecItem>& items);

		void loadGlobalRatio(bool force = false);
		const GlobalRatio& getGlobalRatio() const { return globalRatio; }
#endif

	private:
		void initQuery(sqlite3_command &command, const char *sql);
		bool checkDbPrefix(const string& str);
		void attachDatabase(const string& file, const string& name);

	protected:
		void on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept override;
		void on(Failed, uint64_t id, const string& error) noexcept override;
		void on(Redirected, uint64_t id, const string& redirUrl) noexcept override {}

	private:
		string prefix;
		int64_t dbSize;
		ErrorCallback errorCallback;
		mutable CriticalSection cs;
		sqlite3_connection connection;

#ifdef FLYLINKDC_USE_LMDB
		HashDatabaseLMDB lmdb;
#endif
		struct MMDB_s* mmdb;
		CriticalSection csMmdb;
		uint64_t timeCheckMmdb;

		uint64_t mmdbDownloadReq;
		uint64_t timeDownloadMmdb;
		uint64_t mmdbFileTimestamp; // 0 - file doesn't exist, 1 - not initialized
		bool mmdbDownloading;
		mutable CriticalSection csDownloadMmdb;

	private:
		sqlite3_command selectIgnoredUsers;
		sqlite3_command insertIgnoredUsers;
		sqlite3_command deleteIgnoredUsers;
		
		sqlite3_command selectRegistry;
		sqlite3_command insertRegistry;
		sqlite3_command updateRegistry;
		sqlite3_command deleteRegistry;
		sqlite3_command selectTick;
		
		static const size_t IP_CACHE_SIZE = 3000;

		struct IpCacheItem
		{
			IpKey key;
			IPInfo info;
			IpCacheItem* next;
			IpCacheItem* prev;
		};

		LruCacheEx<IpCacheItem, IpKey> ipCache;
		mutable FastCriticalSection csIpCache;
		
		sqlite3_command insertLocation;
		sqlite3_command deleteLocation;

		sqlite3_command selectLocation;
		sqlite3_command selectP2PGuard;
		sqlite3_command deleteCountry;
		sqlite3_command insertCountry;
		sqlite3_command selectManuallyBlockedIP;
		sqlite3_command deleteManuallyBlockedIP;
		sqlite3_command deleteP2PGuard;
		sqlite3_command insertP2PGuard;
		
		sqlite3_command selectTransfersSummary;
		sqlite3_command selectTransfersDay;
		sqlite3_command insertTransfer;		
		sqlite3_command deleteTransfer;				

		bool deleteOldTransfers;

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		sqlite3_command insertIPStat;
		sqlite3_command updateIPStat;
		sqlite3_command selectIPStat;
		sqlite3_command insertUserStat;
		sqlite3_command updateUserStat;
		sqlite3_command selectUserStat;
		sqlite3_command selectGlobalRatio;
		
		GlobalRatio globalRatio;
		uint64_t timeLoadGlobalRatio;
#endif

		bool safeAlter(const char* sql, bool verbose = false);
		void setPragma(const char* pragma);
		bool hasTable(const string& tableName, const string& db = string());
		
		static const size_t TREE_CACHE_SIZE = 300;

		struct TreeCacheItem
		{
			TTHValue key;
			TigerTree tree;
			TreeCacheItem* next;
		};

		CriticalSection csTreeCache;
		LruCache<TreeCacheItem, TTHValue> treeCache;
};

#endif
