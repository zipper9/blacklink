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

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include "QueueManager.h"
#include "SearchManager.h"
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "DatabaseManager.h"
#include "Download.h"
#include "UploadManager.h"
#include "MerkleCheckOutputStream.h"
#include "SearchResult.h"
#include "SharedFileStream.h"
#include "ADLSearch.h"
#include "ShareManager.h"
#include "Wildcards.h"
#include "Random.h"

static const unsigned SAVE_QUEUE_TIME = 300000; // 5 minutes
static const int64_t MOVER_LIMIT = 10 * 1024 * 1024;
static const int MAX_MATCH_QUEUE_ITEMS = 10;
static const size_t PFS_SOURCES = 10;
static const int PFS_QUERY_INTERVAL = 300000; // 5 minutes

QueueManager::FileQueue QueueManager::fileQueue;
QueueManager::UserQueue QueueManager::userQueue;
bool QueueManager::dirty = false;
uint64_t QueueManager::lastSave = 0;
QueueManager::UserQueue::UserQueueMap QueueManager::UserQueue::userQueueMap[QueueItem::LAST];
QueueManager::UserQueue::RunningMap QueueManager::UserQueue::runningMap;
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
std::unique_ptr<RWLock> QueueManager::UserQueue::csRunningMap = std::unique_ptr<RWLock>(RWLock::create());
#endif
size_t QueueManager::UserQueue::totalDownloads = 0;

using boost::adaptors::map_values;
using boost::range::for_each;

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

