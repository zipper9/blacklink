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

#include "DownloadManager.h"
#include "SettingsManager.h"
#include "ConnectionManager.h"
#include "QueueManager.h"
#include "Download.h"
#include "MerkleCheckOutputStream.h"
#include "UploadManager.h"
#include "FinishedManager.h"
#include "IpTrust.h"
#include "MappingManager.h"
#include "ZUtils.h"
#include "FilteredFile.h"

int64_t DownloadManager::g_runningAverage;

DownloadManager::DownloadManager() : csDownloads(RWLock::create())
{
	TimerManager::getInstance()->addListener(this);
}

DownloadManager::~DownloadManager()
{
	TimerManager::getInstance()->removeListener(this);
	while (true)
	{
		{
			READ_LOCK(*csDownloads);
			if (downloads.empty())
			{
				break;
			}
		}
		Thread::sleep(50);
	}
}

size_t DownloadManager::getDownloadCount() const noexcept
{
	READ_LOCK(*csDownloads);
	return downloads.size();
}

void DownloadManager::clearDownloads() noexcept
{
	WRITE_LOCK(*csDownloads);
	downloads.clear();
}

void DownloadManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	typedef vector<pair<std::string, UserPtr> > TargetList;
	TargetList dropTargets;
	
	DownloadArray tickList;
	{
		READ_LOCK(*csDownloads);
		// Tick each ongoing download
		tickList.reserve(downloads.size());
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		{
			auto d = *i;
			if (d->getPos() > 0)
			{
				DownloadData td(d->getQueueItem());
				td.token = d->getConnectionQueueToken();
				td.hintedUser = d->getHintedUser();
				td.pos = d->getPos();
				td.startPos = d->getStartPos();
				td.secondsLeft = d->getSecondsLeft();
				td.speed = d->getRunningAverage();
				td.startTime = d->getStartTime();
				td.size = d->getSize();
				td.fileSize = d->getFileSize();
				td.type = d->getType();
				td.path = d->getPath();
				td.transferFlags = TRANSFER_FLAG_DOWNLOAD;
				if (d->isSet(Download::FLAG_DOWNLOAD_PARTIAL))
					td.transferFlags |= TRANSFER_FLAG_PARTIAL;
				if (d->isSecure)
				{
					td.transferFlags |= TRANSFER_FLAG_SECURE;
					if (d->isTrusted) td.transferFlags |= TRANSFER_FLAG_TRUSTED;
				}
				if (d->isSet(Download::FLAG_TTH_CHECK))
					td.transferFlags |= TRANSFER_FLAG_TTH_CHECK;
				if (d->isSet(Download::FLAG_ZDOWNLOAD))
					td.transferFlags |= TRANSFER_FLAG_COMPRESSED;
				if (d->isSet(Download::FLAG_CHUNKED))
					td.transferFlags |= TRANSFER_FLAG_CHUNKED;
				td.dumpToLog();
				tickList.push_back(td);
				d->updateSpeed(tick);
			}
			const int64_t currentSingleSpeed = d->getRunningAverage();
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
			if (BOOLSETTING(ENABLE_AUTO_DISCONNECT))
			{
				if (d->getType() == Transfer::TYPE_FILE && d->getStartTime() > 0)
				{
					if (d->getTigerTree().getFileSize() > (SETTING(AUTO_DISCONNECT_MIN_FILE_SIZE) * 1048576))
					{
						if (currentSingleSpeed < SETTING(AUTO_DISCONNECT_SPEED) * 1024 && d->getLastNormalSpeed())
						{
							if (tick - d->getLastNormalSpeed() > (uint32_t)SETTING(AUTO_DISCONNECT_TIME) * 1000)
							{
								if (QueueManager::getInstance()->dropSource(d))
								{
									dropTargets.push_back(make_pair(d->getPath(), d->getUser()));
									d->setLastNormalSpeed(0);
								}
							}
						}
						else
						{
							d->setLastNormalSpeed(tick);
						}
					}
				}
			}
#endif // BL_FEATURE_DROP_SLOW_SOURCES
		}
	}
	if (!tickList.empty())
	{
		fire(DownloadManagerListener::Tick(), tickList);
	}
	
	for (auto i = dropTargets.cbegin(); i != dropTargets.cend(); ++i)
	{
		QueueManager::getInstance()->removeSource(i->first, i->second, QueueItem::Source::FLAG_SLOW_SOURCE);
	}
}

