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

#include "stdinc.h"
#include "PGLoader.h"
#include "IpGuard.h"
#include "Util.h"
#include "SettingsManager.h"
#include "LogManager.h"

PGLoader ipTrust;

PGLoader::PGLoader() : cs(webrtc::RWLockWrapper::CreateRWLock()), hasWhiteList(false)
{
}

string PGLoader::getFileName()
{
	return Util::getConfigPath() + "IPTrust.ini";
}

void PGLoader::load() noexcept
{
	IpList::ParseLineOptions options;
	options.specialChars[0] = '-';
	options.specialChars[1] = '+';
	options.specialCharCount = 2;

	CFlyWriteLock(*cs);
	hasWhiteList = false;
	auto addLine = [this, &options](const string& s) -> bool
	{
		IpList::ParseLineResult out;
		int result = IpList::parseLine(s, out, &options);
		if (!result)
		{
			uint64_t payload = 0;
			if (out.specialChar == '-')
				payload = 1;
			else
				hasWhiteList = true;
			if (!ipList.addRange(out.start, out.end, payload, result))
				LogManager::message("Error adding data from IPTrust.ini: " + IpList::getErrorText(result) + " [" + s + "]", false);
		}
		else
			LogManager::message("Error parsing IPTrust.ini: " + IpList::getErrorText(result) + " [" + s + "]", false);
		return true;
	};
	try
	{
		File f(getFileName(), File::READ, File::OPEN);
		Util::readTextFile(f, addLine);
	}
	catch (Exception& e)
	{
		LogManager::message("Could not load IPTrust.ini: " + e.getError(), false);
		ipList.clear();
	}
}

void PGLoader::clear() noexcept
{
	CFlyWriteLock(*cs);
	ipList.clear();
	hasWhiteList = false;
}

bool PGLoader::isBlocked(uint32_t addr) const noexcept
{
	if (!BOOLSETTING(ENABLE_IPTRUST))
		return false;

	CFlyReadLock(*cs);
	uint64_t payload;
	if (ipList.find(addr, payload))
		return payload != 0;
	return hasWhiteList;
}
