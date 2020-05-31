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

typedef pair<UserPtr, unsigned int> CurrentConnectionPair;
typedef boost::unordered_map<UserPtr, unsigned int, User::Hash> CurrentConnectionMap;
typedef std::list<UploadPtr> UploadList;

class UploadQueueItem :
	public ColumnBase< 13 >,
	public UserInfoBase
{
	public:
		UploadQueueItem(const HintedUser& user, const string& file, int64_t pos, int64_t size) :
			hintedUser(user), file(file), pos(pos), size(size), time(GET_TIME()), iconIndex(-1)
		{
#ifdef _DEBUG
			++g_upload_queue_item_count;
#endif
		}
		~UploadQueueItem()
		{
#ifdef _DEBUG
			--g_upload_queue_item_count;
#endif
		}
		void update();
		const UserPtr& getUser() const
		{
			return hintedUser.user;
		}
		int getImageIndex() const
		{
			return iconIndex < 0 ? 0 : iconIndex;
		}
		void setImageIndex(int index)
		{
			iconIndex = index;
		}
		
		static int compareItems(const UploadQueueItem* a, const UploadQueueItem* b, uint8_t col);
		
		enum
		{
			COLUMN_FILE,
			COLUMN_TYPE,
			COLUMN_PATH,
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_TRANSFERRED,
			COLUMN_SIZE,
			COLUMN_ADDED,
			COLUMN_WAITING,
			COLUMN_LOCATION,
			COLUMN_IP,
#ifdef FLYLINKDC_USE_DNS
			COLUMN_DNS,
#endif
			COLUMN_SLOTS,
			COLUMN_SHARE,
			COLUMN_LAST
		};
		
		GETC(HintedUser, hintedUser, HintedUser);
		GETC(string, file, File);
		GETSET(int64_t, pos, Pos);
		GETC(int64_t, size, Size);
		GETC(uint64_t, time, Time);
		IPInfo ipInfo;
#ifdef _DEBUG
		static boost::atomic_int g_upload_queue_item_count;
#endif

	private:
		int iconIndex;
};

class WaitingUser
{
	public:
		WaitingUser(const HintedUser& hintedUser, const std::string& token, const UploadQueueItemPtr& uqi) : hintedUser(hintedUser), token(token)
		{
			waitingFiles.push_back(uqi);
		}
		operator const UserPtr&() const
		{
			return hintedUser.user;
		}
		UserPtr getUser() const
		{
			return hintedUser.user;
		}
		std::vector<UploadQueueItemPtr> waitingFiles;
		HintedUser hintedUser;
		GETSET(string, token, Token);
};

class UploadManager : private ClientManagerListener, private UserConnectionListener, public Speaker<UploadManagerListener>, private TimerManagerListener, public Singleton<UploadManager>
{
#ifdef FLYLINKDC_USE_DOS_GUARD
		typedef boost::unordered_map<string, uint8_t> CFlyDoSCandidatMap;
		CFlyDoSCandidatMap m_dos_map;
		mutable FastCriticalSection csDos; // [+] IRainman opt.
#endif
	public:
		static uint32_t g_count_WaitingUsersFrame;
		/** @return Number of uploads. */
		static size_t getUploadCount()
		{
			// CFlyReadLock(*g_csUploadsDelay);
			return g_uploads.size();
		}
		
		/**
		 * @remarks This is only used in the tray icons. Could be used in
		 * MainFrame too.
		 *
		 * @return Running average download speed in Bytes/s
		 */
		static int64_t getRunningAverage()
		{
			return g_runningAverage;
		}
		
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
		
		/** @param aUser Reserve an upload slot for this user and connect. */
		void reserveSlot(const HintedUser& hintedUser, uint64_t aTime);
		static void unreserveSlot(const HintedUser& hintedUser);
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
		
		/** @internal */
		void addConnection(UserConnection* conn);
		void removeFinishedUpload(const UserPtr& aUser);
		void abortUpload(const string& aFile, bool waiting = true);
		
		GETSET(int, extraPartial, ExtraPartial);
		GETSET(int, extra, Extra);
		GETSET(uint64_t, lastGrant, LastGrant);
		
		void load(); // !SMT!-S
		static void save(); // !SMT!-S
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		static bool isBanReply(const UserPtr& user); // !SMT!-S
#endif // IRAINMAN_ENABLE_AUTO_BAN
		
		static time_t getReservedSlotTime(const UserPtr& aUser);
		void shutdown();
		
	private:
		bool isFireball;
		bool isFileServer;
		static int  g_running;
		static int64_t g_runningAverage;
		uint64_t fireballStartTick;
		uint64_t fileServerCheckTick;
		
		static UploadList g_uploads;
		UploadList finishedUploads;
		static CurrentConnectionMap g_uploadsPerUser;
		std::unique_ptr<webrtc::RWLockWrapper> csFinishedUploads;
		
		void processSlot(UserConnection::SlotTypes slotType, int delta);
		
		static void increaseUserConnectionAmountL(const UserPtr& user);
		static void decreaseUserConnectionAmountL(const UserPtr& user);
		static unsigned int getUserConnectionAmountL(const UserPtr& user);
		
		int lastFreeSlots; /// amount of free slots at the previous minute
		
		typedef boost::unordered_map<UserPtr, uint64_t, User::Hash> SlotMap;
		
		static SlotMap g_reservedSlots;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csReservedSlots;
		
		SlotMap notifiedUsers;
		SlotQueue slotQueue;
		mutable CriticalSection csQueue;

		std::regex reCompressedFiles;
		string compressedFilesPattern;
		FastCriticalSection csCompressedFiles;
		
		size_t addFailedUpload(const UserConnection* aSource, const string& file, int64_t pos, int64_t size);
		void notifyQueuedUsers(int64_t tick);
		
		friend class Singleton<UploadManager>;
		UploadManager() noexcept;
		~UploadManager();
		
		bool getAutoSlot();
		void removeConnection(UserConnection* conn, bool removeListener = true);
		void removeUpload(UploadPtr& upload, bool delay = false);
		void logUpload(const UploadPtr& u);
		
		static void testSlotTimeout(uint64_t aTick = GET_TICK());
		
		// ClientManagerListener
		void on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept override;
		
		// TimerManagerListener
		void on(Second, uint64_t aTick) noexcept override;
		void on(Minute, uint64_t aTick) noexcept override;
		
		// UserConnectionListener
		void on(Failed, UserConnection*, const string&) noexcept override;
		void on(Get, UserConnection*, const string&, int64_t) noexcept override;
		void on(Send, UserConnection*) noexcept override;
		void on(TransmitDone, UserConnection*) noexcept override;
		void on(GetListLength, UserConnection*) noexcept override;
		
		void on(AdcCommand::GET, UserConnection*, const AdcCommand&) noexcept override;
		void on(AdcCommand::GFI, UserConnection*, const AdcCommand&) noexcept override;
		
		bool prepareFile(UserConnection* aSource, const string& aType, const string& aFile, int64_t aResume, int64_t& aBytes, bool listRecursive = false);
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
		static std::unique_ptr<webrtc::RWLockWrapper> g_csBans;
#endif // IRAINMAN_ENABLE_AUTO_BAN
};

#endif // !defined(UPLOAD_MANAGER_H)
