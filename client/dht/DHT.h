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

#ifndef _DHT_H
#define _DHT_H

#include "Constants.h"
#include "KBucket.h"

#include "../AdcCommand.h"
#include "../CID.h"
#include "../MerkleTree.h"

namespace dht
{

	class BootstrapManager;

	class DHT : public Speaker<ClientListener>, public ClientBase
	{
	public:
		DHT();
		~DHT();

		enum InfType
		{
			NONE = 0,
			PING = 1,
			MAKE_ONLINE = 2
		};

		enum
		{
			STATE_IDLE,
			STATE_INITIALIZING,
			STATE_BOOTSTRAP,
			STATE_ACTIVE,
			STATE_STOPPING,
			STATE_FAILED
		};

		static ClientBasePtr getClientBaseInstance();
		static DHT* getInstance();
		static void deleteInstance();
		static void newInstance();

		/** ClientBase derived functions */
		const string& getHubUrl() const { return NetworkName; }
		string getHubName() const { return NetworkName; }
		string getMyNick() const { return SETTING(NICK); }
		bool isOp() const { return false; }
		bool resendMyINFO(bool alwaysSend, bool forcePassive) { return false; }
		int getType() const { return ClientBase::TYPE_DHT; }
		void dumpUserInfo(const string& userReport);

		/** Starts DHT. */
		void start();
		void stop(bool exiting = false);

		static uint16_t getPort();

		/** Sends command to ip and port */
		void send(AdcCommand& cmd, Ip4Address address, uint16_t port, const CID& targetCID, const CID& udpKey);

		/** Process incoming packet */
		bool processIncoming(const uint8_t* data, size_t size, Ip4Address address, uint16_t remotePort);

		/** Creates new (or update existing) node which is NOT added to our routing table */
		Node::Ptr createNode(const CID& cid, Ip4Address ip, uint16_t port, bool update, bool isUdpKeyValid);

		/** Adds node to routing table */
		bool addNode(const Node::Ptr& node, bool makeOnline);

		/** Returns counts of nodes available in k-buckets */
		size_t getNodesCount() const;

		/** Removes dead nodes */
		void checkExpiration(uint64_t aTick);

		/** Starts TTH search, returns time to wait in the queue */
		unsigned findFile(const string& tth, uint32_t token, void* owner);

		/** Sends our info to specified ip:port */
		void info(Ip4Address ip, uint16_t port, uint32_t type, const CID& targetCID, const CID& udpKey);

		/** Sends Connect To Me request to online node */
		void connect(const OnlineUserPtr& ou, const string& token, bool forcePassive);

		/** Sends private message to online node */
		void privateMessage(const OnlineUserPtr& ou, const string& message, bool thirdPerson);

		/** Is DHT connected? */
		bool isConnected() const;

		/** Mark that network data should be saved */
		void setDirty() { dirty = true; }

		/** Saves network information to XML file */
		void saveData();

		/** Returns if our UDP port is open */
		bool isFirewalled() const;

		/** Returns our IP got from the last firewall check */
		string getLastExternalIP() const;

		void setRequestFWCheck();

		void getPublicIPInfo(string& externalIP, bool &isFirewalled) const;

		/** Generates UDP key for specified IP address */
		CID getUdpKey(const string& targetIp) const;

		int getState() const { return state; }
		bool pingNode(const CID& cid);
		void updateLocalIP(const IpAddress& localIP);

		class LockInstanceNodes
		{
			public:
				LockInstanceNodes(const DHT* instance) : instance(instance)
				{
					instance->cs.lock();
				}
				~LockInstanceNodes()
				{
					instance->cs.unlock();
				}
				LockInstanceNodes(const LockInstanceNodes&) = delete;
				LockInstanceNodes& operator= (const LockInstanceNodes&) = delete;
				const KBucket::NodeList* getNodes() const
				{
					if (!instance->bucket) return nullptr;
					return &instance->bucket->getNodes();
				}

			private:
				const DHT* const instance;
		};

	private:
		/** Classes that can access to my private members */
		friend class Singleton<DHT>;
		friend class SearchManager;
		friend class BootstrapManager;
		friend class TaskManager;

		bool handle(AdcCommand::INF, const Node::Ptr& node, AdcCommand& c) noexcept; // user's info
		bool handle(AdcCommand::SCH, const Node::Ptr& node, AdcCommand& c) noexcept; // incoming search request
		bool handle(AdcCommand::RES, const Node::Ptr& node, AdcCommand& c) noexcept; // incoming search result
		bool handle(AdcCommand::PUB, const Node::Ptr& node, AdcCommand& c) noexcept; // incoming publish request
		bool handle(AdcCommand::CTM, const Node::Ptr& node, AdcCommand& c) noexcept; // connection request
		bool handle(AdcCommand::RCM, const Node::Ptr& node, AdcCommand& c) noexcept; // reverse connection request
		bool handle(AdcCommand::STA, const Node::Ptr& node, AdcCommand& c) noexcept; // status message
		bool handle(AdcCommand::PSR, const Node::Ptr& node, AdcCommand& c) noexcept; // partial file request
		bool handle(AdcCommand::MSG, const Node::Ptr& node, AdcCommand& c) noexcept; // private message
		bool handle(AdcCommand::GET, const Node::Ptr& node, AdcCommand& c) noexcept;
		bool handle(AdcCommand::SND, const Node::Ptr& node, AdcCommand& c) noexcept;

		/** Unsupported command */
		template<typename T> bool handle(T, const Node::Ptr&user, AdcCommand&) { return false; }

		/** Process incoming command */
		bool dispatch(const string& line, Ip4Address address, uint16_t port, bool isUdpKeyValid);

		void handleFwCheckResponse(const AdcCommand& c, Ip4Address fromIP, string& newExternalIP);

		std::atomic<int> state;

		CID myUdpKey;

		/** Routing table */
		KBucket* bucket;

		/** Lock to routing table */
		mutable CriticalSection cs;
		mutable CriticalSection fwCheckCs;

		/** Our external IP got from last firewalled check */
		Ip4Address lastExternalIP;

		/** Time when last packet was received */
		uint64_t lastPacket;

		/** IPs who we received firewalled status from */
		std::unordered_set<uint32_t> firewalledWanted;
		std::unordered_map<uint32_t, std::pair<uint32_t, uint16_t>> firewalledChecks;
		bool firewalled;
		bool requestFWCheck;

		/** Should the network data be saved? */
		bool dirty;

		/** Finds "max" closest nodes and stores them to the list */
		void getClosestNodes(const CID& cid, std::map<CID, Node::Ptr>& closest, unsigned int max, uint8_t maxType);

		/** Loads network information from XML file */
		void loadData();

		void setExternalIP(const IpAddress& ip);

		static ClientBasePtr instance;
	};

}

#endif	// _DHT_H
