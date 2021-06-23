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
#include "TabsPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SETTINGS_TABS_OPTIONS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_BOLD_CONTENTS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_TABSTEXT, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_TAB_WIDTH, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_MAX_TAB_ROWS, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_TABSCOMBO, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_TAB_WIDTH, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_MAX_TAB_ROWS, 0, UNSPEC, UNSPEC, 0, &align1 }
};

static const PropPage::Item items[] =
{
	{ IDC_TAB_WIDTH, SettingsManager::TAB_SIZE, PropPage::T_INT },
	{ IDC_MAX_TAB_ROWS, SettingsManager::MAX_TAB_ROWS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem optionItems[] =
{
	{ SettingsManager::NON_HUBS_FRONT, ResourceManager::NON_HUBS_FRONT },
	{ SettingsManager::TABS_CLOSEBUTTONS, ResourceManager::TABS_CLOSEBUTTONS },
	{ SettingsManager::TABS_BOLD, ResourceManager::TABS_BOLD },
	{ SettingsManager::TABS_SHOW_INFOTIPS, ResourceManager::SETTINGS_TABS_INFO_TIPS },
	{ SettingsManager::HUB_URL_IN_TITLE, ResourceManager::TABS_SHOW_HUB_URL },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::ListItem boldItems[] =
{
	{ SettingsManager::BOLD_FINISHED_DOWNLOADS, ResourceManager::FINISHED_DOWNLOADS },
	{ SettingsManager::BOLD_FINISHED_UPLOADS, ResourceManager::FINISHED_UPLOADS },
	{ SettingsManager::BOLD_QUEUE, ResourceManager::DOWNLOAD_QUEUE },
	{ SettingsManager::BOLD_HUB, ResourceManager::HUB },
	{ SettingsManager::BOLD_PM, ResourceManager::PRIVATE_MESSAGE },
	{ SettingsManager::BOLD_SEARCH, ResourceManager::SEARCH },
	{ SettingsManager::BOLD_WAITING_USERS, ResourceManager::WAITING_USERS },
	{ 0, ResourceManager::Strings() }
};

LRESULT TabsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlOption.Attach(GetDlgItem(IDC_TABS_OPTIONS));
	ctrlBold.Attach(GetDlgItem(IDC_BOLD_BOOLEANS));

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(m_hWnd, items, optionItems, ctrlOption);
	PropPage::read(m_hWnd, nullptr, boldItems, ctrlBold);
	
	CUpDownCtrl updownWidth(GetDlgItem(IDC_SPIN_TAB_WIDTH));
	updownWidth.SetRange32(7, 80);
	updownWidth.SetBuddy(GetDlgItem(IDC_TAB_WIDTH));

	CUpDownCtrl updownRows(GetDlgItem(IDC_SPIN_MAX_TAB_ROWS));
	updownRows.SetRange32(1, 20);
	updownRows.SetBuddy(GetDlgItem(IDC_MAX_TAB_ROWS));

	CComboBox tabsPosition(GetDlgItem(IDC_TABSCOMBO));
	tabsPosition.AddString(CTSTRING(TABS_TOP));
	tabsPosition.AddString(CTSTRING(TABS_BOTTOM));
	tabsPosition.SetCurSel(SETTING(TABS_POS));
	
	return TRUE;
}

void TabsPage::write()
{
	PropPage::write(m_hWnd, items, optionItems, ctrlOption);
	PropPage::write(m_hWnd, nullptr, boldItems, ctrlBold);
	
	CComboBox tabsPosition(GetDlgItem(IDC_TABSCOMBO));
	g_settings->set(SettingsManager::TABS_POS, tabsPosition.GetCurSel());
}