void QueueManager::FileQueue::updatePriority(QueueItem::Priority& p, bool& autoPriority, const string& fileName, int64_t size, QueueItem::MaskType flags)
{
	if (p < QueueItem::DEFAULT || p >= QueueItem::LAST)
		p = QueueItem::DEFAULT;

	autoPriority = false;
	if (BOOLSETTING(AUTO_PRIORITY_USE_PATTERNS))
	{
		const string& pattern = SETTING(AUTO_PRIORITY_PATTERNS);
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
		p = (QueueItem::Priority) SETTING(AUTO_PRIORITY_PATTERNS_PRIO);
		if (p < QueueItem::LOWEST)
			p = QueueItem::LOWEST;
		else
		if (p > QueueItem::HIGHEST)
			p = QueueItem::HIGHEST;
	}
	
	if (p == QueueItem::DEFAULT && BOOLSETTING(AUTO_PRIORITY_USE_SIZE))
	{
		if (size > 0 && size <= SETTING(AUTO_PRIORITY_SMALL_SIZE) << 10)
		{
			autoPriority = true;
			p = (QueueItem::Priority) SETTING(AUTO_PRIORITY_SMALL_SIZE_PRIO);
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
}

QueueItemPtr QueueManager::FileQueue::add(QueueManager* qm,
                                          const string& target,
                                          int64_t targetSize,
                                          QueueItem::MaskType flags,
                                          QueueItem::Priority p,
                                          const string& tempTarget,
                                          time_t added,
                                          const TTHValue& root,
                                          uint8_t maxSegments)
{
	bool autoPriority;
	string fileName = Util::getFileName(target);
	updatePriority(p, autoPriority, fileName, targetSize, flags);

	if (!maxSegments) maxSegments = getMaxSegments(targetSize);
	flags |= qm->getFlagsForFileName(fileName);

	auto qi = std::make_shared<QueueItem>(target, targetSize, p, autoPriority, flags, added, root, maxSegments, tempTarget);
	if (!(flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)) && !tempTarget.empty())
	{
		if (!File::isExist(tempTarget) && File::isExist(tempTarget + ".antifrag"))
		{
			// load old antifrag file
			File::renameFile(tempTarget + ".antifrag", qi->getTempTarget());
		}
	}
	if (!add(qi))
	{
		qi.reset();
		LogManager::message(STRING_F(DUPLICATE_QUEUE_ITEM, target));
	}
	return qi;
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
	auto countTTH = queueTTH.insert(make_pair(qi->getTTH(), QueueItemList{qi}));
	if (!countTTH.second)
		countTTH.first->second.push_back(qi);
	++generationId;
	return true;
}

void QueueManager::FileQueue::remove(const QueueItemPtr& qi)
{
	size_t nrDownloads = qi->downloads.size();
	if (nrDownloads)
	{
		// shouldn't be needed
		assert(0);
		userQueue.modifyRunningCount(-(int) nrDownloads);
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

static QueueItemPtr findCandidateL(const QueueItem::QIStringMap::const_iterator& start, const QueueItem::QIStringMap::const_iterator& end, deque<string>& recent)
{
	QueueItemPtr cand = nullptr;
	
	for (auto i = start; i != end; ++i)
	{
		const QueueItemPtr& q = i->second;
		// We prefer to search for things that are not running...
		if (cand != nullptr && q->getNextSegmentL(0, 0, 0, nullptr).getSize() == 0)
			continue;
		// No finished files
		if (q->isFinished())
			continue;
		// No user lists
		if (q->isSet(QueueItem::FLAG_USER_LIST))
			continue;
		// No IP check
		if (q->isSet(QueueItem::FLAG_USER_GET_IP))
			continue;
		// No paused downloads
		if (q->getPriority() == QueueItem::PAUSED)
			continue;
		// No files that already have more than AUTO_SEARCH_LIMIT online sources
		//if (q->countOnlineUsersGreatOrEqualThanL(SETTING(AUTO_SEARCH_LIMIT)))
		//  continue;
		// Did we search for it recently?
		if (find(recent.begin(), recent.end(), q->getTarget()) != recent.end())
			continue;
			
		cand = q;
		
		if (cand->getNextSegmentL(0, 0, 0, nullptr).getSize() != 0)
			break;
	}
	
	//check this again, if the first item we pick is running and there are no
	//other suitable items this will be true
	if (cand && cand->getNextSegmentL(0, 0, 0, nullptr).getSize() == 0)
	{
		cand = nullptr;
	}
	
	return cand;
}

QueueItemPtr QueueManager::FileQueue::findAutoSearch(deque<string>& recent) const
{
	{
		QueueRLock(*csFQ);
		dcassert(!queue.empty());  // https://crash-server.com/Problem.aspx?ProblemID=32091
		if (!queue.empty())
		{
			const QueueItem::QIStringMap::size_type start = Util::rand((uint32_t) queue.size());

			auto i = queue.begin();
			advance(i, start);
			if (i == queue.end())
			{
				i = queue.begin();
			}
			QueueItemPtr cand = findCandidateL(i, queue.end(), recent);
			if (cand == nullptr)
			{
#ifdef _DEBUG
				LogManager::message("[1] FileQueue::findAutoSearch - cand is null", false);
#endif
				cand = findCandidateL(queue.begin(), i, recent);
#ifdef _DEBUG
				if (cand)
					LogManager::message("[1-1] FileQueue::findAutoSearch - cand" + cand->getTarget(), false);
#endif
			}
			else if (cand->getNextSegmentL(0, 0, 0, nullptr).getSize() == 0)
			{
				QueueItemPtr cand2 = findCandidateL(queue.begin(), i, recent);
				if (cand2 != nullptr && cand2->getNextSegmentL(0, 0, 0, nullptr).getSize() != 0)
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

void QueueManager::FileQueue::moveTarget(const QueueItemPtr& qi, const string& target)
{
	remove(qi);
	qi->setTarget(target);
	add(qi);
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

void QueueManager::UserQueue::addL(const QueueItemPtr& qi)
{
	for (auto i = qi->getSourcesL().cbegin(); i != qi->getSourcesL().cend(); ++i)
	{
		addL(qi, i->first, false);
	}
}

void QueueManager::UserQueue::addL(const QueueItemPtr& qi, const UserPtr& user, bool isFirstLoad)
{
	dcassert(qi->getPriority() < QueueItem::LAST);
	auto& uq = userQueueMap[qi->getPriority()][user];
	
	if (!isFirstLoad && (
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	            qi->isSet(QueueItem::FLAG_USER_CHECK) ||
#endif
	            qi->getDownloadedBytes() > 0))
	{
		uq.push_front(qi);
	}
	else
	{
		uq.push_back(qi);
	}
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

int QueueManager::UserQueue::getNextL(QueueItemPtr& result, const UserPtr& user, QueueItem::Priority minPrio, int64_t wantedSize, int64_t lastSpeed, bool allowRemove)
{
	int p = QueueItem::LAST - 1;
	int lastError = ERROR_NO_ITEM;
	do
	{
		const auto i = userQueueMap[p].find(user);
		if (i != userQueueMap[p].cend())
		{
			dcassert(!i->second.empty());
			for (auto j = i->second.cbegin(); j != i->second.cend(); ++j)
			{
				const QueueItemPtr qi = *j;
				const auto source = qi->findSourceL(user);
				if (source == qi->sources.end())
					continue;
				if (source->second.isSet(QueueItem::Source::FLAG_PARTIAL)) // TODO Crash
				{
					// check partial source
					const Segment segment = qi->getNextSegmentL(qi->getBlockSize(), wantedSize, lastSpeed, source->second.getPartialSource());
					if (allowRemove && segment.getStart() != -1 && segment.getSize() == 0)
					{
						// no other partial chunk from this user, remove him from queue
						removeUserL(qi, user, true);
						qi->removeSourceL(user, QueueItem::Source::FLAG_NO_NEED_PARTS); // https://drdump.com/Problem.aspx?ProblemID=129066
						result = qi;
						return ERROR_NO_NEEDED_PART;
					}
				}
				if (qi->isWaiting())
				{
					// check maximum simultaneous files setting
					size_t fileSlots = SETTING(FILE_SLOTS);
					if (fileSlots == 0 ||
					    qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP) ||
					    userQueue.getRunningCount() < fileSlots)
					{
						result = qi;
						return SUCCESS;
					}
					if (lastError == ERROR_NO_ITEM)
					{
						lastError = ERROR_FILE_SLOTS_TAKEN;
						result = qi;
					}
					continue;
				}
				else if (qi->isDownloadTree()) // No segmented downloading when getting the tree
				{
					continue;
				}
				if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
				{
					const auto blockSize = qi->getBlockSize();
					const Segment segment = qi->getNextSegmentL(blockSize, wantedSize, lastSpeed, source->second.getPartialSource());
					if (segment.getSize() == 0)
					{
						if (lastError == ERROR_NO_ITEM)
						{
							lastError = segment.getStart() == -1 ? ERROR_DOWNLOAD_SLOTS_TAKEN : ERROR_NO_FREE_BLOCK;
							result = qi;
						}
						//LogManager::message("No segment for User " + user->getLastNick() + " target=" + qi->getTarget() + " flags=" + Util::toString(qi->getFlags()), false);
						continue;
					}
				}
				result = qi;
				return SUCCESS;
			}
		}
		p--;
	}
	while (p >= minPrio);
	
	return lastError;
}

void QueueManager::UserQueue::addDownload(const QueueItemPtr& qi, const DownloadPtr& d)
{
	qi->addDownload(d);
	// Only one download per user...
	{
		WRITE_LOCK(*csRunningMap);
		runningMap[d->getUser()] = qi;
		totalDownloads++;
	}
}

void QueueManager::UserQueue::modifyRunningCount(int count)
{
	WRITE_LOCK(*csRunningMap);
	totalDownloads += count;
}

size_t QueueManager::UserQueue::getRunningCount()
{
	READ_LOCK(*csRunningMap);
	return totalDownloads;
}

void QueueManager::FileQueue::getRunningFilesL(QueueItemList& runningFiles)
{
	for (auto i = queue.begin(); i != queue.end(); ++i)
	{
		if (ClientManager::isBeforeShutdown())
			break;
		QueueItemPtr& q = i->second;
		if (q->isRunning())
		{
			q->updateDownloadedBytesAndSpeedL();
			runningFiles.push_back(q);
		}
	}
}

void QueueManager::UserQueue::removeRunning(const UserPtr& user)
{
	WRITE_LOCK(*csRunningMap);
	runningMap.erase(user);
}

bool QueueManager::UserQueue::removeDownload(const QueueItemPtr& qi, const UserPtr& user)
{
	bool result = qi->removeDownload(user);
	WRITE_LOCK(*csRunningMap);
	runningMap.erase(user);
	if (result)
	{
		dcassert(totalDownloads > 0);
		totalDownloads--;
	}
	return result;
}

void QueueManager::UserQueue::setQIPriority(const QueueItemPtr& qi, QueueItem::Priority p)
{
	QueueWLock(*QueueItem::g_cs);
	removeQueueItemL(qi, p == QueueItem::PAUSED);
	qi->setPriority(p);
	addL(qi);
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
	{
		removeUserL(qi, i->first, removeDownloadFlag);
	}
}

bool QueueManager::UserQueue::isInQueue(const QueueItemPtr& qi) const
{
	QueueRLock(*QueueItem::g_cs);
	auto& ulm = userQueueMap[qi->getPriority()];
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
	if (removeDownloadFlag && qi == getRunning(user))
		removeDownload(qi, user);

	const bool isSource = qi->isSourceL(user);
	if (!isSource)
	{
		const string error = "Error QueueManager::UserQueue::removeUserL [dcassert(isSource)] user = " +
		                     (user ? user->getLastNick() : string("null"));
		LogManager::message(error);
		dcassert(0);
		return;
	}
	{
		auto& ulm = userQueueMap[qi->getPriority()];
		const auto j = ulm.find(user);
		if (j == ulm.cend())
		{
#ifdef _DEBUG
			const string error = "Error QueueManager::UserQueue::removeUserL [dcassert(j != ulm.cend())] user = " +
			                     (user ? user->getLastNick() : string("null"));
			LogManager::message(error);
#endif
			//dcassert(j != ulm.cend());
			return;
		}
		
		auto& uq = j->second;
		const auto i = find(uq.begin(), uq.end(), qi);
		// TODO - перевести на set const auto& i = uq.find(qi);
		if (i == uq.cend())
		{
			dcassert(i != uq.cend());
			return;
		}
		uq.erase(i);
		if (uq.empty())
		{
			ulm.erase(j);
		}
	}
}

QueueManager::QueueManager() :
	nextSearch(0),
	listMatcherAbortFlag(false),
	dclstLoaderAbortFlag(false),
	recheckerAbortFlag(false),
	sourceAdded(false)

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
#ifdef FLYLINKDC_USE_DETECT_CHEATING
	m_listQueue.forceStop();
#endif
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
		if (!days || it->getTimeStamp() + days * 86400ull * TimerManager::TIMESTAMP_UNITS_PER_SEC < currentTime)
			delList.push_back(it->getFileName());
	}
}

void QueueManager::deleteFileLists()
{
	unsigned days = SETTING(KEEP_LISTS_DAYS);
	StringList delList;
	const string& path = Util::getListPath();
	uint64_t currentTime = TimerManager::getFileTime();
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
	string searchString;
	vector<const PartsInfoReqParam*> params;
	{
		QueueItem::SourceList sl;
		//find max 10 pfs sources to exchange parts
		//the source basis interval is 5 minutes
		fileQueue.findPFSSources(sl, tick);

		for (const auto& item : sl)
		{
			QueueItem::PartialSource::Ptr source = item.si->second.getPartialSource();
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
	if (fileQueue.getSize() > 0 && tick >= nextSearch && BOOLSETTING(AUTO_SEARCH))
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
			//dcassert(SETTING(AUTO_SEARCH_TIME) > 1)
			nextSearch = tick + SETTING(AUTO_SEARCH_TIME) * 60000;
			if (BOOLSETTING(REPORT_ALTERNATES))
			{
				LogManager::message(STRING(ALTERNATES_SEND) + ' ' + Util::getFileName(qi->getTarget())
#ifdef _DEBUG
				 + " TTH = " + qi->getTTH().toBase32()
#endif
				 );
			}
		}
	}

	// Request parts info from partial file sharing sources
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const PartsInfoReqParam* param = *i;
		dcassert(param->udpPort > 0);
		AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
		SearchManager::toPSR(cmd, true, param->myNick, param->ip.type, param->hubIpPort, param->tth, param->parts);
		string data = cmd.toString(ClientManager::getMyCID());
		if (CMD_DEBUG_ENABLED())
			COMMAND_DEBUG("[Partial-Search]" + data, DebugTask::CLIENT_OUT, Util::printIpAddress(param->ip) + ':' + Util::toString(param->udpPort));
		if (BOOLSETTING(LOG_PSR_TRACE))
		{
			string msg = param->tth + ": sending periodic PSR #" + Util::toString(param->req) + " to ";
			msg += Util::printIpAddress(param->ip, true) + ':' + Util::toString(param->udpPort);
			if (!param->nick.empty()) msg += ", " + param->nick;
			msg += ", we have " + Util::toString(QueueItem::countParts(param->parts)) + '*' + Util::toString(param->blockSize);
			LOG(PSR_TRACE, msg);
		}
		SearchManager::getInstance()->addToSendQueue(data, param->ip, param->udpPort);
		delete param;
	}
	if (!searchString.empty())
	{
		SearchManager::getInstance()->searchAuto(searchString);
	}
}

// TODO HintedUser
void QueueManager::addList(const UserPtr& user, QueueItem::MaskType flags, const string& initialDir /* = Util::emptyString */)
{
	bool getConnFlag = true;
	add(initialDir, -1, TTHValue(), user, (QueueItem::MaskType)(QueueItem::FLAG_USER_LIST | flags), QueueItem::DEFAULT, true, getConnFlag);
}

string QueueManager::getListPath(const UserPtr& user)
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
		const string datetime = Util::formatDateTime("%Y%m%d_%H%M.", time(nullptr));
		return checkTarget(Util::getListPath() + nick + datetime + user->getCID().toBase32(), -1);
	}
	return string();
}

void QueueManager::getDownloadConnection(const UserPtr& user)
{
	if (!ClientManager::isBeforeShutdown() && ConnectionManager::isValidInstance())
	{
		ConnectionManager::getInstance()->getDownloadConnection(user);
	}
}

void QueueManager::add(const string& target, int64_t size, const TTHValue& root, const UserPtr& user,
                       QueueItem::MaskType flags, QueueItem::Priority priority, bool addBad, bool& getConnFlag)
{
	// Check that we're not downloading from ourselves...
	if (user && user->isMe())
	{
		dcassert(0);
		throw QueueException(STRING(NO_DOWNLOADS_FROM_SELF));
	}
	
	const bool fileList = (flags & QueueItem::FLAG_USER_LIST) != 0;
	const bool testIP = (flags & QueueItem::FLAG_USER_GET_IP) != 0;
	bool newItem = !(testIP || fileList);

	string targetPath;
	string tempTarget;
#ifdef DEBUG_TRANSFERS
	bool usePath = false;
#endif

	if (fileList)
	{
		dcassert(user);
		targetPath = getListPath(user);
		if (flags & QueueItem::FLAG_PARTIAL_LIST)
		{
			targetPath += ':';
			targetPath += target;
		}
		tempTarget = target;
	}
	else if (testIP)
	{
		dcassert(user);
		targetPath = getListPath(user) + ".check";
		tempTarget = target;
	}
	else
	{
#ifdef DEBUG_TRANSFERS
		if (target[0] == '/')
		{
			auto pos = target.rfind('/');
			string filename = target.substr(pos + 1);
			targetPath = FavoriteManager::getInstance()->getDownloadDirectory(Util::getFileExt(filename), user) + filename;
			usePath = true;
		}
		else
#endif
		{
			if (File::isAbsolute(target))
				targetPath = target;
			else
				targetPath = FavoriteManager::getInstance()->getDownloadDirectory(Util::getFileExt(target), user) + target;
			targetPath = checkTarget(targetPath, -1);
		}
	}
	
	// Check if it's a zero-byte file, if so, create and return...
	if (size == 0)
	{
		if (!BOOLSETTING(SKIP_ZERO_BYTE))
		{
			File::ensureDirectory(targetPath);
			try { File f(targetPath, File::WRITE, File::CREATE); }
			catch (const FileException&) {}
		}
		return;
	}
	
	bool wantConnection = false;
	
	{
		QueueItemPtr q = fileQueue.findTarget(targetPath);
		// По TTH искать нельзя
		// Проблема описана тут http://www.flylinkdc.ru/2014/04/flylinkdc-strongdc-tth.html
#if 0
		if (q == nullptr &&
		        newItem &&
		        (BOOLSETTING(ENABLE_MULTI_CHUNK) && size > SETTING(MIN_MULTI_CHUNK_SIZE) * 1024 * 1024) // [+] IRainman size in MB.
		   )
		{
			// q = QueueManager::FileQueue::findQueueItem(root);
		}
#endif
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
#ifdef DEBUG_TRANSFERS
				bool targetExists = usePath ? false : File::getAttributes(targetPath, attr);
#else
				bool targetExists = File::getAttributes(targetPath, attr);
#endif
				if (targetExists)
				{
					existingFileSize = attr.getSize();
					existingFileTime = File::timeStampToUnixTime(attr.getTimeStamp());
					if (existingFileSize == size && BOOLSETTING(SKIP_EXISTING))
					{
						LogManager::message(STRING_F(SKIPPING_EXISTING_FILE, targetPath));
						return;
					}
					switch (SETTING(TARGET_EXISTS_ACTION))
					{
						case SettingsManager::TE_ACTION_ASK:
							waitForUserInput = true;
							priority = QueueItem::PAUSED;
							break;
						case SettingsManager::TE_ACTION_REPLACE:
							File::deleteFile(targetPath); // Delete old file.
							break;
						case SettingsManager::TE_ACTION_RENAME:
							targetPath = Util::getNewFileName(targetPath); // Call Util::getNewFileName instead of using CheckTargetDlg's stored name
							break;
						case SettingsManager::TE_ACTION_SKIP:
							return;
					}
				}

				int64_t maxSizeForCopy = (int64_t) SETTING(COPY_EXISTING_MAX_SIZE) << 20;
				if (!waitForUserInput && maxSizeForCopy &&
				    ShareManager::getInstance()->getFileInfo(root, sharedFilePath) &&
				    File::getAttributes(sharedFilePath, attr))
				{
					auto sharedFileSize = attr.getSize();
					if (sharedFileSize == size && sharedFileSize <= maxSizeForCopy)
					{
						LogManager::message(STRING_F(COPYING_EXISTING_FILE, targetPath));
						priority = QueueItem::PAUSED;
						flags |= QueueItem::FLAG_COPYING;
					}
				}
			}

			q = fileQueue.add(this, targetPath, size, flags, priority, tempTarget, GET_TIME(), root, 0);

			if (q)
			{
#ifdef DEBUG_TRANSFERS
				if (usePath) q->setDownloadPath(target);
#endif
				fire(QueueManagerListener::Added(), q);
				if (flags & QueueItem::FLAG_COPYING)
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
					fileQueue.updatePriority(savedPriority, autoPriority, fileName, size, flags);
					q->setAutoPriority(autoPriority);
					fire(QueueManagerListener::FileExistsAction(), targetPath, size, existingFileSize, existingFileTime, savedPriority);
				}
			}
		}
		else
		{
			if (q->getSize() != size)
			{
				throw QueueException(STRING(FILE_WITH_DIFFERENT_SIZE));
			}
			if (!(root == q->getTTH()))
			{
				throw QueueException(STRING(FILE_WITH_DIFFERENT_TTH));
			}
			
			if (q->isFinished())
			{
				throw QueueException(STRING(FILE_ALREADY_FINISHED));
			}
			
			// FIXME: flags must be immutable
			q->flags |= flags; // why ?
		}
		if (user && !(user->getFlags() & User::FAKE) && q)
		{
			QueueWLock(*QueueItem::g_cs);
			wantConnection = addSourceL(q, user, (QueueItem::MaskType)(addBad ? QueueItem::Source::FLAG_MASK : 0));
			if (priority == QueueItem::PAUSED) wantConnection = false;
		}
		else
			wantConnection = false;
		setDirty();
	}
	
	if (getConnFlag)
	{
		if (wantConnection && user->isOnline())
		{
			getDownloadConnection(user);
			getConnFlag = false;
		}
		
		// auto search, prevent DEADLOCK
		if (newItem && BOOLSETTING(AUTO_SEARCH))
			SearchManager::getInstance()->searchAuto(root.toBase32());
	}
}

