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

#ifndef DCPLUSPLUS_CLIENT_RESOURCE_MANAGER_H
#define DCPLUSPLUS_CLIENT_RESOURCE_MANAGER_H

#include "debug.h"
#include "dcformat.h"

#define STRING(x) ResourceManager::getString(ResourceManager::x)
#define CSTRING(x) ResourceManager::getString(ResourceManager::x).c_str()
#define WSTRING(x) ResourceManager::getStringW(ResourceManager::x)
#define CWSTRING(x) ResourceManager::getStringW(ResourceManager::x).c_str()

#define STRING_I(x) ResourceManager::getString(x)
#define CSTRING_I(x) ResourceManager::getString(x).c_str()
#define WSTRING_I(x) ResourceManager::getStringW(x)
#define CWSTRING_I(x) ResourceManager::getStringW(x).c_str()

#define STRING_F(x, args) (dcpp_fmt(ResourceManager::getString(ResourceManager::x)) % args).str()
#define CSTRING_F(x, args) (dcpp_fmt(ResourceManager::getString(ResourceManager::x)) % args).str().c_str()
#define WSTRING_F(x, args) (dcpp_fmt(ResourceManager::getStringW(ResourceManager::x)) % args).str()
#define CWSTRING_F(x, args) (dcpp_fmt(ResourceManager::getStringW(ResourceManager::x)) % args).str().c_str()

#ifdef UNICODE
#define TSTRING WSTRING
#define CTSTRING CWSTRING
#define TSTRING_I WSTRING_I
#define CTSTRING_I CWSTRING_I
#define TSTRING_F WSTRING_F
#define CTSTRING_F CWSTRING_F
#else
#define TSTRING STRING
#define CTSTRING CSTRING
#define TSTRING_I STRING_I
#define CTSTRING_I CSTRING_I
#define TSTRING_F STRING_F
#define CTSTRING_F CSTRING_F
#endif

class ResourceManager
{
	public:
	
#include "StringDefs.h"
	
		static void startup(bool p_is_create_wide)
		{
			if (p_is_create_wide)
			{
				createWide();
			}
#ifdef _DEBUG
			g_debugStarted = true;
#endif
		}
		static bool loadLanguage(const string& aFile);
		static const string& getString(Strings x)
		{
			return g_strings[x];
		}
		static const wstring& getStringW(Strings x)
		{
			dcassert(g_debugStarted);
			return g_wstrings[x];
		}

	private:
		ResourceManager() {}
		
		static string g_strings[LAST];
		static wstring g_wstrings[LAST];
		static string g_names[LAST];
		static void createWide();
#ifdef _DEBUG
		static bool g_debugStarted;
#endif
};

#endif // !defined(RESOURCE_MANAGER_H)
