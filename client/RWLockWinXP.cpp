/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* Modified for blacklink */

#include "stdinc.h"
#include "RWLockWinXP.h"

RWLockWinXP::RWLockWinXP()
{
	InitializeCriticalSection(&cs);
	readEvent.create();
	writeEvent.create();
}

RWLockWinXP::~RWLockWinXP()
{
	DeleteCriticalSection(&cs);
}

#ifdef LOCK_DEBUG
void RWLockWinXP::acquireExclusive(const char* filename, int line)
#else
void RWLockWinXP::acquireExclusive()
#endif
{
	EnterCriticalSection(&cs);
	if (writerActive || readersActive > 0)
	{
		++writersWaiting;
		while (writerActive || readersActive > 0)
		{
			LeaveCriticalSection(&cs);
			writeEvent.wait();
			EnterCriticalSection(&cs);
		}
		--writersWaiting;
	}
	writerActive = true;
#ifdef LOCK_DEBUG
	ownerFile = filename;
	ownerLine = line;
	lockType = 2;
#endif
	LeaveCriticalSection(&cs);
}

void RWLockWinXP::releaseExclusive()
{
	EnterCriticalSection(&cs);
	writerActive = false;
	if (writersWaiting > 0)
	{
		writeEvent.notify();
	}
	else if (readersWaiting > 0)
	{
		readEvent.notify();
	}
#ifdef LOCK_DEBUG
	lockType = 0;
#endif
	LeaveCriticalSection(&cs);
}

#ifdef LOCK_DEBUG
void RWLockWinXP::acquireShared(const char* filename, int line)
#else
void RWLockWinXP::acquireShared()
#endif
{
	EnterCriticalSection(&cs);
	if (writerActive || writersWaiting > 0)
	{
		++readersWaiting;
		while (writerActive || writersWaiting > 0)
		{
			LeaveCriticalSection(&cs);
			readEvent.wait();
			EnterCriticalSection(&cs);
			if (writerActive) readEvent.reset();
		}
		if (--readersWaiting == 0) readEvent.reset();
	}
	++readersActive;
#ifdef LOCK_DEBUG
	ownerFile = filename;
	ownerLine = line;
	lockType = 1;
#endif
	LeaveCriticalSection(&cs);
}

void RWLockWinXP::releaseShared()
{
	EnterCriticalSection(&cs);
	--readersActive;
	if (readersActive == 0 && writersWaiting > 0) writeEvent.notify();
#ifdef LOCK_DEBUG
	lockType = 0;
#endif
	LeaveCriticalSection(&cs);
}
