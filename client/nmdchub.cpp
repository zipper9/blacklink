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
#include "ShareManager.h"
#include "CryptoManager.h"
#include "UserCommand.h"
#include "DebugManager.h"
#include "QueueManager.h"
#include "ThrottleManager.h"
#include "StringTokenizer.h"
#include "MappingManager.h"
#include "CompatibilityManager.h"
#include "../jsoncpp/include/json/json.h"

CFlyUnknownCommand NmdcHub::g_unknown_command;
CFlyUnknownCommandArray NmdcHub::g_unknown_command_array;
FastCriticalSection NmdcHub::g_unknown_cs;
uint8_t NmdcHub::g_version_fly_info = 33;

static const string abracadabraLock("EXTENDEDPROTOCOLABCABCABCABCABCABC");
static const string abracadabraPk("DCPLUSPLUS"  A_VERSIONSTRING);

NmdcHub::NmdcHub(const string& hubURL, bool secure) :
	Client(hubURL, '|', secure, Socket::PROTO_NMDC),
	m_supportFlags(0),
	m_modeChar(0),
	m_version_fly_info(0),
	m_lastBytesShared(0),
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	m_hubSupportsSlots(false),
#endif // IRAINMAN_ENABLE_AUTO_BAN
	m_lastUpdate(0),
	m_is_get_user_ip_from_hub(false),
	myInfoState(WAITING_FOR_MYINFO)
{
#ifdef RIP_USE_CONNECTION_AUTODETECT
	m_bAutodetectionPending = true;
	m_iRequestCount = 0;
	resetDetectActiveConnection();
#endif
	m_myOnlineUser->getUser()->setFlag(User::NMDC);
	m_hubOnlineUser->getUser()->setFlag(User::NMDC);
}

NmdcHub::~NmdcHub()
{
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
	dcassert(m_ext_json_deferred.empty());
#endif
	clearUsers();
}

#define checkstate() if(state != STATE_NORMAL) return

void NmdcHub::disconnect(bool p_graceless)
{
	m_is_get_user_ip_from_hub = false;
	Client::disconnect(p_graceless);
	clearUsers();
	m_cache_hub_url_flood.clear();
}

void NmdcHub::connect(const OnlineUser& p_user, const string& p_token, bool p_is_force_passive)
{
	checkstate();
	dcdebug("NmdcHub::connect %s\n", p_user.getIdentity().getNick().c_str());
	if (p_is_force_passive == false && isActive())
	{
		connectToMe(p_user);
	}
	else
	{
		revConnectToMe(p_user);
	}
}

