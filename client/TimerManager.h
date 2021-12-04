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

#ifndef DCPLUSPLUS_DCPP_TIMER_MANAGER_H
#define DCPLUSPLUS_DCPP_TIMER_MANAGER_H

#include "Speaker.h"
#include "Singleton.h"
#include "Thread.h"
#include "WaitableEvent.h"
#include <atomic>

#ifndef _WIN32
#include <sys/time.h>
#endif

class TimerManagerListener
{
	public:
		virtual ~TimerManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Second;
		typedef X<1> Minute;
		
		virtual void on(Second, uint64_t) noexcept { }
		virtual void on(Minute, uint64_t) noexcept { }
};

class TimerManager : public Speaker<TimerManagerListener>, public Singleton<TimerManager>, public Thread
{
	public:
		void shutdown();

		static time_t getTime()
		{
			return time(nullptr);
		}
		static uint64_t getTick();
		static uint64_t getFileTime();
		void setTicksDisabled(bool disabled)
		{
			ticksDisabled.store(disabled);
		}

#ifdef _WIN32
		static const uint64_t TIMESTAMP_UNITS_PER_SEC = 10000000;
#else
		static const uint64_t TIMESTAMP_UNITS_PER_SEC = 1000000000;
#endif

	private:
		friend class Singleton<TimerManager>;

		TimerManager();
		~TimerManager();

		virtual int run() override;

#ifdef _WIN32
		static const uint64_t frequency;
#else
		static const uint64_t frequency = 1000000000ull;
#endif
		static const uint64_t startup;
		WaitableEvent stopEvent;
		std::atomic_bool ticksDisabled;
};

#define GET_TICK() TimerManager::getTick()
#define GET_TIME() TimerManager::getTime()

#endif // DCPLUSPLUS_DCPP_TIMER_MANAGER_H
