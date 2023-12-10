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


#ifndef DCPLUSPLUS_DCPP_QUEUE_ITEM_H
#define DCPLUSPLUS_DCPP_QUEUE_ITEM_H

#include "Segment.h"
#include "HintedUser.h"
#include "RWLock.h"
#include "Download.h"
#include "TransferFlags.h"
#include "Locks.h"
#include <atomic>
#include <boost/container/flat_set.hpp>

extern const string dctmpExtension;

class QueueManager;

#ifdef USE_QUEUE_RWLOCK
#define QueueRLock READ_LOCK
#define QueueWLock WRITE_LOCK
#else
#define QueueRLock LOCK
#define QueueWLock LOCK
#endif

class QueueItem
{
	public:
		typedef boost::unordered_map<string, QueueItemPtr> QIStringMap;
		typedef uint16_t MaskType;

		static const size_t PFS_MIN_FILE_SIZE = 20 * 1024 * 1024;

		enum Priority
		{
			DEFAULT = -1,
			PAUSED = 0,
			LOWEST,
			LOWER,
			LOW,
			NORMAL,
			HIGH,
			HIGHER,
			HIGHEST,
			LAST
		};

		// immutable flags
		enum
		{
			FLAG_USER_LIST          = 0x0001, // This is a user file listing download
			FLAG_PARTIAL_LIST       = 0x0002, // Request partial file list (used with FLAG_USER_LIST)
			FLAG_RECURSIVE_LIST     = 0x0004, // Request recursive file list (used with FLAG_PARTIAL_LIST)
			FLAG_USER_CHECK         = 0x0008, // Request info from user
			FLAG_USER_GET_IP        = 0x0010,
			FLAG_DCLST_LIST         = 0x0020,
			FLAG_WANT_END           = 0x0040
		};

		// mutable extraFlags
		enum
		{
			XFLAG_AUTO_PRIORITY     = 0x0001,
			XFLAG_REMOVED           = 0x0002, // Item was removed from the Queue
			XFLAG_COPYING           = 0x0004,
			XFLAG_XML_BZLIST        = 0x0008, // The file list downloaded was actually an .xml.bz2 list (used with FLAG_USER_LIST)
			XFLAG_DOWNLOAD_DIR      = 0x0010, // The file list is needed for directory download (used with FLAG_USER_LIST)
			XFLAG_MATCH_QUEUE       = 0x0020, // Match the queue against this list
			XFLAG_CLIENT_VIEW       = 0x0040, // The file must be opened by the app
			XFLAG_TEXT_VIEW         = 0x0080, // File should be viewed as a text file (used with XFLAG_CLIENT_VIEW)
			XFLAG_AUTODROP          = 0x0100, // Slow sources autodrop is enabled for this file
			XFLAG_DOWNLOAD_CONTENTS = 0x0200,
			XFLAG_ALLOW_SEGMENTS    = 0x0400
		};

		// return values for QueueManager::getNextL
		enum
		{
			SUCCESS,
			ERROR_NO_ITEM,
			ERROR_NO_NEEDED_PART,
			ERROR_FILE_SLOTS_TAKEN,
			ERROR_DOWNLOAD_SLOTS_TAKEN,
			ERROR_NO_FREE_BLOCK,
			ERROR_INVALID_CONNECTION
		};

		bool isUserList() const
		{
			return (flags & (FLAG_USER_LIST | FLAG_DCLST_LIST | FLAG_USER_GET_IP)) != 0;
		}

		typedef std::vector<uint16_t> PartsInfo;
		static int countParts(const PartsInfo& pi);
		static bool compareParts(const PartsInfo& a, const PartsInfo& b);

		struct RunningSegment
		{
			Segment seg;
			DownloadPtr d;
		};

		/**
		 * Source parts info
		 * Meaningful only when Source::FLAG_PARTIAL is set
		 */
		class PartialSource
		{
			public:
				PartialSource(const string& myNick, const string& hubIpPort, const IpAddress& ip, uint16_t udp, int64_t blockSize) :
					myNick(myNick), hubIpPort(hubIpPort), ip(ip), udpPort(udp), nextQueryTime(0), pendingQueryCount(0), blockSize(blockSize) { }

				typedef std::shared_ptr<PartialSource> Ptr;
				bool isCandidate(const uint64_t now) const
				{
					return getUdpPort() != 0 && getNextQueryTime() <= now;
				}

				GETSET(PartsInfo, parts, Parts);
				GETSET(string, myNick, MyNick); // for NMDC support only
				GETSET(string, hubIpPort, HubIpPort);
				GETSET(IpAddress, ip, Ip);
				GETSET(uint64_t, nextQueryTime, NextQueryTime);
				GETSET(uint16_t, udpPort, UdpPort);
				GETSET(uint8_t, pendingQueryCount, PendingQueryCount);
				GETSET(int64_t, blockSize, BlockSize);
		};

