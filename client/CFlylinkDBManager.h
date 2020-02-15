//-----------------------------------------------------------------------------
//(c) 2007-2017 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#ifndef CFlylinkDBManager_H
#define CFlylinkDBManager_H

#define FLYLINKDC_USE_LMDB

#include <vector>
#include <boost/unordered/unordered_map.hpp>
#include "QueueItem.h"
#include "Singleton.h"
#include "CFlyThread.h"
#include "sqlite/sqlite3x.hpp"
#include "CFlyMediaInfo.h"
#include "LogManager.h"

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
	e_DIC_COUNTRY = 4,
	e_DIC_LOCATION = 5,
	e_DIC_LAST
};

struct CFlyIPRange
{
	uint32_t m_start_ip;
	uint32_t m_stop_ip;
	CFlyIPRange() //: m_start_ip(0), m_stop_ip(0)
	{
	}
	CFlyIPRange(uint32_t p_start_ip, uint32_t p_stop_ip): m_start_ip(p_start_ip), m_stop_ip(p_stop_ip)
	{
	}
};

struct CFlyP2PGuardIP : public CFlyIPRange
{
	string m_note;
	CFlyP2PGuardIP()
	{
	}
	CFlyP2PGuardIP(const std::string& p_note, uint32_t p_start_ip, uint32_t p_stop_ip) :
		CFlyIPRange(p_start_ip, p_stop_ip), m_note(p_note)
	{
	}
};

struct CFlyLocationIP : public CFlyIPRange
{
	uint16_t m_flag_index;
	string m_location;
	CFlyLocationIP() // : m_flag_index(0)
	{
	}
	CFlyLocationIP(const std::string& p_location, uint32_t p_start_ip, uint32_t p_stop_ip, uint16_t p_flag_index) :
		CFlyIPRange(p_start_ip, p_stop_ip), m_location(p_location), m_flag_index(p_flag_index)
	{
	}
};

struct CFlyLocationDesc : public CFlyLocationIP
{
	tstring m_description;
};

typedef std::vector<CFlyLocationIP> CFlyLocationIPArray;
typedef std::vector<CFlyP2PGuardIP> CFlyP2PGuardArray;

struct TransferHistorySummary
{
	std::string date;
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

enum eTypeSegment
{
	e_ExtraSlot = 1,
	e_RecentHub = 2,
	e_SearchHistory = 3,
	// 4-5 - пропускаем старые маркеры e_TimeStampGeoIP + e_TimeStampCustomLocation
	e_CMDDebugFilterState = 6,
	e_TimeStampGeoIP = 7,
	e_TimeStampCustomLocation = 8,
	// e_IsTTHLevelDBConvert = 9,
	e_IncopatibleSoftwareList = 10,
	e_TimeStampIBlockListCom = 17,
	e_TimeStampP2PGuard = 18,
	e_autoAddSupportHub = 19,
	e_autoAddFirstSupportHub = 20,
	e_LastShareSize = 21,
	e_autoAdd1251SupportHub = 22,
	e_promoHubArray = 23
};

struct CFlyRegistryValue
{
	string m_val_str;
	int64_t  m_val_int64;
	explicit CFlyRegistryValue(int64_t p_val_int64 = 0)
		: m_val_int64(p_val_int64)
	{
	}
	CFlyRegistryValue(const string &p_str, int64_t p_val_int64 = 0)
		: m_val_int64(p_val_int64), m_val_str(p_str)
	{
	}
	operator bool() const
	{
		return m_val_int64 != 0;
	}
	tstring toT() const
	{
		return Text::toT(m_val_str);
	}
};

typedef std::unordered_map<string, CFlyRegistryValue> CFlyRegistryMap;

class CFlylinkDBManager : public Singleton<CFlylinkDBManager>
{
	public:
		CFlylinkDBManager();
		~CFlylinkDBManager();
		static void shutdown_engine();
		void flush();
		
		static string get_db_size_info();
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
		bool load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info, const  boost::asio::ip::address_v4& p_last_ip);
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
		
		bool is_table_exists(const string& p_table_name);
		
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
		bool is_download_tth(const TTHValue& p_tth);
		
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
		
		static void errorDB(const string& p_txt);
		void vacuum();

	private:
		void clean_registryL(eTypeSegment p_Segment, int64_t p_tick);

	public:
		void load_ignore(StringSet& p_ignores);
		void save_ignore(const StringSet& p_ignores);
		void load_registry(CFlyRegistryMap& p_values, eTypeSegment p_Segment);
		void save_registry(const CFlyRegistryMap& p_values, eTypeSegment p_Segment, bool p_is_cleanup_old_value);
		void clean_registry(eTypeSegment p_Segment, int64_t p_tick);
		
