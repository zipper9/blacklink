/*
	This is a modified version of sqlite3x_connection.cpp, not the original code!

	Copyright (C) 2004-2005 Cory Nelson

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgment in the product documentation would be
		appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
*/

#include "stdinc.h"
#include "sqlite3x.hpp"

namespace sqlite3x
{
	sqlite3_connection::sqlite3_connection() : db(nullptr) {}

	sqlite3_connection::sqlite3_connection(const char *dbpath) : db(nullptr)
	{
		open(dbpath);
	}

	sqlite3_connection::~sqlite3_connection()
	{
		if (db) sqlite3_close(db);
	}

	void sqlite3_connection::checkdb() 
	{
		if (!db) throw database_error("database is not open");
	}

	void sqlite3_connection::open(const char *dbpath)
	{
		if (sqlite3_open(dbpath, &db) != SQLITE_OK)
			throw database_error(std::string("unable to open database ") + dbpath);
	}

	void sqlite3_connection::close()
	{
		if (!db) return;
		if (sqlite3_close(db) != SQLITE_OK)
			throw database_error(this);
		db = nullptr;
	}

	long long sqlite3_connection::insertid()
	{
		checkdb();
		return sqlite3_last_insert_rowid(db);
	}

	void sqlite3_connection::setbusytimeout(int ms)
	{
		checkdb();
		if (sqlite3_busy_timeout(db, ms) != SQLITE_OK)
			throw database_error(this);
	}

	const char *sqlite3_connection::executenonquery(const char *sql)
	{
		checkdb();
		sqlite3_command(this, sql).executenonquery();
		return sql;
	}

	void sqlite3_connection::executenonquery(const std::string &sql)
	{
		checkdb();
		sqlite3_command(this, sql).executenonquery();
	}

	int sqlite3_connection::executeint(const char *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint();
	}

	int sqlite3_connection::executeint(const std::string &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint();
	}

	long long sqlite3_connection::executeint64(const char *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint64();
	}

	long long sqlite3_connection::executeint64(const std::string &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint64();
	}

	std::string sqlite3_connection::executestring(const char *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring();
	}

	std::string sqlite3_connection::executestring(const std::string &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring();
	}

#ifndef SQLITE_OMIT_FLOATING_POINT
	double sqlite3_connection::executedouble(const char *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executedouble();
	}

	double sqlite3_connection::executedouble(const std::string &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executedouble();
	}
#endif

#ifndef SQLITE_OMIT_UTF16
	sqlite3_connection::sqlite3_connection(const wchar_t *dbpath) : db(nullptr)
	{
		open(dbpath);
	}

	void sqlite3_connection::open(const wchar_t *dbpath)
	{
		if (sqlite3_open16(dbpath, &db) != SQLITE_OK)
			throw database_error("unable to open database");
			// TODO: convert dbpath to utf8 and append it to exception text
	}

	void sqlite3_connection::executenonquery(const wchar_t *sql)
	{
		checkdb();
		sqlite3_command(this, sql).executenonquery();
	}

	int sqlite3_connection::executeint(const std::wstring &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint();
	}

	void sqlite3_connection::executenonquery(const std::wstring &sql)
	{
		checkdb();
		sqlite3_command(this, sql).executenonquery();
	}

	int sqlite3_connection::executeint(const wchar_t *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint();
	}

	long long sqlite3_connection::executeint64(const wchar_t *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint64();
	}

	long long sqlite3_connection::executeint64(const std::wstring &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executeint64();
	}

#ifndef SQLITE_OMIT_FLOATING_POINT
	double sqlite3_connection::executedouble(const wchar_t *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executedouble();
	}

	double sqlite3_connection::executedouble(const std::wstring &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executedouble();
	}
#endif

	std::wstring sqlite3_connection::executestring16(const wchar_t *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring16();
	}

	std::wstring sqlite3_connection::executestring16(const std::wstring &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring16();
	}

	std::string sqlite3_connection::executestring(const wchar_t *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring();
	}

	std::wstring sqlite3_connection::executestring16(const char *sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring16();
	}

	std::wstring sqlite3_connection::executestring16(const std::string &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring16();
	}

	std::string sqlite3_connection::executestring(const std::wstring &sql)
	{
		checkdb();
		return sqlite3_command(this, sql).executestring();
	}
#endif
}
