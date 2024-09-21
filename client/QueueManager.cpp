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

#if !defined(_WIN32) && !defined(PATH_MAX) // Extra PATH_MAX check for Mac OS X
#include <sys/syslimits.h>
#endif

#include "QueueManager.h"
#include "AppPaths.h"
#include "SearchManager.h"
#include "Download.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "FavoriteManager.h"
#include "ConnectionManager.h"
#include "DatabaseManager.h"
#include "DebugManager.h"
#include "MerkleCheckOutputStream.h"
#include "SearchResult.h"
#include "PathUtil.h"
#include "SharedFileStream.h"
#include "ADLSearch.h"
#include "ShareManager.h"
#include "Wildcards.h"
#include "Random.h"
#include "ConfCore.h"
#include "version.h"

#ifdef _WIN32
#include "SysVersion.h"
#include "HashManager.h"
#endif

static const unsigned SAVE_QUEUE_TIME = 300000; // 5 minutes
static const int64_t MOVER_LIMIT = 10 * 1024 * 1024;
static const int MAX_MATCH_QUEUE_ITEMS = 10;
static const size_t PFS_SOURCES = 10;
static const int PFS_QUERY_INTERVAL = 300000; // 5 minutes

QueueManager::FileQueue QueueManager::fileQueue;
QueueManager::UserQueue QueueManager::userQueue;
bool QueueManager::dirty = false;
uint64_t QueueManager::lastSave = 0;

static string getQueueFile()
{
	return Util::getConfigPath() + "Queue.xml";
}

QueueManager::FileQueue::FileQueue() :
#ifdef USE_QUEUE_RWLOCK
	csFQ(RWLock::create())
#else
	csFQ(new CriticalSection)
#endif
{
	generationId = 0;
}

uint64_t QueueManager::FileQueue::getGenerationId() const
{
	QueueRLock(*csFQ);
	return generationId;
}

void QueueManager::FileQueue::updatePriority(QueueItem::Priority& p, bool& autoPriority, const string& fileName, int64_t size, QueueItem::MaskType flags) noexcept
{
	if (p < QueueItem::DEFAULT || p >= QueueItem::LAST)
		p = QueueItem::DEFAULT;

	autoPriority = false;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	
	if (ss->getBool(Conf::AUTO_PRIORITY_USE_PATTERNS))
	{
		const string& pattern = ss->getString(Conf::AUTO_PRIORITY_PATTERNS);
		if (pattern != autoPriorityPattern)
		{
			autoPriorityPattern = pattern;
			if (!Wildcards::regexFromPatternList(reAutoPriority, autoPriorityPattern, true))
				autoPriorityPattern.clear();
		}
	}
	else
		autoPriorityPattern.clear();

	if (p == QueueItem::DEFAULT && !autoPriorityPattern.empty() && std::regex_match(fileName, reAutoPriority))
	{
		autoPriority = true;
		p = (QueueItem::Priority) ss->getInt(Conf::AUTO_PRIORITY_PATTERNS_PRIO);
		if (p < QueueItem::LOWEST)
			p = QueueItem::LOWEST;
		else
		if (p > QueueItem::HIGHEST)
			p = QueueItem::HIGHEST;
	}

	if (p == QueueItem::DEFAULT && ss->getBool(Conf::AUTO_PRIORITY_USE_SIZE))
	{
		if (size > 0 && size <= ss->getInt(Conf::AUTO_PRIORITY_SMALL_SIZE) << 10)
		{
			autoPriority = true;
			p = (QueueItem::Priority) ss->getInt(Conf::AUTO_PRIORITY_SMALL_SIZE_PRIO);
			if (p < QueueItem::LOWEST)
				p = QueueItem::LOWEST;
			else
			if (p > QueueItem::HIGHEST)
				p = QueueItem::HIGHEST;
		}
	}

	if (flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
		p = QueueItem::HIGHEST;
	else
	if (p == QueueItem::DEFAULT)
		p = QueueItem::NORMAL;
	ss->unlockRead();
}

QueueItemPtr QueueManager::FileQueue::add(QueueManager* qm,
                                          const string& target,
                                          int64_t targetSize,
                                          QueueItem::MaskType flags,
                                          QueueItem::MaskType extraFlags,
                                          QueueItem::Priority p,
                                          const string& tempTarget,
                                          const TTHValue* root,
                                          uint8_t maxSegments)
{
	auto added = GET_TIME();
	bool autoPriority;
	string fileName = Util::getFileName(target);
	updatePriority(p, autoPriority, fileName, targetSize, flags);

	if (!maxSegments) maxSegments = getMaxSegments(targetSize);
	flags |= qm->getFlagsForFileName(fileName);

	if (autoPriority)
		extraFlags |= QueueItem::XFLAG_AUTO_PRIORITY;
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
	if (DownloadManager::getInstance()->getOptions() & DownloadManager::OPT_AUTO_DISCONNECT)
		extraFlags |= QueueItem::XFLAG_AUTODROP;
#endif

	auto qi = root ?
		std::make_shared<QueueItem>(target, targetSize, p, flags, extraFlags, added, *root, maxSegments, tempTarget) :
		std::make_shared<QueueItem>(target, targetSize, p, flags, extraFlags, added, maxSegments, tempTarget);
	checkAntifragFile(tempTarget, flags);
	if (!add(qi))
	{
		qi.reset();
		LogManager::message(STRING_F(DUPLICATE_QUEUE_ITEM, target));
	}
	return qi;
}

void QueueManager::checkAntifragFile(const string& tempTarget, QueueItem::MaskType flags)
{
	if ((flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)) ||
		tempTarget.empty() || File::isExist(tempTarget))
		return;
	string antifrag = tempTarget + ".antifrag";
	if (File::isExist(antifrag))
		File::renameFile(antifrag, tempTarget);
}

bool QueueManager::FileQueue::isQueued(const TTHValue& tth) const
{
	QueueRLock(*csFQ);
	return queueTTH.find(tth) != queueTTH.end();
}

bool QueueManager::FileQueue::add(const QueueItemPtr& qi)
{
	QueueWLock(*csFQ);
	return addL(qi);
}

bool QueueManager::FileQueue::addL(const QueueItemPtr& qi)
{
	dcassert(qi->downloads.empty());
	if (!queue.insert(make_pair(Text::toLower(qi->getTarget()), qi)).second)
		return false;
	if (!qi->getTTH().isZero())
	{
		auto countTTH = queueTTH.insert(make_pair(qi->getTTH(), QueueItemList{qi}));
		if (!countTTH.second)
			countTTH.first->second.push_back(qi);
	}
	++generationId;
	return true;
}

void QueueManager::FileQueue::remove(const QueueItemPtr& qi)
{
	{
		LOCK(qi->csSegments);
		if (!qi->downloads.empty())
		{
			dcassert(0);
			qi->downloads.clear();
		}
	}

	QueueWLock(*csFQ);
	auto j = queue.find(Text::toLower(qi->getTarget()));
	if (j != queue.end())
	{
		queue.erase(j);
		if (queue.empty())
			generationId = 0;
		else
			++generationId;
	}
	if (!qi->getTTH().isZero())
	{
		auto i = queueTTH.find(qi->getTTH());
		dcassert(i != queueTTH.end());
		if (i != queueTTH.end())
		{
			QueueItemList& l = i->second;
			if (l.size() > 1)
			{
				auto j = std::find(l.begin(), l.end(), qi);
				if (j == l.end())
				{
					dcassert(0);
					return;
				}
				l.erase(j);
			}
			else
				queueTTH.erase(i);
		}
	}
}

void QueueManager::FileQueue::clearAll()
{
	QueueWLock(*csFQ);
	queueTTH.clear();
	queue.clear();
	generationId = 0;
}

int QueueManager::FileQueue::findQueueItems(QueueItemList& ql, const TTHValue& tth, int maxCount /*= 0 */) const
{
	int count = 0;
	QueueRLock(*csFQ);
	auto i = queueTTH.find(tth);
	if (i != queueTTH.end())
	{
		const QueueItemList& l = i->second;
		for (const QueueItemPtr& qi : l)
		{
			ql.push_back(qi);
			if (maxCount == ++count)
				break;
		}
	}
	return count;
}

QueueItemPtr QueueManager::FileQueue::findQueueItem(const TTHValue& tth) const
{
	QueueRLock(*csFQ);
	auto i = queueTTH.find(tth);
	if (i != queueTTH.end())
	{
		const QueueItemList& l = i->second;
		dcassert(!l.empty());
		if (!l.empty()) return l.front();
	}
	return nullptr;
}

static QueueItemPtr findCandidateL(const QueueItem::QIStringMap::const_iterator& start, const QueueItem::QIStringMap::const_iterator& end, const deque<string>& recent, const QueueItem::GetSegmentParams& gsp)
{
	QueueItemPtr cand = nullptr;

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	size_t limit = ss->getInt(Conf::AUTO_SEARCH_LIMIT);
	ss->unlockRead();

	for (auto i = start; i != end; ++i)
	{
		const QueueItemPtr& q = i->second;
		// We prefer to search for things that are not running...
		if (cand && q->getNextSegmentL(gsp, nullptr, nullptr).getSize() == 0)
			continue;
		// No finished files
		if (q->isFinished())
			continue;
		// No user lists
		if (q->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			continue;
		// No paused downloads
		q->lockAttributes();
		auto p = q->getPriorityL();
		q->unlockAttributes();
		if (p == QueueItem::PAUSED)
			continue;
		// No files that already have more than AUTO_SEARCH_LIMIT online sources
		if (limit && q->hasOnlineSourcesL(limit))
			continue;
		// Did we search for it recently?
		if (find(recent.begin(), recent.end(), q->getTarget()) != recent.end())
			continue;

		cand = q;

		if (cand->getNextSegmentL(gsp, nullptr, nullptr).getSize() != 0)
			break;
	}

	//check this again, if the first item we pick is running and there are no
	//other suitable items this will be true
	if (cand && cand->getNextSegmentL(gsp, nullptr, nullptr).getSize() == 0)
		cand = nullptr;

	return cand;
}

QueueItemPtr QueueManager::FileQueue::findAutoSearch(const deque<string>& recent) const
{
	QueueItem::GetSegmentParams gsp;
	gsp.readSettings();
	gsp.wantedSize = 0;
	gsp.lastSpeed = 0;

	QueueRLock(*QueueItem::g_cs);
	{
		QueueRLock(*csFQ);
		dcassert(!queue.empty());
		if (!queue.empty())
		{
			const QueueItem::QIStringMap::size_type start = Util::rand((uint32_t) queue.size());

			auto i = queue.begin();
			advance(i, start);
			if (i == queue.end())
			{
				i = queue.begin();
			}
			QueueItemPtr cand = findCandidateL(i, queue.end(), recent, gsp);
			if (!cand)
			{
#ifdef _DEBUG
				LogManager::message("[1] FileQueue::findAutoSearch - cand is null", false);
#endif
				cand = findCandidateL(queue.begin(), i, recent, gsp);
#ifdef _DEBUG
				if (cand)
					LogManager::message("[1-1] FileQueue::findAutoSearch - cand" + cand->getTarget(), false);
#endif
			}
			else if (cand->getNextSegmentL(gsp, nullptr, nullptr).getSize() == 0)
			{
				QueueItemPtr cand2 = findCandidateL(queue.begin(), i, recent, gsp);
				if (cand2 && cand2->getNextSegmentL(gsp, nullptr, nullptr).getSize() != 0)
				{
#ifdef _DEBUG
					LogManager::message("[2] FileQueue::findAutoSearch - cand2 = " + cand2->getTarget(), false);
#endif
					cand = cand2;
				}
			}
#ifdef _DEBUG
			if (cand)
				LogManager::message("[3] FileQueue::findAutoSearch - cand = " + cand->getTarget(), false);
#endif
			return cand;
		}
		else
		{
#ifdef _DEBUG
			LogManager::message("[4] FileQueue::findAutoSearch - not found queue.empty()", false);
#endif
			return QueueItemPtr();
		}
	}
}

QueueItemPtr QueueManager::FileQueue::moveTarget(QueueItemPtr& qi, const string& target)
{
	remove(qi);
	auto qt = std::make_shared<QueueItem>(target, *qi);
	add(qt);
	return qt;
}

QueueManager::UserQueue::UserQueue() : csRunningMap(RWLock::create())
{
}

bool QueueManager::UserQueue::getQueuedItems(const UserPtr& user, QueueItemList& out) const
{
	bool hasDown = false;
	QueueRLock(*QueueItem::g_cs);
	for (size_t i = 0; i < QueueItem::LAST && !ClientManager::isBeforeShutdown(); ++i)
	{
		const auto j = userQueueMap[i].find(user);
		if (j != userQueueMap[i].end())
		{
			out.insert(out.end(), j->second.cbegin(), j->second.cend());
			if (i != QueueItem::PAUSED)
				hasDown = true;
		}
	}
	return hasDown;
}

void QueueManager::UserQueue::addL(const QueueItemPtr& qi, QueueItem::Priority p)
{
	for (auto i = qi->sources.begin(); i != qi->sources.end(); ++i)
		addL(qi, p, i->first);
	qi->prioQueue = p;
}

void QueueManager::UserQueue::addL(const QueueItemPtr& qi, QueueItem::Priority prioQueue, const UserPtr& user)
{
	dcassert(prioQueue >= 0 && prioQueue < QueueItem::LAST);
	auto& uq = userQueueMap[prioQueue][user];

	if ((qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)) || qi->getDownloadedBytes() > 0)
		uq.push_front(qi);
	else
		uq.push_back(qi);
}

bool QueueManager::FileQueue::getTTH(const string& target, TTHValue& tth) const
{
	QueueRLock(*csFQ);
	auto i = queue.find(Text::toLower(target));
	if (i != queue.cend())
	{
		tth = i->second->getTTH();
		return true;
	}
	return false;
}

QueueItemPtr QueueManager::FileQueue::findTarget(const string& target) const
{
	QueueRLock(*csFQ);
	auto i = queue.find(Text::toLower(target));
	if (i != queue.cend())
		return i->second;
	return nullptr;
}

