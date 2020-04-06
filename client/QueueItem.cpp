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

QueueItem::QueueItem(const string& target, int64_t size, Priority priority, bool autoPriority, Flags::MaskType flag,
                     time_t added, const TTHValue& tth, uint8_t maxSegments, const string& tempTarget) :
	target(target),
	tempTarget(tempTarget),
	maxSegments(std::max(uint8_t(1), maxSegments)),
	timeFileBegin(0),
	size(size),
	priority(priority),
	added(added),
	autoPriority(autoPriority),
	m_is_file_not_exist(false),
	tthRoot(tth),
	downloadedBytes(0),
	doneSegmentsSize(0),
	lastsize(0),
	averageSpeed(0),
	cachedOnlineSourceCountInvalid(false),
	cachedOnlineSourceCount(0),
	blockSize(size >= 0 ? TigerTree::getMaxBlockSize(size) : 64 * 1024),
	removed(false)
{
#ifdef _DEBUG
	//LogManager::message("QueueItem::QueueItem aTarget = " + aTarget + " this = " + Util::toString(__int64(this)));
#endif
#ifdef FLYLINKDC_USE_DROP_SLOW
	if (BOOLSETTING(ENABLE_AUTO_DISCONNECT))
	{
		flag |= FLAG_AUTODROP;
	}
#endif
	flags = flag;
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
	CFlyFastLock(csDownloads);
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

size_t QueueItem::getLastOnlineCount()
{
	if (cachedOnlineSourceCountInvalid)
	{
		RLock(*QueueItem::g_cs);
		cachedOnlineSourceCount = 0;
		for (auto i = sources.cbegin(); i != sources.cend(); ++i)
		{
			if (i->first->isOnline())
				++cachedOnlineSourceCount;
		}
		cachedOnlineSourceCountInvalid = false;
	}
	return cachedOnlineSourceCount;
}

bool QueueItem::isBadSourceExceptL(const UserPtr& user, Flags::MaskType exceptions) const
{
	const auto i = badSources.find(user);
	if (i != badSources.end())
	{
		return i->second.isAnySet((Flags::MaskType)(exceptions ^ Source::FLAG_MASK));
	}
	return false;
}

bool QueueItem::countOnlineUsersGreatOrEqualThanL(const size_t maxValue) const // [+] FlylinkDC++ opt.
{
	if (sources.size() < maxValue)
	{
		return false;
	}
	size_t count = 0;
	for (auto i = sources.cbegin(); i != sources.cend(); ++i)
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
		for (auto i = sources.cbegin(); i != sources.cend(); ++i)
		{
			if (i->first->isOnline())
			{
				list.push_back(i->first);
			}
		}
	}
}

void QueueItem::addSourceL(const UserPtr& user, bool isFirstLoad)
{
	if (isFirstLoad)
	{
		sources.insert(std::make_pair(user, Source()));
	}
	else
	{
		dcassert(!isSourceL(user));
		SourceIter i = findBadSourceL(user);
		if (i != badSources.end())
		{
			sources.insert(*i);
			badSources.erase(i->first);
		}
		else
		{
			sources.insert(std::make_pair(user, Source()));  // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=139307
		}
	}
	cachedOnlineSourceCountInvalid = true;
}

void QueueItem::getPFSSourcesL(const QueueItemPtr& qi, SourceListBuffer& sourceList, uint64_t now)
{
	auto addToList = [&](const bool isBadSources) -> void
	{
		const auto& sources = isBadSources ? qi->getBadSourcesL() : qi->getSourcesL();
		for (auto j = sources.cbegin(); j != sources.cend(); ++j)
		{
			const auto &ps = j->second.getPartialSource();
			if (j->second.isCandidate(isBadSources) && ps->isCandidate(now))
			{
				sourceList.insert(make_pair(ps->getNextQueryTime(), make_pair(j, qi)));
			}
		}
	};
	
	addToList(false);
	addToList(true);
}

void QueueItem::resetDownloaded()
{
	CFlyFastLock(csSegments);
	resetDownloadedL();
}

void QueueItem::resetDownloadedL()
{
	doneSegments.clear();
	doneSegmentsSize = 0;
}

