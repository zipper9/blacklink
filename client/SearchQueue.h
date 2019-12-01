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

#ifndef SEARCH_QUEUE_H
#define SEARCH_QUEUE_H

#include "CFlyThread.h"
#include "FileTypes.h"
#include "typedefs.h"

struct Search
{
	enum SizeModes
	{
		SIZE_DONTCARE = 0,
		SIZE_ATLEAST  = 1,
		SIZE_ATMOST   = 2,
		SIZE_EXACT    = 3
	};

	Search() : forcePassive(false), sizeMode(SIZE_DONTCARE), size(0), fileTypesBitmap(0), token(0)
	{
	}

	bool       forcePassive;
	SizeModes  sizeMode;
	int64_t    size;
	uint32_t   fileTypesBitmap;
	string     filter;
	string     filterExclude;
	uint32_t   token;
	StringList extList;
	std::unordered_set<void*> owners;
	bool isAutoToken() const
	{
		return token == 0;
	}
	bool operator==(const Search& rhs) const
	{
		return sizeMode == rhs.sizeMode &&
		       size == rhs.size &&
		       fileTypesBitmap == rhs.fileTypesBitmap &&
		       filter == rhs.filter &&
		       filterExclude == rhs.filterExclude;
	}
};

class Client;

class SearchParamBase
{
	public:
		Search::SizeModes sizeMode;
		int64_t size;
		unsigned maxResults;
		bool isPassive;
		int fileType;
		string filter;
		string filterExclude;
		Client* client;
		SearchParamBase() : size(0), sizeMode(Search::SIZE_DONTCARE), fileType(FILE_TYPE_ANY), maxResults(0), isPassive(false), client(nullptr)
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

class SearchParam : public SearchParamBase
{
	public:
		string rawSearch;
		string seeker;
		string::size_type queryPos;

		SearchParam(): queryPos(string::npos)
		{
		}

		string getRawQuery() const
		{
			dcassert(queryPos != string::npos);
			if (queryPos != string::npos)
				return rawSearch.substr(queryPos);
			return string();
		}
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

class SearchQueue
{
	public:
	
		SearchQueue()
			: lastSearchTime(0), interval(0), intervalPassive(0)
		{
		}
		
		bool add(const Search& s);
		bool pop(Search& s, uint64_t now, bool isPassive);
		void clear()
		{
			CFlyFastLock(cs);
			searchQueue.clear();
		}
		
		bool cancelSearch(void* aOwner);
		
		/** return 0 means not in queue */
		uint64_t getSearchTime(void* aOwner, uint64_t now);
		
		/**
		    by milli-seconds
		    0 means no interval, no auto search and manual search is sent immediately
		*/
		uint32_t interval;
		uint32_t intervalPassive;
		
	private:
		deque<Search> searchQueue;
		uint64_t lastSearchTime;
		FastCriticalSection cs;
};

#endif // SEARCH_QUEUE_H
