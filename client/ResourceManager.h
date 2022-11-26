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
#include <boost/unordered/unordered_map.hpp>

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

#define PLURAL_F(x, number) (dcpp_fmt(ResourceManager::getPluralString(ResourceManager::x, ResourceManager::getPluralCategory(number))) % number).str()
#define WPLURAL_F(x, number) (dcpp_fmt(ResourceManager::getPluralStringW(ResourceManager::x, ResourceManager::getPluralCategory(number))) % number).str()

#ifdef _UNICODE
#define TSTRING WSTRING
#define CTSTRING CWSTRING
#define TSTRING_I WSTRING_I
#define CTSTRING_I CWSTRING_I
#define TSTRING_F WSTRING_F
#define CTSTRING_F CWSTRING_F
#define TPLURAL_F WPLURAL_F
#define getPluralStringT getPluralStringW
#else
#define TSTRING STRING
#define CTSTRING CSTRING
#define TSTRING_I STRING_I
#define CTSTRING_I CSTRING_I
#define TSTRING_F STRING_F
#define CTSTRING_F CSTRING_F
#define TPLURAL_F PLURAL_F
#define getPluralStringT getPluralString
#endif

class ResourceManager
{
	public:

#include "StringDefs.h"

		static bool loadLanguage(const string& filePath);
		static const string& getString(Strings x) { return g_strings[x]; }

#ifdef _UNICODE
		static const wstring& getStringW(Strings x) { return g_wstrings[x]; }
#endif
		static int getStringByName(const string& name);

		enum
		{
			zero, one, two, few, many, other
		};

		static string getPluralString(const string& tpl, int category);
		static string getPluralString(Strings x, int category) { return getPluralString(g_strings[x], category); }
#ifdef _UNICODE
		static wstring getPluralStringW(const wstring& tpl, int category);
		static wstring getPluralStringW(Strings x, int category) { return getPluralStringW(g_wstrings[x], category); }
#endif
		static int getPluralCategory(int num) { return pluralCatFunc(num); }

	private:
		static boost::unordered_map<string, int> nameToIndex;
		static string g_names[LAST];
		static string g_strings[LAST];
		static int (*pluralCatFunc)(int val);

#ifdef _UNICODE
		static wstring g_wstrings[LAST];
		static bool stringsChanged;

		static void createWide();
#endif
};

#endif // !defined(RESOURCE_MANAGER_H)
