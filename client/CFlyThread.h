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

#pragma warning(disable:4456)

#ifdef _DEBUG
#include <boost/noncopyable.hpp>
#include <set>
#endif

#include <atomic>
#include "Exception.h"

#include "CFlyLockProfiler.h"

#define CRITICAL_SECTION_SPIN_COUNT 2000 // [+] IRainman opt. http://msdn.microsoft.com/en-us/library/windows/desktop/ms683476(v=vs.85).aspx You can improve performance significantly by choosing a small spin count for a critical section of short duration. For example, the heap manager uses a spin count of roughly 4,000 for its per-heap critical sections.

STANDARD_EXCEPTION(ThreadException);

#define INVALID_THREAD_HANDLE INVALID_HANDLE_VALUE

class BaseThread
{
	public:
		enum Priority
		{
			IDLE = THREAD_PRIORITY_IDLE,
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

		static inline void join(Handle handle, unsigned milliseconds)
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
};

// [+] IRainman fix: detect long waits.
#ifdef _DEBUG
# define TRACING_LONG_WAITS
# ifdef TRACING_LONG_WAITS
#  define TRACING_LONG_WAITS_TIME_MS (1 * 60 * 1000)
#  define DEBUG_WAITS_INIT(maxSpinCount) int _debugWaits = 0 - maxSpinCount;
#  define DEBUG_WAITS(waitTime) {\
		if (++_debugWaits == waitTime)\
		{\
			dcdebug("Thread %d waits a lockout condition for more than " #waitTime " ms.\n", ::GetCurrentThreadId());\
			/*dcassert(0);*/\
		} }
# else
#  define TRACING_LONG_WAITS_TIME_MS
#  define DEBUG_WAITS_INIT(maxSpinCount)
#  define DEBUG_WAITS(waitTime)
# endif // TRACING_LONG_WAITS
#endif
// [~] IRainman fix.

class Thread : public BaseThread
{
	public:
		Thread() : threadHandle(INVALID_THREAD_HANDLE) { }
		virtual ~Thread()
		{
			if (threadHandle != INVALID_THREAD_HANDLE)
				closeHandle(threadHandle);
		}
		
		Thread(const Thread&) = delete;
		Thread& operator= (const Thread&) = delete;

#ifdef _DEBUG
		class ConditionLocker
		{
			public:
				ConditionLocker(std::atomic_flag& state) : state(state)
				{
					while (state.test_and_set())
						Thread::yield();
				}
				
				~ConditionLocker()
				{
					state.clear();
				}

			private:
				std::atomic_flag& state;
		};
#endif // _DEBUG        
		
		void start(unsigned stackSize = 0, const char* name = nullptr);
		void join(unsigned milliseconds = INFINITE_TIMEOUT);
		void setThreadPriority(Priority p);
		
		bool isRunning() const
		{
			return threadHandle != INVALID_THREAD_HANDLE;
		}
	
	protected:
		virtual int run() = 0;
		
	private:
		Handle threadHandle;
		static unsigned int WINAPI starter(void* p);
};

// TODO: remove
class CFlyStopThread
{
	public:
		CFlyStopThread() : m_stop(false)
		{
		}
		~CFlyStopThread()
		{
			m_stop = true;
		}
	protected:
		volatile bool m_stop;
		void stopThread(bool p_is_stop = true)
		{
			m_stop = p_is_stop;
		}
		bool isShutdown() const
		{
			if (m_stop)
				return true;
			extern volatile bool g_isBeforeShutdown;
			return g_isBeforeShutdown;
		}
};

class CriticalSection
{
	public:
		CriticalSection()
		{
			BOOL result = InitializeCriticalSectionAndSpinCount(&cs, CRITICAL_SECTION_SPIN_COUNT);
			dcassert(result);
		}

		~CriticalSection()
		{
			DeleteCriticalSection(&cs);
		}

		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator= (const CriticalSection&) = delete;

		void lock()
		{
			EnterCriticalSection(&cs);
		}

		long getLockCount() const
		{
			return cs.LockCount;
		}

		long getRecursionCount() const
		{
			return cs.RecursionCount;
		}

		void unlock()
		{
			LeaveCriticalSection(&cs);
		}

		bool tryLock()
		{
			return TryEnterCriticalSection(&cs) != FALSE;
		}

