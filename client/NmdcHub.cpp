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
#include "ConnectionManager.h"
#include "SearchManager.h"
#include "ShareManager.h"
#include "CryptoManager.h"
#include "UserCommand.h"
#include "DebugManager.h"
#include "QueueManager.h"
#include "UploadManager.h"
#include "ThrottleManager.h"
#include "ParamExpander.h"
#include "StringTokenizer.h"
#include "SimpleStringTokenizer.h"
#include "MappingManager.h"
#include "CompatibilityManager.h"
#include "LogManager.h"
#include "../jsoncpp/include/json/json.h"

static const uint64_t MYINFO_UPDATE_INTERVAL = 2 * 60 * 1000;

CFlyUnknownCommand NmdcHub::g_unknown_command;
CFlyUnknownCommandArray NmdcHub::g_unknown_command_array;
FastCriticalSection NmdcHub::g_unknown_cs;

static const string abracadabraLock("EXTENDEDPROTOCOLABCABCABCABCABCABC");
static const string abracadabraPk("DCPLUSPLUS" DCVERSIONSTRING);

#ifdef _DEBUG
bool suppressUserConn = false;
#endif

enum
{
	ST_NONE,
	ST_SEARCH,
	ST_SA,
	ST_SP
};

NmdcHub::NmdcHub(const string& hubURL, const string& address, uint16_t port, bool secure) :
	Client(hubURL, address, port, '|', secure, Socket::PROTO_NMDC),
	hubSupportFlags(0),
	lastModeChar(0),
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	hubSupportsSlots(false),
#endif // IRAINMAN_ENABLE_AUTO_BAN
	lastUpdate(0),
	myInfoState(WAITING_FOR_MYINFO),
	csUsers(std::unique_ptr<RWLock>(RWLock::create()))
{
	myOnlineUser->getUser()->setFlag(User::NMDC);
	hubOnlineUser->getUser()->setFlag(User::NMDC);
}

NmdcHub::~NmdcHub()
{
	clearUsers();
}

void NmdcHub::disconnect(bool graceless)
{
	Client::disconnect(graceless);
	clearUsers();
	m_cache_hub_url_flood.clear();
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

OnlineUserPtr NmdcHub::getUser(const string& aNick)
{
	OnlineUserPtr ou;
	{
		csState.lock();
		bool isMyNick = aNick == myNick;
		csState.unlock();

		WRITE_LOCK(*csUsers);
#if 0
		if (hub)
		{
			dcassert(users.count(aNick) == 0);
			ou = users.insert(make_pair(aNick, getHubOnlineUser())).first->second;
			dcassert(ou->getIdentity().getNick() == aNick);
			ou->getIdentity().setNick(aNick);
		}
		else
#endif
		if (isMyNick)
		{
//			dcassert(users.count(aNick) == 0);
			auto res = users.insert(make_pair(aNick, getMyOnlineUser()));
			if (!res.second)
			{
				dcassert(res.first->second->getIdentity().getNick() == aNick);
				return res.first->second;
			}
			else
			{
				ou = res.first->second;
				dcassert(ou->getIdentity().getNick() == aNick);
			}
		}
		else
		{
			auto res = users.insert(make_pair(aNick, OnlineUserPtr()));
			if (res.second)
			{
				UserPtr p = ClientManager::getUser(aNick, getHubUrl());
				ou = std::make_shared<OnlineUser>(p, *this, 0);
				ou->getIdentity().setNick(aNick);
				res.first->second = ou;
			}
			else
			{
				dcassert(res.first->second->getIdentity().getNick() == aNick);
				return res.first->second;
			}
		}
	}
	if (!ou->getUser()->getCID().isZero())
	{
		ClientManager::getInstance()->putOnline(ou, true);
		//  is_all_my_info_loaded() без true не начинает качать при загрузке
		//  https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1682
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		UserManager::checkUser(ou);
#endif
	}
	return ou;
}

OnlineUserPtr NmdcHub::findUser(const string& aNick) const
{
	READ_LOCK(*csUsers);
	const auto& i = users.find(aNick);
	return i == users.end() ? OnlineUserPtr() : i->second;
}

void NmdcHub::putUser(const string& aNick)
{
	OnlineUserPtr ou;
	{
		WRITE_LOCK(*csUsers);
		const auto& i = users.find(aNick);
		if (i == users.end())
			return;
		auto bytesShared = i->second->getIdentity().getBytesShared();
		ou = i->second;
		users.erase(i);
		decBytesShared(bytesShared);
	}
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
	{
		ClientManager::getInstance()->putOffline(ou); // [2] https://www.box.net/shared/7b796492a460fe528961
	}
	
	fly_fire2(ClientListener::UserRemoved(), this, ou); // [+] IRainman fix.
}

void NmdcHub::clearUsers()
{
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
			{
				ClientManager::getInstance()->putOffline(i->second);
			}
			else
			{
				dcassert(0);
			}
			// Варианты
			// - скармливать юзеров массивом
			// - Держать юзеров в нескольких контейнерах для каждого хаба отдельно
			// - проработать команду на убивание всей мапы сразу без поиска
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
			int items = sscanf_s(tok.c_str() + 2, "%u/%u/%u", &u[0], &u[1], &u[2]);
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
#ifdef IRAINMAN_ENABLE_AUTO_BAN
			if (slots > 0)
				hubSupportsSlots = true;
#endif // IRAINMAN_ENABLE_AUTO_BAN
		}
		else if (tok.compare(0, 2, "M:", 2) == 0)
		{
			if (tok.length() == 3)
			{
				if (tok[2] == 'A')
					id.getUser()->unsetFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
				else
					id.getUser()->setFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
			}
		}
		else if ((j = tok.find("V:")) != string::npos || (j = tok.find("v:")) != string::npos)
		{
			//dcassert(j > 1);
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
			//dcassert(j > 1);
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
#ifdef FLYLINKDC_COLLECT_UNKNOWN_TAG
		else
		{
			LOCK(NmdcSupports::g_debugCsUnknownNmdcTagParam);
			NmdcSupports::g_debugUnknownNmdcTagParam[tag]++;
			// dcassert(0);
			// TODO - сброс ошибочных тэгов в качестве статы?
		}
#endif // FLYLINKDC_COLLECT_UNKNOWN_TAG
	}
	/// @todo Think about this
	// [-] id.setStringParam("TA", '<' + tag + '>'); [-] IRainman opt.
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
		if (BOOLSETTING(LOG_SEARCH_TRACE))
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
						sendPacket(address, port, sr);
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
	ClientManager::getInstance()->fireIncomingSearch(TYPE_NMDC, searchParam.seeker, searchParam.filter, reply);
}

