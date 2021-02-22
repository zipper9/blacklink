/*
 * Copyright (C) 2003-2006 RevConnect, http://www.revconnect.com
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

#ifndef SEARCH_PARAM_H
#define SEARCH_PARAM_H

#include "typedefs.h"
#include "FileTypes.h"
#include "Util.h"
#include "CID.h"

enum SizeModes
{
	SIZE_DONTCARE = 0,
	SIZE_ATLEAST  = 1,
	SIZE_ATMOST   = 2,
	SIZE_EXACT    = 3
};

class SearchParamBase
{
	public:
		enum SearchMode
		{
			MODE_DEFAULT,
			MODE_ACTIVE,
			MODE_PASSIVE
		};

		static const unsigned MAX_RESULTS_ACTIVE  = 10;
		static const unsigned MAX_RESULTS_PASSIVE = 5;

		SearchMode searchMode;
		SizeModes sizeMode;
		int64_t size;
		unsigned maxResults; // NMDC only
		int fileType;
		string filter;
		string filterExclude;
		SearchParamBase() : searchMode(MODE_DEFAULT), sizeMode(SIZE_DONTCARE), size(0), fileType(FILE_TYPE_ANY), maxResults(0)
		{
		}
		static void normalizeWhitespace(string& s)
		{
			for (string::size_type i = 0; i < s.length(); i++)
				if (s[i] == '\t' || s[i] == '\n' || s[i] == '\r')
					s[i] = ' ';
		}
		void normalizeWhitespace()
		{
			normalizeWhitespace(filter);
			normalizeWhitespace(filterExclude);
		}
};

class NmdcSearchParam : public SearchParamBase
{
	public:
		string cacheKey;
		string seeker;
		CID shareGroup;
};

class SearchParamToken : public SearchParamBase
{
	public:
		uint32_t   token;
		void*      owner;
		StringList extList;

		SearchParamToken() : token(0), owner(nullptr) {}
		void generateToken(bool autoToken)
		{
			token = Util::rand();
			if (autoToken) token &= ~1; else token |= 1;
		}
};

struct SearchClientItem
{
	string url;
	unsigned waitTime;
};

#endif // SEARCH_PARAM_H