void DownloadManager::addConnection(UserConnectionPtr& ucPtr)
{
	UserConnection* conn = ucPtr.get();
	if (!conn->isSet(UserConnection::FLAG_SUPPORTS_TTHF) || !conn->isSet(UserConnection::FLAG_SUPPORTS_ADCGET))
	{
		// Can't download from these...
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		ClientManager::setClientStatus(conn->getUser(), STRING(SOURCE_TOO_OLD), -1, true);
#endif
		QueueManager::getInstance()->removeSource(conn->getUser(), QueueItem::Source::FLAG_NO_TTHF);
		conn->disconnect();
		return;
	}
	if (conn->isIpBlocked(true))
	{
		conn->disconnect();
		return;
	}
	checkDownloads(ucPtr);
}

bool DownloadManager::isStartDownload(QueueItem::Priority prio) const noexcept
{
	const size_t downloadCount = getDownloadCount();
	const size_t slots = SETTING(DOWNLOAD_SLOTS);
	const int64_t maxSpeed = SETTING(MAX_DOWNLOAD_SPEED);

	if ((slots && downloadCount >= slots) || (maxSpeed && getRunningAverage() >= maxSpeed << 10))
	{
		if (prio != QueueItem::HIGHEST)
			return false;
		if (slots && downloadCount >= slots + SETTING(EXTRA_DOWNLOAD_SLOTS))
			return false;
	}
	return true;
}

void DownloadManager::checkDownloads(const UserConnectionPtr& ucPtr, bool quickCheck) noexcept
{
	UserConnection* conn = ucPtr.get();
	const QueueItem::Priority prio = QueueManager::hasDownload(conn->getUser());
	if (quickCheck && prio == QueueItem::PAUSED)
		return;
	if (!isStartDownload(prio))
	{
		conn->disconnect();
		return;
	}

	Download::ErrorInfo errorInfo;
	auto d = QueueManager::getInstance()->getDownload(ucPtr, errorInfo);

	if (!d)
	{
		conn->setState(UserConnection::STATE_IDLE);
		if (errorInfo.error != QueueManager::SUCCESS)
			fire(DownloadManagerListener::Status(), conn, errorInfo);
		return;
	}
	
	conn->setState(UserConnection::STATE_SND);
	
	if (conn->isSet(UserConnection::FLAG_SUPPORTS_XML_BZLIST) && d->getType() == Transfer::TYPE_FULL_LIST)
	{
		d->setFlag(Download::FLAG_XML_BZ_LIST);
	}
	
	{
		WRITE_LOCK(*csDownloads);
		dcassert(d->getUser());
		downloads.push_back(d);
	}
	fire(DownloadManagerListener::Requesting(), d);
	
	dcdebug("Requesting " I64_FMT "/" I64_FMT "\n", d->getStartPos(), d->getSize());
	AdcCommand cmd(AdcCommand::CMD_GET);
	d->getCommand(cmd, conn->isSet(UserConnection::FLAG_SUPPORTS_ZLIB_GET));
	conn->send(cmd);
}

void DownloadManager::processSND(UserConnection* source, const AdcCommand& cmd) noexcept
{
	auto d = source->getDownload();
	if (!d)
	{
		dcassert(0);
		source->disconnect(true);
		return;
	}
	
	const string& type = cmd.getParam(0);
	const int64_t start = Util::toInt64(cmd.getParam(2));
	const int64_t bytes = Util::toInt64(cmd.getParam(3));
	
	if (type != Transfer::fileTypeNames[d->getType()])
	{
		// Uhh??? We didn't ask for this...
		dcassert(0);
		source->disconnect();
		return;
	}
	
	if (cmd.hasFlag("TL", 4))
		d->setFlag(Download::FLAG_TTH_LIST);
	startData(source, start, bytes, cmd.hasFlag("ZL", 4));
}

