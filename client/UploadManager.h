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


#ifndef DCPLUSPLUS_DCPP_UPLOAD_MANAGER_H
#define DCPLUSPLUS_DCPP_UPLOAD_MANAGER_H

#include <set>
#include "Singleton.h"
#include "UploadManagerListener.h"
#include "ClientManagerListener.h"
#include "UserConnection.h"
#include "Client.h"
#include "LocationUtil.h"
#include <regex>

#ifdef _DEBUG
#include <atomic>
#endif

typedef std::list<UploadPtr> UploadList;

class UploadQueueFile
{
	public:
		static const int16_t FLAG_PARTIAL_FILE_LIST = 1;

		UploadQueueFile(const string& file, int64_t pos, int64_t size, uint16_t flags) :
			file(file), flags(flags), pos(pos), size(size), time(GET_TIME())
		{
#ifdef _DEBUG
			++g_upload_queue_item_count;
#endif
		}
#ifdef _DEBUG
		~UploadQueueFile()
		{
			--g_upload_queue_item_count;
		}
#endif
		GETC(string, file, File);
		GETSET(uint16_t, flags, Flags);
		GETSET(int64_t, pos, Pos);
		GETC(int64_t, size, Size);
		GETC(uint64_t, time, Time);
#ifdef _DEBUG
		static std::atomic_int g_upload_queue_item_count;
#endif
};

class WaitingUser
{
	public:
		WaitingUser(const HintedUser& hintedUser, const std::string& token, const UploadQueueFilePtr& uqi) : hintedUser(hintedUser), token(token)
		{
			waitingFiles.push_back(uqi);
		}
		operator const UserPtr&() const
		{
			return hintedUser.user;
		}
		const UserPtr& getUser() const
		{
			return hintedUser.user;
		}
		std::vector<UploadQueueFilePtr> waitingFiles;
		HintedUser hintedUser;
		GETSET(string, token, Token);
};

class UploadManager : private ClientManagerListener, public Speaker<UploadManagerListener>, private TimerManagerListener, public Singleton<UploadManager>
{
#ifdef FLYLINKDC_USE_DOS_GUARD
		typedef boost::unordered_map<string, uint8_t> CFlyDoSCandidatMap;
		CFlyDoSCandidatMap m_dos_map;
		mutable FastCriticalSection csDos;
#endif
	public:
		static uint32_t g_count_WaitingUsersFrame;
		/** @return Number of uploads. */
		size_t getUploadCount() const
		{
			// READ_LOCK(*g_csUploadsDelay);
			return uploads.size();
		}
		
		static int getRunningCount() { return g_running; }
		static int64_t getRunningAverage() { return g_runningAverage; }
		static void setRunningAverage(int64_t avg) { g_runningAverage = avg; }
		
		static int getSlots()
		{
			return (max(SETTING(SLOTS), max(SETTING(HUB_SLOTS), 0) * Client::getTotalCounts()));
		}
		
		/** @return Number of free slots. */
		static int getFreeSlots()
		{
			return max((getSlots() - g_running), 0);
		}
		
		/** @internal */
		int getFreeExtraSlots() const
		{
			return max(SETTING(EXTRA_SLOTS) - getExtra(), 0);
		}
		
		/** @param user Reserve an upload slot for this user and connect. */
		void reserveSlot(const HintedUser& hintedUser, uint64_t seconds);
		void unreserveSlot(const HintedUser& hintedUser);
		void clearUserFilesL(const UserPtr&);
		void clearWaitingFilesL(const WaitingUser& wu);
		
		class LockInstanceQueue
		{
			public:
				LockInstanceQueue()
				{
					UploadManager::getInstance()->csQueue.lock();
				}
				~LockInstanceQueue()
				{
					UploadManager::getInstance()->csQueue.unlock();
				}
				UploadManager* operator->()
				{
					return UploadManager::getInstance();
				}
		};
		
		typedef std::list<WaitingUser> SlotQueue;
		const SlotQueue& getUploadQueueL() const
		{
			return slotQueue;
		}
		bool getIsFireballStatus() const
		{
			return isFireball;
		}
		bool getIsFileServerStatus() const
		{
			return isFileServer;
		}

