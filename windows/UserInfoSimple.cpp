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
#include "HubFrame.h"
#include "LineDlg.h"
#include "../client/UploadManager.h"
#include "../client/QueueManager.h"

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

void UserInfoSimple::addSummaryMenu()
{
	// TODO: move obtain information about the user in the UserManager
	if (!getUser())
		return;
		
	UserInfoGuiTraits::userSummaryMenu.InsertSeparatorLast(Text::toT(getUser()->getLastNick()));
	
	ClientManager::UserParams params;
	if (ClientManager::getUserParams(getUser(), params))
	{
		tstring userInfo = TSTRING(SLOTS) + _T(": ") + Util::toStringT(params.slots) + _T(", ") + TSTRING(SHARED) + _T(": ") + Util::formatBytesT(params.bytesShared);
		
		if (params.limit)
			userInfo += _T(", ") + TSTRING(UPLOAD_SPEED_LIMIT) + _T(": ") + Util::formatBytesT(params.limit) + _T('/') + TSTRING(DATETIME_SECONDS);
		
		UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, userInfo.c_str());
		
		uint64_t slotTick = UploadManager::getInstance()->getReservedSlotTick(getUser());
		if (slotTick)
		{
			uint64_t currentTick = GET_TICK();
			if (slotTick >= currentTick + 1000)
			{
				const tstring note = TSTRING(EXTRA_SLOT_TIMEOUT) + _T(": ") + Util::formatSecondsT((slotTick-currentTick)/1000);
				UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, note.c_str());
			}
		}
		
		bool hasIp4 = Util::isValidIp4(params.ip4);
		bool hasIp6 = Util::isValidIp6(params.ip6);
		if (hasIp4 || hasIp6)
		{
			UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, getTagIP(params).c_str());
			IPInfo ipInfo;
			IpAddress ip;
			if (hasIp4)
			{
				ip.data.v4 = params.ip4;
				ip.type = AF_INET;
			}
			else
			{
				memcpy(ip.data.v6.data, params.ip6.data, 16);
				ip.type = AF_INET6;
			}
			Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION, true); // get it from cache
			if (!ipInfo.country.empty() || !ipInfo.location.empty())
			{
				tstring text = TSTRING(LOCATION_BARE) + _T(": ");
				if (!ipInfo.country.empty() && !ipInfo.location.empty())
				{
					text += Text::toT(ipInfo.country);
					text += _T(", ");
				}
				text += Text::toT(Util::getDescription(ipInfo));
				UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, text.c_str());
			}
		}
		else
		{
			tstring tagIp = getTagIP(params);
			if (!tagIp.empty())
				UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, tagIp.c_str());
		}
		HubFrame::addDupeUsersToSummaryMenu(params);
	}
	
	//UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_SEPARATOR);
	
	bool hasCaption = false;
	{
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		for (auto uit = users.cbegin(); uit != users.cend(); ++uit)
		{
			if (uit->getUser() == getUser())
			{
				int countAdded = 0;
				for (auto i = uit->waitingFiles.cbegin(); i != uit->waitingFiles.cend(); ++i)
				{
					if (!hasCaption)
					{
						UserInfoGuiTraits::userSummaryMenu.InsertSeparatorLast(TSTRING(USER_WAIT_MENU));
						hasCaption = true;
					}
					const tstring note =
					    Text::toT(Util::ellipsizePath((*i)->getFile())) +
					    _T("\t[") +
					    Util::toStringT((double)(*i)->getPos() * 100.0 / (double)(*i)->getSize()) +
					    _T("% ") +
					    Util::formatSecondsT(GET_TIME() - (*i)->getTime()) +
					    _T(']');
					UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, note.c_str());
					if (countAdded++ == 10)
					{
						UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, _T("..."));
						break;
					}
				}
			}
		}
	}
	hasCaption = false;
	{
		int countAdded = 0;
		QueueRLock(*QueueItem::g_cs);
		QueueManager::LockFileQueueShared fileQueue;
		const auto& downloads = fileQueue.getQueueL();
		for (auto j = downloads.cbegin(); j != downloads.cend(); ++j)
		{
			const QueueItemPtr& aQI = j->second;
			const bool src = aQI->isSourceL(getUser());
			bool badsrc = false;
			if (!src)
			{
				badsrc = aQI->isBadSourceL(getUser());
			}
			if (src || badsrc)
			{
				if (!hasCaption)
				{
					UserInfoGuiTraits::userSummaryMenu.InsertSeparatorLast(TSTRING(NEED_USER_FILES_MENU));
					hasCaption = true;
				}
				tstring note = Text::toT(aQI->getTarget());
				if (aQI->getSize() > 0)
				{
					note += _T("\t(");
					note += Util::toStringT((double)aQI->getDownloadedBytes() * 100.0 / (double)aQI->getSize());
					note += _T("%)");
				}
				const UINT flags = MF_STRING | MF_DISABLED | (badsrc ? MF_GRAYED : 0);
				UserInfoGuiTraits::userSummaryMenu.AppendMenu(flags, (UINT_PTR) 0, note.c_str());
				if (countAdded++ == 10)
				{
					UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, _T("..."));
					break;
				}
			}
		}
	}
	
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

uint64_t UserInfoSimple::inputSlotTime()
{
	static tstring deftext = _T("00:30");
	
	LineDlg dlg;
	dlg.description = TSTRING(EXTRA_SLOT_TIME_FORMAT);
	dlg.title = TSTRING(SET_EXTRA_SLOT_TIME);
	dlg.line = deftext;
	dlg.allowEmpty = false;
	dlg.icon = IconBitmaps::UPLOAD_QUEUE;
	
	if (dlg.DoModal() == IDOK)
	{
		deftext = dlg.line;
		unsigned int n = 0;
		for (size_t i = 0; i < deftext.length(); i++) // TODO: cleanup.
		{
			if (deftext[i] == L':') n++;
		}
		unsigned int d, h, m;
		switch (n)
		{
			case 1:
				if (swscanf(deftext.c_str(), L"%u:%u", &h, &m) == 2)
					return (h * 3600 + m * 60);
					
				break;
			case 2:
				if (swscanf(deftext.c_str(), L"%u:%u:%u", &d, &h, &m) == 3)
					return (d * 3600 * 24 + h * 3600 + m * 60);
					
				break;
		}
		::MessageBox(GetForegroundWindow(), CTSTRING(INVALID_TIME_FORMAT), CTSTRING(ERRORS), MB_OK | MB_ICONERROR);
	}
	return 0;
}
