/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "File.h"
#include "StrUtil.h"
#include "BaseUtil.h"
#include "Path.h"
#include "Text.h"
#include <memory>
#include <string.h>

#ifndef _CONSOLE
#include "CompatibilityManager.h"
#endif

#ifndef _WIN32
#include <unistd.h>
#include <fnmatch.h>
#include <sys/statvfs.h>
#ifdef _DARWIN_C_SOURCE
#define st_mtim st_mtimespec
#endif
#endif

#if defined(_WIN32) && defined(_CONSOLE)
namespace CompatibilityManager
{
	static FINDEX_INFO_LEVELS findFileLevel = FindExInfoStandard;
	static DWORD findFileFlags = 0;
}
#endif

const FileFindIter FileFindIter::end;

void File::close() noexcept
{
	if (h != INVALID_FILE_HANDLE)
	{
#ifdef _WIN32
		CloseHandle(h);
#else
		::close(h);
#endif
		h = INVALID_FILE_HANDLE;
	}
}

void File::closeStream()
{
	close();
}

void File::setSize(int64_t newSize)
{
	int64_t pos = getPos();
	setPos(newSize);
	setEOF();
	setPos(pos);
}

#ifdef _WIN32
File::File(const wstring& fileName, int access, int mode, bool isAbsolutePath, int perm)
{
	init(fileName, access, mode, isAbsolutePath, perm);
}

File::File(const string& fileName, int access, int mode, bool isAbsolutePath, int perm)
{
	init(Text::utf8ToWide(fileName), access, mode, isAbsolutePath, perm);
}

void File::init(const wstring& fileName, int access, int mode, bool isAbsolutePath, int)
{
	dcassert(access == static_cast<int>(WRITE) || access == static_cast<int>(READ) || access == static_cast<int>(RW));
	int m;
	if (mode & OPEN)
	{
		if (mode & CREATE)
			m = (mode & TRUNCATE) ? CREATE_ALWAYS : OPEN_ALWAYS;
		else
			m = (mode & TRUNCATE) ? TRUNCATE_EXISTING : OPEN_EXISTING;
	}
	else
	{
		if (mode & CREATE)
			m = (mode & TRUNCATE) ? CREATE_ALWAYS : CREATE_NEW;
		else
		{
			m = 0;
			dcassert(0);
		}
	}
	const DWORD shared = FILE_SHARE_READ | (mode & SHARED ? (FILE_SHARE_WRITE | FILE_SHARE_DELETE) : 0);
	const wstring outPath = isAbsolutePath ? formatPath(fileName) : fileName;
	h = ::CreateFileW(outPath.c_str(), access, shared, nullptr, m, mode & NO_CACHE_HINT ? 0 : FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (h == INVALID_HANDLE_VALUE)
	{
#ifdef _DEBUG
		string errorText = Util::translateError();
		string text = "Error = " + Util::toString(GetLastError()) + ", File = " + Text::wideToUtf8(outPath) + '\n';
		DumpDebugMessage(_T("file-error.log"), text.c_str(), text.length(), false);
		throw FileException(errorText);
#else
		throw FileException(Util::translateError());
#endif
	}
}

static inline bool isDrivePath(const wstring& path)
{
	return path.length() >= 3 && path[1] == L':' && path[2] == L'\\';
}

static inline bool isUncPath(const wstring& path)
{
	return path.length() >= 4 && path[0] == L'\\' && path[1] == L'\\' &&
	       path[2] != L'.' && path[2] != L'?';
}

wstring File::formatPath(const wstring& path) noexcept
{
	dcassert(path.find(L'/') == wstring::npos);
	if (isDrivePath(path)) return L"\\\\?\\" + path;
	if (isUncPath(path)) return L"\\\\?\\UNC\\" + path.substr(2);
	return path;
}

wstring File::formatPath(wstring&& path) noexcept
{
	dcassert(path.find(L'/') == wstring::npos);
	if (isDrivePath(path)) path.insert(0, L"\\\\?\\");
	else if (isUncPath(path)) path.insert(2, L"?\\UNC\\");
	return path;
}

size_t File::read(void* buf, size_t& len)
{
	DWORD x = 0;
	if (!::ReadFile(h, buf, (DWORD)len, &x, NULL))
	{
		throw FileException(Util::translateError());
	}
	len = x;
	return x;
}

size_t File::write(const void* buf, size_t len)
{
	DWORD x = 0;
	if (!::WriteFile(h, buf, (DWORD)len, &x, NULL))
		throw FileException(Util::translateError());
	if (x != len)
		throw FileException("Error in File::write x != len");
	return x;
}

uint64_t File::getTimeStamp() const noexcept
{
	FILETIME ft;
	if (!GetFileTime(h, nullptr, nullptr, &ft)) return 0;
	return *reinterpret_cast<uint64_t*>(&ft);
}

uint64_t File::getTimeStamp(const string& fileName) noexcept
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExW(formatPath(Text::utf8ToWide(fileName)).c_str(), GetFileExInfoStandard, &data)) return 0;
	return *reinterpret_cast<const uint64_t*>(&data.ftLastWriteTime);
}

