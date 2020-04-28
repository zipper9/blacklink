//-----------------------------------------------------------------------------
//(c) 2007-2017 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#ifndef CFlylinkDBManager_H
#define CFlylinkDBManager_H

#define FLYLINKDC_USE_LMDB

#include "Singleton.h"
#include "Thread.h"
#include "CFlyUserRatioInfo.h"
#include "LruCache.h"
#include "LogManager.h"
#include "BaseUtil.h"
#include "sqlite/sqlite3x.hpp"
#include <boost/asio/ip/address_v4.hpp>

#ifdef FLYLINKDC_USE_LEVELDB
#include "leveldb/status.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/cache.h"
#include "leveldb/filter_policy.h"
#endif

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/session.hpp"
#endif

#ifdef FLYLINKDC_USE_LMDB
#include "HashDatabaseLMDB.h"
#endif

using sqlite3x::sqlite3_connection;
using sqlite3x::sqlite3_command;
using sqlite3x::sqlite3_reader;
using sqlite3x::sqlite3_transaction;
using sqlite3x::database_error;

typedef unique_ptr<sqlite3_command> CFlySQLCommand;

#define FLYLINKDC_USE_CACHE_HUB_URLS

#ifdef FLYLINKDC_USE_LEVELDB
class CFlyLevelDB
{
		leveldb::DB* m_level_db;
		leveldb::Options      m_options;
		leveldb::ReadOptions  m_readoptions;
		leveldb::ReadOptions  m_iteroptions;
		leveldb::WriteOptions m_writeoptions;
		
	public:
		CFlyLevelDB();
		~CFlyLevelDB();
		static void shutdown();
		
		bool open_level_db(const string& p_db_name, bool& p_is_destroy);
		bool get_value(const void* p_key, size_t p_key_len, string& p_result);
		bool set_value(const void* p_key, size_t p_key_len, const void* p_val, size_t p_val_len);
		bool get_value(const TTHValue& p_tth, string& p_result)
		{
			return get_value(p_tth.data, p_tth.BYTES, p_result);
		}
		bool set_value(const TTHValue& p_tth, const string& p_status)
		{
			dcassert(!p_status.empty());
			if (!p_status.empty())
				return set_value(p_tth.data, p_tth.BYTES, p_status.c_str(), p_status.length());
			else
				return false;
		}
		bool is_open() const
		{
			return m_level_db != nullptr;
		}
		uint32_t set_bit(const TTHValue& p_tth, uint32_t p_mask);
};
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
#pragma pack(push, 1)
struct CFlyIPMessageCache
{
	uint32_t m_message_count;
	unsigned long m_ip;
	CFlyIPMessageCache(uint32_t p_message_count = 0, unsigned long p_ip = 0) : m_message_count(), m_ip(p_ip)
	{
	}
};
#pragma pack(pop)
class CFlyLevelDBCacheIP : public CFlyLevelDB
{
	public:
		void set_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick, uint32_t p_message_count, const boost::asio::ip::address_v4& p_last_ip);
		CFlyIPMessageCache get_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick);
	private:
		void create_key(uint32_t p_hub_id, const string& p_nick, std::vector<char>& p_key)
		{
			p_key.resize(sizeof(uint32_t) + p_nick.size());
			memcpy(&p_key[0], &p_hub_id, sizeof(p_hub_id));
			memcpy(&p_key[0] + sizeof(p_hub_id), p_nick.c_str(), p_nick.size());
		}
};
#endif // FLYLINKDC_USE_IPCACHE_LEVELDB
#endif // FLYLINKDC_USE_LEVELDB

enum eTypeTransfer
{
	e_TransferDownload = 0, // must match e_Download
	e_TransferUpload = 1    // must match e_Upload
};

enum eTypeDIC
{
	e_DIC_HUB = 1,
	e_DIC_NICK = 2,
	e_DIC_IP = 3,
	e_DIC_LAST
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

struct LocationDesc : public LocationInfo
{
	tstring description;
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

struct CFlyLastIPCacheItem
{
	boost::asio::ip::address_v4 m_last_ip;
	uint32_t m_message_count;
	bool m_is_item_dirty;
	// TODO - добавить сохранение страны + провайдера и индексов иконок.
	CFlyLastIPCacheItem(): m_message_count(0), m_is_item_dirty(false)
	{
	}
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

class CFlylinkDBManager : public Singleton<CFlylinkDBManager>
{
	public:
		CFlylinkDBManager();
		~CFlylinkDBManager();
		static void shutdown_engine();
		void flush();
		
