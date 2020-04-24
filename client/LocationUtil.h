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

namespace Util
{
	const char* getCountryShortName(unsigned index);
	int getFlagIndexByCode(const char* countryCode);
	
	void loadCustomLocations();
	void loadGeoIp();
	void loadP2PGuard();
	void loadIBlockList();

	struct CustomNetworkIndex
	{
		public:
			CustomNetworkIndex() : locationCacheIndex(-1), countryCacheIndex(-1)
			{
			}
			CustomNetworkIndex(int locationCacheIndex, int countryCacheIndex) :
				locationCacheIndex(locationCacheIndex), countryCacheIndex(countryCacheIndex)
			{
			}
			bool isNew() const
			{
				return locationCacheIndex == -1 && countryCacheIndex == -1;
			}
			bool isKnown() const
			{
				return locationCacheIndex >= 0 || countryCacheIndex >= 0;
			}
			tstring getDescription() const;
			tstring getCountry() const;
			int getFlagIndex() const;
			int getCountryIndex() const;

		private:
			int countryCacheIndex;
			int locationCacheIndex;
	};
		
	CustomNetworkIndex getIpCountry(uint32_t ip, bool onlyCached = false);
	CustomNetworkIndex getIpCountry(const string& ip, bool onlyCached = false);
}

#endif // LOCATION_UTIL_H_
