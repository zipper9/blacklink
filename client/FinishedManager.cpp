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
#include "FinishedManager.h"
#include "FinishedManagerListener.h"
#include "Download.h"
#include "Upload.h"
#include "QueueManager.h"
#include "UploadManager.h"
#include "LogManager.h"
#include "CFlylinkDBManager.h"

FinishedManager::FinishedManager()
{
	tempId = 0;
	cs[e_Download] = std::unique_ptr<RWLock>(RWLock::create());
	cs[e_Upload] = std::unique_ptr<RWLock>(RWLock::create());
	
	QueueManager::getInstance()->addListener(this);
	UploadManager::getInstance()->addListener(this);
}

FinishedManager::~FinishedManager()
{
	QueueManager::getInstance()->removeListener(this);
	UploadManager::getInstance()->removeListener(this);
	removeAll(e_Upload);
	removeAll(e_Download);
}

bool FinishedManager::removeItem(const FinishedItemPtr& item, eType type)
{
	CFlyWriteLock(*cs[type]);
	const auto it = find(finished[type].begin(), finished[type].end(), item);
	
	if (it != finished[type].end())
	{
		finished[type].erase(it);
		return true;
	}
	return false;
}

void FinishedManager::removeAll(eType type)
{
	CFlyWriteLock(*cs[type]);
	finished[type].clear();
}

void FinishedManager::addItem(FinishedItemPtr& item, eType type)
{
	size_t maxSize = type == e_Download ? SETTING(MAX_FINISHED_DOWNLOADS) : SETTING(MAX_FINISHED_UPLOADS);
	int64_t maxTempId = 0;
	CFlyWriteLock(*cs[type]);
	auto& data = finished[type];
	item->setTempID(++tempId);
	data.push_back(item);
	if (maxSize < 20) maxSize = 20;
	while (data.size() > maxSize)
	{
		maxTempId = data.front()->tempId;
		data.pop_front();
	}
	if (maxTempId)
		fly_fire1(FinishedManagerListener::DroppedItems(), maxTempId);
}

void FinishedManager::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const DownloadPtr& d) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		const bool isFile = !qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP);
		if (isFile || (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST) && BOOLSETTING(LOG_FILELIST_TRANSFERS)))
		{
			auto ip = d->getUser()->getIP();
			auto item = std::make_shared<FinishedItem>(qi->getTarget(), d->getHintedUser(),
			                                           qi->getSize(), d->getRunningAverage(),
			                                           GET_TIME(), qi->getTTH(),
			                                           ip.is_unspecified() ? Util::emptyString : ip.to_string(), d->getActual());
			if (SETTING(DB_LOG_FINISHED_DOWNLOADS))
			{
				CFlylinkDBManager::getInstance()->addTransfer(e_TransferDownload, item);
			}
			addItem(item, e_Download);
			fly_fire2(FinishedManagerListener::AddedDl(), isFile, item);
			log(qi->getTarget(), d->getUser()->getCID(), ResourceManager::FINISHED_DOWNLOAD_FMT);
		}
	}
}

void FinishedManager::pushHistoryFinishedItem(const FinishedItemPtr& item, bool isFile, int type)
{
	if (type == e_Upload)
		fly_fire2(FinishedManagerListener::AddedUl(), isFile, item);
	else
		fly_fire2(FinishedManagerListener::AddedDl(), isFile, item);
}

void FinishedManager::on(UploadManagerListener::Complete, const UploadPtr& u) noexcept
{
	const bool isFile = u->getType() == Transfer::TYPE_FILE;
	if (isFile || (u->getType() == Transfer::TYPE_FULL_LIST && BOOLSETTING(LOG_FILELIST_TRANSFERS)))
	{
		auto ip = u->getUser()->getIP();
		auto item = std::make_shared<FinishedItem>(u->getPath(), u->getHintedUser(),
		                                           u->getFileSize(), u->getRunningAverage(),
		                                           GET_TIME(), u->getTTH(),
		                                           ip.is_unspecified() ? Util::emptyString : ip.to_string(), u->getActual());
		if (SETTING(DB_LOG_FINISHED_UPLOADS))
		{
			CFlylinkDBManager::getInstance()->addTransfer(e_TransferUpload, item);
		}
		addItem(item, e_Upload);
		fly_fire2(FinishedManagerListener::AddedUl(), isFile, item);
	}
}

void FinishedManager::log(const string& path, const CID& cid, ResourceManager::Strings message)
{
	string msg = str(dcpp_fmt(STRING_I(message))
		% Util::getFileName(path) % Util::toString(ClientManager::getNicks(cid, Util::emptyString)));
	LogManager::message(msg);
}
