/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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
#ifdef IRAINMAN_ENABLE_AUTO_BAN
#include "FakeDetectPage.h"
#include "WinUtil.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_ENABLE_AUTO_BAN, ResourceManager::SETTINGS_AUTO_BAN },
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	{ IDC_PROTECT_OP, ResourceManager::SETTINGS_PROTECT_OP },
#endif
	{ IDC_FAKE_SET, ResourceManager::SETTINGS_FAKE_SET },
	{ IDC_RAW_COMMANDS, ResourceManager::RAW_SET },
	{ IDC_DISCONNECTS_T, ResourceManager::DISCONNECT_RAW },
	{ IDC_TIMEOUT_T, ResourceManager::TIMEOUT_RAW },
	{ IDC_FAKESHARE_T, ResourceManager::FAKESHARE_RAW },
	{ IDC_LISTLEN_MISMATCH_T, ResourceManager::LISTLEN_MISMATCH },
	{ IDC_FILELIST_TOO_SMALL_T, ResourceManager::FILELIST_TOO_SMALL },
	{ IDC_FILELIST_UNAVAILABLE_T, ResourceManager::FILELIST_UNAVAILABLE },
	{ IDC_TEXT_FAKEPERCENT, ResourceManager::TEXT_FAKEPERCENT },
	{ IDC_TIMEOUTS, ResourceManager::ACCEPTED_TIMEOUTS },
	{ IDC_DISCONNECTS, ResourceManager::ACCEPTED_DISCONNECTS },
	{ IDC_PROT_USERS, ResourceManager::PROT_USERS },
	{ IDC_PROT_DESC, ResourceManager::PROT_DESC },
	{ IDC_PROT_FAVS, ResourceManager::PROT_FAVS },
	{ IDC_BAN_SLOTS, ResourceManager::BAN_SLOTS },
	{ IDC_BAN_SLOTS_H, ResourceManager::BAN_SLOTS_H },
	{ IDC_BAN_SHARE, ResourceManager::BAN_SHARE },
	{ IDC_BAN_LIMIT, ResourceManager::BAN_LIMIT },
	{ IDC_BAN_MSG, ResourceManager::BAN_MESSAGE },
	{ IDC_BANMSG_PERIOD, ResourceManager::BANMSG_PERIOD },
	{ IDC_BAN_STEALTH, ResourceManager::BAN_STEALTH },
	{ IDC_BAN_FORCE_PM, ResourceManager::BAN_FORCE_PM },	
	{ IDC_AUTOBAN_HINT, ResourceManager::AUTOBAN_HINT },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_ENABLE_AUTO_BAN, SettingsManager::ENABLE_AUTO_BAN, PropPage::T_BOOL },
	{ IDC_PERCENT_FAKE_SHARE_TOLERATED, SettingsManager::AUTOBAN_FAKE_SHARE_PERCENT, PropPage::T_INT },
	{ IDC_TIMEOUTS_NO, SettingsManager::AUTOBAN_MAX_TIMEOUTS, PropPage::T_INT },
	{ IDC_DISCONNECTS_NO, SettingsManager::AUTOBAN_MAX_DISCONNECTS, PropPage::T_INT },
	{ IDC_PROT_PATTERNS, SettingsManager::DONT_BAN_PATTERN, PropPage::T_STR },
	{ IDC_PROT_FAVS, SettingsManager::DONT_BAN_FAVS, PropPage::T_BOOL },
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	{ IDC_PROTECT_OP, SettingsManager::DONT_BAN_OP, PropPage::T_BOOL },
#endif
	{ IDC_BAN_SLOTS_NO, SettingsManager::AUTOBAN_SLOTS_MIN, PropPage::T_INT },
	{ IDC_BAN_SLOTS_NO_H, SettingsManager::AUTOBAN_SLOTS_MAX, PropPage::T_INT },
	{ IDC_BAN_SHARE_NO, SettingsManager::AUTOBAN_SHARE, PropPage::T_INT },
	{ IDC_BAN_LIMIT_NO, SettingsManager::AUTOBAN_LIMIT, PropPage::T_INT },
	{ IDC_BAN_MSG_STR, SettingsManager::BAN_MESSAGE, PropPage::T_STR },
	{ IDC_BANMSG_PERIOD_NO, SettingsManager::AUTOBAN_MSG_PERIOD, PropPage::T_INT },
	{ IDC_BAN_STEALTH, SettingsManager::AUTOBAN_STEALTH, PropPage::T_BOOL },
	{ IDC_BAN_FORCE_PM, SettingsManager::AUTOBAN_SEND_PM, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

LRESULT FakeDetect::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_FAKE_BOOLEANS));
	PropPage::read(*this, items, nullptr, ctrlList);
	CComboBox cRaw;
	
#define ADDSTRINGS \
	cRaw.AddString(CWSTRING(NO_ACTION)); \
	cRaw.AddString(Text::toT(SETTING(RAW1_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW2_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW3_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW4_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW5_TEXT)).c_str());
	
	cRaw.Attach(GetDlgItem(IDC_DISCONNECT_RAW));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::AUTOBAN_CMD_DISCONNECTS));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_TIMEOUT_RAW));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::AUTOBAN_CMD_TIMEOUTS));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_FAKE_RAW));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::AUTOBAN_CMD_FAKESHARE));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_LISTLEN));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::AUTOBAN_FL_LEN_MISMATCH));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_FILELIST_TOO_SMALL));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::AUTOBAN_FL_TOO_SMALL));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_FILELIST_UNAVAILABLE));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::AUTOBAN_FL_UNAVAILABLE));
	cRaw.Detach();
	
#undef ADDSTRINGS
	
	WinUtil::translate(*this, texts);
	
	ctrlList.ShowWindow(SW_HIDE);
	return TRUE;
}

void FakeDetect::write()
{
	PropPage::write(*this, items, nullptr, ctrlList);
	
	CComboBox cRaw(GetDlgItem(IDC_DISCONNECT_RAW));
	SET_SETTING(AUTOBAN_CMD_DISCONNECTS, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_TIMEOUT_RAW);
	SET_SETTING(AUTOBAN_CMD_TIMEOUTS, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_FAKE_RAW);
	SET_SETTING(AUTOBAN_CMD_FAKESHARE, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_LISTLEN);
	SET_SETTING(AUTOBAN_FL_LEN_MISMATCH, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_FILELIST_TOO_SMALL);
	SET_SETTING(AUTOBAN_FL_TOO_SMALL, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_FILELIST_UNAVAILABLE);
	SET_SETTING(AUTOBAN_FL_UNAVAILABLE, cRaw.GetCurSel());
}

#endif // IRAINMAN_ENABLE_AUTO_BAN
