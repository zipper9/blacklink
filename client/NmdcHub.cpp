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
#include <boost/algorithm/string.hpp>

#include "NmdcHub.h"
#include "SettingsManager.h"
#include "ConnectionManager.h"
#include "SearchManager.h"
#include "ShareManager.h"
#include "CryptoManager.h"
#include "UserCommand.h"
#include "DebugManager.h"
#include "QueueManager.h"
#include "UploadManager.h"
#include "ThrottleManager.h"
#include "FavoriteManager.h"
#include "ParamExpander.h"
#include "StringTokenizer.h"
#include "SimpleStringTokenizer.h"
#include "MappingManager.h"
#include "CompatibilityManager.h"
#include "LogManager.h"
#include "Random.h"
#include "Tag16.h"
#include "SocketPool.h"
#include "AdcSupports.h"
#include "Util.h"
#include "ConfCore.h"
#include "version.h"

#ifdef BL_FEATURE_NMDC_EXT_JSON
#include "NmdcExtJson.h"
#include "JsonFormatter.h"
#endif

static const string abracadabraLock("EXTENDEDPROTOCOLABCABCABCABCABCABC");
static const string abracadabraPk("DCPLUSPLUS" DCVERSIONSTRING);

#ifdef _DEBUG
bool suppressUserConn = false;
#endif

#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
#include "TagCollector.h"
extern TagCollector collNmdcFeatures;
#endif

#ifdef BL_FEATURE_COLLECT_UNKNOWN_TAGS
#include "TagCollector.h"
extern TagCollector collNmdcTags;
#endif

extern IpBans udpBans;
extern IpBans tcpBans;

enum
{
	ST_NONE,
	ST_SEARCH,
	ST_SA,
	ST_SP
};

ClientBasePtr NmdcHub::create(const string& hubURL, const string& address, uint16_t port, bool secure)
{
	return std::shared_ptr<Client>(static_cast<Client*>(new NmdcHub(hubURL, address, port, secure)));
}

NmdcHub::NmdcHub(const string& hubURL, const string& address, uint16_t port, bool secure) :
	Client(hubURL, address, port, '|', secure, Socket::PROTO_NMDC),
	hubSupportFlags(0),
	lastModeChar(0),
	lastUpdate(0),
	lastNatUserExpires(0),
	myInfoState(WAITING_FOR_MYINFO),
	csUsers(std::unique_ptr<RWLock>(RWLock::create()))
{
}

NmdcHub::~NmdcHub()
{
	dcassert(users.empty());
}

void NmdcHub::disconnect(bool graceless)
{
	Client::disconnect(graceless);
	clearUsers();
}

void NmdcHub::connect(const OnlineUserPtr& user, const string& token, bool forcePassive)
{
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
	}
	dcdebug("NmdcHub::connect %s\n", user->getIdentity().getNick().c_str());
	if (!forcePassive && isActive())
		connectToMe(*user, token);
	else
		revConnectToMe(*user);
}

void NmdcHub::refreshUserList(bool refreshOnly)
{
	if (refreshOnly)
	{
		OnlineUserList v;
		{
			READ_LOCK(*csUsers);
			v.reserve(users.size());
			for (auto i = users.cbegin(); i != users.cend(); ++i)
			{
				v.push_back(i->second);
			}
		}
		fireUserListUpdated(v);
	}
	else
	{
		clearUsers();
		getNickList();
	}
}

OnlineUserPtr NmdcHub::getUser(const string& nick)
{
	OnlineUserPtr ou;
	{
		csState.lock();
		bool isMyNick = nick == myNick;
		csState.unlock();

		csUsers->acquireExclusive();
#if 0
		if (hub)
		{
			dcassert(users.count(nick) == 0);
			ou = users.insert(make_pair(nick, getHubOnlineUser())).first->second;
			dcassert(ou->getIdentity().getNick() == nick);
			ou->getIdentity().setNick(nick);
		}
		else
#endif
		if (isMyNick)
		{
//			dcassert(users.count(nick) == 0);
			auto res = users.insert(make_pair(nick, getMyOnlineUser()));
			if (res.second)
			{
				ou = res.first->second;
				ou->getUser()->addNick(nick, getHubUrl());
				dcassert(ou->getIdentity().getNick() == nick);
			}
			else
			{
				dcassert(res.first->second->getIdentity().getNick() == nick);
				ou = res.first->second;
				csUsers->releaseExclusive();
				return ou;
			}
		}
		else
		{
			auto res = users.insert(make_pair(nick, OnlineUserPtr()));
			if (res.second)
			{
				UserPtr p = ClientManager::getUser(nick, getHubUrl());
				ou = std::make_shared<OnlineUser>(p, getClientPtr(), 0);
				ou->getIdentity().setNick(nick);
				res.first->second = ou;
			}
			else
			{
				dcassert(res.first->second->getIdentity().getNick() == nick);
				ou = res.first->second;
				csUsers->releaseExclusive();
				return ou;
			}
		}
	}
	csUsers->releaseExclusive();
	if (!ou->getUser()->getCID().isZero())
		ClientManager::getInstance()->putOnline(ou, true);
	return ou;
}

OnlineUserPtr NmdcHub::findUser(const string& nick) const
{
	READ_LOCK(*csUsers);
	const auto& i = users.find(nick);
	return i == users.end() ? OnlineUserPtr() : i->second;
}

void NmdcHub::putUser(const string& nick)
{
	OnlineUserPtr ou;
	{
		WRITE_LOCK(*csUsers);
		const auto& i = users.find(nick);
		if (i == users.end())
			return;
		auto bytesShared = i->second->getIdentity().getBytesShared();
		ou = i->second;
		users.erase(i);
		decBytesShared(bytesShared);
	}
	
	if (!ou->getUser()->getCID().isZero())
		ClientManager::getInstance()->putOffline(ou);

	fire(ClientListener::UserRemoved(), this, ou);
}

void NmdcHub::clearUsers()
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
		NickMap u2;
		{
			WRITE_LOCK(*csUsers);
			u2.swap(users);
			bytesShared.store(0);
		}
		for (auto i = u2.cbegin(); i != u2.cend(); ++i)
		{
			//i->second->getIdentity().setBytesShared(0);
			if (!i->second->getUser()->getCID().isZero())
				ClientManager::getInstance()->putOffline(i->second);
			else
				dcassert(0);
		}
	}
}

void NmdcHub::updateFromTag(Identity& id, const string& tag)
{
	SimpleStringTokenizer<char> st(tag, ',');
	string::size_type j;
	id.setLimit(0);
	string tok;
	while (st.getNextNonEmptyToken(tok))
	{
		if (tok.length() < 2)
			continue;

		else if (tok.compare(0, 2, "H:", 2) == 0)
		{
			unsigned u[3];
			int items = sscanf(tok.c_str() + 2, "%u/%u/%u", &u[0], &u[1], &u[2]);
			if (items != 3)
				continue;
			id.setHubsNormal(u[0]);
			id.setHubsRegistered(u[1]);
			id.setHubsOperator(u[2]);
		}
		else if (tok.compare(0, 2, "S:", 2) == 0)
		{
			const uint16_t slots = Util::toInt(tok.c_str() + 2);
			id.setSlots(slots);
		}
		else if (tok.compare(0, 2, "M:", 2) == 0)
		{
			if (tok.length() == 3)
			{
				if (tok[2] == 'A')
				{
					id.getUser()->unsetFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
					id.setStatusBit(Identity::SF_PASSIVE, false);
				}
				else
				{
					id.getUser()->setFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
					id.setStatusBit(Identity::SF_PASSIVE, true);
				}
			}
		}
		else if ((j = tok.find("V:")) != string::npos || (j = tok.find("v:")) != string::npos)
		{
			if (j > 1)
				id.setStringParam("AP", tok.substr(0, j - 1));
			id.setStringParam("VE", tok.substr(j + 2));
		}
		else if ((j = tok.find("L:")) != string::npos)
		{
			const uint32_t limit = Util::toInt(tok.c_str() + j + 2);
			id.setLimit(limit * 1024);
		}
		else if ((j = tok.find(' ')) != string::npos)
		{
			if (j > 1)
				id.setStringParam("AP", tok.substr(0, j - 1));
			id.setStringParam("VE", tok.substr(j + 1));
		}
		else if ((j = tok.find("++")) != string::npos)
		{
			id.setStringParam("AP", tok);
		}
		else if (tok.compare(0, 2, "O:", 2) == 0)
		{
			// [?] TODO http://nmdc.sourceforge.net/NMDC.html#_tag
		}
		else if (tok.compare(0, 2, "C:", 2) == 0)
		{
			// http://dchublist.ru/forum/viewtopic.php?p=24035#p24035
		}
#ifdef BL_FEATURE_COLLECT_UNKNOWN_TAGS
		else
		{
			collNmdcTags.addTag(tok, getHubUrl());
		}
#endif
	}
}

void NmdcHub::handleSearch(const NmdcSearchParam& searchParam)
{
	ClientManagerListener::SearchReply reply = ClientManagerListener::SEARCH_MISS;
	vector<SearchResultCore> searchResults;
	dcassert(searchParam.maxResults > 0);
	if (ClientManager::isBeforeShutdown())
		return;
	ShareManager::getInstance()->search(searchResults, searchParam, this);
	if (!searchResults.empty())
	{
		if (LogManager::getLogOptions() & LogManager::OPT_LOG_SEARCH)
		{
			string seeker = searchParam.searchMode == SearchParamBase::MODE_PASSIVE ? searchParam.seeker.substr(4) : searchParam.seeker;
			string message;
			if (searchResults.size() > 1)
			{
				string found = STRING_F(SEARCH_HIT_MULTIPLE, Util::toString(searchResults.size()) % searchResults[0].getFile());
				message = STRING_F(SEARCH_HIT_INFO, seeker % getHubUrl() % searchParam.filter % found);
			}
			else
				message = STRING_F(SEARCH_HIT_INFO, seeker % getHubUrl() % searchParam.filter % searchResults[0].getFile());
			LOG(SEARCH_TRACE, message);
		}

		reply = ClientManagerListener::SEARCH_HIT;
		unsigned slots = UploadManager::getSlots();
		unsigned freeSlots = UploadManager::getFreeSlots();
		if (searchParam.searchMode == SearchParamBase::MODE_PASSIVE)
		{
			const string name = searchParam.seeker.substr(4);
			// Good, we have a passive seeker, those are easier...
			string str;
			for (auto i = searchResults.cbegin(); i != searchResults.cend(); ++i)
			{
				const auto& sr = *i;
				str += sr.toSR(*this, freeSlots, slots);
				str[str.length() - 1] = 5;
				str += fromUtf8(name);
				str += '|';
			}
			
			if (!str.empty())
				send(str);
		}
		else
		{
			try
			{
				for (auto i = searchResults.cbegin(); i != searchResults.cend(); ++i)
				{
					string sr = i->toSR(*this, freeSlots, slots);
#if 0
					if (ConnectionManager::checkDuplicateSearchFile(sr))
					{
						if (CMD_DEBUG_ENABLED())
							COMMAND_DEBUG("[~][0]$SR [SkipUDP-File] " + sr, DebugTask::HUB_IN, getIpPort());
					}
					else
#endif
					{
						uint16_t port = 412;
						string address;
						Util::parseIpPort(searchParam.seeker, address, port);
						sendUDP(address, port, sr);
					}
				}
			}
			catch (Exception& e)
			{
#ifdef _DEBUG
				LogManager::message("ClientManager::on(NmdcSearch, Search caught error= " + e.getError());
#endif
				dcdebug("Search caught error = %s\n", + e.getError().c_str());
			}
		}
	}
	else
	{
		if (searchParam.searchMode != SearchParamBase::MODE_PASSIVE)
		{
			if (handlePartialSearch(searchParam))
				reply = ClientManagerListener::SEARCH_PARTIAL_HIT;
		}
	}
	ClientManager::getInstance()->fireIncomingSearch(TYPE_NMDC, searchParam.seeker, getHubUrl(), searchParam.filter, reply);
}

