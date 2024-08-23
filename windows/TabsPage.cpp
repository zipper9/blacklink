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
#include "ConfUI.h"
#include "../client/SettingsManager.h"

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
	{ IDC_TAB_WIDTH, Conf::TAB_SIZE, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_MAX_TAB_ROWS, Conf::MAX_TAB_ROWS, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem optionItems[] =
{
	{ Conf::NON_HUBS_FRONT, ResourceManager::NON_HUBS_FRONT },
	{ Conf::TABS_CLOSEBUTTONS, ResourceManager::TABS_CLOSEBUTTONS },
	{ Conf::TABS_BOLD, ResourceManager::TABS_BOLD },
	{ Conf::TABS_SHOW_INFOTIPS, ResourceManager::SETTINGS_TABS_INFO_TIPS },
	{ Conf::HUB_URL_IN_TITLE, ResourceManager::TABS_SHOW_HUB_URL },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::ListItem boldItems[] =
{
	{ Conf::BOLD_FINISHED_DOWNLOADS, ResourceManager::FINISHED_DOWNLOADS },
	{ Conf::BOLD_FINISHED_UPLOADS, ResourceManager::FINISHED_UPLOADS },
	{ Conf::BOLD_QUEUE, ResourceManager::DOWNLOAD_QUEUE },
	{ Conf::BOLD_HUB, ResourceManager::HUB },
	{ Conf::BOLD_PM, ResourceManager::PRIVATE_MESSAGE },
	{ Conf::BOLD_SEARCH, ResourceManager::SEARCH },
	{ Conf::BOLD_WAITING_USERS, ResourceManager::WAITING_USERS },
	{ 0, ResourceManager::Strings() }
};

LRESULT TabsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlOption.Attach(GetDlgItem(IDC_TABS_OPTIONS));
	ctrlBold.Attach(GetDlgItem(IDC_BOLD_BOOLEANS));

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::initControls(*this, items);
	PropPage::read(m_hWnd, items, optionItems, ctrlOption);
	PropPage::read(m_hWnd, nullptr, boldItems, ctrlBold);

	CComboBox tabsPosition(GetDlgItem(IDC_TABSCOMBO));
	tabsPosition.AddString(CTSTRING(TABS_TOP));
	tabsPosition.AddString(CTSTRING(TABS_BOTTOM));
	const auto ss = SettingsManager::instance.getUiSettings();
	tabsPosition.SetCurSel(ss->getInt(Conf::TABS_POS));

	return TRUE;
}

void TabsPage::write()
{
	PropPage::write(m_hWnd, items, optionItems, ctrlOption);
	PropPage::write(m_hWnd, nullptr, boldItems, ctrlBold);

	auto ss = SettingsManager::instance.getUiSettings();
	CComboBox tabsPosition(GetDlgItem(IDC_TABSCOMBO));
	ss->setInt(Conf::TABS_POS, tabsPosition.GetCurSel());
}
