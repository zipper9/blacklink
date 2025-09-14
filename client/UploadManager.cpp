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

#include "UploadManager.h"
#include "SettingsManager.h"
#include "DownloadManager.h"
#include "ConnectionManager.h"
#include "ShareManager.h"
#include "ClientManager.h"
#include "DatabaseManager.h"
#include "PathUtil.h"
#include "FormatUtil.h"
#include "BZUtils.h"
#include "ZUtils.h"
#include "Upload.h"
#include "QueueManager.h"
#include "UserManager.h"
#include "FavoriteManager.h"
#include "FinishedManager.h"
#include "SharedFileStream.h"
#include "IpGrant.h"
#include "Wildcards.h"
#include "FilteredFile.h"
#include "GlobalState.h"
#include "ConfCore.h"

static const unsigned WAIT_TIME_LAST_CHUNK     = 3000;
static const unsigned WAIT_TIME_OTHER_CHUNK    = 8000;
static const unsigned WAITING_USER_EXPIRE_TIME = 5 * 60000;

#ifdef _DEBUG
std::atomic_int UploadQueueFile::g_upload_queue_item_count(0);
bool disablePartialListUploads = false;
#endif

uint32_t UploadManager::g_count_WaitingUsersFrame = 0;
int UploadManager::g_running = 0;
int64_t UploadManager::g_runningAverage;

void WaitingUser::addWaitingFile(const UploadQueueFilePtr& uqi)
{
	if (waitingFiles.size() >= MAX_WAITING_FILES) waitingFiles.erase(waitingFiles.begin());
	waitingFiles.push_back(uqi);
}

UploadQueueFilePtr WaitingUser::findWaitingFile(const string& file) const
{
	for (const UploadQueueFilePtr& item : waitingFiles)
		if (item->getFile() == file) return item;
	return UploadQueueFilePtr();
}

UploadManager::UploadManager() noexcept :
	extra(0), lastGrant(0), lastFreeSlots(-1),
	fireballStartTick(0), fileServerCheckTick(0), isFireball(false), isFileServer(false), extraPartial(0),
	slotQueueId(0)
{
	csFinishedUploads = std::unique_ptr<RWLock>(RWLock::create());
	csReservedSlots = std::unique_ptr<RWLock>(RWLock::create());
	ClientManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);
	updateSettings();
}

UploadManager::~UploadManager()
{
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	{
		LOCK(csQueue);
		slotQueue.clear();
	}
	while (true)
	{
		{
			READ_LOCK(*csFinishedUploads);
			if (uploads.empty())
				break;
		}
		Thread::sleep(10);
	}
}

void UploadManager::initTransferData(TransferData& td, const Upload* u)
{
	td.token = u->getConnectionQueueToken();
	td.hintedUser = u->getHintedUser();
	td.pos = u->getAdjustedPos();
	td.secondsLeft = u->getSecondsLeft();
	td.speed = u->getRunningAverage();
	td.startTime = u->getStartTime();
	td.size = u->getType() == Transfer::TYPE_TREE ? u->getSize() : u->getFileSize();
	td.fileSize = u->getFileSize();
	td.type = u->getType();
	td.path = u->getPath();
	if (u->isSet(Upload::FLAG_UPLOAD_PARTIAL))
		td.transferFlags |= TRANSFER_FLAG_PARTIAL;
	if (u->isSecure)
	{
		td.transferFlags |= TRANSFER_FLAG_SECURE;
		if (u->isTrusted) td.transferFlags |= TRANSFER_FLAG_TRUSTED;
	}
	if (u->isSet(Upload::FLAG_ZUPLOAD))
		td.transferFlags |= TRANSFER_FLAG_COMPRESSED;
	if (u->isSet(Upload::FLAG_CHUNKED))
		td.transferFlags |= TRANSFER_FLAG_CHUNKED;
}

bool UploadManager::hasUpload(const UserConnection* newLeecher) const
{
	dcassert(!GlobalState::isShuttingDown());
	if (newLeecher->getSocket())
	{
		const auto newLeecherIp = newLeecher->getSocket()->getIp();
		const auto newLeecherShare = newLeecher->getUser()->getBytesShared();
		const auto newLeecherNick = newLeecher->getUser()->getLastNick();
		
		READ_LOCK(*csFinishedUploads);
		
		for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
		{
			const auto u = *i;
			dcassert(u);
			dcassert(u->getUser());
			const auto uploadUserIp = u->getUserConnection()->getSocket()->getIp();
			const auto uploadUserShare = u->getUser()->getBytesShared();
			const auto uploadUserNick = u->getUser()->getLastNick();
			
			if (u->getUserConnection()->getSocket() &&
			        newLeecherIp == uploadUserIp &&
			        (newLeecherShare == uploadUserShare ||
			         newLeecherNick == uploadUserNick) // [+] back port from r4xx.
			   )
			{
				return true;
			}
		}
	}
	return false;
}