		void set_registry_variable_int64(eTypeSegment p_TypeSegment, int64_t p_value);
		int64_t get_registry_variable_int64(eTypeSegment p_TypeSegment);
		void set_registry_variable_string(eTypeSegment p_TypeSegment, const string& p_value);
		string get_registry_variable_string(eTypeSegment p_TypeSegment);
		template <class T> void load_registry(T& p_values, eTypeSegment p_Segment)
		{
			p_values.clear();
			CFlyRegistryMap l_values;
			load_registry(l_values, p_Segment);
			for (auto k = l_values.cbegin(); k != l_values.cend(); ++k)
			{
				p_values.push_back(Text::toT(k->first));
			}
		}
		template <class T>  void save_registry(const T& p_values, eTypeSegment p_Segment)
		{
			CFlyRegistryMap l_values;
			for (auto i = p_values.cbegin(); i != p_values.cend(); ++i)
			{
				const auto& l_res = l_values.insert(CFlyRegistryMap::value_type(
				                                        Text::fromT(*i),
				                                        CFlyRegistryValue()));
				dcassert(l_res.second);
			}
			save_registry(l_values, p_Segment, true);
		}
		
		void save_geoip(const CFlyLocationIPArray& p_geo_ip);
		void save_p2p_guard(const CFlyP2PGuardArray& p_p2p_guard_ip, const string&  p_manual_marker, int p_type);
		string load_manual_p2p_guard();
		void remove_manual_p2p_guard(const string& p_ip);
		string is_p2p_guard(const uint32_t& p_ip);
#ifdef FLYLINKDC_USE_GEO_IP
		void get_country_and_location(uint32_t p_ip, uint16_t& p_country_index, uint32_t& p_location_index, bool p_is_use_only_cache);
		uint16_t get_country_index_from_cache(int16_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_country_cache[p_index - 1].m_flag_index;
		}
		CFlyLocationDesc get_country_from_cache(uint16_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_country_cache[p_index - 1];
		}
#endif
		uint16_t get_location_index_from_cache(int32_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_location_cache_array[p_index - 1].m_flag_index;
		}
		CFlyLocationDesc get_location_from_cache(int32_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_location_cache_array[p_index - 1];
		}
	private:
#ifdef FLYLINKDC_USE_GEO_IP
		string load_country_locations_p2p_guard_from_db(uint32_t p_ip, uint32_t& p_location_cache_index, uint16_t& p_country_cache_index);
		bool find_cache_country(uint32_t p_ip, uint16_t& p_index);
		bool find_cache_location(uint32_t p_ip, uint32_t& p_location_index, uint16_t& p_flag_index);
		int64_t get_dic_country_id(const string& p_country);
		void clear_dic_cache_country();
#endif
	public:	
		void save_location(const CFlyLocationIPArray& p_geo_ip);
		int64_t get_dic_location_id(const string& p_location);
		//void clear_dic_cache_location();
		
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
		
		CFlySQLCommand m_get_ignores;
		CFlySQLCommand m_insert_ignores;
		CFlySQLCommand m_delete_ignores;
		
		CFlySQLCommand m_select_fly_dic;
		CFlySQLCommand m_insert_fly_dic;
		CFlySQLCommand m_get_registry;
		CFlySQLCommand m_insert_registry;
		CFlySQLCommand m_update_registry;
		CFlySQLCommand m_delete_registry;
		
		FastCriticalSection m_cache_location_cs;
		vector<CFlyLocationDesc> m_location_cache_array;
		struct CFlyCacheIPInfo
		{
			string m_description_p2p_guard;
			uint32_t m_location_cache_index;
			uint16_t m_country_cache_index;
			uint16_t m_flag_location_index;
			CFlyCacheIPInfo() : m_location_cache_index(0), m_country_cache_index(0), m_flag_location_index(0)
			{
			}
		};
		boost::unordered_map<uint32_t, CFlyCacheIPInfo> m_ip_info_cache;
		
		int m_count_fly_location_ip_record;
		bool is_fly_location_ip_valid() const
		{
			return m_count_fly_location_ip_record > 5000;
		}
		CFlySQLCommand m_insert_location;
		CFlySQLCommand m_delete_location;
		
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		CFlySQLCommand m_select_count_location;
		CFlySQLCommand m_select_location_lost;
		CFlySQLCommand m_update_location_lost;
		CFlySQLCommand m_insert_location_lost;
		boost::unordered_set<string> m_lost_location_cache;
#endif
#ifdef FLYLINKDC_USE_GEO_IP
		CFlySQLCommand m_select_country_and_location;
		// TODO CFlySQLCommand m_select_only_location;
		CFlySQLCommand m_insert_geoip;
		CFlySQLCommand m_delete_geoip;
		vector<CFlyLocationDesc> m_country_cache;
#endif
#ifdef _DEBUG
		boost::unordered_map<uint32_t, unsigned> m_count_ip_sql_query_guard;
#endif
		CFlySQLCommand m_select_manual_p2p_guard;
		CFlySQLCommand m_delete_manual_p2p_guard;
		CFlySQLCommand m_delete_p2p_guard;
		CFlySQLCommand m_insert_p2p_guard;
		
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

		CFlySQLCommand m_is_download_tth;
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
		void pragma_executor(const char* p_pragma);
		
		int64_t m_queue_id;
		
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
