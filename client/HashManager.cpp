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
#include "HashManager.h"
#include "ResourceManager.h"
#include "SimpleXML.h"
#include "LogManager.h"
#include "DatabaseManager.h"
#include "ClientManager.h"
#include "CompatibilityManager.h"
#include "ShareManager.h"

// Return values of fastHash and slowHash
enum
{
	RESULT_OK,
	RESULT_ERROR,
	RESULT_FILE_SKIPPED,
	RESULT_STOPPED
};

#ifdef _WIN32
static const uint32_t MAGIC = '++lg';
static const string g_streamName(".gltth");
#endif

static const size_t FAST_HASH_BUF_SIZE = 16 * 1024 * 1024; // Why so big?
static const size_t SLOW_HASH_BUF_SIZE = 512 * 1024;

static_assert(SLOW_HASH_BUF_SIZE <= 2*FAST_HASH_BUF_SIZE, "FAST_HASH_BUF_SIZE must be larger");

static const int MAX_SPEED = 256; // Upper limit for user supplied speed value

#ifdef _WIN32
#pragma pack(2)
struct TTHStreamHeader
{
	uint32_t magic;
	uint32_t checksum;  // xor of other TTHStreamHeader DWORDs
	uint64_t fileSize;
	uint64_t timeStamp;
	uint64_t blockSize;
	TTHValue root;
};
#pragma pack()

static inline void setCheckSum(TTHStreamHeader& header)
{
	header.magic = MAGIC;
	header.checksum = 0;
	uint32_t sum = 0;
	for (size_t i = 0; i < sizeof(TTHStreamHeader) / sizeof(uint32_t); i++)
		sum ^= ((uint32_t*) &header)[i];
	header.checksum = sum;
}

static inline bool validateCheckSum(const TTHStreamHeader& header)
{
	if (header.magic != MAGIC) return false;
	uint32_t sum = 0;
	for (size_t i = 0; i < sizeof(TTHStreamHeader) / sizeof(uint32_t); i++)
		sum ^= ((const uint32_t*) &header)[i];
	return sum == 0;
}

bool HashManager::doLoadTree(const string& filePath, TigerTree& tree, int64_t fileSize, bool checkTimestamp) noexcept
{
	try
	{
		uint64_t timeStamp = 0;
		if (checkTimestamp)
		{
			timeStamp = File::getTimeStamp(filePath);
			if (!timeStamp) return false;
		}
		File stream(filePath + ":" + g_streamName, File::READ, File::OPEN);
		size_t size = sizeof(TTHStreamHeader);
		TTHStreamHeader h;
		if (stream.read(&h, size) != sizeof(TTHStreamHeader))
			return false;
		if ((uint64_t) fileSize != h.fileSize || !validateCheckSum(h))
			return false;
		if (checkTimestamp && timeStamp != h.timeStamp)
			return false;
		const size_t dataLen = TigerTree::calcBlocks(fileSize, h.blockSize) * TTHValue::BYTES;
		size = dataLen;
		unique_ptr<uint8_t[]> buf(new uint8_t[dataLen]);
		if (stream.read(buf.get(), size) != dataLen)
			return false;
		if (!tree.load(fileSize, buf.get(), dataLen) ||
		    h.blockSize != static_cast<uint64_t>(tree.getBlockSize()) ||
		    !(tree.getRoot() == h.root))
			return false;
	}
	catch (const Exception&)
	{
		return false;
	}
	return true;
}

bool HashManager::loadTree(const string& filePath, TigerTree& tree, int64_t fileSize) noexcept
{
	if (fileSize == -1)
	{
		fileSize = File::getSize(filePath);
		if (fileSize == -1) return false;
	}
	return doLoadTree(filePath, tree, fileSize, true);
}

