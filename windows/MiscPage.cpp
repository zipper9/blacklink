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
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/ConfCore.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 5, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 6, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { 8, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_ADV_MISC, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_ENABLE_CCPM, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_ENABLE_CPMI, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CCPM_AUTO_START, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_CCPM_TIMEOUT, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CCPM_TIMEOUT, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_CAPTION_MINUTES, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_CAPTION_MAX_UC, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_MAX_UC, 0, UNSPEC, UNSPEC, 0, &align3 }
};

static const PropPage::Item items[] =
{
	{ IDC_ENABLE_CCPM, Conf::USE_CCPM, PropPage::T_BOOL },
	{ IDC_ENABLE_CPMI, Conf::USE_CPMI, PropPage::T_BOOL },
	{ IDC_CCPM_AUTO_START, Conf::CCPM_AUTO_START, PropPage::T_BOOL },
	{ IDC_CCPM_TIMEOUT, Conf::CCPM_IDLE_TIMEOUT, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_MAX_UC, Conf::MAX_HUB_USER_COMMANDS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::NMDC_ENCODING_FROM_DOMAIN, ResourceManager::SETTINGS_NMDC_ENCODING_FROM_DOMAIN },
	{ Conf::HUB_USER_COMMANDS, ResourceManager::SETTINGS_HUB_USER_COMMANDS },
	{ Conf::SEND_UNKNOWN_COMMANDS, ResourceManager::SETTINGS_SEND_UNKNOWN_COMMANDS },
	{ Conf::SEND_BLOOM, ResourceManager::SETTINGS_SEND_BLOOM },
	{ Conf::USE_SUDP, ResourceManager::SETTINGS_USE_SUDP },
	{ Conf::SEND_EXT_JSON, ResourceManager::SETTINGS_SEND_EXT_JSON },
	{ Conf::USE_SALT_PASS, ResourceManager::SETTINGS_USE_SALTPASS },
	{ Conf::USE_BOT_LIST, ResourceManager::SETTINGS_USE_BOTLIST },
	{ Conf::USE_MCTO, ResourceManager::SETTINGS_USE_MCTO },
	{ Conf::USE_TTH_LIST, ResourceManager::SETTINGS_USE_TTH_LIST },
	{ Conf::SEND_DB_PARAM, ResourceManager::SETTINGS_SEND_DB_PARAM },
	{ Conf::SEND_QP_PARAM, ResourceManager::SETTINGS_SEND_QP_PARAM },
	{ 0, ResourceManager::Strings() }
};

LRESULT MiscPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::initControls(*this, items);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
	fixControls();
	return TRUE;
}

void MiscPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
}

void MiscPage::fixControls()
{
	BOOL enable = IsDlgButtonChecked(IDC_ENABLE_CCPM);
	GetDlgItem(IDC_ENABLE_CPMI).EnableWindow(enable);
	GetDlgItem(IDC_CCPM_AUTO_START).EnableWindow(enable);
	GetDlgItem(IDC_CCPM_TIMEOUT).EnableWindow(enable);
}
