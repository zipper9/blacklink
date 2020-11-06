//-----------------------------------------------------------------------------
//(c) 2007-2018 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#include "stdinc.h"

#include "CFlylinkDBManager.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include "ShareManager.h"
#include "ConnectionManager.h"
#include "CompatibilityManager.h"
#include "FinishedManager.h"
#include "TimerManager.h"

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/read_resume_data.hpp"
#endif

using sqlite3x::database_error;
using sqlite3x::sqlite3_transaction;
using sqlite3x::sqlite3_reader;

//bool g_DisableUserStat = false;
bool g_DisableSQLJournal    = false;
bool g_UseWALJournal        = false;
bool g_EnableSQLtrace       = false; // http://www.sqlite.org/c3ref/profile.html
bool g_UseSynchronousOff    = false;
int g_DisableSQLtrace      = 0;
int64_t g_SQLiteDBSizeFree = 0;
int64_t g_TTHLevelDBSize = 0;
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
int64_t g_IPCacheLevelDBSize = 0;
#endif
int64_t g_SQLiteDBSize = 0;

#ifdef FLYLINKDC_USE_TORRENT
FastCriticalSection  CFlylinkDBManager::g_resume_torrents_cs;
std::unordered_set<libtorrent::sha1_hash> CFlylinkDBManager::g_resume_torrents;

FastCriticalSection  CFlylinkDBManager::g_delete_torrents_cs;
std::unordered_set<libtorrent::sha1_hash> CFlylinkDBManager::g_delete_torrents;
#endif

static const char* fileNames[] =
{
	"FlylinkDC.sqlite",
	"FlylinkDC_locations.sqlite",
	"FlylinkDC_transfers.sqlite",
	"FlylinkDC_user.sqlite",
#ifdef FLYLINKDC_USE_TORRENT
	"FlylinkDC_queue.sqlite"
#endif
};

// WARNING: This routine uses Win32 and therefore not portable
static int64_t posixTimeToLocal(int64_t pt)
{
	static const int64_t offset = 11644473600ll;
	static const int64_t scale = 10000000ll;
	int64_t filetime = (pt + offset) * scale;
	int64_t local;
	if (!FileTimeToLocalFileTime((FILETIME *) &filetime, (FILETIME *) &local)) return 0;
	return local / scale - offset;
}

