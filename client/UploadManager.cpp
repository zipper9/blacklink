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
#include "DownloadManager.h"
#include "ConnectionManager.h"
#include "ShareManager.h"
#include "DatabaseManager.h"
#include "CryptoManager.h"
#include "Upload.h"
#include "QueueManager.h"
#include "FinishedManager.h"
#include "SharedFileStream.h"
#include "IpGrant.h"
#include "Wildcards.h"
#include "ZUtils.h"
#include "FilteredFile.h"

static const unsigned WAIT_TIME_LAST_CHUNK  = 3000;
static const unsigned WAIT_TIME_OTHER_CHUNK = 8000;

#ifdef _DEBUG
std::atomic_int UploadQueueFile::g_upload_queue_item_count(0);
#endif
uint32_t UploadManager::g_count_WaitingUsersFrame = 0;
int UploadManager::g_running = 0;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
UploadManager::BanMap UploadManager::g_lastBans;
std::unique_ptr<RWLock> UploadManager::g_csBans = std::unique_ptr<RWLock>(RWLock::create());
#endif
int64_t UploadManager::g_runningAverage;

UploadManager::UploadManager() noexcept :
	extra(0), lastGrant(0), lastFreeSlots(-1),
	fireballStartTick(0), fileServerCheckTick(0), isFireball(false), isFileServer(false), extraPartial(0)
{
	csFinishedUploads = std::unique_ptr<RWLock>(RWLock::create());
	csReservedSlots = std::unique_ptr<RWLock>(RWLock::create());
	ClientManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);
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
	td.actual = u->getAdjustedActual();
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

#ifdef IRAINMAN_ENABLE_AUTO_BAN
bool UploadManager::handleBan(UserConnection* source/*, bool forceBan, bool noChecks*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const UserPtr& user = source->getUser();
	if (!user->isOnline()) // if not online, cheat (connection without hub)
	{
		source->disconnect();
		return true;
	}
	bool l_is_ban = false;
	const bool l_is_favorite = FavoriteManager::getInstance()->isFavoriteUser(user, l_is_ban);
	const auto banType = user->hasAutoBan(nullptr, l_is_favorite);
	bool banByRules = banType != User::BAN_NONE;
	if (banByRules)
	{
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* hub = fm->getFavoriteHubEntryPtr(source->getHubUrl());
		if (hub && hub->getExclChecks())
			banByRules = false;
		fm->releaseFavoriteHubEntryPtr(hub);
	}
	if (/*!forceBan &&*/ (/*noChecks ||*/ !banByRules)) return false;
	
	banmsg_t msg;
	msg.share = (int)(user->getBytesShared() / (1024 * 1024 * 1024));
	msg.slots = user->getSlots();
	msg.limit = user->getLimit();
	msg.min_share = SETTING(AUTOBAN_SHARE);
	msg.min_slots = SETTING(AUTOBAN_SLOTS_MIN);
	msg.max_slots = SETTING(AUTOBAN_SLOTS_MAX);
	msg.min_limit = SETTING(AUTOBAN_LIMIT);
	
	/*
	[-] brain-ripper
	follow rules are ignored!
	comments saved only for history
	//2) "автобан" по шаре максимум ограничить размером "своя шара" (но не более 20 гиг)
	//3) "автобан" по слотам максимум ограничить размером "сколько слотов у меня"/2 (но не более 15 слотов)
	//4) "автобан" по лимиттеру максимум ограничить размером "лимиттер у меня"/2 (но не выше 60 кб/сек)
	*/
	
	// BAN for Slots:[1/5] Share:[17/200]Gb Limit:[30/50]kb/s Msg: share more files, please!
	string banstr;
	//if (!forceBan)
	//{
	/*
	if (msg.slots < msg.min_slots)
	    banstr = banstr + " Slots < "  + Util::toString(msg.min_slots);
	if (msg.slots > msg.max_slots)
	    banstr = banstr + " Slots > " + Util::toString(msg.max_slots);
	if (msg.share < msg.min_share)
	    banstr = banstr + " Share < " + Util::toString(msg.min_share) + " Gb";
	if (msg.limit && msg.limit < msg.min_limit)
	    banstr = banstr + " Limit < " + Util::toString(msg.min_limit) + " kb/s";
	banstr = banstr + " Msg: " + SETTING(BAN_MESSAGE);
	*/
	banstr = "BAN for";
	
	if (banType & User::BAN_BY_MIN_SLOT)
		banstr += " Slots < "  + Util::toString(msg.min_slots);
	if (banType & User::BAN_BY_MAX_SLOT)
		banstr += " Slots > "  + Util::toString(msg.max_slots);
	if (banType & User::BAN_BY_SHARE)
		banstr += " Share < "  + Util::toString(msg.min_share) + " Gb";
	if (banType & User::BAN_BY_LIMIT)
		banstr += " Limit < "  + Util::toString(msg.min_limit) + " kB/s";
		
	banstr += " Msg: " + SETTING(BAN_MESSAGE);
	//}
	//else
	//{
	//  banstr = "BAN forever";
	//}
