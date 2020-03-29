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
#include "ClientManager.h"

static inline uint64_t getHighResFrequency()
{
	LARGE_INTEGER x;
	if (!QueryPerformanceFrequency(&x))
	{
		dcassert(0);
		return 1000;
	}
	return x.QuadPart;
}

static inline uint64_t getHighResTimestamp()
{
	LARGE_INTEGER x;
	if (!QueryPerformanceCounter(&x)) return 0;
	return x.QuadPart;
}

const uint64_t TimerManager::frequency = getHighResFrequency();
const uint64_t TimerManager::startup = getHighResTimestamp();

TimerManager::TimerManager() : ticksDisabled(false)
{
	stopEvent.create();
}

TimerManager::~TimerManager()
{
	dcassert(ClientManager::isShutdown());
}

void TimerManager::shutdown()
{
	stopEvent.notify();
	join();
}

int TimerManager::run()
{
	uint64_t nextMinute = startup + 60 * frequency;
	while (!stopEvent.timedWait(1000))
	{
		uint64_t now = getHighResTimestamp();
		uint64_t tick;
		if (now <= startup)
			tick = 0;
		else
			tick = (now - startup) * 1000 / frequency;
		if (!ticksDisabled)
			fly_fire1(TimerManagerListener::Second(), tick);
		if (now >= nextMinute)
		{
			nextMinute = now + 60 * frequency;
			if (!ticksDisabled)
				fly_fire1(TimerManagerListener::Minute(), tick);
		}
	}
	return 0;
}

uint64_t TimerManager::getTick()
{
	auto now = getHighResTimestamp();
	if (now <= startup) return 0; // this should never happen
	return (now - startup) * 1000 / frequency;
}

uint64_t TimerManager::getFileTime()
{
	uint64_t currentTime;
	GetSystemTimeAsFileTime(reinterpret_cast<FILETIME*>(&currentTime));
	return currentTime;
}
