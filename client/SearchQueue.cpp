/*
 * Copyright (C) 2003-2013 RevConnect, http://www.revconnect.com
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
#include "SearchQueue.h"
#include "QueueManager.h"
#include "SearchManager.h"
#include "NmdcHub.h"

static inline bool equals(const Search& lhs, const Search& rhs)
{
	return lhs.sizeMode == rhs.sizeMode &&
	       lhs.size == rhs.size &&
	       lhs.fileType == rhs.fileType &&
	       lhs.filter == rhs.filter &&
	       lhs.filterExclude == rhs.filterExclude;
}

bool SearchQueue::add(const Search& s)
{
	dcassert(s.owners.size() == 1);
	dcassert(interval >= 2000);
	
	LOCK(cs);
	
	for (auto i = searchQueue.begin(); i != searchQueue.end(); ++i)
	{
		// check dupe
		if (equals(*i, s))
		{
			void* owner = *s.owners.begin();
			i->owners.insert(owner);
			
			// if previous search was autosearch and current one isn't, it should be readded before autosearches
			if (!s.isAutoToken() && i->isAutoToken())
			{
				searchQueue.erase(i);
				break;
			}
			return false;
		}
	}
	
	if (s.isAutoToken() || searchQueue.empty())
	{
		searchQueue.push_back(s);
		return true;
	}

	bool added = false;
	// Insert before the automatic searches (manual search)
	for (auto i = searchQueue.cbegin(); i != searchQueue.cend(); ++i)
	{
		if (i->isAutoToken())
		{
			searchQueue.insert(i, s);
			added = true;
			break;
		}
	}
	if (!added)
		searchQueue.push_back(s);
	return true;
}

bool SearchQueue::pop(Search& s, uint64_t now)
{
	dcassert(interval && intervalPassive);
	LOCK(cs);
	if (searchQueue.empty())
		return false;
	if (lastSearchTime && now <= lastSearchTime + (lastSearchPassive ? intervalPassive : interval))
		return false;
	
	Search& queued = searchQueue.front();
	lastSearchPassive = queued.searchMode == SearchParamBase::MODE_PASSIVE;
	s = std::move(queued);
	searchQueue.pop_front();
	lastSearchTime = now;
	return true;
}

uint64_t SearchQueue::getSearchTime(void* owner, uint64_t now) const
{
	if (!owner) return 0;

	LOCK(cs);
	uint64_t searchTime;
	if (lastSearchTime)
	{
		searchTime = lastSearchTime + (lastSearchPassive ? intervalPassive : interval);
		if (now > searchTime) searchTime = now;
	}
	else
		searchTime = now;
	for (const Search& search : searchQueue)
	{
		if (search.isAutoToken() || search.owners.empty()) break;		
		if (search.owners.find(owner) != search.owners.end()) return searchTime;
		searchTime += search.searchMode == SearchParamBase::MODE_PASSIVE ? intervalPassive : interval;
	}
	return 0;
}

bool SearchQueue::cancelSearch(void* owner)
{
	LOCK(cs);
	for (auto i = searchQueue.begin(); i != searchQueue.end(); ++i)
	{
		auto &owners = i->owners;
		const auto j = owners.find(owner);
		if (j != owners.end())
		{
			owners.erase(j);
			if (owners.empty())
			{
				searchQueue.erase(i);
			}
			return true;
		}
	}
	return false;
}

bool SearchQueue::hasQueuedItems() const
{
	LOCK(cs);
	return !searchQueue.empty();
}