void File::setTimeStamp(const string& fileName, const uint64_t stamp)
{
	HANDLE hCreate = CreateFileW(formatPath(Text::utf8ToWide(fileName)).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hCreate == INVALID_HANDLE_VALUE)
		throw FileException(Util::translateError() + ": " + fileName);
	if (!SetFileTime(hCreate, NULL, NULL, (FILETIME*)&stamp))
	{
		CloseHandle(hCreate);
		throw FileException(Util::translateError() + ": " + fileName);
	}
	CloseHandle(hCreate);
}

uint64_t File::timeStampToUnixTime(uint64_t ts)
{
	return ts / 10000000ull - 11644473600ull;
}

int64_t File::getSize() const noexcept
{
	LARGE_INTEGER x;
	if (!GetFileSizeEx(h, &x)) return -1;
	return x.QuadPart;
}

int64_t File::getPos() const noexcept
{
	LARGE_INTEGER x;
	x.QuadPart = 0;
	return SetFilePointerEx(h, x, &x, FILE_CURRENT) ? x.QuadPart : -1;
}

void File::setPos(int64_t pos)
{
	LARGE_INTEGER x;
	x.QuadPart = pos;
	if (!SetFilePointerEx(h, x, &x, FILE_BEGIN))
		throw FileException(Util::translateError());
}

int64_t File::setEndPos(int64_t pos)
{
	LARGE_INTEGER x;
	x.QuadPart = pos;
	if (!SetFilePointerEx(h, x, &x, FILE_END))
		throw FileException(Util::translateError());
	return x.QuadPart;
}

void File::movePos(int64_t pos)
{
	LARGE_INTEGER x;
	x.QuadPart = pos;
	if (!SetFilePointerEx(h, x, &x, FILE_CURRENT))
		throw FileException(Util::translateError());
}

void File::setEOF()
{
	dcassert(isOpen());
	if (!SetEndOfFile(h))
		throw FileException(Util::translateError());
}

#if 0
string File::getRealPath() const
{
    TCHAR buf[MAX_PATH];
    auto ret = GetFinalPathNameByHandle(h, buf, MAX_PATH, FILE_NAME_OPENED);
    if (!ret)
        throw FileException(Util::translateError(GetLastError()));
    return Text::fromT(buf);
}
#endif

size_t File::flushBuffers(bool unused)
{
	if (h != INVALID_HANDLE_VALUE)
		FlushFileBuffers(h);
	return 0;
}

bool File::deleteFile(const wstring& fileName) noexcept
{
	return ::DeleteFileW(formatPath(fileName).c_str()) != FALSE;
}

bool File::deleteFile(const string& fileName) noexcept
{
	return deleteFile(Text::utf8ToWide(fileName));
}

