#ifndef PROFILE_LOCKER_H_
#define PROFILE_LOCKER_H_

#include "typedefs.h"

#ifdef _WIN32
#include "w.h"
#endif

class ProfileLocker
{
	public:
		ProfileLocker() = default;
		~ProfileLocker() noexcept { unlock(); }

		ProfileLocker(const ProfileLocker&) = delete;
		ProfileLocker& operator= (const ProfileLocker&) = delete;

		bool setPath(const string& path) noexcept;
		bool lock() noexcept;
		void unlock() noexcept;
		bool isLocked() const { return locked; }

	private:
		bool locked = false;
		string profilePath;
#ifdef _WIN32
		HANDLE file = INVALID_HANDLE_VALUE;
#else
		int file = -1;
#endif

		bool createLockFile() noexcept;
		void removeLockFile() noexcept;
};

#endif // PROFILE_LOCKER_H_
