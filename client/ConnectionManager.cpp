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
#include "TimerManager.h"
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CryptoManager.h"
#include "QueueManager.h"
#include "ShareManager.h"
#include "DebugManager.h"
#include "SSLSocket.h"
#include "AutoDetectSocket.h"
#include "PortTest.h"
#include "ConnectivityManager.h"
#include "LogManager.h"
#include "NmdcHub.h"

uint16_t ConnectionManager::g_ConnToMeCount = 0;
std::unique_ptr<RWLock> ConnectionManager::g_csConnection = std::unique_ptr<RWLock>(RWLock::create());
std::unique_ptr<RWLock> ConnectionManager::g_csDownloads = std::unique_ptr<RWLock>(RWLock::create());
//std::unique_ptr<RWLock> ConnectionManager::g_csUploads = std::unique_ptr<RWLock>(RWLock::create());
CriticalSection ConnectionManager::g_csUploads;
FastCriticalSection ConnectionManager::g_csDdosCheck;
std::unique_ptr<RWLock> ConnectionManager::g_csDdosCTM2HUBCheck = std::unique_ptr<RWLock>(RWLock::create());
std::unique_ptr<RWLock> ConnectionManager::g_csTTHFilter = std::unique_ptr<RWLock>(RWLock::create());
std::unique_ptr<RWLock> ConnectionManager::g_csFileFilter = std::unique_ptr<RWLock>(RWLock::create());

boost::unordered_set<UserConnection*> ConnectionManager::g_userConnections;
boost::unordered_map<string, ConnectionManager::CFlyTickTTH> ConnectionManager::g_duplicate_search_tth;
boost::unordered_map<string, ConnectionManager::CFlyTickFile> ConnectionManager::g_duplicate_search_file;
boost::unordered_set<string> ConnectionManager::g_ddos_ctm2hub;
std::map<ConnectionManager::CFlyDDOSkey, ConnectionManager::CFlyDDoSTick> ConnectionManager::g_ddos_map;
std::set<ConnectionQueueItemPtr> ConnectionManager::g_downloads; // TODO - сделать поиск по User?
std::set<ConnectionQueueItemPtr> ConnectionManager::g_uploads; // TODO - сделать поиск по User?

FastCriticalSection ConnectionManager::g_cs_update;
UserSet ConnectionManager::g_users_for_update;
bool ConnectionManager::g_shuttingDown = false;
TokenManager ConnectionManager::g_tokens_manager;

string TokenManager::makeToken() noexcept
{
	string token;
	LOCK(cs);
	do
	{
		token = Util::toString(Util::rand());
	}
	while (tokens.find(token) != tokens.end());
	tokens.insert(token);
#ifdef TOKEN_MANAGER_DEBUG
	LogManager::message("TokenManager::makeToken token = " + token);
#endif
	if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][TokenManager::makeToken] " + token);
	return token;
}

bool TokenManager::isToken(const string& token) const noexcept
{
	LOCK(cs);
	return tokens.find(token) != tokens.end();
}

bool TokenManager::addToken(const string& token) noexcept
{
	LOCK(cs);
	if (tokens.find(token) == tokens.end())
	{
		tokens.insert(token);
#ifdef TOKEN_MANAGER_DEBUG
		LogManager::message("TokenManager::addToken [+] token = " + token);
#endif
		return true;
	}
#ifdef TOKEN_MANAGER_DEBUG
	LogManager::message("TokenManager::addToken [-] token = " + token);
#endif
	return false;
}

size_t TokenManager::getTokenCount() const noexcept
{
	LOCK(cs);
	return tokens.size();
}

void TokenManager::removeToken(const string& token) noexcept
{
	LOCK(cs);
	auto p = tokens.find(token);
	if (p != tokens.end())
	{
#ifdef TOKEN_MANAGER_DEBUG
		LogManager::message("TokenManager::removeToken [+] token = " + token);
#endif
		tokens.erase(p);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][TokenManager::removeToken] " + token);
	}
	else
	{
#ifdef TOKEN_MANAGER_DEBUG
		LogManager::message("TokenManager::removeToken [-] token = " + token);
#endif
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][TokenManager::removeToken][empty] " + token);
	}
}

string TokenManager::toString() const noexcept
{
	LOCK(cs);
	string res;
	for (const string& s : tokens)
	{
		res += res.empty() ? "Tokens:\n" : ", ";
		res += s;
	}
	return res;
}

ConnectionManager::ConnectionManager() : m_floodCounter(0), server(nullptr), secureServer(nullptr)
{
	nmdcFeatures.reserve(5);
	nmdcFeatures.push_back(UserConnection::FEATURE_MINISLOTS);
	nmdcFeatures.push_back(UserConnection::FEATURE_XML_BZLIST);
	nmdcFeatures.push_back(UserConnection::FEATURE_ADCGET);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHL);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHF);
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
	nmdcFeatures.push_back(UserConnection::FEATURE_BANMSG); // !SMT!-B
#endif
	adcFeatures.reserve(4);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BAS0);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BASE);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_TIGR);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BZIP);
	
	TimerManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);
}

ConnectionManager::~ConnectionManager()
{
	dcassert(g_shuttingDown == true);
	dcassert(g_userConnections.empty());
	dcassert(g_downloads.empty());
	dcassert(g_uploads.empty());
}

void ConnectionManager::startListen()
{
	disconnect();
	string bind = SETTING(BIND_ADDRESS);
	bool autoDetectConnection = BOOLSETTING(AUTO_DETECT_CONNECTION);
	bool useTLS = CryptoManager::TLSOk();
	uint16_t portTCP = static_cast<uint16_t>(SETTING(TCP_PORT));
	uint16_t portTLS = useTLS ? static_cast<uint16_t>(SETTING(TLS_PORT)) : 0;
	int serverType = Server::TYPE_TCP;

	if (!useTLS)
	{
		LogManager::message("Skipping secure port: " + Util::toString(SETTING(USE_TLS)));
	}

	if (autoDetectConnection)
	{
		server = new Server(serverType, 0, Util::emptyString);
		SET_SETTING(TCP_PORT, server->getServerPort());
	}
	else
	{
		if (portTCP && portTLS == portTCP) serverType = Server::TYPE_AUTO_DETECT;
		server = new Server(serverType, portTCP, bind);
	}
	
	if (useTLS && serverType != Server::TYPE_AUTO_DETECT)
	{
		if (autoDetectConnection)
		{
			secureServer = new Server(Server::TYPE_SSL, 0, Util::emptyString);
			SET_SETTING(TLS_PORT, secureServer->getServerPort());
		}
		else
		{
			secureServer = new Server(Server::TYPE_SSL, portTLS, bind);
		}
	}
	fly_fire(ConnectionManagerListener::ListenerStarted());
}

/**
 * Request a connection for downloading.
 * DownloadManager::addConnection will be called as soon as the connection is ready
 * for downloading.
 * @param user The user to connect to.
 */
void ConnectionManager::getDownloadConnection(const UserPtr& user)
{
	dcassert(user);
	dcassert(!ClientManager::isBeforeShutdown());
	ConnectionQueueItemPtr cqi;
	if (!ClientManager::isBeforeShutdown())
	{
		{
			WRITE_LOCK(*g_csDownloads);
			const auto i = find(g_downloads.begin(), g_downloads.end(), user);
			if (i == g_downloads.end())
			{
				cqi = getCQI_L(HintedUser(user, Util::emptyString), true);
			}
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
			else
			{
				if (find(checkIdle.begin(), checkIdle.end(), user) == checkIdle.end())
				{
					checkIdle.push_back(user); // TODO - Ћок?
				}
			}
#endif
		}
		if (cqi && !ClientManager::isBeforeShutdown())
		{
			fly_fire3(ConnectionManagerListener::Added(), HintedUser(user, Util::emptyString), true, cqi->getConnectionQueueToken());
			return;
		}
#ifndef USING_IDLERS_IN_CONNECTION_MANAGER
		DownloadManager::checkIdle(user);
#endif
	}
}