bool UploadManager::prepareFile(UserConnection* source, const string& typeStr, const string& fileName, bool hideShare, const CID& shareGroup, int64_t startPos, int64_t& bytes, bool listRecursive, int& compressionType, string& errorText)
{
	dcassert(!GlobalState::isShuttingDown());
	dcdebug("Preparing %s %s " I64_FMT " " I64_FMT " %d\n", typeStr.c_str(), fileName.c_str(), startPos, bytes, listRecursive);
	if (GlobalState::isShuttingDown())
	{
		return false;
	}
	if (fileName.empty() || startPos < 0 || bytes < -1 || bytes == 0)
	{
		source->fileNotAvail("Invalid request");
		return false;
	}
	UserConnectionPtr ucPtr = ConnectionManager::getInstance()->findConnection(source);
	if (!ucPtr) return false;
	const bool isTypeTree = typeStr == Transfer::fileTypeNames[Transfer::TYPE_TREE];
	const bool isTypePartialList = typeStr == Transfer::fileTypeNames[Transfer::TYPE_PARTIAL_LIST];
	int useCompType = compressionType;
	compressionType = COMPRESSION_DISABLED;
	InputStream* is = nullptr;
	int64_t start = 0;
	int64_t size = 0;
	int64_t fileSize = 0;

	const bool isFileList = fileName == Transfer::fileNameFilesBzXml || fileName == Transfer::fileNameFilesXml;
	bool isMinislot = isFileList;
	bool isPartial = false;

	string sourceFile;
	const bool isTypeFile = typeStr == Transfer::fileTypeNames[Transfer::TYPE_FILE];
	const bool isTTH  = (isTypeFile || isTypeTree) && fileName.length() == 43 && fileName.compare(0, 4, "TTH/", 4) == 0;
	Transfer::Type type = Transfer::TYPE_FILE;
	TTHValue tth;
	if (isTTH)
		tth = TTHValue(fileName.c_str() + 4);

	// read settings
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const bool optExtraSlotToDl = ss->getBool(Conf::EXTRA_SLOT_TO_DL);
	const int optMinislotSize = ss->getInt(Conf::MINISLOT_SIZE);
	const int optExtraPartialSlots = ss->getInt(Conf::EXTRA_PARTIAL_SLOTS);
	ss->unlockRead();

	try
	{
		if (isTypeFile)
		{
			int64_t uncompressedXmlSize = 0;
			string shareManagerError;
			if (ShareManager::isValidInstance())
			{
				if (isTTH)
				{
					if (!hideShare)
					{
						ShareManager::getInstance()->getFilePath(tth, sourceFile, shareGroup);
						// Check partial file
						if (sourceFile.empty() && QueueManager::isChunkDownloaded(tth, startPos, bytes, sourceFile))
						{
							dcassert(!sourceFile.empty());
							if (!sourceFile.empty())
							{
								try
								{
									auto f = new SharedFileStream(sourceFile, File::READ, File::OPEN | File::SHARED | File::NO_CACHE_HINT, 0);

									start = startPos;
									fileSize = f->getFastFileSize();
									size = bytes == -1 ? fileSize - start : bytes;

									if (size < 0 || size > fileSize || start + size > fileSize)
									{
										source->fileNotAvail();
										delete f;
										return false;
									}

									f->setPos(start);
									if (start + size < fileSize)
										is = new LimitedInputStream<true>(f, size);
									else
										is = f;
									if (useCompType == COMPRESSION_CHECK_FILE_TYPE && !isCompressedFile(sourceFile))
										useCompType = COMPRESSION_ENABLED;
									if (useCompType == COMPRESSION_ENABLED)
									{
										is = new FilteredInputStream<ZFilter, true>(is);
										compressionType = COMPRESSION_ENABLED;
									}
									isPartial = true;
								}
								catch (const Exception&)
								{
									delete is;
									is = nullptr;
									sourceFile.clear();
								}
							}
						}
					}
				}
				else
					sourceFile = ShareManager::getInstance()->getFileByPath(fileName, hideShare, shareGroup, uncompressedXmlSize, &shareManagerError);
			}
			if (sourceFile.empty())
			{
				source->fileNotAvail(shareManagerError.empty() ? UserConnection::FILE_NOT_AVAILABLE : shareManagerError);
				return false;
			}
			if (!is)
			{
				if (fileName == Transfer::fileNameFilesXml)
				{
					File* f = new File(sourceFile, File::READ, File::OPEN);
					is = new FilteredInputStream<UnBZFilter, true>(f);
					start = 0;
					fileSize = size = uncompressedXmlSize;
				}
				else
				{
					File* f = new File(sourceFile, File::READ, File::OPEN);

					start = startPos;
					fileSize = f->getSize();
					size = bytes == -1 ? fileSize - start : bytes;

					if (size < 0 || size > fileSize || start + size > fileSize)
					{
						source->fileNotAvail();
						delete f;
						return false;
					}

					if (fileSize <= (int64_t) optMinislotSize << 10)
						isMinislot = true;

					f->setPos(start);
					is = f;
					if (start + size < fileSize)
						is = new LimitedInputStream<true>(is, size);
					if (useCompType == COMPRESSION_CHECK_FILE_TYPE && !isCompressedFile(sourceFile))
						useCompType = COMPRESSION_ENABLED;
					if (useCompType == COMPRESSION_ENABLED)
					{
						is = new FilteredInputStream<ZFilter, true>(is);
						compressionType = COMPRESSION_ENABLED;
					}
				}
				type = isFileList ? Transfer::TYPE_FULL_LIST : Transfer::TYPE_FILE;
			}
		}
		else if (isTypeTree)
		{
			if (hideShare)
			{
				source->fileNotAvail();
				return false;
			}
			MemoryInputStream* mis = nullptr;
			if (ShareManager::isValidInstance())
			{
				if (isTTH)
				{
					mis = ShareManager::getInstance()->getTreeByTTH(tth);
					if (!mis)
					{
						if (QueueManager::fileQueue.isQueued(tth))
							mis = ShareManager::getTreeFromStore(tth);
					}
				}
				else
					mis = ShareManager::getInstance()->getTree(fileName, shareGroup);
			}
			if (!mis)
			{
				source->fileNotAvail();
				return false;
			}

			sourceFile = fileName;
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			isMinislot = true;
			type = Transfer::TYPE_TREE;
		}
		else if (isTypePartialList)
		{
#ifdef _DEBUG
			if (disablePartialListUploads)
			{
				source->fileNotAvail("Unknown file type");
				return false;
			}
#endif
			// Partial file list
			MemoryInputStream* mis = nullptr;
			if (ShareManager::isValidInstance())
			{
				try { mis = ShareManager::getInstance()->generatePartialList(fileName, listRecursive, hideShare, shareGroup); }
				catch (Exception&) {}
			}
			if (!mis)
			{
				source->fileNotAvail();
				return false;
			}

			sourceFile = fileName;
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			isMinislot = true;
			type = Transfer::TYPE_PARTIAL_LIST;
		}
		else
		{
			source->fileNotAvail("Unknown file type");
			return false;
		}
	}
	catch (const Exception& e)
	{
		LogManager::message(STRING(UNABLE_TO_SEND_FILE) + ' ' + sourceFile + ": " + e.getError());
		source->fileNotAvail();
		return false;
	}

	auto slotType = source->getSlotType();
	uint64_t slotTimeout = 0;
	{
		READ_LOCK(*csReservedSlots);
		auto i = reservedSlots.find(source->getUser());
		if (i != reservedSlots.end()) slotTimeout = i->second;
	}
	uint64_t tick = GET_TICK();
	bool hasReserved = slotTimeout > tick;
	if (!hasReserved)
	{
		if (slotTimeout) source->getUser()->unsetFlag(User::RESERVED_SLOT);
		hasReserved = optExtraSlotToDl && DownloadManager::getInstance()->checkFileDownload(source->getUser());
	}
	if (slotType == UserConnection::RESSLOT && !hasReserved)
		slotType = UserConnection::NOSLOT;
	const bool isFavGrant = FavoriteManager::getInstance()->hasAutoGrantSlot(source->getUser());
	if ((slotType != UserConnection::STDSLOT && slotType != UserConnection::RESSLOT) || hasReserved)
	{
		bool hasFreeSlot = getFreeSlots() > 0;
		if (hasFreeSlot)
		{
			LOCK(csQueue);
			hasFreeSlot = (slotQueue.empty() && notifiedUsers.empty()) || notifiedUsers.find(source->getUser()) != notifiedUsers.end();
		}

		bool isAutoSlot = getAutoSlot(tick);
		bool isLeecher = (hasReserved || isFavGrant) ? false : hasUpload(source);

#ifdef SSA_IPGRANT_FEATURE
		bool hasSlotByIP = false;
		if (optExtraSlotToDl)
		{
			if (!(hasReserved || isFavGrant || isAutoSlot || hasFreeSlot || isLeecher))
			{
				IpAddress addr = source->getRemoteIp();
				if (addr.type == AF_INET && addr.data.v4)
					hasSlotByIP = ipGrant.check(addr.data.v4);
			}
		}
#endif // SSA_IPGRANT_FEATURE
		if (!(hasReserved || isFavGrant || isAutoSlot || hasFreeSlot
#ifdef SSA_IPGRANT_FEATURE
		        || hasSlotByIP
#endif
		     ) || isLeecher)
		{
			if (isMinislot && source->isSet(UserConnection::FLAG_SUPPORTS_MINISLOTS) &&
				(slotType == UserConnection::MINISLOT || getFreeExtraSlots() > 0))
			{
				slotType = UserConnection::MINISLOT;
			}
			else if (isPartial && (slotType == UserConnection::PFS_SLOT || extraPartial < optExtraPartialSlots))
			{
				slotType = UserConnection::PFS_SLOT;
			}
			else
			{
				delete is;
				errorText = STRING(ALL_UPLOAD_SLOTS_TAKEN);
				source->setUpload(nullptr);
				source->maxedOut(addToQueue(source, sourceFile, startPos, fileSize, isTypePartialList ? UploadQueueFile::FLAG_PARTIAL_FILE_LIST : 0));
				return false;
			}
		}
		else
		{
#ifdef SSA_IPGRANT_FEATURE
			if (hasSlotByIP)
				LogManager::message("IpGrant: " + STRING(GRANTED_SLOT_BY_IP) + ' ' + Util::printIpAddress(source->getRemoteIp()));
#endif
			slotType = hasReserved ? UserConnection::RESSLOT : UserConnection::STDSLOT;
		}
		setLastGrant(tick);
	}
	
	bool notifyRemove;
	{
		LOCK(csQueue);
		// remove file from upload queue
		notifyRemove = clearUserFilesL(source->getUser());

		// remove user from notified list
		auto it = notifiedUsers.find(source->getUser());
		if (it != notifiedUsers.end())
			notifiedUsers.erase(it);
	}
	if (notifyRemove)
		fireUserRemoved(source->getUser());

	bool resumed = false;
	{
		WRITE_LOCK(*csFinishedUploads);
		for (auto i = finishedUploads.cbegin(); i != finishedUploads.cend(); ++i)
		{
			auto up = *i;
			if (source == up->getUserConnection())
			{
				finishedUploads.erase(i);
				if (sourceFile != up->getPath())
				{
					logUpload(up);
				}
				else
				{
					resumed = true;
				}
				break;
			}
		}
	}
	UploadPtr u = std::make_shared<Upload>(ucPtr, tth, sourceFile, is);
	u->setSegment(Segment(start, size));
	source->setUpload(u);

	if (u->getSize() != fileSize)
		u->setFlag(Upload::FLAG_CHUNKED);

	if (resumed)
		u->setFlag(Upload::FLAG_RESUMED);

	if (isPartial)
		u->setFlag(Upload::FLAG_UPLOAD_PARTIAL);

	if (compressionType == COMPRESSION_ENABLED)
		u->setFlag(Upload::FLAG_ZUPLOAD);

	u->setFileSize(fileSize);
	u->setType(type);
	
	{
		WRITE_LOCK(*csFinishedUploads);
		uploads.push_back(u);
		u->getUser()->modifyUploadCount(1);
	}
	
	if (source->getSlotType() != slotType)
	{
		// remove old count
		processSlot(source->getSlotType(), -1);
		// set new slot count
		processSlot(slotType, 1);
		// user got a slot
		source->setSlotType(slotType);
	}
	
	return true;
}