bool File::renameFile(const wstring& source, const wstring& target) noexcept
{
	return MoveFileExW(formatPath(source).c_str(), formatPath(target).c_str(),
		MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool File::renameFile(const string& source, const string& target) noexcept
{
	return renameFile(Text::utf8ToWide(source), Text::utf8ToWide(target));
}

bool File::copyFile(const wstring& source, const wstring& target) noexcept
{
	return CopyFileW(formatPath(source).c_str(), formatPath(target).c_str(), FALSE) != FALSE;
}

bool File::copyFile(const string& source, const string& target) noexcept
{
	return copyFile(Text::utf8ToWide(source), Text::utf8ToWide(target));
}

bool File::isExist(const wstring& filename) noexcept
{
	const DWORD attr = GetFileAttributesW(formatPath(filename).c_str());
	return attr != INVALID_FILE_ATTRIBUTES;
}

bool File::isExist(const string& filename) noexcept
{
	return isExist(Text::utf8ToWide(filename));
}

bool File::getAttributes(const wstring& filename, FileAttributes& attr) noexcept
{
	return GetFileAttributesExW(formatPath(filename).c_str(), GetFileExInfoStandard, &attr.data) != FALSE;
}

bool File::getAttributes(const string& filename, FileAttributes& attr) noexcept
{
	return getAttributes(Text::utf8ToWide(filename), attr);
}

int64_t File::getSize(const wstring& filename) noexcept
{
	FileAttributes attr;
	if (!getAttributes(filename, attr)) return -1;
	return attr.getSize();
}

void File::ensureDirectory(const wstring& file) noexcept
{
	dcassert(!file.empty());
	// Skip the first dir...
	wstring::size_type start = file.find_first_of(L"\\/");
	if (start == wstring::npos)
		return;
	start++;
	while ((start = file.find_first_of(L"\\/", start)) != wstring::npos)
	{
		const wstring subdir = formatPath(file.substr(0, start + 1));
		CreateDirectoryW(subdir.c_str(), NULL);
		++start;
	}
}

void File::ensureDirectory(const string& file) noexcept
{
	return ensureDirectory(Text::utf8ToWide(file));
}

bool File::removeDirectory(const wstring& path) noexcept
{
	return RemoveDirectoryW(formatPath(path).c_str()) != FALSE;
}

bool File::removeDirectory(const string& path) noexcept
{
	return removeDirectory(Text::utf8ToWide(path));
}

bool File::getCurrentDirectory(wstring& path) noexcept
{
	DWORD size = GetCurrentDirectoryW(0, nullptr);
	if (!size) return false;
	path.resize(size);
	size = GetCurrentDirectoryW(size, &path[0]);
	if (!size)
	{
		path.clear();
		return false;
	}
	path.resize(size);
	return true;
}

bool File::getCurrentDirectory(string& path) noexcept
{
	wstring tmp;
	if (!getCurrentDirectory(tmp)) return false;
	Text::wideToUtf8(tmp, path);
	return true;
}

bool File::setCurrentDirectory(const wstring& path) noexcept
{
	return SetCurrentDirectoryW(path.c_str()) != FALSE;
}

bool File::setCurrentDirectory(const string& path) noexcept
{
	return setCurrentDirectory(Text::utf8ToWide(path));
}

bool File::getVolumeInfo(const string& path, VolumeInfo &vi) noexcept
{
	return getVolumeInfo(Text::utf8ToWide(path), vi);
}

bool File::getVolumeInfo(const wstring& path, VolumeInfo &vi) noexcept
{
	ULARGE_INTEGER space[2];
	if (!GetDiskFreeSpaceExW(formatPath(path).c_str(), nullptr, space, space + 1))
	{
		vi.totalBytes = vi.freeBytes = 0;
		return false;
	}
	vi.totalBytes = space[0].QuadPart;
	vi.freeBytes = space[1].QuadPart;
	return true;
}

StringList File::findFiles(const string& path, const string& pattern, bool appendPath /*= true */)
{
	StringList ret;

	WIN32_FIND_DATAW data;
	HANDLE hFind = FindFirstFileExW(formatPath(Text::utf8ToWide(path + pattern)).c_str(),
	                                CompatibilityManager::findFileLevel,
	                                &data,
	                                FindExSearchNameMatch,
	                                nullptr,
	                                0);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			ret.emplace_back(Text::wideToUtf8(data.cFileName));
			string& filename = ret.back();
			if (appendPath)
				filename.insert(0, path);
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				filename += PATH_SEPARATOR;
		}
		while (FindNextFileW(hFind, &data));
		FindClose(hFind);
	}
	return ret;
}

