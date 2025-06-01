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
#include "SettingsManager.h"
#include "SSLSocket.h"
#include "AutoDetectSocket.h"
#include "PortTest.h"
#include "IpTest.h"
#include "ConnectivityManager.h"
#include "LogManager.h"
#include "NmdcHub.h"
#include "NetworkUtil.h"
#include "NetworkDevices.h"
#include "SimpleStringTokenizer.h"
#include "HttpClient.h"
#include "AdcHub.h"
#include "Random.h"
#include "ConfCore.h"
#include "SettingsUtil.h"

#ifdef BL_FEATURE_IP_DATABASE
#include "DatabaseManager.h"
#include "DatabaseOptions.h"
#endif

#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
#include "TagCollector.h"
extern TagCollector collAdcFeatures;
#endif

static const unsigned RETRY_CONNECTION_DELAY = 10;
static const unsigned RECONNECT_AFTER_ERROR_DELAY = 30;
static const unsigned CONNECTION_TIMEOUT = 50;

static const unsigned UC_IDLE_TIME = 60;
static const unsigned UC_RETRY_TIME = 10;
static const unsigned CCPM_IDLE_TIME = 360; // 6 minutes
static const unsigned CCPM_KEEP_ALIVE_TIME = 180; // 3 minutes

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

void TokenManager::getList(vector<pair<string, TokenData>>& result) const noexcept
{
	result.clear();
	{
		LOCK(cs);
		for (const auto& p : tokens)
			result.emplace_back(p);
	}
}

void TokenManager::removeExpired(uint64_t now, StringList& ccpmTokens) noexcept
{
	LOCK(cs);
	for (auto i = tokens.begin(); i != tokens.end();)
	{
		TokenData& td = i->second;
		if (now > td.expires)
		{
			if (td.type == TYPE_CCPM) ccpmTokens.emplace_back(i->first);
			tokens.erase(i++);
		}
		else
			i++;
	}
}

int TokenManager::getTokenType(const string& token) const noexcept
{
	int type = -1;
	LOCK(cs);
	auto p = tokens.find(token);
	if (p != tokens.end())
		type = p->second.type;
	return type;
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

void ExpectedAdcMap::add(const string& token, const CID& cid, const string& hubUrl, uint64_t expires) noexcept
{
	LOCK(cs);
	auto p = expectedConnections.insert(make_pair(token, ExpectedData{cid, hubUrl, expires}));
	if (p.second) return;
	auto& val = p.first->second;
	if (val.hubUrl == hubUrl) val.expires = expires;
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
	uint64_t now = GET_TICK();
	LOCK(cs);
	for (auto i = expectedConnections.begin(); i != expectedConnections.end(); ++i)
	{
		const auto& data = i->second;
		result += i->first + ": ";
		result += " CID=" + data.cid.toBase32();
		result += " hub=" + data.hubUrl;
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

void ExpectedAdcMap::removeExpired(uint64_t now) noexcept
{
	LOCK(cs);
	for (auto i = expectedConnections.begin(); i != expectedConnections.end();)
		if (now > i->second.expires)
			expectedConnections.erase(i++);
		else
			i++;
}

static void modifyFeatures(StringList& features, const string& options)
{
	if (options.empty()) return;
	SimpleStringTokenizer<char> st(options, ' ');
	string tok;
	while (st.getNextNonEmptyToken(tok))
	{
		if (tok[0] == '-')
		{
			tok.erase(0, 1);
			auto i = std::find(features.begin(), features.end(), tok);
			if (i != features.end())
				features.erase(i);
		}
		else if (std::find(features.begin(), features.end(), tok) == features.end())
			features.push_back(tok);
	}
}

ConnectionManager::ConnectionManager() : shuttingDown(false),
	csConnections(RWLock::create()),
	csDownloads(RWLock::create())
{
	servers[0] = servers[1] = servers[2] = servers[3] = nullptr;
	ports[0] = ports[1] = 0;
	timeRemoveExpired = timeCheckConnections = 0;

	auto ss = SettingsManager::instance.getCoreSettings();
	nmdcFeatures.reserve(5);
	nmdcFeatures.push_back(UserConnection::FEATURE_MINISLOTS);
	nmdcFeatures.push_back(UserConnection::FEATURE_XML_BZLIST);
	nmdcFeatures.push_back(UserConnection::FEATURE_ADCGET);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHL);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHF);
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
	nmdcFeatures.push_back(UserConnection::FEATURE_BANMSG);
#endif

	adcFeatures.reserve(4);
	adcFeatures.push_back(UserConnection::FEATURE_ADC_BAS0);
	adcFeatures.push_back(UserConnection::FEATURE_ADC_BASE);
	adcFeatures.push_back(UserConnection::FEATURE_ADC_TIGR);
	adcFeatures.push_back(UserConnection::FEATURE_ADC_BZIP);

	ss->lockRead();
	modifyFeatures(nmdcFeatures, ss->getString(Conf::NMDC_FEATURES_CC));
	modifyFeatures(adcFeatures, ss->getString(Conf::ADC_FEATURES_CC));
	ss->unlockRead();

	TimerManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);
}

ConnectionManager::~ConnectionManager()
{
	dcassert(shuttingDown);
	dcassert(userConnections.empty());
	dcassert(downloads.empty());
	dcassert(uploads.empty());
}

void ConnectionManager::startListen(int af, int type)
{
	string bind;
	Conf::IPSettings ips;
	Conf::getIPSettings(ips, af == AF_INET6);
	int portSetting = type == SERVER_TYPE_SSL ? Conf::TLS_PORT : Conf::TCP_PORT;

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	int options = ss->getInt(ips.bindOptions);
	string bindDev = ss->getString(ips.bindDevice);
	string bindAddr = ss->getString(ips.bindAddress);
	bool autoDetect = ss->getBool(ips.autoDetect);
	uint16_t port = ss->getInt(portSetting);
	ss->unlockRead();

	IpAddressEx bindIp;
	bindIp.type = 0;
	if (!autoDetect)
	{
		if (options & Conf::BIND_OPTION_USE_DEV)
		{
			if (!bindDev.empty())
			{
				if (networkDevices.getDeviceAddress(af, bindDev, bindIp, true))
				{
					bindAddr = Util::printIpAddress(bindIp);
					ss->lockWrite();
					ss->setString(ips.bindAddress, bindAddr);
					ss->unlockWrite();
				}
				else
				{
					if (options & Conf::BIND_OPTION_NO_FALLBACK)
						throw SocketException(STRING_F(NETWORK_DEVICE_NOT_FOUND, bindDev));
					LogManager::message(STRING_F(NETWORK_DEVICE_NOT_FOUND, bindDev));
					ss->lockWrite();
					ss->unsetString(ips.bindAddress);
					ss->unlockWrite();
				}
			}
		}
		if (!bindIp.type)
			bind = std::move(bindAddr);
	}

	if (!bindIp.type) BufferedSocket::getBindAddress(bindIp, af, bind);
	auto newServer = new Server(type, bindIp, port);
	ss->lockWrite();
	ss->setInt(portSetting, newServer->getServerPort());
	ss->unlockWrite();
	int index = type == SERVER_TYPE_SSL ? SERVER_SECURE : 0;
	ports[index] = newServer->getServerPort();
	if (index == 0 && newServer->getType() == SERVER_TYPE_AUTO_DETECT)
		ports[1] = ports[index];
	if (af == AF_INET6)
		index |= SERVER_V6;
	servers[index] = newServer;
}

