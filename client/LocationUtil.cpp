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
#include "LocationUtil.h"
#include "Util.h"
#include "StrUtil.h"
#include "CFlylinkDBManager.h"
#include "IpList.h"
#include "LogManager.h"

static const char* countryCodes[] = // TODO: update this table! http://en.wikipedia.org/wiki/ISO_3166-1
{
	"AD", "AE", "AF", "AG", "AI", "AL", "AM", "AN", "AO", "AQ", "AR", "AS", "AT", "AU", "AW", "AX", "AZ", "BA", "BB",
	"BD", "BE", "BF", "BG", "BH", "BI", "BJ", "BM", "BN", "BO", "BR", "BS", "BT", "BV", "BW", "BY", "BZ", "CA", "CC",
	"CD", "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN", "CO", "CR", "CS", "CU", "CV", "CX", "CY", "CZ", "DE", "DJ",
	"DK", "DM", "DO", "DZ", "EC", "EE", "EG", "EH", "ER", "ES", "ET", "EU", "FI", "FJ", "FK", "FM", "FO", "FR", "GA",
	"GB", "GD", "GE", "GF", "GG", "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU", "GW", "GY", "HK",
	"HM", "HN", "HR", "HT", "HU", "ID", "IE", "IL", "IM", "IN", "IO", "IQ", "IR", "IS", "IT", "JE", "JM", "JO", "JP",
	"KE", "KG", "KH", "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI", "LK", "LR", "LS", "LT",
	"LU", "LV", "LY", "MA", "MC", "MD", "ME", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ", "MR", "MS", "MT",
	"MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI", "NL", "NO", "NP", "NR", "NU", "NZ", "OM",
	"PA", "PE", "PF", "PG", "PH", "PK", "PL", "PM", "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RS", "RU",
	"RW", "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI", "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "ST", "SV", "SY",
	"SZ", "TC", "TD", "TF", "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA", "UG",
	"UM", "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI", "VN", "VU", "WF", "WS", "YE", "YT", "YU", "ZA", "ZM", "ZW"
};

const char* Util::getCountryShortName(unsigned flagIndex)
{
	if (flagIndex < _countof(countryCodes))
		return countryCodes[flagIndex];
	else
		return "";
}

int Util::getFlagIndexByCode(const char* countryCode)
{
	// country codes are sorted, use binary search for better performance
	int begin = 0;
	int end = _countof(countryCodes) - 1;
	const int countryCodeInt = ((uint8_t) countryCode[0]) << 8 | (uint8_t) countryCode[1];
	
	while (begin <= end)
	{
		int mid = (begin + end) / 2;
		const char *val = countryCodes[mid];
		int valInt = ((uint8_t) val[0]) << 8 | (uint8_t) val[1];
		int cmp = countryCodeInt - valInt;
	
		if (cmp > 0)
			begin = mid + 1;
		else if (cmp < 0)
			end = mid - 1;
		else
			return mid + 1;
	}
	return 0;
}

void Util::loadIBlockList()
{
	static const string blockListFile("iblocklist-com.ini");
	const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                            true
#endif
		                        ) + blockListFile;
	
	const uint64_t timeStampFile = File::getTimeStamp(fileName);
	const uint64_t timeStampDb = CFlylinkDBManager::getInstance()->getRegistryVarInt(e_TimeStampIBlockListCom);
	if (timeStampFile == timeStampDb) return;
	vector<P2PGuardData> parsedData;
	auto addLine = [&parsedData](const string& s) -> bool
	{
		if (s.empty() || s[0] == '#') return true;
		int result;
		auto startPos = s.rfind(':');
		if (startPos != string::npos && startPos != 0)
		{		
			IpList::ParseLineResult out;
			result = IpList::parseLine(s, out, nullptr, startPos + 1);
			if (!result)
				parsedData.emplace_back(s.substr(0, startPos), out.start, out.end);
		} else result = IpList::ERR_BAD_FORMAT;
		if (result)
			LogManager::message("Error parsing " + blockListFile + ": " + IpList::getErrorText(result) + " [" + s + "]", false);
		return true;
	};
	try
	{
		File f(fileName, File::READ, File::OPEN);
		LogManager::message("Reading " + fileName, false);
		readTextFile(f, addLine);
	}
	catch (Exception& e)
	{
		LogManager::message("Could not load " + blockListFile + ": " + e.getError(), false);
		return;
	}
	CFlylinkDBManager::getInstance()->saveP2PGuardData(parsedData, "", 3);
	CFlylinkDBManager::getInstance()->setRegistryVarInt(e_TimeStampIBlockListCom, timeStampFile);
}
	
