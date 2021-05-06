#ifndef RW_LOCK_WIN_H_
#define RW_LOCK_WIN_H_

#include "w.h"

#ifndef OSVER_WIN_XP

class RWLockWin
{
	public:
		RWLockWin();
		RWLockWin(const RWLockWin&) = delete;
		RWLockWin& operator=(const RWLockWin&) = delete;

#ifdef LOCK_DEBUG
		void acquireExclusive(const char* filename = nullptr, int line = 0);
		void acquireShared(const char* filename = nullptr, int line = 0);
#else
		void acquireExclusive();
		void acquireShared();
#endif

		void releaseExclusive();
		void releaseShared();

		static RWLockWin* create() { return new RWLockWin; }

	private:
		SRWLOCK lock;
#ifdef LOCK_DEBUG
		int lockType = 0;
		const char* ownerFile = nullptr;
		int ownerLine = 0;
#endif
};

#endif // OSVER_WIN_XP

#endif // RW_LOCK_WIN_H_