#ifdef _DEBUG
	{
		auto logline = banstr + " Log autoban:";
		
		if (banType & User::BAN_BY_MIN_SLOT)
			logline += " Slots:[" + Util::toString(user->getSlots()) + '/' + Util::toString(msg.min_slots) + '/' + Util::toString(msg.max_slots) + ']';
		if (banType & User::BAN_BY_SHARE)
			logline += " Share:[" + Util::toString(int(user->getBytesShared() / (1024 * 1024 * 1024))) + '/' + Util::toString(msg.min_share) + "]Gb";
		if (banType & User::BAN_BY_LIMIT)
			logline += " Limit:[" + Util::toString(user->getLimit()) + '/' + Util::toString(msg.min_limit) + "]kb/s";
			
		logline += " Msg: " + SETTING(BAN_MESSAGE);
		
		LogManager::message("User:" + user->getLastNick() + ' ' + logline); //[+]PPA
	}
#endif
	// old const bool sendStatus = source->isSet(UserConnection::FLAG_SUPPORTS_BANMSG);
	
	if (!BOOLSETTING(AUTOBAN_STEALTH))
	{
		source->error(banstr);
		
		if (BOOLSETTING(AUTOBAN_SEND_PM))
		{
			msg.tick = TimerManager::getTick();
			const auto pmMsgPeriod = SETTING(AUTOBAN_MSG_PERIOD);
			const auto key = user->getCID().toBase32();
			bool sendPm;
			{
				WRITE_LOCK(*g_csBans);
				auto t = g_lastBans.find(key);
				if (t == g_lastBans.end()) // new banned user
				{
					g_lastBans[key] = msg;
					sendPm = pmMsgPeriod != 0;
				}
				else if (!t->second.same(msg)) // new ban message
				{
					t->second = msg;
					sendPm = pmMsgPeriod != 0;
				}
				else if (pmMsgPeriod && (uint32_t)(msg.tick - t->second.tick) >= (uint32_t)pmMsgPeriod * 60 * 1000) // [!] IRainman fix: "Repeat PM period (0 - disable PM), min"
				{
					t->second.tick = msg.tick;
					sendPm = true;
				}
				else
				{
					sendPm = false;
				}
			}
			if (sendPm)
			{
				ClientManager::privateMessage(source->getHintedUser(), banstr, false);
			}
		}
	}
	
//	if (BOOLSETTING(BAN_STEALTH)) source->maxedOut();

	return true;
}

bool UploadManager::isBanReply(const UserPtr& user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const auto key = user->getCID().toBase32();
	{
		READ_LOCK(*g_csBans);
		const auto t = g_lastBans.find(key);
		if (t != g_lastBans.end())
		{
			return (TimerManager::getTick() - t->second.tick) < 2000;
		}
	}
	return false;
}
#endif // IRAINMAN_ENABLE_AUTO_BAN

