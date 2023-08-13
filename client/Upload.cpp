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
#include "Upload.h"
#include "Streams.h"
#include "UserConnection.h"

#ifdef DEBUG_SHUTDOWN
std::atomic<int> Upload::countCreated(0), Upload::countDeleted(0);
#endif

Upload::Upload(const UserConnectionPtr& conn, const TTHValue& tth, const string& path, InputStream* is):
	Transfer(conn, path, tth),
	readStream(is),
	tickForRemove(0),
	downloadedBytes(-1)
{
#ifdef DEBUG_SHUTDOWN
	++countCreated;
#endif
	runningAverage = conn->getLastUploadSpeed();
}

Upload::~Upload()
{
#ifdef DEBUG_SHUTDOWN
	++countDeleted;
#endif
	delete readStream;
}

void Upload::getParams(StringMap& params) const
{
	Transfer::getParams(params);
	params["source"] = getPath();
}

void Upload::updateSpeed(uint64_t currentTick)
{
	LOCK(csSpeed);
	setLastTick(currentTick);
	speed.addSample(actual, currentTick);
	int64_t avg = speed.getAverage(2000, 64 * 1024);
	if (avg >= 0)
	{
		runningAverage = avg;
		userConnection->setLastUploadSpeed(avg);
	}
	else
		runningAverage = userConnection->getLastUploadSpeed();
}

int64_t Upload::getAdjustedPos() const
{
	return (downloadedBytes == -1 ? getStartPos() : downloadedBytes) + pos;
}

int64_t Upload::getSecondsLeft() const
{
	int64_t avg = getRunningAverage();
	int64_t bytesLeft = getFileSize() - getAdjustedPos();
	if (bytesLeft > 0 && avg > 0)
		return bytesLeft / avg;
	return 0;
}