/**
 * Request a connection for downloading.
 * DownloadManager::addConnection will be called as soon as the connection is ready
 * for downloading.
 * @param user The user to connect to.
 */
void ConnectionManager::getDownloadConnection(const HintedUser& hintedUser)
{
	dcassert(hintedUser.user);
	ConnectionQueueItemPtr cqi;
	string existingToken;
	{
		WRITE_LOCK(*csDownloads);
		const auto i = find(downloads.begin(), downloads.end(), hintedUser.user);
		if (i == downloads.end())
		{
			cqi = std::make_shared<ConnectionQueueItem>(hintedUser, true,
				tokenManager.makeToken(TokenManager::TYPE_DOWNLOAD, UINT64_MAX));
			downloads.insert(cqi);
			if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][download] " + cqi->getHintedUser().toString());
		}
		else
			existingToken = (*i)->getConnectionQueueToken();
	}
	if (!existingToken.empty())
	{
		UserConnectionPtr ucPtr = findConnection(existingToken);
		if (ucPtr && ucPtr->getState() == UserConnection::STATE_IDLE)
		{
			DownloadManager::getInstance()->checkDownloads(ucPtr);
			return;
		}
	}
	if (cqi)
		fire(ConnectionManagerListener::Added(), hintedUser, true, cqi->getConnectionQueueToken());
}

void ConnectionManager::putCQI_L(ConnectionQueueItemPtr& cqi)
{
	if (cqi->isDownload())
	{
		downloads.erase(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][putCQI][download] " + cqi->getHintedUser().toString());
	}
	else
	{
		UploadManager::getInstance()->removeFinishedUpload(cqi->getUser());
		uploads.erase(cqi);
		if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][putCQI][upload] " + cqi->getHintedUser().toString());
	}
#ifdef BL_FEATURE_IP_DATABASE
	// cqi->getUser()->flushRatio(); // FIXME
#endif
	QueueManager::userQueue.removeRunning(cqi->getUser());
	tokenManager.removeToken(cqi->getConnectionQueueToken());
	cqi.reset();
}

ConnectionQueueItemPtr ConnectionManager::getDownloadCQI(const string& token) const noexcept
{
	READ_LOCK(*csDownloads);
	auto i = std::find(downloads.begin(), downloads.end(), token);
	return i == downloads.end() ? nullptr : *i;
}

ConnectionQueueItemPtr ConnectionManager::getUploadCQI(const string& token) const noexcept
{
	LOCK(csUploads);
	auto i = std::find(uploads.begin(), uploads.end(), token);
	return i == uploads.end() ? nullptr : *i;
}

UserConnectionPtr ConnectionManager::getConnection(int flags) noexcept
{
	dcassert(!shuttingDown);
	UserConnectionPtr uc = std::make_shared<UserConnection>();
	{
		WRITE_LOCK(*csConnections);
		userConnections.insert(make_pair(uc.get(), uc));
	}
	uc->setFlag(flags);
	return uc;
}

UserConnectionPtr ConnectionManager::findConnection(const UserConnection* conn) const noexcept
{
	READ_LOCK(*csConnections);
	auto i = userConnections.find(conn);
	return i == userConnections.end() ? UserConnectionPtr() : i->second;
}

UserConnectionPtr ConnectionManager::findConnection(const string& token) const noexcept
{
	READ_LOCK(*csConnections);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); ++i)
		if (i->second->getConnectionQueueToken() == token)
			return i->second;
	return UserConnectionPtr();
}

void ConnectionManager::putConnection(UserConnection* conn)
{
	const UserPtr& user = conn->getUser();
	bool ccpm = conn->isAnySet(UserConnection::FLAG_CCPM);
	if (ccpm && user)
		fire(ConnectionManagerListener::PMChannelDisconnected(), user->getCID());
	conn->disconnect(true);
	{
		WRITE_LOCK(*csConnections);
		auto i = userConnections.find(conn);
		if (i != userConnections.end())
		{
			UserConnectionPtr& uc = i->second;
			uc->download.reset();
			uc->upload.reset();
			uc->state = UserConnection::STATE_UNUSED;
		}
		if (ccpm && user)
		{
			auto j = ccpmConn.find(user->getCID());
			if (j != ccpmConn.end())
				ccpmConn.erase(j);
		}
	}
	if (CMD_DEBUG_ENABLED()) 
		DETECTION_DEBUG("[ConnectionManager][putConnection] " + conn->getHintedUser().toString());
}

void ConnectionManager::deleteConnection(UserConnectionPtr& conn)
{
	if (CMD_DEBUG_ENABLED()) 
		DETECTION_DEBUG("[ConnectionManager][deleteConnection] " + conn->getHintedUser().toString());
	{
		WRITE_LOCK(*csConnections);
		auto i = userConnections.find(conn.get());
		if (i != userConnections.end())
			userConnections.erase(i);
	}
}

void ConnectionManager::flushUpdatedUsers()
{
	UserSet users;
	{
		LOCK(csUpdatedUsers);
		users.swap(updatedUsers);
	}
	for (const auto& user : users)
		onUserUpdated(user);
}

