#include "stdinc.h"
#include "RWLockPosix.h"

RWLockPosix::RWLockPosix()
{
	pthread_rwlock_init(&lock, nullptr);
}

RWLockPosix::~RWLockPosix()
{
	pthread_rwlock_destroy(&lock);
}

#ifdef LOCK_DEBUG
void RWLockPosix::acquireShared(const char* filename, int line)
{
	pthread_rwlock_rdlock(&lock);
	ownerFile = filename;
	ownerLine = line;
	lockType = 1;
}

void RWLockPosix::acquireExclusive(const char* filename, int line)
{
	pthread_rwlock_wrlock(&lock);
	ownerFile = filename;
	ownerLine = line;
	lockType = 2;
}

void RWLockPosix::releaseShared()
{
	pthread_rwlock_unlock(&lock);
	lockType = 0;
}

void RWLockPosix::releaseExclusive()
{
	pthread_rwlock_unlock(&lock);
	lockType = 0;
}
#else
void RWLockPosix::acquireShared()
{
	pthread_rwlock_rdlock(&lock);
}

void RWLockPosix::acquireExclusive()
{
	pthread_rwlock_wrlock(&lock);
}

void RWLockPosix::releaseShared()
{
	pthread_rwlock_unlock(&lock);
}

void RWLockPosix::releaseExclusive()
{
	pthread_rwlock_unlock(&lock);
}
#endif
