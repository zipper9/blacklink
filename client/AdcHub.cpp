/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "AdcHub.h"

#include "ClientManager.h"
#include "SearchManager.h"
#include "ShareManager.h"
#include "ParamExpander.h"
#include "SimpleStringTokenizer.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "UserCommand.h"
#include "CryptoManager.h"
#include "LogManager.h"
#include "UploadManager.h"
#include "ThrottleManager.h"
#include "DebugManager.h"
#include "Tag16.h"
#include "SocketPool.h"

#ifdef _DEBUG
extern bool suppressUserConn;
#endif

// these extensions *must* be sorted alphabetically!
const vector<StringList> AdcHub::searchExts
{
	{ "ape", "flac", "m4a", "mid", "mp3", "mpc", "ogg", "ra", "wav", "wma" },
	{ "7z", "ace", "arj", "bz2", "gz", "lha", "lzh", "rar", "tar", "z", "zip" },
	{ "doc", "docx", "htm", "html", "nfo", "odf", "odp", "ods", "odt", "pdf", "ppt", "pptx", "rtf", "txt", "xls", "xlsx", "xml", "xps" },
	{ "app", "bat", "cmd", "com", "dll", "exe", "jar", "msi", "ps1", "vbs", "wsf" },
	{ "bmp", "cdr", "eps", "gif", "ico", "img", "jpeg", "jpg", "png", "ps", "psd", "sfw", "tga", "tif", "webp" },
	{ "3gp", "asf", "asx", "avi", "divx", "flv", "mkv", "mov", "mp4", "mpeg", "mpg", "ogm", "pxp", "qt", "rm", "rmvb", "swf", "vob", "webm", "wmv" }
};

ClientBasePtr AdcHub::create(const string& hubURL, const string& address, uint16_t port, bool secure)
{
	return std::shared_ptr<Client>(static_cast<Client*>(new AdcHub(hubURL, address, port, secure)));
}

AdcHub::AdcHub(const string& hubURL, const string& address, uint16_t port, bool secure) :
	Client(hubURL, address, port, '\n', secure, Socket::PROTO_ADC),
	featureFlags(0), lastErrorCode(0), sid(0),
	csUsers(std::unique_ptr<RWLock>(RWLock::create()))
{
}

AdcHub::~AdcHub()
{
	dcassert(users.empty());
}

void AdcHub::getUserList(OnlineUserList& result) const
{
	READ_LOCK(*csUsers);
	result.reserve(users.size());
	for (auto i = users.cbegin(); i != users.cend(); ++i)
	{
		if (i->first != AdcCommand::HUB_SID)
		{
			result.push_back(i->second);
		}
	}
}

OnlineUserPtr AdcHub::getUser(uint32_t sid, const CID& cid, const string& nick)
{
	OnlineUserPtr ou = findUser(sid);
	if (ou)
		return ou;

	return addUser(sid, cid, nick);
}

OnlineUserPtr AdcHub::addUser(uint32_t sid, const CID& cid, const string& nick)
{
	OnlineUserPtr ou;
	if (cid.isZero())
	{
		WRITE_LOCK(*csUsers);
		ou = users.insert(make_pair(sid, getHubOnlineUser())).first->second;
	}
	else if (cid == getMyOnlineUser()->getUser()->getCID())
	{
		{
			WRITE_LOCK(*csUsers);
			ou = users.insert(make_pair(sid, getMyOnlineUser())).first->second;
		}
		ou->getIdentity().setSID(sid);
#ifdef BL_FEATURE_IP_DATABASE
		ou->getUser()->addNick(nick, getHubUrl());
#endif
	}
	else // User
	{
		UserPtr u = ClientManager::createUser(cid, nick, getHubUrl());
		u->setLastNick(nick);
		auto newUser = std::make_shared<OnlineUser>(u, getClientPtr(), sid);
		WRITE_LOCK(*csUsers);
		ou = users.insert(make_pair(sid, newUser)).first->second;
	}

	if (sid != AdcCommand::HUB_SID)
		ClientManager::getInstance()->putOnline(ou, true);
	return ou;
}

OnlineUserPtr AdcHub::findUser(const string& nick) const
{
	READ_LOCK(*csUsers);
	for (auto i = users.cbegin(); i != users.cend(); ++i)
	{
		if (i->second->getIdentity().getNick() == nick)
		{
			return i->second;
		}
	}
	return nullptr;
}

OnlineUserPtr AdcHub::findUser(uint32_t sid) const
{
	READ_LOCK(*csUsers);
	const auto& i = users.find(sid);
	return i == users.end() ? OnlineUserPtr() : i->second;
}

OnlineUserPtr AdcHub::findUser(const CID& cid) const
{
	READ_LOCK(*csUsers);
	for (auto i = users.cbegin(); i != users.cend(); ++i)
	{
		if (i->second->getUser()->getCID() == cid)
		{
			return i->second;
		}
	}
	return nullptr;
}

void AdcHub::putUser(uint32_t sid, bool disconnect)
{
	OnlineUserPtr ou;
	{
		WRITE_LOCK(*csUsers);
		const auto& i = users.find(sid);
		if (i == users.end())
			return;
		auto bytesShared = i->second->getIdentity().getBytesShared();
		ou = i->second;
		users.erase(i);
		decBytesShared(bytesShared);
	}
	
	if (sid != AdcCommand::HUB_SID)
	{
		ClientManager::getInstance()->putOffline(ou, disconnect);
	}
	
	fire(ClientListener::UserRemoved(), this, ou);
}

void AdcHub::clearUsers()
{
	if (myOnlineUser)
		myOnlineUser->getIdentity().setBytesShared(0);
	if (ClientManager::isBeforeShutdown())
	{
		WRITE_LOCK(*csUsers);
		users.clear();
		bytesShared.store(0);
	}
	else
	{
		SIDMap tmp;
		{
			WRITE_LOCK(*csUsers);
			users.swap(tmp);
			bytesShared.store(0);
		}
		
		for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
		{
			if (i->first != AdcCommand::HUB_SID)
			{
				ClientManager::getInstance()->putOffline(i->second);
			}
		}
	}
}

