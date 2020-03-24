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
#include "QueueItem.h"
#include "LogManager.h"
#include "Download.h"
#include "File.h"
#include "CFlylinkDBManager.h"
#include "ClientManager.h"
#include "UserConnection.h"

#ifdef FLYLINKDC_USE_RWLOCK
std::unique_ptr<webrtc::RWLockWrapper> QueueItem::g_cs = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
#else
std::unique_ptr<CriticalSection> QueueItem::g_cs = std::unique_ptr<CriticalSection>(new CriticalSection);
#endif

const string g_dc_temp_extension = "dctmp";

QueueItem::QueueItem(const string& aTarget, int64_t aSize, Priority aPriority, bool aAutoPriority, Flags::MaskType aFlag,
                     time_t aAdded, const TTHValue& p_tth, uint8_t p_maxSegments, const string& aTempTarget) :
	m_Target(aTarget),
	m_tempTarget(aTempTarget),
	m_maxSegments(std::max(uint8_t(1), p_maxSegments)),
	timeFileBegin(0),
	m_Size(aSize),
	m_priority(aPriority),
	added(aAdded),
	m_AutoPriority(aAutoPriority),
	m_dirty_base(false),
	m_dirty_source(false),
	m_dirty_segment(false),
	m_is_file_not_exist(false),
//	m_is_failed(false),
	m_tthRoot(p_tth),
	downloadedBytes(0),
	doneSegmentsSize(0),
	lastsize(0),
	averageSpeed(0),
	m_diry_sources(0),
	m_last_count_online_sources(0),
	blockSize(64 * 1024),
	removed(false)
{
	m_dirty_base = true;
#ifdef _DEBUG
	//LogManager::message("QueueItem::QueueItem aTarget = " + aTarget + " this = " + Util::toString(__int64(this)));
#endif
#ifdef FLYLINKDC_USE_DROP_SLOW
	if (BOOLSETTING(ENABLE_AUTO_DISCONNECT))
	{
		aFlag |= FLAG_AUTODROP;
	}
#endif
	flags = aFlag;
}

QueueItem::~QueueItem()
{
#if 0
	LogManager::message("[~~~~] QueueItem::~QueueItem aTarget = " + this->getTarget() + " this = " + Util::toString(__int64(this)));
#endif
}

int16_t QueueItem::getTransferFlags(int& flags) const
{
	flags = TRANSFER_FLAG_DOWNLOAD;
	int16_t segs = 0;
	CFlyFastLock(m_fcs_download);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		const auto& d = *i;
		if (d->getStartTime() > 0)
		{
			segs++;
			
			if (d->isSet(Download::FLAG_DOWNLOAD_PARTIAL))
				flags |= TRANSFER_FLAG_PARTIAL;
			if (d->isSecure)
			{
				flags |= TRANSFER_FLAG_SECURE;
				if (d->isTrusted)
					flags |= TRANSFER_FLAG_TRUSTED;
			}
			if (d->isSet(Download::FLAG_TTH_CHECK))
				flags |= TRANSFER_FLAG_TTH_CHECK;
			if (d->isSet(Download::FLAG_ZDOWNLOAD))
				flags |= TRANSFER_FLAG_COMPRESSED;
			if (d->isSet(Download::FLAG_CHUNKED))
				flags |= TRANSFER_FLAG_CHUNKED;
		}
	}
	return segs;
}

QueueItem::Priority QueueItem::calculateAutoPriority() const
{
	if (getAutoPriority())
	{
		QueueItem::Priority p;
		const int percent = static_cast<int>(getDownloadedBytes() * 10 / getSize());
		switch (percent)
		{
			case 0:
			case 1:
			case 2:
				p = QueueItem::LOW;
				break;
			case 3:
			case 4:
			case 5:
			default:
				p = QueueItem::NORMAL;
				break;
			case 6:
			case 7:
			case 8:
				p = QueueItem::HIGH;
				break;
			case 9:
			case 10:
				p = QueueItem::HIGHER;
				break;
		}
		return p;
	}
	return getPriority();
}

string QueueItem::getDCTempName(const string& fileName, const TTHValue* tth)
{
	string result = fileName;
	Util::fixFileNameMaxPathLimit(result);
	if (tth)
	{
		result += '.';
		result += tth->toBase32();
	}
	result += '.';
	result += g_dc_temp_extension;
	return result;
}

