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
#include "../SettingsManager.h"
#include "../AdcSupports.h"
#include "../ConfCore.h"

namespace dht
{

	/*
	 * Sends Connect To Me request to online node
	 */
	void ConnectionManager::connect(const Node::Ptr& node, const string& token)
	{
		bool useTLS = CryptoManager::getInstance()->isInitialized() &&
			(node->getUser()->getFlags() & User::TLS) != 0;
		if (useTLS && node->getIdentity().getStringParam("KP").empty())
		{
			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockRead();
			if (!ss->getBool(Conf::ALLOW_UNTRUSTED_CLIENTS))
				useTLS = false;
			ss->unlockRead();
		}
		connect(node, token, useTLS, false);
	}

	void ConnectionManager::connect(const Node::Ptr& node, const string& token, bool secure, bool revConnect)
	{
		// don't allow connection if we didn't proceed a handshake
		if (!node->isOnline())
		{
			// do handshake at first
			DHT::getInstance()->info(node->getIdentity().getIP4(), node->getIdentity().getUdp4Port(),
				DHT::PING | DHT::MAKE_ONLINE, node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		bool active = ClientManager::isActiveMode(AF_INET, 0, false);

		// if I am not active, send reverse connect to me request
		AdcCommand cmd(active ? AdcCommand::CMD_CTM : AdcCommand::CMD_RCM, AdcCommand::TYPE_UDP);
		cmd.addParam(secure ? AdcSupports::SECURE_CLIENT_PROTOCOL : AdcSupports::CLIENT_PROTOCOL);

		if (active)
		{
			uint16_t port = ::ConnectionManager::getInstance()->getConnectionPort(AF_INET, secure);
			cmd.addParam(Util::toString(port));
			uint64_t expires = revConnect ? GET_TICK() + 60000 : UINT64_MAX;
			::ConnectionManager::getInstance()->adcExpect(token, node->getUser()->getCID(), NetworkName, expires);
		}

		cmd.addParam(token);

		DHT::getInstance()->send(cmd, node->getIdentity().getIP4(), node->getIdentity().getUdp4Port(),
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
			DHT::getInstance()->info(node->getIdentity().getIP4(), node->getIdentity().getUdp4Port(),
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
		else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL && CryptoManager::getInstance()->isInitialized())
		{
			secure = true;
		}
		else
		{
			AdcCommand response(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_UDP);
			response.addParam(TAG('P', 'R'), protocol);
			response.addParam(TAG('T', 'O'), token);

			DHT::getInstance()->send(response, node->getIdentity().getIP4(), node->getIdentity().getUdp4Port(),
				node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		if (!node->getIdentity().isTcpActive())
		{
			AdcCommand err(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "IP unknown", AdcCommand::TYPE_UDP);
			DHT::getInstance()->send(err, node->getIdentity().getIP4(), node->getIdentity().getUdp4Port(),
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
		if (!ClientManager::isActiveMode(AF_INET, 0, false))
			return;

		const string& protocol = cmd.getParam(1);
		const string& token = cmd.getParam(2);

		bool secure;
		if (protocol == AdcSupports::CLIENT_PROTOCOL)
		{
			secure = false;
		}
		else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL && CryptoManager::getInstance()->isInitialized())
		{
			secure = true;
		}
		else
		{
			AdcCommand sta(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_UDP);
			sta.addParam(TAG('P', 'R'), protocol);
			sta.addParam(TAG('T', 'O'), token);

			DHT::getInstance()->send(sta, node->getIdentity().getIP4(), node->getIdentity().getUdp4Port(),
				node->getUser()->getCID(), node->getUdpKey());
			return;
		}

		connect(node, token, secure, true);
	}

}
