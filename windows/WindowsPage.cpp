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
#include "WindowsPage.h"
#include "WinUtil.h"
#include "ConfUI.h"

static const PropPage::Item items[] =
{
	{ IDC_RECONNECT_HUBS, Conf::RECONNECT_HUBS_ON_START, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

static const WinUtil::TextItem textItem[] =
{
	{ IDC_RECONNECT_HUBS, ResourceManager::LAST_RECENT_HUBS },
	{ IDC_SETTINGS_AUTO_OPEN, ResourceManager::SETTINGS_AUTO_OPEN },
	{ IDC_SETTINGS_WINDOWS_OPTIONS, ResourceManager::SETTINGS_WINDOWS_OPTIONS },
	{ IDC_SETTINGS_CONFIRM_OPTIONS, ResourceManager::SETTINGS_CONFIRM_DIALOG_OPTIONS },
	{ 0, ResourceManager::Strings() }
};

// Open on startup
static const PropPage::ListItem listItems[] =
{
	{ Conf::OPEN_FAVORITE_HUBS, ResourceManager::FAVORITE_HUBS },
	{ Conf::OPEN_FAVORITE_USERS, ResourceManager::FAVORITE_USERS },
	{ Conf::OPEN_PUBLIC_HUBS, ResourceManager::PUBLIC_HUBS },
	{ Conf::OPEN_RECENT_HUBS, ResourceManager::RECENT_HUBS },
	{ Conf::OPEN_QUEUE, ResourceManager::DOWNLOAD_QUEUE },
	{ Conf::OPEN_FINISHED_DOWNLOADS, ResourceManager::FINISHED_DOWNLOADS },
	{ Conf::OPEN_WAITING_USERS, ResourceManager::WAITING_USERS },
	{ Conf::OPEN_FINISHED_UPLOADS, ResourceManager::FINISHED_UPLOADS },
	{ Conf::OPEN_DHT, ResourceManager::DHT_TITLE },
	{ Conf::OPEN_NETWORK_STATISTICS, ResourceManager::NETWORK_STATISTICS },
	{ Conf::OPEN_NOTEPAD, ResourceManager::NOTEPAD },
	{ Conf::OPEN_ADLSEARCH, ResourceManager::ADL_SEARCH },
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
	{ Conf::OPEN_CDMDEBUG, ResourceManager::MENU_CDMDEBUG_MESSAGES },
#endif
	{ Conf::OPEN_SEARCH_SPY, ResourceManager::SEARCH_SPY },
	{ 0, ResourceManager::Strings() }
};

// Window settings
static const PropPage::ListItem optionItems[] =
{
	{ Conf::POPUP_PMS_OTHER, ResourceManager::SETTINGS_POPUP_PMS_OTHER },
	{ Conf::POPUP_PMS_HUB, ResourceManager::SETTINGS_POPUP_PMS_HUB },
	{ Conf::POPUP_PMS_BOT, ResourceManager::SETTINGS_POPUP_PMS_BOT },
	{ Conf::POPUNDER_FILELIST, ResourceManager::SETTINGS_POPUNDER_FILELIST },
	{ Conf::POPUNDER_PM, ResourceManager::SETTINGS_POPUNDER_PM },
	{ Conf::JOIN_OPEN_NEW_WINDOW, ResourceManager::SETTINGS_OPEN_NEW_WINDOW },
	{ Conf::TOGGLE_ACTIVE_WINDOW, ResourceManager::SETTINGS_TOGGLE_ACTIVE_WINDOW },
	{ Conf::PROMPT_HUB_PASSWORD, ResourceManager::SETTINGS_PROMPT_PASSWORD },
	{ Conf::REMEMBER_SETTINGS_PAGE, ResourceManager::REMEMBER_SETTINGS_PAGE },
	{ 0, ResourceManager::Strings() }
};

// Confirmations
static const PropPage::ListItem confirmItems[] =
{
	{ Conf::CONFIRM_EXIT, ResourceManager::SETTINGS_CONFIRM_EXIT },
	{ Conf::CONFIRM_HUB_REMOVAL, ResourceManager::SETTINGS_CONFIRM_HUB_REMOVAL },
	{ Conf::CONFIRM_HUBGROUP_REMOVAL, ResourceManager::SETTINGS_CONFIRM_HUBGROUP_REMOVAL },
	{ Conf::CONFIRM_USER_REMOVAL, ResourceManager::SETTINGS_CONFIRM_USER_REMOVAL },
	{ Conf::CONFIRM_ADLS_REMOVAL, ResourceManager::SETTINGS_CONFIRM_ADLS_REMOVAL },
	{ Conf::CONFIRM_DELETE, ResourceManager::SETTINGS_CONFIRM_ITEM_REMOVAL },
	{ Conf::CONFIRM_FINISHED_REMOVAL, ResourceManager::SETTINGS_CONFIRM_FINISHED_REMOVAL },
	{ Conf::CONFIRM_CLEAR_SEARCH_HISTORY, ResourceManager::SETTINGS_CONFIRM_CLEAR_SEARCH_HISTORY },
	{ 0, ResourceManager::Strings() }
};

LRESULT WindowsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlStartup.Attach(GetDlgItem(IDC_WINDOWS_STARTUP));
	ctrlOptions.Attach(GetDlgItem(IDC_WINDOWS_OPTIONS));
	ctrlConfirms.Attach(GetDlgItem(IDC_CONFIRM_OPTIONS));

	WinUtil::translate(*this, textItem);
	PropPage::read(*this, items, listItems, ctrlStartup);
	PropPage::read(*this, nullptr, optionItems, ctrlOptions);
	PropPage::read(*this, nullptr, confirmItems, ctrlConfirms);

	return TRUE;
}

void WindowsPage::write()
{
	PropPage::write(*this, items, listItems, ctrlStartup);
	PropPage::write(*this, nullptr, optionItems, ctrlOptions);
	PropPage::write(*this, nullptr, confirmItems, ctrlConfirms);
}
