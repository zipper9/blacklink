/*
 * FlylinkDC++ // Search Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "SearchPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_S, ResourceManager::S },
	{ IDC_SETTINGS_SEARCH_HISTORY, ResourceManager::SETTINGS_SEARCH_HISTORY },
	{ IDC_SETTINGS_AUTO_SEARCH_LIMIT, ResourceManager::SETTINGS_AUTO_SEARCH_LIMIT },
	{ IDC_INTERVAL_TEXT, ResourceManager::MINIMUM_SEARCH_INTERVAL },
	{ IDC_MATCH_QUEUE_TEXT, ResourceManager::SETTINGS_SB_MAX_SOURCES },
	{ IDC_SEARCH_FORGET, ResourceManager::FORGET_SEARCH_REQUEST },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_SEARCH_HISTORY, SettingsManager::SEARCH_HISTORY, PropPage::T_INT },
	{ IDC_INTERVAL, SettingsManager::MIN_SEARCH_INTERVAL, PropPage::T_INT },
	{ IDC_MATCH, SettingsManager::AUTO_SEARCH_MAX_SOURCES, PropPage::T_INT },
	{ IDC_AUTO_SEARCH_LIMIT, SettingsManager::AUTO_SEARCH_LIMIT, PropPage::T_INT },
	{ IDC_SEARCH_FORGET, SettingsManager::FORGET_SEARCH_REQUEST, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::CLEAR_SEARCH, ResourceManager::SETTINGS_CLEAR_SEARCH },
	{ SettingsManager::ADLS_BREAK_ON_FIRST, ResourceManager::SETTINGS_ADLS_BREAK_ON_FIRST },
	{ SettingsManager::SEARCH_PASSIVE, ResourceManager::SETCZDC_PASSIVE_SEARCH },
	{ SettingsManager::INCOMING_SEARCH_TTH_ONLY, ResourceManager::INCOMING_SEARCH_TTH_ONLY },
	{ SettingsManager::FILTER_ENTER, ResourceManager::SETTINGS_FILTER_ENTER },
	{ 0, ResourceManager::Strings()}
};

#define setMinMax(x, y, z) \
	updown.Attach(GetDlgItem(x)); \
	updown.SetRange32(y, z); \
	updown.Detach();

LRESULT SearchPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_ADVANCED_BOOLEANS));

	PropPage::translate(*this, texts);
	PropPage::read(*this, items, listItems, ctrlList);
	
	CUpDownCtrl updown;
	setMinMax(IDC_SEARCH_HISTORY_SPIN, 1, 100);
	setMinMax(IDC_INTERVAL_SPIN, 2, 100);
	setMinMax(IDC_MATCH_SPIN, 1, 999);
	setMinMax(IDC_AUTO_SEARCH_LIMIT_SPIN, 1, 999);
	
	fixControls();
	return TRUE;
}

void SearchPage::write()
{
	PropPage::write(*this, items, listItems, ctrlList);
}

LRESULT SearchPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // [+]NightOrion
{
	fixControls();
	return 0;
}

void SearchPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_SEARCH_FORGET) != BST_CHECKED;
	::EnableWindow(GetDlgItem(IDC_SETTINGS_SEARCH_HISTORY), state);
	::EnableWindow(GetDlgItem(IDC_SEARCH_HISTORY), state);
	::EnableWindow(GetDlgItem(IDC_SEARCH_HISTORY_SPIN), state);
}
