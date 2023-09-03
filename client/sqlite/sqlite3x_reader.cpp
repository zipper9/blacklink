/*
	This is a modified version of sqlite3x_reader.cpp, not the original code!

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
#include <string.h>

namespace sqlite3x
{
	sqlite3_reader::sqlite3_reader() : cmd(nullptr) {}

	sqlite3_reader::sqlite3_reader(sqlite3_command *cmd) : cmd(cmd)
	{
		++cmd->refs;
	}

	sqlite3_reader::~sqlite3_reader()
	{
		close();
	}

	sqlite3_reader& sqlite3_reader::operator=(sqlite3_reader &&copy)
	{
		close();
		cmd = copy.cmd;
		copy.cmd = nullptr;
		return *this;
	}

	sqlite3_reader::sqlite3_reader(sqlite3_reader&& copy)
	{
		cmd = copy.cmd;
		copy.cmd = nullptr;
	}

	void sqlite3_reader::checkreader()
	{
		if (!cmd) throw database_error("reader is closed");
	}

	bool sqlite3_reader::read()
	{
		checkreader();
		switch (sqlite3_step(cmd->stmt))
		{
		case SQLITE_ROW:
			return true;
		case SQLITE_DONE:
			return false;
		default:
			throw database_error(cmd->conn);
		}
	}

	void sqlite3_reader::reset()
	{
		checkreader();
		if (sqlite3_reset(cmd->stmt) != SQLITE_OK)
			throw database_error(cmd->conn);
	}

	void sqlite3_reader::close()
	{
		if (cmd)
		{
			if (--cmd->refs == 0) sqlite3_reset(cmd->stmt);
			cmd = nullptr;
		}
	}

	int sqlite3_reader::getint(int index)
	{
		checkreader();
		return sqlite3_column_int(cmd->stmt, index);
	}

	long long sqlite3_reader::getint64(int index)
	{
		checkreader();
		return sqlite3_column_int64(cmd->stmt, index);
	}

#ifndef SQLITE_OMIT_FLOATING_POINT
	double sqlite3_reader::getdouble(int index)
	{
		checkreader();
		return sqlite3_column_double(cmd->stmt, index);
	}
#endif

	std::string sqlite3_reader::getstring(int index)
	{
		checkreader();
		int size = sqlite3_column_bytes(cmd->stmt, index);
		if (!size) return std::string();
		return std::string((const char *) sqlite3_column_text(cmd->stmt, index), size);
	}

	bool sqlite3_reader::getblob(int index, void *result, int size)
	{
		checkreader();
		const int datasize = sqlite3_column_bytes(cmd->stmt, index);
		if (datasize != size) return false;
		const void *data = sqlite3_column_blob(cmd->stmt, index);
		if (!data) return false;
		memcpy(result, data, datasize);
	    return true;
	}

	void sqlite3_reader::getblob(int index, std::vector<unsigned char>& result)
	{
		checkreader();
		const int datasize = sqlite3_column_bytes(cmd->stmt, index);
		result.resize(datasize);
		if (datasize)
		memcpy(result.data(), sqlite3_column_blob(cmd->stmt, index), datasize);
	}

	std::string sqlite3_reader::getcolname(int index)
	{
		checkreader();
		return sqlite3_column_name(cmd->stmt, index);
	}

#ifndef SQLITE_OMIT_UTF16
	std::wstring sqlite3_reader::getstring16(int index)
	{
		checkreader();
		int size = sqlite3_column_bytes16(cmd->stmt, index);
		if (!size) return std::wstring();
		return std::wstring((const wchar_t *) sqlite3_column_text16(cmd->stmt, index), size/2);
	}

	std::wstring sqlite3_reader::getcolname16(int index)
	{
		checkreader();
		return (const wchar_t *) sqlite3_column_name16(cmd->stmt, index);
	}
#endif
}
