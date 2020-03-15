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
#include <boost/algorithm/string.hpp>
#include "FinishedManager.h"

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

const char* g_db_file_names[] = {"FlylinkDC.sqlite",
                                 "FlylinkDC_log.sqlite",
                                 "FlylinkDC_stat.sqlite",
                                 "FlylinkDC_locations.sqlite",
                                 "FlylinkDC_transfers.sqlite",
                                 "FlylinkDC_user.sqlite",
                                 "FlylinkDC_queue.sqlite"
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
		const string l_message = STRING(DATA_BASE_LOCKED_STRING);
		static int g_MessageBox = 0; // TODO - fix copy-paste
		CFlyBusy l_busy(g_MessageBox);
		if (g_MessageBox <= 1)
		{
			MessageBox(NULL, Text::toT(l_message).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
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
	static const char* dbName[] = { "main", "stat_db", "location_db", "user_db" };
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
		for (int i = 0; i < _countof(g_db_file_names); ++i)
		{
			string filePath = path + g_db_file_names[i];
			int64_t size, fileTime;
			bool isLink;
			if (File::isExist(filePath, size, fileTime, isLink))
			{
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
			MessageBox(NULL, Text::toT(message).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
		}
	}
}

CFlylinkDBManager::CFlylinkDBManager()
{
	CFlyLock(m_cs);
#ifdef _DEBUG
	m_is_load_global_ratio = false;
#endif
	m_count_fly_location_ip_record = -1;
	m_DIC.resize(e_DIC_LAST - 1);
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
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_stat.sqlite' as stat_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_locations.sqlite' as location_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_user.sqlite' as user_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_transfers.sqlite' as transfer_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_queue.sqlite' as queue_db");
				
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
    
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_revision(rev integer NOT NULL);");
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_dic("
		                              "id integer PRIMARY KEY AUTOINCREMENT NOT NULL,dic integer NOT NULL, name text NOT NULL);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_dic_name ON fly_dic(name,dic);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_ratio(id integer PRIMARY KEY AUTOINCREMENT NOT NULL,"
		    "dic_ip integer not null,dic_nick integer not null, dic_hub integer not null,"
		    "upload int64 default 0,download int64 default 0);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_ratio ON fly_ratio(dic_nick,dic_hub,dic_ip);");
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_ratio AS "
		                              "SELECT fly_ratio.id id, fly_ratio.upload upload,"
		                              "fly_ratio.download download,"
		                              "userip.name userip,"
		                              "nick.name nick,"
		                              "hub.name hub "
		                              "FROM fly_ratio "
		                              "INNER JOIN fly_dic userip ON fly_ratio.dic_ip = userip.id "
		                              "INNER JOIN fly_dic nick ON fly_ratio.dic_nick = nick.id "
		                              "INNER JOIN fly_dic hub ON fly_ratio.dic_hub = hub.id");
		                              
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_ratio_all AS "
		                              "SELECT fly_ratio.id id, fly_ratio.upload upload,"
		                              "fly_ratio.download download,"
		                              "nick.name nick,"
		                              "hub.name hub,"
		                              "fly_ratio.dic_ip dic_ip,"
		                              "fly_ratio.dic_nick dic_nick,"
		                              "fly_ratio.dic_hub dic_hub "
		                              "FROM fly_ratio "
		                              "INNER JOIN fly_dic nick ON fly_ratio.dic_nick = nick.id "
		                              "INNER JOIN fly_dic hub ON fly_ratio.dic_hub = hub.id");
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		const int l_rev = m_flySQLiteDB.executeint("select max(rev) from fly_revision");
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
			// TODO m_flySQLiteDB.executenonquery(
			// TODO             "CREATE TABLE IF NOT EXISTS fly_recent(Name text PRIMARY KEY NOT NULL,Description text, Users int,Shared int64,Server text);");
		}
		m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_geoip");
		if (is_table_exists("fly_country_ip"))
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
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_note ON fly_p2pguard_ip(note);");
		safeAlter("ALTER TABLE location_db.fly_p2pguard_ip add column type integer");
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_country_ip(start_ip integer not null,stop_ip integer not null,country text,flag_index integer);");
		safeAlter("ALTER TABLE location_db.fly_country_ip add column country text");
		
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_country_ip ON fly_country_ip(start_ip);");
		/*
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS i_fly_country_ip ON fly_country_ip(start_ip,stop_ip);");
		ALTER TABLE location_db.fly_country_ip ADD COLUMN idx INTEGER;
		UPDATE fly_country_ip SET idx = (stop_ip - (stop_ip % 65536));
		CREATE INDEX IF NOT EXISTS location_db.i_idx_fly_country_ip ON fly_country_ip(idx);
		*/
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_location_ip(start_ip integer not null,stop_ip integer not null,location text,flag_index integer);");
		    
		safeAlter("ALTER TABLE location_db.fly_location_ip add column location text");
		
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS "
		                              "location_db.i_fly_location_ip ON fly_location_ip(start_ip);");
		                              
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_identity(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,hub text not null,key text not null, value text not null,"
		    "count_get integer, count_set integer,last_time_get text not null, last_time_set text not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS "
		                              "stat_db.iu_fly_identity ON fly_identity(hub,key,value);");
		                              
#endif // FLYLINKDC_USE_GATHER_IDENTITY_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_statistic(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,stat_value_json text not null,stat_time int64, flush_time int64, type text);");
		safeAlter("ALTER TABLE stat_db.fly_statistic add column type text");
		
		// Таблицы - мертвые
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_last_ip(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
		                              "dic_nick integer not null, dic_hub integer not null,dic_ip integer not null);");
		if (!safeAlter("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip ON fly_last_ip(dic_nick,dic_hub);"))
		{
			safeAlter("delete from fly_last_ip where rowid not in (select max(rowid) from fly_last_ip group by dic_nick,dic_hub)");
		}
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_last_ip_nick_hub("
		                              "nick text not null, dic_hub integer not null,ip text);");
		if (!safeAlter("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip_nick_hub ON fly_last_ip_nick_hub(nick,dic_hub);"))
		{
			safeAlter("delete from fly_last_ip_nick_hub where rowid not in (select max(rowid) from fly_last_ip_nick_hub group by nick,dic_hub)");
		}
		// Она не используются в версиях r502 но для отката назад нужны
		
#ifdef FLYLINKDC_USE_COLLECT_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_dc_command_log(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL"
		    ",hub text not null,command text not null,server text,port text, sender_nick text, counter int64, last_time text not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS stat_db.iu_fly_dc_command_log ON fly_dc_command_log(hub,command);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_event(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL "
		    ",type text not null, event_key text not null, event_value text, ip text, port text, hub text, tth char(39), event_time text);");
#endif
		if (l_rev < VERSION_NUM)
		{
			m_flySQLiteDB.executenonquery("insert into fly_revision(rev) values(" A_VERSION_NUM_STR ");");
		}
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS user_db.user_info("
		                              "nick text not null, dic_hub integer not null, last_ip integer, message_count integer);");
		m_flySQLiteDB.executenonquery("DROP INDEX IF EXISTS user_db.iu_user_info;"); //старый индекс был (nick,dic_hub)
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS user_db.iu_user_info_hub_nick ON user_info(dic_hub,nick);");
		
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "tth char(39),path text not null,nick text, hub text,size int64 not null,speed int,ip text, actual int64);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_day_type ON fly_transfer_file(day,type);");
		// TODO - сделать позже если будет тормозить
		// m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.i_fly_transfer_file_tth ON fly_transfer_file(tth);");
		
		safeAlter("ALTER TABLE transfer_db.fly_transfer_file add column actual int64");
		
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file_torrent("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "sha1 char(20),path text,size int64 not null);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_torrentday_type ON fly_transfer_file_torrent(day,type);");
		
