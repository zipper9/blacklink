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
#include "Resource.h"
#include "TabsPage.h"
#include "WinUtil.h"

static const PropPage::Item items[] =
{
	{ IDC_TAB_WIDTH, SettingsManager::TAB_SIZE, PropPage::T_INT },
	{ IDC_MAX_TAB_ROWS, SettingsManager::MAX_TAB_ROWS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::TextItem textItem[] =
{
	{ IDC_SETTINGS_TABS_OPTIONS, ResourceManager::SETTINGS_TABS_OPTIONS },
	{ IDC_SETTINGS_BOLD_CONTENTS, ResourceManager::SETTINGS_BOLD_OPTIONS },
	{ IDC_TABSTEXT, ResourceManager::TABS_POSITION },
	{ IDC_SETTINGS_TAB_WIDTH, ResourceManager::SETTINGS_TAB_WIDTH },
	{ IDC_SETTINGS_MAX_TAB_ROWS, ResourceManager::SETTINGS_MAX_TAB_ROWS },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::ListItem optionItems[] =
{
	{ SettingsManager::NON_HUBS_FRONT, ResourceManager::NON_HUBS_FRONT },
	{ SettingsManager::TABS_CLOSEBUTTONS, ResourceManager::TABS_CLOSEBUTTONS },
	{ SettingsManager::TABS_BOLD, ResourceManager::TABS_BOLD },
	{ SettingsManager::TABS_SHOW_INFOTIPS, ResourceManager::SETTINGS_TABS_INFO_TIPS },
	{ SettingsManager::STRIP_TOPIC, ResourceManager::SETTINGS_STRIP_TOPIC },    //AdvancedPage
	{ SettingsManager::SHOW_FULL_HUB_INFO_ON_TAB, ResourceManager::SETTINGS_SHOW_FULL_HUB_INFO_ON_TAB },
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

	PropPage::translate(m_hWnd, textItem);
	PropPage::read(m_hWnd, items, optionItems, ctrlOption);
	PropPage::read(m_hWnd, nullptr, boldItems, ctrlBold);
	
	CUpDownCtrl updownWidth(GetDlgItem(IDC_SPIN_TAB_WIDTH));
	updownWidth.SetRange32(7, 80);

	CUpDownCtrl updownRows(GetDlgItem(IDC_SPIN_MAX_TAB_ROWS));
	updownRows.SetRange32(1, 20);
	
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