void AdcHub::handle(AdcCommand::INF, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty()) return;
	OnlineUserPtr ou;
	bool newUser = false;
	string cidStr;
	if (c.getParam("ID", 0, cidStr))
	{
		const CID cid(cidStr);
		ou = findUser(cid);
		if (ou)
		{
			if (ou->getIdentity().getSID() != c.getFrom())
			{
				// Same CID but different SID not allowed
				const string message = ou->getIdentity().getNick() + " (" + ou->getIdentity().getSIDString() +
					") has same CID {" + cidStr + "} as " + c.getNick() + " (" + AdcCommand::fromSID(c.getFrom()) + "), ignoring.";
				fire(ClientListener::StatusMessage(), this, message, ClientListener::FLAG_DEBUG_MSG);
				return;
			}
		}
		else
		{
			ou = findUser(c.getFrom());
			if (!ou)
			{
				newUser = true;
				ou = addUser(c.getFrom(), cid, c.getNick());
			}
		}
	}
	else if (c.getFrom() == AdcCommand::HUB_SID)
	{
		ou = getUser(c.getFrom(), CID(), c.getNick());
#ifdef IRAINMAN_USE_HIDDEN_USERS
		ou->getIdentity().setHidden();
#endif
	}
	else
	{
		ou = findUser(c.getFrom());
	}
	if (!ou)
	{
		dcdebug("AdcHub::INF Unknown user / no ID\n");
		return;
	}
	
	auto& id = ou->getIdentity();
	string ip4;
	string ip6;
	for (auto i = c.getParameters().cbegin(); i != c.getParameters().cend(); ++i)
	{
		if (i->length() < 2)
			continue;

		switch (*(const uint16_t*)i->c_str())
		{
			case TAG('S', 'L'):
			{
				id.setSlots(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('F', 'S'):
			{
				id.setFreeSlots(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('S', 'S'):
			{
				changeBytesShared(id, Util::toInt64(i->c_str() + 2));
				break;
			}
			case TAG('S', 'U'):
			{
				uint32_t parsedFeatures;
				AdcSupports::setSupports(id, i->substr(2), getHubUrl(), &parsedFeatures);
				bool isPassive = true;
				if (parsedFeatures & User::TCP4)
					isPassive = false;
				if ((parsedFeatures & User::TCP6) && ConnectivityManager::hasIP6())
					isPassive = false;
				id.setStatusBit(Identity::SF_PASSIVE, isPassive);
				break;
			}
			case TAG('S', 'F'):
			{
				id.setSharedFiles(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('I', '4'):
			{
				ip4 = i->substr(2);
				break;
			}
			case TAG('U', '4'):
			{
				id.setUdp4Port(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('I', '6'):
			{
				ip6 = i->substr(2);
				break;
			}
			case TAG('U', '6'):
			{
				id.setUdp6Port(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('E', 'M'):
			{
				id.setEmail(i->substr(2));
				break;
			}
			case TAG('D', 'E'):
			{
				id.setDescription(i->substr(2));
				break;
			}
			case TAG('C', 'O'):
			{
				// ignored: non official.
				break;
			}
			case TAG('I', 'D'):
			{
				// ignore, already in user.
				break;
			}
			case TAG('D', 'S'):
			{
				id.setDownloadSpeed(Util::toUInt32(i->c_str() + 2));
				break;
			}
			case TAG('O', 'P'):
			case TAG('B', 'O'):
			{
				// ignore, use CT
				break;
			}
			case TAG('C', 'T'):
			{
				id.setClientType(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('U', 'S'):
			{
				id.setLimit(Util::toUInt32(i->c_str() + 2));
				break;
			}
			case TAG('H', 'N'):
			{
				id.setHubsNormal(Util::toUInt32(i->c_str() + 2));
				break;
			}
			case TAG('H', 'R'):
			{
				id.setHubsRegistered(Util::toUInt32(i->c_str() + 2));
				break;
			}
			case TAG('H', 'O'):
			{
				id.setHubsOperator(Util::toUInt32(i->c_str() + 2));
				break;
			}
			case TAG('N', 'I'):
			{
				id.setNick(i->substr(2));
				break;
			}
			case TAG('A', 'W'):
			{
				id.setStatusBit(Identity::SF_AWAY, i->length() == 3 && (*i)[2] == '1');
				break;
			}
#ifdef _DEBUG
			case TAG('V', 'E'):
			{
				id.setStringParam("VE", i->substr(2));
				break;
			}
			case TAG('A', 'P'):
			{
				id.setStringParam("AP", i->substr(2));
				break;
			}
#endif
			default:
			{
				id.setStringParam(i->c_str(), i->substr(2));
			}
		}
	}
	if (!ip4.empty())
	{
		Ip4Address ip;
		if (Util::parseIpAddress(ip, ip4) && Util::isValidIp4(ip))
			id.setIP4(ip);
	}
	if (!ip6.empty())
	{
		Ip6Address ip;
		if (Util::parseIpAddress(ip, ip6) && Util::isValidIp6(ip))
			id.setIP6(ip);
	}

	if (isMe(ou))
	{
		csState.lock();
		bool fireLoggedIn = state != STATE_NORMAL;
		state = STATE_NORMAL;
		connSuccess = true;
		csState.unlock();
		setAutoReconnect(true);
		updateCounts(false);
		updateConnectionStatus(ConnectionStatus::SUCCESS);
		updateUserCheckTime();
		fireUserUpdated(ou);
		if (fireLoggedIn)
			fire(ClientListener::LoggedIn(), this);
		if (newUser && id.isOp())
			fire(ClientListener::HubInfoMessage(), ClientListener::OperatorInfo, this, Util::emptyString);
	}
	else if (id.isHub())
	{
		fire(ClientListener::HubUpdated(), this);
	}
	else
	{
		fireUserUpdated(ou);
	}
}

void AdcHub::handle(AdcCommand::SUP, const AdcCommand& c) noexcept
{
	{
		LOCK(csState);
		if (state != STATE_PROTOCOL)
			return;
	}
	
	bool baseOk = false;
	bool tigrOk = false;
	for (auto i = c.getParameters().cbegin(); i != c.getParameters().cend(); ++i)
	{
		if (*i == AdcSupports::BAS0_SUPPORT)
		{
			baseOk = true;
			tigrOk = true;
		}
		else if (*i == AdcSupports::BASE_SUPPORT)
		{
			baseOk = true;
		}
		else if (*i == AdcSupports::TIGR_SUPPORT)
		{
			tigrOk = true;
		}
	}
	
	if (!baseOk)
	{
		fire(ClientListener::StatusMessage(), this, "Failed to negotiate base protocol"); // TODO: translate
		disconnect(false);
		return;
	}
	if (!tigrOk)
	{
		csState.lock();
		featureFlags |= FEATURE_FLAG_OLD_PASSWORD;
		csState.unlock();
		// Some hubs fake BASE support without TIGR support =/
		fire(ClientListener::StatusMessage(), this, "Hub probably uses an old version of ADC, please encourage the owner to upgrade"); // TODO: translate
	}
}

void AdcHub::handle(AdcCommand::SID, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
		return;
		
	{
		LOCK(csState);
		if (state != STATE_PROTOCOL)
		{
			dcdebug("Invalid state for SID\n");
			return;
		}
		state = STATE_IDENTIFY;
	}
	
	sid = AdcCommand::toSID(c.getParam(0));	
	info(true);
}

void AdcHub::handle(AdcCommand::MSG, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
		return;
	auto user = findUser(c.getFrom());
	if (!user)
	{
		if (c.getFrom() == AdcCommand::HUB_SID)
			fire(ClientListener::StatusMessage(), this, c.getParam(0));
		else
			LogManager::message("Ignored message from unknown SID " + Util::toString(c.getFrom()) + " Message = " + c.getParam(0));
		return;
	}

	bool isPM = false;
	string pmSID;
	if (c.getParam("PM", 1, pmSID))
	{
		if (user->getUser()->isMe()) return;
		isPM = true;
	}

	unique_ptr<ChatMessage> message(new ChatMessage(c.getParam(0), user));
	message->thirdPerson = c.hasFlag("ME", 1);

	string timestamp;
	if (c.getParam("TS", 1, timestamp))
		message->setTimestamp(Util::toInt64(timestamp));

	if (isPM) // add PM<group-cid> as well
	{
		message->to = findUser(c.getTo());
		if (!message->to)
			return;

		message->replyTo = findUser(AdcCommand::toSID(pmSID));
		if (!message->replyTo)
			return;

		string response;
		OnlineUserPtr replyTo = message->replyTo;
		processIncomingPM(message, response);
		if (!response.empty())
			privateMessage(replyTo, response, PM_FLAG_AUTOMATIC | PM_FLAG_THIRD_PERSON);
	}
	else
	if (isChatMessageAllowed(*message, Util::emptyString))
	{
		fire(ClientListener::Message(), this, message);
	}
}

void AdcHub::processCCPMMessage(const AdcCommand& c, const OnlineUserPtr& ou) noexcept
{
	dcassert(!c.getParameters().empty());
	unique_ptr<ChatMessage> message(new ChatMessage(c.getParam(0), ou, nullptr, nullptr, c.hasFlag("ME", 1)));
	message->to = getMyOnlineUser();
	message->replyTo = ou;
	string response;
	processIncomingPM(message, response);
	if (!response.empty())
		ConnectionManager::getInstance()->sendCCPMMessage(ou, response, PM_FLAG_AUTOMATIC | PM_FLAG_THIRD_PERSON);
}

void AdcHub::handle(AdcCommand::GPA, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
		return;

	setRegistered();
	csState.lock();
	salt = c.getParam(0);
	state = STATE_VERIFY;
	string pwd = storedPassword;
	csState.unlock();
	processPasswordRequest(pwd);
}

void AdcHub::handle(AdcCommand::QUI, const AdcCommand& c) noexcept
{
	uint32_t s = AdcCommand::toSID(c.getParam(0));
	
	OnlineUserPtr victim = findUser(s);
	if (victim)
	{
		string tmp;
		if (c.getParam("MS", 1, tmp))
		{
			OnlineUserPtr source = nullptr;
			string tmp2;
			if (c.getParam("ID", 1, tmp2))
			{
				//dcassert(tmp2.size() == 39);
				source = findUser(AdcCommand::toSID(tmp2));
			}
			
			if (source)
				tmp = victim->getIdentity().getNick() + " was kicked by " + source->getIdentity().getNick() + ": " + tmp;
			else
				tmp = victim->getIdentity().getNick() + " was kicked: " + tmp;
			fire(ClientListener::StatusMessage(), this, tmp, ClientListener::FLAG_KICK_MSG);
		}
		putUser(s, c.getParam("DI", 1, tmp));
	}

	if (s == sid)
	{
		// this QUI is directed to us
		string tmp;
		if (c.getParam("TL", 1, tmp))
		{
			if (tmp == "-1")
			{				
				if ((lastErrorCode == AdcCommand::ERROR_NICK_INVALID || lastErrorCode == AdcCommand::ERROR_NICK_TAKEN) && hasRandomTempNick())
					setReconnDelay(30);
				else
					setAutoReconnect(false);
			}
			else
			{
				setAutoReconnect(true);
				setReconnDelay(Util::toUInt32(tmp));
			}
		}
		if (!victim && c.getParam("MS", 1, tmp))
		{
			fire(ClientListener::StatusMessage(), this, tmp);
		}
		if (c.getParam("RD", 1, tmp))
		{
			fire(ClientListener::Redirect(), this, tmp);
		}
	}
}

void AdcHub::handle(AdcCommand::CTM, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou || ou->getUser()->isMe())
		return;
	if (c.getParameters().size() < 3)
		return;
		
	const string& protocol = c.getParam(0);
	const string& port = c.getParam(1);
	const string& token = c.getParam(2);
	
	bool secure = false;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		// Nothing special
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->isInitialized())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}

	IpAddress ip = ou->getIdentity().getConnectIP();
	if (!ip.type)
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "IP unknown", AdcCommand::TYPE_DIRECT).setTo(c.getFrom()));
		return;
	}

#ifdef _DEBUG
	if (suppressUserConn)
	{
		LogManager::message("CTM from " + ou->getUser()->getCID().toBase32() + " ignored!");
		return;
	}
#endif
	ConnectionManager::getInstance()->adcConnect(*ou, static_cast<uint16_t>(Util::toInt(port)), token, secure);
}


void AdcHub::handle(AdcCommand::ZON, const AdcCommand&) noexcept
{
	clientSock->setMode(BufferedSocket::MODE_ZPIPE);
	dcdebug("ZLIF mode enabled on hub: %s\n", getHubUrlAndIP().c_str());
}

void AdcHub::handle(AdcCommand::ZOF, const AdcCommand&) noexcept
{
	clientSock->setMode(BufferedSocket::MODE_LINE);
}

void AdcHub::handle(AdcCommand::RCM, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 2)
		return;

	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
	}

	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou || ou->getUser()->isMe())
		return;

	const string& token = c.getParam(1);
	if (token.empty())
		return;

	const string& protocol = c.getParam(0);
	bool secure;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		secure = false;
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->isInitialized())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}

	if (isActive())
	{
		if (!ConnectionManager::tokenManager.addToken(token, TokenManager::TYPE_UPLOAD, GET_TICK() + 80000))
		{
			LogManager::message("RCM: token " + token + " is already in use");
			return;
		}
		connectUser(*ou, token, secure, true);
		return;
	}
	
	if (!isFeatureSupported(FEATURE_FLAG_ALLOW_NAT_TRAVERSAL) || !(ou->getUser()->getFlags() & User::NAT0))
		return;

	if (ou->getIdentity().getConnectIP().type == AF_INET6)
		return;

	// Attempt to traverse NATs and/or firewalls with TCP.
	// If they respond with their own, symmetric, RNT command, both
	// clients call ConnectionManager::adcConnect.
	const string userKey = ou->getIdentity().getCID() + ou->getIdentity().getSIDString() + '*';
	uint16_t localPort = socketPool.addSocket(userKey, AF_INET, true, secure, true, Util::emptyString);
	if (!localPort)
		return;
	send(AdcCommand(AdcCommand::CMD_NAT, ou->getIdentity().getSID(), AdcCommand::TYPE_DIRECT).
	     addParam(protocol).addParam(Util::toString(localPort)).addParam(token));
}