	private:
		CRITICAL_SECTION cs;
};

// [+] IRainman fix: detect spin lock recursive entry
#ifdef _DEBUG
# define SPIN_LOCK_TRACE_RECURSIVE_ENTRY
# ifdef SPIN_LOCK_TRACE_RECURSIVE_ENTRY
#  define DEBUG_SPIN_LOCK_DECL() std::set<DWORD> _debugOwners; std::atomic_bool _debugOwnersState
#  define DEBUG_SPIN_LOCK_INIT() _debugOwnersState.clear();
#  define DEBUG_SPIN_LOCK_INSERT() {\
		Thread::ConditionLocker cl(_debugOwnersState);\
		const auto s = _debugOwners.insert(GetCurrentThreadId());\
		dcassert (s.second); /* spin lock prevents recursive entry! */\
	}
#  define DEBUG_SPIN_LOCK_ERASE() {\
		Thread::ConditionLocker cl(_debugOwnersState);\
		/*const auto n = */_debugOwners.erase(GetCurrentThreadId());\
		/*dcassert(n);*/ /* remove zombie owner */\
	}
# else
#  define DEBUG_SPIN_LOCK_DECL()
#  define DEBUG_SPIN_LOCK_INIT()
#  define DEBUG_SPIN_LOCK_INSERT()
#  define DEBUG_SPIN_LOCK_ERASE()
# endif // SPIN_LOCK_TRACE_RECURSIVE_ENTRY
#endif // _DEBUG
// [~] IRainman fix.

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

		void lock()
		{
			while (state.test_and_set())
				Thread::yield();
		}

		void unlock()
		{
			state.clear();
		}

		long getLockCount() const
		{
			return 0;
		}

		long getRecursionCount() const
		{
			return 0;
		}

	private:
		std::atomic_flag state;
};
#else
typedef CriticalSection FastCriticalSection;
#endif // IRAINMAN_USE_SPIN_LOCK

template<class T> class LockBase
#ifdef FLYLINKDC_USE_PROFILER_CS
	: public CFlyLockProfiler
#endif
{
	public:
#ifdef FLYLINKDC_USE_PROFILER_CS
		LockBase(T& cs, const char* function, int line) : cs(cs), CFlyLockProfiler(function, line)
#else
		LockBase(T& cs) : cs(cs)
#endif
		{
			cs.lock();
		}
		~LockBase()
		{
			cs.unlock();
		}

	private:
		T& cs;
};

/*
typedef LockBase<CriticalSection> Lock;
*/

#ifdef FLYLINKDC_USE_PROFILER_CS
#define CFlyLock(cs) LockBase<decltype(cs)> l_lock(cs, __FUNCTION__, __LINE__);
#define CFlyLockLine(cs, line) LockBase<decltype(cs)> l_lock(cs, line, 0);
#else
#define CFlyLock(cs) LockBase<decltype(cs)> l_lock(cs);
#endif

/*
#ifdef IRAINMAN_USE_SPIN_LOCK
typedef LockBase<FastCriticalSection> FastLock;
#else
typedef Lock FastLock;
#endif // IRAINMAN_USE_SPIN_LOCK
*/

#ifdef FLYLINKDC_USE_PROFILER_CS
#define CFlyFastLock(cs) LockBase<decltype(cs)> l_lock(cs, __FUNCTION__, __LINE__);
#else
#define CFlyFastLock(cs) LockBase<decltype(cs)> l_lock(cs);
#endif

#if defined(IRAINMAN_USE_SHARED_SPIN_LOCK)

// Multi-reader Single-writer concurrency base class for Win32
//
// http://www.viksoe.dk/code/rwmonitor.htm
//
// This code has been modified for use in the core of FlylinkDC++.
// In addition to this was added the functional upgrade shared lock to unique lock and downgrade back,
// increased performance, also adds support for recursive entry for both locks
// in full shared-critical section ( slowly :( ).
// Author modifications Alexey Solomin (a.rainman@gmail.com), 2012.


#ifdef _DEBUG
//# define RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
# define RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
# ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
#  define RECURSIVE_SHARED_CRITICAL_SECTION_NOT_ALLOW_UNIQUE_RECUSIVE_ENTRY_AFTER_SHARED_LOCK // potential not save.
# endif // RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
#endif


#else // IRAINMAN_USE_SHARED_SPIN_LOCK

typedef CriticalSection SharedCriticalSection;
typedef Lock SharedLock;
typedef Lock UniqueLock;

typedef FastCriticalSection FastSharedCriticalSection;
typedef FastLock FastSharedLock;
typedef FastLock FastUniqueLock;

#endif // IRAINMAN_USE_SHARED_SPIN_LOCK

#endif // !defined(THREAD_H)
