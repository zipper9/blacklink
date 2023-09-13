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
#include "AppPaths.h"
#include "Util.h"
#include "StrUtil.h"
#include "DatabaseManager.h"
#include "IpList.h"
#include "Ip4Address.h"
#include "LogManager.h"

void Util::loadIBlockList()
{
	static const string blockListFile("iblocklist-com.ini");
	const string fileName = getConfigPath() + blockListFile;
	
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	const uint64_t timeStampFile = File::getTimeStamp(fileName);
	const uint64_t timeStampDb = conn->getRegistryVarInt(e_TimeStampIBlockListCom);
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
	conn->saveP2PGuardData(parsedData, DatabaseManager::PG_DATA_IBLOCKLIST_COM, true);
	conn->setRegistryVarInt(e_TimeStampIBlockListCom, timeStampFile);
}
	
void Util::loadP2PGuard()
{
	static const string p2pGuardFile("P2PGuard.ini");
	const string fileName = getConfigPath() + p2pGuardFile;
	
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	const uint64_t timeStampFile = File::getTimeStamp(fileName);
	const uint64_t timeStampDb = conn->getRegistryVarInt(e_TimeStampP2PGuard);
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
	conn->saveP2PGuardData(parsedData, DatabaseManager::PG_DATA_P2P_GUARD_INI, true);
	conn->setRegistryVarInt(e_TimeStampP2PGuard, timeStampFile);
}
	
void Util::loadCustomLocations()
{
	static const string customLocationsFile("CustomLocations.ini");
	const string fileName = getConfigPath() + customLocationsFile;

	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	const uint64_t timeStampFile = File::getTimeStamp(fileName);
	const uint64_t timeStampDb = conn->getRegistryVarInt(e_TimeStampCustomLocation);
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
	conn->saveLocation(parsedData);
	conn->setRegistryVarInt(e_TimeStampCustomLocation, timeStampFile);
}

void Util::getIpInfo(const IpAddress& ip, IPInfo& result, int what, bool onlyCached)
{
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	dm->getIPInfo(conn, ip, result, what, onlyCached);
	if (conn) dm->putConnection(conn);
}

const string& Util::getDescription(const IPInfo& ipInfo)
{
	if (!ipInfo.location.empty()) return ipInfo.location;
	return ipInfo.country;
}
