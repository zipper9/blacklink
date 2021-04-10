#ifndef RW_LOCK_POSIX_H_
#define RW_LOCK_POSIX_H_

#include <pthread.h>

class RWLockPosix
{
	public:
		RWLockPosix();
		~RWLockPosix();
		RWLockPosix(const RWLockPosix&) = delete;
		RWLockPosix& operator=(const RWLockPosix&) = delete;

#ifdef LOCK_DEBUG
		void acquireExclusive(const char* filename = nullptr, int line = 0);
		void acquireShared(const char* filename = nullptr, int line = 0);
#else
		void acquireExclusive();
		void acquireShared();
#endif

		void releaseExclusive();
		void releaseShared();

		static RWLockPosix* create() { return new RWLockPosix; }

	private:
		pthread_rwlock_t lock;
#ifdef LOCK_DEBUG
		int lockType = 0;
		const char* ownerFile = nullptr;
		int ownerLine = 0;
#endif
};

#endif // RW_LOCK_POSIX_H_