FileFindIter::FileFindIter(const string& path)
{
	init(Text::utf8ToWide(path));
}

FileFindIter::~FileFindIter()
{
	if (handle != INVALID_HANDLE_VALUE)
		FindClose(handle);
}

void FileFindIter::init(const wstring& path)
{
	handle = FindFirstFileExW(File::formatPath(path).c_str(),
	                          CompatibilityManager::findFileLevel,
	                          &data,
	                          FindExSearchNameMatch,
	                          nullptr,
	                          CompatibilityManager::findFileFlags);
}

FileFindIter& FileFindIter::operator++()
{
	if (handle != INVALID_HANDLE_VALUE)
	{
		if (!::FindNextFileW(handle, &data))
		{
			FindClose(handle);
			handle = INVALID_HANDLE_VALUE;
		}
	}
	return *this;
}

string FileFindIter::DirData::getFileName() const
{
	return Text::wideToUtf8(cFileName);
}

bool FileFindIter::DirData::isDirectory() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileFindIter::DirData::isReadOnly() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
}

bool FileFindIter::DirData::isHidden() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0
	        /*|| (CompatibilityManager::isWine() && cFileName[0] == L'.')*/;
}

bool FileFindIter::DirData::isLink() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

int64_t FileFindIter::DirData::getSize() const
{
	return (int64_t)nFileSizeLow | ((int64_t)nFileSizeHigh) << 32;
}

uint64_t FileFindIter::DirData::getTimeStamp() const
{
	return *reinterpret_cast<const uint64_t*>(&ftLastWriteTime);
}

bool FileFindIter::DirData::isSystem() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
}

bool FileFindIter::DirData::isTemporary() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) != 0;
}

bool FileFindIter::DirData::isVirtual() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL) != 0;
}

