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

#include "DHT.h"
#include "BootstrapManager.h"
#include "DHTConnectionManager.h"
#include "DHTSearchManager.h"
#include "IndexManager.h"
#include "TaskManager.h"
#include "Utils.h"

#include "../AdcCommand.h"
#include "../AdcSupports.h"
#include "../ChatMessage.h"
#include "../CID.h"
#include "../ClientManager.h"
#include "../CryptoManager.h"
#include "../LogManager.h"
#include "../SettingsManager.h"
#include "../ShareManager.h"
#include "../SearchManager.h"
#include "../UploadManager.h"
#include "../ThrottleManager.h"
#include "../FavoriteManager.h"
#include "../TimerManager.h"
#include "../User.h"
#include "../version.h"

#include <zlib.h>
#include <openssl/rc4.h>

namespace dht
{

	static const string DHT_FILE = "DHT.xml";
	static const size_t PACKET_BUF_SIZE = 16384;

	static const uint8_t MAGICVALUE_UDP = 0x5B;
	static const uint8_t ADC_PACKET_HEADER = 'U';   // byte which every uncompressed packet must begin with
	static const uint8_t ADC_PACKET_FOOTER = 0x0A;  // byte which every uncompressed packet must end with
	static const uint8_t ADC_COMPRESSED_PACKET_HEADER = 0xC1; // compressed packet detection byte

	DHT::DHT() : bucket(nullptr), lastPacket(0), firewalled(true), requestFWCheck(true), dirty(false), state(STATE_IDLE), lastExternalIP(0)
	{
		IndexManager::newInstance();
	}

	DHT::~DHT()
	{
		// when DHT is disabled, we shouldn't try to perform exit cleanup
		if (bucket == nullptr)
		{
			IndexManager::deleteInstance();
			return;
		}

		stop(true);
		IndexManager::deleteInstance();
	}

	/*
	 * Starts DHT.
	 */
	void DHT::start()
	{
		if (!BOOLSETTING(USE_DHT) || !(state == STATE_IDLE || state == STATE_FAILED))
			return;

		state = STATE_INITIALIZING;
		myUdpKey = CID(SETTING(DHT_KEY));

		// start with global firewalled status
		firewalled = !ClientManager::isActive(AF_INET, 0);
		requestFWCheck = true;

		if (!bucket)
		{
#if 0
			if (BOOLSETTING(UPDATE_IP))
				SettingsManager::getInstance()->set(SettingsManager::EXTERNAL_IP, Util::emptyString);
#endif

			bucket = new KBucket();

			BootstrapManager::newInstance();
			SearchManager::newInstance();
			TaskManager::newInstance();
			ConnectionManager::newInstance();

			loadData();
		}
	}

	void DHT::stop(bool exiting)
	{
		if (!bucket)
			return;

		state = STATE_STOPPING;
		if (BootstrapManager::isValidInstance())
			BootstrapManager::getInstance()->cleanup(true);

		if (!BOOLSETTING(USE_DHT) || exiting)
		{
			saveData();

			lastPacket = 0;

			ConnectionManager::deleteInstance();
			TaskManager::deleteInstance();
			SearchManager::deleteInstance();
			BootstrapManager::deleteInstance();

			delete bucket;
			bucket = nullptr;
		}
		state = STATE_IDLE;
	}