ConnectionQueueItemPtr ConnectionManager::getCQI_L(const HintedUser& hintedUser, bool download)
{
	auto cqi = std::make_shared<ConnectionQueueItem>(hintedUser, download, g_tokens_manager.makeToken());
	if (download)
	{
		dcassert(find(g_downloads.begin(), g_downloads.end(), hintedUser) == g_downloads.end());
		g_downloads.insert(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][download] " + hintedUser.toString());
	}
	else
	{
		dcassert(find(g_uploads.begin(), g_uploads.end(), hintedUser) == g_uploads.end());
		g_uploads.insert(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][upload] " + hintedUser.toString());
	}
	return cqi;
}

void ConnectionManager::putCQI_L(ConnectionQueueItemPtr& cqi)
{
	if (cqi->isDownload())
	{
		g_downloads.erase(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][putCQI][download] " + cqi->getHintedUser().toString());
	}
	else
	{
		UploadManager::getInstance()->removeFinishedUpload(cqi->getUser());
		g_uploads.erase(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][putCQI][upload] " + cqi->getHintedUser().toString());
	}
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	// ¬ешаемс€ при активной закачке cqi->getUser()->flushRatio();
#endif
	QueueManager::g_userQueue.removeRunning(cqi->getUser());
	const string token = cqi->getConnectionQueueToken();
	cqi.reset();
	g_tokens_manager.removeToken(token);
	if (!ClientManager::isBeforeShutdown())
	{
		fly_fire1(ConnectionManagerListener::RemoveToken(), token);
	}
}

#if 0
bool ConnectionManager::getCipherNameAndIP(UserConnection* p_conn, string& p_chiper_name, string& p_ip)
{
	READ_LOCK(*g_csConnection);
	const auto l_conn = g_userConnections.find(p_conn);
	dcassert(l_conn != g_userConnections.end());
	if (l_conn != g_userConnections.end())
	{
		p_chiper_name = p_conn->getCipherName();
		p_ip = p_conn->getRemoteIp(); // TODO - перевести на boost?
		return true;
	}
	return false;
}
#endif

UserConnection* ConnectionManager::getConnection(bool nmdc, bool secure) noexcept
{
	dcassert(!g_shuttingDown);
	UserConnection* uc = new UserConnection;
	uc->addListener(this);
	{
		WRITE_LOCK(*g_csConnection);
		g_userConnections.insert(uc);
	}
	if (nmdc)
		uc->setFlag(UserConnection::FLAG_NMDC);
	if (secure)
		uc->setFlag(UserConnection::FLAG_SECURE);
	return uc;
}

void ConnectionManager::putConnection(UserConnection* conn)
{
	conn->removeListener(this);
	conn->disconnect(true);
	{
		WRITE_LOCK(*g_csConnection);
		auto i = g_userConnections.find(conn);
		if (i != g_userConnections.end())
		{
			UserConnection* uc = *i;
			if (uc->upload)
			{
				auto is = uc->upload->getReadStream();
				if (is) is->closeStream();
			}
			uc->state = UserConnection::STATE_UNUSED;
		}
	}
	if (CMD_DEBUG_ENABLED()) 
		DETECTION_DEBUG("[ConnectionManager][putConnection] " + conn->getHintedUser().toString());
}

void ConnectionManager::deleteConnection(UserConnection* conn)
{
	if (CMD_DEBUG_ENABLED()) 
		DETECTION_DEBUG("[ConnectionManager][deleteConnection] " + conn->getHintedUser().toString());
	conn->removeListener(this);
	{
		WRITE_LOCK(*g_csConnection);
		auto i = g_userConnections.find(conn);
		if (i != g_userConnections.end())
		{
			delete *i;
			g_userConnections.erase(i);
		}
	}
}

void ConnectionManager::flushUpdatedUsers()
{
	UserSet users;
	{
		LOCK(g_cs_update);
		users.swap(g_users_for_update);
	}
	for (const auto& user : users)
		onUserUpdated(user);
}

void ConnectionManager::addUpdatedUser(const UserPtr& user)
{
	LOCK(g_cs_update);
	g_users_for_update.insert(user);
}

void ConnectionManager::on(ClientManagerListener::UserConnected, const UserPtr& user) noexcept
{
	addUpdatedUser(user);
}

void ConnectionManager::on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept
{
	addUpdatedUser(user);
}

struct TokenItem
{
	HintedUser hintedUser;
	string token;
};

struct ReasonItem
{
	HintedUser hintedUser;
	string token;
	string reason;
};

void ConnectionManager::onUserUpdated(const UserPtr& user)
{
	if (!ClientManager::isBeforeShutdown())
	{
		std::vector<TokenItem> downloadUsers;
		std::vector<TokenItem> uploadUsers;
		{
			READ_LOCK(*g_csDownloads);
			for (const auto& download : g_downloads)
			{
				if (download->getUser() == user) // todo - map
					downloadUsers.emplace_back(TokenItem{download->getHintedUser(), download->getConnectionQueueToken()});
			}
		}
		{
			LOCK(g_csUploads);
			for (const auto& upload : g_uploads)
			{
				if (upload->getUser() == user)  // todo - map
					uploadUsers.emplace_back(TokenItem{upload->getHintedUser(), upload->getConnectionQueueToken()});
			}
		}
		for (const auto& download : downloadUsers)
		{
			fly_fire3(ConnectionManagerListener::UserUpdated(), download.hintedUser, true, download.token);
		}
		for (const auto& upload : uploadUsers)
		{
			fly_fire3(ConnectionManagerListener::UserUpdated(), upload.hintedUser, false, upload.token);
		}
	}
}

void ConnectionManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	updateAverageSpeed(tick);
#if 0
	if (((tick / 1000) % (CFlyServerConfig::g_max_unique_tth_search + 2)) == 0)
	{
		cleanupDuplicateSearchTTH(tick);
	}
	if (((tick / 1000) % (CFlyServerConfig::g_max_unique_file_search + 2)) == 0)
	{
		cleanupDuplicateSearchFile(tick);
	}
#endif
	flushUpdatedUsers();
	std::vector<ConnectionQueueItemPtr> removed;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	UserList idleList;
