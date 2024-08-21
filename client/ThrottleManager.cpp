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
#include "SettingsManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "Util.h"
#include "ConfCore.h"

static const int64_t MIN_LIMIT = 1;

ThrottleManager::ThrottleManager() : downLimit(0), upLimit(0), enabled(false)
{
}

ThrottleManager::~ThrottleManager()
{
	TimerManager::getInstance()->removeListener(this);
}

// TimerManagerListener
void ThrottleManager::on(TimerManagerListener::Minute, uint64_t /*tick*/) noexcept
{
	updateSettings();
}

static bool isAltLimiterTime(const BaseSettingsImpl* ss) noexcept
{
	if (!ss->getBool(Conf::TIME_DEPENDENT_THROTTLE))
		return false;

	int currentHour = Util::getCurrentHour();
	int s = ss->getInt(Conf::BANDWIDTH_LIMIT_START);
	int e = ss->getInt(Conf::BANDWIDTH_LIMIT_END);
	return ((s < e &&
	         currentHour >= s && currentHour < e) ||
	        (s > e &&
	         (currentHour >= s || currentHour < e)));
}

void ThrottleManager::updateSettings() noexcept
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	if (!ss->getBool(Conf::THROTTLE_ENABLE))
	{
		ss->unlockRead();
		downLimit = 0;
		upLimit = 0;
		enabled = false;
		return;
	}

	size_t optUpload, optDownload;
	if (isAltLimiterTime(ss))
	{
		optUpload = ss->getInt(Conf::MAX_UPLOAD_SPEED_LIMIT_TIME);
		optDownload = ss->getInt(Conf::MAX_DOWNLOAD_SPEED_LIMIT_TIME);
	}
	else
	{
		optUpload = ss->getInt(Conf::MAX_UPLOAD_SPEED_LIMIT_NORMAL);
		optDownload = ss->getInt(Conf::MAX_DOWNLOAD_SPEED_LIMIT_NORMAL);
	}
	ss->unlockRead();
	setUploadLimit(optUpload);
	setDownloadLimit(optDownload);
	enabled = optUpload != 0 || optDownload != 0;
}

int64_t ThrottleManager::getSocketUploadLimit() noexcept
{
	if (!upLimit) return 0;
	size_t n = UploadManager::getInstance()->getUploadCount();
	if (!n) return 0;
	int64_t result = upLimit / n;
	return result < MIN_LIMIT ? MIN_LIMIT : result;
}

int64_t ThrottleManager::getSocketDownloadLimit() noexcept
{
	if (!downLimit) return 0;
	size_t n = DownloadManager::getInstance()->getDownloadCount();
	if (!n) return 0;
	int64_t result = downLimit / n;
	return result < MIN_LIMIT ? MIN_LIMIT : result;
}
