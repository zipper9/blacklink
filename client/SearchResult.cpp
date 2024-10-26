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
#include "SearchResult.h"
#include "Client.h"
#include "LocationUtil.h"
#include "Util.h"
#include "DatabaseManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "AdcCommand.h"

SearchResultCore::SearchResultCore(Types type, int64_t size, const string& file, const TTHValue& tth):
	file(file), size(size), tth(tth), type(type)
{
}

string SearchResultCore::toSR(const Client& c, unsigned freeSlots, unsigned slots) const
{
	// File:        "$SR %s %s%c%s %d/%d%c%s (%s)|"
	// Directory:   "$SR %s %s %d/%d%c%s (%s)|"
	string tmp;
	tmp.reserve(128 + getFile().size());
	tmp.append("$SR ", 4);
	tmp.append(Text::fromUtf8(c.getMyNick(), c.getEncoding()));
	tmp.append(1, ' ');
	const string acpFile = Text::fromUtf8(getFile(), c.getEncoding());
	if (type == TYPE_FILE)
	{
		tmp.append(acpFile);
		tmp.append(1, '\x05');
		tmp.append(Util::toString(getSize()));
	}
	else
	{
		tmp.append(acpFile, 0, acpFile.length() - 1);
	}
	tmp.append(1, ' ');
	tmp.append(Util::toString(freeSlots == SLOTS_UNKNOWN ? 0 : freeSlots));
	tmp.append(1, '/');
	tmp.append(Util::toString(slots == SLOTS_UNKNOWN ? 0 : slots));
	tmp.append("\x05TTH:", 5);
	tmp.append(getTTH().toBase32());
	tmp.append(" (", 2);
	tmp.append(c.getIpPort());
	tmp.append(")|", 2);
	return tmp;
}

void SearchResultCore::toRES(AdcCommand& cmd, unsigned freeSlots) const
{
	cmd.addParam(TAG('S', 'I'), Util::toString(getSize()));
	cmd.addParam(TAG('S', 'L'), Util::toString(freeSlots));
	cmd.addParam(TAG('F', 'N'), Util::toAdcFile(getFile()));
	cmd.addParam(TAG('T', 'R'), getTTH().toBase32());
}

SearchResult::SearchResult(const UserPtr& user, Types type, unsigned slots, unsigned freeSlots,
                           int64_t size, const string& file, const string& hubURL,
						   const IpAddress& ip, const TTHValue& tth, uint32_t token) :
	SearchResultCore(type, size, file, tth),
	hubURL(hubURL),
	user(user),
	ip(ip),
	flags(0),
	token(token),
	freeSlots(freeSlots),
	slots(slots)
{
}

void SearchResult::loadLocation()
{
	static const int flags = IPInfo::FLAG_LOCATION | IPInfo::FLAG_COUNTRY;
	if ((ipInfo.known & flags) != flags && Util::isValidIp(ip))
		Util::getIpInfo(ip, ipInfo, flags);
}

void SearchResult::loadP2PGuard()
{
	if (!(ipInfo.known & IPInfo::FLAG_P2P_GUARD) && Util::isValidIp(ip))
		Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_P2P_GUARD);
}

void SearchResult::checkTTH(HashDatabaseConnection* hashDb)
{
	if (type != TYPE_FILE || (flags & FLAG_STATUS_KNOWN)) return;
	if (ShareManager::getInstance()->isTTHShared(getTTH()))
		flags |= FLAG_SHARED;
	else if (hashDb && !getTTH().isZero())
	{
		unsigned status;
		string path;
		hashDb->getFileInfo(getTTH().data, status, nullptr, &path, nullptr, nullptr);
		if (status & DatabaseManager::FLAG_DOWNLOADED)
			flags |= FLAG_DOWNLOADED;
		if (status & DatabaseManager::FLAG_DOWNLOAD_CANCELED)
			flags |= FLAG_DOWNLOAD_CANCELED;
	}
	if (QueueManager::fileQueue.isQueued(getTTH()))
		flags |= FLAG_QUEUED;
	flags |= FLAG_STATUS_KNOWN;
}

string SearchResult::getFileName() const
{
	auto p1 = file.rfind('\\');
	if (p1 == string::npos)
		return file;
	if (type == TYPE_FILE || p1 != file.length() - 1)
		return file.substr(p1 + 1);
	auto p2 = file.rfind('\\', p1 - 1);
	if (p2 == string::npos)
		p2 = 0;
	else
		++p2;
	return file.substr(p2, p1 - p2);
}

string SearchResult::getFilePath() const
{
	if (type != TYPE_FILE)
		return Util::emptyString;
	auto p = file.rfind('\\');
	return p == string::npos ? Util::emptyString : file.substr(0, p);
}
