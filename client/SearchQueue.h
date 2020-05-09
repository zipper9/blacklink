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

#include "SearchParam.h"
#include "Thread.h"

struct Search
{
	Search() : forcePassive(false), sizeMode(SIZE_DONTCARE), size(0), fileType(0), token(0)
	{
	}

	bool       forcePassive;
	SizeModes  sizeMode;
	int64_t    size;
	int        fileType;
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
		       fileType == rhs.fileType &&
		       filter == rhs.filter &&
		       filterExclude == rhs.filterExclude;
	}
};

class SearchQueue
{
	public:
		SearchQueue() : lastSearchTime(0), interval(0), intervalPassive(0)
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
