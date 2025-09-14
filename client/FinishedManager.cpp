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
#include "SettingsManager.h"
#include "ClientManager.h"
#include "DatabaseManager.h"
#include "GlobalState.h"
#include "ConfCore.h"

FinishedManager::FinishedManager()
{
	tempId = 0;
	finished[e_Download].cs = std::unique_ptr<RWLock>(RWLock::create());
	finished[e_Upload].cs = std::unique_ptr<RWLock>(RWLock::create());
	updateSettings();

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
	WRITE_LOCK(*finished[type].cs);
	auto& items = finished[type].items;
	const auto it = find(items.begin(), items.end(), item);

	if (it != items.end())
	{
		items.erase(it);
		if (items.empty())
			finished[type].generationId = 0;
		else
			++finished[type].generationId;
		return true;
	}
	return false;
}

void FinishedManager::removeAll(eType type)
{
	WRITE_LOCK(*finished[type].cs);
	finished[type].items.clear();
	finished[type].generationId = 0;
}

void FinishedManager::addItem(FinishedItemPtr& item, eType type)
{
	int64_t maxTempId = 0;
	{
		WRITE_LOCK(*finished[type].cs);
		auto& data = finished[type].items;
		item->setTempID(++tempId);
		data.push_back(item);
		size_t maxSize = finished[type].maxSize;
		while (data.size() > maxSize)
		{
			maxTempId = data.front()->tempId;
			data.pop_front();
		}
		++finished[type].generationId;
	}
	if (maxTempId)
		fire(FinishedManagerListener::DroppedItems(), maxTempId);
}

void FinishedManager::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const DownloadPtr& d) noexcept
{
	if (!GlobalState::isShuttingDown())
	{
		const bool isFile = (qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)) == 0;
		if (isFile || ((qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST)) && (options & OPT_LOG_FILELIST_TRANSFERS)))
		{
			HintedUser hintedUser = d->getHintedUser();
			string hubs = Util::toString(ClientManager::getHubNames(hintedUser.user->getCID(), Util::emptyString));
			auto ip = Util::printIpAddress(d->getIP());
			auto item = std::make_shared<FinishedItem>(qi->getTarget(), hintedUser, hubs,
			                                           qi->getSize(), d->getRunningAverage(),
			                                           GET_TIME(), qi->getTTH(),
			                                           ip, d->getActual());
			if (options & OPT_LOG_FINISHED_DOWNLOADS)
				DatabaseManager::getInstance()->addTransfer(e_TransferDownload, item);
			addItem(item, e_Download);
			fire(FinishedManagerListener::AddedDl(), isFile, item);
		}
	}
}

void FinishedManager::pushHistoryFinishedItem(const FinishedItemPtr& item, bool isFile, int type)
{
	if (type == e_Upload)
		fire(FinishedManagerListener::AddedUl(), isFile, item);
	else
		fire(FinishedManagerListener::AddedDl(), isFile, item);
}

void FinishedManager::on(UploadManagerListener::Complete, const UploadPtr& u) noexcept
{
	const bool isFile = u->getType() == Transfer::TYPE_FILE;
	if (isFile || (u->getType() == Transfer::TYPE_FULL_LIST && (options & OPT_LOG_FILELIST_TRANSFERS)))
	{
		HintedUser hintedUser = u->getHintedUser();
		string hubs = Util::toString(ClientManager::getHubNames(hintedUser.user->getCID(), Util::emptyString));
		auto ip = Util::printIpAddress(u->getIP());
		auto item = std::make_shared<FinishedItem>(u->getPath(), hintedUser, hubs,
		                                           u->getFileSize(), u->getRunningAverage(),
		                                           GET_TIME(), u->getTTH(),
		                                           ip, u->getActual());
		DatabaseManager *db = DatabaseManager::getInstance();
		if (options & OPT_LOG_FINISHED_UPLOADS)
			db->addTransfer(e_TransferUpload, item);
		addItem(item, e_Upload);
		if (isFile && (options & OPT_ENABLE_UPLOAD_COUNTER))
		{
			auto hashDb = db->getHashDatabaseConnection();
			if (hashDb)
			{
				hashDb->putFileInfo(u->getTTH().data, 0, u->getFileSize(), nullptr, true);
				db->putHashDatabaseConnection(hashDb);
			}
		}
		fire(FinishedManagerListener::AddedUl(), isFile, item);
	}
}

void FinishedManager::updateSettings()
{
	int newOptions = 0;
	int maxSize[2];
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	if (ss->getInt(Conf::DB_LOG_FINISHED_DOWNLOADS))
		newOptions |= OPT_LOG_FINISHED_DOWNLOADS;
	if (ss->getInt(Conf::DB_LOG_FINISHED_UPLOADS))
		newOptions |= OPT_LOG_FINISHED_UPLOADS;
	if (ss->getBool(Conf::LOG_FILELIST_TRANSFERS))
		newOptions |= OPT_LOG_FILELIST_TRANSFERS;
	if (ss->getBool(Conf::ENABLE_UPLOAD_COUNTER))
		newOptions |= OPT_ENABLE_UPLOAD_COUNTER;
	maxSize[e_Download] = ss->getInt(Conf::MAX_FINISHED_DOWNLOADS);
	maxSize[e_Upload] = ss->getInt(Conf::MAX_FINISHED_UPLOADS);
	ss->unlockRead();

	options.store(newOptions);
	for (int i = 0; i < 2; ++i)
	{
		WRITE_LOCK(*finished[i].cs);
		finished[i].maxSize = std::max(20, maxSize[i]);
	}
}
