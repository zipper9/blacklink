/*
 * Copyright (C) 2008-2011 Big Muscle, http://strongdc.sf.net
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

#include "DHT.h"
#include "IndexManager.h"
#include "DHTSearchManager.h"

#include "../CID.h"
#include "../LogManager.h"
#include "../ShareManager.h"
#include "../TimerManager.h"

namespace dht
{

	IndexManager::IndexManager() : publish(false), publishing(0), nextRepublishTime(GET_TICK())
	{
	}

	/*
	 * Add new source to tth list
	 */
	void IndexManager::addSource(const TTHValue& tth, const Node::Ptr& node, uint64_t size, bool partial)
	{
		Source source;
		source.setCID(node->getUser()->getCID());
		source.setIp(node->getIdentity().getIP4());
		source.setUdpPort(node->getIdentity().getUdp4Port());
		source.setSize(size);
		source.setExpires(GET_TICK() + (partial ? PFS_REPUBLISH_TIME : REPUBLISH_TIME));
		source.setPartial(partial);

		LOCK(cs);

		TTHMap::iterator i = tthList.find(tth);
		if (i != tthList.end())
		{
			// no user duplicites
			SourceList& sources = i->second;
			for (SourceList::iterator s = sources.begin(); s != sources.end(); s++)
			{
				if (node->getUser()->getCID() == (*s).getCID())
				{
					// delete old item
					sources.erase(s);
					break;
				}
			}

			// old items in front, new items in back
			sources.push_back(source);

			// if maximum sources reached, remove the oldest one
			if (sources.size() > MAX_SEARCH_RESULTS)
				sources.pop_front();
		}
		else
		{
			// new file
			tthList.insert(std::make_pair(tth, SourceList(1, source)));
		}

		DHT::getInstance()->setDirty();
	}

	/*
	 * Finds TTH in known indexes and returns it
	 */
	bool IndexManager::findResult(const TTHValue& tth, SourceList& sources) const
	{
		// TODO: does file exist in my own sharelist?
		LOCK(cs);
		TTHMap::const_iterator i = tthList.find(tth);
		if (i != tthList.end())
		{
			sources = i->second;
			return true;
		}

		return false;
	}

	/*
	 * Try to publish next file in queue
	 */
	void IndexManager::publishNextFile()
	{
		File f;
		{
			LOCK(cs);

			if (publishQueue.empty() || publishing >= MAX_PUBLISHES_AT_TIME)
				return;

			incPublishing();

			f = publishQueue.front(); // get the first file in queue
			publishQueue.pop_front(); // and remove it from queue
		}
		SearchManager::getInstance()->findStore(f.tth.toBase32(), f.size, f.partial);
	}

	/*
	 * Loads existing indexes from disk
	 */
	void IndexManager::loadIndexes(SimpleXML& xml)
	{
		xml.resetCurrentChild();
		if (xml.findChild("Indexes"))
		{
			xml.stepIn();
			while (xml.findChild("Index"))
			{
				const TTHValue tth = TTHValue(xml.getChildAttrib("TTH"));
				SourceList sources;

				xml.stepIn();
				while (xml.findChild("Source"))
				{
					const string& ipStr = xml.getChildAttrib("I4");
					Ip4Address ip;
					if (!Util::parseIpAddress(ip, ipStr)) continue;

					Source source;
					source.setCID(CID(xml.getChildAttrib("CID")));
					source.setIp(ip);
					source.setUdpPort(static_cast<uint16_t>(xml.getIntChildAttrib("U4")));
					source.setSize(xml.getInt64ChildAttrib("SI"));
					source.setExpires(xml.getInt64ChildAttrib("EX"));
					source.setPartial(false);
					sources.push_back(source);
				}

				tthList.insert(std::make_pair(tth, sources));
				xml.stepOut();
			}
			xml.stepOut();
		}
	}

	/*
	 * Save all indexes to disk
	 */
	void IndexManager::saveIndexes(SimpleXML& xml)
	{
		xml.addTag("Files");
		xml.stepIn();

		LOCK(cs);
		for (TTHMap::const_iterator i = tthList.begin(); i != tthList.end(); i++)
		{
			xml.addTag("File");
			xml.addChildAttrib("TTH", i->first.toBase32());

			xml.stepIn();
			for (SourceList::const_iterator j = i->second.begin(); j != i->second.end(); j++)
			{
				const Source& source = *j;

				if (source.getPartial())
					continue;	// don't store partial sources

				xml.addTag("Source");
				xml.addChildAttrib("CID", source.getCID().toBase32());
				xml.addChildAttrib("I4", source.getIp());
				xml.addChildAttrib("U4", source.getUdpPort());
				xml.addChildAttrib("SI", source.getSize());
				xml.addChildAttrib("EX", source.getExpires());
			}
			xml.stepOut();
		}

		xml.stepOut();
	}

	/*
	 * Processes incoming request to publish file
	 */
	void IndexManager::processPublishSourceRequest(const Node::Ptr& node, const AdcCommand& cmd)
	{
		string tth;
		if (!cmd.getParam("TR", 1, tth))
			return;	// nothing to identify a file?

		string size;
		if (!cmd.getParam("SI", 1, size))
			return;	// no file size?

		int64_t intSize = Util::toInt64(size);
		if (intSize <= 0)
			return;

		string partial;
		cmd.getParam("PF", 1, partial);

		addSource(TTHValue(tth), node, intSize, partial == "1");

		// send response
		AdcCommand res(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, "File published", AdcCommand::TYPE_UDP);
		res.addParam("FC", "PUB");
		res.addParam("TR", tth);
		DHT::getInstance()->send(res, node->getIdentity().getIP4(),
			node->getIdentity().getUdp4Port(), node->getUser()->getCID(), node->getUdpKey());
	}

	/*
	 * Removes old sources
	 */
	void IndexManager::checkExpiration(uint64_t tick)
	{
		LOCK(cs);

		bool dirty = false;

		TTHMap::iterator i = tthList.begin();
		while (i != tthList.end())
		{
			SourceList::iterator j = i->second.begin();
			while (j != i->second.end())
			{
				const Source& source = *j;
				if (source.getExpires() <= tick)
				{
					dirty = true;
					j = i->second.erase(j);
				}
				else
					break;	// list is sorted, so finding first non-expired can stop iteration
			}

			if (i->second.empty())
				tthList.erase(i++);
			else
				++i;
		}

		if (dirty)
			DHT::getInstance()->setDirty();
	}

	/** Publishes shared file */
	void IndexManager::publishFile(const TTHValue& tth, int64_t size)
	{
		if (size > MIN_PUBLISH_FILESIZE)
		{
			LOCK(cs);
			publishQueue.push_back(File(tth, size, false));
		}
	}

	/*
	 * Publishes partially downloaded file
	 */
	void IndexManager::publishPartialFile(const TTHValue& tth)
	{
		LOCK(cs);
		publishQueue.push_front(File(tth, 0, true));
	}


} // namespace dht
