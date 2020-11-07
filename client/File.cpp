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
#ifndef _CONSOLE
#include "LogManager.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "ClientManager.h"
#include "CompatibilityManager.h"
#endif

#ifdef _CONSOLE
namespace CompatibilityManager
{
	static FINDEX_INFO_LEVELS findFileLevel = FindExInfoStandard;
	static DWORD findFileFlags = 0;
}
#endif

const FileFindIter FileFindIter::end;

void File::init(const tstring& aFileName, int access, int mode, bool isAbsolutePath)
{
	dcassert(access == static_cast<int>(WRITE) || access == static_cast<int>(READ) || access == static_cast<int>((READ | WRITE)));
	
	int m;
	if (mode & OPEN)
	{
		if (mode & CREATE)
		{
			m = (mode & TRUNCATE) ? CREATE_ALWAYS : OPEN_ALWAYS;
		}
		else
		{
			m = (mode & TRUNCATE) ? TRUNCATE_EXISTING : OPEN_EXISTING;
		}
	}
	else
	{
		if (mode & CREATE)
		{
			m = (mode & TRUNCATE) ? CREATE_ALWAYS : CREATE_NEW;
		}
		else
		{
			m = 0;
			dcassert(0);
		}
	}
	const DWORD shared = FILE_SHARE_READ | (mode & SHARED ? (FILE_SHARE_WRITE | FILE_SHARE_DELETE) : 0);
	
	const tstring outPath = isAbsolutePath ? formatPath(aFileName) : aFileName;
	
	h = ::CreateFile(outPath.c_str(), access, shared, nullptr, m, mode & NO_CACHE_HINT ? 0 : FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	
	if (h == INVALID_HANDLE_VALUE)
	{
#ifdef _DEBUG
		string errorText = Util::translateError();
		if (outPath.find(_T(".dctmp")) != tstring::npos)
		{
			dcassert(0);
		}
		string text = "Error = " + Util::toString(GetLastError()) + ", File = " + Text::fromT(outPath) + '\n';
		DumpDebugMessage(_T("file-error.log"), text.c_str(), text.length(), false);
		throw FileException(errorText);
#else
		throw FileException(Util::translateError());
#endif
	}
}

time_t File::getLastModified() const noexcept
{
	FILETIME f = {0};
	::GetFileTime(h, NULL, NULL, &f);
	return static_cast<time_t>(convertTime(&f));
}

uint64_t File::getTimeStamp(const string& filename) noexcept
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesEx(formatPath(Text::toT(filename)).c_str(), GetFileExInfoStandard, &data)) return 0;
	return *reinterpret_cast<const uint64_t*>(&data.ftLastWriteTime);
}

void File::setTimeStamp(const string& aFileName, const uint64_t stamp)
{
	HANDLE hCreate = CreateFile(formatPath(Text::toT(aFileName)).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hCreate == INVALID_HANDLE_VALUE)
		throw FileException(Util::translateError() + ": " + aFileName);
	if (!SetFileTime(hCreate, NULL, NULL, (FILETIME*)&stamp))
	{
		CloseHandle(hCreate);
		throw FileException(Util::translateError() + ": " + aFileName);
	}
	CloseHandle(hCreate);
}

uint64_t File::convertTime(const FILETIME* f)
{
	SYSTEMTIME s = { 1970, 1, 0, 1, 0, 0, 0, 0 };
	FILETIME f2 = {0};
	if (::SystemTimeToFileTime(&s, &f2))
	{
		ULARGE_INTEGER a,b;
		a.LowPart = f->dwLowDateTime;
		a.HighPart = f->dwHighDateTime;
		b.LowPart = f2.dwLowDateTime;
		b.HighPart = f2.dwHighDateTime;
		return (a.QuadPart - b.QuadPart) / (10000000LL); // 100ns -> s
	}
	return 0;
}

bool File::isOpen() const noexcept
{
	return h != INVALID_HANDLE_VALUE;
}