bool NmdcHub::handlePartialSearch(const NmdcSearchParam& searchParam)
{
	if (searchParam.fileType == FILE_TYPE_TTH && Util::isTTHBase32(searchParam.filter))
	{
		QueueItem::PartsInfo outParts;
		uint64_t blockSize;
		TTHValue tth(searchParam.filter.c_str() + 4);
		if (QueueManager::handlePartialSearch(tth, outParts, blockSize))
		{
			string ip;
			uint16_t port = 0;
			Util::parseIpPort(searchParam.seeker, ip, port);
			dcassert(searchParam.seeker == ip + ':' + Util::toString(port));
			if (port == 0)
				return false;
			IpAddress addr;
			if (!Util::parseIpAddress(addr, ip))
				return false;

			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			SearchManager::toPSR(cmd, true, getMyNick(), addr.type, getIpPort(), tth.toBase32(), outParts);
			if (LogManager::getLogOptions() & LogManager::OPT_LOG_PSR)
			{
				string msg = tth.toBase32() + ": sending PSR search result to ";
				msg += Util::printIpAddress(addr, true) + ':' + Util::toString(port);
				msg += ", hub " + getHubUrl();
				msg += ", we have " + Util::toString(QueueItem::countParts(outParts)) + '*' + Util::toString(blockSize);
				LOG(PSR_TRACE, msg);
			}
			string str = cmd.toString(ClientManager::getMyCID());
			sendUDP(ip, port, str);
			return true;
		}
	}
	return false;
}

void NmdcHub::sendUDP(const string& ip, uint16_t port, string& sr)
{
	if (!port) return;
	IpAddress addr;
	if (!(Util::parseIpAddress(addr, ip) && Util::isValidIp(addr))) return;
	SearchManager::getInstance()->addToSendQueue(sr, addr, port);
	if (CMD_DEBUG_ENABLED())
		COMMAND_DEBUG("[Active-Search]" + sr, DebugTask::CLIENT_OUT, ip + ':' + Util::toString(port));
}

bool NmdcHub::getMyExternalIP(IpAddress& ip) const
{
	Ip4Address ip4;
	Ip6Address ip6;
	getLocalIp(ip4, ip6);
	if (Util::isValidIp4(ip4))
	{
		ip.type = AF_INET;
		ip.data.v4 = ip4;
		return true;
	}
	if (Util::isValidIp6(ip6))
	{
		ip.type = AF_INET6;
		ip.data.v6 = ip6;
		return true;
	}
	return false;
}

void NmdcHub::getMyUDPAddr(string& ip, uint16_t& port) const
{
	Ip4Address ip4;
	Ip6Address ip6;
	getLocalIp(ip4, ip6);
	if (Util::isValidIp4(ip4))
	{
		port = SearchManager::getSearchPort(AF_INET);
		ip = Util::printIpAddress(ip4);
		return;
	}
	if (Util::isValidIp6(ip6))
	{
		port = SearchManager::getSearchPort(AF_INET6);
		ip = Util::printIpAddress(ip6);
		return;
	}
	ip = "0.0.0.0";
	port = 0;
}

bool NmdcHub::getShareGroup(const string& seeker, CID& shareGroup, bool& hideShare) const
{
	hideShare = false;
	const CID cid = ClientManager::makeCid(seeker, getHubUrl());
	FavoriteManager::LockInstanceUsers lockedInstance;
	const auto& users = lockedInstance.getFavoriteUsersL();
	auto i = users.find(cid);
	if (i == users.cend()) return false;
	const FavoriteUser& favUser = i->second;
	if (favUser.isAnySet(FavoriteUser::FLAG_HIDE_SHARE))
	{
		hideShare = true;
		return true;
	}
	if (!favUser.shareGroup.isZero())
	{
		shareGroup = favUser.shareGroup;
		return true;
	}
	return false;
}

void NmdcHub::searchParse(const string& param, int type)
{
	if (param.length() < 4) return;
	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL || hideShare) return;
		myNick = this->myNick;
	}

	NmdcSearchParam searchParam;
	bool isPassive;
	int searchOptions = SearchManager::getInstance()->getOptions();
	
	if (type == ST_SEARCH)
	{	
		isPassive = param.compare(0, 4, "Hub:", 4) == 0;

		string::size_type i = 0;
		string::size_type j = param.find(' ', i);
		if (j == string::npos || i == j)
			return;

		searchParam.seeker = param.substr(i, j - i);

		// Filter own searches
		if (isPassive)
		{
			if (searchParam.seeker.compare(4, myNick.length(), myNick) == 0)
				return;
			bool hideShare;
			if (getShareGroup(searchParam.seeker.substr(4), searchParam.shareGroup, hideShare))
			{
				if (hideShare) return;
			}
			else
				searchParam.shareGroup = shareGroup;
		}
		else if (isActive())
		{
			string myIP;
			uint16_t myPort;
			getMyUDPAddr(myIP, myPort);
			if (!myPort)
				return;
			if (searchParam.seeker == myIP + ":" + Util::toString(myPort))
				return;
		}
		i = j + 1;
		if (param.length() < i + 4)
			return;
		if (param[i + 1] != '?' || param[i + 3] != '?')
			return;
		string::size_type queryPos = i;
		if (param[i] == 'F')
			searchParam.sizeMode = SIZE_DONTCARE;
		else if (param[i + 2] == 'F')
			searchParam.sizeMode = SIZE_ATLEAST;
		else
			searchParam.sizeMode = SIZE_ATMOST;
		i += 4;
		j = param.find('?', i);
		if (j == string::npos || i == j)
			return;
		if (j - i == 1 && param[i] == '0')
			searchParam.size = 0;
		else
			searchParam.size = Util::toInt64(param.c_str() + i);
		i = j + 1;
		j = param.find('?', i);
		if (j == string::npos || i == j)
			return;

		searchParam.fileType = Util::toInt(param.c_str() + i) - 1;
		i = j + 1;

		if (searchParam.fileType == FILE_TYPE_TTH)
		{
			if (param.length() - i == 39 + 4)
				searchParam.filter = param.substr(i);
		}
		else
		{
			searchParam.filter = unescape(param.substr(i));
			searchParam.cacheKey = param.substr(queryPos);
			if (!searchParam.shareGroup.isZero())
			{
				searchParam.cacheKey += '|';
				searchParam.cacheKey += searchParam.shareGroup.toBase32();
			}
		}

		if (searchParam.filter.empty())
			return;
	}
	else
	{
		if (param.length() < 41 || param[39] != ' ') return;
		if (!Util::isBase32(param.c_str(), 39)) return;
		searchParam.filter = param.substr(0, 39);
		searchParam.filter.insert(0, "TTH:", 4);
		searchParam.seeker = param.substr(40);
		isPassive = type == ST_SP;
		if (isPassive)
		{
			if (searchParam.seeker.compare(0, myNick.length(), myNick) == 0)
				return;
			searchParam.seeker.insert(0, "Hub:", 4);
		}
		searchParam.fileType = FILE_TYPE_TTH;
	}

	if (searchParam.fileType != FILE_TYPE_TTH && (searchOptions & SearchManager::OPT_INCOMING_SEARCH_TTH_ONLY))
	{
		ClientManager::getInstance()->fireIncomingSearch(TYPE_NMDC, searchParam.seeker, getHubUrl(), searchParam.filter, ClientManagerListener::SEARCH_MISS);
		return;
	}

	if (!isPassive)
	{
		uint16_t port = 0;
		IpAddress ip;
		string host;
		if (!Util::parseIpPort(searchParam.seeker, host, port) || !port || !Util::parseIpAddress(ip, host))
			return;
		if ((searchOptions & SearchManager::OPT_INCOMING_SEARCH_IGNORE_BOTS) && ip == getIp())
		{
			ClientManager::getInstance()->fireIncomingSearch(TYPE_NMDC, searchParam.seeker, getHubUrl(), searchParam.filter, ClientManagerListener::SEARCH_MISS);
			if (LogManager::getLogOptions() & LogManager::OPT_LOG_SEARCH)
			{
				string message = STRING_F(SEARCH_HIT_IGNORED, searchParam.seeker % getHubUrl() % searchParam.filter);
				LOG(SEARCH_TRACE, message);
			}
			return;
		}
		if (!checkSearchFlood(ip, port))
			return;
	}
	else
	{
		if (searchOptions & SearchManager::OPT_INCOMING_SEARCH_IGNORE_PASSIVE)
		{
			ClientManager::getInstance()->fireIncomingSearch(TYPE_NMDC, searchParam.seeker, getHubUrl(), searchParam.filter, ClientManagerListener::SEARCH_MISS);
			return;
		}
		OnlineUserPtr u = findUser(searchParam.seeker.substr(4));
		if (!u)
			return;

		u->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);

		// ignore if we or remote client don't support NAT traversal in passive mode although many NMDC hubs won't send us passive if we're in passive too, so just in case...
		if (!isActive() && (!(u->getUser()->getFlags() & User::NAT0) || !allowNatTraversal()))
		{
			return;
		}
	}
	searchParam.maxResults = isPassive ? SearchParamBase::MAX_RESULTS_PASSIVE : SearchParamBase::MAX_RESULTS_ACTIVE;
	searchParam.searchMode = isPassive ? SearchParamBase::MODE_PASSIVE : SearchParamBase::MODE_ACTIVE;
	if (!searchParam.cacheKey.empty())
		searchParam.cacheKey.insert(0, Util::toString(searchParam.maxResults) + '=');
	handleSearch(searchParam);
}

