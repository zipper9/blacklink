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

#include "stdinc.h"
#include "DHTSearchManager.h"

#include "Constants.h"
#include "DHT.h"
#include "IndexManager.h"
#include "Utils.h"

#include "../ClientManager.h"
#include "../SearchManager.h"
#include "../SearchResult.h"
#include "../SimpleXML.h"

#ifdef DEBUG_DHT_SEARCH
#include "../LogManager.h"
#endif

namespace dht
{

	void DHTSearch::onRemove()
	{
#ifdef DEBUG_DHT_SEARCH
		if (type == TYPE_FILE)
			LogManager::message("DHT Search " + Util::toString(token) + " completed, tried " + Util::toString(triedNodes.size()) + " nodes", false);
#endif
		switch (type)
		{
			case TYPE_NODE: IndexManager::getInstance()->setPublish(true); break;
			case TYPE_STOREFILE: IndexManager::getInstance()->decPublishing(); break;
		}
	}

	/*
	 * Process this search request
	 */
	void DHTSearch::process(uint64_t tick)
	{
		if (stopping)
			return;

		// no node to search
		if (possibleNodes.empty()/* || respondedNodes.size() >= MAX_SEARCH_RESULTS*/)
		{
			stopping = true;
			lifeTime = tick + SEARCH_STOPTIME; // wait before deleting not to lose so much delayed results
			return;
		}

		// send search request to the first ALPHA closest nodes
		size_t nodesCount = min((size_t) SEARCH_ALPHA, possibleNodes.size());
		Node::Map::iterator it;
		for (size_t i = 0; i < nodesCount; i++)
		{
			it = possibleNodes.begin();
			Node::Ptr node = it->second;

			// move to tried and delete from possibles
			triedNodes[it->first] = node;
			possibleNodes.erase(it);

			// send SCH command
			AdcCommand cmd(AdcCommand::CMD_SCH, AdcCommand::TYPE_UDP);
			cmd.addParam("TR", term);
			cmd.addParam("TY", Util::toString(type));
			cmd.addParam("TO", Util::toString(token));

			//node->setTimeout();
			DHT::getInstance()->send(cmd, node->getIdentity().getIp(),
				node->getIdentity().getUdpPort(), node->getUser()->getCID(), node->getUdpKey());
		}
	}

	SearchManager::SearchManager()
	{
		searchQueue.interval = searchQueue.intervalPassive = 15000;
	}

	SearchManager::~SearchManager()
	{
		for (auto& s : searches)
			delete s.second;
	}

	/*
	 * Performs node lookup in the network
	 */
	void SearchManager::findNode(const CID& cid)
	{
		string cidStr = cid.toBase32();
		if (isAlreadySearchingFor(cidStr))
			return;

		DHTSearch* s = new DHTSearch;
		s->type = DHTSearch::TYPE_NODE;
		s->term = std::move(cidStr);
		s->token = Util::rand();

		search(s, GET_TICK());
	}

	/*
	 * Performs value lookup in the network
	 */
	unsigned SearchManager::findFile(const string& tth, uint32_t token, void* owner)
	{
		if (isAlreadySearchingFor(tth))
			return 0;

		Search si;
		si.searchMode = SearchParamBase::MODE_ACTIVE;
		si.fileType = FILE_TYPE_TTH;
		si.filter = tth;
		si.token = token;
		si.owners.insert(owner);
		searchQueue.add(si);

		// do I have requested TTH in my store?
		//IndexManager::SourceList sources;
		//if (IndexManager::getInstance()->findResult(TTHValue(tth), sources))
		//{
		//	for (IndexManager::SourceList::const_iterator i = sources.begin(); i != sources.end(); i++)
		//	{
		//		// create user as offline (only TCP connected users will be online)
		//		UserPtr u = ClientManager::getInstance()->getUser(i->getCID());
		//		u->setFlag(User::DHT);
		//
		//		// contact node that we are online and we want his info
		//		DHT::getInstance()->info(i->getIp(), i->getUdpPort(), true);
		//
		//		SearchResultPtr sr(new SearchResult(u, SearchResult::TYPE_FILE, 0, 0, i->getSize(), tth, "DHT", Util::emptyString, i->getIp(), TTHValue(tth), token));
		//		dcpp::SearchManager::getInstance()->fire(SearchManagerListener::SR(), sr);
		//	}
		//
		//	return;
		//}

		const uint64_t now = GET_TICK();
		uint64_t st = searchQueue.getSearchTime(owner, now);
		return st ? unsigned(st - now) : 0;
	}