/*
void QueueItem::calcBlockSize()
{
	m_block_size = CFlylinkDBManager::getInstance()->get_block_size_sql(getTTH(), getSize());
	dcassert(m_block_size);
}
*/

size_t QueueItem::getLastOnlineCount()
{
	if (m_diry_sources)
	{
		RLock(*QueueItem::g_cs);
		m_last_count_online_sources = 0;
		for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
		{
			if (i->first->isOnline())
				++m_last_count_online_sources;
		}
		m_diry_sources = 0;
	}
	return m_last_count_online_sources;
}

bool QueueItem::isBadSourceExceptL(const UserPtr& aUser, Flags::MaskType exceptions) const
{
	const auto& i = m_badSources.find(aUser);
	if (i != m_badSources.end())
	{
		return i->second.isAnySet((Flags::MaskType)(exceptions ^ Source::FLAG_MASK));
	}
	return false;
}

bool QueueItem::countOnlineUsersGreatOrEqualThanL(const size_t maxValue) const // [+] FlylinkDC++ opt.
{
	if (m_sources.size() < maxValue)
	{
		return false;
	}
	size_t count = 0;
	for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
	{
		if (i->first->isOnline())
		{
			if (++count == maxValue)
			{
				return true;
			}
		}
		/* TODO: needs?
		else if (maxValue - count > static_cast<size_t>(sources.cend() - i))
		{
		    return false;
		}
		/*/
	}
	return false;
}

void QueueItem::getOnlineUsers(UserList& list) const
{
	if (!ClientManager::isBeforeShutdown())
	{
		RLock(*QueueItem::g_cs);
		for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
		{
			if (i->first->isOnline())
			{
				list.push_back(i->first);
			}
		}
	}
}

void QueueItem::addSourceL(const UserPtr& aUser, bool p_is_first_load)
{
	if (p_is_first_load == true)
	{
		m_sources.insert(std::make_pair(aUser, Source()));
	}
	else
	{
		dcassert(!isSourceL(aUser));
		SourceIter i = findBadSourceL(aUser);
		if (i != m_badSources.end())
		{
			m_sources.insert(*i);
			m_badSources.erase(i->first);
		}
		else
		{
			m_sources.insert(std::make_pair(aUser, Source()));  // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=139307
		}
		setDirtySource(true);
	}
	m_diry_sources++;
}

void QueueItem::getPFSSourcesL(const QueueItemPtr& p_qi, SourceListBuffer& p_sourceList, uint64_t p_now)
{
	auto addToList = [&](const bool isBadSourses) -> void
	{
		const auto& sources = isBadSourses ? p_qi->getBadSourcesL() : p_qi->getSourcesL();
		for (auto j = sources.cbegin(); j != sources.cend(); ++j)
		{
			const auto &l_ps = j->second.getPartialSource();
			if (j->second.isCandidate(isBadSourses) && l_ps->isCandidate(p_now))
			{
				p_sourceList.insert(make_pair(l_ps->getNextQueryTime(), make_pair(j, p_qi)));
			}
		}
	};
	
	addToList(false);
	addToList(true);
}

void QueueItem::resetDownloaded()
{
	CFlyFastLock(m_fcs_segment);
	resetDownloadedL();
}

void QueueItem::resetDownloadedL()
{
	if (!doneSegments.empty())
	{
		setDirtySegment(true);
		doneSegments.clear();
	}
	doneSegmentsSize = 0;
}

bool QueueItem::isFinished() const
{
	if (doneSegments.size() == 1)
	{
		CFlyFastLock(m_fcs_segment);
		return doneSegments.size() == 1 &&
			doneSegments.begin()->getStart() == 0 &&
			doneSegments.begin()->getSize() == getSize();
	}
	return false;
}

bool QueueItem::isChunkDownloaded(int64_t startPos, int64_t& len) const
{
	if (len <= 0)
		return false;
	CFlyFastLock(m_fcs_segment);
	for (auto i = doneSegments.cbegin(); i != doneSegments.cend(); ++i)
	{
		const int64_t& start = i->getStart();
		const int64_t& end   = i->getEnd();
		if (start <= startPos && startPos < end)
		{
			len = min(len, end - startPos);
			return true;
		}
	}
	return false;
}

