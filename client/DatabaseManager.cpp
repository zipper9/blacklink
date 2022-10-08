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
#include "FinishedItem.h"
#include "TimerManager.h"
#include "NetworkUtil.h"
#include "BusyCounter.h"
#include "SocketAddr.h"
#include "Random.h"
#include "Tag16.h"
#include "HttpClient.h"
#include "ZUtils.h"
#include "ResourceManager.h"
#include "maxminddb/maxminddb.h"
#include <boost/algorithm/string.hpp>

using sqlite3x::database_error;
using sqlite3x::sqlite3_transaction;
using sqlite3x::sqlite3_reader;

bool g_EnableSQLtrace = false; // http://www.sqlite.org/c3ref/profile.html
bool g_UseSynchronousOff = false;
int g_DisableSQLtrace = 0;

static const unsigned BUSY_TIMEOUT = 120000;
static const unsigned GEOIP_DOWNLOAD_RETRY_TIME = 600000; // 10 minutes

static const string fileNameGeoIP = "country_ip_db.mmdb";

static const char* fileNames[] =
{
	"",
	"locations",
	"transfers",
	"user"
};

static const char* dbNames[] =
{
	"main",
	"location_db",
	"transfer_db",
	"user_db"
};

#ifdef _WIN32
static int64_t posixTimeToLocal(int64_t pt)
{
	static const int64_t offset = 11644473600ll;
	static const int64_t scale = 10000000ll;
	int64_t filetime = (pt + offset) * scale;
	int64_t local;
	if (!FileTimeToLocalFileTime((FILETIME *) &filetime, (FILETIME *) &local)) return 0;
	return local / scale - offset;
}
#else
static int64_t posixTimeToLocal(int64_t pt)
{
	time_t t = (time_t) pt;
	tm local;
#ifdef HAVE_TIME_R
	localtime_r(&t, &local);
#else
	local = *localtime(&t);
#endif
	t = timegm(&local);
	return t == (time_t) -1 ? pt : t;
}
#endif

static int traceCallback(unsigned what, void* ctx, void* p, void* x)
{
	if (g_DisableSQLtrace == 0)
	{
		if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
		{
			const char* z = static_cast<const char*>(x);
			if (*z == '-') return 0;
			char* sql = sqlite3_expanded_sql(static_cast<sqlite3_stmt*>(p));
			StringMap params;
			params["sql"] = sql;
			params["thread_id"] = Util::toString(BaseThread::getCurrentThreadId());
			LOG(SQLITE_TRACE, params);
			free(sql);
		}
	}
	return 0;
}

int DatabaseConnection::progressHandler(void* ctx)
{
	auto conn = static_cast<DatabaseConnection*>(ctx);
	int result = conn->abortFlag && conn->abortFlag->load();
#ifdef _DEBUG
	if (result) LogManager::message("DatabaseManager: statement aborted on conn " + Util::toHexString(conn));
#endif
	return result;
}

void DatabaseConnection::initQuery(sqlite3_command &command, const char *sql)
{
	if (command.empty()) command.open(&connection, sql);
}

void DatabaseConnection::setAbortFlag(std::atomic_bool* af)
{
	abortFlag = af;
	if (af)
		sqlite3_progress_handler(connection.getdb(), 100, progressHandler, this);
	else
		sqlite3_progress_handler(connection.getdb(), 0, nullptr, nullptr);
}

void DatabaseConnection::attachDatabase(const string& path, const string& file, const string& prefix, const string& name)
{
	string cmd = "attach database '";
	string filePath = path;
	filePath += prefix;
	filePath += '_';
	filePath += file;
	filePath += ".sqlite";
	DatabaseManager::quoteString(filePath);
	cmd += filePath;
	cmd += "' as ";
	cmd += name;
	connection.executenonquery(cmd.c_str());
}

void DatabaseConnection::setPragma(const char* pragma)
{
	for (int i = 0; i < _countof(dbNames); ++i)
	{
		string sql = "pragma ";
		sql += dbNames[i];
		sql += '.';
		sql += pragma;
		sql += ';';
		connection.executenonquery(sql);
	}
}

bool DatabaseConnection::hasTable(const string& tableName, const string& db)
{
	dcassert(tableName == Text::toLower(tableName));
	string prefix = db;
	if (!prefix.empty()) prefix += '.';
	return connection.executeint("select count(*) from " + prefix + "sqlite_master where type = 'table' and lower(tbl_name) = '" + tableName + "'") != 0;
}

bool DatabaseConnection::safeAlter(const char* sql, bool verbose)
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

void DatabaseManager::quoteString(string& s) noexcept
{
	string::size_type i = 0;
	while (i < s.length())
	{
		string::size_type j = s.find('\'', i);
		if (j == string::npos) break;
		s.insert(j, 1, '\'');
		i = j + 2;
	}
}

