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
#include <cmath>

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

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/session.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/announce_entry.hpp"
#include "libtorrent/torrent_status.hpp"
#include "libtorrent/create_torrent.hpp"
#include "libtorrent/hex.hpp"
#include "libtorrent/write_resume_data.hpp"
#include "libtorrent/magnet_uri.hpp"
#include "DatabaseManager.h"
#endif

std::unique_ptr<RWLock> DownloadManager::g_csDownload = std::unique_ptr<RWLock>(RWLock::create());
DownloadList DownloadManager::g_download_map;
UserConnectionList DownloadManager::g_idlers;
int64_t DownloadManager::g_runningAverage;

DownloadManager::DownloadManager()
{
	TimerManager::getInstance()->addListener(this);
}

#ifdef FLYLINKDC_USE_TORRENT
static int g_outstanding_resume_data;

void DownloadManager::shutdown_torrent()
{
	if (m_torrent_session)
	{
		/*
		std::vector<lt::torrent_handle> handles = m_torrent_session->get_torrents();
		m_torrent_session->pause();
		while (!m_torrent_session->is_paused())
		{
		 Sleep(10);
		}
		g_outstanding_resume_data = 0;
		for (lt::torrent_handle i : handles)
		{
		 lt::torrent_handle& h = i;
		 if (!h.is_valid()) continue;
		 lt::torrent_status s = h.status();
		 if (!s.has_metadata) continue;
		 if (!h.need_save_resume_data()) continue;
		
		 h.save_resume_data();
		 ++g_outstanding_resume_data;
		}
		
		while (g_outstanding_resume_data > 0)
		{
		 auto const* a = m_torrent_session->wait_for_alert(lt::seconds(10));
		
		 // if we don't get an alert within 10 seconds, abort
		 if (a == 0)
		 {
		     dcassert(0);
		     break;
		 }
		
		 std::vector<lt::alert*> alerts;
		 m_torrent_session->pop_alerts(&alerts);
		
		 for (auto i : alerts)
		 {
		     if (lt::alert_cast<lt::save_resume_data_failed_alert>(a))
		     {
		         //process_alert(a);
		         --g_outstanding_resume_data;
		         continue;
		     }
		
		     auto const* rd = lt::alert_cast<lt::save_resume_data_alert>(a);
		     if (rd == nullptr)
		     {
		         //process_alert(a);
		         continue;
		     }
		
		     auto const l_resume = lt::write_resume_data_buf(rd->params);
		     const lt::torrent_status st = rd->handle.status(lt::torrent_handle::query_name);
		     DatabaseManager::getInstance()->save_torrent_resume(rd->handle.info_hash(), st.name, l_resume);
		     --g_outstanding_resume_data;
		 }
		}
		*/
		
		m_torrent_session->pause();
		while (!m_torrent_session->is_paused())
		{
			Sleep(10);
		}
#ifdef _DEBUG
		int l_count = 0;
#endif
		for (auto i : m_torrents)
		{
			const lt::torrent_status s = i.status();
			if (lt::is_save_resume(s))
			{
				s.handle.save_resume_data();
				++m_torrent_resume_count;
#ifdef _DEBUG
				LogManager::message("[start] save_resume_data. m_torrent_resume_count = " +
				                    Util::toString(m_torrent_resume_count) + " l_count = " + Util::toString(++l_count));
#endif
			}
			else
			{
#ifdef _DEBUG
				LogManager::message("[skip] save_resume_data. m_torrent_resume_count = " +
				                    Util::toString(m_torrent_resume_count) + " l_count = " + Util::toString(++l_count));
#endif
			}
		}
		while (m_torrent_resume_count > 0)
		{
			Sleep(100);
			int l_count_need_save = 0;
			for (auto i : m_torrents) // TODO - fix copy-paste
			{
				const lt::torrent_status s = i.status();
				if (lt::is_save_resume(s)) // TODO https://github.com/qbittorrent/qBittorrent/pull/7569/files
				{
					l_count_need_save++;
				}
			}
			LogManager::message("[sleep] wait m_torrent_resume_count = "
			                    + Util::toString(m_torrent_resume_count) + " l_count_need_save = " + Util::toString(l_count_need_save));
			if (l_count_need_save == 0)
			{
				break;
			}
		}
		dcassert(m_torrent_resume_count == 0);
		m_torrent_session.reset();
	}
}

