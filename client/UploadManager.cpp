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

#include "UploadManager.h"
#include "DownloadManager.h"
#include "ConnectionManager.h"
#include "ShareManager.h"
#include "CFlylinkDBManager.h"
#include "CryptoManager.h"
#include "Upload.h"
#include "QueueManager.h"
#include "FinishedManager.h"
#include "PGLoader.h"
#include "SharedFileStream.h"
#include "IPGrant.h"
#include "Wildcards.h"

static const unsigned WAIT_TIME_LAST_CHUNK  = 3000;
static const unsigned WAIT_TIME_OTHER_CHUNK = 8000;

#ifdef _DEBUG
boost::atomic_int UploadQueueItem::g_upload_queue_item_count(0);
#endif
uint32_t UploadManager::g_count_WaitingUsersFrame = 0;
UploadManager::SlotMap UploadManager::g_reservedSlots;
int UploadManager::g_running = 0;
UploadList UploadManager::g_uploads;
CurrentConnectionMap UploadManager::g_uploadsPerUser;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
UploadManager::BanMap UploadManager::g_lastBans;
std::unique_ptr<webrtc::RWLockWrapper> UploadManager::g_csBans = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif
std::unique_ptr<webrtc::RWLockWrapper> UploadManager::g_csReservedSlots = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
int64_t UploadManager::g_runningAverage;

UploadManager::UploadManager() noexcept :
	extra(0), lastGrant(0), lastFreeSlots(-1),
	fireballStartTick(0), fileServerCheckTick(0), isFireball(false), isFileServer(false), extraPartial(0)
{
	csFinishedUploads = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
	ClientManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);
}

UploadManager::~UploadManager()
{
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	{
		CFlyLock(csQueue); // [!] IRainman opt.
		slotQueue.clear(); // TODO - унести зачистку раньше в метод shutdown
	}
	while (true)
	{
		{
			CFlyReadLock(*csFinishedUploads);
			if (g_uploads.empty())
				break;
		}
		Thread::sleep(10);
	}
	dcassert(g_uploadsPerUser.empty());
}

