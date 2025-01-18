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

#ifndef DCPLUSPLUS_DCPP_FILE_H
#define DCPLUSPLUS_DCPP_FILE_H

#include "typedefs.h"
#include "BaseStreams.h"

#ifdef _WIN32
#include "w.h"
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#endif

class FileAttributes;

#ifdef _WIN32
#define INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#else
#define INVALID_FILE_HANDLE (-1)
#endif

class File : public IOStream
{
	public:
		enum
		{
			OPEN = 0x01,
			CREATE = 0x02,
			TRUNCATE = 0x04,
			SHARED = 0x08,
			NO_CACHE_HINT = 0x10
		};

		enum
		{
#ifdef _WIN32
			READ = GENERIC_READ,
			WRITE = GENERIC_WRITE,
			RW = READ | WRITE
#else
			READ = O_RDONLY,
			WRITE = O_WRONLY,
			RW = O_RDWR
#endif
		};

		struct VolumeInfo
		{
			uint64_t totalBytes;
			uint64_t freeBytes;
		};

		File(const string& fileName, int access, int mode, bool isAbsolutePath = true, int perm = 0644);
#ifdef _WIN32
		typedef HANDLE Handle;

		File(const wstring& fileName, int access, int mode, bool isAbsolutePath = true, int perm = 0644);
		void init(const wstring& fileName, int access, int mode, bool isAbsolutePath = true, int perm = 0644);
#else
		typedef int Handle;

		void init(const string& fileName, int access, int mode, bool isAbsolutePath = true, int perm = 0644);
#endif
		File(): h(INVALID_FILE_HANDLE) {}

		File(File&& src)
		{
			h = src.h;
			src.h = INVALID_FILE_HANDLE;
		}

		File& operator=(File&& src)
		{
			h = src.h;
			src.h = INVALID_FILE_HANDLE;
			return *this;
		}

		File(const File&) = delete;
		File& operator= (const File&) = delete;

		Handle getHandle() const { return h; }
		bool isOpen() const { return h != INVALID_FILE_HANDLE; }

		void close() noexcept;
		int64_t getSize() const noexcept;
		void setSize(int64_t newSize);
		uint64_t getTimeStamp() const noexcept;

		int64_t getPos() const noexcept;
		void setPos(int64_t pos) override;
		int64_t setEndPos(int64_t pos);
		void movePos(int64_t pos);
		void setEOF();

		int64_t getInputSize() const override { return getSize(); }
		int64_t getTotalRead() const override { return getPos(); }

		size_t read(void* buf, size_t& len) override;
		size_t write(const void* buf, size_t len) override;
		size_t flushBuffers(bool force = true) override;
		void closeStream() override;

		static bool isExist(const string& fileName) noexcept;
		static bool getAttributes(const string& filename, FileAttributes& attr) noexcept;
		static int64_t getSize(const string& fileName) noexcept;

		static bool copyFile(const string& src, const string& target) noexcept;
		static bool renameFile(const string& source, const string& target) noexcept;
		static bool deleteFile(const string& fileName) noexcept;

#ifdef _WIN32
		static void ensureDirectory(const string& filename) noexcept;
#else
		static void ensureDirectory(const string& filename, int perm = 0755) noexcept;
#endif
		static bool removeDirectory(const string& path) noexcept;
		static bool getCurrentDirectory(string& path) noexcept;
		static bool setCurrentDirectory(const string& path) noexcept;
		static bool getVolumeInfo(const string& path, VolumeInfo &vi) noexcept;

		static uint64_t getTimeStamp(const string& fileName) noexcept;
		static void setTimeStamp(const string& fileName, const uint64_t stamp);

#ifdef _WIN32
		static bool isExist(const wstring& fileName) noexcept;
		static bool getAttributes(const wstring& filename, FileAttributes& attr) noexcept;
		static int64_t getSize(const wstring& fileName) noexcept;

		static bool copyFile(const wstring& src, const wstring& target) noexcept;
		static bool renameFile(const wstring& source, const wstring& target) noexcept;
		static bool deleteFile(const wstring& fileName) noexcept;

		static void ensureDirectory(const wstring& filename) noexcept;
		static bool removeDirectory(const wstring& path) noexcept;
		static bool getCurrentDirectory(wstring& path) noexcept;
		static bool setCurrentDirectory(const wstring& path) noexcept;
		static bool getVolumeInfo(const wstring& path, VolumeInfo &vi) noexcept;

		static wstring formatPath(const wstring& path) noexcept;
		static wstring formatPath(wstring&& path) noexcept;
#endif

		static bool isAbsolute(const string& path) noexcept
		{
#ifdef _WIN32
			// Note: c:file.txt is a drive relative path!
			if (!path.empty() && (path[0] == '\\' || path[0] == '/')) return true;
			if (path.length() > 2 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) return true;
			return false;
#else
			return !path.empty() && path[0] == '/';
#endif
		}

#ifdef _WIN32
		static bool isAbsolute(const wstring& path) noexcept
		{
			if (!path.empty() && (path[0] == L'\\' || path[0] == L'/')) return true;
			if (path.length() > 2 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) return true;
			return false;
		}
#endif

		static uint64_t timeStampToUnixTime(uint64_t ts);

		virtual ~File() noexcept
		{
			close();
		}

		string read(size_t len);
		string read();
		void write(const string& str)
		{
			write(str.c_str(), str.length());
		}
		static StringList findFiles(const string& path, const string& pattern, bool appendPath = true);
		static uint64_t calcFilesSize(const string& path, const string& pattern);

	protected:
		Handle h;
};

class FileFindIter
{
	private:
#ifdef _WIN32
		FileFindIter() : handle(INVALID_HANDLE_VALUE) {}
#else
		FileFindIter() : handle(nullptr)
		{
			data.ent = nullptr;
		}
#endif

	public:
		explicit FileFindIter(const string& path);
#ifdef _WIN32
		explicit FileFindIter(const wstring& path) { init(path); }
#endif

		~FileFindIter();

		FileFindIter& operator++();
		bool operator==(const FileFindIter& rhs) const;
		bool operator!=(const FileFindIter& rhs) const;

		static const FileFindIter end;

		struct DirData :
#ifdef _WIN32
			public WIN32_FIND_DATAW
#else
			public stat
#endif
		{
			string getFileName() const;
#ifdef _WIN32
			wstring getFileNameW() const { return cFileName; }
#endif
			bool isDirectory() const;
			bool isReadOnly() const;
			bool isHidden() const;
			bool isLink() const;
			int64_t getSize() const;
			uint64_t getTimeStamp() const;
			bool isSystem() const;
			bool isTemporary() const;
			bool isVirtual() const;

#ifndef _WIN32
			struct dirent* ent;
#endif
		};

		DirData& operator*() { return data; }
		const DirData& operator*() const { return data; }
		DirData* operator->() { return &data; }
		const DirData* operator->() const { return &data; }

	private:
#ifdef _WIN32
		HANDLE handle;
#else
		DIR* handle;
		string pattern;
#endif
		DirData data;

#ifdef _WIN32
		void init(const wstring& path);
#else
		void init(const string& path);
		void moveNext();
#endif
};

class FileAttributes
{
		friend class File;

	public:
		bool isDirectory() const;
		bool isReadOnly() const;
		bool isHidden() const;
		bool isLink() const;
		bool isSystem() const;
		bool isTemporary() const;
		bool isVirtual() const;
		int64_t getSize() const;
		uint64_t getTimeStamp() const;

	private:
#ifdef _WIN32
		WIN32_FILE_ATTRIBUTE_DATA data;
#else
		struct stat data;
#endif
};

#endif // !defined(FILE_H)
