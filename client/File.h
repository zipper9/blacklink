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

#include "Streams.h"
#include "Text.h"

class FileAttributes;

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
			READ = GENERIC_READ,
			WRITE = GENERIC_WRITE,
			RW = READ | WRITE
		};
		File(): h(INVALID_HANDLE_VALUE)
		{
		}
		File(const tstring& aFileName, int access, int mode, bool isAbsolutePath = true)
		{
			init(aFileName, access, mode, isAbsolutePath); // [1] https://www.box.net/shared/75247d259e1ee4eab670
		}
		File(const string& aFileName, int access, int mode, bool isAbsolutePath = true) // [+] IRainman opt
		{
			init(Text::toT(aFileName), access, mode, isAbsolutePath);
		}
		File(const File&) = delete;
		File& operator= (const File&) = delete;
		File(File&& src)
		{
			h = src.h;
			src.h = INVALID_HANDLE_VALUE;
		}
		
		void init(const tstring& aFileName, int access, int mode, bool isAbsolutePath);
		bool isOpen() const noexcept;
		HANDLE getHandle() const
		{
			return h;
		}
		void close() noexcept;
		int64_t getSize() const noexcept;
		void setSize(int64_t newSize);
		
		int64_t getPos() const noexcept;
		void setPos(int64_t pos) override;
		int64_t setEndPos(int64_t pos)  throw(FileException);
		void movePos(int64_t pos)  throw(FileException);
		void setEOF();
		
		int64_t getInputSize() const override { return getSize(); }
		int64_t getTotalRead() const override { return getPos(); }
		
		size_t read(void* buf, size_t& len);
		size_t write(const void* buf, size_t len);
		// This has no effect if aForce is false
		// Generally the operating system should decide when the buffered data is written on disk
		size_t flushBuffers(bool aForce = true) override;
		
		time_t getLastModified() const noexcept;

		static uint64_t convertTime(const FILETIME* f);
		static bool copyFile(const tstring& src, const tstring& target) noexcept;
		static bool copyFile(const string& src, const string& target) noexcept
		{
			return copyFile(Text::toT(src), Text::toT(target));
		}
		static bool renameFile(const tstring& source, const tstring& target) noexcept;
		static bool renameFile(const string& source, const string& target) noexcept
		{
			return renameFile(Text::toT(source), Text::toT(target));
		}
		static bool deleteFileT(const tstring& aFileName) noexcept;
		static bool deleteFile(const string& aFileName) noexcept
		{
			return deleteFileT(Text::toT(aFileName));
		}
		
		static int64_t getSize(const tstring& fileName) noexcept;
		static int64_t getSize(const string& fileName) noexcept
		{
			return getSize(Text::toT(fileName));
		}
		static uint64_t getTimeStamp(const string& filename) noexcept;
		static void setTimeStamp(const string& filename, const uint64_t stamp);
		
		static bool isExist(const tstring& fileName) noexcept;
		static bool isExist(const string& fileName) noexcept
		{
			return isExist(Text::toT(fileName));
		}
		
		static void ensureDirectory(const tstring& filename);
		static void ensureDirectory(const string& filename)
		{
			ensureDirectory(Text::toT(filename));
		}
		
		static bool isAbsolute(const string& path) noexcept
		{
			return path.size() > 2 && (path[1] == ':' || path[0] == '/' || path[0] == '\\');
		}
		
		static string formatPath(const string& path);
		static tstring formatPath(const tstring& path);
		
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
		static uint64_t currentTime();

		static bool getAttributes(const tstring& filename, FileAttributes& attr) noexcept;
		static bool getAttributes(const string& filename, FileAttributes& attr) noexcept
		{
			return getAttributes(Text::toT(filename), attr);
		}
		
	protected:
		HANDLE h;
};

class FileFindIter
{
	private:
		/** End iterator constructor */
		FileFindIter() : handle(INVALID_HANDLE_VALUE)
		{
		}
	public:
		/** Begin iterator constructor, path in utf-8 */
		explicit FileFindIter(const tstring& path)
		{
			init(path);
		}
		explicit FileFindIter(const string& path)
		{
			init(Text::toT(path));
		}
		
		~FileFindIter();
		
		FileFindIter& operator++();
		bool operator==(const FileFindIter& rhs) const;
		bool operator!=(const FileFindIter& rhs) const;
		
		static const FileFindIter end;
		
		struct DirData
			: public WIN32_FIND_DATA
		{
			DirData();
			
			string getFileName() const;
			bool isDirectory() const;
			bool isHidden() const;
			bool isLink() const;
			int64_t getSize() const;
			int64_t getLastWriteTime() const; // REMOVE
			uint64_t getTimeStamp() const;
			bool isSystem() const;
			bool isTemporary() const;
			bool isVirtual() const;
		};
		
		DirData& operator*()
		{
			return data;
		}
		const DirData& operator*() const
		{
			return data;
		}
		DirData* operator->()
		{
			return &data;
		}
		const DirData* operator->() const
		{
			return &data;
		}
		
	private:
		HANDLE handle;
		DirData data;

		void init(const tstring& path);
};

class FileAttributes
{
	friend class File;
        public:
		bool isDirectory() const;
		bool isHidden() const;
		bool isLink() const;
		bool isSystem() const;
		bool isTemporary() const;
		bool isVirtual() const;
		int64_t getSize() const;
		uint64_t getTimeStamp() const;
		int64_t getLastWriteTime() const; // REMOVE

	private:
		WIN32_FILE_ATTRIBUTE_DATA data;
};

// on Windows, prefer _wfopen over fopen.
FILE* dcpp_fopen(const char* filename, const char* mode);

#endif // !defined(FILE_H)
