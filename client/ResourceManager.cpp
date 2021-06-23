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
		LogManager::message("Error loading language file " + filePath + ": " + ex.getError(), false);
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