void QueueItem::removeSourceL(const UserPtr& aUser, Flags::MaskType reason)
{
	SourceIter i = findSourceL(aUser); // crash - https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=42877 && http://www.flickr.com/photos/96019675@N02/10488126423/
	dcassert(i != m_sources.end());
	if (i != m_sources.end()) // https://drdump.com/Problem.aspx?ProblemID=129066
	{
		i->second.setFlag(reason);
		m_badSources.insert(*i);
		m_sources.erase(i);
		m_diry_sources++;
		setDirtySource(true);
	}
	else
	{
		string error = "Error QueueItem::removeSourceL, aUser = [" + aUser->getLastNick() + "]";
		LogManager::message(error);
	}
}

string QueueItem::getListName() const
{
	dcassert(isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST));
	if (isSet(QueueItem::FLAG_XML_BZLIST))
		return getTarget() + ".xml.bz2";
	if (isSet(QueueItem::FLAG_DCLST_LIST))
		return getTarget();
	return getTarget() + ".xml";
}

const string& QueueItem::getTempTarget()
{
	if (!isSet(QueueItem::FLAG_USER_LIST) && m_tempTarget.empty())
	{
		const TTHValue& tth = getTTH();
		const string& tempDirectory = SETTING(TEMP_DOWNLOAD_DIRECTORY);
		const string tempName = getDCTempName(getTargetFileName(), tempDirectory.empty()? nullptr : &tth);
		if (!tempDirectory.empty() && File::getSize(getTarget()) == -1)
		{
			::StringMap sm;
			if (m_Target.length() >= 3 && m_Target[1] == ':' && m_Target[2] == '\\')
				sm["targetdrive"] = m_Target.substr(0, 3);
			else
				sm["targetdrive"] = Util::getLocalPath().substr(0, 3);
				
			setTempTarget(Util::formatParams(tempDirectory, sm, false) + tempName);
			
			{
				static bool g_is_first_check = false;
				if (!g_is_first_check && !m_tempTarget.empty())
				{
				
					g_is_first_check = true;
					File::ensureDirectory(tempDirectory);
					const tstring l_temp_targetT = Text::toT(m_tempTarget);
#ifndef _DEBUG
					const auto l_marker_file = Util::getFilePath(l_temp_targetT) + _T(".flylinkdc-test-readonly-") + Util::toStringW(GET_TIME()) + _T(".tmp");
#else
					const auto l_marker_file = Util::getFilePath(l_temp_targetT) + _T(".flylinkdc-test-readonly.tmp");
#endif
					try
					{
						{
							File l_f_ro_test(l_marker_file, File::WRITE, File::CREATE | File::TRUNCATE);
						}
						File::deleteFileT(l_marker_file);
					}
					catch (const Exception&)
					{
						dcassert(0);
						//const DWORD l_error = GetLastError();
						//if (l_error == 5) TODO - позже включить
						{
							SET_SETTING(TEMP_DOWNLOAD_DIRECTORY, "");
							LogManager::message("Error create/write + " + Text::fromT(l_marker_file) + " Error =" + Util::translateError());
						}
					}
				}
			}
		}
		if (tempDirectory.empty())
		{
			setTempTarget(m_Target.substr(0, m_Target.length() - getTargetFileName().length()) + tempName);
		}
	}
	return m_tempTarget;
}

#ifdef _DEBUG
bool QueueItem::isSourceValid(const QueueItem::Source* p_source_ptr)
{
	RLock(*g_cs);
	for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
	{
		if (p_source_ptr == &i->second)
			return true;
	}
	return false;
}
#endif

void QueueItem::addDownload(const DownloadPtr& download)
{
	CFlyFastLock(m_fcs_download);
	dcassert(download->getUser());
	//dcassert(downloads.find(p_download->getUser()) == downloads.end());
	downloads.push_back(download);
}

bool QueueItem::removeDownload(const UserPtr& user)
{
	CFlyFastLock(m_fcs_download);
	const auto sizeBefore = downloads.size();
	if (sizeBefore)
	{
		//dcassert(sizeBefore != downloads.size());
		//downloads.erase(remove(downloads.begin(), downloads.end(), d), downloads.end());
		for (auto i = downloads.begin(); i != downloads.end(); ++i)
			if ((*i)->getUser() == user)
			{
				downloads.erase(i);
				break;
			}
	}
	return sizeBefore != downloads.size();
}

