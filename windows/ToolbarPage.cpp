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
#include "../client/SimpleStringTokenizer.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_MOUSE_OVER, ResourceManager::SETTINGS_MOUSE_OVER },
	{ IDC_NORMAL, ResourceManager::SETTINGS_NORMAL },
	{ IDC_TOOLBAR_IMAGE_BOX, ResourceManager::SETTINGS_TOOLBAR_IMAGE },
	{ IDC_TOOLBAR_ADD, ResourceManager::SETTINGS_TOOLBAR_ADD },
	{ IDC_TOOLBAR_REMOVE, ResourceManager::SETTINGS_TOOLBAR_REMOVE },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_TOOLBAR_IMAGE, SettingsManager::TOOLBARIMAGE, PropPage::T_STR },
	{ IDC_TOOLBAR_HOT_IMAGE, SettingsManager::TOOLBARHOTIMAGE, PropPage::T_STR },
	{ IDC_ICON_SIZE, SettingsManager::TB_IMAGE_SIZE, PropPage::T_INT },
	{ IDC_ICON_SIZE_HOVER, SettingsManager::TB_IMAGE_SIZE_HOT, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static string filter(const string& s)
{
	string::size_type i = s.find('\t');
	if (i != string::npos) return s.substr(0, i);
	return s;
}

LRESULT ToolbarPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WinUtil::translate((HWND)(*this), texts);
	PropPage::read(*this, items);
	
	auto mainFrame = MainFrame::getMainFrame();
	ctrlCommands.Attach(GetDlgItem(IDC_TOOLBAR_POSSIBLE));
	CRect rc;
	ctrlCommands.GetClientRect(rc);
	ctrlCommands.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	ctrlCommands.SetImageList(mainFrame->getToolbarImages(), LVSIL_SMALL);
	WinUtil::setExplorerTheme(ctrlCommands);
	
	tstring tmp;
	LVITEM lvi = {0};
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	lvi.iSubItem = 0;
	
	for (int i = -1; i < 0 || g_ToolbarButtons[i].id != 0; i++)
	{
		makeItem(&lvi, i, tmp);
		lvi.iItem = i + 1;
		ctrlCommands.InsertItem(&lvi);
		ctrlCommands.SetItemData(lvi.iItem, i);
	}
	ctrlCommands.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	
	ctrlToolbar.Attach(GetDlgItem(IDC_TOOLBAR_ACTUAL));
	ctrlToolbar.GetClientRect(rc);
	ctrlToolbar.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	ctrlToolbar.SetImageList(mainFrame->getToolbarHotImages(), LVSIL_SMALL);
	WinUtil::setExplorerTheme(ctrlToolbar);
	
	SimpleStringTokenizer<char> st(SETTING(TOOLBAR), ',');
	int n = 0;
	string token;
	while (st.getNextToken(token))
	{
		int i = Util::toInt(token);
		if (i < g_ToolbarButtonsCount)
		{
			makeItem(&lvi, i, tmp);
			lvi.iItem = n++;
			ctrlToolbar.InsertItem(&lvi);
			ctrlToolbar.SetItemData(lvi.iItem, i);
			
			// disable items that are already in toolbar,
			// to avoid duplicates
			if (i != -1)
				ctrlCommands.SetItemState(i + 1, LVIS_CUT, LVIS_CUT);
		}
	}
	
	ctrlToolbar.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	return TRUE;
}

void ToolbarPage::write()
{
	PropPage::write(*this, items);
	string toolbar;
	for (int i = 0; i < ctrlToolbar.GetItemCount(); i++)
	{
		if (i) toolbar += ',';
		const int j = ctrlToolbar.GetItemData(i);
		toolbar += Util::toString(j);
	}
	if (toolbar != g_settings->get(SettingsManager::TOOLBAR))
	{
		g_settings->set(SettingsManager::TOOLBAR, toolbar);
		dcassert(WinUtil::g_mainWnd);
		if (WinUtil::g_mainWnd)
			::SendMessage(WinUtil::g_mainWnd, IDC_REBUILD_TOOLBAR, 0, 0);
	}
}

void ToolbarPage::browseForPic(int dlgItem)
{
	CEdit edit(GetDlgItem(dlgItem));
	tstring x;
	WinUtil::getWindowText(edit, x);
	if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
		edit.SetWindowText(x.c_str());
}

LRESULT ToolbarPage::onImageBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	browseForPic(IDC_TOOLBAR_IMAGE);
	return 0;
}

LRESULT ToolbarPage::onHotBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	browseForPic(IDC_TOOLBAR_HOT_IMAGE);
	return 0;
}

void ToolbarPage::makeItem(LVITEM* lvi, int item, tstring& tmp)
{
	if (item > -1 && item < g_ToolbarButtonsCount) // [!] Идентификаторы отображаемых иконок: первый (-1 - разделитель) и последний (кол-во иконок начиная с 0) для панели инструментов.
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
		int iSelectedInd = ctrlCommands.GetSelectedIndex();
		bool bAlreadyExist = false;
		
		if (iSelectedInd != 0)
		{
			LVFINDINFO lvifi = { 0 };
			lvifi.flags  = LVFI_PARAM;
			lvifi.lParam = iSelectedInd - 1;
			const int iFoundInd = ctrlToolbar.FindItem(&lvifi, -1);
			
			if (iFoundInd != -1)
			{
				// item already in toolbar,
				// don't add new item, but hilite old one
				ctrlToolbar.SetItemState(iFoundInd, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				bAlreadyExist = true;
			}
		}
		
		if (!bAlreadyExist)
		{
			// add new item to toolbar
			
			LVITEM lvi = {0};
			lvi.mask = LVIF_TEXT | LVIF_IMAGE;
			lvi.iSubItem = 0;
			const int i = ctrlCommands.GetItemData(iSelectedInd);
			tstring tmp;
			makeItem(&lvi, i, tmp);
			lvi.iItem = ctrlToolbar.GetSelectedIndex() + 1;//ctrlToolbar.GetSelectedIndex()>0?ctrlToolbar.GetSelectedIndex():ctrlToolbar.GetItemCount();
			ctrlToolbar.InsertItem(&lvi);
			ctrlToolbar.SetItemData(lvi.iItem, i);
			ctrlCommands.SetItemState(i + 1, LVIS_CUT, LVIS_CUT);
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