void NmdcHub::refreshUserList(bool refreshOnly)
{
	if (refreshOnly)
	{
		OnlineUserList v;
		v.reserve(m_users.size());
		{
			// [!] IRainman fix potential deadlock.
			CFlyReadLock(*m_cs);
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				v.push_back(i->second);
			}
		}
		fire_user_updated(v);
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
		CFlyWriteLock(*m_cs);
#if 0
		if (hub)
		{
			dcassert(m_users.count(aNick) == 0);
			ou = m_users.insert(make_pair(aNick, getHubOnlineUser())).first->second;
			dcassert(ou->getIdentity().getNick() == aNick);
			ou->getIdentity().setNick(aNick);
		}
		else
#endif
		if (aNick == getMyNick())
		{
//			dcassert(m_users.count(aNick) == 0);
			auto res = m_users.insert(make_pair(aNick, getMyOnlineUser()));
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
			auto res = m_users.insert(make_pair(aNick, OnlineUserPtr()));
			if (res.second)
			{
				UserPtr p = ClientManager::getUser(aNick, getHubUrl(), getHubID());
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

void NmdcHub::supports(const StringList& feat)
{
	const string x = Util::toSupportsCommand(feat);
	send(x);
}

OnlineUserPtr NmdcHub::findUser(const string& aNick) const
{
	CFlyReadLock(*m_cs);
	const auto& i = m_users.find(aNick);
	return i == m_users.end() ? OnlineUserPtr() : i->second;
}

void NmdcHub::putUser(const string& aNick)
{
	OnlineUserPtr ou;
	{
		CFlyWriteLock(*m_cs);
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
		m_ext_json_deferred.erase(aNick);
#endif
		const auto& i = m_users.find(aNick);
		if (i == m_users.end())
			return;
		auto l_bytes_shared = i->second->getIdentity().getBytesShared();
		ou = i->second;
		m_users.erase(i);
		decBytesSharedL(l_bytes_shared);
	}
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
	{
		ClientManager::getInstance()->putOffline(ou); // [2] https://www.box.net/shared/7b796492a460fe528961
	}
	
	fly_fire2(ClientListener::UserRemoved(), this, ou); // [+] IRainman fix.
}

void NmdcHub::clearUsers()
{
	if (ClientManager::isBeforeShutdown())
	{
		CFlyWriteLock(*m_cs);
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
		m_ext_json_deferred.clear();
#endif
		m_users.clear();
		clearAvailableBytesL();
	}
	else
	{
		NickMap u2;
		{
			CFlyWriteLock(*m_cs);
			u2.swap(m_users);
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
			m_ext_json_deferred.clear();
#endif
			clearAvailableBytesL();
		}
		for (auto i = u2.cbegin(); i != u2.cend(); ++i)
		{
			//i->second->getIdentity().setBytesShared(0);
			if (!i->second->getUser()->getCID().isZero()) // [+] IRainman fix.
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

void NmdcHub::updateFromTag(Identity& id, const string & tag, bool p_is_version_change) // [!] IRainman opt.
{
	const StringTokenizer<string> tok(tag, ',', 4); // TODO - убрать разбор токенов. сделать простое сканирование в цикле в поиске запятых
	string::size_type j;
	id.setLimit(0);
	for (auto i = tok.getTokens().cbegin(); i != tok.getTokens().cend(); ++i)
	{
		if (i->length() < 2)
		{
			continue;
		}
		// [!] IRainman opt: first use the compare, and only then to find.
		else if (i->compare(0, 2, "H:", 2) == 0)
		{
			int u[3] = {0};
			const auto l_items = sscanf_s(i->c_str() + 2, "%u/%u/%u", &u[0], &u[1], &u[2]);
			if (l_items != 3)
				continue;
			id.setHubNormal(u[0]);
			id.setHubRegister(u[1]);
			id.setHubOperator(u[2]);
		}
		else if (i->compare(0, 2, "S:", 2) == 0)
		{
			const uint16_t slots = Util::toInt(i->c_str() + 2);
			id.setSlots(slots);
#ifdef IRAINMAN_ENABLE_AUTO_BAN
			if (slots > 0)
			{
				m_hubSupportsSlots = true;
			}
#endif // IRAINMAN_ENABLE_AUTO_BAN
		}
		else if (i->compare(0, 2, "M:", 2) == 0)
		{
			if (i->size() == 3)
			{
				// [!] IRainman fix.
				if ((*i)[2] == 'A')
				{
					id.getUser()->unsetFlag(User::NMDC_FILES_PASSIVE);
					id.getUser()->unsetFlag(User::NMDC_SEARCH_PASSIVE);
				}
				else
				{
					id.getUser()->setFlag(User::NMDC_FILES_PASSIVE);
					id.getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
				}
				// [~] IRainman fix.
			}
		}
		else if ((j = i->find("V:")) != string::npos ||
		         (j = i->find("v:")) != string::npos
		        )
		{
			//dcassert(j > 1);
			if (p_is_version_change)
			{
				if (j > 1)
				{
					id.setStringParam("AP", i->substr(0, j - 1));
				}
				id.setStringParam("VE", i->substr(j + 2));
			}
		}
		else if ((j = i->find("L:")) != string::npos)
		{
			const uint32_t l_limit = Util::toInt(i->c_str() + j + 2);
			id.setLimit(l_limit * 1024);
		}
		else if ((j = i->find(' ')) != string::npos)
		{
			//dcassert(j > 1);
			if (p_is_version_change)
			{
				if (j > 1)
				{
					id.setStringParam("AP", i->substr(0, j - 1));
				}
				id.setStringParam("VE", i->substr(j + 1));
			}
		}
		else if ((j = i->find("++")) != string::npos)
		{
			if (p_is_version_change)
			{
				id.setStringParam("AP", *i);
			}
		}
		else if (i->compare(0, 2, "O:", 2) == 0)
		{
			// [?] TODO http://nmdc.sourceforge.net/NMDC.html#_tag
		}
		else if (i->compare(0, 2, "C:", 2) == 0)
		{
			// http://dchublist.ru/forum/viewtopic.php?p=24035#p24035
		}
		// [~] IRainman fix.
#ifdef FLYLINKDC_COLLECT_UNKNOWN_TAG
		else
		{
			CFlyFastLock(NmdcSupports::g_debugCsUnknownNmdcTagParam);
			NmdcSupports::g_debugUnknownNmdcTagParam[tag]++;
			// dcassert(0);
			// TODO - сброс ошибочных тэгов в качестве статы?
		}
#endif // FLYLINKDC_COLLECT_UNKNOWN_TAG
		// [~] IRainman fix.
	}
	/// @todo Think about this
	// [-] id.setStringParam("TA", '<' + tag + '>'); [-] IRainman opt.
}

void NmdcHub::handleSearch(const SearchParam& searchParam)
{
	ClientManagerListener::SearchReply reply = ClientManagerListener::SEARCH_MISS;
	SearchResultList searchResults;
	dcassert(searchParam.maxResults > 0);
	dcassert(searchParam.client);
	if (ClientManager::isBeforeShutdown())
		return;
	ShareManager::search(searchResults, searchParam, this);
	if (!searchResults.empty())
	{
		reply = ClientManagerListener::SEARCH_HIT;
		unsigned slots = UploadManager::getSlots();
		unsigned freeSlots = UploadManager::getFreeSlots();
		if (searchParam.isPassive)
		{
			const string name = searchParam.seeker.substr(4);
			// Good, we have a passive seeker, those are easier...
			string str;
			for (auto i = searchResults.cbegin(); i != searchResults.cend(); ++i)
			{
				const auto& sr = *i;
				str += sr.toSR(*this, freeSlots, slots);
				str[str.length() - 1] = 5;
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//				str += name;
//#else
				str += fromUtf8(name);
//#endif
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
					if (ConnectionManager::checkDuplicateSearchFile(sr))
					{
						if (CMD_DEBUG_ENABLED())
							COMMAND_DEBUG("[~][0]$SR [SkipUDP-File] " + sr, DebugTask::HUB_IN, getIpPort());
					}
					else
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
		if (!searchParam.isPassive)
		{
			if (handlePartialSearch(searchParam))
				reply = ClientManagerListener::SEARCH_PARTIAL_HIT;
		}
	}
	ClientManager::getInstance()->fireIncomingSearch(searchParam.seeker, searchParam.filter, reply);
}

bool NmdcHub::handlePartialSearch(const SearchParam& searchParam)
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
	boost::asio::ip::address_v4 address = Socket::resolveHost(ip);
	if (address.is_unspecified()) return; // TODO: log error
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

void NmdcHub::searchParse(const string& param)
{
	if (param.length() < 4) return;
	if (state != STATE_NORMAL
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	        || getHideShare()
#endif
	   )
		return;
	SearchParam searchParam;
	searchParam.rawSearch = param;
	
	bool isPassive = param.compare(0, 4, "Hub:", 4) == 0;

	string::size_type i = 0;
	string::size_type j = param.find(' ', i);
	searchParam.queryPos = j;
	if (j == string::npos || i == j)
	{
		return;
	}
	searchParam.seeker = param.substr(i, j - i);
	
	// Filter own searches
	if (isPassive)
	{
		const string& myNick = getMyNick();
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
	{
		return;
	}
	if (param[i + 1] != '?' || param[i + 3] != '?')
		return;
	if (param[i] == 'F')
		searchParam.sizeMode = Search::SIZE_DONTCARE;
	else if (param[i + 2] == 'F')
		searchParam.sizeMode = Search::SIZE_ATLEAST;
	else
		searchParam.sizeMode = Search::SIZE_ATMOST;
	i += 4;
	j = param.find('?', i);
	if (j == string::npos || i == j)
	{
		return;
	}
	if (j - i == 1 && param[i] == '0')
		searchParam.size = 0;
	else
		searchParam.size = Util::toInt64(param.c_str() + i);
	i = j + 1;
	j = param.find('?', i);
	if (j == string::npos || i == j)
	{
		return;
	}
	searchParam.fileType = Util::toInt(param.c_str() + i) - 1;
	i = j + 1;
	
	if (searchParam.fileType == FILE_TYPE_TTH)
	{
		if (param.length() - i == 39 + 4)
			searchParam.filter = param.substr(i);
	}
	else
		searchParam.filter = unescape(param.substr(i));

	if (searchParam.filter.empty())
		return;

	if (!isPassive)
	{
		string::size_type m = searchParam.seeker.rfind(':');
		if (m == string::npos)
			return;
		if (searchParam.fileType != FILE_TYPE_TTH)
		{
			// FIXME FIXME FIXME
			if (m_cache_hub_url_flood.empty())
				m_cache_hub_url_flood = getHubUrlAndIP();
			if (ConnectionManager::checkIpFlood(searchParam.seeker.substr(0, m),
			                                    Util::toInt(searchParam.seeker.substr(m + 1)),
			                                    getIp(), param, m_cache_hub_url_flood))
			{
				return; // http://dchublist.ru/forum/viewtopic.php?f=6&t=1028&start=150
			}
		}
	}
	else
	{
		OnlineUserPtr u = findUser(searchParam.seeker.substr(4));
			
		if (!u)
			return;
				
		u->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
			
		// ignore if we or remote client don't support NAT traversal in passive mode although many NMDC hubs won't send us passive if we're in passive too, so just in case...
		if (!isActive() && (!u->getUser()->isSet(User::NAT0) || !BOOLSETTING(ALLOW_NAT_TRAVERSAL)))
		{
			return;
		}
	}
	searchParam.init(this, isPassive);
	handleSearch(searchParam);
}

void NmdcHub::revConnectToMeParse(const string& param)
{
	if (state != STATE_NORMAL)
	{
		return;
	}
	
	string::size_type j = param.find(' ');
	if (j == string::npos)
	{
		return;
	}
	
	OnlineUserPtr u = findUser(param.substr(0, j));
	if (!u)
		return;
		
	if (isActive())
	{
		connectToMe(*u);
	}
	else if (BOOLSETTING(ALLOW_NAT_TRAVERSAL) && u->getUser()->isSet(User::NAT0))
	{
		bool secure = CryptoManager::TLSOk() && u->getUser()->isSet(User::TLS);
		// NMDC v2.205 supports "$ConnectToMe sender_nick remote_nick ip:port", but many NMDC hubsofts block it
		// sender_nick at the end should work at least in most used hubsofts
		if (clientSock->getLocalPort() == 0)
		{
			LogManager::message("Error [3] $ConnectToMe port = 0 : ");
		}
		else
		{
			send("$ConnectToMe " + fromUtf8(u->getIdentity().getNick()) + ' ' + getLocalIp() + ':' + Util::toString(clientSock->getLocalPort()) + (secure ? "NS " : "N ") + getMyNickFromUtf8() + '|');
		}
	}
	else
	{
		// [!] IRainman fix.
		if (!u->getUser()->isSet(User::NMDC_FILES_PASSIVE))
		{
			// [!] IRainman fix: You can not reset the user to flag as if we are passive, not him!
			// [-] u->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
			// [-] Notify the user that we're passive too...
			// [-] updated(u); [-]
			revConnectToMe(*u);
			
			return;
		}
		// [~] IRainman fix.
	}
	
}

void NmdcHub::connectToMeParse(const string& param)
{
	string senderNick;
	string port;
	string server;
	while (true)
	{
		if (state != STATE_NORMAL)
		{
			dcassert(0);
			break;
		}
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
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), clientSock->getLocalPort(),
				                                              BufferedSocket::NAT_CLIENT, getMyNick(), getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				// ... and signal other client to do likewise.
				if (clientSock->getLocalPort() == 0)
				{
					LogManager::message("Error [2] $ConnectToMe port = 0 : ");
				}
				else
				{
					send("$ConnectToMe " + senderNick + ' ' + getLocalIp() + ':' + Util::toString(clientSock->getLocalPort()) + (secure ? "RS|" : "R|"));
				}
				break;
			}
			else if (port[port.size() - 1] == 'R')
			{
				port.erase(port.size() - 1);
				
				// Trigger connection attempt sequence locally
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), clientSock->getLocalPort(),
				                                              BufferedSocket::NAT_SERVER, getMyNick(), getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				break;
			}
		}
		
		if (port.empty())
			break;
			
		// For simplicity, we make the assumption that users on a hub have the same character encoding
		ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), getMyNick(), getHubUrl(),
		                                              getEncoding(),
		                                              secure);
		break; // Все ОК тут брек хороший
	}
#ifdef FLYLINKDC_USE_COLLECT_STAT
	const string l_hub = getHubUrl();
	CFlylinkDBManager::getInstance()->push_dc_command_statistic(l_hub.empty() ? "-" : l_hub, param, server, port, senderNick);
#endif
}

void NmdcHub::chatMessageParse(const string& line)
{
	const string utf8Line = toUtf8(unescape(line));
	const string lowerLine = Text::toLower(utf8Line);

	// Check if we're being banned...
	if (state != STATE_NORMAL)
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
	
	const auto user = findUser(nick);
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
	if (!isSupressChatAndPM())
	{
		chatMessage->translateMe();
		if (!allowChatMessagefromUser(*chatMessage, nick)) // [+] IRainman fix.
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
	// If no " - " found, first word goes to hub name, rest to description
	string::size_type i = param.find(" - ");
	if (i == string::npos)
	{
		i = param.find(' ');
		if (i == string::npos)
		{
			getHubIdentity().setNick(unescape(param));
			getHubIdentity().setDescription(Util::emptyString);
		}
		else
		{
			getHubIdentity().setNick(unescape(param.substr(0, i)));
			getHubIdentity().setDescription(unescape(param.substr(i + 1)));
		}
	}
	else
	{
		getHubIdentity().setNick(unescape(param.substr(0, i)));
		getHubIdentity().setDescription(unescape(param.substr(i + 3)));
	}
	if (BOOLSETTING(STRIP_TOPIC))
	{
		getHubIdentity().setDescription(Util::emptyString);
	}
	fly_fire1(ClientListener::HubUpdated(), this);
}

void NmdcHub::supportsParse(const string& param)
{
	const StringTokenizer<string> st(param, ' '); // TODO убрать токены. сделать поиском.
	const StringList& sl = st.getTokens();
	for (auto i = sl.cbegin(); i != sl.cend(); ++i)
	{
		if (*i == "UserCommand")
		{
			m_supportFlags |= SUPPORTS_USERCOMMAND;
		}
		else if (*i == "NoGetINFO")
		{
			m_supportFlags |= SUPPORTS_NOGETINFO;
		}
		else if (*i == "UserIP2")
		{
			m_supportFlags |= SUPPORTS_USERIP2;
		}
		else if (*i == "NickRule")
		{
			m_supportFlags |= SUPPORTS_NICKRULE;
		}
		else if (*i == "SearchRule")
		{
			m_supportFlags |= SUPPORTS_SEARCHRULE;
		}
#ifdef FLYLINKDC_USE_EXT_JSON
		else if (*i == "ExtJSON2")
		{
			m_supportFlags |= SUPPORTS_EXTJSON2;
			fly_fire1(ClientListener::FirstExtJSON(), this);
		}
#endif
		else if (*i == "TTHS")
		{
			m_supportFlags |= SUPPORTS_SEARCH_TTHS;
		}
	}
	// if (!(m_supportFlags & SUPPORTS_NICKRULE))
	/*
	<Mer> [00:27:44] *** Соединён
	- [00:27:47] <MegaHub> Время работы: 129 дней 8 часов 23 минут 21 секунд. Пользователей онлайн: 10109
	- [00:27:51] <MegaHub> Operation timeout (ValidateNick)
	- [00:27:52] *** [Hub = dchub://hub.o-go.ru] Соединение закрыто
	{
	    const auto l_nick = getMyNick();
	    OnlineUserPtr ou = getUser(l_nick, false, true);
	    sendValidateNick(ou->getIdentity().getNick());
	}
	*/
}

void NmdcHub::userCommandParse(const string& param)
{
	string::size_type i = 0;
	string::size_type j = param.find(' ');
	if (j == string::npos)
		return;
		
	int type = Util::toInt(param.substr(0, j));
	i = j + 1;
	if (type == UserCommand::TYPE_SEPARATOR || type == UserCommand::TYPE_CLEAR || type == UserCommand::TYPE_SEPARATOR_OLD)
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
		string l_name = unescape(param.substr(i, j - i));
		// NMDC uses '\' as a separator but both ADC and our internal representation use '/'
		Util::replace("/", "//", l_name);
		Util::replace("\\", "/", l_name);
		i = j + 1;
		string command = unescape(param.substr(i, param.length() - i));
		fly_fire5(ClientListener::HubUserCommand(), this, type, ctx, l_name, command);
	}
}

void NmdcHub::lockParse(const string& aLine)
{
	if (state != STATE_PROTOCOL || aLine.size() < 6)
	{
		return;
	}
	dcassert(m_users.empty());
	state = STATE_IDENTIFY;
	
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
			StringList feat;
			feat.reserve(13);
			feat.push_back("UserCommand");
			feat.push_back("NoGetINFO");
			feat.push_back("NoHello");
			feat.push_back("UserIP2");
			feat.push_back("TTHSearch");
			feat.push_back("ZPipe0");
#ifdef FLYLINKDC_USE_EXT_JSON
			feat.push_back("ExtJSON2");
#endif
			feat.push_back("HubURL");
			feat.push_back("NickRule");
			feat.push_back("SearchRule");
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
			// http://nmdc.sourceforge.net/NMDC.html#_hubtopic
			feat.push_back("HubTopic");
#endif
			feat.push_back("TTHS");
			if (CryptoManager::TLSOk())
			{
				feat.push_back("TLS");
			}
			supports(feat);
		}
		
		key(makeKeyFromLock(lock));
		
		string nick = getMyNick();
		const string randomTempNick = getRandomTempNick();
		if (!randomTempNick.empty())
		{
			nick = randomTempNick;
			setMyNick(randomTempNick);
		}
		
		OnlineUserPtr ou = getUser(nick);
		sendValidateNick(ou->getIdentity().getNick());		
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
			// [!] IRainman fix.
			if (isActive())
			{
				ou->getUser()->unsetFlag(User::NMDC_FILES_PASSIVE);
				ou->getUser()->unsetFlag(User::NMDC_SEARCH_PASSIVE);
			}
			else
			{
				ou->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
				ou->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
			}
			// [~] IRainman fix.
			
			if (state == STATE_IDENTIFY)
			{
				state = STATE_NORMAL;
				updateCounts(false);
				
				version();
				getNickList();
				myInfo(true);
			}
		}
	}
}

