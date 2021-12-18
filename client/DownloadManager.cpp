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

std::unique_ptr<RWLock> DownloadManager::g_csDownload = std::unique_ptr<RWLock>(RWLock::create());
DownloadList DownloadManager::g_download_map;
UserConnectionList DownloadManager::g_idlers;
int64_t DownloadManager::g_runningAverage;

DownloadManager::DownloadManager()
{
	TimerManager::getInstance()->addListener(this);
}

DownloadManager::~DownloadManager()
{
	TimerManager::getInstance()->removeListener(this);
	while (true)
	{
		{
			READ_LOCK(*g_csDownload);
			if (g_download_map.empty())
			{
				break;
			}
		}
		Thread::sleep(50);
		// dcassert(0);
		// TODO - возможно мы тут висим и не даем разрушитьс€ менеджеру?
		// ƒобавить логирование тиков на флай сервер
	}
}

size_t DownloadManager::getDownloadCount()
{
	READ_LOCK(*g_csDownload);
	return g_download_map.size();
}

void DownloadManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
		
	typedef vector<pair<std::string, UserPtr> > TargetList;
	TargetList dropTargets;
	
	DownloadArray tickList;
	{
		READ_LOCK(*g_csDownload);
		// Tick each ongoing download
		tickList.reserve(g_download_map.size());
		for (auto i = g_download_map.cbegin(); i != g_download_map.cend(); ++i)
		{
			auto d = *i;
			if (d->getPos() > 0) // https://drdump.com/DumpGroup.aspx?DumpGroupID=614035&Login=guest (Wine)
			{
				TransferData td;
				td.token = d->getConnectionQueueToken();
				td.hintedUser = d->getHintedUser();
				td.pos = d->getPos();
				td.actual = d->getActual();
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
#ifdef FLYLINKDC_USE_DROP_SLOW
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
#endif // FLYLINKDC_USE_DROP_SLOW
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

void DownloadManager::removeIdleConnection(UserConnection* source)
{
	WRITE_LOCK(*g_csDownload);
	if (!ClientManager::isBeforeShutdown())
	{
		dcassert(source->getUser());
		auto i = find(g_idlers.begin(), g_idlers.end(), source);
		if (i == g_idlers.end())
		{
			//dcassert(i != g_idlers.end());
			return;
		}
		g_idlers.erase(i);
	}
	else
	{
		g_idlers.clear();
	}
}

void DownloadManager::checkIdle(const UserPtr& user)
{
	if (!ClientManager::isBeforeShutdown())
	{
		READ_LOCK(*g_csDownload);
		dcassert(user);
		for (auto i = g_idlers.begin(); i != g_idlers.end(); ++i)
		{
			UserConnection* uc = *i;
			if (uc->getUser() == user)
			{
				uc->updated();
				return;
			}
		}
	}
}

void DownloadManager::addConnection(UserConnection* conn)
{
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
	checkDownloads(conn);
}

bool DownloadManager::isStartDownload(QueueItem::Priority prio)
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

void DownloadManager::checkDownloads(UserConnection* conn)
{
	if (ClientManager::isBeforeShutdown())
	{
		conn->disconnect();
#ifdef _DEBUG
		LogManager::message("DownloadManager::checkDownloads + isShutdown");
#endif
		return;
	}
	
	auto qm = QueueManager::getInstance();
	const QueueItem::Priority prio = QueueManager::hasDownload(conn->getUser());
	if (!isStartDownload(prio))
	{
		conn->disconnect();
		return;
	}

	Download::ErrorInfo errorInfo;
	auto d = qm->getDownload(conn, errorInfo);

	if (!d)
	{
		if (errorInfo.error != QueueManager::SUCCESS)
		{
			fire(DownloadManagerListener::Status(), conn, errorInfo);
		}
		
		conn->setState(UserConnection::STATE_IDLE);
		if (!ClientManager::isBeforeShutdown())
		{
			WRITE_LOCK(*g_csDownload);
			dcassert(conn->getUser());
			g_idlers.push_back(conn);
		}
		return;
	}
	
	conn->setState(UserConnection::STATE_SND);
	
	if (conn->isSet(UserConnection::FLAG_SUPPORTS_XML_BZLIST) && d->getType() == Transfer::TYPE_FULL_LIST)
	{
		d->setFlag(Download::FLAG_XML_BZ_LIST);
	}
	
	{
		WRITE_LOCK(*g_csDownload);
		dcassert(d->getUser());
		g_download_map.push_back(d);
	}
	fire(DownloadManagerListener::Requesting(), d);
	
	dcdebug("Requesting " I64_FMT "/" I64_FMT "\n", d->getStartPos(), d->getSize());
	AdcCommand cmd(AdcCommand::CMD_GET);
	d->getCommand(cmd, conn->isSet(UserConnection::FLAG_SUPPORTS_ZLIB_GET));
	conn->send(cmd);
}

void DownloadManager::processSND(UserConnection* source, const AdcCommand& cmd) noexcept
{
	if (!source->getDownload())
	{
		dcassert(0);
		source->disconnect(true);
		return;
	}
	
	const string& type = cmd.getParam(0);
	const int64_t start = Util::toInt64(cmd.getParam(2));
	const int64_t bytes = Util::toInt64(cmd.getParam(3));
	
	if (type != Transfer::fileTypeNames[source->getDownload()->getType()])
	{
		// Uhh??? We didn't ask for this...
		dcassert(0);
		source->disconnect();
		return;
	}
	
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
			failDownload(source, STRING(INVALID_SIZE));
			return;
		}
	}
	else if (d->getSize() != bytes || d->getStartPos() != start)
	{
		// This is not what we requested...
		failDownload(source, STRING(INVALID_SIZE));
		return;
	}
	
	try
	{
		QueueManager::getInstance()->setFile(d);
	}
	catch (const FileException& e)
	{
		failDownload(source, STRING(COULD_NOT_OPEN_TARGET_FILE) + e.getError());
		return;
	}
	catch (const QueueException& e)
	{
		failDownload(source, e.getError());
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
		failDownload(source, e.getError());
		return;
	}
	
	if (d->getType() == Transfer::TYPE_FILE)
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
		d->setDownloadFile(new FilteredOutputStream<UnZFilter, true> (d->getDownloadFile()));
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
			failDownload(source, e.getError());
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
		failDownload(source, e.getError());
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
			// This tree is for a different file, remove from queue...// [!]PPA TODO подтереть fly_hash_block
			removeDownload(d);
			fire(DownloadManagerListener::Failed(), d, STRING(INVALID_TREE));
			
			QueueManager::getInstance()->removeSource(d->getPath(), source->getUser(), QueueItem::Source::FLAG_BAD_TREE, false);
			
			QueueManager::getInstance()->putDownload(d->getPath(), d, false);
			
			checkDownloads(source);
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
		catch (const Exception& e) //http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2154
		{
			d->resetPos();
			failDownload(source, e.getError());
			return;
		}
		
		source->setSpeed(d->getRunningAverage());
		source->updateChunkSize(d->getTigerTree().getBlockSize(), d->getSize(), GET_TICK() - d->getStartTime());
		
		dcdebug("Download finished: %s, size " I64_FMT ", pos: " I64_FMT "\n", d->getPath().c_str(), d->getSize(), d->getPos());
	}
	
	removeDownload(d);
	
	if (d->getType() != Transfer::TYPE_FILE)
	{
		fire(DownloadManagerListener::Complete(), d);
		//if (d->getUserConnection())
		//{
		//  const auto l_token = d->getConnectionQueueToken();
		//  fire(DownloadManagerListener::RemoveToken(), l_token);
		//}
	}
	QueueManager::getInstance()->putDownload(d->getPath(), d, true, false);
	checkDownloads(source);
}