void ConnectionManager::addUpdatedUser(const UserPtr& user)
{
	LOCK(csUpdatedUsers);
	updatedUsers.insert(user);
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
			READ_LOCK(*csDownloads);
			for (const auto& download : downloads)
			{
				if (download->getUser() == user) // todo - map
					downloadUsers.emplace_back(TokenItem{download->getHintedUser(), download->getConnectionQueueToken()});
			}
		}
		{
			LOCK(csUploads);
			for (const auto& upload : uploads)
			{
				if (upload->getUser() == user)  // todo - map
					uploadUsers.emplace_back(TokenItem{upload->getHintedUser(), upload->getConnectionQueueToken()});
			}
		}
		for (const auto& download : downloadUsers)
		{
			fire(ConnectionManagerListener::UserUpdated(), download.hintedUser, true, download.token);
		}
		for (const auto& upload : uploadUsers)
		{
			fire(ConnectionManagerListener::UserUpdated(), upload.hintedUser, false, upload.token);
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
		u->getClientBase()->connect(u, nci.token, false);
#ifdef DEBUG_NMDC_UC
	else
		LogManager::message("Expected user " + nci.nick + "/" + cid.toBase32()
			+ " on hub " + nci.hubUrl + " not found", false);
#endif
}

void ConnectionManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	updateAverageSpeed(tick);
	flushUpdatedUsers();
	std::vector<ConnectionQueueItemPtr> removed;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	unsigned maxAttempts = 	ss->getInt(Conf::DOWNCONN_PER_SEC);
	ss->unlockRead();
	if (!maxAttempts) maxAttempts = UINT_MAX;
	std::vector<TokenItem> statusChanged;
	std::vector<ReasonItem> downloadError;
	{
		READ_LOCK(*csDownloads);
		unsigned attempts = 0;
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
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

				QueueItem::Priority prio = QueueManager::hasDownload(cqi->getUser());
				if (prio == QueueItem::PAUSED)
				{
					removed.push_back(cqi);
					continue;
				}

				if (cqi->getErrors() == -1 && cqi->getLastAttempt() != 0) // protocol error
				{
					if (cqi->getLastAttempt() + RECONNECT_AFTER_ERROR_DELAY * 1000 < tick)
						cqi->setErrors(0);
					else
						continue;
				}

				int errorCount = cqi->getErrors();
				if (cqi->getState() == ConnectionQueueItem::WAITING)
				{
					if (cqi->getLastAttempt() == 0 || (attempts < maxAttempts && cqi->getLastAttempt() + RETRY_CONNECTION_DELAY * 1000 * getDelayFactor(errorCount) < tick))
					{
						if (DownloadManager::getInstance()->isStartDownload(prio))
						{
							cqi->setLastAttempt(tick);
							cqi->setState(ConnectionQueueItem::CONNECTING);
							OnlineUserPtr ou = ClientManager::getInstance()->connect(cqi->getHintedUser(), cqi->getConnectionQueueToken(), false);
							if (ou)
								cqi->setHubHint(ou->getClientBase()->getHubUrl());
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
				else if (cqi->getState() == ConnectionQueueItem::NO_DOWNLOAD_SLOTS && DownloadManager::getInstance()->isStartDownload(prio))
				{
					cqi->setLastAttempt(tick);
					cqi->setState(ConnectionQueueItem::WAITING);
				}
				else if (cqi->getState() == ConnectionQueueItem::CONNECTING && cqi->getLastAttempt() + CONNECTION_TIMEOUT * 1000 < tick)
				{
					const string& token = cqi->getConnectionQueueToken();
					int numErrors = cqi->getErrors() + 1;
					cqi->setErrors(numErrors);
					downloadError.emplace_back(ReasonItem{cqi->getHintedUser(), token, STRING(CONNECTION_TIMEOUT)});
					cqi->setLastAttempt(tick);
					cqi->setState(ConnectionQueueItem::WAITING);
					removeExpectedToken(token);
					QueueManager::getInstance()->userCheckProcessFailure(cqi->getUser(), numErrors, true);
#ifdef DEBUG_NMDC_UC
					LogManager::message("Connection timed out: user=" + cqi->getHintedUser().user->getLastNick() +
						" token=" + cqi->getConnectionQueueToken() +
						" errors=" + Util::toString(cqi->getErrors()), false);
#endif
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
				WRITE_LOCK(*csDownloads);
				putCQI_L(cqi);
			}
			else
			{
				LOCK(csUploads);
				putCQI_L(cqi);
			}
			fire(ConnectionManagerListener::RemoveToken(), token);
			fire(ConnectionManagerListener::Removed(), hintedUser, isDownload, token);
		}
		removed.clear();
	}
	
	for (auto j = statusChanged.cbegin(); j != statusChanged.cend(); ++j)
		fire(ConnectionManagerListener::ConnectionStatusChanged(), j->hintedUser, true, j->token);
	statusChanged.clear();
	// TODO - не звать дл€ тех у кого ошибка загрузки
	for (auto k = downloadError.cbegin(); k != downloadError.cend(); ++k)
		fire(ConnectionManagerListener::FailedDownload(), k->hintedUser, k->reason, k->token);
	downloadError.clear();

	if (tick > timeRemoveExpired)
	{
		timeRemoveExpired = tick + 60000;
		StringList ccpmTokens;
		tokenManager.removeExpired(tick, ccpmTokens);
		vector<ExpectedNmdcMap::NextConnectionInfo> vnci;
		expectedNmdc.removeExpired(tick, vnci);
		expectedAdc.removeExpired(tick);
		if (!ClientManager::isBeforeShutdown())
		{
			for (const string& token : ccpmTokens)
				removeExpiredCCPMToken(token);
			for (const auto& nci : vnci)
				connectNextNmdcUser(nci);
		}
	}
	if (tick > timeCheckConnections)
	{
		timeCheckConnections = tick + 15000;
		checkConnections(tick);
	}
}

void ConnectionManager::checkConnections(uint64_t tick)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	unsigned ccpmIdleTimeout = (unsigned) ss->getInt(Conf::CCPM_IDLE_TIMEOUT) * 60000;
	ss->unlockRead();

	WRITE_LOCK(*csConnections);
	for (auto j = userConnections.cbegin(); j != userConnections.cend();)
	{
		UserConnection* uc = j->second.get();
		if (uc->state == UserConnection::STATE_UNUSED)
		{
			j = userConnections.erase(j);
			continue;
		}
		uint64_t lastActivity = uc->getLastActivity();
		if (lastActivity)
		{
			if (uc->isAnySet(UserConnection::FLAG_CCPM))
			{
				if (lastActivity + CCPM_IDLE_TIME * 1000 < tick ||
					(ccpmIdleTimeout && uc->getLastMessageActivity() && uc->getLastMessageActivity() + ccpmIdleTimeout < tick))
				{
					uc->disconnect();
				}
				else if (lastActivity + CCPM_KEEP_ALIVE_TIME * 1000 < tick)
				{
					AdcCommand c(AdcCommand::CMD_PMI);
					c.addParam("\n");
					uc->send(c);
				}
			}
			else if (uc->getState() == UserConnection::STATE_TRY_AGAIN && lastActivity + UC_RETRY_TIME * 1000 < tick)
				DownloadManager::getInstance()->checkDownloads(j->second);
			else if (lastActivity + UC_IDLE_TIME * 1000 < tick)
				uc->disconnect(true);
			else if (uc->getState() == UserConnection::STATE_IDLE)
				DownloadManager::getInstance()->checkDownloads(j->second);
		}
		++j;
	}
}

