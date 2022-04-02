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
#include "../HttpClient.h"
#include "../ResourceManager.h"
#include <zlib.h>

namespace dht
{

	BootstrapManager::BootstrapManager() : downloadRequest(0), hasListener(false)
	{
	}

	BootstrapManager::~BootstrapManager()
	{
		if (hasListener)
			httpClient.removeListener(this);
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

		HttpClient::Request req;
		req.url = url;
		req.userAgent = "StrongDC++ v2.43";
		req.maxRespBodySize = 1024 * 1024;
		req.maxRedirects = 5;
		uint64_t requestId = httpClient.addRequest(req);
		if (!requestId) return;
		
		csState.lock();
		if (downloadRequest)
		{
			csState.unlock();
			httpClient.cancelRequest(requestId);
			return;
		}

		if (BOOLSETTING(LOG_DHT_TRACE))
			LOG(DHT_TRACE, "Using bootstrap URL " + url);
		LogManager::message(STRING(DHT_BOOTSTRAPPING_STARTED));

		downloadRequest = requestId;
		hasListener = true;
		csState.unlock();

		httpClient.addListener(this);
		httpClient.startRequest(requestId);
	}

	void BootstrapManager::on(Failed, uint64_t id, const string& error) noexcept
	{
		csState.lock();
		bool failed = id == downloadRequest;
		if (failed)
		{
			downloadRequest = 0;
			hasListener = false;
		}
		csState.unlock();

		if (failed)
		{
			DHT::getInstance()->state = DHT::STATE_FAILED;
			LogManager::message(STRING_F(DHT_BOOTSTRAP_ERROR, error));
			httpClient.removeListener(this);
		}
	}

	void BootstrapManager::on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept
	{
		csState.lock();
		if (downloadRequest != id)
		{
			csState.unlock();
			return;
		}
		downloadRequest = 0;
		hasListener = false;
		csState.unlock();

		httpClient.removeListener(this);
		if (resp.getResponseCode() != 200)
		{
			DHT::getInstance()->state = DHT::STATE_FAILED;
			string error = Util::toString(resp.getResponseCode()) + ' ' + resp.getResponsePhrase();
			LogManager::message(STRING_F(DHT_BOOTSTRAP_ERROR, error));
			return;
		}

		const string& s = data.responseBody;
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

	void BootstrapManager::cleanup()
	{
		bool removeListener = false;
		csState.lock();
		downloadRequest = 0;
		removeListener = hasListener;
		hasListener = false;
		csState.unlock();
		if (removeListener) httpClient.removeListener(this);
	}

	bool BootstrapManager::hasBootstrapNodes() const
	{
		LOCK(csNodes);
		return !bootstrapNodes.empty();
	}

}
