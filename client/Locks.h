#ifndef LOCKS_H_
#define LOCKS_H_

#include "BaseThread.h"
#include "debug.h"
#include <atomic>

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

	private:
		CRITICAL_SECTION cs;
#ifdef LOCK_DEBUG
		const char* ownerFile = nullptr;
		int ownerLine = 0;
#endif
};

typedef CriticalSection RecursiveMutex;
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

	private:
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef LOCK_DEBUG
		const char* ownerFile = nullptr;
		int ownerLine = 0;
#endif
};

class RecursiveMutex
{
	public:
		RecursiveMutex()
		{
			pthread_mutexattr_t ma;
			pthread_mutexattr_init(&ma);
			pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&mutex, &ma);
			pthread_mutexattr_destroy(&ma);
		}
		RecursiveMutex(const RecursiveMutex&) = delete;
		RecursiveMutex& operator= (const RecursiveMutex&) = delete;

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

	private:
		pthread_mutex_t mutex;
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
				BaseThread::yield();
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

#endif // LOCKS_H_