Segment QueueItem::getNextSegmentForward(const int64_t blockSize, const int64_t targetSize, vector<Segment>* neededParts, const vector<int64_t>& posArray) const
{
	int64_t start = 0;
	int64_t curSize = targetSize;
	if (!doneSegments.empty())
	{
		const Segment& segBegin = *doneSegments.begin();
		if (segBegin.getStart() == 0) start = segBegin.getEnd();
	}
	while (start < getSize())
	{
		int64_t end = std::min(getSize(), start + curSize);
		Segment block(start, end - start);
		bool overlaps = false;
		for (auto i = doneSegments.cbegin(); !overlaps && i != doneSegments.cend(); ++i)
		{
			if (curSize <= blockSize)
			{
				int64_t dstart = i->getStart();
				int64_t dend = i->getEnd();
				// We accept partial overlaps, only consider the block done if it is fully consumed by the done block
				if (dstart <= start && dend >= end)
					overlaps = true;
			}
			else
				overlaps = block.overlaps(*i);
		}
		if (!overlaps)
		{
			for (auto i = downloads.cbegin(); !overlaps && i != downloads.cend(); ++i)
				overlaps = block.overlaps((*i)->getSegment());
		}
				
		if (!overlaps)
		{
			if (!neededParts) return block;
			// store all chunks we could need
			for (vector<int64_t>::size_type j = 0; j < posArray.size(); j += 2)
			{
				if ((posArray[j] <= start && start < posArray[j + 1]) || (start <= posArray[j] && posArray[j] < end))
				{
					int64_t b = max(start, posArray[j]);
					int64_t e = min(end, posArray[j + 1]);

					// segment must be blockSize aligned
					dcassert(b % blockSize == 0);
					dcassert(e % blockSize == 0 || e == getSize());

					neededParts->push_back(Segment(b, e - b));
				}
			}
		}

		if (overlaps && curSize > blockSize)
		{
			curSize -= blockSize;
		}
		else
		{
			start = end;
			curSize = targetSize;
		}
	}
	return Segment(0, 0);
}

Segment QueueItem::getNextSegmentBackward(const int64_t blockSize, const int64_t targetSize, vector<Segment>* neededParts, const vector<int64_t>& posArray) const
{
	int64_t end = 0;
	int64_t curSize = targetSize;
	if (!doneSegments.empty())
	{
		const Segment& segEnd = *doneSegments.crbegin();
		if (segEnd.getEnd() == getSize()) end = segEnd.getStart();
	}
	if (!end) end = Util::roundUp(getSize(), blockSize);
	while (end > 0)
	{
		int64_t start = std::max(0ll, end - curSize);
		Segment block(start, std::min(end, getSize()) - start);
		bool overlaps = false;
		for (auto i = doneSegments.crbegin(); !overlaps && i != doneSegments.crend(); ++i)
		{
			if (curSize <= blockSize)
			{
				int64_t dstart = i->getStart();
				int64_t dend = i->getEnd();
				// We accept partial overlaps, only consider the block done if it is fully consumed by the done block
				if (dstart <= start && dend >= end)
					overlaps = true;
			}
			else
				overlaps = i->overlaps(block);
		}
		if (!overlaps)
		{
			for (auto i = downloads.crbegin(); !overlaps && i != downloads.crend(); ++i)
				overlaps = (*i)->getSegment().overlaps(block);
		}
				
		if (!overlaps)
		{
			if (!neededParts) return block;
			// store all chunks we could need
			for (vector<int64_t>::size_type j = 0; j < posArray.size(); j += 2)
			{
				if ((posArray[j] <= start && start < posArray[j + 1]) || (start <= posArray[j] && posArray[j] < end))
				{
					int64_t b = max(start, posArray[j]);
					int64_t e = min(end, posArray[j + 1]);

					// segment must be blockSize aligned
					dcassert(b % blockSize == 0);
					dcassert(e % blockSize == 0 || e == getSize());

					neededParts->push_back(Segment(b, e - b));
				}
			}
		}

		if (overlaps && curSize > blockSize)
		{
			curSize -= blockSize;
		}
		else
		{
			end = start;
			curSize = targetSize;
		}
	}
	return Segment(0, 0);
}

