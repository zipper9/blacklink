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

#ifndef DCPLUSPLUS_DCPP_FINISHED_MANAGER_H
#define DCPLUSPLUS_DCPP_FINISHED_MANAGER_H

#include "QueueManagerListener.h"
#include "UploadManagerListener.h"
#include "Singleton.h"
#include "FinishedManagerListener.h"
#include "User.h"
#include "ClientManager.h"

class FinishedItem
{
	public:
		enum
		{
			COLUMN_FIRST,
			COLUMN_FILE = COLUMN_FIRST,
			COLUMN_TYPE,
			COLUMN_DONE,
			COLUMN_PATH,
			COLUMN_TTH,
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_SIZE,
			COLUMN_SPEED,
			COLUMN_IP,
			COLUMN_NETWORK_TRAFFIC,
			COLUMN_LAST
		};
		
#ifdef FLYLINKDC_USE_TORRENT
		FinishedItem(const string& target, int64_t size, int64_t speed,
		             time_t time, const libtorrent::sha1_hash& sha1, int64_t actual, int64_t id) :
			target(target),
			size(size),
			avgSpeed(speed),
			time(time),
			actual(actual),
			id(id),
			tempId(0),
			sha1(sha1),
			isTorrent(!sha1.is_all_zeros())
		{
		}
#endif
		
		FinishedItem(const string& target, const string& nick, const string& hubUrl, int64_t size, int64_t speed,
		             time_t time, const TTHValue& tth, const string& ip, int64_t actual, int64_t id) :
			target(target),
			hub(hubUrl),
			hubs(hubUrl),
			size(size),
			avgSpeed(speed),
			time(time),
			tth(tth),
			ip(ip),
			nick(nick),
			actual(actual),
			id(id),
			tempId(0)
#ifdef FLYLINKDC_USE_TORRENT
			, isTorrent(false)
#endif
		{
		}
		
		FinishedItem(const string& target, const HintedUser& user, int64_t size, int64_t speed,
		             time_t time, const TTHValue& tth, const string& ip, int64_t actual) :
			target(target),
			cid(user.user->getCID()),
			hub(user.hint),
			hubs(user.user ? Util::toString(ClientManager::getHubNames(user.user->getCID(), Util::emptyString)) : Util::emptyString),
			size(size),
			avgSpeed(speed),
			time(time),
			tth(tth),
			ip(ip),
			nick(user.user->getLastNick()),
			actual(actual),
			id(0),
			tempId(0)
#ifdef FLYLINKDC_USE_TORRENT
			, isTorrent(false)
#endif
		{
		}
		
		const tstring getText(int col) const
		{
			dcassert(col >= 0 && col < COLUMN_LAST);
			switch (col)
			{
				case COLUMN_FILE:
					return Text::toT(Util::getFileName(getTarget()));
				case COLUMN_TYPE:
					return Text::toT(Util::getFileExtWithoutDot(getTarget()));
				case COLUMN_DONE:
					return Text::toT(id ? Util::formatDigitalClockGMT(getTime()) : Util::formatDigitalClock(getTime()));
				case COLUMN_PATH:
					return Text::toT(Util::getFilePath(getTarget()));
				case COLUMN_NICK:
					return Text::toT(getNick());
				case COLUMN_HUB:
					return Text::toT(getHubs());
				case COLUMN_SIZE:
					return Util::formatBytesT(getSize());
				case COLUMN_NETWORK_TRAFFIC:
					if (getActual())
						return Util::formatBytesT(getActual());
					else
						return Util::emptyStringT;
				case COLUMN_SPEED:
					if (getAvgSpeed())
						return Util::formatBytesT(getAvgSpeed()) + _T('/') + TSTRING(S);
					else
						return Util::emptyStringT;
				case COLUMN_IP:
					return Text::toT(getIP());
				case COLUMN_TTH:
				{
					if (getTTH() != TTHValue())
						return Text::toT(getTTH().toBase32());
					else
						return Util::emptyStringT;
				}
				default:
					return Util::emptyStringT;
			}
		}
		
		static int compareItems(const FinishedItem* a, const FinishedItem* b, int col)
		{
			switch (col)
			{
				case COLUMN_SPEED:
					return compare(a->getAvgSpeed(), b->getAvgSpeed());
				case COLUMN_SIZE:
					return compare(a->getSize(), b->getSize());
				case COLUMN_NETWORK_TRAFFIC:
					return compare(a->getActual(), b->getActual());
				default:
					return lstrcmpi(a->getText(col).c_str(), b->getText(col).c_str());
			}
		}
		GETC(string, target, Target);
		GETC(TTHValue, tth, TTH);
		GETC(string, ip, IP);
		GETC(string, nick, Nick);
		GETC(string, hubs, Hubs);
		GETC(string, hub, Hub);
		GETC(CID, cid, CID);
		GETC(int64_t, size, Size);
		GETC(int64_t, avgSpeed, AvgSpeed);
		GETC(time_t, time, Time);
		GETC(int64_t, actual, Actual); // Socket Bytes!
		GETC(int64_t, id, ID);
		GETSET(int64_t, tempId, TempID);

#ifdef FLYLINKDC_USE_TORRENT
		const libtorrent::sha1_hash sha1;
		const bool isTorrent;
#endif

	private:
		friend class FinishedManager;
};

class FinishedManager : public Singleton<FinishedManager>,
	public Speaker<FinishedManagerListener>, private QueueManagerListener, private UploadManagerListener
{
	public:
		enum eType
		{
			e_Download = 0,
			e_Upload = 1
		};
		const FinishedItemList& lockList(eType type)
		{
			cs[type]->AcquireLockShared();
			return finished[type];
		}
		void unlockList(eType type)
		{
			cs[type]->ReleaseLockShared();
		}
		
		bool removeItem(const FinishedItemPtr& item, eType type);
		void removeAll(eType type);
		void pushHistoryFinishedItem(const FinishedItemPtr& item, int type);
		void updateStatus()
		{
			fly_fire(FinishedManagerListener::UpdateStatus());
		}
		
	private:
		friend class Singleton<FinishedManager>;
		
		FinishedManager();
		~FinishedManager();
		
		void on(QueueManagerListener::Finished, const QueueItemPtr&, const string&, const DownloadPtr& d) noexcept override;
		void on(UploadManagerListener::Complete, const UploadPtr& u) noexcept override;
		
		void log(const string& path, const CID& cid, ResourceManager::Strings message);
		void addItem(FinishedItemPtr& item, eType type);
		
		std::unique_ptr<webrtc::RWLockWrapper> cs[2]; // index = eType
		FinishedItemList finished[2]; // index = eType
		int64_t tempId;
};

#endif // !defined(FINISHED_MANAGER_H)