bool UploadManager::isCompressedFile(const string& target) noexcept
{
	const string name = Util::getFileName(target);
	csCompressedFiles.lock();
	bool result = std::regex_match(name, reCompressedFiles);
	csCompressedFiles.unlock();
	return result;
}

bool UploadManager::getAutoSlot(uint64_t tick) const
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	int minUploadSpeed = ss->getInt(Conf::AUTO_SLOT_MIN_UL_SPEED);
	int autoSlots = ss->getInt(Conf::AUTO_SLOTS);
	ss->unlockRead();
	/** A 0 in settings means disable */
	if (minUploadSpeed == 0)
		return false;
	/** Max slots */
	if (getSlots() + autoSlots < g_running)
		return false;
	/** Only grant one slot per 30 sec */
	if (tick < getLastGrant() + 30 * 1000)
		return false;
	/** Grant if upload speed is less than the threshold speed */
	return getRunningAverage() < minUploadSpeed * 1024;
}

void UploadManager::shutdown()
{
	WRITE_LOCK(*csReservedSlots);
	reservedSlots.clear();
}

void UploadManager::removeUpload(UploadPtr& upload, bool delay)
{
	UploadArray tickList;
	//dcassert(!GlobalState::isShuttingDown());
	{
		WRITE_LOCK(*csFinishedUploads);
		if (!uploads.empty())
		{
			auto i = find(uploads.begin(), uploads.end(), upload);
			if (i != uploads.end())
			{
				uploads.erase(i);
				upload->getUser()->modifyUploadCount(-1);
			}
		}
		if (delay)
		{
			finishedUploads.push_back(upload);
			uint64_t tickForRemove = GET_TICK();
			if (upload->getSegment().getEnd() >= upload->getFileSize())
			{
				tickForRemove += WAIT_TIME_LAST_CHUNK;
				tickList.resize(1);
				initTransferData(tickList[0], upload.get());
			}
			else
			{
				tickForRemove += WAIT_TIME_OTHER_CHUNK;
			}
			upload->setTickForRemove(tickForRemove);
		}
		else
		{
			upload.reset();
		}
	}
	if (!tickList.empty())
		fire(UploadManagerListener::Tick(), tickList);
}

