/*
  BlackLink's database manager (c) 2020
  Based on CFlylinkDBManager
 */

//-----------------------------------------------------------------------------
//(c) 2007-2017 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#ifndef DATABASE_MANAGER_H_
#define DATABASE_MANAGER_H_

#include "Singleton.h"
#include "Locks.h"
#include "LruCache.h"
#include "LogManager.h"
#include "BaseUtil.h"
#include "IPInfo.h"
#include "CID.h"
#include "IpAddress.h"
#include "IpKey.h"
#include "JobExecutor.h"
#include "HttpClientListener.h"
#include "HashDatabaseLMDB.h"
#include "forward.h"
#include "sqlite/sqlite3x.hpp"
#include <atomic>

#ifdef BL_FEATURE_IP_DATABASE
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

struct DBIgnoreListItem
{
	string data;
	int type;
};

struct DBTransferItem
{
	eTypeTransfer type;
	FinishedItemPtr item;
};

#ifdef BL_FEATURE_IP_DATABASE
struct DBIPStatItem
{
	CID cid;
	vector<IPStatVecItem> items;
};

struct DBUserStatItem
{
	CID cid;
	UserStatItem stat;
};
#endif

class DatabaseConnection
{
		friend class DatabaseManager;

	public:
		void setAbortFlag(std::atomic_bool* af);
		void setRegistryVarInt(DBRegistryType type, int64_t value);
		int64_t getRegistryVarInt(DBRegistryType type);
		void setRegistryVarString(DBRegistryType type, const string& value);
		string getRegistryVarString(DBRegistryType type);
		void loadTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out);
		void loadTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out);
		void addTransfer(eTypeTransfer type, const FinishedItemPtr& item);
		void addTransfers(const vector<DBTransferItem>& items);
		void deleteTransferHistory(const vector<int64_t>& id);
		void loadIgnoredUsers(vector<DBIgnoreListItem>& items);
		void saveIgnoredUsers(const vector<DBIgnoreListItem>& items);
		void loadRegistry(DBRegistryMap& values, DBRegistryType type);
		void saveRegistry(const DBRegistryMap& values, DBRegistryType type, bool clearOldValues);
		void clearRegistry(DBRegistryType type, int64_t tick);
		void loadLocation(Ip4Address ip, IPInfo& result);
		void loadP2PGuard(Ip4Address ip, IPInfo& result);
		void saveLocation(const vector<LocationInfo>& data);
		void saveP2PGuardData(const vector<P2PGuardData>& data, int type, bool removeOld);
		void clearP2PGuardData(int type);
		void loadManuallyBlockedIPs(vector<P2PGuardBlockedIP>& result);
		void removeManuallyBlockedIP(Ip4Address ip);
#ifdef BL_FEATURE_IP_DATABASE
		bool loadUserStat(const CID& cid, UserStatItem& stat);
		void saveUserStats(const vector<DBUserStatItem>& items);
		void removeUserStat(const CID& cid);
		IPStatMap* loadIPStat(const CID& cid);
		void saveIPStats(const vector<DBIPStatItem>& items);
		void removeIPStat(const CID& cid);
		bool loadGlobalRatio(uint64_t values[]);
#endif

	private:
		void open(const string& prefix, int journalMode);
		void initQuery(sqlite3_command &command, const char *sql);
		void attachDatabase(const string& path, const string& file, const string& prefix, const string& name);
		void upgradeDatabase();
		bool safeAlter(const char* sql, bool verbose = false);
		void setPragma(const char* pragma);
		bool hasTable(const string& tableName, const string& db = string());
		void clearRegistryL(DBRegistryType type, int64_t tick);
		int64_t getRandValForRegistry();
		void deleteOldTransferHistory();
		void initInsertTransferQuery();
		void doDeleteP2PGuard(int type);
#ifdef BL_FEATURE_IP_DATABASE
		bool convertStatTables(bool hasRatioTable, bool hasUserTable);
		void saveIPStatL(const CID& cid, const string& ip, const IPStatItem& item, int batchSize, int& count, sqlite3_transaction& trans);
		void saveUserStat(const CID& cid, const UserStatItem& stat, int batchSize, int& count, sqlite3_transaction& trans);
