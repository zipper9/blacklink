
/*
 * ApexDC speedmod (c) SMT 2007
 */

#include "stdafx.h"
#include "MessagesPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/ConfCore.h"

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
	{ IDC_DEFAULT_AWAY_MESSAGE, Conf::DEFAULT_AWAY_MESSAGE, PropPage::T_STR },	
	{ IDC_SECONDARY_AWAY_MESSAGE, Conf::SECONDARY_AWAY_MESSAGE, PropPage::T_STR },
	{ IDC_TIME_AWAY, Conf::ENABLE_SECONDARY_AWAY, PropPage::T_BOOL },
	{ IDC_AWAY_START_TIME, Conf::SECONDARY_AWAY_START, PropPage::T_INT },
	{ IDC_AWAY_END_TIME, Conf::SECONDARY_AWAY_END, PropPage::T_INT },	
	{ IDC_BUFFERSIZE, Conf::CHAT_BUFFER_SIZE, PropPage::T_INT },
	{ IDC_CHAT_LINES, Conf::PM_LOG_LINES, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::SEND_SLOTGRANT_MSG, ResourceManager::SEND_SLOTGRANT_MSG },
	{ Conf::USE_CTRL_FOR_LINE_HISTORY, ResourceManager::SETTINGS_USE_CTRL_FOR_LINE_HISTORY },
	{ Conf::CHAT_TIME_STAMPS, ResourceManager::SETTINGS_TIME_STAMPS },
	{ Conf::IP_IN_CHAT, ResourceManager::IP_IN_CHAT },
	{ Conf::COUNTRY_IN_CHAT, ResourceManager::COUNTRY_IN_CHAT },
	{ Conf::ISP_IN_CHAT, ResourceManager::ISP_IN_CHAT },
	{ Conf::DISPLAY_CHEATS_IN_MAIN_CHAT, ResourceManager::SETTINGS_DISPLAY_CHEATS_IN_MAIN_CHAT },
#if 0
	{ Conf::SHOW_CHECKED_USERS, ResourceManager::SETTINGS_ADVANCED_SHOW_SHARE_CHECKED_USERS },
	{ Conf::CHECK_USERS_NMDC, ResourceManager::CHECK_ON_CONNECT },
#endif
	{ Conf::STATUS_IN_CHAT, ResourceManager::SETTINGS_STATUS_IN_CHAT },
	{ Conf::SHOW_JOINS, ResourceManager::SETTINGS_SHOW_JOINS },
	{ Conf::FAV_SHOW_JOINS, ResourceManager::SETTINGS_FAV_SHOW_JOINS },
	{ Conf::SUPPRESS_MAIN_CHAT, ResourceManager::SETTINGS_ADVANCED_SUPPRESS_MAIN_CHAT },
	{ Conf::SUPPRESS_PMS, ResourceManager::SETTINGS_ADVANCED_SUPPRESS_PMS },
	{ Conf::IGNORE_HUB_PMS, ResourceManager::SETTINGS_IGNORE_HUB_PMS },
	{ Conf::IGNORE_BOT_PMS, ResourceManager::SETTINGS_IGNORE_BOT_PMS },
	{ Conf::IGNORE_ME, ResourceManager::IGNORE_ME },
	{ Conf::AUTO_AWAY, ResourceManager::SETTINGS_AUTO_AWAY },    //AdvancedPage
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

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	int secondaryAwayStart = ss->getInt(Conf::SECONDARY_AWAY_START);
	int secondaryAwayEnd = ss->getInt(Conf::SECONDARY_AWAY_END);
	ss->unlockRead();

	timeCtrlBegin.SetCurSel(secondaryAwayStart);
	timeCtrlEnd.SetCurSel(secondaryAwayEnd);

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
	int secondaryAwayStart = timeCtrlBegin.GetCurSel();
	int secondaryAwayEnd = timeCtrlEnd.GetCurSel();

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	ss->setInt(Conf::SECONDARY_AWAY_START, secondaryAwayStart);
	ss->setInt(Conf::SECONDARY_AWAY_END, secondaryAwayEnd);
	ss->unlockWrite();
}

void MessagesPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_TIME_AWAY) == BST_CHECKED;
	GetDlgItem(IDC_AWAY_START_TIME).EnableWindow(state);
	GetDlgItem(IDC_AWAY_END_TIME).EnableWindow(state);
	GetDlgItem(IDC_SECONDARY_AWAY_MESSAGE).EnableWindow(state);
}