void AdcHub::handle(AdcCommand::CMD, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
		return;
	if (!isFeatureSupported(FEATURE_FLAG_USER_COMMANDS))
		return;
	const string& name = c.getParam(0);
	if (c.hasFlag("RM", 1))
	{
		removeUserCommand(name);
		return;
	}
	bool sep = c.hasFlag("SP", 1);
	string sctx;
	if (!c.getParam("CT", 1, sctx))
		return;
	int ctx = Util::toInt(sctx);
	if (ctx <= 0)
		return;
	if (sep)
	{
		addUserCommand(UserCommand(0, UserCommand::TYPE_SEPARATOR, ctx,
			UserCommand::FLAG_FROM_ADC_HUB | UserCommand::UserCommand::FLAG_NOSAVE,
			name, Util::emptyString, Util::emptyString, Util::emptyString));
		return;
	}
	bool once = c.hasFlag("CO", 1);
	string txt;
	if (!c.getParam("TT", 1, txt))
		return;
	addUserCommand(UserCommand(0, once ? UserCommand::TYPE_RAW_ONCE : UserCommand::TYPE_RAW, ctx,
		UserCommand::FLAG_FROM_ADC_HUB | UserCommand::FLAG_NOSAVE,
		name, txt, Util::emptyString, Util::emptyString));
}

