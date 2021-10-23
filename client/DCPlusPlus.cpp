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
#include "FinishedManager.h"
#include "ADLSearch.h"
#include "MappingManager.h"
#include "ConnectivityManager.h"
#include "UserManager.h"
#include "ThrottleManager.h"
#include "HublistManager.h"
#include "DatabaseManager.h"
#include "dht/DHT.h"

#include "IpGuard.h"
#include "IpTrust.h"
#ifdef SSA_IPGRANT_FEATURE
#include "IpGrant.h"
#endif // SSA_IPGRANT_FEATURE

#ifdef ENABLE_WEB_SERVER
#include "WebServerManager.h"
#endif

void startup(PROGRESSCALLBACKPROC pProgressCallbackProc, void* pProgressParam, GUIINITPROC pGuiInitProc, void *pGuiParam, DatabaseManager::ErrorCallback dbErrorCallback)
{
#define LOAD_STEP(name, function)\
	{\
		pProgressCallbackProc(pProgressParam, _T(name));\
		function;\
	}
	
#define LOAD_STEP_L(nameKey, function)\
	{\
		pProgressCallbackProc(pProgressParam, TSTRING(nameKey));\
		function;\
	}
	
	dcassert(pProgressCallbackProc != nullptr);
	
	LOAD_STEP_L(STARTUP_SQLITE_DATABASE, DatabaseManager::newInstance());
	DatabaseManager::getInstance()->init(dbErrorCallback);

	LOAD_STEP_L(STARTUP_GEO_IP, Util::loadGeoIp());
	LOAD_STEP_L(STARTUP_P2P_GUARD, Util::loadP2PGuard());
	LOAD_STEP_L(STARTUP_IBLOCKLIST, Util::loadIBlockList());
	
	LOAD_STEP_L(STARTUP_CUSTOM_LOCATIONS, Util::loadCustomLocations());
	
	HashManager::newInstance();

#ifdef FLYLINKDC_USE_VLD
	VLDDisable();
#endif
// FLYLINKDC_CRYPTO_DISABLE
	LOAD_STEP("SSL", CryptoManager::newInstance());
#ifdef FLYLINKDC_USE_VLD
	VLDEnable();
#endif
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
	LOAD_STEP_L(SETTINGS, SettingsManager::getInstance()->loadOtherSettings());
	LOAD_STEP("IPGuard.ini", ipGuard.load());
	LOAD_STEP("IPTrust.ini", ipTrust.load());
#ifdef SSA_IPGRANT_FEATURE
	LOAD_STEP("IPGrant.ini", ipGrant.load());
#endif
	
	FinishedManager::newInstance();
	LOAD_STEP_L(ADL_SEARCH, ADLSearchManager::newInstance());
	ConnectivityManager::newInstance();
	dht::DHT::newInstance();
	
	LOAD_STEP_L(FAVORITE_HUBS, FavoriteManager::getInstance()->load());
	
	LOAD_STEP_L(CERTIFICATES, CryptoManager::getInstance()->loadCertificates());
	LOAD_STEP_L(DOWNLOAD_QUEUE, QueueManager::getInstance()->loadQueue());
	LOAD_STEP_L(WAITING_USERS, UploadManager::getInstance()->load());

#ifdef ENABLE_WEB_SERVER
	WebServerManager::newInstance();
#endif

	LOAD_STEP_L(HASH_DATABASE, HashManager::getInstance()->startup());
	
#undef LOAD_STEP
#undef LOAD_STEP_L
}

void preparingCoreToShutdown()
{
	static bool g_is_first = false;
	if (!g_is_first)
	{
		g_is_first = true;
		CFlyLog l_log("[Core shutdown]");
		dht::DHT::getInstance()->stop();
		ClientManager::shutdown();
		SearchManager::getInstance()->shutdown();
		HashManager::getInstance()->shutdown();
		TimerManager::getInstance()->shutdown();
		UploadManager::getInstance()->shutdown();
#ifdef ENABLE_WEB_SERVER
		WebServerManager::getInstance()->shutdown();
#endif
		ClientManager::prepareClose();
		ShareManager::getInstance()->shutdown();
		QueueManager::getInstance()->shutdown();
		HublistManager::getInstance()->shutdown();
		ClientManager::clear();
		DatabaseManager::getInstance()->flush();
	}
}