void NmdcHub::revConnectToMeParse(const string& param)
{
	string myNick;
	string natUser;
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
		myNick = this->myNick;
		natUser = lastNatUser;
	}

	string::size_type j = param.find(' ');
	if (j == string::npos)
		return;

	OnlineUserPtr u = findUser(param.substr(0, j));
	if (!u)
		return;

	auto flags = u->getUser()->getFlags();
		
	if (isActive())
	{
		connectToMe(*u, Util::emptyString);
	}
	else if (allowNatTraversal() && (flags & User::NAT0) && !u->getIdentity().getStatusBit(Identity::SF_SOCKS))
	{
		string userKey = u->getUser()->getCID().toBase32();
		if (!natUser.empty() && natUser != userKey)
			return;
		if (u->getIdentity().getConnectIP().type == AF_INET6)
			return;
		IpAddress localIp;
		if (!getMyExternalIP(localIp) || localIp.type == AF_INET6)
			return;
		bool secure = CryptoManager::getInstance()->isInitialized() && (flags & User::TLS);
		// NMDC v2.205 supports "$ConnectToMe sender_nick remote_nick ip:port", but many NMDC hubsofts block it
		// sender_nick at the end should work at least in most used hubsofts
		uint16_t port = socketPool.addSocket(userKey, AF_INET, true, secure, true, Util::emptyString);
		if (port)
		{
			send("$ConnectToMe " + fromUtf8(u->getIdentity().getNick()) + ' ' + Util::printIpAddress(localIp) + ':' +
				Util::toString(port) +
				(secure ? "NS " : "N ") + fromUtf8(myNick) + '|');
			if (natUser.empty())
			{
				uint64_t expires = GET_TICK() + 60000;
				LOCK(csState);
				lastNatUser = std::move(userKey);
				lastNatUserExpires = expires;
			}
		}
	}
	else if (!(flags & User::NMDC_FILES_PASSIVE))
	{
#if 0 // set NMDC_FILES_PASSIVE because we got $RevConnectToMe from this user
		u->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
#else
		revConnectToMe(*u); // reply with our $RevConnectToMe
#endif
	}
}

bool NmdcHub::checkConnectToMeFlood(const IpAddress& ip, uint16_t port)
{
	bool showMessage;
	if (reqConnectToMe.addRequest(tcpBans, ip, port, GET_TICK(), getHubUrl(), showMessage)) return true;
	if (showMessage)
	{
		string addr = Util::printIpAddress(ip, true) + ':' + Util::toString(port);
		fire(ClientListener::StatusMessage(), this, STRING_F(ANTIFLOOD_MESSAGE, "ConnectToMe" % addr));
	}
	return false;
}

bool NmdcHub::checkSearchFlood(const IpAddress& ip, uint16_t port)
{
	bool showMessage;
	if (reqSearch.addRequest(udpBans, ip, port, GET_TICK(), getHubUrl(), showMessage)) return true;
	if (showMessage)
	{
		string addr = Util::printIpAddress(ip, true) + ':' + Util::toString(port);
		fire(ClientListener::StatusMessage(), this, STRING_F(ANTIFLOOD_MESSAGE, "Search" % addr));
	}
	return false;
}

static uint16_t parsePort(const string& s)
{
	if (s.empty() || s.length() > 5) return 0;
	uint32_t port = Util::toUInt32(s);
	return port < 65536 ? static_cast<uint16_t>(port) : 0;
}

void NmdcHub::connectToMeParse(const string& param)
{
	string senderNick;
	string portStr;
	string server;
	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
		myNick = this->myNick;
	}

#ifdef _DEBUG
	if (suppressUserConn)
	{
		LogManager::message("$ConnectToMe ignored!");
		return;
	}
