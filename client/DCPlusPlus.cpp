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
#include "DCPlusPlus.h"
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CryptoManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "HashManager.h"
#include "SearchManager.h"
#include "LogManager.h"
#include "FavoriteManager.h"
#include "FinishedManager.h"
#include "ClientManager.h"
#include "ADLSearch.h"
#include "MappingManager.h"
#include "ConnectivityManager.h"
#include "UserManager.h"
#include "ThrottleManager.h"
#include "HublistManager.h"
#include "DatabaseManager.h"
#include "DatabaseOptions.h"
#include "AdcSupports.h"
#include "SettingsUtil.h"
#include "dht/DHT.h"
#include "ConfCore.h"

#include "IpGuard.h"
#include "IpTrust.h"
#ifdef SSA_IPGRANT_FEATURE
#include "IpGrant.h"
#endif // SSA_IPGRANT_FEATURE

#ifdef BL_FEATURE_WEB_SERVER
#include "WebServerManager.h"
#endif

static void initP2PGuard()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const bool loadP2PGuard = ss->getBool(Conf::ENABLE_P2P_GUARD) && ss->getBool(Conf::P2P_GUARD_LOAD_INI);
	ss->unlockRead();
	if (loadP2PGuard)
		Util::loadP2PGuardIni();
	else
		Util::unloadP2PGuardIni();
}

void startup(PROGRESSCALLBACKPROC pProgressCallbackProc, void* pProgressParam, GUIINITPROC pGuiInitProc, void *pGuiParam, DatabaseManager::ErrorCallback dbErrorCallback)
{
#define LOAD_STEP(name, function)\
	do \
	{\
		pProgressCallbackProc(pProgressParam, _T(name));\
		function;\
	} while (0)
	
#define LOAD_STEP_L(nameKey, function)\
	do \
	{\
		pProgressCallbackProc(pProgressParam, TSTRING(nameKey));\
		function;\
	} while (0)

	dcassert(pProgressCallbackProc != nullptr);

	LOAD_STEP_L(STARTUP_SQLITE_DATABASE, DatabaseManager::newInstance());
	DatabaseManager::getInstance()->init(dbErrorCallback);

	LOAD_STEP_L(STARTUP_P2P_GUARD, initP2PGuard());
	LOAD_STEP_L(STARTUP_IBLOCKLIST, Util::loadIBlockList());

	if (DatabaseManager::getInstance()->getOptions() & DatabaseOptions::USE_CUSTOM_LOCATIONS)
		LOAD_STEP_L(STARTUP_CUSTOM_LOCATIONS, Util::loadCustomLocations());

	HashManager::newInstance();

	LOAD_STEP("SSL", CryptoManager::newInstance());

	HublistManager::newInstance();
	SearchManager::newInstance();
	ConnectionManager::newInstance();
	DownloadManager::newInstance();
	UploadManager::newInstance();

	QueueManager::newInstance();
	LOAD_STEP_L(STARTUP_SHARE_MANAGER, ShareManager::newInstance());
	FavoriteManager::newInstance();
	LOAD_STEP_L(STARTUP_IGNORE_LIST, UserManager::newInstance());
	if (pGuiInitProc) pGuiInitProc(pGuiParam);
	LOAD_STEP_L(SETTINGS, Util::loadOtherSettings());
	ShareManager::getInstance()->init();
	LOAD_STEP("IPGuard.ini", ipGuard.load());
	ipGuard.updateSettings();
	LOAD_STEP("IPTrust.ini", ipTrust.load());
	ipTrust.updateSettings();
#ifdef SSA_IPGRANT_FEATURE
	LOAD_STEP("IPGrant.ini", ipGrant.load());
#endif

	FinishedManager::newInstance();
	LOAD_STEP_L(ADL_SEARCH, ADLSearchManager::newInstance());
	ConnectivityManager::newInstance();
	dht::DHT::newInstance();

	LOAD_STEP_L(FAVORITE_HUBS, FavoriteManager::getInstance()->load());

	LOAD_STEP_L(CERTIFICATES, CryptoManager::getInstance()->initializeKeyPair());
	LOAD_STEP_L(DOWNLOAD_QUEUE, QueueManager::getInstance()->loadQueue());
	LOAD_STEP_L(WAITING_USERS, UploadManager::getInstance()->load());

#ifdef BL_FEATURE_WEB_SERVER
	WebServerManager::newInstance();
#endif

	LOAD_STEP_L(HASH_DATABASE, HashManager::getInstance()->startup());

#undef LOAD_STEP
#undef LOAD_STEP_L
}