void DownloadManager::startData(UserConnection* source, int64_t start, int64_t bytes, bool z)
{
	auto d = source->getDownload();
	dcassert(d);
	
	dcdebug("Preparing " I64_FMT ":" I64_FMT ", " I64_FMT ":" I64_FMT "\n", d->getStartPos(), start, d->getSize(), bytes);
	if (d->getSize() == -1)
	{
		if (bytes >= 0)
		{
			d->setSize(bytes);
		}
		else
		{
			failDownload(source, STRING(INVALID_SIZE), true);
			return;
		}
	}
	else if (d->getSize() != bytes || d->getStartPos() != start)
	{
		// This is not what we requested...
		failDownload(source, STRING(INVALID_SIZE), true);
		return;
	}
	
	try
	{
		QueueManager::getInstance()->setFile(d);
	}
	catch (const FileException& e)
	{
		failDownload(source, STRING(COULD_NOT_OPEN_TARGET_FILE) + e.getError(), true);
		return;
	}
	catch (const QueueException& e)
	{
		failDownload(source, e.getError(), true);
		return;
	}
	
	int64_t bufSize = (int64_t) SETTING(BUFFER_SIZE_FOR_DOWNLOADS) << 10;
	dcassert(bufSize > 0);
	if (bufSize <= 0 || bufSize > 8192 * 1024)
		bufSize = 1024 * 1024;
	try
	{
		if (d->getType() == Transfer::TYPE_FILE || d->getType() == Transfer::TYPE_FULL_LIST)
		{
			const int64_t fileSize = d->getSize();
			auto file = new BufferedOutputStream<true>(d->getDownloadFile(), std::min(fileSize, bufSize));
			d->setDownloadFile(file);
		}
	}
	catch (const Exception& e)
	{
		failDownload(source, e.getError(), true);
		return;
	}
	
	if (d->getType() == Transfer::TYPE_FILE && d->getTreeValid())
	{
		typedef MerkleCheckOutputStream<TigerTree, true> MerkleStream;
		d->setDownloadFile(new MerkleStream(d->getTigerTree(), d->getDownloadFile(), d->getStartPos()));
		d->setFlag(Download::FLAG_TTH_CHECK);
	}
	
	// Check that we don't get too many bytes
	d->setDownloadFile(new LimitedOutputStream(d->getDownloadFile(), bytes));
	
	if (z)
	{
		d->setFlag(Download::FLAG_ZDOWNLOAD);
		d->setDownloadFile(new FilteredOutputStream<UnZFilter, true>(d->getDownloadFile()));
	}
	
	d->setStartTime(source->getLastActivity());
	
	source->setState(UserConnection::STATE_RUNNING);
	
#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
	fire(DownloadManagerListener::Starting(), d);
#endif
	if (d->getPos() == d->getSize())
	{
		try
		{
			// Already finished? A zero-byte file list could cause this...
			endData(source);
		}
		catch (const Exception& e)
		{
			failDownload(source, e.getError(), true);
		}
	}
	else
	{
		source->setDataMode();
	}
}

void DownloadManager::onData(UserConnection* source, const uint8_t* data, size_t len) noexcept
{
	auto d = source->getDownload();
	dcassert(d);
	try
	{
		d->addPos(d->getDownloadFile()->write(data, len), len);
		d->updateSpeed(source->getLastActivity());
		
		if (d->getDownloadFile()->eof())
		{
			endData(source);
			source->setLineMode();
		}
	}
	catch (const Exception& e)
	{
		// d->resetPos(); // is there a better way than resetting the position?
		failDownload(source, e.getError(), true);
	}
	
}

/** Download finished! */
void DownloadManager::endData(UserConnection* source)
{
	dcassert(source->getState() == UserConnection::STATE_RUNNING);
	auto d = source->getDownload();
	dcassert(d);
	d->updateSpeed(source->getLastActivity());
	
	if (d->getType() == Transfer::TYPE_TREE)
	{
		d->getDownloadFile()->flushBuffers(false);
		
		int64_t bl = 1024;
		auto &tree = d->getTigerTree();
		while (bl * (int64_t)tree.getLeaves().size() < tree.getFileSize())
		{
			bl *= 2;
		}
		tree.setBlockSize(bl);
		tree.calcRoot();
		
		if (!(d->getTTH() == tree.getRoot()))
		{
			// This tree is for a different file, remove from queue
			removeDownload(d);
			fire(DownloadManagerListener::Failed(), d, STRING(INVALID_TREE));
			
			auto qm = QueueManager::getInstance();
			qm->removeSource(d->getPath(), source->getUser(), QueueItem::Source::FLAG_BAD_TREE, false);
			qm->putDownload(d, false);
			
			UserConnectionPtr ucPtr = ConnectionManager::getInstance()->findConnection(source);
			if (ucPtr) checkDownloads(ucPtr);
			return;
		}
		d->setTreeValid(true);
	}
	else
	{
		// First, finish writing the file (flushing the buffers and closing the file...)
		try
		{
			d->getDownloadFile()->flushBuffers(true);
		}
		catch (const Exception& e)
		{
			d->resetPos();
			failDownload(source, e.getError(), true);
			return;
		}
		
		source->setSpeed(d->getRunningAverage());
		source->updateChunkSize(d->getTigerTree().getBlockSize(), d->getSize(), GET_TICK() - d->getStartTime());
		
		dcdebug("Download finished: %s, size " I64_FMT ", pos: " I64_FMT "\n", d->getPath().c_str(), d->getSize(), d->getPos());
	}

	removeDownload(d);
	QueueManager::getInstance()->putDownload(d, true, false);

	if (d->getType() != Transfer::TYPE_FILE || d->getQueueItem()->isFinished())
		fire(DownloadManagerListener::Complete(), d);

	UserConnectionPtr ucPtr = ConnectionManager::getInstance()->findConnection(source);
	if (ucPtr) checkDownloads(ucPtr);
}

