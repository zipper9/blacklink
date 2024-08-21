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
#include "LogManager.h"
#include "SettingsManager.h"
#include "AppPaths.h"
#include "PathUtil.h"
#include "Util.h" // toAdcFile
#include "ParamExpander.h"
#include "FormatUtil.h"
#include "Random.h"
#include "ConfCore.h"

#ifdef DEBUG_SHUTDOWN
std::atomic<int> Download::countCreated(0), Download::countDeleted(0);
#endif

Download::Download(const UserConnectionPtr& conn, const QueueItemPtr& item) noexcept :
	Transfer(conn, getTargetPath(item), item->getTTH()),
	qi(item),
	downloadFile(nullptr),
	reasonCode(REASON_CODE_UNSPECIFIED)
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
	, lastNormalSpeed(0)
#endif
{
#ifdef DEBUG_SHUTDOWN
	++countCreated;
#endif
	runningAverage = conn->getLastDownloadSpeed();
	setFileSize(qi->getSize());

	auto queueItemFlags = qi->getFlags();
	if (queueItemFlags & QueueItem::FLAG_PARTIAL_LIST)
	{
		setType(TYPE_PARTIAL_LIST);
		if (queueItemFlags & QueueItem::FLAG_RECURSIVE_LIST)
			setFlag(FLAG_RECURSIVE_LIST);
		if (qi->getExtraFlags() & QueueItem::XFLAG_MATCH_QUEUE)
			setFlag(FLAG_MATCH_QUEUE);
	}
	else if (queueItemFlags & QueueItem::FLAG_USER_LIST)
	{
		setType(TYPE_FULL_LIST);
	}
	if (queueItemFlags & QueueItem::FLAG_USER_CHECK)
		setFlag(FLAG_USER_CHECK);
	if (queueItemFlags & QueueItem::FLAG_USER_GET_IP)
		setFlag(FLAG_USER_GET_IP);
}

Download::~Download()
{
#ifdef DEBUG_SHUTDOWN
	++countDeleted;
#endif
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
		cmd.addParam(isSet(FLAG_XML_BZ_LIST) ? fileNameFilesBzXml : fileNameFilesXml);
	}
	else
	{
		if (getType() == TYPE_TREE) zlib = false;
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

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	if (zlib)
		zlib = ss->getBool(Conf::COMPRESS_TRANSFERS);
	bool sendDBParam = ss->getBool(Conf::SEND_DB_PARAM);
	bool useTTHList = isSet(FLAG_MATCH_QUEUE) ? ss->getBool(Conf::USE_TTH_LIST) : false;
	ss->unlockRead();

	if (zlib)
		cmd.addParam("ZL1");
	if (isSet(FLAG_RECURSIVE_LIST))
		cmd.addParam("RE1");
	if (sendDBParam)
	{
		int64_t bytes = getDownloadedBytes();
		if (bytes > 0)
			cmd.addParam(TAG('D', 'B'), Util::toString(bytes));
	}
	if (useTTHList)
		cmd.addParam("TL1");
}

void Download::getParams(StringMap& params) const
{
	Transfer::getParams(params);
	params["target"] = getPath();
}

string Download::getTempTarget() const
{
	string s;
	qi->lockAttributes();
	s = qi->getTempTargetL();
	qi->unlockAttributes();
	return s;
}

void Download::updateSpeed(uint64_t currentTick)
{
	LOCK(csSpeed);
	setLastTick(currentTick);
	speed.addSample(actual, currentTick);
	int64_t avg = speed.getAverage(2000, 64 * 1024);
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

string Download::getTargetFileName() const
{
	return Util::getFileName(getPath());
}

string Download::getDownloadTarget() const
{
	if (qi->getFlags() & QueueItem::FLAG_USER_LIST)
		return getPath();

	qi->lockAttributes();
	string tempTarget = qi->getTempTargetL();
	qi->unlockAttributes();
	if (!tempTarget.empty())
		return tempTarget;

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const string tempDirectory = ss->getString(Conf::TEMP_DOWNLOAD_DIRECTORY);
	ss->unlockRead();

	const TTHValue& tth = qi->getTTH();
	const string& target = qi->getTarget();
	const string targetFileName = Util::getFileName(target);
	const string tempName = QueueItem::getDCTempName(targetFileName, tempDirectory.empty() ? nullptr : &tth);
	if (!tempDirectory.empty() && File::getSize(target) == -1)
	{
		StringMap sm;
#ifdef _WIN32
		if (target.length() >= 3 && target[1] == ':' && target[2] == '\\')
			sm["targetdrive"] = target.substr(0, 3);
		else
			sm["targetdrive"] = Util::getLocalPath().substr(0, 3);
#endif
		tempTarget = Util::formatParams(tempDirectory, sm, false) + tempName;
		if (QueueItem::checkTempDir && !tempTarget.empty())
		{
			QueueItem::checkTempDir = false;
			File::ensureDirectory(tempDirectory);
			const tstring tempFile = Text::toT(tempTarget) + _T(".test-writable-") + Util::toStringT(Util::rand()) + _T(".tmp");
			try
			{
				{
					File f(tempFile, File::WRITE, File::CREATE | File::TRUNCATE);
				}
				File::deleteFile(tempFile);
			}
			catch (const Exception&)
			{
				LogManager::message(STRING_F(TEMP_DIR_NOT_WRITABLE, tempDirectory));
				ss->lockWrite();
				ss->setString(Conf::TEMP_DOWNLOAD_DIRECTORY, Util::emptyString);
				ss->unlockWrite();
			}
		}
	}

	if (tempDirectory.empty())
		tempTarget = target.substr(0, target.length() - targetFileName.length()) + tempName;

	if (!tempTarget.empty())
	{
		qi->lockAttributes();
		qi->setTempTargetL(tempTarget);
		qi->unlockAttributes();
		return tempTarget;
	}

	return getPath();
}

string Download::getTargetPath(const QueueItemPtr& qi)
{
	string path = qi->getTarget();
	if ((qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST)) == QueueItem::FLAG_USER_LIST)
	{
		static const size_t TTH_SUFFIX_LEN = 39 + 1;
		if (path.length() > TTH_SUFFIX_LEN && path[path.length() - TTH_SUFFIX_LEN] == '.')
		{
			const string datetime = Util::formatDateTime(".%Y%m%d_%H%M", qi->getAdded());
			path.insert(path.length() - TTH_SUFFIX_LEN, datetime);
		}
	}
	return path;
}