#endif
	std::vector<TokenItem> statusChanged;
	std::vector<ReasonItem> downloadError;
	{
		READ_LOCK(*g_csDownloads);
		uint16_t attempts = 0;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		idleList.swap(checkIdle);
#endif
		for (auto i = g_downloads.cbegin(); i != g_downloads.cend() && !ClientManager::isBeforeShutdown(); ++i)
		{
			const auto cqi = *i;
			if (cqi->getState() != ConnectionQueueItem::ACTIVE) // crash - https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=44111
			{
				if (!cqi->getUser()->isOnline())
				{
					// Not online anymore...remove it from the pending...
					removed.push_back(cqi);
					continue;
				}
				
				if (cqi->getErrors() == -1 && cqi->getLastAttempt() != 0)
				{
					// protocol error, don't reconnect except after a forced attempt
					continue;
				}

				QueueItem::Priority prio = QueueManager::hasDownload(cqi->getUser()); // [10] https://www.box.net/shared/i6hgw2qzhr9zyy15vhh1
				if (prio == QueueItem::PAUSED)
				{
					removed.push_back(cqi);
					continue;
				}

#ifdef _DEBUG
				const unsigned l_count_sec = 60;
				const unsigned l_count_sec_connecting = 50;
				//const unsigned l_count_sec = 10;
				//const unsigned l_count_sec_connecting = 5;
#else
				const unsigned l_count_sec = 60;
				const unsigned l_count_sec_connecting = 50;
#endif
				const auto l_count_error = cqi->getErrors();
				if (cqi->getLastAttempt() == 0 || ((SETTING(DOWNCONN_PER_SEC) == 0 || attempts < SETTING(DOWNCONN_PER_SEC)) &&
				                                   cqi->getLastAttempt() + l_count_sec * 1000 * max(1, l_count_error) < tick))
				{
					cqi->setLastAttempt(tick);
					
					const bool startDown = DownloadManager::isStartDownload(prio);
					
					if (cqi->getState() == ConnectionQueueItem::WAITING)
					{
						if (startDown)
						{
							bool unused;
							cqi->setState(ConnectionQueueItem::CONNECTING);
							ClientManager::getInstance()->connect(cqi->getHintedUser(),
							                                      cqi->getConnectionQueueToken(),
							                                      false, unused);
							statusChanged.emplace_back(TokenItem{cqi->getHintedUser(), cqi->getConnectionQueueToken()});
							attempts++;
						}
						else
						{
							cqi->setState(ConnectionQueueItem::NO_DOWNLOAD_SLOTS);
							downloadError.emplace_back(ReasonItem{cqi->getHintedUser(), cqi->getConnectionQueueToken(), STRING(ALL_DOWNLOAD_SLOTS_TAKEN)});
						}
					}
					else if (cqi->getState() == ConnectionQueueItem::NO_DOWNLOAD_SLOTS && startDown)
					{
						cqi->setState(ConnectionQueueItem::WAITING);
					}
				}
				else if (cqi->getState() == ConnectionQueueItem::CONNECTING && cqi->getLastAttempt() + l_count_sec_connecting * 1000 < tick)
				{
					ClientManager::connectionTimeout(cqi->getUser());
					cqi->setErrors(cqi->getErrors() + 1);
					downloadError.emplace_back(ReasonItem{cqi->getHintedUser(), cqi->getConnectionQueueToken(), STRING(CONNECTION_TIMEOUT)});
					cqi->setState(ConnectionQueueItem::WAITING);
				}
			}
		}
	}
	if (!removed.empty())
	{
		for (auto m = removed.begin(); m != removed.end(); ++m)
		{
			const bool isDownload = (*m)->isDownload();
			const auto hintedUser = (*m)->getHintedUser();
			const auto token = (*m)->getConnectionQueueToken();
			if (isDownload)
			{
				WRITE_LOCK(*g_csDownloads);
				putCQI_L(*m);
			}
			else
			{
				LOCK(g_csUploads);
				putCQI_L(*m);
			}
			if (!ClientManager::isBeforeShutdown())
			{
				fly_fire3(ConnectionManagerListener::Removed(), hintedUser, isDownload, token);
			}
		}
		removed.clear();
	}
	
	for (auto j = statusChanged.cbegin(); j != statusChanged.cend(); ++j)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire3(ConnectionManagerListener::ConnectionStatusChanged(), j->hintedUser, true, j->token);
		}
	}
	statusChanged.clear();
	// TODO - не звать дл€ тех у кого ошибка загрузки
	for (auto k = downloadError.cbegin(); k != downloadError.cend(); ++k)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire3(ConnectionManagerListener::FailedDownload(), k->hintedUser, k->reason, k->token);
		}
	}
	downloadError.clear();
	
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	for (auto i = idleList.cbegin(); i != idleList.cend(); ++i)
	{
		DownloadManager::checkIdle(*i);
	}
#endif
}

void ConnectionManager::cleanupIpFlood(const uint64_t p_tick)
{
#if 0
	LOCK(g_csDdosCheck);
	for (auto j = g_ddos_map.cbegin(); j != g_ddos_map.cend();)
	{
		// ≈сли коннектов совершено меньше чем предел в течении минуты - убираем адрес из таблицы - с ним все хорошо!
		const auto l_tick_delta = p_tick - j->second.m_first_tick;
		const bool l_is_min_ban_close = j->second.m_count_connect < CFlyServerConfig::g_max_ddos_connect_to_me && l_tick_delta > 1000 * 60;
		if (l_is_min_ban_close)
		{
#ifdef _DEBUG
			//LogManager::ddos_message("BlockID = " + Util::toString(j->second.m_block_id) + ", Removed mini-ban for: " +
			//                         j->first.first + j->second.getPorts() + ", Hub IP = " + j->first.second.to_string() +
			//                         " m_ddos_map.size() = " + Util::toString(m_ddos_map.size()));
#endif
		}
		// ≈сли коннектов совершено много и IP находитс€ в бане, но уже прошло врем€ больше чем 5 ћинут(по умолчанию)
		// “акже убираем запись из таблицы блокировки
		const bool l_is_ddos_ban_close = j->second.m_count_connect > CFlyServerConfig::g_max_ddos_connect_to_me
		                                 && l_tick_delta > CFlyServerConfig::g_ban_ddos_connect_to_me * 1000 * 60;
		if (l_is_ddos_ban_close && BOOLSETTING(LOG_DDOS_TRACE))
		{
			string l_type;
			if (j->first.m_ip.is_unspecified()) // ≈сли нет второго IP то это команада  ConnectToMe
			{
				l_type =  "IP-1:" + j->first.m_server + j->second.getPorts();
			}
			else
			{
				l_type = " IP-1:" + j->first.m_server + j->second.getPorts() + " IP-2: " + j->first.m_ip.to_string();
			}
			LogManager::ddos_message("BlockID = " + Util::toString(j->second.m_block_id) + ", Removed DDoS lock " + j->second.m_type_block +
			                         ", Count connect = " + Util::toString(j->second.m_count_connect) + " " + l_type +
			                         ", g_ddos_map.size() = " + Util::toString(g_ddos_map.size()));
		}
		if (l_is_ddos_ban_close || l_is_min_ban_close)
			g_ddos_map.erase(j++);
		else
			++j;
	}
#endif
}

void ConnectionManager::cleanupDuplicateSearchFile(const uint64_t p_tick)
{
#if 0
	WRITE_LOCK(*g_csFileFilter);
	for (auto j = g_duplicate_search_file.cbegin(); j != g_duplicate_search_file.cend();)
	{
		if ((p_tick - j->second.m_first_tick) > 1000 * CFlyServerConfig::g_max_unique_file_search)
		{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_FILE_SEARCH
			if (j->second.m_count_connect > 1) // —обытие возникало больше одного раза - логируем?
			{
				LogManager::ddos_message(string(j->second.m_count_connect, '*') + " BlockID = " + Util::toString(j->second.m_block_id) +
				                         ", Unlock duplicate File search: " + j->first +
				                         ", Count connect = " + Util::toString(j->second.m_count_connect) +
				                         ", Hash map size: " + Util::toString(g_duplicate_search_tth.size()));
			}
#endif
			g_duplicate_search_file.erase(j++);
		}
		else
			++j;
	}
#endif
}

void ConnectionManager::cleanupDuplicateSearchTTH(const uint64_t p_tick)
{
#if 0
	WRITE_LOCK(*g_csTTHFilter);
	for (auto j = g_duplicate_search_tth.cbegin(); j != g_duplicate_search_tth.cend();)
	{
		if ((p_tick - j->second.m_first_tick) > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_TTH_SEARCH
			if (j->second.m_count_connect > 1) // —обытие возникало больше одного раза - логируем?
			{
				LogManager::ddos_message(string(j->second.m_count_connect, '*') + " BlockID = " + Util::toString(j->second.m_block_id) +
				                         ", Unlock duplicate TTH search: " + j->first +
				                         ", Count connect = " + Util::toString(j->second.m_count_connect) +
				                         ", Hash map size: " + Util::toString(g_duplicate_search_tth.size()));
			}
#endif
			g_duplicate_search_tth.erase(j++);
		}
		else
			++j;
	}
#endif
}

void ConnectionManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	removeUnusedConnections();
	if (ClientManager::isBeforeShutdown())
		return;
	cleanupIpFlood(tick);
	READ_LOCK(*g_csConnection);
	for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
	{
		auto& connection = *j;
#ifdef _DEBUG
		if ((connection->getLastActivity() + 60 * 1000) < tick)
#else
		if ((connection->getLastActivity() + 60 * 1000) < tick) // «ачем так много минут висеть?
#endif
		{
			connection->disconnect(true);
		}
	}
}

static const uint64_t g_FLOOD_TRIGGER = 20000;
static const uint64_t g_FLOOD_ADD = 2000;

ConnectionManager::Server::Server(int type, uint16_t port, const string& ipAddr/* = "0.0.0.0" */): type(type), stopFlag(false)
{
	sock.create();
	sock.setSocketOpt(SO_REUSEADDR, 1);
	serverIp = ipAddr;
	LogManager::message("Starting to listen " + serverIp + ':' + Util::toString(port) + " type=" + Util::toString(type));
	serverPort = sock.bind(port, serverIp);
	sock.listen();
	start(64);
}