void AdcHub::sendUDP(const AdcCommand& cmd) noexcept
{
	string command;
	IpAddress ip;
	uint16_t port;
	{
		READ_LOCK(*csUsers);
		const auto& i = users.find(cmd.getTo());
		if (i == users.end())
		{
			dcdebug("AdcHub::sendUDP: invalid user\n");
			return;
		}
		const OnlineUserPtr ou = i->second;
		if (!ou->getIdentity().isUdpActive())
		{
			return;
		}
		ip = ou->getIdentity().getConnectIP();
		port = ou->getIdentity().getUdp4Port();
		command = cmd.toString(ou->getUser()->getCID());
	}
	if (!port || !Util::isValidIp(ip)) return;
	SearchManager::getInstance()->addToSendQueue(command, ip, port);
	if (CMD_DEBUG_ENABLED())
		COMMAND_DEBUG("[ADC UDP][" + Util::printIpAddress(ip, true) + ':' + Util::toString(port) + "] " + command, DebugTask::CLIENT_OUT, getIpPort());
}

void AdcHub::handle(AdcCommand::STA, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 2)
		return;
		
	OnlineUserPtr ou;
	if (c.getFrom() == AdcCommand::HUB_SID)
	{
		ou = getUser(c.getFrom(), CID(), c.getNick());
	}
	else
	{
		ou = findUser(c.getFrom());
	}
	if (!ou)
		return;
		
	//int severity = Util::toInt(c.getParam(0).substr(0, 1));
	if (c.getParam(0).size() != 3)
	{
		return;
	}
	lastErrorCode = Util::toInt(c.getParam(0).c_str() + 1);
	switch (lastErrorCode)
	{
	
		case AdcCommand::ERROR_BAD_PASSWORD:
		{
			csState.lock();
			storedPassword.clear();
			csState.unlock();
			break;
		}
		
		case AdcCommand::ERROR_COMMAND_ACCESS:
		{
			string tmp;
			if (c.getParam("FC", 1, tmp) && tmp.size() == 4)
				forbiddenCommands.insert(AdcCommand::toFourCC(tmp.c_str()));
				
			break;
		}
		
		case AdcCommand::ERROR_PROTOCOL_UNSUPPORTED:
		{
			string tmp;
			if (c.getParam("PR", 1, tmp))
			{
				if (tmp == AdcSupports::CLIENT_PROTOCOL)
				{
					ou->getUser()->setFlag(User::NO_ADC_1_0_PROTOCOL);
				}
				else if (tmp == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST)
				{
					ou->getUser()->changeFlags(User::NO_ADCS_0_10_PROTOCOL, User::ADCS);
				}
				// Try again...
				ConnectionManager::getInstance()->force(ou->getUser());
			}
			return;
		}
	}
	unique_ptr<ChatMessage> message(new ChatMessage(c.getParam(1), ou));
	fire(ClientListener::Message(), this, message);
	ClientListener::NickErrorCode nickError = ClientListener::NoError;
	if (lastErrorCode == AdcCommand::ERROR_NICK_INVALID)
		nickError = ClientListener::Rejected;
	else if (lastErrorCode == AdcCommand::ERROR_NICK_TAKEN)
		nickError = ClientListener::Taken;
	else if (lastErrorCode == AdcCommand::ERROR_BAD_PASSWORD)
		nickError = ClientListener::BadPassword;
	if (nickError != ClientListener::NoError)
	{
		csState.lock();
		if (clientSock)
			clientSock->disconnect(false);
		csState.unlock();
		fire(ClientListener::NickError(), nickError);
	}
}

