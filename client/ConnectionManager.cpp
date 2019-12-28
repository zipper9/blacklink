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
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CryptoManager.h"
#include "QueueManager.h"
#include "ShareManager.h"
#include "DebugManager.h"
#include "SSLSocket.h"
#include "PortTest.h"
#include "ConnectivityManager.h"
#include "NmdcHub.h"

uint16_t ConnectionManager::g_ConnToMeCount = 0;
bool ConnectionManager::g_is_test_tcp_port = false;
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csConnection = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csDownloads = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
//std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csUploads = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
CriticalSection ConnectionManager::g_csUploads;
FastCriticalSection ConnectionManager::g_csDdosCheck;
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csDdosCTM2HUBCheck = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csTTHFilter = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csFileFilter = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());

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
	CFlyFastLock(m_cs);
	do
	{
		token = Util::toString(Util::rand());
	}
	while (m_tokens.find(token) != m_tokens.end());
	m_tokens.insert(token);
#ifdef _DEBUG
	LogManager::message("TokenManager::makeToken token = " + token);
#endif
	if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][TokenManager::makeToken] " + token);
	return token;
}

bool TokenManager::isToken(const string& aToken) noexcept
{
	CFlyFastLock(m_cs);
	return m_tokens.find(aToken) != m_tokens.end();
}

bool TokenManager::addToken(const string& aToken) noexcept
{
	CFlyFastLock(m_cs);
	if (m_tokens.find(aToken) == m_tokens.end())
	{
		m_tokens.insert(aToken);
#ifdef _DEBUG
		LogManager::message("TokenManager::addToken [+] token = " + aToken);
#endif
		return true;
	}
#ifdef _DEBUG
	LogManager::message("TokenManager::addToken [-] token = " + aToken);
#endif
	return false;
}

unsigned TokenManager::countToken() noexcept
{
	CFlyFastLock(m_cs);
	return m_tokens.size();
}

void TokenManager::removeToken(const string& aToken) noexcept
{
	CFlyFastLock(m_cs);
	const auto p = m_tokens.find(aToken);
	if (p != m_tokens.end())
	{
#ifdef _DEBUG
		LogManager::message("TokenManager::removeToken [+] token = " + aToken);
#endif
		m_tokens.erase(p);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][TokenManager::removeToken] " + aToken);
	}
	else
	{
#ifdef _DEBUG
		LogManager::message("TokenManager::removeToken [-] token = " + aToken);
#endif
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][TokenManager::removeToken][empty] " + aToken);
//		dcassert(0);
	}
}

string TokenManager::toString() noexcept
{
	CFlyFastLock(m_cs);
	string l_res;
	if (!m_tokens.empty())
	{
		l_res = "Tokens:\r\n";
		for (auto i : m_tokens)
		{
			l_res += i;
			l_res += ",";
		}
	}
	return l_res;
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
	
	TimerManager::getInstance()->addListener(this); // [+] IRainman fix.
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
	uint16_t port;
	string bind = SETTING(BIND_ADDRESS);
	
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		server = new Server(false, 0, Util::emptyString);
	}
	else
	{
		port = static_cast<uint16_t>(SETTING(TCP_PORT));
		server = new Server(false, port, bind);
	}
	SET_SETTING(TCP_PORT, server->getServerPort());
	
	if (!CryptoManager::TLSOk())
	{
		LogManager::message("Skipping secure port: " + Util::toString(SETTING(USE_TLS)));
		dcdebug("Skipping secure port: %d\n", SETTING(USE_TLS));
	}
	else
	{
		if (BOOLSETTING(AUTO_DETECT_CONNECTION))
		{
			secureServer = new Server(true, 0, Util::emptyString);
			SET_SETTING(TLS_PORT, secureServer->getServerPort());
		}
		else
		{
			port = static_cast<uint16_t>(SETTING(TLS_PORT));
			secureServer = new Server(true, port, bind);
		}
	}
	test_tcp_port();
	::PostMessage(LogManager::g_mainWnd, WM_SPEAKER_AUTO_CONNECT, 0, 0);
}

void ConnectionManager::test_tcp_port()
{
#if 0 // ???
	extern bool g_DisableTestPort;
	if (g_DisableTestPort == false)
	{
		string l_external_ip;
		std::vector<unsigned short> l_udp_port, l_tcp_port;
		l_tcp_port.push_back(SETTING(TCP_PORT));
		if (CryptoManager::TLSOk())
		{
			l_tcp_port.push_back(SETTING(TLS_PORT));
		}
		const bool l_is_tcp_port_send = CFlyServerJSON::pushTestPort(l_udp_port, l_tcp_port, l_external_ip, 0, CryptoManager::TLSOk() ? "TCP+TLS" : "TCP");
		//dcassert(l_is_tcp_port_send);
	}
	g_is_test_tcp_port = true;
#endif
}

/**
 * Request a connection for downloading.
 * DownloadManager::addConnection will be called as soon as the connection is ready
 * for downloading.
 * @param aUser The user to connect to.
 */
void ConnectionManager::getDownloadConnection(const UserPtr& aUser)
{
	dcassert(aUser);
	dcassert(!ClientManager::isBeforeShutdown());
	ConnectionQueueItemPtr cqi;
	if (!ClientManager::isBeforeShutdown())
	{
		{
			CFlyWriteLock(*g_csDownloads);
			const auto i = find(g_downloads.begin(), g_downloads.end(), aUser);
			if (i == g_downloads.end())
			{
				cqi = getCQI_L(HintedUser(aUser, Util::emptyString), true);
			}
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
			else
			{
				if (find(m_checkIdle.begin(), m_checkIdle.end(), aUser) == m_checkIdle.end())
				{
					m_checkIdle.push_back(aUser); // TODO - Лок?
				}
			}
#endif
		}
		if (cqi && !ClientManager::isBeforeShutdown())
		{
			fly_fire3(ConnectionManagerListener::Added(), HintedUser(aUser, Util::emptyString), true, cqi->getConnectionQueueToken());
			return;
		}
#ifndef USING_IDLERS_IN_CONNECTION_MANAGER
		DownloadManager::checkIdle(aUser);
#endif
	}
}

