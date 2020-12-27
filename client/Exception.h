/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DCPLUSPLUS_DCPP_EXCEPTION_H
#define DCPLUSPLUS_DCPP_EXCEPTION_H

#include <string>
#include <exception>

#include "debug.h"
#include "noexcept.h"

using std::string;

class Exception : public std::exception
{
	public:
		Exception() {}
		Exception& operator= (const Exception&) = delete;
		virtual ~Exception() noexcept {}
		explicit Exception(const string& error);
		const char* what() const
		{
			return m_error.c_str();
		}
		const string& getError() const
		{
			return m_error;
		}

	protected:
		const string m_error;
};

#ifdef _DEBUG

#define STANDARD_EXCEPTION(name) class name : public Exception { \
		public:\
			explicit name(const string& error) : Exception(#name ": " + error) { } \
	}

#define STANDARD_EXCEPTION_ADD_INFO(name) class name : public Exception { \
		public:\
			name(const string& error, const string& details) : Exception(#name ": " + error + "\n[" + details + ']') { } \
			name(const string& error) : Exception(#name ": " + error) { } \
	}

#else // _DEBUG

#define STANDARD_EXCEPTION(name) class name : public Exception { \
		public:\
			explicit name(const string& error) : Exception(error) { } \
	}

#define STANDARD_EXCEPTION_ADD_INFO(name) class name : public Exception { \
		public:\
			name(const string& error, const string& details) : Exception(error + "\n[" + details + ']') { } \
			name(const string& error) : Exception(error) { } \
	}
#endif

#endif // !defined(EXCEPTION_H)
