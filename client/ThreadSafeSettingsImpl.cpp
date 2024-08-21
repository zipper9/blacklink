#include "stdinc.h"
#include "ThreadSafeSettingsImpl.h"

ThreadSafeSettingsImpl::ThreadSafeSettingsImpl() : lock(RWLock::create())
{
}

void ThreadSafeSettingsImpl::lockRead()
{
	lock->acquireShared();
}

void ThreadSafeSettingsImpl::unlockRead()
{
	lock->releaseShared();
}

void ThreadSafeSettingsImpl::lockWrite()
{
	lock->acquireExclusive();
}

void ThreadSafeSettingsImpl::unlockWrite()
{
	lock->releaseExclusive();
}
