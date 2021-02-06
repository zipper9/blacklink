/*
  Blacklink's database manager (c) 2020
  Based on CFlylinkDBManager
 */

//-----------------------------------------------------------------------------
//(c) 2007-2018 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#include "stdinc.h"

#include "DatabaseManager.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include "FinishedManager.h"
#include "TimerManager.h"
#include "NetworkUtil.h"

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/read_resume_data.hpp"
#endif

using sqlite3x::database_error;
using sqlite3x::sqlite3_transaction;
using sqlite3x::sqlite3_reader;

bool g_DisableSQLJournal = false;
bool g_UseWALJournal = false;
bool g_EnableSQLtrace = false; // http://www.sqlite.org/c3ref/profile.html
bool g_UseSynchronousOff = false;
int g_DisableSQLtrace = 0;

#ifdef FLYLINKDC_USE_TORRENT
FastCriticalSection  DatabaseManager::g_resume_torrents_cs;
std::unordered_set<libtorrent::sha1_hash> DatabaseManager::g_resume_torrents;

FastCriticalSection  DatabaseManager::g_delete_torrents_cs;
std::unordered_set<libtorrent::sha1_hash> DatabaseManager::g_delete_torrents;
#endif

static const char* fileNames[] =
{
	"",
	"locations",
	"transfers",
	"user",
#ifdef FLYLINKDC_USE_TORRENT
	"queue"
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

static void traceCallback(void*, const char* sql)
{
	if (g_DisableSQLtrace == 0)
	{
		if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
		{
			StringMap params;
			params["sql"] = sql;
			params["thread_id"] = Util::toString(::GetCurrentThreadId());
			LOG(SQLITE_TRACE, params);
		}
	}
}

void DatabaseManager::initQuery(unique_ptr<sqlite3_command> &command, const char *sql)
{
	if (!command) command.reset(new sqlite3_command(&connection, sql));
}

void DatabaseManager::initQuery2(sqlite3_command &command, const char *sql)
{
	if (command.empty()) command.open(&connection, sql);
}

void DatabaseManager::setPragma(const char* pragma)
{
	static const char* dbName[] = { "main", "location_db", "user_db" };
	for (int i = 0; i < _countof(dbName); ++i)
	{
		string sql = "pragma ";
		sql += dbName[i];
		sql += '.';
		sql += pragma;
		sql += ';';
		connection.executenonquery(sql);
	}
}

bool DatabaseManager::safeAlter(const char* sql, bool verbose)
{
	try
	{
		connection.executenonquery(sql);
		return true;
	}
	catch (const database_error& e)
	{
		if (verbose || e.getError().find("duplicate column name:") == string::npos)
			LogManager::message("safeAlter: " + e.getError());
	}
	return false;
}

string DatabaseManager::getDBInfo(string& root)
{
	string message;
	string path = Util::getConfigPath();
	dcassert(!path.empty());
	if (path.size() > 2)
	{
		dbSize = 0;
		message = STRING(DATABASE_LOCATIONS);
		message += '\n';
		for (int i = 0; i < _countof(fileNames); ++i)
		{
			string filePath = path + prefix;
			if (*fileNames[i])
			{
				filePath += '_';
				filePath += fileNames[i];
			}
			filePath += ".sqlite";
			FileAttributes attr;
			if (File::getAttributes(filePath, attr))
			{
				auto size = attr.getSize();
				message += "  * ";
				message += filePath;
				dbSize += size;
				message += " (" + Util::formatBytes(size) + ")\n";
			}
		}
		
		root = path.substr(0, 2);
		int64_t freeSpace;
		if (GetDiskFreeSpaceExA(root.c_str(), (PULARGE_INTEGER) &freeSpace, nullptr, nullptr))
		{
			message += '\n';
			message += STRING_F(DATABASE_DISK_INFO, root % Util::formatBytes(freeSpace));
			message += '\n';
		}
		else
		{
			dcassert(0);
		}
	}
	return dbSize ? message : string();
}

void DatabaseManager::errorDB(const string& text, int errorCode)
{
	LogManager::message(text);
	if (errorCode == SQLITE_OK)
		return;

	if (!errorCallback)
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
	message += STRING_F(DATABASE_ERROR_STRING, text);
	message += "\n\n";
	message += dbInfo;
	errorCallback(message, forceExit);
}

DatabaseManager::DatabaseManager() noexcept
{
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	globalRatio.download = globalRatio.upload = 0;
#endif
	deleteOldTransfers = true;
	errorCallback = nullptr;
	dbSize = 0;
}

void DatabaseManager::init(ErrorCallback errorCallback)
{
	this->errorCallback = errorCallback;
	LOCK(cs);
	try
	{
		// http://www.sql.ru/forum/1034900/executenonquery-ne-podkluchaet-dopolnitelnyy-fayly-tablic-bd-esli-v-puti-k-nim-est
		TCHAR l_dir_buffer[MAX_PATH];
		l_dir_buffer[0] = 0;
		DWORD dwRet = GetCurrentDirectory(MAX_PATH, l_dir_buffer);
		if (!dwRet)
		{
			errorDB("SQLite - DatabaseManager: error GetCurrentDirectory " + Util::translateError());
		}
		else
		{
			if (SetCurrentDirectory(Text::toT(Util::getConfigPath()).c_str()))
			{
				if (!checkDbPrefix("bl") && !checkDbPrefix("FlylinkDC"))
					prefix = "bl";
				auto status = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
				if (status != SQLITE_OK)
					LogManager::message("[Error] sqlite3_config(SQLITE_CONFIG_SERIALIZED) = " + Util::toString(status));
				dcassert(status == SQLITE_OK);
				status = sqlite3_initialize();
				if (status != SQLITE_OK)
					LogManager::message("[Error] sqlite3_initialize = " + Util::toString(status));
				dcassert(status == SQLITE_OK);
				
				connection.open((prefix + ".sqlite").c_str());
				if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
					sqlite3_trace(connection.getdb(), traceCallback, nullptr);

				attachDatabase("locations", "location_db");
				attachDatabase("user", "user_db");
				attachDatabase("transfers", "transfer_db");
#ifdef FLYLINKDC_USE_TORRENT
				attachDatabase("queue", "queue_db");
#endif
				
#ifdef FLYLINKDC_USE_LMDB
				lmdb.open();
#endif

				SetCurrentDirectory(l_dir_buffer);
			}
			else
			{
				errorDB("SQLite - DatabaseManager: error SetCurrentDirectory l_db_path = " + Util::getConfigPath()
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

		connection.executenonquery("CREATE TABLE IF NOT EXISTS ip_stat("
		                              "cid blob not null, ip text not null, upload integer not null, download integer not null)");;
		connection.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_ip_stat ON ip_stat(cid, ip);");

		connection.executenonquery("CREATE TABLE IF NOT EXISTS user_db.user_stat("
		                              "cid blob primary key, nick text not null, last_ip text not null, message_count integer not null, pm_in integer not null, pm_out integer not null)");
#endif

		const int l_db_user_version = connection.executeint("PRAGMA user_version");
		
//		if (l_rev <= 379)
		{
			connection.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_ignore(nick text PRIMARY KEY NOT NULL);");
		}
//     if (l_rev <= 381)
		{
			connection.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_registry(segment integer not null, key text not null,val_str text, val_number int64,tick_count int not null);");
			try
			{
				connection.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_registry_key ON fly_registry(segment,key);");
			}
			catch (const database_error&)
			{
				connection.executenonquery("delete from fly_registry");
				connection.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_registry_key ON fly_registry(segment,key);");
			}
		}
		
		connection.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_p2pguard_ip(start_ip integer not null,stop_ip integer not null,note text,type integer);");
		connection.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_ip ON fly_p2pguard_ip(start_ip);");
		connection.executenonquery("DROP INDEX IF EXISTS location_db.i_fly_p2pguard_note;");
		connection.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_type ON fly_p2pguard_ip(type);");
		safeAlter("ALTER TABLE location_db.fly_p2pguard_ip add column type integer");
		
		connection.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_country_ip(start_ip integer not null,stop_ip integer not null,country text,flag_index integer);");
		safeAlter("ALTER TABLE location_db.fly_country_ip add column country text");
		
		connection.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_country_ip ON fly_country_ip(start_ip);");
		
		connection.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_location_ip(start_ip integer not null,stop_ip integer not null,location text,flag_index integer);");
		    
		safeAlter("ALTER TABLE location_db.fly_location_ip add column location text");
		
		connection.executenonquery("CREATE INDEX IF NOT EXISTS "
		                              "location_db.i_fly_location_ip ON fly_location_ip(start_ip);");
		                              
		connection.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "tth char(39),path text not null,nick text, hub text,size int64 not null,speed int,ip text, actual int64);");
		connection.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_day_type ON fly_transfer_file(day,type);");
		
		safeAlter("ALTER TABLE transfer_db.fly_transfer_file add column actual int64");

#ifdef FLYLINKDC_USE_TORRENT
		connection.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file_torrent("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "sha1 char(20),path text,size int64 not null);");
		connection.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_torrentday_type ON fly_transfer_file_torrent(day,type);");

		connection.executenonquery("CREATE TABLE IF NOT EXISTS queue_db.fly_queue_torrent("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,day int64 not null,stamp int64 not null,sha1 char(20) NOT NULL,resume blob, magnet string,name string);");
		connection.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS queue_db.iu_fly_queue_torrent_sha1 ON fly_queue_torrent(sha1);");
#endif
		
		if (l_db_user_version < 1)
		{
			connection.executenonquery("PRAGMA user_version=1");
		}
		if (l_db_user_version < 2)
		{
			// ”дал€ю уже на уровне конвертора файла.
			// connection.executenonquery("delete from location_db.fly_p2pguard_ip where note like '%VimpelCom%'");
			connection.executenonquery("PRAGMA user_version=2");
		}
		if (l_db_user_version < 3)
		{
			connection.executenonquery("PRAGMA user_version=3");
		}
		if (l_db_user_version < 4)
		{
			connection.executenonquery("PRAGMA user_version=4");
		}
		if (l_db_user_version < 5)
		{
			deleteOldTransferHistoryL();
			connection.executenonquery("PRAGMA user_version=5");
		}
		
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		if (!hasNewStatTables && (hasRatioTable || hasUserTable))
			convertStatTables(hasRatioTable, hasUserTable);
		timeLoadGlobalRatio = 0;
#endif
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - DatabaseManager: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseManager::flush()
{
}

void DatabaseManager::saveLocation(const vector<LocationInfo>& data)
{
	LOCK(cs);
	try
	{
		CFlyBusy busy(g_DisableSQLtrace);
		sqlite3_transaction trans(connection);
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

void DatabaseManager::getIPInfo(uint32_t ip, IPInfo& result, int what, bool onlyCached)
{
	dcassert(what);
	dcassert(ip);
	{
		LOCK(csIpCache);
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
	LOCK(csIpCache);
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

void DatabaseManager::loadCountry(uint32_t ip, IPInfo& result)
{
	result.clearCountry();
	dcassert(ip);

	if (Util::isPrivateIp(ip))
	{
		result.known |= IPInfo::FLAG_COUNTRY;
		return;
	}

	LOCK(cs);
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

void DatabaseManager::loadLocation(uint32_t ip, IPInfo& result)
{
	result.clearLocation();
	dcassert(ip);

	LOCK(cs);
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

void DatabaseManager::loadP2PGuard(uint32_t ip, IPInfo& result)
{
	result.p2pGuard.clear();
	dcassert(ip);

	LOCK(cs);
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

void DatabaseManager::removeManuallyBlockedIP(uint32_t ip)
{
	{
		LOCK(csIpCache);
		IpCacheItem* item = ipCache.get(ip);
		if (item)
		{
			item->info.known &= ~IPInfo::FLAG_P2P_GUARD;
			item->info.p2pGuard.clear();
		}
	}
	LOCK(cs);
	try
	{
		if (deleteManuallyBlockedIP.empty())
		{
			string stmt = "delete from location_db.fly_p2pguard_ip where start_ip=? and type=" + Util::toString(PG_DATA_MANUAL);
			deleteManuallyBlockedIP.open(&connection, stmt.c_str());
		}
		deleteManuallyBlockedIP.bind(1, ip);
		deleteManuallyBlockedIP.executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - removeManuallyBlockedIP: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseManager::loadManuallyBlockedIPs(vector<P2PGuardBlockedIP>& result)
{	
	result.clear();
	LOCK(cs);
	try
	{
		if (selectManuallyBlockedIP.empty())
		{
			string stmt = "select distinct start_ip,note from location_db.fly_p2pguard_ip where type=" + Util::toString(PG_DATA_MANUAL);
			selectManuallyBlockedIP.open(&connection, stmt.c_str());
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

void DatabaseManager::saveP2PGuardData(const vector<P2PGuardData>& data, int type, bool removeOld)
{
	LOCK(cs);
	try
	{
		CFlyBusy busy(g_DisableSQLtrace);
		sqlite3_transaction trans(connection);
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

void DatabaseManager::saveGeoIpCountries(const vector<LocationInfo>& data)
{
	LOCK(cs);
	try
	{
		CFlyBusy busy(g_DisableSQLtrace);
		sqlite3_transaction trans(connection);
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

void DatabaseManager::clearCachedP2PGuardData(uint32_t ip)
{
	LOCK(csIpCache);
	IpCacheItem* item = ipCache.get(ip);
	if (item)
	{
		item->info.known &= ~IPInfo::FLAG_P2P_GUARD;
		item->info.p2pGuard.clear();
	}
}

void DatabaseManager::clearIpCache()
{
	LOCK(csIpCache);
	ipCache.clear();
}

void DatabaseManager::setRegistryVarString(DBRegistryType type, const string& value)
{
	DBRegistryMap m;
	m[Util::toString(type)] = value;
	saveRegistry(m, type, false);
}

string DatabaseManager::getRegistryVarString(DBRegistryType type)
{
	DBRegistryMap m;
	loadRegistry(m, type);
	if (!m.empty())
		return m.begin()->second.sval;
	else
		return Util::emptyString;
}

void DatabaseManager::setRegistryVarInt(DBRegistryType type, int64_t value)
{
	DBRegistryMap m;
	m[Util::toString(type)] = DBRegistryValue(value);
	saveRegistry(m, type, false);
}

int64_t DatabaseManager::getRegistryVarInt(DBRegistryType type)
{
	DBRegistryMap m;
	loadRegistry(m, type);
	if (!m.empty())
		return m.begin()->second.ival;
	else
		return 0;
}

void DatabaseManager::loadRegistry(DBRegistryMap& values, DBRegistryType type)
{
	LOCK(cs);
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

void DatabaseManager::clearRegistry(DBRegistryType type, int64_t tick)
{
	LOCK(cs);
	try
	{
		clearRegistryL(type, tick);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - clearRegistry: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseManager::clearRegistryL(DBRegistryType type, int64_t tick)
{
	initQuery2(deleteRegistry, "delete from fly_registry where segment=? and tick_count<>?");
	deleteRegistry.bind(1, type);
	deleteRegistry.bind(2, tick);
	deleteRegistry.executenonquery();
}

void DatabaseManager::saveRegistry(const DBRegistryMap& values, DBRegistryType type, bool clearOldValues)
{
	LOCK(cs);
	try
	{
		const int64_t tick = getRandValForRegistry();
		initQuery2(insertRegistry, "insert or replace into fly_registry (segment,key,val_str,val_number,tick_count) values(?,?,?,?,?)");
		initQuery2(updateRegistry, "update fly_registry set val_str=?,val_number=?,tick_count=? where segment=? and key=?");
		sqlite3_transaction trans(connection, values.size() > 1 || clearOldValues);
		for (auto k = values.cbegin(); k != values.cend(); ++k)
		{
			const auto& val = k->second;
			updateRegistry.bind(1, val.sval, SQLITE_TRANSIENT);
			updateRegistry.bind(2, val.ival);
			updateRegistry.bind(3, tick);
			updateRegistry.bind(4, int(type));
			updateRegistry.bind(5, k->first, SQLITE_TRANSIENT);
			updateRegistry.executenonquery();
			if (connection.changes() == 0)
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

int64_t DatabaseManager::getRandValForRegistry()
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

void DatabaseManager::deleteOldTransferHistoryL()
{
	if ((SETTING(DB_LOG_FINISHED_DOWNLOADS) || SETTING(DB_LOG_FINISHED_UPLOADS)))
	{
		int64_t timestamp = posixTimeToLocal(time(nullptr));
		int currentDay = timestamp/(60*60*24);

		string sqlDC = makeDeleteOldTransferHistory("fly_transfer_file", currentDay);
		sqlite3_command cmdDC(&connection, sqlDC);
		cmdDC.executenonquery();

#ifdef FLYLINKDC_USE_TORRENT
		string sqlTorrent = makeDeleteOldTransferHistory("fly_transfer_file_torrent", currentDay);
		sqlite3_command cmdTorrent(&connection, sqlTorrent);
		cmdTorrent.executenonquery();
#endif
	}
}

void DatabaseManager::loadTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out)
{
	LOCK(cs);
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
void DatabaseManager::loadTorrentTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out)
{
	LOCK(cs);
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

void DatabaseManager::loadTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out)
{
	LOCK(cs);
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
void DatabaseManager::loadTorrentTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out)
{
	LOCK(cs);
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

void DatabaseManager::deleteTransferHistory(const vector<int64_t>& id)
{
	if (id.empty()) return;
	LOCK(cs);
	try
	{
		sqlite3_transaction trans(connection, id.size() > 1);
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
void DatabaseManager::deleteTorrentTransferHistory(const vector<int64_t>& id)
{
	if (id.empty()) return;
	LOCK(cs);
	try
	{
		sqlite3_transaction trans(connection, id.size() > 1);
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
void DatabaseManager::load_torrent_resume(libtorrent::session& p_session)
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
					LOCK(g_resume_torrents_cs);
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

void DatabaseManager::delete_torrent_resume(const libtorrent::sha1_hash& p_sha1)
{
	LOCK(cs);
	try
	{
		initQuery(m_delete_resume_torrent, "delete from queue_db.fly_queue_torrent where sha1=?");
		m_delete_resume_torrent->bind(1, p_sha1.data(), p_sha1.size(), SQLITE_STATIC);
		m_delete_resume_torrent->executenonquery();
		if (connection.changes() == 1)
		{
			LOCK(g_delete_torrents_cs);
			g_delete_torrents.insert(p_sha1);
		}
		else
		{
			dcassert(0); // «овем второй раз удаление - не хорошо
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - delete_torrent_resume: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseManager::save_torrent_resume(const libtorrent::sha1_hash& p_sha1, const std::string& p_name, const std::vector<char>& p_resume)
{
	LOCK(cs);
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

void DatabaseManager::addTransfer(eTypeTransfer type, const FinishedItemPtr& item)
{
	int64_t timestamp = posixTimeToLocal(item->getTime());
	LOCK(cs);
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
void DatabaseManager::addTorrentTransfer(eTypeTransfer type, const FinishedItemPtr& item)
{
	int64_t timestamp = posixTimeToLocal(item->getTime());	
	LOCK(cs);
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

void DatabaseManager::loadIgnoredUsers(StringSet& users)
{
	LOCK(cs);
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

void DatabaseManager::saveIgnoredUsers(const StringSet& users)
{
	LOCK(cs);
	try
	{
		sqlite3_transaction trans(connection);
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
void DatabaseManager::saveIPStat(const CID& cid, const vector<IPStatVecItem>& items)
{
	const int batchSize = 256;
	LOCK(cs);
	try
	{
		int count = 0;
		sqlite3_transaction trans(connection, false);
		for (const IPStatVecItem& item : items)
			saveIPStatL(cid, item.ip, item.item, batchSize, count, trans);
		if (count) trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveIPStat: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseManager::saveIPStatL(const CID& cid, const string& ip, const IPStatItem& item, int batchSize, int& count, sqlite3_transaction& trans)
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

IPStatMap* DatabaseManager::loadIPStat(const CID& cid)
{
	IPStatMap* ipStat = nullptr;
	LOCK(cs);
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
				IPStatItem item;
				item.download = download;
				item.upload = upload;
				item.flags = IPStatItem::FLAG_LOADED;
				if (ipStat->data.insert(make_pair(ip, item)).second)
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

void DatabaseManager::saveUserStatL(const CID& cid, UserStatItem& stat, int batchSize, int& count, sqlite3_transaction& trans)
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

void DatabaseManager::saveUserStat(const CID& cid, UserStatItem& stat)
{
	LOCK(cs);
	try
	{
		sqlite3_transaction trans(connection, false);
		int count = 0;
		saveUserStatL(cid, stat, 1, count, trans);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - saveUserStat: " + e.getError(), e.getErrorCode());
	}
}

bool DatabaseManager::loadUserStat(const CID& cid, UserStatItem& stat)
{
	bool result = false;
	LOCK(cs);
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

bool DatabaseManager::convertStatTables(bool hasRatioTable, bool hasUserTable)
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
		sqlite3_command selectDic(&connection, "select dic,id,name from fly_dic");
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
			sqlite3_command cmd(&connection, "select id,dic_ip,dic_nick,dic_hub,upload,download from fly_ratio");
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
			sqlite3_command cmd(&connection, "select nick,dic_hub,last_ip,message_count from user_db.user_info");
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
				item.stat.lastIp = Util::printIpAddress(static_cast<Ip4Address>(lastIp));
				item.stat.addNick(nick, hub);
				if (messageCount > 0) item.stat.messageCount = static_cast<unsigned>(messageCount);
			}
		}

		for (auto& m : values) m.clear();

		int insertedCount = 0;
		const int batchSize = 256;
		sqlite3_transaction trans(connection, false);
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
	catch (const database_error&)
	{
		result = false;
	}
	for (auto& item : convMap)
		delete item.second.ipStat;
	if (result && (insertedUsers || insertedIPs))
		LogManager::message("Stat tables converted: users=" + Util::toString(insertedUsers) + ", IPs=" + Util::toString(insertedIPs), false);
	return result;
}

void DatabaseManager::loadGlobalRatio(bool force)
{
	uint64_t tick = GET_TICK();
	LOCK(cs);
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

bool DatabaseManager::hasTable(const string& tableName, const string& db)
{
	dcassert(tableName == Text::toLower(tableName));
	string prefix = db;
	if (!prefix.empty()) prefix += '.';
	return connection.executeint("select count(*) from " + prefix + "sqlite_master where type = 'table' and lower(tbl_name) = '" + tableName + "'") != 0;
}

void DatabaseManager::vacuum()
{
#ifdef FLYLINKDC_USE_VACUUM
	LogManager::message("start vacuum", true);
	connection.executenonquery("VACUUM;");
	LogManager::message("stop vacuum", true);
#endif
}

void DatabaseManager::shutdown()
{
	int status = sqlite3_shutdown();
	dcassert(status == SQLITE_OK);
	if (status != SQLITE_OK)
		LogManager::message("[Error] sqlite3_shutdown = " + Util::toString(status));
}

DatabaseManager::~DatabaseManager()
{
	flush();
}

bool DatabaseManager::getFileInfo(const TTHValue &tth, unsigned &flags, string *path, size_t *treeSize)
{
#ifdef FLYLINKDC_USE_LMDB
	return lmdb.getFileInfo(tth.data, flags, path, treeSize);
#else
	flags = 0;
	if (path) path->clear();
	if (treeSize) *treeSize = 0;
	return false;
#endif
}

bool DatabaseManager::setFileInfoDownloaded(const TTHValue &tth, uint64_t fileSize, const string &path)
{
#ifdef FLYLINKDC_USE_LMDB
	if (tth.isZero()) return false;
	return lmdb.putFileInfo(tth.data, FLAG_DOWNLOADED, fileSize, path.empty() ? nullptr : &path);
#else
	return false;
#endif
}

bool DatabaseManager::setFileInfoCanceled(const TTHValue &tth, uint64_t fileSize)
{
#ifdef FLYLINKDC_USE_LMDB
	if (tth.isZero()) return false;
	return lmdb.putFileInfo(tth.data, FLAG_DOWNLOAD_CANCELED, fileSize, nullptr);
#else
	return false;
#endif
}

bool DatabaseManager::addTree(const TigerTree &tree)
{
	if (tree.getRoot().isZero())
	{
		dcassert(0);
		return false;
	}
	{
		LOCK(csTreeCache);
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
	if (tree.getLeaves().size() < 2)
		return true;
	bool result = lmdb.putTigerTree(tree);
	if (!result)
		LogManager::message("Failed to add tiger tree to DB (" + tree.getRoot().toBase32() + ')', false);
	return result;
#else
	return false;
#endif
}

bool DatabaseManager::getTree(const TTHValue &tth, TigerTree &tree)
{
	if (tth.isZero())
		return false;
	{
		LOCK(csTreeCache);
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

bool DatabaseManager::checkDbPrefix(const string& str)
{
	if (File::isExist(str + ".sqlite"))
	{
		prefix = str;
		return true;
	}
	return false;
}

void DatabaseManager::attachDatabase(const string& file, const string& name)
{
	string cmd = "attach database '";
	cmd += prefix;
	cmd += '_';
	cmd += file;
	cmd += ".sqlite' as ";
	cmd += name;
	connection.executenonquery(cmd.c_str());
}

#ifdef FLYLINKDC_USE_TORRENT
bool DatabaseManager::is_resume_torrent(const libtorrent::sha1_hash& p_sha1)
{
	LOCK(g_resume_torrents_cs);
	return g_resume_torrents.find(p_sha1) != g_resume_torrents.end();
}

bool DatabaseManager::is_delete_torrent(const libtorrent::sha1_hash& p_sha1)
{
	LOCK(g_delete_torrents_cs);
	return g_delete_torrents.find(p_sha1) != g_delete_torrents.end();
}
#endif