ConnectionManager::Server::Server(int type, const IpAddressEx& ip, uint16_t port): type(type), bindIp(ip), stopFlag(false)
{
	sock.create(ip.type, Socket::TYPE_TCP);
	sock.setSocketOpt(SOL_SOCKET, SO_REUSEADDR, 1);
	LogManager::message("Starting server on " + Util::printIpAddress(ip, true) + ':' + Util::toString(port) + " type=" + Util::toString(type), false);
	serverPort = sock.bind(port, bindIp);
	sock.listen();
	char threadName[64];
	sprintf(threadName, "Server-%d-v%d", type, ip.type == AF_INET6 ? 6 : 4);
	start(64, threadName);
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
				auto ret = sock.wait(POLL_TIMEOUT, Socket::WAIT_ACCEPT);
				if (ret == Socket::WAIT_ACCEPT)
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
				dcassert(bindIp.type == AF_INET || bindIp.type == AF_INET6);
				sock.disconnect();
				sock.create(bindIp.type, Socket::TYPE_TCP);
				serverPort = sock.bind(serverPort, bindIp);
				dcassert(serverPort);
				LogManager::message("Starting to listen " + Util::printIpAddress(bindIp, true) + ':' + Util::toString(serverPort) + " type=" + Util::toString(type));
				sock.listen();
				if (type != SERVER_TYPE_SSL)
					ConnectionManager::getInstance()->updateLocalIp(bindIp.type);
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
	int mask = 0;
	if (type == SERVER_TYPE_TCP || type == SERVER_TYPE_AUTO_DETECT)
		mask |= 1<<AppPorts::PORT_TCP;
	if (type == SERVER_TYPE_SSL || type == SERVER_TYPE_AUTO_DETECT)
		mask |= 1<<AppPorts::PORT_TLS;
	g_portTest.resetState(mask);
	return 0;
}

/**
 * Someone's connecting, accept the connection and wait for identification...
 * It's always the other fellow that starts sending if he made the connection.
 */
void ConnectionManager::accept(const Socket& sock, int type, Server* server) noexcept
{
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
			if (g_portTest.processInfo(AppPorts::PORT_TLS, AppPorts::PORT_TLS, getConnectionPort(AF_INET, true), Util::emptyString, Util::emptyString, false))
				ConnectivityManager::getInstance()->processPortTestResult();
		}
		return;
	}
	UserConnectionPtr ucPtr = getConnection(type == SERVER_TYPE_SSL ? UserConnection::FLAG_SECURE : 0);
	UserConnection* uc = ucPtr.get();
	uc->setFlag(UserConnection::FLAG_INCOMING);
	uc->setState(UserConnection::STATE_SUPNICK);
	uc->updateLastActivity();
	uc->addAcceptedSocket(newSock, port);
}

void ConnectionManager::updateLocalIp(int af)
{
	int index = af == AF_INET6 ? SERVER_V6 : 0;
	IpAddress localIp;
	memset(&localIp, 0, sizeof(localIp));
	if (servers[index])
		localIp = servers[index]->getServerIP();
	if (!Util::isValidIp(localIp))
	{
		IpAddressEx ip = Util::getLocalIp(af);
		memcpy(&localIp, &ip, sizeof(IpAddress));
	}
	ConnectivityManager::getInstance()->setLocalIP(localIp);
}

void ConnectionManager::nmdcConnect(const IpAddress& address, uint16_t port, const string& myNick, const string& hubUrl, int encoding, bool secure)
{
	nmdcConnect(address, port, 0, BufferedSocket::NAT_NONE, myNick, hubUrl, encoding, secure);
}

void ConnectionManager::nmdcConnect(const IpAddress& address, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& myNick, const string& hubUrl, int encoding, bool secure)
{
	if (shuttingDown)
		return;

	UserConnectionPtr ucPtr = getConnection((secure ? UserConnection::FLAG_SECURE : 0) | UserConnection::FLAG_NMDC);
	UserConnection* uc = ucPtr.get();
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
		deleteConnection(ucPtr);
	}
}

void ConnectionManager::adcConnect(const OnlineUser& user, uint16_t port, const string& token, bool secure)
{
	adcConnect(user, 0, port, 0, BufferedSocket::NAT_NONE, token, secure);
}

void ConnectionManager::adcConnect(const OnlineUser& user, int af, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& token, bool secure)
{
	if (shuttingDown)
		return;

	IpAddress ip;
	if (af == AF_INET)
	{
		Ip4Address ip4 = user.getIdentity().getIP4();
		if (!Util::isValidIp4(ip4)) return;
		ip.type = AF_INET;
		ip.data.v4 = ip4;
	}
	else if (af == AF_INET6)
	{
		if (!ConnectivityManager::hasIP6()) return;
		Ip6Address ip6 = user.getIdentity().getIP6();
		if (!Util::isValidIp6(ip6)) return;
		ip.type = AF_INET6;
		ip.data.v6 = ip6;
	}
	else
	{
		ip = user.getIdentity().getConnectIP();
		if (!ip.type) return;
	}

	UserConnectionPtr ucPtr = getConnection(secure ? UserConnection::FLAG_SECURE : 0);
	UserConnection* uc = ucPtr.get();
	uc->setUserConnectionToken(token);
	uc->setEncoding(Text::CHARSET_UTF8);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setHubUrl(user.getClientBase()->getHubUrl());
	if (!token.empty())
	{
		int type = tokenManager.getTokenType(token);
		if (type == TokenManager::TYPE_CCPM)
			uc->setFlag(UserConnection::FLAG_CCPM);
	}
	try
	{
		uc->connect(ip, port, localPort, natRole);
	}
	catch (const Exception&)
	{
		deleteConnection(ucPtr);
	}
}

