/*
 * Copyright (C) 2009-2013 Big Muscle
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
#include "ThrottleManager.h"

#include "DownloadManager.h"

#include "UploadManager.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#define CONDWAIT_TIMEOUT        250

ThrottleManager::ThrottleManager() : downTokens(0), upTokens(0), downLimit(0), upLimit(0)
{
}

ThrottleManager::~ThrottleManager()
{
	TimerManager::getInstance()->removeListener(this);
	
	// release conditional variables on exit
	downCond.notify_all();
	upCond.notify_all();
}

/*
 * Limits a traffic and reads a packet from the network
 */
int ThrottleManager::read(Socket* sock, void* buffer, size_t len)
{
	const size_t downs = downLimit ? DownloadManager::getDownloadCount() : 0;
	if (downLimit == 0 || downs == 0)
		return sock->read(buffer, len);
		
	boost::unique_lock<boost::mutex> lock(downMutex);
	
	if (downTokens > 0)
	{
		const size_t slice = getDownloadLimitInBytes() / downs;
		size_t readSize = min(slice, min(len, downTokens));
		
		// read from socket
		readSize = sock->read(buffer, readSize);
		
		if (readSize > 0)
			downTokens -= readSize;
			
		// next code can't be in critical section, so we must unlock here
		lock.unlock();
		
		// give a chance to other transfers to get a token
		Thread::yield();
		return readSize;
	}
	
	// no tokens, wait for them
	downCond.timed_wait(lock, boost::posix_time::millisec(CONDWAIT_TIMEOUT));
	return -1;  // from BufferedSocket: -1 = retry, 0 = connection close
}

/*
 * Limits a traffic and writes a packet to the network
 * We must handle this a little bit differently than downloads, because of that stupidity in OpenSSL
 */
int ThrottleManager::write(Socket* sock, const void* buffer, size_t& len)
{
	const auto currentMaxSpeed = sock->getMaxSpeed();
	if (currentMaxSpeed < 0) // SU
	{
		// write to socket
		const int sent = sock->write(buffer, len);
		return sent;
	}
	else if (currentMaxSpeed > 0) // individual
	{
		// Apply individual restriction to the user if it is
		const int64_t currentBucket = sock->getCurrentBucket();
		len = min(len, static_cast<size_t>(currentBucket));
		sock->setCurrentBucket(currentBucket - len);
		
		if (!len)
		{
			uint64_t delay = GET_TICK() - sock->getBucketUpdateTick();
			if (delay > CONDWAIT_TIMEOUT) delay = CONDWAIT_TIMEOUT;
			Thread::sleep(static_cast<unsigned>(delay));
			return 0;
		}

		// write to socket
		const int sent = sock->write(buffer, len);
		return sent;
	}
	else // general
	{
		if (!upLimit)
			return sock->write(buffer, len);
		const size_t ups = UploadManager::getInstance()->getUploadCount();
		if (!ups)
			return sock->write(buffer, len);
		
		boost::unique_lock<boost::mutex> lock(upMutex);
		if (upTokens > 0)
		{
			const size_t slice = getUploadLimitInBytes() / ups;
			len = min(slice, min(len, upTokens));
			
			// Pour buckets of the calculated number of bytes,
			// but as a real restriction on the specified number of bytes
			upTokens -= len;
			
			// next code can't be in critical section, so we must unlock here
			lock.unlock();
			
			// write to socket
			const int sent = sock->write(buffer, len);
			
			// give a chance to other transfers to get a token
			Thread::yield();
			return sent;
		}
		
		// no tokens, wait for them
		upCond.timed_wait(lock, boost::posix_time::millisec(CONDWAIT_TIMEOUT));
		return 0;   // from BufferedSocket: -1 = failed, 0 = retry
	}
}

// TimerManagerListener
void ThrottleManager::on(TimerManagerListener::Second, uint64_t /*aTick*/) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	if (!BOOLSETTING(THROTTLE_ENABLE))
	{
		downLimit = 0;
		upLimit = 0;
		return;
	}
	// readd tokens
	if (downLimit > 0)
	{
		boost::lock_guard<boost::mutex> lock(downMutex);
		downTokens = downLimit;
		downCond.notify_all();
	}
	
	if (upLimit > 0)
	{
		boost::lock_guard<boost::mutex> lock(upMutex);
		upTokens = upLimit;
		upCond.notify_all();
	}
}

void ThrottleManager::on(TimerManagerListener::Minute, uint64_t /*aTick*/) noexcept
{
	if (!BOOLSETTING(THROTTLE_ENABLE))
		return;
		
	updateLimits();
}

static bool isAltLimiterTime()
{
	if (!SETTING(TIME_DEPENDENT_THROTTLE))
		return false;

	int currentHour = Util::getCurrentHour();
	int s = SETTING(BANDWIDTH_LIMIT_START);
	int e = SETTING(BANDWIDTH_LIMIT_END);
	return ((s < e &&
	         currentHour >= s && currentHour < e) ||
	        (s > e &&
	         (currentHour >= s || currentHour < e)));
}

void ThrottleManager::updateLimits()
{
	if (!BOOLSETTING(THROTTLE_ENABLE))
	{
		downLimit = 0;
		upLimit = 0;
		return;
	}

	if (isAltLimiterTime())
	{
		setUploadLimit(SETTING(MAX_UPLOAD_SPEED_LIMIT_TIME));
		setDownloadLimit(SETTING(MAX_DOWNLOAD_SPEED_LIMIT_TIME));
	}
	else
	{
		setUploadLimit(SETTING(MAX_UPLOAD_SPEED_LIMIT_NORMAL));
		setDownloadLimit(SETTING(MAX_DOWNLOAD_SPEED_LIMIT_NORMAL));
	}
}
