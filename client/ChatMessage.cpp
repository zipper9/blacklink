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
#include "ChatMessage.h"
#include "Util.h"
#include "LocationUtil.h"
#include "ClientManager.h"
#include "ChatOptions.h"

string ChatMessage::format() const
{
	dcassert(from);
	string tmp;

	if (timestamp)
	{
		tmp += '[' + Util::getShortTimeString(timestamp) + "] ";
	}
	tmp += text;

	// Check all '<' and '[' after newlines as they're probably pastes...
	size_t i = 0;
	while ((i = tmp.find('\n', i)) != string::npos)
	{
		if (i + 1 < tmp.length())
		{
			if (tmp[i + 1] == '[' || tmp[i + 1] == '<')
			{
				tmp.insert(i + 1, "- ");
				i += 2;
			}
		}
		i++;
	}

#ifdef _WIN32
	Util::convertToDos(tmp);
#endif
	return tmp;
}

void ChatMessage::translateMe(string& text, bool& thirdPerson)
{
	if (Text::isAsciiPrefix2(text, string("/me ")) || Text::isAsciiPrefix2(text, string("+me ")))
	{
		thirdPerson = true;
		text.erase(0, 4);
	}
}

string ChatMessage::getExtra(const Identity& id)
{
	string result;
	int chatOptions = ChatOptions::getOptions();
	int flags = 0;
	if (chatOptions & ChatOptions::OPTION_SHOW_COUNTRY)
		flags |= IPInfo::FLAG_COUNTRY;
	if (chatOptions & ChatOptions::OPTION_SHOW_ISP)
		flags |= IPInfo::FLAG_LOCATION;
	if ((chatOptions & ChatOptions::OPTION_SHOW_IP) || flags)
	{
		IpAddress ip = id.getConnectIP();
		if (Util::isValidIp(ip) && !id.isIPCached(ip.type))
		{
			if (chatOptions & ChatOptions::OPTION_SHOW_IP)
				result += Util::printIpAddress(ip);
			if (flags)
			{
				IPInfo ipInfo;
				Util::getIpInfo(ip, ipInfo, flags);
				if (flags & IPInfo::FLAG_COUNTRY)
				{
					if (!ipInfo.country.empty())
					{
						if (!result.empty()) result += " | ";
						result += ipInfo.country;
					}
				}
				if (flags & IPInfo::FLAG_LOCATION)
				{
					if (!ipInfo.location.empty())
					{
						if (!result.empty()) result += " | ";
						result += ipInfo.location;
					}
				}
			}
		}
	}
	return result;
}

void ChatMessage::getUserParams(StringMap& params, const string& hubUrl, bool myMessage) const
{
	const OnlineUserPtr& ou = myMessage ? to : replyTo;
	params["hubNI"] = ClientManager::getOnlineHubName(hubUrl);
	params["hubURL"] = hubUrl;
	params["userCID"] = ou->getUser()->getCID().toBase32();
	params["userNI"] = ou->getIdentity().getNick();
	params["myCID"] = ClientManager::getMyCID().toBase32();
}