#endif

	while (true)
	{
		string::size_type i = param.find(' ');
		string::size_type j;
		if (i == string::npos || (i + 1) >= param.size())
			break;
		i++;
		j = param.find(':', i);
		if (j == string::npos)
			break;
		server = param.substr(i, j - i);
		if (j + 1 >= param.size())
			break;
		
		i = param.find(' ', j + 1);
		if (i == string::npos)
		{
			portStr = param.substr(j + 1);
		}
		else
		{
			senderNick = param.substr(i + 1);
			portStr = param.substr(j + 1, i - j - 1);
		}

		if (portStr.empty())
			break;

		bool secure = false;
		if (portStr.back() == 'S')
		{
			portStr.erase(portStr.length() - 1);
			if (portStr.empty())
				break;
			if (CryptoManager::getInstance()->isInitialized())
				secure = true;
		}

		IpAddress ip;
		if (!(Util::parseIpAddress(ip, server) && Util::isValidIp(ip) && checkIpType(ip.type)))
			break;

		if (allowNatTraversal())
		{
			if (portStr.back() == 'N')
			{
				if (senderNick.empty())
					break;

				OnlineUserPtr u = findUser(senderNick);
				if (!u)
					break;

				portStr.pop_back();
				uint16_t port = parsePort(portStr);
				if (!port)
					break;

				if (!checkConnectToMeFlood(ip, port))
					break;

				const string userKey = u->getUser()->getCID().toBase32();
				uint16_t localPort = socketPool.addSocket(userKey, ip.type, false, secure, true, Util::emptyString);
				if (!localPort)
					break;
				ConnectionManager::getInstance()->nmdcConnect(ip, port, localPort,
				                                              BufferedSocket::NAT_CLIENT, myNick, getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				Ip4Address ip4;
				Ip6Address ip6;
				getLocalIp(ip4, ip6);
				IpAddress localIp;
				if (ip.type == AF_INET)
				{
					localIp.data.v4 = ip4;
					localIp.type = AF_INET;
				}
				else
				{
					localIp.data.v6 = ip6;
					localIp.type = AF_INET6;
				}
				send("$ConnectToMe " + fromUtf8(senderNick) + ' ' + Util::printIpAddress(localIp) + ':' + Util::toString(localPort) + (secure ? "RS|" : "R|"));
				break;
			}
			else if (portStr.back() == 'R')
			{
				portStr.pop_back();
				uint16_t port = parsePort(portStr);
				if (!port)
					break;
				
				string natUser;
				{
					LOCK(csState);
					natUser = std::move(lastNatUser);
					lastNatUser.clear();
					lastNatUserExpires = 0;
				}
				if (natUser.empty())
					break;

				if (!checkConnectToMeFlood(ip, port))
					break;

				uint16_t localPort;
				int localType;
				if (!socketPool.getPortForUser(natUser, localPort, localType) || localType != ip.type)
					break;

				ConnectionManager::getInstance()->nmdcConnect(ip, port, localPort,
				                                              BufferedSocket::NAT_SERVER, myNick, getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				break;
			}
		}

		uint16_t port = parsePort(portStr);
		if (!port)
			break;

		if (!checkConnectToMeFlood(ip, port))
			break;

		// For simplicity, we make the assumption that users on a hub have the same character encoding
		ConnectionManager::getInstance()->nmdcConnect(ip, port, myNick, getHubUrl(),
		                                              getEncoding(),
		                                              secure);
		break; // OK
	}
}

void NmdcHub::chatMessageParse(const string& line)
{
	const string utf8Line = toUtf8(unescape(line));
	const string lowerLine = Text::toLower(utf8Line);

	// Check if we're being banned...
	States currentState;
	{
		LOCK(csState);
		currentState = state;
	}
	if (currentState != STATE_NORMAL)
	{
		if (lowerLine.find("banned") != string::npos)
			setAutoReconnect(false);
	}
	
	if (lowerLine.find("hub-security") != string::npos && lowerLine.find("was kicked by") != string::npos)
	{
		fire(ClientListener::StatusMessage(), this, utf8Line, ClientListener::FLAG_KICK_MSG);
		return;
	}
	
	if (lowerLine.find("is kicking") != string::npos && lowerLine.find("because:") != string::npos)
	{
		fire(ClientListener::StatusMessage(), this, utf8Line, ClientListener::FLAG_KICK_MSG);
		return;
	}

	string nick;
	string message;
	bool bThirdPerson = false;	
	
	if ((utf8Line.size() > 1 && utf8Line.compare(0, 2, "* ", 2) == 0) ||
	    (utf8Line.size() > 2 && utf8Line.compare(0, 3, "** ", 3) == 0))
	{
		size_t begin = utf8Line[1] == '*' ? 3 : 2;
		size_t end = utf8Line.find(' ', begin);
		if (end != string::npos)
		{
			nick = utf8Line.substr(begin, end - begin);
			message = utf8Line.substr(end + 1);
			bThirdPerson = true;
		}
	}
	else if (utf8Line[0] == '<')
	{
		string::size_type pos = utf8Line.find("> ");
		
		if (pos != string::npos)
		{
			nick = utf8Line.substr(1, pos - 1);
			message = utf8Line.substr(pos + 2);
			
			if (message.empty())
				return;
		}
	}
	if (nick.empty())
	{
		fire(ClientListener::StatusMessage(), this, utf8Line);
		return;
	}
	
	if (message.empty())
		message = utf8Line;
	
	OnlineUserPtr user = findUser(nick);
#ifdef _DEBUG
	if (message.find("&#124") != string::npos)
	{
		dcassert(0);
	}
#endif
	
	std::unique_ptr<ChatMessage> chatMessage(new ChatMessage(message, user));
	chatMessage->thirdPerson = bThirdPerson;
	if (!user)
	{
		chatMessage->text = utf8Line;
		// если юзер подставной - не создаем его в списке
	}

	if (!chatMessage->from)
	{
		if (user)
		{
			chatMessage->from = user; ////getUser(nick, false, false); // Тут внутри снова идет поиск findUser(nick)
			chatMessage->from->getIdentity().setHub();
		}
	}
	chatMessage->translateMe();
	if (isChatMessageAllowed(*chatMessage, nick))
		fire(ClientListener::Message(), this, chatMessage);
}

void NmdcHub::hubNameParse(const string& paramIn)
{
	string param = paramIn;
	boost::replace_all(param, "\r\n", " ");
	std::replace(param.begin(), param.end(), '\n', ' ');

	// Workaround replace newlines in topic with spaces, to avoid funny window titles
	// If " - " found, the first part goes to hub name, rest to description
	string::size_type i = param.find(" - ");
	if (i != string::npos)
	{
		getHubIdentity().setNick(unescape(param.substr(0, i)));
		getHubIdentity().setDescription(unescape(param.substr(i + 3)));
	}
	else
	{
		getHubIdentity().setNick(unescape(param));
		getHubIdentity().setDescription(Util::emptyString);
	}
	fire(ClientListener::HubUpdated(), this);
}

void NmdcHub::supportsParse(const string& param)
{
	SimpleStringTokenizer<char> st(param, ' ');
	string tok;
	unsigned flags = 0;
	while (st.getNextNonEmptyToken(tok))
	{
		if (tok == "UserCommand")
		{
			flags |= SUPPORTS_USERCOMMAND;
		}
		else if (tok == "NoGetINFO")
		{
			flags |= SUPPORTS_NOGETINFO;
		}
		else if (tok == "UserIP2")
		{
			flags |= SUPPORTS_USERIP2;
		}
		else if (tok == "NickRule")
		{
			flags |= SUPPORTS_NICKRULE;
		}
		else if (tok == "SearchRule")
		{
			flags |= SUPPORTS_SEARCHRULE;
		}
#ifdef BL_FEATURE_NMDC_EXT_JSON
		else if (tok == "ExtJSON2")
		{
			flags |= SUPPORTS_EXTJSON2;
		}
#endif
		else if (tok == "TTHS")
		{
			flags |= SUPPORTS_SEARCH_TTHS;
		}
		else if (tok == "SaltPass")
		{
			flags |= SUPPORTS_SALT_PASS;
		}
		else if (tok == "MCTo")
		{
			flags |= SUPPORTS_MCTO;
		}
#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
		else if (!(tok == "NoHello" || tok == "ZPipe0" || tok == "HubTopic" || tok == "HubURL" || tok == "BotList" || tok == "TTHSearch"))
			collNmdcFeatures.addTag(tok, getHubUrl());
#endif
	}
	csState.lock();
	hubSupportFlags |= flags;
	csState.unlock();
}

void NmdcHub::userCommandParse(const string& param)
{
	string::size_type i = 0;
	string::size_type j = param.find(' ');
	if (j == string::npos)
		return;
		
	int type = Util::toInt(param.substr(0, j));
	i = j + 1;
	if (type == UserCommand::TYPE_CLEAR)
	{
		int ctx = Util::toInt(param.substr(i));
		clearUserCommands(ctx);
	}
	else if (type == UserCommand::TYPE_SEPARATOR)
	{
		int ctx = Util::toInt(param.substr(i));
		addUserCommand(UserCommand(0, UserCommand::TYPE_SEPARATOR, ctx, UserCommand::FLAG_NOSAVE,
			Util::emptyString, Util::emptyString, Util::emptyString, Util::emptyString));
	}
	else if (type == UserCommand::TYPE_RAW || type == UserCommand::TYPE_RAW_ONCE)
	{
		j = param.find(' ', i);
		if (j == string::npos)
			return;
		int ctx = Util::toInt(param.substr(i, j - i));
		i = j + 1;
		j = param.find('$');
		if (j == string::npos)
			return;
		string name = unescape(param.substr(i, j - i));
		// NMDC uses '\' as a separator but both ADC and our internal representation use '/'
		Util::replace("/", "//", name);
		Util::replace("\\", "/", name);
		i = j + 1;
		string command = unescape(param.substr(i, param.length() - i));
		addUserCommand(UserCommand(0, type, ctx, UserCommand::FLAG_NOSAVE,
			name, command, Util::emptyString, Util::emptyString));
	}
}

void NmdcHub::lockParse(const char* buf, size_t len)
{
	if (len < 6)
		return;

	{
		LOCK(csState);
		if (state != STATE_PROTOCOL)
			return;
		state = STATE_IDENTIFY;
	}

	dcassert(users.empty());

	// Param must not be toUtf8'd...
	const string param(buf + 6, len - 6);

	if (!param.empty())
	{
		const auto j = param.find(' ');
		const auto lock = j != string::npos ? param.substr(0, j) : param;
		if (isExtended(lock))
		{
			auto ss = SettingsManager::instance.getCoreSettings(); 
			ss->lockRead();
			const bool optionUserCommands = ss->getBool(Conf::HUB_USER_COMMANDS) && ss->getInt(Conf::MAX_HUB_USER_COMMANDS) > 0;
			const bool optionExtJson = ss->getBool(Conf::SEND_EXT_JSON);
			const bool optionBotList = ss->getBool(Conf::USE_BOT_LIST);
			const bool optionSaltPass = ss->getBool(Conf::USE_SALT_PASS);
			const bool optionMcTo = ss->getBool(Conf::USE_MCTO);
			ss->unlockRead();

			string feat = "$Supports";
			feat.reserve(128);
			if (optionUserCommands)
				feat += " UserCommand";
			feat += " NoGetINFO";
			feat += " NoHello";
			feat += " UserIP2";
			feat += " TTHSearch";
			feat += " ZPipe0";
#ifdef BL_FEATURE_NMDC_EXT_JSON
			if (optionExtJson)
				feat += " ExtJSON2";
#endif
			feat += " HubURL";
			if (optionBotList)
				feat += " BotList";
			feat += " NickRule";
			feat += " SearchRule";
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
			// http://nmdc.sourceforge.net/NMDC.html#_hubtopic
			feat += " HubTopic";
#endif
			feat += " TTHS";
			if (CryptoManager::getInstance()->isInitialized())
				feat += " TLS";
			if (optionSaltPass)
				feat += " SaltPass";
			if (optionMcTo)
				feat += " MCTo";
			
			feat += '|';
			send(feat);
		}
		
		send("$Key " + makeKeyFromLock(lock) + '|');
		
		string nick;
		csState.lock();
		if (!randomTempNick.empty())
		{
			nick = randomTempNick;
			myNick = nick;
		}
		else
			nick = myNick;
		csState.unlock();
		
		OnlineUserPtr ou = getUser(nick);
		send("$ValidateNick " + fromUtf8(nick) + '|');
	}
	else
	{
		dcassert(0);
	}
}

void NmdcHub::helloParse(const string& param)
{
	if (!param.empty())
	{
		OnlineUserPtr ou = getUser(param);
		if (isMe(ou))
		{
			if (isActive())
				ou->getUser()->unsetFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
			else
				ou->getUser()->setFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
			
			{
				LOCK(csState);
				if (state != STATE_IDENTIFY) return;
				state = STATE_NORMAL;
				connSuccess = true;
			}
			updateConnectionStatus(ConnectionStatus::SUCCESS);
			updateUserCheckTime();
			updateCounts(false);
			version();
			getNickList();
			myInfo(true);
		}
	}
}

void NmdcHub::userIPParse(const string& param)
{
	if (!param.empty())
	{
		const StringTokenizer<string> t(param, "$$");
		const StringList& sl = t.getTokens();
		for (const string& s : sl)
		{
			string::size_type j = s.find(' ');
			if (j == string::npos || j == 0 || j + 1 == s.length())
				continue;

			const string ipStr = s.substr(j + 1);
			const string user = s.substr(0, j);
			OnlineUserPtr ou = findUser(user);

			if (!ou)
				continue;

			IpAddress ip;
			if (Util::parseIpAddress(ip, ipStr))
			{
				switch (ip.type)
				{
					case AF_INET:
						ou->getIdentity().setIP4(ip.data.v4);
						break;
					case AF_INET6:
						ou->getIdentity().setIP6(ip.data.v6);
						break;
				}
			}
		}
	}
}

void NmdcHub::botListParse(const string& param)
{
	OnlineUserList v;
	const StringTokenizer<string> t(param, "$$");
	const StringList& sl = t.getTokens();
	for (auto it = sl.cbegin(); it != sl.cend(); ++it)
	{
		if (it->empty())
			continue;
		OnlineUserPtr ou = getUser(*it);
		if (ou)
		{
			ou->getIdentity().setBot();
			v.push_back(ou);
		}
	}
	fireUserListUpdated(v);
}

void NmdcHub::nickListParse(const string& param)
{
	if (!param.empty())
	{
		OnlineUserList v;
		const StringTokenizer<string> t(param, "$$");
		const StringList& sl = t.getTokens();
		{
			for (auto it = sl.cbegin(); it != sl.cend(); ++it)
			{
				if (it->empty())
					continue;
				OnlineUserPtr ou = getUser(*it);
				v.push_back(ou);
			}
			
			csState.lock();
			auto supportFlags = hubSupportFlags;
			csState.unlock();

			if (!(supportFlags & SUPPORTS_NOGETINFO))
			{
				csState.lock();
				string myNick = this->myNick;
				csState.unlock();

				string tmp;
				// Let's assume 10 characters per nick...
				tmp.reserve(v.size() * (11 + 10 + myNick.length()));
				string n = ' ' +  fromUtf8(myNick) + '|';
				for (auto i = v.cbegin(); i != v.cend(); ++i)
				{
					tmp += "$GetINFO ";
					tmp += fromUtf8((*i)->getIdentity().getNick());
					tmp += n;
				}
				if (!tmp.empty())
				{
					send(tmp);
				}
			}
		}
		fireUserListUpdated(v);
	}
}

void NmdcHub::opListParse(const string& param)
{
	if (!param.empty())
	{
		OnlineUserList v;
		const StringTokenizer<string> t(param, "$$");
		const StringList& sl = t.getTokens();
		{
			for (auto it = sl.cbegin(); it != sl.cend(); ++it)
			{
				if (it->empty())
					continue;
					
				OnlineUserPtr ou = getUser(*it);
				if (ou)
				{
					ou->getIdentity().setOp(true);
					v.push_back(ou);
				}
			}
		}
		fireUserListUpdated(v);
		updateCounts(false);
		
		// Special...to avoid op's complaining that their count is not correctly
		// updated when they log in (they'll be counted as registered first...)
		myInfo(false);
	}
}

void NmdcHub::getUserList(OnlineUserList& result) const
{
	READ_LOCK(*csUsers);
	result.reserve(users.size());
	for (auto i = users.cbegin(); i != users.cend(); ++i)
	{
		result.push_back(i->second);
	}
}

void NmdcHub::toParse(const string& param)
{
	string::size_type pos_a = param.find(" From: ");

	if (pos_a == string::npos)
		return;

	pos_a += 7;
	string::size_type pos_b = param.find(" $<", pos_a);
	
	if (pos_b == string::npos)
		return;
		
	const string rtNick = param.substr(pos_a, pos_b - pos_a);
	
	if (rtNick.empty())
	{
		dcassert(0);
		return;
	}

	OnlineUserPtr user = findUser(rtNick);
	
#ifdef _DEBUG
	if (!user)
		LogManager::message("NmdcHub::toParse $To: invalid user: rtNick = " + rtNick + " param = " + param + " Hub = " + getHubUrl());
#endif
	
	pos_a = pos_b + 3;
	pos_b = param.find("> ", pos_a);
	
	if (pos_b == string::npos)
	{
#ifdef _DEBUG
		LogManager::message("NmdcHub::toParse pos_b == string::npos param = " + param + " Hub = " + getHubUrl());
#endif
		return;
	}
	
	const string fromNick = param.substr(pos_a, pos_b - pos_a);
	
	if (fromNick.empty())
	{
#ifdef _DEBUG
		LogManager::message("NmdcHub::toParse fromNick.empty() param = " + param + " Hub = " + getHubUrl());
#endif
		return;
	}
	
	const string msgText = param.substr(pos_b + 2);
	
	if (msgText.empty())
	{
#ifdef _DEBUG
		LogManager::message("NmdcHub::toParse msgText.empty() param = " + param + " Hub = " + getHubUrl());
#endif
		return;
	}
	
	unique_ptr<ChatMessage> message(new ChatMessage(unescape(msgText), findUser(fromNick), nullptr, user));
	
	if (message->replyTo == nullptr)
	{
		// Assume it's from the hub
		message->replyTo = getUser(rtNick);
		message->replyTo->getIdentity().setHub();
	}
	if (message->from == nullptr)
	{
		// Assume it's from the hub
		message->from = getUser(fromNick);
		message->from->getIdentity().setHub();
	}

	message->to = getMyOnlineUser();

#if 0
	if (message->to->getUser() == message->from->getUser() && message->from->getUser() == message->replyTo->getUser())
	{
		fire(ClientListener::StatusMessage(), this, message->text, ClientListener::FLAG_IS_SPAM);
		LogManager::message("Magic spam message (from you to you) filtered on hub: " + getHubUrl() + ".");
		return;
	}
#endif

	message->translateMe();
	string response;
	OnlineUserPtr replyTo = message->replyTo;
	processIncomingPM(message, response);
	if (!response.empty())
		privateMessage(replyTo, response, PM_FLAG_AUTOMATIC | PM_FLAG_THIRD_PERSON);
}

void NmdcHub::mcToParse(const string& param)
{
	if (param.empty()) return;
	string::size_type p1 = param.find('$');
	if (p1 == string::npos) return;
	string::size_type p2 = p1;
	if (p2 > 0 && param[p2-1] == ' ') p2--;
	const string toNick = param.substr(0, p2);
	if (toNick.empty()) return;

	string::size_type p3 = param.find(' ', ++p1);
	if (p3 == string::npos) return;
	const string msgText = param.substr(p3 + 1);
	if (msgText.empty()) return;
	const string fromNick = param.substr(p1, p3 - p1);
	if (fromNick.empty()) return;

	OnlineUserPtr user = findUser(fromNick);
	if (!user)
	{
		user = getUser(fromNick);
		user->getIdentity().setHub();
	}
	unique_ptr<ChatMessage> message(new ChatMessage(unescape(msgText), user, getMyOnlineUser(), user));
	message->translateMe();
	string response;
	processIncomingPM(message, response);
	if (!response.empty())
		privateMessage(user, response, PM_FLAG_AUTOMATIC | PM_FLAG_THIRD_PERSON);
}

void NmdcHub::onLine(const char* buf, size_t len)
{
	if (!len)
		return;
		
	if (buf[0] != '$')
	{
		string line(buf, len);
		chatMessageParse(line);
		return;
	}

	string cmd;
	string param;
	const char* p = (const char*) memchr(buf, ' ', len);
	int searchType = ST_NONE;
	bool isMyInfo = false;
	if (!p)
	{
		cmd.assign(buf + 1, len - 1);
	}
	else
	{
		cmd.assign(buf + 1, p - buf - 1);
		param = toUtf8(string(p + 1, len - (p - buf) - 1));
		if (cmd.length() == 2 && cmd[0] == 'S')
		{
			if (cmd[1] == 'A')
				searchType = ST_SA;
			else if (cmd[1] == 'P')
				searchType = ST_SP;
		}
		else
			if (cmd == "Search")
				searchType = ST_SEARCH;
		if (searchType != ST_NONE && hideShare)
			return;
	}
	if (searchType != ST_NONE)
	{
		if (!ClientManager::isStartup())
			searchParse(param, searchType);
	}
	else if (cmd == "MyINFO")
	{
		isMyInfo = true;
		myInfoParse(param);
	}
#ifdef BL_FEATURE_NMDC_EXT_JSON
	else if (cmd == "ExtJSON")
	{
		//bMyInfoCommand = false;
		extJSONParse(param);
	}
#endif
	else if (cmd == "Quit")
	{
		if (!param.empty())
		{
			putUser(param);
		}
		else
		{
			//dcassert(0);
		}
	}
	else if (cmd == "ConnectToMe")
	{
		connectToMeParse(param);
		return;
	}
	else if (cmd == "RevConnectToMe")
	{
		revConnectToMeParse(param);
	}
	else if (cmd == "SR")
	{
		SearchManager::getInstance()->onSearchResult(buf, len, getIp());
	}
	else if (cmd == "HubName")
	{
		hubNameParse(param);
	}
	else if (cmd == "Supports")
	{
		supportsParse(param);
	}
	else if (cmd == "UserCommand")
	{
		userCommandParse(param);
	}
	else if (cmd == "Lock")
	{
		lockParse(buf, len);
	}
	else if (cmd == "Hello")
	{
		helloParse(param);
	}
	else if (cmd == "ForceMove")
	{
		dcassert(clientSock);
		csState.lock();
		if (clientSock)
			clientSock->disconnect(false);
		csState.unlock();
		fire(ClientListener::Redirect(), this, param);
	}
	else if (cmd == "HubIsFull")
	{
		fire(ClientListener::HubFull(), this);
	}
	else if (cmd == "ValidateDenide")        // Mind the spelling...
	{
		dcassert(clientSock);
		csState.lock();
		if (clientSock)
			clientSock->disconnect(false);
		csState.unlock();
		fire(ClientListener::NickError(), ClientListener::Taken);
	}
	else if (cmd == "UserIP")
	{
		userIPParse(param);
	}
	else if (cmd == "BotList")
	{
		botListParse(param);
	}
	else if (cmd == "NickList")
	{
		nickListParse(param);
	}
	else if (cmd == "OpList")
	{
		opListParse(param);
	}
	else if (cmd == "To:")
	{
		toParse(param);
	}
	else if (cmd == "MCTo:")
	{
		mcToParse(param);
	}
	else if (cmd == "GetPass")
	{
		csState.lock();
		string myNick = this->myNick;
		string pwd = storedPassword;
		if (hubSupportFlags & SUPPORTS_SALT_PASS)
			salt = param;
		else
			salt.clear();
		csState.unlock();
		getUser(myNick);
		setRegistered();
		processPasswordRequest(pwd);
	}
	else if (cmd == "BadPass")
	{
		csState.lock();
		storedPassword.clear();
		csState.unlock();
	}
	else if (cmd == "ZOn")
	{
		clientSock->setMode(BufferedSocket::MODE_ZPIPE);
	}
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
	else if (cmd == "HubTopic")
	{
		if (!param.empty())
			fire(ClientListener::HubInfoMessage(), ClientListener::HubTopic, this, param);
	}
#endif
	else if (cmd == "LogedIn")
	{
		fire(ClientListener::HubInfoMessage(), ClientListener::OperatorInfo, this, Util::emptyString);
	}
	else if (cmd == "BadNick")
	{
	
		/*
		$BadNick TooLong 64        -- ник слишком длинный, максимальная допустимая длина ника 64 символа     (флай считает сколько у него в нике символов и убирает лишние, так чтоб осталось максимум 64)
		$BadNick TooShort 3        -- ник слишком короткий, минимальная допустимая длина ника 3 символа     (флай считает сколько у него в нике символов и добавляет нехватающие, так чтоб было минимум 3)
		$BadNick BadPrefix        -- у ника лишний префикс, хаб хочет ник без префикса      (флай уберает все префиксы из ника)
		$BadNick BadPrefix [ISP1] [ISP2]        -- у ника нехватает префикса, хаб хочет ник с префиксом [ISP1] или [ISP2]      (флай добавляет случайный из перечисленых префиксов к нику)
		$BadNick BadChar 32 36        -- ник содержит запрещенные хабом символы, хаб хочет ник в котором не будет перечисленых символов      (флай убирает из ника все перечисленые байты символов)
		*/
		dcassert(clientSock);
		csState.lock();
		if (clientSock)
			clientSock->disconnect(false);
		csState.unlock();
		fire(ClientListener::NickError(), ClientListener::Rejected);
	}
	else if (cmd == "SearchRule")
	{
		const StringTokenizer<string> tok(param, "$$", 4);
		const StringList& sl = tok.getTokens();
		for (auto it = sl.cbegin(); it != sl.cend(); ++it)
		{
			const string& rule = *it;
			auto pos = rule.find(' ');
			if (pos != string::npos && pos < rule.length() - 1)
			{
				const string key = it->substr(0, pos);
				if (key == "Int")
				{
					int value = Util::toInt(rule.c_str() + pos + 1);
					if (value > 0 && !overrideSearchInterval)
						setSearchInterval(value * 1000);
				}
				if (key == "IntPas")
				{
					int value = Util::toInt(rule.c_str() + pos + 1);
					if (value > 0 && !overrideSearchIntervalPassive)
						setSearchIntervalPassive(value * 1000);
				}
			}
		}
	}
	else if (cmd == "NickRule")
	{
		nickRule.reset(new NickRule);
		const StringTokenizer<string> tok(param, "$$", 4);
		const StringList& sl = tok.getTokens();
		for (auto it = sl.cbegin(); it != sl.cend(); ++it)
		{
			const string& rule = *it;
			string::size_type pos = rule.find(' ');
			if (pos != string::npos && pos < rule.length() - 1)
			{
				const string key = rule.substr(0, pos);
				if (key == "Min")
				{
					unsigned minLen = Util::toInt(rule.c_str() + pos + 1);
					if (minLen > 64)
					{
						LogManager::message("Bad value in NickRule: Min=" + rule.substr(pos + 1) + " Hub=" + getHubUrl());
						nickRule.reset();
						break;
					}
					nickRule->minLen = minLen;
				}
				else if (key == "Max")
				{
					unsigned maxLen = Util::toInt(rule.c_str() + pos + 1);
					if (maxLen < 4)
					{
						LogManager::message("Bad value in NickRule: Max=" + rule.substr(pos + 1) + " Hub=" + getHubUrl());
						nickRule.reset();
						break;
					}
					nickRule->maxLen = maxLen;
				}
				else if (key == "Char")
				{
					SimpleStringTokenizer<char> st(rule, ' ', pos + 1);
					string tok;
					while (st.getNextNonEmptyToken(tok))
 					{
						int val = Util::toInt(tok);
						if (val >= 0 && val < 256 && nickRule->invalidChars.size() < NickRule::MAX_CHARS)
							nickRule->invalidChars.push_back((char) val);
					}
				}
				else if (key == "Pref")
				{
					SimpleStringTokenizer<char> st(rule, ' ', pos + 1);
					string tok;
					while (st.getNextNonEmptyToken(tok))
					{
						if (nickRule->prefixes.size() < NickRule::MAX_PREFIXES)
							nickRule->prefixes.push_back(tok);
						else
							break;
					}
				}
			}
			else
			{
				dcassert(0);
			}
		}
		if (nickRule && nickRule->maxLen && nickRule->minLen > nickRule->maxLen)
		{
			LogManager::message("Bad value in NickRule: Max=" + Util::toString(nickRule->maxLen) + " Min=" + Util::toString(nickRule->minLen) + " Hub=" + getHubUrl());
			nickRule.reset();
		}
	}
	else if (cmd == "GetHubURL")
	{
		send("$MyHubURL " + getHubUrl() + "|");
	}
	else
		LogManager::message("Unknown command from hub " + getHubUrl() + ": " + string(buf, len), false);
	updateMyInfoState(isMyInfo);
}

void NmdcHub::updateMyInfoState(bool isMyInfo)
{
	if (!isMyInfo && myInfoState == MYINFO_LIST)
	{
		myInfoState = MYINFO_LIST_COMPLETED;
		userListLoaded = true;
	}
	if (isMyInfo && myInfoState == WAITING_FOR_MYINFO)
	{
		myInfoState = MYINFO_LIST;
		fire(ClientListener::LoggedIn(), this);
	}
}

void NmdcHub::checkNick(string& nick) const noexcept
{
	for (size_t i = 0; i < nick.size(); ++i)
		if (static_cast<uint8_t>(nick[i]) <= 32 || nick[i] == '|' || nick[i] == '$' || nick[i] == '<' || nick[i] == '>')
			nick[i] = '_';
}

void NmdcHub::connectToMe(const OnlineUser& user, const string& token)
{
	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return;
		myNick = this->myNick;
	}

	IpAddress ip;
	if (!getMyExternalIP(ip))
		return;

	bool secure = CryptoManager::getInstance()->isInitialized() && (user.getUser()->getFlags() & User::TLS);
	auto cm = ConnectionManager::getInstance();
	uint16_t port = cm->getConnectionPort(ip.type, secure);
	if (port == 0)
	{
		LogManager::message(STRING(NOT_LISTENING));
		return;
	}

	dcdebug("NmdcHub::connectToMe %s\n", user.getIdentity().getNick().c_str());
	const string nick = fromUtf8(user.getIdentity().getNick());
	uint64_t expires = token.empty() ? GET_TICK() + 45000 : UINT64_MAX;
	if (!cm->nmdcExpect(nick, myNick, getHubUrl(), token, getEncoding(), expires))
		return;

	send("$ConnectToMe " + nick + ' ' + Util::printIpAddress(ip) + ':' + Util::toString(port) + (secure ? "S|" : "|"));
}

