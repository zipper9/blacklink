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

class Client;

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
		SizeModes sizeMode;
		int64_t size;
		unsigned maxResults;
		bool isPassive;
		int fileType;
		string filter;
		string filterExclude;
		Client* client;
		SearchParamBase() : size(0), sizeMode(SIZE_DONTCARE), fileType(FILE_TYPE_ANY), maxResults(0), isPassive(false), client(nullptr)
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
		void init(Client* client, bool isPassive)
		{
			this->client = client;
			this->isPassive = isPassive;
			maxResults = isPassive ? 5 : 10;
		}
};

class NmdcSearchParam : public SearchParamBase
{
	public:
		string cacheKey;
		string seeker;
};

class SearchParamToken : public SearchParamBase
{
	public:
		bool       forcePassive;
		uint32_t   token;
		void*      owner;
		StringList extList;
		SearchParamToken() : forcePassive(false), token(0), owner(nullptr)
		{
		}
};

class SearchParamTokenMultiClient : public SearchParamToken
{
	public:
		StringList clients;
};

#endif // SEARCH_PARAM_H