void shutdown(GUIINITPROC pGuiInitProc, void *pGuiParam)
{
	{
#ifdef FLYLINKDC_COLLECT_UNKNOWN_TAG
		string l_debugTag;
		{
			LOCK(NmdcSupports::g_debugCsUnknownNmdcTagParam);
			//dcassert(NmdcSupports::g_debugUnknownNmdcTagParam.empty());
			const auto& l_debugUnknownNmdcTagParam = NmdcSupports::g_debugUnknownNmdcTagParam;
			for (auto i = l_debugUnknownNmdcTagParam.begin(); i != l_debugUnknownNmdcTagParam.end(); ++i)
			{
				l_debugTag += i->first + "(" + Util::toString(i->second) + ")" + ',';
			}
			NmdcSupports::g_debugUnknownNmdcTagParam.clear();
		}
		if (!l_debugTag.empty())
		{
			LogManager::message("Founded unknown NMDC tag param: " + l_debugTag);
		}
#endif
		
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
		// [!] IRainman fix: supports cleanup.
		string l_debugFeatures;
		string l_debugConnections;
		{
			LOCK(AdcSupports::g_debugCsUnknownAdcFeatures);
			// dcassert(AdcSupports::g_debugUnknownAdcFeatures.empty());
			const auto& l_debugUnknownFeatures = AdcSupports::g_debugUnknownAdcFeatures;
			for (auto i = l_debugUnknownFeatures.begin(); i != l_debugUnknownFeatures.end(); ++i)
			{
				l_debugFeatures += i->first + "[" + i->second + "]" + ',';
			}
			AdcSupports::g_debugUnknownAdcFeatures.clear();
		}
		{
			LOCK(NmdcSupports::g_debugCsUnknownNmdcConnection);
			dcassert(NmdcSupports::g_debugUnknownNmdcConnection.empty());
			const auto& l_debugUnknownConnections = NmdcSupports::g_debugUnknownNmdcConnection;
			for (auto i = l_debugUnknownConnections.begin(); i != l_debugUnknownConnections.end(); ++i)
			{
				l_debugConnections += *i + ',';
			}
			NmdcSupports::g_debugUnknownNmdcConnection.clear();
		}
		if (!l_debugFeatures.empty())
		{
			LogManager::message("Founded unknown ADC supports: " + l_debugFeatures);
		}
		if (!l_debugConnections.empty())
		{
			LogManager::message("Founded unknown NMDC connections: " + l_debugConnections);
		}
#endif // FLYLINKDC_COLLECT_UNKNOWN_FEATURES
		
#ifdef _DEBUG
		dcdebug("shutdown started: userCount = %d, onlineUserCount = %d, clientCount = %d\n",
			User::g_user_counts.load(), OnlineUser::onlineUserCount.load(), Client::clientCount.load());
#endif
#ifdef FLYLINKDC_USE_TORRENT
		DownloadManager::getInstance()->shutdown_torrent();
#endif
		QueueManager::getInstance()->saveQueue(true);
		SettingsManager::getInstance()->save();
		ConnectionManager::getInstance()->shutdown();
		
		preparingCoreToShutdown(); // Зовем тут второй раз т.к. вероятно при автообновлении оно не зовется.
		
#ifdef FLYLINKDC_USE_DNS
		Socket::dnsCache.waitShutdown(); // !SMT!-IP
#endif
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
		BufferedSocket::waitShutdown();
#endif

		ConnectivityManager::deleteInstance();
#ifdef ENABLE_WEB_SERVER
		WebServerManager::deleteInstance();
#endif
		if (pGuiInitProc)
		{
			pGuiInitProc(pGuiParam);
		}
		ADLSearchManager::deleteInstance();
		FinishedManager::deleteInstance();
		ShareManager::deleteInstance();
#ifdef FLYLINKDC_USE_VLD
		VLDDisable(); // TODO VLD показывает там лики - не понял пока как победить OpenSSL
#endif
// FLYLINKDC_CRYPTO_DISABLE
		CryptoManager::deleteInstance();
#ifdef FLYLINKDC_USE_VLD
		VLDEnable(); // TODO VLD показывает там лики - не понял пока как победить OpenSSL
#endif
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
		
		DatabaseManager::deleteInstance();
		DatabaseManager::shutdown();
		TimerManager::deleteInstance();

		SettingsManager::getInstance()->removeListeners();
		SettingsManager::deleteInstance();

		extern SettingsManager* g_settings;
		g_settings = nullptr;

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
}