bool NmdcHub::handlePartialSearch(const NmdcSearchParam& searchParam)
{
	bool isPartial = false;
	if (searchParam.fileType == FILE_TYPE_TTH && isTTHBase32(searchParam.filter))
	{
		PartsInfo partialInfo;
		TTHValue tth(searchParam.filter.c_str() + 4);
		if (QueueManager::handlePartialSearch(tth, partialInfo)) // TODO - часто ищется по ТТХ
		{
#ifdef _DEBUG
			LogManager::message("[OK] handlePartialSearch TTH = " + searchParam.filter);
#endif
			isPartial = true;
			string ip;
			uint16_t port = 0;
			Util::parseIpPort(searchParam.seeker, ip, port);
			dcassert(searchParam.seeker == ip + ':' + Util::toString(port));
			if (port == 0)
				return false;
			try
			{
				AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
				SearchManager::toPSR(cmd, true, getMyNick(), getIpPort(), tth.toBase32(), partialInfo);
				string str = cmd.toString(ClientManager::getMyCID());
				sendPacket(ip, port, str);
				LogManager::psr_message(
				    "[ClientManager::NmdcSearch Send UDP IP = " + ip +
				    " param->udpPort = " + Util::toString(port) +
				    " cmd = " + cmd.toString(ClientManager::getMyCID())
				);
			}
			catch (Exception& e)
			{
				LogManager::psr_message(
				    "[Partial search caught error] Error = " + e.getError() +
				    " IP = " + ip +
				    " param->udpPort = " + Util::toString(port)
				);
				
#ifdef _DEBUG
				LogManager::message("ClientManager::on(NmdcSearch, Partial search caught error = " + e.getError() + " TTH = " + searchParam.filter);
				dcdebug("Partial search caught error\n");
#endif
			}
		}
	}
	return isPartial;
}

void NmdcHub::sendPacket(const string& ip, uint16_t port, string& sr)
{
	// FIXME FIXME: Do we really have to resolve hostnames ?!
	Ip4Address address = Socket::resolveHost(ip);
	if (!address) return; // TODO: log error
	SearchManager::getInstance()->addToSendQueue(sr, address, port);
	if (CMD_DEBUG_ENABLED())
		COMMAND_DEBUG("[Active-Search]" + sr, DebugTask::CLIENT_OUT, ip + ':' + Util::toString(port));
}

string NmdcHub::calcExternalIP() const
{
	string result;
	if (getFavIp().empty())
		result = getLocalIp();
	else
		result = getFavIp();
	result += ':' + SearchManager::getSearchPort();
	return result;
}

