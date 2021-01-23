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
#include "NetworkUtil.h"

static const unsigned RETRY_CONNECTION_DELAY = 10;
static const unsigned CONNECTION_TIMEOUT = 50;

uint16_t ConnectionManager::g_ConnToMeCount = 0;
std::unique_ptr<RWLock> ConnectionManager::g_csConnection = std::unique_ptr<RWLock>(RWLock::create());
std::unique_ptr<RWLock> ConnectionManager::g_csDownloads = std::unique_ptr<RWLock>(RWLock::create());
//std::unique_ptr<RWLock> ConnectionManager::g_csUploads = std::unique_ptr<RWLock>(RWLock::create());
CriticalSection ConnectionManager::g_csUploads;

boost::unordered_set<UserConnection*> ConnectionManager::g_userConnections;
std::set<ConnectionQueueItemPtr> ConnectionManager::g_downloads; // TODO - сделать поиск по User?
std::set<ConnectionQueueItemPtr> ConnectionManager::g_uploads; // TODO - сделать поиск по User?

FastCriticalSection ConnectionManager::g_cs_update;
UserSet ConnectionManager::g_users_for_update;
bool ConnectionManager::g_shuttingDown = false;
TokenManager ConnectionManager::tokenManager;

string TokenManager::makeToken(int type, uint64_t expires) noexcept
{
	string token;
	LOCK(cs);
	do
	{
		token = Util::toString(Util::rand());
	}
	while (tokens.find(token) != tokens.end());
	tokens.insert(make_pair(token, TokenData{type, expires}));
	return token;
}

bool TokenManager::isToken(const string& token) const noexcept
{
	LOCK(cs);
	return tokens.find(token) != tokens.end();
}

bool TokenManager::addToken(const string& token, int type, uint64_t expires) noexcept
{
	LOCK(cs);
	auto p = tokens.find(token);
	if (p == tokens.end())
	{
		tokens.insert(make_pair(token, TokenData{type, expires}));
		return true;
	}
	if (p->second.type == type)
	{
		p->second.expires = expires;
		return true;
	}
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
		tokens.erase(p);
}

string TokenManager::getInfo() const noexcept
{
	uint64_t now = GET_TICK();
	LOCK(cs);
	string res;
	for (const auto& p : tokens)
	{		
		const auto& data = p.second;
		res += p.first;
		res += ": ";
		res += data.type == TYPE_UPLOAD ? "upload" : "download";
		if (data.expires != UINT64_MAX)
		{
			int64_t t = data.expires - now;
			if (t < 0) t = 0;
			res += " (";
			res += Util::toString(t/1000);
			res += ')';
		}
		res += '\n';
	}
	return res;
}

void TokenManager::removeExpired(uint64_t now) noexcept
{
	LOCK(cs);
	for (auto i = tokens.begin(); i != tokens.end();)
		if (now > i->second.expires)
			tokens.erase(i++);
		else
			i++;
}

bool ExpectedNmdcMap::add(const string& nick, const string& myNick, const string& hubUrl, const string& token, int encoding, uint64_t expires) noexcept
{
	bool isDownload = !token.empty();
	LOCK(cs);
	bool result = true;
	auto p = expectedConnections.equal_range(nick);
	auto it = expectedConnections.end();
	while (p.first != p.second)
	{
		const auto& data = p.first->second;
		bool download = !data.token.empty();
		if (data.hubUrl == hubUrl && download == isDownload) it = p.first;
		if (!data.waiting) result = false;
		p.first++;
	}
	if (it == expectedConnections.end())
		expectedConnections.insert(make_pair(nick, ExpectedData{myNick, hubUrl, token, encoding, ++id, expires, !result}));
	else if (result)
	{
		auto& data = it->second;
		data.waiting = false;
		data.token = token;
		data.encoding = encoding;
		data.expires = expires;
	}
	return result;
}

bool ExpectedNmdcMap::remove(const string& nick, ExpectedData& res, NextConnectionInfo& nci) noexcept
{
	LOCK(cs);
	auto p = expectedConnections.equal_range(nick);
	auto it = expectedConnections.end();
	uint64_t minId = UINT64_MAX;
	while (p.first != p.second)
	{
		auto& data = p.first->second;
		if (!data.waiting)
		{
			res = std::move(data);
			it = p.first;
		}
		else if (data.id < minId)
		{
			nci.hubUrl = data.hubUrl;
			nci.nick = Text::toUtf8(nick, data.encoding);
			nci.token = data.token;
			minId = data.id;
		}
		p.first++;
	}

	if (it == expectedConnections.end()) return false;
	expectedConnections.erase(it);
	return true;
}