void UploadManager::reserveSlot(const HintedUser& hintedUser, uint64_t seconds)
{
	dcassert(!GlobalState::isShuttingDown());
	{
		WRITE_LOCK(*csReservedSlots);
		reservedSlots[hintedUser.user] = GET_TICK() + seconds * 1000;
		hintedUser.user->setFlag(User::RESERVED_SLOT);
	}
	save();
	bool notifyUser = false;
	string token;
	if (hintedUser.user->isOnline())
	{
		LOCK(csQueue);
		// find user in uploadqueue to connect with correct token
		auto it = std::find_if(slotQueue.cbegin(), slotQueue.cend(), [&](const UserPtr& u)
		{
			return u == hintedUser.user;
		});
		if (it != slotQueue.cend())
		{
			token = it->getToken();
			notifyUser = true;
		}
	}

	UserManager::getInstance()->fireReservedSlotChanged(hintedUser.user);
	if (ClientManager::getChatOptions() & ClientManager::CHAT_OPTION_SEND_GRANT_MSG)
		ClientManager::privateMessage(hintedUser,
			STRING(SLOT_GRANTED_MSG) + ' ' + Util::formatSeconds(seconds),
			ClientBase::PM_FLAG_AUTOMATIC | ClientBase::PM_FLAG_THIRD_PERSON);
	if (notifyUser)
		ClientManager::getInstance()->connect(hintedUser, token, false);
}

void UploadManager::unreserveSlot(const HintedUser& hintedUser)
{
	dcassert(!GlobalState::isShuttingDown());
	{
		WRITE_LOCK(*csReservedSlots);
		if (!reservedSlots.erase(hintedUser.user)) return;
		hintedUser.user->unsetFlag(User::RESERVED_SLOT);
	}
	save();
	UserManager::getInstance()->fireReservedSlotChanged(hintedUser.user);
	if (ClientManager::getChatOptions() & ClientManager::CHAT_OPTION_SEND_GRANT_MSG)
		ClientManager::privateMessage(hintedUser,
			STRING(SLOT_REMOVED_MSG),
			ClientBase::PM_FLAG_AUTOMATIC | ClientBase::PM_FLAG_THIRD_PERSON);
}

static void getShareGroup(const UserConnection* source, bool& hideShare, CID& shareGroup)
{
	hideShare = false;
	if (source->getUser())
	{
		auto fm = FavoriteManager::getInstance();
		FavoriteUser::MaskType flags;
		int uploadLimit;
		if (fm->getFavUserParam(source->getUser(), flags, uploadLimit, shareGroup))
		{
			if (flags & FavoriteUser::FLAG_HIDE_SHARE)
			{
				hideShare = true;
				return;
			}
			if (!shareGroup.isZero()) return;
		}
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(source->getHintedUser().hint);
		if (fhe)
		{
			hideShare = fhe->getHideShare();
			shareGroup = fhe->getShareGroup();
		}
		fm->releaseFavoriteHubEntryPtr(fhe);
	}
}