void NmdcHub::userIPParse(const string& p_ip_list)
{
	if (!p_ip_list.empty())
	{
		//OnlineUserList v;
		const StringTokenizer<string> t(p_ip_list, "$$", p_ip_list.size() / 30);
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
			
			// CFlyLock(cs); [-] IRainman fix.
			for (auto it = sl.cbegin(); it != sl.cend() && !ClientManager::isBeforeShutdown(); ++it)
			{
				string::size_type j = 0;
				if ((j = it->find(' ')) == string::npos)
					continue;
				if ((j + 1) == it->length())
					continue;
					
				const string l_ip = it->substr(j + 1);
				const string l_user = it->substr(0, j);
				if (l_user == getMyNick())
				{
					const bool l_is_private_ip = Util::isPrivateIp(l_ip);
					setTypeHub(l_is_private_ip);
					m_is_get_user_ip_from_hub = true;
					if (l_is_private_ip)
					{
						LogManager::message("Detect local hub: " + getHubUrl() + " private UserIP = " + l_ip + " User = " + l_user);
					}
				}
				OnlineUserPtr ou = findUser(l_user);
				
				if (!ou)
					continue;
					
				if (l_ip.size() > 15)
				{
					ou->getIdentity().setIP6(l_ip);
					ou->getIdentity().setUseIP6();
					ou->getIdentity().m_is_real_user_ip_from_hub = true;
				}
				else
				{
					dcassert(!l_ip.empty());
					ou->getIdentity().setIp(l_ip);
					ou->getIdentity().m_is_real_user_ip_from_hub = true;
					ou->getIdentity().getUser()->m_last_ip_sql.reset_dirty();
				}
				//v.push_back(ou);
			}
		}
		// TODO - слать сообщения о смене только IP
		// fire_user_updated(v);
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
	fire_user_updated(v);
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
			
			// CFlyLock(cs); [-] IRainman fix: no needs lock here!
			
			for (auto it = sl.cbegin(); it != sl.cend(); ++it)
			{
				if (it->empty())
					continue;
				OnlineUserPtr ou = getUser(*it);
				v.push_back(ou);
			}
			
			if (!(m_supportFlags & SUPPORTS_NOGETINFO))
			{
				string tmp;
				// Let's assume 10 characters per nick...
				tmp.reserve(v.size() * (11 + 10 + getMyNick().length()));
				string n = ' ' +  getMyNickFromUtf8() + '|';
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
		fire_user_updated(v);
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
			
			// CFlyLock(cs); [-] IRainman fix: no needs any lock here!
			
			for (auto it = sl.cbegin(); it != sl.cend(); ++it) // fix copy-paste
			{
				if (it->empty())
					continue;
					
				OnlineUserPtr ou = getUser(*it);
				if (ou)
				{
					ou->getUser()->setFlag(User::IS_OPERATOR);
					ou->getIdentity().setOp(true);
					v.push_back(ou);
				}
			}
		}
		fire_user_updated(v); // не убирать - через эту команду шлют "часы"
		updateCounts(false);
		
		// Special...to avoid op's complaining that their count is not correctly
		// updated when they log in (they'll be counted as registered first...)
		myInfo(false);
	}
}