#ifdef FLYLINKDC_USE_TORRENT
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
		
		/*
		{
		    // Конвертим ip в бинарный формат
		    std::unique_ptr<sqlite3_command> l_src_sql(new sqlite3_command(&m_flySQLiteDB,
		                                                                    "select nick,dic_hub,ip from fly_last_ip_nick_hub"));
		    try
		    {
		        sqlite3_reader l_q = l_src_sql->executereader();
		        sqlite3_transaction l_trans(m_flySQLiteDB);
		        std::unique_ptr<sqlite3_command> l_trg_sql(new sqlite3_command(&m_flySQLiteDB,
		                                                                        "insert or replace into user_db.user_info (nick,dic_hub,last_ip) values(?,?,?)"));
		        while (l_q.read())
		        {
		            boost::system::error_code ec;
		            const auto l_ip = boost::asio::ip::address_v4::from_string(l_q.getstring(2), ec);
		            dcassert(!ec);
		            if (!ec)
		            {
		                l_trg_sql->bind(1, l_q.getstring(0), SQLITE_TRANSIENT);
		                l_trg_sql->bind(2, l_q.getint64(1));
		                l_trg_sql->bind(3, sqlite_int64(l_ip.to_uint()));
		                l_trg_sql->executenonquery();
		            }
		        }
		        l_trans.commit();
		    }
		    catch (const database_error& e)
		    {
		        // Гасим ошибки БД при конвертации
		        LogManager::message("[SQLite] Error convert user_db.user_info = " + e.getError());
		    }
		}
		*/
		load_all_hub_into_cacheL();
		//safeAlter("ALTER TABLE fly_last_ip_nick_hub add column message_count integer");
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - CFlylinkDBManager: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::load_all_hub_into_cacheL()
{
	std::unique_ptr<sqlite3_command> l_load_all_dic(new sqlite3_command(&m_flySQLiteDB,
		"select id,name from fly_dic where dic=1"));
	sqlite3_reader l_q = l_load_all_dic.get()->executereader();
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
	CFlyFastLock(m_hub_dic_fcs);
#endif
	while (l_q.read())
	{
		const string l_hub_name = l_q.getstring(1);
		const auto l_id = l_q.getint(0);
		const auto& l_res = m_DIC[e_DIC_HUB - 1].insert(std::make_pair(l_hub_name, l_id));
		dcassert(l_res.second == true);
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
		m_HubNameMap[l_id] = l_hub_name;
#endif
	}
}

#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
//========================================================================================================
void CFlylinkDBManager::identity_initL(const string& p_hub, const string& p_key, const string& p_value)
{
	try
	{
		m_insert_identity_stat.init(m_flySQLiteDB,
		                            "insert into stat_db.fly_identity (hub,key,value,count_get,count_set,last_time_get,last_time_set) "
		                            "values(?,?,?,0,0,strftime('%d.%m.%Y %H:%M:%S','now','localtime'),strftime('%d.%m.%Y %H:%M:%S','now','localtime'))");
		m_insert_identity_stat->bind(1, p_hub, SQLITE_STATIC);
		m_insert_identity_stat->bind(2, p_key, SQLITE_STATIC);
		m_insert_identity_stat->bind(3, p_value, SQLITE_STATIC);
		m_insert_identity_stat->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - identity_initL: " + e.getError(), e.getErrorCode());
	}
}
//========================================================================================================
void CFlylinkDBManager::identity_set(string p_key, string p_value, const string& p_hub /*= "-" */)
{
	dcassert(!p_key.empty());
	if (p_value.empty())
		p_value = "null";
	if (p_key.size() > 2)
		p_key = p_key.substr(0, 2);
	if (p_key.empty())
		p_key = "null";
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_update_identity_stat_set.init(m_flySQLiteDB,
		                                "update stat_db.fly_identity set count_set = count_set+1, last_time_set = strftime('%d.%m.%Y %H:%M:%S','now','localtime') "
		                                "where hub = ? and key=? and value =?");
		m_update_identity_stat_set->bind(1, p_hub, SQLITE_STATIC);
		m_update_identity_stat_set->bind(2, p_key, SQLITE_STATIC);
		m_update_identity_stat_set->bind(3, p_value, SQLITE_STATIC);
		m_update_identity_stat_set->executenonquery();
		if (m_update_identity_stat_set.changes() == 0)
		{
			identity_initL(p_hub, p_key, p_value);
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - identity_set: " + e.getError(), e.getErrorCode());
	}
}
//========================================================================================================
void CFlylinkDBManager::identity_get(string p_key, string p_value, const string& p_hub /*= "-" */)
{
	dcassert(!p_key.empty());
	if (p_value.empty())
		p_value = "null";
	if (p_key.size() > 2)
		p_key = p_key.substr(0, 2);
	if (p_key.empty())
		p_key = "null";
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_update_identity_stat_get.init(m_flySQLiteDB,
		                                "update stat_db.fly_identity set count_get = count_get+1, last_time_get = strftime('%d.%m.%Y %H:%M:%S','now','localtime') "
		                                "where hub = ? and key=? and value =?"));
		m_update_identity_stat_get->bind(1, p_hub, SQLITE_STATIC);
		m_update_identity_stat_get->bind(2, p_key, SQLITE_STATIC);
		m_update_identity_stat_get->bind(3, p_value, SQLITE_STATIC);
		m_update_identity_stat_get->executenonquery();
		if (m_update_identity_stat_get.changes() == 0)
		{
			identity_initL(p_hub, p_key, p_value);
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - identity_get: " + e.getError(), e.getErrorCode());
	}
}
#endif // FLYLINKDC_USE_GATHER_IDENTITY_STAT

#ifdef FLYLINKDC_USE_COLLECT_STAT
void CFlylinkDBManager::push_event_statistic(const std::string& p_event_type, const std::string& p_event_key,
                                             const string& p_event_value,
                                             const string& p_ip,
                                             const string& p_port,
                                             const string& p_hub,
                                             const string& p_tth
                                            )
{
	CFlyLock(m_cs);
	try
	{
		m_insert_event_stat.init(m_flySQLiteDB,
		                         "insert into stat_db.fly_event(type,event_key,event_value,ip,port,hub,tth,event_time) values(?,?,?,?,?,?,?,strftime('%d.%m.%Y %H:%M:%S','now','localtime'))");
		m_insert_event_stat->bind(1, p_event_type, SQLITE_STATIC);
		m_insert_event_stat->bind(2, p_event_key, SQLITE_STATIC);
		m_insert_event_stat->bind(3, p_event_value, SQLITE_STATIC);
		m_insert_event_stat->bind(4, p_ip, SQLITE_STATIC);
		m_insert_event_stat->bind(5, p_port, SQLITE_STATIC);
		m_insert_event_stat->bind(6, p_hub, SQLITE_STATIC);
		m_insert_event_stat->bind(7, p_tth, SQLITE_STATIC);
		m_insert_event_stat->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - push_event_statistic: " + e.getError(), e.getErrorCode());
	}
}


