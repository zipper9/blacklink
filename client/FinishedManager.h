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

#include "Singleton.h"
#include "Speaker.h"
#include "FinishedItem.h"
#include "FinishedManagerListener.h"
#include "QueueManagerListener.h"
#include "UploadManagerListener.h"

class FinishedManager : public Singleton<FinishedManager>,
	public Speaker<FinishedManagerListener>, private QueueManagerListener, private UploadManagerListener
{
	public:
		enum eType
		{
			e_Download = 0,
			e_Upload = 1
		};

		const FinishedItemList& lockList(eType type, uint64_t* genId = nullptr)
		{
			finished[type].cs->acquireShared();
			if (genId) *genId = finished[type].generationId;
			return finished[type].items;
		}
		void unlockList(eType type)
		{
			finished[type].cs->releaseShared();
		}

		bool removeItem(const FinishedItemPtr& item, eType type);
		void removeAll(eType type);
		void pushHistoryFinishedItem(const FinishedItemPtr& item, bool isFile, int type);
		void updateStatus()
		{
			fire(FinishedManagerListener::UpdateStatus());
		}
		void updateSettings();

	private:
		friend class Singleton<FinishedManager>;

		FinishedManager();
		~FinishedManager();

		void on(QueueManagerListener::Finished, const QueueItemPtr&, const string&, const DownloadPtr& d) noexcept override;
		void on(UploadManagerListener::Complete, const UploadPtr& u) noexcept override;

		void addItem(FinishedItemPtr& item, eType type);

		struct ItemList
		{
			std::unique_ptr<RWLock> cs;
			FinishedItemList items;
			uint64_t generationId;
			size_t maxSize;
		};

		ItemList finished[2];
		int64_t tempId;
		std::atomic_int options;

		enum
		{
			OPT_LOG_FINISHED_DOWNLOADS = 1,
			OPT_LOG_FINISHED_UPLOADS   = 2,
			OPT_LOG_FILELIST_TRANSFERS = 4,
			OPT_ENABLE_UPLOAD_COUNTER  = 8
		};
};

#endif // !defined(FINISHED_MANAGER_H)