bool UploadManager::hasUpload(const UserConnection* newLeecher) const
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (newLeecher->getSocket())
	{
		const auto newLeecherIp = newLeecher->getSocket()->getIp4();
		const auto newLeecherShare = newLeecher->getUser()->getBytesShared();
		const auto newLeecherNick = newLeecher->getUser()->getLastNick();
		
		READ_LOCK(*csFinishedUploads);
		
		for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
		{
			const auto u = *i;
			dcassert(u);
			dcassert(u->getUser());
			const auto uploadUserIp = u->getUserConnection()->getSocket()->getIp4();
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

bool UploadManager::prepareFile(UserConnection* source, const string& typeStr, const string& fileName, bool hideShare, const CID& shareGroup, int64_t startPos, int64_t& bytes, bool listRecursive)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcdebug("Preparing %s %s " I64_FMT " " I64_FMT " %d\n", typeStr.c_str(), fileName.c_str(), startPos, bytes, listRecursive);
	if (ClientManager::isBeforeShutdown())
	{
		return false;
	}
	if (fileName.empty() || startPos < 0 || bytes < -1 || bytes == 0)
	{
		source->fileNotAvail("Invalid request");
		return false;
	}
	const auto ipAddr = source->getRemoteIp();
	const auto cipherName = source->getCipherName();
	const bool isTypeTree = typeStr == Transfer::fileTypeNames[Transfer::TYPE_TREE];
	const bool isTypePartialList = typeStr == Transfer::fileTypeNames[Transfer::TYPE_PARTIAL_LIST];
#ifdef FLYLINKDC_USE_DOS_GUARD
	if (isTypeTree || isTypePartialList)
	{
		const HintedUser& l_User = source->getHintedUser();
		if (l_User.user)
		{
			// [!] IRainman opt: CID on ADC hub is unique to all hubs,
			// in NMDC it is obtained from the hash of username and hub address (computed locally).
			// There is no need to use any additional data, but it is necessary to consider TTH of the file.
			// This ensures the absence of a random user locks sitting on several ADC hubs,
			// because the number of connections from a single user much earlier limited in the ConnectionManager.
			const string l_hash_key = Util::toString(l_User.user->getCID().toHash()) + fileName;
			uint8_t l_count_dos;
			{
				LOCK(csDos);
				l_count_dos = ++m_dos_map[l_hash_key];
			}
			const uint8_t l_count_attempts = isTypePartialList ? 5 : 30;
			if (l_count_dos > l_count_attempts)
			{
				dcdebug("l_hash_key = %s ++m_dos_map[l_hash_key] = %d\n", l_hash_key.c_str(), l_count_dos);
				if (BOOLSETTING(LOG_DDOS_TRACE))
				{
					char l_buf[2000];
					l_buf[0] = 0;
					sprintf_s(l_buf, _countof(l_buf), CSTRING(DOS_ATACK),
					          l_User.user->getLastNick().c_str(),
					          l_User.hint.c_str(),
					          l_count_attempts,
					          fileName.c_str());
					LogManager::ddos_message(l_buf);
				}
#ifdef IRAINMAN_DISALLOWED_BAN_MSG
				if (source->isSet(UserConnection::FLAG_SUPPORTS_BANMSG))
				{
					source->error(UserConnection::g_PLEASE_UPDATE_YOUR_CLIENT);
				}
#endif
				if (isTypePartialList)
				{
					/*
					Fix
					[2017-10-01 16:20:21] <MikeKMV> оказывается флай умеет флудить
					[2017-10-01 16:20:25] <MikeKMV> Client: [Incoming][194.186.25.22]       $ADCGET list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 -1 ZL1
					Client: [Outgoing][194.186.25.22]       $ADCSND list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 2029 ZL1|
					Client: [Incoming][194.186.25.22]       $ADCGET list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 -1 ZL1
					Client: [Outgoing][194.186.25.22]       $ADCSND list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 2029 ZL1|
					Client: [Incoming][194.186.25.22]       $ADCGET list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 -1 ZL1
					Client: [Outgoing][194.186.25.22]       $ADCSND list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 2029 ZL1|
					*/
					LogManager::message("Disconnect bug client: $ADCGET / $ADCSND User: " + l_User.user->getLastNick() + " Hub:" + source->getHubUrl());
					source->disconnect();
					
				}
				return false;
			}
		}
	}
#endif // FLYLINKDC_USE_DOS_GUARD
	InputStream* is = nullptr;
	int64_t start = 0;
	int64_t size = 0;
	int64_t fileSize = 0;
	
	const bool isFileList = fileName == Transfer::fileNameFilesBzXml || fileName == Transfer::fileNameFilesXml;
	bool isFree = isFileList;
	bool isPartial = false;
	
	string sourceFile;
	const bool isTypeFile = typeStr == Transfer::fileTypeNames[Transfer::TYPE_FILE];
	const bool isTTH  = (isTypeFile || isTypeTree) && fileName.length() == 43 && fileName.compare(0, 4, "TTH/", 4) == 0;
	Transfer::Type type;
	TTHValue tth;
	if (isTTH)
	{
		tth = TTHValue(fileName.c_str() + 4);
	}
	try
	{
		if (isTypeFile && !ClientManager::isBeforeShutdown() && ShareManager::isValidInstance())
		{
			sourceFile = isTTH ? 
				ShareManager::getInstance()->getFilePathByTTH(tth) :
				ShareManager::getInstance()->getFilePath(fileName, hideShare, shareGroup);
			if (fileName == Transfer::fileNameFilesXml)
			{
				// FIXME: Use UnBZ2 filter
				string bz2 = File(sourceFile, File::READ, File::OPEN).read();
				string xml;
				CryptoManager::getInstance()->decodeBZ2(reinterpret_cast<const uint8_t*>(bz2.data()), bz2.size(), xml);
				// Clear to save some memory...
				bz2.clear();
				bz2.shrink_to_fit();
				is = new MemoryInputStream(xml);
				start = 0;
				fileSize = size = xml.size();
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
				
				if (fileSize <= (int64_t)(SETTING(MINISLOT_SIZE)) << 10)
					isFree = true;
				
				f->setPos(start);
				is = f;
				if (start + size < fileSize)
				{
					is = new LimitedInputStream<true>(is, size);
				}
			}
			type = isFileList ? Transfer::TYPE_FULL_LIST : Transfer::TYPE_FILE;
		}
		else if (isTypeTree)
		{
			if (hideShare)
			{
				source->fileNotAvail();
				return false;
			}
			MemoryInputStream* mis;
			if (isTTH)
			{
				mis = ShareManager::getInstance()->getTreeByTTH(tth);
				if (!mis)
				{
					if (QueueManager::g_fileQueue.isQueued(tth))
						mis = ShareManager::getTreeFromStore(tth);
				}
			}
			else
				mis = ShareManager::getInstance()->getTree(fileName);
			if (!mis)
			{
				source->fileNotAvail();
				return false;
			}
			
			sourceFile = fileName;
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			isFree = true;
			type = Transfer::TYPE_TREE;
		}
		else if (isTypePartialList)
		{
			// Partial file list
			MemoryInputStream* mis = ShareManager::getInstance()->generatePartialList(fileName, listRecursive, hideShare, shareGroup);
			if (!mis)
			{
				source->fileNotAvail();
				return false;
			}
			
			sourceFile = fileName;
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			isFree = true;
			type = Transfer::TYPE_PARTIAL_LIST;
		}
		else
		{
			source->fileNotAvail("Unknown file type");
			return false;
		}
	}
	catch (const ShareException&)
	{
		// Partial file sharing upload
		if (isTTH)
		{
			if (QueueManager::isChunkDownloaded(tth, startPos, bytes, sourceFile))
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
						isPartial = true;
						type = Transfer::TYPE_FILE;
						goto ok;
					}
					catch (const Exception&)
					{
						delete is;
						is = nullptr;
					}
				}
			}
		}
		source->fileNotAvail();
		return false;
	}
	catch (const Exception& e)
	{
		LogManager::message(STRING(UNABLE_TO_SEND_FILE) + ' ' + sourceFile + ": " + e.getError());
		source->fileNotAvail();
		return false;
	}
	