bool ExpectedNmdcMap::removeToken(const string& token, NextConnectionInfo& nci) noexcept
{
	LOCK(cs);
	for (auto i = expectedConnections.begin(); i != expectedConnections.end(); ++i)
		if (i->second.token == token)
		{
			string nick = i->first;
			expectedConnections.erase(i);
			auto p = expectedConnections.equal_range(nick);
			uint64_t minId = UINT64_MAX;
			while (p.first != p.second)
			{
				auto& data = p.first->second;
				if (data.waiting && data.id < minId)
				{
					nci.hubUrl = data.hubUrl;
					nci.nick = Text::toUtf8(nick, data.encoding);
					nci.token = data.token;
					minId = data.id;
				}
				p.first++;
			}
			return true;
		}
	return false;
}

string ExpectedNmdcMap::getInfo() const noexcept
{
	string result;
	uint64_t now = GET_TICK();
	LOCK(cs);
	for (auto i = expectedConnections.begin(); i != expectedConnections.end(); ++i)
	{
		const auto& data = i->second;
		result += Text::toUtf8(i->first, data.encoding) + ": ";
		result += " id=" + Util::toString(data.id);
		result += " hub=" + data.hubUrl;
		result += " myNick=" + data.myNick;
		result += " encoding=" + Util::toString(data.encoding);
		if (!data.token.empty()) result += " token=" + data.token;
		result += " waiting=" + Util::toString(data.waiting);
		if (data.expires != UINT64_MAX)
		{
			int64_t t = data.expires - now;
			if (t < 0) t = 0;
			result += " expires=";
			result += Util::toString(t/1000);
		}
		result += '\n';
	}
	return result;
}

void ExpectedNmdcMap::removeExpired(uint64_t now, vector<NextConnectionInfo>& vnci) noexcept
{
	vector<string> tmp;
	LOCK(cs);
	for (auto i = expectedConnections.begin(); i != expectedConnections.end();)
		if (now > i->second.expires)
		{
			if (!i->second.waiting) tmp.push_back(i->first);
			expectedConnections.erase(i++);
		} else i++;
	NextConnectionInfo nci;
	for (const string& nick : tmp)
	{
		auto p = expectedConnections.equal_range(nick);
		uint64_t minId = UINT64_MAX;
		while (p.first != p.second)
		{
			const auto& data = p.first->second;
			if (data.waiting && data.id < minId)
			{
				nci.hubUrl = data.hubUrl;
				nci.nick = Text::toUtf8(nick, data.encoding);
				nci.token = data.token;
				minId = data.id;
			}
			p.first++;
		}
		if (minId != UINT64_MAX) vnci.push_back(nci);
	}
}

void ExpectedAdcMap::add(const string& token, const CID& cid, const string& hubUrl) noexcept
{
	LOCK(cs);
	expectedConnections.insert(make_pair(token, ExpectedData{cid, hubUrl}));
}

bool ExpectedAdcMap::remove(const string& token, ExpectedData& res) noexcept
{
	LOCK(cs);
	const auto i = expectedConnections.find(token);
	if (i == expectedConnections.end())
		return false;

	res = std::move(i->second);
	expectedConnections.erase(i);
	return true;
}

bool ExpectedAdcMap::removeToken(const string& token) noexcept
{
	LOCK(cs);
	const auto i = expectedConnections.find(token);
	if (i == expectedConnections.end())
		return false;
	expectedConnections.erase(i);
	return true;
}