void File::close() noexcept
{
	if (isOpen())
	{
		CloseHandle(h);
		h = INVALID_HANDLE_VALUE;
	}
}

int64_t File::getSize() const noexcept
{
	LARGE_INTEGER x;
	if (!GetFileSizeEx(h, &x)) return -1;
	return x.QuadPart;
}

int64_t File::getPos() const noexcept
{
	// [!] IRainman use SetFilePointerEx function!
	// http://msdn.microsoft.com/en-us/library/aa365542(v=VS.85).aspx
	LARGE_INTEGER x = {0};
	BOOL bRet = ::SetFilePointerEx(h, x, &x, FILE_CURRENT);
	
	if (bRet == FALSE)
		return -1;
		
	return x.QuadPart;
}

void File::setSize(int64_t newSize)
{
	int64_t pos = getPos();
	setPos(newSize);
	setEOF();
	setPos(pos);
}

void File::setPos(int64_t pos)
{
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_BEGIN))
		throw FileException(Util::translateError());
}

int64_t File::setEndPos(int64_t pos)
{
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_END))
		throw FileException(Util::translateError());
	return x.QuadPart;
}

void File::movePos(int64_t pos)
{
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_CURRENT))
		throw FileException(Util::translateError());
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
	{
		throw FileException(Util::translateError());
	}
	if (x != len)
	{
		throw FileException("Error in File::write x != len"); //[+]PPA
	}
	return x;
}