bool HashManager::saveTree(const string& filePath, const TigerTree& tree) noexcept
{
	try
	{
		TTHStreamHeader h;
		h.fileSize = tree.getFileSize();
		h.timeStamp = File::getTimeStamp(filePath);
		h.root = tree.getRoot();
		h.blockSize = tree.getBlockSize();
		setCheckSum(h);
		{
			File stream(filePath + ":" + g_streamName, File::WRITE, File::CREATE | File::TRUNCATE);
			stream.write(&h, sizeof(TTHStreamHeader));
			const auto& leaves = tree.getLeaves();
			stream.write(leaves[0].data, leaves.size() * TTHValue::BYTES);
		}
		File::setTimeStamp(filePath, h.timeStamp);
	}
	catch (const Exception& e)
	{
		LogManager::message(STRING(ERROR_ADD_TTH_STREAM) + ' ' + filePath + ": " + e.getError());
		return false;
	}
	return true;
}

void HashManager::deleteTree(const string& filePath) noexcept
{
	File::deleteFile(filePath + ":" + g_streamName);
	// TODO: report error ERROR_DELETE_TTH_STREAM
}
#endif

void HashManager::hashDone(uint64_t tick, int64_t fileID, const SharedFilePtr& file, const string& fileName, const TigerTree& tth, int64_t speed, int64_t size)
{
	auto db = DatabaseManager::getInstance();
	auto hashDb = db->getHashDatabaseConnection();
	if (hashDb)
	{
		db->addTree(hashDb, tth);
		db->putHashDatabaseConnection(hashDb);
	}
	fire(HashManagerListener::FileHashed(), fileID, file, fileName, tth.getRoot(), size);

	bool useStatus = false;
	if (tick > nextPostTime)
	{
		useStatus = true;
		nextPostTime = tick + 1000;
	}
	if (speed > 0)
	{
		LogManager::message(STRING(HASHING_FINISHED) + ' ' + Util::ellipsizePath(fileName) + " (" + Util::formatBytes(speed) + '/' + STRING(S) + ")", useStatus);
	}
	else
	{
		LogManager::message(STRING(HASHING_FINISHED) + ' ' + Util::ellipsizePath(fileName), useStatus);
	}
}

void HashManager::reportError(int64_t fileID, const SharedFilePtr& file, const string& fileName, const string& error)
{
	LogManager::message(STRING(ERROR_HASHING) + ' ' + fileName + ": " + error);
	fire(HashManagerListener::HashingError(), fileID, file, fileName);
}

HashManager::Hasher::Hasher() :
	stopFlag(false), tempHashSpeed(0), skipFile(false),
	maxHashSpeed(SETTING(MAX_HASH_SPEED)),
	currentFileRemaining(0),
	totalBytesToHash(0), totalBytesHashed(0),
	totalFilesHashed(0), startTick(0), startTickSavedSize(0)
{
	semaphore.create();
}

void HashManager::Hasher::hashFile(int64_t fileID, const SharedFilePtr& file, const string& fileName, int64_t size)
{
	HashTaskItem newItem;
	newItem.path = fileName;
	newItem.fileSize = size;
	newItem.fileID = fileID;
	newItem.file = file;

	uint64_t tick = GET_TICK();
	bool signal;
	{
		LOCK(cs);
		signal = wl.empty();
		wl.emplace_back(std::move(newItem));
		totalBytesToHash += size;
		if (signal)
		{
			startTick = tick;
			startTickSavedSize = 0;
		}
	}
	if (signal)
		semaphore.notify();
}

void HashManager::Hasher::stopHashing(const string& baseDir)
{
	bool signal = false;
	if (baseDir.empty())
	{
		{
			LOCK(cs);
			wl.clear();
			if (!currentFile.empty())
			{
				currentFile.clear();
				skipFile = true;
			}
			currentFileRemaining = totalBytesToHash = totalBytesHashed = 0;
			totalFilesHashed = 0;
			startTick = 0;
			startTickSavedSize = 0;
			if (setMaxHashSpeed(0) < 0) signal = true;
		}
		HashManager::getInstance()->fire(HashManagerListener::HashingAborted());
	}
	else
	{
		LOCK(cs);
		for (auto i = wl.cbegin(); i != wl.cend();)
		{
			if (strnicmp(baseDir, i->path, baseDir.length()) == 0)
			{
				totalBytesToHash -= i->fileSize;
				wl.erase(i++);
			}
			else
			{
				++i;
			}
		}
		if (!currentFile.empty() && strnicmp(baseDir, currentFile, baseDir.length()) == 0)
		{
			currentFile.clear();
			currentFileRemaining = 0;
			skipFile = true;
		}
		// TODO: notify ShareManager
	}
	if (signal)
		semaphore.notify();
}

