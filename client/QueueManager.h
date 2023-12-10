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

#ifndef DCPLUSPLUS_DCPP_QUEUE_MANAGER_H
#define DCPLUSPLUS_DCPP_QUEUE_MANAGER_H

#include "TimerManager.h"
#include "ClientManagerListener.h"
#include "QueueManagerListener.h"
#include "SearchManagerListener.h"
#include "QueueItem.h"
#include "SharedFileStream.h"
#include "JobExecutor.h"
#include <regex>

class QueueException : public Exception
{
	public:
		enum
		{
			BAD_USER = 1,
			BAD_FILE_NAME,
			BAD_FILE_SIZE,
			BAD_FILE_TTH,
			ALREADY_FINISHED,
			DUPLICATE_SOURCE,
			TARGET_REMOVED
		};

		explicit QueueException(int code, const string& error) : Exception(error), code(code) {}
		int getCode() const { return code; }

	private:
		const int code;
};

class UserConnection;
class QueueLoader;
class DirectoryItem;
class DirectoryListing;

class QueueManager : public Singleton<QueueManager>,
	public Speaker<QueueManagerListener>,
	private TimerManagerListener,
	private SearchManagerListener, private ClientManagerListener
{
	public:
		enum
		{
			DIR_FLAG_DOWNLOAD_DIRECTORY = 1,
			DIR_FLAG_MATCH_QUEUE        = 2,
			// internal flags used by processList
			DIR_FLAG_TEXT               = 4,
			DIR_FLAG_TTH_LIST           = 8
		};

		struct QueueItemParams
		{
			int64_t size = -1;
			const TTHValue* root = nullptr;
#ifdef DEBUG_TRANSFERS
			string sourcePath;
#endif
			QueueItem::Priority priority = QueueItem::DEFAULT;
			bool readdBadSource = true;
		};

		struct QueueItemSegment
		{
			QueueItemPtr qi;
			Segment seg;
			DownloadPtr download;
			Flags::MaskType sourceFlags;
		};

		// Add a file to the queue
		void add(const string& target, const QueueItemParams& params, const UserPtr& user, QueueItem::MaskType flags, QueueItem::MaskType extraFlags, bool& getConnFlag);
		// Add a user's filelist to the queue
		void addList(const UserPtr& user, QueueItem::MaskType flags, QueueItem::MaskType extraFlags, const string& initialDir = Util::emptyString);
#if 0
		void addCheckUserIP(const UserPtr& user);
#endif
		bool userCheckStart(const UserPtr& user);
		void userCheckProcessFailure(const UserPtr& user, int numErrors, bool removeQueueItem);
		bool addDclstFile(const string& path);
		void processFileExistsQuery(const string& path, int action, const string& newPath, QueueItem::Priority priority);
		void startBatch() noexcept;
		void endBatch() noexcept;

	public:
		class LockFileQueueShared
		{
			public:
				LockFileQueueShared()
				{
#ifdef USE_QUEUE_RWLOCK
					fileQueue.csFQ->acquireShared();
#else
					fileQueue.csFQ->lock();
#endif
				}
				~LockFileQueueShared()
				{
#ifdef USE_QUEUE_RWLOCK
					fileQueue.csFQ->releaseShared();
#else
					fileQueue.csFQ->unlock();
#endif
				}
				const QueueItem::QIStringMap& getQueueL()
				{
					return fileQueue.getQueueL();
				}
		};
		
		
		class DirectoryListInfo
		{
			public:
				DirectoryListInfo(const HintedUser& user, const string& file, const string& dir, int64_t speed, bool isDclst = false) : hintedUser(user), file(file), dir(dir), speed(speed), isDclst(isDclst) { }
				const HintedUser hintedUser;
				const string file;
				const string dir;
				const int64_t speed;
				const bool isDclst;
		};
		typedef DirectoryListInfo* DirectoryListInfoPtr;
		
		bool matchAllFileLists();
		
	private:
		class ListMatcherJob : public JobExecutor::Job
		{
				QueueManager& manager;

			public:
				ListMatcherJob(QueueManager& manager) : manager(manager) {}
				virtual void run();
		};

		JobExecutor listMatcher;

		class DclstLoaderJob : public JobExecutor::Job
		{
				QueueManager& manager;
				const string path;

			public:
				DclstLoaderJob(QueueManager& manager, const string& path) : manager(manager), path(path) {}
				virtual void run();
		};

		JobExecutor dclstLoader;

		class FileMoverJob : public JobExecutor::Job
		{
			public:
				enum JobType
				{
					MOVE_FILE,
					MOVE_FILE_RETRY,
					COPY_QI_FILE
				};

				FileMoverJob(QueueManager& manager, JobType type, const string& source, const string& target, bool moveToOtherDir, const QueueItemPtr& qi) :
					manager(manager), type(type), source(source), target(target), moveToOtherDir(moveToOtherDir), qi(qi) {}
				virtual void run();

			private:
				QueueManager& manager;
				const string source;
				const string target;
				const bool moveToOtherDir;
				const JobType type;
				QueueItemPtr qi;
		};

		JobExecutor fileMover;

		class RecheckerJob : public JobExecutor::Job
		{
				QueueManager& manager;
				const string file;

			public:
				RecheckerJob(QueueManager& manager, const string& file) : manager(manager), file(file) {}
				virtual void run();
		};

		JobExecutor rechecker;

	public:
		void shutdown();

		/** Readd a source that was removed */
		void readd(const string& target, const UserPtr& user);
		void readdAll(const QueueItemPtr& q);
		/** Add a directory to the queue (downloads filelist and matches the directory). */
		void addDirectory(const string& dir, const UserPtr& user, const string& target, QueueItem::Priority p, int flag) noexcept;
		int matchListing(DirectoryListing& dl) noexcept;
		size_t getDirectoryItemCount() const noexcept;

	private:
		void removeItem(const QueueItemPtr& qi, bool removeFromUserQueue);
		int matchTTHList(const string& data, const UserPtr& user) noexcept;

	public:
		static bool getTTH(const string& target, TTHValue& tth)
		{
			return fileQueue.getTTH(target, tth);
		}
		
		/** Move the target location of a queued item. Running items are silently ignored */
		void move(const string& source, const string& newTarget) noexcept;

		bool removeTarget(const string& target);

		void removeAll();
		void removeSource(const string& target, const UserPtr& user, Flags::MaskType reason, bool removeConn = true) noexcept;
		void removeSource(const UserPtr& user, Flags::MaskType reason) noexcept;
		
		bool recheck(const string& target);
		
		void setPriority(const string& target, QueueItem::Priority p, bool resetAutoPriority) noexcept;
		void setAutoPriority(const string& target, bool ap);

		static void getTargets(const TTHValue& tth, StringList& sl, int maxCount = 0);
		static QueueItemPtr getQueuedItem(const UserPtr& user) noexcept;
		DownloadPtr getDownload(const UserConnectionPtr& ucPtr, Download::ErrorInfo& error, bool checkSlots = false) noexcept;
		void putDownload(DownloadPtr download, bool finished, bool reportFinish = true) noexcept;
		void setFile(const DownloadPtr& download);

		/** @return The highest priority download the user has, PAUSED may also mean no downloads */
		static QueueItem::Priority hasDownload(const UserPtr& user);
		
		void loadQueue() noexcept;
		void saveQueue(bool force = false) noexcept;

		static bool handlePartialSearch(const TTHValue& tth, QueueItem::PartsInfo& outPartsInfo, uint64_t& blockSize);
		bool handlePartialResult(const UserPtr& user, const TTHValue& tth, QueueItem::PartialSource& partialSource, QueueItem::PartsInfo& outPartialInfo);
		
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
		bool dropSource(const DownloadPtr& d);
#endif
	public:
		static bool isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& target);
		/** Sanity check for the target filename */
		static string checkTarget(const string& target, const int64_t size, bool validateFileName = true);
		/** Add a source to an existing queue item */
		bool addSourceL(const QueueItemPtr& qi, const UserPtr& user, QueueItem::MaskType addBad);

	private:
		static uint64_t lastSave;

		std::regex reWantEndFiles;
		string wantEndFilesPattern;
		FastCriticalSection csWantEndFiles;

		int batchCounter;
		vector<QueueItemPtr> batchAdd;
		vector<QueueItemPtr> batchRemove;
		CriticalSection csBatch;

		QueueItem::MaskType getFlagsForFileName(const string& fileName) noexcept;
		QueueItem::MaskType getFlagsForFileNameL(const string& fileName) const noexcept;

	public:
		/** All queue items by target */
		class FileQueue
		{
				friend class LockFileQueueShared;
				friend class QueueLoader;

			public:
				FileQueue();
				bool add(const QueueItemPtr& qi);
				QueueItemPtr add(QueueManager* qm, const string& target, int64_t size,
				                 QueueItem::MaskType flags, QueueItem::MaskType extraFlags,
				                 QueueItem::Priority p, const string& tempTarget,
				                 const TTHValue* root, uint8_t maxSegments);
				bool getTTH(const string& name, TTHValue& tth) const;
				QueueItemPtr findTarget(const string& target) const;
				int findQueueItems(QueueItemList& ql, const TTHValue& tth, int maxCount = 0) const;
				QueueItemPtr findQueueItem(const TTHValue& tth) const;
				static uint8_t getMaxSegments(uint64_t filesize);
				// find some PFS sources to exchange parts info
				void findPFSSources(QueueItem::SourceList& sl, uint64_t now) const;

				QueueItemPtr findAutoSearch(const deque<string>& recent) const;
				size_t getSize() const { return queue.size(); }
				bool empty() const { return queue.empty(); }
				const QueueItem::QIStringMap& getQueueL() const { return queue; }
				QueueItemPtr moveTarget(QueueItemPtr& qi, const string& target);
				void remove(const QueueItemPtr& qi);
				void clearAll();

				bool isQueued(const TTHValue& tth) const;
				void updatePriority(QueueItem::Priority& p, bool& autoPriority, const string& fileName, int64_t size, QueueItem::MaskType flags);
				uint64_t getGenerationId() const;

			private:
				bool addL(const QueueItemPtr& qi);

#ifdef USE_QUEUE_RWLOCK
				std::unique_ptr<RWLock> csFQ;
#else
				std::unique_ptr<CriticalSection> csFQ;
#endif
				QueueItem::QIStringMap queue;
				uint64_t generationId;
				boost::unordered_map<TTHValue, QueueItemList> queueTTH;
				std::regex reAutoPriority;
				string autoPriorityPattern;
		};

		/** QueueItems by target */
		static FileQueue fileQueue;

		/** All queue items indexed by user (this is a cache for the FileQueue really...) */
		class UserQueue
		{
			friend class QueueManager;

			public:
				UserQueue();
				size_t getRunningCount() const;
				void removeRunning(const UserPtr& d);

			private:	
				// flags for getNextL
				enum
				{
					FLAG_ADD_SEGMENT  = 1,
					FLAG_ALLOW_REMOVE = 2
				};

				void addL(const QueueItemPtr& qi, QueueItem::Priority p);
				void addL(const QueueItemPtr& qi, QueueItem::Priority prioQueue, const UserPtr& user);
				int getNextL(QueueItemSegment& result, const UserPtr& user, const QueueItem::GetSegmentParams& gsp, QueueItem::Priority minPrio, int flags);
				QueueItemPtr getRunning(const UserPtr& user);
				void setRunningDownload(const QueueItemPtr& qi, const UserPtr& user);
				void removeDownload(const QueueItemPtr& qi, const UserPtr& user);
				void removeQueueItemL(const QueueItemPtr& qi, bool removeDownloadFlag);
				void removeQueueItem(const QueueItemPtr& qi);
				void removeUserL(const QueueItemPtr& qi, const UserPtr& user, bool removeDownloadFlag);
				bool isInQueue(const QueueItemPtr& qi) const;
				void setQIPriority(const QueueItemPtr& qi, QueueItem::Priority p);
				bool getQueuedItems(const UserPtr& user, QueueItemList& out) const;
				QueueItemPtr findItemByFlag(const UserPtr& user, int flag) const;

				typedef boost::unordered_map<UserPtr, QueueItemList> UserQueueMap;
				typedef boost::unordered_map<UserPtr, QueueItemPtr> RunningMap;

			private:
				/** QueueItems by priority and user (this is where the download order is determined) */
				UserQueueMap userQueueMap[QueueItem::LAST];
				/** Currently running downloads, a QueueItem is always either here or in the userQueue */
				RunningMap runningMap;
				/** Last error message to sent to TransferView */
				std::unique_ptr<RWLock> csRunningMap;
		};

		/** QueueItems by user */
		static UserQueue userQueue;

	private:
		friend class QueueLoader;
		friend class Singleton<QueueManager>;

		QueueManager();
		~QueueManager() noexcept;

		class DirectoryItem
		{
			public:
				DirectoryItem(const UserPtr& user, const string& name, const string& target, QueueItem::Priority p, int flag) :
					name(name), target(target), priority(p), user(user), flags(flag) {}

				const UserPtr& getUser() const { return user; }
				int getFlags() const { return flags; }
				void addFlags(int f)
				{
					flags |= f;
					if (flags & DIR_FLAG_DOWNLOAD_DIRECTORY)
						flags &= ~DIR_FLAG_MATCH_QUEUE;
				}
				const string& getName() const { return name; }
				const string& getTarget() const { return target; }
				QueueItem::Priority getPriority() const { return priority; }

			private:
				const UserPtr user;
				const string name;
				const string target;
				const QueueItem::Priority priority;
				int flags;
		};

		mutable FastCriticalSection csDirectories;

		/** Directories for downloading or matching the queue */
		boost::unordered_multimap<UserPtr, DirectoryItem> directories;
		/** Recent searches list, to avoid searching for the same thing too often */
		deque<string> m_recent;
		/** The queue needs to be saved */
		static bool dirty;
		/** Next search */
		uint64_t nextSearch;

		std::atomic_bool listMatcherAbortFlag;
		std::atomic_flag listMatcherRunning;
		std::atomic_bool dclstLoaderAbortFlag;
		std::atomic_bool recheckerAbortFlag;

		static int queueItemFlagsToDirFlags(QueueItem::MaskType extraFlags);
		static QueueItem::MaskType dirFlagsToQueueItemFlags(int dirFlags);
		void processList(const string& name, const HintedUser& hintedUser, int flags, const DirectoryItem* dirItem);
		void deleteFileLists();

		void moveFile(const string& source, const string& target, int64_t moverLimit);
		static void keepFileInTempDir(const string& source, const string& target);
		void copyFile(const string& source, const string& target, QueueItemPtr& qi);
		void rechecked(const QueueItemPtr& qi);

		static void setDirty();
		static void checkAntifragFile(const string& tempTarget, QueueItem::MaskType flags);
		static string getFileListTarget(const UserPtr& user);

		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
		void on(TimerManagerListener::Minute, uint64_t tick) noexcept override;
		
		// SearchManagerListener
		void on(SearchManagerListener::SR, const SearchResult&) noexcept override;
		
		// ClientManagerListener
		void on(ClientManagerListener::UserConnected, const UserPtr& user) noexcept override;
		void on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept override;
		
	private:
		bool sourceAdded;
		CriticalSection csUpdatedSources;
		void fireStatusUpdated(const QueueItemPtr& qi);

	public:
		static void getDownloadConnection(const UserPtr& user);
};

#endif // !defined(QUEUE_MANAGER_H)