void File::setEOF()
{
	dcassert(isOpen());
	if (!SetEndOfFile(h))
	{
		throw FileException(Util::translateError());
	}
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

size_t File::flushBuffers(bool aForce)
{
	/* TODO
	if (!aForce) {
	        return 0;
	    }
	*/
#ifndef _CONSOLE
	if (isOpen() && !ClientManager::isBeforeShutdown())
	{
		//static int g_count = 0;
		//LogManager::message("File::flush() count = " + Util::toString(++g_count));
		if (!FlushFileBuffers(h)) // TODO - похерить вообще  https://msdn.microsoft.com/ru-ru/library/windows/desktop/aa364439%28v=vs.85%29.aspx
			// и нужно юзать FILE_FLAG_NO_BUFFERING and FILE_FLAG_WRITE_THROUGH ?
			
		{
			string l_error = Util::translateError();
			l_error = "File::flush() error = " + l_error;
			LogManager::message(l_error);
			if (!ClientManager::isBeforeShutdown()) // fix https://drdump.com/Bug.aspx?ProblemID=135087
			{
				throw FileException(Util::translateError());
			}
		}
	}
#endif
	return 0;
}

void File::closeStream()
{
	close();
}

bool File::deleteFile(const tstring& fileName) noexcept
{
	bool result = ::DeleteFile(formatPath(fileName).c_str()) != FALSE;
#if !defined(_CONSOLE) && defined(_DEBUG)
	if (!result)
	{
		string error = "Error deleting file " + Text::fromT(fileName) + ": " + Util::toString(GetLastError());
		LogManager::message(error);
	}
#endif
	return result;
}

bool File::renameFile(const tstring& source, const tstring& target) noexcept
{
	return MoveFileEx(formatPath(source).c_str(), formatPath(target).c_str(),
		MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool File::copyFile(const tstring& source, const tstring& target) noexcept
{
	return CopyFile(formatPath(source).c_str(), formatPath(target).c_str(), FALSE) != FALSE;
}

int64_t File::getSize(const tstring& filename) noexcept
{
	FileAttributes attr;
	if (!getAttributes(filename, attr)) return -1;
	return attr.getSize();
}

bool File::isExist(const tstring& filename) noexcept
{
	const DWORD attr = GetFileAttributes(formatPath(filename).c_str());
	return (attr != INVALID_FILE_ATTRIBUTES);
}

bool File::getAttributes(const tstring& filename, FileAttributes& attr) noexcept
{
	return GetFileAttributesEx(formatPath(filename).c_str(), GetFileExInfoStandard, &attr.data) != FALSE;
}

tstring File::formatPath(const tstring& path) noexcept
{
	dcassert(path.find(_T('/')) == tstring::npos);
	if (path.length() > 2 && path[0] == _T('\\') && path[1] == _T('\\'))
		return _T("\\\\?\\UNC\\") + path.substr(2);
	if (path.length() > 2 && path[1] == _T(':') && path[2] == _T('\\'))
		return _T("\\\\?\\") + path;
	return path;
}

tstring File::formatPath(tstring&& path) noexcept
{
	dcassert(path.find(_T('/')) == tstring::npos);
	if (path.length() > 2 && path[0] == _T('\\') && path[1] == _T('\\'))
	{
		path.erase(0, 2);
		path.insert(0, _T("\\\\?\\UNC\\"));
	}
	else
	if (path.length() > 2 && path[1] == _T(':') && path[2] == _T('\\'))
		path.insert(0, _T("\\\\?\\"));
	return path;
}

void File::ensureDirectory(const tstring& file) noexcept
{
	dcassert(!file.empty());
	// Skip the first dir...
	tstring::size_type start = file.find_first_of(_T("\\/"));
	if (start == tstring::npos)
		return;
	start++;
	while ((start = file.find_first_of(_T("\\/"), start)) != tstring::npos)
	{
		const tstring subdir = formatPath(file.substr(0, start + 1));
		::CreateDirectory(subdir.c_str(), NULL);
		++start;
	}
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

uint64_t File::calcFilesSize(const string& path, const string& pattern)
{
	uint64_t size = 0;
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFileEx(formatPath(Text::toT(path + pattern)).c_str(),
	                               CompatibilityManager::findFileLevel,
	                               &data,
	                               FindExSearchNameMatch,
	                               nullptr,
	                               0);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				size += (int64_t)data.nFileSizeLow | ((int64_t)data.nFileSizeHigh) << 32;
		}
		while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	return size;
}

StringList File::findFiles(const string& path, const string& pattern, bool appendPath /*= true */)
{
	StringList ret;
	
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFileEx(formatPath(Text::toT(path + pattern)).c_str(),
	                               CompatibilityManager::findFileLevel,
	                               &data,
	                               FindExSearchNameMatch,
	                               nullptr,
	                               0);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			ret.emplace_back(Text::fromT(data.cFileName));
			string& filename = ret.back();
			if (appendPath)
				filename.insert(0, path);
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				filename += PATH_SEPARATOR;
		}
		while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	return ret;
}

void FileFindIter::init(const tstring& path)
{
	handle = FindFirstFileEx(File::formatPath(path).c_str(),
	                         CompatibilityManager::findFileLevel,
	                         &data,
	                         FindExSearchNameMatch,
	                         nullptr,
	                         CompatibilityManager::findFileFlags);
}

FileFindIter::~FileFindIter()
{
	if (handle != INVALID_HANDLE_VALUE)
	{
		FindClose(handle);
	}
}

FileFindIter& FileFindIter::operator++()
{
	if (handle != INVALID_HANDLE_VALUE)
	{
		if (!::FindNextFile(handle, &data))
		{
			FindClose(handle);
			handle = INVALID_HANDLE_VALUE;
		}
	}
	return *this;
}

bool FileFindIter::operator==(const FileFindIter& rhs) const
{
	return handle == rhs.handle;
}

bool FileFindIter::operator!=(const FileFindIter& rhs) const
{
	return handle != rhs.handle;
}

FileFindIter::DirData::DirData()
{
// TODO   WIN32_FIND_DATA l_init = {0};
// TODO   *this = l_init;
}

string FileFindIter::DirData::getFileName() const
{
	return Text::fromT(cFileName);
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

int64_t FileFindIter::DirData::getLastWriteTime() const
{
	return File::convertTime(&ftLastWriteTime);
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

int64_t FileAttributes::getLastWriteTime() const
{
	return File::convertTime(&data.ftLastWriteTime);
}