ConnectionQueueItemPtr ConnectionManager::getCQI_L(const HintedUser& aHintedUser, bool download)
{
	auto cqi = std::make_shared<ConnectionQueueItem>(aHintedUser, download, g_tokens_manager.makeToken());
	if (download)
	{
		dcassert(find(g_downloads.begin(), g_downloads.end(), aHintedUser) == g_downloads.end());
		g_downloads.insert(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][download] " + aHintedUser.toString());
	}
	else
	{
		dcassert(find(g_uploads.begin(), g_uploads.end(), aHintedUser) == g_uploads.end());
		g_uploads.insert(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][upload] " + aHintedUser.toString());
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
	// Вешаемся при активной закачке cqi->getUser()->flushRatio();
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
	CFlyReadLock(*g_csConnection);
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
		CFlyWriteLock(*g_csConnection);
		g_userConnections.insert(uc);
	}
	if (nmdc)
		uc->setFlag(UserConnection::FLAG_NMDC);
	if (secure)
		uc->setFlag(UserConnection::FLAG_SECURE);
	if (CMD_DEBUG_ENABLED())
		DETECTION_DEBUG("[ConnectionManager][getConnection] " + uc->getHintedUser().toString());
	return uc;
}

void ConnectionManager::putConnection(UserConnection* conn)
{
	conn->removeListener(this);
	conn->disconnect(true);
	{
		CFlyWriteLock(*g_csConnection);
		auto i = g_userConnections.find(conn);
		if (i != g_userConnections.end())
			(*i)->state = UserConnection::STATE_UNUSED;
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
		CFlyWriteLock(*g_csConnection);
		auto i = g_userConnections.find(conn);
		if (i != g_userConnections.end())
		{
			delete *i;
			g_userConnections.erase(i);
		}
	}
}

void ConnectionManager::flushOnUserUpdated()
{
	UserSet l_users_for_update;
	{
		CFlyFastLock(g_cs_update);
		l_users_for_update.swap(g_users_for_update);
	}
	for (auto i = l_users_for_update.cbegin(); i != l_users_for_update.cend(); ++i)
	{
		onUserUpdated(*i);
	}
}

void ConnectionManager::addOnUserUpdated(const UserPtr& aUser)
{
	size_t l_size = 0;
	{
		CFlyFastLock(g_cs_update);
		const auto l_res_insert = g_users_for_update.insert(aUser);
		l_size = g_users_for_update.size();
			
#ifdef _DEBUG
		if (l_res_insert.second == false)
		{
			//LogManager::message("Skip dup g_users_for_update.insert(aUser) " + aUser->getLastNick());
		}
#endif
	}
}

void ConnectionManager::on(ClientManagerListener::UserConnected, const UserPtr& aUser) noexcept
{
	addOnUserUpdated(aUser);
}

void ConnectionManager::on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept
{
	addOnUserUpdated(aUser);
}

struct CFlyTokenItem
{
	HintedUser m_hinted_user;
	string m_token;
	CFlyTokenItem()
	{
	}
	CFlyTokenItem(const ConnectionQueueItemPtr& p_cqi) : m_hinted_user(p_cqi->getHintedUser()), m_token(p_cqi->getConnectionQueueToken())
	{
	}
};

struct CFlyReasonItem : public CFlyTokenItem
{
	string m_reason;
	CFlyReasonItem()
	{
	}
	CFlyReasonItem(const ConnectionQueueItemPtr& p_cqi, const string& p_reason) : CFlyTokenItem(p_cqi), m_reason(p_reason)
	{
	}
};

void ConnectionManager::onUserUpdated(const UserPtr& aUser)
{
	if (!ClientManager::isBeforeShutdown())
	{
		std::vector<CFlyTokenItem> l_download_users;
		std::vector<CFlyTokenItem> l_upload_users;
		{
			CFlyReadLock(*g_csDownloads);
			for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
			{
				if ((*i)->getUser() == aUser) // todo - map
				{
					l_download_users.push_back(CFlyTokenItem(*i));
				}
			}
		}
		{
			//CFlyReadLock(*g_csUploads);
			CFlyLock(g_csUploads);
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
			{
				if ((*i)->getUser() == aUser)  // todo - map
				{
					l_upload_users.push_back(CFlyTokenItem(*i));
				}
			}
		}
		for (auto i = l_download_users.cbegin(); i != l_download_users.cend(); ++i)
		{
			fly_fire3(ConnectionManagerListener::UserUpdated(), i->m_hinted_user, true, i->m_token);
		}
		for (auto i = l_upload_users.cbegin(); i != l_upload_users.cend(); ++i)
		{
			fly_fire3(ConnectionManagerListener::UserUpdated(), i->m_hinted_user, false, i->m_token);
		}
	}
}

void ConnectionManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
#if 0
	if (((aTick / 1000) % (CFlyServerConfig::g_max_unique_tth_search + 2)) == 0)
	{
		cleanupDuplicateSearchTTH(aTick);
	}
	if (((aTick / 1000) % (CFlyServerConfig::g_max_unique_file_search + 2)) == 0)
	{
		cleanupDuplicateSearchFile(aTick);
	}
#endif
	flushOnUserUpdated();
	std::vector<ConnectionQueueItemPtr> removed;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	UserList l_idlers;
