
/*
 * ApexDC speedmod (c) SMT 2007
 */

#include "stdafx.h"
#include "MessagesPage.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_SETTINGS_DEFAULT_AWAY_MSG, ResourceManager::SETTINGS_DEFAULT_AWAY_MSG },	
	{ IDC_TIME_AWAY, ResourceManager::SET_SECONDARY_AWAY },
	{ IDC_AWAY_TO, ResourceManager::SETCZDC_TO },	
	{ IDC_BUFFER_STR, ResourceManager::BUFFER_STR },
	{ IDC_CAPTION_CHAT_LINES, ResourceManager::SETTINGS_CHAT_HISTORY },
	{ 0, ResourceManager::Strings() }
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
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_MESSAGES_BOOLEANS));
	
	ctrlList.Attach(GetDlgItem(IDC_MESSAGES_BOOLEANS));
	
	PropPage::translate((HWND)(*this), texts);
	
	timeCtrlBegin.Attach(GetDlgItem(IDC_AWAY_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_AWAY_END_TIME));
	
	WinUtil::GetTimeValues(timeCtrlBegin);
	WinUtil::GetTimeValues(timeCtrlEnd);
	
	timeCtrlBegin.SetCurSel(SETTING(SECONDARY_AWAY_START));
	timeCtrlEnd.SetCurSel(SETTING(SECONDARY_AWAY_END));
	
	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach();
	
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
	
	timeCtrlBegin.Attach(GetDlgItem(IDC_AWAY_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_AWAY_END_TIME));
	
	g_settings->set(SettingsManager::SECONDARY_AWAY_START, timeCtrlBegin.GetCurSel());
	g_settings->set(SettingsManager::SECONDARY_AWAY_END, timeCtrlEnd.GetCurSel());
	
	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach();
}

void MessagesPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_TIME_AWAY) == BST_CHECKED;
	::EnableWindow(GetDlgItem(IDC_AWAY_START_TIME), state);
	::EnableWindow(GetDlgItem(IDC_AWAY_END_TIME), state);
	::EnableWindow(GetDlgItem(IDC_SECONDARY_AWAY_MESSAGE), state);
	::EnableWindow(GetDlgItem(IDC_SECONDARY_AWAY_MSG), state);
	::EnableWindow(GetDlgItem(IDC_AWAY_TO), state);
}