	/*
	 * Process incoming command
	 */
	bool DHT::dispatch(const string& line, Ip4Address address, uint16_t port, bool isUdpKeyValid)
	{
		// check node's IP address
		if (!Utils::isGoodIPPort(address, port))
		{
			//socket.send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_BAD_IP, "Your client supplied invalid IP: " + ip, AdcCommand::TYPE_UDP), ip, port);
			return false; // invalid ip/port supplied
		}

		AdcCommand cmd(0);
		if (cmd.parse(line))
		{
			dcdebug("Invalid ADC command: %s\n", line.c_str());
			return false;
		}

		// flood protection
		if (!Utils::checkFlood(address, cmd))
			return false;

		string cid = cmd.getParam(0);
		if (cid.size() != 39)
			return false;

		// ignore message from myself
		if (CID(cid) == ClientManager::getMyCID() || address == lastExternalIP)
			return false;

		lastPacket = GET_TICK();

		// don't add node here, because it may block verified nodes when table becomes full of unverified nodes
		Node::Ptr node = createNode(CID(cid), address, port, true, isUdpKeyValid);
		const UserPtr& user = node->getUser();

		// all communication to this node will be encrypted with this key
		string udpKey;
		if (cmd.getParam("UK", 1, udpKey))
			node->setUdpKey(CID(udpKey));

		// node is requiring FW check
		string internalUdpPort;
		if (cmd.getParam("FW", 1, internalUdpPort))
		{
			bool firewalled = Util::toInt(internalUdpPort) != port;
			if (firewalled)
				user->setFlag(User::PASSIVE | User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE_BIT);

			// send him his external ip and port
			AdcCommand cmd(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, !firewalled ? "UDP port opened" : "UDP port closed", AdcCommand::TYPE_UDP);
			cmd.addParam("FC", "FWCHECK");
			cmd.addParam("I4", Util::printIpAddress(address));
			cmd.addParam("U4", Util::toString(port));
			send(cmd, address, port, user->getCID(), node->getUdpKey());
		}

#define C(n) case AdcCommand::CMD_##n: return handle(AdcCommand::n(), node, cmd);
		switch (cmd.getCommand())
		{
			C(INF);	// user's info
			C(SCH);	// search request
			C(RES);	// response to SCH
			C(PUB);	// request to publish file
			C(CTM); // connection request
			C(RCM); // reverse connection request
			C(STA);	// status message
			C(PSR);	// partial file request
			C(MSG);	// private message
			C(GET); // get some data
			C(SND); // response to GET

		default:
			dcdebug("Unknown ADC command: %s\n", line.c_str());
			return false;
#undef C
		}
	}

	/*
	 * Sends command to ip and port
	 */
	void DHT::send(AdcCommand& cmd, Ip4Address address, uint16_t port, const CID& targetCID, const CID& udpKey)
	{
		{
			// FW check
			LOCK(fwCheckCs);
			if (requestFWCheck && firewalledWanted.size() + firewalledChecks.size() < FW_RESPONSES)
			{
				if (firewalledWanted.find(address) == firewalledWanted.end()) // only when not requested from this node yet
				{
					firewalledWanted.insert(address);
					cmd.addParam("FW", Util::toString(getPort()));
				}
			}
		}
		Utils::trackOutgoingPacket(address, cmd);

		// pack data
		cmd.addParam("UK", getUdpKey(Util::printIpAddress(address)).toBase32()); // add our key for the IP address
		string command = cmd.toString(ClientManager::getMyCID());
		if (command.length() > PACKET_BUF_SIZE - 3)
		{
			LogManager::message("DHT: Outgoing command too large (size=" + Util::toString(command.length()) + ')');
			return;
		}

		if (BOOLSETTING(LOG_UDP_PACKETS))
			LogManager::commandTrace(command, LogManager::FLAG_UDP, Util::printIpAddress(address), port);

		if (CMD_DEBUG_ENABLED())
			COMMAND_DEBUG(command, DebugTask::HUB_OUT, Util::printIpAddress(address) + ':' + Util::toString(port));

		size_t destLen;
		uint8_t buf[PACKET_BUF_SIZE];
		uLongf bufSize = PACKET_BUF_SIZE - 3;
		uint8_t* destBuf = buf + 3;
		int result = compress2(destBuf, &bufSize, (const Bytef*) command.data(), command.length(), Z_BEST_COMPRESSION);
		if (result == Z_OK && bufSize < command.length())
		{
			destBuf--;
			*destBuf = ADC_COMPRESSED_PACKET_HEADER;
			destLen = bufSize + 1;
		}
		else
		{
			// compression failed, send uncompressed packet
			destLen = command.length();
			memcpy(destBuf, command.data(), command.length());
		}

		if (!udpKey.isZero())
		{
			// generate encryption key
			TigerHash th;
			th.update(udpKey.data(), CID::SIZE);
			th.update(targetCID.data(), CID::SIZE);

			RC4_KEY sentKey;
			RC4_set_key(&sentKey, TigerTree::BYTES, th.finalize());

			destBuf -= 2;

			// some random character except of ADC_PACKET_HEADER or ADC_COMPRESSED_PACKET_HEADER
			uint8_t randomByte = Util::rand() & 0xFF;
			destBuf[0] = (randomByte == ADC_PACKET_HEADER || randomByte == ADC_COMPRESSED_PACKET_HEADER) ? (randomByte + 1) : randomByte;
			destBuf[1] = MAGICVALUE_UDP;

			RC4(&sentKey, destLen + 1, destBuf + 1, destBuf + 1);
			destLen += 2;
		}

		string data((const char *) destBuf, destLen);
		IpAddress ip;
		ip.type = AF_INET;
		ip.data.v4 = address;
		::SearchManager::getInstance()->addToSendQueue(data, ip, port, ::SearchManager::FLAG_NO_TRACE);
	}

