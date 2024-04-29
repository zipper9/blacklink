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

#include "stdinc.h"
#include "ResourceManager.h"
#include "Text.h"
#include "SimpleXML.h"
#include "LogManager.h"

#ifdef _UNICODE
bool ResourceManager::stringsChanged = true;
wstring ResourceManager::g_wstrings[ResourceManager::LAST];
#endif

boost::unordered_map<string, int> ResourceManager::nameToIndex;

static int defaultPluralCatFunc(int num)
{
	if (num == 0) return ResourceManager::zero;
	if (num == 1) return ResourceManager::one;
	return ResourceManager::other;
}

int (*ResourceManager::pluralCatFunc)(int val) = defaultPluralCatFunc;

bool ResourceManager::loadLanguage(const string& filePath)
{
	if (nameToIndex.empty())
		for (int i = 0; i < LAST; ++i)
		{
			g_names[i].shrink_to_fit();
			nameToIndex[g_names[i]] = i;
		}

	bool result = false;
	try
	{
		File f(filePath, File::READ, File::OPEN);
		SimpleXML xml;
		xml.fromXML(f.read());

		if (xml.findChild("Language"))
		{
			xml.stepIn();
			if (xml.findChild("Strings"))
			{
				xml.stepIn();
				const string strChildName("String");
				const string strAttribName("Name");
				while (xml.findChild(strChildName))
				{
					const string& name = xml.getChildAttrib(strAttribName);
					if (!name.empty())
					{
						auto i = nameToIndex.find(name);
						if (i != nameToIndex.end())
						{
							int index = i->second;
							g_strings[index] = xml.getChildData();
							g_strings[index].shrink_to_fit();
							result = true;
#ifdef _UNICODE
							stringsChanged = true;
#endif
						}
					}
				}
			}
		}
	}
	catch (Exception& ex)
	{
#if 0
		LogManager::message("Error loading language file " + filePath + ": " + ex.getError(), false);
#endif
		result = false;
	}
#ifdef _UNICODE
	if (stringsChanged) createWide();
#endif
	return result;
}

#ifdef _UNICODE
void ResourceManager::createWide()
{
	for (size_t i = 0; i < LAST; ++i)
	{
		g_wstrings[i].clear();
		if (!g_strings[i].empty())
		{
			Text::toT(g_strings[i], g_wstrings[i]);
			g_wstrings[i].shrink_to_fit();
		}
	}
	stringsChanged = false;
}
#endif

int ResourceManager::getStringByName(const string& name)
{
	auto i = nameToIndex.find(name);
	return i != nameToIndex.end() ? i->second : -1;
}

static const char* pluralKeywords[] = { "zero", "one", "two", "few", "many", "other" };

template<typename string_t>
static int checkKeyword(const string_t& s, size_t start, size_t len, int index)
{
	const char* kw = pluralKeywords[index];
	for (size_t i = 0; i < len; i++)
		if (kw[i] != s[start + i]) return -1;
	return kw[len] ? -1 : index;
}

template<typename string_t>
static int getCategoryFromKeyword(const string_t& s, size_t start, size_t len)
{
	if (len < 3) return -1;
	static const uint8_t offset[] = { 0, 1, 0, 0, 0, 1 };
	for (int i = 0; i < 6; i++)
		if (s[start + offset[i]] == pluralKeywords[i][offset[i]])
			return checkKeyword<string_t>(s, start, len, i);
	return -1;
}

template<typename string_t>
static string_t getPluralStringText(const string_t& source, size_t start, size_t len)
{
	string_t s = source.substr(start, len);
	size_t i = 0;
	while (i < s.length())
	{
		size_t j = s.find('|', i);
		if (j == string_t::npos || j + 1 >= s.length()) break;
		if (s[j + 1] == '|') s.erase(j, 1);
		i = j + 1;
	}
	return s;
}

template<typename string_t>
static string_t getPluralStringImpl(const string_t& tpl, int category)
{
	string_t otherVal;
	size_t i = 0;
	while (i < tpl.length())
	{
		size_t j = tpl.find('|', i);
		if (j == 0)
		{
			i = j + 1;
			continue;
		}
		while (true)
		{
			if (j == string_t::npos) { j = tpl.length(); break; }
			if (!(j + 1 < tpl.length() && tpl[j + 1] == '|')) break;
			j = tpl.find('|', j + 2);
		}
		size_t k = tpl.find(':', i);
		if (k != string_t::npos && k < j)
		{
			int cat = getCategoryFromKeyword<string_t>(tpl, i, k - i);
			if (cat == category)
				return getPluralStringText<string_t>(tpl, k + 1, j - (k + 1));
			if (cat == ResourceManager::other)
				otherVal = getPluralStringText<string_t>(tpl, k + 1, j - (k + 1));
		}
		i = j + 1;
	}
	return otherVal;
}

string ResourceManager::getPluralString(const string& tpl, int category)
{
	return getPluralStringImpl<string>(tpl, category);
}

#ifdef _UNICODE
wstring ResourceManager::getPluralStringW(const wstring& tpl, int category)
{
	return getPluralStringImpl<wstring>(tpl, category);
}
#endif