int QueueManager::UserQueue::getNextL(QueueItemSegment& result, const UserPtr& user, const QueueItem::GetSegmentParams& gsp, QueueItem::Priority minPrio, int flags)
{
	int p = QueueItem::LAST - 1;
	int lastError = QueueItem::ERROR_NO_ITEM;

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	size_t fileSlots = ss->getInt(Conf::FILE_SLOTS);
	ss->unlockRead();
	bool hasFreeSlots = fileSlots == 0 || userQueue.getRunningCount() < fileSlots;

	do
	{
		const auto i = userQueueMap[p].find(user);
		if (i != userQueueMap[p].cend())
		{
			QueueItemList& userItems = i->second;
			auto j = userItems.cbegin();
			while (j != userItems.cend())
			{
				const QueueItemPtr qi = *j;
				const auto source = qi->findSourceL(user);
				if (source == qi->sources.end())
				{
					++j;
					continue;
				}
				bool isFileList = (qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)) != 0;
				if (qi->isWaiting())
				{
					// Check maximum simultaneous files setting
					if (!isFileList && !hasFreeSlots)
					{
						lastError = QueueItem::ERROR_FILE_SLOTS_TAKEN;
						result.qi = qi;
						++j;
						continue;
					}
				}
				else if (isFileList || qi->getSize() == -1 || !(qi->getExtraFlags() & QueueItem::XFLAG_ALLOW_SEGMENTS))
				{
					// Disallow multiple segments when getting a tree, file list or file of unknown size
					++j;
					continue;
				}
				int sourceError;
				Segment segment = qi->getNextSegmentL(gsp, source->second.partialSource, &sourceError);
				if (segment.getSize())
				{
					if ((flags & FLAG_ADD_SEGMENT) && segment.getSize() != -1)
					{
						qi->addDownload(segment);
						result.seg = segment;
					}
					else
						result.seg = Segment(0, -1);
					result.qi = qi;
					result.sourceFlags = source->second.getFlags();
					return QueueItem::SUCCESS;
				}
				if (lastError == QueueItem::ERROR_NO_ITEM)
				{
					lastError = sourceError;
					result.qi = qi;
					//LogManager::message("No segment for User " + user->getLastNick() + " target=" + qi->getTarget() + " flags=" + Util::toString(qi->getFlags()), false);
				}
				if ((flags & FLAG_ALLOW_REMOVE) && source->second.partialSource && sourceError == QueueItem::ERROR_NO_NEEDED_PART)
				{
					qi->removeSourceL(user, QueueItem::Source::FLAG_NO_NEED_PARTS);
					j = userItems.erase(j);
					continue;
				}
				++j;
			}
			if (userItems.empty())
				userQueueMap[p].erase(i);
		}
		p--;
	}
	while (p >= minPrio);
	return lastError;
}

void QueueManager::UserQueue::setRunningDownload(const QueueItemPtr& qi, const UserPtr& user)
{
	WRITE_LOCK(*csRunningMap);
	runningMap[user] = qi;
}

size_t QueueManager::UserQueue::getRunningCount() const
{
	READ_LOCK(*csRunningMap);
	return runningMap.size();
}

void QueueManager::UserQueue::removeRunning(const UserPtr& user)
{
	WRITE_LOCK(*csRunningMap);
	dcdebug("removeRunning: %s\n", user->getLastNick().c_str());
	runningMap.erase(user);
}

void QueueManager::UserQueue::removeDownload(const QueueItemPtr& qi, const UserPtr& user)
{
	qi->removeDownload(user);
	WRITE_LOCK(*csRunningMap);
	auto i = runningMap.find(user);
	if (i != runningMap.end() && i->second == qi)
		runningMap.erase(i);
}

void QueueManager::UserQueue::setQIPriority(const QueueItemPtr& qi, QueueItem::Priority p)
{
	QueueWLock(*QueueItem::g_cs);
	removeQueueItemL(qi, p == QueueItem::PAUSED);
	qi->lockAttributes();
	qi->setPriorityL(p);
	qi->unlockAttributes();
	addL(qi, p);
}

QueueItemPtr QueueManager::UserQueue::getRunning(const UserPtr& user)
{
	READ_LOCK(*csRunningMap);
	const auto i = runningMap.find(user);
	return i == runningMap.cend() ? nullptr : i->second;
}

void QueueManager::UserQueue::removeQueueItem(const QueueItemPtr& qi)
{
	QueueWLock(*QueueItem::g_cs);
	userQueue.removeQueueItemL(qi, true);
}

void QueueManager::UserQueue::removeQueueItemL(const QueueItemPtr& qi, bool removeDownloadFlag)
{
	const auto& s = qi->getSourcesL();
	for (auto i = s.cbegin(); i != s.cend(); ++i)
		removeUserL(qi, i->first, removeDownloadFlag);
}

bool QueueManager::UserQueue::isInQueue(const QueueItemPtr& qi) const
{
	QueueRLock(*QueueItem::g_cs);
	if (qi->prioQueue < 0 || qi->prioQueue >= QueueItem::LAST)
	{
		dcassert(qi->prioQueue == QueueItem::DEFAULT);
		return false;
	}
	auto& ulm = userQueueMap[qi->prioQueue];
	const auto& s = qi->getSourcesL();
	for (auto i = s.cbegin(); i != s.cend(); ++i)
	{
		auto j = ulm.find(i->first);
		if (j != ulm.end()) return true;
	}
	return false;
}

void QueueManager::UserQueue::removeUserL(const QueueItemPtr& qi, const UserPtr& user, bool removeDownloadFlag)
{
	if (qi->prioQueue < 0 || qi->prioQueue >= QueueItem::LAST)
	{
		dcassert(0);
		return;
	}

	if (removeDownloadFlag)
		removeDownload(qi, user);

	auto& ulm = userQueueMap[qi->prioQueue];
	const auto j = ulm.find(user);
	if (j == ulm.cend())
	{
#ifdef _DEBUG
		const string error = "Error QueueManager::UserQueue::removeUserL [dcassert(j != ulm.cend())] user = " +
			                     (user ? user->getLastNick() : string("null"));
		LogManager::message(error);
#endif
		return;
	}

	auto& uq = j->second;
	const auto i = find(uq.begin(), uq.end(), qi);
	if (i == uq.cend())
		return;
	uq.erase(i);
	if (uq.empty()) ulm.erase(j);
}

QueueItemPtr QueueManager::UserQueue::findItemByFlag(const UserPtr& user, int flag) const
{
	QueueRLock(*QueueItem::g_cs);
	for (int p = QueueItem::LAST - 1; p >= 0; --p)
	{
		const auto j = userQueueMap[p].find(user);
		if (j != userQueueMap[p].end())
		{
			const QueueItemList& ql = j->second;
			for (const QueueItemPtr& qi : ql)
				if (qi->getFlags() & flag)
					return qi;
		}
	}
	return QueueItemPtr();
}

QueueManager::QueueManager() :
	nextSearch(0),
	listMatcherAbortFlag(false),
	dclstLoaderAbortFlag(false),
	recheckerAbortFlag(false),
	sourceAdded(false),
	batchCounter(0)
{
	listMatcherRunning.clear();

	TimerManager::getInstance()->addListener(this);
	SearchManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);

	File::ensureDirectory(Util::getListPath());
	deleteFileLists();
}

QueueManager::~QueueManager() noexcept
{
	SearchManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	saveQueue();
	SharedFileStream::finalCleanup();
}

void QueueManager::shutdown()
{
	listMatcherAbortFlag.store(true);
	dclstLoaderAbortFlag.store(true);
	recheckerAbortFlag.store(true);
	listMatcher.shutdown();
	dclstLoader.shutdown();
	fileMover.shutdown();
	rechecker.shutdown();
}

static void getOldFiles(StringList& delList, const string& path, uint64_t currentTime, unsigned days)
{
	for (FileFindIter it(path); it != FileFindIter::end; ++it)
	{
		if (!days || it->getTimeStamp() + days * 86400ull * Util::FILETIME_UNITS_PER_SEC < currentTime)
			delList.push_back(it->getFileName());
	}
}

void QueueManager::deleteFileLists()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	unsigned days = ss->getInt(Conf::KEEP_LISTS_DAYS);
	ss->unlockRead();

	StringList delList;
	const string& path = Util::getListPath();
	uint64_t currentTime = Util::getFileTime();
	getOldFiles(delList, path + "*.xml.bz2", currentTime, days);
	getOldFiles(delList, path + "*.xml", currentTime, days);
	getOldFiles(delList, path + "*.dctmp", currentTime, 0);
	for (const string& filename : delList)
		File::deleteFile(path + filename);
}

struct PartsInfoReqParam
{
	QueueItem::PartsInfo parts;
	uint64_t  blockSize;
	string    tth;
	string    nick;
	string    myNick;
	string    hubIpPort;
	IpAddress ip;
	uint16_t  udpPort;
	uint8_t   req;
};

void QueueManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;

	auto sm = SearchManager::getInstance();
	if (sm->getOptions() & SearchManager::OPT_ENABLE_SUDP)
		sm->createNewDecryptKey(tick);

	auto ss = SettingsManager::instance.getCoreSettings();
	string searchString;
	vector<const PartsInfoReqParam*> params;
	{
		QueueItem::SourceList sl;
		//find max 10 pfs sources to exchange parts
		//the source basis interval is 5 minutes
		fileQueue.findPFSSources(sl, tick);

		for (const auto& item : sl)
		{
			QueueItem::PartialSource::Ptr source = item.si->second.partialSource;
			uint8_t req = source->getPendingQueryCount() + 1;
			source->setPendingQueryCount(req);
			source->setNextQueryTime(tick + PFS_QUERY_INTERVAL);

			PartsInfoReqParam* param = new PartsInfoReqParam;
			item.qi->getParts(param->parts, item.qi->getBlockSize());

			param->blockSize = source->getBlockSize();
			param->tth = item.qi->getTTH().toBase32();
			param->ip  = source->getIp();
			param->udpPort = source->getUdpPort();
			param->myNick = source->getMyNick();
			param->nick = item.si->first->getLastNick();
			param->hubIpPort = source->getHubIpPort();
			param->req = req;

			params.push_back(param);
		}
	}
	// TODO: improve auto search
	if (fileQueue.getSize() > 0 && tick >= nextSearch)
	{
		ss->lockRead();
		bool autoSearch = ss->getBool(Conf::AUTO_SEARCH);
		ss->unlockRead();
		if (autoSearch)
		{		
			// We keep 30 recent searches to avoid duplicate searches
			while (m_recent.size() >= fileQueue.getSize() || m_recent.size() > 30)
			{
				m_recent.pop_front();
			}

			QueueItemPtr qi;
			while ((qi = fileQueue.findAutoSearch(m_recent)) == nullptr && !m_recent.empty()) // Местами не переставлять findAutoSearch меняет recent
			{
				m_recent.pop_front();
			}
			if (qi)
			{
				searchString = qi->getTTH().toBase32();
				m_recent.push_back(qi->getTarget());
				ss->lockRead();
				nextSearch = tick + ss->getInt(Conf::AUTO_SEARCH_TIME) * 60000;
				bool reportAlternates = ss->getBool(Conf::REPORT_ALTERNATES);
				ss->unlockRead();
				if (reportAlternates)
				{
					LogManager::message(STRING(ALTERNATES_SEND) + ' ' + Util::getFileName(qi->getTarget())
#ifdef _DEBUG
					+ " TTH = " + qi->getTTH().toBase32()
#endif
					 );
				}
			}
		}
	}

	// Request parts info from partial file sharing sources
	int logOptions = LogManager::getLogOptions();
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const PartsInfoReqParam* param = *i;
		dcassert(param->udpPort > 0);
		AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
		SearchManager::toPSR(cmd, true, param->myNick, param->ip.type, param->hubIpPort, param->tth, param->parts);
		string data = cmd.toString(ClientManager::getMyCID());
		if (CMD_DEBUG_ENABLED())
			COMMAND_DEBUG("[Partial-Search]" + data, DebugTask::CLIENT_OUT, Util::printIpAddress(param->ip) + ':' + Util::toString(param->udpPort));
		if (logOptions & LogManager::OPT_LOG_PSR)
		{
			string msg = param->tth + ": sending periodic PSR #" + Util::toString(param->req) + " to ";
			msg += Util::printIpAddress(param->ip, true) + ':' + Util::toString(param->udpPort);
			if (!param->nick.empty()) msg += ", " + param->nick;
			msg += ", we have " + Util::toString(QueueItem::countParts(param->parts)) + '*' + Util::toString(param->blockSize);
			LOG(PSR_TRACE, msg);
		}
		sm->addToSendQueue(data, param->ip, param->udpPort);
		delete param;
	}
	if (!searchString.empty())
		sm->searchAuto(searchString);
}

void QueueManager::addList(const HintedUser& hintedUser, QueueItem::MaskType flags, QueueItem::MaskType extraFlags, const string& initialDir /* = Util::emptyString */)
{
	bool getConnFlag = true;
	QueueItemParams params;
	add(initialDir, params, hintedUser, (QueueItem::MaskType)(QueueItem::FLAG_USER_LIST | flags), extraFlags, getConnFlag);
}

#if 0
void QueueManager::addCheckUserIP(const UserPtr& user)
{
	bool getConnFlag = true;
	QueueItemParams params;
	add(Util::emptyString, params, user, QueueItem::FLAG_USER_GET_IP, 0, getConnFlag);
}
#endif

bool QueueManager::userCheckStart(const HintedUser& hintedUser)
{
	if (!hintedUser.user->startUserCheck(Util::getTick())) return false;
	try
	{
		addList(hintedUser, QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_CHECK, 0, Util::emptyString);
	}
	catch (const Exception&)
	{
		hintedUser.user->changeFlags(User::USER_CHECK_FAILED, User::USER_CHECK_RUNNING);
		return false;
	}
	return true;
}

void QueueManager::userCheckProcessFailure(const UserPtr& user, int numErrors, bool removeQueueItem)
{
	if ((numErrors >= 3 || numErrors == -1) && (user->getFlags() & User::USER_CHECK_RUNNING))
	{
		user->changeFlags(User::USER_CHECK_FAILED, User::USER_CHECK_RUNNING);
		if (removeQueueItem)
		{
			QueueItemPtr qi = userQueue.findItemByFlag(user, QueueItem::FLAG_USER_CHECK);
			if (qi) removeItem(qi, true);
		}
		LogManager::message("User check failed: " + user->getLastNick(), false);
	}
}

string QueueManager::getFileListTarget(const UserPtr& user)
{
	dcassert(user);
	if (user)
	{
		string nick = user->getLastNick();
		if (nick.empty())
		{
			const StringList nicks = ClientManager::getNicks(user->getCID(), Util::emptyString);
			if (!nicks.empty()) nick = std::move(nicks[0]);
		}
		if (!nick.empty())
		{
			nick = Util::validateFileName(nick, true);
			std::replace(nick.begin(), nick.end(), PATH_SEPARATOR, '_');
			std::replace(nick.begin(), nick.end(), '.', '_');
			nick += '.';
		}
		return checkTarget(Util::getListPath() + nick + user->getCID().toBase32(), -1);
	}
	return string();
}

void QueueManager::getDownloadConnection(const HintedUser& hintedUser)
{
	ConnectionManager::getInstance()->getDownloadConnection(hintedUser);
}