#endif
		static int progressHandler(void* ctx);

	private:
		bool busy = true;
		uint64_t removeTime = 0;
		std::atomic_bool* abortFlag = nullptr;

		sqlite3_connection connection;
		sqlite3_command selectIgnoredUsers;
		sqlite3_command insertIgnoredUsers;
		sqlite3_command deleteIgnoredUsers;

		sqlite3_command selectRegistry;
		sqlite3_command insertRegistry;
		sqlite3_command updateRegistry;
		sqlite3_command deleteRegistry;
		sqlite3_command selectTick;

		sqlite3_command insertLocation;
		sqlite3_command deleteLocation;

		sqlite3_command selectLocation;
		sqlite3_command selectP2PGuard;
		sqlite3_command selectManuallyBlockedIP;
		sqlite3_command deleteManuallyBlockedIP;
		sqlite3_command deleteP2PGuard;
		sqlite3_command insertP2PGuard;

		sqlite3_command selectTransfersSummary;
		sqlite3_command selectTransfersDay;
		sqlite3_command insertTransfer;
		sqlite3_command deleteTransfer;

#ifdef BL_FEATURE_IP_DATABASE
		sqlite3_command insertIPStat;
		sqlite3_command updateIPStat;
		sqlite3_command selectIPStat;
		sqlite3_command deleteIPStat;
		sqlite3_command insertUserStat;
		sqlite3_command updateUserStat;
		sqlite3_command selectUserStat;
		sqlite3_command deleteUserStat;
		sqlite3_command selectGlobalRatio;
#endif
};

class DatabaseManager : public Singleton<DatabaseManager>, public HttpClientListener
{
		friend class DatabaseConnection;

	public:
		enum
		{
			JOURNAL_MODE_PERSIST = 1,
			JOURNAL_MODE_WAL     = 2,
			JOURNAL_MODE_MEMORY  = 3,
			DEFAULT_JOURNAL_MODE = JOURNAL_MODE_WAL
		};

		typedef void (*ErrorCallback)(const string& message,  bool forceExit);

		DatabaseManager() noexcept;
		~DatabaseManager();
		void shutdown();

		string getDBInfo();
		int64_t getDBSize() const { return dbSize; }
		enum
		{
			FLAG_SHARED            = 1,
			FLAG_DOWNLOADED        = 2,
			FLAG_DOWNLOAD_CANCELED = 4
		};

		void init(ErrorCallback errorCallback, int journalMode);
		DatabaseConnection* getDefaultConnection();
		DatabaseConnection* getConnection();
		void putConnection(DatabaseConnection* conn);
		void closeIdleConnections(uint64_t tick);
		bool addTree(HashDatabaseConnection* conn, const TigerTree& tree) noexcept;
		bool getTree(HashDatabaseConnection* conn, const TTHValue& tth, TigerTree& tree) noexcept;
		HashDatabaseConnection* getHashDatabaseConnection() noexcept { return lmdb.getConnection(); }
		HashDatabaseConnection* getDefaultHashDatabaseConnection() noexcept { return lmdb.getDefaultConnection(); }
		void putHashDatabaseConnection(HashDatabaseConnection* conn) noexcept { lmdb.putConnection(conn); }
		static void quoteString(string& s) noexcept;
		void addTransfer(eTypeTransfer type, const FinishedItemPtr& item) noexcept;
		void processTimer(uint64_t tick) noexcept;

	public:
		void reportError(const string& text, int errorCode = SQLITE_ERROR);

		enum
		{
			PG_DATA_P2P_GUARD_INI  = 2,
			PG_DATA_IBLOCKLIST_COM = 3,
			PG_DATA_MANUAL         = 64
		};

		void getIPInfo(DatabaseConnection* conn, const IpAddress& ip, IPInfo& result, int what, bool onlyCached);
		void clearCachedP2PGuardData(Ip4Address ip);
		void clearIpCache();
		void downloadGeoIPDatabase(uint64_t timestamp, bool force, const string &url) noexcept;

