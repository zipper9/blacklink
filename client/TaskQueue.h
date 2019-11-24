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

#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <vector>
#include <utility>
#include <algorithm>
#include <stdint.h>
#include "CFlyThread.h"

class Task
{
	public:
		Task() {}
		virtual ~Task() {}
};

class StringTask : public Task
{
	public:
		template<typename T>
		explicit StringTask(T&& str) : str(str) {}
		string str;
};

class StringArrayTask : public Task
{
	public:
		template<typename T>
		explicit StringArrayTask(T&& strArray) : strArray(strArray) {}
		StringList strArray;
};

class TaskQueue
{
	public:
		typedef std::vector<std::pair<int, Task*>> List;
		
		TaskQueue(): disabled(false), lastTick(0) {}

		TaskQueue(const TaskQueue&) = delete;
		TaskQueue& operator= (const TaskQueue&) = delete;

		~TaskQueue() noexcept { clear(); }

		bool add(int type, Task* data, bool& firstItem, uint64_t& tick) noexcept
		{
			cs.lock();
			if (disabled)
			{
				cs.unlock();
				delete data;
				firstItem = false;
				return false;
			}
			
			firstItem = tasks.empty();
			std::swap(lastTick, tick);
			tasks.push_back(std::make_pair(type, data));
			cs.unlock();
			return true;
		}

		void get(List& result) noexcept
		{
			cs.lock();
			result = std::move(tasks);
			tasks.clear();
			cs.unlock();
		}

		void clear() noexcept
		{
			cs.lock();
			std::for_each(tasks.begin(), tasks.end(), [](auto v){ delete v.second; });
			tasks.clear();
			lastTick = 0;
			cs.unlock();
		}
			
		void setDisabled(bool flag) noexcept
		{
			cs.lock();
			disabled = flag;
			cs.unlock();
		}
	
	private:
		FastCriticalSection cs;
		List tasks;
		bool disabled;
		uint64_t lastTick;
};

#endif // TASK_QUEUE_H