void UploadManager::initTransferData(TransferData& td, const Upload* u)
{
	td.token = u->getConnectionQueueToken();
	td.hintedUser = u->getHintedUser();
	td.pos = u->getStartPos() + u->getPos();
	td.actual = u->getStartPos() + u->getActual();
	td.secondsLeft = u->getSecondsLeft(true);
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
bool UploadManager::handleBan(UserConnection* aSource/*, bool forceBan, bool noChecks*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const UserPtr& user = aSource->getUser();
	if (!user->isOnline()) // if not online, cheat (connection without hub)
	{
		aSource->disconnect();
		return true;
	}
	bool l_is_ban = false;
	const bool l_is_favorite = FavoriteManager::isFavoriteUser(user, l_is_ban);
	const auto banType = user->hasAutoBan(nullptr, l_is_favorite);
	bool banByRules = banType != User::BAN_NONE;
	if (banByRules)
	{
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* hub = fm->getFavoriteHubEntryPtr(aSource->getHubUrl());
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
	// old const bool sendStatus = aSource->isSet(UserConnection::FLAG_SUPPORTS_BANMSG);
	
	if (!BOOLSETTING(AUTOBAN_STEALTH))
	{
		aSource->error(banstr);
		
		if (BOOLSETTING(AUTOBAN_SEND_PM))
		{
			// [!] IRainman fix.
			msg.tick = TimerManager::getTick();
			const auto pmMsgPeriod = SETTING(AUTOBAN_MSG_PERIOD);
			const auto key = user->getCID().toBase32();
			bool sendPm;
			{
				CFlyWriteLock(*g_csBans); // [+] IRainman opt.
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
				ClientManager::privateMessage(aSource->getHintedUser(), banstr, false);
			}
			// [~] IRainman fix.
		}
	}
	
//	if (BOOLSETTING(BAN_STEALTH)) aSource->maxedOut();

	return true;
}
// !SMT!-S
bool UploadManager::isBanReply(const UserPtr& user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const auto key = user->getCID().toBase32();
	{
		CFlyReadLock(*g_csBans); // [+] IRainman opt.
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
		const auto newLeecherIp = newLeecher->getSocket()->getIp();
		const auto newLeecherShare = newLeecher->getUser()->getBytesShared();
		const auto newLeecherNick = newLeecher->getUser()->getLastNick();
		
		CFlyReadLock(*csFinishedUploads);
		
		for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
		{
			const auto u = *i;
			dcassert(u);
			dcassert(u->getUser());
			const auto uploadUserIp = u->getUserConnection()->getSocket()->getIp(); // TODO - boost
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

bool UploadManager::prepareFile(UserConnection* aSource, const string& aType, const string& aFile, int64_t aStartPos, int64_t& aBytes, bool listRecursive)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcdebug("Preparing %s %s " I64_FMT " " I64_FMT " %d\n", aType.c_str(), aFile.c_str(), aStartPos, aBytes, listRecursive);
	if (ClientManager::isBeforeShutdown())
	{
		return false;
	}
	if (aFile.empty() || aStartPos < 0 || aBytes < -1 || aBytes == 0)
	{
		aSource->fileNotAvail("Invalid request");
		return false;
	}
	const auto ipAddr = aSource->getRemoteIp();
	const auto cipherName = aSource->getCipherName();
	const bool isTypeTree = aType == Transfer::g_type_names[Transfer::TYPE_TREE];
	const bool isTypePartialList = aType == Transfer::g_type_names[Transfer::TYPE_PARTIAL_LIST];
#ifdef FLYLINKDC_USE_DOS_GUARD
	if (isTypeTree || isTypePartialList)
	{
		const HintedUser& l_User = aSource->getHintedUser();
		if (l_User.user)
		{
			// [!] IRainman opt: CID on ADC hub is unique to all hubs,
			// in NMDC it is obtained from the hash of username and hub address (computed locally).
			// There is no need to use any additional data, but it is necessary to consider TTH of the file.
			// This ensures the absence of a random user locks sitting on several ADC hubs,
			// because the number of connections from a single user much earlier limited in the ConnectionManager.
			const string l_hash_key = Util::toString(l_User.user->getCID().toHash()) + aFile;
			uint8_t l_count_dos;
			{
				CFlyFastLock(csDos);
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
					          aFile.c_str());
					LogManager::ddos_message(l_buf);
				}
#ifdef IRAINMAN_DISALLOWED_BAN_MSG
				if (aSource->isSet(UserConnection::FLAG_SUPPORTS_BANMSG))
				{
					aSource->error(UserConnection::g_PLEASE_UPDATE_YOUR_CLIENT);
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
					LogManager::message("Disconnect bug client: $ADCGET / $ADCSND User: " + l_User.user->getLastNick() + " Hub:" + aSource->getHubUrl());
					aSource->disconnect();
					
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
	
	const bool isFileList = (aFile == Transfer::g_user_list_name_bz || aFile == Transfer::g_user_list_name);
	bool isFree = isFileList;
	bool isPartial = false;
	
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	bool isHidingShare;
	if (aSource->getUser())
	{
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(aSource->getHintedUser().hint);
		isHidingShare = fhe && fhe->getHideShare();
		fm->releaseFavoriteHubEntryPtr(fhe);
	}
	else
		isHidingShare = false;
#endif
		
	string sourceFile;
	const bool isTypeFile = aType == Transfer::g_type_names[Transfer::TYPE_FILE];
	const bool isTTH  = (isTypeFile || isTypeTree) && aFile.size() == 43 && aFile.compare(0, 4, "TTH/", 4) == 0;
	Transfer::Type type;
	TTHValue tth;
	if (isTTH)
	{
		tth = TTHValue(aFile.c_str() + 4);
	}
	try
	{
		if (isTypeFile && !ClientManager::isBeforeShutdown() && ShareManager::isValidInstance())
		{
			sourceFile = isTTH ? 
				ShareManager::getInstance()->getFilePathByTTH(tth) :
				ShareManager::getInstance()->getFilePath(aFile
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
					, isHidingShare
#endif
				);
			                                                
			if (aFile == Transfer::g_user_list_name)
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
				
				start = aStartPos;
				fileSize = f->getSize();
				size = aBytes == -1 ? fileSize - start : aBytes;
				
				if (size < 0 || size > fileSize || start + size > fileSize)
				{
					aSource->fileNotAvail();
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
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			if (isHidingShare)
			{
				aSource->fileNotAvail();
				return false;
			}
#endif
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
				mis = ShareManager::getInstance()->getTree(aFile);
			if (!mis)
			{
				aSource->fileNotAvail();
				return false;
			}
			
			sourceFile = aFile;
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			isFree = true;
			type = Transfer::TYPE_TREE;
		}
		else if (isTypePartialList)
		{
			// Partial file list
			MemoryInputStream* mis = ShareManager::getInstance()->generatePartialList(aFile, listRecursive
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			                                                                          , isHidingShare
#endif
			                                                                         );
			if (!mis)
			{
				aSource->fileNotAvail();
				return false;
			}
			
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			isFree = true;
			type = Transfer::TYPE_PARTIAL_LIST;
		}
		else
		{
			aSource->fileNotAvail("Unknown file type");
			return false;
		}
	}
	catch (const ShareException& e)
	{
		// Partial file sharing upload
		if (isTTH)
		{
			if (QueueManager::isChunkDownloaded(tth, aStartPos, aBytes, sourceFile))
			{
				dcassert(!sourceFile.empty());
				if (!sourceFile.empty())
				{
					try
					{
						auto f = new SharedFileStream(sourceFile, File::READ, File::OPEN | File::SHARED | File::NO_CACHE_HINT, 0);
						
						start = aStartPos;
						fileSize = f->getFastFileSize();
						size = aBytes == -1 ? fileSize - start : aBytes;
						
						if (size < 0 || size > fileSize || start + size > fileSize)
						{
							aSource->fileNotAvail();
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
						safe_delete(is);
					}
				}
			}
		}
		aSource->fileNotAvail(e.getError());
		return false;
	}
	catch (const Exception& e)
	{
		LogManager::message(STRING(UNABLE_TO_SEND_FILE) + ' ' + sourceFile + ": " + e.getError());
		aSource->fileNotAvail();
		return false;
	}
	
ok:

	auto slotType = aSource->getSlotType();
	
	//[!] IRainman autoban fix: please check this code after merge
	bool hasReserved;
	{
		CFlyReadLock(*g_csReservedSlots);
		hasReserved = g_reservedSlots.find(aSource->getUser()) != g_reservedSlots.end();
	}
	if (!hasReserved)
	{
		hasReserved = BOOLSETTING(EXTRA_SLOT_TO_DL) && DownloadManager::checkFileDownload(aSource->getUser());// !SMT!-S
	}
	
	const bool isFavorite = FavoriteManager::hasAutoGrantSlot(aSource->getUser())
#ifdef IRAINMAN_ENABLE_AUTO_BAN
# ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	                          || (SETTING(DONT_BAN_OP) && (aSource->getUser()->getFlags() & User::OPERATOR))
# endif
#endif
	                          ;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	// !SMT!-S
	if (!isFavorite && SETTING(ENABLE_AUTO_BAN))
	{
		// фавориты под автобан не попадают
		if (!isFileList && !hasReserved && handleBan(aSource))
		{
			delete is;
			addFailedUpload(aSource, sourceFile, aStartPos, size);
			aSource->disconnect();
			return false;
		}
	}
#endif // IRAINMAN_ENABLE_AUTO_BAN
	
	if (slotType != UserConnection::STDSLOT || hasReserved)
	{
		//[-] IRainman autoban fix: please check this code after merge
		//bool hasReserved = reservedSlots.find(aSource->getUser()) != reservedSlots.end();
		//bool isFavorite = FavoriteManager::getInstance()->hasSlot(aSource->getUser());
		bool hasFreeSlot = getFreeSlots() > 0;
		if (hasFreeSlot)
		{
			CFlyLock(csQueue);
			hasFreeSlot = (slotQueue.empty() && notifiedUsers.empty() || (notifiedUsers.find(aSource->getUser()) != notifiedUsers.end()));
		}
		
		bool isAutoSlot = getAutoSlot();
		bool isHasUpload = hasUpload(aSource);
		
#ifdef SSA_IPGRANT_FEATURE // SSA - additional slots for special IP's
		bool hasSlotByIP = false;
		if (BOOLSETTING(EXTRA_SLOT_BY_IP))
		{
			if (!(hasReserved || isFavorite || isAutoSlot || hasFreeSlot || isHasUpload))
			{
				hasSlotByIP = IpGrant::check(Socket::convertIP4(aSource->getRemoteIp()));
			}
		}
#endif // SSA_IPGRANT_FEATURE
		if (!(hasReserved || isFavorite || isAutoSlot || hasFreeSlot
#ifdef SSA_IPGRANT_FEATURE
		        || hasSlotByIP
#endif
		     )
		        || isHasUpload)
		{
			const bool supportsFree = aSource->isSet(UserConnection::FLAG_SUPPORTS_MINISLOTS);
			const bool allowedFree = slotType == UserConnection::EXTRASLOT
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
			                              || aSource->isSet(UserConnection::FLAG_OP)
#endif
			                              || getFreeExtraSlots() > 0;
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
				safe_delete(is);
				aSource->maxedOut(addFailedUpload(aSource, sourceFile, aStartPos, fileSize)); // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=130703
				aSource->disconnect();
				return false;
			}
		}
		else
		{
#ifdef SSA_IPGRANT_FEATURE
			if (hasSlotByIP)
				LogManager::message("IpGrant: " + STRING(GRANTED_SLOT_BY_IP) + ' ' + aSource->getRemoteIp());
#endif
			slotType = UserConnection::STDSLOT;
		}
		
		setLastGrant(GET_TICK());
	}
	
	{
		CFlyLock(csQueue);
		
		// remove file from upload queue
		clearUserFilesL(aSource->getUser());
		
		// remove user from notified list
		const auto cu = notifiedUsers.find(aSource->getUser());
		if (cu != notifiedUsers.end())
		{
			notifiedUsers.erase(cu);
		}
	}
	
	bool resumed = false;
	{
		CFlyWriteLock(*csFinishedUploads);
		for (auto i = finishedUploads.cbegin(); i != finishedUploads.cend(); ++i)
		{
			auto up = *i;
			if (aSource == up->getUserConnection())
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
	UploadPtr u = std::make_shared<Upload>(aSource, tth, sourceFile, ipAddr, cipherName);
	u->setReadStream(is);
	u->setSegment(Segment(start, size));
	aSource->setUpload(u);
	
	if (u->getSize() != fileSize)
		u->setFlag(Upload::FLAG_CHUNKED);
		
	if (resumed)
		u->setFlag(Upload::FLAG_RESUMED);
		
	if (isPartial)
		u->setFlag(Upload::FLAG_UPLOAD_PARTIAL);
		
	u->setFileSize(fileSize);
	u->setType(type);
	
	{
		CFlyWriteLock(*csFinishedUploads);
		g_uploads.push_back(u);
		increaseUserConnectionAmountL(u->getUser());
	}
	
	if (aSource->getSlotType() != slotType)
	{
		// remove old count
		process_slot(aSource->getSlotType(), -1);
		// set new slot count
		process_slot(slotType, 1);
		// user got a slot
		aSource->setSlotType(slotType);
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
	{
		CFlyWriteLock(*csFinishedUploads);
		g_uploadsPerUser.clear();
	}
	{
		CFlyWriteLock(*g_csReservedSlots);
		g_reservedSlots.clear();
	}
}

void UploadManager::increaseUserConnectionAmountL(const UserPtr& p_user)
{
	if (!ClientManager::isBeforeShutdown())
	{
		const auto i = g_uploadsPerUser.find(p_user);
		if (i != g_uploadsPerUser.end())
		{
			i->second++;
		}
		else
		{
			g_uploadsPerUser.insert(CurrentConnectionPair(p_user, 1));
		}
	}
}

void UploadManager::decreaseUserConnectionAmountL(const UserPtr& p_user)
{
	if (!ClientManager::isBeforeShutdown())
	{
		const auto i = g_uploadsPerUser.find(p_user);
		//dcassert(i != g_uploadsPerUser.end());
		if (i != g_uploadsPerUser.end())
		{
			i->second--;
			if (i->second == 0)
			{
				g_uploadsPerUser.erase(p_user);
			}
		}
	}
}

unsigned int UploadManager::getUserConnectionAmountL(const UserPtr& p_user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const auto i = g_uploadsPerUser.find(p_user);
	if (i != g_uploadsPerUser.end())
	{
		return i->second;
	}
	
	dcassert(0);
	return 1;
}

void UploadManager::removeUpload(UploadPtr& upload, bool delay)
{
	UploadArray tickList;
	//dcassert(!ClientManager::isBeforeShutdown());
	{
		CFlyWriteLock(*csFinishedUploads);
		if (!g_uploads.empty())
			g_uploads.erase(remove(g_uploads.begin(), g_uploads.end(), upload), g_uploads.end());
		decreaseUserConnectionAmountL(upload->getUser());
	
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

void UploadManager::reserveSlot(const HintedUser& hintedUser, uint64_t aTime)
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		CFlyWriteLock(*g_csReservedSlots);
		g_reservedSlots[hintedUser.user] = GET_TICK() + aTime * 1000;
	}
	save();
	if (hintedUser.user->isOnline())
	{
		CFlyLock(csQueue);
		// find user in uploadqueue to connect with correct token
		auto it = std::find_if(slotQueue.cbegin(), slotQueue.cend(), [&](const UserPtr & u)
		{
			return u == hintedUser.user;
		});
		if (it != slotQueue.cend())
		{
			bool unused;
			ClientManager::getInstance()->connect(hintedUser, it->getToken(), false, unused);
		}/* else {
            token = Util::toString(Util::rand());
        }*/
	}
	
	UserManager::getInstance()->fireReservedSlotChanged(hintedUser.user);
	if (BOOLSETTING(SEND_SLOTGRANT_MSG))
	{
		ClientManager::privateMessage(hintedUser, "+me " + STRING(SLOT_GRANTED_MSG) + ' ' + Util::formatSeconds(aTime), false); // !SMT!-S
	}
}