void QueueManager::getDownloadConnections(const UserList& users)
{
	if (users.empty()) return;
	auto cm = ConnectionManager::getInstance();
	HintedUser hintedUser;
	for (const UserPtr& user : users)
	{
		hintedUser.user = user;
		cm->getDownloadConnection(hintedUser);
	}
}

void QueueManager::add(const string& target, const QueueItemParams& params, const HintedUser& hintedUser, QueueItem::MaskType flags, QueueItem::MaskType extraFlags, bool& getConnFlag)
{
	// Check that we're not downloading from ourselves...
	if (hintedUser.user && hintedUser.user->isMe())
	{
		dcassert(0);
		throw QueueException(QueueException::BAD_USER, STRING(NO_DOWNLOADS_FROM_SELF));
	}

	const bool fileList = (flags & QueueItem::FLAG_USER_LIST) != 0;
	const bool testIP = (flags & QueueItem::FLAG_USER_GET_IP) != 0;
	bool newItem = !(testIP || fileList);

	if (!(params.size == -1 && (fileList || testIP)) && (params.size < 0 || params.size > FILE_SIZE_LIMIT))
		throw QueueException(QueueException::BAD_FILE_SIZE, STRING(INVALID_SIZE));

	auto ss = SettingsManager::instance.getCoreSettings();

	string targetPath;
	string tempTarget;
	if (fileList)
	{
		dcassert(hintedUser.user);
		targetPath = getFileListTarget(hintedUser.user);
		if (flags & QueueItem::FLAG_PARTIAL_LIST)
		{
			targetPath += ':';
			targetPath += target;
		}
		tempTarget = target;
	}
	else if (testIP)
	{
		dcassert(hintedUser.user);
		targetPath = getFileListTarget(hintedUser.user) + ".check";
		tempTarget = target;
	}
	else
	{
		if (File::isAbsolute(target))
			targetPath = target;
		else
			targetPath = FavoriteManager::getInstance()->getDownloadDirectory(Util::getFileExt(target), hintedUser.user) + target;
		targetPath = checkTarget(targetPath, -1);
	}

	// Check if it's a zero-byte file, if so, create and return...
	if (params.size == 0)
	{
		ss->lockRead();
		bool skipZeroByte = ss->getBool(Conf::SKIP_ZERO_BYTE);
		ss->unlockRead();
		if (!skipZeroByte)
		{
			File::ensureDirectory(targetPath);
			try { File f(targetPath, File::WRITE, File::CREATE); }
			catch (const FileException&) {}
		}
		return;
	}

	bool wantConnection = false;
	QueueItem::Priority priority = params.priority;

	QueueItemPtr q = fileQueue.findTarget(targetPath);
	string sharedFilePath;
	if (!q)
	{
		int64_t existingFileSize = 0;
		int64_t existingFileTime = 0;
		bool waitForUserInput = false;
		QueueItem::Priority savedPriority = priority;
		if (newItem)
		{
			FileAttributes attr;
			bool targetExists = File::getAttributes(targetPath, attr);
			if (targetExists)
			{
				ss->lockRead();
				bool skipExisting = ss->getBool(Conf::SKIP_EXISTING);
				int targetExistsAction = ss->getInt(Conf::TARGET_EXISTS_ACTION);
				ss->unlockRead();
				existingFileSize = attr.getSize();
				existingFileTime = File::timeStampToUnixTime(attr.getTimeStamp());
				if (existingFileSize == params.size && skipExisting)
				{
					LogManager::message(STRING_F(SKIPPING_EXISTING_FILE, targetPath));
					return;
				}
				switch (targetExistsAction)
				{
					case Conf::TE_ACTION_ASK:
						waitForUserInput = true;
						priority = QueueItem::PAUSED;
						break;
					case Conf::TE_ACTION_REPLACE:
						File::deleteFile(targetPath); // Delete old file.
						break;
					case Conf::TE_ACTION_RENAME:
						targetPath = Util::getNewFileName(targetPath); // Call Util::getNewFileName instead of using CheckTargetDlg's stored name
						break;
					case Conf::TE_ACTION_SKIP:
						return;
				}
			}

			ss->lockRead();
			int copyExistingMaxSize = ss->getInt(Conf::COPY_EXISTING_MAX_SIZE);
			ss->unlockRead();
			int64_t maxSizeForCopy = (int64_t) copyExistingMaxSize << 20;
			if (!waitForUserInput && maxSizeForCopy && params.root &&
			    ShareManager::getInstance()->getFileInfo(*params.root, sharedFilePath) &&
			    File::getAttributes(sharedFilePath, attr))
			{
				auto sharedFileSize = attr.getSize();
				if (sharedFileSize == params.size && sharedFileSize <= maxSizeForCopy)
				{
					LogManager::message(STRING_F(COPYING_EXISTING_FILE, targetPath));
					priority = QueueItem::PAUSED;
					extraFlags |= QueueItem::XFLAG_COPYING;
				}
			}
		}

		q = fileQueue.add(this, targetPath, params.size, flags, extraFlags, priority, tempTarget, params.root, 0);

		if (q)
		{
#ifdef DEBUG_TRANSFERS
			if (!params.sourcePath.empty()) q->setSourcePath(params.sourcePath);
#endif
			csBatch.lock();
			if (batchCounter)
			{
				batchAdd.push_back(q);
				csBatch.unlock();
			}
			else
			{
				csBatch.unlock();
				fire(QueueManagerListener::Added(), q);
			}
			if (extraFlags & QueueItem::XFLAG_COPYING)
			{
				newItem = false;
				FileMoverJob* job = new FileMoverJob(*this, FileMoverJob::COPY_QI_FILE, sharedFilePath, targetPath, false, q);
				if (!fileMover.addJob(job))
					delete job;
			}
			else if (waitForUserInput)
			{
				string fileName = Util::getFileName(targetPath);
				bool autoPriority;
				fileQueue.updatePriority(savedPriority, autoPriority, fileName, params.size, flags);
				q->changeExtraFlags(autoPriority ? QueueItem::XFLAG_AUTO_PRIORITY : 0, QueueItem::XFLAG_AUTO_PRIORITY);
				fire(QueueManagerListener::FileExistsAction(), targetPath, params.size, existingFileSize, existingFileTime, savedPriority);
			}
		}
	}
	else
	{
		if (q->getFlags() & QueueItem::FLAG_USER_LIST)
			throw QueueException(QueueException::DUPLICATE_SOURCE, STRING(FILE_LIST_ALREADY_QUEUED));
		if (q->getSize() != params.size)
			throw QueueException(QueueException::BAD_FILE_SIZE, STRING(FILE_WITH_DIFFERENT_SIZE));
		if (params.root && !(*params.root == q->getTTH()))
			throw QueueException(QueueException::BAD_FILE_TTH, STRING(FILE_WITH_DIFFERENT_TTH));
		if (q->isFinished())
			throw QueueException(QueueException::ALREADY_FINISHED, STRING(FILE_ALREADY_FINISHED));

		static const QueueItem::MaskType UPDATE_FLAGS_MASK = QueueItem::XFLAG_CLIENT_VIEW | QueueItem::XFLAG_TEXT_VIEW | QueueItem::XFLAG_DOWNLOAD_CONTENTS;
		q->changeExtraFlags(flags & UPDATE_FLAGS_MASK, UPDATE_FLAGS_MASK);
	}
	if (!(extraFlags & QueueItem::XFLAG_COPYING) && !(hintedUser.user->getFlags() & User::FAKE) && q)
	{
		QueueWLock(*QueueItem::g_cs);
		wantConnection = addSourceL(q, hintedUser.user, (QueueItem::MaskType)(params.readdBadSource ? QueueItem::Source::FLAG_MASK : 0));
		if (priority == QueueItem::PAUSED) wantConnection = false;
	}
	else
		wantConnection = false;
	setDirty();

	if (getConnFlag)
	{
		if (wantConnection && hintedUser.user->isOnline())
		{
			getDownloadConnection(hintedUser);
			getConnFlag = false;
		}

		// Perform auto search
		if (newItem && params.root)
		{
			ss->lockRead();
			bool autoSearch = ss->getBool(Conf::AUTO_SEARCH);
			ss->unlockRead();
			if (autoSearch)
				SearchManager::getInstance()->searchAuto(params.root->toBase32());
		}
	}
}

void QueueManager::readdAll(const QueueItemPtr& q)
{
	QueueItem::SourceMap badSources;
	{
		QueueRLock(*QueueItem::g_cs);
		badSources = q->getBadSourcesL();
	}
	for (auto s = badSources.cbegin(); s != badSources.cend(); ++s)
	{
		readd(q->getTarget(), s->first);
	}
}

void QueueManager::readd(const string& target, const UserPtr& user)
{
	bool wantConnection = false;
	{
		const QueueItemPtr q = fileQueue.findTarget(target);
		QueueWLock(*QueueItem::g_cs);
		if (q && q->isBadSourceL(user))
		{
			wantConnection = addSourceL(q, user, QueueItem::Source::FLAG_MASK);
		}
	}
	if (wantConnection && user->isOnline())
	{
		getDownloadConnection(HintedUser(user, Util::emptyString));
	}
}

void QueueManager::setDirty()
{
	if (!dirty)
	{
		dirty = true;
		lastSave = GET_TICK();
	}
}

string QueueManager::checkTarget(const string& target, const int64_t size, bool validateFileName /* = true*/)
{
#ifdef _WIN32
	if (target.length() > FULL_MAX_PATH)
		throw QueueException(QueueException::BAD_FILE_NAME, STRING(TARGET_FILENAME_TOO_LONG));
	// Check that target starts with a drive or is an UNC path
	if (!(target.length() > 3 && ((target[1] == ':' && target[2] == '\\') || (target[0] == '\\' && target[1] == '\\'))))
		throw QueueException(QueueException::BAD_FILE_NAME, STRING(INVALID_TARGET_FILE));
#else
	if (target.length() > PATH_MAX)
		throw QueueException(QueueException::BAD_FILE_NAME, STRING(TARGET_FILENAME_TOO_LONG));
	// Check that target contains at least one directory...we don't want headless files...
	if (!File::isAbsolute(target))
		throw QueueException(QueueException::BAD_FILE_NAME, STRING(INVALID_TARGET_FILE));
#endif

	const string validatedTarget = validateFileName ? Util::validateFileName(target) : target;

	if (size != -1)
	{
		// Check that the file doesn't already exist...
		const int64_t sz = File::getSize(validatedTarget);
		if (size <= sz)
			throw FileException(STRING(LARGER_TARGET_FILE_EXISTS));
	}
	return validatedTarget;
}

void QueueManager::processFileExistsQuery(const string& path, int action, const string& newPath, QueueItem::Priority priority)
{
	switch (action)
	{
		case Conf::TE_ACTION_REPLACE:
			File::deleteFile(path);
			setPriority(path, priority, false);
			break;
		case Conf::TE_ACTION_RENAME:
			move(path, newPath);
			setPriority(newPath, priority, false);
			break;
		case Conf::TE_ACTION_SKIP:
			removeTarget(path);
	}
}

/** Add a source to an existing queue item */
bool QueueManager::addSourceL(const QueueItemPtr& qi, const UserPtr& user, QueueItem::MaskType addBad)
{
	dcassert(user);
	bool wantConnection;
	{
		qi->lockAttributes();
		auto p = qi->getPriorityL();
		qi->unlockAttributes();

		wantConnection = p != QueueItem::PAUSED && !userQueue.getRunning(user);
		if (qi->isSourceL(user))
		{
			if (qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
				return wantConnection;
			throw QueueException(QueueException::DUPLICATE_SOURCE, STRING(DUPLICATE_SOURCE) + ": " + Util::getFileName(qi->getTarget()));
		}
		if (qi->isBadSourceExceptL(user, addBad))
		{
			throw QueueException(QueueException::DUPLICATE_SOURCE, STRING(DUPLICATE_SOURCE) +
			                     " TTH = " + Util::getFileName(qi->getTarget()) +
			                     " Nick = " + user->getLastNick());
		}

		qi->addSourceL(user);
		/*if(user.user->isSet(User::PASSIVE) && !ClientManager::isActive(user.hint)) {
		    qi->removeSource(user, QueueItem::Source::FLAG_PASSIVE);
		    wantConnection = false;
		} else */
		if (qi->isFinished())
		{
			wantConnection = false;
		}
		else
		{
			if (!(qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)))
			{
				LOCK(csUpdatedSources);
				sourceAdded = true;
			}
			if (qi->prioQueue == QueueItem::DEFAULT)
				qi->prioQueue = p;
			userQueue.addL(qi, qi->prioQueue, user);
		}
	}
	setDirty();
	return wantConnection;
}

void QueueManager::addDirectory(const string& dir, const HintedUser& hintedUser, const string& target, QueueItem::Priority p, int flag) noexcept
{
	dcassert(flag == DIR_FLAG_DOWNLOAD_DIRECTORY || flag == DIR_FLAG_MATCH_QUEUE);
	dcassert(hintedUser.user);
	if (!hintedUser.user)
		return;

	{
		int matchQueueItems = 0;
		LOCK(csDirectories);
		const auto dp = directories.equal_range(hintedUser.user);
		for (auto i = dp.first; i != dp.second; ++i)
		{
			if (flag == DIR_FLAG_MATCH_QUEUE && (i->second.getFlags() & DIR_FLAG_MATCH_QUEUE))
			{
				if (++matchQueueItems == MAX_MATCH_QUEUE_ITEMS) return;
			}
			if (stricmp(dir, i->second.getName()) == 0)
			{
				i->second.addFlags(flag);
				return;
			}
		}
		// Unique directory, fine...
		directories.emplace(make_pair(hintedUser.user, DirectoryItem(hintedUser.user, dir, target, p, flag)));
	}

	try
	{
		addList(hintedUser, QueueItem::FLAG_PARTIAL_LIST, dirFlagsToQueueItemFlags(flag), dir);
	}
	catch (const Exception&)
	{
		// ...
	}
}

size_t QueueManager::getDirectoryItemCount() const noexcept
{
	LOCK(csDirectories);
	return directories.size();
}

QueueItem::Priority QueueManager::hasDownload(const UserPtr& user)
{
	QueueItemSegment result;
	QueueItem::GetSegmentParams gsp;
	gsp.readSettings();
	gsp.wantedSize = 0;
	gsp.lastSpeed = 0;

	{
		QueueRLock(*QueueItem::g_cs);
		if (userQueue.getNextL(result, user, gsp, QueueItem::LOWEST, 0) != QueueItem::SUCCESS)
			return QueueItem::PAUSED;
	}
	const QueueItem* qi = result.qi.get();
	qi->lockAttributes();
	auto p = qi->getPriorityL();
	qi->unlockAttributes();
	return p;
}