static const uint64_t POLL_TIMEOUT = 250;

int ConnectionManager::Server::run() noexcept
{
	while (!stopFlag)
	{
		try
		{
			while (!stopFlag)
			{
				auto ret = sock.wait(POLL_TIMEOUT, Socket::WAIT_READ);
				if (ret == Socket::WAIT_READ)
				{
					ConnectionManager::getInstance()->accept(sock, type, this);
				}
			}
		}
		catch (const Exception& e)
		{
			LogManager::message(STRING(LISTENER_FAILED) + ' ' + e.getError());
		}
		bool failed = false;
		while (!stopFlag)
		{
			try
			{
				sock.disconnect();
				sock.create();
				serverPort = sock.bind(serverPort, serverIp);
				dcassert(serverPort);
				LogManager::message("Starting to listen " + serverIp + ':' + Util::toString(serverPort) + " type=" + Util::toString(type));
				sock.listen();
				if (failed)
				{
					LogManager::message(STRING(CONNECTIVITY_RESTORED));
					failed = false;
				}
				break;
			}
			catch (const SocketException& e)
			{
				dcdebug("ConnectionManager::Server::run Stopped listening: %s\n", e.getError().c_str());
				
				if (!failed)
				{
					LogManager::message(STRING(CONNECTIVITY_ERROR) + ' ' + e.getError());
					failed = true;
				}
				
				// Spin for 60 seconds
				for (int i = 0; i < 60 && !stopFlag; ++i)
				{
					sleep(1000);
					LogManager::message("ConnectionManager::Server::run - sleep(1000)");
				}
			}
		}
	}
	return 0;
}

/**
 * Someone's connecting, accept the connection and wait for identification...
 * It's always the other fellow that starts sending if he made the connection.
 */
void ConnectionManager::accept(const Socket& sock, int type, Server* server) noexcept
{
	uint32_t now = GET_TICK();
	
	if (g_ConnToMeCount > 0)
		g_ConnToMeCount--;
		
	if (now > m_floodCounter)
	{
		m_floodCounter = now + g_FLOOD_ADD;
	}
	else
	{
		/*if (false  // TODO - узнать почему тут такой затыкон оставлен в оригинальном dc++
		        && now + g_FLOOD_TRIGGER < m_floodCounter)
		{
		    Socket s;
		    try
		    {
		        s.accept(sock);
		    }
		    catch (const SocketException&)
		    {
		        // ...
		    }
		    LogManager::flood_message("Connection flood detected, port = " + Util::toString(sock.getPort()) + " IP = " + sock.getIp());
		    dcdebug("Connection flood detected!\n");
		    return;
		}
		else
		*/
		{
			if (g_ConnToMeCount <= 0)
			{
				m_floodCounter += g_FLOOD_ADD;
			}
		}
	}
	uint16_t port;
	bool allowUntrusted = BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS);
	unique_ptr<Socket> newSock;
	switch (type)
	{
		case Server::TYPE_AUTO_DETECT:
			newSock.reset(new AutoDetectSocket);
			break;
		case Server::TYPE_SSL:
			newSock.reset(new SSLSocket(CryptoManager::SSL_SERVER, allowUntrusted, Util::emptyString));
			break;
		default:
			newSock.reset(new Socket);
	}
	try
	{
		port = newSock->accept(sock);
	}
	catch (const Exception&)
	{
		if (type == Server::TYPE_SSL && server)
		{
			// FIXME: Is it possible to get FlyLink's magic string from SSL socket buffer?
			if (g_portTest.processInfo(PortTest::PORT_TLS, PortTest::PORT_TLS, server->getServerPort(), Util::emptyString, Util::emptyString, false))
				ConnectivityManager::getInstance()->processPortTestResult();
		}
		return;
	}
	UserConnection* uc = getConnection(false, type == Server::TYPE_SSL);
	uc->setFlag(UserConnection::FLAG_INCOMING);
	uc->setState(UserConnection::STATE_SUPNICK);
	uc->updateLastActivity();
	uc->addAcceptedSocket(newSock, port);
}

bool ConnectionManager::checkDuplicateSearchFile(const string& p_search_command)
{
#if 0
	WRITE_LOCK(*g_csFileFilter);
	const auto l_tick = GET_TICK();
	CFlyTickFile l_item;
	l_item.m_first_tick = l_tick;
	l_item.m_last_tick = l_tick;
	const auto l_key_pos = p_search_command.rfind(' ');
	if (l_key_pos != string::npos && l_key_pos)
	{
		const string l_key = p_search_command.substr(0, l_key_pos);
		auto l_result = g_duplicate_search_file.insert(std::pair<string, CFlyTickFile>(l_key, l_item));
		auto& l_cur_value = l_result.first->second;
		++l_cur_value.m_count_connect;
		if (l_result.second == false) // Ёлемент уже существует - проверим его счетчик и старость.
		{
			l_cur_value.m_last_tick = l_tick;
			if (l_tick - l_cur_value.m_first_tick > 1000 * CFlyServerConfig::g_max_unique_file_search)
			{
				// “ут можно сразу стереть элемент устаревший
				return false;
			}
			if (l_cur_value.m_count_connect > 1)
			{
				static uint16_t g_block_id = 0;
				if (l_cur_value.m_block_id == 0)
				{
					l_cur_value.m_block_id = ++g_block_id;
				}
				if (l_cur_value.m_count_connect >= 2)
				{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_FILE_SEARCH
					LogManager::ddos_message(string(l_cur_value.m_count_connect, '*') + " BlockID = " + Util::toString(l_cur_value.m_block_id) +
					                         ", Lock File search = " + p_search_command +
					                         ", Count = " + Util::toString(l_cur_value.m_count_connect) +
					                         ", Hash map size: " + Util::toString(g_duplicate_search_file.size()));
#endif
				}
				return true;
			}
		}
	}
#endif
	return false;
}

bool ConnectionManager::checkDuplicateSearchTTH(const string& p_search_command, const TTHValue& p_tth)
{
#if 0
	WRITE_LOCK(*g_csTTHFilter);
	const auto l_tick = GET_TICK();
	CFlyTickTTH l_item;
	l_item.m_first_tick = l_tick;
	l_item.m_last_tick  = l_tick;
	auto l_result = g_duplicate_search_tth.insert(std::pair<string, CFlyTickTTH>(p_search_command + ' ' + p_tth.toBase32(), l_item));
	auto& l_cur_value = l_result.first->second;
	++l_cur_value.m_count_connect;
	if (l_result.second == false) // Ёлемент уже существует - проверим его счетчик и старость.
	{
		l_cur_value.m_last_tick  = l_tick;
		if (l_tick - l_cur_value.m_first_tick > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
			// “ут можно сразу стереть элемент устаревший
			return false;
		}
		if (l_cur_value.m_count_connect > 1)
		{
			static uint16_t g_block_id = 0;
			if (l_cur_value.m_block_id == 0)
			{
				l_cur_value.m_block_id = ++g_block_id;
			}
			if (l_cur_value.m_count_connect >= 2)
			{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_TTH_SEARCH
				LogManager::ddos_message(string(l_cur_value.m_count_connect, '*') + " BlockID = " + Util::toString(l_cur_value.m_block_id) +
				                         ", Lock TTH search = " + p_search_command +
				                         ", TTH = " + p_tth.toBase32() +
				                         ", Count = " + Util::toString(l_cur_value.m_count_connect) +
				                         ", Hash map size: " + Util::toString(g_duplicate_search_tth.size()));
#endif
			}
			return true;
		}
	}
#endif
	return false;
}

void ConnectionManager::addCTM2HUB(const string& serverAddr, const HintedUser& hintedUser)
{
	const string cmt2hub = "[" + Util::formatCurrentDate() + "] CTM2HUB = " + serverAddr + " <<= DDoS block from: " + hintedUser.hint;
	bool isDuplicate;
	{
		WRITE_LOCK(*g_csDdosCTM2HUBCheck);
		//dcassert(hintedUser.user);
		isDuplicate = g_ddos_ctm2hub.insert(Text::toLower(serverAddr)).second;
	}
	LogManager::message(cmt2hub);
}

