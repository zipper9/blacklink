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

static const unsigned SAVE_QUEUE_TIME = 300000; // 5 minutes
static const int64_t MOVER_LIMIT = 10 * 1024 * 1024;


QueueManager::FileQueue QueueManager::g_fileQueue;
QueueManager::UserQueue QueueManager::g_userQueue;
bool QueueManager::g_dirty = false;
uint64_t QueueManager::g_lastSave = 0;
QueueManager::UserQueue::UserQueueMap QueueManager::UserQueue::g_userQueueMap[QueueItem::LAST];
QueueManager::UserQueue::RunningMap QueueManager::UserQueue::g_runningMap;
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
std::unique_ptr<RWLock> QueueManager::UserQueue::g_runningMapCS = std::unique_ptr<RWLock>(RWLock::create());
#endif

using boost::adaptors::map_values;
using boost::range::for_each;

class DirectoryItem
{
	public:
		DirectoryItem() : m_priority(QueueItem::DEFAULT) {}
		DirectoryItem(const UserPtr& aUser, const string& aName, const string& aTarget,
		              QueueItem::Priority p) : m_name(aName), m_target(aTarget), m_priority(p), m_user(aUser) { }
		              
		DirectoryItem(const DirectoryItem&) = delete;
		DirectoryItem& operator= (const DirectoryItem&) = delete;
		
		const UserPtr& getUser() const
		{
			return m_user;
		}
		
		GETSET(string, m_name, Name);
		GETSET(string, m_target, Target);
		GETSET(QueueItem::Priority, m_priority, Priority);
	private:
		const UserPtr m_user;
};

QueueManager::FileQueue::FileQueue() :
#ifdef FLYLINKDC_USE_RWLOCK
	csFQ(RWLock::create())
#else
	csFQ(new CriticalSection)
#endif
{
}

QueueItemPtr QueueManager::FileQueue::add(const string& target,
                                          int64_t targetSize,
                                          QueueItem::MaskType flags,
                                          QueueItem::Priority p,
                                          const string& tempTarget,
                                          time_t added,
                                          const TTHValue& root,
                                          uint8_t maxSegments)
{
	if (p < QueueItem::DEFAULT || p >= QueueItem::LAST)
		p = QueueItem::DEFAULT;

	bool autoPriority = false;
	
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

	string fileName = Util::getFileName(target);
	
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
		if (targetSize > 0 && targetSize <= SETTING(AUTO_PRIORITY_SMALL_SIZE) << 10)
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
	
	if (!maxSegments) maxSegments = getMaxSegments(targetSize);
	
	// TODO: make it configurable?
	string fileExt = Util::getFileExt(fileName);
	Text::asciiMakeLower(fileExt);
	if (fileExt == ".mov" || fileExt == ".mp4" || fileExt == ".3gp")
		flags |= QueueItem::FLAG_WANT_END;

	auto qi = std::make_shared<QueueItem>(target, targetSize, p, autoPriority, flags, added, root, maxSegments, tempTarget);
	
	if (!(flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)) && !tempTarget.empty())
	{
		if (!File::isExist(tempTarget) && File::isExist(tempTarget + ".antifrag"))
		{
			// load old antifrag file
			File::renameFile(tempTarget + ".antifrag", qi->getTempTarget());
		}
	}
	if (findTarget(target) == nullptr)
	{
		add(qi);
	}
	else
	{
		dcassert(0);
		qi.reset();
		LogManager::message("Skip duplicate target QueueManager::FileQueue::add file = " + target);
	}
	return qi;
}

bool QueueManager::FileQueue::isQueued(const TTHValue& tth) const
{
	RLock(*csFQ);
	return queueTTH.find(tth) != queueTTH.end();
}

void QueueManager::FileQueue::add(const QueueItemPtr& qi)
{
	WLock(*csFQ);
	if (!queue.insert(make_pair(Text::toLower(qi->getTarget()), qi)).second)
		return;
	auto countTTH = queueTTH.insert(make_pair(qi->getTTH(), QueueItemList{qi}));
	if (!countTTH.second)
		countTTH.first->second.push_back(qi);
}

void QueueManager::FileQueue::removeInternal(const QueueItemPtr& qi)
{
	WLock(*csFQ);
	queue.erase(Text::toLower(qi->getTarget()));
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
	WLock(*csFQ);
	queueTTH.clear();
	queue.clear();
}

int QueueManager::FileQueue::findQueueItems(QueueItemList& ql, const TTHValue& tth, int maxCount /*= 0 */) const
{
	int count = 0;
	RLock(*csFQ);
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
	RLock(*csFQ);
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
		RLock(*csFQ);
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
	removeInternal(qi);
	qi->setTarget(target);
	add(qi);
}