void AdcHub::handle(AdcCommand::SCH, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou)
	{
		dcdebug("Invalid user in AdcHub::onSCH\n");
		return;
	}
	
	fire(ClientListener::AdcSearch(), this, c, ou);
}

void AdcHub::handle(AdcCommand::RES, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou)
	{
		dcdebug("Invalid user in AdcHub::onRES\n");
		return;
	}
	SearchManager::getInstance()->onRES(c, false, ou->getUser(), IpAddress{0});
}

void AdcHub::handle(AdcCommand::PSR, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou)
	{
		dcdebug("Invalid user in AdcHub::onPSR\n");
		if (BOOLSETTING(LOG_PSR_TRACE))
			LOG(PSR_TRACE, "PSR from unknown user " + Identity::getSIDString(c.getFrom()) + " on " + getHubUrl());
		return;
	}
	SearchManager::getInstance()->onPSR(c, false, ou->getUser(), IpAddress{0});
}

void AdcHub::handle(AdcCommand::GET, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Too few parameters for GET", AdcCommand::TYPE_HUB));
		return;
	}

	if (c.getParam(0) != "blom" || !isFeatureSupported(FEATURE_FLAG_SEND_BLOOM))
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC, "Unknown transfer type", AdcCommand::TYPE_HUB));
		return;
	}
	
	string sk, sh;
	if (c.getParameters().size() < 5 || !c.getParam("BK", 4, sk) || !c.getParam("BH", 4, sh))
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Too few parameters for blom", AdcCommand::TYPE_HUB));
		return;
	}
	
	ByteVector v;
	size_t m = Util::toUInt32(c.getParam(3)) * 8;
	size_t k = Util::toUInt32(sk);
	size_t h = Util::toUInt32(sh);
		
	if (k > 8 || k < 1)
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC, "Unsupported k", AdcCommand::TYPE_HUB));
		return;
	}
	if (h > 64 || h < 1)
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC, "Unsupported h", AdcCommand::TYPE_HUB));
		return;
	}
		
	ShareManager* sm = ShareManager::getInstance();
	const size_t n = sm->getSharedTTHCount();
	// When h >= 32, m can't go above 2^h anyway since it's stored in a size_t.
	if (!m || m > 5 * Util::roundUp((size_t)(n * k / log(2.)), (size_t) 64) ||
	    (h < 32 && m > ((size_t) 1 << h)))
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC, "Unsupported m", AdcCommand::TYPE_HUB));
		return;
	}
	sm->getHashBloom(v, k, m, h);
	if (v.empty())
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC, "Internal error", AdcCommand::TYPE_HUB));
		return;
	}

	AdcCommand cmd(AdcCommand::CMD_SND, AdcCommand::TYPE_HUB);
	for (int i = 0; i < 5; i++)
		cmd.addParam(c.getParam(i));
	send(cmd);
	send((const char*) v.data(), v.size());
}

void AdcHub::handle(AdcCommand::NAT, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 3)
		return;

	{
		LOCK(csState);
		if (state != STATE_NORMAL || !(featureFlags & FEATURE_FLAG_ALLOW_NAT_TRAVERSAL))
			return;
	}

	if (SETTING(OUTGOING_CONNECTIONS) != SettingsManager::OUTGOING_DIRECT)
		return;

	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou || ou->getUser()->isMe())
		return;

	const string& protocol = c.getParam(0);
	const string& port = c.getParam(1);
	const string& token = c.getParam(2);

	bool secure;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		secure = false;
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->isInitialized())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}

	IpAddress ip = ou->getIdentity().getConnectIP();
	if (!ip.type)
		return;

	const string userKey = ou->getIdentity().getCID() + ou->getIdentity().getSIDString();
	uint16_t localPort = socketPool.addSocket(userKey, ip.type, false, secure, true, Util::emptyString);
	if (!localPort)
		return;

	dcdebug("triggering connecting attempt in NAT: remote port = %s, local port = %d\n", port.c_str(), localPort);
	ConnectionManager::getInstance()->adcConnect(*ou, ip.type, static_cast<uint16_t>(Util::toInt(port)), localPort, BufferedSocket::NAT_CLIENT, token, secure);

	send(AdcCommand(AdcCommand::CMD_RNT, ou->getIdentity().getSID(), AdcCommand::TYPE_DIRECT).addParam(protocol).
	     addParam(Util::toString(localPort)).addParam(token));
}

void AdcHub::handle(AdcCommand::RNT, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 3)
		return;

	{
		LOCK(csState);
		if (state != STATE_NORMAL || !(featureFlags & FEATURE_FLAG_ALLOW_NAT_TRAVERSAL))
			return;
	}

	if (SETTING(OUTGOING_CONNECTIONS) != SettingsManager::OUTGOING_DIRECT)
		return;

	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou || ou->getUser()->isMe())
		return;

	const string& protocol = c.getParam(0);
	const string& port = c.getParam(1);
	const string& token = c.getParam(2);

	bool secure;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		secure = false;
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->isInitialized())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}

	const string userKey = ou->getIdentity().getCID() + ou->getIdentity().getSIDString() + '*';
	uint16_t localPort;
	int localType;
	if (!socketPool.getPortForUser(userKey, localPort, localType))
		return;

	dcdebug("triggering connecting attempt in RNT: remote port = %s, local port = %d\n", port.c_str(), localPort);
	ConnectionManager::getInstance()->adcConnect(*ou, localType, static_cast<uint16_t>(Util::toInt(port)), localPort, BufferedSocket::NAT_SERVER, token, secure);
}

void AdcHub::connect(const OnlineUserPtr& user, const string& token, bool /*forcePassive*/)
{
	connectUser(*user, token,
		CryptoManager::getInstance()->isInitialized() &&
		(user->getUser()->getFlags() & User::ADCS) != 0,
		false);
}