int ConnectionManager::adcConnectHbri(const IpAddress& addr, uint16_t port, const string& token, bool secure)
{
	if (shuttingDown)
		return 0;

	UserConnectionPtr ucPtr = getConnection((secure ? UserConnection::FLAG_SECURE : 0) | UserConnection::FLAG_HBRI);
	UserConnection* uc = ucPtr.get();
	uc->setUserConnectionToken(token);
	uc->setEncoding(Text::CHARSET_UTF8);
	uc->setState(UserConnection::STATE_CONNECT);
	try
	{
		uc->connect(addr, port, 0, BufferedSocket::NAT_NONE);
	}
	catch (const Exception&)
	{
		deleteConnection(ucPtr);
		return 0;
	}
	return uc->getId();
}

void ConnectionManager::stopServer(int af) noexcept
{
	int index = af == AF_INET6 ? SERVER_V6 : 0;
	for (int i = 0; i < 2; ++i)
	{
		delete servers[index + i];
		servers[index + i] = nullptr;
	}
	bool hasServer = false;
	for (int i = 0; i < 4; ++i)
		if (servers[i])
		{
			hasServer = true;
			break;
		}
	if (!hasServer)
		ports[0] = ports[1] = 0;
}

void ConnectionManager::processMyNick(UserConnection* source, const string& nick) noexcept
{
	dcassert(!nick.empty());
	dcdebug("ConnectionManager::onMyNick %p, %s\n", (void*) source, nick.c_str());
	dcassert(!source->getUser());

	string hubUrl;
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
		hubUrl = std::move(ed.hubUrl);
	}
	else
		hubUrl = source->getHubUrl();
	
	const string nickUtf8 = Text::toUtf8(nick, source->getEncoding());
	const CID cid = ClientManager::makeCid(nickUtf8, hubUrl);
	
	// First, we try looking in the pending downloads...hopefully it's one of them...
	if (!ClientManager::isBeforeShutdown())
	{
		READ_LOCK(*csDownloads);
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		{
			const ConnectionQueueItemPtr& cqi = *i;
			if ((cqi->getState() == ConnectionQueueItem::CONNECTING || cqi->getState() == ConnectionQueueItem::WAITING) &&
			    cqi->getUser()->getCID() == cid)
			{
				cqi->setErrors(0);
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
		if (!source->getUser())
		{
			LogManager::message("Upload: unknown user " + nickUtf8 + '/' + cid.toBase32() + (hubUrl.empty() ? Util::emptyString : " (" + hubUrl + ")"), false);
			putConnection(source);
			return;
		}
		if (!source->getUser()->isOnline())
		{
			LogManager::message("Upload: offline user " + nickUtf8 + '/' + cid.toBase32() + (hubUrl.empty() ? Util::emptyString : " (" + hubUrl + ")"), false);
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
		READ_LOCK(*csDownloads);
		const auto i = find(downloads.begin(), downloads.end(), conn->getUser());
		if (i != downloads.end())
		{
			cqi = *i;
			isActive = true;
			conn->setConnectionQueueToken(cqi->getConnectionQueueToken());
			if (cqi->getState() == ConnectionQueueItem::WAITING || cqi->getState() == ConnectionQueueItem::CONNECTING)
			{
				cqi->setState(ConnectionQueueItem::ACTIVE);
				conn->setFlag(UserConnection::FLAG_ASSOCIATED);
				
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
				fire(ConnectionManagerListener::Connected(), cqi);
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
		UserConnectionPtr ucPtr = findConnection(conn);
		if (ucPtr)
		{
			DownloadManager::getInstance()->addConnection(ucPtr);
			setIP(conn, cqi);
		}
		else
			isActive = false;
	}
	if (!isActive)
		putConnection(conn);
}

void ConnectionManager::addUploadConnection(UserConnection* conn)
{
	dcassert(conn->isSet(UserConnection::FLAG_UPLOAD));
	ConnectionQueueItemPtr cqi;
	{
		//WRITE_LOCK(*csUploads);
		LOCK(csUploads);
		const auto i = find(uploads.begin(), uploads.end(), conn->getUser());
		if (i == uploads.cend())
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
			uploads.insert(cqi);
			if (CMD_DEBUG_ENABLED()) DETECTION_DEBUG("[ConnectionManager][getCQI][upload] " + conn->getHintedUser().toString());
			cqi->setState(ConnectionQueueItem::ACTIVE);
		}
	}
	if (cqi)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			fire(ConnectionManagerListener::Added(), cqi->getHintedUser(), false, cqi->getConnectionQueueToken());
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
			fire(ConnectionManagerListener::Connected(), cqi);
#endif
		}
		UploadManager::getInstance()->addConnection(conn);
		setIP(conn, cqi);
#ifdef DEBUG_USER_CONNECTION
		if (LogManager::getLogOptions() & LogManager::OPT_LOG_SOCKET_INFO)
		{
			const UserPtr& user = conn->getUser();
			LogManager::message("UserConnection(" + Util::toString(conn->id) +
				"): Using upload token " + conn->getConnectionQueueToken() + " for user=" +
				user->getLastNick() + ", CID=" + user->getCID().toBase32(), false);
		}
#endif
	}
	else
	{
#ifdef DEBUG_USER_CONNECTION
		if (LogManager::getLogOptions() & LogManager::OPT_LOG_SOCKET_INFO)
		{
			const UserPtr& user = conn->getUser();
			LogManager::message("UserConnection(" + Util::toString(conn->id) +
				"): Upload token not found, user=" + user->getLastNick() + ", CID=" + user->getCID().toBase32(), false);
		}
#endif
		putConnection(conn);
	}
}

void ConnectionManager::processKey(UserConnection* source) noexcept
{
	dcassert(source->getUser());
	if (source->isSet(UserConnection::FLAG_DOWNLOAD))
		addDownloadConnection(source);
	else
		addUploadConnection(source);
}

void ConnectionManager::processINF(UserConnection* source, const AdcCommand& cmd) noexcept
{
	string cidStr;
	if (!cmd.getParam(TAG('I', 'D'), 0, cidStr))
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_INF_MISSING, "ID missing").addParam(TAG('F', 'L'), "ID"));
		source->disconnect();
		return;
	}

	CID cid(cidStr);
	string token;
	if (source->isSet(UserConnection::FLAG_INCOMING))
	{
		if (!cmd.getParam(TAG('T', 'O'), 0, token))
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
		int type = tokenManager.getTokenType(token);
		if (type == TokenManager::TYPE_CCPM)
		{
			tokenManager.removeToken(token);
			source->setFlag(UserConnection::FLAG_CCPM);
		}
	}
	else
		token = source->getUserConnectionToken();

	source->setUser(ClientManager::findUser(cid));

	if (!source->getUser())
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "User not found"));
		putConnection(source);
		return;
	}