ok:

	auto slotType = source->getSlotType();
	uint64_t slotTimeout = 0;
	{
		READ_LOCK(*csReservedSlots);
		auto i = reservedSlots.find(source->getUser());
		if (i != reservedSlots.end()) slotTimeout = i->second;
	}
	bool hasReserved = slotTimeout == 0 ? false : slotTimeout > GET_TICK();
	if (!hasReserved)
		hasReserved = BOOLSETTING(EXTRA_SLOT_TO_DL) && DownloadManager::checkFileDownload(source->getUser());// !SMT!-S
	
	const bool isFavorite = FavoriteManager::getInstance()->hasAutoGrantSlot(source->getUser());
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	// !SMT!-S
	if (!isFavorite && SETTING(ENABLE_AUTO_BAN))
	{
		// фавориты под автобан не попадают
		if (!isFileList && !hasReserved && handleBan(source))
		{
			delete is;
			addFailedUpload(source, sourceFile, startPos, size, isTypePartialList ? UploadQueueFile::FLAG_PARTIAL_FILE_LIST : 0);
			source->disconnect();
			return false;
		}
	}
#endif // IRAINMAN_ENABLE_AUTO_BAN
	
	if (slotType != UserConnection::STDSLOT || hasReserved)
	{
		//[-] IRainman autoban fix: please check this code after merge
		//bool hasReserved = reservedSlots.find(source->getUser()) != reservedSlots.end();
		//bool isFavorite = FavoriteManager::getInstance()->hasSlot(source->getUser());
		bool hasFreeSlot = getFreeSlots() > 0;
		if (hasFreeSlot)
		{
			LOCK(csQueue);
			hasFreeSlot = (slotQueue.empty() && notifiedUsers.empty() || (notifiedUsers.find(source->getUser()) != notifiedUsers.end()));
		}
		
		bool isAutoSlot = getAutoSlot();
		bool isHasUpload = hasUpload(source);
		
#ifdef SSA_IPGRANT_FEATURE // SSA - additional slots for special IP's
		bool hasSlotByIP = false;
		if (BOOLSETTING(EXTRA_SLOT_BY_IP))
		{
			if (!(hasReserved || isFavorite || isAutoSlot || hasFreeSlot || isHasUpload))
			{
				Ip4Address addr = source->getRemoteIp();
				if (addr) hasSlotByIP = ipGrant.check(addr);
			}
		}
#endif // SSA_IPGRANT_FEATURE
		if (!(hasReserved || isFavorite || isAutoSlot || hasFreeSlot
#ifdef SSA_IPGRANT_FEATURE
		        || hasSlotByIP
#endif
		     ) || isHasUpload)
		{
			const bool supportsFree = source->isSet(UserConnection::FLAG_SUPPORTS_MINISLOTS);
			const bool allowedFree = slotType == UserConnection::EXTRASLOT || getFreeExtraSlots() > 0;
			bool partialFree = isPartial && ((slotType == UserConnection::PARTIALSLOT) || (extraPartial < SETTING(EXTRA_PARTIAL_SLOTS)));
			
			if (isFree && supportsFree && allowedFree)
			{
				slotType = UserConnection::EXTRASLOT;
			}
			else if (partialFree)
			{
				slotType = UserConnection::PARTIALSLOT;
			}
			else
			{
				delete is;
				source->maxedOut(addFailedUpload(source, sourceFile, startPos, fileSize, isTypePartialList ? UploadQueueFile::FLAG_PARTIAL_FILE_LIST : 0));
				source->disconnect();
				return false;
			}
		}
		else
		{
#ifdef SSA_IPGRANT_FEATURE
			if (hasSlotByIP)
				LogManager::message("IpGrant: " + STRING(GRANTED_SLOT_BY_IP) + ' ' + Util::printIpAddress(source->getRemoteIp()));
#endif
			slotType = UserConnection::STDSLOT;
		}
		
		setLastGrant(GET_TICK());
	}
	
	{
		LOCK(csQueue);
		
		// remove file from upload queue
		clearUserFilesL(source->getUser());
		
		// remove user from notified list
		const auto cu = notifiedUsers.find(source->getUser());
		if (cu != notifiedUsers.end())
		{
			notifiedUsers.erase(cu);
		}
	}
	
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
	UploadPtr u = std::make_shared<Upload>(source, tth, sourceFile, Util::printIpAddress(ipAddr), cipherName);
	u->setReadStream(is);
	u->setSegment(Segment(start, size));
	source->setUpload(u);
	
	if (u->getSize() != fileSize)
		u->setFlag(Upload::FLAG_CHUNKED);
		
	if (resumed)
		u->setFlag(Upload::FLAG_RESUMED);
		
	if (isPartial)
		u->setFlag(Upload::FLAG_UPLOAD_PARTIAL);
		
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