bool ConnectionManager::checkIpFlood(const string& aIPServer, uint16_t aPort, const boost::asio::ip::address_v4& p_ip_hub, const string& p_userInfo, const string& p_HubInfo)
{
#if 0
	if (CFlyServerConfig::isGuardTCPPort(aPort))
	{
		const string l_guard_port = "[" + Util::formatCurrentDate() + "] [TCP PortGuard] Block DDoS: " + aIPServer + ':' + Util::toString(aPort) + " HubInfo: " + p_HubInfo + " UserInfo: " + p_userInfo;
		LogManager::ddos_message(l_guard_port);
		/*
		static int g_is_first = 0;
		if (++g_is_first < 10)
		{
		    CFlyServerJSON::pushError(61, l_guard_port);
		}
		*/
		return true;
	}
	{
		const auto l_tick = GET_TICK();
#ifdef _DEBUG
		const string l_server_lower = Text::toLowerFast(aIPServer);
		dcassert(l_server_lower == aIPServer);
#endif
		// boost::system::error_code ec;
		// const auto l_ip = boost::asio::ip::address_v4::from_string(aIPServer, ec);
		const CFlyDDOSkey l_key(aIPServer, p_ip_hub);
		// dcassert(!ec); // TODO - тут бывает и Host
		bool l_is_ctm2hub = false;
		{
			READ_LOCK(*g_csDdosCTM2HUBCheck);
			l_is_ctm2hub = !g_ddos_ctm2hub.empty() && g_ddos_ctm2hub.find(aIPServer + ':' + Util::toString(aPort)) != g_ddos_ctm2hub.end();
		}
		if (l_is_ctm2hub)
		{
			const string l_cmt2hub = "Block CTM2HUB = " + aIPServer + ':' + Util::toString(aPort) + " HubInfo: " + p_HubInfo + " UserInfo: " + p_userInfo;
#ifdef FLYLINKDC_BETA
			// LogManager::message(l_cmt2hub);
#endif
			LogManager::ddos_message(l_cmt2hub);
			return true;
		}
		CFlyDDoSTick l_item;
		l_item.m_first_tick = l_tick;
		l_item.m_last_tick = l_tick;
		LOCK(g_csDdosCheck);
		auto l_result = g_ddos_map.insert(std::pair<CFlyDDOSkey, CFlyDDoSTick>(l_key, l_item));
		auto& l_cur_value = l_result.first->second;
		++l_cur_value.m_count_connect;
		string l_debug_key;
		if (BOOLSETTING(LOG_DDOS_TRACE))
		{
			l_debug_key = " Time: " + Util::getShortTimeString() + " Hub info = " + p_HubInfo; // https://drdump.com/Problem.aspx?ClientID=guest&ProblemID=92733
			if (!p_userInfo.empty())
			{
				l_debug_key + " UserInfo = [" + p_userInfo + "]";
			}
			l_cur_value.m_original_query_for_debug[l_debug_key]++;
		}
		if (l_result.second == false)
		{
			// Ёлемент уже существует
			l_cur_value.m_last_tick = l_tick;   //  орректируем врем€ последней активности.
			l_cur_value.m_ports.insert(aPort);  // —охраним последний порт
			if (l_cur_value.m_count_connect == CFlyServerConfig::g_max_ddos_connect_to_me) // ѕревысили кол-во коннектов по одному IP
			{
				static uint16_t g_block_id = 0;
				l_cur_value.m_block_id = ++g_block_id;
				if (BOOLSETTING(LOG_DDOS_TRACE))
				{
					const string l_info   = "[Count limit: " + Util::toString(CFlyServerConfig::g_max_ddos_connect_to_me) + "]\t";
					const string l_target = "[Target: " + aIPServer + l_cur_value.getPorts() + "]\t";
					const string l_user_info = !p_userInfo.empty() ? "[UserInfo: " + p_userInfo + "]\t"  : "";
					l_cur_value.m_type_block = "Type DDoS:" + std::string(p_ip_hub.is_unspecified() ? "[$ConnectToMe]" : "[$Search]");
					LogManager::ddos_message("BlockID=" + Util::toString(l_cur_value.m_block_id) + ", " + l_cur_value.m_type_block + p_HubInfo + l_info + l_target + l_user_info);
					for (auto k = l_cur_value.m_original_query_for_debug.cbegin() ; k != l_cur_value.m_original_query_for_debug.cend(); ++k)
					{
						LogManager::ddos_message("  Detail BlockID=" + Util::toString(l_cur_value.m_block_id) + " " + k->first + " Count:" + Util::toString(k->second)); // TODO - сдать дубликаты + показать кол-во
					}
				}
				l_cur_value.m_original_query_for_debug.clear();
			}
			if (l_cur_value.m_count_connect >= CFlyServerConfig::g_max_ddos_connect_to_me)
			{
				if ((l_cur_value.m_last_tick - l_cur_value.m_first_tick) < CFlyServerConfig::g_ban_ddos_connect_to_me * 1000 * 60)
				{
					return true; // Ћочим этот коннект до наступлени€ амнистии. TODO - проверить эту часть внимательей
					// в след части фикса - проводить анализ протокола и коннекты на порты лочить на вечно.
				}
			}
		}
	}
	{
		READ_LOCK(*g_csConnection);
		// We don't want to be used as a flooding instrument
		int count = 0;
		for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
		{
			const UserConnection& uc = **j;
			if (uc.socket == nullptr || !uc.socket->hasSocket())
				continue;
			if (uc.getPort() == aPort && uc.getRemoteIp() == aIPServer) // TODO - не поддерживаетс€ DNS
			{
				if (++count >= 5)
				{
					// More than 5 outbound connections to the same addr/port? Can't trust that..
					// LogManager::message("ConnectionManager::connect Tried to connect more than 5 times to " + aIPServer + ":" + Util::toString(aPort));
					dcdebug("ConnectionManager::connect Tried to connect more than 5 times to %s:%hu, connect dropped\n", aIPServer.c_str(), aPort);
					return true;
				}
			}
		}
	}
#endif
	return false;
}

void ConnectionManager::nmdcConnect(const string& address, uint16_t port, const string& myNick, const string& hubUrl, int encoding, bool secure)
{
	nmdcConnect(address, port, 0, BufferedSocket::NAT_NONE, myNick, hubUrl, encoding, secure);
}

void ConnectionManager::nmdcConnect(const string& address, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& myNick, const string& hubUrl, int encoding, bool secure)
{
	if (isShuttingDown())
	{
		dcassert(0);
		return;
	}
	if (checkIpFlood(address, port, boost::asio::ip::address_v4(), "", "[nmdcConnect][Hub: " + hubUrl + "]"))
	{
		//dcassert(0);
		return;
	}
	
	UserConnection* uc = getConnection(true, secure);
	uc->setServerPort(address + ':' + Util::toString(port));
	uc->setUserConnectionToken(myNick);
	uc->setHubUrl(hubUrl);
	uc->setEncoding(encoding);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setFlag(UserConnection::FLAG_NMDC);
	try
	{
		uc->connect(address, port, localPort, natRole);
	}
	catch (const Exception&)
	{
		deleteConnection(uc);
	}
}

void ConnectionManager::adcConnect(const OnlineUser& user, uint16_t port, const string& token, bool secure)
{
	adcConnect(user, port, 0, BufferedSocket::NAT_NONE, token, secure);
}

void ConnectionManager::adcConnect(const OnlineUser& user, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& token, bool secure)
{
	if (isShuttingDown())
		return;
		
	if (checkIpFlood(user.getIdentity().getIpAsString(), port, boost::asio::ip::address_v4(), "", "[adcConnect][Hub: " + user.getClientBase().getHubName() + "]")) // "ADC Nick: " + user.getIdentity().getNick() +
		return;
		
	UserConnection* uc = getConnection(false, secure);
	uc->setUserConnectionToken(token);
	uc->setEncoding(Text::CHARSET_UTF8);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setHubUrl(&user.getClient() == nullptr ? "DHT" : user.getClient().getHubUrl());
	try
	{
		uc->connect(user.getIdentity().getIpAsString(), port, localPort, natRole);
	}
	catch (const Exception&)
	{
		deleteConnection(uc);
	}
}