int QueueManager::matchListing(DirectoryListing& dl) noexcept
{
	if (!dl.getUser())
	{
		dcassert(0);
		return 0;
	}

	int matches = 0;
	if (!fileQueue.empty())
	{
		if (!dl.getTTHSet()) dl.buildTTHSet();
		const DirectoryListing::TTHMap& tthMap = *dl.getTTHSet();
		bool sourceAdded = false;
		bool addSource = !(dl.getUser()->getFlags() & User::FAKE);
		if (!tthMap.empty())
		{
			QueueWLock(*QueueItem::g_cs);
			{
				LockFileQueueShared lockQueue;
				const auto& queue = lockQueue.getQueueL();
				for (auto i = queue.cbegin(); i != queue.cend(); ++i)
				{
					const QueueItemPtr& qi = i->second;
					if (qi->isFinished())
						continue;
					if (qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
						continue;
					const auto j = tthMap.find(qi->getTTH());
					if (j != tthMap.cend() && j->second == qi->getSize())
					{
						matches++;
						if (addSource)
						{
							try
							{
								addSourceL(qi, dl.getUser(), QueueItem::Source::FLAG_FILE_NOT_AVAILABLE);
								sourceAdded = true;
							}
							catch (const Exception&)
							{
								// Ignore...
							}
						}
					}
				}
			}
		}
		if (sourceAdded)
			getDownloadConnection(HintedUser(dl.getUser(), Util::emptyString));
	}
	return matches;
}

int QueueManager::matchTTHList(const string& data, const UserPtr& user) noexcept
{
	boost::unordered_set<TTHValue> tthSet;
	SimpleStringTokenizer<char> st(data, ' ');
	string tok;
	while (st.getNextToken(tok))
	{
		if (tok.length() != 39) break;
		tthSet.insert(TTHValue(tok));
	}
	if (tthSet.empty()) return 0;
	int matches = 0;
	bool sourceAdded = false;
	{
		QueueWLock(*QueueItem::g_cs);
		{
			LockFileQueueShared lockQueue;
			const auto& queue = lockQueue.getQueueL();
			for (auto i = queue.cbegin(); i != queue.cend(); ++i)
			{
				const QueueItemPtr& qi = i->second;
				if (qi->isFinished())
					continue;
				if (qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
					continue;
				if (tthSet.find(qi->getTTH()) != tthSet.end())
				{
					matches++;
					try
					{
						addSourceL(qi, user, QueueItem::Source::FLAG_FILE_NOT_AVAILABLE);
						sourceAdded = true;
					}
					catch (const Exception&)
					{
						// Ignore...
					}
				}
			}
		}
	}
	if (sourceAdded)
		getDownloadConnection(HintedUser(user, Util::emptyString));
	return matches;
}

void QueueManager::move(const string& source, const string& newTarget) noexcept
{
	const string target = Util::validateFileName(newTarget);
	if (source == target)
		return;

	QueueItemPtr qs = fileQueue.findTarget(source);
	if (!qs)
		return;

	// Don't move file lists
	if (qs->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
		return;

	// TODO: moving running downloads is not implemented
	if (qs->isRunning())
		return;

	// Let's see if the target exists...then things get complicated...
	QueueItemPtr qt = fileQueue.findTarget(target);
	if (!qt || stricmp(source, target) == 0)
	{
		{
			QueueWLock(*QueueItem::g_cs);
			userQueue.removeQueueItemL(qs, false);
		}

		qt = fileQueue.moveTarget(qs, target);

		{
			QueueWLock(*QueueItem::g_cs);
			qs->lockAttributes();
			auto p = qs->getPriorityL();
			qs->unlockAttributes();
			userQueue.addL(qt, p);
		}

		fire(QueueManagerListener::Moved(), qs, qt);
		setDirty();
	}
	else
	{
		// Don't move to target of different size
		if (qs->getSize() != qt->getSize() || qs->getTTH() != qt->getTTH())
			return; // TODO: display error

		{
			QueueWLock(*QueueItem::g_cs);
			for (auto i = qs->getSourcesL().cbegin(); i != qs->getSourcesL().cend(); ++i)
			{
				try
				{
					addSourceL(qt, i->first, QueueItem::Source::FLAG_MASK);
				}
				catch (const Exception&)
				{
				}
			}
		}
		removeTarget(source);
	}
}

void QueueManager::startBatch() noexcept
{
	csBatch.lock();
	batchCounter++;
	csBatch.unlock();
}

void QueueManager::endBatch() noexcept
{
	vector<QueueItemPtr> added, removed;
	csBatch.lock();
	if (--batchCounter < 0)
	{
		dcassert(batchCounter >= 0);
		batchCounter = 0;
	}
	if (!batchCounter)
	{
		if (!batchAdd.empty())
		{
			added = std::move(batchAdd);
			batchAdd.clear();
		}
		if (!batchRemove.empty())
		{
			removed = std::move(batchRemove);
			batchRemove.clear();
		}
	}
	csBatch.unlock();
	if (!added.empty())
		fire(QueueManagerListener::AddedArray(), added);
	if (!removed.empty())
		fire(QueueManagerListener::RemovedArray(), removed);
}

QueueItemPtr QueueManager::getQueuedItem(const UserPtr& user) noexcept
{
	QueueItemSegment result;
	QueueItem::GetSegmentParams gsp;
	gsp.readSettings();
	gsp.wantedSize = 0;
	gsp.lastSpeed = 0;

	{
		QueueRLock(*QueueItem::g_cs);
		if (userQueue.getNextL(result, user, gsp, QueueItem::LOWEST, 0) == QueueItem::SUCCESS)
			return result.qi;
	}
	return QueueItemPtr();
}

uint8_t QueueManager::FileQueue::getMaxSegments(uint64_t filesize)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	unsigned value;
	ss->lockRead();
	if (ss->getBool(Conf::SEGMENTS_MANUAL))
		value = min(ss->getInt(Conf::NUMBER_OF_SEGMENTS), 200);
	else
		value = static_cast<unsigned>(min<uint64_t>(filesize / (50 * MIN_BLOCK_SIZE) + 2, 200));
	ss->unlockRead();
	if (!value) value = 1;
	return static_cast<uint8_t>(value);
}

void QueueManager::getTargets(const TTHValue& tth, StringList& sl, int maxCount)
{
	QueueItemList ql;
	fileQueue.findQueueItems(ql, tth, maxCount);
	for (auto i = ql.cbegin(); i != ql.cend(); ++i)
	{
		sl.push_back((*i)->getTarget());
	}
}

DownloadPtr QueueManager::getDownload(const UserConnectionPtr& ucPtr, Download::ErrorInfo& errorInfo, bool checkSlots) noexcept
{
	DownloadPtr d;
	UserConnection* source = ucPtr.get();
	const UserPtr u = source->getUser();
	if (!u)
	{
		errorInfo.error = QueueItem::ERROR_INVALID_CONNECTION;
		return d;
	}

	dcdebug("Getting download for %s...", u->getCID().toBase32().c_str());

	bool segmentAdded = false;
	QueueItem* q = nullptr;
	QueueItemSegment qs;
	QueueItem::GetSegmentParams gsp;
	gsp.readSettings();
	gsp.wantedSize = source->getChunkSize();
	gsp.lastSpeed = source->getSpeed();

	{
		QueueWLock(*QueueItem::g_cs);
		errorInfo.error = userQueue.getNextL(qs, u, gsp, QueueItem::LOWEST, UserQueue::FLAG_ADD_SEGMENT | UserQueue::FLAG_ALLOW_REMOVE);
		q = qs.qi.get();
		if (errorInfo.error != QueueItem::SUCCESS)
		{
			if (q)
			{
				errorInfo.target = q->getTarget();
				errorInfo.size = q->getSize();
				auto flags = q->getFlags();
				if (flags & QueueItem::FLAG_PARTIAL_LIST)
					errorInfo.type = Transfer::TYPE_PARTIAL_LIST;
				else if (flags & QueueItem::FLAG_USER_LIST)
					errorInfo.type = Transfer::TYPE_FULL_LIST;
				else
					errorInfo.type = Transfer::TYPE_FILE;
			}
			else
			{
				errorInfo.size = -1;
				errorInfo.type = Transfer::TYPE_FILE;
			}
			dcdebug("none\n");
			return d;
		}

		// Check that the file we will be downloading to exists
		q->updateDownloadedBytesAndSpeed();
		if (q->getDownloadedBytes() > 0)
		{
			q->lockAttributes();
			string tempTarget = q->getTempTargetL();
			q->unlockAttributes();
			if (!File::isExist(tempTarget)) // TODO: clear tempTarget?
			{
				// Temp target gone?
				q->resetDownloaded();
			}
		}

		segmentAdded = qs.seg.getSize() != -1;
		if (checkSlots)
		{
			q->lockAttributes();
			auto p = q->getPriorityL();
			q->unlockAttributes();
			if (!DownloadManager::getInstance()->isStartDownload(p))
			{
				if (segmentAdded) q->removeDownload(qs.seg);
				errorInfo.error = QueueItem::ERROR_DOWNLOAD_SLOTS_TAKEN;
				errorInfo.target = q->getTarget();
				errorInfo.size = q->getSize();
				errorInfo.type = Transfer::TYPE_FILE;
				return d;
			}
		}
	}

	dcassert(qs.qi);
	d = std::make_shared<Download>(ucPtr, qs.qi);
#ifdef DEBUG_TRANSFERS
	const string& sourcePath = q->getSourcePath();
	if (!sourcePath.empty()) d->setDownloadPath(sourcePath);
#endif

	const bool isFile = d->getType() == Transfer::TYPE_FILE && q->getSize() != -1;
	const bool hasTTH = !q->getTTH().isZero();
	bool treeValid = false;
	if (isFile && hasTTH)
	{
		auto db = DatabaseManager::getInstance();
		auto hashDb = db->getHashDatabaseConnection();
		if (hashDb)
		{
			treeValid = db->getTree(hashDb, q->getTTH(), d->getTigerTree());
			db->putHashDatabaseConnection(hashDb);
		}
		if (treeValid)
		{
			if (q->updateBlockSize(d->getTigerTree().getBlockSize()))
			{
				if (segmentAdded)
				{
					q->removeDownload(qs.seg);
					segmentAdded = false;
				}
				QueueRLock(*QueueItem::g_cs);
				auto it = q->findSourceL(u);
				if (it != q->getSourcesL().end())
				{
					const auto& src = it->second;
					qs.seg = q->getNextSegmentL(gsp, src.partialSource, nullptr);
					qs.sourceFlags = src.getFlags();
				}
				else
					qs.seg = Segment(-1, 0);
			}
			q->changeExtraFlags(QueueItem::XFLAG_ALLOW_SEGMENTS, QueueItem::XFLAG_ALLOW_SEGMENTS);
		}
		else if (ucPtr->isSet(UserConnection::FLAG_SUPPORTS_TTHL) && !(qs.sourceFlags & QueueItem::Source::FLAG_NO_TREE) && q->getSize() > MIN_BLOCK_SIZE)
		{
			// Get the tree unless the file is small (for small files, we'd probably only get the root anyway)
			d->setType(Transfer::TYPE_TREE);
			d->getTigerTree().setFileSize(q->getSize());
			if (segmentAdded)
			{
				q->removeDownload(qs.seg);
				segmentAdded = false;
			}
			qs.seg = Segment(0, -1);
		}
		else
		{
			if (segmentAdded)
			{
				q->removeDownload(qs.seg);
				segmentAdded = false;
			}
			qs.seg = Segment(0, -1);
		}
	}

	if (qs.seg.getSize() == 0)
	{
		dcassert(!segmentAdded);
		errorInfo.error = QueueItem::ERROR_NO_NEEDED_PART;
		errorInfo.target = q->getTarget();
		errorInfo.size = q->getSize();
		errorInfo.type = Transfer::TYPE_FILE;
		d.reset();
		return d;
	}

	if (segmentAdded)
		q->setDownloadForSegment(qs.seg, d);
	else
		q->addDownload(d);

	d->setSegment(qs.seg);
	source->setDownload(d);
	userQueue.setRunningDownload(qs.qi, u);

	if (qs.seg.getStart() || (qs.seg.getSize() != -1 && qs.seg.getEnd() != q->getSize()))
		d->setFlag(Download::FLAG_CHUNKED);

	dcdebug("found %s\n", q->getTarget().c_str());
	return d;
}

class TreeOutputStream : public OutputStream
{
	public:
		explicit TreeOutputStream(TigerTree& tree) : tree(tree), bufPos(0)
		{
		}

		size_t write(const void* data, size_t len) override
		{
			size_t pos = 0;
			const uint8_t* b = static_cast<const uint8_t*>(data);
			while (pos < len)
			{
				size_t left = len - pos;
				if (bufPos == 0 && left >= TigerTree::BYTES)
				{
					tree.getLeaves().push_back(TTHValue(b + pos));
					pos += TigerTree::BYTES;
				}
				else
				{
					size_t bytes = min(TigerTree::BYTES - bufPos, left);
					memcpy(buf + bufPos, b + pos, bytes);
					bufPos += bytes;
					pos += bytes;
					if (bufPos == TigerTree::BYTES)
					{
						tree.getLeaves().push_back(TTHValue(buf));
						bufPos = 0;
					}
				}
			}
			return len;
		}

		size_t flushBuffers(bool force) override
		{
			// TODO: check bufPos
			return 0;
		}

	private:
		TigerTree& tree;
		uint8_t buf[TigerTree::BYTES];
		size_t bufPos;
};

static string getFileListTempTarget(const DownloadPtr& d)
{
	string target = d->getPath();
	if (d->isSet(Download::FLAG_XML_BZ_LIST))
		target += ".xml.bz2";
	else
		target += ".xml";
	target += dctmpExtension;
	return target;
}

void QueueManager::setFile(const DownloadPtr& d)
{
	if (d->getType() == Transfer::TYPE_FILE)
	{
		const QueueItemPtr qi = d->getQueueItem();
		if (qi->getExtraFlags() & QueueItem::XFLAG_REMOVED)
			throw QueueException(QueueException::TARGET_REMOVED, STRING(TARGET_REMOVED));

		if (d->getOverlapped())
		{
			d->setOverlapped(false);

			const bool isFound = qi->disconnectSlow(d);
			if (!isFound)
			{
				// slow chunk already finished ???
				throw QueueException(QueueException::ALREADY_FINISHED, STRING(DOWNLOAD_FINISHED_IDLE));
			}
		}

		const string target = d->getDownloadTarget();

		if (qi->getDownloadedBytes() > 0)
		{
			qi->lockAttributes();
			string tempTarget = qi->getTempTargetL();
			qi->unlockAttributes();
			if (!File::isExist(tempTarget))
			{
				// When trying the download the next time, the resume pos will be reset
				qi->setLastSize(0);
				throw QueueException(QueueException::TARGET_REMOVED, STRING(TARGET_REMOVED));
			}
		}
		else
		{
			File::ensureDirectory(target);
		}

		// open stream for both writing and reading, because UploadManager can request reading from it
		const int64_t treeFileSize = d->getTigerTree().getFileSize();
		const int64_t fileSize = treeFileSize ? treeFileSize : d->getSize();
		int64_t diffFileSize = 0;
		if (qi->getSize() == -1)
		{
			diffFileSize = fileSize;
			qi->setSize(fileSize);
		}

		auto f = new SharedFileStream(target, File::RW, File::OPEN | File::CREATE | File::SHARED | File::NO_CACHE_HINT, fileSize);
		if (qi->getSize() != qi->getLastSize())
		{
			if (f->getFastFileSize() != qi->getSize())
			{
				try
				{
					f->setSize(fileSize);
					qi->setLastSize(fileSize);
				}
				catch (Exception&)
				{
					delete f;
					throw;
				}
			}
		}
		else
		{
			dcdebug("Skip for file %s qi->getSize() == qi->getLastSize()\r\n", target.c_str());
		}

		f->setPos(d->getSegment().getStart());
		d->setDownloadFile(f);
		if (diffFileSize)
			fire(QueueManagerListener::FileSizeUpdated(), qi, diffFileSize);
	}
	else if (d->getType() == Transfer::TYPE_FULL_LIST)
	{
		const QueueItemPtr qi = d->getQueueItem();
		if (qi->getExtraFlags() & QueueItem::XFLAG_REMOVED)
			throw QueueException(QueueException::TARGET_REMOVED, STRING(TARGET_REMOVED));

		// set filelist's size
		int64_t oldSize = qi->getSize();
		if (oldSize < 0) oldSize = 0;
		int64_t diffFileSize = d->getSize() - oldSize;
		qi->setSize(d->getSize());

		string target = getFileListTempTarget(d);
		File::ensureDirectory(target);
		d->setDownloadFile(new File(target, File::WRITE, File::OPEN | File::TRUNCATE | File::CREATE));
		if (diffFileSize)
			fire(QueueManagerListener::FileSizeUpdated(), qi, diffFileSize);
	}
	else if (d->getType() == Transfer::TYPE_PARTIAL_LIST)
	{
		d->setDownloadFile(new StringOutputStream(d->getFileListBuffer()));
	}
	else if (d->getType() == Transfer::TYPE_TREE)
	{
		d->setDownloadFile(new TreeOutputStream(d->getTigerTree()));
	}
}

void QueueManager::moveFile(const string& source, const string& target, int64_t moverLimit)
{
	string sourcePath = Util::getFilePath(source);
	string destPath = Util::getFilePath(target);

	bool moveToOtherDir = sourcePath != destPath;
	bool useMover = false;
	if (moveToOtherDir)
	{
		if (!File::isExist(destPath))
			File::ensureDirectory(destPath);
		useMover = moverLimit < 0 ? false : File::getSize(source) > moverLimit;
	}
	if (useMover)
	{
		FileMoverJob* job = new FileMoverJob(*this, FileMoverJob::MOVE_FILE, source, target, moveToOtherDir, QueueItemPtr());
		if (!fileMover.addJob(job))
			delete job;
		return;
	}
	if (File::renameFile(source, target))
		return;
	LogManager::message(STRING_F(UNABLE_TO_RENAME_FMT, source % target % Util::translateError()));
	int64_t size = File::getSize(source);
	if (size < 0)
		return;
	if (size > MOVER_LIMIT)
	{
		FileMoverJob* job = new FileMoverJob(*this, FileMoverJob::MOVE_FILE_RETRY, source, target, moveToOtherDir, QueueItemPtr());
		if (!fileMover.addJob(job))
			delete job;
		return;
	}
	if (File::copyFile(source, target))
	{
		File::deleteFile(source);
		return;
	}
	if (moveToOtherDir)
		keepFileInTempDir(source, target);
}

// try to remove the .dctmp suffix keeping it in the temp directory
void QueueManager::keepFileInTempDir(const string& source, const string& target)
{
	string newTarget = Util::getFilePath(source) + Util::getFileName(target);
	if (!File::renameFile(source, newTarget))
		LogManager::message(STRING_F(UNABLE_TO_RENAME_FMT, source % newTarget % Util::translateError()));
}

void QueueManager::copyFile(const string& source, const string& target, QueueItemPtr& qi)
{
	auto flags = qi->getExtraFlags();
	dcassert(flags & QueueItem::XFLAG_COPYING);
	if (flags & QueueItem::XFLAG_REMOVED)
		return;
	string tempTarget = QueueItem::getDCTempName(target, nullptr);
	File::ensureDirectory(tempTarget);
	if (!File::copyFile(source, tempTarget))
	{
		LogManager::message(STRING_F(ERROR_COPYING_FILE, target % Util::translateError()));
		return;
	}
	if (!File::renameFile(tempTarget, target))
	{
		File::deleteFile(tempTarget);
		return;
	}
	dcassert(!userQueue.isInQueue(qi));
	removeItem(qi, false);
}

void QueueManager::fireStatusUpdated(const QueueItemPtr& qi)
{
	if (!ClientManager::isBeforeShutdown())
		fire(QueueManagerListener::StatusUpdated(), qi);
}

void QueueManager::rechecked(const QueueItemPtr& qi)
{
	fire(QueueManagerListener::RecheckDone(), qi->getTarget());
	fireStatusUpdated(qi);
	setDirty();
}

void QueueManager::putDownload(DownloadPtr download, bool finished, bool reportFinish) noexcept
{
	UserList getConn;
	unique_ptr<DirectoryItem> processListDirItem;
	string processListFileName;
	const HintedUser hintedUser = download->getHintedUser();
	UserPtr user = download->getUser();

	dcassert(user);

	QueueItem::MaskType processListFlags = 0;
	bool downloadList = false;

	download->resetDownloadFile();
	if (download->getType() == Transfer::TYPE_PARTIAL_LIST)
	{
		QueueItemPtr q = fileQueue.findTarget(download->getQueueItem()->getTarget());
		if (q)
		{
			if (!download->getFileListBuffer().empty())
			{
				int dirFlags = 0;
				if (q->getExtraFlags() & (QueueItem::XFLAG_MATCH_QUEUE | QueueItem::XFLAG_DOWNLOAD_DIR))
				{
					q->lockAttributes();
					string tempTarget = q->getTempTargetL();
					q->unlockAttributes();
					LOCK(csDirectories);
					auto dp = directories.equal_range(download->getUser());
					for (auto i = dp.first; i != dp.second; ++i)
						if (stricmp(tempTarget, i->second.getName()) == 0)
						{
							dirFlags = i->second.getFlags();
							processListDirItem.reset(new DirectoryItem(std::move(i->second)));
							directories.erase(i);
							break;
						}
				}
				if (dirFlags)
				{
					processListFileName = std::move(download->getFileListBuffer());
					processListFlags = dirFlags | DIR_FLAG_TEXT;
					if (download->isSet(Download::FLAG_TTH_LIST))
						processListFlags |= DIR_FLAG_TTH_LIST;
				}
				else if (q->getFlags() & QueueItem::FLAG_USER_CHECK)
				{
					// User check completed
					// TODO: process partial list
					user->changeFlags(0, User::USER_CHECK_RUNNING | User::USER_CHECK_FAILED);
				}
				else
					fire(QueueManagerListener::PartialList(), hintedUser, download->getFileListBuffer());
			}
			else if (q->getFlags() & QueueItem::FLAG_USER_CHECK)
			{
				user->changeFlags(User::USER_CHECK_FAILED, User::USER_CHECK_RUNNING);
			}
			else
			{
				// partial filelist probably failed, redownload full list
				dcassert(!finished);
				processListFlags = 0;
				{
					LOCK(csDirectories);
					auto dp = directories.equal_range(download->getUser());
					for (auto i = dp.first; i != dp.second; ++i)
						processListFlags |= i->second.getFlags();
				}
				if (processListFlags) downloadList = true;
			}

			removeItem(q, true);
		}
	}
	else
	{
		QueueItemPtr q = fileQueue.findTarget(download->getQueueItem()->getTarget());
		if (q)
		{
			if (download->getType() == Transfer::TYPE_FULL_LIST)
			{
				QueueItem::MaskType listFlag = download->isSet(Download::FLAG_XML_BZ_LIST) ? QueueItem::XFLAG_XML_BZLIST : 0;
				q->changeExtraFlags(listFlag, QueueItem::XFLAG_XML_BZLIST);
			}

			if (finished)
			{
				auto db = DatabaseManager::getInstance();
				auto hashDb = db->getHashDatabaseConnection();
				if (download->getType() == Transfer::TYPE_TREE)
				{
					// Got a full tree, now add it to the database
					dcassert(download->getTigerTree().getFileSize() != 0);
					bool hasTree = download->getTigerTree().getLeaves().size() >= 2;
					if (hashDb && hasTree)
						db->addTree(hashDb, download->getTigerTree());
					else
					{
						QueueWLock(*QueueItem::g_cs);
						auto i = q->findSourceL(user);
						if (i != q->sources.end())
							i->second.setFlag(QueueItem::Source::FLAG_NO_TREE);
					}
					userQueue.removeDownload(q, download->getUser());
					if (hasTree)
						q->changeExtraFlags(QueueItem::XFLAG_ALLOW_SEGMENTS, QueueItem::XFLAG_ALLOW_SEGMENTS);
					fireStatusUpdated(q);
				}
				else
				{
					// Now, let's see if this was a directory download filelist...
					auto extraFlags = q->getExtraFlags();
					if (extraFlags & (QueueItem::XFLAG_DOWNLOAD_DIR | QueueItem::XFLAG_MATCH_QUEUE))
					{
						processListFileName = download->getPath() + q->getListExt();
						processListFlags = queueItemFlagsToDirFlags(extraFlags);
					}
					const bool isFile = download->getType() == Transfer::TYPE_FILE;
					bool isFinishedFile = false;

					string dir;
					if (download->getType() == Transfer::TYPE_FULL_LIST)
					{
						q->lockAttributes();
						dir = q->getTempTargetL();
						q->unlockAttributes();
						q->addSegment(Segment(0, q->getSize()));
						string destName = download->getPath() + q->getListExt();
						string sourceName = destName + dctmpExtension;
						File::renameFile(sourceName, destName);
					}
					else if (isFile)
					{
						download->setOverlapped(false);
						q->addSegment(download->getSegment());
						isFinishedFile = q->isFinished();
					}

					if (!isFile || isFinishedFile)
					{
						auto ss = SettingsManager::instance.getCoreSettings();
						ss->lockRead();
						bool logDownloads = ss->getBool(Conf::LOG_DOWNLOADS);
						bool logFileListTransfers = ss->getBool(Conf::LOG_FILELIST_TRANSFERS);
#ifdef _WIN32
						bool saveTTHToStream = ss->getBool(Conf::SAVE_TTH_IN_NTFS_FILESTREAM);
						int minSizeToSaveTTH = ss->getInt(Conf::SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM);
#endif
						ss->unlockRead();

						const string& path = download->getPath();
						if (!(q->getFlags() & (QueueItem::FLAG_USER_GET_IP | QueueItem::FLAG_USER_CHECK)))
						{
							string tempTarget;
							if (isFile)
							{
								q->lockAttributes();
								tempTarget = q->getTempTargetL();
								q->unlockAttributes();
								// For partial-share, abort upload first to move file correctly
								if (!tempTarget.empty())
									UploadManager::getInstance()->abortUpload(tempTarget);
								q->disconnectOthers(download);
							}

							// Check if we need to move the file
							if (isFile && !tempTarget.empty() && stricmp(path, tempTarget) != 0)
								moveFile(tempTarget, path, MOVER_LIMIT);

							SharedFileStream::cleanup();
							if (logDownloads && (logFileListTransfers || isFile))
							{
								StringMap params;
								download->getParams(params);
								LOG(DOWNLOAD, params);
							}
						}

						if (hashDb && !q->getTTH().isZero() && !(q->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)))
							hashDb->putFileInfo(q->getTTH().data, DatabaseManager::FLAG_DOWNLOADED, q->getSize(), path.empty() ? nullptr : &path, false);
#ifdef _WIN32
						if (isFinishedFile && !SysVersion::isWine() && saveTTHToStream &&
						    q->getSize() >= (int64_t) minSizeToSaveTTH << 20)
						{
							const string directory = Util::getFilePath(path);
							if (!directory.empty() && ShareManager::getInstance()->isDirectoryShared(directory))
							{
								const TigerTree& tree = download->getTigerTree();
								if (!tree.getRoot().isZero() && tree.getLeaves().size() > 1)
									HashManager::saveTree(path, tree);
							}
						}
#endif

						if (!ClientManager::isBeforeShutdown())
						{
							fire(QueueManagerListener::Finished(), q, dir, download);
						}
						if ((q->getFlags() & QueueItem::FLAG_DCLST_LIST) && (q->getExtraFlags() & QueueItem::XFLAG_DOWNLOAD_CONTENTS))
						{
							addDclstFile(q->getTarget());
						}
						removeItem(q, true);
					}
					else
					{
						userQueue.removeDownload(q, download->getUser());
						if (download->getType() != Transfer::TYPE_FILE || (reportFinish && q->isWaiting()))
						{
							fireStatusUpdated(q);
						}
					}
					setDirty();
				}
				if (hashDb)
					db->putHashDatabaseConnection(hashDb);
			}
			else
			{
				if (download->getType() != Transfer::TYPE_TREE)
				{
					q->updateDownloadedBytesAndSpeed();
					if (q->getFlags() & QueueItem::FLAG_USER_LIST)
					{
						// Blah...no use keeping an unfinished file list...
						File::deleteFile(download->getPath() + q->getListExt() + dctmpExtension);
						if (download->getReasonCode() == Download::REASON_CODE_FILE_UNAVAILABLE)
						{
							removeItem(q, true);
							download.reset();
							return;
						}
					}
					if (download->getType() == Transfer::TYPE_FILE)
					{
						// mark partially downloaded chunk, but align it to block size
						int64_t downloaded = download->getPos();
						downloaded -= downloaded % download->getTigerTree().getBlockSize();

						if (downloaded > 0)
						{
							// since download is not finished, it should never happen that downloaded size is same as segment size
							//dcassert(downloaded < download->getSize());
							q->addSegment(Segment(download->getStartPos(), downloaded));
							setDirty();
						}
					}
				}

				q->lockAttributes();
				auto prio = q->getPriorityL();
				q->unlockAttributes();
				if (prio != QueueItem::PAUSED)
				{
					const string& reason = download->getReasonText();
					if (reason.empty())
					{
						q->getOnlineUsers(getConn);
					}
					else
					{
#ifdef _DEBUG
						LogManager::message("Skip get connection - reason = " + reason);
#endif
					}
				}

				userQueue.removeDownload(q, download->getUser());

				fireStatusUpdated(q);

				if (download->isSet(Download::FLAG_OVERLAP))
				{
					// overlapping segment disconnected, unoverlap original segment
					q->setOverlapped(download->getSegment(), false);
				}
			}
		}
		else if (download->getType() == Transfer::TYPE_FULL_LIST)
		{
			const string tmpTarget = getFileListTempTarget(download);
			if (File::isExist(tmpTarget))
			{
				if (!File::deleteFile(tmpTarget))
					SharedFileStream::deleteFile(tmpTarget);
			}
		}
		else if (download->getType() != Transfer::TYPE_TREE && (download->getQueueItem()->getExtraFlags() & QueueItem::XFLAG_REMOVED))
		{
			const string tmpTarget = download->getTempTarget();
			if (!tmpTarget.empty() && tmpTarget != download->getPath())
			{
				if (File::isExist(tmpTarget))
				{
					if (!File::deleteFile(tmpTarget))
						SharedFileStream::deleteFile(tmpTarget);
				}
			}
		}
	}
	download.reset();

	getDownloadConnections(getConn);

	if (!processListFileName.empty())
		processList(processListFileName, hintedUser, processListFlags, processListDirItem.get());

	// partial file list failed, redownload full list
	if (downloadList && user->isOnline())
	{
		try
		{
			addList(HintedUser(user, Util::emptyString), 0, dirFlagsToQueueItemFlags(processListFlags));
		}
		catch (const Exception&) {}
	}
}

static void logMatchedFiles(const UserPtr& user, int count)
{
	dcassert(user);
	string str = PLURAL_F(PLURAL_MATCHED_FILES, count);
	str += ": ";
	str += user->getLastNick();
	LogManager::message(str);
}

void QueueManager::processList(const string& name, const HintedUser& hintedUser, int flags, const DirectoryItem* dirItem)
{
	dcassert(hintedUser.user);
	if ((flags & (DIR_FLAG_TEXT | DIR_FLAG_TTH_LIST)) == (DIR_FLAG_TEXT | DIR_FLAG_TTH_LIST))
	{
		logMatchedFiles(hintedUser.user, matchTTHList(name, hintedUser.user));
		return;
	}

	std::atomic_bool unusedAbortFlag(false);
	DirectoryListing dirList(unusedAbortFlag);
	dirList.setHintedUser(hintedUser);
	try
	{
		if (flags & DIR_FLAG_TEXT)
		{
			MemoryInputStream mis(name);
			dirList.loadXML(mis, nullptr, false);
		}
		else
		{
			dirList.loadFile(name, nullptr, false);
		}
	}
	catch (const Exception&)
	{
		if (!(flags & DIR_FLAG_TEXT))
			LogManager::message(STRING(UNABLE_TO_OPEN_FILELIST) + ' ' + name);
		return;
	}
	if (!dirList.getRoot()->getTotalFileCount())
		return;
	if (flags & DIR_FLAG_DOWNLOAD_DIRECTORY)
	{
		struct DownloadItem
		{
			DirectoryListing::Directory* d;
			string target;
			QueueItem::Priority p;
		};
		vector<DownloadItem> dl;
		if (flags & DIR_FLAG_TEXT)
		{
			// Process partial list, use supplied dirItem
			if (dirItem)
			{
				DirectoryListing::Directory* d = dirList.findDirPath(Util::toAdcFile(dirItem->getName()));
				if (d)
					dl.emplace_back(DownloadItem{d, dirItem->getTarget(), dirItem->getPriority()});
			}
		}
		else
		{
			// Process full list, remove all directories belonging to this user
			LOCK(csDirectories);
			const auto dp = directories.equal_range(hintedUser.user);
			for (auto i = dp.first; i != dp.second; ++i)
			{
				if (i->second.getFlags() & DIR_FLAG_DOWNLOAD_DIRECTORY)
				{
					const DirectoryItem& dir = i->second;
					DirectoryListing::Directory* d = dirList.findDirPath(Util::toAdcFile(dir.getName()));
					if (d)
						dl.emplace_back(DownloadItem{d, dir.getTarget(), dir.getPriority()});
				}
			}
		}
		bool getConn = false;
		for (const auto& di : dl)
			dirList.download(di.d, di.target, di.p, getConn);
	}
	if (!(flags & DIR_FLAG_TEXT))
	{
		LOCK(csDirectories);
		directories.erase(hintedUser.user);
	}
	if (flags & DIR_FLAG_MATCH_QUEUE)
	{
		logMatchedFiles(hintedUser.user, matchListing(dirList));
	}
}

QueueItem::MaskType QueueManager::getFlagsForFileName(const string& fileName) noexcept
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const string& pattern = ss->getString(Conf::WANT_END_FILES);
	csWantEndFiles.lock();
	if (pattern != wantEndFilesPattern)
	{
		wantEndFilesPattern = pattern;
		if (!Wildcards::regexFromPatternList(reWantEndFiles, wantEndFilesPattern, true))
			wantEndFilesPattern.clear();
	}
	bool result = std::regex_match(fileName, reWantEndFiles);
	csWantEndFiles.unlock();
	ss->unlockRead();
	return result ? QueueItem::FLAG_WANT_END : 0;
}

