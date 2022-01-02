/*
 * Copyright (C) 2006-2013 Crise, crise<at>mail.berlios.de
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
#include "MiscPage.h"
#include "DialogLayout.h"
#include "../client/SettingsManager.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_ADV_MISC, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_MAX_UC, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_RAW_TEXTS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_MAX_UC, 0, UNSPEC, UNSPEC, 0, &align1 }
};

static const PropPage::Item items[] =
{
	{ IDC_MAX_UC, SettingsManager::MAX_HUB_USER_COMMANDS, PropPage::T_INT },
	{ IDC_RAW1_TEXT, SettingsManager::RAW1_TEXT, PropPage::T_STR },
	{ IDC_RAW2_TEXT, SettingsManager::RAW2_TEXT, PropPage::T_STR },
	{ IDC_RAW3_TEXT, SettingsManager::RAW3_TEXT, PropPage::T_STR },
	{ IDC_RAW4_TEXT, SettingsManager::RAW4_TEXT, PropPage::T_STR },
	{ IDC_RAW5_TEXT, SettingsManager::RAW5_TEXT, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::HUB_USER_COMMANDS, ResourceManager::SETTINGS_HUB_USER_COMMANDS },
	{ SettingsManager::SEND_UNKNOWN_COMMANDS, ResourceManager::SETTINGS_SEND_UNKNOWN_COMMANDS },
	{ SettingsManager::SEND_BLOOM, ResourceManager::SETTINGS_SEND_BLOOM },
	{ SettingsManager::USE_CCPM, ResourceManager::SETTINGS_USE_CCPM },
	{ SettingsManager::USE_CPMI, ResourceManager::SETTINGS_USE_CPMI },
	{ SettingsManager::SEND_EXT_JSON, ResourceManager::SETTINGS_SEND_EXT_JSON },
	{ SettingsManager::USE_SALT_PASS, ResourceManager::SETTINGS_USE_SALTPASS },
	{ SettingsManager::USE_BOT_LIST, ResourceManager::SETTINGS_USE_BOTLIST },
	{ 0, ResourceManager::Strings() }
};

LRESULT MiscPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
	return TRUE;
}

void MiscPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
}