#endif // FLYLINKDC_USE_TORRENT

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
		// TODO - возможно мы тут висим и не даем разрушиться менеджеру?
		// Добавить логирование тиков на флай сервер
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
		fly_fire1(DownloadManagerListener::Tick(), tickList);
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
			fly_fire2(DownloadManagerListener::Status(), conn, errorInfo);
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
	fly_fire1(DownloadManagerListener::Requesting(), d);
	
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
		failDownload(source, STRING(COULD_NOT_OPEN_TARGET_FILE) + ' ' + e.getError());
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
	fly_fire1(DownloadManagerListener::Starting(), d);
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
			fly_fire2(DownloadManagerListener::Failed(), d, STRING(INVALID_TREE));
			
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
		fly_fire1(DownloadManagerListener::Complete(), d);
		//if (d->getUserConnection())
		//{
		//  const auto l_token = d->getConnectionQueueToken();
		//  fly_fire1(DownloadManagerListener::RemoveToken(), l_token);
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
		fly_fire2(DownloadManagerListener::Failed(), d, reason);
		
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
			catch (const Exception& e)
			{
#ifdef _DEBUG
				//dcassert(0);
				LogManager::message("DownloadManager::removeDownload error =" + string(e.what()));
#endif // _DEBUG
			}
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
	fly_fire2(DownloadManagerListener::Failed(), d, STRING(FILE_NOT_AVAILABLE));
	
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	if (d->isSet(Download::FLAG_USER_CHECK))
	{
		ClientManager::setClientStatus(source->getUser(), "Filelist Not Available", SETTING(FILELIST_UNAVAILABLE), false);
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

#ifdef FLYLINKDC_USE_TORRENT

using namespace libtorrent;
namespace lt = libtorrent;

void DownloadManager::select_files(const libtorrent::torrent_handle& p_torrent_handle)
{
	CFlyTorrentFileArray l_files;
	if (p_torrent_handle.is_valid())
	{
		auto l_file = p_torrent_handle.torrent_file();
		l_files.reserve(l_file->num_files());
		//const std::string torrentName = p_torrent_handle.status(torrent_handle::query_name).name;
		const file_storage fileStorage = l_file->files();
		for (int i = 0; i < fileStorage.num_files(); i++)
		{
			p_torrent_handle.file_priority(file_index_t(i), dont_download);
			CFlyTorrentFile l_item;
			l_item.m_file_path = fileStorage.file_path(file_index_t(i));
			l_item.m_size = fileStorage.file_size(file_index_t(i));
			l_files.push_back(l_item);
#ifdef _DEBUG
			LogManager::torrent_message("metadata_received_alert: File = " + l_item.m_file_path);
#endif
		}
		p_torrent_handle.unset_flags(lt::torrent_flags::auto_managed);
		const auto l_torrent_info = p_torrent_handle.torrent_file();
		p_torrent_handle.pause();
		fly_fire3(DownloadManagerListener::SelectTorrent(), l_file->info_hash(), l_files, l_torrent_info);
	}
}

void DownloadManager::onTorrentAlertNotify(libtorrent::session* p_torrent_sesion)
{
	try
	{
		p_torrent_sesion->get_io_service().post([p_torrent_sesion, this]
		{
			std::vector<lt::alert*> alerts;
			p_torrent_sesion->pop_alerts(&alerts);
			unsigned l_alert_pos = 0;
			for (lt::alert const * a : alerts)
			{
				++l_alert_pos;
				try
				{
#if 0
//#ifdef _DEBUG
					if (const auto l_port = lt::alert_cast<lt::log_alert>(a))
					{
						// LogManager::torrent_message("log_alert: " + a->message() + " info:" + std::string(a->what()));
					}
					else
					{
						std::string l_dbg_message = ".:::. TorrentAllert:" + a->message() + " info:" + std::string(a->what() + std::string(" typeid:") + std::string(typeid(*a).name()));
						if (std::string(a->what()) != "torrent_log_alert"
#ifndef TORRENT_NO_STATE_CHANGES_ALERTS
						        && std::string(typeid(*a).name()) != "struct libtorrent::state_update_alert" &&
#endif
#ifndef TORRENT_NO_BLOCK_ALERTS
						        std::string(typeid(*a).name()) != "struct libtorrent::block_finished_alert" &&  // TODO - opt
						        std::string(typeid(*a).name()) != "struct libtorrent::block_downloading_alert" &&
						        std::string(typeid(*a).name()) != "struct libtorrent::block_timeout_alert" &&
#endif // #ifndef TORRENT_NO_BLOCK_ALERTS
#ifndef TORRENT_NO_PIECE_ALERTS
						        std::string(typeid(*a).name()) != "struct libtorrent::piece_finished_alert"
#endif
						   )
						{
							// LogManager::torrent_message(l_dbg_message);
						}
						if (const auto l_port = lt::alert_cast<lt::torrent_log_alert>(a))
						{
							// LogManager::torrent_message("torrent_log_alert: " + a->message() + " info:" + std::string(a->what()));
						}
					}
#endif
#if 0 // FIXME FIXME
					if (const auto l_ext_ip = lt::alert_cast<lt::external_ip_alert>(a))
					{
						MappingManager::setExternalIP(l_ext_ip->external_address.to_string());
					}
#endif
					if (const auto l_port = lt::alert_cast<lt::portmap_alert>(a))
					{
						LogManager::torrent_message("portmap_alert: " + a->message() + " info:" +
						                            std::string(a->what()) + " index = " + Util::toString(int(l_port->mapping)));
						if (l_port->mapping == port_mapping_t(0)) // TODO (1)
							SettingsManager::g_upnpTorrentLevel = true;
					}
					
					if (const auto l_port = lt::alert_cast<lt::portmap_error_alert>(a))
					{
						LogManager::torrent_message("portmap_error_alert: " + a->message() + " info:" +
						                            std::string(a->what()) + " index = " + Util::toString(int(l_port->mapping)));
						if (l_port->mapping == port_mapping_t(0)) // TODO (1)
							SettingsManager::g_upnpTorrentLevel = false;
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_removed_alert>(a))
					{
						LogManager::torrent_message("torrent_removed_alert: " + a->message());
						auto const l_sha1 = l_delete->info_hash;
						auto i = m_torrents.find(l_delete->handle);
						if (i == m_torrents.end())
						{
							for (i = m_torrents.cbegin(); i != m_torrents.cend(); ++i)
							{
								if (!i->is_valid())
									continue;
								if (i->info_hash() == l_sha1)
									break;
							}
						}
						else
						{
							m_torrents.erase(i);
						}
						--m_torrent_resume_count;
						DatabaseManager::getInstance()->delete_torrent_resume(l_sha1);
						LogManager::torrent_message("DatabaseManager::getInstance()->delete_torrent_resume(l_sha1): " + l_delete->handle.info_hash().to_string());
						fly_fire1(DownloadManagerListener::RemoveTorrent(), l_sha1);
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_delete_failed_alert>(a))
					{
						LogManager::torrent_message("torrent_delete_failed_alert: " + a->message());
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_deleted_alert>(a))
					{
						LogManager::torrent_message("torrent_deleted_alert: " + a->message());
						//fly_fire1(DownloadManagerListener::RemoveTorrent(), l_delete->info_hash);
					}
					if (const auto l_rename = lt::alert_cast<lt::file_renamed_alert>(a))
					{
						LogManager::torrent_message("file_renamed_alert: " + l_rename->message(), false);
						if (!--m_torrent_rename_count)
						{
							if (!DatabaseManager::is_delete_torrent(l_rename->handle.info_hash()))
							{
								l_rename->handle.save_resume_data();
								++m_torrent_resume_count;
							}
							else
							{
								LogManager::torrent_message("file_renamed_alert: skip save_resume_data - is_delete_torrent!");
							}
						}
					}
					if (const auto l_rename = lt::alert_cast<lt::file_rename_failed_alert>(a))
					{
						LogManager::torrent_message("file_rename_failed_alert: " + a->message() +
						                            " error = " + Util::toString(l_rename->error.value()), true);
						if (!--m_torrent_rename_count)
						{
							l_rename->handle.save_resume_data();
							++m_torrent_resume_count;
						}
					}
					if (lt::alert_cast<lt::peer_connect_alert>(a))
					{
						LogManager::torrent_message("peer_connect_alert: " + a->message());
					}
					if (lt::alert_cast<lt::peer_disconnected_alert>(a))
					{
						LogManager::torrent_message("peer_disconnected_alert: " + a->message());
					}
					if (lt::alert_cast<lt::peer_error_alert>(a))
					{
						LogManager::torrent_message("peer_error_alert: " + a->message());
					}
					if (auto l_metadata = lt::alert_cast<metadata_received_alert>(a))
					{
						if (!DatabaseManager::is_resume_torrent(l_metadata->handle.info_hash()))
						{
							select_files(l_metadata->handle);
						}
					}
					if (const auto l_a = lt::alert_cast<torrent_paused_alert>(a))
					{
						LogManager::torrent_message("torrent_paused_alert: " + a->message());
						// TODO - тут разобрать файлы и показать что хочется качать
					}
					if (const auto l_a = lt::alert_cast<lt::file_completed_alert>(a))
					{
						auto l_files = l_a->handle.torrent_file()->files();
						const auto l_hash_file = l_files.hash(l_a->index);
						const auto l_size = l_files.file_size(l_a->index);
						const auto l_file_name = l_files.file_name(l_a->index).to_string();
						//const auto l_file_path = l_files.file_path(l_a->index);
						
						const torrent_status st = l_a->handle.status(torrent_handle::query_save_path);
						
						auto l_full_file_path = st.save_path + l_files.file_path(l_a->index);
						auto const l_is_tmp = l_full_file_path.find_last_of(".!fl");
						if (l_is_tmp != std::string::npos && l_is_tmp > 3)
						{
							l_full_file_path = l_full_file_path.substr(0, l_is_tmp - 3);
							++m_torrent_rename_count;
							l_a->handle.rename_file(l_a->index, l_full_file_path);
						}
						
						// TODO заменить SETTING(DOWNLOAD_DIRECTORY) на путь выбора
						// const torrent_handle h = l_a->handle;
						libtorrent::sha1_hash l_sha1;
						bool p_is_sha1_for_file = false;
						if (!l_hash_file.is_all_zeros())
						{
							l_sha1 = l_hash_file;
							p_is_sha1_for_file = true;
						}
						else
						{
							l_sha1 = l_a->handle.info_hash();
						}
						LogManager::torrent_message("file_completed_alert: " + a->message() + " Path:" + l_full_file_path +
						                            +" sha1:" + aux::to_hex(l_sha1));
						auto item = std::make_shared<FinishedItem>(l_full_file_path, l_size, 0, GET_TIME(), l_sha1, 0, 0);
						DatabaseManager::getInstance()->addTorrentTransfer(e_TransferDownload, item);
						
						if (FinishedManager::isValidInstance())
						{
							FinishedManager::getInstance()->pushHistoryFinishedItem(item, e_TransferDownload);
							FinishedManager::getInstance()->updateStatus();
						}
						
						fly_fire1(DownloadManagerListener::CompleteTorrentFile(), l_full_file_path);
					}
					if (const auto l_a = lt::alert_cast<lt::add_torrent_alert>(a))
					{
						//++m_torrent_resume_count;
						auto l_name = l_a->handle.status(torrent_handle::query_name);
						LogManager::torrent_message("Add torrent: " + l_name.name);
						m_torrents.insert(l_a->handle);
						if (l_name.has_metadata)
						{
							if (!DatabaseManager::is_resume_torrent(l_a->handle.info_hash()))
							{
								select_files(l_a->handle);
							}
							else
							{
#ifdef _DEBUG
								LogManager::torrent_message("DatabaseManager::is_resume_torrent: sha1 = " + lt::aux::to_hex(l_a->handle.info_hash()));
#endif
							}
						}
					}
					if (const auto l_a = lt::alert_cast<lt::torrent_finished_alert>(a))
					{
						LogManager::torrent_message("torrent_finished_alert: " + a->message());
					
						//TODO
						//l_a->handle.set_max_connections(max_connections / 2);
						// TODO ?
						DatabaseManager::getInstance()->delete_torrent_resume(l_a->handle.info_hash());
						m_torrents.erase(l_a->handle);
#ifdef _DEBUG
						LogManager::torrent_message("DatabaseManager::delete_torrent_resume: sha1 = " + lt::aux::to_hex(l_a->handle.info_hash()));
#endif
					}
					
					if (const auto l_a = lt::alert_cast<save_resume_data_failed_alert>(a))
					{
						dcassert(0);
						dcassert(m_torrent_resume_count > 0);
						--m_torrent_resume_count;
						LogManager::torrent_message("save_resume_data_failed_alert: " + l_a->message() + " info:" + std::string(a->what()), true);
					}
					if (const auto l_a = lt::alert_cast<save_resume_data_alert>(a))
					{
						auto const l_resume = lt::write_resume_data_buf(l_a->params);
						const torrent_status st = l_a->handle.status(torrent_handle::query_name);
						dcassert(st.info_hash == l_a->handle.info_hash());
						DatabaseManager::getInstance()->save_torrent_resume(l_a->handle.info_hash(), st.name, l_resume);
						--m_torrent_resume_count;
						LogManager::torrent_message("save_resume_data_alert: " + l_a->message(), false);
						// TODO l_a->handle.set_pinned(false);
					}
					if (lt::alert_cast<lt::torrent_error_alert>(a))
					{
						LogManager::torrent_message("torrent_error_alert: " + a->message() + " info:" + std::string(a->what()));
					}
					if (lt::alert_cast<lt::file_error_alert>(a))
					{
						LogManager::torrent_message("file_error_alert: " + a->message() + " info:" + std::string(a->what()));
					}
					/* TODO
					if (auto st = lt::alert_cast<lt::state_changed_alert>(a))
					                    {
					
					                    }
					                    */
					if (auto st = lt::alert_cast<lt::state_update_alert>(a))
					{
						if (st->status.empty())
						{
							continue;
						}
						int l_pos = 1;
						for (const auto j : st->status)
						{
							lt::torrent_status const& s = j;
#ifdef _DEBUG
							const std::string l_log = "[" + Util::toString(l_alert_pos) + " - "  + Util::toString(l_pos) + "] Status: " + st->message() + " [ " + s.save_path + "\\" + s.name
							                          + " ] Download: " + Util::toString(s.download_payload_rate / 1000) + " kB/s "
							                          + " ] Upload: " + Util::toString(s.upload_payload_rate / 1000) + " kB/s "
							                          + Util::toString(s.total_done / 1000) + " kB ("
							                          + Util::toString(s.progress_ppm / 10000) + "%) downloaded sha1: " + aux::to_hex(s.info_hash);
							static std::string g_last_log;
							if (g_last_log != l_log)
							{
								g_last_log = l_log;
							}
							LogManager::torrent_message(l_log, false);
#endif
							l_pos++;
							DownloadArray l_tickList;
							{
								TransferData l_td;
								l_td.init(s);
								l_tickList.push_back(l_td);
								/*                      for (int i = 0; i < l_count_files; ++i)
								{
								const std::string l_file_name = ti->files().file_name(i).to_string();
								const std::string l_file_path = ti->files().file_path(i);
								TransferData l_td(");
								l_td.init(s);
								l_td.m_size = ti->files().file_size(i); // s.total_wanted; - это полный размер торрента
								l_td.log_debug();
								l_tickList.push_back(l_td);
								}
								*/
							}
							if (!l_tickList.empty())
							{
								fly_fire1(DownloadManagerListener::TorrentEvent(), l_tickList);
							}
						}
					}
				}
				catch (const system_error& e)
				{
					const std::string l_error = "[system_error-1] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
					LogManager::torrent_message("Torrent system_error = " + l_error);
				}
				catch (const std::runtime_error& e)
				{
					const std::string l_error = "[runtime_error-1] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
					LogManager::torrent_message("Torrent runtime_error = " + l_error);
				}
			}
#ifdef _DEBUG
			LogManager::torrent_message("Torrent alerts.size() = " + Util::toString(alerts.size()));
#endif
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			if (!alerts.empty())
			{
				p_torrent_sesion->post_torrent_updates();
				// Спам. p_torrent_sesion->post_session_stats();
				// p_torrent_sesion->post_dht_stats();
			}
			else
			{
			}
		}
		                                       );
	}
	catch (const system_error& e)
	{
		const std::string l_error = "[system_error-2] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
		LogManager::message(l_error);
	}
	catch (const std::runtime_error& e)
	{
		const std::string l_error = "[runtime_error-2] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
		LogManager::message(l_error);
	}
}

std::string DownloadManager::get_torrent_magnet(const libtorrent::sha1_hash& p_sha1)
{
	if (m_torrent_session)
	{
		const torrent_handle h = m_torrent_session->find_torrent(p_sha1);
		if (h.is_valid())
		{
			const auto l_magnet = libtorrent::make_magnet_uri(h);
			return l_magnet;
		}
	}
	return "";
}

std::string DownloadManager::get_torrent_name(const libtorrent::sha1_hash& p_sha1)
{
	if (m_torrent_session)
	{
		const torrent_handle h = m_torrent_session->find_torrent(p_sha1);
		if (h.is_valid())
		{
			const torrent_status st = h.status(torrent_handle::query_name | torrent_handle::query_save_path); // );
			return st.save_path + '\\' + st.name;
		}
	}
	return "";
}

void DownloadManager::fire_added_torrent(const libtorrent::sha1_hash& p_sha1)
{
	if (m_torrent_session)
	{
		fly_fire2(DownloadManagerListener::AddedTorrent(), p_sha1, get_torrent_name(p_sha1)); // TODO opt
	}
}

int DownloadManager::listen_torrent_port()
{
	if (m_torrent_session)
	{
		return m_torrent_session->listen_port();
	}
	return 0;
}

int DownloadManager::ssl_listen_torrent_port()
{
	if (m_torrent_session)
	{
		return m_torrent_session->ssl_listen_port();
	}
	return 0;
}
bool DownloadManager::set_file_priority(const libtorrent::sha1_hash& p_sha1, const CFlyTorrentFileArray& p_files,
                                        const std::vector<libtorrent::download_priority_t>& p_file_priority, const std::string& p_save_path)
{
	if (m_torrent_session)
	{
		lt::error_code ec;
		try
		{
			const auto l_h = m_torrent_session->find_torrent(p_sha1);
			if (l_h.is_valid())
			{
				const torrent_status st = l_h.status(torrent_handle::query_save_path);
				if (p_save_path != st.save_path)
				{
					l_h.move_storage(p_save_path);
				}
				if (!p_file_priority.empty())
				{
					l_h.prioritize_files(p_file_priority);
				}
				dcassert(p_file_priority.size() == p_files.size());
				for (int i = 0; i < p_files.size(); i++)
				{
					if (p_file_priority.size() == 0 || (i < p_file_priority.size() && p_file_priority[i] != dont_download))
					{
						// TODO https://drdump.com/Problem.aspx?ProblemID=334718
						++m_torrent_rename_count;
						l_h.rename_file(file_index_t(i), p_files[i].m_file_path + ".!fl");
					}
				}
				
				l_h.set_flags(lt::torrent_flags::auto_managed);
				l_h.resume();
				// l_h.save_resume_data(); // torrent_handle::save_info_dict | torrent_handle::only_if_modified);
				// Сохраняем на диск когда все переименуется.
				//++m_torrent_resume_count;
			}
		}
		catch (const std::runtime_error& e)
		{
			LogManager::message("Error set_file_priority: " + std::string(e.what()));
		}
	}
	return false;
	
}

bool DownloadManager::pause_torrent_file(const libtorrent::sha1_hash& p_sha1, bool p_is_resume)
{
	if (m_torrent_session)
	{
		lt::error_code ec;
		try
		{
			const auto l_h = m_torrent_session->find_torrent(p_sha1);
			if (l_h.is_valid())
			{
				if (p_is_resume)
				{
					l_h.set_flags(lt::torrent_flags::auto_managed);
					l_h.resume();
				}
				else
				{
					l_h.unset_flags(lt::torrent_flags::auto_managed);
					l_h.pause();
				}
			}
			return true;
		}
		catch (const std::runtime_error& e)
		{
			LogManager::torrent_message("Error pause_torrent_file: " + std::string(e.what()));
		}
	}
	return false;
	
}

bool DownloadManager::remove_torrent_file(const libtorrent::sha1_hash& p_sha1, libtorrent::remove_flags_t p_options)
{
	if (m_torrent_session)
	{
		lt::error_code ec;
		try
		{
			const auto l_h = m_torrent_session->find_torrent(p_sha1);
			//if (l_h.is_valid())
			{
				m_torrent_session->remove_torrent(l_h, p_options);
			}
			//else
			//{
			//  LogManager::torrent_message("Error remove_torrent_file: sha1 = " + lt::aux::to_hex());
			//}
			return true;
		}
		catch (const std::runtime_error& e)
		{
			LogManager::torrent_message("Error remove_torrent_file: " + std::string(e.what()));
		}
	}
	return false;
}

bool DownloadManager::add_torrent_file(const tstring& p_torrent_path, const tstring& p_torrent_url)
{
	if (!m_torrent_session)
	{
		if (MessageBox(NULL, CTSTRING(TORRENT_ENABLE_WARNING), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES)
		{
			SET_SETTING(USE_TORRENT_SEARCH, true);
			SET_SETTING(USE_TORRENT_RSS, true);
			DownloadManager::getInstance()->init_torrent(true);
		}
	}
	if (m_torrent_session)
	{
		lt::error_code ec;
		lt::add_torrent_params p;
		if (!p_torrent_path.empty())
		{
			p.ti = std::make_shared<torrent_info>(Text::fromT(p_torrent_path), ec);
			// for .torrent
			//p.flags |= lt::add_torrent_params::flag_paused;
			//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
			//p.flags &= ~lt::add_torrent_params::flag_duplicate_is_error;
			if (ec)
			{
				dcdebug("%s\n", ec.message().c_str());
				dcassert(0);
				LogManager::torrent_message("Error add_torrent_file:" + ec.message());
				return false;
			}
		}
		else
		{
			p = lt::parse_magnet_uri(Text::fromT(p_torrent_url), ec);
			if (ec)
			{
				dcassert(0);
				LogManager::torrent_message("Error parse_magnet_uri:" + ec.message());
				return false;
			}
			//p.flags &= ~lt::add_torrent_params::flag_paused;
			//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
			//p.flags |= lt::add_torrent_params::flag_upload_mode;
		}
		p.save_path = SETTING(DOWNLOAD_DIRECTORY);
		// TODO - прокинуть фичу в либторрент    auto renamed_files = p.renamed_files; // ".!fl" ?
		p.storage_mode = storage_mode_sparse;
		
		
		m_torrent_session->add_torrent(p, ec);
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			LogManager::torrent_message("Error add_torrent_file:" + ec.message());
			return false;
		}
		return true;
	}
	return false;
}

void DownloadManager::init_torrent(bool p_is_force)
{
	/*
	if (!BOOLSETTING(USE_TORRENT_SEARCH) && p_is_force == false)
	    {
	        LogManager::torrent_message("Disable torrent DHT...");
	        return;
	    }
	*/
	try
	{
		m_torrent_resume_count = 0;
		m_torrent_rename_count = 0;
		lt::settings_pack l_sett;
		l_sett.set_int(lt::settings_pack::alert_mask
		               , lt::alert::error_notification
		               | lt::alert::storage_notification
		               | lt::alert::status_notification
		               | lt::alert::port_mapping_notification
#define FLYLINKDC_USE_OLD_LIBTORRENT_R21298
#ifdef FLYLINKDC_USE_OLD_LIBTORRENT_R21298
		               | lt::alert::progress_notification
#else
		               | lt::alert::file_progress_notification
		               | lt::alert::upload_notification
		               //| lt::alert::piece_progress_notification
		               | lt::alert::block_progress_notification
#endif
#ifdef _DEBUG
		               //  | lt::alert::peer_notification
//		               | lt::alert::session_log_notification
		               | lt::alert::torrent_log_notification
		               // Много спама | lt::alert::peer_log_notification
#endif
		              );
		l_sett.set_str(settings_pack::user_agent, APPNAME "/" VERSION_STR); // LIBTORRENT_VERSION //  A_VERSION_NUM_STR
		l_sett.set_int(settings_pack::choking_algorithm, settings_pack::rate_based_choker);
		
		l_sett.set_int(settings_pack::active_downloads, -1);
		l_sett.set_int(settings_pack::active_seeds, -1);
		l_sett.set_int(settings_pack::active_limit, -1);
		l_sett.set_int(settings_pack::active_tracker_limit, -1);
		l_sett.set_int(settings_pack::active_dht_limit, -1);
		l_sett.set_int(settings_pack::active_lsd_limit, -1);
		
		l_sett.set_bool(settings_pack::enable_upnp, true);
		l_sett.set_bool(settings_pack::enable_natpmp, true);
		l_sett.set_bool(settings_pack::enable_lsd, true);
		l_sett.set_bool(settings_pack::enable_dht, true);
		int l_dht_port = SETTING(DHT_PORT);
		if (l_dht_port < 1024 || l_dht_port >= 65535)
			l_dht_port = 8999;
#ifdef _DEBUG
		l_dht_port++;
#endif
		l_sett.set_str(settings_pack::listen_interfaces, "0.0.0.0:" + Util::toString(l_dht_port));
#if 0 // ???
		std::string l_dht_nodes;
		for (const auto & j : CFlyServerConfig::getTorrentDHTServer())
		{
			if (!l_dht_nodes.empty())
				l_dht_nodes += ",";
			const auto l_boot_dht = j;
			LogManager::torrent_message("Add torrent DHT router: " + l_boot_dht.getServerAndPort());
			l_dht_nodes += l_boot_dht.getIp() + ':' + Util::toString(l_boot_dht.getPort());
		}
		l_sett.set_str(settings_pack::dht_bootstrap_nodes, l_dht_nodes);
#endif

		m_torrent_session = std::make_unique<lt::session>(l_sett);
		m_torrent_session->set_alert_notify(std::bind(&DownloadManager::onTorrentAlertNotify, this, m_torrent_session.get()));
		//lt::dht_settings dht;
		//m_torrent_session->set_dht_settings(dht);
#ifdef _DEBUG
		lt::error_code ec;
		lt::add_torrent_params p = lt::parse_magnet_uri("magnet:?xt=urn:btih:df38ab92ca37c136bcdd7a6ff2aa8a644fc78a00", ec);
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			//return 1;
		}
		p.save_path = SETTING(DOWNLOAD_DIRECTORY); // "."
		p.storage_mode = storage_mode_sparse; //
		// for magnet + load metadata
		//p.flags |= lt::add_torrent_params::flag_paused;
		//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
		//p.flags |= lt::add_torrent_params::flag_upload_mode;
		
		//p.flags |= lt::add_torrent_params::flag_paused;
		p.flags |= lt::torrent_flags::auto_managed;
		p.flags |= lt::torrent_flags::duplicate_is_error;
		// TODO p.flags |= lt::add_torrent_params::flag_update_subscribe;
		
		// TODO p.flags |= lt::add_torrent_params::flag_sequential_download;
		
		// for .torrent
		//p.flags |= lt::add_torrent_params::flag_paused;
		//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
		//p.flags &= ~lt::add_torrent_params::flag_duplicate_is_error;
		
		/*  TODO    if (skipChecking)
		            p.flags |= lt::add_torrent_params::flag_seed_mode;
		        else
		            p.flags &= ~lt::add_torrent_params::flag_seed_mode;
		*/
		/*
		        auto l_h = m_torrent_session->add_torrent(p, ec);
		        if (ec)
		        {
		            dcdebug("%s\n", ec.message().c_str());
		            dcassert(0);
		            return;
		        }
		*/
		/*
		while (!l_h.status().has_metadata)
		        {
		            ::Sleep(100);
		        }
		
		        l_h.unset_flags(lt::torrent_flags::auto_managed);;
		        l_h.pause();
		*/
		
#endif
		DatabaseManager::getInstance()->load_torrent_resume(*m_torrent_session);
	}
	catch (const std::exception& e)
	{
		LogManager::message("DownloadManager::init_torrent error: " + std::string(e.what()));
	}
}
#endif