	bool DHT::processIncoming(const uint8_t* data, size_t size, Ip4Address address, uint16_t remotePort)
	{
		if (state != STATE_ACTIVE) return false;
		if (size < 2 || size > PACKET_BUF_SIZE) return false;
		bool isUdpKeyValid = false;
		bool isEncrypted = false;
		bool isCompressed = false;

		uint8_t buf[PACKET_BUF_SIZE];
		uint8_t zbuf[PACKET_BUF_SIZE];
		const uint8_t* srcBuf;

		string remoteIp = Util::printIpAddress(address);
		if (data[0] != ADC_COMPRESSED_PACKET_HEADER && data[0] != ADC_PACKET_HEADER)
		{
			// it seems to be encrypted packet
			// the first try decrypts with our UDP key and CID
			// if it fails, decryption will happen with CID only
			int tries = 0;
			size--;

			do
			{
				if (++tries == 3)
				{
					// decryption error, it could be malicious packet
					return false;
				}

				// generate key
				TigerHash th;
				if (tries == 1)
					th.update(getUdpKey(remoteIp).data(), CID::SIZE);
				th.update(ClientManager::getMyCID().data(), CID::SIZE);

				RC4_KEY recvKey;
				RC4_set_key(&recvKey, TigerHash::BYTES, th.finalize());

				// decrypt data
				RC4(&recvKey, size, data + 1, buf);
			}
			while (buf[0] != MAGICVALUE_UDP);

			srcBuf = buf + 1; // skip magic value
			size--;

			// if decryption was successful in first try, it happened via UDP key
			// it happens only when we sent our UDP key to this node some time ago
			if (tries == 1) isUdpKeyValid = true;
			isEncrypted = true;
		}
		else
		{
			// plain packet
			srcBuf = data;
		}

		if (srcBuf[0] == ADC_COMPRESSED_PACKET_HEADER && size > 1) // is this compressed packet?
		{
			uLongf bufSize = PACKET_BUF_SIZE;

			// decompress incoming packet
			int result = uncompress(zbuf, &bufSize, srcBuf + 1, size - 1);
			if (result != Z_OK)
			{
				// decompression error
				return false;
			}
			//LogManager::message("DHT packet decompressed: " + Util::toString(size) + " -> " + Util::toString(bufSize), false);
			srcBuf = zbuf;
			size = bufSize;
			isCompressed = true;
		}

		// process decompressed packet
		if (!(size > 2 && srcBuf[0] == ADC_PACKET_HEADER && srcBuf[size - 1] == ADC_PACKET_FOOTER))
			return false;

		string s((const char*) srcBuf, size - 1);
		if (CMD_DEBUG_ENABLED())
			COMMAND_DEBUG(s, DebugTask::HUB_IN, remoteIp + ':' + Util::toString(remotePort));
		if (BOOLSETTING(LOG_UDP_PACKETS))
			LogManager::commandTrace(s, LogManager::FLAG_UDP | LogManager::FLAG_IN, remoteIp, remotePort);
		return dispatch(s, address, remotePort, isUdpKeyValid) || isCompressed || isEncrypted;
	}

