/*
 * FlylinkDC++ // Search Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "SearchPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/ConfCore.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 2, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { 7, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SAVE_SEARCH, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SETTINGS_SEARCH_HISTORY, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SEARCH_HISTORY, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_INTERVAL_TEXT, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_AUTO_SEARCH_LIMIT, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_MATCH_QUEUE_TEXT, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_INTERVAL, 0, UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_AUTO_SEARCH_LIMIT, 0, UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_MATCH, 0, UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_S, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align3 }
};

static const PropPage::Item items[] =
{
	{ IDC_SEARCH_HISTORY, Conf::SEARCH_HISTORY, PropPage::T_INT },
	{ IDC_INTERVAL, Conf::MIN_SEARCH_INTERVAL, PropPage::T_INT },
	{ IDC_MATCH, Conf::AUTO_SEARCH_MAX_SOURCES, PropPage::T_INT },
	{ IDC_AUTO_SEARCH_LIMIT, Conf::AUTO_SEARCH_LIMIT, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::CLEAR_SEARCH, ResourceManager::SETTINGS_CLEAR_SEARCH },
	{ Conf::ADLS_BREAK_ON_FIRST, ResourceManager::SETTINGS_ADLS_BREAK_ON_FIRST },
	{ Conf::SEARCH_PASSIVE, ResourceManager::SETCZDC_PASSIVE_SEARCH },
	{ Conf::INCOMING_SEARCH_TTH_ONLY, ResourceManager::INCOMING_SEARCH_TTH_ONLY },
	{ Conf::INCOMING_SEARCH_IGNORE_BOTS, ResourceManager::INCOMING_SEARCH_IGNORE_BOTS },
	{ Conf::INCOMING_SEARCH_IGNORE_PASSIVE, ResourceManager::INCOMING_SEARCH_IGNORE_PASSIVE },
	{ 0, ResourceManager::Strings()}
};

LRESULT SearchPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_ADVANCED_BOOLEANS));

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, listItems, ctrlList);
	const auto* ss = SettingsManager::instance.getUiSettings();
	CButton(GetDlgItem(IDC_SAVE_SEARCH)).SetCheck(ss->getBool(Conf::FORGET_SEARCH_REQUEST) ? BST_UNCHECKED : BST_CHECKED);

	CUpDownCtrl spin1(GetDlgItem(IDC_SEARCH_HISTORY_SPIN));
	spin1.SetRange32(1, 100);
	spin1.SetBuddy(GetDlgItem(IDC_SEARCH_HISTORY));

	CUpDownCtrl spin2(GetDlgItem(IDC_INTERVAL_SPIN));
	spin2.SetRange32(1, 999);
	spin2.SetBuddy(GetDlgItem(IDC_INTERVAL));

	CUpDownCtrl spin3(GetDlgItem(IDC_MATCH_SPIN));
	spin3.SetRange32(1, 999);
	spin3.SetBuddy(GetDlgItem(IDC_MATCH));

	CUpDownCtrl spin4(GetDlgItem(IDC_AUTO_SEARCH_LIMIT_SPIN));
	spin4.SetRange32(1, 999);
	spin4.SetBuddy(GetDlgItem(IDC_AUTO_SEARCH_LIMIT));

	fixControls();
	return TRUE;
}

void SearchPage::write()
{
	PropPage::write(*this, items, listItems, ctrlList);
	bool state = IsDlgButtonChecked(IDC_SAVE_SEARCH) == BST_CHECKED;
	auto ss = SettingsManager::instance.getUiSettings();
	ss->setBool(Conf::FORGET_SEARCH_REQUEST, !state);
}

LRESULT SearchPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void SearchPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_SAVE_SEARCH) == BST_CHECKED;
	GetDlgItem(IDC_SETTINGS_SEARCH_HISTORY).EnableWindow(state);
	GetDlgItem(IDC_SEARCH_HISTORY).EnableWindow(state);
	GetDlgItem(IDC_SEARCH_HISTORY_SPIN).EnableWindow(state);
}
