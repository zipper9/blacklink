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

STANDARD_EXCEPTION(QueueException);

class UserConnection;
class QueueLoader;
class DirectoryItem;
class DirectoryListing;
typedef DirectoryItem* DirectoryItemPtr;

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
		void addFromWebServer(const string& target, int64_t size, const TTHValue& root);
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

	public:
		class LockFileQueueShared
		{
			public:
				LockFileQueueShared()
				{
#ifdef USE_QUEUE_RWLOCK
					g_fileQueue.csFQ->acquireShared();
#else
					g_fileQueue.csFQ->lock();
#endif
				}
				~LockFileQueueShared()
				{
#ifdef USE_QUEUE_RWLOCK
					g_fileQueue.csFQ->releaseShared();
#else
					g_fileQueue.csFQ->unlock();
#endif
				}
				const QueueItem::QIStringMap& getQueueL()
				{
					return g_fileQueue.getQueueL();
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
				QueueManager& manager;
				const string source;
				const string target;
				const bool moveToOtherDir;
				QueueItemPtr qi;

			public:
				FileMoverJob(QueueManager& manager, const string& source, const string& target, bool moveToOtherDir, const QueueItemPtr& qi) :
					manager(manager), source(source), target(target), moveToOtherDir(moveToOtherDir), qi(qi) {}
				virtual void run();
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
		void addDirectory(const string& dir, const UserPtr& user, const string& target,
		                  QueueItem::Priority p = QueueItem::DEFAULT) noexcept;
		                  
		int matchListing(DirectoryListing& dl) noexcept;

	private:
		void removeItem(const QueueItemPtr& qi, bool removeFromUserQueue);

	public:
		static bool getTTH(const string& target, TTHValue& tth)
		{
			return g_fileQueue.getTTH(target, tth);
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
		
		static bool handlePartialSearch(const TTHValue& tth, PartsInfo& outPartsInfo);
		bool handlePartialResult(const UserPtr& user, const TTHValue& tth, QueueItem::PartialSource& partialSource, PartsInfo& outPartialInfo);
		
#ifdef FLYLINKDC_USE_DROP_SLOW
		bool dropSource(const DownloadPtr& d);
#endif
	private:
		static void getRunningFilesL(QueueItemList& runningFiles)
		{
			g_fileQueue.getRunningFilesL(runningFiles);
		}
		static size_t getRunningFileCount(const size_t stopKey)
		{
			return g_fileQueue.getRunningFileCount(stopKey);
		}

	public:
		static bool isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& target);
		/** Sanity check for the target filename */
		static string checkTarget(const string& target, const int64_t size, bool validateFileName = true);
		/** Add a source to an existing queue item */
		bool addSourceL(const QueueItemPtr& qi, const UserPtr& user, Flags::MaskType addBad, bool isFirstLoad = false);

	private:
		static uint64_t g_lastSave;

	public:
		typedef vector<pair<QueueItem::SourceConstIter, const QueueItemPtr> > PFSSourceList;
		
		/** All queue items by target */
		class FileQueue
		{
			public:
				FileQueue();
				void add(const QueueItemPtr& qi);
				QueueItemPtr add(const string& target, int64_t size, Flags::MaskType flags,
				                 QueueItem::Priority p, const string& tempTarget, time_t added,
				                 const TTHValue& root, uint8_t maxSegments);
				bool getTTH(const string& name, TTHValue& tth) const;
				QueueItemPtr findTarget(const string& target) const;
				int findQueueItems(QueueItemList& ql, const TTHValue& tth, int maxCount = 0) const;
				QueueItemPtr findQueueItem(const TTHValue& tth) const;
				static uint8_t getMaxSegments(const uint64_t filesize);
				// find some PFS sources to exchange parts info
				void findPFSSourcesL(PFSSourceList&) const;
				
				QueueItemPtr findAutoSearch(deque<string>& recent) const;
				size_t getSize() const { return queue.size(); }
				bool empty() const { return queue.empty(); }
				const QueueItem::QIStringMap& getQueueL() const { return queue; }
				void getRunningFilesL(QueueItemList& runningFiles);
				size_t getRunningFileCount(const size_t stopCount) const;
				void moveTarget(const QueueItemPtr& qi, const string& target);
				void remove(const QueueItemPtr& qi);
				void clearAll();
				
#ifdef USE_QUEUE_RWLOCK
				std::unique_ptr<RWLock> csFQ;
#else
				std::unique_ptr<CriticalSection> csFQ;
#endif
				bool isQueued(const TTHValue& tth) const;

			private:
				QueueItem::QIStringMap queue;
				boost::unordered_map<TTHValue, QueueItemList> queueTTH;
				std::regex reAutoPriority;
				string autoPriorityPattern;				
		};

		/** QueueItems by target */
		static FileQueue g_fileQueue;

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
				
				typedef boost::unordered_map<UserPtr, QueueItemList, User::Hash> UserQueueMap; // TODO - set ?
				typedef boost::unordered_map<UserPtr, QueueItemPtr, User::Hash> RunningMap;
				
			private:
				/** QueueItems by priority and user (this is where the download order is determined) */
				static UserQueueMap g_userQueueMap[QueueItem::LAST];
				/** Currently running downloads, a QueueItem is always either here or in the userQueue */
				static RunningMap g_runningMap;
				/** Last error message to sent to TransferView */
#define FLYLINKDC_USE_RUNNING_QUEUE_CS
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
				static std::unique_ptr<RWLock> g_runningMapCS;
#endif
		};

		/** QueueItems by user */
		static UserQueue g_userQueue;

	private:
		friend class QueueLoader;
		friend class Singleton<QueueManager>;
		
		QueueManager();
		~QueueManager() noexcept;
		
		mutable FastCriticalSection csDirectories;
		
		/** Directories queued for downloading */
		boost::unordered_multimap<UserPtr, DirectoryItemPtr, User::Hash> m_directories;
		/** Recent searches list, to avoid searching for the same thing too often */
		deque<string> m_recent;
		/** The queue needs to be saved */
		static bool g_dirty;
		/** Next search */
		uint64_t nextSearch;
		
		std::atomic_bool listMatcherAbortFlag;
		std::atomic_flag listMatcherRunning;
		std::atomic_bool dclstLoaderAbortFlag;
		std::atomic_bool recheckerAbortFlag;

		void processList(const string& name, const HintedUser& hintedUser, int flags);
		void deleteFileLists();
		
		bool moveFile(const string& source, const string& target);
		bool internalMoveFile(const string& source, const string& target, bool moveToOtherDir);
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
