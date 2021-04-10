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

#ifndef DCPLUSPLUS_DCPP_SPEAKER_H
#define DCPLUSPLUS_DCPP_SPEAKER_H

#include <boost/range/algorithm/find.hpp>
#include <utility>
#include <vector>
#include "Thread.h"
#include "RWLock.h"
#include "noexcept.h"

#ifdef _DEBUG
extern volatile bool g_isBeforeShutdown;
extern volatile bool g_isShutdown;
#endif

template<typename Listener>
class Speaker
{
		typedef std::vector<Listener*> ListenerList;
#ifdef _DEBUG
# define  _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1 // Only critical event debug.
# ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
//#  define  _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2 // Show all events in debug window.
# endif
#endif

		void log_listener_list(const ListenerList& p_list, const char* p_log_message)
		{
			dcdebug("[log_listener_list][%s][tid = %u] [this=%p] count = %d ", p_log_message, BaseThread::getCurrentThreadId(), this, p_list.size());
			for (size_t i = 0; i != p_list.size(); ++i)
			{
				dcdebug("[%u] = %p, ", i, p_list[i]);
			}
			dcdebug("\r\n");
		}
		
	public:
#ifdef _DEBUG
		~Speaker()
		{
			dcassert(m_listeners.empty());
		}
#endif
		
#define fly_fire  fire
#define fly_fire1 fire
#define fly_fire2 fire
#define fly_fire3 fire
#define fly_fire4 fire
#define fly_fire5 fire

		template<typename... ArgT>
		void fire(ArgT && ... args)
		{
			LOCK(m_listenerCS);
			ListenerList tmp = m_listeners;
#ifdef _DEBUG
			if (g_isBeforeShutdown && !tmp.empty())
			{
				log_listener_list(tmp, "fire-before-destroy!");
			}

			if (g_isShutdown && !tmp.empty())
			{
				log_listener_list(tmp, "fire-destroy!");
			}
#endif
			for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
			{
				(*i)->on(std::forward<ArgT>(args)...);
			}
		}
		
		void addListener(Listener* aListener)
		{
			dcassert(!g_isBeforeShutdown);
			LOCK(m_listenerCS);
			if (boost::range::find(m_listeners, aListener) == m_listeners.end())
			{
				m_listeners.push_back(aListener);
				m_listeners.shrink_to_fit();
			}
#ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
			else
			{
				dcassert(0);
# ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2
				log_listener_list(m_listeners, "addListener-twice!!!");
# endif
			}
#endif // _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
		}
		
		void removeListener(Listener* aListener) noexcept
		{
			LOCK(m_listenerCS);
			if (!m_listeners.empty())
			{
				auto it = boost::range::find(m_listeners, aListener);
				if (it != m_listeners.end())
				{
					m_listeners.erase(it);
				}
#ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
				else
				{
					dcassert(0);
# ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2
					log_listener_list(m_listeners, "removeListener-zombie!!!");
# endif
				}
#endif // _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
			}
		}
		
		void removeListeners() noexcept
		{
			LOCK(m_listenerCS);
			m_listeners.clear();
		}
		
	private:
		ListenerList m_listeners;
		CriticalSection m_listenerCS;
};

#endif // !defined(SPEAKER_H)