#endif
	std::vector<CFlyTokenItem> statusChanged;
	std::vector<CFlyReasonItem> downloadError;
	{
		CFlyReadLock(*g_csDownloads);
		uint16_t l_attempts = 0;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		l_idlers.swap(m_checkIdle); // [!] IRainman opt: use swap.
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
				if (cqi->getLastAttempt() == 0 || ((SETTING(DOWNCONN_PER_SEC) == 0 || l_attempts < SETTING(DOWNCONN_PER_SEC)) &&
				                                   cqi->getLastAttempt() + l_count_sec * 1000 * max(1, l_count_error) < aTick))
				{
					cqi->setLastAttempt(aTick);
					
					const bool startDown = DownloadManager::isStartDownload(prio);
					
					if (cqi->getState() == ConnectionQueueItem::WAITING)
					{
						if (startDown)
						{
							cqi->setState(ConnectionQueueItem::CONNECTING);
							
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
							if (BOOLSETTING(AUTO_PASSIVE_INCOMING_CONNECTIONS))
							{
								cqi->m_count_waiting++;
								cqi->m_is_force_passive = cqi->m_is_active_client ? (cqi->m_count_waiting > 1) : false; // Делаем вторую попытку подключения в пассивке ?
							}
							else
							{
								cqi->m_is_force_passive = false;
							}
#endif
							
							ClientManager::getInstance()->connect(cqi->getHintedUser(),
							                                      cqi->getConnectionQueueToken(),
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
							                                      cqi->m_is_force_passive,
#else
							                                      false,
#endif
							                                      cqi->m_is_active_client // <- Out param!
							                                     );
							statusChanged.emplace_back(cqi);
							l_attempts++;
						}
						else
						{
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
							cqi->m_count_waiting = 0;
#endif
							cqi->setState(ConnectionQueueItem::NO_DOWNLOAD_SLOTS);
							downloadError.emplace_back(cqi, STRING(ALL_DOWNLOAD_SLOTS_TAKEN));
						}
					}
					else if (cqi->getState() == ConnectionQueueItem::NO_DOWNLOAD_SLOTS && startDown)
					{
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
						cqi->m_count_waiting = 0;
#endif
						cqi->setState(ConnectionQueueItem::WAITING);
					}
				}
				else if (cqi->getState() == ConnectionQueueItem::CONNECTING && cqi->getLastAttempt() + l_count_sec_connecting * 1000 < aTick)
				{
					ClientManager::connectionTimeout(cqi->getUser());
					
					cqi->setErrors(cqi->getErrors() + 1);
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
					if (cqi->m_count_waiting > 2)
					{
						downloadError.push_back(make_pair(cqi->getUser(), STRING(CONNECTION_TIMEOUT)));
						cqi->setState(ConnectionQueueItem::WAITING);
						// TODO - удаление пока не делаем - нужно потестировать лучше
						// removed.push_back(cqi);
					}
					else
#endif
					{
						downloadError.emplace_back(cqi, STRING(CONNECTION_TIMEOUT));
						cqi->setState(ConnectionQueueItem::WAITING);
					}
				}
			}
		}
	}
	if (!removed.empty())
	{
		for (auto m = removed.begin(); m != removed.end(); ++m)
		{
			// const HintedUser l_hinted_user = (*m)->getHintedUser();
			const bool l_is_download = (*m)->isDownload();
			const auto l_hinted_user = (*m)->getHintedUser();
			const auto token = (*m)->getConnectionQueueToken();
			if (l_is_download)
			{
				CFlyWriteLock(*g_csDownloads);
				putCQI_L(*m);
			}
			else
			{
				CFlyLock(g_csUploads);
				putCQI_L(*m);
			}
			if (!ClientManager::isBeforeShutdown())
			{
				fly_fire3(ConnectionManagerListener::Removed(), l_hinted_user, l_is_download, token);
			}
		}
		removed.clear();
	}
	
	for (auto j = statusChanged.cbegin(); j != statusChanged.cend(); ++j)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire3(ConnectionManagerListener::ConnectionStatusChanged(), j->m_hinted_user, true, j->m_token);
		}
	}
	statusChanged.clear();
	// TODO - не звать для тех у кого ошибка загрузки
	for (auto k = downloadError.cbegin(); k != downloadError.cend(); ++k)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire3(ConnectionManagerListener::FailedDownload(), k->m_hinted_user, k->m_reason, k->m_token);
		}
	}
	downloadError.clear();
	
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	for (auto i = l_idlers.cbegin(); i != l_idlers.cend(); ++i)
	{
		DownloadManager::checkIdle(*i);
	}
#endif
}

