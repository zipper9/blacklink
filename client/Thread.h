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
#include "Exception.h"

STANDARD_EXCEPTION(ThreadException);

#ifdef _WIN32
#define CRITICAL_SECTION_SPIN_COUNT 2000
#define INVALID_THREAD_HANDLE INVALID_HANDLE_VALUE
#else
#include <pthread.h>
#define INVALID_THREAD_HANDLE 0
#endif

#ifdef _WIN32
class BaseThread
{
	public:
		enum Priority
		{
			IDLE = THREAD_PRIORITY_IDLE,
			LOWEST = THREAD_PRIORITY_BELOW_NORMAL,
			LOW = THREAD_PRIORITY_BELOW_NORMAL,
			NORMAL = THREAD_PRIORITY_NORMAL
			//HIGH = THREAD_PRIORITY_ABOVE_NORMAL
		};

		typedef HANDLE Handle;
		static const unsigned INFINITE_TIMEOUT = INFINITE;

		static inline void closeHandle(Handle handle)
		{
			CloseHandle(handle);
		}

		static inline void join(Handle handle, unsigned milliseconds = INFINITE_TIMEOUT)
		{
			WaitForSingleObject(handle, milliseconds);
			CloseHandle(handle);
		}

		static void sleep(unsigned milliseconds)
		{
			::Sleep(milliseconds);
		}

		static void yield()
		{
			::Sleep(0);
		}

		static unsigned getCurrentThreadId()
		{
			return ::GetCurrentThreadId();
		}
};
#else
class BaseThread
{
	public:
		enum Priority
		{
			IDLE = 19,
			LOWEST = 14,
			LOW = 7,
			NORMAL = 0
		};

		typedef pthread_t Handle;

		static inline void join(Handle handle)
		{
			pthread_join(handle, nullptr);
		}

		static void sleep(unsigned milliseconds)
		{
			usleep(milliseconds * 1000);
		}

		static void yield()
		{
			sched_yield();
		}

		static unsigned getCurrentThreadId()
		{
			return pthread_self();
		}
};
#endif

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

#ifdef _WIN32
class CriticalSection
{
	public:
		CriticalSection()
		{
			#ifdef _DEBUG
			BOOL result = 
			#endif
			InitializeCriticalSectionAndSpinCount(&cs, CRITICAL_SECTION_SPIN_COUNT);
			#ifdef _DEBUG
			dcassert(result);
			#endif
		}

		~CriticalSection()
		{
			DeleteCriticalSection(&cs);
		}

		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator= (const CriticalSection&) = delete;

#ifdef LOCK_DEBUG
		void lock(const char* filename = nullptr, int line = 0)
#else
		void lock()
#endif
		{
			EnterCriticalSection(&cs);
#ifdef LOCK_DEBUG
			ownerFile = filename;
			ownerLine = line;
#endif
		}

		void unlock()
		{
			LeaveCriticalSection(&cs);
#ifdef LOCK_DEBUG
			ownerFile = nullptr;
			ownerLine = 0;
#endif
		}

#ifdef LOCK_DEBUG
		bool tryLock(const char* filename = nullptr, int line = 0)
		{
			if (TryEnterCriticalSection(&cs))
			{
				ownerFile = filename;
				ownerLine = line;
				return true;
			}
			return false;
		}
#else
		bool tryLock()
		{
			return TryEnterCriticalSection(&cs) != FALSE;
		}
#endif
	private:
		CRITICAL_SECTION cs;
#ifdef LOCK_DEBUG
		const char* ownerFile = nullptr;
		int ownerLine = 0;
#endif
};
#else
class CriticalSection
{
	public:
		CriticalSection() {}
		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator= (const CriticalSection&) = delete;

#ifdef LOCK_DEBUG
		void lock(const char* filename = nullptr, int line = 0)
#else
		void lock()
#endif
		{
			pthread_mutex_lock(&mutex);
#ifdef LOCK_DEBUG
			ownerFile = filename;
			ownerLine = line;
#endif
		}

		void unlock()
		{
			pthread_mutex_unlock(&mutex);
#ifdef LOCK_DEBUG
			ownerFile = nullptr;
			ownerLine = 0;
#endif
		}

#ifdef LOCK_DEBUG
		bool tryLock(const char* filename = nullptr, int line = 0)
		{
			if (!pthread_mutex_trylock(&mutex))
			{
				ownerFile = filename;
				ownerLine = line;
				return true;
			}
			return false;
		}
#else
		bool tryLock()
		{
			return pthread_mutex_trylock(&mutex) == 0;
		}
#endif
	private:
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef LOCK_DEBUG
		const char* ownerFile = nullptr;
		int ownerLine = 0;
#endif
};
#endif

#ifdef IRAINMAN_USE_SPIN_LOCK

/**
 * A fast, non-recursive and unfair implementation of the Critical Section.
 * It is meant to be used in situations where the risk for lock conflict is very low,
 * i e locks that are held for a very short time. The lock is _not_ recursive, i e if
 * the same thread will try to grab the lock it'll hang in a never-ending loop. The lock
 * is not fair, i e the first to try to enter a locked lock is not guaranteed to be the
 * first to get it when it's freed...
 */

class FastCriticalSection
{
	public:
		FastCriticalSection()
		{
			state.clear();
		}
		
		FastCriticalSection(const FastCriticalSection&) = delete;
		FastCriticalSection& operator= (const FastCriticalSection&) = delete;

#ifdef LOCK_DEBUG
		void lock(const char* filename = nullptr, int line = 0)
#else
		void lock()
#endif
		{
			while (state.test_and_set())
				Thread::yield();
		}

		void unlock()
		{
			state.clear();
		}

	private:
		std::atomic_flag state;
};
#else
typedef CriticalSection FastCriticalSection;
#endif // IRAINMAN_USE_SPIN_LOCK

template<class T> class LockBase
{
	public:
#ifdef LOCK_DEBUG
		LockBase(T& cs, const char* filename = nullptr, int line = 0) : cs(cs)
		{
			cs.lock(filename, line);
		}
#else
		LockBase(T& cs) : cs(cs)
		{
			cs.lock();
		}
#endif

		~LockBase()
		{
			cs.unlock();
		}

	private:
		T& cs;
};

#ifdef LOCK_DEBUG
#define LOCK(cs) LockBase<decltype(cs)> lock(cs, __FUNCTION__, __LINE__);
#else
#define LOCK(cs) LockBase<decltype(cs)> lock(cs);
#endif

#endif // !defined(THREAD_H)