#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
	string& unknownFeatures = source->getUnknownFeatures();
	if (!unknownFeatures.empty())
	{
		string sourceName = source->getUser()->getLastNick();
		const string& hubUrl = source->getHubUrl();
		if (!hubUrl.empty())
		{
			sourceName += '\t';
			sourceName += hubUrl;
		}
		SimpleStringTokenizer<char> st(unknownFeatures, ' ');
		string tok;
		while (st.getNextToken(tok))
			collAdcFeatures.addTag(tok, sourceName);
		unknownFeatures.clear();
		unknownFeatures.shrink_to_fit();
	}
#endif

	if (!checkKeyprint(source))
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "Keyprint validation failed"));
		putConnection(source);
		return;
	}

	if (cmd.hasFlag(TAG('P', 'M'), 0))
		source->setFlag(UserConnection::FLAG_CCPM);
	if (source->isSet(UserConnection::FLAG_CCPM))
	{
		bool cpmiSupported = source->isSet(UserConnection::FLAG_SUPPORTS_CPMI);
		const CID& cid = source->getUser()->getCID();
		bool result = true;
		{
			WRITE_LOCK(*csConnections);
			auto i = ccpmConn.find(cid);
			if (i == ccpmConn.end())
			{
				if (token.empty())
					result = false;
				else
					ccpmConn.insert(make_pair(cid, PMConnInfo{ source, token, cpmiSupported, false, false, 0 }));
			}
			else
			{
				auto& ci = i->second;
				if (!ci.uc)
				{
					ci.uc = source;
					ci.cpmiSupported = cpmiSupported;
				}
				else
					result = false;
			}
		}
		if (!result)
		{
			// TODO: send error
			putConnection(source);
			return;
		}
		if (source->isSet(UserConnection::FLAG_INCOMING))
			source->inf(false);
		source->setState(UserConnection::STATE_IDLE);
		source->setLastMessageActivity(GET_TICK());
		fire(ConnectionManagerListener::PMChannelConnected(), cid, cpmiSupported);
		return;
	}

	if (source->isSet(UserConnection::FLAG_INCOMING))
		source->inf(false);

	dcassert(!token.empty());
	bool down;
	{
		READ_LOCK(*csDownloads);
		const auto i = find(downloads.begin(), downloads.end(), token);
		if (i != downloads.cend())
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

void ConnectionManager::processMSG(UserConnection* source, const AdcCommand& cmd) noexcept
{
	if (cmd.getParameters().empty())
		return;
	if (!source->getUser())
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "User not found"));
		putConnection(source);
		return;
	}
	if (!source->isAnySet(UserConnection::FLAG_CCPM))
	{
		source->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "Unsupported command"));
		putConnection(source);
		return;
	}

	const CID& cid = source->getUser()->getCID();
	OnlineUserList ouList;
	ClientManager::findOnlineUsers(cid, ouList, ClientBase::TYPE_ADC);
	if (ouList.empty())
	{
		// got CCPM message from an offline user
		return;
	}
	auto ou = ouList.front();

	if (cmd.getCommand() == AdcCommand::CMD_PMI)
	{
		string valTyping, valSeen, valQuit;
		bool resTyping = cmd.getParam(TAG('T', 'P'), 0, valTyping);
		bool resSeen = cmd.getParam(TAG('S', 'N'), 0, valSeen);
		bool resQuit = cmd.getParam(TAG('Q', 'U'), 0, valQuit);
		CPMINotification info;
		bool notify = false;
		if (resTyping || resSeen || resQuit)
		{
			uint64_t now = GET_TICK();
			{
				WRITE_LOCK(*csConnections);
				auto i = ccpmConn.find(cid);
				if (i != ccpmConn.end())
				{
					notify = true;
					auto& data = i->second;
					if (valQuit == "1")
						data.isClosed = true;
					if (resTyping)
					{
						data.isTyping = valTyping == "1";
						data.isClosed = false;
						info.isTyping = data.isTyping ? 1 : 0;
					}
					else
						info.isTyping = -1;
					if (valSeen == "1")
					{
						data.seenTime = now;
						data.isClosed = false;
					}
					info.seenTime = data.seenTime;
					info.isClosed = data.isClosed;
				}
			}
		}
		if (notify) fire(ConnectionManagerListener::CPMIReceived(), cid, info);
		return;
	}

	source->setLastMessageActivity(GET_TICK());
	ClientBase* clientBase = ou->getClientBase().get();
	{
		WRITE_LOCK(*csConnections);
		auto i = ccpmConn.find(cid);
		if (i != ccpmConn.end())
		{
			auto& data = i->second;
			data.isTyping = false;
			data.isClosed = false;
		}
	}
	static_cast<AdcHub*>(clientBase)->processCCPMMessage(cmd, ou);
}

void ConnectionManager::force(const UserPtr& user)
{
	READ_LOCK(*csDownloads);
	
	const auto i = find(downloads.begin(), downloads.end(), user);
	if (i != downloads.end())
	{
#ifdef FLYLINKDC_USE_FORCE_CONNECTION
		// TODO унести из лока
		fire(ConnectionManagerListener::Forced(), *i);
#endif
		(*i)->setLastAttempt(0);
	}
}

bool ConnectionManager::checkKeyprint(UserConnection* source)
{
	if (!source->isSecure() || source->isTrusted())
		return true;
	const string kp = ClientManager::getStringField(source->getUser()->getCID(), source->getHubUrl(), "KP");
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool allowUntrusted = ss->getBool(Conf::ALLOW_UNTRUSTED_CLIENTS);
	ss->unlockRead();
	return source->verifyKeyprint(kp, allowUntrusted);
}

