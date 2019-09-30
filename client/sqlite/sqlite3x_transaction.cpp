/*
	This is a modified version of sqlite3x_transaction.cpp, not the original code!

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
#include "sqlite3.h"
#include "sqlite3x.hpp"

namespace sqlite3x
{
	sqlite3_transaction::sqlite3_transaction(sqlite3_connection &conn, bool start /*= true*/) : conn(conn), intrans(false) 
	{
		if (start) begin();
	}

	sqlite3_transaction::~sqlite3_transaction()
	{
		if (intrans)
		{
			try
			{
				dcassert(0);
				rollback();
			}
			catch (...)
			{
				dcassert(0);
				return;
			}
		}
	}

	void sqlite3_transaction::begin()
	{
		if (sqlite3_get_autocommit(conn.getdb()))
		{
			conn.executenonquery("begin;");
			intrans = true;
		}
		else
			intrans = false;
	}

	void sqlite3_transaction::commit()
	{
		if (intrans)
		{
			conn.executenonquery("commit;");
			intrans = false;
		}
	}

	void sqlite3_transaction::rollback()
	{
		if (intrans)
		{
			conn.executenonquery("rollback;");
			intrans = false;
		}
	}
}