bool QueueItem::shouldSearchBackward() const
{
	if (!isSet(FLAG_WANT_END) || doneSegments.empty()) return false;
	const Segment& segBegin = *doneSegments.begin();
	if (segBegin.getStart() != 0 || segBegin.getSize() < 1024*1204) return false;
	const Segment& segEnd = *doneSegments.rbegin();
	if (segEnd.getEnd() == getSize())
	{
		int64_t requiredSize = getSize()*5/100; // 5% of file
		if (segEnd.getSize() > requiredSize) return false;
	}
	return true;
}

Segment QueueItem::getNextSegmentL(const int64_t blockSize, const int64_t wantedSize, const int64_t lastSpeed, const PartialSource::Ptr &partialSource) const
{
	if (getSize() == -1 || blockSize == 0)
	{
		return Segment(0, -1);
	}
	
	if (!BOOLSETTING(ENABLE_MULTI_CHUNK))
	{
		{
			CFlyFastLock(m_fcs_download);
			if (!downloads.empty())
				return Segment(-1, 0);
		}
		
		int64_t start = 0;
		int64_t end = getSize();
		
		if (!doneSegments.empty())
		{
			CFlyFastLock(m_fcs_segment);
			const Segment& first = *doneSegments.begin();
			
			if (first.getStart() > 0)
			{
				end = Util::roundUp(first.getStart(), blockSize);
			}
			else
			{
				start = Util::roundDown(first.getEnd(), blockSize);
				{
					CFlyFastLock(m_fcs_segment);
					if (doneSegments.size() > 1)
					{
						const Segment& second = *(++doneSegments.begin());
						end = Util::roundUp(second.getStart(), blockSize);
					}
				}
			}
		}
		
		return Segment(start, std::min(getSize(), end) - start);
	}
	{
		CFlyFastLock(m_fcs_download);
		if (downloads.size() >= getMaxSegments() ||
		    (BOOLSETTING(DONT_BEGIN_SEGMENT) && static_cast<int64_t>(SETTING(DONT_BEGIN_SEGMENT_SPEED) * 1024) < getAverageSpeed()))
		{
			// no other segments if we have reached the speed or segment limit
			return Segment(-1, 0);
		}
	}
	
	/* added for PFS */
	vector<int64_t> posArray;
	vector<Segment> neededParts;
	
	if (partialSource)
	{
		posArray.reserve(partialSource->getPartialInfo().size());
		
		// Convert block index to file position
		for (auto i = partialSource->getPartialInfo().cbegin(); i != partialSource->getPartialInfo().cend(); ++i)
		{
			posArray.push_back(min(getSize(), (int64_t)(*i) * blockSize));
		}
	}
	
	/***************************/
	
	double donePart;
	{
		CFlyFastLock(m_fcs_segment);
		donePart = static_cast<double>(doneSegmentsSize) / getSize();
	}
	
	// We want smaller blocks at the end of the transfer, squaring gives a nice curve...
	int64_t targetSize = static_cast<int64_t>(static_cast<double>(wantedSize) * std::max(0.25, (1. - (donePart * donePart))));
	
	if (targetSize > blockSize)
	{
		// Round off to nearest block size
		targetSize = Util::roundDown(targetSize, blockSize);
	}
	else
	{
		targetSize = blockSize;
	}
	
	{
		CFlyFastLock(m_fcs_download);
		{
			CFlyFastLock(m_fcs_segment);
			Segment block = shouldSearchBackward()?
				getNextSegmentBackward(blockSize, targetSize, partialSource? &neededParts : nullptr, posArray) :
				getNextSegmentForward(blockSize, targetSize, partialSource? &neededParts : nullptr, posArray);
			if (block.getSize()) return block;
		}
	} // end lock
	
	if (!neededParts.empty())
	{
		// select random chunk for download
		dcdebug("Found partial chunks: %d\n", int(neededParts.size()));
		
		Segment& selected = neededParts[Util::rand(0, static_cast<uint32_t>(neededParts.size()))];
		selected.setSize(std::min(selected.getSize(), targetSize)); // request only wanted size
		
		return selected;
	}
	
	if (partialSource == NULL && BOOLSETTING(OVERLAP_CHUNKS) && lastSpeed > 10 * 1024)
	{
		// overlap slow running chunk
		
		const uint64_t l_CurrentTick = GET_TICK();//[+]IRainman refactoring transfer mechanism
		CFlyFastLock(m_fcs_download);
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		{
			const auto d = *i;
			
			// current chunk mustn't be already overlapped
			if (d->getOverlapped())
				continue;
				
			// current chunk must be running at least for 2 seconds
			if (d->getStartTime() == 0 || l_CurrentTick - d->getStartTime() < 2000)
				continue;
				
			// current chunk mustn't be finished in next 10 seconds
			if (d->getSecondsLeft() < 10)
				continue;
				
			// overlap current chunk at last block boundary
			int64_t l_pos = d->getPos() - (d->getPos() % blockSize);
			int64_t l_size = d->getSize() - l_pos;
			
			// new user should finish this chunk more than 2x faster
			int64_t newChunkLeft = l_size / lastSpeed;
			if (2 * newChunkLeft < d->getSecondsLeft())
			{
				dcdebug("Overlapping... old user: %I64d s, new user: %I64d s\n", d->getSecondsLeft(), newChunkLeft);
				return Segment(d->getStartPos() + l_pos, l_size, true);
			}
		}
	}
	
	return Segment(0, 0);
}

