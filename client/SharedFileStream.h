/*
 * Copyright (C) 2003-2005 RevConnect, http://www.revconnect.com
 * Copyright (C) 2011      Big Muscle, http://strongdc.sf.net
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


#ifndef _SHAREDFILESTREAM_H
#define _SHAREDFILESTREAM_H

#include "File.h"
#include "Thread.h"
#include "Util.h"

struct SharedFileHandle
{
		SharedFileHandle(const string& path, int access, int mode);
		~SharedFileHandle();
		void init(int64_t size);
		
		CriticalSection cs;
		File file;
		string path;
		int refCount;
		const int mode;
		const int access;
		int64_t lastFileSize;
		HANDLE mapping;
		uint8_t* mappingPtr;
		bool mappingError;

	private:
		void close();
};

class SharedFileStream : public IOStream
{
	public:
		typedef std::unordered_map<std::string, std::shared_ptr<SharedFileHandle>, noCaseStringHash, noCaseStringEq> SharedFileHandleMap;
		
		SharedFileStream(const string& fileName, int access, int mode, int64_t fileSize);
		~SharedFileStream();
		
		size_t write(const void* buf, size_t len) override;
		size_t read(void* buf, size_t& len) override;
		
		//int64_t getFileSize();
		int64_t getFastFileSize();
		void setSize(int64_t newSize);
		
		size_t flushBuffers(bool aForce) override;
		
		static CriticalSection csPool;
		static std::vector<bool> badDrives;
		static std::map<std::string, unsigned> filesToDelete;
		static SharedFileHandleMap readPool;
		static SharedFileHandleMap writePool;
		static void cleanup();
		static void finalCleanup();
		static void deleteFile(const std::string& file);
		static bool isBadDrive(const string& path);
		static void setBadDrive(const string& path);
		void setPos(int64_t pos) override;

	private:
		std::shared_ptr<SharedFileHandle> sfh;
		int64_t pos;

		static void cleanupL(SharedFileHandleMap& pool);
};

#endif  // _SHAREDFILESTREAM_H