bool UploadManager::isCompressedFile(const Upload* u)
{
	string name = Util::getFileName(u->getPath());
	const string& pattern = SETTING(COMPRESSED_FILES);
	csCompressedFiles.lock();
	if (pattern != compressedFilesPattern)
	{
		compressedFilesPattern = pattern;
		if (!Wildcards::regexFromPatternList(reCompressedFiles, compressedFilesPattern, true))
			compressedFilesPattern.clear();
	}
	bool result = std::regex_match(name, reCompressedFiles);
	csCompressedFiles.unlock();
	return result;
}

bool UploadManager::getAutoSlot()
{
	dcassert(!ClientManager::isBeforeShutdown());
	/** A 0 in settings means disable */
	if (SETTING(AUTO_SLOT_MIN_UL_SPEED) == 0)
		return false;
	/** Max slots */
	if (getSlots() + SETTING(AUTO_SLOTS) < g_running)
		return false;
	/** Only grant one slot per 30 sec */
	if (GET_TICK() < getLastGrant() + 30 * 1000)
		return false;
	/** Grant if upload speed is less than the threshold speed */
	return getRunningAverage() < SETTING(AUTO_SLOT_MIN_UL_SPEED) * 1024;
}

void UploadManager::shutdown()
{
	WRITE_LOCK(*csReservedSlots);
	reservedSlots.clear();
}

void UploadManager::removeUpload(UploadPtr& upload, bool delay)
{
	UploadArray tickList;
	//dcassert(!ClientManager::isBeforeShutdown());
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
		fly_fire1(UploadManagerListener::Tick(), tickList);		
}

void UploadManager::reserveSlot(const HintedUser& hintedUser, uint64_t seconds)
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		WRITE_LOCK(*csReservedSlots);
		reservedSlots[hintedUser.user] = GET_TICK() + seconds * 1000;
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
	if (BOOLSETTING(SEND_SLOTGRANT_MSG))
	{
		ClientManager::privateMessage(hintedUser, "+me " + STRING(SLOT_GRANTED_MSG) + ' ' + Util::formatSeconds(seconds), false);
	}
	if (notifyUser)
		ClientManager::getInstance()->connect(hintedUser, token, false);
}