void Util::loadP2PGuard()
{
	static const string p2pGuardFile("P2PGuard.ini");
	const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                            true
#endif
		                        ) + p2pGuardFile;
	
	const uint64_t timeStampFile = File::getTimeStamp(fileName);
	const uint64_t timeStampDb = CFlylinkDBManager::getInstance()->getRegistryVarInt(e_TimeStampP2PGuard);
	if (timeStampFile == timeStampDb) return;
	vector<P2PGuardData> parsedData;
	auto addLine = [&parsedData](const string& s) -> bool
	{
		IpList::ParseLineResult out;
		int result = IpList::parseLine(s, out);
		if (result == IpList::ERR_LINE_SKIPPED) return true;
		if (!result)
		{			
			auto pos = out.pos;
			if (pos + 1 < s.length() && s[pos] == ' ')
				parsedData.emplace_back(s.substr(pos+1), out.start, out.end);
			else
				result = IpList::ERR_BAD_FORMAT;
		}
		if (result)
			LogManager::message("Error parsing " + p2pGuardFile + ": " + IpList::getErrorText(result) + " [" + s + "]", false);
		return true;
	};
	try
	{
		File f(fileName, File::READ, File::OPEN);
		LogManager::message("Reading " + fileName, false);
		readTextFile(f, addLine);
	}
	catch (Exception& e)
	{
		LogManager::message("Could not load " + p2pGuardFile + ": " + e.getError(), false);
		return;
	}
	CFlylinkDBManager::getInstance()->saveP2PGuardData(parsedData, "", 2);
	CFlylinkDBManager::getInstance()->setRegistryVarInt(e_TimeStampP2PGuard, timeStampFile);
}
	
#ifdef FLYLINKDC_USE_GEO_IP
void Util::loadGeoIp()
{
	// This product includes GeoIP data created by MaxMind, available from http://maxmind.com/
	// Updates at http://www.maxmind.com/app/geoip_country
	static const string geoIpFile("GeoIPCountryWhois.csv");
	const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                            true
#endif
		                        ) + geoIpFile;
	
	const uint64_t timeStampFile = File::getTimeStamp(fileName);
	const uint64_t timeStampDb = CFlylinkDBManager::getInstance()->getRegistryVarInt(e_TimeStampGeoIP);
	if (timeStampFile == timeStampDb) return;
	vector<LocationInfo> parsedData;
	int parseErrors = 0;
	auto addLine = [&parsedData, &parseErrors](const string& s) -> bool
	{
		bool result = false;
		string::size_type pos;
		do
		{
			pos = s.find(',');
			if (pos == string::npos) break;
			pos = s.find(',', pos + 1);
			if (pos == string::npos || pos + 2 >= s.length() || s[pos+1] != '"') break;
			auto startIp = toUInt32(s.c_str() + pos + 2);
			pos = s.find(',', pos + 1);
			if (pos == string::npos || pos + 2 >= s.length() || s[pos+1] != '"') break;
			auto endIp = toUInt32(s.c_str() + pos + 2);
			pos = s.find(',', pos + 1);
			if (pos == string::npos || pos + 3 >= s.length() || s[pos+1] != '"') break;
			auto flagIndex = getFlagIndexByCode(s.c_str() + pos + 2);
			pos = s.find(',', pos + 1);
			if (pos == string::npos || pos + 2 >= s.length() || s[pos+1] != '"') break;
			pos += 2;
			auto endPos = s.find('"', pos);
			if (endPos == string::npos || endPos == pos) break;
			parsedData.emplace_back(s.substr(pos, endPos - pos), startIp, endIp, flagIndex);
			result = true;
		} while (0);
		if (!result && parseErrors < 100)
		{
			++parseErrors;
			LogManager::message("Error parsing " + geoIpFile + " at pos " + Util::toString(pos) +  " [" + s + "]", false);
		}
		return true;
	};
	try
	{
		File f(fileName, File::READ, File::OPEN);
		LogManager::message("Reading " + fileName, false);
		readTextFile(f, addLine);
	}
	catch (Exception& e)
	{
		LogManager::message("Could not load " + geoIpFile + ": " + e.getError(), false);
		return;
	}
	CFlylinkDBManager::getInstance()->saveGeoIpCountries(parsedData);
	CFlylinkDBManager::getInstance()->setRegistryVarInt(e_TimeStampGeoIP, timeStampFile);
}
#endif
	