QueueItem::MaskType QueueManager::getFlagsForFileNameL(const string& fileName) const noexcept
{
	return std::regex_match(fileName, reWantEndFiles) ? QueueItem::FLAG_WANT_END : 0;
}

void QueueManager::removeAll()
{
	fileQueue.clearAll();
}

void QueueManager::removeItem(const QueueItemPtr& qi, bool removeFromUserQueue)
{
	if (removeFromUserQueue)
	{
		if (qi->getFlags() & QueueItem::FLAG_USER_CHECK)
		{
			UserPtr user = qi->getFirstSource();
			if (user) userCheckProcessFailure(user, -1, false);
		}
		userQueue.removeQueueItem(qi);
	}
	fileQueue.remove(qi);
	csBatch.lock();
	if (batchCounter)
	{
		batchRemove.push_back(qi);
		csBatch.unlock();
	}
	else
	{
		csBatch.unlock();
		fire(QueueManagerListener::Removed(), qi);
	}
}

bool QueueManager::removeTarget(const string& target)
{
#if 0
	string logMessage = "removeTarget: " + target;
	DumpDebugMessage(_T("queue-debug.log"), logMessage.c_str(), logMessage.length(), true);
#endif

	QueueItemPtr q = fileQueue.findTarget(target);
	if (!q)
		return false;

	auto flags = q->getFlags();

	q->lockAttributes();
	auto extraFlags = q->getExtraFlagsL();
	const string tempTarget = q->getTempTargetL();
	q->unlockAttributes();

	if (extraFlags & (QueueItem::XFLAG_MATCH_QUEUE | QueueItem::XFLAG_DOWNLOAD_DIR))
	{
		UserPtr user;
		{
			QueueRLock(*QueueItem::g_cs);
			dcassert(q->getSourcesL().size() == 1);
			user = q->getSourcesL().begin()->first;
		}
		{
			LOCK(csDirectories);
			if (flags & QueueItem::FLAG_PARTIAL_LIST)
			{
				auto dp = directories.equal_range(user);
				for (auto i = dp.first; i != dp.second; ++i)
					if (stricmp(tempTarget, i->second.getName()) == 0)
					{
						directories.erase(i);
						break;
					}
			}
			else
			{
				directories.erase(user);
			}
		}
	}

	if (!q->getTTH().isZero() && !(flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)))
	{
		auto db = DatabaseManager::getInstance();
		auto hashDb = db->getHashDatabaseConnection();
		if (hashDb)
		{
			hashDb->putFileInfo(q->getTTH().data, DatabaseManager::FLAG_DOWNLOAD_CANCELED, q->getSize(), nullptr, false);
			db->putHashDatabaseConnection(hashDb);
		}
	}

	if (!(flags & QueueItem::FLAG_USER_LIST) && !tempTarget.empty())
	{
		// For partial file sharing
		UploadManager::getInstance()->abortUpload(tempTarget);
	}

	UserList x;
	if (q->isRunning())
	{
		q->getUsers(x);
	}
	else if (!(flags & QueueItem::FLAG_USER_LIST) && !tempTarget.empty() && tempTarget != q->getTarget())
	{
		if (File::isExist(tempTarget))
		{
			if (!File::deleteFile(tempTarget))
				SharedFileStream::deleteFile(tempTarget);
		}
	}

	q->changeExtraFlags(QueueItem::XFLAG_REMOVED, QueueItem::XFLAG_REMOVED);
	removeItem(q, true);
	setDirty();

	auto cm = ConnectionManager::getInstance();
	for (auto i = x.cbegin(); i != x.cend(); ++i)
		cm->disconnect(*i, true);
	return true;
}

