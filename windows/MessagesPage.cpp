
/*
 * ApexDC speedmod (c) SMT 2007
 */

#include "stdafx.h"
#include "MessagesPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 3, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 4, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { 5, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align4 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SETTINGS_DEFAULT_AWAY_MSG, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_TIME_AWAY, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_AWAY_FROM, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_AWAY_START_TIME, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_AWAY_TO, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_AWAY_END_TIME, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_BUFFER_STR, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_CAPTION_CHAT_LINES, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_BUFFERSIZE, 0, UNSPEC, UNSPEC, 0, &align4 },
	{ IDC_CHAT_LINES, 0, UNSPEC, UNSPEC, 0, &align4 }
};

static const PropPage::Item items[] =
{
	{ IDC_DEFAULT_AWAY_MESSAGE, SettingsManager::DEFAULT_AWAY_MESSAGE, PropPage::T_STR },	
	{ IDC_SECONDARY_AWAY_MESSAGE, SettingsManager::SECONDARY_AWAY_MESSAGE, PropPage::T_STR },
	{ IDC_TIME_AWAY, SettingsManager::ENABLE_SECONDARY_AWAY, PropPage::T_BOOL },
	{ IDC_AWAY_START_TIME, SettingsManager::SECONDARY_AWAY_START, PropPage::T_INT },
	{ IDC_AWAY_END_TIME, SettingsManager::SECONDARY_AWAY_END, PropPage::T_INT },	
	{ IDC_BUFFERSIZE, SettingsManager::CHAT_BUFFER_SIZE, PropPage::T_INT },
	{ IDC_CHAT_LINES, SettingsManager::PM_LOG_LINES, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::SEND_SLOTGRANT_MSG, ResourceManager::SEND_SLOTGRANT_MSG },
	{ SettingsManager::USE_CTRL_FOR_LINE_HISTORY, ResourceManager::SETTINGS_USE_CTRL_FOR_LINE_HISTORY },
	{ SettingsManager::CHAT_TIME_STAMPS, ResourceManager::SETTINGS_TIME_STAMPS },
	{ SettingsManager::IP_IN_CHAT, ResourceManager::IP_IN_CHAT },
	{ SettingsManager::COUNTRY_IN_CHAT, ResourceManager::COUNTRY_IN_CHAT },
	{ SettingsManager::ISP_IN_CHAT, ResourceManager::ISP_IN_CHAT },
	{ SettingsManager::DISPLAY_CHEATS_IN_MAIN_CHAT, ResourceManager::SETTINGS_DISPLAY_CHEATS_IN_MAIN_CHAT },
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	{ SettingsManager::SHOW_SHARE_CHECKED_USERS, ResourceManager::SETTINGS_ADVANCED_SHOW_SHARE_CHECKED_USERS },
	{ SettingsManager::CHECK_NEW_USERS, ResourceManager::CHECK_ON_CONNECT },
#endif
	{ SettingsManager::STATUS_IN_CHAT, ResourceManager::SETTINGS_STATUS_IN_CHAT },
	{ SettingsManager::SHOW_JOINS, ResourceManager::SETTINGS_SHOW_JOINS },
	{ SettingsManager::FAV_SHOW_JOINS, ResourceManager::SETTINGS_FAV_SHOW_JOINS },
	{ SettingsManager::SUPPRESS_MAIN_CHAT, ResourceManager::SETTINGS_ADVANCED_SUPPRESS_MAIN_CHAT },
	{ SettingsManager::SUPPRESS_PMS, ResourceManager::SETTINGS_ADVANCED_SUPPRESS_PMS },
	{ SettingsManager::IGNORE_HUB_PMS, ResourceManager::SETTINGS_IGNORE_HUB_PMS },
	{ SettingsManager::IGNORE_BOT_PMS, ResourceManager::SETTINGS_IGNORE_BOT_PMS },
	{ SettingsManager::IGNORE_ME, ResourceManager::IGNORE_ME },
	{ SettingsManager::AUTO_AWAY, ResourceManager::SETTINGS_AUTO_AWAY },    //AdvancedPage
	{ 0, ResourceManager::Strings() }
};

LRESULT MessagesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_MESSAGES_BOOLEANS));

	CComboBox timeCtrlBegin(GetDlgItem(IDC_AWAY_START_TIME));
	CComboBox timeCtrlEnd(GetDlgItem(IDC_AWAY_END_TIME));

	WinUtil::fillTimeValues(timeCtrlBegin);
	WinUtil::fillTimeValues(timeCtrlEnd);

	timeCtrlBegin.SetCurSel(SETTING(SECONDARY_AWAY_START));
	timeCtrlEnd.SetCurSel(SETTING(SECONDARY_AWAY_END));

	fixControls();
	return TRUE;
}

LRESULT MessagesPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void MessagesPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_MESSAGES_BOOLEANS));

	CComboBox timeCtrlBegin(GetDlgItem(IDC_AWAY_START_TIME));
	CComboBox timeCtrlEnd(GetDlgItem(IDC_AWAY_END_TIME));

	g_settings->set(SettingsManager::SECONDARY_AWAY_START, timeCtrlBegin.GetCurSel());
	g_settings->set(SettingsManager::SECONDARY_AWAY_END, timeCtrlEnd.GetCurSel());
}

void MessagesPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_TIME_AWAY) == BST_CHECKED;
	GetDlgItem(IDC_AWAY_START_TIME).EnableWindow(state);
	GetDlgItem(IDC_AWAY_END_TIME).EnableWindow(state);
	GetDlgItem(IDC_SECONDARY_AWAY_MESSAGE).EnableWindow(state);
}
