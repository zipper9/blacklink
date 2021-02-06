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
#include "BootstrapManager.h"
#include "DHT.h"
#include "DHTSearchManager.h"
#include "../AdcCommand.h"
#include "../ClientManager.h"
#include "../LogManager.h"
#include "../HttpConnection.h"
#include "../ResourceManager.h"
#include <zlib.h>

namespace dht
{

	BootstrapManager::BootstrapManager() : conn(nullptr), downloading(false)
	{
	}

	BootstrapManager::~BootstrapManager()
	{
		if (conn)
		{
			conn->removeListeners();
			delete conn;
		}
	}

	void BootstrapManager::bootstrap()
	{
		if (hasBootstrapNodes())
			return;

#if 1
		const CID& cid = ClientManager::getMyCID();
#else
		const CID cid = CID::generate();
#endif
		string url = SETTING(URL_DHT_BOOTSTRAP);
		if (!Util::isHttpLink(url))
		{
			LogManager::message(STRING(DHT_INVALID_BOOTSTRAP_URL));
			return;
		}
		url += "?cid=" + cid.toBase32() + "&encryption=1";

		// store only active nodes to database
		//if (ClientManager::isActive(0))
		{
			int port = DHT::getPort();
			if (port)
				url += "&u4=" + Util::toString(port);
		}

		LOCK(csState);
		if (downloading) return;

		LogManager::message("Using bootstrap URL " + url, false);
		LogManager::message(STRING(DHT_BOOTSTRAPPING_STARTED));

		delete conn;
		conn = new HttpConnection(0);
		conn->addListener(this);
		conn->setMaxBodySize(1024*1024);
		conn->setUserAgent("StrongDC++ v2.43");
		conn->downloadFile(url);
		downloading = true;
		downloadBuf.clear();
		csState.unlock();
	}

	// HttpConnectionListener
	void BootstrapManager::on(Data, HttpConnection *conn, const uint8_t *buf, size_t len) noexcept
	{
		LOCK(csState);
		if (downloading)
			downloadBuf.append((const char *) buf, len);
	}


	void BootstrapManager::on(Failed, HttpConnection *conn, const string &line) noexcept
	{
		csState.lock();
		downloadBuf.clear();
		downloading = false;
		csState.unlock();

		DHT::getInstance()->state = DHT::STATE_FAILED;
		LogManager::message(STRING_F(DHT_BOOTSTRAP_ERROR, line));
	}

	void BootstrapManager::on(Complete, HttpConnection *conn, const string &) noexcept
	{
		csState.lock();
		if (!downloading)
		{
			csState.unlock();
			return;
		}
		string s = std::move(downloadBuf);
		downloadBuf.clear();
		downloading = false;
		csState.unlock();

		try
		{
			uLongf destLen = s.length();
			std::unique_ptr<uint8_t[]> destBuf;

			int result;
			do
			{
				destLen *= 2;
				destBuf.reset(new uint8_t[destLen]);
				result = uncompress(&destBuf[0], &destLen, (Bytef*) s.data(), s.length());
			}
			while (result == Z_BUF_ERROR);

			if (result != Z_OK)
				throw Exception("Decompress error.");

			SimpleXML remoteXml;
			remoteXml.fromXML(string((char*)&destBuf[0], destLen));
			remoteXml.stepIn();

			while (remoteXml.findChild("Node"))
			{
				CID cid = CID(remoteXml.getChildAttrib("CID"));
				string i4 = remoteXml.getChildAttrib("I4");
				int u4 = remoteXml.getIntChildAttrib("U4");

				addBootstrapNode(i4, static_cast<uint16_t>(u4), cid, UDPKey());
			}

			remoteXml.stepOut();
		}
		catch (Exception& e)
		{
			LogManager::message(STRING_F(DHT_BOOTSTRAP_ERROR, e.getError()));
		}
	}

	void BootstrapManager::addBootstrapNode(const string& ip, uint16_t udpPort, const CID& targetCID, const UDPKey& udpKey)
	{
		csNodes.lock();
		bootstrapNodes.emplace_back(BootstrapNode{ip, udpPort, targetCID, udpKey});
		csNodes.unlock();
	}

	void BootstrapManager::process()
	{
		csNodes.lock();
		if (bootstrapNodes.empty())
		{
			csNodes.unlock();
			return;
		}

		BootstrapNode node = bootstrapNodes.front();
		bootstrapNodes.pop_front();
		csNodes.unlock();

		Ip4Address address;
		if (!Util::parseIpAddress(address, node.ip) || !Util::isValidIp4(address))
			return;

		DHT::getInstance()->state = DHT::STATE_ACTIVE;

		// send bootstrap request
		AdcCommand cmd(AdcCommand::CMD_GET, AdcCommand::TYPE_UDP);
		cmd.addParam("nodes");
		cmd.addParam("dht.xml");

		CID key;
		// if our external IP changed from the last time, we can't encrypt packet with this key
		// this won't probably work now
		if (DHT::getInstance()->getLastExternalIP() == node.udpKey.ip)
			key = node.udpKey.key;

		DHT::getInstance()->send(cmd, address, node.udpPort, node.cid, key);
	}

	void BootstrapManager::cleanup(bool force)
	{
		LOCK(csState);
		if (conn && (force || !downloading))
		{
			conn->removeListeners();
			delete conn;
			conn = nullptr;
			downloading = false;
		}
	}

	bool BootstrapManager::hasBootstrapNodes() const
	{
		LOCK(csNodes);
		return !bootstrapNodes.empty();
	}

}
