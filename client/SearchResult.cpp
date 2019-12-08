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
#include "UploadManager.h"
#include "Text.h"
#include "User.h"
#include "ClientManager.h"
#include "Client.h"
#include "CFlylinkDBManager.h"
#include "ShareManager.h"
#include "QueueManager.h"

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
	tmp.append(Util::toString(freeSlots));
	tmp.append(1, '/');
	tmp.append(Util::toString(slots));
	tmp.append("\x05TTH:", 5);
	tmp.append(getTTH().toBase32());
	tmp.append(" (", 2);
	tmp.append(c.getIpPort());
	tmp.append(")|", 2);
	return tmp;
}

void SearchResultCore::toRES(AdcCommand& cmd, unsigned freeSlots) const
{
	cmd.addParam("SI", Util::toString(getSize()));
	cmd.addParam("SL", Util::toString(freeSlots));
	cmd.addParam("FN", Util::toAdcFile(getFile()));
	cmd.addParam("TR", getTTH().toBase32());
}

SearchResult::SearchResult(Types type, int64_t size, const string& file, const TTHValue& tth, uint32_t token) :
	SearchResultCore(type, size, file, tth),
	user(ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks()),
	flags(0),
	token(token),
	p2pGuardInit(false),
	freeSlots(0),
	slots(0)
{
}

SearchResult::SearchResult(const UserPtr& user, Types type, unsigned slots, unsigned freeSlots,
                           int64_t size, const string& file, const string& hubName,
                           const string& hubURL, boost::asio::ip::address_v4 ip4, const TTHValue& tth, uint32_t token) :
	SearchResultCore(type, size, file, tth),
	hubName(hubName),
	hubURL(hubURL),
	user(user),
	ip(ip4),
	flags(0),
	token(token),
	p2pGuardInit(false),
	freeSlots(freeSlots),
	slots(slots)
{
}

void SearchResult::calcP2PGuard()
{
	if (!p2pGuardInit)
	{
		if (ip.to_ulong())
		{
			p2pGuardText = CFlylinkDBManager::getInstance()->is_p2p_guard(ip.to_ulong());
			p2pGuardInit = true;
		}
	}
}

void SearchResult::calcHubName()
{
	if (hubName.empty() && getUser())
	{
		const StringList names = ClientManager::getHubNames(getUser()->getCID(), Util::emptyString);
		hubName = names.empty() ? STRING(OFFLINE) : Util::toString(names);
	}
}

void SearchResult::checkTTH()
{
	if (type != TYPE_FILE || (flags & FLAG_STATUS_KNOWN)) return;
	if (ShareManager::isTTHShared(getTTH()))
		flags |= FLAG_SHARED;
	else
	{
		unsigned status;
		string path;
		CFlylinkDBManager::getInstance()->getFileInfo(getTTH(), status, path);
		if (status & CFlylinkDBManager::FLAG_DOWNLOADED)
			flags |= FLAG_DOWNLOADED;
		if (status & CFlylinkDBManager::FLAG_DOWNLOAD_CANCELED)
			flags |= FLAG_DOWNLOAD_CANCELED;
	}
	flags |= FLAG_STATUS_KNOWN;
}

string SearchResult::getFilePath() const
{
	if (getType() == TYPE_FILE)
		return Util::getFilePath(getFile());
	else
		return Util::emptyString;
}

string SearchResult::getFileName() const
{
	if (getType() == TYPE_FILE)
		return Util::getFileName(getFile());
		
	if (getFile().size() < 2)
		return getFile();
		
	const string::size_type i = getFile().rfind('\\', getFile().length() - 2);
	if (i == string::npos)
		return getFile();
		
	return getFile().substr(i + 1);
}