void AdcHub::connectUser(const OnlineUser& user, const string& token, bool secure, bool revConnect)
{
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
	}

	lastErrorCode = 0;
	const string* proto;
	if (secure)
	{
		if (user.getUser()->getFlags() & User::NO_ADCS_0_10_PROTOCOL)
		{
			/// @todo log
			return;
		}
		proto = &AdcSupports::SECURE_CLIENT_PROTOCOL_TEST;
	}
	else
	{
		if (user.getUser()->getFlags() & User::NO_ADC_1_0_PROTOCOL)
		{
			/// @todo log
			return;
		}
		proto = &AdcSupports::CLIENT_PROTOCOL;
	}
	if (isActive())
	{
		uint16_t port = secure ? ConnectionManager::getInstance()->getSecurePort() : ConnectionManager::getInstance()->getPort();
		if (port == 0)
		{
			// Oops?
			LogManager::message(STRING(NOT_LISTENING));
			return;
		}
		if (send(AdcCommand(AdcCommand::CMD_CTM, user.getIdentity().getSID(), AdcCommand::TYPE_DIRECT).addParam(*proto).addParam(Util::toString(port)).addParam(token)))
		{
			dcassert(!token.empty());
			uint64_t expires = revConnect ? GET_TICK() + 60000 : UINT64_MAX;
			ConnectionManager::getInstance()->adcExpect(token, user.getUser()->getCID(), getHubUrl(), expires);
		}
	}
	else
	{
		send(AdcCommand(AdcCommand::CMD_RCM, user.getIdentity().getSID(), AdcCommand::TYPE_DIRECT).addParam(*proto).addParam(token));
	}
}

void AdcHub::hubMessage(const string& message, bool thirdPerson)
{
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
	}

	AdcCommand cmd(AdcCommand::CMD_MSG, AdcCommand::TYPE_BROADCAST);
	cmd.addParam(message);
	if (thirdPerson)
		cmd.addParam("ME", "1");
	send(cmd);
}

bool AdcHub::privateMessage(const OnlineUserPtr& user, const string& message, int flags)
{
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return false;
	}

	AdcCommand cmd(AdcCommand::CMD_MSG, user->getIdentity().getSID(), AdcCommand::TYPE_ECHO);
	cmd.addParam(message);
	if (flags & PM_FLAG_THIRD_PERSON)
		cmd.addParam("ME", "1");
	cmd.addParam("PM", getMySID());
	send(cmd);

	fireOutgoingPM(user, message, flags);
	return true;
}

void AdcHub::sendUserCmd(const UserCommand& command, const StringMap& params)
{
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
	}

	string cmd = Util::formatParams(command.getCommand(), params, false);
	if (command.isChat())
	{
		if (command.getTo().empty())
		{
			hubMessage(cmd);
		}
		else
		{
			string to = Util::formatParams(command.getTo(), params, false);
			READ_LOCK(*csUsers);
			for (auto i = users.cbegin(); i != users.cend(); ++i)
			{
				if (i->second->getIdentity().getNick() == to)
				{
					privateMessage(i->second, cmd, 0);
					return;
				}
			}
		}
	}
	else
	{
		if (!cmd.empty() && cmd.back() != '\n') cmd += '\n';
		send(cmd);
	}
}

StringList AdcHub::parseSearchExts(int flag)
{
	StringList ret;
	for (size_t i = 0; i < searchExts.size(); ++i)
		if (flag & (1 << i))
			ret.insert(ret.begin(), searchExts[i].begin(), searchExts[i].end());
	return ret;
}

void AdcHub::searchToken(const SearchParam& sp)
{
	{
		LOCK(csState);
		if (state != STATE_NORMAL || hideShare) return;
	}		
	AdcCommand cmd(AdcCommand::CMD_SCH, AdcCommand::TYPE_BROADCAST);
	
	//dcassert(aToken);
	cmd.addParam("TO", Util::toString(sp.token));
	
	if (sp.fileType == FILE_TYPE_TTH)
	{
		cmd.addParam("TR", sp.filter);
	}
	else
	{
		switch (sp.sizeMode)
		{
			case SIZE_ATLEAST:
				if (sp.size > 0) cmd.addParam("GE", Util::toString(sp.size));
				break;
			case SIZE_ATMOST:
				cmd.addParam("LE", Util::toString(sp.size));
				break;
			case SIZE_EXACT:
			{
				string size = Util::toString(sp.size);
				cmd.addParam("GE", size);
				cmd.addParam("LE", size);
			}
		}

		SimpleStringTokenizer<char> include(sp.filter, ' ');
		string tok;
		while (include.getNextNonEmptyToken(tok))
			cmd.addParam("AN", tok);
		
		SimpleStringTokenizer<char> exclude(sp.filterExclude, ' ');
		while (exclude.getNextNonEmptyToken(tok))
			cmd.addParam("NO", tok);

		if (sp.fileType == FILE_TYPE_DIRECTORY)
		{
			cmd.addParam("TY", "2");
		}
		
		if (sp.extList.size() > 2)
		{
			StringList exts = sp.extList;
			sort(exts.begin(), exts.end());
			
			uint8_t gr = 0;
			StringList rx;
			
			const auto& searchExts = getSearchExts();
			for (auto i = searchExts.cbegin(), iend = searchExts.cend(); i != iend; ++i)
			{
				const StringList& def = *i;
				
				// gather the exts not present in any of the lists
				StringList temp(def.size() + exts.size());
				temp = StringList(temp.begin(), set_symmetric_difference(def.begin(), def.end(),
				                                                         exts.begin(), exts.end(), temp.begin()));
				                                                         
				// figure out whether the remaining exts have to be added or removed from the set
				StringList rx_;
				bool ok = true;
				for (auto diff = temp.cbegin(); diff != temp.cend();)
				{
					if (find(def.cbegin(), def.cend(), *diff) == def.cend())
					{
						++diff; // will be added further below as an "EX"
					}
					else
					{
						if (rx_.size() == 2)
						{
							ok = false;
							break;
						}
						rx_.push_back(*diff);
						diff = temp.erase(diff);
					}
				}
				if (!ok) // too many "RX"s necessary - disregard this group
					continue;
					
				// let's include this group!
				gr += 1 << (i - searchExts.cbegin());
				
				exts = std::move(temp); // the exts to still add (that were not defined in the group)
				
				rx.insert(rx.begin(), rx_.begin(), rx_.end());
				
				if (exts.size() <= 2)
					break;
				// keep looping to see if there are more exts that can be grouped
			}
			
			if (gr)
			{
				// some extensions can be grouped; let's send a command with grouped exts.
				AdcCommand c_gr(AdcCommand::CMD_SCH, AdcCommand::TYPE_FEATURE);
				c_gr.setFeatures('+' + AdcSupports::SEGA_FEATURE);
				
				const auto& params = cmd.getParameters();
				for (auto i = params.cbegin(), iend = params.cend(); i != iend; ++i)
					c_gr.addParam(*i);
					
				for (auto i = exts.cbegin(), iend = exts.cend(); i != iend; ++i)
					c_gr.addParam("EX", *i);
				c_gr.addParam("GR", Util::toString(gr));
				for (auto i = rx.cbegin(), iend = rx.cend(); i != iend; ++i)
					c_gr.addParam("RX", *i);
					
				sendSearch(c_gr, sp.searchMode);
				
				// make sure users with the feature don't receive the search twice.
				cmd.setType(AdcCommand::TYPE_FEATURE);
				cmd.setFeatures('-' + AdcSupports::SEGA_FEATURE);
			}
		}
		
		for (auto i = sp.extList.cbegin(), iend = sp.extList.cend(); i != iend; ++i)
		{
			cmd.addParam("EX", *i);
		}
	}
	sendSearch(cmd, sp.searchMode);
}