		static string getDBInfo(string& root);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		void store_all_ratio_and_last_ip(uint32_t p_hub_id,
		                                 const string& p_nick,
		                                 CFlyUserRatioInfo& p_user_ratio,
		                                 const uint32_t p_message_count,
		                                 const boost::asio::ip::address_v4& p_last_ip,
		                                 bool p_is_last_ip_dirty,
		                                 bool p_is_message_count_dirty,
		                                 bool& p_is_sql_not_found
		                                );
		uint32_t get_dic_hub_id(const string& p_hub);
		void load_global_ratio();
		void load_all_hub_into_cacheL();
#ifdef _DEBUG
		bool m_is_load_global_ratio;
#endif
		bool load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info);
		bool load_last_ip_and_user_stat(uint32_t p_hub_id, const string& p_nick, uint32_t& p_message_count, boost::asio::ip::address_v4& p_last_ip);
		void update_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick,
		                                      const boost::asio::ip::address_v4& p_last_ip,
		                                      const uint32_t p_message_count,
		                                      bool& p_is_sql_not_found,
		                                      const bool p_is_last_ip_dirty,
		                                      const bool p_is_message_count_dirty
		                                     );
	private:
		void store_all_ratio_internal(uint32_t p_hub_id, const int64_t p_dic_nick,
		                              const int64_t p_ip,
		                              const uint64_t p_upload,
		                              const uint64_t p_download
		                             );
		void update_last_ip_deferredL(uint32_t p_hub_id, const string& p_nick, uint32_t p_message_count, boost::asio::ip::address_v4 p_last_ip, bool& p_is_sql_not_found,
		                              bool p_is_last_ip_dirty,
		                              bool p_is_message_count_dirty
		                             );
		void flush_all_last_ip_and_message_count();

	public:
		CFlyGlobalRatioItem  m_global_ratio;
		double get_ratio() const;
		tstring get_ratioW() const;
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		
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
		string getP2PGuardInfo(uint32_t ip);
#ifdef FLYLINKDC_USE_GEO_IP
		void getCountryAndLocation(uint32_t ip, int& countryIndex, int& locationIndex, bool onlyCached);
		int getCountryImageFromCache(int index) const
		{
			dcassert(index > 0);
			CFlyFastLock(csLocationCache);
			if (index <= 0 || index > (int) countryCache.size())
				return -1;
			return countryCache[index-1].imageIndex;
		}
		LocationDesc getCountryFromCache(int index) const
		{
			dcassert(index > 0);
			CFlyFastLock(csLocationCache);
			if (index <= 0 || index > (int) countryCache.size())
				return LocationDesc();
			return countryCache[index-1];
		}
#endif
		int getLocationImageFromCache(int index) const
		{
			dcassert(index > 0);
			CFlyFastLock(csLocationCache);
			if (index <= 0 || index > (int) locationCache.size())
				return -1;
			return locationCache[index-1].imageIndex;
		}
		LocationDesc getLocationFromCache(int index) const
		{			
			dcassert(index > 0);
			CFlyFastLock(csLocationCache);
			if (index <= 0 || index > (int) locationCache.size())
				return LocationDesc();
			return locationCache[index-1];
		}

	private:
#ifdef FLYLINKDC_USE_GEO_IP
		string loadCountryAndLocation(uint32_t ip, int& locationCacheIndex, int& countryCacheIndex);
		bool findCountryInCache(uint32_t ip, int& index) const;
		bool findLocationInCache(uint32_t ip, int& index, int& imageIndex) const;
#endif
	public:	
		void saveLocation(const vector<LocationInfo>& data);
		
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
		string get_hub_name(unsigned p_hub_id);
#endif
	private:
		void initQuery(unique_ptr<sqlite3_command> &command, const char *sql);
		void initQuery2(sqlite3_command &command, const char *sql);

		mutable CriticalSection m_cs;
		// http://leveldb.googlecode.com/svn/trunk/doc/index.html Concurrency
		//  A database may only be opened by one process at a time. The leveldb implementation acquires
		// a lock from the operating system to prevent misuse. Within a single process, the same leveldb::DB
		// object may be safely shared by multiple concurrent threads. I.e., different threads may write into or
		// fetch iterators or call Get on the same database without any external synchronization
		// (the leveldb implementation will automatically do the required synchronization).
		// However other objects (like Iterator and WriteBatch) may require external synchronization.
		// If two threads share such an object, they must protect access to it using their own locking protocol.
		// More details are available in the public header files.
		sqlite3_connection m_flySQLiteDB;

#ifdef FLYLINKDC_USE_LMDB
		HashDatabaseLMDB lmdb;
#endif

