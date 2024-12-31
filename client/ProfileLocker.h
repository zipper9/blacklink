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

		void setLockFileName(const string& name) noexcept;
		bool setPath(const string& path) noexcept;
		bool lock() noexcept;
		void unlock() noexcept;
		bool isLocked() const { return locked; }

#ifndef _WIN32
		bool updatePidFile() noexcept;
		void setUpdateLater(bool flag) { updateLater = flag; }
#endif

	private:
		bool locked = false;
		string profilePath;
		string lockFileName;
#ifdef _WIN32
		HANDLE file = INVALID_HANDLE_VALUE;
#else
		int file = -1;
		bool updateLater = false;
#endif

		bool createLockFile() noexcept;
		void removeLockFile() noexcept;
};

#endif // PROFILE_LOCKER_H_
