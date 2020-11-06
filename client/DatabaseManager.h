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
#include "Thread.h"
#include "LruCache.h"
#include "LogManager.h"
#include "BaseUtil.h"
#include "IPInfo.h"
#include "CID.h"
#include "sqlite/sqlite3x.hpp"
#include <boost/asio/ip/address_v4.hpp>

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/session.hpp"
#endif

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
	e_TimeStampP2PGuard = 18	
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

class DatabaseManager : public Singleton<DatabaseManager>
{
	public:
		DatabaseManager();
		~DatabaseManager();
		static void shutdown();
		void flush();
		
		static string getDBInfo(string& root);
		enum
		{
			FLAG_SHARED            = 1,
			FLAG_DOWNLOADED        = 2,
			FLAG_DOWNLOAD_CANCELED = 4
		};		

		bool getFileInfo(const TTHValue &tth, unsigned &flags, string &path);
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
#ifdef FLYLINKDC_USE_TORRENT
		void save_torrent_resume(const libtorrent::sha1_hash& p_sha1, const std::string& p_name, const std::vector<char>& p_resume);
		void load_torrent_resume(libtorrent::session& p_session);
		void delete_torrent_resume(const libtorrent::sha1_hash& p_sha1);
		void loadTorrentTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out);
		void loadTorrentTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out);
		void addTorrentTransfer(eTypeTransfer type, const FinishedItemPtr& item);
		void deleteTorrentTransferHistory(const vector<int64_t>& id);
#endif
		
		static void errorDB(const string& text, int errorCode = SQLITE_ERROR);
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
		
		void saveGeoIpCountries(const vector<LocationInfo>& data);
		void saveP2PGuardData(const vector<P2PGuardData>& data, int type, bool removeOld);
		void loadManuallyBlockedIPs(vector<P2PGuardBlockedIP>& result);
		void removeManuallyBlockedIP(uint32_t ip);
		void getIPInfo(uint32_t ip, IPInfo& result, int what, bool onlyCached);
		void clearCachedP2PGuardData(uint32_t ip);
		void clearIpCache();

	private:
		void loadCountry(uint32_t ip, IPInfo& result);
		void loadLocation(uint32_t ip, IPInfo& result);
		void loadP2PGuard(uint32_t ip, IPInfo& result);

	public:	
		void saveLocation(const vector<LocationInfo>& data);

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
		void initQuery(unique_ptr<sqlite3_command> &command, const char *sql);
		void initQuery2(sqlite3_command &command, const char *sql);

		mutable CriticalSection cs;
		sqlite3_connection connection;

#ifdef FLYLINKDC_USE_LMDB
		HashDatabaseLMDB lmdb;
#endif

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
			uint32_t key;
			IPInfo info;
			IpCacheItem* next;
			IpCacheItem* prev;
		};

		LruCacheEx<IpCacheItem, uint32_t> ipCache;
		mutable FastCriticalSection csIpCache;
		
		sqlite3_command insertLocation;
		sqlite3_command deleteLocation;
		
		sqlite3_command selectCountry;
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

#ifdef FLYLINKDC_USE_TORRENT
		sqlite3_command selectTransfersSummaryTorrent;
		sqlite3_command selectTransfersDayTorrent;
		sqlite3_command insertTransferTorrent;
		sqlite3_command deleteTransferTorrent;

		unique_ptr<sqlite3_command> m_insert_resume_torrent;
		unique_ptr<sqlite3_command> m_update_resume_torrent;
		unique_ptr<sqlite3_command> m_check_resume_torrent;
		unique_ptr<sqlite3_command> m_select_resume_torrent;
		unique_ptr<sqlite3_command> m_delete_resume_torrent;
#endif
		
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
		
#ifdef FLYLINKDC_USE_TORRENT
		static FastCriticalSection  g_resume_torrents_cs;
		static FastCriticalSection  g_delete_torrents_cs;
		static std::unordered_set<libtorrent::sha1_hash> g_resume_torrents;
		static std::unordered_set<libtorrent::sha1_hash> g_delete_torrents;
#endif

	public:
#ifdef FLYLINKDC_USE_TORRENT
		static bool is_resume_torrent(const libtorrent::sha1_hash& p_sha1);
		static bool is_delete_torrent(const libtorrent::sha1_hash& p_sha1);
#endif
};
#endif