void NmdcHub::revConnectToMe(const OnlineUser& user)
{
	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return;
		myNick = this->myNick;
	}
	dcdebug("NmdcHub::revConnectToMe %s\n", user.getIdentity().getNick().c_str());
	send("$RevConnectToMe " + fromUtf8(myNick) + ' ' + fromUtf8(user.getIdentity().getNick()) + '|');
}

void NmdcHub::hubMessage(const string& message, bool thirdPerson)
{
	string nick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return;
		nick = myNick;
	}
	send(fromUtf8('<' + nick + "> " + escape(thirdPerson ? "/me " + message : message) + '|'));
}

void NmdcHub::password(const string& pwd, bool setPassword)
{
	string cmd = "$MyPass ";
	string useSalt;
	{
		LOCK(csState);
		if (setPassword) storedPassword = pwd;
		useSalt = salt;
	}
	if (!useSalt.empty())
	{
		size_t saltBytes = salt.size() * 5 / 8;
		std::unique_ptr<uint8_t[]> buf(new uint8_t[saltBytes]);
		Util::fromBase32(useSalt.c_str(), buf.get(), saltBytes);
		TigerHash th;
		string localPwd = fromUtf8(pwd);
		th.update(localPwd.data(), localPwd.length());
		th.update(buf.get(), saltBytes);
		cmd += Util::toBase32(th.finalize(), TigerHash::BYTES);
	}
	else
		cmd += fromUtf8(pwd);
	cmd += '|';
	send(cmd);
}