void UploadManager::processGet(UserConnection* source, const string& fileName, int64_t resume) noexcept
{
	if (GlobalState::isShuttingDown())
		return;

	bool hideShare;
	CID shareGroup;
	getShareGroup(source, hideShare, shareGroup);

	int64_t bytes = -1;
	string errorText;
	int compressionType = COMPRESSION_DISABLED;
	if (prepareFile(source, Transfer::fileTypeNames[Transfer::TYPE_FILE], Util::toAdcFile(fileName), hideShare, shareGroup, resume, bytes, false, compressionType, errorText))
	{
		source->setState(UserConnection::STATE_SEND);
		source->fileLength(Util::toString(source->getUpload()->getSize()));
	}
}

void UploadManager::processSend(UserConnection* source) noexcept
{
	if (GlobalState::isShuttingDown())
		return;
	auto u = source->getUpload();
	dcassert(u != nullptr);
	u->setStartTime(source->getLastActivity());
	
	source->setState(UserConnection::STATE_RUNNING);
	source->transmitFile(u->getReadStream());
	fire(UploadManagerListener::Starting(), u);
}

void UploadManager::processGetBlock(UserConnection* source, const string& cmd, const string& param) noexcept
{
	if (GlobalState::isShuttingDown())
		return;

	string fname;
	int64_t startPos = 0;
	int64_t bytes = 0;
	auto pos = param.find(' ');
	if (pos != string::npos)
	{
		startPos = Util::toInt64(param);
		auto nextPos = param.find(' ', ++pos);
		if (nextPos != string::npos)
		{
			bytes = Util::toInt64(param.c_str() + pos);
			fname = param.substr(nextPos + 1);
		}
	}

	bool hideShare;
	CID shareGroup;
	getShareGroup(source, hideShare, shareGroup);

	int compressionType;
	if (cmd[0] != 'U')
	{
		int encoding = source->getEncoding();
		fname = Text::toUtf8(fname, encoding);
		compressionType = cmd[3] == 'Z' ? COMPRESSION_ENABLED : COMPRESSION_DISABLED;
	}
	else
		compressionType = cmd[4] == 'Z' ? COMPRESSION_ENABLED : COMPRESSION_DISABLED;
		
	string errorText;
	if (prepareFile(source, Transfer::fileTypeNames[Transfer::TYPE_FILE], Util::toAdcFile(fname), hideShare, shareGroup, startPos, bytes, false, compressionType, errorText))
	{
		auto u = source->getUpload();
		dcassert(u != nullptr);
		source->sending(Util::toString(u->getSize()));

		u->setStartTime(source->getLastActivity());
		source->setState(UserConnection::STATE_RUNNING);
		source->transmitFile(u->getReadStream());
		fire(UploadManagerListener::Starting(), u);
	}
	else
	{
		if (errorText.empty()) errorText = STRING(UNABLE_TO_SEND_FILE);
		auto u = source->getUpload();
		if (u)
		{
			u->setType(Transfer::TYPE_FILE);
			fire(UploadManagerListener::Failed(), u, errorText);
		}
		else
			ConnectionManager::getInstance()->fireUploadError(source->getHintedUser(), errorText, source->getConnectionQueueToken());
	}
}

void UploadManager::processGET(UserConnection* source, const AdcCommand& c) noexcept
{
	if (GlobalState::isShuttingDown())
		return;

	const string& type = c.getParam(0);
	const string& fname = c.getParam(1);
	int64_t startPos = Util::toInt64(c.getParam(2));
	int64_t bytes = Util::toInt64(c.getParam(3));
#ifdef _DEBUG
//	LogManager::message("on(AdcCommand::GET startPos = " + Util::toString(startPos) + " bytes = " + Util::toString(bytes));
#endif

	bool hideShare;
	CID shareGroup;
	getShareGroup(source, hideShare, shareGroup);

	string errorText;
	int compressionType = COMPRESSION_DISABLED;
	if (c.hasFlag(TAG('Z', 'L'), 4))
	{
		auto ss = SettingsManager::instance.getCoreSettings();
		ss->lockRead();
		if (ss->getInt(Conf::MAX_COMPRESSION))
			compressionType = COMPRESSION_CHECK_FILE_TYPE;
		ss->unlockRead();
	}

	if (prepareFile(source, type, fname, hideShare, shareGroup, startPos, bytes, c.hasFlag(TAG('R', 'E'), 4), compressionType, errorText))
	{
		auto u = source->getUpload();
		dcassert(u != nullptr);
		AdcCommand cmd(AdcCommand::CMD_SND);
		cmd.addParam(type);
		cmd.addParam(fname);
		cmd.addParam(Util::toString(u->getStartPos()));
		cmd.addParam(Util::toString(u->getSize()));
		
		if (compressionType == COMPRESSION_ENABLED)
			cmd.addParam("ZL1");

		string downloadedBytesStr;
		if (c.getParam(TAG('D', 'B'), 4, downloadedBytesStr))
		{
			int64_t downloadedBytes = Util::toInt64(downloadedBytesStr);
			if (downloadedBytes >= 0)
				u->setDownloadedBytes(downloadedBytes);
		}

		source->send(cmd);

		u->setStartTime(source->getLastActivity());
		source->setState(UserConnection::STATE_RUNNING);
		source->transmitFile(u->getReadStream());
		fire(UploadManagerListener::Starting(), u);
	}
	else
	{
		if (errorText.empty()) errorText = STRING(UNABLE_TO_SEND_FILE);
		auto u = source->getUpload();
		if (u)
		{
			if (type == Transfer::fileTypeNames[Transfer::TYPE_FILE])
				u->setType(Transfer::TYPE_FILE);
			fire(UploadManagerListener::Failed(), u, errorText);
		}
		else
			ConnectionManager::getInstance()->fireUploadError(source->getHintedUser(), errorText, source->getConnectionQueueToken());
	}
}

