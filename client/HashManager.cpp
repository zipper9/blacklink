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
#include "CFlylinkDBManager.h"
#include "ClientManager.h"
#include "CompatibilityManager.h"
#include "ShareManager.h"

#ifdef IRAINMAN_NTFS_STREAM_TTH

static const uint32_t MAGIC = '++lg';
static const string g_streamName(".gltth");

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
void HashManager::StreamStore::SetFsDetectorNotifyWnd(HWND hWnd)
{
	m_FsDetect.SetNotifyWnd(hWnd);
}
#else
std::unordered_set<char> HashManager::StreamStore::g_error_tth_stream;
#endif

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

bool HashManager::StreamStore::doLoadTree(const string& filePath, TigerTree& tree, int64_t fileSize, bool checkTimestamp) noexcept
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
		AutoArray<uint8_t> buf(dataLen);
		if (stream.read(buf.data(), size) != dataLen)
			return false;
		if (!tree.load(fileSize, buf, dataLen) ||
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

bool HashManager::StreamStore::loadTree(const string& filePath, TigerTree& tree, int64_t fileSize)
{
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
	if (!m_FsDetect.IsStreamSupported(filePath.c_str()))
#else
	if (isBan(filePath))
#endif
		return false;
	if (fileSize == -1) fileSize = File::getSize(filePath);
	if (fileSize < SETTING(SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM) * 1048576) // that's why minStreamedFileSize never be changed![*]NightOrion
		return false;
	return doLoadTree(filePath, tree, fileSize, true);
}

bool HashManager::StreamStore::saveTree(const string& p_filePath, const TigerTree& p_Tree)
{
	if (CompatibilityManager::isWine() || //[+]PPA под wine не пишем в поток
	        !BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
		return false;
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
	if (!m_FsDetect.IsStreamSupported(p_filePath.c_str()))
#else
	if (isBan(p_filePath))
#endif
		return false;
	try
	{
		TTHStreamHeader h;
		h.fileSize = File::getSize(p_filePath);
		if (h.fileSize < (SETTING(SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM) * 1048576) || h.fileSize != (uint64_t)p_Tree.getFileSize()) //[*]NightOrion
			return false; // that's why minStreamedFileSize never be changed!
		h.timeStamp = File::getTimeStamp(p_filePath);
		h.root = p_Tree.getRoot();
		h.blockSize = p_Tree.getBlockSize();
		setCheckSum(h);
		{
			File stream(p_filePath + ":" + g_streamName, File::WRITE, File::CREATE | File::TRUNCATE);
			stream.write(&h, sizeof(TTHStreamHeader));
			stream.write(p_Tree.getLeaves()[0].data, p_Tree.getLeaves().size() * TTHValue::BYTES);
		}
		File::setTimeStamp(p_filePath, h.timeStamp);
	}
	catch (const Exception& e)
	{
#ifndef RIP_USE_STREAM_SUPPORT_DETECTION
		addBan(p_filePath);
#endif
		LogManager::message(STRING(ERROR_ADD_TTH_STREAM) + ' ' + p_filePath + " : " + e.getError());// [+]IRainman
		return false;
	}
	return true;
}

void HashManager::StreamStore::deleteStream(const string& p_filePath)
{
	try
	{
		File::deleteFile(p_filePath + ":" + g_streamName);
	}
	catch (const FileException& e)
	{
		LogManager::message(STRING(ERROR_DELETE_TTH_STREAM) + ' ' + p_filePath + " : " + e.getError());
	}
}

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
void HashManager::SetFsDetectorNotifyWnd(HWND hWnd)
{
	m_streamstore.SetFsDetectorNotifyWnd(hWnd);
}
#endif

#if 0
void HashManager::addFileFromStream(int64_t p_path_id, const string& p_name, const TigerTree& p_TT, int64_t p_size)
{
	const int64_t l_TimeStamp = File::getTimeStamp(p_name);
	addFile(p_path_id, p_name, l_TimeStamp, p_TT, p_size);
}
#endif

#endif // IRAINMAN_NTFS_STREAM_TTH

void HashManager::hashDone(int64_t fileID, const SharedFilePtr& file, const string& aFileName, int64_t aTimeStamp, const TigerTree& tth, int64_t speed, bool isNTFS, int64_t size)
{
	// CFlyLock(cs); [-] IRainman fix: no data to lock.
	dcassert(!aFileName.empty());
	if (aFileName.empty())
	{
		LogManager::message("HashManager::hashDone - aFileName.empty()");
		return;
	}
	try
	{
		addFile(aFileName, aTimeStamp, tth, size);
#ifdef IRAINMAN_NTFS_STREAM_TTH
		if (BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
		{
			if (!isNTFS) // got TTH from the NTFS stream, do not write back!
			{
				m_streamstore.saveTree(aFileName, tth);
			}
		}
		else
		{
			m_streamstore.deleteStream(aFileName);
		}
#endif // IRAINMAN_NTFS_STREAM_TTH
	}
	catch (const Exception& e)
	{
		LogManager::message(STRING(HASHING_FAILED) + ' ' + aFileName + e.getError());
		return;
	}
	fire(HashManagerListener::TTHDone(), fileID, file, aFileName, tth.getRoot(), size);

	string fn = aFileName;
	if (count(fn.begin(), fn.end(), PATH_SEPARATOR) >= 2)
	{
		string::size_type i = fn.rfind(PATH_SEPARATOR);
		i = fn.rfind(PATH_SEPARATOR, i - 1);
		fn.erase(0, i);
		fn.insert(0, "...");
	}
	if (speed > 0)
	{
		LogManager::message(STRING(HASHING_FINISHED) + ' ' + fn + " (" + Util::formatBytes(speed) + '/' + STRING(S) + ")");
	}
	else
	{
		LogManager::message(STRING(HASHING_FINISHED) + ' ' + fn);
	}
}

void HashManager::addFile(const string& filename, int64_t timestamp, const TigerTree& tigerTree, int64_t size)
{
	CFlylinkDBManager::getInstance()->addTree(tigerTree);
}

void HashManager::Hasher::hashFile(int64_t fileID, const SharedFilePtr& file, const string& fileName, int64_t size)
{
	CFlyFastLock(cs);
	HashTaskItem newItem;
	newItem.fileSize = size;
	newItem.fileID = fileID;
	newItem.file = file;
	
	if (w.insert(make_pair(fileName, newItem)).second)
	{
		m_CurrentBytesLeft += size;
		if (m_paused > 0)
			m_paused++;
		else
			m_hash_semaphore.signal();
			
		int64_t bytesLeft;
		size_t filesLeft;
		getBytesAndFileLeft(bytesLeft, filesLeft);
		
		if (bytesLeft > iMaxBytes)
			iMaxBytes = bytesLeft;
			
		if (filesLeft > dwMaxFiles)
			dwMaxFiles = filesLeft;
	}
}

bool HashManager::Hasher::pause()
{
	CFlyFastLock(cs);
	return m_paused++ > 0;
}

void HashManager::Hasher::resume()
{
	CFlyFastLock(cs);
	while (--m_paused > 0)
	{
		m_hash_semaphore.signal();
	}
}

bool HashManager::Hasher::isPaused() const
{
	CFlyFastLock(cs);
	return m_paused > 0;
}

int HashManager::Hasher::GetMaxHashSpeed() const
{
	return m_ForceMaxHashSpeed != 0 ? m_ForceMaxHashSpeed : SETTING(MAX_HASH_SPEED);
}

void HashManager::Hasher::stopHashing(const string& baseDir)
{
	CFlyFastLock(cs);
	if (baseDir.empty())
	{
		// [+]IRainman When user closes the program with a chosen operation "abort hashing"
		// in the hashing dialog then the hesher is cleaning.
		w.clear();
		m_CurrentBytesLeft = 0;
	}
	else
	{
		for (auto i = w.cbegin(); i != w.cend();)
		{
			if (strnicmp(baseDir, i->first, baseDir.length()) == 0) // TODO сравнивать ID каталогов?
			{
				m_CurrentBytesLeft -= i->second.fileSize;
				w.erase(i++);
			}
			else
			{
				++i;
			}
		}
	}
	// [+] brain-ripper
	// cleanup state
	m_running = false;
	dwMaxFiles = 0;
	iMaxBytes = 0;
	m_fname.erase();
	m_currentSize = 0;
	m_fileID = 0;
}

void HashManager::Hasher::instantPause()
{
	bool wait = false;
	{
		CFlyFastLock(cs);
		if (m_paused > 0)
		{
			m_paused++;
			wait = true;
		}
	}
	if (wait)
	{
		m_hash_semaphore.wait();
	}
}

static size_t g_HashBufferSize = 16 * 1024 * 1024;

bool HashManager::Hasher::fastHash(const string& fname, uint8_t* buf, unsigned p_buf_size, TigerTree& tth, int64_t& p_size, bool p_is_link)
{
	int64_t l_size = p_size;
	HANDLE h = INVALID_HANDLE_VALUE;
	DWORD l_sector_size = 0;
	DWORD l_tmp;
	BOOL l_sector_result;
	// TODO - размер сектора определять один раз для одной буквы через массив от A до Z и не звать GetDiskFreeSpaceA
	// на каждый файл
	//
	// TODO - узнать зачем его вообще определять?
	// DONE: FILE_FLAG_NO_BUFFERING - отключает кэширование чтения\записи с диска\на диск
	// для увеличения производительности дискового ввода\вывода.
	// При этом читать\писать нужно (must) секторами.
	// Ещё там есть зависящее от диска (should) ограничение на выравнивание буфера в памяти.
	// https://msdn.microsoft.com/en-us/library/windows/desktop/cc644950(v=vs.85).aspx
	
	if (fname.size() >= 3 && fname[1] == ':')
	{
		char l_drive[4];
		memcpy(l_drive, fname.c_str(), 3);
		l_drive[3] = 0;
		l_sector_result = GetDiskFreeSpaceA(l_drive, &l_tmp, &l_sector_size, &l_tmp, &l_tmp);
	}
	else
	{
		l_sector_result = GetDiskFreeSpace(Text::toT(Util::getFilePath(fname)).c_str(), &l_tmp, &l_sector_size, &l_tmp, &l_tmp);
	}
	if (!l_sector_result)
	{
		dcassert(0);
		return false;
		// TODO Залогировать ошибку.
	}
	else
	{
		if ((g_HashBufferSize % l_sector_size) != 0)
		{
			dcassert(0);
			return false;
		}
		else
		{
			h = ::CreateFile(File::formatPath(Text::toT(fname), true).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			                 FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, nullptr);
			// TODO | FILE_FLAG_POSIX_SEMANTICS
			if (h == INVALID_HANDLE_VALUE)
			{
				dcassert(0);
				return false;
			}
			else
			{
				if (p_is_link && p_size == 0) // iSymLink?
				{
					LARGE_INTEGER x = {0};
					BOOL bRet = ::GetFileSizeEx(h, &x);
					if (bRet == FALSE)
					{
						dcassert(0);
						return false;
					}
					p_size = x.QuadPart; // fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/14
					l_size = p_size;
				}
			}
		}
	}
	DWORD hn = 0;
	DWORD rn = 0;
	uint8_t* hbuf = buf + g_HashBufferSize;
	uint8_t* rbuf = buf;
	
	OVERLAPPED over = { 0 };
	BOOL res = TRUE;
	over.hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	
	bool ok = false;
	
	if (l_size == 0)
	{
		ok = true;
		goto cleanup; // TODO  fix goto
	}
	
	uint64_t lastRead = GET_TICK();
	if (!::ReadFile(h, hbuf, g_HashBufferSize, &hn, &over))
	{
		m_last_error =   GetLastError();
		if (m_last_error == ERROR_HANDLE_EOF)
		{
			hn = 0;
		}
		else if (m_last_error == ERROR_IO_PENDING)
		{
			if (!GetOverlappedResult(h, &over, &hn, TRUE))
			{
				m_last_error_overlapped =   GetLastError();
				if (m_last_error_overlapped == ERROR_HANDLE_EOF)
				{
					hn = 0;
				}
				else
				{
					goto cleanup;
				}
			}
		}
		else
		{
			goto cleanup;
		}
	}
	
	over.Offset = hn;
	l_size -= hn;
	dcassert(l_size >= 0);
	// [+] brain-ripper
	// exit loop if "running" equals false.
	// "running" sets to false in stopHashing function
	while (!isShutdown() && m_running && l_size >= 0)
	{
		if (l_size > 0)
		{
			// Start a new overlapped read
			ResetEvent(over.hEvent);
			if (GetMaxHashSpeed() > 0)
			{
				const uint64_t now = GET_TICK();
				const uint64_t minTime = hn * 1000LL / (GetMaxHashSpeed() * 1024LL * 1024LL);
				if (lastRead + minTime > now)
				{
					const uint64_t diff = now - lastRead;
					sleep(minTime - diff);
				}
				lastRead = lastRead + minTime;
			}
			else
			{
				lastRead = GET_TICK();
			}
			res = ReadFile(h, rbuf, g_HashBufferSize, &rn, &over);
		}
		else
		{
			rn = 0;
		}
		
		tth.update(hbuf, hn);
		
		{
			CFlyFastLock(cs);
			m_currentSize = max(m_currentSize - hn, _LL(0));
		}
		
		if (l_size == 0)
		{
			ok = true;
			break;
		}
		
		if (!res)
		{
			// deal with the error code
			switch (GetLastError())
			{
				case ERROR_IO_PENDING:
					if (!GetOverlappedResult(h, &over, &rn, TRUE))
					{
						dcdebug("Error 0x%x: %s\n", GetLastError(), Util::translateError().c_str());
						goto cleanup;
					}
					break;
				default:
					dcdebug("Error 0x%x: %s\n", GetLastError(), Util::translateError().c_str());
					goto cleanup;
			}
		}
		
		instantPause();
		
		*((uint64_t*)&over.Offset) += rn;
		l_size -= rn;
		
		std::swap(rbuf, hbuf);
		std::swap(rn, hn);
	}
	
cleanup:
	if (!::CloseHandle(over.hEvent))
	{
		LogManager::message("CloseHandle(over.hEvent) error: " + Util::translateError());
	}
	if (!::CloseHandle(h))
	{
		LogManager::message("CloseHandle(h) error: " + Util::translateError());
	}
	return ok;
}

int HashManager::Hasher::run()
{
	setThreadPriority(Thread::IDLE);
	
	uint8_t* l_buf = nullptr;
	unsigned l_buf_size = 0;
	bool l_is_virtualBuf = true;
	bool l_is_last = false;
	for (;;)
	{
		m_hash_semaphore.wait();
		if (isShutdown())
			break;
		if (m_rebuild)
		{
			HashManager::getInstance()->doRebuild();
			m_rebuild = false;
			LogManager::message(STRING(HASH_REBUILT));
			continue;
		}
		{
			CFlyFastLock(cs);
			if (!w.empty())
			{
				m_fname = w.begin()->first;
				const auto& data = w.begin()->second;
				m_currentSize = data.fileSize;
				m_fileID = data.fileID;
				m_filePtr = data.file;
				m_CurrentBytesLeft -= m_currentSize;
				w.erase(w.begin());
				l_is_last = w.empty();
				if (!m_running)
				{
					uiStartTime = GET_TICK();
					m_running = true;
				}
			}
			else
			{
				m_currentSize = 0;
				l_is_last = true;
				m_fname.clear();
				m_filePtr.reset();
				m_running = false;
				iMaxBytes = 0;
				dwMaxFiles = 0;
				m_CurrentBytesLeft = 0;// [+]IRainman
			}
		}
		string l_fname;
		{
			CFlyFastLock(cs);
			l_fname = m_fname;
		}
		if (!l_fname.empty())
		{
			int64_t l_size = 0;
			int64_t l_outFiletime = 0;
			bool l_is_link = false;
			File::isExist(l_fname, l_size, l_outFiletime, l_is_link); // TODO - вернуть признак isLink
			int64_t l_sizeLeft = l_size;
#ifdef _WIN32
			if (l_buf == NULL)
			{
				l_is_virtualBuf = true;
				l_buf_size = g_HashBufferSize * 2;
				l_buf = (uint8_t*)VirtualAlloc(NULL, l_buf_size, MEM_COMMIT, PAGE_READWRITE);  // Нельзя убирать *2!
				// какой-то %%% заюзал это в fastHash
			}
#endif
			if (l_buf == NULL)
			{
				l_is_virtualBuf = false;
				dcassert(g_HashBufferSize);
				l_buf = new uint8_t[g_HashBufferSize];
				l_buf_size = g_HashBufferSize;
			}
			try
			{
				if (l_size == 0) //  && l_is_link - для файлов запретных имен aux.h
					// https://msdn.microsoft.com/en-us/library/aa365247.aspxтоже
					//размер вертается = 0
				{
					File f(l_fname, File::READ, File::OPEN);
					l_size = f.getSize(); // fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/15
				}
				const int64_t bs = TigerTree::getMaxBlockSize(l_size);
				const uint64_t start = GET_TICK();
				const int64_t timestamp = l_outFiletime;
				int64_t speed = 0;
				size_t n = 0;
				TigerTree fastTTH(bs);
				TigerTree slowTTH(bs);
				TigerTree* tth = &fastTTH;
				bool l_is_ntfs = false;
#ifdef IRAINMAN_NTFS_STREAM_TTH
				if (l_size > 0 && HashManager::getInstance()->m_streamstore.loadTree(l_fname, fastTTH, l_size))
				{
					l_is_ntfs = true;
					LogManager::message(STRING(LOAD_TTH_FROM_NTFS) + ' ' + l_fname, false);
				}
#endif
#ifdef _WIN32
#ifdef IRAINMAN_NTFS_STREAM_TTH
				if (!l_is_ntfs)
				{
#endif
					if (l_is_virtualBuf == false || !BOOLSETTING(FAST_HASH) || !fastHash(l_fname, l_buf, l_buf_size, fastTTH, l_size, l_is_link))
					{
#else
				if (!BOOLSETTING(FAST_HASH) || !fastHash(fname, 0, fastTTH, l_size))
				{
#endif
						// [+] brain-ripper
						if (m_running)
						{
							tth = &slowTTH;
							uint64_t lastRead = GET_TICK();
							File l_slow_file_reader(l_fname, File::READ, File::OPEN);
							do
							{
								size_t bufSize = g_HashBufferSize;
								
								if (GetMaxHashSpeed() > 0) // [+] brain-ripper
								{
									const uint64_t now = GET_TICK();
									const uint64_t minTime = n * 1000LL / (GetMaxHashSpeed() * 1024LL * 1024LL);
									if (lastRead + minTime > now)
									{
										sleep(minTime - (now - lastRead));
									}
									lastRead = lastRead + minTime;
								}
								else
								{
									lastRead = GET_TICK();
								}
								n = l_slow_file_reader.read(l_buf, bufSize);
								if (n > 0)
								{
									tth->update(l_buf, n);
									{
										CFlyFastLock(cs);
										m_currentSize = max(static_cast<uint64_t>(m_currentSize - n), static_cast<uint64_t>(0)); // TODO - max от 0 для беззнакового?
									}
									l_sizeLeft -= n;
									
									instantPause();
								}
							}
							while (!isShutdown() && m_running && n > 0);
						}
						else
							tth = nullptr;
					}
					else
					{
						l_sizeLeft = 0; // Variable 'l_sizeLeft' is assigned a value that is never used.
					}
#ifdef IRAINMAN_NTFS_STREAM_TTH
				}
#endif
				const uint64_t end = GET_TICK();
				if (end > start) // TODO: Why is not possible?
				{
					speed = l_size * _LL(1000) / (end - start);
				}
				if (m_running)
				{
#ifdef IRAINMAN_NTFS_STREAM_TTH
					if (l_is_ntfs)
					{
						HashManager::getInstance()->hashDone(m_fileID, m_filePtr, l_fname, timestamp, *tth, speed, l_is_ntfs, l_size);
					}
					else
#endif
					if (tth)
					{
						tth->finalize();
						HashManager::getInstance()->hashDone(m_fileID, m_filePtr, l_fname, timestamp, *tth, speed, l_is_ntfs, l_size);
					}
				}
			}
			catch (const FileException& e)
			{
				LogManager::message(STRING(ERROR_HASHING) + ' ' + l_fname + ": " + e.getError());
			}
		}
		{
			CFlyFastLock(cs);
			m_fname.clear();
			m_currentSize = 0;
			m_fileID = 0;
			m_filePtr.reset();
			
			if (w.empty())
			{
				m_running = false;
				iMaxBytes = 0;
				dwMaxFiles = 0;
				m_CurrentBytesLeft = 0;//[+]IRainman
			}
		}
		
		if (l_buf != NULL && (l_is_last || isShutdown()))
		{
			if (l_is_virtualBuf)
				VirtualFree(l_buf, 0, MEM_RELEASE);
			else
				delete [] l_buf;
			l_buf = nullptr;
		}
	}
	return 0;
}

HashManager::HashPauser::HashPauser()
{
	resume = !HashManager::getInstance()->pauseHashing();
}

HashManager::HashPauser::~HashPauser()
{
	if (resume)
	{
		HashManager::getInstance()->resumeHashing();
	}
}

bool HashManager::pauseHashing()
{
	return hasher.pause();
}

void HashManager::resumeHashing()
{
	hasher.resume();
}

bool HashManager::isHashingPaused() const
{
	return hasher.isPaused();
}