bool NmdcHub::resendMyINFO(bool alwaysSend, bool forcePassive)
{
	if (forcePassive)
	{
		LOCK(csState);
		if (lastModeChar == 'P')
			return false;
	}
	myInfo(alwaysSend, forcePassive);
	return true;
}

void NmdcHub::myInfo(bool alwaysSend, bool forcePassive)
{
	const uint64_t currentTick = GET_TICK();

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string uploadSpeed = ss->getString(Conf::UPLOAD_SPEED);
	const int gender = ss->getInt(Conf::GENDER);
	const unsigned myInfoDelay = ss->getInt(Conf::MYINFO_DELAY);
	const int outgoingConnections = ss->getInt(Conf::OUTGOING_CONNECTIONS);
	ss->unlockRead();	

	{
		LOCK(csState);
		uint64_t nextUpdate = lastUpdate + myInfoDelay * 1000;
		if (!forcePassive && !alwaysSend && nextUpdate > currentTick)
		{
			if (!pendingUpdate)
				pendingUpdate = nextUpdate;
			return; // antispam
		}
		if (state != STATE_NORMAL)
			return;
		pendingUpdate = 0;
	}

	reloadSettings(false);

	csState.lock();
	string myNick = this->myNick;
	csState.unlock();

	char modeChar;
	if (forcePassive)
	{
		modeChar = 'P';
	}
	else
	{
		if (outgoingConnections == Conf::OUTGOING_SOCKS5)
			modeChar = '5';
		else if (isActive())
			modeChar = 'A';
		else
			modeChar = 'P';
	}

	size_t limit = ThrottleManager::getInstance()->getUploadLimitInBytes();
	if (limit)
	{
		uploadSpeed = Util::toString(limit);
		uploadSpeed += " KiB/s";
	}

	char status = NmdcSupports::NORMAL;

	if (Util::getAway())
	{
		status |= NmdcSupports::AWAY;
	}
	if (fakeClientStatus)
	{
		if (fakeClientStatus == 2)
			status |= NmdcSupports::SERVER;
		else if (fakeClientStatus == 3)
			status |= NmdcSupports::FIREBALL;
	}
	else
	{
		auto um = UploadManager::getInstance();
		if (um->getIsFileServerStatus())
			status |= NmdcSupports::SERVER;
		if (um->getIsFireballStatus())
			status |= NmdcSupports::FIREBALL;
	}
	if (allowNatTraversal() && !isActive())
		status |= NmdcSupports::NAT0;

	if (CryptoManager::getInstance()->isInitialized())
		status |= NmdcSupports::TLS;

	if (getClientName().find("AirDC") != string::npos)
		status |= NmdcSupports::AIRDC;

	unsigned normal, registered, op;
	if (fakeHubCount)
	{	
		getFakeCounts(normal, registered, op);
	} else
	{
		getCounts(normal, registered, op);
		if (normal + registered + op == 0) normal = 1; // fix H:0/0/0
	}
	
	char hubCounts[64];
	sprintf(hubCounts, ",H:%u/%u/%u", normal, registered, op);

	string currentMyInfo = "$MyINFO $ALL ";
	currentMyInfo += fromUtf8(myNick);
	currentMyInfo += ' ';
	currentMyInfo += fromUtf8(escape(getCurrentDescription()));
	currentMyInfo += '<';
	currentMyInfo += getClientName();
	currentMyInfo += " V:";
	currentMyInfo += getClientVersion();
	currentMyInfo += ",M:";
	currentMyInfo += modeChar;
	currentMyInfo += hubCounts;
	currentMyInfo += ",S:";
	currentMyInfo += Util::toString(getSlots());
	currentMyInfo += ">$ $";
	currentMyInfo += uploadSpeed;
	currentMyInfo += status;
	currentMyInfo += '$';
	currentMyInfo += fromUtf8(escape(getCurrentEmail()));
	currentMyInfo += '$';

	int64_t bytesShared, filesShared;
	if (hideShare)
		bytesShared = filesShared = 0;
	else if (fakeShareSize >= 0)
	{
		bytesShared = fakeShareSize;
		filesShared = 0;
		if (fakeShareSize)
		{
			filesShared = fakeShareFiles;
			if (filesShared <= 0) filesShared = (fakeShareSize + averageFakeFileSize - 1)/averageFakeFileSize;
		}
	}
	else
		ShareManager::getInstance()->getShareGroupInfo(shareGroup, bytesShared, filesShared);
	currentMyInfo += Util::toString(bytesShared);
	currentMyInfo += "$|";

	csState.lock();
	const bool myInfoChanged = currentMyInfo != lastMyInfo;
	const unsigned supportFlags = hubSupportFlags;
	csState.unlock();

	if (alwaysSend || myInfoChanged)
	{
		if (myInfoChanged)
		{
			csState.lock();
			lastMyInfo = currentMyInfo;
			lastUpdate = currentTick;
			lastModeChar = modeChar;
			csState.unlock();
			send(currentMyInfo);
		}
#ifdef BL_FEATURE_NMDC_EXT_JSON
		if (supportFlags & SUPPORTS_EXTJSON2)
		{
			JsonFormatter json;
			json.setDecorate(false);
			json.open('{');
			NmdcExtJson::appendInt64Attrib(json, NmdcExtJson::EXT_JSON_FILES, hideShare ? 0 : filesShared);
			NmdcExtJson::appendIntAttrib(json, NmdcExtJson::EXT_JSON_GENDER, gender + 1);
			json.close('}');
#if 0
			if (ShareManager::getLastSharedDate())
			{
				json["LastDate"] = ShareManager::getLastSharedDate();
			}
			extern int g_RAM_WorkingSetSize;
			if (g_RAM_WorkingSetSize)
			{
				static int g_LastRAM_WorkingSetSize;
				if (std::abs(g_LastRAM_WorkingSetSize - g_RAM_WorkingSetSize) > 10)
				{
					json["RAM"] = g_RAM_WorkingSetSize;
					g_LastRAM_WorkingSetSize = g_RAM_WorkingSetSize;
				}
			}
			if (CompatibilityManager::getFreePhysMemory())
			{
				json["RAMFree"] = CompatibilityManager::getFreePhysMemory() / 1024 / 1024;
			}
			if (g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI])
			{
				json["StartGUI"] = uint32_t(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI]);
			}
			if (g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE])
			{
				json["StartCore"] = uint32_t(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE]);
			}
			if (DatabaseManager::getCountQueueFiles())
			{
				json["QueueFiles"] = DatabaseManager::getCountQueueFiles();
			}
			if (DatabaseManager::getCountQueueSources())
			{
				json["QueueSrc"] = DatabaseManager::getCountQueueSources();
			}
			extern int g_RAM_PeakWorkingSetSize;
			if (g_RAM_PeakWorkingSetSize)
			{
				json["RAMPeak"] = g_RAM_PeakWorkingSetSize;
			}
			extern int64_t g_SQLiteDBSize;
			if (const int l_value = g_SQLiteDBSize / 1024 / 1024)
			{
				json["SQLSize"] = l_value;
			}
			extern int64_t g_SQLiteDBSizeFree;
			if (const int l_value = g_SQLiteDBSizeFree / 1024 / 1024)
			{
				json["SQLFree"] = l_value;
			}
			extern int64_t g_TTHLevelDBSize;
			if (const int l_value = g_TTHLevelDBSize / 1024 / 1024)
			{
				json["LDBHistSize"] = l_value;
			}
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
			extern int64_t g_IPCacheLevelDBSize;
			if (const int l_value = g_IPCacheLevelDBSize / 1024 / 1024)
			{
				json["LDBIPCacheSize"] = l_value;
			}