void UploadManager::unreserveSlot(const HintedUser& hintedUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		WRITE_LOCK(*csReservedSlots);
		if (!reservedSlots.erase(hintedUser.user)) return;
	}
	save();
	UserManager::getInstance()->fireReservedSlotChanged(hintedUser.user);
	if (BOOLSETTING(SEND_SLOTGRANT_MSG))
	{
		ClientManager::privateMessage(hintedUser, "+me " + STRING(SLOT_REMOVED_MSG), false);
	}
}

static void getShareGroup(const UserConnection* source, bool& hideShare, CID& shareGroup)
{
	hideShare = false;
	if (source->getUser())
	{
		auto fm = FavoriteManager::getInstance();
		FavoriteUser::MaskType flags;
		int uploadLimit;
		if (fm->getFavUserParam(source->getUser(), flags, uploadLimit, shareGroup) && !shareGroup.isZero())
			return;
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(source->getHintedUser().hint);
		if (fhe)
		{
			hideShare = fhe->getHideShare();
			shareGroup = fhe->getShareGroup();
		}
		fm->releaseFavoriteHubEntryPtr(fhe);
	}
}

void UploadManager::on(UserConnectionListener::Get, UserConnection* source, const string& fileName, int64_t resume) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
	{
		return;
	}
	if (source->getState() != UserConnection::STATE_GET)
	{
		dcdebug("UM::onGet Bad state, ignoring\n");
		return;
	}
	
	bool hideShare;
	CID shareGroup;
	getShareGroup(source, hideShare, shareGroup);

	int64_t bytes = -1;
	if (prepareFile(source, Transfer::fileTypeNames[Transfer::TYPE_FILE], Util::toAdcFile(fileName), hideShare, shareGroup, resume, bytes))
	{
		source->setState(UserConnection::STATE_SEND);
		source->fileLength(Util::toString(source->getUpload()->getSize()));
	}
}

void UploadManager::on(UserConnectionListener::Send, UserConnection* source) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
	{
		return;
	}
	if (source->getState() != UserConnection::STATE_SEND)
	{
		LogManager::message("Send ignored in state " + Util::toString(source->getState()) + ", p=" + Util::toHexString(source), false);
		//dcdebug("UM::onSend Bad state, ignoring\n");
		return;
	}
	
	auto u = source->getUpload();
	dcassert(u != nullptr);
	u->setStartTime(source->getLastActivity());
	
	source->setState(UserConnection::STATE_RUNNING);
	source->transmitFile(u->getReadStream());
	fly_fire1(UploadManagerListener::Starting(), u);
}

void UploadManager::on(AdcCommand::GET, UserConnection* source, const AdcCommand& c) noexcept
{
	if (ClientManager::isBeforeShutdown())
	{
		return;
	}
	if (source->getState() != UserConnection::STATE_GET)
	{
		LogManager::message("GET ignored in state " + Util::toString(source->getState()) + ", p=" + Util::toHexString(source), false);
		return;
	}
	
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

	if (prepareFile(source, type, fname, hideShare, shareGroup, startPos, bytes, c.hasFlag("RE", 4)))
	{
		auto u = source->getUpload();
		dcassert(u != nullptr);
		AdcCommand cmd(AdcCommand::CMD_SND);
		cmd.addParam(type);
		cmd.addParam(fname);
		cmd.addParam(Util::toString(u->getStartPos()));
		cmd.addParam(Util::toString(u->getSize()));
		
		if (SETTING(MAX_COMPRESSION) && c.hasFlag("ZL", 4) && !isCompressedFile(u.get()))
		{
			try
			{
				u->setReadStream(new FilteredInputStream<ZFilter, true>(u->getReadStream()));
				u->setFlag(Upload::FLAG_ZUPLOAD);
				cmd.addParam("ZL1");
			}
			catch (Exception& e)
			{
				const string message = "Error UploadManager::on(AdcCommand::GET) path:" + u->getPath() + ", error: " + e.getError();
				LogManager::message(message);
				fly_fire2(UploadManagerListener::Failed(), u, message);
				return;
			}
		}

		string downloadedBytesStr;
		if (c.getParam("DB", 4, downloadedBytesStr))
		{
			int64_t downloadedBytes = Util::toInt64(downloadedBytesStr);
			if (downloadedBytes >= 0)
				u->setDownloadedBytes(downloadedBytes);
		}

		source->send(cmd);
		
		u->setStartTime(source->getLastActivity());
		source->setState(UserConnection::STATE_RUNNING);
		source->transmitFile(u->getReadStream());
		fly_fire1(UploadManagerListener::Starting(), u);
	}
	else
	{
		auto u = source->getUpload();
		if (u)
		{
			if (type == Transfer::fileTypeNames[Transfer::TYPE_FILE])
				u->setType(Transfer::TYPE_FILE);
			fly_fire2(UploadManagerListener::Failed(), u, STRING(UNABLE_TO_SEND_FILE));
		}
		else
			ConnectionManager::getInstance()->fireUploadError(source->getHintedUser(), STRING(UNABLE_TO_SEND_FILE), source->getConnectionQueueToken());
	}
}