void QueueManager::removeSource(const string& target, const UserPtr& user, Flags::MaskType reason, bool removeConn /* = true */) noexcept
{
	bool isRunning = false;
	bool removeCompletely = false;
	do
	{
		QueueWLock(*QueueItem::g_cs);
		QueueItemPtr q = fileQueue.findTarget(target);
		if (!q)
			return;

		auto source = q->findSourceL(user);
		if (source == q->sources.end()) return;
		if (reason == QueueItem::Source::FLAG_NO_TREE)
			source->second.setFlag(reason);

		if (q->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
		{
			removeCompletely = true;
			break;
		}

		if (q->isRunning() && userQueue.getRunning(user) == q)
		{
			isRunning = true;
			userQueue.removeDownload(q, user);
		}
		userQueue.removeUserL(q, user, true);
		q->removeSourceL(user, reason);

		setDirty();
	}
	while (false);

	if (isRunning && removeConn)
	{
		ConnectionManager::getInstance()->disconnect(user, true);
	}
	if (removeCompletely)
	{
		removeTarget(target);
	}
}

void QueueManager::removeSource(const UserPtr& user, Flags::MaskType reason) noexcept
{
	// @todo remove from finished items
	bool dirty = false;
	bool disconnect = false;
	list<string> targetsToRemove;
	{
		QueueWLock(*QueueItem::g_cs);
		for (int p = QueueItem::LAST - 1; p >= 0; --p)
		{
			auto& ulm = userQueue.userQueueMap[p];
			auto i = ulm.find(user);
			if (i != ulm.end())
			{
				QueueItemList& ql = i->second;
				for (QueueItemPtr& qi : ql)
				{
					qi->removeSourceL(user, reason);
					if (qi->getFlags() & QueueItem::FLAG_USER_LIST)
						targetsToRemove.push_back(qi->getTarget());
					else
						dirty = true;
				}
				ulm.erase(i);
			}
		}
	}
	QueueItemPtr qi = userQueue.getRunning(user);
	if (qi && !(qi->getFlags() & QueueItem::FLAG_USER_LIST))
	{
		userQueue.removeDownload(qi, user);
		disconnect = dirty = true;
		fireStatusUpdated(qi);
	}
	if (disconnect)
		ConnectionManager::getInstance()->disconnect(user, true);
	for (const string& target : targetsToRemove)
		removeTarget(target);
	if (dirty)
		setDirty();
}

void QueueManager::setPriority(const string& target, QueueItem::Priority p, bool resetAutoPriority) noexcept
{
	UserList getConn;
	bool isRunning = false;

	QueueItemPtr q = fileQueue.findTarget(target);
	if (!q) return;
	bool upd = false;

	q->lockAttributes();
	auto extraFlags = q->getExtraFlagsL();
	auto oldPriority = q->getPriorityL();
	if (resetAutoPriority && (extraFlags & QueueItem::XFLAG_AUTO_PRIORITY))
	{
		q->setExtraFlagsL(extraFlags & ~QueueItem::XFLAG_AUTO_PRIORITY);
		upd = true;
	}
	q->unlockAttributes();

	if (oldPriority != p)
	{
		bool isFinished = q->isFinished();
		if (!isFinished)
		{
			isRunning = q->isRunning();

			if (oldPriority == QueueItem::PAUSED || p == QueueItem::HIGHEST)
			{
				// Problem, we have to request connections to all these users...
				q->getOnlineUsers(getConn);
			}
			userQueue.setQIPriority(q, p); // remove and re-add the item
#ifdef _DEBUG
			LogManager::message("QueueManager userQueue.setQIPriority q->getTarget = " + q->getTarget());
#endif
			upd = true;
		}
	}
	if (upd)
	{
		setDirty();
		fireStatusUpdated(q);
	}

	if (p == QueueItem::PAUSED)
	{
		if (isRunning)
			DownloadManager::getInstance()->abortDownload(target);
	}
	else
		getDownloadConnections(getConn);
}

void QueueManager::setAutoPriority(const string& target, bool ap)
{
	QueueItemPtr q = fileQueue.findTarget(target);
	if (!q) return;

	vector<pair<string, QueueItem::Priority>> priorities;
	q->lockAttributes();
	QueueItem::MaskType oldFlags = q->getExtraFlagsL();
	QueueItem::MaskType newFlags = oldFlags;
	if (ap)
		newFlags |= QueueItem::XFLAG_AUTO_PRIORITY;
	else
		newFlags &= ~QueueItem::XFLAG_AUTO_PRIORITY;
	if (newFlags != oldFlags)
	{
		q->setExtraFlagsL(newFlags);
		QueueItem::Priority prio = ap ? q->calculateAutoPriorityL() : QueueItem::DEFAULT;
		q->unlockAttributes();
		if (ap)
			priorities.push_back(make_pair(q->getTarget(), prio));
		setDirty();
		fireStatusUpdated(q);
	}
	else
		q->unlockAttributes();

	for (auto p = priorities.cbegin(); p != priorities.cend(); ++p)
		setPriority(p->first, p->second, false);
}

#define LIT(n) n, sizeof(n)-1

void QueueManager::saveQueue(bool force) noexcept
{
	if (!dirty && !force)
		return;

#if 0
	string logMessage = "Saving queue";
	DumpDebugMessage(_T("queue-debug.log"), logMessage.c_str(), logMessage.length(), true);
#endif

	try
	{
		string queueFile = getQueueFile();
		string tempFile = queueFile + ".tmp";
		File ff(tempFile, File::WRITE, File::CREATE | File::TRUNCATE);
		BufferedOutputStream<false> f(&ff, 2 * 1024 * 1024);

		f.write(SimpleXML::utf8Header);
		f.write(LIT("<Downloads Version=\"" VERSION_STR "\">\r\n"));
		string tmp;
		string b32tmp;
		vector<Segment> done;

		QueueRLock(*QueueItem::g_cs);
		{
			LockFileQueueShared lockQueue;
			const auto& queue = lockQueue.getQueueL();
			for (auto i = queue.cbegin(); i != queue.cend(); ++i)
			{
				auto qi = i->second;
				if (!(qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)))
				{
					qi->lockAttributes();
					int priority = (int) qi->getPriorityL();
					int autoPriority = (qi->getExtraFlagsL() & QueueItem::XFLAG_AUTO_PRIORITY) ? 1 : 0;
					uint8_t maxSegments = qi->getMaxSegmentsL();
					qi->unlockAttributes();

					f.write(LIT("\t<Download Target=\""));
					f.write(SimpleXML::escape(qi->getTarget(), tmp, true));
					f.write(LIT("\" Size=\""));
					f.write(Util::toString(qi->getSize()));
					f.write(LIT("\" Priority=\""));
					f.write(Util::toString(priority));
					f.write(LIT("\" Added=\""));
					f.write(Util::toString(qi->getAdded()));
					b32tmp.clear();
					f.write(LIT("\" TTH=\""));
					f.write(qi->getTTH().toBase32(b32tmp));
					qi->getDoneSegments(done);
					if (!done.empty())
					{
						qi->lockAttributes();
						string tempTarget = qi->getTempTargetL();
						qi->unlockAttributes();
						if (!tempTarget.empty())
						{
							f.write(LIT("\" TempTarget=\""));
							f.write(SimpleXML::escape(tempTarget, tmp, true));
						}
					}
					f.write(LIT("\" AutoPriority=\""));
					f.write(Util::toString(autoPriority));
					f.write(LIT("\" MaxSegments=\""));
					f.write(Util::toString(maxSegments));

					f.write(LIT("\">\r\n"));

					for (auto j = done.cbegin(); j != done.cend(); ++j)
					{
						f.write(LIT("\t\t<Segment Start=\""));
						f.write(Util::toString(j->getStart()));
						f.write(LIT("\" Size=\""));
						f.write(Util::toString(j->getSize()));
						f.write(LIT("\"/>\r\n"));
					}

					const auto& sources = qi->getSourcesL();
					for (auto j = sources.cbegin(); j != sources.cend(); ++j)
					{
						const UserPtr& user = j->first;
						const QueueItem::Source& source = j->second;
						if (source.isSet(QueueItem::Source::FLAG_PARTIAL)/* || user->hint == "DHT"*/) continue;

						const CID& cid = user->getCID();
#if 0
						const string& hint = user.hint;
#endif
						f.write(LIT("\t\t<Source CID=\""));
						f.write(cid.toBase32());
						f.write(LIT("\" Nick=\""));
						f.write(SimpleXML::escape(user->getLastNick(), tmp, true));
#if 0
						f.write(SimpleXML::escape(ClientManager::getInstance()->getNicks(cid, hint)[0], tmp, true));
						if (!hint.empty())
						{
							f.write(LIT("\" HubHint=\""));
							f.write(hint);
						}
#endif
						f.write(LIT("\"/>\r\n"));
					}

					f.write(LIT("\t</Download>\r\n"));
				}
			}
		}

		f.write(LIT("</Downloads>\r\n"));
		f.flushBuffers(true);
		ff.close();

		File::copyFile(queueFile, queueFile + ".bak");
		File::renameFile(tempFile, queueFile);

		dirty = false;
	}
	catch(...)
	{
		// ...
	}
	// Put this here to avoid very many saves tries when disk is full...
	lastSave = GET_TICK();
}

