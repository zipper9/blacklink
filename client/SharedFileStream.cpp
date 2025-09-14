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

#include "stdinc.h"
#include "SharedFileStream.h"
#include "LogManager.h"
#include "GlobalState.h"
#include "SettingsManager.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "ConfCore.h"

#ifdef _WIN32
static const int64_t MAX_MAPPED_FILE_SIZE = 2ll << 30;
std::vector<bool> SharedFileStream::badDrives(26, false);
#endif

CriticalSection SharedFileStream::csPool;
std::map<std::string, unsigned > SharedFileStream::filesToDelete;
SharedFileStream::SharedFileHandleMap SharedFileStream::readPool;
SharedFileStream::SharedFileHandleMap SharedFileStream::writePool;

SharedFileHandle::SharedFileHandle(const string& path, int access, int mode) :
	refCount(1), path(path), mode(mode), access(access), lastFileSize(0)
{
}

#ifdef _WIN32
void SharedFileHandle::close()
{
	if (mapping != INVALID_HANDLE_VALUE)
	{
		if (mappingPtr)
		{
			if (!UnmapViewOfFile(mappingPtr))
			{
				LogManager::message("Failed to unmap " + path + ", Error: " + Util::translateError(), false);
			}
			mappingPtr = nullptr;
		}
		CloseHandle(mapping);
		mapping = INVALID_HANDLE_VALUE;
	}
	dcassert(mappingPtr == nullptr);
}
#endif

SharedFileHandle::~SharedFileHandle()
{
#ifdef _WIN32
	close();
#endif
}

void SharedFileHandle::init(int64_t fileSize)
{
	file.init(Text::toT(path), access, mode, true);
	lastFileSize = file.getSize();
	if (fileSize == 0 && lastFileSize > 0)
		fileSize = lastFileSize;

#ifdef _WIN32
	if ((access == File::READ || access == File::RW) && fileSize && fileSize < MAX_MAPPED_FILE_SIZE)
	{
		auto ss = SettingsManager::instance.getCoreSettings();
		ss->lockRead();
		bool useMemoryMapped = ss->getBool(Conf::USE_MEMORY_MAPPED_FILES);
		ss->unlockRead();
		if (useMemoryMapped && !path.empty() && !SharedFileStream::isBadDrive(path))
		{
			mapping = CreateFileMapping(file.getHandle(), nullptr, access == File::READ ? PAGE_READONLY : PAGE_READWRITE, 0, fileSize, NULL);
			if (mapping != nullptr)
			{
				mappingPtr = (uint8_t*) MapViewOfFile(mapping, (access == File::READ ? 0 : FILE_MAP_WRITE) | FILE_MAP_READ, 0, 0, fileSize);
				if (!mappingPtr)
				{
					if (!mappingError)
					{
						LogManager::message("Failed to map " + path + ",  Error: " + Util::translateError());
						mappingError = true;
					}
					close();
				}
			}
			else
			{
				if (!mappingError)
				{
					int error = GetLastError();
					if (error == ERROR_INVALID_PARAMETER)
					{
						SharedFileStream::setBadDrive(path);
					}
					else
					{
						LogManager::message("Failed to create file mapping for " + path + ", Error: " + Util::toString(error));
					}
					mappingError = true;
				}
			}
		}
	}
#endif
}

SharedFileStream::SharedFileStream(const string& fileName, int access, int mode, int64_t fileSize)
{
	dcassert(access == File::READ || access == File::RW);
	dcassert(!fileName.empty());

	pos = 0;
	LOCK(csPool);
	if (access == File::READ)
	{
		auto p = writePool.find(fileName);
		if (p != writePool.end())
		{
			access = File::RW;
			sfh = p->second;
			mode = sfh->mode;
		}
	}
	if (!sfh)
	{
		auto& pool = access == File::READ ? readPool : writePool;
		auto p = pool.find(fileName);
		if (p != pool.end())
			sfh = p->second;
	}
	if (sfh)
	{
		sfh->refCount++;
#ifdef DEBUG_SHARED_FILE_HANDLE
		LogManager::message("SharedFileHandle: fileName=" + fileName + ", new refCount=" + Util::toString(sfh->refCount), false);
#endif
		dcassert(sfh->access == access);
		dcassert(sfh->mode == mode);
	}
	else
	{
#ifdef DEBUG_SHARED_FILE_HANDLE
		LogManager::message("new SharedFileHandle: fileName=" + fileName + ", access=" + Util::toHexString(access), false);
#endif
		sfh = std::make_shared<SharedFileHandle>(fileName, access, mode);
		try
		{
			sfh->init(fileSize);
		}
		catch (FileException& e)
		{
			sfh.reset();
			const auto error = "SharedFileStream error: fileName="
				+ fileName + ", error=" + e.getError() + ", access=" + Util::toString(access) + ", mode=" + Util::toString(mode);
			LogManager::message(error);
			throw;
		}
		auto& pool = access == File::READ ? readPool : writePool;
		pool[fileName] = sfh;
	}
}

