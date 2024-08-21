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
#include "DefaultClickPage.h"
#include "DialogLayout.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_DOUBLE_CLICK_ACTION,        FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_TRANSFERLISTDBLCLICKACTION, FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_CHATDBLCLICKACTION,         FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_USERLISTDBLCLICKACTION,     FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_FAVUSERLISTDBLCLICKACTION,  FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_MAGNETURLCLICKACTION,       FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_CAPTION_CLICK_NEWMAGNET,    FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_CAPTION_CLICK_OLDMAGNET,    FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_TRANSFERLIST_DBLCLICK,      0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_CHAT_DBLCLICK,              0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_USERLIST_DBLCLICK,          0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_FAVUSERLIST_DBLCLICK,       0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_CLICK_NEWMAGNET,            0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_CLICK_OLDMAGNET,            0,              UNSPEC, UNSPEC, 0, &align1 }
};

LRESULT DefaultClickPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, nullptr);

	userListAction.Attach(GetDlgItem(IDC_USERLIST_DBLCLICK));
	transferListAction.Attach(GetDlgItem(IDC_TRANSFERLIST_DBLCLICK));
	chatAction.Attach(GetDlgItem(IDC_CHAT_DBLCLICK));
	newMagnetAction.Attach(GetDlgItem(IDC_CLICK_NEWMAGNET));
	oldMagnetAction.Attach(GetDlgItem(IDC_CLICK_OLDMAGNET));

	favUserListAction.Attach(GetDlgItem(IDC_FAVUSERLIST_DBLCLICK));
	favUserListAction.AddString(CTSTRING(GET_FILE_LIST));
	favUserListAction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	favUserListAction.AddString(CTSTRING(MATCH_QUEUE));
	favUserListAction.AddString(CTSTRING(EDIT_PROPERTIES));
	favUserListAction.AddString(CTSTRING(OPEN_USER_LOG));

	userListAction.AddString(CTSTRING(GET_FILE_LIST));
	userListAction.AddString(CTSTRING(ADD_NICK_TO_CHAT));
	userListAction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	userListAction.AddString(CTSTRING(MATCH_QUEUE));
	userListAction.AddString(CTSTRING(GRANT_EXTRA_SLOT));
	userListAction.AddString(CTSTRING(ADD_TO_FAVORITES));
	userListAction.AddString(CTSTRING(BROWSE_FILE_LIST));

	transferListAction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	transferListAction.AddString(CTSTRING(GET_FILE_LIST));
	transferListAction.AddString(CTSTRING(MATCH_QUEUE));
	transferListAction.AddString(CTSTRING(GRANT_EXTRA_SLOT));
	transferListAction.AddString(CTSTRING(ADD_TO_FAVORITES));
	transferListAction.AddString(CTSTRING(FORCE_ATTEMPT));
	transferListAction.AddString(CTSTRING(BROWSE_FILE_LIST));
	transferListAction.AddString(CTSTRING(OPEN_DOWNLOAD_QUEUE));

	chatAction.AddString(CTSTRING(SELECT_USER_LIST));
	chatAction.AddString(CTSTRING(ADD_NICK_TO_CHAT));
	chatAction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	chatAction.AddString(CTSTRING(GET_FILE_LIST));
	chatAction.AddString(CTSTRING(MATCH_QUEUE));
	chatAction.AddString(CTSTRING(GRANT_EXTRA_SLOT));
	chatAction.AddString(CTSTRING(ADD_TO_FAVORITES));

	const auto ss = SettingsManager::instance.getUiSettings();
	userListAction.SetCurSel(ss->getInt(Conf::USERLIST_DBLCLICK));
	transferListAction.SetCurSel(ss->getInt(Conf::TRANSFERLIST_DBLCLICK));
	chatAction.SetCurSel(ss->getInt(Conf::CHAT_DBLCLICK));
	favUserListAction.SetCurSel(ss->getInt(Conf::FAVUSERLIST_DBLCLICK));

	newMagnetAction.AddString(CTSTRING(ASK));
	newMagnetAction.AddString(CTSTRING(MAGNET_DLG_BRIEF_SEARCH));
	newMagnetAction.AddString(CTSTRING(MAGNET_DLG_BRIEF_DOWNLOAD));
	newMagnetAction.AddString(CTSTRING(MAGNET_DLG_BRIEF_OPEN));

	oldMagnetAction.AddString(CTSTRING(ASK));
	oldMagnetAction.AddString(CTSTRING(SEARCH_FOR_ALTERNATES));
	oldMagnetAction.AddString(CTSTRING(OPEN_FILE));

	if (ss->getBool(Conf::MAGNET_ASK))
	{
		newMagnetAction.SetCurSel(0);
	}
	else
	{
		int index = ss->getInt(Conf::MAGNET_ACTION);
		if (!(index >= Conf::MAGNET_ACTION_SEARCH && index <= Conf::MAGNET_ACTION_DOWNLOAD_AND_OPEN))
			index = Conf::MAGNET_ACTION_SEARCH;
		newMagnetAction.SetCurSel(index + 1);
	}

	if (ss->getBool(Conf::SHARED_MAGNET_ASK))
	{
		oldMagnetAction.SetCurSel(0);
	}
	else
	{
		int index = ss->getInt(Conf::SHARED_MAGNET_ACTION);
		oldMagnetAction.SetCurSel(index == Conf::MAGNET_ACTION_OPEN_EXISTING ? 2 : 1);
	}

	return TRUE;
}

void DefaultClickPage::write()
{
	PropPage::write(*this, nullptr);

	auto ss = SettingsManager::instance.getUiSettings();
	ss->setInt(Conf::USERLIST_DBLCLICK, userListAction.GetCurSel());
	ss->setInt(Conf::TRANSFERLIST_DBLCLICK, transferListAction.GetCurSel());
	ss->setInt(Conf::CHAT_DBLCLICK, chatAction.GetCurSel());
	ss->setInt(Conf::FAVUSERLIST_DBLCLICK, favUserListAction.GetCurSel());

	int index = newMagnetAction.GetCurSel();
	if (index == 0)
	{
		ss->setBool(Conf::MAGNET_ASK, true);
	}
	else
	{
		ss->setBool(Conf::MAGNET_ASK, false);
		ss->setInt(Conf::MAGNET_ACTION, index - 1);
	}

	index = oldMagnetAction.GetCurSel();
	if (index == 0)
	{
		ss->setBool(Conf::SHARED_MAGNET_ASK, true);
	}
	else
	{
		index = index == 1 ? Conf::MAGNET_ACTION_SEARCH : Conf::MAGNET_ACTION_OPEN_EXISTING;
		ss->setBool(Conf::SHARED_MAGNET_ASK, false);
		ss->setInt(Conf::SHARED_MAGNET_ACTION, index);
	}
}