class QueueLoader : public SimpleXMLReader::CallBack
{
	public:
		QueueLoader() : cur(nullptr), isInDownloads(false)
		{
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockRead();
			enableAutoDisconnect = ss->getBool(Conf::ENABLE_AUTO_DISCONNECT);
			ss->unlockRead();
#endif
			qm = QueueManager::getInstance();
#ifdef USE_QUEUE_RWLOCK
			QueueItem::g_cs->acquireExclusive();
			QueueManager::fileQueue.csFQ->acquireExclusive();
#else
			QueueItem::g_cs->lock();
			QueueManager::fileQueue.csFQ->lock();
#endif
		}
		~QueueLoader()
		{
			QueueManager::fileQueue.generationId = QueueManager::fileQueue.empty() ? 0 : 1;
#ifdef USE_QUEUE_RWLOCK
			QueueManager::fileQueue.csFQ->releaseExclusive();
			QueueItem::g_cs->releaseExclusive();
#else
			QueueManager::fileQueue.csFQ->unlock();
			QueueItem::g_cs->unlock();
#endif
		}
		void startTag(const string& name, StringPairList& attribs, bool simple);
		void endTag(const string& name, const string& data);

	private:
		string target;

		QueueManager* qm;
		QueueItemPtr cur;
		bool isInDownloads;
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
		bool enableAutoDisconnect;
#endif
};

void QueueManager::loadQueue() noexcept
{
	try
	{
		File f(getQueueFile(), File::READ, File::OPEN);
		QueueLoader l;
		SimpleXMLReader(&l).parse(f);
	}
	catch (const Exception&)
	{
	}
	dirty = false;
}

static const string sDownload = "Download";
static const string sTempTarget = "TempTarget";
static const string sTarget = "Target";
static const string sSize = "Size";
static const string sDownloaded = "Downloaded";
static const string sPriority = "Priority";
static const string sSource = "Source";
static const string sNick = "Nick";
static const string sDirectory = "Directory";
static const string sAdded = "Added";
static const string sTTH = "TTH";
static const string sCID = "CID";
static const string sHubHint = "HubHint";
static const string sSegment = "Segment";
static const string sStart = "Start";
static const string sAutoPriority = "AutoPriority";
static const string sMaxSegments = "MaxSegments";

void QueueLoader::startTag(const string& name, StringPairList& attribs, bool simple)
{
	if (!isInDownloads && name == "Downloads")
	{
		isInDownloads = true;
	}
	else if (isInDownloads)
	{
		if (cur == nullptr && name == sDownload)
		{
			int64_t size = Util::toInt64(getAttrib(attribs, sSize, 1));
			if (size <= 0)
				return;
			const string& tthRoot = getAttrib(attribs, sTTH, 5);
			if (tthRoot.empty())
				return;

			try
			{
				const string& tgt = getAttrib(attribs, sTarget, 0);
				// @todo do something better about existing files
				target = QueueManager::checkTarget(tgt,  /*checkExistence*/ -1);
				if (target.empty())
					return;
			}
			catch (const Exception&)
			{
				return;
			}
			QueueItem::Priority p = (QueueItem::Priority)Util::toInt(getAttrib(attribs, sPriority, 3));
			time_t added = static_cast<time_t>(Util::toInt(getAttrib(attribs, sAdded, 4)));

			const string& tempTarget = getAttrib(attribs, sTempTarget, 5);
			int maxSegments = Util::toInt(getAttrib(attribs, sMaxSegments, 5));
			if (maxSegments < 0) maxSegments = 0;
			else if (maxSegments > 255) maxSegments = 255;

			int64_t downloaded = Util::toInt64(getAttrib(attribs, sDownloaded, 5));
			if (downloaded > size || downloaded < 0)
				downloaded = 0;

			if (added == 0)
				added = GET_TIME();

			if (!maxSegments)
				maxSegments = QueueManager::FileQueue::getMaxSegments(size);

			string fileName = Util::getFileName(target);
			QueueItem::MaskType flags = qm->getFlagsForFileName(fileName);
			QueueItem::MaskType extraFlags = 0;

			if (Util::toInt(getAttrib(attribs, sAutoPriority, 6)) == 1)
				extraFlags |= QueueItem::XFLAG_AUTO_PRIORITY;
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
			if (enableAutoDisconnect)
				extraFlags |= QueueItem::XFLAG_AUTODROP;
#endif
			auto qi = std::make_shared<QueueItem>(target, size, p, flags, extraFlags, added, TTHValue(tthRoot), maxSegments, tempTarget);
			QueueManager::checkAntifragFile(tempTarget, flags);

			if (QueueManager::fileQueue.addL(qi))
			{
				if (downloaded > 0)
					qi->addSegment(Segment(0, downloaded));
				if (simple)
				{
					qi->updateDownloadedBytes();
					qi->lockAttributes();
					qi->setPriorityL(qi->calculateAutoPriorityL());
					qi->unlockAttributes();
				}
			}
			else
				qi.reset();
			if (!simple)
				cur = qi;
		}
		else if (name == sSegment)
		{
			if (cur)
			{
				int64_t start = Util::toInt64(getAttrib(attribs, sStart, 0));
				int64_t size = Util::toInt64(getAttrib(attribs, sSize, 1));
				if (size > 0 && size <= cur->getSize() && start >= 0 && start + size <= cur->getSize())
					cur->addSegment(Segment(start, size));
			}
		}
		else if (name == sSource)
		{
			if (cur)
			{
				const string cid = getAttrib(attribs, sCID, 0);
				UserPtr user;
				const string nick = getAttrib(attribs, sNick, 1);
				const string hubHint = getAttrib(attribs, sHubHint, 1);
				if (cid.length() != 39)
					user = ClientManager::getUser(nick, hubHint);
				else
					user = ClientManager::createUser(CID(cid), nick, hubHint);
				try { qm->addSourceL(cur, user, 0) && user->isOnline(); }
				catch (const Exception&) {}
			}
		}
	}
}

void QueueLoader::endTag(const string& name, const string&)
{
	if (isInDownloads)
	{
		if (name == sDownload)
		{
			if (cur)
			{
				cur->updateDownloadedBytes();
				cur->lockAttributes();
				cur->setPriorityL(cur->calculateAutoPriorityL());
				cur->unlockAttributes();
				cur = nullptr;
			}
		}
		else if (name == "Downloads")
		{
			isInDownloads = false;
		}
	}
}

// SearchManagerListener
void QueueManager::on(SearchManagerListener::SR, const SearchResult& sr) noexcept
{
	if (sr.getType() != SearchResult::TYPE_FILE || sr.getTTH().isZero()) return;

	bool added = false;
	bool wantConnection = false;
	bool downloadFileList = false;

	{
		QueueItemList matches;
		fileQueue.findQueueItems(matches, sr.getTTH());
		if (!matches.empty())
		{
			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockRead();
			bool autoSearchDlList = ss->getBool(Conf::AUTO_SEARCH_DL_LIST);
			int autoSearchMaxSources = ss->getInt(Conf::AUTO_SEARCH_MAX_SOURCES);
			ss->unlockRead();

			QueueWLock(*QueueItem::g_cs);
			for (auto i = matches.cbegin(); i != matches.cend(); ++i)
			{
				const QueueItemPtr& qi = *i;
				// Size compare to avoid popular spoof
				if (qi->getSize() == sr.getSize())
				{
					if (!qi->isSourceL(sr.getUser()))
					{
						if (qi->isFinished())
							break;  // don't add sources to already finished files
						try
						{
							downloadFileList = autoSearchDlList && !qi->hasOnlineSourcesL(autoSearchMaxSources);
							wantConnection = addSourceL(qi, sr.getUser(), 0);
							added = true;
						}
						catch (const Exception&)
						{
							// ...
						}
						break;
					}
					else
					{
						// Found a source and not downloading from it
						qi->lockAttributes();
						auto prio = qi->getPriorityL();
						qi->unlockAttributes();
						if (prio != QueueItem::PAUSED && !userQueue.getRunning(sr.getUser()))
							wantConnection = true;
					}
				}
			}
		}
	}

	if (added && downloadFileList)
	{
		try
		{
			const string path = Util::getFilePath(sr.getFile());
			addDirectory(path, sr.getHintedUser(), Util::emptyString, QueueItem::DEFAULT, DIR_FLAG_MATCH_QUEUE);
		}
		catch (const Exception&)
		{
			dcassert(0);
			// ...
		}
	}
	if (wantConnection && sr.getUser()->isOnline())
		getDownloadConnection(sr.getHintedUser());
}

// ClientManagerListener
void QueueManager::on(ClientManagerListener::UserConnected, const UserPtr& user) noexcept
{
	QueueItemList itemList;
	const bool hasDown = userQueue.getQueuedItems(user, itemList);
	if (!itemList.empty())
	{
		for (auto& qi : itemList)
			qi->updateSourcesVersion();
		fire(QueueManagerListener::StatusUpdatedList(), itemList);
	}
	if (hasDown)
	{
		// the user just came on, so there's only 1 possible hub, no need for a hint
		getDownloadConnection(HintedUser(user, Util::emptyString));
	}
}

void QueueManager::on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept
{
	QueueItemList itemList;
	userQueue.getQueuedItems(user, itemList);
	if (!itemList.empty())
	{
		for (auto& qi : itemList)
			qi->updateSourcesVersion();
		if (!ClientManager::isBeforeShutdown())
		{
			fire(QueueManagerListener::StatusUpdatedList(), itemList);
		}
	}
#if 0
	userQueue.removeRunning(user);
#endif
}

bool QueueManager::isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& target)
{
	QueueItemPtr qi = fileQueue.findQueueItem(tth);
	if (!qi || !qi->isChunkDownloaded(startPos, bytes))
		return false;
	if (!qi->isFinished())
	{
		qi->lockAttributes();
		target = qi->getTempTargetL();
		qi->unlockAttributes();
	}
	else
		target = qi->getTarget();
	return true;
}

void QueueManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (dirty && lastSave + SAVE_QUEUE_TIME < tick)
	{
#ifdef _DEBUG
		LogManager::message("Saving download queue");
#endif
		saveQueue();
	}
	if (ClientManager::isBeforeShutdown())
		return;

	bool sourceAddedFlag = false;
	{
		LOCK(csUpdatedSources);
		std::swap(sourceAdded, sourceAddedFlag);
	}
	if (sourceAddedFlag)
		fire(QueueManagerListener::SourceAdded());
}

#ifdef BL_FEATURE_DROP_SLOW_SOURCES
bool QueueManager::dropSource(const DownloadPtr& d)
{
	bool multipleSegments;
	uint64_t overallSpeed;
	{
		QueueRLock(*QueueItem::g_cs);

		const QueueItemPtr q = userQueue.getRunning(d->getUser());

		dcassert(q);
		if (!q)
			return false;

		dcassert(q->isSourceL(d->getUser()));

		if (!(q->getExtraFlags() & QueueItem::XFLAG_AUTODROP))
			return false;

		if (!q->hasOnlineSourcesL(2))
			return false;

		multipleSegments = q->isMultipleSegments();
		overallSpeed = q->getAverageSpeed();
	}

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool autoDiscMultiSourceOnly = ss->getBool(Conf::AUTO_DISCONNECT_MULTISOURCE_ONLY);
	int autoDiscFileSpeed = ss->getInt(Conf::AUTO_DISCONNECT_FILE_SPEED);
	int autoDiscRemoveSpeed = ss->getInt(Conf::AUTO_DISCONNECT_REMOVE_SPEED);
	ss->unlockRead();

	if (multipleSegments || !autoDiscMultiSourceOnly)
	{
		if (autoDiscFileSpeed == 0 || overallSpeed > (uint64_t) autoDiscFileSpeed * 1024)
		{
			d->setFlag(Download::FLAG_SLOWUSER);
			if (d->getRunningAverage() < autoDiscRemoveSpeed * 1024)
				return true;
			d->disconnect();
		}
	}

	return false;
}
#endif

