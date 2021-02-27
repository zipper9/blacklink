/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "FavoriteDirsPage.h"
#include "FavDirDlg.h"
#include "WinUtil.h"
#include "../client/Util.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_REMOVE, ResourceManager::REMOVE },
	{ IDC_ADD, ResourceManager::SETTINGS_ADD_FOLDER },
	{ IDC_CHANGE, ResourceManager::EDIT_ACCEL },
	{ 0, ResourceManager::Strings() }
};

LRESULT FavoriteDirsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	ctrlDirectories.Attach(GetDlgItem(IDC_FAVORITE_DIRECTORIES));
	ctrlDirectories.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	SET_LIST_COLOR_IN_SETTING(ctrlDirectories);
	WinUtil::setExplorerTheme(ctrlDirectories);
	
	CRect rc;
	ctrlDirectories.GetClientRect(rc);
	ctrlDirectories.InsertColumn(0, CTSTRING(FAVORITE_DIR_NAME), LVCFMT_LEFT, rc.Width() / 4, 0);
	ctrlDirectories.InsertColumn(1, CTSTRING(DIRECTORY), LVCFMT_LEFT, rc.Width() * 2 / 4, 1);
	ctrlDirectories.InsertColumn(2, CTSTRING(SETTINGS_EXTENSIONS), LVCFMT_LEFT, rc.Width() / 4, 2);

	TStringList sl;
	sl.resize(3);
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& directories = lockedInstance.getFavoriteDirs();
	for (const auto& d : directories)
	{
		sl[0] = Text::toT(d.name);
		sl[1] = Text::toT(d.dir);
		sl[2] = Text::toT(Util::toString(';', d.ext));
		ctrlDirectories.insert(sl);
	}
	
	return TRUE;
}

void FavoriteDirsPage::write() { }

LRESULT FavoriteDirsPage::onDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HDROP drop = (HDROP)wParam;
	unique_ptr<TCHAR[]> buf(new TCHAR[FULL_MAX_PATH]);
	UINT nrFiles;
	
	nrFiles = DragQueryFile(drop, (UINT) - 1, NULL, 0);
	
	for (UINT i = 0; i < nrFiles; ++i)
	{
		if (DragQueryFile(drop, i, buf.get(), FULL_MAX_PATH))
		{
			if (PathIsDirectory(buf.get()))
				addDirectory(buf.get());
		}
	}
	
	DragFinish(drop);
	return 0;
}

void FavoriteDirsPage::updateButtons()
{
	int count = ctrlDirectories.GetSelectedCount();
	GetDlgItem(IDC_REMOVE).EnableWindow(count != 0);
	GetDlgItem(IDC_CHANGE).EnableWindow(count == 1);
}

LRESULT FavoriteDirsPage::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	updateButtons();
	return 0;
}

LRESULT FavoriteDirsPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT FavoriteDirsPage::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	
	if (item->iItem >= 0)
		PostMessage(WM_COMMAND, IDC_CHANGE, 0);
	else
		PostMessage(WM_COMMAND, IDC_ADD, 0);
	
	return 0;
}

LRESULT FavoriteDirsPage::onClickedAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	addDirectory();
	return 0;
}

LRESULT FavoriteDirsPage::onClickedRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlDirectories.GetSelectedCount())
	{
		TCHAR buf[MAX_PATH];
		LVITEM item = {0};
		item.mask = LVIF_TEXT;
		item.cchTextMax = _countof(buf);
		item.pszText = buf;
		
		int i = -1;
		while ((i = ctrlDirectories.GetNextItem(-1, LVNI_SELECTED)) != -1)
		{
			item.iItem = i;
			ctrlDirectories.GetItem(&item);
			if (FavoriteManager::getInstance()->removeFavoriteDir(Text::fromT(buf)))
				ctrlDirectories.DeleteItem(i);
			else
				break;
		}
		updateButtons();
	}
	return 0;
}

LRESULT FavoriteDirsPage::onClickedChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = ctrlDirectories.GetNextItem(-1, LVNI_SELECTED);
	if (i != -1)
	{
		TCHAR buf[MAX_PATH];
		LVITEM item = { 0 };
		item.mask = LVIF_TEXT;
		item.cchTextMax = _countof(buf);
		item.pszText = buf;

		item.iItem = i;
		FavDirDlg dlg(false);
		ctrlDirectories.GetItem(&item);
		dlg.name = buf;
		string oldName = Text::fromT(dlg.name);
		item.iSubItem = 1;
		ctrlDirectories.GetItem(&item);
		dlg.dir = buf;
		item.iSubItem = 2;
		ctrlDirectories.GetItem(&item);
		dlg.extensions = buf;
		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			Util::appendPathSeparator(dlg.dir);
			if (FavoriteManager::getInstance()->updateFavoriteDir(oldName, Text::fromT(dlg.name), Text::fromT(dlg.dir), Text::fromT(dlg.extensions)))
			{
				ctrlDirectories.SetItemText(i, 0, dlg.name.c_str());
				ctrlDirectories.SetItemText(i, 1, dlg.dir.c_str());
				ctrlDirectories.SetItemText(i, 2, dlg.extensions.c_str());
			}
			else
			{
				MessageBox(CTSTRING(DIRECTORY_ADD_ERROR), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
			}
		}
	}
	return 0;
}

void FavoriteDirsPage::addDirectory(const tstring& aPath /*= Util::emptyStringT*/)
{
	tstring path = aPath;
	Util::appendPathSeparator(path);
	
	FavDirDlg dlg(true);
	dlg.name = Util::getLastDir(path);
	dlg.dir = path;
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		Util::appendPathSeparator(dlg.dir);		
		if (FavoriteManager::getInstance()->addFavoriteDir(Text::fromT(dlg.dir), Text::fromT(dlg.name), Text::fromT(dlg.extensions)))
		{
			int j = ctrlDirectories.insert(ctrlDirectories.GetItemCount(), dlg.name);
			ctrlDirectories.SetItemText(j, 1, dlg.dir.c_str());
			ctrlDirectories.SetItemText(j, 2, dlg.extensions.c_str());
		}
		else
		{
			MessageBox(CTSTRING(DIRECTORY_ADD_ERROR), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
		}
	}
}
