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

#include "../client/SettingsManager.h"
#include "AVIPreviewPage.h"
#include "PreviewDlg.h"
#include "WinUtil.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_ADD_MENU,    ResourceManager::ADD        },
	{ IDC_CHANGE_MENU, ResourceManager::EDIT_ACCEL },
	{ IDC_REMOVE_MENU, ResourceManager::REMOVE     },
	{ 0,               ResourceManager::Strings()  }
};

LRESULT AVIPreview::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WinUtil::translate(*this, texts);
	
	CRect rc;
	
	ctrlCommands.Attach(GetDlgItem(IDC_MENU_ITEMS));
	ctrlCommands.GetClientRect(rc);
	
	ctrlCommands.InsertColumn(0, CTSTRING(SETTINGS_NAME), LVCFMT_LEFT, rc.Width() / 5, 0);
	ctrlCommands.InsertColumn(1, CTSTRING(SETTINGS_COMMAND), LVCFMT_LEFT, rc.Width() * 2 / 5, 1);
	ctrlCommands.InsertColumn(2, CTSTRING(SETTINGS_ARGUMENT), LVCFMT_LEFT, rc.Width() / 5, 2);
	ctrlCommands.InsertColumn(3, CTSTRING(SETTINGS_EXTENSIONS), LVCFMT_LEFT, rc.Width() / 5, 3);
	
	ctrlCommands.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlCommands);
	
	const auto& lst = FavoriteManager::getInstance()->getPreviewApps();
	int cnt = ctrlCommands.GetItemCount();
	for (auto i = lst.cbegin(); i != lst.cend(); ++i)
		addEntry(*i, cnt++);
	checkMenu();
	return 0;
}

void AVIPreview::addEntry(PreviewApplication* pa, int pos)
{
	TStringList lst;
	lst.push_back(Text::toT(pa->name));
	lst.push_back(Text::toT(pa->application));
	lst.push_back(Text::toT(pa->arguments));
	lst.push_back(Text::toT(pa->extension));
	ctrlCommands.insert(pos, lst, 0, 0);
}

void AVIPreview::checkMenu()
{
	BOOL enable = (ctrlCommands.GetItemCount() > 0 && ctrlCommands.GetSelectedCount() == 1) ? TRUE : FALSE;
	::EnableWindow(GetDlgItem(IDC_CHANGE_MENU), enable);
	::EnableWindow(GetDlgItem(IDC_REMOVE_MENU), enable);
}

LRESULT AVIPreview::onAddMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PreviewDlg dlg(true);
	if (dlg.DoModal() == IDOK)
	{
		addEntry(FavoriteManager::getInstance()->addPreviewApp(
			Text::fromT(dlg.name),
		    Text::fromT(dlg.application),
		    Text::fromT(dlg.arguments),
		    Text::fromT(dlg.extensions)), ctrlCommands.GetItemCount());
	}
	checkMenu();
	return 0;
}

LRESULT AVIPreview::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	//NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	checkMenu();
	//::EnableWindow(GetDlgItem(IDC_CHANGE_MENU), (lv->uNewState & LVIS_FOCUSED));
	//::EnableWindow(GetDlgItem(IDC_REMOVE_MENU), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT AVIPreview::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD_MENU, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE_MENU, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT AVIPreview::onChangeMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int sel = ctrlCommands.GetSelectedIndex();
		PreviewApplication* pa = FavoriteManager::getInstance()->getPreviewApp(sel);
		if (pa)
		{
			PreviewDlg dlg(false);
			dlg.name = Text::toT(pa->name);
			dlg.application = Text::toT(pa->application);
			dlg.arguments = Text::toT(pa->arguments);
			dlg.extensions = Text::toT(pa->extension);
			
			if (dlg.DoModal() == IDOK)
			{
				pa->name = Text::fromT(dlg.name);
				pa->application = Text::fromT(dlg.application);
				pa->arguments = Text::fromT(dlg.arguments);
				pa->extension = Text::fromT(dlg.extensions);
				
				ctrlCommands.SetItemText(sel, 0, dlg.name.c_str());
				ctrlCommands.SetItemText(sel, 1, dlg.application.c_str());
				ctrlCommands.SetItemText(sel, 2, dlg.arguments.c_str());
				ctrlCommands.SetItemText(sel, 3, dlg.extensions.c_str());
			}
		}
	}
	
	return 0;
}

LRESULT AVIPreview::onRemoveMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int sel = ctrlCommands.GetSelectedIndex();
		FavoriteManager::getInstance()->removePreviewApp(sel);
		ctrlCommands.DeleteItem(sel);
	}
	checkMenu();
	return 0;
}