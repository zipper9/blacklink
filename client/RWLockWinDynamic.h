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

#ifndef RW_LOCK_WIN_DYNAMIC_H_
#define RW_LOCK_WIN_DYNAMIC_H_

#include "w.h"
#include "RWLockWrapper.h"

class RWLockWinDynamic : public RWLockWrapper
{
	public:
		static RWLockWinDynamic *create();

#ifdef LOCK_DEBUG
		virtual void acquireExclusive(const char* filename = nullptr, int line = 0) override;
		virtual void acquireShared(const char* filename = nullptr, int line = 0) override;
#else
		virtual void acquireExclusive() override;
		virtual void acquireShared() override;
#endif

		virtual void releaseExclusive() override;
		virtual void releaseShared() override;

	private:
		RWLockWinDynamic();
		SRWLOCK lock;
};

#endif