string ExpectedAdcMap::getInfo() const noexcept
{
	string result;
	LOCK(cs);
	for (auto i = expectedConnections.begin(); i != expectedConnections.end(); ++i)
	{
		const auto& data = i->second;
		result += i->first + ": ";
		result += " CID=" + data.cid.toBase32();
		result += " hub=" + data.hubUrl;
		result += '\n';
	}
	return result;
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

void ConnectionManager::startListen(int type)
{
	uint16_t port = 0;
	string bind;
	bool autoDetectConnection = BOOLSETTING(AUTO_DETECT_CONNECTION);
	SettingsManager::IntSetting portSetting = type == SERVER_TYPE_SSL ? SettingsManager::TLS_PORT : SettingsManager::TCP_PORT;
	if (!autoDetectConnection)
	{
		bind = SETTING(BIND_ADDRESS);
		port = SettingsManager::get(portSetting);
	}

	auto newServer = new Server(type, port, bind);
	if (autoDetectConnection)
		SettingsManager::set(portSetting, newServer->getServerPort());
	if (type == SERVER_TYPE_SSL)
		secureServer = newServer;
	else
		server = newServer;
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
				auto cqi = std::make_shared<ConnectionQueueItem>(HintedUser(user, Util::emptyString), true,
					tokenManager.makeToken(TokenManager::TYPE_DOWNLOAD, UINT64_MAX));
				g_downloads.insert(cqi);
				if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][download] " + cqi->getHintedUser().toString());
			}
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
			else
			{
				if (find(checkIdle.begin(), checkIdle.end(), user) == checkIdle.end())
				{
					checkIdle.push_back(user); // TODO - Лок?
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
	tokenManager.removeToken(token);
	if (!ClientManager::isBeforeShutdown())
	{
		fly_fire1(ConnectionManagerListener::RemoveToken(), token);
	}
}

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

static inline unsigned getDelayFactor(int errorCount)
{
	if (errorCount <= 1) return 1;
	if (errorCount >= 4) return 8;
	return 1 << (errorCount-1);
}

void ConnectionManager::removeExpectedToken(const string& token)
{
	if (expectedAdc.removeToken(token)) return;
	ExpectedNmdcMap::NextConnectionInfo nci;
	expectedNmdc.removeToken(token, nci);
	if (!nci.hubUrl.empty()) connectNextNmdcUser(nci);
}

void ConnectionManager::connectNextNmdcUser(const ExpectedNmdcMap::NextConnectionInfo& nci)
{
	const CID cid = ClientManager::makeCid(nci.nick, nci.hubUrl);
	OnlineUserPtr u = ClientManager::findOnlineUser(cid, nci.hubUrl, true);
	if (u)
		u->getClientBase().connect(u, nci.token, false);
}

void ConnectionManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	updateAverageSpeed(tick);
	flushUpdatedUsers();
	std::vector<ConnectionQueueItemPtr> removed;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	UserList idleList;
#endif
	unsigned maxAttempts = SETTING(DOWNCONN_PER_SEC);
	if (!maxAttempts) maxAttempts = UINT_MAX;
	std::vector<TokenItem> statusChanged;
	std::vector<ReasonItem> downloadError;
	{
		READ_LOCK(*g_csDownloads);
		unsigned attempts = 0;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		idleList.swap(checkIdle);
#endif
		for (auto i = g_downloads.cbegin(); i != g_downloads.cend() && !ClientManager::isBeforeShutdown(); ++i)
		{
			const auto cqi = *i;
			if (cqi->getState() != ConnectionQueueItem::ACTIVE)
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

				QueueItem::Priority prio = QueueManager::hasDownload(cqi->getUser());
				if (prio == QueueItem::PAUSED)
				{
					removed.push_back(cqi);
					continue;
				}

				int errorCount = cqi->getErrors();
				if (cqi->getState() == ConnectionQueueItem::WAITING)
				{
					if (cqi->getLastAttempt() == 0 || (attempts < maxAttempts && cqi->getLastAttempt() + RETRY_CONNECTION_DELAY * 1000 * getDelayFactor(errorCount) < tick))
					{
						if (DownloadManager::isStartDownload(prio))
						{
							cqi->setLastAttempt(tick);
							cqi->setState(ConnectionQueueItem::CONNECTING);
							ClientManager::getInstance()->connect(cqi->getHintedUser(),
							                                      cqi->getConnectionQueueToken(),
							                                      false);
							statusChanged.emplace_back(TokenItem{cqi->getHintedUser(), cqi->getConnectionQueueToken()});
							attempts++;
						}
						else
						{
							cqi->setLastAttempt(tick);
							cqi->setState(ConnectionQueueItem::NO_DOWNLOAD_SLOTS);
							downloadError.emplace_back(ReasonItem{cqi->getHintedUser(), cqi->getConnectionQueueToken(), STRING(ALL_DOWNLOAD_SLOTS_TAKEN)});
						}
					}
				}
				else if (cqi->getState() == ConnectionQueueItem::NO_DOWNLOAD_SLOTS && DownloadManager::isStartDownload(prio))
				{
					cqi->setLastAttempt(tick);
					cqi->setState(ConnectionQueueItem::WAITING);
				}
				else if (cqi->getState() == ConnectionQueueItem::CONNECTING && cqi->getLastAttempt() + CONNECTION_TIMEOUT * 1000 < tick)
				{
					const string& token = cqi->getConnectionQueueToken();
					ClientManager::connectionTimeout(cqi->getUser());
					cqi->setErrors(cqi->getErrors() + 1);
					downloadError.emplace_back(ReasonItem{cqi->getHintedUser(), token, STRING(CONNECTION_TIMEOUT)});
					cqi->setLastAttempt(tick);
					cqi->setState(ConnectionQueueItem::WAITING);
					removeExpectedToken(token);
				}
			}
		}
	}
	if (!removed.empty())
	{
		for (ConnectionQueueItemPtr& cqi : removed)
		{
			bool isDownload = cqi->isDownload();
			const HintedUser hintedUser = cqi->getHintedUser();
			const string token = cqi->getConnectionQueueToken();
			if (isDownload)
			{
				removeExpectedToken(token);
				WRITE_LOCK(*g_csDownloads);
				putCQI_L(cqi);
			}
			else
			{
				LOCK(g_csUploads);
				putCQI_L(cqi);
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
	// TODO - не звать для тех у кого ошибка загрузки
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

void ConnectionManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	removeUnusedConnections();
	if (ClientManager::isBeforeShutdown())
		return;
	tokenManager.removeExpired(tick);
	vector<ExpectedNmdcMap::NextConnectionInfo> vnci;
	expectedNmdc.removeExpired(tick, vnci);
	for (const auto& nci : vnci)
		connectNextNmdcUser(nci);
	READ_LOCK(*g_csConnection);
	for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
	{
		auto& connection = *j;
		if ((connection->getLastActivity() + 60 * 1000) < tick)
			connection->disconnect(true);
	}
}

static const uint64_t g_FLOOD_TRIGGER = 20000;
static const uint64_t g_FLOOD_ADD = 2000;

ConnectionManager::Server::Server(int type, uint16_t port, const string& ipAddr): type(type), stopFlag(false), bindIp(ipAddr)
{
	sock.create();
	sock.setSocketOpt(SO_REUSEADDR, 1);
	LogManager::message("Starting to listen " + bindIp + ':' + Util::toString(port) + " type=" + Util::toString(type), false);
	serverPort = sock.bind(port, bindIp);
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
				serverPort = sock.bind(serverPort, bindIp);
				dcassert(serverPort);
				LogManager::message("Starting to listen " + bindIp + ':' + Util::toString(serverPort) + " type=" + Util::toString(type));
				sock.listen();
				if (type != SERVER_TYPE_SSL)
					ConnectionManager::getInstance()->updateLocalIp();
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
	unique_ptr<Socket> newSock;
	switch (type)
	{
		case SERVER_TYPE_AUTO_DETECT:
			newSock.reset(new AutoDetectSocket);
			break;
		case SERVER_TYPE_SSL:
			newSock.reset(new SSLSocket(CryptoManager::SSL_SERVER, true, Util::emptyString));
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
		if (type == SERVER_TYPE_SSL && server)
		{
			// FIXME: Is it possible to get FlyLink's magic string from SSL socket buffer?
			if (g_portTest.processInfo(PortTest::PORT_TLS, PortTest::PORT_TLS, server->getServerPort(), Util::emptyString, Util::emptyString, false))
				ConnectivityManager::getInstance()->processPortTestResult();
		}
		return;
	}
	UserConnection* uc = getConnection(false, type == SERVER_TYPE_SSL);
	uc->setFlag(UserConnection::FLAG_INCOMING);
	uc->setState(UserConnection::STATE_SUPNICK);
	uc->updateLastActivity();
	uc->addAcceptedSocket(newSock, port);
}

void ConnectionManager::updateLocalIp()
{
	string localIp = server ? server->getServerIP() : Util::emptyString;
	uint32_t addr;
	if (!(Util::parseIpAddress(addr, localIp) && addr)) localIp = Util::getLocalIp();
	ConnectivityManager::getInstance()->setLocalIP(localIp);
}

void ConnectionManager::nmdcConnect(const string& address, uint16_t port, const string& myNick, const string& hubUrl, int encoding, bool secure)
{
	nmdcConnect(address, port, 0, BufferedSocket::NAT_NONE, myNick, hubUrl, encoding, secure);
}

void ConnectionManager::nmdcConnect(const string& address, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& myNick, const string& hubUrl, int encoding, bool secure)
{
	if (isShuttingDown())
		return;

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

void ConnectionManager::disconnect() noexcept
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
#if 0
	if (source->isSecure() && !source->isTrusted() && !BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS))
	{
		putConnection(source);
		LogManager::message(STRING(CERTIFICATE_NOT_TRUSTED));
		return;
	}
#endif
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
		ExpectedNmdcMap::ExpectedData ed;
		ExpectedNmdcMap::NextConnectionInfo nci;
		if (!expectedNmdc.remove(nick, ed, nci))
		{
			LogManager::message("Unknown incoming connection from \"" + nick + '"', false);
			putConnection(source);
			return;
		}

		source->setUserConnectionToken(ed.myNick);
		source->setHubUrl(ed.hubUrl);
		source->setEncoding(ed.encoding);
		if (!nci.hubUrl.empty()) connectNextNmdcUser(nci);
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
			string token = conn->getConnectionQueueToken();
			if (!token.empty())
				tokenManager.addToken(token, TokenManager::TYPE_UPLOAD, UINT64_MAX);
			else
			{
				token = tokenManager.makeToken(TokenManager::TYPE_UPLOAD, UINT64_MAX);
				conn->setConnectionQueueToken(token);
			}
			conn->setFlag(UserConnection::FLAG_ASSOCIATED);
			cqi = std::make_shared<ConnectionQueueItem>(conn->getHintedUser(), false, token);
			g_uploads.insert(cqi);
			if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][upload] " + conn->getHintedUser().toString());
			cqi->setState(ConnectionQueueItem::ACTIVE);
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
	
	string cidStr;
	if (!cmd.getParam("ID", 0, cidStr))
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_INF_MISSING, "ID missing").addParam("FL", "ID"));
		dcdebug("CM::onINF missing ID\n");
		source->disconnect();
		return;
	}
	
	CID cid(cidStr);
	string token;
	if (source->isSet(UserConnection::FLAG_INCOMING))
	{
		if (!cmd.getParam("TO", 0, token))
		{
			source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "TO missing"));
			putConnection(source);
			return;
		}
		ExpectedAdcMap::ExpectedData ed;
		if (!expectedAdc.remove(token, ed) || ed.cid != cid)
		{
			source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "Connection not expected"));
			putConnection(source);
			return;
		}
		source->setConnectionQueueToken(token);
		source->setHubUrl(ed.hubUrl);
	}

	source->setUser(ClientManager::findUser(cid));
	
	if (!source->getUser())
	{
		dcdebug("CM::onINF: User not found");
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "User not found"));
		putConnection(source);
		return;
	}
	
	if (!checkKeyprint(source))
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "Keyprint validation failed"));
		putConnection(source);
		return;
	}
	
	if (source->isSet(UserConnection::FLAG_INCOMING))
		source->inf(false);
	else
		token = source->getUserConnectionToken();
	
	dcassert(!token.empty());
	bool down;
	{
		READ_LOCK(*g_csDownloads);
		const auto i = find(g_downloads.begin(), g_downloads.end(), token);
		if (i != g_downloads.cend())
		{
			(*i)->setErrors(0);
			down = true;
		}
		else
			down = false;
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
				//!!! putCQI_L(cqi); не делаем отключение - теряем докачку https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1679
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
			// такого удаления нет в ApexDC++
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
	// Сбрасываем рейтинг в базу пока не нашли причину почему тут остаются записи.
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

string ConnectionManager::getExpectedInfo()
{
	auto cm = ConnectionManager::getInstance();
	return cm->expectedNmdc.getInfo() + cm->expectedAdc.getInfo();
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

void ConnectionManager::fireListenerStarted() noexcept
{
	fly_fire(ConnectionManagerListener::ListenerStarted());
}

void ConnectionManager::fireListenerFailed(const char* type, int errorCode) noexcept
{
	fly_fire2(ConnectionManagerListener::ListenerFailed(), type, errorCode);
}
