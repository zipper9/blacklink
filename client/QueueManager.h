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
		// return values for getNextL
		enum
		{
			SUCCESS,
			ERROR_NO_ITEM,
			ERROR_NO_NEEDED_PART,
			ERROR_FILE_SLOTS_TAKEN,
			ERROR_DOWNLOAD_SLOTS_TAKEN,
			ERROR_NO_FREE_BLOCK
		};

		/** Add a file to the queue. */
		void add(const string& target, int64_t size, const TTHValue& root, const UserPtr& user,
		         QueueItem::MaskType flags, QueueItem::Priority priority, bool addBad, bool& getConnFlag);
		/** Add a user's filelist to the queue. */
		void addList(const UserPtr& user, QueueItem::MaskType flags, const string& initialDir = Util::emptyString);
		
		void addCheckUserIP(const UserPtr& user)
		{
			bool getConnFlag = true;
			add(Util::emptyString, -1, TTHValue(), user, QueueItem::FLAG_USER_GET_IP, QueueItem::DEFAULT, true, getConnFlag);
		}

		bool addDclstFile(const string& path);
		void processFileExistsQuery(const string& path, int action, const string& newPath, QueueItem::Priority priority);

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

#ifdef FLYLINKDC_USE_DETECT_CHEATING
		class FileListQueue: public BackgroundTaskExecuter<DirectoryListInfoPtr, 15000> // [<-] IRainman fix: moved from MainFrame to core.
		{
				void execute(const DirectoryListInfoPtr& list);
		} m_listQueue;
#endif

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
		void move(const string& aSource, const string& target) noexcept;
		
		bool removeTarget(const string& target, bool isBatchRemove);
		
		void removeAll();
		void removeSource(const string& target, const UserPtr& user, Flags::MaskType reason, bool removeConn = true) noexcept;
		void removeSource(const UserPtr& user, Flags::MaskType reason) noexcept;
		
		bool recheck(const string& target);
		
		void setPriority(const string& target, QueueItem::Priority p, bool resetAutoPriority) noexcept;
		void setAutoPriority(const string& target, bool ap);
		
		static void getTargets(const TTHValue& tth, StringList& sl, int maxCount = 0);
#ifdef _DEBUG
		bool isSourceValid(const QueueItemPtr& qi, const QueueItem::Source* source) const
		{
			return qi->isSourceValid(source);
		}
#endif
		static void getChunksVisualisation(const QueueItemPtr& qi, vector<QueueItem::RunningSegment>& running, vector<Segment>& done)
		{
			qi->getChunksVisualisation(running, done);
		}
		
		static bool getQueueInfo(const UserPtr& user, string& target, int64_t& size, int& flags) noexcept;
		DownloadPtr getDownload(UserConnection* source, Download::ErrorInfo& error) noexcept;
		// FIXME: remove path parameter, use download->getPath()
		void putDownload(const string& path, DownloadPtr download, bool finished, bool reportFinish = true) noexcept;
		void setFile(const DownloadPtr& aDownload);
		
		/** @return The highest priority download the user has, PAUSED may also mean no downloads */
		static QueueItem::Priority hasDownload(const UserPtr& user);
		
		void loadQueue() noexcept;
		void saveQueue(bool force = false) noexcept;

		static string getQueueFile() { return Util::getConfigPath() + "Queue.xml"; }
		
		static bool handlePartialSearch(const TTHValue& tth, QueueItem::PartsInfo& outPartsInfo, uint64_t& blockSize);
		bool handlePartialResult(const UserPtr& user, const TTHValue& tth, QueueItem::PartialSource& partialSource, QueueItem::PartsInfo& outPartialInfo);
		
#ifdef FLYLINKDC_USE_DROP_SLOW
		bool dropSource(const DownloadPtr& d);
