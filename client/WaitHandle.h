#ifndef WAIT_HANDLE_H_
#define WAIT_HANDLE_H_

#include "w.h"
#include "debug.h"

typedef HANDLE WaitHandle;

static inline int waitMultiple(WaitHandle* events, int count)
{
	dcassert(count <= 64);
	DWORD result = WaitForMultipleObjects(count, events, FALSE, INFINITE);
	if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count) return result - WAIT_OBJECT_0;
	return -1;
}

static inline int timedWaitMultiple(WaitHandle* events, int count, int msec)
{
	dcassert(count <= 64);
	DWORD result = WaitForMultipleObjects(count, events, FALSE, msec);
	if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count) return result - WAIT_OBJECT_0;
	if (result == WAIT_TIMEOUT) return 0;
	return -1;
}

#endif // WAIT_HANDLE_H_