void NmdcHub::getUserList(OnlineUserList& p_list) const
{
	CFlyReadLock(*m_cs);
	p_list.reserve(m_users.size());
	for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
	{
		p_list.push_back(i->second);
	}
}

#ifdef RIP_USE_CONNECTION_AUTODETECT
void NmdcHub::AutodetectComplete()
{
	m_bAutodetectionPending = false;
	m_iRequestCount = 0;
	setDetectActiveConnection();
	// send MyInfo, to update state on hub
	myInfo(true);
}
#endif // RIP_USE_CONNECTION_AUTODETECT

void NmdcHub::toParse(const string& param)
{
	if (isSupressChatAndPM())
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
	const auto l_user_for_message = findUser(rtNick);
	
	if (l_user_for_message == nullptr)
	{
#ifdef FLYLINKDC_BETA
		LogManager::speak_status_message("NmdcHub::toParse $To: invalid user: rtNick = " + rtNick + " param = " + param + " Hub = " + getHubUrl());
#endif
		// return;
	}
	
	pos_a = pos_b + 3;
	pos_b = param.find("> ", pos_a);
	
	if (pos_b == string::npos)
	{
		dcassert(0);
#ifdef FLYLINKDC_BETA
		LogManager::flood_message("NmdcHub::toParse pos_b == string::npos param = " + param + " Hub = " + getHubUrl());
#endif
		return;
	}
	
	const string fromNick = param.substr(pos_a, pos_b - pos_a);
	
	if (fromNick.empty())
	{
#ifdef FLYLINKDC_BETA
		LogManager::message("NmdcHub::toParse fromNick.empty() param = " + param + " Hub = " + getHubUrl());
#endif
		dcassert(0);
		return;
	}
	
	const string msgText = param.substr(pos_b + 2);
	
	if (msgText.empty())
	{
#ifdef FLYLINKDC_BETA
		LogManager::message("NmdcHub::toParse msgText.empty() param = " + param + " Hub = " + getHubUrl());
#endif
		//dcassert(0);
		return;
	}
	
	unique_ptr<ChatMessage> message(new ChatMessage(unescape(msgText), findUser(fromNick), nullptr, l_user_for_message));
	
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
	
	message->to = getMyOnlineUser(); // !SMT!-S [!] IRainman fix.
	
	// [+]IRainman fix: message from you to you is not allowed! Block magical spam.
	if (message->to->getUser() == message->from->getUser() && message->from->getUser() == message->replyTo->getUser())
	{
		fly_fire3(ClientListener::StatusMessage(), this, message->text, ClientListener::FLAG_IS_SPAM);
		LogManager::message("Magic spam message (from you to you) filtered on hub: " + getHubUrl() + ".");
		return;
	}
	if (!allowPrivateMessagefromUser(*message)) // [+] IRainman fix.
	{
		if (message->from && message->from->getUser())
		{
			logPM(message->from->getUser(), message->text, getHubUrl());
		}
		return;
	}
	
	fly_fire2(ClientListener::Message(), this, message); // [+]
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
	bool isSearch = false;
	bool isMyInfo = false;
	if (x == string::npos)
	{
		cmd = aLine.substr(1);
	}
	else
	{
		cmd = aLine.substr(1, x - 1);
		param = toUtf8(aLine.substr(x + 1));
		isSearch = cmd == "Search";
		if (isSearch && getHideShare())
		{
			return;
		}
	}
	if (!isSearch && isFloodCommand(cmd, param))
	{
		return;
	}
	if (isSearch)
	{
		if (!ClientManager::isStartup())
			searchParse(param);
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
		if (clientSock)
			clientSock->disconnect(false);
		fly_fire2(ClientListener::Redirect(), this, param);
	}
	else if (cmd == "HubIsFull")
	{
		fly_fire1(ClientListener::HubFull(), this);
	}
	else if (cmd == "ValidateDenide")        // Mind the spelling...
	{
		dcassert(clientSock);
		if (clientSock)
			clientSock->disconnect(false);
		fly_fire(ClientListener::NickTaken());
		//m_count_validate_denide++;
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
		getUser(getMyNick());
		setRegistered();
		// setMyIdentity(ou->getIdentity()); [-]
		processingPassword();
	}
	else if (cmd == "BadPass")
	{
		setPassword(Util::emptyString);
	}
	else if (cmd == "ZOn")
	{
		clientSock->setMode(BufferedSocket::MODE_ZPIPE);
	}
	else if (cmd == "HubTopic")
	{
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
		fly_fire2(ClientListener::HubTopic(), this, param);
#endif
	}
	// [+] IRainman.
	else if (cmd == "LogedIn")
	{
		messageYouAreOp();
	}
	// [~] IRainman.
	//else if (cmd == "myinfo")
	//{
	//}
	else if (cmd == "UserComman" || cmd == "myinfo")
	{
		// Где-то ошибка в плагине - много спама идет на сервер - отрубил нахрен
		const string l_message = "NmdcHub::onLine first unknown command! hub = [" + getHubUrl() + "], command = [" + cmd + "], param = [" + param + "]";
		LogManager::message(l_message);
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
		if (clientSock)
			clientSock->disconnect(false);
		if (m_nick_rule)
		{
			auto nick = getMyNick();
			m_nick_rule->convert_nick(nick);
			setMyNick(nick);
		}
		fly_fire(ClientListener::NickTaken());
		//m_count_validate_denide++;
	}
	else if (cmd == "SearchRule")
	{
		const StringTokenizer<string> l_nick_rule(param, "$$", 4);
		const StringList& sl = l_nick_rule.getTokens();
		for (auto it = sl.cbegin(); it != sl.cend(); ++it)
		{
			auto l_pos = it->find(' ');
			if (l_pos != string::npos && l_pos < it->size() + 1)
			{
				const string l_key = it->substr(0, l_pos);
				if (l_key == "Int")
				{
					auto l_int = Util::toInt(it->substr(l_pos + 1));
					if (l_int > 0)
					{
						setSearchInterval(l_int * 1000, true);
					}
				}
				if (l_key == "IntPas")
				{
					auto l_int = Util::toInt(it->substr(l_pos + 1));
					if (l_int > 0)
					{
						setSearchIntervalPassive(l_int * 1000, true);
					}
				}
			}
		}
	}
	else if (cmd == "NickRule")
	{
		m_nick_rule = std::unique_ptr<CFlyNickRule>(new CFlyNickRule);
		const StringTokenizer<string> l_nick_rule(param, "$$", 4);
		const StringList& sl = l_nick_rule.getTokens();
		for (auto it = sl.cbegin(); it != sl.cend(); ++it)
		{
			auto l_pos = it->find(' ');
			if (l_pos != string::npos && l_pos < it->size() + 1)
			{
				const string l_key = it->substr(0, l_pos);
				if (l_key == "Min")
				{
					unsigned l_nick_rule_min = Util::toInt(it->substr(l_pos + 1));
					if (l_nick_rule_min > 64)
					{
						LogManager::message("Error value NickRule Min = " + it->substr(l_pos + 1) +
						                          " replace: 64" + "Hub = " + getHubUrl());
						l_nick_rule_min = 64;
						disconnect(false);
					}
					m_nick_rule->m_nick_rule_min = l_nick_rule_min;
				}
				else if (l_key == "Max")
				{
					unsigned l_nick_rule_max = Util::toInt(it->substr(l_pos + 1));
					if (l_nick_rule_max > 200)
					{
						LogManager::message("Error value NickRule Max = " + it->substr(l_pos + 1) +
						                          " replace: 200" + "Hub = " + getHubUrl());
						l_nick_rule_max = 200;
						disconnect(false);
					}
					m_nick_rule->m_nick_rule_max = l_nick_rule_max;
				}
				else if (l_key == "Char")
				{
					const StringTokenizer<string> l_char(it->substr(l_pos + 1), " ");
					const StringList& l = l_char.getTokens();
					for (auto j = l.cbegin(); j != l.cend(); ++j)
					{
						if (!j->empty())
						{
							m_nick_rule->m_invalid_char.push_back(uint8_t(Util::toInt(*j)));
						}
					}
				}
				else if (l_key == "Pref")
				{
					const StringTokenizer<string> l_pref(it->substr(l_pos + 1), " ");
					const StringList& l = l_pref.getTokens();
					for (auto j = l.cbegin(); j != l.cend(); ++j)
					{
						if (!j->empty())
						{
							m_nick_rule->m_prefix.push_back(*j);
						}
					}
				}
			}
			else
			{
				dcassert(0);
			}
		}
		if (m_supportFlags & SUPPORTS_NICKRULE)
		{
			if (m_nick_rule)
			{
				string l_nick = getMyNick();
				const string l_fly_nick = getRandomTempNick();
				if (!l_fly_nick.empty())
				{
					l_nick = l_fly_nick;
				}
				m_nick_rule->convert_nick(l_nick);
				setMyNick(l_nick);
				
				// Тут пока не пашет.
				//OnlineUserPtr ou = getUser(l_nick, false, true);
				//sendValidateNick(ou->getIdentity().getNick());
			}
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
			CFlyFastLock(g_unknown_cs);
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
	processAutodetect(isMyInfo);
}

size_t NmdcHub::getMaxLenNick() const
{
	size_t l_max_len = 0;
	if (m_nick_rule)
	{
		l_max_len = m_nick_rule->m_nick_rule_max;
	}
	return l_max_len;
}

string NmdcHub::get_all_unknown_command()
{
	string l_message;
	CFlyFastLock(g_unknown_cs);
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
		CFlyFastLock(g_unknown_cs);
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

void NmdcHub::processAutodetect(bool isMyInfo)
{
	if (!isMyInfo && myInfoState == MYINFO_LIST)
	{
		if (m_is_get_user_ip_from_hub //|| !Util::isPrivateIp(getLocalIp())
		   )
		{
#ifdef RIP_USE_CONNECTION_AUTODETECT
			// This is first command after $MyInfo.
			// Do autodetection now, because at least VerliHub hag such a bug:
			// when hub sends $myInfo for each user on handshake sequence, it may skip
			// much connection requests (or may be also other commands), so it is better not to send
			// anything to it when receiving $myInfos is in progress
			RequestConnectionForAutodetect();
#endif
		}
		myInfoState = MYINFO_LIST_COMPLETED;
		userListLoaded = true;
	}
	if (isMyInfo && myInfoState == WAITING_FOR_MYINFO)
	{
		myInfoState = MYINFO_LIST;
	}
}

void NmdcHub::checkNick(string& aNick)
{
	for (size_t i = 0; i < aNick.size(); ++i)
	{
		if (static_cast<uint8_t>(aNick[i]) <= 32 || aNick[i] == '|' || aNick[i] == '$' || aNick[i] == '<' || aNick[i] == '>')
		{
			aNick[i] = '_';
		}
	}
}

void NmdcHub::connectToMe(const OnlineUser& aUser
#ifdef RIP_USE_CONNECTION_AUTODETECT
                          , ExpectedMap::DefinedExpectedReason reason /* = ExpectedMap::REASON_DEFAULT */
#endif
                         )
{
	checkstate();
	dcdebug("NmdcHub::connectToMe %s\n", aUser.getIdentity().getNick().c_str());
	const string nick = fromUtf8(aUser.getIdentity().getNick());
	ConnectionManager::getInstance()->nmdcExpect(nick, getMyNick(), getHubUrl()
#ifdef RIP_USE_CONNECTION_AUTODETECT
	                                             , reason
#endif
	                                            );
	ConnectionManager::g_ConnToMeCount++;
	
	const bool secure = CryptoManager::TLSOk() && aUser.getUser()->isSet(User::TLS);
	const uint16_t port = secure ? ConnectionManager::getInstance()->getSecurePort() : ConnectionManager::getInstance()->getPort();
	
	if (port == 0)
	{
		LogManager::message("Error [2] $ConnectToMe port = 0 :");
		dcassert(0);
	}
	else
	{
		// dcassert(isActive());
		send("$ConnectToMe " + nick + ' ' + getLocalIp() + ':' + Util::toString(port) + (secure ? "S|" : "|"));
	}
}

void NmdcHub::revConnectToMe(const OnlineUser& aUser)
{
	checkstate();
	dcdebug("NmdcHub::revConnectToMe %s\n", aUser.getIdentity().getNick().c_str());
	send("$RevConnectToMe " + getMyNickFromUtf8() + ' ' + fromUtf8(aUser.getIdentity().getNick()) + '|'); //[1] https://www.box.net/shared/f8330d2c54b2d7dcf3e4
}

void NmdcHub::hubMessage(const string& aMessage, bool thirdPerson)
{
	checkstate();
	send(fromUtf8Chat('<' + getMyNick() + "> " + escape(thirdPerson ? "/me " + aMessage : aMessage) + '|')); // IRAINMAN_USE_UNICODE_IN_NMDC
}

bool NmdcHub::resendMyINFO(bool alwaysSend, bool forcePassive)
{
	if (forcePassive)
	{
		if (m_modeChar == 'P')
			return false; // Уходим из обновления MyINFO - уже находимся в пассивном режиме
	}
	myInfo(alwaysSend, forcePassive);
	return true;
}

void NmdcHub::myInfo(bool alwaysSend, bool forcePassive)
{
	const uint64_t l_limit = 2 * 60 * 1000;
	const uint64_t l_currentTick = GET_TICK(); // [!] IRainman opt.
	if (!forcePassive && !alwaysSend && m_lastUpdate + l_limit > l_currentTick)
	{
		return; // antispam
	}
	checkstate();
	const FavoriteHubEntry *l_fhe = reloadSettings(false);
	char l_modeChar;
	if (forcePassive)
	{
		l_modeChar = 'P';
	}
	else
	{
		if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)
		{
			l_modeChar = '5';
		}
		else if (isActive())
		{
			l_modeChar = 'A';
		}
		else
		{
			l_modeChar = 'P';
		}
	}
	const int64_t upLimit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getUploadLimitInKBytes() : 0;// [!] IRainman SpeedLimiter
	const string uploadSpeed = (upLimit > 0) ? Util::toString(upLimit) + " KiB/s" : SETTING(UPLOAD_SPEED); // [!] IRainman SpeedLimiter
	
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
	const string l_currentCounts = l_fhe && l_fhe->getExclusiveHub() ? getCountsIndivid() : getCounts();
	
	// IRAINMAN_USE_UNICODE_IN_NMDC
	string l_currentMyInfo;
	l_currentMyInfo.resize(256);
	const string l_version = getClientName() + " V:" + getFullClientVersion();
	string l_ExtJSONSupport;
	if (m_supportFlags & SUPPORTS_EXTJSON2)
	{
		l_ExtJSONSupport = MappingManager::getPortmapInfo(false);
#if 0
		if (isFlySupportHub())
		{
			static string g_VID;
			static bool g_VID_check = false;
			static bool g_promo[3];
			if (g_VID_check == false)
			{
				g_promo[0] = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_autoAddSupportHub);
				g_promo[1] = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_autoAddFirstSupportHub);
				g_promo[2] = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_autoAdd1251SupportHub);
				g_VID_check = true;
				g_VID = Util::getRegistryCommaSubkey(_T("VID"));
			}
			if (g_promo[0])
			{
				l_ExtJSONSupport += "+Promo";
			}
			if (g_promo[1])
			{
				l_ExtJSONSupport += "+PromoF";
			}
			if (g_promo[2])
			{
				l_ExtJSONSupport += "+PromoL";
			}
			if (isDetectActiveConnection())
			{
				l_ExtJSONSupport += "+TCP(ok)";
			}
			if (!g_VID.empty()
			        && g_VID != "50000000"
			        && g_VID != "60000000"
			        && g_VID != "70000000"
			        && g_VID != "30000000"
			        && g_VID != "40000000"
			        && g_VID != "10000000"
			        && g_VID != "20000000"
			        && g_VID != "501"
			        && g_VID != "502"
			        && g_VID != "503"
			        && g_VID != "401"
			        && g_VID != "402"
			        && g_VID != "901"
			        && g_VID != "902")
			{
				l_ExtJSONSupport += "+VID:" + g_VID;
			}
		}
#endif
		if (CompatibilityManager::g_is_teredo)
		{
			l_ExtJSONSupport += "+Teredo";
		}
		if (CompatibilityManager::g_is_ipv6_enabled)
		{
			l_ExtJSONSupport += "+IPv6";
		}
#if 0
		l_ExtJSONSupport += "+Cache:"
		                    + Util::toString(CFlylinkDBManager::get_tth_cache_size()) + "/"
		                    + Util::toString(ShareManager::get_cache_size_file_not_exists_set()) + "/"
		                    + Util::toString(ShareManager::get_cache_file_map());
#endif
	}
	l_currentMyInfo.resize(_snprintf(&l_currentMyInfo[0], l_currentMyInfo.size() - 1, "$MyINFO $ALL %s %s<%s,M:%c,H:%s,S:%d"
	                                 ">$ $%s%c$%s$",
	                                 getMyNickFromUtf8().c_str(),
	                                 fromUtf8Chat(escape(getCurrentDescription())).c_str(),
	                                 l_version.c_str(), // [!] IRainman mimicry function.
	                                 l_modeChar,
	                                 l_currentCounts.c_str(),
	                                 UploadManager::getSlots(),
	                                 uploadSpeed.c_str(), status,
	                                 fromUtf8Chat(escape(getCurrentEmail())).c_str()));
	                                 
	const int64_t l_currentBytesShared =
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	    getHideShare() ? 0 :
#endif
	    ShareManager::getShareSize();
	    