void Util::loadCustomLocations()
{
	static const string customLocationsFile("CustomLocations.ini");
	const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                            true
#endif
		                        ) + customLocationsFile;

	const uint64_t timeStampFile = File::getTimeStamp(fileName);
	const uint64_t timeStampDb = CFlylinkDBManager::getInstance()->getRegistryVarInt(e_TimeStampCustomLocation);
	if (timeStampFile == timeStampDb) return;
	vector<LocationInfo> parsedData;
	auto addLine = [&parsedData](const string& s) -> bool
	{
		IpList::ParseLineResult out;
		int result = IpList::parseLine(s, out);
		if (result == IpList::ERR_LINE_SKIPPED) return true;
		if (!result)
		{			
			auto pos = out.pos;
			if (pos + 1 < s.length() && s[pos] == ' ')
			{
				pos++;
				unsigned imageIndex = Util::toUInt32(s.c_str() + pos);
				pos = s.find(',', pos);
				if (pos != string::npos && pos + 1 < s.length())
					parsedData.emplace_back(s.substr(pos+1), out.start, out.end, imageIndex);
				else
					result = IpList::ERR_BAD_FORMAT;
			} else result = IpList::ERR_BAD_FORMAT;
		}
		if (result)
			LogManager::message("Error parsing " + customLocationsFile + ": " + IpList::getErrorText(result) + " [" + s + "]", false);
		return true;
	};
	try
	{
		File f(fileName, File::READ, File::OPEN);
		LogManager::message("Reading " + fileName, false);
		readTextFile(f, addLine);
	}
	catch (Exception& e)
	{
		LogManager::message("Could not load " + customLocationsFile + ": " + e.getError(), false);
		return;
	}
	CFlylinkDBManager::getInstance()->saveLocation(parsedData);
	CFlylinkDBManager::getInstance()->setRegistryVarInt(e_TimeStampCustomLocation, timeStampFile);
}

tstring Util::CustomNetworkIndex::getCountry() const
{
#ifdef FLYLINKDC_USE_GEO_IP
	if (countryCacheIndex > 0)
	{
		const LocationDesc res = CFlylinkDBManager::getInstance()->getCountryFromCache(countryCacheIndex);
		return Text::toT(Util::getCountryShortName(res.imageIndex - 1));
	}
#endif
	return Util::emptyStringT;
}

tstring Util::CustomNetworkIndex::getDescription() const
{
	if (locationCacheIndex > 0)
	{
		const LocationDesc res =  CFlylinkDBManager::getInstance()->getLocationFromCache(locationCacheIndex);
		return res.description;
	}
#ifdef FLYLINKDC_USE_GEO_IP
	if (countryCacheIndex > 0)
	{
		const LocationDesc res = CFlylinkDBManager::getInstance()->getCountryFromCache(countryCacheIndex);
		return res.description;
	}
#endif
	return Util::emptyStringT;
}

int Util::CustomNetworkIndex::getFlagIndex() const
{
	if (locationCacheIndex > 0)
		return CFlylinkDBManager::getInstance()->getLocationImageFromCache(locationCacheIndex);
	return 0;
}

#ifdef FLYLINKDC_USE_GEO_IP
int Util::CustomNetworkIndex::getCountryIndex() const
{
	if (countryCacheIndex > 0)
		return CFlylinkDBManager::getInstance()->getCountryImageFromCache(countryCacheIndex);
	return 0;
}
#endif

Util::CustomNetworkIndex Util::getIpCountry(uint32_t ip, bool onlyCached)
{
	if (ip && ip != INADDR_NONE)
	{
		int countryIndex = 0;
		int locationIndex = 0;
		CFlylinkDBManager::getInstance()->getCountryAndLocation(ip, countryIndex, locationIndex, onlyCached);
		if (locationIndex || countryIndex)
			return CustomNetworkIndex(locationIndex, countryIndex);
	}
	else
	{
		dcassert(0);
	}
	return CustomNetworkIndex(0, 0);
}

Util::CustomNetworkIndex Util::getIpCountry(const string& ip, bool onlyCached)
{
	boost::system::error_code ec;
	boost::asio::ip::address_v4 addr = boost::asio::ip::make_address_v4(ip, ec);
	if (ec) return CustomNetworkIndex(0, 0);
	return getIpCountry(addr.to_ulong(), onlyCached);
}
