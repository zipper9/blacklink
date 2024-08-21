/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef IPGUARD_H
#define IPGUARD_H

#include "IpList.h"
#include "RWLock.h"
#include <atomic>

class IpGuard
{
	public:
		IpGuard();
		bool isEnabled() const noexcept { return enabled; }
		bool isBlocked(uint32_t addr) const noexcept;
		void load() noexcept;
		void clear() noexcept;
		void updateSettings() noexcept;
		static std::string getFileName();
		
	private:
		std::atomic_bool enabled;
		IpList ipList;
		bool isWhiteList;
		mutable unique_ptr<RWLock> cs;
};

extern IpGuard ipGuard;

#endif // IPGUARD_H