void UploadManager::processGFI(UserConnection* source, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 2)
	{
		source->send(AdcCommand(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_PROTOCOL_GENERIC, "Missing parameters"));
		return;
	}

	const string& type = c.getParam(0);
	const string& ident = c.getParam(1);

	bool hideShare;
	CID shareGroup;
	getShareGroup(source, hideShare, shareGroup);

	if (type == Transfer::fileTypeNames[Transfer::TYPE_FILE])
	{
		AdcCommand cmd(AdcCommand::CMD_RES);
		if (ShareManager::getInstance()->getFileInfo(cmd, ident, hideShare, shareGroup))
			source->send(cmd);
		else
			source->fileNotAvail();
	}
	else
	{
		source->fileNotAvail();
	}
}

void UploadManager::failed(UserConnection* source, const string& error) noexcept
{
	auto u = source->getUpload();
	
	if (u)
	{
		fire(UploadManagerListener::Failed(), u, error);
		dcdebug("UM::onFailed (%s): Removing upload\n", error.c_str());
		removeUpload(u);
	}
	
	removeConnectionSlot(source);
}

void UploadManager::transmitDone(UserConnection* source) noexcept
{
	//dcassert(!GlobalState::isShuttingDown());
	dcassert(source->getState() == UserConnection::STATE_RUNNING);
	auto u = source->getUpload();
	dcassert(u != nullptr);
	u->updateSpeed(source->getLastActivity());
	
	source->setState(UserConnection::STATE_GET);
	
	if (!u->isSet(Upload::FLAG_CHUNKED))
	{
		logUpload(u);
		removeUpload(u);
	}
	else
	{
		removeUpload(u, true);
	}
}

void UploadManager::logUpload(const UploadPtr& upload)
{
	if (GlobalState::isShuttingDown())
		return;

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const bool logUploads = ss->getBool(Conf::LOG_UPLOADS);
	const bool logFileLists = ss->getBool(Conf::LOG_FILELIST_TRANSFERS);
	ss->unlockRead();
	if (logUploads && upload->getType() != Transfer::TYPE_TREE && (logFileLists || upload->getType() != Transfer::TYPE_FULL_LIST))
	{
		StringMap params;
		upload->getParams(params);
		LOG(UPLOAD, params);
	}
	fire(UploadManagerListener::Complete(), upload);
}

size_t UploadManager::addToQueue(const UserConnection* source, const string& file, int64_t pos, int64_t size, uint16_t flags)
{
	dcassert(!GlobalState::isShuttingDown());
	size_t queuePosition = 0;
	
	UploadQueueFilePtr uqi;
	uint64_t expires = GET_TICK() + WAITING_USER_EXPIRE_TIME;
	HintedUser hintedUser = source->getHintedUser();
	{
		LOCK(csQueue);
		auto it = std::find_if(slotQueue.begin(), slotQueue.end(), [&](const UserPtr& u) -> bool { ++queuePosition; return u == source->getUser(); });
		if (it != slotQueue.end())
		{
			it->setToken(source->getConnectionQueueToken());
			UploadQueueFilePtr uqfp = it->findWaitingFile(file);
			if (uqfp)
			{
				uqfp->setPos(pos);
				uqfp->setFlags(flags);
				it->setExpires(expires);
				return queuePosition;
			}
		}
		uqi.reset(new UploadQueueFile(file, pos, size, flags));
		if (it == slotQueue.end())
		{
			++queuePosition;
			slotQueue.emplace_back(hintedUser, source->getConnectionQueueToken(), uqi, expires);
		}
		else
		{
			auto& wu = *it;
			wu.addWaitingFile(uqi);
			wu.setExpires(expires);
		}
		++slotQueueId;
	}
	if (g_count_WaitingUsersFrame)
		fire(UploadManagerListener::QueueAdd(), hintedUser, uqi);
	return queuePosition;
}

bool UploadManager::clearUserFilesL(const UserPtr& user)
{
	//dcassert(!GlobalState::isShuttingDown());
	auto it = std::find_if(slotQueue.cbegin(), slotQueue.cend(), [&](const UserPtr& u)
	{
		return u == user;
	});
	if (it == slotQueue.end()) return false;
	slotQueue.erase(it);
	if (slotQueue.empty()) slotQueueId = 0; else ++slotQueueId;
	return true;
}

void UploadManager::fireUserRemoved(const UserPtr& user) noexcept
{
	if (g_count_WaitingUsersFrame && !GlobalState::isShuttingDown())
		fire(UploadManagerListener::QueueRemove(), user);
}

void UploadManager::clearUploads() noexcept
{
	WRITE_LOCK(*csFinishedUploads);
	uploads.clear();
	finishedUploads.clear();
}

void UploadManager::addConnection(UserConnection* conn)
{
	dcassert(!GlobalState::isShuttingDown());
	if (conn->isIpBlocked(false))
	{
		removeConnectionSlot(conn);
		return;
	}
	conn->setState(UserConnection::STATE_GET);
}

void UploadManager::testSlotTimeout(uint64_t tick /*= GET_TICK()*/)
{
	dcassert(!GlobalState::isShuttingDown());
	vector<UserPtr> users;
	{
		WRITE_LOCK(*csReservedSlots);
		for (auto j = reservedSlots.cbegin(); j != reservedSlots.cend();)
		{
			if (j->second < tick)
			{
				users.push_back(j->first);
				reservedSlots.erase(j++);			
			}
			else
				++j;
		}
	}
	if (!users.empty())
	{
		auto um = UserManager::getInstance();
		for (UserPtr& user : users)
		{
			user->unsetFlag(User::RESERVED_SLOT);
			um->fireReservedSlotChanged(user);
		}
	}
}