void CFlylinkDBManager::push_dc_command_statistic(const std::string& p_hub, const std::string& p_command,
                                                  const string& p_server, const string& p_port, const string& p_sender_nick)
{
	dcassert(!p_hub.empty() && !p_command.empty());
	if (!p_hub.empty() && !p_command.empty())
	{
		CFlyLock(m_cs);
		try
		{
			int64_t l_counter = 0;
			{
				m_select_statistic_dc_command.init(m_flySQLiteDB,
				                                   "select counter from stat_db.fly_dc_command_log where hub = ? and command = ?");
				m_select_statistic_dc_command->bind(1, p_hub, SQLITE_STATIC);
				m_select_statistic_dc_command->bind(2, p_command, SQLITE_STATIC);
				sqlite3_reader l_q = m_select_statistic_dc_command.get()->executereader();
				while (l_q.read())
				{
					l_counter = l_q.getint64(0);
				}
			}
			m_insert_statistic_dc_command.init(m_flySQLiteDB,
			                                   "insert or replace into stat_db.fly_dc_command_log(hub, command, server, port, sender_nick, counter, last_time) values(?,?,?,?,?,?,strftime('%d.%m.%Y %H:%M:%S','now','localtime'))");
			m_insert_statistic_dc_command->bind(1, p_hub, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(2, p_command, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(3, p_server, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(4, p_port, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(5, p_sender_nick, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(6, l_counter + 1);
			m_insert_statistic_dc_command->executenonquery();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - push_dc_command_statistic: " + e.getError(), e.getErrorCode());
		}
	}
	else
	{
		// Log
	}
}
#endif // FLYLINKDC_USE_COLLECT_STAT

void CFlylinkDBManager::save_location(const CFlyLocationIPArray& p_geo_ip)
{
	CFlyLock(m_cs);
	try
	{
		CFlyBusy l_disable_log(g_DisableSQLtrace);
		sqlite3_transaction l_trans(m_flySQLiteDB);
		initQuery(m_delete_location, "delete from location_db.fly_location_ip");
		m_delete_location->executenonquery();
		m_count_fly_location_ip_record = 0;
		initQuery(m_insert_location, "insert into location_db.fly_location_ip (start_ip,stop_ip,location,flag_index) values(?,?,?,?)");
		for (auto i = p_geo_ip.begin(); i != p_geo_ip.end(); ++i)
		{
			dcassert(i->m_start_ip  && !i->m_location.empty());
			m_insert_location->bind(1, i->m_start_ip);
			m_insert_location->bind(2, i->m_stop_ip);
			m_insert_location->bind(3, i->m_location, SQLITE_STATIC);
			m_insert_location->bind(4, i->m_flag_index);
			m_insert_location->executenonquery();
			++m_count_fly_location_ip_record;
		}
		l_trans.commit();
		{
			CFlyFastLock(m_cache_location_cs);
			m_location_cache_array.clear();
			m_ip_info_cache.clear();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_location: " + e.getError(), e.getErrorCode());
	}
}
#ifdef FLYLINKDC_USE_GEO_IP

int64_t CFlylinkDBManager::get_dic_country_id(const string& p_country)
{
	CFlyLock(m_cs);
	return get_dic_idL(p_country, e_DIC_COUNTRY, true);
}

bool CFlylinkDBManager::find_cache_country(uint32_t p_ip, uint16_t& p_index)
{
	CFlyFastLock(m_cache_location_cs);
	dcassert(p_ip);
	p_index = 0;
	const auto l_result = m_ip_info_cache.find(p_ip);
	if (m_ip_info_cache.find(p_ip) != m_ip_info_cache.end())
	{
		const auto& l_record = l_result->second;
		p_index = l_record.m_country_cache_index;
		return true;
	}
	dcassert(m_country_cache.size() <= 0xFFFF);
	for (auto i =  m_country_cache.begin(); i !=  m_country_cache.end(); ++i)
	{
		++p_index;
		if (p_ip >= i->m_start_ip && p_ip < i->m_stop_ip)
		{
			return true;
		}
	}
	p_index = 0;
	return false;
}

bool CFlylinkDBManager::find_cache_location(uint32_t p_ip, uint32_t& p_location_index, uint16_t& p_flag_location_index)
{
	CFlyFastLock(m_cache_location_cs);
	p_location_index = 0;
	p_flag_location_index = 0;
	const auto l_result = m_ip_info_cache.find(p_ip);
	if (m_ip_info_cache.find(p_ip) != m_ip_info_cache.end())
	{
		const auto& l_record = l_result->second;
		p_location_index = l_record.m_location_cache_index;
		p_flag_location_index = l_record.m_flag_location_index;
		return true;
	}
	for (auto i = m_location_cache_array.cbegin(); i != m_location_cache_array.cend(); ++i, ++p_location_index)
	{
		if (p_ip >= i->m_start_ip && p_ip < i->m_stop_ip)
		{
			++p_location_index;
			p_flag_location_index = i->m_flag_index;
			return true;
		}
	}
	return false;
}

void CFlylinkDBManager::get_country_and_location(uint32_t p_ip, uint16_t& p_country_index, uint32_t& p_location_index, bool p_is_use_only_cache)
{
	dcassert(p_ip);
	uint16_t l_flag_location_index = 0; // TODO ?
	const bool l_is_find_country   = Util::isPrivateIp(p_ip) || find_cache_country(p_ip, p_country_index);
	const bool l_is_find_location = find_cache_location(p_ip, p_location_index, l_flag_location_index);
	if (p_is_use_only_cache == false)
	{
		if (l_is_find_country == false || l_is_find_location == false)
		{
			load_country_locations_p2p_guard_from_db(p_ip, p_location_index, p_country_index);
		}
	}
}

string CFlylinkDBManager::load_country_locations_p2p_guard_from_db(uint32_t p_ip, uint32_t& p_location_cache_index, uint16_t& p_country_cache_index)
{
	dcassert(p_ip);
	CFlyLock(m_cs); // Без этого падает почему-то
	string l_p2p_guard_text;
	try
	{
		// http://www.sql.ru/forum/783621/faq-nahozhdenie-zapisey-gde-zadannoe-znachenie-nahoditsya-mezhdu-znacheniyami-poley
		// http://habrahabr.ru/post/138067/
		
		// TODO - optimisation if(!Util::isPrivateIp(p_ip))
		// TODO - склеить выборку в один запрос
		// для стран и p2p не запрашивать приватные адреса
		initQuery(m_select_country_and_location,
			"select country,flag_index,start_ip,stop_ip,0 from "
		    "(select country,flag_index,start_ip,stop_ip from location_db.fly_country_ip where start_ip <=? order by start_ip desc limit 1) "
		    "where stop_ip >=?"
		    "\nunion all\n"
		    "select location,flag_index,start_ip,stop_ip,1 from "
		    "(select location,flag_index,start_ip,stop_ip from location_db.fly_location_ip where start_ip <=? order by start_ip desc limit 1) "
		    "where stop_ip >=?"
		    "\nunion all\n"
		    "select note,0,start_ip,stop_ip,2 from "
		    "(select note,start_ip,stop_ip from location_db.fly_p2pguard_ip where start_ip <=? order by start_ip desc limit 1) "
		    "where stop_ip >=?");
		m_select_country_and_location->bind(1, p_ip);
		m_select_country_and_location->bind(2, p_ip);
		m_select_country_and_location->bind(3, p_ip);
		m_select_country_and_location->bind(4, p_ip);
		m_select_country_and_location->bind(5, p_ip);
		m_select_country_and_location->bind(6, p_ip);
		sqlite3_reader l_q = m_select_country_and_location->executereader();
		CFlyLocationDesc l_location;
		p_location_cache_index = 0;
		p_country_cache_index = 0;
		l_location.m_flag_index = 0;
		unsigned l_count_country = 0;
		CFlyCacheIPInfo* l_ip_cahe_item = nullptr;
		{
			CFlyFastLock(m_cache_location_cs);
			l_ip_cahe_item = &m_ip_info_cache[p_ip];
		}
		while (l_q.read())
		{
			const unsigned l_id = l_q.getint(4);
			dcassert(l_id < 3)
			const string l_description = l_q.getstring(0);
			l_location.m_description = Text::toT(l_description);
			l_location.m_flag_index = l_q.getint(1);
			l_location.m_start_ip = l_q.getint(2);
			l_location.m_stop_ip = l_q.getint(3);
			switch (l_q.getint(4))
			{
				case 0:
				{
					l_count_country++;
					{
						CFlyFastLock(m_cache_location_cs);
						m_country_cache.push_back(l_location);
						p_country_cache_index = uint16_t(m_country_cache.size());
						l_ip_cahe_item->m_country_cache_index = p_country_cache_index;
						l_ip_cahe_item->m_flag_location_index = l_location.m_flag_index;
					}
					break;
				}
				case 1:
				{
					CFlyFastLock(m_cache_location_cs);
					m_location_cache_array.push_back(l_location);
					p_location_cache_index = m_location_cache_array.size();
					l_ip_cahe_item->m_location_cache_index = p_location_cache_index;
					l_ip_cahe_item->m_flag_location_index = l_location.m_flag_index;
					break;
				}
				case 2:
				{
					{
						CFlyFastLock(m_cache_location_cs);
						l_ip_cahe_item->m_description_p2p_guard = l_description;
					}
					if (!l_p2p_guard_text.empty())
					{
						l_p2p_guard_text += " + ";
					}
					l_p2p_guard_text += l_description;
					continue;
				}
				default:
					dcassert(0);
			}
		}
		dcassert(l_count_country <= 1); // Второго диапазона в GeoIPCountryWhois.csv быть не должно!
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_country_locations_p2p_guard_from_db: " + e.getError(), e.getErrorCode());
	}
	return l_p2p_guard_text;
}

string CFlylinkDBManager::is_p2p_guard(const uint32_t& p_ip)
{
	// dcassert(Util::isPrivateIp(p_ip) == false);
	dcassert(p_ip && p_ip != INADDR_NONE);
	string l_p2p_guard_text;
	if (p_ip && p_ip != INADDR_NONE)
	{
		{
			CFlyFastLock(m_cache_location_cs);
			const auto l_p2p = m_ip_info_cache.find(p_ip);
			if (l_p2p != m_ip_info_cache.end())
				return l_p2p->second.m_description_p2p_guard;
		}
		uint16_t l_country_index;
		uint32_t l_location_index;
		l_p2p_guard_text = load_country_locations_p2p_guard_from_db(p_ip, l_location_index, l_country_index);
	}
	return l_p2p_guard_text;
}

void CFlylinkDBManager::remove_manual_p2p_guard(const string& p_ip)
{
	try
	{
		initQuery(m_delete_manual_p2p_guard, "delete from location_db.fly_p2pguard_ip where note = 'Manual block IP' and start_ip=?");
		boost::system::error_code ec;
		const auto l_ip_boost = boost::asio::ip::address_v4::from_string(p_ip, ec);
		if (!ec)
		{
		
			m_delete_manual_p2p_guard->bind(1, l_ip_boost.to_uint());
			m_delete_manual_p2p_guard->executenonquery();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_manual_p2p_guard: " + e.getError(), e.getErrorCode());
	}
}

string CFlylinkDBManager::load_manual_p2p_guard()
{
	string l_result;
	try
	{
		initQuery(m_select_manual_p2p_guard, "select distinct start_ip from location_db.fly_p2pguard_ip where note = 'Manual block IP'");
		sqlite3_reader l_q = m_select_manual_p2p_guard->executereader();
		while (l_q.read())
		{
			l_result += boost::asio::ip::address_v4(l_q.getint(0)).to_string() + "\r\n";
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_manual_p2p_guard: " + e.getError(), e.getErrorCode());
	}
	return l_result;
}

void CFlylinkDBManager::save_p2p_guard(const CFlyP2PGuardArray& p_p2p_guard_ip, const string& p_manual_marker, int p_type)
{
	CFlyLock(m_cs);
	try
	{
		{
			CFlyFastLock(m_cache_location_cs);
			m_ip_info_cache.clear();
		}
		CFlyBusy l_disable_log(g_DisableSQLtrace);
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (p_manual_marker.empty())
		{
			initQuery(m_delete_p2p_guard, "delete from location_db.fly_p2pguard_ip where note <> 'Manual block IP' and (type=? or type is null)");
			m_delete_p2p_guard->bind(1, p_type);
			m_delete_p2p_guard->executenonquery();
		}
		initQuery(m_insert_p2p_guard, "insert into location_db.fly_p2pguard_ip (start_ip,stop_ip,note,type) values(?,?,?,?)");
		for (auto i = p_p2p_guard_ip.begin(); i != p_p2p_guard_ip.end(); ++i)
		{
			dcassert(!i->m_note.empty());
			m_insert_p2p_guard->bind(1, i->m_start_ip);
			m_insert_p2p_guard->bind(2, i->m_stop_ip);
			m_insert_p2p_guard->bind(3, i->m_note, SQLITE_STATIC);
			m_insert_p2p_guard->bind(4, p_type);
			m_insert_p2p_guard->executenonquery();
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_p2p_guard: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::save_geoip(const CFlyLocationIPArray& p_geo_ip)
{
	CFlyLock(m_cs);
	try
	{
		CFlyBusy l_disable_log(g_DisableSQLtrace);
		sqlite3_transaction l_trans(m_flySQLiteDB);
		initQuery(m_delete_geoip, "delete from location_db.fly_country_ip");
		m_delete_geoip->executenonquery();
		initQuery(m_insert_geoip, "insert into location_db.fly_country_ip (start_ip,stop_ip,country,flag_index) values(?,?,?,?)");
		for (auto i = p_geo_ip.begin(); i != p_geo_ip.end(); ++i)
		{
			dcassert(i->m_start_ip  && !i->m_location.empty());
			m_insert_geoip->bind(1, i->m_start_ip);
			m_insert_geoip->bind(2, i->m_stop_ip);
			m_insert_geoip->bind(3, i->m_location, SQLITE_STATIC);
			m_insert_geoip->bind(4, i->m_flag_index);
			m_insert_geoip->executenonquery();
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_geoip: " + e.getError(), e.getErrorCode());
	}
}
#endif // FLYLINKDC_USE_GEO_IP

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
			time_t pt = static_cast<time_t>(item.dateAsInt*(60*60*24));
			const tm* t = gmtime(&pt);
			char buf[256];
			sprintf(buf, "%d.%02d.%d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
			item.date = buf;
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
			time_t pt = static_cast<time_t>(item.dateAsInt*(60*60*24));
			const tm* t = gmtime(&pt);
			char buf[256];
			sprintf(buf, "%d.%02d.%d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
			item.date = buf;
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

#if 0 // TODO: remove
class CFlySourcesItem
{
	public:
		CID m_CID;
		string m_nick;
		int m_hub_id;
		CFlySourcesItem(const CID& p_CID, const string& p_nick, int p_hub_id):
			m_CID(p_CID), m_nick(p_nick), m_hub_id(p_hub_id)
		{
		}
};

int32_t CFlylinkDBManager::load_queue()
{
	vector<QueueItemPtr> l_qitem;
	{
		g_count_queue_source = 0;
		g_count_queue_files = 0;
#ifdef _DEBUG
		std::unordered_set<string> l_cache_duplicate_tth;
		
// #define FLYLINKDC_USE_DEBUG_10_RECORD
#endif
		try
		{
			boost::unordered_map<int, std::vector< CFlySourcesItem > > l_sources_map;
			{
				CFlyLog l_src_log("[Load queue source]");
#ifdef FLYLINKDC_USE_DEBUG_10_RECORD
				initQuery(m_get_fly_queue_all_source, "select fly_queue_id,cid,nick,dic_hub from fly_queue_source where fly_queue_id < 10");
#else
				initQuery(m_get_fly_queue_all_source, "select fly_queue_id,cid,nick,dic_hub from fly_queue_source");
#endif
				sqlite3_reader l_q = m_get_fly_queue_all_source->executereader();
				unsigned l_count_users = 0;
				while (l_q.read() && !ClientManager::isBeforeShutdown())
				{
					CID l_cid;
					if (l_q.getblob(1, l_cid.writableData(), 24))
					{
						dcassert(!l_cid.isZero());
						//if (!l_cid.isZero())
						{
							auto& l_source_items = l_sources_map[l_q.getint(0)];
							dcassert(!l_q.getstring(2).empty());
							l_source_items.push_back(CFlySourcesItem(l_cid, l_q.getstring(2), l_q.getint(3)));
							l_count_users++;
						}
					}
				}
				if (!l_sources_map.empty())
				{
					l_src_log.step("Files: " + Util::toString(l_sources_map.size()) + " Users: " + Util::toString(l_count_users));
				}
			}
			const char* l_sql = "select "
			                    "id,"
			                    "Target,"
			                    "Size,"
			                    "Priority,"
			                    "Sections,"
			                    "Added,"
			                    "TTH,"
			                    "TempTarget,"
			                    "AutoPriority,"
			                    "MaxSegments"
			                    " from fly_queue"
#ifdef FLYLINKDC_USE_DEBUG_10_RECORD
			                    " where id < 10"
#endif
			                    ;
			initQuery(m_get_fly_queue, l_sql);
			vector<int64_t> l_bad_targets;
			{
				CFlyLog l_q_files_log("[Load queue files]");
				sqlite3_reader l_q = m_get_fly_queue->executereader();
				while (l_q.read() && !ClientManager::isBeforeShutdown())
				{
					const int64_t l_size = l_q.getint64(2);
					if (l_size < 0)
						continue;
					//[-] brain-ripper
					// int l_flags = QueueItem::FLAG_RESUME;
					int l_flags = 0;
					g_count_queue_files++;
					const string l_target = l_q.getstring(1);
#if 0
					const string l_tgt = l_q.getstring(1);
					string l_target;
					try
					{
						// TODO отложить валидацию на потом
						l_target = QueueManager::checkTarget(l_tgt, l_size, false); // Валидация не проводить - в базе уже храниться хорошо
						if (l_target.empty())
						{
							dcassert(0);
							CFlyServerJSON::pushError(28, "Error CFlylinkDBManager::load_queue l_tgt = " + l_tgt);
							continue;
						}
					}
					catch (const Exception& e)
					{
						l_bad_targets.push_back(l_q.getint64(0));
						LogManager::message("SQLite - load_queue[1]: " + l_tgt + e.getError(), true);
						continue;
					}
#endif
					const QueueItem::Priority l_p = QueueItem::Priority(l_q.getint(3));
					time_t l_added = static_cast<time_t>(l_q.getint64(5));
					if (l_added == 0)
						l_added = GET_TIME();
					TTHValue l_tthRoot;
					if (!l_q.getblob(6, l_tthRoot.data, 24))
					{
						dcassert(0);
						continue;
					}
#ifdef _DEBUG
					if (0)
					{
						const string l_tth_check = l_tthRoot.toBase32();
						if (l_cache_duplicate_tth.find(l_tth_check) == l_cache_duplicate_tth.end())
						{
							initQuery(m_select_transfer_tth, "select distinct path,size from transfer_db.fly_transfer_file where TTH=? and path<>? limit 1");
							m_select_transfer_tth->bind(1, l_tth_check, SQLITE_STATIC);
							m_select_transfer_tth->bind(2, l_target, SQLITE_STATIC);
							sqlite3_reader l_q_tth = m_select_transfer_tth->executereader();
							int l_count_download_tth = 0;
							while (l_q_tth.read())
							{
								l_cache_duplicate_tth.insert(l_tthRoot.toBase32());
								l_count_download_tth++;
								LogManager::message("Already download [" + Util::toString(l_count_download_tth) +
								                    "] TTH = " + l_tthRoot.toBase32() + " Path = " + l_q_tth.getstring(0) + " size = " + Util::toString(l_q_tth.getint64(1)));
								//l_q_tth.getstring(0),
								//l_q.getint64(1),
							}
						}
					}
#endif
					//if(tthRoot.empty()) ?
					//  continue;
					string l_tempTarget = l_q.getstring(7);
					if (l_tempTarget.length() >= MAX_PATH)
					{
						const auto i = l_tempTarget.rfind(PATH_SEPARATOR);
						if (l_tempTarget.length() - i >= MAX_PATH || i == string::npos) // Имя файла больше MAX_PATH - обрежем
						{
							string l_file_name = Util::getFileName(l_tempTarget);
							dcassert(l_file_name.length() >= MAX_PATH);
							if (l_file_name.length() >= MAX_PATH)
							{
								const string l_file_path = Util::getFilePath(l_tempTarget);
								Util::fixFileNameMaxPathLimit(l_file_name);
								l_tempTarget = l_file_path + l_file_name;
							}
						}
					}
					const uint8_t l_maxSegments = uint8_t(l_q.getint(9));
					const int64_t l_ID = l_q.getint64(0);
					m_queue_id = std::max(m_queue_id, l_ID);
					QueueItemPtr qi = QueueManager::FileQueue::find_target(l_target); //TODO после отказа от конвертации XML варианта очереди можно удалить
					if (!qi)
					{
						qi = QueueManager::g_fileQueue.add(l_ID, l_target, l_size, Flags::MaskType(l_flags), l_p,
						                                   l_q.getint(8) != 0,
						                                   l_tempTarget,
						                                   l_added, l_tthRoot, max((uint8_t)1, l_maxSegments));
						if (qi) // Возможны дубли
						{
							dcassert(qi->isDirtyAll() == false);
							qi->setDirty(false);
							l_qitem.push_back(qi);
						}
						else
						{
#ifdef  _DEBUG
							LogManager::message("Skip QueueManager::g_fileQueue.add - l_target = " + l_target);
#endif //  _DEBUG
						}
					}
					if (qi)
					{
						// [+] brain-ripper
						qi->setSectionString(l_q.getstring(4), true);
						const auto l_source_items = l_sources_map.find(l_ID);
						if (l_source_items != l_sources_map.end())
						{
							// TODO - возможно появление дублей
							for (auto i = l_source_items->second.cbegin(); i != l_source_items->second.cend(); ++i)
							{
								add_sourceL(qi, i->m_CID, i->m_nick, i->m_hub_id); //
							}
						}
						else
						{
							//dcassert(0);
						}
						qi->calcAverageSpeedAndCalcAndGetDownloadedBytesL();
						qi->resetDirtyAll();
					}
				}
				if (g_count_queue_source)
				{
					l_q_files_log.step("Items:" + Util::toString(g_count_queue_source));
				}
			}
			// if (!l_sources_map.empty())
			{
				// dcassert(l_sources_map.empty());
				// Удаление пока не делаем
				//for (auto i = l_sources_map.cbegin(); i != l_sources_map.cend(); ++i)
				//{
				//  delete_queue_sourcesL(i->first);
				//}
			}
			{
				// TODO - есть отдельный метод
				CFlyLock(m_cs);
				sqlite3_transaction l_trans(m_flySQLiteDB, l_bad_targets.size() > 1);
				for (auto i = l_bad_targets.cbegin(); i != l_bad_targets.cend(); ++i)
				{
					remove_queue_itemL(*i);
				}
				l_trans.commit();
			}
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - load_queue: " + e.getError(), e.getErrorCode());
		}
	}
	if (!l_qitem.empty())
	{
		QueueManager::getInstance()->fly_fire1(QueueManagerListener::AddedArray(), l_qitem);
	}
	return g_count_queue_source;
}

void CFlylinkDBManager::add_sourceL(const QueueItemPtr& p_QueueItem, const CID& p_cid, const string& p_nick, int p_hub_id)
{
	dcassert(!p_nick.empty());
	dcassert(!p_cid.isZero());
	if (!p_cid.isZero())
	{
		UserPtr l_user = ClientManager::createUser(p_cid, p_nick, p_hub_id); // Создаем юзера в любом случае - http://www.flylinkdc.ru/2012/09/flylinkdc-r502-beta59.html
		bool wantConnection = false;
		try
		{
			CFlyLock(*QueueItem::g_cs); // [+] IRainman fix.
			//TODO- LOCK ??      QueueManager::LockFileQueueShared l_fileQueue; //[+]PPA
			wantConnection = QueueManager::addSourceL(p_QueueItem, l_user, 0, true) && l_user->isOnline(); // Добавить флаг ускоренной загрузки первый раз.
			g_count_queue_source++;
		}
		catch (const Exception &e)
		{
			LogManager::message("CFlylinkDBManager::add_sourceL, Error = " + e.getError(), true);
		}
		if (wantConnection)
		{
			QueueManager::getDownloadConnection(l_user);
		}
		/* [-]
		else
		{
		    dcdebug("p_cid not found p_cid = %s", p_cid.toBase32().c_str());
		}
		[-] */
	}
}
#endif

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
void CFlylinkDBManager::load_global_ratio()
{
	try
	{
		CFlyLock(m_cs);
		std::unique_ptr<sqlite3_command> l_select_global_ratio_load(new sqlite3_command(&m_flySQLiteDB, "select total(upload),total(download) from fly_ratio"));
		// http://www.sqlite.org/lang_aggfunc.html
		// Sum() will throw an "integer overflow" exception if all inputs are integers or NULL and an integer overflow occurs at any point during the computation.
		// Total() never throws an integer overflow.
		sqlite3_reader l_q = l_select_global_ratio_load.get()->executereader();
		if (l_q.read())
		{
			m_global_ratio.set_upload(l_q.getdouble(0));
			m_global_ratio.set_download(l_q.getdouble(1));
		}
#ifdef _DEBUG
		m_is_load_global_ratio = true;
#endif
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_global_ratio: " + e.getError(), e.getErrorCode());
	}
}

bool CFlylinkDBManager::load_last_ip_and_user_stat(uint32_t p_hub_id, const string& p_nick, uint32_t& p_message_count, boost::asio::ip::address_v4& p_last_ip)
{
	dcassert(BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER));
	//CFlyLock(m_cs);
	try
	{
		p_message_count = 0;
#ifdef FLYLINKDC_USE_LASTIP_CACHE
		CFlyFastLock(m_last_ip_cs);
		auto l_find_cache_item = m_last_ip_cache.find(p_hub_id);
		if (l_find_cache_item == m_last_ip_cache.end()) // Хаб первый раз? (TODO - добавить задержку на кол-во запросов. если больше N - выполнить пакетную загрузку)
		{
			auto& l_cache_item = m_last_ip_cache[p_hub_id];
			m_select_all_last_ip_and_message_count.init(m_flySQLiteDB,
			                                            "select last_ip,message_count,nick"
			                                            //"(select flag_index ||'~'|| location from location_db.fly_location_ip where start_ip <= last_ip and stop_ip > last_ip limit 1) location_id,\n"
			                                            //"(select flag_index ||'~'|| country from location_db.fly_country_ip where start_ip <= last_ip and stop_ip > last_ip) country_id\n"
			                                            " from user_db.user_info where dic_hub=?");
			m_select_all_last_ip_and_message_count->bind(1, p_hub_id);
			sqlite3_reader l_q = m_select_all_last_ip_and_message_count->executereader();
			while (l_q.read())
			{
				CFlyLastIPCacheItem l_item;
				l_item.m_last_ip = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
				l_item.m_message_count = l_q.getint64(1);
				l_cache_item.insert(std::make_pair(l_q.getstring(2), l_item));
			}
		}
		auto& l_hub_cache = m_last_ip_cache[p_hub_id]; // TODO - убрать лишний поиск. его нашли уже выше
		const auto& l_cache_nick_item = l_hub_cache.find(p_nick);
		if (l_cache_nick_item != l_hub_cache.end())
		{
			p_message_count = l_cache_nick_item->second.m_message_count;
			p_last_ip = l_cache_nick_item->second.m_last_ip;
			return true;
		}
#else
		initQuery(m_select_last_ip_and_message_count, "select last_ip,message_count from user_db.user_info where nick=? and dic_hub=?");
		sqlite3_command* l_sql_command = m_select_last_ip_and_message_count.get();
		l_sql_command->bind(1, p_nick, SQLITE_STATIC);
		l_sql_command->bind(2, p_hub_id);
		sqlite3_reader l_q = l_sql_command->executereader();
		if (l_q.read())
		{
			p_last_ip = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
			p_message_count = l_q.getint64(1);
			return true;
		}
		
#endif // FLYLINKDC_USE_LASTIP_CACHE
	}
	catch (const database_error& e)
	{
		// errorDB("SQLite - load_last_ip_and_user_stat: " + e.getError(), e.getErrorCode());
		dcassert(0);
		LogManager::message("SQLite - load_last_ip_and_user_stat: " + e.getError());
	}
	return false;
}

bool CFlylinkDBManager::load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info, const boost::asio::ip::address_v4& p_last_ip)
{
	/*
	sqlite> select dic_hub,upload,download,(select name from fly_dic where id = dic_ip) from fly_ratio where dic_nick=(select id from fly_dic where name='FlylinkDC-dev' and dic=2) and dic_hub=789612;
	--EQP-- 0,0,0,SEARCH TABLE fly_ratio USING INDEX iu_fly_ratio (dic_nick=? AND dic_hub=?)
	--EQP-- 0,0,0,EXECUTE SCALAR SUBQUERY 1
	--EQP-- 1,0,0,SEARCH TABLE fly_dic USING COVERING INDEX iu_fly_dic_name (name=? AND dic=?)
	--EQP-- 0,0,0,EXECUTE CORRELATED SCALAR SUBQUERY 2
	--EQP-- 2,0,0,SEARCH TABLE fly_dic USING INTEGER PRIMARY KEY (rowid=?)
	789612|1740114051|0|185.90.227.251
	Memory Used:                         512608 (max 518096) bytes
	Number of Outstanding Allocations:   339 (max 350)
	Number of Pcache Overflow Bytes:     4096 (max 4096) bytes
	Number of Scratch Overflow Bytes:    0 (max 0) bytes
	Largest Allocation:                  425600 bytes
	Largest Pcache Allocation:           4096 bytes
	Largest Scratch Allocation:          0 bytes
	Lookaside Slots Used:                17 (max 78)
	Successful lookaside attempts:       335
	Lookaside failures due to size:      92
	Lookaside failures due to OOM:       0
	Pager Heap Usage:                    72624 bytes
	Page cache hits:                     16
	Page cache misses:                   16
	Page cache writes:                   0
	Schema Heap Usage:                   13392 bytes
	Statement Heap/Lookaside Usage:      4008 bytes
	Fullscan Steps:                      0
	Sort Operations:                     0
	Autoindex Inserts:                   0
	Virtual Machine Steps:               42
	
	
	After vacuum
	
	Memory Used:                         512608 (max 518592) bytes
	Number of Outstanding Allocations:   339 (max 390)
	Number of Pcache Overflow Bytes:     4096 (max 4096) bytes
	Number of Scratch Overflow Bytes:    0 (max 0) bytes
	Largest Allocation:                  425600 bytes
	Largest Pcache Allocation:           4096 bytes
	Largest Scratch Allocation:          0 bytes
	Lookaside Slots Used:                15 (max 71)
	Successful lookaside attempts:       107
	Lookaside failures due to size:      53
	Lookaside failures due to OOM:       0
	Pager Heap Usage:                    68380 bytes
	Page cache hits:                     2
	Page cache misses:                   15
	Page cache writes:                   0
	Schema Heap Usage:                   13392 bytes
	Statement Heap/Lookaside Usage:      4008 bytes
	Fullscan Steps:                      0
	Sort Operations:                     0
	Autoindex Inserts:                   0
	Virtual Machine Steps:               42
	
	*/
	dcassert(BOOLSETTING(ENABLE_RATIO_USER_LIST));
	dcassert(p_hub_id != 0);
	dcassert(!p_nick.empty());
	bool l_res = false;
	try
	{
		if (!p_last_ip.is_unspecified()) // Если нет в таблице user_db.user_info, в fly_ratio можно не ходить - там ничего нет
		{
			CFlyLock(m_cs); // TODO - убирать нельзя падаем https://drdump.com/Problem.aspx?ProblemID=118720
			initQuery(m_select_ratio_load,
				"select upload,download,(select name from fly_dic where id = dic_ip) " // TODO перевести на хранение IP как числа?
				"from fly_ratio where dic_nick=(select id from fly_dic where name=? and dic=2) and dic_hub=? ");
			m_select_ratio_load->bind(1, p_nick, SQLITE_STATIC);
			m_select_ratio_load->bind(2, p_hub_id);
			sqlite3_reader l_q = m_select_ratio_load->executereader();
			string l_ip_from_ratio;
			while (l_q.read())
			{
				l_ip_from_ratio = l_q.getstring(2);
				// dcassert(!l_ip_from_ratio.empty()); // TODO - сделать зачистку таких
				if (!l_ip_from_ratio.empty())
				{
					boost::system::error_code ec;
					const auto l_ip = boost::asio::ip::address_v4::from_string(l_ip_from_ratio, ec);
					dcassert(!ec);
					if (!l_ip_from_ratio.empty())
					{
						const auto l_u = l_q.getint64(0);
						const auto l_d = l_q.getint64(1);
						dcassert(l_d || l_u);
						p_ratio_info.addDownload(l_ip, l_d);
						p_ratio_info.addUpload(l_ip, l_u);
						l_res = true;
					}
				}
			}
			p_ratio_info.reset_dirty();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_ratio: " + e.getError(), e.getErrorCode());
	}
	return l_res;
}

uint32_t CFlylinkDBManager::get_dic_hub_id(const string& p_hub)
{
	CFlyLock(m_cs);
	return get_dic_idL(p_hub, e_DIC_HUB, true);
}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

int64_t CFlylinkDBManager::get_dic_location_id(const string& p_location)
{
	CFlyLock(m_cs);
	return get_dic_idL(p_location, e_DIC_LOCATION, true);
}

#if 0
void CFlylinkDBManager::clear_dic_cache_location()
{
	clear_dic_cache(e_DIC_LOCATION);
}

void CFlylinkDBManager::clear_dic_cache_country()
{
	clear_dic_cache(e_DIC_COUNTRY);
}

void CFlylinkDBManager::clear_dic_cache(const eTypeDIC p_DIC)
{
	CFlyLock(m_cs);
	m_DIC[p_DIC - 1].clear();
}
#endif

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
void CFlylinkDBManager::store_all_ratio_internal(uint32_t p_hub_id, const int64_t p_dic_nick,
                                                 const int64_t p_ip,
                                                 const uint64_t p_upload,
                                                 const uint64_t p_download
                                                )
{
	m_update_ratio->bind(3, p_ip);
	m_update_ratio->bind(1, (long long) p_upload);
	m_update_ratio->bind(2, (long long) p_download);
	m_update_ratio->executenonquery();
	if (m_flySQLiteDB.changes() == 0)
	{
		m_insert_ratio->bind(4, p_dic_nick);
		m_insert_ratio->bind(5, p_hub_id);
		m_insert_ratio->bind(3, p_ip);
		m_insert_ratio->bind(1, (long long) p_upload);
		m_insert_ratio->bind(2, (long long) p_download);
		m_insert_ratio->executenonquery();
	}
}

void CFlylinkDBManager::store_all_ratio_and_last_ip(uint32_t p_hub_id,
                                                    const string& p_nick,
                                                    CFlyUserRatioInfo& p_user_ratio,
                                                    const uint32_t p_message_count,
                                                    const boost::asio::ip::address_v4& p_last_ip,
                                                    bool p_is_last_ip_dirty,
                                                    bool p_is_message_count_dirty,
                                                    bool& p_is_sql_not_found)
{
	dcassert(BOOLSETTING(ENABLE_RATIO_USER_LIST));
	CFlyLock(m_cs);
	try
	{
		dcassert(p_hub_id);
		dcassert(!p_nick.empty());
		const bool l_is_exist_map = p_user_ratio.getUploadDownloadMap() && !p_user_ratio.getUploadDownloadMap()->empty();
		const int64_t l_dic_nick = get_dic_idL(p_nick, e_DIC_NICK, true);
		// Транзакции делать нельзя
		// sqlite3_transaction l_trans_insert(m_flySQLiteDB, p_user_ratio.getUploadDownloadMap()->size() > 1);
		initQuery(m_update_ratio, "update fly_ratio set upload=?,download=? where dic_ip=? and dic_nick=? and dic_hub=?");
		initQuery(m_insert_ratio, "insert or replace into fly_ratio(upload,download,dic_ip,dic_nick,dic_hub) values(?,?,?,?,?)");
		// TODO провести конвертацию в другой формат и файл БД + отказаться от DIC
		m_update_ratio->bind(4, l_dic_nick);
		m_update_ratio->bind(5, p_hub_id);
		int64_t l_last_ip_id = 0;
		if (l_is_exist_map)
		{
			for (auto i = p_user_ratio.getUploadDownloadMap()->begin(); i != p_user_ratio.getUploadDownloadMap()->end(); ++i)
			{
				l_last_ip_id = get_dic_idL(boost::asio::ip::address_v4(i->first).to_string(), e_DIC_IP, true);
				dcassert(i->second.get_upload() != 0 || i->second.get_download() != 0);
				if (l_last_ip_id &&  // Коннект еще не наступил - не пишем в базу 0
				        i->second.is_dirty() &&
				        (i->second.get_upload() != 0 || i->second.get_download() != 0)) // Если все по нулям - тоже странно
				{
					store_all_ratio_internal(p_hub_id, l_dic_nick, l_last_ip_id, i->second.get_upload(), i->second.get_download());
					i->second.reset_dirty();
				}
			}
		}
		if (p_user_ratio.is_dirty() && !p_user_ratio.m_ip.is_unspecified())
		{
			l_last_ip_id = get_dic_idL(p_user_ratio.m_ip.to_string(), e_DIC_IP, true);
			store_all_ratio_internal(p_hub_id, l_dic_nick, l_last_ip_id, p_user_ratio.get_upload(), p_user_ratio.get_download());
			p_user_ratio.reset_dirty();
		}
		// Иначе фиксируем только последний IP и cчетчик мессаг
		if (p_is_last_ip_dirty || p_is_message_count_dirty)
		{
			update_last_ip_deferredL(p_hub_id, p_nick, p_message_count, p_last_ip, p_is_sql_not_found,
			                         p_is_last_ip_dirty,
			                         p_is_message_count_dirty
			                        );
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - store_all_ratio_and_last_ip: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::update_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick,
                                                         const boost::asio::ip::address_v4& p_last_ip,
                                                         const uint32_t p_message_count,
                                                         bool& p_is_sql_not_found,
                                                         const bool p_is_last_ip_dirty,
                                                         const bool p_is_message_count_dirty
                                                        )
{
#ifndef FLYLINKDC_USE_LASTIP_CACHE
	CFlyLock(m_cs);
#endif
	try
	{
		update_last_ip_deferredL(p_hub_id, p_nick, p_message_count, p_last_ip, p_is_sql_not_found,
		                         p_is_last_ip_dirty,
		                         p_is_message_count_dirty);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - update_last_ip_and_message_count: " + e.getError(), e.getErrorCode());
	}
}

void CFlylinkDBManager::flush()
{
	flush_all_last_ip_and_message_count();
}

void CFlylinkDBManager::flush_all_last_ip_and_message_count()
{
#ifdef FLYLINKDC_USE_LASTIP_CACHE
	CFlyLock(m_cs);
	try
	{
		CFlyFastLock(m_last_ip_cs);
		CFlyLogFile l_log("[sqlite - flush-user-info]");
		int l_count = 0;
		{
			sqlite3_transaction l_trans_insert(m_flySQLiteDB, m_last_ip_cache.size() > 1);
			for (auto h = m_last_ip_cache.begin(); h != m_last_ip_cache.end(); ++h)
			{
				for (auto i = h->second.begin(); i != h->second.end(); ++i)
				{
					if (i->second.m_is_item_dirty)
					{
#ifdef _DEBUG
						{
							// Проверим что данные не затираются
							m_check_message_count.init(m_flySQLiteDB,
							"select message_count from user_db.user_info where nick=? and dic_hub=?");
							sqlite3_command* l_sql_command = m_check_message_count.get();
							l_sql_command->bind(1, i->first, SQLITE_STATIC);
							l_sql_command->bind(2, h->first);
							sqlite3_reader l_q = l_sql_command->executereader();
							if (l_q.read())
							{
								const auto l_message_count = l_q.getint64(0);
								if (l_message_count > i->second.m_message_count)
								{
									dcassert(0);
									l_log.log("Error update message_count for user = " + i->first +
									" new_message_count = " + Util::toString(i->second.m_message_count) +
									" sqlite_message_count = " + Util::toString(l_message_count)
									         );
									// В базе оказалось знаение больше чем пишется - не затираем его нужно разбираться когда так получается
									i->second.m_is_item_dirty = false;
									continue;
								}
							}
						}
#endif
						++l_count;
						m_insert_store_all_ip_and_message_count.init(m_flySQLiteDB,
						                                             "insert or replace into user_db.user_info (nick,dic_hub,last_ip,message_count) values(?,?,?,?)");
						sqlite3_command* l_sql = m_insert_store_all_ip_and_message_count.get_sql();
						l_sql->bind(1, i->first, SQLITE_STATIC);
						l_sql->bind(2, h->first));
						l_sql->bind(3, i->second.m_last_ip.to_uint());
						l_sql->bind(4, i->second.m_message_count);
						l_sql->executenonquery();
						i->second.m_is_item_dirty = false;
					}
				}
			}
			l_trans_insert.commit();
		}
		if (l_count)
		{
			l_log.log("Save dirty record user_db.user_info:" + Util::toString(l_count));
		}
		
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - flush_all_last_ip_and_message_count: " + e.getError(), e.getErrorCode());
	}
#endif // FLYLINKDC_USE_LASTIP_CACHE
}

void CFlylinkDBManager::update_last_ip_deferredL(uint32_t p_hub_id, const string& p_nick,
                                                 uint32_t p_message_count,
                                                 boost::asio::ip::address_v4 p_last_ip,
                                                 bool& p_is_sql_not_found,
                                                 const bool p_is_last_ip_dirty,
                                                 const bool p_is_message_count_dirty
                                                )
{
	dcassert(BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER));
	
	dcassert(p_hub_id);
	dcassert(!p_nick.empty());
#ifdef FLYLINKDC_USE_LASTIP_CACHE
	CFlyFastLock(m_last_ip_cs);
#ifdef _DEBUG
	{
#if 0
		m_select_store_ip.init(m_flySQLiteDB,
		                       "select last_ip,message_count from user_db.user_info where nick=? and dic_hub=?");
		m_select_store_ip->bind(1, p_nick, SQLITE_STATIC);
		m_select_store_ip->bind(2, p_hub_id);
		sqlite3_reader l_q = m_select_store_ip.get()->executereader();
		while (l_q.read())
		{
			const auto l_current_count = l_q.getint64(1);
			if (p_message_count < l_current_count)
			{
				dcassert(p_message_count >= l_current_count);
			}
		}
#endif
	}
#endif
	
	auto& l_hub_cache_item = m_last_ip_cache[p_hub_id][p_nick];
	if (!p_last_ip.is_unspecified() && p_message_count)
	{
#if 0
		m_insert_store_ip_and_message_count.init(m_flySQLiteDB,
		                                         "insert or replace into user_db.user_info (nick,dic_hub,last_ip,message_count) values(?,?,?,?)");
		sqlite3_command* l_sql = m_insert_store_ip_and_message_count.get();
		l_sql->bind(1, p_nick, SQLITE_STATIC);
		l_sql->bind(2, p_hub_id);
		l_sql->bind(3, p_last_ip.to_uint());
		l_sql->bind(4, p_message_count);
		l_sql->executenonquery();
#endif
		if (l_hub_cache_item.m_last_ip != p_last_ip || l_hub_cache_item.m_message_count != p_message_count)
		{
			l_hub_cache_item.m_is_item_dirty = true;
		}
		l_hub_cache_item.m_last_ip = p_last_ip;
		l_hub_cache_item.m_message_count = p_message_count;
	}
	else
	{
		if (!p_last_ip.is_unspecified())
		{
#if 0
			m_insert_store_ip.init(m_flySQLiteDB,
			                       "insert or replace into user_db.user_info (nick,dic_hub,last_ip) values(?,?,?)");
			sqlite3_command* l_sql = m_insert_store_ip.get();
			l_sql->bind(1, p_nick, SQLITE_STATIC);
			l_sql->bind(2, p_hub_id);
			l_sql->bind(3, p_last_ip.to_uint());
			l_sql->executenonquery();
#endif
			if (l_hub_cache_item.m_last_ip != p_last_ip)
			{
				l_hub_cache_item.m_is_item_dirty = true;
			}
			
			l_hub_cache_item.m_last_ip = p_last_ip;
		}
		if (p_message_count)
		{
#if 0
			m_insert_store_message_count.init(m_flySQLiteDB,
			                                  "insert or replace into user_db.user_info (nick,dic_hub,message_count) values(?,?,?)");
			sqlite3_command* l_sql = m_insert_store_message_count.get();
			l_sql->bind(1, p_nick, SQLITE_STATIC);
			l_sql->bind(2, p_hub_id);
			l_sql->bind(3, p_message_count);
			l_sql->executenonquery();
#endif
			if (l_hub_cache_item.m_message_count != p_message_count)
			{
				l_hub_cache_item.m_is_item_dirty = true;
			}
			l_hub_cache_item.m_message_count = p_message_count;
		}
	}
#else
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
	if (p_message_count == 0 || p_last_ip.is_unspecified())
	{
		CFlyIPMessageCache l_old = m_IPCacheLevelDB.get_last_ip_and_message_count(p_hub_id, p_nick);
		if (p_message_count == 0)
			p_message_count = l_old.m_message_count;
		if (p_last_ip.is_unspecified())
			p_last_ip = boost::asio::ip::address_v4(l_old.m_ip);
	}
	m_IPCacheLevelDB.set_last_ip_and_message_count(p_hub_id, p_nick, p_message_count, p_last_ip);
#else
	
	if (!p_last_ip.is_unspecified() && p_message_count)
	{
		int changes = 0;
		if (p_is_sql_not_found == false)
		{
			initQuery(m_update_last_ip_and_message_count,
				"update user_db.user_info set last_ip=?,message_count=? where dic_hub=? and nick=?");
			m_update_last_ip_and_message_count->bind(1, p_last_ip.to_uint());
			m_update_last_ip_and_message_count->bind(2, p_message_count);
			m_update_last_ip_and_message_count->bind(3, p_hub_id);
			m_update_last_ip_and_message_count->bind(4, p_nick, SQLITE_STATIC);
			m_update_last_ip_and_message_count->executenonquery();
			changes = m_flySQLiteDB.changes();
		}
		if (p_is_sql_not_found == true || changes == 0)
		{
			initQuery(m_insert_last_ip_and_message_count,
				"insert or replace into user_db.user_info(nick,dic_hub,last_ip,message_count) values(?,?,?,?)");
			m_insert_last_ip_and_message_count->bind(1, p_nick, SQLITE_STATIC);
			m_insert_last_ip_and_message_count->bind(2, p_hub_id);
			m_insert_last_ip_and_message_count->bind(3, p_last_ip.to_uint());
			m_insert_last_ip_and_message_count->bind(4, p_message_count);
			m_insert_last_ip_and_message_count->executenonquery();
		}
		p_is_sql_not_found = false;
	}
	else
	{
		if (!p_last_ip.is_unspecified())
		{
			//LogManager::message("Update lastip p_nick = " + p_nick);
			int changes = 0;
			if (p_is_sql_not_found == false)
			{
				initQuery(m_update_last_ip, "update user_db.user_info set last_ip=? where dic_hub=? and nick=?");
				m_update_last_ip->bind(1, p_last_ip.to_uint());
				m_update_last_ip->bind(2, p_hub_id);
				m_update_last_ip->bind(3, p_nick, SQLITE_STATIC);
				m_update_last_ip->executenonquery();
				changes = m_flySQLiteDB.changes();
			}
			if (p_is_sql_not_found == true || changes == 0)
			{
				boost::asio::ip::address_v4 l_ip_from_db;
				// Проверим наличие в базе записи
				{
					initQuery(m_select_last_ip, "select last_ip from user_db.user_info where nick=? and dic_hub=?");
					m_select_last_ip->bind(1, p_nick, SQLITE_STATIC);
					m_select_last_ip->bind(2, p_hub_id);
					sqlite3_reader l_q = m_select_last_ip->executereader();
					if (l_q.read())
					{
						l_ip_from_db = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
						p_is_sql_not_found = false;
					}
					else
					{
					}
				}
				if (p_last_ip != l_ip_from_db)
				{
					initQuery(m_insert_last_ip, "insert or replace into user_db.user_info(nick,dic_hub,last_ip) values(?,?,?)");
					m_insert_last_ip->bind(1, p_nick, SQLITE_STATIC);
					m_insert_last_ip->bind(2, p_hub_id);
					m_insert_last_ip->bind(3, p_last_ip.to_uint());
					m_insert_last_ip->executenonquery();
				}
				p_is_sql_not_found = false;
			}
		}
		else if (p_message_count)
		{
			int changes = 0;
			if (p_is_sql_not_found == false)
			{
				initQuery(m_update_message_count, "update user_db.user_info set message_count=? where dic_hub=? and nick=?");
				m_update_message_count->bind(1, p_message_count);
				m_update_message_count->bind(2, p_hub_id);
				m_update_message_count->bind(3, p_nick, SQLITE_STATIC);
				m_update_message_count->executenonquery();
				changes = m_flySQLiteDB.changes();
			}
			if (p_is_sql_not_found == true || changes == 0)
			{
				initQuery(m_insert_message_count, "insert or replace into user_db.user_info(nick,dic_hub,message_count) values(?,?,?)");
				m_insert_message_count->bind(1, p_nick, SQLITE_STATIC);
				m_insert_message_count->bind(2, p_hub_id);
				m_insert_message_count->bind(3, p_message_count);
				m_insert_message_count->executenonquery();
			}
			p_is_sql_not_found = false;
		}
		else
		{
			dcassert(0);
		}
	}
#endif // FLYLINKDC_USE_IPCACHE_LEVELDB
	
#endif // FLYLINKDC_USE_LASTIP_CACHE
	
}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

bool CFlylinkDBManager::is_table_exists(const string& p_table_name)
{
	dcassert(p_table_name == Text::toLower(p_table_name));
	return m_flySQLiteDB.executeint(
	           "select count(*) from sqlite_master where type = 'table' and lower(tbl_name) = '" + p_table_name + "'") != 0;
}

int64_t CFlylinkDBManager::find_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_cache_result)
{
	initQuery(m_select_fly_dic, "select id from fly_dic where name=? and dic=?");
	sqlite3_command* sql = m_select_fly_dic.get();
	sql->bind(1, p_name, SQLITE_STATIC);
	sql->bind(2, p_DIC);
	sqlite3_reader r = sql->executereader();
	if (!r.read()) return 0;
	int64_t l_dic_id = r.getint64(0);
	if (l_dic_id && p_cache_result)
		m_DIC[p_DIC - 1][p_name] = l_dic_id;
	return l_dic_id;
}

int64_t CFlylinkDBManager::get_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_create)
{
#ifdef _DEBUG
	static int g_count = 0;
	LogManager::message("[" + Util::toString(++g_count) + "] get_dic_idL " + p_name + " type = " + Util::toString(p_DIC) + " is_create = " + Util::toString(p_create));
#endif
	dcassert(!p_name.empty());
	if (p_name.empty())
		return 0;
	try
	{
		auto& l_cache_dic = m_DIC[p_DIC - 1];
		if (!p_create)
		{
			auto i =  l_cache_dic.find(p_name);
			if (i != l_cache_dic.end()) // [1] https://www.box.net/shared/8f01665fe1a5d584021f
				return i->second;
			else
				return find_dic_idL(p_name, p_DIC, true);
		}
		int64_t& l_Cache_ID = l_cache_dic[p_name];
		if (l_Cache_ID)
			return l_Cache_ID;
		l_Cache_ID = find_dic_idL(p_name, p_DIC, false);
		if (!l_Cache_ID)
		{
			initQuery(m_insert_fly_dic, "insert into fly_dic (dic,name) values(?,?)");
			sqlite3_command* l_sql = m_insert_fly_dic.get();
			l_sql->bind(1, p_DIC);
			l_sql->bind(2, p_name, SQLITE_STATIC);
			l_sql->executenonquery();
			l_Cache_ID = m_flySQLiteDB.insertid();
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
			if (p_DIC == e_DIC_HUB)
			{
				CFlyFastLock(m_hub_dic_fcs);
				m_HubNameMap[l_Cache_ID] = p_name;
			}
#endif
		}
		return l_Cache_ID;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_dic_idL: " + e.getError(), e.getErrorCode());
	}
	return 0;
}

#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
string CFlylinkDBManager::get_hub_name(unsigned p_hub_id)
{
	CFlyFastLock(m_hub_dic_fcs);
	const string l_name = m_HubNameMap[p_hub_id];
	dcassert(!l_name.empty())
	return l_name;
}
#endif

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
#ifdef _DEBUG
	{
#ifdef FLYLINKDC_USE_LASTIP_CACHE
		CFlyFastLock(m_last_ip_cs);
		for (auto h = m_last_ip_cache.cbegin(); h != m_last_ip_cache.cend(); ++h)
		{
			for (auto i = h->second.begin(); i != h->second.end(); ++i)
			{
				dcassert(i->second.m_is_item_dirty == false);
			}
		}
#endif //FLYLINKDC_USE_LASTIP_CACHE
	}
#ifdef FLYLINKDC_USE_GEO_IP
	{
		dcdebug("CFlylinkDBManager::m_country_cache size = %d\n", m_country_cache.size());
	}
#endif
	{
		dcdebug("CFlylinkDBManager::m_location_cache_array size = %d\n", m_location_cache_array.size());
	}
#endif // _DEBUG
}

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
double CFlylinkDBManager::get_ratio() const
{
	dcassert(m_is_load_global_ratio);
	return m_global_ratio.get_ratio();
}

tstring CFlylinkDBManager::get_ratioW() const
{
	dcassert(m_is_load_global_ratio);
	if (m_global_ratio.get_download() > 0)
	{
		LocalArray<TCHAR, 32> buf;
		_snwprintf(buf.data(), buf.size(), _T("%.2f"), get_ratio());
		return buf.data();
	}
	return Util::emptyStringT;
}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

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
