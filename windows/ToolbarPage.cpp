/*
 * Copyright (C) 2003 Twink,  spm7@waikato.ac.nz
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
#include "ToolbarPage.h"
#include "Toolbar.h"
#include "WinUtil.h"
#include "MainFrm.h"
#include "DialogLayout.h"
#include "../client/SimpleStringTokenizer.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 1, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_ICON_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_ICON_SIZE, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_TOOLBAR_ADD, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_TOOLBAR_REMOVE, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

static string filter(const string& s)
{
	string res = s;
	auto i = res.find('\t');
	if (i != string::npos) res.erase(i);
	i = res.find('&');
	if (i != string::npos) res.erase(i, 1);
	return res;
}

LRESULT ToolbarPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	ctrlIconSize.Attach(GetDlgItem(IDC_ICON_SIZE));
	ctrlIconSize.AddString(CTSTRING(SETTINGS_TOOLBAR_SMALL));
	ctrlIconSize.AddString(CTSTRING(SETTINGS_TOOLBAR_LARGE));
	int iconSize = SETTING(TB_IMAGE_SIZE);
	ctrlIconSize.SetCurSel(iconSize == 16 ? 0 : 1);

	auto mainFrame = MainFrame::getMainFrame();
	ctrlCommands.Attach(GetDlgItem(IDC_TOOLBAR_POSSIBLE));
	CRect rc;
	ctrlCommands.GetClientRect(rc);
	ctrlCommands.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	ctrlCommands.SetImageList(mainFrame->getToolbarImages(), LVSIL_SMALL);
	WinUtil::setExplorerTheme(ctrlCommands);

	tstring tmp;
	LVITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	lvi.iSubItem = 0;

	int n = 0;
	for (int i = -1; i < 0 || g_ToolbarButtons[i].id != 0; i++)
	{
		if (g_ToolbarButtons[i].id == -1) continue;
		makeItem(&lvi, i, tmp);
		lvi.iItem = n++;
		ctrlCommands.InsertItem(&lvi);
		ctrlCommands.SetItemData(lvi.iItem, i);
	}
	ctrlCommands.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);

	ctrlToolbar.Attach(GetDlgItem(IDC_TOOLBAR_ACTUAL));
	ctrlToolbar.GetClientRect(rc);
	ctrlToolbar.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	ctrlToolbar.SetImageList(mainFrame->getToolbarHotImages(), LVSIL_SMALL);
	WinUtil::setExplorerTheme(ctrlToolbar);

	n = 0;
	LVFINDINFO lvifi = { 0 };
	lvifi.flags  = LVFI_PARAM;
	SimpleStringTokenizer<char> st(SETTING(TOOLBAR), ',');
	string token;
	while (st.getNextToken(token))
	{
		int i = Util::toInt(token);
		if (i == -1 || (i >= 0 && i < g_ToolbarButtonsCount && g_ToolbarButtons[i].id != -1))
		{
			makeItem(&lvi, i, tmp);
			lvi.iItem = n++;
			ctrlToolbar.InsertItem(&lvi);
			ctrlToolbar.SetItemData(lvi.iItem, i);
			lvifi.lParam = i;
			int foundIndex = ctrlCommands.FindItem(&lvifi, -1);
			if (foundIndex != -1)
				ctrlCommands.SetItemState(foundIndex, LVIS_CUT, LVIS_CUT);
		}
	}
	
	ctrlToolbar.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	return TRUE;
}

void ToolbarPage::write()
{
	int iconSize = 16 + ctrlIconSize.GetCurSel() * 8;

	string toolbar;
	int count = ctrlToolbar.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		if (i) toolbar += ',';
		int j = ctrlToolbar.GetItemData(i);
		toolbar += Util::toString(j);
	}
	if (toolbar != g_settings->get(SettingsManager::TOOLBAR) || iconSize != g_settings->get(SettingsManager::TB_IMAGE_SIZE))
	{
		g_settings->set(SettingsManager::TOOLBAR, toolbar);
		g_settings->set(SettingsManager::TB_IMAGE_SIZE, iconSize);
		dcassert(WinUtil::g_mainWnd);
		if (WinUtil::g_mainWnd)
			::SendMessage(WinUtil::g_mainWnd, IDC_REBUILD_TOOLBAR, 0, 0);
	}
}

void ToolbarPage::makeItem(LVITEM* lvi, int item, tstring& tmp)
{
	if (item > -1 && item < g_ToolbarButtonsCount)
	{
		lvi->iImage = item;
		tmp = Text::toT(filter(ResourceManager::getString(g_ToolbarButtons[item].tooltip)));
	}
	else
	{
		tmp = TSTRING(SEPARATOR);
		lvi->iImage = -1;
	}
	lvi->pszText = const_cast<TCHAR*>(tmp.c_str());
}

LRESULT ToolbarPage::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int selectedIndex = ctrlCommands.GetSelectedIndex();
		int foundIndex = -1;
		int id = 0;

		if (selectedIndex != 0)
		{
			id = ctrlCommands.GetItemData(selectedIndex);
			LVFINDINFO lvifi = { 0 };
			lvifi.flags  = LVFI_PARAM;
			lvifi.lParam = id;
			foundIndex = ctrlToolbar.FindItem(&lvifi, -1);
			if (foundIndex != -1)
			{
				// item already in toolbar,
				// don't add new item, but hilite old one
				ctrlToolbar.SetItemState(foundIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				ctrlToolbar.EnsureVisible(foundIndex, FALSE);
			}
		}

		if (foundIndex == -1)
		{
			// add new item to toolbar
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_TEXT | LVIF_IMAGE;
			lvi.iSubItem = 0;
			tstring tmp;
			makeItem(&lvi, id, tmp);
			lvi.iItem = ctrlToolbar.GetSelectedIndex() + 1;
			ctrlToolbar.InsertItem(&lvi);
			ctrlToolbar.SetItemData(lvi.iItem, id);
			ctrlCommands.SetItemState(selectedIndex, LVIS_CUT, LVIS_CUT);
		}
	}
	return 0;
}

LRESULT ToolbarPage::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlToolbar.GetSelectedCount() == 1)
	{
		const int sel = ctrlToolbar.GetSelectedIndex();
		const int ind = ctrlToolbar.GetItemData(sel);
		ctrlToolbar.DeleteItem(sel);
		ctrlToolbar.SelectItem(max(sel - 1, 0));
		ctrlCommands.SetItemState(ind + 1, 0, LVIS_CUT);
	}
	return 0;
}
