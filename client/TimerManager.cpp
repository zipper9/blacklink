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
#include "TimeUtil.h"

#ifdef _DEBUG
#include "ClientManager.h"
#endif

TimerManager::TimerManager() : ticksDisabled(false)
{
	stopEvent.create();
}

TimerManager::~TimerManager()
{
#ifdef _DEBUG
	dcassert(ClientManager::isShutdown());
#endif
}

void TimerManager::shutdown()
{
	removeListeners();
	stopEvent.notify();
	join();
}

int TimerManager::run()
{
	uint64_t nextMinute = 60000;
	while (!stopEvent.timedWait(1000))
	{
		uint64_t tick = Util::getTick();
		if (!ticksDisabled)
			fire(TimerManagerListener::Second(), tick);
		if (tick >= nextMinute)
		{
			nextMinute = tick + 60000;
			if (!ticksDisabled)
				fire(TimerManagerListener::Minute(), tick);
		}
	}
	return 0;
}
