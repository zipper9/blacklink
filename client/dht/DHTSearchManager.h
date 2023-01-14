/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
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

#ifndef _DHT_SEARCHMANAGER_H
#define _DHT_SEARCHMANAGER_H

#include "KBucket.h"

#include "../CID.h"
#include "../MerkleTree.h"
#include "../Singleton.h"
#include "../TimerManager.h"
#include "../User.h"
#include "../SearchResult.h"
#include "../SearchQueue.h"

namespace dht
{

	struct DHTSearch
	{

		DHTSearch() : partial(false), stopping(false) {}

		enum SearchType { TYPE_FILE = 1, TYPE_NODE = 3, TYPE_STOREFILE = 4 }; // standard types should match ADC protocol

		Node::Map possibleNodes;  // nodes where send search request soon to
		Node::Map triedNodes;     // nodes where search request has already been sent to
		Node::Map respondedNodes; // nodes who responded to this search request

		uint32_t token;    // search token
		string term;       // search term (TTH/CID)
		uint64_t lifeTime; // time when this search has been started
		int64_t filesize;  // file size
		SearchType type;   // search type
		bool partial;      // is this partial file search?
		bool stopping;     // search is being stopped
#ifdef DEBUG_DHT_SEARCH
		unsigned nodeCtr = 0;
#endif

		/** Processes this search request */
		void process(uint64_t tick);
		void onRemove();
	};

	class SearchManager : public Singleton<SearchManager>
	{
	public:
		SearchManager();
		~SearchManager();

		/** Performs node lookup in the network */
		void findNode(const CID& cid);

		/** Performs value lookup in the network, returns time to wait in the queue */
		unsigned findFile(const string& tth, uint32_t token, uint64_t owner);

		/** Performs node lookup to store key/value pair in the network */
		void findStore(const string& tth, int64_t size, bool partial);

		/** Process incoming search request */
		void processSearchRequest(const Node::Ptr& node, const AdcCommand& cmd);

		/** Process incoming search result */
		bool processSearchResult(const Node::Ptr& node, const AdcCommand& cmd);

		/** Processes all running searches and removes long-time ones */
		void processSearches();

		/** Processes incoming search results */
		bool processSearchResults(const UserPtr& user, size_t slots);

		void processQueue(uint64_t tick);

	private:
		/** Running search operations */
		typedef std::unordered_map<uint32_t, DHTSearch*> SearchMap;
		SearchMap searches;

		/** Locks access to "searches" */
		CriticalSection cs;

		typedef std::unordered_multimap<CID, std::pair<uint64_t, SearchResult>> ResultsMap;
		ResultsMap searchResults;

		SearchQueue searchQueue;

		/** Performs general search operation in the network */
		void search(DHTSearch* s, uint64_t startTick);

		/** Sends publishing request */
		void publishFile(const Node::Map& nodes, const string& tth, int64_t size, bool partial);

		/** Checks whether we are alreading searching for a term */
		bool isAlreadySearchingFor(const string& term);
	};

}

#endif	// _DHT_SEARCHMANAGER_H