void UploadManager::processSlot(UserConnection::SlotTypes slotType, int delta)
{
	switch (slotType)
	{
		case UserConnection::STDSLOT:
		case UserConnection::RESSLOT:
			g_running += delta;
			break;
		case UserConnection::MINISLOT:
			extra += delta;
			break;
		case UserConnection::PFS_SLOT:
			extraPartial += delta;
			break;
	}
}

void UploadManager::removeConnectionSlot(UserConnection* source)
{
	//dcassert(source->getUpload() == nullptr);
	processSlot(source->getSlotType(), -1);
	source->setSlotType(UserConnection::NOSLOT);
}

void UploadManager::notifyQueuedUsers(int64_t tick)
{
	dcassert(!GlobalState::isShuttingDown());
	if (slotQueue.empty())
		return; //no users to notify
	vector<WaitingUser> notifyList;
	{
		LOCK(csQueue);
		int freeslots = getFreeSlots();
		if (freeslots > 0)
		{
			freeslots -= notifiedUsers.size();
			while (freeslots > 0 && !slotQueue.empty())
			{
				// let's keep him in the connectingList until he asks for a file
				const WaitingUser& wu = slotQueue.front();
				if (wu.getUser()->isOnline())
				{
					notifiedUsers[wu.getUser()] = tick;
					freeslots--;
				}
				notifyList.push_back(wu);
				slotQueue.pop_front();
			}
		}
	}
	for (const WaitingUser& wu : notifyList)
	{
		if (wu.getUser()->isOnline())
			ClientManager::getInstance()->connect(wu.getHintedUser(), wu.getToken(), false);
		if (g_count_WaitingUsersFrame)
			fire(UploadManagerListener::QueueRemove(), wu.getUser());
	}
}

void UploadManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	if (GlobalState::isShuttingDown())
		return;
	UserList disconnects;
	{
		testSlotTimeout(tick);
		
		{
			LOCK(csQueue);
			for (auto i = notifiedUsers.cbegin(); i != notifiedUsers.cend();)
			{
				if (i->second + 90 * 1000 < tick)
				{
					const UserPtr& user = i->first;
					if (clearUserFilesL(user)) tmpUsers.push_back(user);
					i = notifiedUsers.erase(i);
				}
				else
					++i;
			}
			for (auto i = slotQueue.begin(); i != slotQueue.end();)
				if (i->isExpired(tick))
				{
					tmpUsers.push_back(i->getUser());
					i = slotQueue.erase(i);
				}
				else
					++i;
		}
		
		if (!tmpUsers.empty())
		{
			for (const auto& user : tmpUsers)
				fireUserRemoved(user);
			tmpUsers.clear();
		}

		auto ss = SettingsManager::instance.getCoreSettings();
		ss->lockRead();
		const bool autoKick = ss->getBool(Conf::AUTO_KICK);
		const bool autoKickNoFavs = ss->getBool(Conf::AUTO_KICK_NO_FAVS);
		ss->unlockRead();

		if (autoKick)
		{
			READ_LOCK(*csFinishedUploads);
			for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
			{
				if (auto u = *i)
				{
					if (u->getUser() && u->getUser()->isOnline())
					{
						u->unsetFlag(Upload::FLAG_PENDING_KICK);
						continue;
					}
					
					if (u->isSet(Upload::FLAG_PENDING_KICK))
					{
						disconnects.push_back(u->getUser());
						continue;
					}
					if (autoKickNoFavs && (u->getUser()->getFlags() & (User::FAVORITE | User::BANNED)) == User::FAVORITE)
					{
						continue;
					}
					u->setFlag(Upload::FLAG_PENDING_KICK);
				}
			}
		}
	}
	
	auto cm = ConnectionManager::getInstance();
	for (auto i = disconnects.cbegin(); i != disconnects.cend(); ++i)
	{
		LogManager::message(STRING(DISCONNECTED_USER) + ' ' + Util::toString(ClientManager::getNicks((*i)->getCID(), Util::emptyString)));
		cm->disconnect(*i, false);
	}
	
	lastFreeSlots = getFreeSlots();
}

// TimerManagerListener
void UploadManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (GlobalState::isShuttingDown())
		return;

	UploadArray tickList;
	{
		WRITE_LOCK(*csFinishedUploads);
		for (auto i = finishedUploads.cbegin(); i != finishedUploads.cend();)
		{
			auto u = *i;
			if (u)
			{
				if (tick > u->getTickForRemove())
				{
					logUpload(u);
					finishedUploads.erase(i++);
				} else i++;
			} else
			{
				dcassert(0);
				finishedUploads.erase(i++);
			}
		}
	}
	static int g_count = 11;
	if (++g_count % 10 == 0)
		SharedFileStream::cleanup();
	tickList.reserve(uploads.size());
	{
		READ_LOCK(*csFinishedUploads);
		for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
		{
			auto u = *i;
			if (u->getPos() > 0)
			{
				TransferData td;
				initTransferData(td, u.get());
				td.dumpToLog();
				tickList.push_back(td);
				u->updateSpeed(tick);
			}
		}
	}
	if (!tickList.empty())
		fire(UploadManagerListener::Tick(), tickList);
	notifyQueuedUsers(tick);
	
	if (!isFireball)
	{
		if (getRunningAverage() >= 1024 * 1024)
		{
			if (fireballStartTick)
			{
				if (tick - fireballStartTick > 60 * 1000)
				{
					isFireball = true;
					ClientManager::infoUpdated();
				}
			}
			else
				fireballStartTick = tick;
		}
		else 
			fireballStartTick = 0;
		if (!isFireball && !isFileServer && tick > fileServerCheckTick)
		{
			if ((Util::getUpTime() > 10 * 24 * 60 * 60) && // > 10 days uptime
			    (Socket::g_stats.tcp.uploaded + Socket::g_stats.ssl.uploaded > 100ULL * 1024 * 1024 * 1024) && // > 100 GiB uploaded
			    (ShareManager::getInstance()->getTotalSharedSize() > 1.5 * 1024 * 1024 * 1024 * 1024)) // > 1.5 TiB shared
			{
				isFileServer = true;
				ClientManager::infoUpdated();
			}
			fileServerCheckTick = tick + 300 * 1000;
		}
	}
}

