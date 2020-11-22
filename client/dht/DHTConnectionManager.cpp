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
#include "DHTConnectionManager.h"
#include "DHT.h"

#include "../ClientManager.h"
#include "../ConnectionManager.h"
#include "../CryptoManager.h"

namespace dht
{

	/*
	 * Sends Connect To Me request to online node
	 */
	void ConnectionManager::connect(const Node::Ptr& node, const string& token)
	{
		connect(node, token, CryptoManager::getInstance()->TLSOk() && (node->getUser()->getFlags() & User::TLS) != 0);
	}

	void ConnectionManager::connect(const Node::Ptr& node, const string& token, bool secure)
	{
		// don't allow connection if we didn't proceed a handshake
		if (!node->isOnline())
		{
			// do handshake at first
			DHT::getInstance()->info(node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
				DHT::PING | DHT::MAKE_ONLINE, node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		bool active = ClientManager::isActive(0);

		// if I am not active, send reverse connect to me request
		AdcCommand cmd(active ? AdcCommand::CMD_CTM : AdcCommand::CMD_RCM, AdcCommand::TYPE_UDP);
		cmd.addParam(secure ? AdcSupports::SECURE_CLIENT_PROTOCOL_TEST : AdcSupports::CLIENT_PROTOCOL);

		if (active)
		{
			uint16_t port = secure ? ::ConnectionManager::getInstance()->getSecurePort() : ::ConnectionManager::getInstance()->getPort();
			cmd.addParam(Util::toString(port));
		}

		cmd.addParam(token);

		DHT::getInstance()->send(cmd, node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
			node->getUser()->getCID(), node->getUdpKey());
	}

	/*
	 * Creates connection to specified node
	 */
	void ConnectionManager::connectToMe(const Node::Ptr& node, const AdcCommand& cmd)
	{
		// don't allow connection if we didn't proceed a handshake
		if (!node->isOnline())
		{
			// do handshake at first
			DHT::getInstance()->info(node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
				DHT::PING | DHT::MAKE_ONLINE, node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		const string& protocol = cmd.getParam(1);
		const string& port = cmd.getParam(2);
		const string& token = cmd.getParam(3);

		bool secure = false;
		if (protocol == AdcSupports::CLIENT_PROTOCOL)
		{
			// Nothing special
		}
		else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->TLSOk())
		{
			secure = true;
		}
		else
		{
			AdcCommand response(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_UDP);
			response.addParam("PR", protocol);
			response.addParam("TO", token);

			DHT::getInstance()->send(response, node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
				node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		if (!node->getIdentity().isTcpActive())
		{
			AdcCommand err(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "IP unknown", AdcCommand::TYPE_UDP);
			DHT::getInstance()->send(err, node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
				node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		::ConnectionManager::getInstance()->adcConnect(*node, static_cast<uint16_t>(Util::toInt(port)), token, secure);
	}

	/*
	 * Sends request to create connection with me
	 */
	void ConnectionManager::revConnectToMe(const Node::Ptr& node, const AdcCommand& cmd)
	{
		// don't allow connection if we didn't proceed a handshake
		//if (!node->isOnline())
		//	return;

		// this is valid for active-passive connections only
		if (!ClientManager::isActive(0))
			return;

		const string& protocol = cmd.getParam(1);
		const string& token = cmd.getParam(2);

		bool secure;
		if (protocol == AdcSupports::CLIENT_PROTOCOL)
		{
			secure = false;
		}
		else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->TLSOk())
		{
			secure = true;
		}
		else
		{
			AdcCommand sta(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_UDP);
			sta.addParam("PR", protocol);
			sta.addParam("TO", token);

			DHT::getInstance()->send(sta, node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
				node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		connect(node, token, secure);
	}

}