void SharedFileStream::deleteFile(const std::string& file)
{
	LOCK(csPool);
	auto res = filesToDelete.insert(std::make_pair(file, 0));
	dcassert(res.second);
}

void SharedFileStream::cleanupL(SharedFileHandleMap& pool)
{
	for (auto i = pool.begin(); i != pool.end();)
	{
		if (i->second && i->second->refCount == 0)
		{
			dcassert(0);
#ifdef DEBUG_SHARED_FILE_HANDLE
			LogManager::message("[!] SharedFileStream::cleanup() fileName=" + i->first, false);
#endif
			pool.erase(i);
			i = pool.begin();
		}
		else
			++i;
	}
}

void SharedFileStream::finalCleanup()
{
#ifdef _DEBUG
	{
		LOCK(csPool);
		dcassert(readPool.empty());
		dcassert(writePool.empty());
	}
#endif
	cleanup();
}

void SharedFileStream::cleanup()
{
	LOCK(csPool);
	cleanupL(readPool);
	cleanupL(writePool);
	for (auto j = filesToDelete.begin(); j != filesToDelete.end();)
	{
		if (File::isExist(j->first))
		{
			if (File::deleteFile(j->first))
			{
				filesToDelete.erase(j++);
				continue;
			}
			else
			{
				j->second++;
			}
		}
		else
		{
			filesToDelete.erase(j++);
			continue;
		}
		++j;
	}
}

SharedFileStream::~SharedFileStream()
{
	LOCK(csPool);
	
	dcassert(sfh->refCount > 0);
	if (--sfh->refCount == 0)
	{
#ifdef DEBUG_SHARED_FILE_HANDLE
		LogManager::message("SharedFileHandle: fileName=" + sfh->path + " destroyed", false);
#endif
		auto& pool = sfh->access == File::READ ? readPool : writePool;
		auto it = pool.find(sfh->path);
		if (it != pool.end())
			pool.erase(it);
		else
			dcassert(0);
	}
}

size_t SharedFileStream::write(const void* buf, size_t len)
{
	LOCK(sfh->cs);
#ifdef _WIN32
	if (sfh->mappingPtr)
	{
		memcpy(sfh->mappingPtr + pos, buf, len);
		pos += len;
	}
	else
#endif
	{
		sfh->file.setPos(pos);
		sfh->file.write(buf, len);
		pos += len;
	}
	if (sfh->lastFileSize < pos)
	{
		dcassert(0);
		sfh->lastFileSize = pos;
	}
	return len;
}

size_t SharedFileStream::read(void* buf, size_t& len)
{
	// TODO: use mappingPtr
	LOCK(sfh->cs);
	sfh->file.setPos(pos);
	len = sfh->file.read(buf, len);
	pos += len;
	return len;
}

int64_t SharedFileStream::getFastFileSize()
{
	LOCK(sfh->cs);
	//dcassert(sfh->lastFileSize == sfh->m_file.getSize());
	return sfh->lastFileSize;
}

void SharedFileStream::setSize(int64_t newSize)
{
	LOCK(sfh->cs);
	sfh->file.setSize(newSize); // FIXME: this fails when file is memory mapped
	sfh->lastFileSize = newSize;
}

size_t SharedFileStream::flushBuffers(bool force)
{
	if (!GlobalState::isShuttingDown())
	{
		try
		{
			LOCK(sfh->cs);
#ifdef _WIN32
			if (sfh->mappingPtr)
				return 0;
#endif
			return sfh->file.flushBuffers(force);
		}
		catch (const Exception&)
		{
			dcassert(0);
		}
	}
	return 0;
}

void SharedFileStream::setPos(int64_t pos)
{
	LOCK(sfh->cs);
	this->pos = pos;
}

#ifdef _WIN32
bool SharedFileStream::isBadDrive(const string& path)
{
	if (path.length() < 2 || path[1] != ':') return false;
	int ch = path[0];
	if (ch >= 'a' && ch <= 'z')
		return badDrives[ch-'a'];
	if (ch >= 'A' && ch <= 'Z')
		return badDrives[ch-'A'];
	return false;
}

void SharedFileStream::setBadDrive(const string& path)
{
	if (path.length() < 2 || path[1] != ':') return;
	int ch = path[0];
	if (ch >= 'a' && ch <= 'z')
		badDrives[ch-'a'] = true;
	if (ch >= 'A' && ch <= 'Z')
		badDrives[ch-'A'] = true;
}
#endif
