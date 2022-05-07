#ifndef BASE_THREAD_H_
#define BASE_THREAD_H_

#ifdef _WIN32
#include "w.h"
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

		static uintptr_t getCurrentThreadId()
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

		static uintptr_t getCurrentThreadId()
		{
			return (uintptr_t) pthread_self();
		}
};
#endif

#endif // BASE_THREAD_H_