void UploadManager::unreserveSlot(const HintedUser& hintedUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		CFlyWriteLock(*g_csReservedSlots);
		if (!g_reservedSlots.erase(hintedUser.user)) return;
	}
	save();
	UserManager::getInstance()->fireReservedSlotChanged(hintedUser.user);
	if (BOOLSETTING(SEND_SLOTGRANT_MSG))
	{
		ClientManager::privateMessage(hintedUser, "+me " + STRING(SLOT_REMOVED_MSG), false);
	}
}

void UploadManager::on(UserConnectionListener::Get, UserConnection* aSource, const string& aFile, int64_t aResume) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
	{
		return;
	}
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		dcdebug("UM::onGet Bad state, ignoring\n");
		return;
	}
	
	int64_t bytes = -1;
	if (prepareFile(aSource, Transfer::g_type_names[Transfer::TYPE_FILE], Util::toAdcFile(aFile), aResume, bytes))
	{
		aSource->setState(UserConnection::STATE_SEND);
		aSource->fileLength(Util::toString(aSource->getUpload()->getSize()));
	}
}

void UploadManager::on(UserConnectionListener::Send, UserConnection* aSource) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
	{
		return;
	}
	if (aSource->getState() != UserConnection::STATE_SEND)
	{
		LogManager::message("Send ignored in state " + Util::toString(aSource->getState()) + ", p=" + Util::toHexString(aSource), false);
		//dcdebug("UM::onSend Bad state, ignoring\n");
		return;
	}
	
	auto u = aSource->getUpload();
	dcassert(u != nullptr);
	u->setStartTime(aSource->getLastActivity());
	
	aSource->setState(UserConnection::STATE_RUNNING);
	aSource->transmitFile(u->getReadStream());
	fly_fire1(UploadManagerListener::Starting(), u);
}