		class Source : public Flags
		{
			public:
				enum
				{
					FLAG_NONE               = 0x000,
					FLAG_FILE_NOT_AVAILABLE = 0x001,
					FLAG_PASSIVE            = 0x002,
					FLAG_REMOVED            = 0x004,
					FLAG_NO_TTHF            = 0x008,
					FLAG_BAD_TREE           = 0x010,
					FLAG_SLOW_SOURCE        = 0x020,
					FLAG_NO_TREE            = 0x040,
					FLAG_NO_NEED_PARTS      = 0x080,
					FLAG_PARTIAL            = 0x100,
					FLAG_TTH_INCONSISTENCY  = 0x200,
					FLAG_UNTRUSTED          = 0x400,
					FLAG_MASK = FLAG_FILE_NOT_AVAILABLE
					            | FLAG_PASSIVE | FLAG_REMOVED | FLAG_BAD_TREE | FLAG_SLOW_SOURCE
					            | FLAG_NO_TREE | FLAG_TTH_INCONSISTENCY | FLAG_UNTRUSTED
				};

				Source() {}

				bool isCandidate(bool isBadSource) const
				{
					return isSet(FLAG_PARTIAL) && (isBadSource || !isSet(FLAG_TTH_INCONSISTENCY));
				}

				PartialSource::Ptr partialSource;
		};

		// used by getChunksVisualisation
		struct SegmentEx
		{
			int64_t start;
			int64_t end;
			int64_t pos;
		};

		struct GetSegmentParams
		{
			int64_t wantedSize;
			int64_t lastSpeed;
			bool enableMultiChunk;
			bool dontBeginSegment;
			bool overlapChunks;
			int dontBeginSegSpeed;
			int maxChunkSize;

			void readSettings();
		};

		typedef boost::unordered_map<UserPtr, Source> SourceMap;
		typedef SourceMap::iterator SourceIter;
		typedef SourceMap::const_iterator SourceConstIter;

		typedef boost::container::flat_set<Segment> SegmentSet;

		QueueItem(const string& target, int64_t size, Priority priority, MaskType flags, MaskType extraFlags,
		          time_t added, const TTHValue& tth, uint8_t maxSegments, const string& tempTarget);
		QueueItem(const string& target, int64_t size, Priority priority, MaskType flags, MaskType extraFlags,
		          time_t added, uint8_t maxSegments, const string& tempTarget);
		QueueItem(const string& newTarget, QueueItem& src);

		QueueItem(const QueueItem &) = delete;
		QueueItem& operator= (const QueueItem &) = delete;

		struct SourceListItem
		{
			uint64_t queryTime;
			QueueItemPtr qi;
			SourceConstIter si;
			bool operator< (const SourceListItem& other) const { return queryTime < other.queryTime; }
		};
		typedef boost::container::flat_multiset<SourceListItem> SourceList;

		static void getPFSSourcesL(const QueueItemPtr& qi, SourceList& sourceList, uint64_t now, size_t maxCount);

		bool countOnlineUsersGreatOrEqualThanL(const size_t maxValue) const;
		void getOnlineUsers(UserList& l) const;
		UserPtr getFirstSource() const;

#ifdef USE_QUEUE_RWLOCK
		static std::unique_ptr<RWLock> g_cs;
#else
		static std::unique_ptr<CriticalSection> g_cs;
#endif
		static std::atomic_bool checkTempDir;

		const SourceMap& getSourcesL() const { return sources; }
		const SourceMap& getBadSourcesL() const { return badSources; }

		SourceIter findSourceL(const UserPtr& user)
		{
			return sources.find(user);
		}
		SourceIter findBadSourceL(const UserPtr& user)
		{
			return badSources.find(user);
		}
		bool isSourceL(const UserPtr& user) const
		{
			return sources.find(user) != sources.end();
		}
		bool isBadSourceL(const UserPtr& user) const
		{
			return badSources.find(user) != badSources.end();
		}
		bool isBadSourceExceptL(const UserPtr& user, MaskType exceptions) const;
		size_t getOnlineSourceCountL() const;
		uint32_t getSourcesVersion() const { return sourcesVersion.load(); }
		void updateSourcesVersion() { ++sourcesVersion; }
		void getChunksVisualisation(vector<SegmentEx>& running, vector<Segment>& done) const;
		bool isChunkDownloaded(int64_t startPos, int64_t& len) const;
		void setOverlapped(const Segment& segment, bool isOverlapped);

		// Are specified parts needed by this download?
		static bool isNeededPart(const PartsInfo& theirParts, const PartsInfo& ourParts);

		// Get shared parts info, max 255 parts range pairs
		void getParts(PartsInfo& partialInfo, uint64_t blockSize) const;