void ConnectionManager::disconnect()
{
	delete server;
	server = nullptr;
	
	delete secureServer;
	secureServer = nullptr;
}

void ConnectionManager::on(AdcCommand::SUP, UserConnection* source, const AdcCommand& cmd) noexcept
{
	if (source->getState() != UserConnection::STATE_SUPNICK)
	{
		// Already got this once, ignore...@todo fix support updates
		dcdebug("CM::onSUP %p sent sup twice\n", (void*)source);
		return;
	}
	
	bool baseOk = false;
	
	for (auto i = cmd.getParameters().cbegin(); i != cmd.getParameters().cend(); ++i)
	{
		if (i->compare(0, 2, "AD", 2) == 0)
		{
			bool tigrOk = false;
			string feat = i->substr(2);
			if (feat == UserConnection::FEATURE_ADC_BASE || feat == UserConnection::FEATURE_ADC_BAS0)
			{
				baseOk = true;
				// For bas0 tiger is implicit
				if (feat == UserConnection::FEATURE_ADC_BAS0)
					tigrOk = true;
				// ADC clients must support all these...
				source->setFlag(
					UserConnection::FLAG_SUPPORTS_ADCGET |
					UserConnection::FLAG_SUPPORTS_MINISLOTS |
					UserConnection::FLAG_SUPPORTS_TTHF |
					UserConnection::FLAG_SUPPORTS_TTHL |
					UserConnection::FLAG_SUPPORTS_XML_BZLIST // For compatibility with older clients...
				);
			}
			else if (feat == UserConnection::FEATURE_ZLIB_GET)
			{
				source->setFlag(UserConnection::FLAG_SUPPORTS_ZLIB_GET);
			}
			else if (feat == UserConnection::FEATURE_ADC_BZIP)
			{
				source->setFlag(UserConnection::FLAG_SUPPORTS_XML_BZLIST);
			}
			else if (feat == UserConnection::FEATURE_ADC_TIGR)
			{
				tigrOk = true; // Variable 'tigrOk' is assigned a value that is never used.
			}
		}
	}
	
	if (!baseOk)
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Invalid SUP"));
		source->disconnect();
		return;
	}
	
	if (source->isSet(UserConnection::FLAG_INCOMING))
	{
		StringList defFeatures = adcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back("AD" + UserConnection::FEATURE_ZLIB_GET);
		}
		source->sup(defFeatures);
	}
	else
	{
		source->inf(true);
	}
	source->setState(UserConnection::STATE_INF);
}

void ConnectionManager::on(AdcCommand::STA, UserConnection*, const AdcCommand& /*cmd*/) noexcept
{

}

void ConnectionManager::on(UserConnectionListener::Connected, UserConnection* source) noexcept
{
	if (source->isSecure() && !source->isTrusted() && !BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS))
	{
		putConnection(source);
		LogManager::message(STRING(CERTIFICATE_NOT_TRUSTED));
		return;
	}
	dcassert(source->getState() == UserConnection::STATE_CONNECT);
	if (source->isSet(UserConnection::FLAG_NMDC))
	{
		source->myNick(source->getUserConnectionToken());
		source->lock(NmdcHub::getLock(), NmdcHub::getPk() + "Ref=" + source->getHubUrl());
	}
	else
	{
		StringList defFeatures = adcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back("AD" + UserConnection::FEATURE_ZLIB_GET);
		}
		source->sup(defFeatures);
		source->send(AdcCommand(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, Util::emptyString).addParam("RF", source->getHubUrl()));
	}
	source->setState(UserConnection::STATE_SUPNICK);
}

void ConnectionManager::on(UserConnectionListener::MyNick, UserConnection* source, const string& nick) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (source->getState() != UserConnection::STATE_SUPNICK)
	{
		// Already got this once, ignore...
		dcdebug("CM::onMyNick %p sent nick twice\n", (void*)source);
		//LogManager::message("CM::onMyNick "+ nick + " sent nick twice");
		return;
	}
	
	dcassert(!nick.empty());
	dcdebug("ConnectionManager::onMyNick %p, %s\n", (void*)source, nick.c_str());
	dcassert(!source->getUser());
	
	if (source->isSet(UserConnection::FLAG_INCOMING))
	{
		// Try to guess where this came from...
		const auto i = expectedConnections.remove(nick);
		
		if (i.hubUrl.empty())
		{
			dcassert(i.nick.empty());
			LogManager::message("Unknown incoming connection from \"" + nick + '"', false);
			putConnection(source);
			return;
		}
		
		source->setUserConnectionToken(i.nick);
		source->setHubUrl(i.hubUrl);
		const auto encoding = ClientManager::findHubEncoding(i.hubUrl);
		source->setEncoding(encoding);
	}
	
	const string nickUtf8 = Text::toUtf8(nick, source->getEncoding());
	const CID cid = ClientManager::makeCid(nickUtf8, source->getHubUrl());
	
	// First, we try looking in the pending downloads...hopefully it's one of them...
	if (!ClientManager::isBeforeShutdown())
	{
		READ_LOCK(*g_csDownloads);
		for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
		{
			const ConnectionQueueItemPtr& cqi = *i;
			cqi->setErrors(0);
			if ((cqi->getState() == ConnectionQueueItem::CONNECTING || cqi->getState() == ConnectionQueueItem::WAITING) &&
			    cqi->getUser()->getCID() == cid)
			{
				source->setUser(cqi->getUser());
				// Indicate that we're interested in this file...
				source->setFlag(UserConnection::FLAG_DOWNLOAD);
				break;
			}
		}
	}
	
	if (!source->getUser())
	{
		// Make sure we know who it is, i e that he/she is connected...
		
		source->setUser(ClientManager::findUser(cid));
		if (!source->getUser() || !source->getUser()->isOnline())
		{
			LogManager::message("Incoming connection from unknown user \"" + nickUtf8 + '"', false);
			putConnection(source);
			return;
		}
		// We don't need this connection for downloading...make it an upload connection instead...
		source->setFlag(UserConnection::FLAG_UPLOAD);
	}
	
	ClientManager::setUserIP(source->getUser(), source->getRemoteIp());
	
	if (source->isSet(UserConnection::FLAG_INCOMING))
	{
		source->myNick(source->getUserConnectionToken());
		source->lock(NmdcHub::getLock(), NmdcHub::getPk());
	}
	
	source->setState(UserConnection::STATE_LOCK);
}

void ConnectionManager::on(UserConnectionListener::CLock, UserConnection* source, const string& lock) noexcept
{
	if (source->getState() != UserConnection::STATE_LOCK)
	{
		dcdebug("CM::onLock %p received lock twice, ignoring\n", (void*)source);
		return;
	}
	
	if (NmdcHub::isExtended(lock))
	{
		StringList defFeatures = nmdcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back(UserConnection::FEATURE_ZLIB_GET);
		}
		source->supports(defFeatures);
	}
	
	source->setState(UserConnection::STATE_DIRECTION);
	source->direction(source->getDirectionString(), source->getNumber());
	source->key(NmdcHub::makeKeyFromLock(lock));
}

void ConnectionManager::on(UserConnectionListener::Direction, UserConnection* source, const string& dir, const string& num) noexcept
{
	if (source->getState() != UserConnection::STATE_DIRECTION)
	{
		dcdebug("CM::onDirection %p received direction twice, ignoring\n", (void*)source);
		return;
	}
	
	dcassert(source->isSet(UserConnection::FLAG_DOWNLOAD) ^ source->isSet(UserConnection::FLAG_UPLOAD));
	if (dir == "Upload")
	{
		// Fine, the other fellow want's to send us data...make sure we really want that...
		if (source->isSet(UserConnection::FLAG_UPLOAD))
		{
			// Huh? Strange...disconnect...
			putConnection(source);
			return;
		}
	}
	else
	{
		if (source->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			int number = Util::toInt(num);
			// Damn, both want to download...the one with the highest number wins...
			if (source->getNumber() < number)
			{
				// Damn! We lost!
				source->unsetFlag(UserConnection::FLAG_DOWNLOAD);
				source->setFlag(UserConnection::FLAG_UPLOAD);
			}
			else if (source->getNumber() == number)
			{
				putConnection(source);
				return;
			}
		}
	}
	
	dcassert(source->isSet(UserConnection::FLAG_DOWNLOAD) ^ source->isSet(UserConnection::FLAG_UPLOAD));
	source->setState(UserConnection::STATE_KEY);
}

