/*
	This is a modified version of sqlite3x_command.cpp, not the original code!

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
	sqlite3_command::sqlite3_command() noexcept : conn(nullptr), stmt(nullptr), refs(0)
	{
	}
	
	sqlite3_command::sqlite3_command(sqlite3_connection *conn, const char *sql) : conn(nullptr), stmt(nullptr), refs(0)
	{
		open(conn, sql);
	}

	sqlite3_command::sqlite3_command(sqlite3_connection *conn, const std::string &sql) : conn(nullptr), stmt(nullptr), refs(0)
	{
		open(conn, sql);
	}

#ifndef SQLITE_OMIT_UTF16
	sqlite3_command::sqlite3_command(sqlite3_connection *conn, const wchar_t *sql) : conn(nullptr), stmt(nullptr), refs(0)
	{
		open(conn, sql);
	}

	sqlite3_command::sqlite3_command(sqlite3_connection *conn, const std::wstring &sql) : conn(nullptr), stmt(nullptr), refs(0)
	{
		open(conn, sql);
	}
#endif

	sqlite3_command::~sqlite3_command()
	{
		if (stmt) sqlite3_finalize(stmt);
	}

	void sqlite3_command::open(sqlite3_connection *conn, const char *sql)
	{
		checknotopen();
		this->conn = conn;
		const char *tail = nullptr;
		if (sqlite3_prepare_v2(conn->db, sql, -1, &stmt, &tail) != SQLITE_OK)
			throw database_error(conn);
	}

	void sqlite3_command::open(sqlite3_connection *conn, const std::string &sql)
	{
		checknotopen();
		this->conn = conn;
		const char *tail = nullptr;
		if (sqlite3_prepare_v2(conn->db, sql.c_str(), (int) sql.length(), &stmt, &tail) != SQLITE_OK)
			throw database_error(conn);
	}

#ifndef SQLITE_OMIT_UTF16
	void sqlite3_command::open(sqlite3_connection *conn, const wchar_t *sql)
	{
		checknotopen();
		this->conn = conn;
		const wchar_t *tail = nullptr;
		if (sqlite3_prepare16_v2(conn->db, sql, -1, &stmt, (const void **) &tail) != SQLITE_OK)
			throw database_error(conn);
	}

	void sqlite3_command::open(sqlite3_connection *conn, const std::wstring &sql)
	{
		checknotopen();
		this->conn = conn;
		const wchar_t *tail = nullptr;
		if (sqlite3_prepare16_v2(conn->db, sql.c_str(), (int) sql.length()*2, &stmt, (const void **) &tail) != SQLITE_OK)
			throw database_error(conn);
	}
#endif

	void sqlite3_command::bind(int index)
	{
		checkopen();
		if (sqlite3_bind_null(stmt, index) != SQLITE_OK)
			throw database_error(conn);
	}

	void sqlite3_command::bind(int index, int data)
	{
		checkopen();
		if (sqlite3_bind_int(stmt, index, data) != SQLITE_OK)
			throw database_error(conn);
	}

	void sqlite3_command::bind(int index, long long data)
	{
		checkopen();
		if (sqlite3_bind_int64(stmt, index, data) != SQLITE_OK)
			throw database_error(conn);
	}

#ifndef SQLITE_OMIT_FLOATING_POINT
	void sqlite3_command::bind(int index, double data)
	{
		checkopen();
		if (sqlite3_bind_double(stmt, index, data) != SQLITE_OK)
			throw database_error(conn);
	}
#endif

	void sqlite3_command::bind(int index, const char *data, int datalen, sqlite3_destructor_type dtype)
	{
		checkopen();
		if (sqlite3_bind_text(stmt, index, data, datalen, dtype) != SQLITE_OK)
			throw database_error(conn);
	}

	void sqlite3_command::bind(int index, const void *data, int datalen, sqlite3_destructor_type dtype)
	{
		checkopen();
		if (sqlite3_bind_blob(stmt, index, data, datalen, dtype) != SQLITE_OK)
			throw database_error(conn);
	}

	void sqlite3_command::bind(int index, const std::string &data, sqlite3_destructor_type dtype)
	{
		checkopen();
		if (sqlite3_bind_text(stmt, index, data.c_str(), (int) data.length(), dtype) != SQLITE_OK)
			throw database_error(conn);
	}

#ifndef SQLITE_OMIT_UTF16
	void sqlite3_command::bind(int index, const wchar_t *data, int datalen)
	{
		checkopen();
		if (sqlite3_bind_text16(stmt, index, data, datalen, SQLITE_STATIC) != SQLITE_OK)
			throw database_error(conn);
	}

	void sqlite3_command::bind(int index, const std::wstring &data)
	{
		checkopen();
		if (sqlite3_bind_text16(stmt, index, data.c_str(), (int) data.length()*2, SQLITE_STATIC) != SQLITE_OK)
			throw database_error(conn);
	}
#endif

	sqlite3_reader sqlite3_command::executereader()
	{
		checkopen();
		return sqlite3_reader(this);
	}

	void sqlite3_command::executenonquery()
	{
		executereader().read();
	}

	int sqlite3_command::executeint()
	{
		checkopen();
		sqlite3_reader reader = executereader();
		checknodata(!reader.read());
		return reader.getint(0);
	}

	long long sqlite3_command::executeint64()
	{
		checkopen();
		sqlite3_reader reader = executereader();
		checknodata(!reader.read());
		return reader.getint64(0);
	}

	void sqlite3_command::checknotopen()
	{
		if (stmt)
			throw database_error("command is already open");
	}

	void sqlite3_command::checkopen()
	{
		if (!stmt)
			throw database_error("command is empty");
	}

	void sqlite3_command::checknodata(bool nodata)
	{
		if (nodata)
			throw database_error("nothing to read");
	}

#ifndef SQLITE_OMIT_FLOATING_POINT
	double sqlite3_command::executedouble()
	{
		checkopen();
		sqlite3_reader reader = executereader();
		checknodata(!reader.read());
		return reader.getdouble(0);
	}
#endif

	std::string sqlite3_command::executestring()
	{
		checkopen();
		sqlite3_reader reader = executereader();
		checknodata(!reader.read());
		return reader.getstring(0);
	}

#ifndef SQLITE_OMIT_UTF16
	std::wstring sqlite3_command::executestring16()
	{
		checkopen();
		sqlite3_reader reader = executereader();
		checknodata(!reader.read());
		return reader.getstring16(0);
	}
#endif
} // namespace sqlite3x
