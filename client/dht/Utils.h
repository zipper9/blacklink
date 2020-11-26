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

#ifndef _UTILS_H
#define _UTILS_H

#include <list>

#include "NodeAddress.h"
#include "../AdcCommand.h"
#include "../CID.h"
#include "../MerkleTree.h"
#include "../Thread.h"

namespace dht
{

	// all members must be static!
	class Utils
	{
	public:
		/** Returns distance between two nodes */
		static CID getDistance(const CID& cid1, const CID& cid2);

		/** Returns distance between node and file */
		static TTHValue getDistance(const CID& cid, const TTHValue& tth)
		{
			return TTHValue(const_cast<uint8_t*>(getDistance(cid, CID(tth.data)).data()));
		}

		/** Detect whether it is correct to use IP:port in DHT network */
		static bool isGoodIPPort(uint32_t ip, uint16_t port);

		/** General flooding protection */
		static bool checkFlood(uint32_t ip, const AdcCommand& cmd);

		/** Removes tracked packets. Called once a minute. */
		static void cleanFlood();

		/** Stores outgoing request to avoid receiving invalid responses */
		static void trackOutgoingPacket(uint32_t ip, const AdcCommand& cmd);

		static const string& compressXML(string& xml);

	private:
		Utils() {}

		struct OutPacket
		{
			uint32_t ip;
			uint64_t time;
			uint32_t cmd;
		};

		static CriticalSection cs;
		static std::unordered_map<uint32_t, std::unordered_multiset<uint32_t>> receivedPackets;
		static std::list<OutPacket> sentPackets;
	};

} // namespace dht

#endif	// _UTILS_H
