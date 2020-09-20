#ifndef RW_LOCK_WRAPPER_H_
#define RW_LOCK_WRAPPER_H_

class RWLockWrapper
{
	public:
		RWLockWrapper() {}
		virtual ~RWLockWrapper() {}

		RWLockWrapper(const RWLockWrapper&) = delete;
		RWLockWrapper& operator=(const RWLockWrapper&) = delete;

#ifdef LOCK_DEBUG
		virtual void acquireExclusive(const char* filename = nullptr, int line = 0) = 0;
		virtual void acquireShared(const char* filename = nullptr, int line = 0) = 0;
#else
		virtual void acquireExclusive() = 0;
		virtual void acquireShared() = 0;
#endif
		
		virtual void releaseExclusive() = 0;
		virtual void releaseShared() = 0;

		static RWLockWrapper *create();

#ifdef LOCK_DEBUG
		int lockType = 0;
		const char* ownerFile = nullptr;
		int ownerLine = 0;
#endif
};

#endif