int gf_busy_handler(void *p_params, int p_tryes)
{
	//CFlylinkDBManager *l_db = (CFlylinkDBManager *)p_params;
	Sleep(500);
	LogManager::message("SQLite database is locked. try: " + Util::toString(p_tryes));
	if (p_tryes && p_tryes % 5 == 0)
	{
		static int g_MessageBox = 0; // TODO - fix copy-paste
		CFlyBusy busy(g_MessageBox);
		if (g_MessageBox <= 1)
		{
			MessageBox(NULL, CTSTRING(DATA_BASE_LOCKED_STRING), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
		}
	}
	return 1;
}

static void gf_trace_callback(void* p_udp, const char* p_sql)
{
	if (g_DisableSQLtrace == 0)
	{
		if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
		{
			StringMap params;
			params["sql"] = p_sql;
			params["thread_id"] = Util::toString(::GetCurrentThreadId());
			LOG(SQLITE_TRACE, params);
		}
	}
}

void CFlylinkDBManager::initQuery(unique_ptr<sqlite3_command> &command, const char *sql)
{
	if (!command) command.reset(new sqlite3_command(&m_flySQLiteDB, sql));
}

void CFlylinkDBManager::initQuery2(sqlite3_command &command, const char *sql)
{
	if (command.empty()) command.open(&m_flySQLiteDB, sql);
}

//========================================================================================================
//static void profile_callback( void* p_udp, const char* p_sql, sqlite3_uint64 p_time)
//{
//	const string l_log = "profile_callback - " + string(p_sql) + " time = "+ Util::toString(p_time);
//	LogManager::message(l_log,true);
//}
//========================================================================================================

void CFlylinkDBManager::setPragma(const char* pragma)
{
	static const char* dbName[] = { "main", "location_db", "user_db" };
	for (int i = 0; i < _countof(dbName); ++i)
	{
		string sql = "pragma ";
		sql += dbName[i];
		sql += '.';
		sql += pragma;
		sql += ';';
		m_flySQLiteDB.executenonquery(sql);
	}
}

bool CFlylinkDBManager::safeAlter(const char* p_sql, bool p_is_all_log /*= false */)
{
	try
	{
		m_flySQLiteDB.executenonquery(p_sql);
		return true;
	}
	catch (const database_error& e)
	{
		if (p_is_all_log == true || e.getError().find("duplicate column name:") == string::npos) // Логируем только неизвестные ошибки
		{
			LogManager::message("safeAlter: " + e.getError());
		}
	}
	return false;
}

string CFlylinkDBManager::getDBInfo(string& root)
{
	string message;
	string path = Util::getConfigPath();
	dcassert(!path.empty());
	if (path.size() > 2)
	{
		g_SQLiteDBSize = 0;
		message = STRING(DATABASE_LOCATIONS);
		message += '\n';
		for (int i = 0; i < _countof(fileNames); ++i)
		{
			string filePath = path + fileNames[i];
			FileAttributes attr;
			if (File::getAttributes(filePath, attr))
			{
				auto size = attr.getSize();
				message += "  * ";
				message += filePath;
				g_SQLiteDBSize += size;
				message += " (" + Util::formatBytes(size) + ")\n";
			}
		}
		
		root = path.substr(0, 2);
		g_SQLiteDBSizeFree = 0;
		if (GetDiskFreeSpaceExA(root.c_str(), (PULARGE_INTEGER)&g_SQLiteDBSizeFree, NULL, NULL))
		{
			message += '\n';
			message += STRING_F(DATABASE_DISK_INFO, root % Util::formatBytes(g_SQLiteDBSizeFree));
			message += '\n';
		}
		else
		{
			dcassert(0);
		}
	}
	return g_SQLiteDBSize ? message : string();
}

void CFlylinkDBManager::errorDB(const string& text, int errorCode)
{
	LogManager::message(text);
	if (errorCode == SQLITE_OK)
		return;
	
	string root, message;
	string dbInfo = getDBInfo(root);
	bool forceExit = false;	
	
	if (errorCode == SQLITE_READONLY)
	{
		message = STRING_F(DATABASE_READ_ONLY, APPNAME);
		message += "\n\n";
		forceExit = true;
	}
	else
	if (errorCode == SQLITE_CORRUPT || errorCode == SQLITE_CANTOPEN || errorCode == SQLITE_IOERR)
	{
		message = STRING(DATABASE_CORRUPTED);
		message += "\n\n";
		forceExit = true;
	}
	else
	if (errorCode == SQLITE_FULL)
	{
		message = STRING_F(DATABASE_DISK_FULL, root);
		message += "\n\n";
		forceExit = true;
	}
	static int g_MessageBox = 0;
	{
		CFlyBusy busy(g_MessageBox);
		if (g_MessageBox <= 1)
		{
			message += STRING_F(DATABASE_ERROR_STRING, text);
			message += "\n\n";
			message += dbInfo;
			MessageBox(NULL, Text::toT(message).c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
		}
	}
}

CFlylinkDBManager::CFlylinkDBManager()
{
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	globalRatio.download = globalRatio.upload = 0;
#endif

	CFlyLock(m_cs);
	deleteOldTransfers = true;
	try
	{
		// http://www.sql.ru/forum/1034900/executenonquery-ne-podkluchaet-dopolnitelnyy-fayly-tablic-bd-esli-v-puti-k-nim-est
		TCHAR l_dir_buffer[MAX_PATH];
		l_dir_buffer[0] = 0;
		DWORD dwRet = GetCurrentDirectory(MAX_PATH, l_dir_buffer);
		if (!dwRet)
		{
			errorDB("SQLite - CFlylinkDBManager: error GetCurrentDirectory " + Util::translateError());
		}
		else
		{
			if (SetCurrentDirectory(Text::toT(Util::getConfigPath()).c_str()))
			{
				auto l_status = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
				if (l_status != SQLITE_OK)
				{
					LogManager::message("[Error] sqlite3_config(SQLITE_CONFIG_SERIALIZED) = " + Util::toString(l_status));
				}
				dcassert(l_status == SQLITE_OK);
				l_status = sqlite3_initialize();
				if (l_status != SQLITE_OK)
				{
					LogManager::message("[Error] sqlite3_initialize = " + Util::toString(l_status));
				}
				dcassert(l_status == SQLITE_OK);
				
				m_flySQLiteDB.open("FlylinkDC.sqlite");
				sqlite3_busy_handler(m_flySQLiteDB.getdb(), gf_busy_handler, this);
				// m_flySQLiteDB.setbusytimeout(1000);
				// TODO - sqlite3_busy_handler
				// Пример реализации обработчика -
				// https://github.com/iso9660/linux-sdk/blob/d819f98a72776fced31131b1bc22a4bcb4c492bb/SDKLinux/LFC/Data/sqlite3db.cpp
				// https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=17660
				if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
				{
					sqlite3_trace(m_flySQLiteDB.getdb(), gf_trace_callback, NULL);
					// sqlite3_profile(m_flySQLiteDB.getdb(), profile_callback, NULL);
				}
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_locations.sqlite' as location_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_user.sqlite' as user_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_transfers.sqlite' as transfer_db");
#ifdef FLYLINKDC_USE_TORRENT
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_queue.sqlite' as queue_db");
#endif
				
#ifdef FLYLINKDC_USE_LEVELDB
				// Тут обязательно полный путь. иначе при смене рабочего каталога levelDB не сомжет открыть базу.
				const string l_full_path_level_db = Text::fromUtf8(Util::getConfigPath() + "tth-history.leveldb");
				
				bool l_is_destroy = false;
				m_TTHLevelDB.open_level_db(l_full_path_level_db, l_is_destroy);
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
				l_full_path_level_db = Util::getConfigPath() + "ip-history.leveldb";
				m_IPCacheLevelDB.open_level_db(l_full_path_level_db);
				g_IPCacheLevelDBSize = File::calcFilesSize(l_full_path_level_db, "\\*.*");
#endif
#endif // FLYLINKDC_USE_LEVELDB

#ifdef FLYLINKDC_USE_LMDB
				lmdb.open();
#endif

				SetCurrentDirectory(l_dir_buffer);
#ifdef FLYLINKDC_USE_LEVELDB
				if (l_is_destroy)
				{
					convert_tth_history();
				}
#endif
			}
			else
			{
				errorDB("SQLite - CFlylinkDBManager: error SetCurrentDirectory l_db_path = " + Util::getConfigPath()
				        + " Error: " +  Util::translateError());
			}
		}
		
		string root;
		string dbInfo = getDBInfo(root);
		if (!dbInfo.empty())
			LogManager::message(dbInfo, false);
		const string l_thread_type = "sqlite3_threadsafe() = " + Util::toString(sqlite3_threadsafe());
		LogManager::message(l_thread_type);
		dcassert(sqlite3_threadsafe() >= 1);
		
		setPragma("page_size=4096");
		if (g_DisableSQLJournal || BOOLSETTING(SQLITE_USE_JOURNAL_MEMORY))
		{
			setPragma("journal_mode=MEMORY");
		}
		else
		{
			if (g_UseWALJournal)
			{
				setPragma("journal_mode=WAL");
			}
			else
			{
				setPragma("journal_mode=PERSIST");
			}
			setPragma("journal_size_limit=16384"); // http://src.chromium.org/viewvc/chrome/trunk/src/sql/connection.cc
			setPragma("secure_delete=OFF"); // http://www.sqlite.org/pragma.html#pragma_secure_delete
			if (g_UseSynchronousOff)
			{
				setPragma("synchronous=OFF");
			}
			else
			{
				setPragma("synchronous=FULL");
			}
		}
		setPragma("temp_store=MEMORY");

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		bool hasRatioTable = hasTable("fly_ratio");
		bool hasUserTable = hasTable("user_info", "user_db");
		bool hasNewStatTables = hasTable("ip_stat") || hasTable("user_stat", "user_db");

		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS ip_stat("
		                              "cid blob not null, ip text not null, upload integer not null, download integer not null)");;
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_ip_stat ON ip_stat(cid, ip);");

		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS user_db.user_stat("
		                              "cid blob primary key, nick text not null, last_ip text not null, message_count integer not null, pm_in integer not null, pm_out integer not null)");
#endif

		const int l_db_user_version = m_flySQLiteDB.executeint("PRAGMA user_version");
		
//		if (l_rev <= 379)
		{
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_ignore(nick text PRIMARY KEY NOT NULL);");
		}
//     if (l_rev <= 381)
		{
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_registry(segment integer not null, key text not null,val_str text, val_number int64,tick_count int not null);");
			try
			{
				m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_registry_key ON fly_registry(segment,key);");
			}
			catch (const database_error&)
			{
				m_flySQLiteDB.executenonquery("delete from fly_registry");
				m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_registry_key ON fly_registry(segment,key);");
			}
		}
		m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_geoip");
		if (hasTable("fly_country_ip"))
		{
			// Перезагрузим локации в отдельный файл базы
			// Для этого скинем метку времени для файлов данных ч тобы при следующем запуске выполнилась перезагрузка
			// и удалим таблицы в основной базе данных
			setRegistryVarInt(e_TimeStampGeoIP, 0);
			setRegistryVarInt(e_TimeStampCustomLocation, 0);
			m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_country_ip");
			m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_location_ip");
			m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_location_ip_lost");
		}
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_p2pguard_ip(start_ip integer not null,stop_ip integer not null,note text,type integer);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_ip ON fly_p2pguard_ip(start_ip);");
		m_flySQLiteDB.executenonquery("DROP INDEX IF EXISTS location_db.i_fly_p2pguard_note;");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_type ON fly_p2pguard_ip(type);");
		safeAlter("ALTER TABLE location_db.fly_p2pguard_ip add column type integer");
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_country_ip(start_ip integer not null,stop_ip integer not null,country text,flag_index integer);");
		safeAlter("ALTER TABLE location_db.fly_country_ip add column country text");
		
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_country_ip ON fly_country_ip(start_ip);");
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_location_ip(start_ip integer not null,stop_ip integer not null,location text,flag_index integer);");
		    
		safeAlter("ALTER TABLE location_db.fly_location_ip add column location text");
		
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS "
		                              "location_db.i_fly_location_ip ON fly_location_ip(start_ip);");
		                              
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "tth char(39),path text not null,nick text, hub text,size int64 not null,speed int,ip text, actual int64);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_day_type ON fly_transfer_file(day,type);");
		
		safeAlter("ALTER TABLE transfer_db.fly_transfer_file add column actual int64");

#ifdef FLYLINKDC_USE_TORRENT
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file_torrent("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "sha1 char(20),path text,size int64 not null);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_torrentday_type ON fly_transfer_file_torrent(day,type);");

		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS queue_db.fly_queue_torrent("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,day int64 not null,stamp int64 not null,sha1 char(20) NOT NULL,resume blob, magnet string,name string);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS queue_db.iu_fly_queue_torrent_sha1 ON fly_queue_torrent(sha1);");
#endif
		
		if (l_db_user_version < 1)
		{
			m_flySQLiteDB.executenonquery("PRAGMA user_version=1");
		}
		if (l_db_user_version < 2)
		{
			// Удаляю уже на уровне конвертора файла.
			// m_flySQLiteDB.executenonquery("delete from location_db.fly_p2pguard_ip where note like '%VimpelCom%'");
			m_flySQLiteDB.executenonquery("PRAGMA user_version=2");
		}
		if (l_db_user_version < 3)
		{
			m_flySQLiteDB.executenonquery("PRAGMA user_version=3");
		}
		if (l_db_user_version < 4)
		{
			m_flySQLiteDB.executenonquery("PRAGMA user_version=4");
		}
		if (l_db_user_version < 5)
		{
			deleteOldTransferHistoryL();
			m_flySQLiteDB.executenonquery("PRAGMA user_version=5");
		}
		
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		if (!hasNewStatTables && (hasRatioTable || hasUserTable))
			convertStatTables(hasRatioTable, hasUserTable);
		timeLoadGlobalRatio = 0;
#endif
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - CFlylinkDBManager: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::flush()
{
}

void CFlylinkDBManager::saveLocation(const vector<LocationInfo>& data)
{
	CFlyLock(m_cs);
	try
	{
		CFlyBusy busy(g_DisableSQLtrace);
		sqlite3_transaction trans(m_flySQLiteDB);
		initQuery2(deleteLocation, "delete from location_db.fly_location_ip");
		deleteLocation.executenonquery();
		initQuery2(insertLocation, "insert into location_db.fly_location_ip (start_ip,stop_ip,location,flag_index) values(?,?,?,?)");
		for (const auto& val : data)
		{
			dcassert(val.startIp && !val.location.empty());
			insertLocation.bind(1, val.startIp);
			insertLocation.bind(2, val.endIp);
			insertLocation.bind(3, val.location, SQLITE_STATIC);
			insertLocation.bind(4, val.imageIndex);
			insertLocation.executenonquery();
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveLocation: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::getIPInfo(uint32_t ip, IPInfo& result, int what, bool onlyCached)
{
	dcassert(what);
	dcassert(ip);
	{
		CFlyFastLock(csIpCache);
		IpCacheItem* item = ipCache.get(ip);
		if (item)
		{
			ipCache.makeNewest(item);
			int found = what & item->info.known;
			if (found & IPInfo::FLAG_COUNTRY)
			{
				result.country = item->info.country;
				result.countryImage = item->info.countryImage;
			}
			if (found & IPInfo::FLAG_LOCATION)
			{
				result.location = item->info.location;
				result.locationImage = item->info.locationImage;
			}
			if (found & IPInfo::FLAG_P2P_GUARD)
				result.p2pGuard = item->info.p2pGuard;
			result.known |= found;
			what &= ~found;
			if (!what) return;
		}
	}
	if (onlyCached) return;
	if (what & IPInfo::FLAG_COUNTRY)
		loadCountry(ip, result);
	if (what & IPInfo::FLAG_LOCATION)
		loadLocation(ip, result);
	if (what & IPInfo::FLAG_P2P_GUARD)
		loadP2PGuard(ip, result);
	CFlyFastLock(csIpCache);
	IpCacheItem* storedItem;
	IpCacheItem newItem;
	newItem.info = result;
	newItem.key = ip;
	if (!ipCache.add(newItem, &storedItem))
	{
		if (result.known & IPInfo::FLAG_COUNTRY)
		{
			storedItem->info.known |= IPInfo::FLAG_COUNTRY;
			storedItem->info.country = result.country;
			storedItem->info.countryImage = result.countryImage;
		}
		if (result.known & IPInfo::FLAG_LOCATION)
		{
			storedItem->info.known |= IPInfo::FLAG_LOCATION;
			storedItem->info.location = result.location;
			storedItem->info.locationImage = result.locationImage;
		}
		if (result.known & IPInfo::FLAG_P2P_GUARD)
		{
			storedItem->info.known |= IPInfo::FLAG_P2P_GUARD;
			storedItem->info.p2pGuard = result.p2pGuard;
		}
	}
	ipCache.removeOldest(IP_CACHE_SIZE + 1);
}

void CFlylinkDBManager::loadCountry(uint32_t ip, IPInfo& result)
{
	result.clearCountry();
	dcassert(ip);

	if (Util::isPrivateIp(ip))
	{
		result.known |= IPInfo::FLAG_COUNTRY;
		return;
	}

	CFlyLock(m_cs);
	try
	{		
		initQuery2(selectCountry,
			"select country,flag_index,start_ip,stop_ip from "
			"(select country,flag_index,start_ip,stop_ip from location_db.fly_country_ip where start_ip <=? order by start_ip desc limit 1) "
			"where stop_ip >=?");
		selectCountry.bind(1, ip);
		selectCountry.bind(2, ip);
		int countryHits = 0;
		sqlite3_reader reader = selectCountry.executereader();
		while (reader.read())
		{
			countryHits++;
			result.country = reader.getstring(0);
			result.countryImage = reader.getint(1);
		}
		dcassert(countryHits <= 1);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadCountry: " + e.getError(), e.getErrorCode());
	}
	result.known |= IPInfo::FLAG_COUNTRY;
}

void CFlylinkDBManager::loadLocation(uint32_t ip, IPInfo& result)
{
	result.clearLocation();
	dcassert(ip);

	CFlyLock(m_cs);
	try
	{		
		initQuery2(selectLocation,
			"select location,flag_index,start_ip,stop_ip from "
			"(select location,flag_index,start_ip,stop_ip from location_db.fly_location_ip where start_ip <=? order by start_ip desc limit 1) "
			"where stop_ip >=?");
		selectLocation.bind(1, ip);
		selectLocation.bind(2, ip);		
		sqlite3_reader reader = selectLocation.executereader();
		while (reader.read())
		{
			result.location = reader.getstring(0);
			result.locationImage = reader.getint(1);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadLocation: " + e.getError(), e.getErrorCode());
	}
	result.known |= IPInfo::FLAG_LOCATION;
}

void CFlylinkDBManager::loadP2PGuard(uint32_t ip, IPInfo& result)
{
	result.p2pGuard.clear();
	dcassert(ip);

	CFlyLock(m_cs);
	try
	{		
		initQuery2(selectP2PGuard,
			"select note,start_ip,stop_ip,2 from "
		    "(select note,start_ip,stop_ip from location_db.fly_p2pguard_ip where start_ip <=? order by start_ip desc limit 1) "
			"where stop_ip >=?");
		selectP2PGuard.bind(1, ip);
		selectP2PGuard.bind(2, ip);		
		sqlite3_reader reader = selectP2PGuard.executereader();
		while (reader.read())
		{
			if (!result.p2pGuard.empty())
			{
				result.p2pGuard += " + ";
				result.p2pGuard += reader.getstring(0);
			}
			else
				result.p2pGuard = reader.getstring(0);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadP2PGuard: " + e.getError(), e.getErrorCode());
	}
	result.known |= IPInfo::FLAG_P2P_GUARD;
}

void CFlylinkDBManager::removeManuallyBlockedIP(uint32_t ip)
{
	{
		CFlyFastLock(csIpCache);
		IpCacheItem* item = ipCache.get(ip);
		if (item)
		{
			item->info.known &= ~IPInfo::FLAG_P2P_GUARD;
			item->info.p2pGuard.clear();
		}
	}
	CFlyLock(m_cs);
	try
	{
		if (deleteManuallyBlockedIP.empty())
		{
			string stmt = "delete from location_db.fly_p2pguard_ip where start_ip=? and type=" + Util::toString(PG_DATA_MANUAL);
			deleteManuallyBlockedIP.open(&m_flySQLiteDB, stmt.c_str());
		}
		deleteManuallyBlockedIP.bind(1, ip);
		deleteManuallyBlockedIP.executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - removeManuallyBlockedIP: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::loadManuallyBlockedIPs(vector<P2PGuardBlockedIP>& result)
{	
	result.clear();
	CFlyLock(m_cs);
	try
	{
		if (selectManuallyBlockedIP.empty())
		{
			string stmt = "select distinct start_ip,note from location_db.fly_p2pguard_ip where type=" + Util::toString(PG_DATA_MANUAL);
			selectManuallyBlockedIP.open(&m_flySQLiteDB, stmt.c_str());
		}
		sqlite3_reader reader = selectManuallyBlockedIP.executereader();
		while (reader.read())
			result.emplace_back(reader.getint(0), reader.getstring(1));
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadManuallyBlockedIPs: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::saveP2PGuardData(const vector<P2PGuardData>& data, int type, bool removeOld)
{
	CFlyLock(m_cs);
	try
	{
		CFlyBusy busy(g_DisableSQLtrace);
		sqlite3_transaction trans(m_flySQLiteDB);
		if (removeOld)
		{
			initQuery2(deleteP2PGuard, "delete from location_db.fly_p2pguard_ip where (type=? or type is null)");
			deleteP2PGuard.bind(1, type);
			deleteP2PGuard.executenonquery();
		}
		initQuery2(insertP2PGuard, "insert into location_db.fly_p2pguard_ip (start_ip,stop_ip,note,type) values(?,?,?,?)");
		for (const auto& val : data)
		{
			dcassert(!val.note.empty());
			insertP2PGuard.bind(1, val.startIp);
			insertP2PGuard.bind(2, val.endIp);
			insertP2PGuard.bind(3, val.note, SQLITE_STATIC);
			insertP2PGuard.bind(4, type);
			insertP2PGuard.executenonquery();
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveP2PGuardData: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::saveGeoIpCountries(const vector<LocationInfo>& data)
{
	CFlyLock(m_cs);
	try
	{
		CFlyBusy busy(g_DisableSQLtrace);
		sqlite3_transaction trans(m_flySQLiteDB);
		initQuery2(deleteCountry, "delete from location_db.fly_country_ip");
		deleteCountry.executenonquery();
		initQuery2(insertCountry, "insert into location_db.fly_country_ip (start_ip,stop_ip,country,flag_index) values(?,?,?,?)");
		for (const auto& val : data)
		{
			dcassert(val.startIp && !val.location.empty());
			insertCountry.bind(1, val.startIp);
			insertCountry.bind(2, val.endIp);
			insertCountry.bind(3, val.location, SQLITE_STATIC);
			insertCountry.bind(4, val.imageIndex);
			insertCountry.executenonquery();
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveGeoIpCountries: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::clearCachedP2PGuardData(uint32_t ip)
{
	CFlyFastLock(csIpCache);
	IpCacheItem* item = ipCache.get(ip);
	if (item)
	{
		item->info.known &= ~IPInfo::FLAG_P2P_GUARD;
		item->info.p2pGuard.clear();
	}
}

void CFlylinkDBManager::clearIpCache()
{
	CFlyFastLock(csIpCache);
	ipCache.clear();
}

void CFlylinkDBManager::setRegistryVarString(DBRegistryType type, const string& value)
{
	DBRegistryMap m;
	m[Util::toString(type)] = value;
	saveRegistry(m, type, false);
}

string CFlylinkDBManager::getRegistryVarString(DBRegistryType type)
{
	DBRegistryMap m;
	loadRegistry(m, type);
	if (!m.empty())
		return m.begin()->second.sval;
	else
		return Util::emptyString;
}

void CFlylinkDBManager::setRegistryVarInt(DBRegistryType type, int64_t value)
{
	DBRegistryMap m;
	m[Util::toString(type)] = DBRegistryValue(value);
	saveRegistry(m, type, false);
}

int64_t CFlylinkDBManager::getRegistryVarInt(DBRegistryType type)
{
	DBRegistryMap m;
	loadRegistry(m, type);
	if (!m.empty())
		return m.begin()->second.ival;
	else
		return 0;
}

void CFlylinkDBManager::loadRegistry(DBRegistryMap& values, DBRegistryType type)
{
	CFlyLock(m_cs);
	try
	{
		initQuery2(selectRegistry, "select key,val_str,val_number from fly_registry where segment=? order by rowid");
		selectRegistry.bind(1, type);
		sqlite3_reader reader = selectRegistry.executereader();
		while (reader.read())
		{
			auto res = values.insert(DBRegistryMap::value_type(reader.getstring(0),
			                         DBRegistryValue(reader.getstring(1), reader.getint64(2))));
			dcassert(res.second);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadRegistry: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::clearRegistry(DBRegistryType type, int64_t tick)
{
	CFlyLock(m_cs);
	try
	{
		clearRegistryL(type, tick);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - clearRegistry: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::clearRegistryL(DBRegistryType type, int64_t tick)
{
	initQuery2(deleteRegistry, "delete from fly_registry where segment=? and tick_count<>?");
	deleteRegistry.bind(1, type);
	deleteRegistry.bind(2, tick);
	deleteRegistry.executenonquery();
}

void CFlylinkDBManager::saveRegistry(const DBRegistryMap& values, DBRegistryType type, bool clearOldValues)
{
	CFlyLock(m_cs);
	try
	{
		const int64_t tick = getRandValForRegistry();
		initQuery2(insertRegistry, "insert or replace into fly_registry (segment,key,val_str,val_number,tick_count) values(?,?,?,?,?)");
		initQuery2(updateRegistry, "update fly_registry set val_str=?,val_number=?,tick_count=? where segment=? and key=?");
		sqlite3_transaction trans(m_flySQLiteDB, values.size() > 1 || clearOldValues);
		for (auto k = values.cbegin(); k != values.cend(); ++k)
		{
			const auto& val = k->second;
			updateRegistry.bind(1, val.sval, SQLITE_TRANSIENT);
			updateRegistry.bind(2, val.ival);
			updateRegistry.bind(3, tick);
			updateRegistry.bind(4, int(type));
			updateRegistry.bind(5, k->first, SQLITE_TRANSIENT);
			updateRegistry.executenonquery();
			if (m_flySQLiteDB.changes() == 0)
			{
				insertRegistry.bind(1, int(type));
				insertRegistry.bind(2, k->first, SQLITE_TRANSIENT);
				insertRegistry.bind(3, val.sval, SQLITE_TRANSIENT);
				insertRegistry.bind(4, val.ival);
				insertRegistry.bind(5, tick);
				insertRegistry.executenonquery();
			}
		}
		if (clearOldValues)
			clearRegistryL(type, tick);
		trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveRegistry: " + e.getError(), e.getErrorCode());
	}
}

int64_t CFlylinkDBManager::getRandValForRegistry()
{
	int64_t val;
	while (true)
	{
		val = ((int64_t) Util::rand() << 31) ^ Util::rand();
		initQuery2(selectTick, "select count(*) from fly_registry where tick_count=?");
		selectTick.bind(1, val);
		if (selectTick.executeint() == 0) break;
	}
	return val;
}

static string makeDeleteOldTransferHistory(const string& tableName, int currentDay)
{
	string sql = "delete from transfer_db.";
	sql += tableName;
	sql += " where (type=1 and day<" + Util::toString(currentDay - SETTING(DB_LOG_FINISHED_UPLOADS));
	sql += ") or (type=0 and day<" + Util::toString(currentDay - SETTING(DB_LOG_FINISHED_DOWNLOADS));
	sql += ")";
	return sql;
}

void CFlylinkDBManager::deleteOldTransferHistoryL()
{
	if ((SETTING(DB_LOG_FINISHED_DOWNLOADS) || SETTING(DB_LOG_FINISHED_UPLOADS)))
	{
		int64_t timestamp = posixTimeToLocal(time(nullptr));
		int currentDay = timestamp/(60*60*24);

		string sqlDC = makeDeleteOldTransferHistory("fly_transfer_file", currentDay);
		sqlite3_command cmdDC(&m_flySQLiteDB, sqlDC);
		cmdDC.executenonquery();

#ifdef FLYLINKDC_USE_TORRENT
		string sqlTorrent = makeDeleteOldTransferHistory("fly_transfer_file_torrent", currentDay);
		sqlite3_command cmdTorrent(&m_flySQLiteDB, sqlTorrent);
		cmdTorrent.executenonquery();
#endif
	}
}

void CFlylinkDBManager::loadTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out)
{
	CFlyLock(m_cs);
	try
	{
		if (deleteOldTransfers)
		{
			deleteOldTransfers = false;
			deleteOldTransferHistoryL();
		}

		initQuery2(selectTransfersSummary,
			"select day,count(*),sum(actual) "
			"from transfer_db.fly_transfer_file where type=? group by day order by day desc");
		selectTransfersSummary.bind(1, type);
		sqlite3_reader reader = selectTransfersSummary.executereader();
		while (reader.read())
		{
			TransferHistorySummary item;
			item.dateAsInt = reader.getint(0);
			item.count = reader.getint(1);
			item.actual = reader.getint64(2);
			item.date = static_cast<time_t>(item.dateAsInt*(60*60*24));
			out.push_back(item);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadTransferHistorySummary: " + e.getError(), e.getErrorCode());
	}
}

#ifdef FLYLINKDC_USE_TORRENT
void CFlylinkDBManager::loadTorrentTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out)
{
	CFlyLock(m_cs);
	try
	{
		if (deleteOldTransfers)
		{
			deleteOldTransfers = false;
			deleteOldTransferHistoryL();
		}

		initQuery2(selectTransfersSummaryTorrent,
			"select day,count(*) "
			"from transfer_db.fly_transfer_file_torrent where type=? group by day order by day desc");
		selectTransfersSummaryTorrent.bind(1, type);
		sqlite3_reader reader = selectTransfersSummaryTorrent.executereader();
		while (reader.read())
		{
			TransferHistorySummary item;
			item.dateAsInt = reader.getint(0);
			item.count = reader.getint(1);
			// TODO: fill item.actual
			item.date = static_cast<time_t>(item.dateAsInt*(60*60*24));
			out.push_back(item);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadTorrentTransferHistorySummary: " + e.getError(), e.getErrorCode());
	}
}
#endif

void CFlylinkDBManager::loadTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out)
{
	CFlyLock(m_cs);
	try
	{
		initQuery2(selectTransfersDay,
			"select path,nick,hub,size,speed,stamp,ip,tth,id,actual "
			"from transfer_db.fly_transfer_file where type=? and day=?");
		selectTransfersDay.bind(1, type);
		selectTransfersDay.bind(2, day);
		sqlite3_reader reader = selectTransfersDay.executereader();
		
		while (reader.read())
		{
			auto item = std::make_shared<FinishedItem>(reader.getstring(0), // target
			                                           reader.getstring(1), // nick
			                                           reader.getstring(2), // hub
			                                           reader.getint64(3), // size
			                                           reader.getint64(4), // speed
			                                           reader.getint64(5), // time
			                                           TTHValue(reader.getstring(7)), // TTH
			                                           reader.getstring(6), // IP
			                                           reader.getint64(9), // actual
			                                           reader.getint64(8)); // id
			out.push_back(item);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadTransferHistory: " + e.getError(), e.getErrorCode());
	}
}

#ifdef FLYLINKDC_USE_TORRENT
void CFlylinkDBManager::loadTorrentTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out)
{
	CFlyLock(m_cs);
	try
	{
		initQuery2(selectTransfersDayTorrent,
			"select path,sha1,size,stamp,id "
			"from transfer_db.fly_transfer_file_torrent where type=? and day=?");
		selectTransfersDayTorrent.bind(1, type);
		selectTransfersDayTorrent.bind(2, day);
		sqlite3_reader reader = selectTransfersDayTorrent.executereader();
		
		while (reader.read())
		{
			libtorrent::sha1_hash sha1;
			reader.getblob(1, sha1.data(), sha1.size());
			auto item = std::make_shared<FinishedItem>(reader.getstring(0), // target
			                                           reader.getint64(2), // size
			                                           0, // speed
			                                           reader.getint64(3), // time
			                                           sha1, // SHA1
			                                           0, // actual
			                                           reader.getint64(4)); // id
			out.push_back(item);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadTorrentTransferHistory: " + e.getError(), e.getErrorCode());
	}
}
#endif

void CFlylinkDBManager::deleteTransferHistory(const vector<int64_t>& id)
{
	if (id.empty()) return;
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction trans(m_flySQLiteDB, id.size() > 1);
		initQuery2(deleteTransfer, "delete from transfer_db.fly_transfer_file where id=?");
		for (auto i = id.cbegin(); i != id.cend(); ++i)
		{
			dcassert(*i);
			deleteTransfer.bind(1, *i);
			deleteTransfer.executenonquery();
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - deleteTransferHistory: " + e.getError(), e.getErrorCode());
	}
}

#ifdef FLYLINKDC_USE_TORRENT
void CFlylinkDBManager::deleteTorrentTransferHistory(const vector<int64_t>& id)
{
	if (id.empty()) return;
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction trans(m_flySQLiteDB, id.size() > 1);
		initQuery2(deleteTransferTorrent, "delete from transfer_db.fly_transfer_file_torrent where id=?");
		for (auto i = id.cbegin(); i != id.cend(); ++i)
		{
			dcassert(*i);
			deleteTransferTorrent.bind(1, *i);
			deleteTransferTorrent.executenonquery();
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - deleteTorrentTransferHistory: " + e.getError(), e.getErrorCode());
	}
}
#endif

#ifdef FLYLINKDC_USE_TORRENT
void CFlylinkDBManager::load_torrent_resume(libtorrent::session& p_session)
{
	try
	{
		initQuery(m_select_resume_torrent, "select resume,sha1 from queue_db.fly_queue_torrent");
		sqlite3_reader l_q = m_select_resume_torrent->executereader();
		while (l_q.read())
		{
			vector<uint8_t> l_resume;
			l_q.getblob(0, l_resume);
			//dcassert(!l_resume.empty());
			if (!l_resume.empty())
			{
				libtorrent::error_code ec;
				libtorrent::add_torrent_params p = libtorrent::read_resume_data({ (const char*)l_resume.data(), l_resume.size()}, ec);
				//p.save_path = SETTING(DOWNLOAD_DIRECTORY); // TODO - load from DB ?
				libtorrent::sha1_hash l_sha1;
				l_q.getblob(1, l_sha1.data(), l_sha1.size());
				p.info_hash = l_sha1;
				p.flags |= libtorrent::torrent_flags::auto_managed;
				{
					CFlyFastLock(g_resume_torrents_cs);
					g_resume_torrents.insert(l_sha1);
				}
#ifdef _DEBUG
				ec.clear();
				p_session.async_add_torrent(std::move(p)); // TODO sync for debug
				if (ec)
				{
					dcdebug("%s\n", ec.message().c_str());
					dcassert(0);
					LogManager::message("Error add_torrent_file: " + ec.message());
				}
#else
				p_session.async_add_torrent(std::move(p));
#endif
			}
			else
			{
				LogManager::torrent_message("Error add_torrent_file: resume data is empty()");
				// TODO delete_torrent_resume
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_torrent_resume: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::delete_torrent_resume(const libtorrent::sha1_hash& p_sha1)
{
	CFlyLock(m_cs);
	try
	{
		initQuery(m_delete_resume_torrent, "delete from queue_db.fly_queue_torrent where sha1=?");
		m_delete_resume_torrent->bind(1, p_sha1.data(), p_sha1.size(), SQLITE_STATIC);
		m_delete_resume_torrent->executenonquery();
		if (m_flySQLiteDB.changes() == 1)
		{
			CFlyFastLock(g_delete_torrents_cs);
			g_delete_torrents.insert(p_sha1);
		}
		else
		{
			dcassert(0); // Зовем второй раз удаление - не хорошо
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - delete_torrent_resume: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::save_torrent_resume(const libtorrent::sha1_hash& p_sha1, const std::string& p_name, const std::vector<char>& p_resume)
{
	CFlyLock(m_cs);
	try
	{
		initQuery(m_check_resume_torrent, "select resume,id from queue_db.fly_queue_torrent where sha1=?");
		m_check_resume_torrent->bind(1, p_sha1.data(), p_sha1.size(), SQLITE_STATIC);
		bool l_is_need_update = true;
		int64_t l_ID = 0;
		{
			sqlite3_reader l_q_check = m_check_resume_torrent->executereader();
			while (l_q_check.read())
			{
				l_ID = l_q_check.getint64(1);
				vector<uint8_t> l_resume;
				l_q_check.getblob(0, l_resume);
				//dcassert(!l_resume.empty());
				if (!l_resume.empty() && l_resume.size() == p_resume.size())
				{
					l_is_need_update = !memcmp(&p_resume[0], &l_resume[0], l_resume.size());
				}
			}
		}
		if (l_is_need_update)
		{
			if (l_ID == 0)
			{
				initQuery(m_insert_resume_torrent,
					"insert or replace into queue_db.fly_queue_torrent (day,stamp,sha1,resume,name) "
					"values(strftime('%s','now','localtime')/60/60/24,strftime('%s','now','localtime'),?,?,?)");
				m_insert_resume_torrent->bind(1, p_sha1.data(), p_sha1.size(), SQLITE_STATIC);
				m_insert_resume_torrent->bind(2, &p_resume[0], p_resume.size(), SQLITE_STATIC);
				m_insert_resume_torrent->bind(3, p_name, SQLITE_STATIC);
				m_insert_resume_torrent->executenonquery();
			}
			else
			{
				initQuery(m_update_resume_torrent,
					"update queue_db.fly_queue_torrent set day = strftime('%s','now','localtime')/60/60/24,"
					"stamp = strftime('%s','now','localtime'),"
					"resume = ?,"
					"name = ? where id = ?");
				m_update_resume_torrent->bind(1, &p_resume[0], p_resume.size(), SQLITE_STATIC);
				m_update_resume_torrent->bind(2, p_name, SQLITE_STATIC);
				m_update_resume_torrent->bind(3, l_ID);
				m_update_resume_torrent->executenonquery();
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_torrent_resume: " + e.getError(), e.getErrorCode());
	}
}
#endif

void CFlylinkDBManager::addTransfer(eTypeTransfer type, const FinishedItemPtr& item)
{
	int64_t timestamp = posixTimeToLocal(item->getTime());
	CFlyLock(m_cs);
	try
	{
#if 0
		string name = Text::toLower(Util::getFileName(item->getTarget()));
		string path = Text::toLower(Util::getFilePath(item->getTarget()));
		inc_hitL(path, name);
#endif
			
		initQuery2(insertTransfer,
			"insert into transfer_db.fly_transfer_file (type,day,stamp,path,nick,hub,size,speed,ip,tth,actual) "
			"values(?,?,?,?,?,?,?,?,?,?,?)");
		insertTransfer.bind(1, type);
		insertTransfer.bind(2, timestamp/(60*60*24));
		insertTransfer.bind(3, timestamp);
		insertTransfer.bind(4, item->getTarget(), SQLITE_STATIC);
		insertTransfer.bind(5, item->getNick(), SQLITE_STATIC);
		insertTransfer.bind(6, item->getHub(), SQLITE_STATIC);
		insertTransfer.bind(7, item->getSize());
		insertTransfer.bind(8, item->getAvgSpeed());
		insertTransfer.bind(9, item->getIP(), SQLITE_STATIC);
		if (!item->getTTH().isZero())
			insertTransfer.bind(10, item->getTTH().toBase32(), SQLITE_TRANSIENT); // SQLITE_TRANSIENT!
		else
			insertTransfer.bind(10);
		insertTransfer.bind(11, item->getActual());
		insertTransfer.executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - addTransfer: " + e.getError(), e.getErrorCode());
	}
}

#ifdef FLYLINKDC_USE_TORRENT
void CFlylinkDBManager::addTorrentTransfer(eTypeTransfer type, const FinishedItemPtr& item)
{
	int64_t timestamp = posixTimeToLocal(item->getTime());	
	CFlyLock(m_cs);
	try
	{
		initQuery2(insertTransferTorrent,
			"insert into transfer_db.fly_transfer_file_torrent (day,type,stamp,path,size,sha1) "
			"values(?,?,?,?,?,?)");
		insertTransferTorrent.bind(1, timestamp/(60*60*24));
		insertTransferTorrent.bind(2, type);
		insertTransferTorrent.bind(3, item->getTime());
		insertTransferTorrent.bind(4, item->getTarget(), SQLITE_STATIC);
		insertTransferTorrent.bind(5, item->getSize());
		if (!item->sha1.is_all_zeros())
			insertTransferTorrent.bind(6, item->sha1.data(), item->sha1.size(), SQLITE_STATIC);
		else
			insertTransferTorrent.bind(6);
		insertTransferTorrent.executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - addTorrentTransfer: " + e.getError(), e.getErrorCode());
	}
}
#endif

void CFlylinkDBManager::loadIgnoredUsers(StringSet& users)
{
	CFlyLock(m_cs);
	try
	{
		initQuery2(selectIgnoredUsers, "select trim(nick) from fly_ignore");
		sqlite3_reader reader = selectIgnoredUsers.executereader();
		while (reader.read())
		{
			string user = reader.getstring(0);
			if (!user.empty())
				users.insert(user);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadIgnoredUsers: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::saveIgnoredUsers(const StringSet& users)
{
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction trans(m_flySQLiteDB);
		initQuery2(deleteIgnoredUsers, "delete from fly_ignore");
		deleteIgnoredUsers.executenonquery();
		initQuery2(insertIgnoredUsers, "insert or replace into fly_ignore (nick) values(?)");
		for (auto k = users.cbegin(); k != users.cend(); ++k)
		{
			string user = (*k);
			boost::algorithm::trim(user);
			if (!user.empty())
			{
				insertIgnoredUsers.bind(1, user, SQLITE_TRANSIENT);
				insertIgnoredUsers.executenonquery();
			}
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveIgnoredUsers: " + e.getError(), e.getErrorCode());
	}
}

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
void CFlylinkDBManager::saveIPStat(const CID& cid, const vector<IPStatVecItem>& items)
{
	const int batchSize = 256;
	CFlyLock(m_cs);
	try
	{
		int count = 0;
		sqlite3_transaction trans(m_flySQLiteDB, false);
		for (const IPStatVecItem& item : items)
			saveIPStatL(cid, item.ip, item.item, batchSize, count, trans);
		if (count) trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveIPStat: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::saveIPStatL(const CID& cid, const string& ip, const IPStatItem& item, int batchSize, int& count, sqlite3_transaction& trans)
{
	if (!count) trans.begin();
	if (item.flags & IPStatItem::FLAG_LOADED)
	{
		initQuery2(updateIPStat, "update ip_stat set upload=?, download=? where cid=? and ip=?");
		updateIPStat.bind(1, static_cast<int64_t>(item.upload));
		updateIPStat.bind(2, static_cast<int64_t>(item.download));
		updateIPStat.bind(3, cid.data(), CID::SIZE, SQLITE_STATIC);
		updateIPStat.bind(4, ip, SQLITE_STATIC);
		updateIPStat.executenonquery();
	}
	else
	{
		initQuery2(insertIPStat, "insert into ip_stat values(?,?,?,?)"); // cid, ip, upload, download
		insertIPStat.bind(1, cid.data(), CID::SIZE, SQLITE_STATIC);
		insertIPStat.bind(2, ip, SQLITE_STATIC);
		insertIPStat.bind(3, static_cast<int64_t>(item.upload));
		insertIPStat.bind(4, static_cast<int64_t>(item.download));
		insertIPStat.executenonquery();
	}
	if (++count == batchSize)
	{
		trans.commit();
		count = 0;
	}
}

IPStatMap* CFlylinkDBManager::loadIPStat(const CID& cid)
{
	IPStatMap* ipStat = nullptr;
	CFlyLock(m_cs);
	try
	{
		initQuery2(selectIPStat, "select ip, upload, download from ip_stat where cid=?");
		selectIPStat.bind(1, cid.data(), CID::SIZE, SQLITE_STATIC);
		sqlite3_reader reader = selectIPStat.executereader();
		while (reader.read())
		{
			string ip = reader.getstring(0);
			int64_t upload = reader.getint64(1);
			int64_t download = reader.getint64(2);
			if (!ip.empty() && (upload > 0 || download > 0))
			{
				if (!ipStat) ipStat = new IPStatMap;
				if (ipStat->data.insert(make_pair(ip,
					IPStatItem{static_cast<uint64_t>(download), static_cast<uint64_t>(upload), IPStatItem::FLAG_LOADED})).second)
				{
					ipStat->totalDownloaded += download;
					ipStat->totalUploaded += upload;
				}
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadIPStat: " + e.getError(), e.getErrorCode());
	}
	return ipStat;
}

void CFlylinkDBManager::saveUserStatL(const CID& cid, UserStatItem& stat, int batchSize, int& count, sqlite3_transaction& trans)
{
	string nickList;
	for (const string& nick : stat.nickList)
	{
		if (!nickList.empty()) nickList += '\n';
		nickList += nick;
	}
	if (!count) trans.begin();
	if (stat.flags & UserStatItem::FLAG_LOADED)
	{
		initQuery2(updateUserStat, "update user_db.user_stat set nick=?, last_ip=?, message_count=? where cid=?");
		updateUserStat.bind(1, nickList, SQLITE_STATIC);
		updateUserStat.bind(2, stat.lastIp, SQLITE_STATIC);
		updateUserStat.bind(3, stat.messageCount);
		updateUserStat.bind(4, cid.data(), CID::SIZE, SQLITE_STATIC);
		updateUserStat.executenonquery();
	}
	else
	{
		initQuery2(insertUserStat, "insert into user_db.user_stat values(?,?,?,?,0,0)"); // cid, nick, last_ip, message_count
		insertUserStat.bind(1, cid.data(), CID::SIZE, SQLITE_STATIC);
		insertUserStat.bind(2, nickList, SQLITE_STATIC);
		insertUserStat.bind(3, stat.lastIp, SQLITE_STATIC);
		insertUserStat.bind(4, stat.messageCount);
		insertUserStat.executenonquery();
	}
	if (++count == batchSize)
	{
		trans.commit();
		count = 0;
	}
	stat.flags &= ~UserStatItem::FLAG_CHANGED;
}

void CFlylinkDBManager::saveUserStat(const CID& cid, UserStatItem& stat)
{
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction trans(m_flySQLiteDB, false);
		int count = 0;
		saveUserStatL(cid, stat, 1, count, trans);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveUserStat: " + e.getError(), e.getErrorCode());
	}
}

bool CFlylinkDBManager::loadUserStat(const CID& cid, UserStatItem& stat)
{
	bool result = false;
	CFlyLock(m_cs);
	try
	{
		initQuery2(selectUserStat, "select nick, last_ip, message_count from user_db.user_stat where cid=?");
		selectUserStat.bind(1, cid.data(), CID::SIZE, SQLITE_STATIC);
		sqlite3_reader reader = selectUserStat.executereader();
		if (reader.read())
		{
			string nick = reader.getstring(0);
			string lastIp = reader.getstring(1);
			int64_t messageCount = reader.getint(2);
			if (!nick.empty() && (!lastIp.empty() || messageCount > 0))
			{
				stat.messageCount = static_cast<unsigned>(messageCount);
				stat.lastIp = std::move(lastIp);
				stat.nickList.emplace_back(std::move(nick));
				result = true;
			}
			stat.flags |= UserStatItem::FLAG_LOADED;
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadUserStat: " + e.getError(), e.getErrorCode());
	}
	return result;
}

bool CFlylinkDBManager::convertStatTables(bool hasRatioTable, bool hasUserTable)
{
	enum eTypeDIC
	{
		e_DIC_HUB = 1,
		e_DIC_NICK = 2,
		e_DIC_IP = 3,
		e_DIC_LAST
	};

	struct ConvertUserStatItem
	{
		UserStatItem stat;
		IPStatMap* ipStat = nullptr;
		int64_t currentId = -1;
		string currentIp;
	};

	std::unordered_map<CID, ConvertUserStatItem> convMap;

	bool result = true;
	int insertedUsers = 0;
	int insertedIPs = 0;
	try
	{
		sqlite3_command selectDic(&m_flySQLiteDB, "select dic,id,name from fly_dic");
		sqlite3_reader reader = selectDic.executereader();
		std::unordered_map<int, string> values[e_DIC_LAST-1];
		while (reader.read())
		{
			int type = reader.getint(0);
			if (type >= e_DIC_HUB && type <= e_DIC_IP)
			{
				string value = reader.getstring(2);
				if (!value.empty())
					values[type-1][reader.getint(1)] = value;
			}
		}

		if (hasRatioTable)
		{
			sqlite3_command cmd(&m_flySQLiteDB, "select id,dic_ip,dic_nick,dic_hub,upload,download from fly_ratio");
			sqlite3_reader reader = cmd.executereader();
			while (reader.read())
			{
				int64_t id = reader.getint(0);

				auto it = values[e_DIC_HUB-1].find(reader.getint(3));
				if (it == values[e_DIC_HUB-1].end()) continue;
				string hub = it->second;
				if (Util::isAdcHub(hub)) continue;

				it = values[e_DIC_IP-1].find(reader.getint(1));
				if (it == values[e_DIC_IP-1].end()) continue;
				string ip = it->second;
			
				it = values[e_DIC_NICK-1].find(reader.getint(2));
				if (it == values[e_DIC_NICK-1].end()) continue;
				string nick = it->second;

				int64_t upload = reader.getint(4);
				int64_t download = reader.getint(5);

				TigerHash th;
				th.update(nick.c_str(), nick.length());
				th.update(hub.c_str(), hub.length());
				CID cid(th.finalize());

				ConvertUserStatItem& item = convMap[cid];
				if (id > item.currentId)
				{
					item.currentId = id;
					item.currentIp = ip;
				}
				if (upload <= 0 && download <= 0) continue;
				if (!item.ipStat) item.ipStat = new IPStatMap;
				IPStatItem& ipStatItem = item.ipStat->data[ip];
				if (upload > 0) ipStatItem.upload += upload;
				if (download > 0) ipStatItem.download += download;
			}
		}

		if (hasUserTable)
		{
			sqlite3_command cmd(&m_flySQLiteDB, "select nick,dic_hub,last_ip,message_count from user_db.user_info");
			sqlite3_reader reader = cmd.executereader();
			while (reader.read())
			{
				auto it = values[e_DIC_HUB-1].find(reader.getint(1));
				if (it == values[e_DIC_HUB-1].end()) continue;
				string hub = it->second;
				if (Util::isAdcHub(hub)) continue;

				string nick = reader.getstring(0);
				if (nick.empty()) continue;

				int64_t lastIp = reader.getint64(2);
				int64_t messageCount = reader.getint64(3);
				if (lastIp <= 0 && messageCount <= 0) continue;

				TigerHash th;
				th.update(nick.c_str(), nick.length());
				th.update(hub.c_str(), hub.length());
				CID cid(th.finalize());

				ConvertUserStatItem& item = convMap[cid];
				item.stat.lastIp = boost::asio::ip::address_v4(static_cast<uint32_t>(lastIp)).to_string();
				item.stat.addNick(nick, hub);
				if (messageCount > 0) item.stat.messageCount = static_cast<unsigned>(messageCount);
			}
		}

		for (auto& m : values) m.clear();

		int insertedCount = 0;
		const int batchSize = 256;
		sqlite3_transaction trans(m_flySQLiteDB, false);
		for (auto& item : convMap)
		{
			ConvertUserStatItem& statItem = item.second;
			if (statItem.stat.lastIp.empty() && !statItem.currentIp.empty())
				statItem.stat.lastIp = std::move(statItem.currentIp);
			if (!statItem.stat.lastIp.empty() || statItem.stat.messageCount)
			{
				saveUserStatL(item.first, statItem.stat, batchSize, insertedCount, trans);
				insertedUsers++;
			}
			if (statItem.ipStat)
				for (auto& val : statItem.ipStat->data)
				{
					saveIPStatL(item.first, val.first, val.second, batchSize, insertedCount, trans);
					val.second.flags &= ~IPStatItem::FLAG_CHANGED;
					insertedIPs++;
				}
		}
		if (insertedCount) trans.commit();
	}
	catch (const database_error& e)
	{
		result = false;
	}
	for (auto& item : convMap)
		delete item.second.ipStat;
	if (result && (insertedUsers || insertedIPs))
		LogManager::message("Stat tables converted: users=" + Util::toString(insertedUsers) + ", IPs=" + Util::toString(insertedIPs), false);
	return result;
}

void CFlylinkDBManager::loadGlobalRatio(bool force)
{
	uint64_t tick = GET_TICK();
	CFlyLock(m_cs);
	if (!force && tick < timeLoadGlobalRatio) return;
	try
	{
		initQuery2(selectGlobalRatio, "select total(upload), total(download) from ip_stat");
		sqlite3_reader reader = selectGlobalRatio.executereader();
		if (reader.read())
		{
			globalRatio.upload = reader.getint64(0);
			globalRatio.download = reader.getint64(1);
		}
		timeLoadGlobalRatio = tick + 10 * 60000;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - loadGlobalRatio: " + e.getError(), e.getErrorCode());
	}
}
#endif

bool CFlylinkDBManager::hasTable(const string& tableName, const string& db)
{
	dcassert(tableName == Text::toLower(tableName));
	string prefix = db;
	if (!prefix.empty()) prefix += '.';
	return m_flySQLiteDB.executeint("select count(*) from " + prefix + "sqlite_master where type = 'table' and lower(tbl_name) = '" + tableName + "'") != 0;
}

void CFlylinkDBManager::vacuum()
{
#ifdef FLYLINKDC_USE_VACUUM
	LogManager::message("start vacuum", true);
	m_flySQLiteDB.executenonquery("VACUUM;");
	LogManager::message("stop vacuum", true);
#endif
}

void CFlylinkDBManager::shutdown_engine()
{
	auto l_status = sqlite3_shutdown();
	if (l_status != SQLITE_OK)
	{
		LogManager::message("[Error] sqlite3_shutdown = " + Util::toString(l_status));
	}
	dcassert(l_status == SQLITE_OK);
}

CFlylinkDBManager::~CFlylinkDBManager()
{
	flush();
}

bool CFlylinkDBManager::getFileInfo(const TTHValue &tth, unsigned &flags, string &path)
{
#ifdef FLYLINKDC_USE_LMDB
	return lmdb.getFileInfo(tth.data, flags, path);
#else
	flags = 0;
	path.clear();
	return false;
#endif
}

bool CFlylinkDBManager::setFileInfoDownloaded(const TTHValue &tth, uint64_t fileSize, const string &path)
{
#ifdef FLYLINKDC_USE_LMDB
	if (tth.isZero()) return false;
	return lmdb.putFileInfo(tth.data, FLAG_DOWNLOADED, fileSize, path.empty() ? nullptr : &path);
#else
	return false;
#endif
}

bool CFlylinkDBManager::setFileInfoCanceled(const TTHValue &tth, uint64_t fileSize)
{
#ifdef FLYLINKDC_USE_LMDB
	if (tth.isZero()) return false;
	return lmdb.putFileInfo(tth.data, FLAG_DOWNLOAD_CANCELED, fileSize, nullptr);
#else
	return false;
#endif
}

bool CFlylinkDBManager::addTree(const TigerTree &tree)
{
	if (tree.getRoot().isZero())
	{
		dcassert(0);
		return false;
	}
	{
		CFlyLock(csTreeCache);
		treeCache.removeOldest(TREE_CACHE_SIZE);
		TreeCacheItem item;
		item.key = tree.getRoot();
		item.tree = tree;
		TreeCacheItem* prevItem;
		if (!treeCache.add(item, &prevItem))
		{
			if (tree.getLeaves().size() > prevItem->tree.getLeaves().size())
				prevItem->tree = tree;
			else
				return false;
		}
	}
#ifdef FLYLINKDC_USE_LMDB
	return lmdb.putTigerTree(tree);
#else
	return false;
#endif
}

bool CFlylinkDBManager::getTree(const TTHValue &tth, TigerTree &tree)
{
	if (tth.isZero())
		return false;
	{
		CFlyLock(csTreeCache);
		const TreeCacheItem* item = treeCache.get(tth);
		if (item)
		{
			tree = item->tree;
			return true;
		}
	}
#ifdef FLYLINKDC_USE_LMDB
	return lmdb.getTigerTree(tth.data, tree);
#else
	return false;
#endif
}

#ifdef FLYLINKDC_USE_TORRENT
bool CFlylinkDBManager::is_resume_torrent(const libtorrent::sha1_hash& p_sha1)
{
	CFlyFastLock(g_resume_torrents_cs);
	return g_resume_torrents.find(p_sha1) != g_resume_torrents.end();
}

bool CFlylinkDBManager::is_delete_torrent(const libtorrent::sha1_hash& p_sha1)
{
	CFlyFastLock(g_delete_torrents_cs);
	return g_delete_torrents.find(p_sha1) != g_delete_torrents.end();
}
#endif

#ifdef FLYLINKDC_USE_LEVELDB
CFlyLevelDB::CFlyLevelDB(): m_level_db(nullptr)
{
	m_readoptions.verify_checksums = true;
	m_readoptions.fill_cache = true;
	
	m_iteroptions.verify_checksums = true;
	m_iteroptions.fill_cache = false;
	
	m_writeoptions.sync      = true;
	
	m_options.compression = leveldb::kNoCompression;
	m_options.max_open_files = 10;
	m_options.block_size = 4096;
	m_options.write_buffer_size = 1 << 20;
	m_options.block_cache = leveldb::NewLRUCache(1 * 1024); // 1M
	m_options.paranoid_checks = true;
	m_options.filter_policy = leveldb::NewBloomFilterPolicy(10);
	m_options.create_if_missing = true;
}

CFlyLevelDB::~CFlyLevelDB()
{
	safe_delete(m_level_db);
	safe_delete(m_options.filter_policy);
	safe_delete(m_options.block_cache);
	// нельзя удалять - падаем. safe_delete(m_options.env);
	// TODO - leak delete m_options.comparator;
}

bool CFlyLevelDB::open_level_db(const string& p_db_name, bool& p_is_destroy)
{
	p_is_destroy = false;
	int64_t l_count_files = 0;
	int64_t l_size_files = 0;
	dcassert(m_level_db == nullptr);
	auto l_status = leveldb::DB::Open(m_options, p_db_name, &m_level_db, l_count_files, l_size_files);
	if (l_count_files > 1000)
	{
		safe_delete(m_level_db);
		const string l_new_name = p_db_name + ".old";
		const bool l_rename_result = File::renameFile(p_db_name, l_new_name);
#if 0
		if (l_rename_result)
		{
			CFlyServerJSON::pushError(90, "open_level_db rename : " + p_db_name + " - > " + l_new_name);
		}
		else
		{
			CFlyServerJSON::pushError(91, "open_level_db error rename : " + p_db_name + " - > " + l_new_name);
		}
#endif
		l_status = leveldb::DB::Open(m_options, p_db_name, &m_level_db, l_count_files, l_size_files);
	}
	dcassert(l_count_files != 0);
	if (!l_status.ok())
	{
		const auto l_result_error = l_status.ToString();
		if (l_status.IsIOError() || l_status.IsCorruption())
		{
			LogManager::message("[CFlyLevelDB::open_level_db] l_status.IsIOError() || l_status.IsCorruption() = " + l_result_error);
			//dcassert(0);
			const StringList l_delete_file = File::findFiles(p_db_name + '\\', "*.*");
			unsigned l_count_delete_error = 0;
			for (auto i = l_delete_file.cbegin(); i != l_delete_file.cend(); ++i)
			{
				if (i->size())
					if ((*i)[i->size() - 1] != '\\')
					{
						if (!File::deleteFile(*i))
						{
							++l_count_delete_error;
							LogManager::message("[CFlyLevelDB::open_level_db] error delete corrupt leveldb file  = " + *i);
						}
						else
						{
							LogManager::message("[CFlyLevelDB::open_level_db] OK delete corrupt leveldb file  = " + *i);
						}
					}
			}
			if (l_count_delete_error == 0)
			{
				l_count_files = 0;
				l_size_files = 0;
				// Create new leveldb-database
				l_status = leveldb::DB::Open(m_options, p_db_name, &m_level_db, l_count_files, l_size_files);
				if (l_status.ok())
				{
					LogManager::message("[CFlyLevelDB::open_level_db] OK Create new leveldb database: " + p_db_name);
					p_is_destroy = true;
				}
			}
			// most likely there's another instance running or the permissions are wrong
//			messageF(STRING_F(DB_OPEN_FAILED_IO, getNameLower() % Text::toUtf8(ret.ToString()) % APPNAME % dbPath % APPNAME), false, true);
//			exit(0);
		}
		else
		{
			LogManager::message("[CFlyLevelDB::open_level_db] !l_status.IsIOError() the database is corrupted? = " + l_result_error);
			dcassert(0);
			// the database is corrupted?
			// messageF(STRING_F(DB_OPEN_FAILED_REPAIR, getNameLower() % Text::toUtf8(ret.ToString()) % APPNAME), false, false);
			// repair(stepF, messageF);
			// try it again
			//ret = leveldb::DB::Open(options, l_pdb_name, &db);
		}
	}
	return l_status.ok();
}

bool CFlyLevelDB::get_value(const void* p_key, size_t p_key_len, string& p_result)
{
	dcassert(m_level_db);
	if (m_level_db)
	{
		const leveldb::Slice l_key((const char*)p_key, p_key_len);
		const auto l_status = m_level_db->Get(m_readoptions, l_key, &p_result);
		if (!(l_status.ok() || l_status.IsNotFound()))
		{
			const auto l_message = l_status.ToString();
			LogManager::message(l_message);
		}
		dcassert(l_status.ok() || l_status.IsNotFound());
		return l_status.ok() || l_status.IsNotFound();
	}
	else
	{
		return false;
	}
}

bool CFlyLevelDB::set_value(const void* p_key, size_t p_key_len, const void* p_val, size_t p_val_len)
{
	dcassert(m_level_db);
	if (m_level_db)
	{
		const leveldb::Slice l_key((const char*)p_key, p_key_len);
		const leveldb::Slice l_val((const char*)p_val, p_val_len);
		const auto l_status = m_level_db->Put(m_writeoptions, l_key, l_val);
		if (!l_status.ok())
		{
			const auto l_message = l_status.ToString();
			LogManager::message(l_message);
		}
		return l_status.ok();
	}
	else
	{
		return false;
	}
}

uint32_t CFlyLevelDB::set_bit(const TTHValue& p_tth, uint32_t p_mask)
{
	string l_value;
	if (get_value(p_tth, l_value))
	{
		uint32_t l_mask = Util::toInt(l_value);
		l_mask |= p_mask;
		if (set_value(p_tth, Util::toString(l_mask)))
		{
			return l_mask;
		}
	}
	dcassert(0);
	return 0;
}
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB

CFlyIPMessageCache CFlyLevelDBCacheIP::get_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick)
{
	CFlyIPMessageCache l_res;
	std::vector<char> l_key;
	create_key(p_hub_id, p_nick, l_key);
	string l_result;
	if (get_value(l_key.data(), l_key.size(), l_result))
	{
		if (!l_result.empty())
		{
			dcassert(l_result.size() == sizeof(CFlyIPMessageCache))
			if (l_result.size() == sizeof(CFlyIPMessageCache))
			{
				memcpy(&l_res, l_result.c_str(), sizeof(CFlyIPMessageCache));
			}
		}
	}
	return l_res;
}

void CFlyLevelDBCacheIP::set_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick, uint32_t p_message_count, const boost::asio::ip::address_v4& p_last_ip)
{

	std::vector<char> l_key;
	create_key(p_hub_id, p_nick, l_key);
	CFlyIPMessageCache l_value(p_message_count, p_last_ip.to_uint());
	set_value(l_key.data(), l_key.size(), &l_value, sizeof(l_value));
}
#endif // FLYLINKDC_USE_IPCACHE_LEVELDB

#endif // FLYLINKDC_USE_LEVELDB