void UploadManager::on(UserConnectionListener::Failed, UserConnection* source, const string& aError) noexcept
{
	auto u = source->getUpload();
	
	if (u)
	{
		fly_fire2(UploadManagerListener::Failed(), u, aError);
		
		dcdebug("UM::onFailed (%s): Removing upload\n", aError.c_str());
		removeUpload(u);
	}
	
	removeConnection(source);
}

void UploadManager::on(UserConnectionListener::TransmitDone, UserConnection* source) noexcept
{
	//dcassert(!ClientManager::isBeforeShutdown());
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

void UploadManager::logUpload(const UploadPtr& aUpload)
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (BOOLSETTING(LOG_UPLOADS) && aUpload->getType() != Transfer::TYPE_TREE && (BOOLSETTING(LOG_FILELIST_TRANSFERS) || aUpload->getType() != Transfer::TYPE_FULL_LIST))
		{
			StringMap params;
			aUpload->getParams(params);
			LOG(UPLOAD, params);
		}
		fly_fire1(UploadManagerListener::Complete(), aUpload);
	}
}

size_t UploadManager::addFailedUpload(const UserConnection* source, const string& file, int64_t pos, int64_t size, uint16_t flags)
{
	dcassert(!ClientManager::isBeforeShutdown());
	size_t queuePosition = 0;
	
	UploadQueueFilePtr uqi;
	HintedUser hintedUser = source->getHintedUser();
	{
		LOCK(csQueue);
		auto it = std::find_if(slotQueue.begin(), slotQueue.end(), [&](const UserPtr& u) -> bool { ++queuePosition; return u == source->getUser(); });
		if (it != slotQueue.end())
		{
			it->setToken(source->getConnectionQueueToken());
			for (auto i = it->waitingFiles.cbegin(); i != it->waitingFiles.cend(); ++i)
				if ((*i)->getFile() == file)
				{
					(*i)->setPos(pos);
					(*i)->setFlags(flags);
					return queuePosition;
				}
		}
		uqi.reset(new UploadQueueFile(file, pos, size, flags));
		if (it == slotQueue.end())
		{
			++queuePosition;
			slotQueue.push_back(WaitingUser(hintedUser, source->getConnectionQueueToken(), uqi));
		}
		else
			it->waitingFiles.push_back(uqi);
	}
	if (g_count_WaitingUsersFrame)
		fly_fire1(UploadManagerListener::QueueAdd(), hintedUser, uqi);
	return queuePosition;
}

void UploadManager::clearWaitingFilesL(const WaitingUser& wu)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (g_count_WaitingUsersFrame)
		for (const UploadQueueFilePtr& uqi : wu.waitingFiles)
			fly_fire1(UploadManagerListener::QueueItemRemove(), wu.hintedUser, uqi);
}

void UploadManager::clearUserFilesL(const UserPtr& user)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	auto it = std::find_if(slotQueue.cbegin(), slotQueue.cend(), [&](const UserPtr & u)
	{
		return u == user;
	});
	if (it != slotQueue.end())
	{
		clearWaitingFilesL(*it);
		if (g_count_WaitingUsersFrame && !ClientManager::isBeforeShutdown())
		{
			fly_fire1(UploadManagerListener::QueueRemove(), user);
		}
		slotQueue.erase(it);
	}
}

void UploadManager::addConnection(UserConnection* conn)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (conn->isIpBlocked(false))
	{
		removeConnection(conn, false);
		return;
	}
	conn->addListener(this);
	conn->setState(UserConnection::STATE_GET);
}