void ConnectionManager::failed(UserConnection* source, const string& error, bool protocolError)
{
	if (source->isSet(UserConnection::FLAG_ASSOCIATED))
	{
		HintedUser user;
		ReasonItem reasonItem;
		string token;
		bool fireFailedDownload = true;
		if (source->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			WRITE_LOCK(*csDownloads);
			auto i = find(downloads.begin(), downloads.end(), source->getUser());
			if (i == downloads.end())
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
			}
		}
		else if (source->isSet(UserConnection::FLAG_UPLOAD))
		{
			string token;
			{
				LOCK(csUploads);
				auto i = find(uploads.begin(), uploads.end(), source->getUser());
				dcassert(i != uploads.end());
				if (i == uploads.end())
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
			if (!token.empty())
				fire(ConnectionManagerListener::RemoveToken(), token);
			fireFailedDownload = false;
		}
		if (fireFailedDownload && reasonItem.hintedUser.user)
			fire(ConnectionManagerListener::FailedDownload(), reasonItem.hintedUser, reasonItem.reason, token);
	}
	putConnection(source);
}

void ConnectionManager::disconnect(const UserPtr& user)
{
	READ_LOCK(*csConnections);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); ++i)
	{
		UserConnection* uc = i->second.get();
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
	READ_LOCK(*csConnections);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); ++i)
	{
		UserConnection* uc = i->second.get();
		dcassert(uc);
		if (uc->getUser() == user && uc->isSet((Flags::MaskType)(isDownload ? UserConnection::FLAG_DOWNLOAD : UserConnection::FLAG_UPLOAD)))
		{
			uc->disconnect(true);
			if (CMD_DEBUG_ENABLED()) 
				DETECTION_DEBUG("[ConnectionManager][disconnect] " + uc->getHintedUser().toString());
		}
	}
}

bool ConnectionManager::connectCCPM(const HintedUser& hintedUser)
{
	const CID& cid = hintedUser.user->getCID();
	OnlineUserPtr ou = ClientManager::findOnlineUser(cid, hintedUser.hint, true);
	if (!ou) return false;
	ClientBasePtr& client = ou->getClientBase();
	if (client->getType() != ClientBase::TYPE_ADC) return false;
	bool result;
	uint64_t expires = GET_TICK() + 60000;
	string token = tokenManager.makeToken(TokenManager::TYPE_CCPM, expires);
	{
		WRITE_LOCK(*csConnections);
		auto i = ccpmConn.insert(make_pair(cid, PMConnInfo{ nullptr, token, false, false, false, 0 }));
		result = i.second;
	}
	if (!result)
	{
		tokenManager.removeToken(token);
		return false;
	}
	client->connect(ou, token, false);
	return true;
}

bool ConnectionManager::disconnectCCPM(const CID& cid)
{
	bool result = false;
	UserConnection* uc = nullptr;
	string token;
	{
		WRITE_LOCK(*csConnections);
		auto i = ccpmConn.find(cid);
		if (i != ccpmConn.end())
		{
			result = true;
			uc = i->second.uc;
			token = std::move(i->second.token);
			ccpmConn.erase(i);
		}
	}
	if (!token.empty())
		tokenManager.removeToken(token);
	if (uc)
		uc->disconnect();
	return result;
}

void ConnectionManager::getCCPMState(const CID& cid, PMConnState& s) const
{
	READ_LOCK(*csConnections);
	auto i = ccpmConn.find(cid);
	if (i == ccpmConn.end())
	{
		s.state = CCPM_STATE_DISCONNECTED;
		s.cpmiSupported = false;
		s.cpmi.isTyping = -1;
		s.cpmi.isClosed = false;
		s.cpmi.seenTime = 0;
		return;
	}
	const auto& data = i->second;
	s.state = data.uc ? CCPM_STATE_CONNECTED : CCPM_STATE_CONNECTING;
	s.cpmiSupported = data.cpmiSupported;
	s.cpmi.isTyping = data.isTyping ? 1 : 0;
	s.cpmi.isClosed = data.isClosed;
	s.cpmi.seenTime = data.seenTime;
}

void ConnectionManager::removeExpiredCCPMToken(const string& token)
{
	bool found = false;
	CID cid;
	{
		WRITE_LOCK(*csConnections);
		for (auto i = ccpmConn.begin(); i != ccpmConn.end(); ++i)
		{
			PMConnInfo& ci = i->second;
			if (ci.token == token)
			{
				if (!ci.uc)
				{
					cid = i->first;
					found = true;
					ccpmConn.erase(i);
				}
				break;
			}
		}
	}
	if (found)
		fire(ConnectionManagerListener::PMChannelDisconnected(), cid);
}

bool ConnectionManager::sendCCPMMessage(const HintedUser& hintedUser, const string& text, int flags)
{
	OnlineUserPtr ou = ClientManager::findOnlineUser(hintedUser.user->getCID(), hintedUser.hint, false);
	if (!ou) return false;
	return sendCCPMMessage(ou, text, flags);
}

bool ConnectionManager::sendCCPMMessage(const OnlineUserPtr& ou, const string& text, int flags)
{
	ClientBase* clientBase = ou->getClientBase().get();
	if (clientBase->getType() != ClientBase::TYPE_ADC) return false;
	AdcCommand cmd(AdcCommand::CMD_MSG);
	cmd.addParam(text);
	if (flags & ClientBase::PM_FLAG_THIRD_PERSON) cmd.addParam("ME1");
	if (!sendCCPMMessage(ou->getUser()->getCID(), cmd)) return false;
	static_cast<AdcHub*>(clientBase)->fireOutgoingPM(ou, text, flags);
	return true;
}

bool ConnectionManager::sendCCPMMessage(const CID& cid, AdcCommand& cmd)
{
	string message = cmd.toString(0);
	READ_LOCK(*csConnections);
	auto i = ccpmConn.find(cid);
	if (i == ccpmConn.end()) return false;
	const auto& ci = i->second;
	if (!ci.uc) return false;
	if (cmd.getCommand() == AdcCommand::CMD_MSG)
		ci.uc->setLastMessageActivity(GET_TICK());
	ci.uc->send(message);
	return true;
}

void ConnectionManager::stopServers()
{
	stopServer(AF_INET);
	stopServer(AF_INET6);
}