void DownloadManager::noSlots(UserConnection* source, const string& param) noexcept
{
	string extra = param.empty() ? Util::emptyString : " - " + STRING(QUEUED) + ' ' + param;
	source->setState(UserConnection::STATE_TRY_AGAIN);
	failDownload(source, STRING(NO_SLOTS_AVAILABLE) + extra, false);
}

void DownloadManager::fail(UserConnection* source, const string& error) noexcept
{
	failDownload(source, error, true);
}

void DownloadManager::failDownload(UserConnection* source, const string& reason, bool disconnect)
{
	auto d = source->getDownload();
	if (d)
	{
		removeDownload(d);
		fire(DownloadManagerListener::Failed(), d, reason);
		d->setReason(reason);
		QueueManager::getInstance()->putDownload(d, false);
	}
#ifdef _DEBUG
	LogManager::message("DownloadManager::failDownload reason =" + reason);
#endif // _DEBUG
	if (disconnect)
		source->disconnect();
}

void DownloadManager::removeDownload(const DownloadPtr& d)
{
	if (d->getDownloadFile())
	{
		if (d->getActual() > 0)
		{
			try
			{
				d->getDownloadFile()->flushBuffers(false);
			}
#ifdef _DEBUG
			catch (const Exception& e)
			{
				LogManager::message("DownloadManager::removeDownload error =" + string(e.what()));
			}
#else
			catch (const Exception&) {}
#endif
		}
	}

	{
		WRITE_LOCK(*csDownloads);
		if (!downloads.empty())
		{
			auto i = remove(downloads.begin(), downloads.end(), d);
			if (i != downloads.end())
				downloads.erase(i, downloads.end());
		}
	}
}

void DownloadManager::abortDownload(const string& target)
{
	READ_LOCK(*csDownloads);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		const auto& d = *i;
		if (d->getPath() == target)
		{
			dcdebug("Trying to close connection for download %p\n", d.get());
			d->disconnect(true);
		}
	}
}

/** @todo Handle errors better */
void DownloadManager::processSTA(UserConnection* source, const AdcCommand& cmd) noexcept
{
	if (cmd.getParameters().size() < 2)
	{
		source->disconnect();
		return;
	}
	
	const string& err = cmd.getParameters()[0];
	if (err.length() != 3)
	{
		source->disconnect();
		return;
	}
	
	switch (err[0] - '0')
	{
		case AdcCommand::SEV_FATAL:
			source->disconnect();
			return;
		case AdcCommand::SEV_RECOVERABLE:
			switch (Util::toInt(err.substr(1)))
			{
				case AdcCommand::ERROR_FILE_NOT_AVAILABLE:
					fileNotAvailable(source);
					return;
				case AdcCommand::ERROR_SLOTS_FULL:
					string param;
					cmd.getParam("QP", 1, param);
					noSlots(source, param);
					return;
			}
		case AdcCommand::SEV_SUCCESS:
			// We don't know any messages that would give us these...
			dcdebug("Unknown success message %s %s", err.c_str(), cmd.getParam(1).c_str());
			return;
	}
	source->disconnect();
}

void DownloadManager::fileNotAvailable(UserConnection* source)
{
	auto d = source->getDownload();
	dcassert(d);
	dcdebug("File Not Available: %s\n", d->getPath().c_str());
	
	removeDownload(d);
	fire(DownloadManagerListener::Failed(), d, STRING(FILE_NOT_AVAILABLE));
	
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	if (d->isSet(Download::FLAG_USER_CHECK))
	{
		ClientManager::setClientStatus(source->getUser(), "Filelist Not Available", SETTING(AUTOBAN_FL_UNAVAILABLE), false);
	}
#endif
	
	auto qm = QueueManager::getInstance();
	qm->removeSource(d->getPath(), source->getUser(), (Flags::MaskType)(d->getType() == Transfer::TYPE_TREE ? QueueItem::Source::FLAG_NO_TREE : QueueItem::Source::FLAG_FILE_NOT_AVAILABLE), false);
	d->setReason(STRING(FILE_NOT_AVAILABLE));
	qm->putDownload(d, false);
	UserConnectionPtr ucPtr = ConnectionManager::getInstance()->findConnection(source);
	checkDownloads(ucPtr);
}

bool DownloadManager::checkFileDownload(const UserPtr& user) const
{
	READ_LOCK(*csDownloads);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		const auto d = *i;
		if (d->getUser() == user &&
		    d->getType() != Download::TYPE_PARTIAL_LIST &&
		    d->getType() != Download::TYPE_FULL_LIST &&
		    d->getType() != Download::TYPE_TREE)
				return true;
	}
	return false;
}

void DownloadManager::checkUserIP(UserConnection* source) noexcept
{
	auto d = source->getDownload();
	dcassert(d);
	removeDownload(d);
	QueueManager::getInstance()->putDownload(d, true);
	source->disconnect();
}