void UploadManager::on(AdcCommand::GET, UserConnection* aSource, const AdcCommand& c) noexcept
{
	if (ClientManager::isBeforeShutdown())
	{
		//dcassert(0);
		return;
	}
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		LogManager::message("GET ignored in state " + Util::toString(aSource->getState()) + ", p=" + Util::toHexString(aSource), false);
		//dcassert(0);
		return;
	}
	
	const string& type = c.getParam(0);
	const string& fname = c.getParam(1);
	int64_t aStartPos = Util::toInt64(c.getParam(2));
	int64_t aBytes = Util::toInt64(c.getParam(3));
#ifdef _DEBUG
//	LogManager::message("on(AdcCommand::GET aStartPos = " + Util::toString(aStartPos) + " aBytes = " + Util::toString(aBytes));
#endif

	if (prepareFile(aSource, type, fname, aStartPos, aBytes, c.hasFlag("RE", 4)))
	{
		auto u = aSource->getUpload();
		dcassert(u != nullptr);
		AdcCommand cmd(AdcCommand::CMD_SND);
		cmd.addParam(type);
		cmd.addParam(fname);
		cmd.addParam(Util::toString(u->getStartPos()));
		cmd.addParam(Util::toString(u->getSize()));
		
		if (SETTING(MAX_COMPRESSION) && !ZFilter::g_is_disable_compression &&
		    c.hasFlag("ZL", 4) && !isCompressedFile(u.get()))
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
		aSource->send(cmd);
		
		u->setStartTime(aSource->getLastActivity());
		aSource->setState(UserConnection::STATE_RUNNING);
		aSource->transmitFile(u->getReadStream());
		fly_fire1(UploadManagerListener::Starting(), u);
	}
}