void ConnectionManager::setIP(UserConnection* conn, const ConnectionQueueItemPtr& qi)
{
	dcassert(conn);
	dcassert(conn->getUser());
	dcassert(qi);
	conn->getUser()->setIP(conn->getSocket()->getIp());
}

void ConnectionManager::addDownloadConnection(UserConnection* conn)
{
	dcassert(conn->isSet(UserConnection::FLAG_DOWNLOAD));
	ConnectionQueueItemPtr cqi;
	bool isActive = false;
	{
		READ_LOCK(*g_csDownloads);
		const auto i = find(g_downloads.begin(), g_downloads.end(), conn->getUser());
		if (i != g_downloads.end())
		{
			cqi = *i;
			isActive = true;
			conn->setConnectionQueueToken(cqi->getConnectionQueueToken());
			if (cqi->getState() == ConnectionQueueItem::WAITING || cqi->getState() == ConnectionQueueItem::CONNECTING)
			{
				cqi->setState(ConnectionQueueItem::ACTIVE);
				conn->setFlag(UserConnection::FLAG_ASSOCIATED);
				
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
				fly_fire1(ConnectionManagerListener::Connected(), cqi);
#endif
				dcdebug("ConnectionManager::addDownloadConnection, leaving to downloadmanager\n");
			}
			else
			{
				isActive = false;
			}
		}
	}
	
	if (isActive)
	{
		DownloadManager::getInstance()->addConnection(conn);
		setIP(conn, cqi);
	}
	else
	{
		putConnection(conn);
	}
}

void ConnectionManager::addUploadConnection(UserConnection* conn)
{
	dcassert(conn->isSet(UserConnection::FLAG_UPLOAD));
	
#ifdef IRAINMAN_DISALLOWED_BAN_MSG
	if (uc->isSet(UserConnection::FLAG_SUPPORTS_BANMSG))
	{
		uc->error(UserConnection::g_PLEASE_UPDATE_YOUR_CLIENT);
		return;
	}
#endif
	
	ConnectionQueueItemPtr cqi;
	{
		//WRITE_LOCK(*g_csUploads);
		LOCK(g_csUploads);
		const auto i = find(g_uploads.begin(), g_uploads.end(), conn->getUser());
		if (i == g_uploads.cend())
		{
			conn->setFlag(UserConnection::FLAG_ASSOCIATED);
			cqi = getCQI_L(conn->getHintedUser(), false);
			cqi->setState(ConnectionQueueItem::ACTIVE);
			conn->setConnectionQueueToken(cqi->getConnectionQueueToken());
			dcdebug("ConnectionManager::addUploadConnection, leaving to uploadmanager\n");
		}
	}
	if (cqi)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire3(ConnectionManagerListener::Added(), cqi->getHintedUser(), false, cqi->getConnectionQueueToken());
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
			fly_fire1(ConnectionManagerListener::Connected(), cqi);
#endif
		}
		UploadManager::getInstance()->addConnection(conn);
		setIP(conn, cqi);
	}
	else
	{
		putConnection(conn);
	}
}

void ConnectionManager::on(UserConnectionListener::Key, UserConnection* source, const string&/* key*/) noexcept
{
	if (source->getState() != UserConnection::STATE_KEY)
	{
		dcdebug("CM::onKey Bad state, ignoring");
		return;
	}
	dcassert(source->getUser());
	if (source->isSet(UserConnection::FLAG_DOWNLOAD))
		addDownloadConnection(source);
	else
		addUploadConnection(source);
}

void ConnectionManager::on(AdcCommand::INF, UserConnection* source, const AdcCommand& cmd) noexcept
{
	if (source->getState() != UserConnection::STATE_INF)
	{
		// Already got this once, ignore...
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Expecting INF"));
		dcdebug("CM::onINF %p sent INF twice\n", (void*)source);
		source->disconnect();
		return;
	}
	
	string cid;
	if (!cmd.getParam("ID", 0, cid))
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_INF_MISSING, "ID missing").addParam("FL", "ID"));
		dcdebug("CM::onINF missing ID\n");
		source->disconnect();
		return;
	}
	
	source->setUser(ClientManager::findUser(CID(cid)));
	
	if (!source->getUser())
	{
		dcdebug("CM::onINF: User not found");
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "User not found"));
		putConnection(source);
		return;
	}
	
	if (!checkKeyprint(source))
	{
		putConnection(source);
		return;
	}
	
	string token;
	if (source->isSet(UserConnection::FLAG_INCOMING))
	{
		if (!cmd.getParam("TO", 0, token))
		{
			source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "TO missing"));
			putConnection(source);
			return;
		}
	}
	else
	{
		token = source->getUserConnectionToken();
	}
	
	if (source->isSet(UserConnection::FLAG_INCOMING))
	{
		source->inf(false);
	}
	
	dcassert(!token.empty());
	bool down;
	{
		READ_LOCK(*g_csDownloads);
		const auto i = find(g_downloads.begin(), g_downloads.end(), source->getUser());
		
		if (i != g_downloads.cend())
		{
			(*i)->setErrors(0);
			if ((*i)->getConnectionQueueToken() == token)
			{
				down = true;
			}
			else
			{
				down = false;
#ifdef IRAINMAN_CONNECTION_MANAGER_TOKENS_DEBUG
				dcassert(0); // [+] IRainman fix: download token mismatch.
#endif
			}
		}
		else // [!] IRainman fix: check tokens for upload connections.
#ifndef IRAINMAN_CONNECTION_MANAGER_TOKENS_DEBUG
		{
			down = false;
		}
#else
		{
			const ConnectionQueueItem::Iter j = find(uploads.begin(), uploads.end(), source->getUser());
		
			if (j != uploads.cend())
			{
				const string& to = (*j)->getToken();
		
				if (to == token)
				{
					down = false;
				}
				else
				{
					down = false;
					dcassert(0); // [!] IRainman fix: upload token mismatch.
				}
			}
			else
			{
				down = false;
			}
		}
#endif
	}
	
	if (down)
	{
		source->setFlag(UserConnection::FLAG_DOWNLOAD);
		addDownloadConnection(source);
	}
	else
	{
		source->setFlag(UserConnection::FLAG_UPLOAD);
		addUploadConnection(source);
	}
}

void ConnectionManager::force(const UserPtr& user)
{
	READ_LOCK(*g_csDownloads);
	
	const auto i = find(g_downloads.begin(), g_downloads.end(), user);
	if (i != g_downloads.end())
	{
#ifdef FLYLINKDC_USE_FORCE_CONNECTION
		// TODO унести из лока
		fly_fire1(ConnectionManagerListener::Forced(), *i);
#endif
		(*i)->setLastAttempt(0);
	}
}

bool ConnectionManager::checkKeyprint(UserConnection* source)
{
	if (!source->isSecure() || source->isTrusted())
		return true;
	const string kp = ClientManager::getStringField(source->getUser()->getCID(), source->getHubUrl(), "KP");
	return source->verifyKeyprint(kp, BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS));
}

