#include "stdinc.h"
#include "RWLockWin.h"

#ifndef OSVER_WIN_XP
RWLockWin::RWLockWin()
{
	InitializeSRWLock(&lock);
}

#ifdef LOCK_DEBUG
void RWLockWin::acquireShared(const char* filename, int line)
{
	AcquireSRWLockShared(&lock);
	ownerFile = filename;
	ownerLine = line;
	lockType = 1;
}

void RWLockWin::acquireExclusive(const char* filename, int line)
{
	AcquireSRWLockExclusive(&lock);
	ownerFile = filename;
	ownerLine = line;
	lockType = 2;
}

void RWLockWin::releaseShared()
{
	ReleaseSRWLockShared(&lock);
	lockType = 0;
}

void RWLockWin::releaseExclusive()
{
	ReleaseSRWLockExclusive(&lock);
	lockType = 0;
}
#else
void RWLockWin::acquireShared()
{
	AcquireSRWLockShared(&lock);
}

void RWLockWin::acquireExclusive()
{
	AcquireSRWLockExclusive(&lock);
}

void RWLockWin::releaseShared()
{
	ReleaseSRWLockShared(&lock);
}

void RWLockWin::releaseExclusive()
{
	ReleaseSRWLockExclusive(&lock);
}
#endif

#endif // OSVER_WIN_XP