void DatabaseConnection::open(const string& prefix, int journalMode)
{
	const string& dbPath = Util::getConfigPath();
	connection.open((dbPath + prefix + ".sqlite").c_str());

	for (int i = 1; i < _countof(dbNames); i++)
		attachDatabase(dbPath, fileNames[i], prefix, dbNames[i]);

	if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
		sqlite3_trace_v2(connection.getdb(), SQLITE_TRACE_STMT, traceCallback, nullptr);
	sqlite3_busy_timeout(connection.getdb(), BUSY_TIMEOUT);

	setPragma("page_size=4096");
	if (journalMode == DatabaseManager::JOUNRAL_MODE_MEMORY)
	{
		setPragma("journal_mode=MEMORY");
	}
	else
	{
		if (journalMode == DatabaseManager::JOUNRAL_MODE_WAL)
			setPragma("journal_mode=WAL");
		else
			setPragma("journal_mode=PERSIST");
		setPragma("secure_delete=OFF"); // http://www.sqlite.org/pragma.html#pragma_secure_delete
		if (g_UseSynchronousOff)
			setPragma("synchronous=OFF");
		else
			setPragma("synchronous=FULL");
	}
	setPragma("temp_store=MEMORY");
}

void DatabaseConnection::upgradeDatabase()
{
#ifdef BL_FEATURE_IP_DATABASE
	bool hasRatioTable = hasTable("fly_ratio");
	bool hasUserTable = hasTable("user_info", "user_db");
	bool hasNewStatTables = hasTable("ip_stat") || hasTable("user_stat", "user_db");

	connection.executenonquery("CREATE TABLE IF NOT EXISTS ip_stat("
	                              "cid blob not null, ip text not null, upload integer not null, download integer not null)");;
	connection.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_ip_stat ON ip_stat(cid, ip);");

	connection.executenonquery("CREATE TABLE IF NOT EXISTS user_db.user_stat("
		                              "cid blob primary key, nick text not null, last_ip text not null, message_count integer not null, pm_in integer not null, pm_out integer not null)");
#endif

	int userVersion = connection.executeint("PRAGMA user_version");

	connection.executenonquery(
		"CREATE TABLE IF NOT EXISTS fly_ignore(nick text PRIMARY KEY NOT NULL, type integer not null);");
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

	connection.executenonquery(
	    "CREATE TABLE IF NOT EXISTS location_db.fly_p2pguard_ip(start_ip integer not null,stop_ip integer not null,note text,type integer);");
	connection.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_ip ON fly_p2pguard_ip(start_ip);");
	connection.executenonquery("DROP INDEX IF EXISTS location_db.i_fly_p2pguard_note;");
	connection.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_type ON fly_p2pguard_ip(type);");
	safeAlter("ALTER TABLE location_db.fly_p2pguard_ip add column type integer");

	connection.executenonquery(
	    "CREATE TABLE IF NOT EXISTS location_db.fly_location_ip(start_ip integer not null,stop_ip integer not null,location text,flag_index integer);");

	safeAlter("ALTER TABLE location_db.fly_location_ip add column location text");

	connection.executenonquery(
		"CREATE INDEX IF NOT EXISTS location_db.i_fly_location_ip ON fly_location_ip(start_ip);");
	connection.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file("
	                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
	                              "tth char(39),path text not null,nick text, hub text,size int64 not null,speed int,ip text, actual int64);");
	connection.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_day_type ON fly_transfer_file(day,type);");

	safeAlter("ALTER TABLE transfer_db.fly_transfer_file add column actual int64");

	if (userVersion < 5)
		deleteOldTransferHistory();

	if (userVersion < 12)
	{
		safeAlter("ALTER TABLE fly_ignore add column type integer");
		connection.executenonquery("PRAGMA user_version=12");
	}

#ifdef BL_FEATURE_IP_DATABASE
	if (!hasNewStatTables && (hasRatioTable || hasUserTable))
		convertStatTables(hasRatioTable, hasUserTable);
#endif
}

void DatabaseConnection::setRegistryVarString(DBRegistryType type, const string& value)
{
	DBRegistryMap m;
	m[Util::toString(type)] = value;
	saveRegistry(m, type, false);
}

string DatabaseConnection::getRegistryVarString(DBRegistryType type)
{
	DBRegistryMap m;
	loadRegistry(m, type);
	if (!m.empty())
		return m.begin()->second.sval;
	else
		return Util::emptyString;
}

void DatabaseConnection::setRegistryVarInt(DBRegistryType type, int64_t value)
{
	DBRegistryMap m;
	m[Util::toString(type)] = DBRegistryValue(value);
	saveRegistry(m, type, false);
}

int64_t DatabaseConnection::getRegistryVarInt(DBRegistryType type)
{
	DBRegistryMap m;
	loadRegistry(m, type);
	if (!m.empty())
		return m.begin()->second.ival;
	else
		return 0;
}