		enum
		{
			MMDB_STATUS_OK,
			MMDB_STATUS_MISSING,
			MMDB_STATUS_DOWNLOADING,
			MMDB_STATUS_DL_FAILED
		};

		int getGeoIPDatabaseStatus(uint64_t& timestamp) noexcept;

	private:
		bool openGeoIPDatabaseL() noexcept;
		void closeGeoIPDatabaseL() noexcept;
		bool loadGeoIPInfo(const IpAddress& ip, IPInfo& result) noexcept;
		void processDownloadResult(uint64_t reqId, const string& text, bool isError) noexcept;
		void clearDownloadRequest(bool isRetry, int newStatus) noexcept;
		uint64_t getGeoIPTimestamp() const noexcept;

#ifdef BL_FEATURE_IP_DATABASE
	public:
		struct GlobalRatio
		{
			uint64_t download;
			uint64_t upload;
		};

		void loadGlobalRatio(bool force = false);
		GlobalRatio getGlobalRatio() const;
		void saveIPStat(const CID& cid, const vector<IPStatVecItem>& items) noexcept;
		void saveUserStat(const CID& cid, const UserStatItem& stat) noexcept;
		void loadIPStatAsync(const UserPtr& user) noexcept;
		void loadUserStatAsync(const UserPtr& user) noexcept;
#endif

	private:
		bool checkDbPrefix(const string& path, const string& str) noexcept;
		void flushPendingData() noexcept;
		void savePendingData(uint64_t tick) noexcept;

	protected:
		void on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept override;
		void on(Failed, uint64_t id, const string& error) noexcept override;
		void on(Redirected, uint64_t id, const string& redirUrl) noexcept override {}

	private:
		string prefix;
		int64_t dbSize;
		ErrorCallback errorCallback;
		int journalMode;
		mutable CriticalSection cs;
		std::unique_ptr<DatabaseConnection> defConn;
		std::list<std::unique_ptr<DatabaseConnection>> conn;
		uintptr_t defThreadId;
		HashDatabaseLMDB lmdb;

		struct MMDB_s* mmdb;
		CriticalSection csMmdb;
		uint64_t timeCheckMmdb;

		uint64_t mmdbDownloadReq;
		uint64_t timeDownloadMmdb;
		uint64_t mmdbFileTimestamp; // 0 - file doesn't exist, 1 - not initialized
		int mmdbStatus;
		CriticalSection csDownloadMmdb;

	private:
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

		struct SaveTransfersJob : public JobExecutor::Job
		{
			vector<DBTransferItem> items;
			void run() override;
		};

#ifdef BL_FEATURE_IP_DATABASE
		struct SaveIPStatJob : public JobExecutor::Job
		{
			vector<DBIPStatItem> items;
			void run() override;
		};

		struct SaveUserStatJob : public JobExecutor::Job
		{
			vector<DBUserStatItem> items;
			void run() override;
		};

		struct LoadIPStatJob : public JobExecutor::Job
		{
			UserPtr user;
			void run() override;
		};

		struct LoadUserStatJob : public JobExecutor::Job
		{
			UserPtr user;
			void run() override;
		};
#endif

		std::atomic_bool deleteOldTransfers;
		vector<DBTransferItem> transfers;
		CriticalSection csTransfers;
		uint64_t timeTransfersSaved;

#ifdef BL_FEATURE_IP_DATABASE
		vector<DBIPStatItem> ipStatItems;
		CriticalSection csIpStat;
		uint64_t timeIpStatSaved;

		vector<DBUserStatItem> userStatItems;
		CriticalSection csUserStat;
		uint64_t timeUserStatSaved;

		mutable CriticalSection csGlobalRatio;
		GlobalRatio globalRatio;
		uint64_t timeLoadGlobalRatio;
#endif

		JobExecutor backgroundThread;

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

#endif // DATABASE_MANAGER_H_
