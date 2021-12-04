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

#ifndef DCPLUSPLUS_DCPP_THREAD_H
#define DCPLUSPLUS_DCPP_THREAD_H

#include <atomic>
#include "BaseThread.h"
#include "Exception.h"

STANDARD_EXCEPTION(ThreadException);

class Thread : public BaseThread
{
	public:
		Thread() : threadHandle(INVALID_THREAD_HANDLE) { }
		virtual ~Thread()
		{
#ifdef _WIN32
			if (threadHandle != INVALID_THREAD_HANDLE)
				closeHandle(threadHandle);
#endif
		}

		Thread(const Thread&) = delete;
		Thread& operator= (const Thread&) = delete;

		void start(unsigned stackSize = 0, const char* name = nullptr);
		void join();
		void setThreadPriority(Priority p);

		bool isRunning() const
		{
			return threadHandle != INVALID_THREAD_HANDLE;
		}

	protected:
		virtual int run() = 0;

		Handle threadHandle;

	private:
#ifdef _WIN32
		static unsigned __stdcall starter(void* p);
#else
		static void* starter(void* p);
#endif
};

#endif