	/*
	 * Creates new (or update existing) node which is NOT added to our routing table
	 */
	Node::Ptr DHT::createNode(const CID& cid, Ip4Address ip, uint16_t port, bool update, bool isUdpKeyValid)
	{
		// create user as offline (only TCP connected users will be online)
		UserPtr u = ClientManager::createUser(cid, Util::emptyString, Util::emptyString);

		LOCK(cs);
		return bucket->createNode(u, ip, port, update, isUdpKeyValid);
	}

	/*
	 * Adds node to routing table
	 */
	bool DHT::addNode(const Node::Ptr& node, bool makeOnline)
	{
		bool isAcceptable = true;
		if (!node->isOnline())
		{
			{
				LOCK(cs);
				isAcceptable = bucket->insert(node); // insert node to our routing table
			}

			if (makeOnline)
			{
				// put him online so we can make a connection with him
				node->setOnline(true);
				ClientManager::getInstance()->putOnline(node, true);
				fly_fire1(ClientListener::UserUpdated(), node);
			}
		}

		return isAcceptable;
	}

	/*
	 * Finds "max" closest nodes and stores them to the list
	 */
	void DHT::getClosestNodes(const CID& cid, std::map<CID, Node::Ptr>& closest, unsigned int max, uint8_t maxType)
	{
		LOCK(cs);
		bucket->getClosestNodes(cid, closest, max, maxType);
	}

	/*
	 * Removes dead nodes
	 */
	void DHT::checkExpiration(uint64_t tick)
	{
		OnlineUserList removedList;
		{
			LOCK(cs);
			if (bucket->checkExpiration(tick, removedList))
				setDirty();
		}

		if (!removedList.empty())
		{
			auto cm = ClientManager::getInstance();
			for (auto& ou : removedList)
				cm->putOffline(ou);
			fly_fire2(ClientListener::UserListRemoved(), this, removedList);
		}

		{
			LOCK(fwCheckCs);
			firewalledWanted.clear();
		}
	}

	/*
	 * Starts TTH search
	 */
	unsigned DHT::findFile(const string& tth, uint32_t token, void* owner)
	{
		if (!isConnected()) return 0;
		return SearchManager::getInstance()->findFile(tth, token, owner);
	}

	/*
	 * Sends our info to specified ip:port
	 */
	void DHT::info(Ip4Address ip, uint16_t port, uint32_t type, const CID& targetCID, const CID& udpKey)
	{
		// TODO: what info is needed?
		AdcCommand cmd(AdcCommand::CMD_INF, AdcCommand::TYPE_UDP);

		string clientName;
		string clientVersion;
		if (BOOLSETTING(OVERRIDE_CLIENT_ID))
			FavoriteManager::splitClientId(SETTING(CLIENT_ID), clientName, clientVersion);
		if (clientName.empty())
		{
			clientName = APPNAME;
			clientVersion = VERSION_STR;
		}

		cmd.addParam("TY", Util::toString(type));
		cmd.addParam("AP", clientName);
		cmd.addParam("VE", clientVersion);
		cmd.addParam("NI", SETTING(NICK));
		cmd.addParam("SL", Util::toString(UploadManager::getSlots()));

		int64_t limit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getUploadLimitInKBytes() : 0;
		if (limit > 0)
			cmd.addParam("US", Util::toString(limit * 1024));
		else
			cmd.addParam("US", Util::toString((long)(Util::toDouble(SETTING(UPLOAD_SPEED))*1024*1024/8)));

		string su;
		if (CryptoManager::TLSOk())
			su = AdcSupports::ADCS_FEATURE;

		// TCP status according to global status
		if (ClientManager::isActive(AF_INET, 0))
		{
			if (!su.empty()) su += ',';
			su += AdcSupports::TCP4_FEATURE;
		}

		// UDP status according to UDP status check
		if (!isFirewalled())
		{
			if (!su.empty()) su += ',';
			su += AdcSupports::UDP4_FEATURE;
		}

		cmd.addParam("SU", su);

		send(cmd, ip, port, targetCID, udpKey);
	}