void QueueItem::setOverlapped(const Segment& segment, const bool isOverlapped)
{
	// set overlapped flag to original segment
	CFlyFastLock(m_fcs_download);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		auto d = *i;
		if (d->getSegment().contains(segment))
		{
			d->setOverlapped(isOverlapped);
			break;
		}
	}
}

// FIXME: remove
string QueueItem::getSectionString() const
{
	string l_strSections;
	{
		CFlyFastLock(m_fcs_segment);
		l_strSections.reserve(doneSegments.size() * 10);
		for (auto i = doneSegments.cbegin(); i != doneSegments.cend(); ++i)
		{
			char buf[48];
			buf[0] = 0;
			_snprintf(buf, _countof(buf), "%I64d %I64d ", i->getStart(), i->getSize());
			l_strSections += buf;
		}
	}
	if (!l_strSections.empty())
	{
		l_strSections.resize(l_strSections.size() - 1);
	}
	return l_strSections;
}

void QueueItem::updateDownloadedBytesAndSpeedL()
{
	int64_t totalSpeed = 0;
	{
		CFlyFastLock(m_fcs_segment);
		downloadedBytes = doneSegmentsSize;
	}
	// count running segments
	CFlyFastLock(m_fcs_download);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		const auto d = *i;
		downloadedBytes += d->getPos();
		totalSpeed += d->getRunningAverage();
	}
	averageSpeed = totalSpeed;
}

void QueueItem::addSegment(const Segment& segment, bool isFirstLoad /*= false */)
{
	CFlyFastLock(m_fcs_segment);
	addSegmentL(segment, isFirstLoad);
}

void QueueItem::addSegmentL(const Segment& segment, bool isFirstLoad)
{
	dcassert(!segment.getOverlapped());
	doneSegments.insert(segment);
	doneSegmentsSize += segment.getSize();
	// Consolidate segments
	if (doneSegments.size() == 1)
		return;
	if (!isFirstLoad)
		setDirtySegment(true);
	for (auto current = ++doneSegments.cbegin(); current != doneSegments.cend();)
	{
		SegmentSet::iterator prev = current;
		--prev;
		const Segment& segCurrent = *current;
		const Segment& segPrev = *prev;
		if (segPrev.getEnd() >= segCurrent.getStart())
		{
			const Segment big(segPrev.getStart(), segCurrent.getEnd() - segPrev.getStart());
			int64_t removedSize = segPrev.getSize() + segCurrent.getSize();
			doneSegments.erase(prev);
			doneSegments.erase(current++);
			doneSegments.insert(big);
			doneSegmentsSize -= removedSize;
			doneSegmentsSize += big.getSize();
		} else ++current;
	}
}

bool QueueItem::isNeededPart(const PartsInfo& partsInfo, int64_t blockSize) const
{
	dcassert(partsInfo.size() % 2 == 0);
	CFlyFastLock(m_fcs_segment);
	auto i = doneSegments.begin();
	for (auto j = partsInfo.cbegin(); j != partsInfo.cend(); j += 2)
	{
		while (i != doneSegments.end() && (*i).getEnd() <= (*j) * blockSize)
			++i;
			
		if (i == doneSegments.end() || !((*i).getStart() <= (*j) * blockSize && (*i).getEnd() >= (*(j + 1)) * blockSize))
			return true;
	}
	
	return false;
}

