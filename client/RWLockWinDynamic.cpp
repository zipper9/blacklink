/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* Modified for blacklink */

#include "stdinc.h"
#include "RWLockWinDynamic.h"

typedef void(WINAPI *PInitializeSRWLock)(PSRWLOCK);

typedef void(WINAPI *PAcquireSRWLockExclusive)(PSRWLOCK);
typedef void(WINAPI *PReleaseSRWLockExclusive)(PSRWLOCK);

typedef void(WINAPI *PAcquireSRWLockShared)(PSRWLOCK);
typedef void(WINAPI *PReleaseSRWLockShared)(PSRWLOCK);

static PInitializeSRWLock initializeSRWLock;
static PAcquireSRWLockExclusive acquireSRWLockExclusive;
static PAcquireSRWLockShared acquireSRWLockShared;
static PReleaseSRWLockShared releaseSRWLockShared;
static PReleaseSRWLockExclusive releaseSRWLockExclusive;

static bool moduleLoadAttempted = false;
static bool nativeRWLocksSupported = false;

static bool loadModule()
{
	if (moduleLoadAttempted) return nativeRWLocksSupported;

	HMODULE library = GetModuleHandle(_T("Kernel32.dll"));
	if (library)
	{
		initializeSRWLock = (PInitializeSRWLock)GetProcAddress(library, "InitializeSRWLock");
		acquireSRWLockExclusive = (PAcquireSRWLockExclusive)GetProcAddress(library, "AcquireSRWLockExclusive");
		releaseSRWLockExclusive = (PReleaseSRWLockExclusive)GetProcAddress(library, "ReleaseSRWLockExclusive");
		acquireSRWLockShared = (PAcquireSRWLockShared)GetProcAddress(library, "AcquireSRWLockShared");
		releaseSRWLockShared = (PReleaseSRWLockShared)GetProcAddress(library, "ReleaseSRWLockShared");

		if (initializeSRWLock && acquireSRWLockExclusive && releaseSRWLockExclusive && acquireSRWLockShared && releaseSRWLockShared)
			nativeRWLocksSupported = true;
	}
	moduleLoadAttempted = true;
	return nativeRWLocksSupported;
}

RWLockWinDynamic::RWLockWinDynamic()
{
	initializeSRWLock(&lock);
}

RWLockWinDynamic *RWLockWinDynamic::create()
{
	if (!loadModule()) return nullptr;
	return new RWLockWinDynamic;
}

#ifdef LOCK_DEBUG
void RWLockWinDynamic::acquireShared(const char* filename, int line)
{
	acquireSRWLockShared(&lock);
	ownerFile = filename;
	ownerLine = line;
	lockType = 1;
}

void RWLockWinDynamic::acquireExclusive(const char* filename, int line)
{
	acquireSRWLockExclusive(&lock);
	ownerFile = filename;
	ownerLine = line;
	lockType = 2;
}

void RWLockWinDynamic::releaseShared()
{
	releaseSRWLockShared(&lock);
	lockType = 0;
}

void RWLockWinDynamic::releaseExclusive()
{
	releaseSRWLockExclusive(&lock);
	lockType = 0;
}
#else
void RWLockWinDynamic::acquireShared()
{
	acquireSRWLockShared(&lock);
}

void RWLockWinDynamic::acquireExclusive()
{
	acquireSRWLockExclusive(&lock);
}

void RWLockWinDynamic::releaseShared()
{
	releaseSRWLockShared(&lock);
}

void RWLockWinDynamic::releaseExclusive()
{
	releaseSRWLockExclusive(&lock);
}
#endif