inline static bool isTTHChar(char c)
{
	return (c >= '2' && c <= '7') || (c >= 'A' && c <= 'Z');
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
		}
		else if (isActive())
		{
			if (!SearchManager::isSearchPortValid())
				return;
			if (searchParam.seeker == calcExternalIP())
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
		}

		if (searchParam.filter.empty())
			return;
	}
	else
	{
		if (param.length() < 41 || param[39] != ' ') return;
		for (size_t i = 0; i < 39; ++i)
			if (!isTTHChar(param[i]))
				return;
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

	if (searchParam.fileType != FILE_TYPE_TTH && BOOLSETTING(INCOMING_SEARCH_TTH_ONLY))
	{
		ClientManager::getInstance()->fireIncomingSearch(TYPE_NMDC, searchParam.seeker, searchParam.filter, ClientManagerListener::SEARCH_MISS);
		return;
	}

	if (!isPassive)
	{
		string::size_type m = searchParam.seeker.rfind(':');
		if (m == string::npos)
			return;
		if (searchParam.fileType != FILE_TYPE_TTH)
		{
#if 0
			// FIXME FIXME FIXME
			if (m_cache_hub_url_flood.empty())
				m_cache_hub_url_flood = getHubUrlAndIP();
			if (ConnectionManager::checkIpFlood(searchParam.seeker.substr(0, m),
			                                    Util::toInt(searchParam.seeker.substr(m + 1)),
			                                    getIp(), param, m_cache_hub_url_flood))
			{
				return; // http://dchublist.ru/forum/viewtopic.php?f=6&t=1028&start=150
			}
#endif
		}
	}
	else
	{
		OnlineUserPtr u = findUser(searchParam.seeker.substr(4));
			
		if (!u)
			return;
				
		u->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
			
		// ignore if we or remote client don't support NAT traversal in passive mode although many NMDC hubs won't send us passive if we're in passive too, so just in case...
		if (!isActive() && (!(u->getUser()->getFlags() & User::NAT0) || !BOOLSETTING(ALLOW_NAT_TRAVERSAL)))
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
	uint16_t localPort;
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
		myNick = this->myNick;
		localPort = clientSock->getLocalPort();
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
	else if (BOOLSETTING(ALLOW_NAT_TRAVERSAL) && (flags & User::NAT0))
	{
		bool secure = CryptoManager::TLSOk() && (flags & User::TLS);
		// NMDC v2.205 supports "$ConnectToMe sender_nick remote_nick ip:port", but many NMDC hubsofts block it
		// sender_nick at the end should work at least in most used hubsofts
		if (localPort == 0)
		{
			LogManager::message("Error [3] $ConnectToMe port = 0 : ");
		}
		else
		{
			send("$ConnectToMe " + fromUtf8(u->getIdentity().getNick()) + ' ' + getLocalIp() + ':' +
				Util::toString(localPort) +
				(secure ? "NS " : "N ") + fromUtf8(myNick) + '|');
		}
	}
	else
	{
		if (!(flags & User::NMDC_FILES_PASSIVE))
		{
			// [!] IRainman fix: You can not reset the user to flag as if we are passive, not him!
			// [-] u->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
			// [-] Notify the user that we're passive too...
			// [-] updated(u); [-]
			revConnectToMe(*u);
			
			return;
		}
	}
	
}

void NmdcHub::connectToMeParse(const string& param)
{
	string senderNick;
	string port;
	string server;
	string myNick;
	uint16_t localPort;
	{
		LOCK(csState);
		if (state != STATE_NORMAL)
			return;
		myNick = this->myNick;
		localPort = clientSock->getLocalPort();
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
		{
			dcassert(0);
			break;
		}
		i++;
		j = param.find(':', i);
		if (j == string::npos)
		{
			dcassert(0);
			break;
		}
		server = param.substr(i, j - i);
		if (j + 1 >= param.size())
		{
			dcassert(0);
			break;
		}
		
		i = param.find(' ', j + 1);
		if (i == string::npos)
		{
			port = param.substr(j + 1);
		}
		else
		{
			senderNick = param.substr(i + 1);
			port = param.substr(j + 1, i - j - 1);
		}
		
		bool secure = false;
		if (port[port.size() - 1] == 'S')
		{
			port.erase(port.size() - 1);
			if (CryptoManager::TLSOk())
			{
				secure = true;
			}
		}
		
		if (BOOLSETTING(ALLOW_NAT_TRAVERSAL))
		{
			if (port[port.size() - 1] == 'N')
			{
				if (senderNick.empty())
					break;
					
				port.erase(port.size() - 1);
				
				// Trigger connection attempt sequence locally ...
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), localPort,
				                                              BufferedSocket::NAT_CLIENT, myNick, getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				// ... and signal other client to do likewise.
				if (localPort == 0)
				{
					LogManager::message("Error [2] $ConnectToMe port = 0 : ");
				}
				else
				{
					send("$ConnectToMe " + senderNick + ' ' + getLocalIp() + ':' + Util::toString(localPort) + (secure ? "RS|" : "R|"));
				}
				break;
			}
			else if (port[port.size() - 1] == 'R')
			{
				port.erase(port.size() - 1);
				
				// Trigger connection attempt sequence locally
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), localPort,
				                                              BufferedSocket::NAT_SERVER, myNick, getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				break;
			}
		}
		
		if (port.empty())
			break;
			
		// For simplicity, we make the assumption that users on a hub have the same character encoding
		ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), myNick, getHubUrl(),
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
		fly_fire3(ClientListener::StatusMessage(), this, utf8Line, ClientListener::FLAG_IS_SPAM);
		return;
	}
	
	if (lowerLine.find("is kicking") != string::npos && lowerLine.find("because:") != string::npos)
	{
		fly_fire3(ClientListener::StatusMessage(), this, utf8Line, ClientListener::FLAG_IS_SPAM);
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
				
			if (isFloodCommand("<Nick>", line))
				return;
		}
	}
	if (nick.empty())
	{
		fly_fire2(ClientListener::StatusMessage(), this, utf8Line);
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
	// [~] IRainman fix.
	
	if (!chatMessage->from)
	{
		if (user)
		{
			chatMessage->from = user; ////getUser(nick, false, false); // Тут внутри снова идет поиск findUser(nick)
			chatMessage->from->getIdentity().setHub();
		}
	}
	if (!getSuppressChatAndPM())
	{
		chatMessage->translateMe();
		if (!isChatMessageAllowed(*chatMessage, nick))
			return;
		fly_fire2(ClientListener::Message(), this, chatMessage);
	}
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
	fly_fire1(ClientListener::HubUpdated(), this);
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
#ifdef FLYLINKDC_USE_EXT_JSON
		else if (tok == "ExtJSON2")
		{
			flags |= SUPPORTS_EXTJSON2;
		}
#endif
		else if (tok == "TTHS")
		{
			flags |= SUPPORTS_SEARCH_TTHS;
		}
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
	if (type == UserCommand::TYPE_SEPARATOR || type == UserCommand::TYPE_CLEAR)
	{
		int ctx = Util::toInt(param.substr(i));
		fly_fire5(ClientListener::HubUserCommand(), this, type, ctx, Util::emptyString, Util::emptyString);
	}
	else if (type == UserCommand::TYPE_RAW || type == UserCommand::TYPE_RAW_ONCE)
	{
		j = param.find(' ', i);
		if (j == string::npos)
			return;
		int ctx = Util::toInt(param.substr(i));
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
		fly_fire5(ClientListener::HubUserCommand(), this, type, ctx, name, command);
	}
}