void UploadManager::testSlotTimeout(uint64_t tick /*= GET_TICK()*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
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
		for (UserPtr user : users)
			um->fireReservedSlotChanged(user);
	}
}

void UploadManager::processSlot(UserConnection::SlotTypes slotType, int delta)
{
	switch (slotType)
	{
		case UserConnection::STDSLOT:
			g_running += delta;
			break;
		case UserConnection::EXTRASLOT:
			extra += delta;
			break;
		case UserConnection::PARTIALSLOT:
			extraPartial += delta;
			break;
	}
}

void UploadManager::removeConnection(UserConnection* source, bool removeListener /*= true */)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	//dcassert(source->getUpload() == nullptr);
	if (removeListener)
		source->removeListener(this);
	processSlot(source->getSlotType(), -1);
	source->setSlotType(UserConnection::NOSLOT);
}

void UploadManager::notifyQueuedUsers(int64_t tick)
{
	dcassert(!ClientManager::isBeforeShutdown());
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
				clearWaitingFilesL(wu);
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
			ClientManager::getInstance()->connect(wu.hintedUser, wu.getToken(), false);
		if (g_count_WaitingUsersFrame)
			fly_fire1(UploadManagerListener::QueueRemove(), wu.getUser());
	}
}

void UploadManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	UserList disconnects;
	{
#ifdef FLYLINKDC_USE_DOS_GUARD
		{
			LOCK(csDos);
			m_dos_map.clear();
		}
#endif
		testSlotTimeout(tick);
		
		{
			LOCK(csQueue);
			
			for (auto i = notifiedUsers.cbegin(); i != notifiedUsers.cend();)
			{
				if (i->second + 90 * 1000 < tick)
				{
					clearUserFilesL(i->first);
					notifiedUsers.erase(i++);
				}
				else
					++i;
			}
		}
		
		if (BOOLSETTING(AUTO_KICK))
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
					bool unused;
					if (BOOLSETTING(AUTO_KICK_NO_FAVS) && FavoriteManager::getInstance()->isFavoriteUser(u->getUser(), unused))
					{
						continue;
					}
					u->setFlag(Upload::FLAG_PENDING_KICK);
				}
			}
		}
	}
	
	for (auto i = disconnects.cbegin(); i != disconnects.cend(); ++i)
	{
		LogManager::message(STRING(DISCONNECTED_USER) + ' ' + Util::toString(ClientManager::getNicks((*i)->getCID(), Util::emptyString)));
		ConnectionManager::disconnect(*i, false);
	}
	
	lastFreeSlots = getFreeSlots();
}

void UploadManager::on(GetListLength, UserConnection* conn) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	conn->error("GetListLength not supported");
	conn->disconnect(false);
}

void UploadManager::on(AdcCommand::GFI, UserConnection* source, const AdcCommand& c) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (source->getState() != UserConnection::STATE_GET)
	{
		LogManager::message("GFI ignored in state " + Util::toString(source->getState()) + ", p=" + Util::toHexString(source), false);
		return;
	}
	
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

// TimerManagerListener
void UploadManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
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
			u->getUserConnection()->getSocket()->updateSocketBucket(u->getUser()->getUploadCount(), tick);
		}
	}
	if (!tickList.empty())
		fly_fire1(UploadManagerListener::Tick(), tickList);
	notifyQueuedUsers(tick);
	
	if (g_count_WaitingUsersFrame)
		fly_fire(UploadManagerListener::QueueUpdate());
	
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
	//dcassert(!ClientManager::isBeforeShutdown());
	if (!user->isOnline())
	{
		LOCK(csQueue);
		clearUserFilesL(user);
	}
}

void UploadManager::removeFinishedUpload(const UserPtr& user)
{
	//dcassert(!ClientManager::isBeforeShutdown());
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
	//dcassert(!ClientManager::isBeforeShutdown());
	bool nowait = true;
	{
		READ_LOCK(*csFinishedUploads);
		for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
		{
			auto u = *i;
			if (u->getPath() == fileName)
			{
				u->getUserConnection()->disconnect(true);
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
	DatabaseManager::getInstance()->saveRegistry(values, e_ExtraSlot, true);
}

void UploadManager::load()
{
	DBRegistryMap values;
	DatabaseManager::getInstance()->loadRegistry(values, e_ExtraSlot);
	uint64_t currentTick = GET_TICK();
	int64_t currentTime = (int64_t) GET_TIME();
	for (auto k = values.cbegin(); k != values.cend(); ++k)
	{
		auto user = ClientManager::createUser(CID(k->first), Util::emptyString, Util::emptyString);
		WRITE_LOCK(*csReservedSlots);
		int64_t timeout = k->second.ival;
		if (timeout > currentTime)
			reservedSlots[user] = (timeout-currentTime)*1000 + currentTick;
	}
	testSlotTimeout();
}