#endif
#endif			
			string jsonStr;
			json.moveResult(jsonStr);
			jsonStr.erase(std::remove_if(jsonStr.begin(), jsonStr.end(),
				[](char c) { return c=='$' || c=='|'; }), jsonStr.end());

			string currentExtJSONInfo = "$ExtJSON " + fromUtf8(myNick) + " " + escape(jsonStr) + '|';
			csState.lock();
			if (lastExtJSONInfo != currentExtJSONInfo)
			{
				lastExtJSONInfo = currentExtJSONInfo;
				lastUpdate = currentTick;
				csState.unlock();
				send(currentExtJSONInfo);
			} else
				csState.unlock();
		}
#endif // BL_FEATURE_NMDC_EXT_JSON
	}
}

void NmdcHub::searchToken(const SearchParam& sp)
{
	string myNick;
	unsigned supportFlags;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return;
		myNick = this->myNick;
		supportFlags = hubSupportFlags;
	}
	
	int fileType = sp.fileType;
	if (fileType > FILE_TYPE_TTH)
		fileType = 0;
	bool active;
	if (sp.searchMode == SearchParamBase::MODE_DEFAULT)
		active = isActive();
	else
		active = sp.searchMode != SearchParamBase::MODE_PASSIVE;
	string myUDPAddr;
	if (active)
	{
		uint16_t port;
		getMyUDPAddr(myUDPAddr, port);
		if (!port)
		{
			active = false;
			LogManager::message("Error: UDP port is zero");
		}
		else
			myUDPAddr += ":" + Util::toString(port);
	}
	string cmd;
	if ((supportFlags & SUPPORTS_SEARCH_TTHS) == SUPPORTS_SEARCH_TTHS && sp.fileType == FILE_TYPE_TTH)
	{
		dcassert(sp.filter == TTHValue(sp.filter).toBase32());
		if (active)
			cmd = "$SA " + sp.filter + ' ' + myUDPAddr + '|';
		else
			cmd = "$SP " + sp.filter + ' ' + fromUtf8(myNick) + '|';
	}
	else
	{
		char c1, c2;
		string query;
		if (sp.fileType == FILE_TYPE_TTH)
		{
			c1 = 'F';
			c2 = 'T';
			query = "TTH:" + sp.filter;
		}
		else
		{
			c1 = (sp.sizeMode == SIZE_DONTCARE || sp.sizeMode == SIZE_EXACT) ? 'F' : 'T';
			c2 = sp.sizeMode == SIZE_ATLEAST ? 'F' : 'T';
			query = fromUtf8(escape(sp.filter));
			std::replace(query.begin(), query.end(), ' ', '$');
		}
		cmd = "$Search ";
		if (active)
			cmd += myUDPAddr;
		else
			cmd += "Hub:" + fromUtf8(myNick);
		cmd += ' ';
		cmd += c1;
		cmd += '?';
		cmd += c2;
		cmd += '?';
		cmd += Util::toString(sp.size);
		cmd += '?';
		cmd += Util::toString(fileType + 1);
		cmd += '?';
		cmd += query;
		cmd += '|';
	}
	send(cmd);
}

string NmdcHub::validateMessage(string tmp, bool reverse) noexcept
{
	string::size_type i = 0;
	const auto j = tmp.find('&');
	if (reverse)
	{
		if (j != string::npos)
		{
			i = j;
			while ((i = tmp.find("&#36;", i)) != string::npos)
			{
				tmp.replace(i, 5, "$");
				i++;
			}
			i = j;
			while ((i = tmp.find("&#124;", i)) != string::npos)
			{
				tmp.replace(i, 6, "|");
				i++;
			}
			i = j;
			while ((i = tmp.find("&amp;", i)) != string::npos)
			{
				tmp.replace(i, 5, "&");
				i++;
			}
		}
	}
	else
	{
		if (j != string::npos)
		{
			i = j;
			while ((i = tmp.find("&amp;", i)) != string::npos)
			{
				tmp.replace(i, 1, "&amp;");
				i += 4;
			}
			i = j;
			while ((i = tmp.find("&#36;", i)) != string::npos)
			{
				tmp.replace(i, 1, "&amp;");
				i += 4;
			}
			i = j;
			while ((i = tmp.find("&#124;", i)) != string::npos)
			{
				tmp.replace(i, 1, "&amp;");
				i += 4;
			}
		}
		i = 0;
		while ((i = tmp.find('$', i)) != string::npos)
		{
			tmp.replace(i, 1, "&#36;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find('|', i)) != string::npos)
		{
			tmp.replace(i, 1, "&#124;");
			i += 5;
		}
	}
	return tmp;
}

void NmdcHub::privateMessage(const string& nick, const string& myNick, const string& message, int flags)
{
	string cmd = (flags & PM_FLAG_MAIN_CHAT) ? "$MCTo: " : "$To: ";
	string myNickEncoded = fromUtf8(myNick);
	cmd += fromUtf8(nick);
	if (flags & PM_FLAG_MAIN_CHAT)
	{
		cmd += " $";
		cmd += myNickEncoded;
		cmd += ' ';
	}
	else
	{
		cmd += " From: ";
		cmd += myNickEncoded;
		cmd += " $<";
		cmd += escape(myNickEncoded);
		cmd += "> ";
	}
	if (flags & PM_FLAG_THIRD_PERSON) cmd += "/me ";
	cmd += escape(fromUtf8(message));
	cmd += '|';
	send(cmd);
}

bool NmdcHub::privateMessage(const OnlineUserPtr& user, const string& message, int flags)
{
	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return false;
		myNick = this->myNick;
	}

	privateMessage(user->getIdentity().getNick(), myNick, message, flags);
	fireOutgoingPM(user, message, flags);
	return true;
}

void NmdcHub::sendUserCmd(const UserCommand& command, const StringMap& params)
{
	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return;
		myNick = this->myNick;
	}
	string cmd = Util::formatParams(command.getCommand(), params, false);
	if (command.isChat())
	{
		if (command.getTo().empty())
			hubMessage(cmd);
		else
			privateMessage(Util::formatParams(command.getTo(), params, false), myNick, cmd, 0);
	}
	else
		send(fromUtf8(cmd));
}

void NmdcHub::onConnected() noexcept
{
	Client::onConnected();
	string natUser;
	{
		LOCK(csState);
		salt.clear();
		if (state != STATE_PROTOCOL)
			return;
		lastModeChar = 0;
		hubSupportFlags = 0;
		lastMyInfo.clear();
		natUser = std::move(lastNatUser);
		lastNatUser.clear();
		lastUpdate = pendingUpdate = lastNatUserExpires = 0;
		lastExtJSONInfo.clear();
		myInfoState = WAITING_FOR_MYINFO;
	}
	if (!natUser.empty())
		socketPool.removeSocket(natUser);
}

void NmdcHub::onTimer(uint64_t tick) noexcept
{
	csState.lock();
	if (lastNatUser.empty() || tick < lastNatUserExpires)
	{
		csState.unlock();
		return;
	}
	string user = std::move(lastNatUser);
	lastNatUser.clear();
	lastNatUserExpires = 0;
	csState.unlock();
	socketPool.removeSocket(user);
}