void AdcHub::sendSearch(AdcCommand& c, SearchParamBase::SearchMode searchMode)
{
	bool active;
	if (searchMode == SearchParamBase::MODE_DEFAULT)
		active = isActive();
	else
		active = searchMode != SearchParamBase::MODE_PASSIVE;	
	if (!active)
	{
		c.setType(AdcCommand::TYPE_FEATURE);
		string features = c.getFeatures();
		if (isFeatureSupported(FEATURE_FLAG_ALLOW_NAT_TRAVERSAL))
		{
			c.setFeatures(features + '+' + AdcSupports::TCP4_FEATURE + '-' + AdcSupports::NAT0_FEATURE);
			send(c);
			c.setFeatures(features + "+" + AdcSupports::NAT0_FEATURE);
		}
		else
		{
			c.setFeatures(features + "+" + AdcSupports::TCP4_FEATURE);
		}
	}
	send(c);
}

void AdcHub::password(const string& pwd, bool setPassword)
{
	AdcCommand c(AdcCommand::CMD_PAS, AdcCommand::TYPE_HUB);
	{
		LOCK(csState);
		if (setPassword)
			storedPassword = pwd;
		if (state != STATE_VERIFY)
			return;
		if (!salt.empty())
		{
			size_t saltBytes = salt.size() * 5 / 8;
			std::unique_ptr<uint8_t[]> buf(new uint8_t[saltBytes]);
			Util::fromBase32(salt.c_str(), &buf[0], saltBytes);
			TigerHash th;
			if (featureFlags & FEATURE_FLAG_OLD_PASSWORD)
			{
				const CID cid = getMyIdentity().getUser()->getCID();
				th.update(cid.data(), CID::SIZE);
			}
			th.update(pwd.data(), pwd.length());
			th.update(&buf[0], saltBytes);
			salt.clear();
			c.addParam(Util::toBase32(th.finalize(), TigerHash::BYTES));
		}
	}
	send(c);
}

void AdcHub::addInfoParam(AdcCommand& c, const string& var, const string& value)
{
	LOCK(csState);
	auto i = lastInfoMap.find(var);
	
	if (i != lastInfoMap.end())
	{
		if (i->second != value)
		{
			if (value.empty())
				lastInfoMap.erase(i);
			else
				i->second = value;
			c.addParam(var, value);
		}
	}
	else if (!value.empty())
	{
		lastInfoMap.emplace(var, value);
		c.addParam(var, value);
	}
}

bool AdcHub::resendMyINFO(bool alwaysSend, bool forcePassive)
{
	return false;
}

void AdcHub::info(bool/* forceUpdate*/)
{
	csState.lock();
	States state = this->state;
	string nick = myNick;
	csState.unlock();

	if (state != STATE_IDENTIFY && state != STATE_NORMAL)
		return;
		
	reloadSettings(false);
	
	AdcCommand c(AdcCommand::CMD_INF, AdcCommand::TYPE_BROADCAST);
	c.getParameters().reserve(20);
	
	if (state == STATE_NORMAL)
		updateCounts(false);
	
	addInfoParam(c, "ID", ClientManager::getMyCID().toBase32());
	addInfoParam(c, "PD", ClientManager::getMyPID().toBase32());
	addInfoParam(c, "NI", nick);
	addInfoParam(c, "DE", getCurrentDescription());
	addInfoParam(c, "SL", Util::toString(getSlots()));
	addInfoParam(c, "FS", Util::toString(getFreeSlots()));
	if (hideShare)
	{
		addInfoParam(c, "SS", "0");
		addInfoParam(c, "SF", "0");
	}
	else if (fakeShareSize >= 0)
	{
		addInfoParam(c, "SS", Util::toString(fakeShareSize));
		int64_t fileCount = 0;
		if (fakeShareSize)
		{
			fileCount = fakeShareFiles;
			if (fileCount <= 0) fileCount = (fakeShareSize + averageFakeFileSize - 1)/averageFakeFileSize;
		}
		addInfoParam(c, "SF", Util::toString(fileCount));
	}
	else
	{
		int64_t size, files;
		ShareManager::getInstance()->getShareGroupInfo(shareGroup, size, files);
		addInfoParam(c, "SS", Util::toString(size));
		addInfoParam(c, "SF", Util::toString(files));
	}
	
	
	addInfoParam(c, "EM", SETTING(EMAIL));
	// Exclusive hub mode
	if (fakeHubCount)
	{
		unsigned normal, registered, op;
		getFakeCounts(normal, registered, op);
		addInfoParam(c, "HN", Util::toString(normal));
		addInfoParam(c, "HR", Util::toString(registered));
		addInfoParam(c, "HO", Util::toString(op));
	}
	else
	{
		uint32_t normal = g_counts[COUNT_NORMAL];
		uint32_t registered = g_counts[COUNT_REGISTERED];
		uint32_t op = g_counts[COUNT_OP];
		if (normal + registered + op == 0) normal = 1; // fix H:0/0/0
		addInfoParam(c, "HN", Util::toString(normal));
		addInfoParam(c, "HR", Util::toString(registered));
		addInfoParam(c, "HO", Util::toString(op));
	}
	addInfoParam(c, "AP", getClientName());
	addInfoParam(c, "VE", getClientVersion());
	addInfoParam(c, "AW", Util::getAway() ? "1" : Util::emptyString);
	
	size_t limit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getDownloadLimitInBytes() : 0;
	if (limit > 0)
	{
		addInfoParam(c, "DS", Util::toString(limit));
	}
	
	limit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getUploadLimitInBytes() : 0;
	if (limit > 0)
	{
		addInfoParam(c, "US", Util::toString(limit));
	}
	else
	{
		limit = Util::toInt64(SETTING(UPLOAD_SPEED)) * 1024 * 1024 / 8;
		addInfoParam(c, "US", Util::toString(limit));
	}
	
	addInfoParam(c, "LC", Util::getIETFLang()); // http://adc.sourceforge.net/ADC-EXT.html#_lc_locale_specification
	
	string su(AdcSupports::SEGA_FEATURE);
	
	auto cryptoManager = CryptoManager::getInstance();
	if (cryptoManager->isInitialized())
	{
		su += "," + AdcSupports::ADCS_FEATURE;
		ByteVector kp;
		cryptoManager->getCertFingerprint(kp);
		if (!kp.empty())
			addInfoParam(c, "KP", "SHA256/" + Util::toBase32(kp.data(), kp.size()));
	}
	
	const bool optionNatTraversal = BOOLSETTING(ALLOW_NAT_TRAVERSAL);
	csState.lock();
	if (optionNatTraversal)
		featureFlags |= FEATURE_FLAG_ALLOW_NAT_TRAVERSAL;
	else
		featureFlags &= ~FEATURE_FLAG_ALLOW_NAT_TRAVERSAL;
	csState.unlock();

	bool active = isActive();
	bool hasIP = false;
	Ip4Address ip4;
	Ip6Address ip6;
	getLocalIp(ip4, ip6);

	uint16_t udpPort = SearchManager::getUdpPort();
	if (ip4)
	{
		hasIP = true;
		addInfoParam(c, "I4", Util::printIpAddress(ip4));
		if (active)
		{
			su += "," + AdcSupports::TCP4_FEATURE;
			if (udpPort)
			{
				addInfoParam(c, "U4", Util::toString(udpPort));
				su += "," + AdcSupports::UDP4_FEATURE;
			}
		}
	}
	if (!Util::isEmpty(ip6))
	{
		hasIP = true;
		addInfoParam(c, "I6", Util::printIpAddress(ip6));
		if (active)
		{
			su += "," + AdcSupports::TCP6_FEATURE;
			if (udpPort)
			{
				addInfoParam(c, "U6", Util::toString(udpPort));
				su += "," + AdcSupports::UDP6_FEATURE;
			}
		}
	}
	if (!active && hasIP && optionNatTraversal)
		su += ',' + AdcSupports::NAT0_FEATURE;
	if (BOOLSETTING(USE_CCPM))
		su += ',' + AdcSupports::CCPM_FEATURE;

	addInfoParam(c, "SU", su);

	if (!c.getParameters().empty())
		send(c);
}