void ConnectionManager::cleanupIpFlood(const uint64_t p_tick)
{
#if 0
	CFlyFastLock(g_csDdosCheck);
	for (auto j = g_ddos_map.cbegin(); j != g_ddos_map.cend();)
	{
		// Если коннектов совершено меньше чем предел в течении минуты - убираем адрес из таблицы - с ним все хорошо!
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
		// Если коннектов совершено много и IP находится в бане, но уже прошло время больше чем 5 Минут(по умолчанию)
		// Также убираем запись из таблицы блокировки
		const bool l_is_ddos_ban_close = j->second.m_count_connect > CFlyServerConfig::g_max_ddos_connect_to_me
		                                 && l_tick_delta > CFlyServerConfig::g_ban_ddos_connect_to_me * 1000 * 60;
		if (l_is_ddos_ban_close && BOOLSETTING(LOG_DDOS_TRACE))
		{
			string l_type;
			if (j->first.m_ip.is_unspecified()) // Если нет второго IP то это команада  ConnectToMe
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
	CFlyWriteLock(*g_csFileFilter);
	for (auto j = g_duplicate_search_file.cbegin(); j != g_duplicate_search_file.cend();)
	{
		if ((p_tick - j->second.m_first_tick) > 1000 * CFlyServerConfig::g_max_unique_file_search)
		{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_FILE_SEARCH
			if (j->second.m_count_connect > 1) // Событие возникало больше одного раза - логируем?
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
	CFlyWriteLock(*g_csTTHFilter);
	for (auto j = g_duplicate_search_tth.cbegin(); j != g_duplicate_search_tth.cend();)
	{
		if ((p_tick - j->second.m_first_tick) > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_TTH_SEARCH
			if (j->second.m_count_connect > 1) // Событие возникало больше одного раза - логируем?
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
	CFlyReadLock(*g_csConnection);
	for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
	{
		auto& connection = *j;
#ifdef _DEBUG
		if ((connection->getLastActivity() + 60 * 1000) < tick)
#else
		if ((connection->getLastActivity() + 60 * 1000) < tick) // Зачем так много минут висеть?
#endif
		{
			connection->disconnect(true);
		}
	}
}

static const uint64_t g_FLOOD_TRIGGER = 20000;
static const uint64_t g_FLOOD_ADD = 2000;

ConnectionManager::Server::Server(bool p_is_secure, uint16_t p_port, const string& p_server_ip /* = "0.0.0.0" */)
	: m_is_secure(p_is_secure)
{
	m_sock.create();
	m_sock.setSocketOpt(SO_REUSEADDR, 1);
	m_server_ip   = p_server_ip; // в AirDC++ и дургих этого уже нет
	LogManager::message("Starting to listen " + m_server_ip + ':' + Util::toString(p_port) + " secure=" + Util::toString((int) m_is_secure));
	m_server_port = m_sock.bind(p_port, p_server_ip);
	m_sock.listen();
	start(64);
}

static const uint64_t POLL_TIMEOUT = 250;

int ConnectionManager::Server::run() noexcept
{
	while (!isShutdown())
	{
		try
		{
			while (!isShutdown())
			{
				auto ret = m_sock.wait(POLL_TIMEOUT, Socket::WAIT_READ);
				if (ret == Socket::WAIT_READ)
				{
					ConnectionManager::getInstance()->accept(m_sock, m_is_secure, this);
				}
			}
		}
		catch (const Exception& e)
		{
			LogManager::message(STRING(LISTENER_FAILED) + ' ' + e.getError());
		}
		bool failed = false;
		while (!isShutdown())
		{
			try
			{
				m_sock.disconnect();
				m_sock.create();
				m_server_port = m_sock.bind(m_server_port, m_server_ip);
				dcassert(m_server_port);
				LogManager::message("Starting to listen " + m_server_ip + ':' + Util::toString(m_server_port) + " secure=" + Util::toString((int) m_is_secure));
				m_sock.listen();
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
				for (int i = 0; i < 60 && !isShutdown(); ++i)
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
void ConnectionManager::accept(const Socket& sock, bool secure, Server* server) noexcept
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
	unique_ptr<Socket> newSock(secure ? new SSLSocket(CryptoManager::SSL_SERVER, allowUntrusted, Util::emptyString) : new Socket(/*Socket::TYPE_TCP */));
	try
	{
		port = newSock->accept(sock);
	}
	catch (const Exception&)
	{
		if (secure && server)
		{
			int portTLS;
			g_portTest.getState(PortTest::PORT_TLS, portTLS, nullptr);
			if (server->getServerPort() == portTLS)
			{
				// FIXME: Is it possible to get FlyLink's magic string from SSL socket buffer?
				g_portTest.processInfo(PortTest::PORT_TLS, string(), false);
				ConnectivityManager::getInstance()->processPortTestResult();
			}
		}
		return;
	}
	UserConnection* uc = getConnection(false, secure);
	uc->setFlag(UserConnection::FLAG_INCOMING);
	uc->setState(UserConnection::STATE_SUPNICK);
	uc->updateLastActivity();
	uc->addAcceptedSocket(newSock, port);
}

bool ConnectionManager::checkDuplicateSearchFile(const string& p_search_command)
{
#if 0
	CFlyWriteLock(*g_csFileFilter);
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
		if (l_result.second == false) // Элемент уже существует - проверим его счетчик и старость.
		{
			l_cur_value.m_last_tick = l_tick;
			if (l_tick - l_cur_value.m_first_tick > 1000 * CFlyServerConfig::g_max_unique_file_search)
			{
				// Тут можно сразу стереть элемент устаревший
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
	CFlyWriteLock(*g_csTTHFilter);
	const auto l_tick = GET_TICK();
	CFlyTickTTH l_item;
	l_item.m_first_tick = l_tick;
	l_item.m_last_tick  = l_tick;
	auto l_result = g_duplicate_search_tth.insert(std::pair<string, CFlyTickTTH>(p_search_command + ' ' + p_tth.toBase32(), l_item));
	auto& l_cur_value = l_result.first->second;
	++l_cur_value.m_count_connect;
	if (l_result.second == false) // Элемент уже существует - проверим его счетчик и старость.
	{
		l_cur_value.m_last_tick  = l_tick;
		if (l_tick - l_cur_value.m_first_tick > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
			// Тут можно сразу стереть элемент устаревший
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
	const string cmt2hub = "[" + Util::formatDigitalDate() + "] CTM2HUB = " + serverAddr + " <<= DDoS block from: " + hintedUser.hint;
	bool isDuplicate;
	{
		CFlyWriteLock(*g_csDdosCTM2HUBCheck);
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
		const string l_guard_port = "[" + Util::formatDigitalDate() + "] [TCP PortGuard] Block DDoS: " + aIPServer + ':' + Util::toString(aPort) + " HubInfo: " + p_HubInfo + " UserInfo: " + p_userInfo;
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
			CFlyReadLock(*g_csDdosCTM2HUBCheck);
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
		CFlyFastLock(g_csDdosCheck);
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
			// Элемент уже существует
			l_cur_value.m_last_tick = l_tick;   // Корректируем время последней активности.
			l_cur_value.m_ports.insert(aPort);  // Сохраним последний порт
			if (l_cur_value.m_count_connect == CFlyServerConfig::g_max_ddos_connect_to_me) // Превысили кол-во коннектов по одному IP
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
					return true; // Лочим этот коннект до наступления амнистии. TODO - проверить эту часть внимательей
					// в след части фикса - проводить анализ протокола и коннекты на порты лочить на вечно.
				}
			}
		}
	}
	{
		CFlyReadLock(*g_csConnection);
		// We don't want to be used as a flooding instrument
		int count = 0;
		for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
		{
			const UserConnection& uc = **j;
			if (uc.socket == nullptr || !uc.socket->hasSocket())
				continue;
			if (uc.getPort() == aPort && uc.getRemoteIp() == aIPServer) // TODO - не поддерживается DNS
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

void ConnectionManager::nmdcConnect(const string& aIPServer, uint16_t aPort, const string& aNick, const string& hubUrl,
                                    const string& encoding,
                                    bool secure)
{
	nmdcConnect(aIPServer, aPort, 0, BufferedSocket::NAT_NONE, aNick, hubUrl,
	            encoding,
	            secure);
}

void ConnectionManager::nmdcConnect(const string& aIPServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aNick, const string& hubUrl,
                                    const string& encoding,
                                    bool secure)
{
	if (isShuttingDown())
	{
		dcassert(0);
		return;
	}
#ifdef FLYLINKDC_USE_BLOCK_ERROR_CMD
	if (UserConnection::is_error_user(aIPServer))
	{
		dcassert(0);
		return;
	}
#endif
	
	if (checkIpFlood(aIPServer, aPort, boost::asio::ip::address_v4(), "", "[nmdcConnect][Hub: " + hubUrl + "]"))
	{
		//dcassert(0);
		return;
	}
	
	UserConnection* uc = getConnection(true, secure); // [!] IRainman fix SSL connection on NMDC(S) hubs.
	uc->setServerPort(aIPServer + ':' + Util::toString(aPort)); // CTM2HUB
	uc->setUserConnectionToken(aNick); // Токен = ник?
	uc->setHubUrl(hubUrl);
	uc->setEncoding(encoding);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setFlag(UserConnection::FLAG_NMDC);
	try
	{
		uc->connect(aIPServer, aPort, localPort, natRole);
	}
	catch (const Exception&)
	{
		deleteConnection(uc);
	}
}

void ConnectionManager::adcConnect(const OnlineUser& aUser, uint16_t aPort, const string& aToken, bool secure)
{
	adcConnect(aUser, aPort, 0, BufferedSocket::NAT_NONE, aToken, secure);
}

void ConnectionManager::adcConnect(const OnlineUser& aUser, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aToken, bool secure)
{
	if (isShuttingDown())
		return;
		
	if (checkIpFlood(aUser.getIdentity().getIpAsString(), aPort, boost::asio::ip::address_v4(), "", "[adcConnect][Hub: " + aUser.getClientBase().getHubName() + "]")) // "ADC Nick: " + aUser.getIdentity().getNick() +
		return;
		
	UserConnection* uc = getConnection(false, secure);
	uc->setUserConnectionToken(aToken);
	uc->setEncoding(Text::g_utf8);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setHubUrl(&aUser.getClient() == nullptr ? "DHT" : aUser.getClient().getHubUrl());
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	if (aUser.getIdentity().isOp())
	{
		uc->setFlag(UserConnection::FLAG_OP);
	}
#endif
	try
	{
		uc->connect(aUser.getIdentity().getIpAsString(), aPort, localPort, natRole);
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

void ConnectionManager::on(AdcCommand::SUP, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	if (aSource->getState() != UserConnection::STATE_SUPNICK)
	{
		// Already got this once, ignore...@todo fix support updates
		dcdebug("CM::onSUP %p sent sup twice\n", (void*)aSource);
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
				{
					tigrOk = true;
				}
				// ADC clients must support all these...
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_ADCGET);
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_MINISLOTS);
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_TTHF);
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_TTHL);
				// For compatibility with older clients...
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_XML_BZLIST);
			}
			else if (feat == UserConnection::FEATURE_ZLIB_GET)
			{
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_ZLIB_GET);
			}
			else if (feat == UserConnection::FEATURE_ADC_BZIP)
			{
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_XML_BZLIST);
			}
			else if (feat == UserConnection::FEATURE_ADC_TIGR)
			{
				tigrOk = true; // Variable 'tigrOk' is assigned a value that is never used.
			}
		}
	}
	
	if (!baseOk)
	{
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Invalid SUP"));
		aSource->disconnect();
		return;
	}
	
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		StringList defFeatures = adcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back("AD" + UserConnection::FEATURE_ZLIB_GET);
		}
		aSource->sup(defFeatures);
	}
	else
	{
		aSource->inf(true);
	}
	aSource->setState(UserConnection::STATE_INF);
}

void ConnectionManager::on(AdcCommand::STA, UserConnection*, const AdcCommand& /*cmd*/) noexcept
{

}

void ConnectionManager::on(UserConnectionListener::Connected, UserConnection* aSource) noexcept
{
	if (aSource->isSecure() && !aSource->isTrusted() && !BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS))
	{
		putConnection(aSource);
		LogManager::message(STRING(CERTIFICATE_NOT_TRUSTED));
		return;
	}
	dcassert(aSource->getState() == UserConnection::STATE_CONNECT);
	if (aSource->isSet(UserConnection::FLAG_NMDC))
	{
		aSource->myNick(aSource->getUserConnectionToken());
		aSource->lock(NmdcHub::getLock(), NmdcHub::getPk() + "Ref=" + aSource->getHubUrl());
	}
	else
	{
		StringList defFeatures = adcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back("AD" + UserConnection::FEATURE_ZLIB_GET);
		}
		aSource->sup(defFeatures);
		aSource->send(AdcCommand(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, Util::emptyString).addParam("RF", aSource->getHubUrl()));
	}
	aSource->setState(UserConnection::STATE_SUPNICK);
}

void ConnectionManager::on(UserConnectionListener::MyNick, UserConnection* aSource, const string& aNick) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (aSource->getState() != UserConnection::STATE_SUPNICK)
	{
		// Already got this once, ignore...
		dcdebug("CM::onMyNick %p sent nick twice\n", (void*)aSource);
		//LogManager::message("CM::onMyNick "+ aNick + " sent nick twice");
		return;
	}
	
	dcassert(!aNick.empty());
	dcdebug("ConnectionManager::onMyNick %p, %s\n", (void*)aSource, aNick.c_str());
	dcassert(!aSource->getUser());
	
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		// Try to guess where this came from...
		const auto& i = m_expectedConnections.remove(aNick);
		
		if (i.m_HubUrl.empty())
		{
			dcassert(i.m_Nick.empty());
			dcdebug("Unknown incoming connection from %s\n", aNick.c_str());
			putConnection(aSource);
			return;
		}
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		if (i.reason == ExpectedMap::REASON_DETECT_CONNECTION)
		{
			dcdebug("REASON_DETECT_CONNECTION received %s\n", aNick.c_str());
			
			// Auto-detection algorithm: we send $ConnectToMe command to first found user
			// and mark this user for expecting only for connection auto-detection.
			// So if we receive connection of this type, simply drop it.
			
			FavoriteHubEntry* fhub = FavoriteManager::getFavoriteHubEntry(i.m_HubUrl);
			if (!fhub)
				dcdebug("REASON_DETECT_CONNECTION: can't find favorite hub %s\n", i.m_HubUrl.c_str());
			//dcassert(fhub);
			
			// WARNING: only Nmdc hub requests for REASON_DETECT_CONNECTION.
			// if another hub added, one must implement autodetection in base Client class
			NmdcHub* hub = static_cast<NmdcHub*>(ClientManager::findClient(i.m_HubUrl));
			if (!hub)
				dcdebug("REASON_DETECT_CONNECTION: can't find hub %s\n", i.m_HubUrl.c_str());
			//dcassert(hub);
			
			if (hub && hub->IsAutodetectPending())
			{
				// set connection type to ACTIVE
				// TODO - if (fhub)
				//     fhub->setMode(1);
				
				hub->AutodetectComplete();
				
				dcdebug("REASON_DETECT_CONNECTION: All OK %s Nick = %s\n", i.m_HubUrl.c_str(), aNick.c_str());
				
				// TODO: allow to disable through GUI saving of detected mode to
				// favorite hub's settings
				
				fly_fire1(ConnectionManagerListener::OpenTCPPortDetected(), i.m_HubUrl);
			}
			
			putConnection(aSource);
			
			return;
		}
#endif // RIP_USE_CONNECTION_AUTODETECT
		aSource->setUserConnectionToken(i.m_Nick);
		aSource->setHubUrl(i.m_HubUrl); // TODO - тут юзера почему-то еще нет
		const auto l_encoding = ClientManager::findHubEncoding(i.m_HubUrl);
		aSource->setEncoding(l_encoding);
	}
	
	const string nick = Text::toUtf8(aNick, aSource->getEncoding());// TODO IRAINMAN_USE_UNICODE_IN_NMDC
	const CID cid = ClientManager::makeCid(nick, aSource->getHubUrl());
	
	// First, we try looking in the pending downloads...hopefully it's one of them...
	if (!ClientManager::isBeforeShutdown())
	{
		CFlyReadLock(*g_csDownloads);
		for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
		{
			const ConnectionQueueItemPtr& cqi = *i;
			cqi->setErrors(0);
			if ((cqi->getState() == ConnectionQueueItem::CONNECTING || cqi->getState() == ConnectionQueueItem::WAITING) &&
			        cqi->getUser()->getCID() == cid)
			{
				aSource->setUser(cqi->getUser());
				// Indicate that we're interested in this file...
				aSource->setFlag(UserConnection::FLAG_DOWNLOAD);
				break;
			}
		}
	}
	
	if (!aSource->getUser())
	{
		// Make sure we know who it is, i e that he/she is connected...
		
		aSource->setUser(ClientManager::findUser(cid));
		if (!aSource->getUser() || !aSource->getUser()->isOnline())
		{
			dcdebug("CM::onMyNick Incoming connection from unknown user %s\n", nick.c_str());
			putConnection(aSource);
			return;
		}
		// We don't need this connection for downloading...make it an upload connection instead...
		aSource->setFlag(UserConnection::FLAG_UPLOAD);
	}
	
	ClientManager::setIPUser(aSource->getUser(), aSource->getRemoteIp());
	
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE_ON_NMDC
	if (ClientManager::isOp(aSource->getUser(), aSource->getHubUrl()))
		aSource->setFlag(UserConnection::FLAG_OP);
#endif
		
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		aSource->myNick(aSource->getUserConnectionToken());
		aSource->lock(NmdcHub::getLock(), NmdcHub::getPk());
	}
	
	aSource->setState(UserConnection::STATE_LOCK);
}