	private:
		CFlySQLCommand m_select_ratio_load;
		
#ifdef FLYLINKDC_USE_LASTIP_CACHE
		CFlySQLCommand m_select_all_last_ip_and_message_count;
		boost::unordered_map<uint32_t, boost::unordered_map<std::string, CFlyLastIPCacheItem> > m_last_ip_cache;
		FastCriticalSection m_last_ip_cs;
#else
		CFlySQLCommand m_select_last_ip_and_message_count;
		CFlySQLCommand m_select_last_ip;
		CFlySQLCommand m_insert_last_ip_and_message_count;
		CFlySQLCommand m_update_last_ip_and_message_count;
		CFlySQLCommand m_insert_last_ip;
		CFlySQLCommand m_update_last_ip;
		CFlySQLCommand m_insert_message_count;
		CFlySQLCommand m_update_message_count;
#endif // FLYLINKDC_USE_LASTIP_CACHE
		
	private:
		CFlySQLCommand m_insert_ratio;
		CFlySQLCommand m_update_ratio;
		
#ifdef _DEBUG
		CFlySQLCommand m_check_message_count;
		CFlySQLCommand m_select_store_ip;
#endif
		CFlySQLCommand m_insert_store_all_ip_and_message_count;
		
		sqlite3_command selectIgnoredUsers;
		sqlite3_command insertIgnoredUsers;
		sqlite3_command deleteIgnoredUsers;
		
		CFlySQLCommand m_select_fly_dic;
		CFlySQLCommand m_insert_fly_dic;
		sqlite3_command selectRegistry;
		sqlite3_command insertRegistry;
		sqlite3_command updateRegistry;
		sqlite3_command deleteRegistry;
		sqlite3_command selectTick;
		
		mutable FastCriticalSection csLocationCache;
		vector<LocationDesc> locationCache;
		
		struct IpCacheItem
		{
			string p2pGuardInfo;
			int locationCacheIndex;
			int countryCacheIndex;
			int flagIndex;
		};

		boost::unordered_map<uint32_t, IpCacheItem> ipCache;
		
		sqlite3_command insertLocation;
		sqlite3_command deleteLocation;
		
#ifdef FLYLINKDC_USE_GEO_IP
		sqlite3_command selectCountryAndLocation;
		sqlite3_command deleteCountry;
		sqlite3_command insertCountry;
		vector<LocationDesc> countryCache;
#endif
		sqlite3_command selectManuallyBlockedIP;
		sqlite3_command deleteManuallyBlockedIP;
		sqlite3_command deleteP2PGuard;
		sqlite3_command insertP2PGuard;
		
		CFlySQLCommand m_insert_fly_message;
		static inline const string& getString(const StringMap& p_params, const char* p_type)
		{
			const auto& i = p_params.find(p_type);
			if (i != p_params.end())
				return i->second;
			else
				return Util::emptyString;
		}
		typedef boost::unordered_map<string, int64_t> CFlyCacheDIC;
		std::vector<CFlyCacheDIC> m_DIC;
		
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
		typedef boost::unordered_map<unsigned, string> CFlyCacheDICName;
		CFlyCacheDICName m_HubNameMap;
		FastCriticalSection m_hub_dic_fcs;
#endif
		
		sqlite3_command selectTransfersSummary;
		sqlite3_command selectTransfersDay;
		sqlite3_command insertTransfer;		
		sqlite3_command deleteTransfer;		

		CFlySQLCommand m_select_transfer_convert_leveldb;

#ifdef FLYLINKDC_USE_TORRENT
		sqlite3_command selectTransfersSummaryTorrent;
		sqlite3_command selectTransfersDayTorrent;
		sqlite3_command insertTransferTorrent;
		sqlite3_command deleteTransferTorrent;

		CFlySQLCommand m_insert_resume_torrent;
		CFlySQLCommand m_update_resume_torrent;
		CFlySQLCommand m_check_resume_torrent;
		CFlySQLCommand m_select_resume_torrent;
		CFlySQLCommand m_delete_resume_torrent;
#endif
		
		bool deleteOldTransfers;
		
		int64_t find_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_cache_result);
		int64_t get_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_create);
		//void clear_dic_cache(const eTypeDIC p_DIC);
		
		bool safeAlter(const char* p_sql, bool p_is_all_log = false);
		void setPragma(const char* pragma);
		bool hasTable(const string& tableName);
		
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