#ifdef FLYLINKDC_BETA
	if (p_is_force_passive == true && l_currentBytesShared == m_lastBytesShared && !m_lastMyInfo.empty() && l_currentMyInfo == m_lastMyInfo)
	{
		dcassert(0);
		LogManager::message("Duplicate send MyINFO = " + l_currentMyInfo + " hub: " + getHubUrl());
	}
#endif
	const bool l_is_change_my_info = (l_currentBytesShared != m_lastBytesShared && m_lastUpdate + l_limit < l_currentTick) || l_currentMyInfo != m_lastMyInfo;
	const bool l_is_change_fly_info = g_version_fly_info != m_version_fly_info || m_lastExtJSONInfo.empty() || m_lastExtJSONSupport != l_ExtJSONSupport;
	if (alwaysSend || l_is_change_my_info || l_is_change_fly_info)
	{
		if (l_is_change_my_info)
		{
			m_lastMyInfo = l_currentMyInfo;
			m_lastBytesShared = l_currentBytesShared;
			send(m_lastMyInfo + Util::toString(l_currentBytesShared) + "$|");
			m_lastUpdate = l_currentTick;
		}
#ifdef FLYLINKDC_USE_EXT_JSON
		if ((m_supportFlags & SUPPORTS_EXTJSON2) && l_is_change_fly_info)
		{
			m_lastExtJSONSupport = l_ExtJSONSupport;
			m_version_fly_info = g_version_fly_info;
			Json::Value l_json_info;
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
			if (!SETTING(LOCATION_COUNTRY).empty())
				l_json_info["Country"] = SETTING(LOCATION_COUNTRY).substr(0, 30);
			if (!SETTING(LOCATION_CITY).empty())
				l_json_info["City"] = SETTING(LOCATION_CITY).substr(0, 30);
			if (!SETTING(LOCATION_ISP).empty())
				l_json_info["ISP"] = SETTING(LOCATION_ISP).substr(0, 30);
#endif
			l_json_info["Gender"] = SETTING(GENDER) + 1;
			if (!l_ExtJSONSupport.empty())
				l_json_info["Support"] = l_ExtJSONSupport;
			if (!getHideShare())
			{
				if (const auto l_count_files = ShareManager::getLastSharedFiles())
				{
					l_json_info["Files"] = l_count_files;
				}
			}
#if 0
			if (ShareManager::getLastSharedDate())
			{
				l_json_info["LastDate"] = ShareManager::getLastSharedDate();
			}
			extern int g_RAM_WorkingSetSize;
			if (g_RAM_WorkingSetSize)
			{
				static int g_LastRAM_WorkingSetSize;
				if (std::abs(g_LastRAM_WorkingSetSize - g_RAM_WorkingSetSize) > 10)
				{
					l_json_info["RAM"] = g_RAM_WorkingSetSize;
					g_LastRAM_WorkingSetSize = g_RAM_WorkingSetSize;
				}
			}
			if (CompatibilityManager::getFreePhysMemory())
			{
				l_json_info["RAMFree"] = CompatibilityManager::getFreePhysMemory() / 1024 / 1024;
			}
			if (g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI])
			{
				l_json_info["StartGUI"] = uint32_t(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI]);
			}
			if (g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE])
			{
				l_json_info["StartCore"] = uint32_t(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE]);
			}
			if (CFlylinkDBManager::getCountQueueFiles())
			{
				l_json_info["QueueFiles"] = CFlylinkDBManager::getCountQueueFiles();
			}
			if (CFlylinkDBManager::getCountQueueSources())
			{
				l_json_info["QueueSrc"] = CFlylinkDBManager::getCountQueueSources();
			}
			extern int g_RAM_PeakWorkingSetSize;
			if (g_RAM_PeakWorkingSetSize)
			{
				l_json_info["RAMPeak"] = g_RAM_PeakWorkingSetSize;
			}
			extern int64_t g_SQLiteDBSize;
			if (const int l_value = g_SQLiteDBSize / 1024 / 1024)
			{
				l_json_info["SQLSize"] = l_value;
			}
			extern int64_t g_SQLiteDBSizeFree;
			if (const int l_value = g_SQLiteDBSizeFree / 1024 / 1024)
			{
				l_json_info["SQLFree"] = l_value;
			}
			extern int64_t g_TTHLevelDBSize;
			if (const int l_value = g_TTHLevelDBSize / 1024 / 1024)
			{
				l_json_info["LDBHistSize"] = l_value;
			}
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
			extern int64_t g_IPCacheLevelDBSize;
			if (const int l_value = g_IPCacheLevelDBSize / 1024 / 1024)
			{
				l_json_info["LDBIPCacheSize"] = l_value;
			}