void ConnectionManager::on(UserConnectionListener::CLock, UserConnection* aSource, const string& aLock) noexcept
{
	if (aSource->getState() != UserConnection::STATE_LOCK)
	{
		dcdebug("CM::onLock %p received lock twice, ignoring\n", (void*)aSource);
		return;
	}
	
	if (NmdcHub::isExtended(aLock))
	{
		StringList defFeatures = nmdcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back(UserConnection::FEATURE_ZLIB_GET);
		}
		aSource->supports(defFeatures);
	}
	
	aSource->setState(UserConnection::STATE_DIRECTION);
	aSource->direction(aSource->getDirectionString(), aSource->getNumber());
	aSource->key(NmdcHub::makeKeyFromLock(aLock));
}

void ConnectionManager::on(UserConnectionListener::Direction, UserConnection* aSource, const string& dir, const string& num) noexcept
{
	if (aSource->getState() != UserConnection::STATE_DIRECTION)
	{
		dcdebug("CM::onDirection %p received direction twice, ignoring\n", (void*)aSource);
		return;
	}
	
	dcassert(aSource->isSet(UserConnection::FLAG_DOWNLOAD) ^ aSource->isSet(UserConnection::FLAG_UPLOAD));
	if (dir == "Upload")
	{
		// Fine, the other fellow want's to send us data...make sure we really want that...
		if (aSource->isSet(UserConnection::FLAG_UPLOAD))
		{
			// Huh? Strange...disconnect...
			putConnection(aSource);
			return;
		}
	}
	else
	{
		if (aSource->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			int number = Util::toInt(num);
			// Damn, both want to download...the one with the highest number wins...
			if (aSource->getNumber() < number)
			{
				// Damn! We lost!
				aSource->unsetFlag(UserConnection::FLAG_DOWNLOAD);
				aSource->setFlag(UserConnection::FLAG_UPLOAD);
			}
			else if (aSource->getNumber() == number)
			{
				putConnection(aSource);
				return;
			}
		}
	}
	
	dcassert(aSource->isSet(UserConnection::FLAG_DOWNLOAD) ^ aSource->isSet(UserConnection::FLAG_UPLOAD));
	
	aSource->setState(UserConnection::STATE_KEY);
}