	/*
	 * Sends Connect To Me request to online node
	 */
	void DHT::connect(const OnlineUserPtr& ou, const string& token, bool forcePassive)
	{
		if (ou->getClientBase().getType() != ClientBase::TYPE_DHT) return;
		Node::Ptr node = std::static_pointer_cast<Node>(ou);
		ConnectionManager::getInstance()->connect(node, token);
	}

	/*
	 * Sends private message to online node
	 */
	void DHT::privateMessage(const OnlineUserPtr& /*ou*/, const string& /*aMessage*/, bool /*thirdPerson*/)
	{
		//AdcCommand cmd(AdcCommand::CMD_MSG, AdcCommand::TYPE_UDP);
		//cmd.addParam(aMessage);
		//if (thirdPerson)
		//	cmd.addParam("ME", "1");
		//
		//send(cmd, ou.getIdentity().getIP4(), static_cast<uint16_t>(Util::toInt(ou.getIdentity().getUdp4Port())));
	}

	/*
	 * Loads network information from XML file
	 */
	void DHT::loadData()
	{
		try
		{
			::File f(Util::getPath(Util::PATH_USER_CONFIG) + DHT_FILE, ::File::READ, ::File::OPEN);
			SimpleXML xml;
			xml.fromXML(f.read());

			xml.stepIn();

			// load nodes; when file is older than 7 days, bootstrap from database later
			if ((int64_t) ::File::timeStampToUnixTime(f.getTimeStamp()) > (int64_t) time(nullptr) - 7 * 24 * 60 * 60)
				bucket->loadNodes(xml);

			// load indexes
			IndexManager::getInstance()->loadIndexes(xml);
			xml.stepOut();
		}
		catch (Exception& e)
		{
			LogManager::message(STRING_F(DHT_LOADING_ERROR, e.getError()));
		}
	}

	/*
	 * Finds "max" closest nodes and stores them to the list
	 */
	void DHT::saveData()
	{
		if (!dirty)
			return;

		LOCK(cs);

		SimpleXML xml;
		xml.addTag("DHT");
		xml.stepIn();

		// save nodes
		bucket->saveNodes(xml);

		// save foreign published files
		IndexManager::getInstance()->saveIndexes(xml);

		xml.stepOut();

		string path = Util::getPath(Util::PATH_USER_CONFIG) + DHT_FILE;
		try
		{
			string tempPath = path + ".tmp";
			::File file(tempPath, ::File::WRITE, ::File::CREATE | ::File::TRUNCATE);
			BufferedOutputStream<false> bos(&file, 256 * 1024);
			bos.write(SimpleXML::utf8Header);
			xml.toXML(&bos);
			bos.flushBuffers(true);
			file.close();
			::File::renameFile(tempPath, path);
		}
		catch (const Exception& e)
		{
			LogManager::message("Error writing " + path + ": " + e.getError());
		}
	}

	/*
	 * Message processing
	 */