static void logPartialSourceInfo(const QueueItem::PartialSource& partialSource, const UserPtr& user, const TTHValue& tth, const string& header, bool hasPart)
{
	string msg = tth.toBase32() + ": ";
	msg += header;
	msg += ' ';
	msg += Util::printIpAddress(partialSource.getIp(), true) + ':' + Util::toString(partialSource.getUdpPort());
	string nick = user->getLastNick();
	if (!nick.empty()) msg += ", " + nick;
	const string& hub = partialSource.getHubIpPort();
	if (!hub.empty()) msg += ", hub " + hub;
	msg += ", they have " + Util::toString(QueueItem::countParts(partialSource.getParts())) + '*' + Util::toString(partialSource.getBlockSize());
	if (!hasPart) msg += " (no needed part)";
	LOG(PSR_TRACE, msg);
}

bool QueueManager::handlePartialResult(const UserPtr& user, const TTHValue& tth, QueueItem::PartialSource& partialSource, QueueItem::PartsInfo& outPartialInfo)
{
	bool wantConnection = false;
	dcassert(outPartialInfo.empty());

	{
		// Locate target QueueItem in download queue
		QueueItemPtr qi = fileQueue.findQueueItem(tth);
		if (!qi)
			return false;

		// Don't add sources to finished files
		if (qi->isFinished())
			return false;

		int logOptions = LogManager::getLogOptions();

		// Check min size
		if (qi->getSize() < QueueItem::PFS_MIN_FILE_SIZE)
		{
			if (logOptions & LogManager::OPT_LOG_PSR)
				LOG(PSR_TRACE, tth.toBase32() + ": file size below minimum (" + Util::toString(qi->getSize()) + ")");
			return false;
		}

		// Get my parts info
		const auto blockSize = qi->getBlockSize();
		partialSource.setBlockSize(blockSize);
		qi->getParts(outPartialInfo, qi->getBlockSize());

		// Any parts for me?
		wantConnection = QueueItem::isNeededPart(partialSource.getParts(), outPartialInfo);

		QueueWLock(*QueueItem::g_cs);

		// If this user isn't a source and has no parts needed, ignore it
		auto si = qi->findSourceL(user);
		if (si == qi->getSourcesL().end())
		{
			si = qi->findBadSourceL(user);

			if (si != qi->getBadSourcesL().end() && si->second.isSet(QueueItem::Source::FLAG_TTH_INCONSISTENCY))
				return false;

			if (!wantConnection)
			{
				if (si == qi->getBadSourcesL().end())
					return false;
				const auto& ps = si->second.partialSource;
				if (ps) ps->setNextQueryTime(GET_TICK() + PFS_QUERY_INTERVAL);
			}
			else
			{
				// add this user as partial file sharing source
				qi->lockAttributes();
				auto p = qi->getPriorityL();
				qi->unlockAttributes();

				si = qi->addSourceL(user);
				si->second.setFlag(QueueItem::Source::FLAG_PARTIAL);

				const auto ps = std::make_shared<QueueItem::PartialSource>(partialSource.getMyNick(),
					partialSource.getHubIpPort(), partialSource.getIp(), partialSource.getUdpPort(), partialSource.getBlockSize());
				ps->setParts(partialSource.getParts());
				ps->setNextQueryTime(GET_TICK() + PFS_QUERY_INTERVAL);
				si->second.partialSource = ps;

				userQueue.addL(qi, p, user);
				dcassert(si != qi->getSourcesL().end());
				if (logOptions & LogManager::OPT_LOG_PSR)
					logPartialSourceInfo(partialSource, user, tth, "adding partial source", wantConnection);
			}
		}
		else
		{
			// Update source's parts info
			QueueItem::PartialSource::Ptr ps = si->second.partialSource;
			if (ps)
			{
				uint16_t oldPort = ps->getUdpPort();
				uint16_t newPort = partialSource.getUdpPort();
				if (!newPort) newPort = oldPort;
				if ((logOptions & LogManager::OPT_LOG_PSR) &&
					(!QueueItem::compareParts(ps->getParts(), partialSource.getParts()) || oldPort != newPort))
					logPartialSourceInfo(partialSource, user, tth, "updating partial source", wantConnection);
				ps->setParts(partialSource.getParts());
				ps->setUdpPort(newPort);
				ps->setNextQueryTime(GET_TICK() + PFS_QUERY_INTERVAL);
			}
		}
	}

	// Connect to this user
	if (wantConnection)
		getDownloadConnection(HintedUser(user, Util::emptyString));

	return true;
}

bool QueueManager::handlePartialSearch(const TTHValue& tth, QueueItem::PartsInfo& outPartsInfo, uint64_t& blockSize)
{
	// Locate target QueueItem in download queue
	const QueueItemPtr qi = fileQueue.findQueueItem(tth);
	if (!qi)
		return false;
	if (qi->getSize() < QueueItem::PFS_MIN_FILE_SIZE || !qi->getDownloadedBytes())
		return false;

	// don't share when file does not exist
	if (qi->isFinished())
	{
		if (!File::isExist(qi->getTarget()))
			return false;
	}
	else
	{
		qi->lockAttributes();
		string path = qi->getTempTargetL();
		qi->unlockAttributes();
		if (!File::isExist(path))
			return false;
	}

	blockSize = qi->getBlockSize();
	qi->getParts(outPartsInfo, blockSize);
	return !outPartsInfo.empty();
}

// compare nextQueryTime, get the oldest ones
void QueueManager::FileQueue::findPFSSources(QueueItem::SourceList& sl, uint64_t now) const
{
	QueueRLock(*QueueItem::g_cs);
	{
		QueueRLock(*csFQ);
		for (auto i = queue.cbegin(); i != queue.cend(); ++i)
		{
			if (ClientManager::isBeforeShutdown())
				return;
			const auto q = i->second;
			if (q->getSize() < QueueItem::PFS_MIN_FILE_SIZE || !q->getDownloadedBytes()) continue;

#if 0
			// don't share when file does not exist
			if (!File::isExist(q->isFinished() ? q->getTarget() : q->getTempTargetConst()))
				continue;
#endif
			QueueItem::getPFSSourcesL(q, sl, now, PFS_SOURCES);
		}
	}
}

void QueueManager::ListMatcherJob::run()
{
	StringList list = File::findFiles(Util::getListPath(), "*.xml*");
	for (auto i = list.cbegin(); i != list.cend(); ++i)
	{
		UserPtr u = DirectoryListing::getUserFromFilename(*i);
		if (!u)
			continue;

		DirectoryListing dl(manager.listMatcherAbortFlag, false);
		dl.setHintedUser(HintedUser(u, Util::emptyString));
		try
		{
			dl.loadFile(*i, nullptr, false);
			logMatchedFiles(u, QueueManager::getInstance()->matchListing(dl));
		}
		catch (const Exception&)
		{
			if (ClientManager::isBeforeShutdown() || dl.isAborted()) break;
		}
	}
	manager.listMatcherRunning.clear();
}

bool QueueManager::matchAllFileLists()
{
	if (listMatcherRunning.test_and_set())
		return false;
	ListMatcherJob* job = new ListMatcherJob(*this);
	if (!listMatcher.addJob(job))
	{
		delete job;
		return false;
	}
	return true;
}

void QueueManager::DclstLoaderJob::run()
{
	DirectoryListing dl(manager.dclstLoaderAbortFlag);
	dl.loadFile(path, nullptr, false);

	bool getConnFlag = true;
	dl.download(dl.getRoot(), Util::getFilePath(path), QueueItem::DEFAULT, getConnFlag);

	if (!dl.getIncludeSelf())
		File::deleteFile(path);
}

bool QueueManager::addDclstFile(const string& path)
{
	DclstLoaderJob* job = new DclstLoaderJob(*this, path);
	if (!dclstLoader.addJob(job))
	{
		delete job;
		return false;
	}
	return true;
}

void QueueManager::FileMoverJob::run()
{
	switch (type)
	{
		case MOVE_FILE:
		case MOVE_FILE_RETRY:
			if (type == MOVE_FILE)
			{
				if (File::renameFile(source, target))
					return;
				LogManager::message(STRING_F(UNABLE_TO_RENAME_FMT, source % target % Util::translateError()));
			}
			if (!File::isExist(source))
				return;
			if (File::copyFile(source, target))
			{
				File::deleteFile(source);
				return;
			}
			if (moveToOtherDir)
				keepFileInTempDir(source, target);
			break;

		case COPY_QI_FILE:
			dcassert(qi);
			manager.copyFile(source, target, qi);
			break;
	}
}

bool QueueManager::recheck(const string& target)
{
	RecheckerJob* job = new RecheckerJob(*this, target);
	if (!rechecker.addJob(job))
	{
		delete job;
		return false;
	}
	return true;
}

struct DummyOutputStream : OutputStream
{
	size_t write(const void*, size_t n) override
	{
		return n;
	}
	size_t flushBuffers(bool force) override
	{
		return 0;
	}
};

void QueueManager::RecheckerJob::run()
{
	QueueItemPtr q;
	int64_t tempSize;

	q = fileQueue.findTarget(file);
	if (!q || (q->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)))
		return;

	manager.fire(QueueManagerListener::RecheckStarted(), q->getTarget());
	dcdebug("Rechecking %s\n", file.c_str());

	q->lockAttributes();
	string tempTarget = q->getTempTargetL();
	q->unlockAttributes();

	if (tempTarget.empty() || (tempSize = File::getSize(tempTarget)) == -1)
	{
		manager.fire(QueueManagerListener::RecheckNoFile(), q->getTarget());
		q->resetDownloaded();
		manager.rechecked(q);
		return;
	}

	if (tempSize < 64 * 1024)
	{
		manager.fire(QueueManagerListener::RecheckFileTooSmall(), q->getTarget());
		q->resetDownloaded();
		manager.rechecked(q);
		return;
	}

	if (tempSize != q->getSize())
	{
		try
		{
			File(tempTarget, File::WRITE, File::OPEN).setSize(q->getSize());
		}
		catch (FileException& e)
		{
			LogManager::message("[Error] setSize - " + tempTarget + " Error=" + e.getError());
		}
	}

	if (q->isRunning())
	{
		manager.fire(QueueManagerListener::RecheckDownloadsRunning(), q->getTarget());
		return;
	}

	TigerTree tt;
	TTHValue tth = q->getTTH();

	// get q again in case it has been (re)moved
	q = fileQueue.findTarget(file);
	if (!q)
		return;
	auto db = DatabaseManager::getInstance();
	auto hashDb = db->getHashDatabaseConnection();
	if (!hashDb || !db->getTree(hashDb, tth, tt))
	{
		if (hashDb) db->putHashDatabaseConnection(hashDb);
		manager.fire(QueueManagerListener::RecheckNoTree(), q->getTarget());
		return;
	}

	db->putHashDatabaseConnection(hashDb);
	q->lockAttributes();
	tempTarget = q->getTempTargetL();
	q->unlockAttributes();

	//Merklecheck
	int64_t startPos = 0;
	DummyOutputStream dummy;
	int64_t blockSize = tt.getBlockSize();
	bool hasBadBlocks = false;
	vector<uint8_t> buf((size_t)min((int64_t)1024 * 1024, blockSize));
	vector< pair<int64_t, int64_t> > sizes;

	try
	{
		File inFile(tempTarget, File::READ, File::OPEN);
		while (startPos < tempSize)
		{
			try
			{
				MerkleCheckOutputStream<TigerTree, false> os(tt, &dummy, startPos);

				inFile.setPos(startPos);
				int64_t bytesLeft = min(tempSize - startPos, blockSize); //Take care of the last incomplete block
				int64_t segmentSize = bytesLeft;
				while (bytesLeft > 0)
				{
					if (manager.recheckerAbortFlag.load())
						return;
					size_t n = (size_t)min((int64_t)buf.size(), bytesLeft);
					size_t nr = inFile.read(&buf[0], n);
					os.write(&buf[0], nr);
					bytesLeft -= nr;
					if (bytesLeft > 0 && nr == 0)
					{
						// Huh??
						throw Exception();
					}
				}
				os.flushBuffers(true);
				sizes.push_back(make_pair(startPos, segmentSize));
			}
			catch (const Exception&)
			{
				hasBadBlocks = true;
				dcdebug("Found bad block at " I64_FMT "\n", startPos);
			}
			startPos += blockSize;
		}
	}
	catch (const FileException&)
	{
		return;
	}

	// get q again in case it has been (re)moved
	q = fileQueue.findTarget(file);

	if (!q)
		return;

	//If no bad blocks then the file probably got stuck in the temp folder for some reason
	if (!hasBadBlocks && q->isFinished())
	{
		manager.moveFile(tempTarget, file, -1);
		userQueue.removeQueueItem(q);
		manager.fire(QueueManagerListener::RecheckAlreadyFinished(), file);
		return;
	}

	{
		LOCK(q->csSegments);
		q->resetDownloadedL();
		for (auto i = sizes.cbegin(); i != sizes.cend(); ++i)
			q->addSegmentL(Segment(i->first, i->second));
		q->downloadedBytes = q->doneSegmentsSize;
	}

	manager.rechecked(q);
}

int QueueManager::queueItemFlagsToDirFlags(QueueItem::MaskType extraFlags)
{
	if (extraFlags & QueueItem::XFLAG_DOWNLOAD_DIR)
		return DIR_FLAG_DOWNLOAD_DIRECTORY;
	if (extraFlags & QueueItem::XFLAG_MATCH_QUEUE)
		return DIR_FLAG_MATCH_QUEUE;
	return 0;
}

QueueItem::MaskType QueueManager::dirFlagsToQueueItemFlags(int dirFlags)
{
	if (dirFlags & DIR_FLAG_DOWNLOAD_DIRECTORY)
		return QueueItem::XFLAG_DOWNLOAD_DIR;
	if (dirFlags & DIR_FLAG_MATCH_QUEUE)
		return QueueItem::XFLAG_MATCH_QUEUE;
	return 0;
}
