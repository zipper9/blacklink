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
#include "Download.h"
#include "UserConnection.h"
#include "QueueItem.h"
#include "DatabaseManager.h"

Download::Download(UserConnection* conn, const QueueItemPtr& item, const string& remoteIp, const string& cipherName) noexcept :
	Transfer(conn, item->getTarget(), item->getTTH(), remoteIp, cipherName),
	qi(item),
	downloadFile(nullptr),
	treeValid(false)
#ifdef FLYLINKDC_USE_DROP_SLOW
	, lastNormalSpeed(0)
#endif
{
	runningAverage = conn->getLastDownloadSpeed();
	setFileSize(qi->getSize());
	////////// p_conn->setDownload(this);
	
	// [-] QueueItem::SourceConstIter source = qi.getSource(getUser()); [-] IRainman fix.
	
	if (qi->isSet(QueueItem::FLAG_PARTIAL_LIST))
	{
		setType(TYPE_PARTIAL_LIST);
		//pfs = qi->getTarget(); // [+] IRainman fix. TODO: пофикшено, но не до конца :(
	}
	else if (qi->isSet(QueueItem::FLAG_USER_LIST))
	{
		setType(TYPE_FULL_LIST);
	}

#ifdef IRAINMAN_INCLUDE_USER_CHECK
	if (qi->isSet(QueueItem::FLAG_USER_CHECK))
		setFlag(FLAG_USER_CHECK);
#endif
	if (qi->isSet(QueueItem::FLAG_USER_GET_IP))
		setFlag(FLAG_USER_GET_IP);
		
	const bool isFile = getType() == TYPE_FILE && qi->getSize() != -1;
	
	if (!getTTH().isZero() && isFile)
	{
		treeValid = DatabaseManager::getInstance()->getTree(getTTH(), tigerTree);
		if (treeValid)
			qi->updateBlockSize(tigerTree.getBlockSize());
	}
	{
		RLock(*QueueItem::g_cs);
		auto it = qi->findSourceL(getUser());
		if (it != qi->getSourcesL().end())
		{
			const auto& src = it->second;
			if (src.isSet(QueueItem::Source::FLAG_PARTIAL))
			{
				setFlag(FLAG_DOWNLOAD_PARTIAL);
			}
			
			if (isFile)
			{
				if (treeValid)
				{
					setSegment(qi->getNextSegmentL(tigerTree.getBlockSize(), conn->getChunkSize(), conn->getSpeed(), src.getPartialSource()));
				}
				else if (conn->isSet(UserConnection::FLAG_SUPPORTS_TTHL) && !src.isSet(QueueItem::Source::FLAG_NO_TREE) && qi->getSize() > MIN_BLOCK_SIZE)
				{
					// Get the tree unless the file is small (for small files, we'd probably only get the root anyway)
					setType(TYPE_TREE);
					tigerTree.setFileSize(qi->getSize());
					setSegment(Segment(0, -1));
				}
				else
				{
					// Use the root as tree to get some sort of validation at least...
					// TODO: use aligned block size ??
					tigerTree = TigerTree(qi->getSize(), qi->getSize(), getTTH());
					setTreeValid(true);
					setSegment(qi->getNextSegmentL(tigerTree.getBlockSize(), 0, 0, src.getPartialSource()));
				}
				
				if ((getStartPos() + getSize()) != qi->getSize())
				{
					setFlag(FLAG_CHUNKED);
				}
				
				if (getSegment().getOverlapped())
				{
					setFlag(FLAG_OVERLAP);
					qi->setOverlapped(getSegment(), true);
				}
			}
		}
		else
		{
			dcassert(0);
		}
	}
}

Download::~Download()
{
	dcassert(downloadFile == nullptr);
}

int64_t Download::getDownloadedBytes() const
{
	return qi->getDownloadedBytes();
}

void Download::getCommand(AdcCommand& cmd, bool zlib) const
{
	cmd.addParam(Transfer::fileTypeNames[getType()]);
	
	if (getType() == TYPE_PARTIAL_LIST)
	{
		cmd.addParam(Util::toAdcFile(getTempTarget()));
	}
	else if (getType() == TYPE_FULL_LIST)
	{
		cmd.addParam(isSet(Download::FLAG_XML_BZ_LIST) ? fileNameFilesBzXml : fileNameFilesXml);
	}
	else
	{
#ifdef DEBUG_TRANSFERS
		if (!downloadPath.empty())
			cmd.addParam(downloadPath);
		else
#endif
			cmd.addParam("TTH/" + getTTH().toBase32());
	}
	//dcassert(getStartPos() >= 0);
	cmd.addParam(Util::toString(getStartPos()));
	cmd.addParam(Util::toString(getSize()));
	
	if (zlib && BOOLSETTING(COMPRESS_TRANSFERS))
		cmd.addParam("ZL1");
	if (BOOLSETTING(SEND_DB_PARAM))
		cmd.addParam("DB", Util::toString(getDownloadedBytes())); 
}

void Download::getParams(StringMap& params) const
{
	Transfer::getParams(getUserConnection(), params);
	params["target"] = getPath();
}

string Download::getTempTarget() const
{
	return qi->getTempTarget();
}

void Download::updateSpeed(uint64_t currentTick)
{
	LOCK(csSpeed);
	setLastTick(currentTick);
	speed.addSample(actual, currentTick);
	int64_t avg = speed.getAverage(2000);
	if (avg >= 0)
	{
		runningAverage = avg;
		userConnection->setLastDownloadSpeed(avg);
	}
	else
		runningAverage = userConnection->getLastDownloadSpeed();
}

int64_t Download::getSecondsLeft(bool wholeFile) const
{
	int64_t avg = getRunningAverage();
	int64_t bytesLeft = (wholeFile ? getFileSize() : getSize()) - getPos();
	if (bytesLeft > 0 && avg > 0)
		return bytesLeft / avg;
	return 0;
}