void DownloadManager::noSlots(UserConnection* source, const string& param) noexcept
{
	string extra = param.empty() ? Util::emptyString : " - " + STRING(QUEUED) + ' ' + param;
	failDownload(source, STRING(NO_SLOTS_AVAILABLE) + extra);
}

void DownloadManager::fail(UserConnection* source, const string& error) noexcept
{
	removeIdleConnection(source);
	failDownload(source, error);
}

void DownloadManager::failDownload(UserConnection* source, const string& reason)
{
	auto d = source->getDownload();
	if (d)
	{
		const std::string path = d->getPath();
		removeDownload(d);
		fire(DownloadManagerListener::Failed(), d, reason);
		
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		if (d->isSet(Download::FLAG_USER_CHECK))
		{
			if (reason == STRING(DISCONNECTED))
			{
				ClientManager::fileListDisconnected(source->getUser());
			}
			else
			{
				ClientManager::setClientStatus(source->getUser(), reason, -1, false);
			}
		}
#endif
		d->setReason(reason);
		QueueManager::getInstance()->putDownload(path, d, false);
	}
#ifdef _DEBUG
	LogManager::message("DownloadManager::failDownload reason =" + reason);
#endif // _DEBUG
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
		WRITE_LOCK(*g_csDownload);
		if (!g_download_map.empty())
		{
			//dcassert(find(g_download_map.begin(), g_download_map.end(), d) != g_download_map.end());
			auto l_end = remove(g_download_map.begin(), g_download_map.end(), d);
			if (l_end != g_download_map.end())
			{
				g_download_map.erase(l_end, g_download_map.end());
			}
		}
	}
}

void DownloadManager::abortDownload(const string& target)
{
	READ_LOCK(*g_csDownload);
	for (auto i = g_download_map.cbegin(); i != g_download_map.cend(); ++i)
	{
		const auto& d = *i;
		if (d->getPath() == target)
		{
			dcdebug("Trying to close connection for download %p\n", d.get());
			d->getUserConnection()->disconnect(true);
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
	
	switch (Util::toInt(err.substr(0, 1)))
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
					cmd.getParam("QP", 0, param);
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

void DownloadManager::processUpdatedConnection(UserConnection* source) noexcept
{
	removeIdleConnection(source);
	checkDownloads(source);
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
	
	QueueManager::getInstance()->removeSource(d->getPath(), source->getUser(), (Flags::MaskType)(d->getType() == Transfer::TYPE_TREE ? QueueItem::Source::FLAG_NO_TREE : QueueItem::Source::FLAG_FILE_NOT_AVAILABLE), false);
	
	d->setReason(STRING(FILE_NOT_AVAILABLE));
	QueueManager::getInstance()->putDownload(d->getPath(), d, false);
	checkDownloads(source);
}

bool DownloadManager::checkFileDownload(const UserPtr& user)
{
	READ_LOCK(*g_csDownload);
	for (auto i = g_download_map.cbegin(); i != g_download_map.cend(); ++i)
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
	QueueManager::getInstance()->putDownload(d->getPath(), d, true);
	
	source->disconnect();
}
