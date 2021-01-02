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

#include "Constants.h"
#include "DHT.h"
#include "KBucket.h"
#include "BootstrapManager.h"
#include "Utils.h"

#include "../ClientManager.h"
#include "../SimpleXML.h"

namespace dht
{

	// Set all new nodes' type to 3 to avoid spreading dead nodes..
	Node::Node(const UserPtr& u) : OnlineUser(u, *DHT::getInstance(), 0),
		created(GET_TICK()), expires(0), type(3), ipVerified(false), online(false), isInList(false)
	{
	}

	CID Node::getUdpKey() const
	{
		// if our external IP changed from the last time, we can't encrypt packet with this key
		if (DHT::getInstance()->getLastExternalIP() == key.ip)
			return key.key;
		else
			return CID();
	}

	void Node::setUdpKey(const CID& udpKey)
	{
		// store key with our current IP address
		key.ip = DHT::getInstance()->getLastExternalIP();
		key.key = udpKey;
	}

	void Node::setAlive()
	{
		// long existing nodes will probably be there for another long time
		uint64_t now = GET_TICK();
		uint64_t hours = (now - created) / (60 * 60 * 1000);
		switch (hours)
		{
			case 0:
				type = 2;
				expires = now + NODE_EXPIRATION / 2;
				break;
			case 1:
				type = 1;
				expires = now + (uint64_t)(NODE_EXPIRATION / 1.5);
				break;
			default: // long existing nodes
				type = 0;
				expires = now + NODE_EXPIRATION;
		}
	}

	void Node::setTimeout(uint64_t now)
	{
		if (type == 4)
			return;

		type++;
		expires = now + NODE_RESPONSE_TIMEOUT;
	}


	KBucket::~KBucket()
	{
		// empty table
		for (auto it = nodes.begin(); it != nodes.end(); ++it)
		{
			Node::Ptr& node = *it;
			if (node->isOnline())
				ClientManager::getInstance()->putOffline(node);
		}

		nodes.clear();
	}

	/*
	 * Creates new (or update existing) node which is NOT added to our routing table
	 */
	Node::Ptr KBucket::createNode(const UserPtr& u, boost::asio::ip::address_v4 ip, uint16_t port, bool update, bool isUdpKeyValid)
	{
		if (u->getFlags() & User::DHT) // is this user already known in DHT?
		{
			Node::Ptr node;

			// no online node found, try get from routing table
			for (auto it = nodes.begin(); it != nodes.end(); ++it)
			{
				if (u->getCID() == (*it)->getUser()->getCID())
				{
					node = *it;

					// put node at the end of the list
					nodes.erase(it);
					nodes.push_back(node);
					break;
				}
			}

			if (!node && u->isOnline())
			{
				// try to get node from ClientManager (user can be online but not in our routing table)
				// this fixes the bug with DHT node online twice
				OnlineUserPtr ou = ClientManager::getInstance()->findDHTNode(u->getCID());
				if (ou)
					node = std::static_pointer_cast<Node>(ou);
			}

			if (node)
			{
				// fine, node found, update it and return it
				if (update)
				{
					boost::asio::ip::address_v4 oldIp = node->getIdentity().getIp();
					uint16_t oldPort = node->getIdentity().getUdpPort();
					if (ip != oldIp || oldPort != port)
					{
						node->setIpVerified(false);

						 // TODO: don't allow update when new IP already exists for different node

						// erase old IP and remember new one
						ipMap.erase(NodeAddress(oldIp, oldPort));
						ipMap.insert(NodeAddress(ip, port));
					}

					if (!node->isIpVerified())
						node->setIpVerified(isUdpKeyValid);

					node->setAlive();
					node->getIdentity().setIp(ip);
					node->getIdentity().setUdpPort(port);

					DHT::getInstance()->setDirty();
				}

				return node;
			}
		}

		u->setFlag(User::DHT);

		Node::Ptr node = std::make_shared<Node>(u);
		node->getIdentity().setIp(ip);
		node->getIdentity().setUdpPort(port);
		node->setIpVerified(isUdpKeyValid);
		return node;
	}

	/*
	 * Adds node to routing table
	 */
	bool KBucket::insert(const Node::Ptr& node)
	{
		if (node->isInList)
			return true;	// node is already in the table

		boost::asio::ip::address_v4 ip = node->getIdentity().getIp();
		uint16_t port = node->getIdentity().getUdpPort();
		NodeAddress na(ip, port);

		// allow only one same IP:port
		bool isAcceptable = ipMap.find(na) == ipMap.end();

		if ((nodes.size() < (K * ID_BITS)) && isAcceptable)
		{
			nodes.push_back(node);
			node->isInList = true;
			ipMap.insert(na);

			if (DHT::getInstance())
				DHT::getInstance()->setDirty();

			return true;
		}

		return isAcceptable;
	}