		int64_t getDownloadedBytes() const { return downloadedBytes; }
		void updateDownloadedBytes();
		void updateDownloadedBytesAndSpeed();
		void addDownload(const DownloadPtr& download);
		void addDownload(const Segment& seg);
		bool setDownloadForSegment(const Segment& seg, const DownloadPtr& download);
		bool removeDownload(const UserPtr& user);
		bool removeDownload(const Segment& seg);
		size_t getDownloadsSegmentCount() const { return downloads.size(); }
		bool disconnectSlow(const DownloadPtr& d);
		void disconnectOthers(const DownloadPtr& d);
		bool isMultipleSegments() const;
		void getUsers(UserList& users) const;

		// Next segment that is not done and not being downloaded, zero-sized segment returned if there is none is found
		Segment getNextSegmentL(const GetSegmentParams& gsp, const PartialSource::Ptr &partialSource, int* error) const;

		void addSegment(const Segment& segment);
		void addSegmentL(const Segment& segment);
		void resetDownloaded();
		void resetDownloadedL();

		bool isFinished() const;

		bool isRunning() const
		{
			return !isWaiting();
		}
		bool isWaiting() const
		{
			LOCK(csSegments);
			return downloads.empty();
		}
		const string& getListExt() const;

	private:
		const string target;
		int64_t size;
		const time_t added;
		mutable FastCriticalSection csAttribs;
		const MaskType flags;
		MaskType extraFlags;
		const TTHValue tthRoot;
		uint64_t blockSize;
		Priority priority;
		uint8_t maxSegments;

		Segment getNextSegmentForward(const int64_t blockSize, const int64_t targetSize, vector<Segment>* neededParts, const vector<int64_t>& posArray) const;
		Segment getNextSegmentBackward(const int64_t blockSize, const int64_t targetSize, vector<Segment>* neededParts, const vector<int64_t>& posArray) const;
		bool shouldSearchBackward() const;

	public:
		MaskType getFlags() const { return flags; }
		MaskType getExtraFlagsL() const { return extraFlags; }
		void setExtraFlagsL(MaskType flags) { extraFlags = flags; }

		MaskType getExtraFlags() const
		{
			LOCK(csAttribs);
			return extraFlags;
		}

		void setExtraFlags(MaskType flags)
		{
			LOCK(csAttribs);
			extraFlags = flags;
		}

		void changeExtraFlags(MaskType value, MaskType mask)
		{
			LOCK(csAttribs);
			extraFlags = (extraFlags & ~mask) | value;
		}

		void toggleExtraFlag(MaskType flag)
		{
			LOCK(csAttribs);
			extraFlags ^= flag;
		}

		const TTHValue& getTTH() const { return tthRoot; }
		time_t getAdded() const { return added; }

		bool updateBlockSize(uint64_t treeBlockSize);
		uint64_t getBlockSize() const { return blockSize; }

		static string getDCTempName(const string& fileName, const TTHValue* tth);

	private:
		mutable CriticalSection csSegments;
		std::vector<RunningSegment> downloads;

		SegmentSet doneSegments;
		int64_t doneSegmentsSize;
		int64_t downloadedBytes;

	public:
		void getDoneSegments(vector<Segment>& done) const;

		GETSET(uint64_t, timeFileBegin, TimeFileBegin);
		GETSET(int64_t, lastSize, LastSize);
#ifdef DEBUG_TRANSFERS
		GETSET(string, sourcePath, SourcePath);
#endif

	public:
		void lockAttributes() const noexcept { csAttribs.lock(); }
		void unlockAttributes() const noexcept { csAttribs.unlock(); }

		Priority getPriorityL() const { return priority; }
		void setPriorityL(Priority value) { priority = value; }

		uint8_t getMaxSegmentsL() const { return maxSegments; }
		void setMaxSegmentsL(uint8_t value) { maxSegments = value; }

		int64_t getSize() const { return size; }
		void setSize(int64_t value) { size = value; }

		const string& getTempTargetL() const { return tempTarget; }
		void setTempTargetL(const string& value) { tempTarget = value; }
		void setTempTargetL(string&& value) { tempTarget = std::move(value); }

		const string& getTarget() const { return target; }

		int16_t getTransferFlags(int& flags) const;
		QueueItem::Priority calculateAutoPriorityL() const;

		int64_t getAverageSpeed() const { return averageSpeed; }

	private:
		int64_t averageSpeed;
		std::atomic<uint32_t> sourcesVersion;
		SourceMap sources;
		SourceMap badSources;
		QueueItem::Priority prioQueue;
		string tempTarget;

		SourceIter addSourceL(const UserPtr& user);
		void removeSourceL(const UserPtr& user, MaskType reason);

		friend class QueueManager;
};

#endif // !defined(QUEUE_ITEM_H)