	// user's info
	bool DHT::handle(AdcCommand::INF, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		Identity& id = node->getIdentity();
		uint16_t udpPort = id.getUdp4Port();

		InfType it = NONE;
		for (auto i = c.getParameters().begin() + 1; i != c.getParameters().end(); ++i)
		{
			if (i->length() < 2)
				continue;

			switch (*(const uint16_t*) i->c_str())
			{
				case TAG('T', 'Y'):
					it = (InfType) Util::toInt(i->c_str() + 2);
					break;

				case TAG('N', 'I'):
				{
					string nick = i->substr(2);
					if (!nick.empty())
					{
						id.setNick(nick);
						id.getUser()->addNick(nick, NetworkName);
					}
					break;
				}

				case TAG('S', 'L'):
					id.setSlots(Util::toInt(i->c_str() + 2));
					break;

				case TAG('F', 'S'):
					id.setFreeSlots(Util::toInt(i->c_str() + 2));
					break;

				case TAG('S', 'S'):
					id.setBytesShared(Util::toInt64(i->c_str() + 2));
					break;

				case TAG('S', 'U'):
					AdcSupports::setSupports(id, i->substr(2));
					break;

				case TAG('S', 'F'):
					id.setSharedFiles(Util::toInt(i->c_str() + 2));
					break;

				case TAG('E', 'M'):
					id.setEmail(i->substr(2));
					break;

				case TAG('D', 'E'):
					id.setDescription(i->substr(2));
					break;

				case TAG('U', 'S'):
					id.setLimit(Util::toUInt32(i->c_str() + 2));
					break;

				case TAG('A', 'W'):
					id.setStatusBit(Identity::SF_AWAY, i->length() == 3 && (*i)[2] == '1');
					break;

				case TAG('V', 'E'):
				case TAG('A', 'P'):
					id.setStringParam(i->c_str(), i->substr(2));
					break;
			}
		}

#if 0 // FIXME
		if (node->getIdentity().supports(ADCS_FEATURE))
		{
			node->getUser()->setFlag(User::TLS);
		}
#endif

		// add node to our routing table and put it online
		addNode(node, true);

		ClientManager::getInstance()->updateUser(node);

		// do we wait for any search results from this user?
		SearchManager::getInstance()->processSearchResults(node->getUser(), node->getIdentity().getSlots());

		if (it & PING)
		{
			// remove ping flag to avoid ping-pong-ping-pong-ping...
			info(node->getIdentity().getIP4(), udpPort, it & ~PING, node->getUser()->getCID(), node->getUdpKey());
		}
		return true;
	}

	// incoming search request
	bool DHT::handle(AdcCommand::SCH, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		SearchManager::getInstance()->processSearchRequest(node, c);
		return true;
	}

	// incoming search result
	bool DHT::handle(AdcCommand::RES, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		return SearchManager::getInstance()->processSearchResult(node, c);
	}

	// incoming publish request
	bool DHT::handle(AdcCommand::PUB, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		if (!isFirewalled()) // we should index this entry only if our UDP port is opened
			IndexManager::getInstance()->processPublishSourceRequest(node, c);
		return true;
	}

	// connection request
	bool DHT::handle(AdcCommand::CTM, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		ConnectionManager::getInstance()->connectToMe(node, c);
		return true;
	}

	// reverse connection request
	bool DHT::handle(AdcCommand::RCM, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		ConnectionManager::getInstance()->revConnectToMe(node, c);
		return true;
	}

