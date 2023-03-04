/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
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

#include "BootstrapManager.h"
#include "DHT.h"
#include "IndexManager.h"
#include "DHTSearchManager.h"
#include "TaskManager.h"
#include "Utils.h"

#include "../SettingsManager.h"
#include "../ShareManager.h"
#include "../SearchManager.h"
#include "../ConnectivityManager.h"

namespace dht
{

	TaskManager::TaskManager()
	{
		uint64_t tick = GET_TICK();
		nextPublishTime = nextSearchTime = tick;
		nextSelfLookup = tick + SELF_LOOKUP_TIME_INIT;
		nextFirewallCheck = tick + FWCHECK_TIME;
		lastBootstrap = 0;
		nextXmlSave = tick + 60*60*1000;
		TimerManager::getInstance()->addListener(this);
	}

	TaskManager::~TaskManager()
	{
		TimerManager::getInstance()->removeListener(this);
	}

	// TimerManagerListener
	void TaskManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
	{
		DHT* d = DHT::getInstance();
		if (d->getState() == DHT::STATE_INITIALIZING)
		{
			auto cm = ConnectivityManager::getInstance();
			if (cm->isSetupInProgress())
				return;
			IpAddress reflectedIP = cm->getReflectedIP(AF_INET);
			if (!Util::isEmpty(reflectedIP))
				d->setExternalIP(reflectedIP);
			int port = DHT::getPort();
			if (!port)
				return;
			d->state = DHT::STATE_BOOTSTRAP;
			BootstrapManager::getInstance()->bootstrap();
		}
		if (d->isConnected() && d->getNodesCount() >= K)
		{
			if (!d->isFirewalled() && IndexManager::getInstance()->getPublish() && tick >= nextPublishTime)
			{
				// publish next file
				IndexManager::getInstance()->publishNextFile();
				nextPublishTime = tick + PUBLISH_TIME;
			}
		}
		else
		{
			if (tick - lastBootstrap > 15000 || (d->getNodesCount() == 0 && tick - lastBootstrap >= 2000))
			{
				// bootstrap if we doesn't know any remote node
				BootstrapManager::getInstance()->process();
				lastBootstrap = tick;
			}
		}

		auto sm = SearchManager::getInstance();
		sm->processQueue(tick);

		if (tick >= nextSearchTime)
		{
			sm->processSearches();
			nextSearchTime = tick + SEARCH_PROCESSTIME;
		}

		if (tick >= nextSelfLookup)
		{
			// find myself in the network
			sm->findNode(ClientManager::getMyCID());
			nextSelfLookup = tick + SELF_LOOKUP_TIME;
		}

		if (tick >= nextFirewallCheck)
		{
			d->setRequestFWCheck();
			nextFirewallCheck = tick + FWCHECK_TIME;
		}
	}

	void TaskManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
	{
		Utils::cleanFlood();

		// remove dead nodes
		DHT::getInstance()->checkExpiration(tick);
		IndexManager::getInstance()->checkExpiration(tick);

		if (tick >= nextXmlSave)
		{
			DHT::getInstance()->saveData();
			nextXmlSave = tick + 60*60*1000;
		}

		BootstrapManager::getInstance()->cleanup();
	}

}
