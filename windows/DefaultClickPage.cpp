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

	userListAction.SetCurSel(SETTING(USERLIST_DBLCLICK));
	transferListAction.SetCurSel(SETTING(TRANSFERLIST_DBLCLICK));
	chatAction.SetCurSel(SETTING(CHAT_DBLCLICK));
	favUserListAction.SetCurSel(SETTING(FAVUSERLIST_DBLCLICK));

	newMagnetAction.AddString(CTSTRING(ASK));
	newMagnetAction.AddString(CTSTRING(MAGNET_DLG_BRIEF_SEARCH));
	newMagnetAction.AddString(CTSTRING(MAGNET_DLG_BRIEF_DOWNLOAD));
	newMagnetAction.AddString(CTSTRING(MAGNET_DLG_BRIEF_OPEN));

	oldMagnetAction.AddString(CTSTRING(ASK));
	oldMagnetAction.AddString(CTSTRING(SEARCH_FOR_ALTERNATES));
	oldMagnetAction.AddString(CTSTRING(OPEN_FILE));

	if (BOOLSETTING(MAGNET_ASK))
	{
		newMagnetAction.SetCurSel(0);
	}
	else
	{
		int index = SETTING(MAGNET_ACTION);
		if (!(index >= SettingsManager::MAGNET_ACTION_SEARCH && index <= SettingsManager::MAGNET_ACTION_DOWNLOAD_AND_OPEN))
			index = SettingsManager::MAGNET_ACTION_SEARCH;
		newMagnetAction.SetCurSel(index + 1);
	}

	if (BOOLSETTING(SHARED_MAGNET_ASK))
	{
		oldMagnetAction.SetCurSel(0);
	}
	else
	{
		int index = SETTING(SHARED_MAGNET_ACTION);
		index = index == SettingsManager::MAGNET_ACTION_OPEN_EXISTING ? 2 : 1;
		oldMagnetAction.SetCurSel(index);
	}

	return TRUE;
}

void DefaultClickPage::write()
{
	PropPage::write(*this, nullptr);

	g_settings->set(SettingsManager::USERLIST_DBLCLICK, userListAction.GetCurSel());
	g_settings->set(SettingsManager::TRANSFERLIST_DBLCLICK, transferListAction.GetCurSel());
	g_settings->set(SettingsManager::CHAT_DBLCLICK, chatAction.GetCurSel());
	g_settings->set(SettingsManager::FAVUSERLIST_DBLCLICK, favUserListAction.GetCurSel());

	int index = newMagnetAction.GetCurSel();
	if (index == 0)
	{
		g_settings->set(SettingsManager::MAGNET_ASK, true);
	}
	else
	{
		g_settings->set(SettingsManager::MAGNET_ASK, false);
		g_settings->set(SettingsManager::MAGNET_ACTION, index - 1);
	}

	index = oldMagnetAction.GetCurSel();
	if (index == 0)
	{
		g_settings->set(SettingsManager::SHARED_MAGNET_ASK, true);
	}
	else
	{
		index = index == 1 ? SettingsManager::MAGNET_ACTION_SEARCH : SettingsManager::MAGNET_ACTION_OPEN_EXISTING;
		g_settings->set(SettingsManager::SHARED_MAGNET_ASK, false);
		g_settings->set(SettingsManager::SHARED_MAGNET_ACTION, index);
	}
}