void UploadManager::on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept
{
	bool notifyRemove = false;
	if (!user->isOnline())
	{
		LOCK(csQueue);
		notifyRemove = clearUserFilesL(user);
	}
	if (notifyRemove) fireUserRemoved(user);
}

void UploadManager::removeFinishedUpload(const UserPtr& user)
{
	//dcassert(!GlobalState::isShuttingDown());
	WRITE_LOCK(*csFinishedUploads);
	for (auto i = finishedUploads.cbegin(); i != finishedUploads.cend(); ++i)
	{
		auto up = *i;
		if (user == up->getUser())
		{
			finishedUploads.erase(i);
			break;
		}
	}
}

/**
 * Abort upload of specific file
 */
void UploadManager::abortUpload(const string& fileName, bool waiting)
{
	//dcassert(!GlobalState::isShuttingDown());
	bool nowait = true;
	{
		READ_LOCK(*csFinishedUploads);
		for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
		{
			auto u = *i;
			if (u->getPath() == fileName)
			{
				u->disconnect(true);
				nowait = false;
			}
		}
	}
	
	if (nowait) return;
	if (!waiting) return;
	
	for (int i = 0; i < 20 && !nowait; i++)
	{
		Thread::sleep(100);
		{
			READ_LOCK(*csFinishedUploads);
			nowait = true;
			for (auto j = uploads.cbegin(); j != uploads.cend(); ++j)
			{
				if ((*j)->getPath() == fileName)
				{
					dcdebug("upload %s is not removed\n", fileName.c_str());
					nowait = false;
					break;
				}
			}
		}
	}
	
	if (!nowait)
	{
		dcdebug("abort upload timeout %s\n", fileName.c_str());
	}
}

uint64_t UploadManager::getReservedSlotTick(const UserPtr& user) const
{
	READ_LOCK(*csReservedSlots);
	const auto j = reservedSlots.find(user);
	return j != reservedSlots.end() ? j->second : 0;
}

void UploadManager::getReservedSlots(vector<UploadManager::ReservedSlotInfo>& out) const
{
	READ_LOCK(*csReservedSlots);
	out.reserve(reservedSlots.size());
	for (auto& rs : reservedSlots)
		out.emplace_back(ReservedSlotInfo{ rs.first, rs.second });
}

void UploadManager::save()
{
	DBRegistryMap values;
	{
		uint64_t currentTick = GET_TICK();
		uint64_t currentTime = (uint64_t) GET_TIME();
		READ_LOCK(*csReservedSlots);
		for (auto i = reservedSlots.cbegin(); i != reservedSlots.cend(); ++i)
		{
			uint64_t timeout = i->second;
			if (timeout > currentTick + 1000)
				values[i->first->getCID().toBase32()] = DBRegistryValue((timeout-currentTick)/1000 + currentTime);
		}
	}
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	if (conn)
	{
		conn->saveRegistry(values, e_ExtraSlot, true);
		dm->putConnection(conn);
	}
}

void UploadManager::load()
{
	DBRegistryMap values;
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	if (conn)
	{
		conn->loadRegistry(values, e_ExtraSlot);
		dm->putConnection(conn);
	}
	uint64_t currentTick = GET_TICK();
	int64_t currentTime = (int64_t) GET_TIME();
	for (auto k = values.cbegin(); k != values.cend(); ++k)
	{
		auto user = ClientManager::createUser(CID(k->first), Util::emptyString, Util::emptyString);
		WRITE_LOCK(*csReservedSlots);
		int64_t timeout = k->second.ival;
		if (timeout > currentTime)
		{
			reservedSlots[user] = (timeout-currentTime)*1000 + currentTick;
			user->setFlag(User::RESERVED_SLOT);
		}
	}
	testSlotTimeout();
}

int UploadManager::getSlots() noexcept
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const int slots = ss->getInt(Conf::SLOTS);
	const int hubSlots = ss->getInt(Conf::HUB_SLOTS);
	ss->unlockRead();
	return std::max(slots, std::max(hubSlots, 0) * Client::getTotalCounts());
}

int UploadManager::getFreeExtraSlots() const
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const int extraSlots = ss->getInt(Conf::EXTRA_SLOTS);
	ss->unlockRead();
	return std::max(extraSlots - getExtra(), 0);
}

void UploadManager::updateSettings() noexcept
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string pattern = ss->getString(Conf::COMPRESSED_FILES);
	ss->unlockRead();

	csCompressedFiles.lock();
	if (pattern != compressedFilesPattern)
	{
		compressedFilesPattern = std::move(pattern);
		if (!Wildcards::regexFromPatternList(reCompressedFiles, compressedFilesPattern, true))
			compressedFilesPattern.clear();
	}
	csCompressedFiles.unlock();
}
