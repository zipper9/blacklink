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

#ifndef LOCATION_UTIL_H_
#define LOCATION_UTIL_H_

#include "typedefs.h"
#include "IPInfo.h"
#include "IpAddress.h"

namespace Util
{
	void loadCustomLocations();
	void loadP2PGuardIni();
	void unloadP2PGuardIni();
	void loadIBlockList();

	void getIpInfo(const IpAddress& ip, IPInfo& result, int what, bool onlyCached = false);
	const string& getDescription(const IPInfo& ipInfo);
}

#endif // LOCATION_UTIL_H_
