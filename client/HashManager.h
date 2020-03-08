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

#ifndef DCPLUSPLUS_DCPP_HASH_MANAGER_H
#define DCPLUSPLUS_DCPP_HASH_MANAGER_H

#include "Semaphore.h"
#include "TimerManager.h"
#include "Streams.h"
#include "WinEvent.h"
#include "HashManagerListener.h"

class File;

class FileException;

class HashManager : public Singleton<HashManager>, public Speaker<HashManagerListener>, private TimerManagerListener
{
	public:
		struct Info
		{
			string filename;
			int64_t sizeToHash;
			int64_t sizeHashed;
			size_t filesLeft;
			size_t filesHashed;
			int64_t startTick;
			int64_t startTickSavedSize;
		};

		HashManager()
		{
			TimerManager::getInstance()->addListener(this);
		}

		virtual ~HashManager() noexcept
		{
			TimerManager::getInstance()->removeListener(this);
			shutdown();
		}
		
		void hashFile(int64_t fileID, const SharedFilePtr& file, const string& fileName, int64_t size)
		{
			hasher.hashFile(fileID, file, fileName, size);
		}
		
		void stopHashing(const string& baseDir)
		{
			hasher.stopHashing(baseDir);
		}
		
		void setThreadPriority(Thread::Priority p)
		{
			hasher.setThreadPriority(p);
		}
		
		void getInfo(Info& info) const
		{
			hasher.getInfo(info);
		}
		
		void startup()
		{
			hasher.start(0, "HashManager");
		}
		
		void shutdown()
		{
			hasher.shutdown();
			hasher.join();
		}
		
		int setMaxHashSpeed(int speed)
		{
			return hasher.setMaxHashSpeed(speed);
		}

		int getHashSpeed() const
		{
			return hasher.getTempHashSpeed();
		}

		bool isHashing() const
		{
			return hasher.isHashing();
		}
		
		struct HashPauser
		{
				HashPauser();
				~HashPauser();
				
			private:
				int prevState;
		};
		
		static bool doLoadTree(const string& filePath, TigerTree& tree, int64_t fileSize, bool checkTimestamp) noexcept;
		static bool loadTree(const string& filePath, TigerTree& tree, int64_t fileSize = -1) noexcept;

	private:
		static bool saveTree(const string& filePath, const TigerTree& tree) noexcept;
		static void deleteTree(const string& filePath) noexcept;

	private:
		class Hasher : public Thread
		{
			public:
				Hasher();
					
				void hashFile(int64_t fileID, const SharedFilePtr& file, const string& fileName, int64_t size);
				
				void stopHashing(const string& baseDir);
				bool isHashing() const;
#ifdef _WIN32
				int fastHash(const string& fileName, int64_t fileSize, uint8_t* buf, TigerTree& tree) noexcept;
#endif
				int slowHash(const string& fileName, int64_t fileSize, uint8_t* buf, TigerTree& tree);
				void getInfo(Info& info) const;
				
				void shutdown();
				int getMaxHashSpeed() const;
				int getTempHashSpeed() const { return tempHashSpeed; }
				int setMaxHashSpeed(int val);

			private:
				// Case-sensitive (faster), it is rather unlikely that case changes, and if it does it's harmless.
				// map because it's sorted (to avoid random hash order that would create quite strange shares while hashing)
				struct HashTaskItem
				{
					int64_t fileSize;
					int64_t fileID;
					SharedFilePtr file;
				};
				
				std::map<string, HashTaskItem> w;
				mutable FastCriticalSection cs;
				std::atomic_bool stopFlag;
				std::atomic_int tempHashSpeed; // 0 = default, -1 = paused
				WinEvent<FALSE> semaphore;
				string currentFile;
				bool skipFile;
				int maxHashSpeed; // saved value of SETTING(MAX_HASH_SPEED)
				int64_t currentFileRemaining;
				int64_t totalBytesToHash, totalBytesHashed;
				size_t totalFilesHashed;
				int64_t startTick;
				int64_t startTickSavedSize;
		
			protected:
				virtual int run() override;
				void waitResume();
		};
		
		friend class Hasher;

	private:
		Hasher hasher;
		
		void hashDone(int64_t fileID, const SharedFilePtr& file, const string& fileName, const TigerTree& tth, int64_t speed, int64_t Size);
		void reportError(int64_t fileID, const SharedFilePtr& file, const string& fileName, const string& error);
};

#endif // !defined(HASH_MANAGER_H)