	// status message
	bool DHT::handle(AdcCommand::STA, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		if (c.getParameters().size() < 3)
			return true;

		Ip4Address fromIP = node->getIdentity().getIP4();
		int code = Util::toInt(c.getParam(1).substr(1));

		if (code == 0)
		{
			string resTo;
			if (!c.getParam("FC", 2, resTo))
				return true;

			if (resTo == "PUB")
			{
/*#ifndef NDEBUG
				// don't do anything
				string tth;
				if (!c.getParam("TR", 1, tth))
					return;

				try
				{
					string fileName = Util::getFileName(ShareManager::getInstance()->toVirtual(TTHValue(tth)));
					LogManager::getInstance()->message("DHT (" + fromIP + "): File published: " + fileName);
				}
				catch (ShareException&)
				{
					// published non-shared file??? Maybe partial file
					LogManager::getInstance()->message("DHT (" + fromIP + "): Partial file published: " + tth);

				}
#endif*/
			}
			else if (resTo == "FWCHECK")
			{
				LOCK(fwCheckCs);
				auto j = firewalledWanted.find(fromIP);
				if (j == firewalledWanted.end())
					return true; // we didn't requested firewall check from this node

				firewalledWanted.erase(j);
				if (firewalledChecks.find(fromIP) != firewalledChecks.end())
					return true; // already received firewall check from this node

				string i4, u4;
				if (!c.getParam("I4", 1, i4) || !c.getParam("U4", 1, u4))
					return true; // no IP and port in response

				Ip4Address externalIP;
				if (!Util::parseIpAddress(externalIP, i4) || !Util::isValidIp4(externalIP))
					return true;

				uint16_t externalUdpPort = static_cast<uint16_t>(Util::toInt(u4));				
				
				firewalledChecks.insert(std::make_pair((uint32_t) fromIP, std::make_pair((uint32_t) externalIP, externalUdpPort)));

				if (firewalledChecks.size() >= FW_RESPONSES)
				{
					// when we received more firewalled statuses, we will be firewalled
					int fw = 0;
					uint32_t lastIP = 0;
					for (auto i = firewalledChecks.cbegin(); i != firewalledChecks.cend(); i++)
					{
						uint32_t ip = i->second.first;
						uint16_t udpPort = i->second.second;

						if (udpPort != getPort())
							fw++;
						else
							fw--;

						if (!lastIP)
						{
							externalIP = ip;
							lastIP = ip;
						}

						//If the last check matches this one, reset our current IP.
						//If the last check does not match, wait for our next incoming IP.
						//This happens for one reason.. a client responsed with a bad IP.
						if (ip == lastIP)
							externalIP = ip;
						else
							lastIP = ip;
					}

					if (fw >= 0)
					{
						// we are probably firewalled, so our internal UDP port is unaccessible
						if (externalIP != lastExternalIP || !firewalled)
							LogManager::message(STRING_F(DHT_PORT_FIREWALLED, Util::printIpAddress(externalIP)));
						firewalled = true;
					}
					else
					{
						if (externalIP != lastExternalIP || firewalled)
							LogManager::message(STRING_F(DHT_PORT_OPEN, Util::printIpAddress(externalIP)));
						firewalled = false;
					}

#if 0
					if (BOOLSETTING(UPDATE_IP))
						SettingsManager::getInstance()->set(SettingsManager::EXTERNAL_IP, externalIP);
#endif
					firewalledChecks.clear();
					firewalledWanted.clear();

					lastExternalIP = externalIP;
					requestFWCheck = false;
				}
			}
		}
		return true;
	}

	// partial file request
	bool DHT::handle(AdcCommand::PSR, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		IpAddress ip;
		ip.type = AF_INET;
		ip.data.v4 = node->getIdentity().getIP4();
		::SearchManager::getInstance()->onPSR(c, true, node->getUser(), ip);
		return true;
	}

	// private message
	bool DHT::handle(AdcCommand::MSG, const Node::Ptr& /*node*/, AdcCommand& /*c*/) noexcept
	{
		// not implemented yet
		//fire(ClientListener::PrivateMessage(), this, *node, to, node, c.getParam(0), c.hasFlag("ME", 1));

		//privateMessage(*node, "Sorry, private messages aren't supported yet!", false);
		return false;
	}

	bool DHT::handle(AdcCommand::GET, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		if (c.getParam(1) == "nodes" && c.getParam(2) == "dht.xml")
		{
			AdcCommand cmd(AdcCommand::CMD_SND, AdcCommand::TYPE_UDP);
			cmd.addParam(c.getParam(1));
			cmd.addParam(c.getParam(2));

			SimpleXML xml;
			xml.addTag("Nodes");
			xml.stepIn();

			// get 20 random contacts
			Node::Map nodes;
			DHT::getInstance()->getClosestNodes(CID::generate(), nodes, 20, 2);

			// add nodelist in XML format
			for (Node::Map::const_iterator i = nodes.begin(); i != nodes.end(); i++)
			{
				xml.addTag("Node");
				xml.addChildAttrib("CID", i->second->getUser()->getCID().toBase32());
				xml.addChildAttrib("I4", Util::printIpAddress(i->second->getIdentity().getIP4()));
				xml.addChildAttrib("U4", i->second->getIdentity().getUdp4Port());
			}

			xml.stepOut();

			string nodesXML;
			StringOutputStream sos(nodesXML);
			xml.toXML(&sos);

			cmd.addParam(Utils::compressXML(nodesXML));

			send(cmd, node->getIdentity().getIP4(),
				node->getIdentity().getUdp4Port(), node->getUser()->getCID(), node->getUdpKey());
		}
		return true;
	}

