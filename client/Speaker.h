/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 * Copyright (C) 2011-2013 Alexey Solomin, a.rainman on gmail pt com
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

#ifndef SPEAKER_H_
#define SPEAKER_H_

#include "Locks.h"
#include <utility>
#include <vector>
#include <algorithm>

#ifdef _DEBUG
extern volatile bool g_isBeforeShutdown;
#endif

template<typename Listener>
class Speaker
{
		typedef std::vector<Listener*> ListenerList;

	public:
#ifdef _DEBUG
		~Speaker()
		{
			dcassert(listeners.empty());
		}
#endif

		template<typename... ArgT>
		void fire(ArgT && ... args) noexcept
		{
			LOCK(cs);
			tmpListeners = listeners;
			for (auto listener : tmpListeners)
				listener->on(std::forward<ArgT>(args)...);
		}

		void addListener(Listener* listener) noexcept
		{
			dcassert(!g_isBeforeShutdown);
			LOCK(cs);
			if (std::find(listeners.begin(), listeners.end(), listener) == listeners.end())
				listeners.push_back(listener);
		}

		void removeListener(Listener* listener) noexcept
		{
			LOCK(cs);
			auto i = std::find(listeners.begin(), listeners.end(), listener);
			if (i != listeners.end())
				listeners.erase(i);
		}

		void removeListeners() noexcept
		{
			LOCK(cs);
			listeners.clear();
		}

	private:
		ListenerList listeners;
		ListenerList tmpListeners;
		RecursiveMutex cs;
};

#endif // SPEAKER_H_