void ConnectionManager::setIP(UserConnection* conn, const ConnectionQueueItemPtr& qi)
{
	dcassert(conn);
	dcassert(conn->getUser());
	dcassert(qi);
	conn->getUser()->setIP(conn->getSocket()->getIp(), true);
}

void ConnectionManager::addDownloadConnection(UserConnection* conn)
{
	dcassert(conn->isSet(UserConnection::FLAG_DOWNLOAD));
	ConnectionQueueItemPtr cqi;
	bool l_is_active = false;
	{
		CFlyReadLock(*g_csDownloads);
		
		const auto i = find(g_downloads.begin(), g_downloads.end(), conn->getUser());
		if (i != g_downloads.end())
		{
			cqi = *i;
			l_is_active = true;
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
				l_is_active = false;
			}
		}
	}
	
	if (l_is_active)
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
		//CFlyWriteLock(*g_csUploads);
		CFlyLock(g_csUploads);
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

void ConnectionManager::on(UserConnectionListener::Key, UserConnection* aSource, const string&/* aKey*/) noexcept
{
	if (aSource->getState() != UserConnection::STATE_KEY)
	{
		dcdebug("CM::onKey Bad state, ignoring");
		return;
	}
	dcassert(aSource->getUser());
	if (aSource->isSet(UserConnection::FLAG_DOWNLOAD))
	{
		addDownloadConnection(aSource);
	}
	else
	{
		addUploadConnection(aSource);
	}
}