void QueueManager::readdAll(const QueueItemPtr& q)
{
	QueueItem::SourceMap badSources;
	{
		QueueWLock(*QueueItem::g_cs);
		badSources = q->getBadSourcesL(); // fix https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=62702
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
		getDownloadConnection(user);
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
		throw QueueException(STRING(TARGET_FILENAME_TOO_LONG));
	// Check that target starts with a drive or is an UNC path
	if (!(target.length() > 3 && ((target[1] == ':' && target[2] == '\\') || (target[0] == '\\' && target[1] == '\\'))))
		throw QueueException(STRING(INVALID_TARGET_FILE));
#else
	if (target.length() > PATH_MAX)
		throw QueueException(STRING(TARGET_FILENAME_TOO_LONG));
	// Check that target contains at least one directory...we don't want headless files...
	if (!File::isAbsolute(target))
		throw QueueException(STRING(INVALID_TARGET_FILE));
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
		case SettingsManager::TE_ACTION_REPLACE:
			File::deleteFile(path);
			setPriority(path, priority, false);
			break;
		case SettingsManager::TE_ACTION_RENAME:
			move(path, newPath);
			setPriority(newPath, priority, false);
			break;
		case SettingsManager::TE_ACTION_SKIP:
			removeTarget(path, false);
	}
}