	bool DHT::handle(AdcCommand::SND, const Node::Ptr& node, AdcCommand& c) noexcept
	{
		if (c.getParam(1) == "nodes" && c.getParam(2) == "dht.xml")
		{
			// add node to our routing table
			if (node->isIpVerified())
				addNode(node, false);

			try
			{
				SimpleXML xml;
				xml.fromXML(c.getParam(3));
				xml.stepIn();

				// extract bootstrap nodes
				unsigned int n = 20;
				while (xml.findChild("Node"))
				{
					CID cid = CID(xml.getChildAttrib("CID"));

					if (cid.isZero())
						continue;

					// don't bother with myself
					if (ClientManager::getMyCID() == cid)
						continue;

					const string& i4 = xml.getChildAttrib("I4");
					Ip4Address address;
					if (!Util::parseIpAddress(address, i4))
						continue;

					uint16_t u4 = static_cast<uint16_t>(xml.getIntChildAttrib("U4"));

					// don't bother with private IPs
					if (!Utils::isGoodIPPort(address, u4))
						continue;

					// create verified node, it's not big risk here and allows faster bootstrapping
					// if this node already exists in our routing table, don't update it's ip/port for security reasons
					Node::Ptr newNode = DHT::getInstance()->createNode(cid, address, u4, false, true);
					DHT::getInstance()->addNode(newNode, false);
					if (--n == 0) break;
				}

				xml.stepOut();
			}
			catch (const SimpleXMLException&)
			{
				// malformed node list
			}
		}
		return true;
	}

	bool DHT::isConnected() const
	{
		return state == STATE_ACTIVE && lastPacket && GET_TICK() - lastPacket < CONNECTED_TIMEOUT;
	}

	uint16_t DHT::getPort()
	{
		return ::SearchManager::getUdpPort();
	}

	void DHT::setRequestFWCheck()
	{
		LOCK(fwCheckCs);
		requestFWCheck = true;
		firewalledWanted.clear();
		firewalledChecks.clear();
	}

	bool DHT::isFirewalled() const
	{
		LOCK(fwCheckCs);
		return firewalled;
	}

	string DHT::getLastExternalIP() const
	{
		LOCK(fwCheckCs);
		return Util::printIpAddress(lastExternalIP);
	}

	void DHT::setExternalIP(const string& ip)
	{
		Ip4Address address;
		if (!Util::parseIpAddress(address, ip) || !Util::isValidIp4(address)) return;
		LOCK(fwCheckCs);
		lastExternalIP = address;
	}

	void DHT::getPublicIPInfo(string& externalIP, bool &isFirewalled) const
	{
		LOCK(fwCheckCs);
		externalIP = Util::printIpAddress(lastExternalIP);
		isFirewalled = firewalled;
	}

	/*
	 * Generates UDP key for specified IP address
	 */
	CID DHT::getUdpKey(const string& targetIp) const
	{
		TigerTree th;
		th.update(myUdpKey.data(), CID::SIZE);
		th.update(targetIp.data(), targetIp.length());
		return CID(th.finalize());
	}

	size_t DHT::getNodesCount() const
	{
		LOCK(cs);
		return bucket ? bucket->getNodes().size() : 0;
	}

	bool DHT::pingNode(const CID& cid)
	{
		Node::Ptr node;
		{
			LOCK(cs);
			if (bucket)
			{
				const KBucket::NodeList& nodes = bucket->getNodes();
				for (const Node::Ptr& n : nodes)
					if (n->getUser()->getCID() == cid)
					{
						node = n;
						break;
					}
			}
		}
		if (!node) return false;
		node->setTimeout();
		info(node->getIdentity().getIP4(), node->getIdentity().getUdp4Port(),
			PING | MAKE_ONLINE, node->getUser()->getCID(), node->getUdpKey());
		return true;
	}

	void DHT::dumpUserInfo(const string& userReport)
	{
		fly_fire2(ClientListener::UserReport(), this, userReport);
	}

	void DHT::updateLocalIP(const string& localIP)
	{
		Ip4Address ip;
		if (!Util::parseIpAddress(ip, localIP) || !Util::isValidIp4(ip)) return;
		LOCK(fwCheckCs);
		if (!lastExternalIP) lastExternalIP = ip;
	}

}
