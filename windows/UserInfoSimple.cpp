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


#include "stdafx.h"
#include "UserInfoSimple.h"
#include "UserInfoBaseHandler.h"
#include "LineDlg.h"

tstring UserInfoSimple::getTagIP(const string& tag, Ip4Address ip4, const Ip6Address& ip6)
{
	string out = tag;
	if (ip4)
	{
		if (!out.empty()) out += ' ';
		out += Util::printIpAddress(ip4);
	}
	if (!Util::isEmpty(ip6))
	{
		if (!out.empty()) out += ' ';
		out += Util::printIpAddress(ip6);
	}
	return Text::toT(out);
}

tstring UserInfoSimple::getTagIP(const ClientManager::UserParams& params)
{
	return getTagIP(params.tag, params.ip4, params.ip6);
}

tstring UserInfoSimple::getBroadcastPrivateMessage()
{
	static tstring deftext;

	LineDlg dlg;
	dlg.description = TSTRING(PRIVATE_MESSAGE);
	dlg.title = TSTRING(SEND_TO_ALL_USERS);
	dlg.line = deftext;
	dlg.allowEmpty = false;
	dlg.icon = IconBitmaps::PM;

	if (dlg.DoModal() == IDOK)
	{
		deftext = std::move(dlg.line);
		return deftext;
	}
	return Util::emptyStringT;
}

static bool parseSlotTime(const tstring& s, uint64_t& result)
{
	unsigned values[3];
	int index = 0;
	int digits = 0;
	values[0] = 0;
	for (size_t i = 0; i < s.length(); i++)
	{
		if (s[i] == _T(':'))
		{
			if (!digits || index == 2) return false;
			values[++index] = 0;
			digits = 0;
			continue;
		}
		if (s[i] >= _T('0') && s[i] <= _T('9'))
		{
			values[index] *= 10;
			values[index] += s[i] - _T('0');
			++digits;
			continue;
		}
		return false;
	}
	if (index < 1 || !digits) return false;
	if (index == 2)
		result = values[0] * 3600 * 24 + values[1] * 3600 + values[2] * 60;
	else
		result = values[0] * 3600 + values[1] * 60;
	return true;
}

uint64_t UserInfoSimple::inputSlotTime()
{
	static tstring deftext = _T("00:30");
	uint64_t result = 0;
	
	LineDlg dlg;
	dlg.description = TSTRING(EXTRA_SLOT_TIME_FORMAT);
	dlg.title = TSTRING(SET_EXTRA_SLOT_TIME);
	dlg.line = deftext;
	dlg.allowEmpty = false;
	dlg.icon = IconBitmaps::UPLOAD_QUEUE;
	dlg.validator = [&result](LineDlg& dlg, tstring& errorMsg) -> bool
	{
		bool ret = parseSlotTime(dlg.line, result);
		if (!ret) errorMsg = TSTRING(INVALID_TIME_FORMAT);
		return ret;
	};
	if (dlg.DoModal() == IDOK)
	{
		deftext = std::move(dlg.line);
		return result;
	}
	return 0;
}