void UploadManager::on(UserConnectionListener::Failed, UserConnection* aSource, const string& aError) noexcept
{
	auto u = aSource->getUpload();
	
	if (u)
	{
		fly_fire2(UploadManagerListener::Failed(), u, aError);
		
		dcdebug("UM::onFailed (%s): Removing upload\n", aError.c_str());
		removeUpload(u);
	}
	
	removeConnection(aSource);
}

void UploadManager::on(UserConnectionListener::TransmitDone, UserConnection* aSource) noexcept
{
	//dcassert(!ClientManager::isBeforeShutdown());
	dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
	auto u = aSource->getUpload();
	dcassert(u != nullptr);
	u->tick(aSource->getLastActivity());
	
	aSource->setState(UserConnection::STATE_GET);
	
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

size_t UploadManager::addFailedUpload(const UserConnection* aSource, const string& file, int64_t pos, int64_t size)
{
	dcassert(!ClientManager::isBeforeShutdown());
	size_t queue_position = 0;
	
	CFlyLock(csQueue); // [+] IRainman opt.
	
	auto it = std::find_if(slotQueue.begin(), slotQueue.end(), [&](const UserPtr & u) -> bool { ++queue_position; return u == aSource->getUser(); });
	if (it != slotQueue.end())
	{
		it->setToken(aSource->getUserConnectionToken());
		// https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=130703
		for (auto i = it->m_waiting_files.cbegin(); i != it->m_waiting_files.cend(); ++i) //TODO https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=128318
		{
			if ((*i)->getFile() == file)
			{
				(*i)->setPos(pos);
				return queue_position;
			}
		}
	}
	UploadQueueItemPtr uqi(new UploadQueueItem(aSource->getHintedUser(), file, pos, size));
	if (it == slotQueue.end())
	{
		++queue_position;
		slotQueue.push_back(WaitingUser(aSource->getHintedUser(), aSource->getUserConnectionToken(), uqi));
	}
	else
	{
		it->m_waiting_files.push_back(uqi);
	}
	// Crash https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=29270
	if (g_count_WaitingUsersFrame)
	{
		fly_fire1(UploadManagerListener::QueueAdd(), uqi);
	}
	return queue_position;
}

void UploadManager::clearWaitingFilesL(const WaitingUser& p_wu)
{
	dcassert(!ClientManager::isBeforeShutdown());
	for (auto i = p_wu.m_waiting_files.cbegin(); i != p_wu.m_waiting_files.cend(); ++i)
	{
		if (g_count_WaitingUsersFrame)
		{
			fly_fire1(UploadManagerListener::QueueItemRemove(), (*i));
		}
	}
}

void UploadManager::clearUserFilesL(const UserPtr& aUser)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	auto it = std::find_if(slotQueue.cbegin(), slotQueue.cend(), [&](const UserPtr & u)
	{
		return u == aUser;
	});
	if (it != slotQueue.end())
	{
		clearWaitingFilesL(*it);
		if (g_count_WaitingUsersFrame && !ClientManager::isBeforeShutdown())
		{
			fly_fire1(UploadManagerListener::QueueRemove(), aUser);
		}
		slotQueue.erase(it);
	}
}