#endif
#endif			

			string l_json_str = l_json_info.toStyledString(false);
			
			boost::algorithm::trim(l_json_str); // TODO - убрать в конце пробел в json
			Text::removeString_rn(l_json_str);
			boost::replace_all(l_json_str, "$", "");
			boost::replace_all(l_json_str, "|", "");
			
			const string l_lastExtJSONInfo = "$ExtJSON " + getMyNickFromUtf8() + " " + escape(l_json_str);
			if (m_lastExtJSONInfo != l_lastExtJSONInfo)
			{
				m_lastExtJSONInfo = l_lastExtJSONInfo;
				send(m_lastExtJSONInfo + "|");
				m_lastUpdate = l_currentTick;
			}
		}
#endif // FLYLINKDC_USE_EXT_JSON
		m_modeChar = l_modeChar;
	}
}

void NmdcHub::searchToken(const SearchParamToken& sp)
{
	checkstate();
	
	int fileType = sp.fileType;
	if (fileType > FILE_TYPE_TTH)
		fileType = 0;
	extern bool g_DisableTestPort;
	bool passive = (sp.forcePassive || BOOLSETTING(SEARCH_PASSIVE)) && !g_DisableTestPort; // ???
	if (SearchManager::getSearchPortUint() == 0)
	{
		passive = true;
		LogManager::message("Error search port = 0 : ");
	}
	bool active = isActive();
	string cmd;
	if ((m_supportFlags & SUPPORTS_SEARCH_TTHS) == SUPPORTS_SEARCH_TTHS && sp.fileType == FILE_TYPE_TTH)
	{
		dcassert(sp.filter == TTHValue(sp.filter).toBase32());
		if (active && !passive)
			cmd = "$SA " + sp.filter + ' ' + calcExternalIP() + '|';
		else
			cmd = "$SP " + sp.filter + ' ' + getMyNickFromUtf8() + '|';
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
			c1 = (sp.sizeMode == Search::SIZE_DONTCARE || sp.sizeMode == Search::SIZE_EXACT) ? 'F' : 'T';
			c2 = sp.sizeMode == Search::SIZE_ATLEAST ? 'F' : 'T';
			query = fromUtf8(escape(sp.filter));
			std::replace(query.begin(), query.end(), ' ', '$');
		}
		cmd = "$Search ";
		if (active && !passive)
			cmd += calcExternalIP();
		else
			cmd += "Hub:" + getMyNickFromUtf8();
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

string NmdcHub::validateMessage(string tmp, bool reverse)
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

void NmdcHub::privateMessage(const string& nick, const string& message, bool thirdPerson)
{
	send("$To: " + fromUtf8(nick) + " From: " + getMyNickFromUtf8() + " $" + fromUtf8Chat(escape('<' + getMyNick() + "> " + (thirdPerson ? "/me " + message : message))) + '|'); // IRAINMAN_USE_UNICODE_IN_NMDC
}

void NmdcHub::privateMessage(const OnlineUserPtr& aUser, const string& aMessage, bool thirdPerson)   // !SMT!-S
{
	if (isSupressChatAndPM())
		return;
	checkstate();
	
	privateMessage(aUser->getIdentity().getNick(), aMessage, thirdPerson);
	// Emulate a returning message...
	// CFlyLock(cs); // !SMT!-fix: no data to lock
	
	// [!] IRainman fix.
	const OnlineUserPtr& me = getMyOnlineUser();
	// fly_fire1(ClientListener::PrivateMessage(), this, getMyNick(), me, aUser, me, '<' + getMyNick() + "> " + aMessage, thirdPerson); // !SMT!-S [-] IRainman fix.
	
	unique_ptr<ChatMessage> message(new ChatMessage(aMessage, me, aUser, me, thirdPerson));
	if (!allowPrivateMessagefromUser(*message))
		return;
		
	fly_fire2(ClientListener::Message(), this, message);
	// [~] IRainman fix.
}

void NmdcHub::sendUserCmd(const UserCommand& command, const StringMap& params)
{
	checkstate();
	string cmd = Util::formatParams(command.getCommand(), params, false);
	if (command.isChat())
	{
		if (command.getTo().empty())
		{
			hubMessage(cmd);
		}
		else
		{
			privateMessage(command.getTo(), cmd, false);
		}
	}
	else
	{
		send(fromUtf8(cmd));
	}
}
void NmdcHub::onConnected() noexcept
{
	Client::onConnected();
	
	if (state != STATE_PROTOCOL)
	{
		return;
	}
	m_version_fly_info = 0;
	m_modeChar = 0;
	m_supportFlags = 0;
	m_lastMyInfo.clear();
	m_lastExtJSONSupport.clear();
	m_lastBytesShared = 0;
	m_lastUpdate = 0;
	m_lastExtJSONInfo.clear();
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
	{
		CFlyWriteLock(*m_cs);
		dcassert(m_ext_json_deferred.empty());
		m_ext_json_deferred.clear();
	}
#endif
}

#ifdef FLYLINKDC_USE_EXT_JSON
bool NmdcHub::extJSONParse(const string& param, bool p_is_disable_fire /*= false */)
{
	string::size_type j = param.find(' ', 0);
	if (j == string::npos)
		return false;
	const string l_nick = param.substr(0, j);
	
	dcassert(!l_nick.empty())
	if (l_nick.empty())
	{
		dcassert(0);
		return false;
	}
	if (p_is_disable_fire == false)
	{
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
		CFlyWriteLock(*m_cs);
		if (m_ext_json_deferred.find(l_nick) == m_ext_json_deferred.end())
		{
			m_ext_json_deferred.insert(std::make_pair(l_nick, param));
			return false;
		}
#endif
	}
	
//#ifdef _DEBUG
//	string l_json_result = "{\"Gender\":1,\"RAM\":39,\"RAMFree\":1541,\"RAMPeak\":39,\"SQLFree\":35615,\"SQLSize\":19,\"StartCore\":4368,\"StartGUI\":1420,\"Support\":\"+Auto+UPnP(MiniUPnP)+Router+Public IP,TCP:55527(+)+IPv6\"}";
//#else
	const string l_json_result = unescape(param.substr(l_nick.size() + 1));
//#endif
	OnlineUserPtr ou = getUser(l_nick);
	try
	{
		Json::Value l_root;
		Json::Reader l_reader(Json::Features::strictMode());
		const bool l_parsingSuccessful = l_reader.parse(l_json_result, l_root);
		if (!l_parsingSuccessful && !l_json_result.empty())
		{
			dcassert(0);
			LogManager::message("Failed to parse ExtJSON:" + param);
			return false;
		}
		else
		{
			if (ou->getIdentity().setExtJSON(param))
			{
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
				ou->getIdentity().setStringParam("F1", l_root["Country"].asString());
				ou->getIdentity().setStringParam("F2", l_root["City"].asString());
				ou->getIdentity().setStringParam("F3", l_root["ISP"].asString());
#endif
				ou->getIdentity().setStringParam("F4", l_root["Gender"].asString());
				ou->getIdentity().setExtJSONSupportInfo(l_root["Support"].asString());
				ou->getIdentity().setExtJSONRAMWorkingSet(l_root["RAM"].asInt());
				ou->getIdentity().setExtJSONRAMPeakWorkingSet(l_root["RAMPeak"].asInt());
				ou->getIdentity().setExtJSONRAMFree(l_root["RAMFree"].asInt());
				//ou->getIdentity().setExtJSONGDI(l_root["GDI"].asInt());
				ou->getIdentity().setExtJSONCountFiles(l_root["Files"].asInt());
				ou->getIdentity().setExtJSONLastSharedDate(l_root["LastDate"].asInt64());
				ou->getIdentity().setExtJSONSQLiteDBSize(l_root["SQLSize"].asInt());
				ou->getIdentity().setExtJSONlevelDBHistSize(l_root["LDBHistSize"].asInt());
				ou->getIdentity().setExtJSONSQLiteDBSizeFree(l_root["SQLFree"].asInt());
				ou->getIdentity().setExtJSONQueueFiles(l_root["QueueFiles"].asInt());
				ou->getIdentity().setExtJSONQueueSrc(l_root["QueueSrc"].asInt64()); //TODO - временны баг - тут 32 бита
				ou->getIdentity().setExtJSONTimesStartCore(l_root["StartCore"].asInt64());  //TODO тут тоже 32 бита
				ou->getIdentity().setExtJSONTimesStartGUI(l_root["StartGUI"].asInt64()); //TODO тут тоже 32 бита
				
				if (p_is_disable_fire == false)
				{
					updatedMyINFO(ou); // TODO обновлять только JSON
				}
			}
		}
		return true;
	}
	catch (std::runtime_error& e)
	{
		LogManager::message("NmdcHub::extJSONParse error JSON =  " + l_json_result + " error = " + string(e.what()));
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
	const string l_nick = param.substr(i, j - i);
	
	dcassert(!l_nick.empty())
	if (l_nick.empty())
	{
		dcassert(0);
		return;
	}
	i = j + 1;
	
	OnlineUserPtr ou = getUser(l_nick);
	ou->getUser()->setFlag(User::IS_MYINFO);
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	string l_my_info_before_change;
	if (ou->m_raw_myinfo != param)
	{
		if (ou->m_raw_myinfo.empty())
		{
			// LogManager::message("[!!!!!!!!!!!] First MyINFO = " + param);
		}
		else
		{
			l_my_info_before_change = ou->m_raw_myinfo;
			// LogManager::message("[!!!!!!!!!!!] Change MyINFO New = " + param + " Old = " + ou->m_raw_myinfo);
		}
		ou->m_raw_myinfo = param;
	}
	else
	{
		//dcassert(0);
#ifdef _DEBUG
		LogManager::message("[!!!!!!!!!!!] Dup MyINFO = " + param + " hub = " + getHubUrl());
#endif
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	j = param.find('$', i);
	dcassert(j != string::npos)
	if (j == string::npos)
		return;
	bool l_is_only_desc_change = false;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	if (!l_my_info_before_change.empty())
	{
		const string::size_type l_pos_begin_tag = param.find('<', i);
		if (l_pos_begin_tag != string::npos)
		{
			const string::size_type l_pos_begin_tag_old = l_my_info_before_change.find('<', i);
			if (l_pos_begin_tag_old != string::npos)
			{
			
				if (strcmp(param.c_str() + l_pos_begin_tag, l_my_info_before_change.c_str() + l_pos_begin_tag_old) == 0)
				{
					l_is_only_desc_change = true;
#ifdef _DEBUG
					LogManager::message("[!!!!!!!!!!!] Only change Description New = " +
					                    param.substr(0, l_pos_begin_tag) + " old = " +
					                    l_my_info_before_change.substr(0, l_pos_begin_tag_old));
#endif
				}
			}
		}
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO 
	string tmpDesc = unescape(param.substr(i, j - i));
	// Look for a tag...
	if (!tmpDesc.empty() && tmpDesc[tmpDesc.size() - 1] == '>')
	{
		const string::size_type x = tmpDesc.rfind('<');
		if (x != string::npos)
		{
			// Hm, we have something...disassemble it...
			//dcassert(tmpDesc.length() > x + 2)
			if (tmpDesc.length() > x + 2 && l_is_only_desc_change == false)
			{
				const string l_tag = tmpDesc.substr(x + 1, tmpDesc.length() - x - 2);
				bool l_is_version_change = true;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_TAG
				if (ou->isTagUpdate(l_tag, l_is_version_change))
#endif
				{
					updateFromTag(ou->getIdentity(), l_tag, l_is_version_change); // тяжелая операция с токенами. TODO - оптимизнуть
					//if (!ou->m_tag_old.empty())
					//  ou->m_tag_old = l_tag;
				}
				//ou->m_tag_old = l_tag;
			}
			ou->getIdentity().setDescription(tmpDesc.erase(x));
		}
	}
	else
	{
		ou->getIdentity().setDescription(tmpDesc); //
		dcassert(param.size() > (j + 2)); //
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
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	if (l_is_only_desc_change && !ClientManager::isBeforeShutdown())
	{
		fly_fire1(ClientListener::UserDescUpdated(), ou);
		return;
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO 
	
	i = j + 3;
	j = param.find('$', i);
	if (j == string::npos)
		return;
		
	// [!] IRainman fix.
	if (i == j || j - i - 1 == 0)
	{
		// No connection = bot...
		ou->getIdentity().setBot();
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1]);
	}
	else
	{
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1], param.substr(i, j - i - 1));
	}
	// [~] IRainman fix.
	
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
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	// Проверим что меняетс только шара
	bool l_is_change_only_share = false;
	if (!l_my_info_before_change.empty())
	{
		if (i < l_my_info_before_change.size())
		{
			if (strcmp(param.c_str() + i, l_my_info_before_change.c_str() + i) != 0)
			{
				if (strncmp(param.c_str(), l_my_info_before_change.c_str(), i) == 0)
				{
					l_is_change_only_share = true;
#ifdef _DEBUG
					LogManager::message("[!!!!!!!!!!!] Only change Share New = " +
					                    param.substr(i) + " old = " +
					                    l_my_info_before_change.substr(i) + " l_nick = " + l_nick + " hub = " + getHubUrl());
#endif
				}
			}
		}
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	
	int64_t shareSize = Util::toInt64(param.c_str() + i);
	if (shareSize < 0) shareSize = 0;
	changeBytesSharedL(ou->getIdentity(), shareSize);

#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	if (l_is_change_only_share && !ClientManager::isBeforeShutdown())
	{
		fly_fire1(ClientListener::UserShareUpdated(), ou);
		return;
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO 
	
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
	string l_ext_json_param;
	{
		CFlyReadLock(*m_cs);
		const auto l_find_ext_json = m_ext_json_deferred.find(l_nick);
		if (l_find_ext_json != m_ext_json_deferred.end())
		{
			l_ext_json_param = l_find_ext_json->second;
		}
	}
	if (!l_ext_json_param.empty())
	{
		extJSONParse(l_ext_json_param, true); // true - не зовем ClientListener::UserUpdatedMyINFO
		{
			CFlyWriteLock(*m_cs);
			m_ext_json_deferred.erase(l_nick);
		}
	}
#endif // FLYLINKDC_USE_EXT_JSON
	updatedMyINFO(ou);
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
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		if (BOOLSETTING(NMDC_DEBUG))
		{
			fly_fire2(ClientListener::StatusMessage(), this, "<NMDC>" + toUtf8(aLine) + "</NMDC>");
		}
#endif
		onLine(aLine);
	}
}

void NmdcHub::onFailed(const string& aLine) noexcept
{
	clearUsers();
	Client::onFailed(aLine);
	updateCounts(true);
}

#ifdef RIP_USE_CONNECTION_AUTODETECT
void NmdcHub::RequestConnectionForAutodetect()
{
	const unsigned c_MAX_CONNECTION_REQUESTS_COUNT = 3;
	
	if (m_bAutodetectionPending && m_iRequestCount < c_MAX_CONNECTION_REQUESTS_COUNT)
	{
		bool bWantAutodetect = false;
		const auto l_fav = FavoriteManager::getFavoriteHubEntry(getHubUrl());
		const auto l_mode = ClientManager::getMode(l_fav, bWantAutodetect);
		//if (l_mode == SettingsManager::INCOMING_FIREWALL_PASSIVE ||
		//    l_mode == SettingsManager::INCOMING_DIRECT)
		{
			if (bWantAutodetect)
			{
			
				CFlyReadLock(*m_cs);
				for (auto i = m_users.cbegin(); i != m_users.cend() && m_iRequestCount < c_MAX_CONNECTION_REQUESTS_COUNT; ++i)
				{
					if (i->second->getIdentity().isBot() ||
					        i->second->getUser()->getFlags() & User::NMDC_FILES_PASSIVE ||
					        i->second->getUser()->getFlags() & User::NMDC_SEARCH_PASSIVE ||
					        i->first == getMyNick())
						continue;
					// TODO optimize:
					// request for connection from users with fastest connection, or operators
					connectToMe(*i->second, ExpectedMap::REASON_DETECT_CONNECTION);
#ifdef _DEBUG
					dcdebug("[!!!!!!!!!!!!!!] AutoDetect connectToMe! Nick = %s Hub = %s\r\n", i->first.c_str(), + getHubUrl().c_str());
					LogManager::message("AutoDetect connectToMe - Nick = " + i->first + " Hub = " + getHubUrl());
#endif
					++m_iRequestCount;
				}
			}
		}
	}
}
#endif // RIP_USE_CONNECTION_AUTODETECT

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