bool QueueManager::UserQueue::getQueuedItems(const UserPtr& user, QueueItemList& out) const
{
	bool hasDown = false;
	RLock(*QueueItem::g_cs);
	for (size_t i = 0; i < QueueItem::LAST && !ClientManager::isBeforeShutdown(); ++i)
	{
		const auto j = g_userQueueMap[i].find(user);
		if (j != g_userQueueMap[i].end())
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

void QueueManager::UserQueue::addL(const QueueItemPtr& qi, const UserPtr& aUser, bool isFirstLoad)
{
	dcassert(qi->getPriority() < QueueItem::LAST);
	auto& uq = g_userQueueMap[qi->getPriority()][aUser];
	
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
	RLock(*csFQ);
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
	RLock(*csFQ);
	auto i = queue.find(Text::toLower(target));
	if (i != queue.cend())
		return i->second;
	return nullptr;
}

int QueueManager::UserQueue::getNextL(QueueItemPtr& result, const UserPtr& aUser, QueueItem::Priority minPrio, int64_t wantedSize, int64_t lastSpeed, bool allowRemove)
{
	int p = QueueItem::LAST - 1;
	int lastError = ERROR_NO_ITEM;
	do
	{
		const auto i = g_userQueueMap[p].find(aUser);
		if (i != g_userQueueMap[p].cend())
		{
			dcassert(!i->second.empty());
			for (auto j = i->second.cbegin(); j != i->second.cend(); ++j)
			{
				const QueueItemPtr qi = *j;
				const auto source = qi->findSourceL(aUser);
				if (source == qi->sources.end())
					continue;
				if (source->second.isSet(QueueItem::Source::FLAG_PARTIAL)) // TODO Crash
				{
					// check partial source
					const Segment segment = qi->getNextSegmentL(qi->getBlockSize(), wantedSize, lastSpeed, source->second.getPartialSource());
					if (allowRemove && segment.getStart() != -1 && segment.getSize() == 0)
					{
						// no other partial chunk from this user, remove him from queue
						removeUserL(qi, aUser, true);
						qi->removeSourceL(aUser, QueueItem::Source::FLAG_NO_NEED_PARTS); // https://drdump.com/Problem.aspx?ProblemID=129066
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
					    QueueManager::getRunningFileCount(fileSlots) < fileSlots) // TODO - двойной лок
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
						//LogManager::message("No segment for User " + aUser->getLastNick() + " target=" + qi->getTarget() + " flags=" + Util::toString(qi->getFlags()), false);
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

void QueueManager::UserQueue::addDownload(const QueueItemPtr& qi, const DownloadPtr& d) // [!] IRainman fix: this function needs external lock.
{
	qi->addDownload(d);
	// Only one download per user...
	{
		CFlyWriteLock(*g_runningMapCS);
		g_runningMap[d->getUser()] = qi;
	}
}

size_t QueueManager::FileQueue::getRunningFileCount(const size_t stopCount) const
{
	size_t count = 0;
	RLock(*csFQ);
	for (auto i = queue.cbegin(); i != queue.cend(); ++i)
	{
		const QueueItemPtr& q = i->second;
		if (q->isRunning())
		{
			++count;
			if (count > stopCount && stopCount != 0)
				break; // Выходим раньше и не бегаем по всей очереди.
		}
		// TODO - можно не бегать по очереди а считать в онлайне.
		// Алгоритм:
		// 1. при удалении из g_queue -1 (если m_queue.download заполнена)
		// 2  при добавлении к g_queue +1 (если m_queue.download заполнена)
		// 2. при добавлении в g_queue.download +1
		// 3. при опустошении g_queue.download -1
	}
	return count;
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
	CFlyWriteLock(*g_runningMapCS);
	g_runningMap.erase(user);
}

bool QueueManager::UserQueue::removeDownload(const QueueItemPtr& qi, const UserPtr& aUser)
{
	removeRunning(aUser);
	return qi->removeDownload(aUser);
}

void QueueManager::UserQueue::setQIPriority(const QueueItemPtr& qi, QueueItem::Priority p)
{
	WLock(*QueueItem::g_cs);
	removeQueueItemL(qi, p == QueueItem::PAUSED);
	qi->setPriority(p);
	addL(qi);
}

QueueItemPtr QueueManager::UserQueue::getRunning(const UserPtr& aUser)
{
	CFlyReadLock(*g_runningMapCS);
	const auto i = g_runningMap.find(aUser);
	return i == g_runningMap.cend() ? nullptr : i->second;
}

void QueueManager::UserQueue::removeQueueItem(const QueueItemPtr& qi)
{
	WLock(*QueueItem::g_cs);
	g_userQueue.removeQueueItemL(qi, true);
}

void QueueManager::UserQueue::removeQueueItemL(const QueueItemPtr& qi, bool removeDownloadFlag)
{
	const auto& s = qi->getSourcesL();
	for (auto i = s.cbegin(); i != s.cend(); ++i)
	{
		removeUserL(qi, i->first, removeDownloadFlag);
	}
}

void QueueManager::UserQueue::removeUserL(const QueueItemPtr& qi, const UserPtr& aUser, bool removeDownloadFlag)
{
	if (removeDownloadFlag && qi == getRunning(aUser))
		removeDownload(qi, aUser);

	const bool isSource = qi->isSourceL(aUser); // crash https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=78346
	if (!isSource)
	{
		const string error = "Error QueueManager::UserQueue::removeUserL [dcassert(isSource)] aUser = " +
		                     (aUser ? aUser->getLastNick() : string("null"));
		LogManager::message(error);
		dcassert(0);
		return;
	}
	{
		auto& ulm = g_userQueueMap[qi->getPriority()];
		const auto j = ulm.find(aUser);
		if (j == ulm.cend())
		{
#ifdef _DEBUG
			const string error = "Error QueueManager::UserQueue::removeUserL [dcassert(j != ulm.cend())] aUser = " +
			                     (aUser ? aUser->getLastNick() : string("null"));
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
			//const string l_error = "Error QueueManager::UserQueue::removeUserL [dcassert(i != uq.cend());] aUser = " +
			//                       (aUser ? aUser->getLastNick() : string("null"));
			//CFlyServerJSON::pushError(55, l_error);
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
}

QueueManager::~QueueManager() noexcept
{
	SearchManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	saveQueue();
#ifdef FLYLINKDC_USE_KEEP_LISTS
	
	if (!BOOLSETTING(KEEP_LISTS))
	{
		std::sort(protectedFileLists.begin(), protectedFileLists.end());
		StringList filelists = File::findFiles(Util::getListPath(), "*.xml.bz2");
		if (!filelists.empty())
		{
			std::sort(filelists.begin(), filelists.end());
			std::for_each(filelists.begin(),
			              std::set_difference(filelists.begin(), filelists.end(), protectedFileLists.begin(), protectedFileLists.end(), filelists.begin()),
			              File::deleteFile);
		}
	}
#endif
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

struct PartsInfoReqParam
{
	PartsInfo   parts;
	string      tth;
	string      myNick;
	string      hubIpPort;
	boost::asio::ip::address_v4      ip;
	uint16_t    udpPort;
};

void QueueManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	string searchString;
	vector<const PartsInfoReqParam*> params;
	{
		PFSSourceList sl;
		//find max 10 pfs sources to exchange parts
		//the source basis interval is 5 minutes
		g_fileQueue.findPFSSourcesL(sl);
		
		for (auto i = sl.cbegin(); i != sl.cend(); ++i)
		{
			if (ClientManager::isBeforeShutdown())
				return;
			QueueItem::PartialSource::Ptr source = i->first->second.getPartialSource();
			const QueueItemPtr qi = i->second;
			
			PartsInfoReqParam* param = new PartsInfoReqParam;
			
			qi->getPartialInfo(param->parts, qi->getBlockSize());
			
			param->tth = qi->getTTH().toBase32();
			param->ip  = source->getIp();
			param->udpPort = source->getUdpPort();
			param->myNick = source->getMyNick();
			param->hubIpPort = source->getHubIpPort();
			
			params.push_back(param);
			
			source->setPendingQueryCount(source->getPendingQueryCount() + 1);
			source->setNextQueryTime(aTick + 300000);       // 5 minutes
		}
	}
	if (g_fileQueue.getSize() > 0 && aTick >= nextSearch && BOOLSETTING(AUTO_SEARCH))
	{
		// We keep 30 recent searches to avoid duplicate searches
		while (m_recent.size() >= g_fileQueue.getSize() || m_recent.size() > 30)
		{
			m_recent.pop_front();
		}
		
		QueueItemPtr qi;
		while ((qi = g_fileQueue.findAutoSearch(m_recent)) == nullptr && !m_recent.empty()) // Местами не переставлять findAutoSearch меняет recent
		{
			m_recent.pop_front();
		}
		if (qi)
		{
			searchString = qi->getTTH().toBase32();
			m_recent.push_back(qi->getTarget());
			//dcassert(SETTING(AUTO_SEARCH_TIME) > 1)
			nextSearch = aTick + SETTING(AUTO_SEARCH_TIME) * 60000;
			if (BOOLSETTING(REPORT_ALTERNATES))
			{
				LogManager::message(STRING(ALTERNATES_SEND) + ' ' + Util::getFileName(qi->getTarget())
#ifdef _DEBUG
				 + " TTH = " + qi->getTTH().toBase32()
#endif
				 );
			}
		}
		else
		{
#ifdef _DEBUG
			//LogManager::message("[!]g_fileQueue.findAutoSearch - empty()");
#endif
		}
	}
	else
	{
#ifdef _DEBUG
		//LogManager::message("[!]g_fileQueue.getSize() > 0 && aTick >= nextSearch && BOOLSETTING(AUTO_SEARCH)");
#endif
	}
	// [~] IRainman fix.
	
	// Request parts info from partial file sharing sources
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const PartsInfoReqParam* param = *i;
		dcassert(param->udpPort > 0);
		
		try
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			SearchManager::toPSR(cmd, true, param->myNick, param->hubIpPort, param->tth, param->parts);
			string data = cmd.toString(ClientManager::getMyCID());
			if (CMD_DEBUG_ENABLED())
				COMMAND_DEBUG("[Partial-Search]" + data, DebugTask::CLIENT_OUT, param->ip.to_string() + ':' + Util::toString(param->udpPort));
			LogManager::psr_message(
			    "[PartsInfoReq] Send UDP IP = " + param->ip.to_string() +
			    " param->udpPort = " + Util::toString(param->udpPort) +
			    " cmd = " + data);
			SearchManager::getInstance()->addToSendQueue(data, param->ip, param->udpPort);
		}
		catch (Exception& e)
		{
			dcdebug("Partial search caught error\n");
			LogManager::psr_message(
			    "[Partial search caught error] Error = " + e.getError() +
			    " IP = " + param->ip.to_string() +
			    " param->udpPort = " + Util::toString(param->udpPort)
			);
		}
		
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
			nick = Util::cleanPathChars(nick);
			nick += '.';
		}
		const string datetime = Util::formatDateTime("%Y%m%d_%H%M.", time(nullptr));
		return checkTarget(Util::getListPath() + nick + datetime + user->getCID().toBase32(), -1);
	}
	return string();
}

void QueueManager::getDownloadConnection(const UserPtr& aUser)
{
	if (!ClientManager::isBeforeShutdown() && ConnectionManager::isValidInstance())
	{
		ConnectionManager::getInstance()->getDownloadConnection(aUser);
	}
}

void QueueManager::addFromWebServer(const string& target, int64_t size, const TTHValue& root)
{
	const int oldValue = SETTING(TARGET_EXISTS_ACTION);
	try
	{
		SET_SETTING(TARGET_EXISTS_ACTION, SettingsManager::TE_ACTION_RENAME);
		bool getConnFlag = true;
		add(target, size, root, HintedUser(), 0, QueueItem::DEFAULT, true, getConnFlag);
		SET_SETTING(TARGET_EXISTS_ACTION, oldValue);
	}
	catch (Exception&)
	{
		SET_SETTING(TARGET_EXISTS_ACTION, oldValue);
		throw;
	}
}

void QueueManager::add(const string& aTarget, int64_t size, const TTHValue& root, const UserPtr& user,
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

	string target;
	string tempTarget;
	
	if (fileList)
	{
		dcassert(user);
		target = getListPath(user);
		tempTarget = aTarget;
	}
	else if (testIP)
	{
		dcassert(user);
		target = getListPath(user) + ".check";
		tempTarget = aTarget;
	}
	else
	{
		if (File::isAbsolute(aTarget))
			target = aTarget;
		else
			target = FavoriteManager::getDownloadDirectory(Util::getFileExt(aTarget)) + aTarget;
		target = checkTarget(target, -1);
	}
	
	// Check if it's a zero-byte file, if so, create and return...
	if (size == 0)
	{
		if (!BOOLSETTING(SKIP_ZERO_BYTE))
		{
			File::ensureDirectory(target);
			try { File f(target, File::WRITE, File::CREATE); }
			catch (const FileException&) {}
		}
		return;
	}
	
	bool wantConnection = false;
	
	{
		// [-] CFlyLock(cs); [-] IRainman fix.
		
		QueueItemPtr q = g_fileQueue.findTarget(target);
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
			if (newItem)
			{
				int64_t existingFileSize;
				int64_t existingFileTime;
				FileAttributes attr;
				bool targetExists = File::getAttributes(target, attr);
				if (targetExists)
				{
					existingFileSize = attr.getSize();
					existingFileTime = attr.getLastWriteTime();
					if (existingFileSize == size && BOOLSETTING(SKIP_EXISTING))
					{
						LogManager::message(STRING_F(SKIPPING_EXISTING_FILE, target));
						return;
					}
					int targetExistsAction = SETTING(TARGET_EXISTS_ACTION);
					if (targetExistsAction == SettingsManager::TE_ACTION_ASK)
					{
						fly_fire5(QueueManagerListener::TryAdding(), target, size, existingFileSize, existingFileTime, targetExistsAction);
					}
					
					switch (targetExistsAction)
					{
						case SettingsManager::TE_ACTION_REPLACE:
							File::deleteFile(target); // Delete old file.
							break;
						case SettingsManager::TE_ACTION_RENAME:
							target = Util::getFilenameForRenaming(target); // Call Util::getFilenameForRenaming instead of using CheckTargetDlg's stored name
							break;
						case SettingsManager::TE_ACTION_SKIP:
							return;
					}
				}
	
				int64_t maxSizeForCopy = (int64_t) SETTING(COPY_EXISTING_MAX_SIZE) << 20;
				if (maxSizeForCopy &&
				    ShareManager::getInstance()->getFilePath(root, sharedFilePath) &&
				    File::getAttributes(sharedFilePath, attr))
				{
					auto sharedFileSize = attr.getSize();
					if (sharedFileSize == size && sharedFileSize <= maxSizeForCopy)
					{
						LogManager::message(STRING_F(COPYING_EXISTING_FILE, target));
						priority = QueueItem::PAUSED;
						flags |= QueueItem::FLAG_COPYING;
					}
				}
			}

			q = g_fileQueue.add(target, size, flags, priority, tempTarget, GET_TIME(), root, 0);

			if (q)
			{
				fly_fire1(QueueManagerListener::Added(), q);
				if (flags & QueueItem::FLAG_COPYING)
				{
					newItem = false;
					FileMoverJob* job = new FileMoverJob(*this, sharedFilePath, target, false, q);
					if (!fileMover.addJob(job))
						delete job;
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
		if (user && q && priority != QueueItem::PAUSED)
		{
			WLock(*QueueItem::g_cs);
			wantConnection = addSourceL(q, user, (QueueItem::MaskType)(addBad ? QueueItem::Source::FLAG_MASK : 0));
		}
		else
		{
			wantConnection = false;
		}
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
		WLock(*QueueItem::g_cs);
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
		const QueueItemPtr q = g_fileQueue.findTarget(target);
		WLock(*QueueItem::g_cs);
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
	if (!g_dirty)
	{
		g_dirty = true;
		g_lastSave = GET_TICK();
	}
}

string QueueManager::checkTarget(const string& aTarget, const int64_t aSize, bool validateFileName /* = true*/)
{
#ifdef _WIN32
	if (aTarget.length() > FULL_MAX_PATH)
	{
		throw QueueException(STRING(TARGET_FILENAME_TOO_LONG));
	}
	// Check that target starts with a drive or is an UNC path
	if (!(aTarget.length() > 3 && ((aTarget[1] == ':' && aTarget[2] == '\\') || (aTarget[0] == '\\' && aTarget[1] == '\\'))))
	{
		throw QueueException(STRING(INVALID_TARGET_FILE));
	}
#else
	if (aTarget.length() > PATH_MAX)
	{
		throw QueueException(STRING(TARGET_FILENAME_TOO_LONG));
	}
	// Check that target contains at least one directory...we don't want headless files...
	if (aTarget.empty() || aTarget[0] != '/')
	{
		throw QueueException(STRING(INVALID_TARGET_FILE));
	}
#endif
	
	const string target = validateFileName ? Util::validateFileName(aTarget) : aTarget;
	
	if (aSize != -1)
	{
		// Check that the file doesn't already exist...
		const int64_t sz = File::getSize(target);
		if (aSize <= sz)
			throw FileException(STRING(LARGER_TARGET_FILE_EXISTS));
	}
	return target;
}

/** Add a source to an existing queue item */
bool QueueManager::addSourceL(const QueueItemPtr& qi, const UserPtr& aUser, QueueItem::MaskType addBad, bool isFirstLoad)
{
	dcassert(aUser);
	bool wantConnection;
	{
		if (isFirstLoad)
			wantConnection = true;
		else
			wantConnection = qi->getPriority() != QueueItem::PAUSED
			                 && !g_userQueue.getRunning(aUser);
		
		if (qi->isSourceL(aUser))
		{
			if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			{
				return wantConnection;
			}
			throw QueueException(STRING(DUPLICATE_SOURCE) + ": " + Util::getFileName(qi->getTarget()));
		}
		dcassert((isFirstLoad && !qi->isBadSourceExceptL(aUser, addBad)) || !isFirstLoad);
		if (qi->isBadSourceExceptL(aUser, addBad))
		{
			throw QueueException(STRING(DUPLICATE_SOURCE) +
			                     " TTH = " + Util::getFileName(qi->getTarget()) +
			                     " Nick = " + aUser->getLastNick());
		}
		
		qi->addSourceL(aUser, isFirstLoad);
		/*if(aUser.user->isSet(User::PASSIVE) && !ClientManager::isActive(aUser.hint)) {
		    qi->removeSource(aUser, QueueItem::Source::FLAG_PASSIVE);
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
				CFlyLock(csUpdatedSources);
				sourceAdded = true;
			}
			g_userQueue.addL(qi, aUser, isFirstLoad);
		}
	}
	if (!isFirstLoad)
	{
		addUpdatedSource(qi);
		setDirty();
	}
	return wantConnection;
}

void QueueManager::addDirectory(const string& aDir, const UserPtr& aUser, const string& aTarget, QueueItem::Priority p /* = QueueItem::DEFAULT */) noexcept
{
	bool needList;
	dcassert(aUser);
	if (aUser) // torrent
	{
		{
			CFlyFastLock(csDirectories);
			
			const auto dp = m_directories.equal_range(aUser);
			
			for (auto i = dp.first; i != dp.second; ++i)
			{
				if (stricmp(aTarget.c_str(), i->second->getName().c_str()) == 0)
					return;
			}
			
			// Unique directory, fine...
			m_directories.insert(make_pair(aUser, new DirectoryItem(aUser, aDir, aTarget, p)));
			needList = (dp.first == dp.second);
		}
		
		setDirty();
		
		if (needList)
		{
			try
			{
				addList(aUser, QueueItem::FLAG_DIRECTORY_DOWNLOAD | QueueItem::FLAG_PARTIAL_LIST, aDir);
			}
			catch (const Exception&)
			{
				dcassert(0);
				// Ignore, we don't really care...
			}
		}
	}
}

QueueItem::Priority QueueManager::hasDownload(const UserPtr& aUser)
{
	RLock(*QueueItem::g_cs);
	QueueItemPtr qi;
	if (g_userQueue.getNextL(qi, aUser, QueueItem::LOWEST) != SUCCESS)
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
	if (!g_fileQueue.empty())
	{
		if (!dl.getTTHSet()) dl.buildTTHSet();
		const DirectoryListing::TTHMap& tthMap = *dl.getTTHSet();
		bool sourceAdded = false;
		if (!tthMap.empty())
		{
			WLock(*QueueItem::g_cs);
			{
				RLock(*g_fileQueue.csFQ);
				for (auto i = g_fileQueue.getQueueL().cbegin(); i != g_fileQueue.getQueueL().cend(); ++i)
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
		
	const QueueItemPtr qs = g_fileQueue.findTarget(aSource);
	if (!qs)
		return;
		
	// Don't move file lists
	if (qs->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
		return;
		
	// TODO: moving running downloads is not implemented
	if (qs->isRunning())
		return;

	// Let's see if the target exists...then things get complicated...
	const QueueItemPtr qt = g_fileQueue.findTarget(target);
	if (!qt || stricmp(aSource, target) == 0)
	{
		// Good, update the target and move in the queue...
		fly_fire2(QueueManagerListener::Moved(), qs, aSource);
		g_fileQueue.moveTarget(qs, target);
		setDirty();
	}
	else
	{
		// Don't move to target of different size
		if (qs->getSize() != qt->getSize() || qs->getTTH() != qt->getTTH())
			return; // TODO: ask user
			
		{
			WLock(*QueueItem::g_cs);
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

bool QueueManager::getQueueInfo(const UserPtr& aUser, string& aTarget, int64_t& aSize, int& aFlags) noexcept
{
	RLock(*QueueItem::g_cs);
	QueueItemPtr qi;
	if (g_userQueue.getNextL(qi, aUser) != SUCCESS)
		return false;
		
	aTarget = qi->getTarget();
	aSize = qi->getSize();
	aFlags = qi->flags;
	return true;
}

uint8_t QueueManager::FileQueue::getMaxSegments(const uint64_t filesize)
{
	unsigned value;
	if (BOOLSETTING(SEGMENTS_MANUAL))
		value = min(SETTING(NUMBER_OF_SEGMENTS), 200);
	else
		value = static_cast<unsigned>(min(filesize / (50 * MIN_BLOCK_SIZE) + 2, 200Ui64));
	if (!value) value = 1;
	return static_cast<uint8_t>(value);
}

void QueueManager::getTargets(const TTHValue& tth, StringList& sl, int maxCount)
{
	QueueItemList ql;
	g_fileQueue.findQueueItems(ql, tth, maxCount);
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
		WLock(*QueueItem::g_cs);
		
		errorInfo.error = g_userQueue.getNextL(q, u, QueueItem::LOWEST, source->getChunkSize(), source->getSpeed(), true);
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
	d = std::make_shared<Download>(source, q, source->getRemoteIp(), source->getCipherName());
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
	g_userQueue.addDownload(q, d);
	
	addUpdatedSource(q);
	dcdebug("found %s\n", q->getTarget().c_str());
	return d;
}

namespace
{
class TreeOutputStream : public OutputStream
{
	public:
		explicit TreeOutputStream(TigerTree& aTree) : tree(aTree), bufPos(0)
		{
		}
		
		size_t write(const void* xbuf, size_t len)
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

}

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
		const QueueItemPtr qi = g_fileQueue.findTarget(d->getPath());
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
		// Only use antifrag if we don't have a previous non-antifrag part
		// if (BOOLSETTING(ANTI_FRAG))
		// Всегда юзаем антифрагментатор
		{
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
		}
		
		f->setPos(d->getSegment().getStart());
		d->setDownloadFile(f);
	}
	else if (d->getType() == Transfer::TYPE_FULL_LIST)
	{
		{
			const auto path = d->getPath();
			QueueItemPtr qi = g_fileQueue.findTarget(path);
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
		// [!] IRainman fix. TODO
		d->setDownloadFile(new StringOutputStream(d->getPFS()));
		// d->setFile(new File(d->getPFS(), File::WRITE, File::OPEN | File::TRUNCATE | File::CREATE));
		// [~] IRainman fix
	}
	else if (d->getType() == Transfer::TYPE_TREE)
	{
		d->setDownloadFile(new TreeOutputStream(d->getTigerTree()));
	}
}

bool QueueManager::moveFile(const string& source, const string& target)
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
		FileMoverJob* job = new FileMoverJob(*this, source, target, moveToOtherDir, QueueItemPtr());
		if (!fileMover.addJob(job))
		{
			delete job;
			return false;
		}
		return true;
	}
	return internalMoveFile(source, target, moveToOtherDir);
}

bool QueueManager::internalMoveFile(const string& source, const string& target, bool moveToOtherDir)
{
	if (File::renameFile(source, target))
		return true;
	string error = Util::translateError();
	if (!moveToOtherDir)
	{
		LogManager::message(STRING_F(UNABLE_TO_RENAME_FMT, source % target % error));
		return false;
	}
	const string newTarget = Util::getFilePath(source) + Util::getFileName(target);
	if (!File::renameFile(source, newTarget))
	{
		LogManager::message(STRING_F(UNABLE_TO_RENAME_FMT, source % newTarget % Util::translateError()));
		return false;
	}
	LogManager::message(STRING_F(UNABLE_TO_MOVE_FMT, target % error % newTarget));
	return true;
}

void QueueManager::moveStuckFile(const QueueItemPtr& qi)
{
	moveFile(qi->getTempTarget(), qi->getTarget());
	{
		if (qi->isFinished())
		{
			g_userQueue.removeQueueItem(qi);
		}
	}
	
	const string target = qi->getTarget();
	
	if (/*!BOOLSETTING(NEVER_REPLACE_TARGET)*/true)
	{
		fire_remove_internal(qi, false, false);
	}
	else
	{
		qi->addSegment(Segment(0, qi->getSize()));
		fireStatusUpdated(qi);
	}
	if (!ClientManager::isBeforeShutdown())
	{
		fly_fire1(QueueManagerListener::RecheckAlreadyFinished(), target);
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
	fire_remove_internal(qi, false, false);
}

void QueueManager::addUpdatedSource(const QueueItemPtr& qi)
{
	if (!ClientManager::isBeforeShutdown())
	{
		CFlyLock(csUpdatedSources);
		updatedSources.insert(qi->getTarget());
	}
}

void QueueManager::fireStatusUpdated(const QueueItemPtr& qi)
{
	if (!ClientManager::isBeforeShutdown())
		fly_fire1(QueueManagerListener::StatusUpdated(), qi);
}

void QueueManager::rechecked(const QueueItemPtr& qi)
{
	fly_fire1(QueueManagerListener::RecheckDone(), qi->getTarget());
	fireStatusUpdated(qi);
	setDirty();
}

void QueueManager::putDownload(const string& path, DownloadPtr download, bool finished, bool reportFinish) noexcept
{
#if 0
	dcassert(!ClientManager::isBeforeShutdown());
	// fix https://drdump.com/Problem.aspx?ProblemID=112136
	if (!ClientManager::isBeforeShutdown())
	{
		// TODO - check and delete download?
		// return;
	}
#endif
	UserList getConn;
	string fileName;
	const HintedUser hintedUser = download->getHintedUser();
	UserPtr user = download->getUser();
	
	dcassert(user);
	
	QueueItem::MaskType flags = 0;
	bool downloadList = false;
	
	{
		download->resetDownloadFile();
		
		if (download->getType() == Transfer::TYPE_PARTIAL_LIST)
		{
			QueueItemPtr q = g_fileQueue.findTarget(path);
			if (q)
			{
				//q->setFailed(!download->m_reason.empty());
				
				if (!download->getPFS().empty())
				{
					// [!] IRainman fix.
					bool fulldir = q->isSet(QueueItem::FLAG_MATCH_QUEUE);
					if (!fulldir)
					{
						fulldir = q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD);
						if (fulldir)
						{
							CFlyFastLock(csDirectories);
							fulldir &= m_directories.find(download->getUser()) != m_directories.end();
						}
					}
					if (fulldir)
					{
						fileName = download->getPFS();
						flags = (q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD) ? QueueItem::FLAG_DIRECTORY_DOWNLOAD : 0)
						        | (q->isSet(QueueItem::FLAG_MATCH_QUEUE) ? QueueItem::FLAG_MATCH_QUEUE : 0)
						        | QueueItem::FLAG_TEXT;
					}
					else
					{
						fly_fire2(QueueManagerListener::PartialList(), hintedUser, download->getPFS());
					}
				}
				else
				{
					// partial filelist probably failed, redownload full list
					dcassert(!finished);
					
					downloadList = true;
					flags = q->flags & ~QueueItem::FLAG_PARTIAL_LIST;
				}
				
				fire_remove_internal(q, true, true);
			}
		}
		else
		{
			QueueItemPtr q = g_fileQueue.findTarget(path);
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
					if (download->getType() == Transfer::TYPE_TREE)
					{
						// Got a full tree, now add it to the database
						dcassert(download->getTreeValid());
						DatabaseManager::getInstance()->addTree(download->getTigerTree());
						g_userQueue.removeDownload(q, download->getUser());
						
						fireStatusUpdated(q);
					}
					else
					{
						// Now, let's see if this was a directory download filelist...
						dcassert(!q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD));
						if (q->isSet(QueueItem::FLAG_MATCH_QUEUE))
						{
							fileName = q->getListName();
							flags = q->isSet(QueueItem::FLAG_MATCH_QUEUE) ? QueueItem::FLAG_MATCH_QUEUE : 0;
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
								if (isFile && !download->getTempTarget().empty() && stricmp(path.c_str(), download->getTempTarget().c_str()) != 0)
									moveFile(download->getTempTarget(), path);

								SharedFileStream::cleanup();
								if (BOOLSETTING(LOG_DOWNLOADS) && (BOOLSETTING(LOG_FILELIST_TRANSFERS) || isFile))
								{
									StringMap params;
									download->getParams(params);
									LOG(DOWNLOAD, params);
								}
							}

							if (!q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
								DatabaseManager::getInstance()->setFileInfoDownloaded(q->getTTH(), q->getSize(), path);
							
							if (!ClientManager::isBeforeShutdown())
							{
								fly_fire3(QueueManagerListener::Finished(), q, dir, download);
							}
							
#ifdef FLYLINKDC_USE_DETECT_CHEATING
							if (q->isSet(QueueItem::FLAG_USER_LIST))
							{
								const auto user = q->getFirstUser();
								if (user)
									m_listQueue.addTask(new DirectoryListInfo(HintedUser(user, Util::emptyString), q->getListName(), dir, download->getRunningAverage()));
							}
#endif
							g_userQueue.removeQueueItem(q);
							if (q->isSet(QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_DOWNLOAD_CONTENTS))
							{
								addDclstFile(q->getTarget());
							}
							
							if (/*!BOOLSETTING(NEVER_REPLACE_TARGET) || download->getType() == Transfer::TYPE_FULL_LIST*/true)
							{
								fire_remove_internal(q, false, false);
							}
							else
							{
								fireStatusUpdated(q);
							}
						}
						else
						{
							g_userQueue.removeDownload(q, download->getUser());
							if (download->getType() != Transfer::TYPE_FILE || (reportFinish && q->isWaiting()))
							{
								fireStatusUpdated(q);
							}
						}
						setDirty();
					}
				}
				else
				{
					if (download->getType() != Transfer::TYPE_TREE)
					{
						bool isEmpty;
						{
							// TODO - убрать лок тут
							RLock(*QueueItem::g_cs);
							q->updateDownloadedBytesAndSpeedL();
							isEmpty = q->getDownloadedBytes() == 0;
						}
						// Не затираем путь к временному файлу
						//if (isEmpty)
						//{
						//  q->setTempTarget(Util::emptyString);
						//}
						if (q->isSet(QueueItem::FLAG_USER_LIST))
						{
							// Blah...no use keeping an unfinished file list...
							File::deleteFile(q->getListName());
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
					
					g_userQueue.removeDownload(q, download->getUser());
					
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
			else if (download->getType() != Transfer::TYPE_TREE)
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

	if (!fileName.empty())
		processList(fileName, hintedUser, flags);
	
	// partial file list failed, redownload full list
	if (downloadList && user->isOnline())
	{
		try
		{
			addList(user, flags);
		}
		catch (const Exception&) {}
	}
}

static void logMatchedFiles(const UserPtr& user, int count)
{
	dcassert(user);
	string str = STRING_F(MATCHED_FILES_FMT, count);
	str += ": ";
	str += user->getLastNick();
	LogManager::message(str);
}

void QueueManager::processList(const string& name, const HintedUser& hintedUser, int flags)
{
	dcassert(hintedUser.user);
	
	std::atomic_bool abortFlag(false);
	DirectoryListing dirList(abortFlag);
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
		LogManager::message(STRING(UNABLE_TO_OPEN_FILELIST) + ' ' + name);
		return;
	}
	if (!dirList.getRoot()->getTotalFileCount())
	{
		LogManager::message(STRING(UNABLE_TO_OPEN_FILELIST) + " (dirList.getTotalFileCount() == 0) " + name);
		return;
	}
	#if 0 // FIXME: function removed
	if (flags & QueueItem::FLAG_DIRECTORY_DOWNLOAD)
	{
		vector<DirectoryItemPtr> dl;
		{
			CFlyFastLock(csDirectories);
			const auto dp = m_directories.equal_range(hintedUser.user) | map_values;
			dl.assign(boost::begin(dp), boost::end(dp));
			m_directories.erase(hintedUser.user);
		}
		
		for (auto i = dl.cbegin(); i != dl.cend(); ++i)
		{
			DirectoryItem* di = *i;
			dirList.download(di->getName(), di->getTarget(), false);
			delete di;
		}
	}
	#endif
	if (flags & QueueItem::FLAG_MATCH_QUEUE)
	{
		logMatchedFiles(hintedUser.user, matchListing(dirList));
	}
}

void QueueManager::removeAll()
{
	g_fileQueue.clearAll();
}

void QueueManager::fire_remove_internal(const QueueItemPtr& qi, bool p_is_remove_item, bool p_is_force_remove_item)
{
	if (!ClientManager::isBeforeShutdown())
		fly_fire1(QueueManagerListener::Removed(), qi);
	
	if (p_is_remove_item)
	{
		if (p_is_force_remove_item || p_is_remove_item && !qi->isFinished())
		{
			g_userQueue.removeQueueItem(qi);
		}
	}
	g_fileQueue.removeInternal(qi);
	fly_fire1(QueueManagerListener::RemovedTransfer(), qi);
}

bool QueueManager::removeTarget(const string& aTarget, bool isBatchRemove)
{
#if 0
	string logMessage = "removeTarget: " + aTarget;
	DumpDebugMessage(_T("queue-debug.log"), logMessage.c_str(), logMessage.length(), true);
#endif

	UserList x;
	{
		QueueItemPtr q = g_fileQueue.findTarget(aTarget);
		if (!q)
			return false;
			
		if (q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD))
		{
			RLock(*QueueItem::g_cs); // [+] IRainman fix.
			dcassert(q->getSourcesL().size() == 1);
			{
				CFlyFastLock(csDirectories); // [+] IRainman fix.
				for_each(m_directories.equal_range(q->getSourcesL().begin()->first) | map_values, [](auto p) { delete p; });
				m_directories.erase(q->getSourcesL().begin()->first);
			}
		}

		if (!q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
			DatabaseManager::getInstance()->setFileInfoCanceled(q->getTTH(), q->getSize());
		
		const string& tempTarget = q->getTempTargetConst();
		if (!tempTarget.empty())
		{
			// For partial-share
			UploadManager::getInstance()->abortUpload(tempTarget);
		}
		
		if (q->isRunning())
		{
			q->getUsers(x);
		}
		else if (!tempTarget.empty() && tempTarget != q->getTarget())
		{
			if (File::isExist(tempTarget))
			{
				if (!File::deleteFile(tempTarget))
					SharedFileStream::deleteFile(tempTarget);
			}
		}
		
		q->removed = true;
		fire_remove_internal(q, true, false);
		setDirty();
	}
	
	for (auto i = x.cbegin(); i != x.cend(); ++i)
	{
		ConnectionManager::disconnect(*i, true);
	}
	return true;
}

void QueueManager::removeSource(const string& target, const UserPtr& user, Flags::MaskType reason, bool removeConn /* = true */) noexcept
{
	bool isRunning = false;
	bool removeCompletely = false;
	do
	{
		RLock(*QueueItem::g_cs);
		QueueItemPtr q = g_fileQueue.findTarget(target);
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
		
		if (q->isRunning() && g_userQueue.getRunning(user) == q)
		{
			isRunning = true;
			g_userQueue.removeDownload(q, user);
			addUpdatedSource(q);
		}
		if (!q->isFinished())
		{
			g_userQueue.removeUserL(q, user, true);
		}
		q->removeSourceL(user, reason);
		
		addUpdatedSource(q);
		setDirty();
	}
	while (false);
	
	if (isRunning && removeConn)
	{
		ConnectionManager::disconnect(user, true);
	}
	if (removeCompletely)
	{
		removeTarget(target, false);
	}
}

void QueueManager::removeSource(const UserPtr& aUser, Flags::MaskType reason) noexcept
{
	// @todo remove from finished items
	bool isRunning = false;
	string removeRunning;
	{
		QueueItemPtr qi;
		WLock(*QueueItem::g_cs);
		while (g_userQueue.getNextL(qi, aUser, QueueItem::PAUSED) == QueueManager::SUCCESS)
		{
			if (qi->isSet(QueueItem::FLAG_USER_LIST))
			{
				bool found = removeTarget(qi->getTarget(), false);
				if (!found)
				{
					break;
					/*
					Читаем например с с юзера список файлов. В трансфере (внизу, окно передач) висит -1Б ( у меня гавноинторенты, но можно повторить на БОЛЬШОМ файллисте наверное),
					Идём в Очередь Скачивания. Слева в дереве - в ПКМ на File Lists выбираем Удалить всё / Удалить... бла-бла-бла
					Внизу в трансфере на строке с -1Б давим ПКМ, выбираем Удалить пользователя из очереди
					Флай виснет намертво.
					*/
				}
			}
			else
			{
				g_userQueue.removeUserL(qi, aUser, true);
				qi->removeSourceL(aUser, reason);
				addUpdatedSource(qi);
				setDirty();
			}
		}
		
		qi = g_userQueue.getRunning(aUser);
		if (qi)
		{
			if (qi->isSet(QueueItem::FLAG_USER_LIST))
			{
				removeRunning = qi->getTarget();
			}
			else
			{
				g_userQueue.removeDownload(qi, aUser);
				g_userQueue.removeUserL(qi, aUser, true);
				isRunning = true;
				qi->removeSourceL(aUser, reason);
				fireStatusUpdated(qi);
				addUpdatedSource(qi);
				setDirty();
			}
		}
	}
	
	if (isRunning)
	{
		ConnectionManager::disconnect(aUser, true);
	}
	if (!removeRunning.empty())
	{
		removeTarget(removeRunning, false);
	}
}

void QueueManager::setPriority(const string& target, QueueItem::Priority p, bool resetAutoPriority) noexcept
{
	UserList getConn;
	bool isRunning = false;
	
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		QueueItemPtr q = g_fileQueue.findTarget(target);
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
				g_userQueue.setQIPriority(q, p); // remove and re-add the item
#ifdef _DEBUG
				LogManager::message("QueueManager g_userQueue.setQIPriority q->getTarget = " + q->getTarget());
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
			DownloadManager::abortDownload(target);
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
		QueueItemPtr q = g_fileQueue.findTarget(target);
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
	if (!g_dirty && !force)
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

		RLock(*QueueItem::g_cs);
		{
			RLock(*g_fileQueue.csFQ);

			for (auto i = g_fileQueue.getQueueL().cbegin(); i != g_fileQueue.getQueueL().cend(); ++i)
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

		g_dirty = false;
	}
	catch(...)
	{
		// ...
	}
	// Put this here to avoid very many saves tries when disk is full...
	g_lastSave = GET_TICK();
}

class QueueLoader : public SimpleXMLReader::CallBack
{
	public:
		QueueLoader() : cur(nullptr), isInDownloads(false)
		{
			qm = QueueManager::getInstance();
		}
		~QueueLoader() { }
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
		QueueLoader l;
		File f(getQueueFile(), File::READ, File::OPEN);
		SimpleXMLReader(&l).parse(f);
	}
	catch (const Exception&)
	{
	}
	g_dirty = false;
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
			if (size == 0)
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
			const string& tthRoot = getAttrib(attribs, sTTH, 5);
			if (tthRoot.empty())
				return;
				
			const string& tempTarget = getAttrib(attribs, sTempTarget, 5);
			const uint8_t maxSegments = (uint8_t)Util::toInt(getAttrib(attribs, sMaxSegments, 5));
			int64_t downloaded = Util::toInt64(getAttrib(attribs, sDownloaded, 5));
			if (downloaded > size || downloaded < 0)
				downloaded = 0;
				
			if (added == 0)
				added = GET_TIME();
				
			QueueItemPtr qi = QueueManager::g_fileQueue.findTarget(target);
			
			if (qi == NULL)
			{
				qi = QueueManager::g_fileQueue.add(target, size, 0, p, tempTarget, added, TTHValue(tthRoot), maxSegments);
				if (downloaded > 0)
				{
					qi->addSegment(Segment(0, downloaded));
					qi->setPriority(qi->calculateAutoPriority());
				}
				
				const bool ap = Util::toInt(getAttrib(attribs, sAutoPriority, 6)) == 1;
				qi->setAutoPriority(ap);
				
				qm->fly_fire1(QueueManagerListener::Added(), qi);
			}
			if (!simple)
				cur = qi;
		}
		else if (cur && name == sSegment)
		{
			int64_t start = Util::toInt64(getAttrib(attribs, sStart, 0));
			int64_t size = Util::toInt64(getAttrib(attribs, sSize, 1));
			
			if (size > 0 && start >= 0 && start + size <= cur->getSize())
			{
				cur->addSegment(Segment(start, size));
				cur->setPriority(cur->calculateAutoPriority());
			}
		}
		else if (cur && name == sSource)
		{
			const string cid = getAttrib(attribs, sCID, 0);
			UserPtr user;
			const string nick = getAttrib(attribs, sNick, 1);
			const string hubHint = getAttrib(attribs, sHubHint, 1);
			if (cid.length() != 39)
			{
				user = ClientManager::getUser(nick, hubHint);
			}
			else
			{
				user = ClientManager::createUser(CID(cid), nick, hubHint);
			}
			
			bool wantConnection;
			try
			{
				WLock(*QueueItem::g_cs);
				wantConnection = qm->addSourceL(cur, user, 0) && user->isOnline();
			}
			catch (const Exception&)
			{
				return;
			}
			if (wantConnection)
			{
				QueueManager::getDownloadConnection(user);
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
				cur = nullptr;
			}
		}
		else if (name == "Downloads")
		{
			isInDownloads = false;
		}
	}
}

#ifdef FLYLINKDC_USE_KEEP_LISTS
void QueueManager::noDeleteFileList(const string& path)
{
	if (!BOOLSETTING(KEEP_LISTS))
	{
		protectedFileLists.push_back(path);
	}
}
#endif

// SearchManagerListener
void QueueManager::on(SearchManagerListener::SR, const SearchResult& sr) noexcept
{
	bool added = false;
	bool wantConnection = false;
	bool downloadFileList = false;
	
	{
		QueueItemList matches;
		g_fileQueue.findQueueItems(matches, sr.getTTH());
		if (!matches.empty())
		{
			WLock(*QueueItem::g_cs);
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
						if (qi->getPriority() != QueueItem::PAUSED && !g_userQueue.getRunning(sr.getUser()))
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
			// [!] IRainman fix: please always match listing without hint! old code: sr->getHubUrl().
			addList(sr.getUser(), QueueItem::FLAG_MATCH_QUEUE | (path.empty() ? 0 : QueueItem::FLAG_PARTIAL_LIST), path);
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
	const bool hasDown = g_userQueue.getQueuedItems(user, itemList);
	if (!itemList.empty())
	{
		for (auto& qi : itemList)
			qi->cachedOnlineSourceCountInvalid = true;
		fly_fire1(QueueManagerListener::StatusUpdatedList(), itemList);
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
	g_userQueue.getQueuedItems(user, itemList);
	if (!itemList.empty())
	{
		for (auto& qi : itemList)
			qi->cachedOnlineSourceCountInvalid = true;
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire1(QueueManagerListener::StatusUpdatedList(), itemList);
		}
	}
	g_userQueue.removeRunning(user); // fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1673
}

bool QueueManager::isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& target)
{
	QueueItemPtr qi = g_fileQueue.findQueueItem(tth);
	if (!qi)
		return false;
	target = qi->isFinished() ? qi->getTarget() : qi->getTempTarget();
	
	return qi->isChunkDownloaded(startPos, bytes);
}

void QueueManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (g_dirty && (g_lastSave + SAVE_QUEUE_TIME < aTick))
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
		RLock(*g_fileQueue.csFQ);
		getRunningFilesL(runningItems);
	}
	if (!runningItems.empty())
	{
		fly_fire1(QueueManagerListener::Tick(), runningItems); // Don't fire when locked
	}
	StringList targetList;
	bool sourceAddedFlag = false;
	{
		CFlyLock(csUpdatedSources);
		targetList.reserve(updatedSources.size());
		for (auto i = updatedSources.cbegin(); i != updatedSources.cend(); ++i)
			targetList.push_back(*i);
		updatedSources.clear();
		std::swap(sourceAdded, sourceAddedFlag);
	}
	if (!targetList.empty())
		fly_fire1(QueueManagerListener::TargetsUpdated(), targetList);
	if (sourceAddedFlag)
		fly_fire(QueueManagerListener::SourceAdded());
}

#ifdef FLYLINKDC_USE_DROP_SLOW
bool QueueManager::dropSource(const DownloadPtr& aDownload)
{
	uint8_t activeSegments = 0;
	bool   allowDropSource;
	uint64_t overallSpeed;
	{
		RLock(*QueueItem::g_cs);
		
		const QueueItemPtr q = g_userQueue.getRunning(aDownload->getUser());
		
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

bool QueueManager::handlePartialResult(const UserPtr& user, const TTHValue& tth, QueueItem::PartialSource& partialSource, PartsInfo& outPartialInfo)
{
	bool wantConnection = false;
	dcassert(outPartialInfo.empty());
	
	{
		// Locate target QueueItem in download queue
		QueueItemPtr qi = g_fileQueue.findQueueItem(tth);
		if (!qi)
			return false;
		LogManager::psr_message("[QueueManager::handlePartialResult] findQueueItem - OK TTH = " + tth.toBase32());
		
		
		// don't add sources to finished files
		// this could happen when "Keep finished files in queue" is enabled
		if (qi->isFinished())
			return false;
			
		// Check min size
		if (qi->getSize() < PARTIAL_SHARE_MIN_SIZE)
		{
			dcassert(0);
			LogManager::psr_message(
			    "[QueueManager::handlePartialResult] qi->getSize() < PARTIAL_SHARE_MIN_SIZE. qi->getSize() = " + Util::toString(qi->getSize()));
			return false;
		}
		
		// Get my parts info
		const auto blockSize = qi->getBlockSize();
		partialSource.setBlockSize(blockSize);
		qi->getPartialInfo(outPartialInfo, qi->getBlockSize());
		
		// Any parts for me?
		wantConnection = qi->isNeededPart(partialSource.getPartialInfo(), blockSize);
		
		WLock(*QueueItem::g_cs); // TODO - опустить ниже?
		
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
			}
			else
			{
				// add this user as partial file sharing source
				qi->addSourceL(user, false);
				si = qi->findSourceL(user); // TODO - повторный поиск?
				si->second.setFlag(QueueItem::Source::FLAG_PARTIAL);
				
				const auto ps = std::make_shared<QueueItem::PartialSource>(partialSource.getMyNick(),
					partialSource.getHubIpPort(), partialSource.getIp(), partialSource.getUdpPort(), partialSource.getBlockSize());
				si->second.setPartialSource(ps);
				
				g_userQueue.addL(qi, user, false);
				dcassert(si != qi->getSourcesL().end());
				addUpdatedSource(qi);
				LogManager::psr_message(
				    "[QueueManager::handlePartialResult] new QueueItem::PartialSource nick = " + partialSource.getMyNick() +
				    " HubIpPort = " + partialSource.getHubIpPort() +
				    " IP = " + partialSource.getIp().to_string() +
				    " UDP port = " + Util::toString(partialSource.getUdpPort())
				);
			}
		}
		
		// Update source's parts info
		if (si->second.getPartialSource())
			si->second.getPartialSource()->setPartialInfo(partialSource.getPartialInfo());
	}
	
	// Connect to this user
	if (wantConnection)
		getDownloadConnection(user);
	
	return true;
}

bool QueueManager::handlePartialSearch(const TTHValue& tth, PartsInfo& outPartsInfo)
{
	// Locate target QueueItem in download queue
	const QueueItemPtr qi = g_fileQueue.findQueueItem(tth);
	if (!qi)
		return false;
	if (qi->getSize() < PARTIAL_SHARE_MIN_SIZE)
		return false;
	
	// don't share when file does not exist
	if (!File::isExist(qi->isFinished() ? qi->getTarget() : qi->getTempTargetConst()))
		return false;
		
	qi->getPartialInfo(outPartsInfo, qi->getBlockSize());
	return !outPartsInfo.empty();
}

// compare nextQueryTime, get the oldest ones
void QueueManager::FileQueue::findPFSSourcesL(PFSSourceList& sl) const
{
	QueueItem::SourceListBuffer buffer;
#ifdef _DEBUG
	QueueItem::SourceListBuffer debug_buffer;
#endif
	const uint64_t now = GET_TICK();
	// TODO - утащить в отдельный метод
	RLock(*csFQ);
	for (auto i = queue.cbegin(); i != queue.cend(); ++i)
	{
		if (ClientManager::isBeforeShutdown())
			return;
		const auto q = i->second;
		
		if (q->getSize() < PARTIAL_SHARE_MIN_SIZE) continue;
		
		// don't share when file does not exist
		if (q->m_is_file_not_exist == true)
		{
			continue;
		}
		if (q->m_is_file_not_exist == false && !File::isExist(q->isFinished() ? q->getTarget() : q->getTempTargetConst())) // Обязательно Const
		{
			q->m_is_file_not_exist = true;
			continue;
		}
		
		QueueItem::getPFSSourcesL(q, buffer, now);
//////////////////
#ifdef _DEBUG
		// После отладки убрать сарый вариант наполнения
		const auto& sources = q->getSourcesL();
		const auto& badSources = q->getBadSourcesL();
		for (auto j = sources.cbegin(); j != sources.cend(); ++j)
		{
			const auto &l_getPartialSource = j->second.getPartialSource(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the '(* j).getPartialSource()' expression repeatedly. queuemanager.cpp 2900
			if (j->second.isSet(QueueItem::Source::FLAG_PARTIAL) &&
			        l_getPartialSource->getNextQueryTime() <= now &&
			        l_getPartialSource->getPendingQueryCount() < 10 && l_getPartialSource->getUdpPort() > 0)
			{
				debug_buffer.insert(make_pair(l_getPartialSource->getNextQueryTime(), make_pair(j, q)));
			}
			
		}
		for (auto j = badSources.cbegin(); j != badSources.cend(); ++j)
		{
			const auto &l_getPartialSource = j->second.getPartialSource(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the '(* j).getPartialSource()' expression repeatedly. queuemanager.cpp 2900
			if (j->second.isSet(QueueItem::Source::FLAG_TTH_INCONSISTENCY) == false && j->second.isSet(QueueItem::Source::FLAG_PARTIAL) &&
			        l_getPartialSource->getNextQueryTime() <= now && l_getPartialSource->getPendingQueryCount() < 10 &&
			        l_getPartialSource->getUdpPort() > 0)
			{
				debug_buffer.insert(make_pair(l_getPartialSource->getNextQueryTime(), make_pair(j, q)));
			}
		}
#endif
//////////////////
	}
	// TODO: opt this function.
	// copy to results
	dcassert(debug_buffer == buffer);
	dcassert(sl.empty());
	if (!buffer.empty())
	{
		const size_t maxElements = 10;
		sl.reserve(std::min(buffer.size(), maxElements));
		for (auto i = buffer.cbegin(); i != buffer.cend() && sl.size() < maxElements; ++i)
		{
			sl.push_back(i->second);
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
	if (qi)
		manager.copyFile(source, target, qi);
	else
		manager.internalMoveFile(source, target, moveToOtherDir);
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
	size_t flushBuffers(bool aForce) override
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
		q = g_fileQueue.findTarget(file);
		if (!q || q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			return;
			
		manager.fly_fire1(QueueManagerListener::RecheckStarted(), q->getTarget());
		dcdebug("Rechecking %s\n", file.c_str());
		
		tempSize = File::getSize(q->getTempTarget());
		
		if (tempSize == -1)
		{
			manager.fly_fire1(QueueManagerListener::RecheckNoFile(), q->getTarget());
			q->resetDownloaded();
			manager.rechecked(q);
			return;
		}
		
		if (tempSize < 64 * 1024)
		{
			manager.fly_fire1(QueueManagerListener::RecheckFileTooSmall(), q->getTarget());
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
			catch (FileException& p_e)
			{
				LogManager::message("[Error] setSize - " + q->getTempTarget() + " Error=" + p_e.getError());
			}
		}
		
		if (q->isRunning())
		{
			manager.fly_fire1(QueueManagerListener::RecheckDownloadsRunning(), q->getTarget());
			return;
		}
		
		tth = q->getTTH();
	}
	
	TigerTree tt;
	string tempTarget;
	
	{
		// get q again in case it has been (re)moved
		q = g_fileQueue.findTarget(file);
		if (!q)
			return;
		if (!DatabaseManager::getInstance()->getTree(tth, tt))
		{
			manager.fly_fire1(QueueManagerListener::RecheckNoTree(), q->getTarget());
			return;
		}
		
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
	q = g_fileQueue.findTarget(file);
	
	if (!q)
		return;
		
	//If no bad blocks then the file probably got stuck in the temp folder for some reason
	if (!hasBadBlocks)
	{
		manager.moveStuckFile(q);
		return;
	}
	
	{
		CFlyFastLock(q->csSegments);
		q->resetDownloadedL();
		for (auto i = sizes.cbegin(); i != sizes.cend(); ++i)
		{
			q->addSegmentL(Segment(i->first, i->second));
		}
	}
	
	manager.rechecked(q);
}