bool QueueItem::isFinished() const
{
	if (doneSegments.size() == 1)
	{
		CFlyFastLock(csSegments);
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
	CFlyFastLock(csSegments);
	for (auto i = doneSegments.cbegin(); i != doneSegments.cend(); ++i)
	{
		const int64_t start = i->getStart();
		const int64_t end   = i->getEnd();
		if (start <= startPos && startPos < end)
		{
			len = min(len, end - startPos);
			return true;
		}
	}
	return false;
}

void QueueItem::removeSourceL(const UserPtr& user, Flags::MaskType reason)
{
	SourceIter i = findSourceL(user); // crash - https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=42877 && http://www.flickr.com/photos/96019675@N02/10488126423/
	if (i != sources.end()) // https://drdump.com/Problem.aspx?ProblemID=129066
	{
		i->second.setFlag(reason);
		badSources.insert(*i);
		sources.erase(i);
		cachedOnlineSourceCountInvalid = true;
	}
#ifdef _DEBUG
	else
	{
		string error = "Error QueueItem::removeSourceL, user = [" + user->getLastNick() + "]";
		LogManager::message(error);
		dcassert(0);
	}
#endif
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
	if (!isSet(QueueItem::FLAG_USER_LIST) && tempTarget.empty())
	{
		const TTHValue& tth = getTTH();
		const string& tempDirectory = SETTING(TEMP_DOWNLOAD_DIRECTORY);
		const string targetFileName = Util::getFileName(target);
		const string tempName = getDCTempName(targetFileName, tempDirectory.empty()? nullptr : &tth);
		if (!tempDirectory.empty() && File::getSize(getTarget()) == -1)
		{
			::StringMap sm;
			if (target.length() >= 3 && target[1] == ':' && target[2] == '\\')
				sm["targetdrive"] = target.substr(0, 3);
			else
				sm["targetdrive"] = Util::getLocalPath().substr(0, 3);
				
			setTempTarget(Util::formatParams(tempDirectory, sm, false) + tempName);
			
			{
				static bool g_is_first_check = false;
				if (!g_is_first_check && !tempTarget.empty())
				{
				
					g_is_first_check = true;
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
						SET_SETTING(TEMP_DOWNLOAD_DIRECTORY, "");
						LogManager::message("Error create/write + " + Text::fromT(tempFile) + " Error =" + Util::translateError());
					}
				}
			}
		}
		if (tempDirectory.empty())
		{
			setTempTarget(target.substr(0, target.length() - targetFileName.length()) + tempName);
		}
	}
	return tempTarget;
}

#ifdef _DEBUG
bool QueueItem::isSourceValid(const QueueItem::Source* sourcePtr)
{
	RLock(*g_cs);
	for (auto i = sources.cbegin(); i != sources.cend(); ++i)
	{
		if (sourcePtr == &i->second)
			return true;
	}
	return false;
}
#endif

void QueueItem::addDownload(const DownloadPtr& download)
{
	CFlyFastLock(csDownloads);
	dcassert(download->getUser());
	//dcassert(downloads.find(p_download->getUser()) == downloads.end());
	downloads.push_back(download);
}