void AdcHub::refreshUserList(bool)
{
	OnlineUserList v;
	{
		READ_LOCK(*csUsers);
		for (auto i = users.cbegin(); i != users.cend(); ++i)
		{
			if (i->first != AdcCommand::HUB_SID)
			{
				v.push_back(i->second);
			}
		}
	}
	fireUserListUpdated(v);
}

void AdcHub::checkNick(string& nick) const noexcept
{
	for (size_t i = 0; i < nick.size(); ++i)
		if (static_cast<uint8_t>(nick[i]) <= 32)
			nick[i] = '_';
}

bool AdcHub::send(const AdcCommand& cmd)
{
	if (forbiddenCommands.find(AdcCommand::toFourCC(cmd.getFourCC().c_str())) != forbiddenCommands.end())
		return false;
	if (cmd.getType() == AdcCommand::TYPE_UDP)
		sendUDP(cmd);
	send(cmd.toString(sid));
	return true;
}

void AdcHub::unknownProtocol(uint32_t target, const string& protocol, const string& token)
{
	AdcCommand cmd(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_DIRECT);
	cmd.setTo(target);
	cmd.addParam("PR", protocol);
	cmd.addParam("TO", token);
	
	send(cmd);
}

void AdcHub::onConnected() noexcept
{
	Client::onConnected();

	const bool optionUserCommands = BOOLSETTING(HUB_USER_COMMANDS) && SETTING(MAX_HUB_USER_COMMANDS) > 0;
	const bool optionSendBloom = BOOLSETTING(SEND_BLOOM);

	userListLoaded = true;
	{
		LOCK(csState);
		if (state != STATE_PROTOCOL)
			return;
		lastInfoMap.clear();
		featureFlags &= ~(FEATURE_FLAG_USER_COMMANDS | FEATURE_FLAG_SEND_BLOOM);
		if (optionUserCommands)
			featureFlags |= FEATURE_FLAG_USER_COMMANDS;
		if (optionSendBloom)
			featureFlags |= FEATURE_FLAG_SEND_BLOOM;
	}
	sid = 0;
	forbiddenCommands.clear();
	
	AdcCommand cmd(AdcCommand::CMD_SUP, AdcCommand::TYPE_HUB);
	cmd.addParam(AdcSupports::BAS0_SUPPORT).addParam(AdcSupports::BASE_SUPPORT).addParam(AdcSupports::TIGR_SUPPORT);
	
	if (optionUserCommands)
		cmd.addParam(AdcSupports::UCM0_SUPPORT);
	if (optionSendBloom)
		cmd.addParam(AdcSupports::BLO0_SUPPORT);
	cmd.addParam(AdcSupports::ZLIF_SUPPORT);

	send(cmd);
}

void AdcHub::onDataLine(const string& aLine) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		Client::onDataLine(aLine);
		
		if (!Text::validateUtf8(aLine))
		{
			// @todo report to user?
			return;
		}
		dispatch(aLine);
	}
}

void AdcHub::onFailed(const string& aLine) noexcept
{
	clearUsers();
	Client::onFailed(aLine);
}

void AdcHub::getUsersToCheck(UserList& res, int64_t tick, int timeDiff) const noexcept
{
	READ_LOCK(*csUsers);
	for (SIDMap::const_iterator i = users.cbegin(); i != users.cend(); ++i)
	{
		const OnlineUserPtr& ou = i->second;
		if (ou->getIdentity().isBotOrHub()) continue;
		const UserPtr& user = ou->getUser();
		if (user->getFlags() & (User::USER_CHECK_RUNNING | User::MYSELF)) continue;
		int64_t lastCheckTime = user->getLastCheckTime();
		if (lastCheckTime && lastCheckTime + timeDiff > tick) continue;
		res.push_back(user);
	}
}