bool HashManager::Hasher::isHashing() const
{
	LOCK(cs);
	return !wl.empty() || !currentFile.empty();
}

#ifdef _WIN32
int HashManager::Hasher::fastHash(const string& fileName, int64_t fileSize, uint8_t* buf, TigerTree& tree) noexcept
{
	HANDLE h = ::CreateFile(File::formatPath(Text::toT(fileName)).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
	                        FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return RESULT_ERROR;

	int result = RESULT_ERROR;
	int64_t savedFileSize = fileSize;
	OVERLAPPED over = { 0 };

	if (!fileSize)
	{
		result = RESULT_OK;
		tree.finalize();
		goto cleanup;
	}

	DWORD hsize = 0;
	DWORD rsize = 0;
	uint8_t* hbuf = buf + FAST_HASH_BUF_SIZE;
	uint8_t* rbuf = buf;
	
	over.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	uint64_t lastRead = GET_TICK();
	if (!::ReadFile(h, hbuf, FAST_HASH_BUF_SIZE, &hsize, &over))
	{
		int error = GetLastError();
		if (error != ERROR_IO_PENDING)
			goto cleanup;
		if (!GetOverlappedResult(h, &over, &hsize, TRUE))
			goto cleanup;
	}

	if ((int64_t) hsize > fileSize)
		hsize = fileSize;
	fileSize -= hsize;
	
	over.Offset = hsize;
	while (fileSize)
	{
		if (stopFlag)
		{
			result = RESULT_STOPPED;
			goto cleanup;
		}
		int speed = getMaxHashSpeed();
		if (speed < 0)
		{
			waitResume();
			continue;
		}
		if (speed && speed <= MAX_SPEED)
		{
			const uint64_t now = GET_TICK();
			const uint64_t minTime = hsize * 1000LL / (speed << 20);
			if (lastRead + minTime > now)
			{
				const uint64_t diff = now - lastRead;
				sleep(minTime - diff);
				if (stopFlag)
				{
					result = RESULT_STOPPED;
					goto cleanup;
				}
			}
		}
		lastRead = GET_TICK();
		
		// Start a new overlapped read
		BOOL readResult = ReadFile(h, rbuf, FAST_HASH_BUF_SIZE, &rsize, &over);
		tree.update(hbuf, hsize);
		if (!readResult)
		{
			int error = GetLastError();
			if (error != ERROR_IO_PENDING)
				goto cleanup;
			if (!GetOverlappedResult(h, &over, &rsize, TRUE))
				goto cleanup;
		}

		if ((int64_t) rsize > fileSize)
			rsize = fileSize;

		{
			LOCK(cs);
			if (skipFile)
			{
				skipFile = false;
				result = RESULT_FILE_SKIPPED;
				goto cleanup;
			}
			if ((int64_t) rsize > currentFileRemaining)
				currentFileRemaining = 0;
			else
				currentFileRemaining -= rsize;		
		}

		fileSize -= rsize;
		*((uint64_t*) &over.Offset) += rsize;

		std::swap(rbuf, hbuf);
		std::swap(rsize, hsize);
	}

	tree.update(hbuf, hsize);
	tree.finalize();
	result = RESULT_OK;
	{
		LOCK(cs);
		currentFileRemaining = 0;
	}
	
cleanup:
	if (over.hEvent) CloseHandle(over.hEvent);
	CloseHandle(h);
	if (result == RESULT_ERROR)
	{
		LOCK(cs);
		currentFileRemaining = savedFileSize; // restore the value of currentFileRemaining for slowHash
	}
	return result;
}
#endif

int HashManager::Hasher::slowHash(const string& fileName, int64_t fileSize, uint8_t* buf, TigerTree& tree)
{
	size_t size = 0;
	File f(fileName, File::READ, File::OPEN);
	uint64_t lastRead = GET_TICK();
	while (fileSize)
	{
		if (stopFlag) return RESULT_STOPPED;
		int speed = getMaxHashSpeed();
		if (speed < 0)
		{
			waitResume();
			continue;
		}
		if (speed && speed <= MAX_SPEED)
		{
			const uint64_t now = GET_TICK();
			const uint64_t minTime = size * 1000LL / (speed << 20);
			if (lastRead + minTime > now)
			{
				sleep(minTime - (now - lastRead));
				if (stopFlag) return RESULT_STOPPED;
			}
		}
		lastRead = GET_TICK();

		size = SLOW_HASH_BUF_SIZE;
		if (fileSize < (int64_t) size)
			size = (size_t) fileSize;
		f.read(buf, size);
		if (!size) break;
		{
			LOCK(cs);
			if (skipFile)
			{
				skipFile = false;
				return RESULT_FILE_SKIPPED;
			}
			if ((int64_t) size > currentFileRemaining)
				currentFileRemaining = 0;
			else
				currentFileRemaining -= size;
		}
		tree.update(buf, size);
		fileSize -= size;
	}
	tree.finalize();
	return RESULT_OK;
}

static uint8_t* allocateBuffer()
{
#ifdef _WIN32
	return static_cast<uint8_t*>(VirtualAlloc(nullptr, FAST_HASH_BUF_SIZE*2, MEM_COMMIT, PAGE_READWRITE));
#else
	return new uint8_t[SLOW_HASH_BUF_SIZE];
#endif
}

static void freeBuffer(uint8_t* buf)
{
#ifdef _WIN32
	if (buf)
		VirtualFree(buf, 0, MEM_RELEASE);
#else
	delete[] buf;
#endif
}

int HashManager::Hasher::run()
{
	bool wait = false;
#ifdef _WIN32
	bool couldNotWriteTree = false;
#endif
	string currentDir;
	string filename;
	uint8_t* buf = nullptr;
	TigerTree tree;

	auto hashManager = HashManager::getInstance();
	setThreadPriority(Thread::IDLE);
	
	while (!stopFlag)
	{
		if (wait)
		{
			semaphore.wait();
			semaphore.reset();
			if (stopFlag) break;
		}
		HashTaskItem currentItem;
		{
			LOCK(cs);
			if (wl.empty())
			{
				currentFile.clear();
				totalBytesHashed = totalBytesToHash = 0;
				totalFilesHashed = 0;
				currentFileRemaining = 0;
				startTick = 0;
				startTickSavedSize = 0;
				wait = true;
				continue;
			}
			currentItem = std::move(wl.front());
			currentFile = filename = currentItem.path;
			currentFileRemaining = currentItem.fileSize;
			totalBytesHashed += currentItem.fileSize;
			totalFilesHashed++;
			wl.pop_front();
			wait = false;
		}
		string dir = Util::getFilePath(filename);
		if (currentDir != dir)
		{
			currentDir = std::move(dir);
#ifdef _WIN32
			couldNotWriteTree = false;
#endif
		}
		maxHashSpeed = SETTING(MAX_HASH_SPEED);
		if (tempHashSpeed < 0) waitResume();
		FileAttributes attr;
		if (!File::getAttributes(filename, attr))
		{
			hashManager->reportError(currentItem.fileID, currentItem.file, filename, STRING(ERROR_OPENING_FILE));
			continue;
		}
		auto size = attr.getSize();
#ifdef _WIN32
		if (attr.isLink() && size == 0)
		{
			try
			{
				File f(filename, File::READ, File::OPEN | File::SHARED);
				size = f.getSize();
			}
			catch (FileException&) {}
		}
#endif
		if (size != currentItem.fileSize)
		{
			hashManager->reportError(currentItem.fileID, currentItem.file, filename, STRING(ERROR_SIZE_MISMATCH));
			continue;
		}
		if (!buf)
			buf = allocateBuffer();
		tree.clear();
		tree.setBlockSize(TigerTree::getMaxBlockSize(size));
#ifdef _WIN32
		if (size > 0 && BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM) && HashManager::loadTree(filename, tree, size))
		{
			LogManager::message(STRING(LOAD_TTH_FROM_NTFS) + ' ' + filename, false);
			hashManager->hashDone(GET_TICK(), currentItem.fileID, currentItem.file, filename, tree, 0, size);
			continue;
		}
#endif
		try
		{
			const uint64_t start = GET_TICK();
			int result = RESULT_ERROR;
#ifdef _WIN32
			if (BOOLSETTING(FAST_HASH))
				result = fastHash(filename, size, buf, tree);
#endif
			if (result == RESULT_ERROR)
				result = slowHash(filename, size, buf, tree);
			const uint64_t end = GET_TICK();
			if (result == RESULT_STOPPED) break;
			if (result == RESULT_OK)
			{
				const uint64_t speed = end > start ? size * 1000 / (end - start) : 0;
				hashManager->hashDone(end, currentItem.fileID, currentItem.file, filename, tree, speed, size);
#ifdef _WIN32
				if (!CompatibilityManager::isWine() && size >= SETTING(SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM) * 1048576)
				{
					if (BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
					{
						if (attr.isReadOnly())
							LogManager::message(STRING_F(SKIPPING_READ_ONLY_FILE, filename));
						else
						if (!couldNotWriteTree)
							couldNotWriteTree = !HashManager::saveTree(filename, tree);
					}
					else
						HashManager::deleteTree(filename);
				}
#endif
			}
		}
		catch (const FileException& e)
		{
			hashManager->reportError(currentItem.fileID, currentItem.file, filename, e.getError());
		}
	}
	freeBuffer(buf);
	{
		LOCK(cs);
		wl.clear();
		currentFile.clear();
		currentFileRemaining = totalBytesToHash = totalBytesHashed = 0;
		totalFilesHashed = 0;
		startTick = 0;
		startTickSavedSize = 0;
	}
	return 0;
}

int HashManager::Hasher::getMaxHashSpeed() const
{
	int result = tempHashSpeed;
	if (!result) result = maxHashSpeed;
	return result;
}

int HashManager::Hasher::setMaxHashSpeed(int val)
{
	val = tempHashSpeed.exchange(val);
	semaphore.notify();
	return val;
}

void HashManager::Hasher::shutdown()
{
	stopFlag.store(true);
	semaphore.notify();
}

void HashManager::Hasher::waitResume()
{
	semaphore.wait();
	semaphore.reset();
	int64_t tick = GET_TICK();
	LOCK(cs);
	if (startTick)
	{
		startTick = tick;
		startTickSavedSize = totalBytesHashed - currentFileRemaining;
	}
}

void HashManager::Hasher::getInfo(HashManager::Info& info) const
{
	LOCK(cs);
	info.filename = currentFile;
	info.sizeToHash = totalBytesToHash;
	info.sizeHashed = totalBytesHashed - currentFileRemaining;
	info.filesHashed = totalFilesHashed;
	info.filesLeft = wl.size();
	if (currentFileRemaining && info.filesHashed)
	{
		info.filesHashed--;
		info.filesLeft++;
	}
	info.startTick = startTick;
	info.startTickSavedSize = startTickSavedSize;
}

HashManager::HashPauser::HashPauser()
{
	prevState = HashManager::getInstance()->setMaxHashSpeed(0);
}

HashManager::HashPauser::~HashPauser()
{
	if (prevState)
		HashManager::getInstance()->setMaxHashSpeed(prevState);
}