	/*
	 * Performs node lookup to store key/value pair in the network
	 */
	void SearchManager::findStore(const string& tth, int64_t size, bool partial)
	{
		if (isAlreadySearchingFor(tth))
		{
			IndexManager::getInstance()->decPublishing();
			return;
		}

		DHTSearch* s = new DHTSearch;
		s->type = DHTSearch::TYPE_STOREFILE;
		s->term = tth;
		s->filesize = size;
		s->partial = partial;
		s->token = Util::rand();

		search(s, GET_TICK());
	}

	/*
	 * Performs general search operation in the network
	 */
	void SearchManager::search(DHTSearch* s, uint64_t startTick)
	{
		// set search lifetime
		s->lifeTime = startTick;
		switch (s->type)
		{
			case DHTSearch::TYPE_FILE:
				s->lifeTime += SEARCHFILE_LIFETIME;
				break;
			case DHTSearch::TYPE_NODE:
				s->lifeTime += SEARCHNODE_LIFETIME;
				break;
			case DHTSearch::TYPE_STOREFILE:
				s->lifeTime += SEARCHSTOREFILE_LIFETIME;
				break;
		}

		// get nodes closest to requested ID
		DHT::getInstance()->getClosestNodes(CID(s->term), s->possibleNodes, 100, 3);

#ifdef DEBUG_DHT_SEARCH
		if (s->type == DHTSearch::TYPE_FILE)
		{
			for (auto i = s->possibleNodes.cbegin(); i != s->possibleNodes.cend(); ++i)
				LogManager::message("DHT Search " + Util::toString(s->token) + " using initial node " +
					Util::toString(++s->nodeCtr) + ": " + i->second->getUser()->getCID().toBase32(), false);
		}
#endif

		if (s->possibleNodes.empty())
		{
			s->onRemove();
			delete s;
			return;
		}

		CFlyLock(cs);
		// store search
		searches[s->token] = s;

		s->process(startTick);
	}

	/*
	 * Process incoming search request
	 */
	void SearchManager::processSearchRequest(const Node::Ptr& node, const AdcCommand& cmd)
	{
		string token;
		if (!cmd.getParam("TO", 1, token))
			return;	// missing search token?

		string term;
		if (!cmd.getParam("TR", 1, term))
			return;	// nothing to search?

		string type;
		if (!cmd.getParam("TY", 1, type))
			return;	// type not specified?

		AdcCommand res(AdcCommand::CMD_RES, AdcCommand::TYPE_UDP);
		res.addParam("TO", token);

		SimpleXML xml;
		xml.addTag("Nodes");
		xml.stepIn();

		bool empty = true;
		unsigned int searchType = Util::toInt(type);
		switch (searchType)
		{
			case DHTSearch::TYPE_FILE:
			{
				unsigned maxSearchResults = MAX_SEARCH_RESULTS;
				TTHValue tth(term);

				// check our share first
				int64_t size;
				if (::ShareManager::getInstance()->getFileInfo(tth, size))
				{
					xml.addTag("Source");
					xml.addChildAttrib("CID", ClientManager::getMyCID().toBase32());
					xml.addChildAttrib("I4", DHT::getInstance()->getLastExternalIP());
					xml.addChildAttrib("U4", DHT::getPort());
					xml.addChildAttrib("SI", size);
					xml.addChildAttrib("PF", false);

					empty = false;
					maxSearchResults--;
				}

				// check file hash in our database
				// if it's there, then select sources else do the same as node search
				IndexManager::SourceList sources;
				if (IndexManager::getInstance()->findResult(tth, sources))
				{
					// yes, we got sources for this file
					for (IndexManager::SourceList::const_iterator i = sources.begin(); i != sources.end(); i++)
					{
						xml.addTag("Source");
						xml.addChildAttrib("CID", i->getCID().toBase32());
						xml.addChildAttrib("I4", i->getIp());
						xml.addChildAttrib("U4", i->getUdpPort());
						xml.addChildAttrib("SI", i->getSize());
						xml.addChildAttrib("PF", i->getPartial());

						empty = false;
						if (--maxSearchResults == 0) break;
					}
				}

				if (!empty) break;
			}
			default:
			{
				// maximum nodes in response is based on search type
				unsigned int count;
				switch (searchType)
				{
				case DHTSearch::TYPE_FILE: count = 2; break;
				case DHTSearch::TYPE_NODE: count = 10; break;
				case DHTSearch::TYPE_STOREFILE: count = 4; break;
				default: return; // unknown type
				}

				// get nodes closest to requested ID
				Node::Map nodes;
				DHT::getInstance()->getClosestNodes(CID(term), nodes, count, 2);

				// add nodelist in XML format
				for (Node::Map::const_iterator i = nodes.begin(); i != nodes.end(); i++)
				{
					xml.addTag("Node");
					xml.addChildAttrib("CID", i->second->getUser()->getCID().toBase32());
					xml.addChildAttrib("I4", i->second->getIdentity().getIpAsString());
					xml.addChildAttrib("U4", i->second->getIdentity().getUdpPort());

					empty = false;
				}

				break;
			}
		}

		xml.stepOut();

		if (empty)
			return;	// no requested nodes found, don't send empty list

		string nodes;
		StringOutputStream sos(nodes);
		xml.toXML(&sos);

		res.addParam("NX", Utils::compressXML(nodes));

		// send search result
		DHT::getInstance()->send(res, node->getIdentity().getIp(),
			node->getIdentity().getUdpPort(), node->getUser()->getCID(), node->getUdpKey());
	}