/** Add a source to an existing queue item */
bool QueueManager::addSourceL(const QueueItemPtr& qi, const UserPtr& user, QueueItem::MaskType addBad, bool isFirstLoad)
{
	dcassert(user);
	bool wantConnection;
	{
		if (isFirstLoad)
			wantConnection = true;
		else
			wantConnection = qi->getPriority() != QueueItem::PAUSED
			                 && !userQueue.getRunning(user);
		
		if (qi->isSourceL(user))
		{
			if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			{
				return wantConnection;
			}
			throw QueueException(STRING(DUPLICATE_SOURCE) + ": " + Util::getFileName(qi->getTarget()));
		}
		dcassert((isFirstLoad && !qi->isBadSourceExceptL(user, addBad)) || !isFirstLoad);
		if (qi->isBadSourceExceptL(user, addBad))
		{
			throw QueueException(STRING(DUPLICATE_SOURCE) +
			                     " TTH = " + Util::getFileName(qi->getTarget()) +
			                     " Nick = " + user->getLastNick());
		}
		
		qi->addSourceL(user, isFirstLoad);
		/*if(user.user->isSet(User::PASSIVE) && !ClientManager::isActive(user.hint)) {
		    qi->removeSource(user, QueueItem::Source::FLAG_PASSIVE);
		    wantConnection = false;
		} else */
		if (!isFirstLoad && qi->isFinished())
		{
			wantConnection = false;
		}
		else
		{
			if (!isFirstLoad && !qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			{
				LOCK(csUpdatedSources);
				sourceAdded = true;
			}
			userQueue.addL(qi, user, isFirstLoad);
		}
	}
	if (!isFirstLoad)
	{
		addUpdatedSource(qi);
		setDirty();
	}
	return wantConnection;
}

void QueueManager::addDirectory(const string& dir, const UserPtr& user, const string& target, QueueItem::Priority p, int flag) noexcept
{
	dcassert(flag == QueueItem::FLAG_DIRECTORY_DOWNLOAD || flag == QueueItem::FLAG_MATCH_QUEUE);
	dcassert(user);
	if (user)
	{
		{
			int matchQueueItems = 0;
			LOCK(csDirectories);
			const auto dp = directories.equal_range(user);
			for (auto i = dp.first; i != dp.second; ++i)
			{
				if (flag == QueueItem::FLAG_MATCH_QUEUE && (i->second.getFlags() & QueueItem::FLAG_MATCH_QUEUE))
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
			directories.emplace(make_pair(user, DirectoryItem(user, dir, target, p, flag)));
		}

		try
		{
			addList(user, flag | QueueItem::FLAG_PARTIAL_LIST, dir);
		}
		catch (const Exception&)
		{
			dcassert(0);
		}
	}
}

size_t QueueManager::getDirectoryItemCount() const noexcept
{
	LOCK(csDirectories);
	return directories.size();
}

QueueItem::Priority QueueManager::hasDownload(const UserPtr& user)
{
	QueueRLock(*QueueItem::g_cs);
	QueueItemPtr qi;
	if (userQueue.getNextL(qi, user, QueueItem::LOWEST) != SUCCESS)
		return QueueItem::PAUSED;
	return qi->getPriority();
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
					if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
						continue;
					const auto j = tthMap.find(qi->getTTH());
					if (j != tthMap.cend() && j->second == qi->getSize())
					{
						matches++;
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
		if (sourceAdded)
			getDownloadConnection(dl.getUser());
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
				if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
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
		getDownloadConnection(user);
	return matches;
}

#ifdef FLYLINKDC_USE_DETECT_CHEATING
void QueueManager::FileListQueue::execute(const DirectoryListInfoPtr& list) // [+] IRainman fix: moved form MainFrame to core.
{
	if (File::isExist(list->file))
	{
		unique_ptr<DirectoryListing> dl(new DirectoryListing(list->m_hintedUser));
		try
		{
			dl->loadFile(list->file);
			ADLSearchManager::getInstance()->matchListing(*dl);
			ClientManager::checkCheating(list->m_hintedUser, dl.get());
		}
		catch (const Exception& e)
		{
			LogManager::message(e.getError());
		}
	}
	delete list;
}
#endif

void QueueManager::move(const string& aSource, const string& aTarget) noexcept
{
	const string target = Util::validateFileName(aTarget);
	if (aSource == target)
		return;
		
	const QueueItemPtr qs = fileQueue.findTarget(aSource);
	if (!qs)
		return;
		
	// Don't move file lists
	if (qs->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
		return;
		
	// TODO: moving running downloads is not implemented
	if (qs->isRunning())
		return;

	// Let's see if the target exists...then things get complicated...
	const QueueItemPtr qt = fileQueue.findTarget(target);
	if (!qt || stricmp(aSource, target) == 0)
	{
		// Good, update the target and move in the queue...
		fire(QueueManagerListener::Moved(), qs, aSource);
		fileQueue.moveTarget(qs, target);
		setDirty();
	}
	else
	{
		// Don't move to target of different size
		if (qs->getSize() != qt->getSize() || qs->getTTH() != qt->getTTH())
			return; // TODO: ask user

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
		removeTarget(aSource, false);
	}
}

bool QueueManager::getQueueInfo(const UserPtr& user, string& target, int64_t& size, int& flags) noexcept
{
	QueueRLock(*QueueItem::g_cs);
	QueueItemPtr qi;
	if (userQueue.getNextL(qi, user) != SUCCESS)
		return false;
		
	target = qi->getTarget();
	size = qi->getSize();
	flags = qi->flags;
	return true;
}

uint8_t QueueManager::FileQueue::getMaxSegments(uint64_t filesize)
{
	unsigned value;
	if (BOOLSETTING(SEGMENTS_MANUAL))
		value = min(SETTING(NUMBER_OF_SEGMENTS), 200);
	else
		value = static_cast<unsigned>(min<uint64_t>(filesize / (50 * MIN_BLOCK_SIZE) + 2, 200));
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

DownloadPtr QueueManager::getDownload(UserConnection* source, Download::ErrorInfo& errorInfo) noexcept
{
	const UserPtr u = source->getUser();
	dcdebug("Getting download for %s...", u->getCID().toBase32().c_str());
	QueueItemPtr q;
	DownloadPtr d;
	{
		QueueWLock(*QueueItem::g_cs);
		errorInfo.error = userQueue.getNextL(q, u, QueueItem::LOWEST, source->getChunkSize(), source->getSpeed(), true);
		if (errorInfo.error != SUCCESS)
		{
			if (q)
			{
				errorInfo.target = q->getTarget();
				errorInfo.size = q->getSize();
				if (q->isSet(QueueItem::FLAG_PARTIAL_LIST))
					errorInfo.type = Transfer::TYPE_PARTIAL_LIST;
				else if (q->isSet(QueueItem::FLAG_USER_LIST))
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
		q->updateDownloadedBytesAndSpeedL();
		if (q->getDownloadedBytes() > 0)
		{
			if (!File::isExist(q->getTempTarget()))
			{
				// Temp target gone?
				q->resetDownloaded();
			}
		}
	}
	
	// Нельзя звать new Download под локом QueueItem::g_cs
	d = std::make_shared<Download>(source, q, Util::printIpAddress(source->getRemoteIp()), source->getCipherName());
#ifdef DEBUG_TRANSFERS
	const string& downloadPath = q->getDownloadPath();
	if (!downloadPath.empty()) d->setDownloadPath(downloadPath);
#endif
	if (d->getSegment().getStart() != -1 && d->getSegment().getSize() == 0)
	{
		errorInfo.error = ERROR_NO_NEEDED_PART;
		errorInfo.target = q->getTarget();		
		errorInfo.size = q->getSize();
		errorInfo.type = Transfer::TYPE_FILE;
		d.reset();
		return d;
	}
	
	source->setDownload(d);
	userQueue.addDownload(q, d);
	
	addUpdatedSource(q);
	dcdebug("found %s\n", q->getTarget().c_str());
	return d;
}

class TreeOutputStream : public OutputStream
{
	public:
		explicit TreeOutputStream(TigerTree& aTree) : tree(aTree), bufPos(0)
		{
		}

		size_t write(const void* xbuf, size_t len) override
		{
			size_t pos = 0;
			uint8_t* b = (uint8_t*)xbuf;
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
		
		size_t flushBuffers(bool aForce) override
		{
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
		const QueueItemPtr qi = fileQueue.findTarget(d->getPath());
		if (!qi)
		{
			throw QueueException(STRING(TARGET_REMOVED));
		}
		
		if (d->getOverlapped())
		{
			d->setOverlapped(false);
			
			const bool isFound = qi->disconnectSlow(d);
			if (!isFound)
			{
				// slow chunk already finished ???
				throw QueueException(STRING(DOWNLOAD_FINISHED_IDLE));
			}
		}
		
		const string target = d->getDownloadTarget();
		
		if (qi->getDownloadedBytes() > 0)
		{
			if (!File::isExist(qi->getTempTarget()))
			{
				// When trying the download the next time, the resume pos will be reset
				qi->setLastSize(0);
				throw QueueException(STRING(TARGET_REMOVED));
			}
		}
		else
		{
			File::ensureDirectory(target);
		}
		
		// open stream for both writing and reading, because UploadManager can request reading from it
		const auto fileSize = d->getTigerTree().getFileSize();

		auto f = new SharedFileStream(target, File::RW, File::OPEN | File::CREATE | File::SHARED | File::NO_CACHE_HINT, fileSize);
		if (qi->getSize() != qi->getLastSize())
		{
			if (f->getFastFileSize() != qi->getSize())
			{
				dcassert(fileSize == d->getTigerTree().getFileSize());
				f->setSize(fileSize);
				qi->setLastSize(fileSize);
			}
		}
		else
		{
			dcdebug("Skip for file %s qi->getSize() == qi->getLastSize()\r\n", target.c_str());
		}

		f->setPos(d->getSegment().getStart());
		d->setDownloadFile(f);
	}
	else if (d->getType() == Transfer::TYPE_FULL_LIST)
	{
		{
			const auto path = d->getPath();
			QueueItemPtr qi = fileQueue.findTarget(path);
			if (!qi)
			{
				throw QueueException(STRING(TARGET_REMOVED));
			}
			
			// set filelist's size
			qi->setSize(d->getSize());
		}
		
		string target = getFileListTempTarget(d);
		File::ensureDirectory(target);
		d->setDownloadFile(new File(target, File::WRITE, File::OPEN | File::TRUNCATE | File::CREATE));
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

void QueueManager::moveFile(const string& source, const string& target)
{
	string sourcePath = Util::getFilePath(source);
	string destPath = Util::getFilePath(target);

	bool moveToOtherDir = sourcePath != destPath;
	bool useMover = false;
	if (moveToOtherDir)
	{
		if (!File::isExist(destPath))
			File::ensureDirectory(destPath);
		useMover = File::getSize(source) > MOVER_LIMIT;
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

void QueueManager::moveStuckFile(const QueueItemPtr& qi)
{
	moveFile(qi->getTempTarget(), qi->getTarget());
	if (qi->isFinished())
		userQueue.removeQueueItem(qi);
	
	const string target = qi->getTarget();
	
	if (/*!BOOLSETTING(NEVER_REPLACE_TARGET)*/true)
	{
		removeItem(qi, false); // ???
	}
	else
	{
		qi->addSegment(Segment(0, qi->getSize()));
		fireStatusUpdated(qi);
	}
	if (!ClientManager::isBeforeShutdown())
	{
		fire(QueueManagerListener::RecheckAlreadyFinished(), target);
	}
}

void QueueManager::copyFile(const string& source, const string& target, QueueItemPtr& qi)
{
	dcassert(qi->isSet(QueueItem::FLAG_COPYING));
	if (qi->removed)
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

void QueueManager::addUpdatedSource(const QueueItemPtr& qi)
{
	if (!ClientManager::isBeforeShutdown())
	{
		LOCK(csUpdatedSources);
		updatedSources.insert(qi->getTarget());
	}
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

void QueueManager::putDownload(const string& path, DownloadPtr download, bool finished, bool reportFinish) noexcept
{
	UserList getConn;
	unique_ptr<DirectoryItem> processListDirItem;
	string processListFileName;
	const HintedUser hintedUser = download->getHintedUser();
	UserPtr user = download->getUser();
	
	dcassert(user);
	
	QueueItem::MaskType processListFlags = 0;
	bool downloadList = false;
	
	{
		download->resetDownloadFile();
		
		if (download->getType() == Transfer::TYPE_PARTIAL_LIST)
		{
			QueueItemPtr q = fileQueue.findTarget(path);
			if (q)
			{
				//q->setFailed(!download->m_reason.empty());
				
				if (!download->getFileListBuffer().empty())
				{
					int dirFlags = 0;
					if (q->isAnySet(QueueItem::FLAG_MATCH_QUEUE | QueueItem::FLAG_DIRECTORY_DOWNLOAD))
					{
						LOCK(csDirectories);
						auto dp = directories.equal_range(download->getUser());
						for (auto i = dp.first; i != dp.second; ++i)
							if (stricmp(q->getTempTargetConst(), i->second.getName()) == 0)
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
						processListFlags = dirFlags | QueueItem::FLAG_TEXT;
						if (download->isSet(Download::FLAG_TTH_LIST)) processListFlags |= QueueItem::FLAG_TTH_LIST;
					}
					else
					{
						fire(QueueManagerListener::PartialList(), hintedUser, download->getFileListBuffer());
					}
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
			QueueItemPtr q = fileQueue.findTarget(path);
			if (q)
			{
				if (download->getType() == Transfer::TYPE_FULL_LIST)
				{
					// FIXME: flags must be immutable
					if (download->isSet(Download::FLAG_XML_BZ_LIST))
						q->flags |= QueueItem::FLAG_XML_BZLIST;
					else
						q->flags &= ~QueueItem::FLAG_XML_BZLIST;
				}

				if (finished)
				{
					auto db = DatabaseManager::getInstance();
					auto hashDb = db->getHashDatabaseConnection();
					if (download->getType() == Transfer::TYPE_TREE)
					{
						// Got a full tree, now add it to the database
						dcassert(download->getTreeValid());
						if (hashDb)
							db->addTree(hashDb, download->getTigerTree());
						userQueue.removeDownload(q, download->getUser());
						fireStatusUpdated(q);
					}
					else
					{
						// Now, let's see if this was a directory download filelist...
						if (q->isAnySet(QueueItem::FLAG_DIRECTORY_DOWNLOAD | QueueItem::FLAG_MATCH_QUEUE))
						{
							processListFileName = q->getListName();
							processListFlags = q->getFlags() & (QueueItem::FLAG_DIRECTORY_DOWNLOAD | QueueItem::FLAG_MATCH_QUEUE);
						}
						
						const bool isFile = download->getType() == Transfer::TYPE_FILE;
						bool isFinishedFile = false;

						string dir;
						if (download->getType() == Transfer::TYPE_FULL_LIST)
						{
							dir = q->getTempTarget();
							q->addSegment(Segment(0, q->getSize()));
							string destName = q->getListName();
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
							if (!q->isSet(Download::FLAG_USER_GET_IP))
							{
								if (isFile)
								{
									// For partial-share, abort upload first to move file correctly
									UploadManager::getInstance()->abortUpload(q->getTempTarget());
									q->disconnectOthers(download);
								}
							
								// Check if we need to move the file
								if (isFile && !download->getTempTarget().empty() && stricmp(path, download->getTempTarget()) != 0)
									moveFile(download->getTempTarget(), path);

								SharedFileStream::cleanup();
								if (BOOLSETTING(LOG_DOWNLOADS) && (BOOLSETTING(LOG_FILELIST_TRANSFERS) || isFile))
								{
									StringMap params;
									download->getParams(params);
									LOG(DOWNLOAD, params);
								}
							}

							if (hashDb && !q->getTTH().isZero() && !q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
								hashDb->putFileInfo(q->getTTH().data, DatabaseManager::FLAG_DOWNLOADED, q->getSize(), path.empty() ? nullptr : &path, false);

							if (!ClientManager::isBeforeShutdown())
							{
								fire(QueueManagerListener::Finished(), q, dir, download);
							}
							
#ifdef FLYLINKDC_USE_DETECT_CHEATING
							if (q->isSet(QueueItem::FLAG_USER_LIST))
							{
								const auto user = q->getFirstUser();
								if (user)
									m_listQueue.addTask(new DirectoryListInfo(HintedUser(user, Util::emptyString), q->getListName(), dir, download->getRunningAverage()));
							}
#endif
							if (q->isSet(QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_DOWNLOAD_CONTENTS))
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
						{
							QueueRLock(*QueueItem::g_cs); // ???
							q->updateDownloadedBytesAndSpeedL();
						}
						if (q->isSet(QueueItem::FLAG_USER_LIST))
						{
							// Blah...no use keeping an unfinished file list...
							File::deleteFile(q->getListName() + dctmpExtension);
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
								
								{
									q->addSegment(Segment(download->getStartPos(), downloaded));
								}
								
								setDirty();
							}
						}
					}
					
					if (q->getPriority() != QueueItem::PAUSED)
					{
						const string& reason = download->getReason();
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
			else if (download->getType() != Transfer::TYPE_TREE && download->getQueueItem()->removed)
			{
				const string tmpTarget = download->getTempTarget();
				if (!tmpTarget.empty() && tmpTarget != path)
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
	}
	
	for (auto i = getConn.cbegin(); i != getConn.cend(); ++i)
		getDownloadConnection(*i);

	if (!processListFileName.empty())
		processList(processListFileName, hintedUser, processListFlags, processListDirItem.get());
	
	// partial file list failed, redownload full list
	if (downloadList && user->isOnline())
	{
		try
		{
			addList(user, processListFlags);
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
	if ((flags & (QueueItem::FLAG_TEXT | QueueItem::FLAG_TTH_LIST)) == (QueueItem::FLAG_TEXT | QueueItem::FLAG_TTH_LIST))
	{
		logMatchedFiles(hintedUser.user, matchTTHList(name, hintedUser.user));
		return;
	}

	std::atomic_bool unusedAbortFlag(false);
	DirectoryListing dirList(unusedAbortFlag);
	dirList.setHintedUser(hintedUser);
	try
	{
		if (flags & QueueItem::FLAG_TEXT)
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
		if (!(flags & QueueItem::FLAG_TEXT))
			LogManager::message(STRING(UNABLE_TO_OPEN_FILELIST) + ' ' + name);
		return;
	}
	if (!dirList.getRoot()->getTotalFileCount())
		return;
	if (flags & QueueItem::FLAG_DIRECTORY_DOWNLOAD)
	{
		struct DownloadItem
		{
			DirectoryListing::Directory* d;
			string target;
			QueueItem::Priority p;
		};
		vector<DownloadItem> dl;
		if (flags & QueueItem::FLAG_TEXT)
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
				if (i->second.getFlags() & QueueItem::FLAG_DIRECTORY_DOWNLOAD)
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
	if (!(flags & QueueItem::FLAG_TEXT))
	{
		LOCK(csDirectories);
		directories.erase(hintedUser.user);
	}
	if (flags & QueueItem::FLAG_MATCH_QUEUE)
	{
		logMatchedFiles(hintedUser.user, matchListing(dirList));
	}
}

QueueItem::MaskType QueueManager::getFlagsForFileName(const string& fileName)
{
	const string& pattern = SETTING(WANT_END_FILES);
	csWantEndFiles.lock();
	if (pattern != wantEndFilesPattern)
	{
		wantEndFilesPattern = pattern;
		if (!Wildcards::regexFromPatternList(reWantEndFiles, wantEndFilesPattern, true))
			wantEndFilesPattern.clear();
	}
	bool result = std::regex_match(fileName, reWantEndFiles);
	csWantEndFiles.unlock();
	return result ? QueueItem::FLAG_WANT_END : 0;
}

void QueueManager::removeAll()
{
	fileQueue.clearAll();
}

void QueueManager::removeItem(const QueueItemPtr& qi, bool removeFromUserQueue)
{
	if (removeFromUserQueue) userQueue.removeQueueItem(qi);
	fileQueue.remove(qi);
	fire(QueueManagerListener::Removed(), qi);
}

bool QueueManager::removeTarget(const string& target, bool isBatchRemove)
{
#if 0
	string logMessage = "removeTarget: " + target;
	DumpDebugMessage(_T("queue-debug.log"), logMessage.c_str(), logMessage.length(), true);
#endif

	QueueItemPtr q = fileQueue.findTarget(target);
	if (!q)
		return false;
			
	if (q->isAnySet(QueueItem::FLAG_MATCH_QUEUE | QueueItem::FLAG_DIRECTORY_DOWNLOAD))
	{
		UserPtr user;
		{
			QueueRLock(*QueueItem::g_cs);
			dcassert(q->getSourcesL().size() == 1);
			user = q->getSourcesL().begin()->first;
		}
		{
			LOCK(csDirectories);
			if (q->isSet(QueueItem::FLAG_PARTIAL_LIST))
			{
				auto dp = directories.equal_range(user);
				for (auto i = dp.first; i != dp.second; ++i)
					if (stricmp(q->getTempTargetConst(), i->second.getName()) == 0)
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

	if (!q->getTTH().isZero() && !q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		auto db = DatabaseManager::getInstance();
		auto hashDb = db->getHashDatabaseConnection();
		if (hashDb)
		{
			hashDb->putFileInfo(q->getTTH().data, DatabaseManager::FLAG_DOWNLOAD_CANCELED, q->getSize(), nullptr, false);
			db->putHashDatabaseConnection(hashDb);
		}
	}

	const string& tempTarget = q->getTempTargetConst();
	if (!q->isSet(QueueItem::FLAG_USER_LIST) && !tempTarget.empty())
	{
		// For partial-share
		UploadManager::getInstance()->abortUpload(tempTarget);
	}

	UserList x;
	if (q->isRunning())
	{
		q->getUsers(x);
	}
	else if (!q->isSet(QueueItem::FLAG_USER_LIST) && !tempTarget.empty() && tempTarget != q->getTarget())
	{
		if (File::isExist(tempTarget))
		{
			if (!File::deleteFile(tempTarget))
				SharedFileStream::deleteFile(tempTarget);
		}
	}

	q->removed = true;
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
		QueueRLock(*QueueItem::g_cs);
		QueueItemPtr q = fileQueue.findTarget(target);
		if (!q)
			return;

		auto source = q->findSourceL(user);
		if (source == q->sources.end()) return;
		if (reason == QueueItem::Source::FLAG_NO_TREE)
			source->second.setFlag(reason);
		
		if (q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
		{
			removeCompletely = true;
			break;
		}
		
		if (q->isRunning() && userQueue.getRunning(user) == q)
		{
			isRunning = true;
			userQueue.removeDownload(q, user);
			addUpdatedSource(q);
		}
		if (!q->isFinished())
		{
			userQueue.removeUserL(q, user, true);
		}
		q->removeSourceL(user, reason);
		
		addUpdatedSource(q);
		setDirty();
	}
	while (false);
	
	if (isRunning && removeConn)
	{
		ConnectionManager::getInstance()->disconnect(user, true);
	}
	if (removeCompletely)
	{
		removeTarget(target, false);
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
			auto& ulm = UserQueue::userQueueMap[p];
			auto i = ulm.find(user);
			if (i != ulm.end())
			{
				QueueItemList& ql = i->second;
				for (QueueItemPtr& qi : ql)
				{
					qi->removeSourceL(user, reason);
					if (qi->isSet(QueueItem::FLAG_USER_LIST))
					{
						targetsToRemove.push_back(qi->getTarget());
					}
					else
					{
						addUpdatedSource(qi);
						dirty = true;
					}
				}
				ulm.erase(i);
			}
		}
	}
	QueueItemPtr qi = userQueue.getRunning(user);
	if (qi && !qi->isSet(QueueItem::FLAG_USER_LIST))
	{
		userQueue.removeDownload(qi, user);
		disconnect = dirty = true;
		fireStatusUpdated(qi);
	}
	if (disconnect)
		ConnectionManager::getInstance()->disconnect(user, true);
	for (const string& target : targetsToRemove)
		removeTarget(target, false);
	if (dirty)
		setDirty();
}

void QueueManager::setPriority(const string& target, QueueItem::Priority p, bool resetAutoPriority) noexcept
{
	UserList getConn;
	bool isRunning = false;
	
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		QueueItemPtr q = fileQueue.findTarget(target);
		if (!q) return;
		bool upd = false;
		if (resetAutoPriority && q->getAutoPriority())
		{
			q->setAutoPriority(false);
			upd = true;			
		}
		if (q->getPriority() != p)
		{
			bool isFinished = q->isFinished();
			if (!isFinished)
			{
				isRunning = q->isRunning();
				
				if (q->getPriority() == QueueItem::PAUSED || p == QueueItem::HIGHEST)
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
	}
	
	if (p == QueueItem::PAUSED)
	{
		if (isRunning)
			DownloadManager::getInstance()->abortDownload(target);
	}
	else
	{
		for (auto i = getConn.cbegin(); i != getConn.cend(); ++i)
			getDownloadConnection(*i);
	}
}

void QueueManager::setAutoPriority(const string& target, bool ap)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		vector<pair<string, QueueItem::Priority>> priorities;
		QueueItemPtr q = fileQueue.findTarget(target);
		if (q && q->getAutoPriority() != ap)
		{
			q->setAutoPriority(ap);
			if (ap)
			{
				priorities.push_back(make_pair(q->getTarget(), q->calculateAutoPriority()));
			}
			setDirty();
			fireStatusUpdated(q);
		}
		
		for (auto p = priorities.cbegin(); p != priorities.cend(); ++p)
		{
			setPriority((*p).first, (*p).second, false);
		}
	}
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
		BufferedOutputStream<false> f(&ff, 256 * 1024);
		
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
				if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
				{
					f.write(LIT("\t<Download Target=\""));
					f.write(SimpleXML::escape(qi->getTarget(), tmp, true));
					f.write(LIT("\" Size=\""));
					f.write(Util::toString(qi->getSize()));
					f.write(LIT("\" Priority=\""));
					f.write(Util::toString((int)qi->getPriority()));
					f.write(LIT("\" Added=\""));
					f.write(Util::toString(qi->getAdded()));
					b32tmp.clear();
					f.write(LIT("\" TTH=\""));
					f.write(qi->getTTH().toBase32(b32tmp));
					qi->getDoneSegments(done);
					if (!done.empty())
					{
						f.write(LIT("\" TempTarget=\""));
						f.write(SimpleXML::escape(qi->getTempTarget(), tmp, true));
					}
					f.write(LIT("\" AutoPriority=\""));
					f.write(Util::toString(qi->getAutoPriority()));
					f.write(LIT("\" MaxSegments=\""));
					f.write(Util::toString(qi->getMaxSegments()));

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
	
			string fileName = Util::getFileName(target);
			QueueItem::MaskType flags = 0;
			if (!maxSegments) maxSegments = QueueManager::FileQueue::getMaxSegments(size);
			flags |= qm->getFlagsForFileName(fileName);

			const bool autoPriority = Util::toInt(getAttrib(attribs, sAutoPriority, 6)) == 1;
			auto qi = std::make_shared<QueueItem>(target, size, p, autoPriority, flags, added, TTHValue(tthRoot), maxSegments, tempTarget);
			if (!(flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)) && !tempTarget.empty())
			{
				if (!File::isExist(tempTarget) && File::isExist(tempTarget + ".antifrag"))
					File::renameFile(tempTarget + ".antifrag", qi->getTempTarget());
			}

			if (QueueManager::fileQueue.addL(qi))
			{
				if (downloaded > 0)
					qi->addSegment(Segment(0, downloaded));
				if (simple)
				{
					qi->updateDownloadedBytes();
					qi->setPriority(qi->calculateAutoPriority());
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
				cur->setPriority(cur->calculateAutoPriority());
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
							downloadFileList = BOOLSETTING(AUTO_SEARCH_DL_LIST) && !qi->countOnlineUsersGreatOrEqualThanL(SETTING(AUTO_SEARCH_MAX_SOURCES));
							wantConnection = addSourceL(qi, sr.getHintedUser(), 0);
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
						// Нашли источник но он не активный еще
						if (qi->getPriority() != QueueItem::PAUSED && !userQueue.getRunning(sr.getUser()))
						{
							wantConnection = true;
						}
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
			addDirectory(path, sr.getUser(), Util::emptyString, QueueItem::DEFAULT, QueueItem::FLAG_MATCH_QUEUE);
		}
		catch (const Exception&)
		{
			dcassert(0);
			// ...
		}
	}
	if (wantConnection && sr.getUser()->isOnline())
		getDownloadConnection(sr.getUser());
}

// ClientManagerListener
void QueueManager::on(ClientManagerListener::UserConnected, const UserPtr& user) noexcept
{
	QueueItemList itemList;
	const bool hasDown = userQueue.getQueuedItems(user, itemList);
	if (!itemList.empty())
	{
		for (auto& qi : itemList)
			qi->cachedOnlineSourceCountInvalid = true;
		fire(QueueManagerListener::StatusUpdatedList(), itemList);
	}
	if (hasDown)
	{
		// the user just came on, so there's only 1 possible hub, no need for a hint
		getDownloadConnection(user);
	}
}

void QueueManager::on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept
{
	QueueItemList itemList;
	userQueue.getQueuedItems(user, itemList);
	if (!itemList.empty())
	{
		for (auto& qi : itemList)
			qi->cachedOnlineSourceCountInvalid = true;
		if (!ClientManager::isBeforeShutdown())
		{
			fire(QueueManagerListener::StatusUpdatedList(), itemList);
		}
	}
	userQueue.removeRunning(user);
}

bool QueueManager::isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& target)
{
	QueueItemPtr qi = fileQueue.findQueueItem(tth);
	if (!qi)
		return false;
	target = qi->isFinished() ? qi->getTarget() : qi->getTempTarget();
	
	return qi->isChunkDownloaded(startPos, bytes);
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

	QueueItemList runningItems;
	{
		LockFileQueueShared lock;
		getRunningFilesL(runningItems);
	}
	if (!runningItems.empty())
	{
		fire(QueueManagerListener::Tick(), runningItems); // Don't fire when locked
	}
	StringList targetList;
	bool sourceAddedFlag = false;
	{
		LOCK(csUpdatedSources);
		targetList.reserve(updatedSources.size());
		for (auto i = updatedSources.cbegin(); i != updatedSources.cend(); ++i)
			targetList.push_back(*i);
		updatedSources.clear();
		std::swap(sourceAdded, sourceAddedFlag);
	}
	if (!targetList.empty())
		fire(QueueManagerListener::TargetsUpdated(), targetList);
	if (sourceAddedFlag)
		fire(QueueManagerListener::SourceAdded());
}

#ifdef FLYLINKDC_USE_DROP_SLOW
bool QueueManager::dropSource(const DownloadPtr& aDownload)
{
	uint8_t activeSegments = 0;
	bool   allowDropSource;
	uint64_t overallSpeed;
	{
		QueueRLock(*QueueItem::g_cs);
		
		const QueueItemPtr q = userQueue.getRunning(aDownload->getUser());
		
		dcassert(q);
		if (!q)
			return false;
			
		dcassert(q->isSourceL(aDownload->getUser()));
		
		if (!q->isAutoDrop())
			return false;
			
		activeSegments = q->calcActiveSegments();
		allowDropSource = q->countOnlineUsersGreatOrEqualThanL(2);
		overallSpeed = q->getAverageSpeed();
	}
	
	if (activeSegments > 1 || !SETTING(AUTO_DISCONNECT_MULTISOURCE_ONLY))
	{
		if (allowDropSource)
		{
			const size_t iHighSpeed = SETTING(AUTO_DISCONNECT_FILE_SPEED);
			
			if (iHighSpeed == 0 || overallSpeed > iHighSpeed * 1024)
			{
				aDownload->setFlag(Download::FLAG_SLOWUSER);
				
				if (aDownload->getRunningAverage() < SETTING(AUTO_DISCONNECT_REMOVE_SPEED) * 1024)
				{
					return true;
				}
				else
				{
					aDownload->getUserConnection()->disconnect();
				}
			}
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

		// Check min size
		if (qi->getSize() < QueueItem::PFS_MIN_FILE_SIZE)
		{
			if (BOOLSETTING(LOG_PSR_TRACE))
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
				auto ps = si->second.getPartialSource();
				if (ps) ps->setNextQueryTime(GET_TICK() + PFS_QUERY_INTERVAL);
			}
			else
			{
				// add this user as partial file sharing source
				si = qi->addSourceL(user, false);
				si->second.setFlag(QueueItem::Source::FLAG_PARTIAL);

				const auto ps = std::make_shared<QueueItem::PartialSource>(partialSource.getMyNick(),
					partialSource.getHubIpPort(), partialSource.getIp(), partialSource.getUdpPort(), partialSource.getBlockSize());
				ps->setParts(partialSource.getParts());
				ps->setNextQueryTime(GET_TICK() + PFS_QUERY_INTERVAL);
				si->second.setPartialSource(ps);

				userQueue.addL(qi, user, false);
				dcassert(si != qi->getSourcesL().end());
				addUpdatedSource(qi);
				if (BOOLSETTING(LOG_PSR_TRACE))
					logPartialSourceInfo(partialSource, user, tth, "adding partial source", wantConnection);
			}
		}
		else
		{
			// Update source's parts info
			QueueItem::PartialSource::Ptr ps = si->second.getPartialSource();
			if (ps)
			{
				uint16_t oldPort = ps->getUdpPort();
				uint16_t newPort = partialSource.getUdpPort();
				if (!newPort) newPort = oldPort;
				if (BOOLSETTING(LOG_PSR_TRACE) && (!QueueItem::compareParts(ps->getParts(), partialSource.getParts()) || oldPort != newPort))
					logPartialSourceInfo(partialSource, user, tth, "updating partial source", wantConnection);
				ps->setParts(partialSource.getParts());
				ps->setUdpPort(newPort);
				ps->setNextQueryTime(GET_TICK() + PFS_QUERY_INTERVAL);
			}
		}
	}

	// Connect to this user
	if (wantConnection)
		getDownloadConnection(user);

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
	if (!File::isExist(qi->isFinished() ? qi->getTarget() : qi->getTempTargetConst()))
		return false;

	blockSize = qi->getBlockSize();
	qi->getParts(outPartsInfo, blockSize);
	return !outPartsInfo.empty();
}

// compare nextQueryTime, get the oldest ones
void QueueManager::FileQueue::findPFSSources(QueueItem::SourceList& sl, uint64_t now) const
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
	TTHValue tth;
	
	{
		q = fileQueue.findTarget(file);
		if (!q || q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			return;
			
		manager.fire(QueueManagerListener::RecheckStarted(), q->getTarget());
		dcdebug("Rechecking %s\n", file.c_str());
		
		tempSize = File::getSize(q->getTempTarget());
		
		if (tempSize == -1)
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
				File(q->getTempTarget(), File::WRITE, File::OPEN).setSize(q->getSize());
			}
			catch (FileException& e)
			{
				LogManager::message("[Error] setSize - " + q->getTempTarget() + " Error=" + e.getError());
			}
		}
		
		if (q->isRunning())
		{
			manager.fire(QueueManagerListener::RecheckDownloadsRunning(), q->getTarget());
			return;
		}
		
		tth = q->getTTH();
	}
	
	TigerTree tt;
	string tempTarget;
	
	{
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
		tempTarget = q->getTempTarget();
	}
	
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
				int64_t bytesLeft = min((tempSize - startPos), blockSize); //Take care of the last incomplete block
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
	if (!hasBadBlocks)
	{
		manager.moveStuckFile(q);
		return;
	}
	
	{
		LOCK(q->csSegments);
		q->resetDownloadedL();
		for (auto i = sizes.cbegin(); i != sizes.cend(); ++i)
		{
			q->addSegmentL(Segment(i->first, i->second));
		}
	}
	
	manager.rechecked(q);
}