void NmdcHub::getUsersToCheck(UserList& res, int64_t tick, int timeDiff) const noexcept
{
	READ_LOCK(*csUsers);
	for (NickMap::const_iterator i = users.cbegin(); i != users.cend(); ++i)
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

#ifdef BL_FEATURE_NMDC_EXT_JSON
bool NmdcHub::extJSONParse(const string& param)
{
	string::size_type j = param.find(' ', 0);
	if (j == string::npos)
		return false;
	const string nick = param.substr(0, j);
	
	if (nick.empty())
		return false;

	const string text = unescape(param.substr(nick.size() + 1));
	OnlineUserPtr ou = getUser(nick);
	NmdcExtJson::Data data;
	if (data.parse(text))
	{
		ou->getIdentity().setExtJSON();
		ou->getIdentity().setStringParam("F4", data.attr[NmdcExtJson::EXT_JSON_GENDER]);
		ou->getIdentity().setExtJSONSupportInfo(data.attr[NmdcExtJson::EXT_JSON_SUPPORT]);
		ou->getIdentity().setExtJSONRAMWorkingSet(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_RAM]));
		ou->getIdentity().setExtJSONRAMPeakWorkingSet(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_RAM_PEAK]));
		ou->getIdentity().setExtJSONRAMFree(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_RAM_FREE]));
		ou->getIdentity().setExtJSONCountFiles(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_FILES]));
		ou->getIdentity().setExtJSONLastSharedDate(Util::toInt64(data.attr[NmdcExtJson::EXT_JSON_LAST_DATE]));
		ou->getIdentity().setExtJSONSQLiteDBSize(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_SQL_SIZE]));
		ou->getIdentity().setExtJSONlevelDBHistSize(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_LDB_HIST_SIZE]));
		ou->getIdentity().setExtJSONSQLiteDBSizeFree(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_SQL_FREE]));
		ou->getIdentity().setExtJSONQueueFiles(Util::toInt(data.attr[NmdcExtJson::EXT_JSON_QUEUE_FILES]));
		ou->getIdentity().setExtJSONQueueSrc(Util::toInt64(data.attr[NmdcExtJson::EXT_JSON_QUEUE_SRC]));
		ou->getIdentity().setExtJSONTimesStartCore(Util::toInt64(data.attr[NmdcExtJson::EXT_JSON_START_CORE]));
		ou->getIdentity().setExtJSONTimesStartGUI(Util::toInt64(data.attr[NmdcExtJson::EXT_JSON_START_GUI]));
		fireUserUpdated(ou);
		return true;
	}
	return false;
}
#endif // BL_FEATURE_NMDC_EXT_JSON

void NmdcHub::myInfoParse(const string& param)
{
	if (ClientManager::isBeforeShutdown())
		return;
	string::size_type i = 5;
	string::size_type j = param.find(' ', i);
	if (j == string::npos || j == i)
		return;
	const string nick = param.substr(i, j - i);
	
	dcassert(!nick.empty());
	if (nick.empty())
	{
		dcassert(0);
		return;
	}
	i = j + 1;
	
	char modeChar = 0;
	OnlineUserPtr ou = getUser(nick);
	//ou->getUser()->setFlag(User::IS_MYINFO);
	j = param.find('$', i);
	dcassert(j != string::npos);
	if (j == string::npos)
		return;
	string tmpDesc = unescape(param.substr(i, j - i));
	// Look for a tag...
	if (!tmpDesc.empty() && tmpDesc.back() == '>')
	{
		const string::size_type x = tmpDesc.rfind('<');
		if (x != string::npos)
		{
			// Hm, we have something...disassemble it...
			//dcassert(tmpDesc.length() > x + 2)
			if (tmpDesc.length() > x + 2)
			{
				const string tag = tmpDesc.substr(x + 1, tmpDesc.length() - x - 2);
				updateFromTag(ou->getIdentity(), tag);
			}
			ou->getIdentity().setDescription(tmpDesc.erase(x));
		}
	}
	else
	{
		ou->getIdentity().setDescription(tmpDesc);
		dcassert(param.length() > j + 2);
		if (param.length() > j + 3 && param[j] == '$')
			modeChar = param[j + 1];
	}
	
	i = j + 3;
	j = param.find('$', i);
	if (j == string::npos)
		return;
		
	if (i == j || j - i - 1 == 0)
	{
#if 0
		// No connection = bot...
		ou->getIdentity().setBot();
#endif
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1], modeChar, Util::emptyString);
	}
	else
	{
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1], modeChar, param.substr(i, j - i - 1));
	}
	
	i = j + 1;
	j = param.find('$', i);
	
	if (j == string::npos)
		return;
	if (j != i)
	{
		ou->getIdentity().setEmail(unescape(param.substr(i, j - i)));
	}
	else
	{
		ou->getIdentity().setEmail(Util::emptyString);
	}
	
	i = j + 1;
	j = param.find('$', i);
	if (j == string::npos)
		return;
	
	int64_t shareSize = Util::toInt64(param.c_str() + i);
	if (shareSize < 0) shareSize = 0;
	changeBytesShared(ou->getIdentity(), shareSize);

	fireUserUpdated(ou);
}

void NmdcHub::onDataLine(const char* buf, size_t len) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		Client::onDataLine(buf, len);
		onLine(buf, len);
	}
}

void NmdcHub::onFailed(const string& line) noexcept
{
	clearUsers();
	Client::onFailed(line);
	updateCounts(true);
	csState.lock();
	string natUser = std::move(lastNatUser);
	lastNatUser.clear();
	lastNatUserExpires = 0;
	csState.unlock();
	if (!natUser.empty())
		socketPool.removeSocket(natUser);
}

bool NmdcHub::isMcPmSupported() const
{
	csState.lock();
	int flags = hubSupportFlags;
	csState.unlock();
	return (flags & SUPPORTS_MCTO) != 0;
}

const string& NmdcHub::getLock()
{
	return abracadabraLock;
}

const string& NmdcHub::getPk()
{
	return abracadabraPk;
}

static inline bool isExtra(uint8_t b)
{
	return b == 0 || b == 5 || b == 124 || b == 96 || b == 126 || b == 36;
}
		
static string keySubst(const uint8_t* key, size_t len, size_t n)
{
	string temp;
	temp.resize(len + n * 10);
	size_t j = 0;
	
	for (size_t i = 0; i < len; i++)
	{
		if (isExtra(key[i]))
		{
			temp[j++] = '/';
			temp[j++] = '%';
			temp[j++] = 'D';
			temp[j++] = 'C';
			temp[j++] = 'N';
			temp[j++] = '0' + key[i] / 100;
			temp[j++] = '0' + (key[i] / 10) % 10;
			temp[j++] = '0' + key[i] % 10;
			temp[j++] = '%';
			temp[j++] = '/';
		}
		else
		{
			temp[j++] = key[i];
		}
	}
	return temp;
}

string NmdcHub::makeKeyFromLock(const string& lock)
{
	if (lock.length() < 3 || lock.length() > 512) // How long can it be?
		return Util::emptyString;
		
	uint8_t* temp = static_cast<uint8_t*>(alloca(lock.length()));
	uint8_t v1;
	size_t extra = 0;
	
	v1 = (uint8_t)(lock[0] ^ 5);
	v1 = ((v1 >> 4) | (v1 << 4)) & 0xff;
	temp[0] = v1;
	
	for (size_t i = 1; i < lock.length(); i++)
	{
		v1 = (uint8_t)(lock[i] ^ lock[i - 1]);
		v1 = ((v1 >> 4) | (v1 << 4)) & 0xff;
		temp[i] = v1;
		if (isExtra(temp[i]))
			extra++;
	}
	
	temp[0] ^= temp[lock.length() - 1];
	
	if (isExtra(temp[0]))
		extra++;
	
	return keySubst(temp, lock.length(), extra);
}

bool NmdcHub::NickRule::convertNick(string& nick, bool& suffixAppended) const noexcept
{
	suffixAppended = false;
	if (!invalidChars.empty())
	{
		if (find(invalidChars.cbegin(), invalidChars.cend(), '_') != invalidChars.end() &&
		    invalidChars.size() > 1 && nick.find('_') != string::npos)
				return false;
		for (auto i = invalidChars.cbegin(); i != invalidChars.cend(); ++i)
			std::replace(nick.begin(), nick.end(), *i, '_');
	}
	if (!prefixes.empty())
	{
		bool hasPrefix = false;
		for (auto j = prefixes.cbegin(); j != prefixes.cend(); ++j)
		{
			const string& prefix = *j;
			if (nick.length() > prefix.length() && !nick.compare(0, prefix.length(), prefix))
			{
				hasPrefix = true;
				break;
			}
		}
		if (!hasPrefix)
			nick.insert(0, prefixes[Util::rand(prefixes.size())]);
		if (minLen && nick.length() < minLen)
		{
			nick += "_R";
			bool addFirst = true;
			while (nick.length() < minLen || addFirst)
			{
				nick += char('0' + Util::rand(10));
				addFirst = false;
			}
			suffixAppended = true;
		}
	}
	if (maxLen && nick.length() > maxLen)
		return false;
	return true;
}

struct CountryCodepage
{
	uint16_t domain;
	uint16_t encoding;
};

// This table is incomplete
static const CountryCodepage domainToEncoding[] =
{
	// 1250
	{ TAG('a','l'), 1250 }, { TAG('c','z'), 1250 }, { TAG('h','r'), 1250 }, { TAG('h','u'), 1250 },
	{ TAG('p','l'), 1250 }, { TAG('s','i'), 1250 }, { TAG('s','k'), 1250 },
	// 1251
	{ TAG('b','g'), 1251 }, { TAG('b','y'), 1251 }, { TAG('r','s'), 1251 }, { TAG('r','u'), 1251 },
	{ TAG('u','a'), 1251 },
	// 1252
	{ TAG('a','t'), 1252 }, { TAG('b','e'), 1252 }, { TAG('c','h'), 1252 }, { TAG('d','e'), 1252 },
	{ TAG('d','k'), 1252 }, { TAG('e','s'), 1252 }, { TAG('f','i'), 1252 }, { TAG('f','r'), 1252 },
	{ TAG('i','e'), 1252 }, { TAG('i','t'), 1252 }, { TAG('n','l'), 1252 }, { TAG('n','o'), 1252 },
	{ TAG('r','o'), 1252 }, { TAG('s','e'), 1252 }, { TAG('u','k'), 1252 },
	// 1253
	{ TAG('g','r'), 1253 },
	// 1254
	{ TAG('t','r'), 1254 },
	// 1255
	{ TAG('i','l'), 1255 },
	// 1256
	{ TAG('e','g'), 1256 }, { TAG('i','r'), 1256 }, { TAG('p','k'), 1256 },
	// 1257
	{ TAG('e','e'), 1257 }, { TAG('l','t'), 1257 }, { TAG('l','v'), 1257 },
	// 1258
	{ TAG('v','n'), 1258 }
};

int NmdcHub::getEncodingFromDomain(const string& domain)
{
	if (domain.length() < 4) return 0;
	string::size_type end = domain.length();
	if (domain.back() == '.') end--;
	string::size_type start = domain.rfind('.', end-1);
	if (start == string::npos || end - start != 3) return 0;
	int c1 = Text::asciiToLower(domain[start+1]);
	int c2 = Text::asciiToLower(domain[start+2]);
	uint16_t tag = TAG(c1, c2);
	for (size_t i = 0; i < _countof(domainToEncoding); ++i)
		if (domainToEncoding[i].domain == tag)
			return domainToEncoding[i].encoding;
	return 0;
}