void QueueItem::getPartialInfo(PartsInfo& partialInfo, uint64_t blockSize) const
{
	dcassert(blockSize);
	if (blockSize == 0) // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=31115
		return;
		
	CFlyFastLock(m_fcs_segment);
	const size_t maxSize = min(doneSegments.size() * 2, (size_t) 510);
	partialInfo.reserve(maxSize);
	
	for (auto i = doneSegments.cbegin(); i != doneSegments.cend() && partialInfo.size() < maxSize; ++i)
	{	
		uint16_t s = (uint16_t)(i->getStart() / blockSize);
		uint16_t e = (uint16_t)((i->getEnd() - 1) / blockSize + 1);		
		partialInfo.push_back(s);
		partialInfo.push_back(e);
	}
}

void QueueItem::getDoneSegments(vector<Segment>& done) const
{
	done.clear();
	CFlyFastLock(m_fcs_segment);
	done.reserve(doneSegments.size());
	for (auto i = doneSegments.cbegin(); i != doneSegments.cend(); ++i)
		done.push_back(*i);
}

void QueueItem::getChunksVisualisation(vector<RunningSegment>& running, vector<Segment>& done) const
{
	running.clear();
	done.clear();
	{
		CFlyFastLock(m_fcs_download);
		running.reserve(downloads.size());
		RunningSegment rs;
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		{
			const Segment &segment = (*i)->getSegment();
			rs.start = segment.getStart();
			rs.end = segment.getEnd();
			rs.pos = (*i)->getPos();
			running.push_back(rs);
		}
	}
	{
		CFlyFastLock(m_fcs_segment);
		done.reserve(doneSegments.size());
		for (auto i = doneSegments.cbegin(); i != doneSegments.cend(); ++i)
			done.push_back(*i);
	}
}

uint8_t QueueItem::calcActiveSegments()
{
	uint8_t activeSegments = 0;
	CFlyFastLock(m_fcs_download);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		if ((*i)->getStartTime() > 0)
		{
			activeSegments++;
		}
		// more segments won't change anything
		if (activeSegments > 1)
			break;
	}
	return activeSegments;
}

void QueueItem::getAllDownloadsUsers(UserList& users)
{
	users.clear();
	CFlyFastLock(m_fcs_download);
	users.reserve(downloads.size());
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		users.push_back((*i)->getUser());
}

UserPtr QueueItem::getFirstUser()
{
	CFlyFastLock(m_fcs_download);
	if (!downloads.empty())
		return (*downloads.begin())->getUser();
	else
		return UserPtr();
}

bool QueueItem::isDownloadTree()
{
	CFlyFastLock(m_fcs_download);
	if (!downloads.empty())
		return (*downloads.begin())->getType() == Transfer::TYPE_TREE;
	else
		return false;
}

void QueueItem::getAllDownloadUser(UserList& users)
{
	CFlyFastLock(m_fcs_download);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		users.push_back((*i)->getUser());
	}
}

void QueueItem::disconectedAllPosible(const DownloadPtr& aDownload)
{
	CFlyFastLock(m_fcs_download);
	// Disconnect all possible overlapped downloads
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		if ((*i) != aDownload)
		{
			(*i)->getUserConnection()->disconnect();
		}
}

bool QueueItem::disconectedSlow(const DownloadPtr& aDownload)
{
	bool found = false;
	// ok, we got a fast slot, so it's possible to disconnect original user now
	CFlyFastLock(m_fcs_download);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		const auto& j = *i;
		if (j != aDownload && j->getSegment().contains(aDownload->getSegment()))
		{
			// overlapping has no sense if segment is going to finish
			if (j->getSecondsLeft() < 10)
				break;
				
			found = true;
			
			// disconnect slow chunk
			j->getUserConnection()->disconnect();
			break;
		}
	}
	return found;
}

void QueueItem::updateBlockSize(uint64_t treeBlockSize)
{
	if (treeBlockSize > blockSize)
	{
		dcassert(!(treeBlockSize % blockSize));
		blockSize = treeBlockSize;		
	}
}
