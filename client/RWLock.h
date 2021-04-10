#ifndef RW_LOCK_H_
#define RW_LOCK_H_

#ifdef _WIN32
#include "w.h"
#ifdef FLYLINKDC_SUPPORT_WIN_XP
#include "RWLockWrapper.h"
typedef RWLockWrapper RWLock;
#else
#include "RWLockWin.h"
typedef RWLockWin RWLock;
#endif
#else
#include "RWLockPosix.h"
typedef RWLockPosix RWLock;
#endif

class ReadLockScoped
{
	public:
#ifdef LOCK_DEBUG
		explicit ReadLockScoped(RWLock& rwLock, const char* filename, int line) : rwLock(rwLock) { rwLock.acquireShared(filename, line); }
#else
		explicit ReadLockScoped(RWLock& rwLock) : rwLock(rwLock) { rwLock.acquireShared(); }
#endif
		~ReadLockScoped() { rwLock.releaseShared(); }

	private:
		RWLock& rwLock;
};

class WriteLockScoped
{
	public:
#ifdef LOCK_DEBUG
		explicit WriteLockScoped(RWLock& rwLock, const char* filename, int line) : rwLock(rwLock) { rwLock.acquireExclusive(filename, line); }
#else
		explicit WriteLockScoped(RWLock& rwLock) : rwLock(rwLock) { rwLock.acquireExclusive(); }
#endif
		~WriteLockScoped() { rwLock.releaseExclusive(); }

	private:
		RWLock& rwLock;
};

#ifdef LOCK_DEBUG
#define READ_LOCK(cs)  ReadLockScoped  lock(cs, __FUNCTION__, __LINE__);
#define WRITE_LOCK(cs) WriteLockScoped lock(cs, __FUNCTION__, __LINE__);
#else
#define READ_LOCK(cs)  ReadLockScoped  lock(cs);
#define WRITE_LOCK(cs) WriteLockScoped lock(cs);
#endif

#endif // RW_LOCK_H_
