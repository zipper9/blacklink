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

#ifndef RW_LOCK_WIN_XP_H_
#define RW_LOCK_WIN_XP_H_

#include "w.h"
#include "RWLockWrapper.h"
#include "WinEvent.h"

class RWLockWinXP : public RWLockWrapper
{
	public:
		RWLockWinXP();
		virtual ~RWLockWinXP() override;

#ifdef LOCK_DEBUG
		virtual void acquireExclusive(const char* filename = nullptr, int line = 0) override;
		virtual void acquireShared(const char* filename = nullptr, int line = 0) override;
#else
		virtual void acquireExclusive() override;
		virtual void acquireShared() override;
#endif

		virtual void releaseExclusive() override;
		virtual void releaseShared() override;

		static RWLockWinXP* create() { return new RWLockWinXP; }

	private:
		CRITICAL_SECTION cs;
		WinEvent<TRUE> readEvent;
		WinEvent<FALSE> writeEvent;

		int readersActive = 0;
		bool writerActive = false;
		int readersWaiting = 0;
		int writersWaiting = 0;
};

#endif