void ConnectionManager::shutdown()
{
	dcassert(!shuttingDown);
	shuttingDown = true;
	g_portTest.shutdown();
	g_ipTest.shutdown();
	removeUnusedConnections();
	httpClient.shutdown();
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	{
		LOCK(csUpdatedUsers);
		updatedUsers.clear();
	}
	
	stopServers();
	{
		READ_LOCK(*csConnections);
		for (auto j = userConnections.cbegin(); j != userConnections.cend(); ++j)
			j->second->disconnect(true);
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
			READ_LOCK(*csConnections);
			if (userConnections.empty()) break;
#ifdef _DEBUG
			size = userConnections.size();
#endif
		}
#ifdef _DEBUG
		LogManager::message("ConnectionManager::shutdown userConnections: " + Util::toString(size), false);
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
#ifdef BL_FEATURE_IP_DATABASE
	int options = DatabaseManager::getInstance()->getOptions();
	if (options & (DatabaseOptions::ENABLE_IP_STAT | DatabaseOptions::ENABLE_USER_STAT))
	{
		{
			READ_LOCK(*csDownloads);
			for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
			{
				const ConnectionQueueItemPtr& cqi = *i;
				cqi->getUser()->saveStats(options);
			}
		}
		{
			LOCK(csUploads);
			for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
			{
				const ConnectionQueueItemPtr& cqi = *i;
				cqi->getUser()->saveStats(options);
			}
		}
	}
#endif
	downloads.clear();
	uploads.clear();
}

void ConnectionManager::setUploadLimit(const UserPtr& user, int lim)
{
	WRITE_LOCK(*csConnections);
	auto i = userConnections.begin();
	while (i != userConnections.end())
	{
		UserConnection* uc = i->second.get();
		if (uc->state == UserConnection::STATE_UNUSED)
		{
			i = userConnections.erase(i);
			continue;
		}
		if (uc->isSet(UserConnection::FLAG_UPLOAD) && uc->getUser() == user)
			uc->setUploadLimit(lim);
		i++;
	}
}

void ConnectionManager::removeUnusedConnections()
{
	WRITE_LOCK(*csConnections);
	auto i = userConnections.begin();
	while (i != userConnections.end())
	{
		UserConnection* uc = i->second.get();
		if (uc->state == UserConnection::STATE_UNUSED)
			i = userConnections.erase(i);
		else
			i++;
	}
}

void ConnectionManager::updateAverageSpeed(uint64_t tick)
{
	uploadSpeed.addSample(Socket::g_stats.tcp.uploaded + Socket::g_stats.ssl.uploaded, tick);
	int64_t avg = uploadSpeed.getAverage(3000, 64 * 1024);
	if (avg >= 0) UploadManager::setRunningAverage(avg);
	downloadSpeed.addSample(Socket::g_stats.tcp.downloaded + Socket::g_stats.ssl.downloaded, tick);
	avg = downloadSpeed.getAverage(3000, 64 * 1024);
	if (avg >= 0) DownloadManager::setRunningAverage(avg);
}

string ConnectionManager::getUserConnectionInfo() const
{
	string info;
	READ_LOCK(*csConnections);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); i++)
	{
		if (!info.empty()) info += '\n';
		info += i->second->getDescription();
	}
	return info;
}

string ConnectionManager::getExpectedInfo() const
{
	return expectedNmdc.getInfo() + expectedAdc.getInfo();
}

string ConnectionManager::getTokenInfo() const
{
	vector<pair<string, TokenManager::TokenData>> v;
	tokenManager.getList(v);
	uint64_t now = GET_TICK();
	string res;
	for (const auto& p : v)
	{
		const auto& data = p.second;
		res += p.first;
		res += ": ";
		ConnectionQueueItemPtr cqi;
		if (data.type == TokenManager::TYPE_UPLOAD)
		{
			res += "upload";
			LOCK(csUploads);
			auto i = find(uploads.begin(), uploads.end(), p.first);
			if (i != uploads.end()) cqi = *i;
		}
		else if (data.type == TokenManager::TYPE_DOWNLOAD)
		{
			res += "download";
			READ_LOCK(*csDownloads);
			auto i = find(downloads.begin(), downloads.end(), p.first);
			if (i != downloads.end()) cqi = *i;
		}
		else
			res += "ccpm";
		if (data.expires != UINT64_MAX)
		{
			int64_t t = data.expires - now;
			if (t < 0) t = 0;
			res += " expires=";
			res += Util::toString(t/1000);
		}
		if (cqi)
		{
			const auto& hintedUser = cqi->getHintedUser();
			res += " user=" + (hintedUser.user ? hintedUser.user->getLastNick() : "<empty>");
			if (!hintedUser.hint.empty()) res += " hub=" + hintedUser.hint;
			res += " state=" + Util::toString(cqi->getState());
			int errors = cqi->getErrors();
			if (errors) res += " errors=" + Util::toString(errors);
		}
		res += '\n';
	}
	return res;
}

#ifdef DEBUG_USER_CONNECTION
void ConnectionManager::dumpUserConnections()
{
	READ_LOCK(*csConnections);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); i++)
		i->second->dumpInfo();
}
#endif

void ConnectionManager::fireUploadError(const HintedUser& hintedUser, const string& reason, const string& token) noexcept
{
	fire(ConnectionManagerListener::FailedUpload(), hintedUser, reason, token);
}

void ConnectionManager::fireListenerStarted() noexcept
{
	fire(ConnectionManagerListener::ListenerStarted());
}

void ConnectionManager::fireListenerFailed(const char* type, int af, int errorCode, const string& errorText) noexcept
{
	fire(ConnectionManagerListener::ListenerFailed(), type, af, errorCode, errorText);
}

StringList ConnectionManager::getNmdcFeatures() const
{
	StringList features = nmdcFeatures;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	if (ss->getBool(Conf::COMPRESS_TRANSFERS))
		features.push_back(UserConnection::FEATURE_ZLIB_GET);
	ss->unlockRead();
	return features;
}

StringList ConnectionManager::getAdcFeatures() const
{
	StringList features = adcFeatures;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	if (ss->getBool(Conf::COMPRESS_TRANSFERS))
		features.push_back(UserConnection::FEATURE_ZLIB_GET);
	if (ss->getBool(Conf::USE_CCPM) && ss->getBool(Conf::USE_CPMI))
		features.push_back(UserConnection::FEATURE_ADC_CPMI);
	ss->unlockRead();
	return features;
}

uint16_t ConnectionManager::getConnectionPort(int af, bool secure) const
{
	uint16_t port = ports[secure ? 1 : 0];
	if (port)
	{
		int what = secure ? AppPorts::PORT_TLS : AppPorts::PORT_TCP;
		if (what == AppPorts::PORT_TLS && ports[0] == ports[1])
			what = AppPorts::PORT_TCP;
		int rport = ConnectivityManager::getInstance()->getReflectedPort(af, what);
		if (rport)
			port = rport;
	}
	return port;
}
