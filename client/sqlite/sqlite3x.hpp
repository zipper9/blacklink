/*
	This is a modified version of sqlite3x.hpp, not the original code!	

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

#ifndef __SQLITE3X_HPP__
#define __SQLITE3X_HPP__

#include <string>
#include <vector>
#include "sqlite3.h"
#include "../client/Exception.h"

#ifndef SQLITE_USE_UNICODE
#define SQLITE_USE_UNICODE
#endif

namespace sqlite3x
{
	class sqlite3_connection
	{
	private:
		friend class sqlite3_command;
		friend class database_error;

		sqlite3 *db;
		void checkdb();

	public:
		sqlite3_connection();
		explicit sqlite3_connection(const char *db);
#ifdef SQLITE_USE_UNICODE
		explicit sqlite3_connection(const wchar_t *db);
#endif
		sqlite3_connection(const sqlite3_connection &) = delete;
		sqlite3_connection& operator= (const sqlite3_connection&) = delete;
		~sqlite3_connection();
		sqlite3 *getdb() { return db; }

		void open(const char *dbpath);
#ifdef SQLITE_USE_UNICODE
		void open(const wchar_t *dbpath);
#endif
		void close();
		int changes() { return sqlite3_changes(db); }
		int getautocommit() { return sqlite3_get_autocommit(db); }
		long long insertid();
		void setbusytimeout(int ms);

		const char* executenonquery(const char *sql);
		void executenonquery(const std::string &sql);
#ifdef SQLITE_USE_UNICODE
		void executenonquery(const wchar_t *sql);
		void executenonquery(const std::wstring &sql);
		int executeint(const wchar_t *sql);
		int executeint(const std::wstring &sql);
		long long executeint64(const wchar_t *sql);
		long long executeint64(const std::wstring &sql);
		double executedouble(const wchar_t *sql);
		double executedouble(const std::wstring &sql);
		std::string executestring(const wchar_t *sql);
		std::string executestring(const std::wstring &sql);
		std::wstring executestring16(const char *sql);
		std::wstring executestring16(const std::string &sql);
		std::wstring executestring16(const wchar_t *sql);
		std::wstring executestring16(const std::wstring &sql);
#endif
		int executeint(const char *sql);
		int executeint(const std::string &sql);
		long long executeint64(const char *sql);
		long long executeint64(const std::string &sql);

#ifndef SQLITE_OMIT_FLOATING_POINT
		double executedouble(const char *sql);
		double executedouble(const std::string &sql);
#endif
		std::string executestring(const char *sql);
		std::string executestring(const std::string &sql);
	};

	class sqlite3_transaction
	{
	private:
		sqlite3_connection &conn;
		bool intrans;

	public:
		sqlite3_transaction(sqlite3_connection &conn, bool start = true);
		~sqlite3_transaction();

		void begin();
		void commit();
		void rollback();
	};

	class sqlite3_reader;
	
	class sqlite3_command
	{
	private:
		friend class sqlite3_reader;

		sqlite3_connection *conn;
		sqlite3_stmt *stmt;
		unsigned int refs;

		void checknotopen();
		void checkopen();
		void checknodata(bool nodata);

	public:
		sqlite3_command() noexcept;
		sqlite3_command(sqlite3_connection *conn, const char *sql);
		sqlite3_command(sqlite3_connection *conn, const std::string &sql);
#ifdef SQLITE_USE_UNICODE
		sqlite3_command(sqlite3_connection *conn, const wchar_t *sql);
		sqlite3_command(sqlite3_connection *conn, const std::wstring &sql);
#endif
		~sqlite3_command();
		sqlite3_connection *getconnection() const { return conn; };
		
		void open(sqlite3_connection *conn, const char *sql);
		void open(sqlite3_connection *conn, const std::string &sql);
#ifdef SQLITE_USE_UNICODE
		void open(sqlite3_connection *conn, const wchar_t *sql);
		void open(sqlite3_connection *conn, const std::wstring &sql);
#endif
		
		void bind(int index);
		void bind(int index, int data);
		void bind(int index, long long data);
		void bind(int index, unsigned data) { bind(index, static_cast<int>(data)); }
		
#ifndef SQLITE_OMIT_FLOATING_POINT
		void bind(int index, double data);
#endif
		void bind(int index, const char *data, int datalen, sqlite3_destructor_type dtype /*= SQLITE_STATIC or SQLITE_TRANSIENT*/);
		void bind(int index, const void *data, int datalen, sqlite3_destructor_type dtype /*= SQLITE_STATIC or SQLITE_TRANSIENT*/);
		void bind(int index, const std::string &data, sqlite3_destructor_type dtype /*= SQLITE_STATIC or SQLITE_TRANSIENT*/);
#ifdef SQLITE_USE_UNICODE
		void bind(int index, const wchar_t *data, int datalen);
		void bind(int index, const std::wstring &data);
#endif
		sqlite3_reader executereader();
		void executenonquery();
		int executeint();
		long long executeint64();
#ifndef SQLITE_OMIT_FLOATING_POINT
		double executedouble();
#endif
		std::string executestring();
#ifdef SQLITE_USE_UNICODE
		std::wstring executestring16();
#endif
		bool empty() const { return stmt == nullptr; }
	};

	class sqlite3_reader
	{
	private:
		friend class sqlite3_command;

		sqlite3_command *cmd;

		explicit sqlite3_reader(sqlite3_command *cmd);
		void checkreader();

	public:
		sqlite3_reader();
		sqlite3_reader(const sqlite3_reader &copy) = delete;
		sqlite3_reader(sqlite3_reader &&copy);
		~sqlite3_reader();

		sqlite3_reader& operator=(const sqlite3_reader &copy) = delete;
		sqlite3_reader& operator=(sqlite3_reader &&copy);

		bool read();
		void reset();
		void close();

		int getint(int index);
		long long getint64(int index);
#ifndef SQLITE_OMIT_FLOATING_POINT
		double getdouble(int index);
#endif
		std::string getstring(int index);
#ifdef SQLITE_USE_UNICODE
		std::wstring getstring16(int index);
		std::wstring getcolname16(int index);
#endif
		void getblob(int index, std::vector<unsigned char> &result);
		bool getblob(int index, void *result, int size);
		std::string getcolname(int index);
	};
	
	class database_error : public Exception
	{
		int errorCode;
	
	public:
		database_error(const std::string &msg)
			: Exception(msg), errorCode(SQLITE_ERROR) {}
		explicit database_error(const sqlite3_connection *conn)
			: Exception(sqlite3_errmsg(conn->db)), errorCode(sqlite3_errcode(conn->db)) {}
		int getErrorCode() const { return errorCode; }
	};
}

#endif