void ConnectionManager::failed(UserConnection* source, const string& error, bool protocolError)
{
	if (source->isSet(UserConnection::FLAG_ASSOCIATED))
	{
		HintedUser user;
		ReasonItem reasonItem;
		string token;
		bool doFire = true;
		if (source->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			WRITE_LOCK(*g_csDownloads);
			auto i = find(g_downloads.begin(), g_downloads.end(), source->getUser());
			//dcassert(i != g_downloads.end());
			if (i == g_downloads.end())
			{
				dcassert(0);
			}
			else
			{
				ConnectionQueueItemPtr cqi = *i;
				user = cqi->getHintedUser();
				token = cqi->getConnectionQueueToken();
				cqi->setState(ConnectionQueueItem::WAITING);
				cqi->setLastAttempt(GET_TICK());
				cqi->setErrors(protocolError ? -1 : (cqi->getErrors() + 1));
				reasonItem.hintedUser = cqi->getHintedUser();
				reasonItem.reason = error;
				reasonItem.token = cqi->getConnectionQueueToken();
				//!!! putCQI_L(cqi); не делаем отключение - тер€ем докачку https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1679
			}
		}
		else if (source->isSet(UserConnection::FLAG_UPLOAD))
		{
			{
				LOCK(g_csUploads);
				auto i = find(g_uploads.begin(), g_uploads.end(), source->getUser());
				dcassert(i != g_uploads.end());
				if (i == g_uploads.end())
				{
					dcassert(0);
				}
				else
				{
					ConnectionQueueItemPtr cqi = *i;
					user = cqi->getHintedUser();
					token = cqi->getConnectionQueueToken();
					putCQI_L(cqi);
				}
			}
			doFire = false;
			// такого удалени€ нет в ApexDC++
			//if (!ClientManager::isBeforeShutdown())
			//{
			//  fly_fire3(ConnectionManagerListener::Removed(), source->getHintedUser(), l_is_download, token);
			//}
		}
		if (doFire && !ClientManager::isBeforeShutdown() && reasonItem.hintedUser.user)
		{
			fly_fire3(ConnectionManagerListener::FailedDownload(), reasonItem.hintedUser, reasonItem.reason, token);
		}
	}
	else
	{
		//dcassert(0);
	}
	putConnection(source);
}

void ConnectionManager::on(UserConnectionListener::Failed, UserConnection* source, const string& error) noexcept
{
	failed(source, error, false);
}

void ConnectionManager::on(UserConnectionListener::ProtocolError, UserConnection* source, const string& error) noexcept
{
	failed(source, error, true);
}

void ConnectionManager::disconnect(const UserPtr& user)
{
	READ_LOCK(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); ++i)
	{
		UserConnection* uc = *i;
		if (uc->getUser() == user)
		{
			uc->disconnect(true);
			if (CMD_DEBUG_ENABLED()) 
				DETECTION_DEBUG("[ConnectionManager][disconnect] " + uc->getHintedUser().toString());
		}
	}
}

void ConnectionManager::disconnect(const UserPtr& user, bool isDownload)
{
	READ_LOCK(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); ++i)
	{
		UserConnection* uc = *i;
		dcassert(uc);
		if (uc->getUser() == user && uc->isSet((Flags::MaskType)(isDownload ? UserConnection::FLAG_DOWNLOAD : UserConnection::FLAG_UPLOAD)))
		{
			uc->disconnect(true);
			if (CMD_DEBUG_ENABLED()) 
				DETECTION_DEBUG("[ConnectionManager][disconnect] " + uc->getHintedUser().toString());
		}
	}
}

void ConnectionManager::shutdown()
{
	dcassert(!g_shuttingDown);
	g_shuttingDown = true;
	g_portTest.shutdown();
	removeUnusedConnections();
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	{
		LOCK(g_cs_update);
		g_users_for_update.clear();
	}
	
	disconnect();
	{
		READ_LOCK(*g_csConnection);
		for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
		{
			(*j)->disconnect(true);
		}
	}

	// Wait until all connections have died out...
	int waitFor = 10;
	uint64_t now = GET_TICK();
#ifdef DEBUG_USER_CONNECTION
	uint64_t dumpTime = now + 4000;
#else
	uint64_t exitTime = now + 12000;
#endif
	while (true)
	{
#ifdef _DEBUG
		size_t size;
#endif
		{
			READ_LOCK(*g_csConnection);
			if (g_userConnections.empty()) break;
#ifdef _DEBUG
			size = g_userConnections.size();
#endif
		}
#ifdef _DEBUG
		LogManager::message("ConnectionManager::shutdown g_userConnections: " + Util::toString(size), false);
#endif
		Thread::sleep(waitFor);
#ifdef _DEBUG
		if (waitFor < 1000) waitFor *= 2;
#endif
		removeUnusedConnections();
		now = GET_TICK();
#ifdef DEBUG_USER_CONNECTION
		if (now >= dumpTime)
		{
			dumpUserConnections();
			dumpTime = GET_TICK() + 15000;
		}
#else
		if (now > exitTime)
		{
			LogManager::message("ConnectionManager::shutdown exit by timeout", false);
			break;
		}
#endif
	}
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	// —брасываем рейтинг в базу пока не нашли причину почему тут остаютс€ записи.
	{
		bool ipStat = BOOLSETTING(ENABLE_RATIO_USER_LIST);
		bool userStat = BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER);
		{
			READ_LOCK(*g_csDownloads);
			for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
			{
				const ConnectionQueueItemPtr& cqi = *i;
				cqi->getUser()->saveStats(ipStat, userStat);
			}
		}
		{
			//READ_LOCK(*g_csUploads);
			LOCK(g_csUploads);
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
			{
				const ConnectionQueueItemPtr& cqi = *i;
				cqi->getUser()->saveStats(ipStat, userStat);
			}
		}
	}
#endif
	g_downloads.clear();
	g_uploads.clear();
}

// UserConnectionListener
void ConnectionManager::on(UserConnectionListener::Supports, UserConnection* conn, StringList& feat) noexcept
{
	dcassert(conn);
	dcassert(conn->getUser());
	if (conn->getUser()) // 44 падени€ https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=48388
	{
		uint8_t knownUcSupports = 0;
		auto unknownUcSupports = UcSupports::setSupports(conn, feat, knownUcSupports);
		ClientManager::setSupports(conn->getUser(), unknownUcSupports, knownUcSupports);
	}
	else
	{
		LogManager::message("Error UserConnectionListener::Supports conn->getUser() == nullptr, url = " + conn->getHintedUser().hint);
	}
}

void ConnectionManager::setUploadLimit(const UserPtr& user, int lim)
{
	WRITE_LOCK(*g_csConnection);
	auto i = g_userConnections.begin();
	while (i != g_userConnections.end())
	{
		if ((*i)->state == UserConnection::STATE_UNUSED)
		{
			delete *i;
			g_userConnections.erase(i++);
			continue;
		}
		if ((*i)->isSet(UserConnection::FLAG_UPLOAD) && (*i)->getUser() == user)
			(*i)->setUploadLimit(lim);
		i++;
	}
}

void ConnectionManager::removeUnusedConnections()
{
	WRITE_LOCK(*g_csConnection);
	auto i = g_userConnections.begin();
	while (i != g_userConnections.end())
	{
		if ((*i)->state == UserConnection::STATE_UNUSED)
		{
			delete *i;
			g_userConnections.erase(i++);
		} else i++;
	}
}

void ConnectionManager::updateAverageSpeed(uint64_t tick)
{
	uploadSpeed.addSample(Socket::g_stats.tcp.uploaded + Socket::g_stats.ssl.uploaded, tick);
	int64_t avg = uploadSpeed.getAverage(3000);
	if (avg >= 0) UploadManager::setRunningAverage(avg);
	downloadSpeed.addSample(Socket::g_stats.tcp.downloaded + Socket::g_stats.ssl.downloaded, tick);
	avg = downloadSpeed.getAverage(3000);
	if (avg >= 0) DownloadManager::setRunningAverage(avg);
}

string ConnectionManager::getUserConnectionInfo()
{
	string info;
	READ_LOCK(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); i++)
	{
		if (!info.empty()) info += '\n';
		info += (*i)->getDescription();
	}
	return info;
}

#ifdef DEBUG_USER_CONNECTION
void ConnectionManager::dumpUserConnections()
{
	READ_LOCK(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); i++)
		(*i)->dumpInfo();
}
#endif

void ConnectionManager::fireUploadError(const HintedUser& hintedUser, const string& reason, const string& token) noexcept
{
	fly_fire3(ConnectionManagerListener::FailedUpload(), hintedUser, reason, token);
}