		void processGet(UserConnection* source, const string& fileName, int64_t resume) noexcept;
		void processGetBlock(UserConnection* source, const string& cmd, const string& param) noexcept;
		void processSend(UserConnection* source) noexcept;
		void processGET(UserConnection*, const AdcCommand&) noexcept;
		void processGFI(UserConnection*, const AdcCommand&) noexcept;

		void failed(UserConnection* source, const string& error) noexcept;
		void transmitDone(UserConnection* source) noexcept;

		/** @internal */
		void addConnection(UserConnection* conn);
		void removeFinishedUpload(const UserPtr& user);
		void abortUpload(const string& aFile, bool waiting = true);
		
		GETSET(int, extraPartial, ExtraPartial);
		GETSET(int, extra, Extra);
		GETSET(uint64_t, lastGrant, LastGrant);
		
		void load();
		void save();
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		static bool isBanReply(const UserPtr& user);
#endif // IRAINMAN_ENABLE_AUTO_BAN
		
		uint64_t getReservedSlotTick(const UserPtr& user) const;

		struct ReservedSlotInfo
		{
			UserPtr user;
			uint64_t timeout;
		};
		void getReservedSlots(vector<ReservedSlotInfo>& out) const;

		void shutdown();
		
	private:
		bool isFireball;
		bool isFileServer;
		static int g_running;
		static int64_t g_runningAverage;
		uint64_t fireballStartTick;
		uint64_t fileServerCheckTick;
		
		UploadList uploads;
		UploadList finishedUploads;
		std::unique_ptr<RWLock> csFinishedUploads;
		
		void processSlot(UserConnection::SlotTypes slotType, int delta);
		
		int lastFreeSlots; /// amount of free slots at the previous minute
		
		typedef boost::unordered_map<UserPtr, uint64_t, User::Hash> SlotMap;
		
		SlotMap reservedSlots;
		mutable std::unique_ptr<RWLock> csReservedSlots;
		
		SlotMap notifiedUsers;
		SlotQueue slotQueue;
		mutable CriticalSection csQueue;

		std::regex reCompressedFiles;
		string compressedFilesPattern;
		FastCriticalSection csCompressedFiles;
		
		size_t addFailedUpload(const UserConnection* aSource, const string& file, int64_t pos, int64_t size, uint16_t flags);
		void notifyQueuedUsers(int64_t tick);
		
		friend class Singleton<UploadManager>;
		UploadManager() noexcept;
		~UploadManager();
		
		bool getAutoSlot();
		void removeConnection(UserConnection* conn);
		void removeUpload(UploadPtr& upload, bool delay = false);
		void logUpload(const UploadPtr& u);
		
		void testSlotTimeout(uint64_t tick = GET_TICK());
		
		// ClientManagerListener
		void on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept override;
		
		// TimerManagerListener
		void on(Second, uint64_t aTick) noexcept override;
		void on(Minute, uint64_t aTick) noexcept override;
		
		bool prepareFile(UserConnection* source, const string& type, const string& file, bool hideShare, const CID& shareGroup, int64_t resume, int64_t& bytes, bool listRecursive = false);
		bool isCompressedFile(const Upload* u);
		bool hasUpload(const UserConnection* newLeecher) const;
		static void initTransferData(TransferData& td, const Upload* u);

#ifdef IRAINMAN_ENABLE_AUTO_BAN
		struct banmsg_t
		{
			uint32_t tick;
			int slots, share, limit, min_slots, max_slots, min_share, min_limit;
			bool same(const banmsg_t& a) const
			{
				return ((slots ^ a.slots) | (share ^ a.share) | (limit ^ a.limit) |
				        (min_slots ^ a.min_slots) |
				        (max_slots ^ a.max_slots) |
				        (min_share ^ a.min_share) | (min_limit ^ a.min_limit)) == 0;
			}
		};
		typedef boost::unordered_map<string, banmsg_t> BanMap;
		bool handleBan(UserConnection* aSource/*, bool forceBan, bool noChecks*/);
		static BanMap g_lastBans;
		static std::unique_ptr<RWLock> g_csBans;
#endif // IRAINMAN_ENABLE_AUTO_BAN
};

#endif // !defined(UPLOAD_MANAGER_H)