void ConnectionManager::on(AdcCommand::INF, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	if (aSource->getState() != UserConnection::STATE_INF)
	{
		// Already got this once, ignore...
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Expecting INF"));
		dcdebug("CM::onINF %p sent INF twice\n", (void*)aSource);
		aSource->disconnect();
		return;
	}
	
	string cid;
	if (!cmd.getParam("ID", 0, cid))
	{
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_INF_MISSING, "ID missing").addParam("FL", "ID"));
		dcdebug("CM::onINF missing ID\n");
		aSource->disconnect();
		return;
	}
	
	aSource->setUser(ClientManager::findUser(CID(cid)));
	
	if (!aSource->getUser())
	{
		dcdebug("CM::onINF: User not found");
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "User not found"));
		putConnection(aSource);
		return;
	}
	
	if (!checkKeyprint(aSource))
	{
		putConnection(aSource);
		return;
	}
	
	string token;
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		if (!cmd.getParam("TO", 0, token))
		{
			aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "TO missing"));
			putConnection(aSource);
			return;
		}
	}
	else
	{
		token = aSource->getUserConnectionToken();
	}
	
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		aSource->inf(false);
	}
	
	dcassert(!token.empty());
	bool down;
	{
		CFlyReadLock(*g_csDownloads);
		const auto i = find(g_downloads.begin(), g_downloads.end(), aSource->getUser());
		
		if (i != g_downloads.cend())
		{
			(*i)->setErrors(0);
			if ((*i)->getConnectionQueueToken() == token) // TODO - мутоное место Ник с рандомным числом никогда ведь не могут быть равны?
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
			const ConnectionQueueItem::Iter j = find(uploads.begin(), uploads.end(), aSource->getUser());
		
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
		aSource->setFlag(UserConnection::FLAG_DOWNLOAD);
		addDownloadConnection(aSource);
	}
	else
	{
		aSource->setFlag(UserConnection::FLAG_UPLOAD);
		addUploadConnection(aSource);
	}
}

void ConnectionManager::force(const UserPtr& aUser)
{
	CFlyReadLock(*g_csDownloads);
	
	const auto i = find(g_downloads.begin(), g_downloads.end(), aUser);
	if (i != g_downloads.end())
	{
#ifdef FLYLINKDC_USE_FORCE_CONNECTION
		// TODO унести из лока
		fly_fire1(ConnectionManagerListener::Forced(), *i);
#endif
		(*i)->setLastAttempt(0);
	}
}

