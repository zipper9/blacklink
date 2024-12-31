#include "stdinc.h"
#include "ProfileLocker.h"
#include "File.h"
#include "PathUtil.h"
#include "Text.h"

#ifndef _WIN32
#include <sys/file.h>
#include <errno.h>
#include <unistd.h>
#endif

void ProfileLocker::setLockFileName(const string& name) noexcept
{
	dcassert(lockFileName.empty());
	lockFileName = name;
}

bool ProfileLocker::setPath(const string& path) noexcept
{
	if (locked) return false;
	dcassert(File::isAbsolute(path));
	profilePath = path;
	Util::appendPathSeparator(profilePath);
	return true;
}

bool ProfileLocker::lock() noexcept
{
	if (locked || !createLockFile()) return false;
	locked = true;
	return true;
}

void ProfileLocker::unlock() noexcept
{
	if (!locked) return;
#ifdef _WIN32
	CloseHandle(file);
	file = INVALID_HANDLE_VALUE;
#else
	close(file);
	file = -1;
#endif
	removeLockFile();
	locked = false;
}

bool ProfileLocker::createLockFile() noexcept
{
	if (lockFileName.empty()) return false;
	string lockFile = profilePath + lockFileName;
	dcassert(File::isAbsolute(lockFile));
	char buf[64];
#ifdef _WIN32
	wstring wsLockFile = File::formatPath(Text::utf8ToWide(lockFile));
	file = CreateFileW(wsLockFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	int len = sprintf(buf, "%u", GetCurrentProcessId());
	DWORD outSize;
	WriteFile(file, buf, len, &outSize, nullptr);
#else
	file = open(lockFile.c_str(), O_CREAT | O_WRONLY, 0644);
	if (file < 0) return false;
	while (flock(file, LOCK_EX | LOCK_NB))
	{
		if (errno != EINTR)
		{
			close(file);
			file = -1;
			return false;
		}
	}
	if (!updateLater)
	{
		int len = sprintf(buf, "%u", (unsigned) getpid());
		len = write(file, buf, len);
		if (len >= 0) ftruncate(file, len);
	}
#endif
	return true;
}

void ProfileLocker::removeLockFile() noexcept
{
	string lockFile = profilePath + lockFileName;
#ifdef _WIN32
	wstring wsLockFile = File::formatPath(Text::utf8ToWide(lockFile));
	DeleteFileW(wsLockFile.c_str());
#else
	unlink(lockFile.c_str());
#endif
}

#ifndef _WIN32
bool ProfileLocker::updatePidFile() noexcept
{
	char buf[64];
	if (file < 0 || lseek(file, 0, SEEK_SET) != 0) return false;
	int len = sprintf(buf, "%u", (unsigned) getpid());
	len = write(file, buf, len);
	if (len > 0)
	{
		ftruncate(file, len);
		return true;
	}
	return false;
}
#endif