void UploadManager::addConnection(UserConnection* conn)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (conn->isIPGuard(ResourceManager::BLOCKED_INCOMING_CONN, false))
	{
		removeConnection(conn, false);
		return;
	}
	conn->addListener(this);
	conn->setState(UserConnection::STATE_GET);
}

// TODO: ReservedSlotChanged must fire when slot expires
void UploadManager::testSlotTimeout(uint64_t tick /*= GET_TICK()*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	CFlyWriteLock(*g_csReservedSlots);
	for (auto j = g_reservedSlots.cbegin(); j != g_reservedSlots.cend();)
	{
		if (j->second < tick)
			g_reservedSlots.erase(j++);
		else
			++j;
	}
}

void UploadManager::process_slot(UserConnection::SlotTypes p_slot_type, int p_delta)
{
	switch (p_slot_type)
	{
		case UserConnection::STDSLOT:
			g_running += p_delta;
			break;
		case UserConnection::EXTRASLOT:
			extra += p_delta;
			break;
		case UserConnection::PARTIALSLOT:
			extraPartial += p_delta;
			break;
	}
}

void UploadManager::removeConnection(UserConnection* aSource, bool p_is_remove_listener /*= true */)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	//dcassert(aSource->getUpload() == nullptr);
	if (p_is_remove_listener)
	{
		aSource->removeListener(this);
	}
	process_slot(aSource->getSlotType(), -1);
	aSource->setSlotType(UserConnection::NOSLOT);
}

void UploadManager::notifyQueuedUsers(int64_t tick)
{
	dcassert(!ClientManager::isBeforeShutdown());
	// Сверху лочится через csQueue
	if (slotQueue.empty())
		return; //no users to notify
	vector<WaitingUser> notifyList;
	{
		CFlyLock(csQueue); // [+] IRainman opt.
		int freeslots = getFreeSlots();
		if (freeslots > 0)
		{
			freeslots -= notifiedUsers.size();
			while (freeslots > 0 && !slotQueue.empty())
			{
				// let's keep him in the connectingList until he asks for a file
				const WaitingUser& wu = slotQueue.front(); // TODO -  https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=128150
				//         https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=56833
				clearWaitingFilesL(wu);
				if (g_count_WaitingUsersFrame)
				{
					fly_fire1(UploadManagerListener::QueueRemove(), wu.getUser()); // TODO унести из лока?
				}
				if (wu.getUser()->isOnline())
				{
					notifiedUsers[wu.getUser()] = tick;
					notifyList.push_back(wu);
					freeslots--;
				}
				slotQueue.pop_front();
			}
		}
	}
	for (auto it = notifyList.cbegin(); it != notifyList.cend(); ++it)
	{
		bool unused;
		ClientManager::getInstance()->connect(it->m_hintedUser, it->getToken(), false, unused);
	}
	
}

void UploadManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	UserList disconnects;
	{
#ifdef FLYLINKDC_USE_DOS_GUARD
		{
			CFlyFastLock(csDos);
			m_dos_map.clear();
		}
#endif
		testSlotTimeout(aTick);
		
		{
			CFlyLock(csQueue);
			
			for (auto i = notifiedUsers.cbegin(); i != notifiedUsers.cend();)
			{
				if ((i->second + (90 * 1000)) < aTick)
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
			CFlyReadLock(*csFinishedUploads);
			
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
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
					if (BOOLSETTING(AUTO_KICK_NO_FAVS) && FavoriteManager::isFavoriteUser(u->getUser(), unused))
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

void UploadManager::on(AdcCommand::GFI, UserConnection* aSource, const AdcCommand& c) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		LogManager::message("GFI ignored in state " + Util::toString(aSource->getState()) + ", p=" + Util::toHexString(aSource), false);
		return;
	}
	
	if (c.getParameters().size() < 2)
	{
		aSource->send(AdcCommand(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_PROTOCOL_GENERIC, "Missing parameters"));
		return;
	}
	
	const string& type = c.getParam(0);
	const string& ident = c.getParam(1);
	
	if (type == Transfer::g_type_names[Transfer::TYPE_FILE])
	{
		AdcCommand cmd(AdcCommand::CMD_RES);
		if (ShareManager::getInstance()->getFileInfo(cmd, ident))
			aSource->send(cmd);
		else
			aSource->fileNotAvail();
	}
	else
	{
		aSource->fileNotAvail();
	}
}

// TimerManagerListener
void UploadManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;

	UploadArray tickList;
	{
		CFlyWriteLock(*csFinishedUploads);
		for (auto i = finishedUploads.cbegin(); i != finishedUploads.cend();)
		{
			auto u = *i;
			if (u)
			{
				if (aTick > u->getTickForRemove())
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
	tickList.reserve(g_uploads.size());
	{
		int64_t currentSpeed = 0;
		CFlyReadLock(*csFinishedUploads);
		for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
		{
			auto u = *i;
			if (u->getPos() > 0)
			{
				TransferData td;
				initTransferData(td, u.get());
				td.dumpToLog();					
				tickList.push_back(td);
				u->tick(aTick);
			}
			u->getUserConnection()->getSocket()->updateSocketBucket(getUserConnectionAmountL(u->getUser()));
			currentSpeed += u->getRunningAverage();
		}
		g_runningAverage = currentSpeed;
	}
	if (!tickList.empty())
		fly_fire1(UploadManagerListener::Tick(), tickList);
	notifyQueuedUsers(aTick);
	
	if (g_count_WaitingUsersFrame)
		fly_fire(UploadManagerListener::QueueUpdate());
	
	if (!isFireball)
	{
		if (getRunningAverage() >= 1024 * 1024)
		{
			if (fireballStartTick)
			{
				if (aTick - fireballStartTick > 60 * 1000)
				{
					isFireball = true;
					ClientManager::infoUpdated();
				}
			}
			else
				fireballStartTick = aTick;
		}
		else 
			fireballStartTick = 0;
		if (!isFireball && !isFileServer && aTick > fileServerCheckTick)
		{
			if ((Util::getUpTime() > 10 * 24 * 60 * 60) && // > 10 days uptime
			    (Socket::g_stats.m_tcp.totalUp > 100ULL * 1024 * 1024 * 1024) && // > 100 GiB uploaded
			    (ShareManager::getInstance()->getSharedSize() > 1.5 * 1024 * 1024 * 1024 * 1024)) // > 1.5 TiB shared
			{
				isFileServer = true;
				ClientManager::infoUpdated();
			}
			fileServerCheckTick = aTick + 300 * 1000;
		}
	}
}

void UploadManager::on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept
{
	//dcassert(!ClientManager::isBeforeShutdown());
	if (!aUser->isOnline())
	{
		CFlyLock(csQueue);  // [+] IRainman opt.
		clearUserFilesL(aUser);
	}
}

void UploadManager::removeFinishedUpload(const UserPtr& aUser)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	CFlyWriteLock(*csFinishedUploads);
	for (auto i = finishedUploads.cbegin(); i != finishedUploads.cend(); ++i)
	{
		auto up = *i;
		if (aUser == up->getUser())
		{
			finishedUploads.erase(i);
			break;
		}
	}
}

/**
 * Abort upload of specific file
 */
void UploadManager::abortUpload(const string& aFile, bool waiting)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	bool nowait = true;
	{
		CFlyReadLock(*csFinishedUploads);
		for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
		{
			auto u = *i;
			if (u->getPath() == aFile)
			{
				u->getUserConnection()->disconnect(true);
				nowait = false;
			}
		}
	}
	
	if (nowait) return;
	if (!waiting) return;
	
	for (int i = 0; i < 20 && nowait == false; i++)
	{
		Thread::sleep(100);
		{
			CFlyReadLock(*csFinishedUploads);
			nowait = true;
			for (auto j = g_uploads.cbegin(); j != g_uploads.cend(); ++j)
			{
				if ((*j)->getPath() == aFile)
				{
					dcdebug("upload %s is not removed\n", aFile.c_str());
					nowait = false;
					break;
				}
			}
		}
	}
	
	if (!nowait)
	{
		dcdebug("abort upload timeout %s\n", aFile.c_str());
	}
}

time_t UploadManager::getReservedSlotTime(const UserPtr& aUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	CFlyReadLock(*g_csReservedSlots);
	const auto j = g_reservedSlots.find(aUser);
	return j != g_reservedSlots.end() ? j->second : 0;
}

void UploadManager::save()
{
	DBRegistryMap values;
	{
		CFlyReadLock(*g_csReservedSlots);
		for (auto i = g_reservedSlots.cbegin(); i != g_reservedSlots.cend(); ++i)
		{
			values[i->first->getCID().toBase32()] = DBRegistryValue(i->second);
		}
	}
	CFlylinkDBManager::getInstance()->saveRegistry(values, e_ExtraSlot, true);
}

void UploadManager::load()
{
	DBRegistryMap values;
	CFlylinkDBManager::getInstance()->loadRegistry(values, e_ExtraSlot);
	for (auto k = values.cbegin(); k != values.cend(); ++k)
	{
		auto user = ClientManager::createUser(CID(k->first), "", 0);
		CFlyWriteLock(*g_csReservedSlots);
		g_reservedSlots[user] = uint32_t(k->second.ival);
	}
	testSlotTimeout();
}

int UploadQueueItem::compareItems(const UploadQueueItem* a, const UploadQueueItem* b, uint8_t col)
{
	dcassert(!ClientManager::isBeforeShutdown());
	//+BugMaster: small optimization; fix; correct IP sorting
	switch (col)
	{
		case COLUMN_FILE:
		case COLUMN_TYPE:
		case COLUMN_PATH:
		case COLUMN_NICK:
		case COLUMN_HUB:
			return stricmp(a->getText(col), b->getText(col));
		case COLUMN_TRANSFERRED:
			return compare(a->m_pos, b->m_pos);
		case COLUMN_SIZE:
			return compare(a->m_size, b->m_size);
		case COLUMN_ADDED:
		case COLUMN_WAITING:
			return compare(a->m_time, b->m_time);
		case COLUMN_SLOTS:
			return compare(a->getUser()->getSlots(), b->getUser()->getSlots());
		case COLUMN_SHARE:
			return compare(a->getUser()->getBytesShared(), b->getUser()->getBytesShared());
		case COLUMN_IP:
			return compare(Socket::convertIP4(Text::fromT(a->getText(col))), Socket::convertIP4(Text::fromT(b->getText(col))));
	}
	return stricmp(a->getText(col), b->getText(col));
}

void UploadQueueItem::update()
{
	dcassert(!ClientManager::isBeforeShutdown());
	
	const auto& user = getUser();
	string nick;
	boost::asio::ip::address_v4 ip;
	int64_t bytesShared;
	int slots;
	user->getInfo(nick, ip, bytesShared, slots);

	setText(COLUMN_FILE, Text::toT(Util::getFileName(getFile())));
	setText(COLUMN_TYPE, Text::toT(Util::getFileExtWithoutDot(getFile())));
	setText(COLUMN_PATH, Text::toT(Util::getFilePath(getFile())));
	setText(COLUMN_NICK, Text::toT(nick));
	setText(COLUMN_HUB, getHintedUser().user ? Text::toT(Util::toString(ClientManager::getHubNames(getHintedUser().user->getCID(), Util::emptyString))) : Util::emptyStringT);
	setText(COLUMN_TRANSFERRED, Util::formatBytesT(getPos()) + _T(" (") + Util::toStringT((double)getPos() * 100.0 / (double)getSize()) + _T("%)"));
	setText(COLUMN_SIZE, Util::formatBytesT(getSize()));
	setText(COLUMN_ADDED, Text::toT(Util::formatDigitalClock(getTime())));
	setText(COLUMN_WAITING, Util::formatSecondsW(GET_TIME() - getTime()));
	setText(COLUMN_SHARE, Util::formatBytesT(bytesShared));
	setText(COLUMN_SLOTS, Util::toStringT(slots)); 
	if (m_location.isNew() && !ip.is_unspecified()) // [!] IRainman opt: Prevent multiple repeated requests to the database if the location has not been found!
	{
		m_location = Util::getIpCountry(ip.to_ulong());
		setText(COLUMN_IP, Text::toT(ip.to_string()));
	}
	if (m_location.isKnown())
	{
		setText(COLUMN_LOCATION, m_location.getDescription());
	}
#ifdef FLYLINKDC_USE_DNS
	if (m_dns.empty())
	{
		m_dns = Socket::nslookup(m_ip);
		setText(COLUMN_DNS, Text::toT(m_dns)); // todo: paint later if not resolved yet
	}
#endif
}
