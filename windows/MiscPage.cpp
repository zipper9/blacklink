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
#include "Resource.h"
#include "MiscPage.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_ADV_MISC, ResourceManager::SETTINGS_EXPERTS_ONLY },
	{ IDC_PSR_DELAY_STR, ResourceManager::PSR_DELAY },
	{ IDC_THOLD_STR, ResourceManager::THRESHOLD },
	{ IDC_IGNORE_ADD, ResourceManager::ADD },
	{ IDC_IGNORE_REMOVE, ResourceManager::REMOVE },
	{ IDC_IGNORE_CLEAR, ResourceManager::IGNORE_CLEAR },
	{ IDC_MISC_IGNORE, ResourceManager::IGNORED_USERS },
	{ IDC_RAW_TEXTS, ResourceManager::RAW_TEXTS },	

	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_PSR_DELAY, SettingsManager::PSR_DELAY, PropPage::T_INT },
	{ IDC_THOLD, SettingsManager::USER_THERSHOLD, PropPage::T_INT },
	
	{ IDC_RAW1_TEXT, SettingsManager::RAW1_TEXT, PropPage::T_STR },
	{ IDC_RAW2_TEXT, SettingsManager::RAW2_TEXT, PropPage::T_STR },
	{ IDC_RAW3_TEXT, SettingsManager::RAW3_TEXT, PropPage::T_STR },
	{ IDC_RAW4_TEXT, SettingsManager::RAW4_TEXT, PropPage::T_STR },
	{ IDC_RAW5_TEXT, SettingsManager::RAW5_TEXT, PropPage::T_STR },
	
	{ 0, 0, PropPage::T_END }
};

LRESULT MiscPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	
	CRect rc;
	UserManager::getIgnoreList(ignoreList);
	ignoreListCtrl.Attach(GetDlgItem(IDC_IGNORELIST));
	ignoreListCtrl.GetClientRect(rc);
	ignoreListCtrl.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, (rc.Width() - 17), 0);
	setListViewExtStyle(ignoreListCtrl, BOOLSETTING(SHOW_GRIDLINES), false);
	SET_LIST_COLOR_IN_SETTING(ignoreListCtrl);
	
	auto cnt = ignoreListCtrl.GetItemCount();
	for (auto i = ignoreList.cbegin(); i != ignoreList.cend(); ++i)
	{
		ignoreListCtrl.insert(cnt++, Text::toT(*i));
	}
	
	return TRUE;
}

LRESULT MiscPage::onIgnoreAdd(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	tstring buf;
	WinUtil::getWindowText(GetDlgItem(IDC_IGNORELIST_EDIT), buf);
	if (!buf.empty())
	{
		auto p = ignoreList.insert(Text::fromT(buf));
		if (p.second)
		{
			ignoreListCtrl.insert(ignoreListCtrl.GetItemCount(), buf);
			ignoreListChanged = true;
		}
		else
		{
			MessageBox(CTSTRING(ALREADY_IGNORED), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK);
		}
	}
	SetDlgItemText(IDC_IGNORELIST_EDIT, _T(""));
	return 0;
}

LRESULT MiscPage::onIgnoreRemove(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	int i = -1;
	while ((i = ignoreListCtrl.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		ignoreList.erase(ignoreListCtrl.ExGetItemText(i, 0));
		ignoreListCtrl.DeleteItem(i);
		ignoreListChanged = true;
	}
	return 0;
}

LRESULT MiscPage::onIgnoreClear(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	if (!ignoreList.empty())
	{
		ignoreList.clear();
		ignoreListCtrl.DeleteAllItems();
		ignoreListChanged = true;
	}
	return 0;
}

void MiscPage::write()
{
	PropPage::write(*this, items);
	
	if (ignoreListChanged)
	{
		UserManager::setIgnoreList(ignoreList);
		ignoreListChanged = false;
	}
	/*if(SETTING(PSR_DELAY) < 5)
	    g_settings->set(SettingsManager::PSR_DELAY, 5);*/
}
