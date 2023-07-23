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
#include "ClientManager.h"
#include "Upload.h"
#include "UserConnection.h"
#include "TimeUtil.h"

const string Transfer::fileTypeNames[] =
{
	"file", "file", "list", "tthl"
};

const string Transfer::fileNameFilesXml = "files.xml";
const string Transfer::fileNameFilesBzXml = "files.xml.bz2";

Transfer::Transfer(UserConnection* conn, const string& path, const TTHValue& tth) :
	type(TYPE_FILE),
	path(path), tth(tth), actual(0), pos(0), userConnection(conn), hintedUser(conn->getHintedUser()), startPos(0),
	isSecure(conn->isSecure()), isTrusted(conn->isTrusted()),
	startTime(0), lastTick(GET_TICK()),
	cipherName(conn->getCipherName()),
	ip(conn->getRemoteIp()),
	fileSize(-1)
{
	speed.setStartTick(lastTick);
}

const string& Transfer::getConnectionQueueToken() const
{
	return userConnection ? userConnection->getConnectionQueueToken() : Util::emptyString;
}

void Transfer::getParams(const UserConnection* source, StringMap& params) const
{
	const string& hint = source->getHintedUser().hint;
	const UserPtr& user = source->getUser();
	dcassert(user);
	if (user)
	{
		const string nick = user->getLastNick();
		params["userCID"] = user->getCID().toBase32();
		params["userNI"] = !nick.empty() ? nick : Util::toString(ClientManager::getNicks(user->getCID(), Util::emptyString, false));
		IpAddress ip = source->getRemoteIp();
		switch (ip.type)
		{
			case AF_INET:
				params["userI4"] = Util::printIpAddress(ip.data.v4);
				break;
			case AF_INET6:
				params["userI6"] = Util::printIpAddress(ip.data.v6);
				break;
		}
		
		StringList hubNames = ClientManager::getHubNames(user->getCID(), hint);
		if (hubNames.empty())
		{
			hubNames.push_back(STRING(OFFLINE));
		}
		params["hub"] = Util::toString(hubNames);
		
		StringList hubs = ClientManager::getHubs(user->getCID(), hint);
		if (hubs.empty())
		{
			hubs.push_back(STRING(OFFLINE));
		}
		params["hubURL"] = Util::toString(hubs);
		
		params["fileSI"] = Util::toString(getSize());
		params["fileSIshort"] = Util::formatBytes(getSize());
		params["fileSIchunk"] = Util::toString(getPos());
		params["fileSIchunkshort"] = Util::formatBytes(getPos());
		params["fileSIactual"] = Util::toString(getActual());
		params["fileSIactualshort"] = Util::formatBytes(getActual());
		params["speed"] = Util::formatBytes(getRunningAverage()) + '/' + STRING(S);
		params["time"] = getStartTime() == 0 ? "-" : Util::formatSeconds((getLastTick() - getStartTime()) / 1000);
		params["fileTR"] = getTTH().toBase32();
	}
}

void Transfer::setStartTime(uint64_t tick)
{
	startTime = tick;
	LOCK(csSpeed);
	setLastTick(tick);
	speed.setStartTick(tick);
}

uint64_t Transfer::getLastActivity() const
{
	return userConnection ? userConnection->getLastActivity() : 0;
}

int64_t Transfer::getRunningAverage() const
{
	LOCK(csSpeed);
	return runningAverage;
}