	/*
	 * Process incoming search result
	 */
	bool SearchManager::processSearchResult(const Node::Ptr& node, const AdcCommand& cmd)
	{
		string token;
		if (!cmd.getParam("TO", 1, token))
			return false; // missing search token?

		string nodes;
		if (!cmd.getParam("NX", 1, nodes))
			return false; // missing search token?

		uint32_t intToken = Util::toUInt32(token);

		CFlyLock(cs);
		SearchMap::iterator i = searches.find(intToken);
		if (i == searches.end())
		{
			// we didn't search for this
			return true;
		}

		DHTSearch* s = i->second;

		// store this node
		s->respondedNodes.insert(std::make_pair(Utils::getDistance(node->getUser()->getCID(), CID(s->term)), node));

		try
		{
			SimpleXML xml;
			xml.fromXML(nodes);
			xml.stepIn();

			if (s->type == DHTSearch::TYPE_FILE) // this is response to TYPE_FILE, check sources first
			{
				// extract file sources
				while (xml.findChild("Source"))
				{
					const CID cid = CID(xml.getChildAttrib("CID"));
					const string& i4 = xml.getChildAttrib("I4");
					uint16_t u4 = static_cast<uint16_t>(xml.getIntChildAttrib("U4"));
					int64_t size = xml.getInt64ChildAttrib("SI");
					bool partial = xml.getBoolChildAttrib("PF");

					boost::system::error_code ec;
					boost::asio::ip::address_v4 address = boost::asio::ip::address_v4::from_string(i4, ec);
					if (ec)
						continue;

					// don't bother with invalid sources and private IPs
					if (cid.isZero() || ClientManager::getMyCID() == cid || !Utils::isGoodIPPort(address.to_uint(), u4))
						continue;

					// create user as offline (only TCP connected users will be online)
					Node::Ptr source = DHT::getInstance()->createNode(cid, address, u4, false, false);

					if (partial)
					{
						if (!source->isOnline())
						{
							// node is not online, try to contact him
							DHT::getInstance()->info(address, u4, DHT::PING | DHT::MAKE_ONLINE, cid, source->getUdpKey());
						}

						// ask for partial file
						AdcCommand request(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
						request.addParam("U4", Util::toString(::SearchManager::getInstance()->getSearchPortUint()));
						request.addParam("TR", s->term);

						DHT::getInstance()->send(request, address, u4, cid, source->getUdpKey());
					}
					else
					{
						// create search result: hub name+ip => "DHT", file name => TTH
						SearchResult sr(source->getUser(), SearchResult::TYPE_FILE, 0, SearchResult::SLOTS_UNKNOWN, size, s->term, dht::NetworkName, dht::NetworkName, address, TTHValue(s->term), intToken);
						if (!source->isOnline())
						{
							// node is not online, try to contact him if we didn't contact him recently
							if (searchResults.find(source->getUser()->getCID()) != searchResults.end())
								DHT::getInstance()->info(address, u4, DHT::PING | DHT::MAKE_ONLINE, cid, source->getUdpKey());

							searchResults.insert(std::make_pair(source->getUser()->getCID(), std::make_pair(GET_TICK(), sr)));
						}
						else
						{
							sr.slots = source->getIdentity().getSlots();
							::SearchManager::getInstance()->fire(SearchManagerListener::SR(), sr);
						}
					}
				}

				xml.resetCurrentChild();
			}

			// extract possible nodes
			unsigned int n = K;
			while (xml.findChild("Node"))
			{
				CID cid = CID(xml.getChildAttrib("CID"));
				CID distance = Utils::getDistance(cid, CID(s->term));
				const string& i4 = xml.getChildAttrib("I4");
				uint16_t u4 = static_cast<uint16_t>(xml.getIntChildAttrib("U4"));

				boost::system::error_code ec;
				boost::asio::ip::address_v4 address = boost::asio::ip::address_v4::from_string(i4, ec);
				if (ec)
					continue;

				// don't bother with myself and nodes we've already tried or queued
				if (ClientManager::getMyCID() == cid ||
					s->possibleNodes.find(distance) != s->possibleNodes.end() ||
					s->triedNodes.find(distance) != s->triedNodes.end())
				{
					continue;
				}

				// don't bother with private IPs
				if (!Utils::isGoodIPPort(address.to_uint(), u4))
					continue;

				// create unverified node
				// if this node already exists in our routing table, don't update it's ip/port for security reasons
				Node::Ptr newNode = DHT::getInstance()->createNode(cid, address, u4, false, false);

				// node won't be accept for several reasons (invalid IP etc.)
				// if routing table is full, node can be accept
				bool isAcceptable = DHT::getInstance()->addNode(newNode, false);
				if (isAcceptable)
				{
					// update our list of possible nodes
					s->possibleNodes[distance] = newNode;
				}
#ifdef DEBUG_DHT_SEARCH
				if (s->type == DHTSearch::TYPE_FILE)
				{
					if (isAcceptable)
						LogManager::message("DHT Search " + Util::toString(s->token) + " using node " +
							Util::toString(++s->nodeCtr) + ": " + newNode->getUser()->getCID().toBase32(), false);
					else
						LogManager::message("DHT Search " + Util::toString(s->token) + " unacceptable node: " +
							newNode->getUser()->getCID().toBase32(), false);
				}
#endif
				if (--n == 0) break;
			}

			xml.stepOut();
		}
		catch (const SimpleXMLException&)
		{
			// malformed node list
		}
		return true;
	}

	/*
	 * Sends publishing request
	 */
	void SearchManager::publishFile(const Node::Map& nodes, const string& tth, int64_t size, bool partial)
	{
		// send PUB command to K nodes
		int n = K;
		for (Node::Map::const_iterator i = nodes.begin(); i != nodes.end() && n > 0; i++, n--)
		{
			const Node::Ptr& node = i->second;

			AdcCommand cmd(AdcCommand::CMD_PUB, AdcCommand::TYPE_UDP);
			cmd.addParam("TR", tth);
			cmd.addParam("SI", Util::toString(size));

			if (partial)
				cmd.addParam("PF", "1");

			//i->second->setTimeout();
			DHT::getInstance()->send(cmd, node->getIdentity().getIp(),
				node->getIdentity().getUdpPort(), node->getUser()->getCID(), node->getUdpKey());
		}
	}

	/*
	 * Processes all running searches and removes long-time ones
	 */
	void SearchManager::processSearches()
	{
		uint64_t now = GET_TICK();
		CFlyLock(cs);

		SearchMap::iterator it = searches.begin();
		while (it != searches.end())
		{
			DHTSearch* s = it->second;

			// process active search
			s->process(now);

			// remove long search
			if (s->lifeTime < now)
			{
				// search timed out, stop it
				searches.erase(it++);

				if (s->type == DHTSearch::TYPE_STOREFILE)
				{
					publishFile(s->respondedNodes, s->term, s->filesize, s->partial);
				}
				s->onRemove();
				delete s;
			}
			else
				++it;
		}
	}

	/*
	 * Processes incoming search results
	 */
	bool SearchManager::processSearchResults(const UserPtr& user, size_t slots)
	{
		bool ok = false;
		uint64_t tick = GET_TICK();

		ResultsMap::iterator it = searchResults.begin();
		while (it != searchResults.end())
		{
			if (it->first == user->getCID())
			{
				// user is online, process his result
				SearchResult& sr = it->second.second;
				sr.slots = slots; // slot count should be known now

				::SearchManager::getInstance()->fire(SearchManagerListener::SR(), sr);
				searchResults.erase(it++);

				ok = true;
			}
			else if (it->second.first + 60*1000 <= tick)
			{
				// delete result from possibly offline users
				searchResults.erase(it++);
			}
			else
				++it;
		}

		return ok;
	}

	/*
	 * Checks whether we are alreading searching for a term
	 */
	bool SearchManager::isAlreadySearchingFor(const string& term)
	{
		CFlyLock(cs);
		for (SearchMap::const_iterator i = searches.begin(); i != searches.end(); i++)
		{
			if (i->second->term == term)
				return true;
		}

		return false;
	}

	void SearchManager::processQueue(uint64_t tick)
	{
		if (!searchQueue.hasQueuedItems()) return;
		Search si;
		if (!searchQueue.pop(si, tick)) return;

		DHTSearch* s = new DHTSearch;
		s->type = DHTSearch::TYPE_FILE;
		s->term = si.filter;
		s->token = si.token;

		search(s, tick);
	}

}