	/*
	 * Finds "max" closest nodes and stores them to the list
	 */
	void KBucket::getClosestNodes(const CID& cid, Node::Map& closest, unsigned int max, uint8_t maxType) const
	{
		for (NodeList::const_iterator it = nodes.begin(); it != nodes.end(); it++)
		{
			const Node::Ptr& node = *it;
			if (node->getType() <= maxType && node->isIpVerified() && !(node->getUser()->getFlags() & User::PASSIVE))
			{
				CID distance = Utils::getDistance(cid, node->getUser()->getCID());

				if (closest.size() < max)
				{
					// just insert
					closest.insert(std::make_pair(distance, node));
				}
				else
				{
					// not enough room, so insert only closer nodes
					if (distance < closest.rbegin()->first)	// "closest" is sorted map, so just compare with last node
					{
						closest.erase(closest.rbegin()->first);
						closest.insert(std::make_pair(distance, node));
					}
				}
			}
		}
	}

	/*
	 * Remove dead nodes
	 */
	bool KBucket::checkExpiration(uint64_t currentTime, OnlineUserList& removedList)
	{
		bool dirty = false;

		// we should ping oldest node from every bucket here
		// but since we have only one bucket now, simulate it by pinging more nodes
		unsigned pingCount = max(K, min(2 * K, (unsigned)(nodes.size() / (K * 10)) + 1)); // <-- pings 10 - 20 oldest nodes
		unsigned pinged = 0;
		dcdrun(unsigned removed = 0);

		// first, remove dead nodes
		auto i = nodes.begin();
		while (i != nodes.end())
		{
			Node::Ptr& node = *i;
			if (node->getType() == 4 && node->expires > 0 && node->expires <= currentTime)
			{
				// node is dead, remove it
				boost::asio::ip::address_v4 ip = node->getIdentity().getIp();
				uint16_t port = node->getIdentity().getUdpPort();
				ipMap.erase(NodeAddress(ip, port));
				if (node->isOnline())
					removedList.push_back(node);

				i = nodes.erase(i);
				dirty = true;
				dcdrun(removed++);
				continue;
			}

			if (node->expires == 0)
				node->expires = currentTime;

			// select the oldest expired node
			if (pinged < pingCount && node->getType() < 4 && node->expires <= currentTime)
			{
				// ping the oldest (expired) node
				node->setTimeout(currentTime);
				DHT::getInstance()->info(node->getIdentity().getIp(), node->getIdentity().getUdpPort(), DHT::PING, node->getUser()->getCID(), node->getUdpKey());
				pinged++;
			}

			++i;
		}

#ifndef NDEBUG
		int verified = 0; int types[5] = { 0 };
		for (NodeList::const_iterator j = nodes.begin(); j != nodes.end(); j++)
		{
			Node::Ptr n = *j;
			if (n->isIpVerified()) verified++;

			dcassert(n->getType() >= 0 && n->getType() <= 4);
			types[n->getType()]++;
		}

		dcdebug("DHT Nodes: %d (%d verified), Types: %d/%d/%d/%d/%d, pinged %d of %d, removed %d\n", nodes.size(), verified, types[0], types[1], types[2], types[3], types[4], pinged, pingCount, removed);
#endif

		return dirty;
	}

	/*
	 * Loads existing nodes from disk
	 */
	void KBucket::loadNodes(SimpleXML& xml)
	{
		xml.resetCurrentChild();
		if (xml.findChild("Nodes"))
		{
			xml.stepIn();
			while (xml.findChild("Node"))
			{
				CID cid = CID(xml.getChildAttrib("CID"));
				string i4 = xml.getChildAttrib("I4");
				uint16_t u4 = static_cast<uint16_t>(xml.getIntChildAttrib("U4"));

				boost::system::error_code ec;
				boost::asio::ip::address_v4 address = boost::asio::ip::address_v4::from_string(i4, ec);
				if (ec)
					continue;

				if (!Utils::isGoodIPPort(address.to_uint(), u4))
					continue;

				UDPKey udpKey;
				string key = xml.getChildAttrib("key");
				string keyIp = xml.getChildAttrib("keyIP");

				if (!key.empty() && !keyIp.empty())
				{
					udpKey.key = CID(key);
					udpKey.ip = keyIp;
				}

				//addUser(cid, i4, u4);
				BootstrapManager::getInstance()->addBootstrapNode(i4, u4, cid, udpKey);
			}
			xml.stepOut();
		}
	}

	/*
	 * Save bootstrap nodes to disk
	 */
	void KBucket::saveNodes(SimpleXML& xml)
	{
		xml.addTag("Nodes");
		xml.stepIn();

		// get 50 random nodes to bootstrap from them next time
		Node::Map closestToMe;
		getClosestNodes(CID::generate(), closestToMe, 50, 3);

		for (Node::Map::const_iterator j = closestToMe.begin(); j != closestToMe.end(); j++)
		{
			const Node::Ptr& node = j->second;

			xml.addTag("Node");
			xml.addChildAttrib("CID", node->getUser()->getCID().toBase32());
			xml.addChildAttrib("type", node->getType());
			xml.addChildAttrib("verified", node->isIpVerified());

			if (!node->getUDPKey().key.isZero() && !node->getUDPKey().ip.empty())
			{
				xml.addChildAttrib("key", node->getUDPKey().key.toBase32());
				xml.addChildAttrib("keyIP", node->getUDPKey().ip);
			}

			StringMap params;
			node->getIdentity().getParams(params, Util::emptyString, false, true);

			for (StringMap::const_iterator i = params.begin(); i != params.end(); i++)
				xml.addChildAttrib(i->first, i->second);
		}

		xml.stepOut();
	}

}
