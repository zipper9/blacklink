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
#include <process.h>
#include "Thread.h"
#include "StrUtil.h"

#if !defined(USE_WIN_THREAD_NAME) && defined(_WIN32) && defined(_DEBUG)
#define USE_WIN_THREAD_NAME
#endif

#ifdef USE_WIN_THREAD_NAME

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

static void SetThreadName(DWORD threadId, const char* threadName)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = threadId;
	info.dwFlags = 0;
	
	__try
	{
		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

#endif // USE_WIN_THREAD_NAME

void Thread::join(unsigned milliseconds /*= INFINITE*/)
{	
	if (threadHandle != INVALID_THREAD_HANDLE)
	{
		Handle handle = threadHandle;
		threadHandle = INVALID_THREAD_HANDLE;
		BaseThread::join(handle, milliseconds);
	}
}

unsigned int WINAPI Thread::starter(void* p)
{
	if (Thread* t = reinterpret_cast<Thread*>(p))
		t->run();
	return 0;
}

void Thread::setThreadPriority(Priority p)
{
	if (threadHandle != INVALID_THREAD_HANDLE)
		::SetThreadPriority(threadHandle, p);
}

void Thread::start(unsigned stackSize, const char* name)
{
	join();
	stackSize <<= 10;
	HANDLE h = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, starter, this, 0, nullptr));
	if (h == nullptr || h == INVALID_THREAD_HANDLE)
	{
		auto lastError = GetLastError();
		throw ThreadException("Error creating thread: " + Util::toString(lastError));
	}
	else
	{
		threadHandle = h;
#ifdef USE_WIN_THREAD_NAME
		if (name) SetThreadName(GetThreadId(h), name);
#endif
	}
}