void preparingCoreToShutdown()
{
	static std::atomic_bool runOnce(false);
	bool flag = runOnce.exchange(true);
	if (!flag)
	{
		StepLogger sl("[Core shutdown]", false);
		dht::DHT::getInstance()->stop();
		ClientManager::shutdown();
		ConnectionManager::getInstance()->stopServers();
#ifdef DEBUG_SHUTDOWN
		sl.step("ConnectionManager");
#endif
		SearchManager::getInstance()->shutdown();
		HashManager::getInstance()->shutdown();
#ifdef DEBUG_SHUTDOWN
		sl.step("HashManager");
#endif
		TimerManager::getInstance()->setTicksDisabled(true);
		UploadManager::getInstance()->shutdown();
#ifdef DEBUG_SHUTDOWN
		sl.step("UploadManager");
#endif
#ifdef BL_FEATURE_WEB_SERVER
		WebServerManager::getInstance()->shutdown();
#endif
		ClientManager::prepareClose();
		ShareManager::getInstance()->shutdown();
#ifdef DEBUG_SHUTDOWN
		sl.step("ShareManager");
#endif
		QueueManager::getInstance()->shutdown();
#ifdef DEBUG_SHUTDOWN
		sl.step("QueueManager");
#endif
		HublistManager::getInstance()->shutdown();
#ifdef DEBUG_SHUTDOWN
		sl.step("HublistManager");
#endif
		ClientManager::clear();
	}
}

void shutdown(GUIINITPROC pGuiInitProc, void *pGuiParam)
{
#if defined(BL_FEATURE_COLLECT_UNKNOWN_FEATURES) || defined(BL_FEATURE_COLLECT_UNKNOWN_TAGS)
	string tags = AdcSupports::getCollectedUnknownTags();
	if (!tags.empty())
		LogManager::message("Dumping collected tags\n" + tags, false);
#endif

#ifdef _DEBUG
	dcdebug("shutdown started: userCount = %d, onlineUserCount = %d, clientCount = %d\n",
		User::g_user_counts.load(), OnlineUser::onlineUserCount.load(), Client::clientCount.load());
#endif
	QueueManager::getInstance()->saveQueue(true);
	SettingsManager::instance.saveSettings();
	ConnectionManager::getInstance()->shutdown();

	preparingCoreToShutdown();

	DownloadManager::getInstance()->clearDownloads();
	UploadManager::getInstance()->clearUploads();
#ifdef DEBUG_SHUTDOWN
	LogManager::message("Uploads:   created=" + Util::toString(Upload::countCreated) + ", deleted=" + Util::toString(Upload::countDeleted), false);
	LogManager::message("Downloads: created=" + Util::toString(Download::countCreated) + ", deleted=" + Util::toString(Download::countDeleted), false);
#endif
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	BufferedSocket::waitShutdown();
#ifdef DEBUG_SHUTDOWN
	LogManager::message("BufferedSockets deleted", false);
#endif
#endif

	ConnectivityManager::deleteInstance();
#ifdef BL_FEATURE_WEB_SERVER
	WebServerManager::deleteInstance();
#endif
	if (pGuiInitProc) pGuiInitProc(pGuiParam);
	ADLSearchManager::deleteInstance();
	FinishedManager::deleteInstance();
	ShareManager::deleteInstance();
	CryptoManager::deleteInstance();
	dht::DHT::getInstance()->stop(true);
	dht::DHT::deleteInstance();
	ThrottleManager::deleteInstance();
	DownloadManager::deleteInstance();
	UploadManager::deleteInstance();
	QueueManager::deleteInstance();
	ConnectionManager::deleteInstance();
	SearchManager::deleteInstance();
	HublistManager::deleteInstance();
	UserManager::deleteInstance();
	FavoriteManager::deleteInstance();
	ClientManager::deleteInstance();
	HashManager::deleteInstance();

	DatabaseManager::getInstance()->shutdown();
	DatabaseManager::deleteInstance();

	TimerManager::getInstance()->shutdown();
	TimerManager::deleteInstance();

	SettingsManager::instance.removeListeners();

#ifdef _WIN32
	WSACleanup();
#endif
#ifdef _DEBUG
	dcdebug("shutdown completed: userCount = %d, onlineUserCount = %d, clientCount = %d\n",
		User::g_user_counts.load(), OnlineUser::onlineUserCount.load(), Client::clientCount.load());
	dcassert(OnlineUser::onlineUserCount == 0);
	dcassert(Client::clientCount == 0);
	dcassert(UploadQueueFile::g_upload_queue_item_count == 0);
	dcdebug("shutdown start - UploadQueueItem::g_upload_queue_item_count = %d \n", int(UploadQueueFile::g_upload_queue_item_count));
#endif
}