#endif
	private:
		static void getRunningFilesL(QueueItemList& runningFiles)
		{
			fileQueue.getRunningFilesL(runningFiles);
		}

	public:
		static bool isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& target);
		/** Sanity check for the target filename */
		static string checkTarget(const string& target, const int64_t size, bool validateFileName = true);
		/** Add a source to an existing queue item */
		bool addSourceL(const QueueItemPtr& qi, const UserPtr& user, Flags::MaskType addBad, bool isFirstLoad = false);

	private:
		static uint64_t lastSave;

		std::regex reWantEndFiles;
		string wantEndFilesPattern;
		FastCriticalSection csWantEndFiles;

		QueueItem::MaskType getFlagsForFileName(const string& fileName);

	public:
		/** All queue items by target */
		class FileQueue
		{
				friend class LockFileQueueShared;
				friend class QueueLoader;

			public:
				FileQueue();
				bool add(const QueueItemPtr& qi);
				QueueItemPtr add(QueueManager* qm, const string& target, int64_t size, Flags::MaskType flags,
				                 QueueItem::Priority p, const string& tempTarget, time_t added,
				                 const TTHValue& root, uint8_t maxSegments);
				bool getTTH(const string& name, TTHValue& tth) const;
				QueueItemPtr findTarget(const string& target) const;
				int findQueueItems(QueueItemList& ql, const TTHValue& tth, int maxCount = 0) const;
				QueueItemPtr findQueueItem(const TTHValue& tth) const;
				static uint8_t getMaxSegments(uint64_t filesize);
				// find some PFS sources to exchange parts info
				void findPFSSources(QueueItem::SourceList& sl, uint64_t now) const;

				QueueItemPtr findAutoSearch(deque<string>& recent) const;
				size_t getSize() const { return queue.size(); }
				bool empty() const { return queue.empty(); }
				const QueueItem::QIStringMap& getQueueL() const { return queue; }
				void getRunningFilesL(QueueItemList& runningFiles);
				void moveTarget(const QueueItemPtr& qi, const string& target);
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
				void addL(const QueueItemPtr& qi);
				void addL(const QueueItemPtr& qi, const UserPtr& user, bool isFirstLoad);
				int getNextL(QueueItemPtr& result, const UserPtr& user, QueueItem::Priority minPrio = QueueItem::LOWEST, int64_t wantedSize = 0, int64_t lastSpeed = 0, bool allowRemove = false);
				QueueItemPtr getRunning(const UserPtr& user);
				void addDownload(const QueueItemPtr& qi, const DownloadPtr& d);
				bool removeDownload(const QueueItemPtr& qi, const UserPtr& d);
				void removeRunning(const UserPtr& d);
				void removeQueueItemL(const QueueItemPtr& qi, bool removeDownloadFlag);
				void removeQueueItem(const QueueItemPtr& qi);
				void removeUserL(const QueueItemPtr& qi, const UserPtr& user, bool removeDownloadFlag);
				bool isInQueue(const QueueItemPtr& qi) const;
				void setQIPriority(const QueueItemPtr& qi, QueueItem::Priority p);
				bool getQueuedItems(const UserPtr& user, QueueItemList& out) const;
				static void modifyRunningCount(int count);
				static size_t getRunningCount();
				
				typedef boost::unordered_map<UserPtr, QueueItemList, User::Hash> UserQueueMap; // TODO - set ?
				typedef boost::unordered_map<UserPtr, QueueItemPtr, User::Hash> RunningMap;
				
			private:
				/** QueueItems by priority and user (this is where the download order is determined) */
				static UserQueueMap userQueueMap[QueueItem::LAST];
				/** Currently running downloads, a QueueItem is always either here or in the userQueue */
				static RunningMap runningMap;
				/** Last error message to sent to TransferView */
#define FLYLINKDC_USE_RUNNING_QUEUE_CS
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
				static std::unique_ptr<RWLock> csRunningMap;
#endif
				static size_t totalDownloads;
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
				void addFlags(int f) { flags |= f; }
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
		boost::unordered_multimap<UserPtr, DirectoryItem, User::Hash> directories;
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

		void processList(const string& name, const HintedUser& hintedUser, int flags, const DirectoryItem* dirItem);
		void deleteFileLists();
		
		void moveFile(const string& source, const string& target);
		static void keepFileInTempDir(const string& source, const string& target);
		void moveStuckFile(const QueueItemPtr& qi);
		void copyFile(const string& source, const string& target, QueueItemPtr& qi);
		void rechecked(const QueueItemPtr& qi);
		
		static void setDirty();
		
		static string getListPath(const UserPtr& user);
		
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
		StringSet updatedSources;
		CriticalSection csUpdatedSources;
		void fireStatusUpdated(const QueueItemPtr& qi);
		void addUpdatedSource(const QueueItemPtr& qi);

	public:
		static void getDownloadConnection(const UserPtr& user);
};

#endif // !defined(QUEUE_MANAGER_H)
