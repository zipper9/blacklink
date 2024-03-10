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

#ifndef _THROTTLEMANAGER_H
#define _THROTTLEMANAGER_H

#include "TimerManager.h"

class ThrottleManager : public Singleton<ThrottleManager>, private TimerManagerListener
{
	public:
		size_t getDownloadLimitInKBytes() const { return downLimit >> 10; }
		size_t getDownloadLimitInBytes() const { return downLimit; }
		void setDownloadLimit(size_t limitKb) { downLimit = limitKb << 10; }

		size_t getUploadLimitInKBytes() const { return upLimit >> 10; }
		size_t getUploadLimitInBytes() const { return upLimit; }
		void setUploadLimit(size_t limitKb) { upLimit = limitKb << 10; }

		void updateLimits() noexcept;

		void startup() noexcept
		{
			TimerManager::getInstance()->addListener(this);
			updateLimits();
		}

		int64_t getSocketUploadLimit() noexcept;
		int64_t getSocketDownloadLimit() noexcept;

	private:
		friend class Singleton<ThrottleManager>;

		size_t downLimit;
		size_t upLimit;

		ThrottleManager();
		~ThrottleManager();

		// TimerManagerListener
		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept override;
};

#endif  // _THROTTLEMANAGER_H