void DatabaseConnection::loadRegistry(DBRegistryMap& values, DBRegistryType type)
{
	try
	{
		initQuery(selectRegistry, "select key,val_str,val_number from fly_registry where segment=? order by rowid");
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
		DatabaseManager::getInstance()->reportError("SQLite - loadRegistry: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::clearRegistry(DBRegistryType type, int64_t tick)
{
	try
	{
		clearRegistryL(type, tick);
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - clearRegistry: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::clearRegistryL(DBRegistryType type, int64_t tick)
{
	initQuery(deleteRegistry, "delete from fly_registry where segment=? and tick_count<>?");
	deleteRegistry.bind(1, type);
	deleteRegistry.bind(2, (long long) tick);
	deleteRegistry.executenonquery();
}

void DatabaseConnection::saveRegistry(const DBRegistryMap& values, DBRegistryType type, bool clearOldValues)
{
	try
	{
		const int64_t tick = getRandValForRegistry();
		initQuery(insertRegistry, "insert or replace into fly_registry (segment,key,val_str,val_number,tick_count) values(?,?,?,?,?)");
		initQuery(updateRegistry, "update fly_registry set val_str=?,val_number=?,tick_count=? where segment=? and key=?");
		sqlite3_transaction trans(connection, values.size() > 1 || clearOldValues);
		for (auto k = values.cbegin(); k != values.cend(); ++k)
		{
			const auto& val = k->second;
			updateRegistry.bind(1, val.sval, SQLITE_TRANSIENT);
			updateRegistry.bind(2, (long long) val.ival);
			updateRegistry.bind(3, (long long) tick);
			updateRegistry.bind(4, int(type));
			updateRegistry.bind(5, k->first, SQLITE_TRANSIENT);
			updateRegistry.executenonquery();
			if (connection.changes() == 0)
			{
				insertRegistry.bind(1, int(type));
				insertRegistry.bind(2, k->first, SQLITE_TRANSIENT);
				insertRegistry.bind(3, val.sval, SQLITE_TRANSIENT);
				insertRegistry.bind(4, (long long) val.ival);
				insertRegistry.bind(5, (long long) tick);
				insertRegistry.executenonquery();
			}
		}
		if (clearOldValues)
			clearRegistryL(type, tick);
		trans.commit();
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - saveRegistry: " + e.getError(), e.getErrorCode());
	}
}

int64_t DatabaseConnection::getRandValForRegistry()
{
	int64_t val;
	while (true)
	{
		val = ((int64_t) Util::rand() << 31) ^ Util::rand();
		initQuery(selectTick, "select count(*) from fly_registry where tick_count=?");
		selectTick.bind(1, (long long) val);
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

void DatabaseConnection::deleteOldTransferHistory()
{
	if ((SETTING(DB_LOG_FINISHED_DOWNLOADS) || SETTING(DB_LOG_FINISHED_UPLOADS)))
	{
		int64_t timestamp = posixTimeToLocal(time(nullptr));
		int currentDay = timestamp/(60*60*24);

		string sqlDC = makeDeleteOldTransferHistory("fly_transfer_file", currentDay);
		sqlite3_command cmdDC(&connection, sqlDC);
		cmdDC.executenonquery();
	}
}

void DatabaseConnection::loadTransferHistorySummary(eTypeTransfer type, vector<TransferHistorySummary> &out)
{
	DatabaseManager* dm = DatabaseManager::getInstance();
	try
	{
		if (dm->deleteOldTransfers)
		{
			dm->deleteOldTransfers = false;
			deleteOldTransferHistory();
		}

		initQuery(selectTransfersSummary,
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
		dm->reportError("SQLite - loadTransferHistorySummary: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::loadTransferHistory(eTypeTransfer type, int day, vector<FinishedItemPtr> &out)
{
	try
	{
		initQuery(selectTransfersDay,
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
		DatabaseManager::getInstance()->reportError("SQLite - loadTransferHistory: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::deleteTransferHistory(const vector<int64_t>& id)
{
	if (id.empty()) return;
	try
	{
		sqlite3_transaction trans(connection, id.size() > 1);
		initQuery(deleteTransfer, "delete from transfer_db.fly_transfer_file where id=?");
		for (auto i = id.cbegin(); i != id.cend(); ++i)
		{
			dcassert(*i);
			deleteTransfer.bind(1, (long long) *i);
			deleteTransfer.executenonquery();
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - deleteTransferHistory: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::addTransfer(eTypeTransfer type, const FinishedItemPtr& item)
{
	int64_t timestamp = posixTimeToLocal(item->getTime());
	try
	{
#if 0
		string name = Text::toLower(Util::getFileName(item->getTarget()));
		string path = Text::toLower(Util::getFilePath(item->getTarget()));
		inc_hitL(path, name);
#endif

		initQuery(insertTransfer,
			"insert into transfer_db.fly_transfer_file (type,day,stamp,path,nick,hub,size,speed,ip,tth,actual) "
			"values(?,?,?,?,?,?,?,?,?,?,?)");
		insertTransfer.bind(1, type);
		insertTransfer.bind(2, (long long) (timestamp/(60*60*24)));
		insertTransfer.bind(3, (long long) timestamp);
		insertTransfer.bind(4, item->getTarget(), SQLITE_STATIC);
		insertTransfer.bind(5, item->getNick(), SQLITE_STATIC);
		insertTransfer.bind(6, item->getHub(), SQLITE_STATIC);
		insertTransfer.bind(7, (long long) item->getSize());
		insertTransfer.bind(8, (long long) item->getAvgSpeed());
		insertTransfer.bind(9, item->getIP(), SQLITE_STATIC);
		if (!item->getTTH().isZero())
			insertTransfer.bind(10, item->getTTH().toBase32(), SQLITE_TRANSIENT);
		else
			insertTransfer.bind(10);
		insertTransfer.bind(11, (long long) item->getActual());
		insertTransfer.executenonquery();
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - addTransfer: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::loadIgnoredUsers(vector<DBIgnoreListItem>& items)
{
	try
	{
		initQuery(selectIgnoredUsers, "select trim(nick), type from fly_ignore");
		sqlite3_reader reader = selectIgnoredUsers.executereader();
		while (reader.read())
		{
			string user = reader.getstring(0);
			if (!user.empty())
				items.emplace_back(DBIgnoreListItem{user, reader.getint(1)});
		}
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - loadIgnoredUsers: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::saveIgnoredUsers(const vector<DBIgnoreListItem>& items)
{
	try
	{
		sqlite3_transaction trans(connection);
		initQuery(deleteIgnoredUsers, "delete from fly_ignore");
		deleteIgnoredUsers.executenonquery();
		initQuery(insertIgnoredUsers, "insert or replace into fly_ignore (nick, type) values(?, ?)");
		for (const auto& item : items)
		{
			string user = item.data;
			boost::algorithm::trim(user);
			if (!user.empty())
			{
				insertIgnoredUsers.bind(1, user, SQLITE_TRANSIENT);
				insertIgnoredUsers.bind(2, item.type);
				insertIgnoredUsers.executenonquery();
			}
		}
		trans.commit();
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - saveIgnoredUsers: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::saveLocation(const vector<LocationInfo>& data)
{
	try
	{
		BusyCounter<int> busy(g_DisableSQLtrace);
		sqlite3_transaction trans(connection);
		initQuery(deleteLocation, "delete from location_db.fly_location_ip");
		deleteLocation.executenonquery();
		initQuery(insertLocation, "insert into location_db.fly_location_ip (start_ip,stop_ip,location,flag_index) values(?,?,?,?)");
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
		DatabaseManager::getInstance()->reportError("SQLite - saveLocation: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::loadLocation(Ip4Address ip, IPInfo& result)
{
	result.clearLocation();
	dcassert(ip);

	try
	{
		initQuery(selectLocation,
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
		DatabaseManager::getInstance()->reportError("SQLite - loadLocation: " + e.getError(), e.getErrorCode());
	}
	result.known |= IPInfo::FLAG_LOCATION;
}

void DatabaseConnection::loadP2PGuard(Ip4Address ip, IPInfo& result)
{
	result.p2pGuard.clear();
	dcassert(ip);

	try
	{
		initQuery(selectP2PGuard,
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
		DatabaseManager::getInstance()->reportError("SQLite - loadP2PGuard: " + e.getError(), e.getErrorCode());
	}
	result.known |= IPInfo::FLAG_P2P_GUARD;
}

void DatabaseConnection::removeManuallyBlockedIP(Ip4Address ip)
{
	DatabaseManager* dm = DatabaseManager::getInstance();
	dm->clearCachedP2PGuardData(ip);
	try
	{
		if (deleteManuallyBlockedIP.empty())
		{
			string stmt = "delete from location_db.fly_p2pguard_ip where start_ip=? and type=" + Util::toString(DatabaseManager::PG_DATA_MANUAL);
			deleteManuallyBlockedIP.open(&connection, stmt.c_str());
		}
		deleteManuallyBlockedIP.bind(1, ip);
		deleteManuallyBlockedIP.executenonquery();
	}
	catch (const database_error& e)
	{
		dm->reportError("SQLite - removeManuallyBlockedIP: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::loadManuallyBlockedIPs(vector<P2PGuardBlockedIP>& result)
{
	result.clear();
	try
	{
		if (selectManuallyBlockedIP.empty())
		{
			string stmt = "select distinct start_ip,note from location_db.fly_p2pguard_ip where type=" + Util::toString(DatabaseManager::PG_DATA_MANUAL);
			selectManuallyBlockedIP.open(&connection, stmt.c_str());
		}
		sqlite3_reader reader = selectManuallyBlockedIP.executereader();
		while (reader.read())
			result.emplace_back(reader.getint(0), reader.getstring(1));
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - loadManuallyBlockedIPs: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::saveP2PGuardData(const vector<P2PGuardData>& data, int type, bool removeOld)
{
	try
	{
		BusyCounter<int> busy(g_DisableSQLtrace);
		sqlite3_transaction trans(connection);
		if (removeOld)
		{
			initQuery(deleteP2PGuard, "delete from location_db.fly_p2pguard_ip where (type=? or type is null)");
			deleteP2PGuard.bind(1, type);
			deleteP2PGuard.executenonquery();
		}
		initQuery(insertP2PGuard, "insert into location_db.fly_p2pguard_ip (start_ip,stop_ip,note,type) values(?,?,?,?)");
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
		DatabaseManager::getInstance()->reportError("SQLite - saveP2PGuardData: " + e.getError(), e.getErrorCode());
	}
}

#ifdef BL_FEATURE_IP_DATABASE
void DatabaseConnection::saveIPStat(const CID& cid, const vector<IPStatVecItem>& items)
{
	const int batchSize = 256;
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
		DatabaseManager::getInstance()->reportError("SQLite - saveIPStat: " + e.getError(), e.getErrorCode());
	}
}

void DatabaseConnection::saveIPStatL(const CID& cid, const string& ip, const IPStatItem& item, int batchSize, int& count, sqlite3_transaction& trans)
{
	if (!count) trans.begin();
	if (item.flags & IPStatItem::FLAG_LOADED)
	{
		initQuery(updateIPStat, "update ip_stat set upload=?, download=? where cid=? and ip=?");
		updateIPStat.bind(1, static_cast<long long>(item.upload));
		updateIPStat.bind(2, static_cast<long long>(item.download));
		updateIPStat.bind(3, cid.data(), CID::SIZE, SQLITE_STATIC);
		updateIPStat.bind(4, ip, SQLITE_STATIC);
		updateIPStat.executenonquery();
	}
	else
	{
		initQuery(insertIPStat, "insert into ip_stat values(?,?,?,?)"); // cid, ip, upload, download
		insertIPStat.bind(1, cid.data(), CID::SIZE, SQLITE_STATIC);
		insertIPStat.bind(2, ip, SQLITE_STATIC);
		insertIPStat.bind(3, static_cast<long long>(item.upload));
		insertIPStat.bind(4, static_cast<long long>(item.download));
		insertIPStat.executenonquery();
	}
	if (++count == batchSize)
	{
		trans.commit();
		count = 0;
	}
}

IPStatMap* DatabaseConnection::loadIPStat(const CID& cid)
{
	IPStatMap* ipStat = nullptr;
	try
	{
		initQuery(selectIPStat, "select ip, upload, download from ip_stat where cid=?");
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
		DatabaseManager::getInstance()->reportError("SQLite - loadIPStat: " + e.getError(), e.getErrorCode());
	}
	return ipStat;
}

void DatabaseConnection::saveUserStat(const CID& cid, UserStatItem& stat, int batchSize, int& count, sqlite3_transaction& trans)
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
		initQuery(updateUserStat, "update user_db.user_stat set nick=?, last_ip=?, message_count=? where cid=?");
		updateUserStat.bind(1, nickList, SQLITE_STATIC);
		updateUserStat.bind(2, stat.lastIp, SQLITE_STATIC);
		updateUserStat.bind(3, stat.messageCount);
		updateUserStat.bind(4, cid.data(), CID::SIZE, SQLITE_STATIC);
		updateUserStat.executenonquery();
	}
	else
	{
		initQuery(insertUserStat, "insert into user_db.user_stat values(?,?,?,?,0,0)"); // cid, nick, last_ip, message_count
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

void DatabaseConnection::saveUserStat(const CID& cid, UserStatItem& stat)
{
	try
	{
		sqlite3_transaction trans(connection, false);
		int count = 0;
		saveUserStat(cid, stat, 1, count, trans);
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - saveUserStat: " + e.getError(), e.getErrorCode());
	}
}

bool DatabaseConnection::loadUserStat(const CID& cid, UserStatItem& stat)
{
	bool result = false;
	try
	{
		initQuery(selectUserStat, "select nick, last_ip, message_count from user_db.user_stat where cid=?");
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
		DatabaseManager::getInstance()->reportError("SQLite - loadUserStat: " + e.getError(), e.getErrorCode());
	}
	return result;
}

bool DatabaseConnection::convertStatTables(bool hasRatioTable, bool hasUserTable)
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
				saveUserStat(item.first, statItem.stat, batchSize, insertedCount, trans);
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

bool DatabaseConnection::loadGlobalRatio(uint64_t values[])
{
	bool result = false;
	try
	{
		initQuery(selectGlobalRatio, "select total(upload), total(download) from ip_stat");
		sqlite3_reader reader = selectGlobalRatio.executereader();
		if (reader.read())
		{
			values[0] = reader.getint64(0);
			values[1] = reader.getint64(1);
			result = true;
		}
	}
	catch (const database_error& e)
	{
		DatabaseManager::getInstance()->reportError("SQLite - loadGlobalRatio: " + e.getError(), e.getErrorCode());
	}
	return result;
}
#endif

DatabaseManager::DatabaseManager() noexcept
{
#ifdef BL_FEATURE_IP_DATABASE
	globalRatio.download = globalRatio.upload = 0;
#endif
	journalMode = JOUNRAL_MODE_PERSIST;
	defThreadId = 0;
	timeLoadGlobalRatio = 0;
	deleteOldTransfers = true;
	errorCallback = nullptr;
	dbSize = 0;
	mmdb = nullptr;
	timeCheckMmdb = 0;
	mmdbDownloadReq = 0;
	timeDownloadMmdb = 0;
	mmdbFileTimestamp = 1;
	mmdbStatus = MMDB_STATUS_MISSING;
}

DatabaseManager::~DatabaseManager()
{
	closeGeoIPDatabaseL();
	httpClient.removeListener(this);
}

string DatabaseManager::getDBInfo()
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

		size_t hashDbItems;
		uint64_t hashDbSize;
		if (lmdb.getDBInfo(hashDbItems, hashDbSize))
		{
			string size = Util::formatBytes(hashDbSize);
			message += "  * ";
			message += HashDatabaseLMDB::getDBPath();
			message += " (";
			message += STRING_F(HASH_DB_INFO, hashDbItems % size);
			message += ")\n";
		}

		File::VolumeInfo vi;
		if (File::getVolumeInfo(path, vi))
		{
			message += '\n';
#ifdef _WIN32
			message += STRING_F(DATABASE_DISK_INFO, path.substr(0, 2) % Util::formatBytes(vi.freeBytes));
#else
			message += STRING_F(DATABASE_DISK_INFO, path % Util::formatBytes(vi.freeBytes));
#endif
			message += '\n';
		}
		else
		{
			dcassert(0);
		}
	}
	return dbSize ? message : string();
}

void DatabaseManager::reportError(const string& text, int errorCode)
{
	if (errorCode == SQLITE_OK || errorCode == SQLITE_INTERRUPT)
		return;

	LogManager::message(text);
	if (!errorCallback)
		return;

	string message;
	string dbInfo = getDBInfo();
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
		string root = Util::getConfigPath();
#ifdef _WIN32
		root.erase(2);
#endif
		message = STRING_F(DATABASE_DISK_FULL, root);
		message += "\n\n";
		forceExit = true;
	}
	message += STRING_F(DATABASE_ERROR_STRING, text);
	message += "\n\n";
	message += dbInfo;
	errorCallback(message, forceExit);
}

DatabaseConnection* DatabaseManager::getDefaultConnection()
{
	ASSERT_MAIN_THREAD();
	LOCK(cs);
	return defConn.get();
}

DatabaseConnection* DatabaseManager::getConnection()
{
	if (BaseThread::getCurrentThreadId() == defThreadId)
		return getDefaultConnection();
	{
		LOCK(cs);
		for (auto& c : conn)
			if (!c->busy)
			{
				c->busy = true;
				return c.get();
			}
	}
	DatabaseConnection* newConn = new DatabaseConnection;
	std::unique_ptr<DatabaseConnection> c(newConn);
	try
	{
		c->open(prefix, journalMode);
	}
	catch (const database_error& e)
	{
		reportError("SQLite - getConnection: " + e.getError(), e.getErrorCode());
		return nullptr;
	}
	LOCK(cs);
	conn.emplace_back(std::move(c));
	return newConn;
}

void DatabaseManager::putConnection(DatabaseConnection* conn)
{
	uint64_t removeTime = GET_TICK() + 120 * 1000;
	LOCK(cs);
	dcassert(conn->busy || conn == defConn.get());
	conn->busy = false;
	conn->removeTime = removeTime;
}

void DatabaseManager::closeIdleConnections(uint64_t tick)
{
	vector<unique_ptr<DatabaseConnection>> v;
	{
		LOCK(cs);
		for (auto i = conn.begin(); i != conn.end();)
		{
			unique_ptr<DatabaseConnection>& c = *i;
			if (!c->busy && tick > c->removeTime)
			{
				v.emplace_back(std::move(c));
				i = conn.erase(i);
			}
			else
				++i;
		}
	}
	v.clear();
	lmdb.closeIdleConnections(tick);
}

void DatabaseManager::init(ErrorCallback errorCallback, int journalMode)
{
	{
		LOCK(cs);
		this->errorCallback = errorCallback;
		if (journalMode >= JOUNRAL_MODE_PERSIST && journalMode <= JOUNRAL_MODE_MEMORY)
			this->journalMode = journalMode;
		try
		{
			const string& dbPath = Util::getConfigPath();
			if (!checkDbPrefix(dbPath, "bl") && !checkDbPrefix(dbPath, "FlylinkDC"))
				prefix = "bl";

			int status = sqlite3_initialize();
			if (status != SQLITE_OK)
				LogManager::message("[Error] sqlite3_initialize = " + Util::toString(status));
			dcassert(status == SQLITE_OK);
			dcassert(sqlite3_threadsafe() >= 1);

			status = sqlite3_enable_shared_cache(1);
			dcassert(status == SQLITE_OK);

			std::unique_ptr<DatabaseConnection> c(new DatabaseConnection);
			c->open(prefix, this->journalMode);
			c->upgradeDatabase();
			defConn = std::move(c);
		}
		catch (const database_error& e)
		{
			reportError("SQLite - DatabaseManager: " + e.getError(), e.getErrorCode());
		}
	}
	defThreadId = BaseThread::getCurrentThreadId();
	lmdb.open();
	string dbInfo = getDBInfo();
	if (!dbInfo.empty())
		LogManager::message(dbInfo, false);
}

void DatabaseManager::getIPInfo(DatabaseConnection* conn, const IpAddress& ip, IPInfo& result, int what, bool onlyCached)
{
	dcassert(what);
	dcassert(Util::isValidIp(ip));
	IpKey ipKey;
	if (ip.type == AF_INET6)
		ipKey.setIP(ip.data.v6);
	else
		ipKey.setIP(ip.data.v4);

	{
		LOCK(csIpCache);
		IpCacheItem* item = ipCache.get(ipKey);
		if (item)
		{
			ipCache.makeNewest(item);
			int found = what & item->info.known;
			if (found & IPInfo::FLAG_COUNTRY)
			{
				result.country = item->info.country;
				result.countryCode = item->info.countryCode;
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
	{
		result.clearCountry();
		if (Util::isPublicIp(ip))
			loadGeoIPInfo(ip, result);
		else
			result.known |= IPInfo::FLAG_COUNTRY;
	}
	if (what & IPInfo::FLAG_LOCATION)
	{
		if (BOOLSETTING(USE_CUSTOM_LOCATIONS) && ip.type == AF_INET)
		{
			if (conn) conn->loadLocation(ip.data.v4, result);
		}
		else
			result.known |= IPInfo::FLAG_LOCATION;
	}
	if (what & IPInfo::FLAG_P2P_GUARD)
	{
		if (ip.type == AF_INET)
		{
			if (conn) conn->loadP2PGuard(ip.data.v4, result);
		}
		else
			result.known |= IPInfo::FLAG_P2P_GUARD;
	}
	LOCK(csIpCache);
	IpCacheItem* storedItem;
	IpCacheItem newItem;
	newItem.info = result;
	newItem.key = ipKey;
	if (!ipCache.add(newItem, &storedItem))
	{
		if (result.known & IPInfo::FLAG_COUNTRY)
		{
			storedItem->info.known |= IPInfo::FLAG_COUNTRY;
			storedItem->info.country = result.country;
			storedItem->info.countryCode = result.countryCode;
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

void DatabaseManager::clearCachedP2PGuardData(Ip4Address ip)
{
	IpKey ipKey;
	ipKey.setIP(ip);
	LOCK(csIpCache);
	IpCacheItem* item = ipCache.get(ipKey);
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

#ifdef BL_FEATURE_IP_DATABASE
void DatabaseManager::loadGlobalRatio(bool force)
{
	uint64_t tick = GET_TICK();
	LOCK(csGlobalRatio);
	if (!force && tick < timeLoadGlobalRatio) return;
	auto conn = getConnection();
	if (conn)
	{
		uint64_t values[2];
		if (conn->loadGlobalRatio(values))
		{
			globalRatio.upload = values[0];
			globalRatio.download = values[1];
		}
		putConnection(conn);
		timeLoadGlobalRatio = tick + 10 * 60000;
	}
}

DatabaseManager::GlobalRatio DatabaseManager::getGlobalRatio() const
{
	LOCK(csGlobalRatio);
	return globalRatio;
}
#endif

void DatabaseManager::shutdown()
{
	lmdb.close();
	{
		LOCK(cs);
		defConn.reset();
		conn.clear();
	}
	int status = sqlite3_shutdown();
	dcassert(status == SQLITE_OK);
	if (status != SQLITE_OK)
		LogManager::message("[Error] sqlite3_shutdown = " + Util::toString(status));
}

bool DatabaseManager::addTree(HashDatabaseConnection* conn, const TigerTree& tree) noexcept
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
	if (tree.getLeaves().size() < 2)
		return true;
	bool result = conn->putTigerTree(tree);
	if (!result)
		LogManager::message("Failed to add tiger tree to DB (" + tree.getRoot().toBase32() + ')', false);
	return result;
}

bool DatabaseManager::getTree(HashDatabaseConnection* conn, const TTHValue &tth, TigerTree &tree) noexcept
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
	return conn->getTigerTree(tth.data, tree);
}

bool DatabaseManager::checkDbPrefix(const string& path, const string& str)
{
	if (File::isExist(path + str + ".sqlite"))
	{
		prefix = str;
		return true;
	}
	return false;
}

bool DatabaseManager::openGeoIPDatabaseL() noexcept
{
	if (mmdb) return true;
	uint64_t now = GET_TICK();
	if (now < timeCheckMmdb) return false;
	string path = Util::getConfigPath() + fileNameGeoIP;
	mmdb = new MMDB_s;
	if (MMDB_open(path.c_str(), MMDB_MODE_MMAP, mmdb) == 0)
	{
		timeCheckMmdb = 0;
		return true;
	}
	delete mmdb;
	mmdb = nullptr;
	timeCheckMmdb = now + 600000; // 10 minutes
	return false;
}

void DatabaseManager::closeGeoIPDatabaseL() noexcept
{
	if (mmdb)
	{
		MMDB_close(mmdb);
		delete mmdb;
		mmdb = nullptr;
	}
	timeCheckMmdb = 0;
}

static string getMMDBString(MMDB_lookup_result_s& lres, const char* *path) noexcept
{
	MMDB_entry_data_s data;
	int error = MMDB_aget_value(&lres.entry, &data, path);
	if (error != MMDB_SUCCESS || !data.has_data || data.type != MMDB_DATA_TYPE_UTF8_STRING)
		return Util::emptyString;
	return string(data.utf8_string, data.data_size);
}

bool DatabaseManager::loadGeoIPInfo(const IpAddress& ip, IPInfo& result) noexcept
{
	LOCK(csMmdb);
	if (!openGeoIPDatabaseL()) return false;
	sockaddr_u sa;
	socklen_t size;
	toSockAddr(sa, size, ip, 0);
	result.known |= IPInfo::FLAG_COUNTRY;
	result.countryCode = 0;
	int error = 0;
	auto lres = MMDB_lookup_sockaddr(mmdb, (const sockaddr *) &sa, &error);
	if (error) return false;
	const char* path[4] = { "country", "iso_code", nullptr };
	string countryCode = getMMDBString(lres, path);
	if (countryCode.length() != 2) return false;
	path[1] = "names";
	path[2] = "en";
	path[3] = nullptr;
	string countryName = getMMDBString(lres, path);
	if (countryName.empty()) return false;
	Text::asciiMakeLower(countryCode);
	result.country = std::move(countryName);
	result.countryCode = TAG(countryCode[0], countryCode[1]);
	return true;
}

uint64_t DatabaseManager::getGeoIPTimestamp() const noexcept
{
	uint64_t timestamp = 0;
	try
	{
		File f(Util::getConfigPath() + fileNameGeoIP, File::READ, File::OPEN);
		timestamp = File::timeStampToUnixTime(f.getTimeStamp());
	}
	catch (FileException&) {}
	return timestamp;
}

void DatabaseManager::downloadGeoIPDatabase(uint64_t timestamp, bool force, const string &url) noexcept
{
	if (url.empty()) return;
	uint64_t fileTimestamp;
	{
		LOCK(csDownloadMmdb);
		if (mmdbStatus == MMDB_STATUS_DOWNLOADING || (!force && timestamp < timeDownloadMmdb)) return;
		mmdbStatus = MMDB_STATUS_DOWNLOADING;
		fileTimestamp = mmdbFileTimestamp;
	}
	if (force || fileTimestamp == 1)
	{
		fileTimestamp = getGeoIPTimestamp();
		LOCK(csDownloadMmdb);
		mmdbFileTimestamp = fileTimestamp;
	}
	if (!force && fileTimestamp && fileTimestamp + SETTING(GEOIP_CHECK_HOURS) * 3600 > static_cast<uint64_t>(time(nullptr)))
	{
		// File was downloaded recently
		LOCK(csDownloadMmdb);
		mmdbStatus = MMDB_STATUS_OK;
		return;
	}

	HttpClient::Request req;
	req.outputPath = Util::getHttpDownloadsPath();
	File::ensureDirectory(req.outputPath);
	req.outputPath += fileNameGeoIP + ".gz";
	req.url = url;
	req.userAgent = "Airdcpp/4.11";
	req.maxRedirects = 5;
	req.maxRespBodySize = 10 * 1024 * 1024;
	req.ifModified = fileTimestamp;
	mmdbDownloadReq = httpClient.addRequest(req);
	if (!mmdbDownloadReq)
	{
		LogManager::message(STRING_F(GEOIP_DOWNLOAD_FAIL, STRING(HTTP_INVALID_URL)));
		LOCK(csDownloadMmdb);
		mmdbStatus = MMDB_STATUS_DL_FAILED;
		return;
	}
	httpClient.addListener(this);
	httpClient.startRequest(mmdbDownloadReq);
}

int DatabaseManager::getGeoIPDatabaseStatus(uint64_t& timestamp) noexcept
{
	csDownloadMmdb.lock();
	int status = mmdbStatus;
	timestamp = mmdbFileTimestamp;
	csDownloadMmdb.unlock();
	if (status == MMDB_STATUS_DOWNLOADING || status == MMDB_STATUS_DL_FAILED) return status;

	csMmdb.lock();
	openGeoIPDatabaseL();
	bool isOpen = mmdb != nullptr;
	csMmdb.unlock();

	if (!isOpen) return MMDB_STATUS_MISSING;
	if (timestamp == 1)
	{
		timestamp = getGeoIPTimestamp();
		LOCK(csDownloadMmdb);
		mmdbFileTimestamp = timestamp;
	}
	return MMDB_STATUS_OK;
}

void DatabaseManager::on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept
{
	if (resp.getResponseCode() != 200)
	{
		processDownloadResult(id,
			Util::toString(resp.getResponseCode()) + ' ' + resp.getResponsePhrase(),
			resp.getResponseCode() != 304);
		return;
	}
	{
		LOCK(csDownloadMmdb);
		if (mmdbStatus != MMDB_STATUS_DOWNLOADING || id != mmdbDownloadReq) return;
	}
	httpClient.removeListener(this);
	bool ok = true;
	string path = Util::getConfigPath() + fileNameGeoIP;
	string tempPath = path + ".dctmp";
	try
	{
		GZip::decompress(data.outputPath, tempPath);
	}
	catch (FileException& ex)
	{
		LogManager::message(STRING_F(GEOIP_DECOMPRESS_FAIL, ex.getError()));
		ok = false;
	}
	catch (Exception&)
	{
		LogManager::message(STRING_F(GEOIP_DECOMPRESS_FAIL, STRING(INVALID_FILE_FORMAT)));
		ok = false;
	}
	if (ok)
	{
		LOCK(csMmdb);
		closeGeoIPDatabaseL();
		ok = File::renameFile(tempPath, path);
		if (!ok)
			LogManager::message(STRING_F(GEOIP_DECOMPRESS_FAIL, Util::translateError()));
	}
	if (ok)
	{
		LogManager::message(STRING(GEOIP_DOWNLOAD_OK));
		uint64_t timestamp = getGeoIPTimestamp();
		LOCK(csDownloadMmdb);
		mmdbFileTimestamp = timestamp;
	}
	File::deleteFile(data.outputPath);
	clearDownloadRequest(false, ok ? MMDB_STATUS_OK : MMDB_STATUS_DL_FAILED);
}

void DatabaseManager::on(Failed, uint64_t id, const string& error) noexcept
{
	processDownloadResult(id, error, true);
}

void DatabaseManager::processDownloadResult(uint64_t reqId, const string& text, bool isError) noexcept
{
	{
		LOCK(csDownloadMmdb);
		if (mmdbStatus != MMDB_STATUS_DOWNLOADING || reqId != mmdbDownloadReq) return;
	}
	httpClient.removeListener(this);
	clearDownloadRequest(isError, isError ? MMDB_STATUS_DL_FAILED : MMDB_STATUS_OK);
	if (isError) LogManager::message(STRING_F(GEOIP_DOWNLOAD_FAIL, text));
}

void DatabaseManager::clearDownloadRequest(bool isRetry, int newStatus) noexcept
{
	LOCK(csDownloadMmdb);
	mmdbStatus = newStatus;
	mmdbDownloadReq = 0;
	timeDownloadMmdb = GET_TICK() + (isRetry ? GEOIP_DOWNLOAD_RETRY_TIME : SETTING(GEOIP_CHECK_HOURS) * 3600);
}