bool FileAttributes::isDirectory() const
{
	return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileAttributes::isReadOnly() const
{
	return (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
}

bool FileAttributes::isHidden() const
{
	return (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
}

bool FileAttributes::isLink() const
{
	return (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool FileAttributes::isSystem() const
{
	return (data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
}

bool FileAttributes::isTemporary() const
{
	return (data.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) != 0;
}

bool FileAttributes::isVirtual() const
{
	return (data.dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL) != 0;
}

int64_t FileAttributes::getSize() const
{
	return (int64_t) data.nFileSizeHigh << 32 | data.nFileSizeLow;
}

uint64_t FileAttributes::getTimeStamp() const
{
	return *reinterpret_cast<const uint64_t*>(&data.ftLastWriteTime);
}

#else

File::File(const string& fileName, int access, int mode, bool isAbsolutePath, int perm)
{
	init(fileName, access, mode, isAbsolutePath, perm);
}

void File::init(const string& fileName, int access, int mode, bool, int perm)
{
	dcassert(access == static_cast<int>(WRITE) || access == static_cast<int>(READ) || access == static_cast<int>(RW));
	int m;
	if (mode & OPEN)
	{
		if (mode & CREATE)
			m = (mode & TRUNCATE) ? O_CREAT | O_TRUNC : O_CREAT;
		else
			m = (mode & TRUNCATE) ? O_TRUNC : 0;
	}
	else if (mode & CREATE)
		m = O_CREAT | ((mode & TRUNCATE) ? O_TRUNC : O_EXCL);
	else
	{
		m = 0;
		dcassert(0);
	}
	switch (access)
	{
		case READ:
			m |= O_RDONLY;
			break;
		case WRITE:
			m |= O_WRONLY;
			break;
		default:
			m |= O_RDWR;
	}

	h = open(fileName.c_str(), m, perm);
	if (h == -1)
		throw FileException(Util::translateError());
}

size_t File::read(void* buf, size_t& len)
{
	while (true)
	{
		ssize_t result = ::read(h, buf, len);
		if (result == -1)
		{
			if (errno == EINTR) continue;
			throw FileException(Util::translateError());
		}
		len = result;
		break;
	}
	return len;
}

size_t File::write(const void* buf, size_t len)
{
	size_t left = len;
	while (left)
	{
		ssize_t result = ::write(h, buf, left);
		if (result == -1)
		{
			if (errno == EINTR) continue;
			throw FileException(Util::translateError());
		}
		left -= result;
		buf = (const uint8_t*) buf + result;
	}
	return len;
}

static inline uint64_t timeSpecToLinear(const struct timespec& ts)
{
	return (uint64_t) 1000000000 * ts.tv_sec + ts.tv_nsec;
}

static inline void timeSpecFromLinear(struct timespec& ts, uint64_t val)
{
	ts.tv_sec = val / 1000000000;
	ts.tv_nsec = val % 1000000000;
}

uint64_t File::getTimeStamp() const noexcept
{
	struct stat st;
	return fstat(h, &st) ? 0 : timeSpecToLinear(st.st_mtim);
}

uint64_t File::getTimeStamp(const string& fileName) noexcept
{
	struct stat st;
	return stat(fileName.c_str(), &st) ? 0 : timeSpecToLinear(st.st_mtim);
}

void File::setTimeStamp(const string& fileName, const uint64_t stamp)
{
	struct timespec ts[2];
	timeSpecFromLinear(ts[0], stamp);
	ts[1] = ts[0];
	if (utimensat(AT_FDCWD, fileName.c_str(), ts, 0))
		throw FileException(Util::translateError() + ": " + fileName);
}

uint64_t File::timeStampToUnixTime(uint64_t ts)
{
	return ts / 1000000000;
}

int64_t File::getSize() const noexcept
{
	struct stat st;
	if (fstat(h, &st)) return -1;
	return st.st_size;
}

int64_t File::getPos() const noexcept
{
	return (int64_t) lseek(h, 0, SEEK_CUR);
}

void File::setPos(int64_t pos)
{
	if (lseek(h, pos, SEEK_SET) == (off_t) -1)
		throw FileException(Util::translateError());
}

int64_t File::setEndPos(int64_t pos)
{
	off_t result = lseek(h, pos, SEEK_END);
	if (result == (off_t) -1)
		throw FileException(Util::translateError());
	return (int64_t) result;
}

void File::movePos(int64_t pos)
{
	if (lseek(h, pos, SEEK_CUR) == (off_t) -1)
		throw FileException(Util::translateError());
}

void File::setEOF()
{
	dcassert(isOpen());
	auto pos = lseek(h, 0, SEEK_CUR);
	auto fileSize = lseek(h, 0, SEEK_END);
	if (pos == fileSize) return;
	if (fileSize < pos)
	{
		char c = 0;
		if (lseek(h, pos, SEEK_SET) == (off_t) -1 || ::write(h, &c, 1) != 1)
			throw FileException(Util::translateError());
	}
	if (ftruncate(h, pos))
		throw FileException(Util::translateError());
	lseek(h, pos, SEEK_SET);
}


size_t File::flushBuffers(bool force)
{
	if (force && h != -1) fsync(h);
	return 0;
}

bool File::deleteFile(const string& fileName) noexcept
{
	return unlink(fileName.c_str()) == 0;
}

bool File::renameFile(const string& source, const string& target) noexcept
{
	if (!rename(source.c_str(), target.c_str())) return true;
	if (errno != EXDEV) return false;
	return copyFile(source, target) && deleteFile(source);
}

bool File::copyFile(const string& source, const string& target) noexcept
{
	static const size_t BUF_SIZE = 256 * 1024;
	int in = open(source.c_str(), O_RDONLY);
	if (in < 0) return false;
	struct stat st;
	if (fstat(in, &st))
	{
		::close(in);
		return false;
	}
	int out = open(target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
	if (out < 0)
	{
		::close(in);
		return false;
	}
	std::unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);
	bool result = false;
	while (true)
	{
		ssize_t size = ::read(in, buf.get(), BUF_SIZE);
		if (!size)
		{
			result = true;
			break;
		}
		if (size < 0 || ::write(out, buf.get(), size) != size) break;
	}
	int err = errno;
	::close(in);
	::close(out);
	if (!result)
	{
		unlink(target.c_str());
		errno = err;
	}
	return result;
}

bool File::isExist(const string& filename) noexcept
{
	struct stat st;
	return stat(filename.c_str(), &st) == 0;
}

bool File::getAttributes(const string& filename, FileAttributes& attr) noexcept
{
	return stat(filename.c_str(), &attr.data) == 0;
}

void File::ensureDirectory(const string& file, int perm) noexcept
{
	dcassert(!file.empty());
	// Skip the first dir...
	string::size_type start = 0;
	if (!file.empty() && file[0] == '/') start++;
	string tmp = file;
	while ((start = tmp.find('/', start)) != string::npos)
	{
		tmp[start] = 0;
		mkdir(tmp.c_str(), perm);
		tmp[start++] = '/';
	}
}

bool File::removeDirectory(const string& path) noexcept
{
	return rmdir(path.c_str()) == 0;
}

bool File::getCurrentDirectory(string& path) noexcept
{
	path.resize(PATH_MAX);
	while (true)
	{
		char* result = getcwd(&path[0], path.length());
		if (!result)
		{
			if (errno != ERANGE)
			{
				path.clear();
				return false;
			}
			path.resize(path.length() * 2);
		}
		break;
	}
	path.resize(strlen(path.c_str()));
	return true;
}

bool File::setCurrentDirectory(const string& path) noexcept
{
	return chdir(path.c_str()) == 0;
}

bool File::getVolumeInfo(const string& path, VolumeInfo &vi) noexcept
{
	struct statvfs st;
	if (statvfs(path.c_str(), &st))
	{
		vi.totalBytes = vi.freeBytes = 0;
		return false;
	}
	vi.totalBytes = static_cast<uint64_t>(st.f_blocks) * st.f_bsize;
	vi.freeBytes = static_cast<uint64_t>(st.f_bavail) * st.f_bsize;
	return true;
}

StringList File::findFiles(const string& path, const string& pattern, bool appendPath /*= true */)
{
	StringList ret;
	DIR* d = opendir(path.c_str());
	bool appendSlash = !path.empty() && path.back() != PATH_SEPARATOR;
	if (d)
	{
		for (struct dirent* ent = readdir(d); ent; ent = readdir(d))
		{
			if (!pattern.empty() && fnmatch(pattern.c_str(), ent->d_name, FNM_PATHNAME | FNM_NOESCAPE)) continue;
#if defined DT_DIR && defined DT_REG && defined DT_LNK && defined DT_UNKNOWN
			if (ent->d_type == DT_DIR || ent->d_type == DT_REG)
			{
				ret.emplace_back(ent->d_name);
				string& filename = ret.back();
				if (appendPath)
				{
					if (appendSlash) filename.insert(0, 1, PATH_SEPARATOR);
					filename.insert(0, path);
				}
				if (ent->d_type == DT_DIR) filename += PATH_SEPARATOR;
				continue;
			}
			if (ent->d_type != DT_LNK && ent->d_type != DT_UNKNOWN) continue;
#endif
			struct stat st;
			if (fstatat(dirfd(d), ent->d_name, &st, 0) == 0 && (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)))
			{
				ret.emplace_back(ent->d_name);
				string& filename = ret.back();
				if (appendPath)
				{
					if (appendSlash) filename.insert(0, 1, PATH_SEPARATOR);
					filename.insert(0, path);
				}
				if (S_ISDIR(st.st_mode)) filename += PATH_SEPARATOR;
			}
		}
		closedir(d);
	}
	return ret;
}

FileFindIter::FileFindIter(const string& path)
{
	init(path);
}

FileFindIter::~FileFindIter()
{
	if (handle) closedir(handle);
}

void FileFindIter::init(const string& path)
{
	pattern = path;
	string::size_type pos = pattern.rfind('/');
	if (pos == string::npos)
	{
		pattern.insert(0, "./");
		pos = 1;
	}
	string dir = path.substr(0, pos + 1);
	handle = opendir(dir.c_str());
	if (!handle) return;
	pattern.erase(0, pos + 1);
	if (pattern == "*") pattern.clear();
	moveNext();
}

void FileFindIter::moveNext()
{
	while (true)
	{
		data.ent = readdir(handle);
		if (!data.ent)
		{
			closedir(handle);
			handle = nullptr;
			data.ent = nullptr;
			return;
		}
		if (data.ent->d_name[0] == '.')
		{
			if (data.ent->d_name[1] == 0) continue;
			if (data.ent->d_name[1] == '.' && data.ent->d_name[2] == 0) continue;
		}
		if (!pattern.empty() && fnmatch(pattern.c_str(), data.ent->d_name, FNM_PATHNAME | FNM_NOESCAPE)) continue;
		if (fstatat(dirfd(handle), data.ent->d_name, &data, 0) == 0) break;
	}
}

FileFindIter& FileFindIter::operator++()
{
	if (data.ent) moveNext();
	return *this;
}

string FileFindIter::DirData::getFileName() const
{
	return ent ? ent->d_name : string();
}

bool FileFindIter::DirData::isDirectory() const
{
	return S_ISDIR(st_mode);
}

bool FileFindIter::DirData::isReadOnly() const
{
	return false;
}

bool FileFindIter::DirData::isHidden() const
{
	return ent && ent->d_name[0] == '.';
}

bool FileFindIter::DirData::isLink() const
{
	return S_ISLNK(st_mode);
}

int64_t FileFindIter::DirData::getSize() const
{
	return st_size;
}

uint64_t FileFindIter::DirData::getTimeStamp() const
{
	return timeSpecToLinear(st_mtim);
}

bool FileFindIter::DirData::isSystem() const
{
	return false;
}

bool FileFindIter::DirData::isTemporary() const
{
	return false;
}

bool FileFindIter::DirData::isVirtual() const
{
	return !S_ISREG(st_mode);
}

bool FileAttributes::isDirectory() const
{
	return S_ISDIR(data.st_mode);
}

bool FileAttributes::isReadOnly() const
{
	return false;
}

bool FileAttributes::isHidden() const
{
	return false;
}

bool FileAttributes::isLink() const
{
	return S_ISLNK(data.st_mode);
}

bool FileAttributes::isSystem() const
{
	return false;
}

bool FileAttributes::isTemporary() const
{
	return false;
}

bool FileAttributes::isVirtual() const
{
	return !S_ISREG(data.st_mode);
}

int64_t FileAttributes::getSize() const
{
	return data.st_size;
}

uint64_t FileAttributes::getTimeStamp() const
{
	return timeSpecToLinear(data.st_mtim);
}
#endif

int64_t File::getSize(const string& filename) noexcept
{
	FileAttributes attr;
	if (!getAttributes(filename, attr)) return -1;
	return attr.getSize();
}

string File::read(size_t len)
{
	string s(len, 0);
	size_t x = read(&s[0], len);
	if (x != s.size())
		s.resize(x);
	return s;
}

string File::read()
{
	setPos(0);
	int64_t sz = getSize();
	if (sz <= 0)
		return Util::emptyString;
	return read((uint32_t)sz);
}

bool FileFindIter::operator==(const FileFindIter& rhs) const
{
	return handle == rhs.handle;
}

bool FileFindIter::operator!=(const FileFindIter& rhs) const
{
	return handle != rhs.handle;
}