bool ConnectionManager::checkKeyprint(UserConnection *aSource)
{
	if (!aSource->isSecure() || aSource->isTrusted())
		return true;
	const string l_kp = ClientManager::getStringField(aSource->getUser()->getCID(), aSource->getHubUrl(), "KP");
	return aSource->verifyKeyprint(l_kp, BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS));
}

void ConnectionManager::failed(UserConnection* aSource, const string& aError, bool protocolError)
{
	if (aSource->isSet(UserConnection::FLAG_ASSOCIATED))
	{
		HintedUser user;
		CFlyReasonItem reasonItem;
		string token;
		bool doFire = true;
		if (aSource->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			CFlyWriteLock(*g_csDownloads);
			auto i = find(g_downloads.begin(), g_downloads.end(), aSource->getUser());
			//dcassert(i != g_downloads.end());
			if (i == g_downloads.end())
			{
				dcassert(0);
				//CFlyServerJSON::pushError(5, "ConnectionManager::failed (i == g_downloads.end()) aError = " + aError);
			}
			else
			{
				ConnectionQueueItemPtr cqi = *i;
				user = cqi->getHintedUser();
				token = cqi->getConnectionQueueToken();
				cqi->setState(ConnectionQueueItem::WAITING);
				cqi->setLastAttempt(GET_TICK());
				cqi->setErrors(protocolError ? -1 : (cqi->getErrors() + 1));
				reasonItem.m_hinted_user = cqi->getHintedUser();
				reasonItem.m_reason = aError;
				reasonItem.m_token = cqi->getConnectionQueueToken();
				//!!! putCQI_L(cqi); не делаем отключение - теряем докачку https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1679
			}
		}
		else if (aSource->isSet(UserConnection::FLAG_UPLOAD))
		{
			{
				//CFlyWriteLock(*g_csUploads);
				CFlyLock(g_csUploads);
				auto i = find(g_uploads.begin(), g_uploads.end(), aSource->getUser());
				dcassert(i != g_uploads.end());
				if (i == g_uploads.end())
				{
					dcassert(0);
					// CFlyServerJSON::pushError(6, "ConnectionManager::failed (i == g_uploads.end()) aError = " + aError);
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
			// такого удаления нет в ApexDC++
			//if (!ClientManager::isBeforeShutdown())
			//{
			//  fly_fire3(ConnectionManagerListener::Removed(), aSource->getHintedUser(), l_is_download, token);
			//}
		}
		if (doFire && !ClientManager::isBeforeShutdown() && reasonItem.m_hinted_user.user)
		{
			fly_fire3(ConnectionManagerListener::FailedDownload(), reasonItem.m_hinted_user, reasonItem.m_reason, token);
		}
	}
	else
	{
		//dcassert(0);
	}
	putConnection(aSource);
}

void ConnectionManager::on(UserConnectionListener::Failed, UserConnection* aSource, const string& aError) noexcept
{
	failed(aSource, aError, false);
}

void ConnectionManager::on(UserConnectionListener::ProtocolError, UserConnection* aSource, const string& aError) noexcept
{
	failed(aSource, aError, true);
}

void ConnectionManager::disconnect(const UserPtr& aUser)
{
	CFlyReadLock(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); ++i)
	{
		UserConnection* uc = *i;
		if (uc->getUser() == aUser)
		{
			uc->disconnect(true);
			if (CMD_DEBUG_ENABLED()) 
				DETECTION_DEBUG("[ConnectionManager][disconnect] " + uc->getHintedUser().toString());
		}
	}
}

void ConnectionManager::disconnect(const UserPtr& aUser, bool isDownload) // [!] IRainman fix.
{
	CFlyReadLock(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); ++i)
	{
		UserConnection* uc = *i;
		dcassert(uc);
		if (uc->getUser() == aUser && uc->isSet((Flags::MaskType)(isDownload ? UserConnection::FLAG_DOWNLOAD : UserConnection::FLAG_UPLOAD)))
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
		CFlyFastLock(g_cs_update);
		g_users_for_update.clear();
	}
	
	disconnect();
	{
		CFlyReadLock(*g_csConnection);
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
			CFlyReadLock(*g_csConnection);
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
	// Сбрасываем рейтинг в базу пока не нашли причину почему тут остаются записи.
	{
		{
			CFlyReadLock(*g_csDownloads);
			for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
			{
				const ConnectionQueueItemPtr& cqi = *i;
				cqi->getUser()->flushRatio();
			}
		}
		{
			//CFlyReadLock(*g_csUploads);
			CFlyLock(g_csUploads);
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
			{
				const ConnectionQueueItemPtr& cqi = *i;
				cqi->getUser()->flushRatio();
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
	if (conn->getUser()) // 44 падения https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=48388
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

void ConnectionManager::setUploadLimit(const UserPtr& aUser, int lim)
{
	CFlyWriteLock(*g_csConnection);
	auto i = g_userConnections.begin();
	while (i != g_userConnections.end())
	{
		if ((*i)->state == UserConnection::STATE_UNUSED)
		{
			delete *i;
			g_userConnections.erase(i++);
			continue;
		}
		if ((*i)->isSet(UserConnection::FLAG_UPLOAD) && (*i)->getUser() == aUser)
			(*i)->setUploadLimit(lim);
		i++;
	}
}

void ConnectionManager::removeUnusedConnections()
{
	CFlyWriteLock(*g_csConnection);
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

#ifdef DEBUG_USER_CONNECTION
void ConnectionManager::dumpUserConnections()
{
	CFlyReadLock(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); i++)
		(*i)->dumpInfo();
}
#endif