bool QueueItem::removeDownload(const UserPtr& user)
{
	CFlyFastLock(csDownloads);
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
			CFlyFastLock(csDownloads);
			if (!downloads.empty())
				return Segment(-1, 0);
		}
		
		int64_t start = 0;
		int64_t end = getSize();
		
		if (!doneSegments.empty())
		{
			CFlyFastLock(csSegments);
			const Segment& first = *doneSegments.begin();
			
			if (first.getStart() > 0)
			{
				end = Util::roundUp(first.getStart(), blockSize);
			}
			else
			{
				start = Util::roundDown(first.getEnd(), blockSize);
				{
					CFlyFastLock(csSegments);
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
		CFlyFastLock(csDownloads);
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
	
	if (partialSource && partialSource->getBlockSize() == blockSize)
	{
		posArray.reserve(partialSource->getPartialInfo().size());
		
		// Convert block index to file position
		for (auto i = partialSource->getPartialInfo().cbegin(); i != partialSource->getPartialInfo().cend(); ++i)
		{
			int64_t pos = (int64_t) *i * blockSize;
			posArray.push_back(min(getSize(), pos));
		}
	}
	
	double donePart;
	{
		CFlyFastLock(csSegments);
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
		CFlyFastLock(csDownloads);
		{
			CFlyFastLock(csSegments);
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
		
		const uint64_t currentTick = GET_TICK();
		CFlyFastLock(csDownloads);
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		{
			const auto d = *i;
			
			// current chunk mustn't be already overlapped
			if (d->getOverlapped())
				continue;
				
			// current chunk must be running at least for 2 seconds
			if (d->getStartTime() == 0 || currentTick - d->getStartTime() < 2000)
				continue;
				
			// current chunk mustn't be finished in next 10 seconds
			if (d->getSecondsLeft() < 10)
				continue;
				
			// overlap current chunk at last block boundary
			int64_t pos = d->getPos() - (d->getPos() % blockSize);
			int64_t size = d->getSize() - pos;
			
			// new user should finish this chunk more than 2x faster
			int64_t newChunkLeft = size / lastSpeed;
			if (2 * newChunkLeft < d->getSecondsLeft())
			{
				dcdebug("Overlapping... old user: %I64d s, new user: %I64d s\n", d->getSecondsLeft(), newChunkLeft);
				return Segment(d->getStartPos() + pos, size, true);
			}
		}
	}

	return Segment(0, 0);
}

void QueueItem::setOverlapped(const Segment& segment, const bool isOverlapped)
{
	// set overlapped flag to original segment
	CFlyFastLock(csDownloads);
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

void QueueItem::updateDownloadedBytesAndSpeedL()
{
	int64_t totalSpeed = 0;
	{
		CFlyFastLock(csSegments);
		downloadedBytes = doneSegmentsSize;
	}
	// count running segments
	CFlyFastLock(csDownloads);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		const auto d = *i;
		downloadedBytes += d->getPos();
		totalSpeed += d->getRunningAverage();
	}
	averageSpeed = totalSpeed;
}

void QueueItem::updateDownloadedBytes()
{
	CFlyFastLock(csSegments);
	downloadedBytes = doneSegmentsSize;
}

void QueueItem::addSegment(const Segment& segment)
{
	CFlyFastLock(csSegments);
	addSegmentL(segment);
}

void QueueItem::addSegmentL(const Segment& segment)
{
	dcassert(!segment.getOverlapped());
	doneSegments.insert(segment);
	doneSegmentsSize += segment.getSize();
	// Consolidate segments
	if (doneSegments.size() == 1)
		return;
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
	CFlyFastLock(csSegments);
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
		
	CFlyFastLock(csSegments);
	const size_t maxSize = min(doneSegments.size() * 2, (size_t) 510);
	partialInfo.reserve(maxSize);
	
	for (auto i = doneSegments.cbegin(); i != doneSegments.cend() && partialInfo.size() < maxSize; ++i)
	{
		uint16_t s = (uint16_t)((i->getStart() + blockSize - 1) / blockSize); // round up
		int64_t end = i->getEnd();
		if (end >= getSize()) end += blockSize - 1;
		uint16_t e = (uint16_t)(end / blockSize); // round down for all chunks but last
		partialInfo.push_back(s);
		partialInfo.push_back(e);
	}
}

void QueueItem::getDoneSegments(vector<Segment>& done) const
{
	done.clear();
	CFlyFastLock(csSegments);
	done.reserve(doneSegments.size());
	for (auto i = doneSegments.cbegin(); i != doneSegments.cend(); ++i)
		done.push_back(*i);
}

void QueueItem::getChunksVisualisation(vector<RunningSegment>& running, vector<Segment>& done) const
{
	running.clear();
	done.clear();
	{
		CFlyFastLock(csDownloads);
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
		CFlyFastLock(csSegments);
		done.reserve(doneSegments.size());
		for (auto i = doneSegments.cbegin(); i != doneSegments.cend(); ++i)
			done.push_back(*i);
	}
}

uint8_t QueueItem::calcActiveSegments() const
{
	uint8_t activeSegments = 0;
	CFlyFastLock(csDownloads);
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

UserPtr QueueItem::getFirstUser() const
{
	CFlyFastLock(csDownloads);
	if (!downloads.empty())
		return downloads.front()->getUser();
	else
		return UserPtr();
}

bool QueueItem::isDownloadTree() const
{
	CFlyFastLock(csDownloads);
	if (!downloads.empty())
		return downloads.front()->getType() == Transfer::TYPE_TREE;
	else
		return false;
}

void QueueItem::getUsers(UserList& users) const
{
	CFlyFastLock(csDownloads);
	users.reserve(downloads.size());
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		users.push_back((*i)->getUser());
}

void QueueItem::disconnectOthers(const DownloadPtr& download)
{
	CFlyFastLock(csDownloads);
	// Disconnect all possible overlapped downloads
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		if ((*i) != download)
			(*i)->getUserConnection()->disconnect();
}

bool QueueItem::disconnectSlow(const DownloadPtr& download)
{
	bool found = false;
	// ok, we got a fast slot, so it's possible to disconnect original user now
	CFlyFastLock(csDownloads);
	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
	{
		const auto& j = *i;
		if (j != download && j->getSegment().contains(download->getSegment()))
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
