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
#include "Utils.h"

#include "Constants.h"

#include "../AdcCommand.h"
#include "../CID.h"
#include "../MerkleTree.h"
#include "../TimerManager.h"
#include "../SettingsManager.h"
#include "../LogManager.h"
#include "../NetworkUtil.h"

namespace dht
{

	CriticalSection Utils::cs;
	std::unordered_map<uint32_t, std::unordered_multiset<uint32_t>> Utils::receivedPackets;
	std::list<Utils::OutPacket> Utils::sentPackets;

	CID Utils::getDistance(const CID& cid1, const CID& cid2)
	{
		static_assert(CID::SIZE % sizeof(size_t) == 0, "CID::SIZE is not multiple of sizeof(size_t)");
		union
		{
			uint8_t b[CID::SIZE];
			size_t  w[CID::SIZE/sizeof(size_t)];
		} distance;

		for (size_t i = 0; i < CID::SIZE/sizeof(size_t); i++)
			distance.w[i] = cid1.dataW()[i] ^ cid2.dataW()[i];

		return CID(distance.b);
	}

	/*
	 * Detect whether it is correct to use IP:port in DHT network
	 */
	bool Utils::isGoodIPPort(uint32_t ip, uint16_t port)
	{
		// don't allow empty IP and port lower than 1024
		// ports below 1024 are known service ports, so they shouldn't be used for DHT else it could be used for attacks
		if (ip == 0 || ip == 0xFFFFFFFF || port < 1024)
			return false;

		// don't allow private IPs
		if (Util::isPrivateIp(ip))
			return false;

		return true;
	}

	/*
	 * General flooding protection
	 */
	bool Utils::checkFlood(uint32_t ip, const AdcCommand& cmd)
	{
		// ignore empty commands
		if (cmd.getParameters().empty())
			return false;

		// there maximum allowed request packets from one IP per minute
		// response packets are allowed only if request has been sent to the IP address
		size_t maxAllowedPacketsPerMinute = 0;
		uint32_t requestCmd = AdcCommand::CMD_SCH;
		switch (cmd.getCommand())
		{
			// request packets
			case AdcCommand::CMD_SCH: maxAllowedPacketsPerMinute = 20; break;
			case AdcCommand::CMD_PUB: maxAllowedPacketsPerMinute = 10; break;
			case AdcCommand::CMD_INF: maxAllowedPacketsPerMinute = 3; break;
			case AdcCommand::CMD_CTM: maxAllowedPacketsPerMinute = 2; break;
			case AdcCommand::CMD_RCM: maxAllowedPacketsPerMinute = 2; break;
			case AdcCommand::CMD_GET: maxAllowedPacketsPerMinute = 2; break;
			case AdcCommand::CMD_PSR: maxAllowedPacketsPerMinute = 3; break;

			// response packets
			case AdcCommand::CMD_STA:
				return true; // STA can be response for more commands, but since it is for informative purposes only, there shouldn't be no way to abuse it

			case AdcCommand::CMD_SND: requestCmd = AdcCommand::CMD_GET;
			case AdcCommand::CMD_RES: // default value of requestCmd
			{
				LOCK(cs);
				for (std::list<OutPacket>::iterator i = sentPackets.begin(); i != sentPackets.end(); i++)
				{
					if (i->cmd == requestCmd && i->ip == ip)
					{
						sentPackets.erase(i);
						return true;
					}
				}

				LogManager::message("Received unwanted response from " + boost::asio::ip::make_address_v4(ip).to_string() + ". Packet dropped.", false);
				return false;
			}
		}

		LOCK(cs);
		std::unordered_multiset<uint32_t>& packetsPerIp = receivedPackets[ip];
		packetsPerIp.insert(cmd.getCommand());

		if (packetsPerIp.count(cmd.getCommand()) > maxAllowedPacketsPerMinute)
		{
			LogManager::message("Request flood detected (" + Util::toString(packetsPerIp.count(cmd.getCommand())) +
				") from " + boost::asio::ip::make_address_v4(ip).to_string() + ". Packet dropped.", false);
			return false;
		}

		return true;
	}

	/*
	 * Removes tracked packets. Called once a minute.
	 */
	void Utils::cleanFlood()
	{
		LOCK(cs);
		receivedPackets.clear();
	}

	/*
	 * Stores outgoing request to avoid receiving invalid responses
	 */
	void Utils::trackOutgoingPacket(uint32_t ip, const AdcCommand& cmd)
	{
		LOCK(cs);

		uint64_t now = GET_TICK();
		switch (cmd.getCommand())
		{
			// request packets
			case AdcCommand::CMD_SCH:
			case AdcCommand::CMD_PUB:
			case AdcCommand::CMD_INF:
			case AdcCommand::CMD_CTM:
			case AdcCommand::CMD_GET:
			case AdcCommand::CMD_PSR:
				sentPackets.emplace_back(OutPacket{ip, now, cmd.getCommand()});
				break;
		}

		// clean up old items
		// list is sorted by time, so the first unmatched item can break the loop
		while (!sentPackets.empty())
		{
			uint64_t diff = now - sentPackets.front().time;
			if (diff >= TIME_FOR_RESPONSE)
				sentPackets.pop_front();
			else
				break;
		}
	}

	const string& Utils::compressXML(string& xml)
	{
		xml.erase(std::remove_if(xml.begin(), xml.end(),
			[](char ch){ return ch == '\r' || ch == '\n' || ch == '\t'; }), xml.end());
		return xml;
	}

} // namespace dht