void NmdcHub::lockParse(const string& aLine)
{
	if (aLine.size() < 6)
		return;

	{
		LOCK(csState);
		if (state != STATE_PROTOCOL)
			return;
		state = STATE_IDENTIFY;
	}

	dcassert(users.empty());

	// Param must not be toUtf8'd...
	const string param = aLine.substr(6);
	
	if (!param.empty())
	{
		/* [-] IRainman fix: old proto code: http://mydc.ru/topic915.html?p=6719#entry6719
		   [-]string::size_type j = param.find(" Pk=");
		   [-]string lock, pk;
		   [-]if (j != string::npos)
		   [-]{
		   [-]  lock = param.substr(0, j);
		   [-]  pk = param.substr(j + 4);
		   [-]}
		   [-]else
		   [-] */
		// [-]{
		const auto j = param.find(' '); // [!]
		const auto lock = (j != string::npos) ? param.substr(0, j) : param; // [!]
		// [-]}
		// [~] IRainman fix.
		
		if (isExtended(lock))
		{
			string feat = "$Supports";
			feat.reserve(128);
			feat += " UserCommand";
			feat += " NoGetINFO";
			feat += " NoHello";
			feat += " UserIP2";
			feat += " TTHSearch";
			feat += " ZPipe0";
#ifdef FLYLINKDC_USE_EXT_JSON
			if (BOOLSETTING(SEND_EXT_JSON))
				feat += " ExtJSON2";
#endif
			feat += " HubURL";
			feat += " NickRule";
			feat += " SearchRule";
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
			// http://nmdc.sourceforge.net/NMDC.html#_hubtopic
			feat += " HubTopic";
#endif
			feat += " TTHS";
			if (CryptoManager::TLSOk())
				feat += " TLS";
			
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
		//OnlineUserList v;
		const StringTokenizer<string> t(param, "$$", param.size() / 30);
		const StringList& sl = t.getTokens();
		{
			// [-] brain-ripper
			// I can't see any work with ClientListener
			// in this block, so I commented LockInstance,
			// because it caused deadlock in some situations.
			
			//ClientManager::LockInstance lc; // !SMT!-fix
			
			// Perhaps we should use Lock(cs) here, because
			// some elements of this class can be (theoretically)
			// changed in other thread
			
			// LOCK(cs); [-] IRainman fix.
			for (auto it = sl.cbegin(); it != sl.cend() && !ClientManager::isBeforeShutdown(); ++it)
			{
				string::size_type j = 0;
				if ((j = it->find(' ')) == string::npos)
					continue;
				if ((j + 1) == it->length())
					continue;
					
				const string ip = it->substr(j + 1);
				const string user = it->substr(0, j);
#if 0
				if (user == getMyNick())
				{
					const bool l_is_private_ip = Util::isPrivateIp(ip);
					setTypeHub(l_is_private_ip);
					if (l_is_private_ip)
					{
						LogManager::message("Detect local hub: " + getHubUrl() + " private UserIP = " + ip + " User = " + user);
					}
				}
#endif
				OnlineUserPtr ou = findUser(user);
				
				if (!ou)
					continue;
					
				if (ip.find(':') != string::npos)
				{
					ou->getIdentity().setIP6(ip);
					ou->getIdentity().setUseIP6();
				}
				else
				{
					dcassert(!ip.empty());
					ou->getIdentity().setIp(ip);
				}
				//v.push_back(ou);
			}
		}
		// TODO - send only IP changes
		// fireUserListUpdated(v);
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
			// [-] brain-ripper
			// I can't see any work with ClientListener
			// in this block, so I commented LockInstance,
			// because it caused deadlock in some situations.
			
			//ClientManager::LockInstance l; // !SMT!-fix
			
			// Perhaps we should use Lock(cs) here, because
			// some elements of this class can be (theoretically)
			// changed in other thread
			
			// LOCK(cs); [-] IRainman fix: no needs lock here!
			
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
			// [-] brain-ripper
			// I can't see any work with ClientListener
			// in this block, so I commented LockInstance,
			// because it caused deadlock in some situations.
			
			//ClientManager::LockInstance l; // !SMT!-fix
			
			// Perhaps we should use Lock(cs) here, because
			// some elements of this class can be (theoretically)
			// changed in other thread
			
			// LOCK(cs); [-] IRainman fix: no needs any lock here!
			
			for (auto it = sl.cbegin(); it != sl.cend(); ++it) // fix copy-paste
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
		fireUserListUpdated(v); // не убирать - через эту команду шлют "часы"
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
	if (getSuppressChatAndPM())
		return;
	// string param = "FlylinkDC-dev4 From: FlylinkDC-dev4 $<!> ";
	//"SCALOlaz From: 13382 $<k> "
	//string param = p_param;
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
	
	//if (message.replyTo == nullptr || message.from == nullptr) [-] IRainman fix.
	{
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
		
		// Update pointers just in case they've been invalidated
		// message.replyTo = findUser(rtNick); [-] IRainman fix. Imposibru!!!
		// message.from = findUser(fromNick); [-] IRainman fix. Imposibru!!!
	}
	
	message->to = getMyOnlineUser();
	
#if 0
	if (message->to->getUser() == message->from->getUser() && message->from->getUser() == message->replyTo->getUser())
	{
		fly_fire3(ClientListener::StatusMessage(), this, message->text, ClientListener::FLAG_IS_SPAM);
		LogManager::message("Magic spam message (from you to you) filtered on hub: " + getHubUrl() + ".");
		return;
	}
#endif

	if (!isPrivateMessageAllowed(*message))
	{
		if (message->from && message->from->getUser())
		{
			logPM(message->from->getUser(), message->text, getHubUrl());
		}
		return;
	}
	
	fly_fire2(ClientListener::Message(), this, message);
}

void NmdcHub::logPM(const UserPtr& user, const string& msg, const string& hubUrl)
{
	if (BOOLSETTING(LOG_PRIVATE_CHAT))
	{
		StringMap params;
		params["hubNI"] = Util::toString(ClientManager::getHubNames(user->getCID(), hubUrl));
		params["hubURL"] = Util::toString(ClientManager::getHubs(user->getCID(), hubUrl));
		params["userNI"] = user->getLastNick();
		params["myCID"] = ClientManager::getMyCID().toBase32();
		params["message"] = msg;
		LOG(PM, params);
	}
	LogManager::speakStatusMessage(msg);
}

void NmdcHub::onLine(const string& aLine)
{
	if (aLine.empty())
		return;
		
#ifdef _DEBUG
//	if (aLine.find("$Search") == string::npos)
//	{
//		LogManager::message("[NmdcHub::onLine][" + getHubUrl() + "] aLine = " + aLine);
//	}
#endif

	if (aLine[0] != '$')
	{
		chatMessageParse(aLine);
		return;
	}
	
	string cmd;
	string param;
	string::size_type x = aLine.find(' ');
	int searchType = ST_NONE;
	bool isMyInfo = false;
	if (x == string::npos)
	{
		cmd = aLine.substr(1);
	}
	else
	{
		cmd = aLine.substr(1, x - 1);
		param = toUtf8(aLine.substr(x + 1));
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
	if (searchType == ST_NONE && isFloodCommand(cmd, param))
	{
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
#ifdef FLYLINKDC_USE_EXT_JSON
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
		SearchManager::getInstance()->onSearchResult(aLine);
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
		lockParse(aLine); // aLine!
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
		fly_fire2(ClientListener::Redirect(), this, param);
	}
	else if (cmd == "HubIsFull")
	{
		fly_fire1(ClientListener::HubFull(), this);
	}
	else if (cmd == "ValidateDenide")        // Mind the spelling...
	{
		dcassert(clientSock);
		csState.lock();
		if (clientSock)
			clientSock->disconnect(false);
		csState.unlock();
		fly_fire1(ClientListener::NickError(), ClientListener::Taken);
	}
	else if (cmd == "UserIP")
	{
		userIPParse(param);
	}
	else if (cmd == "BotList")
	{
		botListParse(param);
	}
	else if (cmd == "NickList") // TODO - убить
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
	else if (cmd == "GetPass")
	{
		csState.lock();
		string myNick = this->myNick;
		string pwd = storedPassword;
		csState.unlock();
		getUser(myNick);
		setRegistered();
		// setMyIdentity(ou->getIdentity()); [-]
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
		csState.lock();
		clientSock->setMode(BufferedSocket::MODE_ZPIPE);
		csState.unlock();
	}
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
	else if (cmd == "HubTopic")
	{
		if (!param.empty())
			fly_fire3(ClientListener::HubInfoMessage(), ClientListener::HubTopic, this, param);
	}
#endif
	else if (cmd == "LogedIn")
	{
		fly_fire3(ClientListener::HubInfoMessage(), ClientListener::LoggedIn, this, Util::emptyString);
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
		fly_fire1(ClientListener::NickError(), ClientListener::Rejected);
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
					if (value > 0)
						setSearchInterval(value * 1000, true);
				}
				if (key == "IntPas")
				{
					int value = Util::toInt(rule.c_str() + pos + 1);
					if (value > 0)
						setSearchIntervalPassive(value * 1000, true);
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
	{
		//dcassert(0);
		dcdebug("NmdcHub::onLine Unknown command %s\n", aLine.c_str());
		string l_message;
		{
			LOCK(g_unknown_cs);
			g_unknown_command_array[getHubUrl()][cmd]++;
			auto& l_item = g_unknown_command[cmd + "[" + getHubUrl() + "]"];
			l_item.second++;
			if (l_item.first.empty())
			{
				l_item.first = aLine;
				l_message = "NmdcHub::onLine first unknown command! hub = [" + getHubUrl() + "], command = [" + cmd + "], param = [" + param + "]";
			}
		}
		if (!l_message.empty())
		{
			LogManager::message(l_message + " Raw = " + aLine);
		}
	}
	updateMyInfoState(isMyInfo);
}

string NmdcHub::get_all_unknown_command()
{
	string l_message;
	LOCK(g_unknown_cs);
	for (auto i = g_unknown_command_array.cbegin(); i != g_unknown_command_array.cend(); ++i)
	{
		l_message += "Hub: " + i->first + " Invalid command: ";
		string l_separator;
		for (auto j = i->second.cbegin(); j != i->second.cend(); ++j)
		{
			l_message += l_separator + j->first + " ( count: " + Util::toString(j->second) + ") ";
			l_separator = " ";
		}
	}
	return l_message;
}

void NmdcHub::log_all_unknown_command()
{
	{
		LOCK(g_unknown_cs);
		for (auto i = g_unknown_command.cbegin(); i != g_unknown_command.cend(); ++i)
		{
			const string l_message = "NmdcHub::onLine summary unknown command! Count = " +
			                         Util::toString(i->second.second) + " Key = [" + i->first + "], first value = [" + i->second.first + "]";
			LogManager::message(l_message);
		}
		g_unknown_command.clear();
	}
	LogManager::message(get_all_unknown_command());
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

	bool secure = CryptoManager::TLSOk() && (user.getUser()->getFlags() & User::TLS);	
	uint16_t port = secure ? ConnectionManager::getInstance()->getSecurePort() : ConnectionManager::getInstance()->getPort();
	if (port == 0)
	{
		LogManager::message("Error [2] $ConnectToMe port = 0 :");
		dcassert(0);
		return;
	}

	dcdebug("NmdcHub::connectToMe %s\n", user.getIdentity().getNick().c_str());
	const string nick = fromUtf8(user.getIdentity().getNick());
	uint64_t expires = token.empty() ? GET_TICK() + 45000 : UINT64_MAX;
	if (!ConnectionManager::getInstance()->nmdcExpect(nick, myNick, getHubUrl(), token, getEncoding(), expires))
		return;

	ConnectionManager::g_ConnToMeCount++;
	send("$ConnectToMe " + nick + ' ' + getLocalIp() + ':' + Util::toString(port) + (secure ? "S|" : "|"));
}

void NmdcHub::revConnectToMe(const OnlineUser& aUser)
{
	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return;
		myNick = this->myNick;
	}
	dcdebug("NmdcHub::revConnectToMe %s\n", aUser.getIdentity().getNick().c_str());
	send("$RevConnectToMe " + fromUtf8(myNick) + ' ' + fromUtf8(aUser.getIdentity().getNick()) + '|');
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
	if (setPassword)
	{
		LOCK(csState);
		storedPassword = pwd;
	}
	send("$MyPass " + fromUtf8(pwd) + '|');
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
	{
		LOCK(csState);
		uint64_t nextUpdate = lastUpdate + MYINFO_UPDATE_INTERVAL;
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
		if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)
			modeChar = '5';
		else if (isActive())
			modeChar = 'A';
		else
			modeChar = 'P';
	}
	const int64_t upLimit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getUploadLimitInKBytes() : 0;
	const string uploadSpeed = upLimit > 0 ? Util::toString(upLimit) + " KiB/s" : SETTING(UPLOAD_SPEED);
	
	char status = NmdcSupports::NORMAL;
	
	if (Util::getAway())
	{
		status |= NmdcSupports::AWAY;
	}
	if (UploadManager::getInstance()->getIsFileServerStatus())
	{
		status |= NmdcSupports::SERVER;
	}
	if (UploadManager::getInstance()->getIsFireballStatus())
	{
		status |= NmdcSupports::FIREBALL;
	}
	if (BOOLSETTING(ALLOW_NAT_TRAVERSAL) && !isActive())
	{
		status |= NmdcSupports::NAT0;
	}
	
	if (CryptoManager::TLSOk())
	{
		status |= NmdcSupports::TLS;
	}
	
	unsigned normal, registered, op;
	if (isExclusiveHub)
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
	currentMyInfo += Util::toString(UploadManager::getSlots());
	currentMyInfo += ">$ $";
	currentMyInfo += uploadSpeed;
	currentMyInfo += status;
	currentMyInfo += '$';
	currentMyInfo += fromUtf8(escape(getCurrentEmail()));
	currentMyInfo += '$';
	                                 
	int64_t bytesShared, filesShared;
	if (hideShare)
		bytesShared = filesShared = 0;
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
#ifdef FLYLINKDC_USE_EXT_JSON
		if (supportFlags & SUPPORTS_EXTJSON2)
		{
			Json::Value json;
			json["Gender"] = SETTING(GENDER) + 1;
			if (!hideShare)
				json["Files"] = filesShared;
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

			string jsonStr = json.toStyledString(false);
			
			boost::algorithm::trim(jsonStr);
			Text::removeString_rn(jsonStr);
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
#endif // FLYLINKDC_USE_EXT_JSON
	}
}

void NmdcHub::searchToken(const SearchParamToken& sp)
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
	if (SearchManager::getSearchPortUint() == 0)
	{
		active = false;
		LogManager::message("Error: UDP port is zero");
	}
	string cmd;
	if ((supportFlags & SUPPORTS_SEARCH_TTHS) == SUPPORTS_SEARCH_TTHS && sp.fileType == FILE_TYPE_TTH)
	{
		dcassert(sp.filter == TTHValue(sp.filter).toBase32());
		if (active)
			cmd = "$SA " + sp.filter + ' ' + calcExternalIP() + '|';
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
			cmd += calcExternalIP();
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

void NmdcHub::privateMessage(const string& nick, const string& myNick, const string& message, bool thirdPerson)
{
	string cmd = "$To: ";
	cmd += fromUtf8(nick);
	cmd += " From: ";
	string myNickEncoded = fromUtf8(myNick);
	cmd += myNickEncoded;
	cmd += " $<";
	cmd += escape(myNickEncoded);
	cmd += "> ";
	if (thirdPerson) cmd += "/me ";
	cmd += escape(fromUtf8(message));
	cmd += '|';
	send(cmd);
}

void NmdcHub::privateMessage(const OnlineUserPtr& user, const string& message, bool thirdPerson)
{
	if (getSuppressChatAndPM())
		return;

	string myNick;
	{
		LOCK(csState);
		if (state != STATE_NORMAL) return;
		myNick = this->myNick;
	}
	
	privateMessage(user->getIdentity().getNick(), myNick, message, thirdPerson);
	// Emulate a returning message...
	// LOCK(cs); // !SMT!-fix: no data to lock
	
	const OnlineUserPtr& me = getMyOnlineUser();
	
	unique_ptr<ChatMessage> chatMessage(new ChatMessage(message, me, user, me, thirdPerson));
	if (!isPrivateMessageAllowed(*chatMessage))
		return;
		
	fly_fire2(ClientListener::Message(), this, chatMessage);
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
			privateMessage(command.getTo(), myNick, cmd, false);
	}
	else
		send(fromUtf8(cmd));
}

void NmdcHub::onConnected() noexcept
{
	Client::onConnected();
	
	{
		LOCK(csState);
		if (state != STATE_PROTOCOL)
			return;
		lastModeChar = 0;
		hubSupportFlags = 0;
		lastMyInfo.clear();
		lastUpdate = pendingUpdate = 0;
		lastExtJSONInfo.clear();
	}
}

#ifdef FLYLINKDC_USE_EXT_JSON
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
	try
	{
		Json::Value root;
		Json::Reader reader(Json::Features::strictMode());
		if (!reader.parse(text, root))
		{
			LogManager::message("Failed to parse ExtJSON: " + param, false);
			return false;
		}
		ou->getIdentity().setExtJSON();
		ou->getIdentity().setStringParam("F4", root["Gender"].asString());
		ou->getIdentity().setExtJSONSupportInfo(root["Support"].asString());
		ou->getIdentity().setExtJSONRAMWorkingSet(root["RAM"].asInt());
		ou->getIdentity().setExtJSONRAMPeakWorkingSet(root["RAMPeak"].asInt());
		ou->getIdentity().setExtJSONRAMFree(root["RAMFree"].asInt());
		//ou->getIdentity().setExtJSONGDI(root["GDI"].asInt());
		ou->getIdentity().setExtJSONCountFiles(root["Files"].asInt());
		ou->getIdentity().setExtJSONLastSharedDate(root["LastDate"].asInt64());
		ou->getIdentity().setExtJSONSQLiteDBSize(root["SQLSize"].asInt());
		ou->getIdentity().setExtJSONlevelDBHistSize(root["LDBHistSize"].asInt());
		ou->getIdentity().setExtJSONSQLiteDBSizeFree(root["SQLFree"].asInt());
		ou->getIdentity().setExtJSONQueueFiles(root["QueueFiles"].asInt());
		ou->getIdentity().setExtJSONQueueSrc(root["QueueSrc"].asInt64()); //TODO - временны баг - тут 32 бита
		ou->getIdentity().setExtJSONTimesStartCore(root["StartCore"].asInt64());  //TODO тут тоже 32 бита
		ou->getIdentity().setExtJSONTimesStartGUI(root["StartGUI"].asInt64()); //TODO тут тоже 32 бита
		fireUserUpdated(ou);
		return true;
	}
	catch (Json::RuntimeError& e)
	{
		LogManager::message("Failed to parse ExtJSON: " + param + " (" + string(e.what()) + ")", false);
	}
	return false;
}
#endif // FLYLINKDC_USE_EXT_JSON

void NmdcHub::myInfoParse(const string& param)
{
	if (ClientManager::isBeforeShutdown())
		return;
	string::size_type i = 5;
	string::size_type j = param.find(' ', i);
	if (j == string::npos || j == i)
		return;
	const string nick = param.substr(i, j - i);
	
	dcassert(!nick.empty())
	if (nick.empty())
	{
		dcassert(0);
		return;
	}
	i = j + 1;
	
	OnlineUserPtr ou = getUser(nick);
	//ou->getUser()->setFlag(User::IS_MYINFO);
	j = param.find('$', i);
	dcassert(j != string::npos)
	if (j == string::npos)
		return;
	string tmpDesc = unescape(param.substr(i, j - i));
	// Look for a tag...
	if (!tmpDesc.empty() && tmpDesc[tmpDesc.size() - 1] == '>')
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
		dcassert(param.size() > j + 2);
		if (param.size() > j + 3)
		{
			if (param[j] == '$')
			{
				if (param[j + 1] == 'A')
				{
					ou->getIdentity().getUser()->unsetFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
				}
				else if (param[j + 1] == 'P')
				{
					ou->getIdentity().getUser()->setFlag(User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE);
				}
			}
		}
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
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1]);
	}
	else
	{
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1], param.substr(i, j - i - 1));
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

void NmdcHub::onDDoSSearchDetect(const string& p_error) noexcept
{
	fly_fire1(ClientListener::DDoSSearchDetect(), p_error);
}

void NmdcHub::onDataLine(const string& aLine) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		Client::onDataLine(aLine); // TODO skip Start
		onLine(aLine);
	}
}

void NmdcHub::onFailed(const string& aLine) noexcept
{
	clearUsers();
	Client::onFailed(aLine);
	updateCounts(true);
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
	if (lock.size() < 3 || lock.length() > 512) // How long can it be?
		return Util::emptyString;
		
	uint8_t* temp = static_cast<uint8_t*>(_alloca(lock.length()));
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
