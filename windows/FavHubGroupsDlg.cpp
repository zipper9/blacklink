/*
 * Copyright (C) 2007-2013 adrian_007, adrian-007 on o2 point pl
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
#include "FavHubGroupsDlg.h"
#include "LineDlg.h"
#include "WinUtil.h"
#include "ExMessageBox.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_ADD,    ResourceManager::ADD        },
	{ IDC_EDIT,   ResourceManager::EDIT_ACCEL },
	{ IDC_REMOVE, ResourceManager::REMOVE     },
	{ IDCANCEL,   ResourceManager::CLOSE      },
	{ 0,          ResourceManager::Strings()  }
};

LRESULT FavHubGroupsDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::FAVORITES, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	ctrlGroups.Attach(GetDlgItem(IDC_GROUPS));

	CRect rc;
	ctrlGroups.GetClientRect(rc);
	int width = rc.Width() - 20; // for scroll

	SetWindowText(CTSTRING(MANAGE_GROUPS));
	WinUtil::translate(*this, texts);

	int nameWidth = WinUtil::percent(width, 70);
	ctrlGroups.InsertColumn(0, CTSTRING(NAME), LVCFMT_LEFT, nameWidth, 0);
	ctrlGroups.InsertColumn(1, CTSTRING(GROUPS_PRIVATE), LVCFMT_LEFT, width - nameWidth, 0);
	ctrlGroups.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	setListViewColors(ctrlGroups);
	WinUtil::setExplorerTheme(ctrlGroups);
	
	{
		FavoriteManager::LockInstanceHubs lock(FavoriteManager::getInstance(), false);
		const FavHubGroups& groups = lock.getFavHubGroups();
		for (auto i = groups.cbegin(); i != groups.cend(); ++i)
			addItem(Text::toT(i->first), i->second.priv);
	}
	updateSelectedGroup();
	return 0;
}

LRESULT FavHubGroupsDlg::onClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	FavoriteManager::getInstance()->saveFavorites();
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT FavHubGroupsDlg::onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	updateSelectedGroup();
	return 0;
}

void FavHubGroupsDlg::addItem(const tstring& name, bool priv, bool select /*= false*/)
{
	int item = ctrlGroups.InsertItem(ctrlGroups.GetItemCount(), name.c_str());
	ctrlGroups.SetItemText(item, 1, priv ? CTSTRING(YES) : CTSTRING(NO));
	if (select)
		ctrlGroups.SelectItem(item);
}

tstring FavHubGroupsDlg::getText(int column, int item /*= -1*/)
{
	if (item == -1)
		item = ctrlGroups.GetSelectedIndex();
	if (item >= 0)
		return ctrlGroups.ExGetItemTextT(item, column);
	return Util::emptyStringT;
}

void FavHubGroupsDlg::updateSelectedGroup()
{
	BOOL enableButtons = ctrlGroups.GetSelectedIndex() != -1;
	GetDlgItem(IDC_REMOVE).EnableWindow(enableButtons);
	GetDlgItem(IDC_EDIT).EnableWindow(enableButtons);
}

LRESULT FavHubGroupsDlg::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LineDlg dlg;
	dlg.title = TSTRING(GROUPS_PROPERTIES_FIELD);
	dlg.description = TSTRING(GROUPS_ADD_NEW);
	dlg.checkBox = true; 
	dlg.checked = false;
	dlg.checkBoxText = ResourceManager::GROUPS_PRIVATE_CHECKBOX;
	dlg.allowEmpty = false;
	dlg.icon = IconBitmaps::FAVORITES;
	if (dlg.DoModal(m_hWnd) != IDOK) return 0;
	if (!FavoriteManager::getInstance()->addFavHubGroup(Text::fromT(dlg.line), dlg.checked))
	{
		MessageBox(CTSTRING_F(GROUPS_GROUP_ALREADY_EXISTS, dlg.line), getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
		return 0;
	}
	addItem(dlg.line, dlg.checked, true);
	updateSelectedGroup();
	return 0;
}

static bool changeHubsGroup(const string& oldName, const string& newName)
{
	bool result = false;
	FavoriteManager::LockInstanceHubs lock(FavoriteManager::getInstance(), true);
	auto& hubs = lock.getFavoriteHubs();
	for (FavoriteHubEntry* hub : hubs)
		if (hub->getGroup() == oldName)
		{
			hub->setGroup(newName);
			result = true;
		}
	return result;
}

LRESULT FavHubGroupsDlg::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int pos = ctrlGroups.GetSelectedIndex();
	if (pos < 0) return 0;

	if (BOOLSETTING(CONFIRM_HUBGROUP_REMOVAL))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return 0;
		if (checkState == BST_CHECKED) SET_SETTING(CONFIRM_HUBGROUP_REMOVAL, FALSE);
	}

	tstring nameT = getText(0, pos);
	string name = Text::fromT(nameT);
	std::vector<int> hubIds;
	auto fm = FavoriteManager::getInstance();
	{
		FavoriteManager::LockInstanceHubs lock(fm, false);
		const auto& hubs = lock.getFavoriteHubs();
		for (FavoriteHubEntry* hub : hubs)
			if (hub->getGroup() == name)
				hubIds.push_back(hub->getID());
	}
	bool save = false;
	if (!hubIds.empty())
	{
		int remove = MessageBox(CTSTRING_F(GROUPS_REMOVENOTIFY_FMT, nameT % hubIds.size()),
			CTSTRING(GROUPS_REMOVEGROUP), MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON1);
		switch (remove)
		{
			case IDCANCEL:
				return 0;
			case IDYES:
				for (int id : hubIds)
					if (fm->removeFavoriteHub(id, false))
						save = true;
				break;
			case IDNO:
				save = changeHubsGroup(name, Util::emptyString);
		}
	}
	fm->removeFavHubGroup(name);
	if (save) fm->saveFavorites();
	ctrlGroups.DeleteItem(pos);
	updateSelectedGroup();
	return 0;
}

LRESULT FavHubGroupsDlg::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int item = ctrlGroups.GetSelectedIndex();
	if (item < 0) return 0;

	LineDlg dlg;
	dlg.title = TSTRING(GROUPS_PROPERTIES_FIELD);
	dlg.description = TSTRING(GROUPS_EDIT);
	dlg.line = getText(0, item);
	dlg.checkBox = true; 
	dlg.checked = getText(1, item) == TSTRING(YES);
	dlg.checkBoxText = ResourceManager::GROUPS_PRIVATE_CHECKBOX;
	dlg.allowEmpty = false;
	dlg.icon = IconBitmaps::FAVORITES;
	string oldName = Text::fromT(dlg.line);
	if (dlg.DoModal(m_hWnd) != IDOK) return 0;
	string newName = Text::fromT(dlg.line);
	if (!FavoriteManager::getInstance()->updateFavHubGroup(oldName, newName, dlg.checked))
	{
		MessageBox(CTSTRING_F(GROUPS_GROUP_ALREADY_EXISTS, dlg.line), getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
		return 0;
	}
	ctrlGroups.SetItemText(item, 0, dlg.line.c_str());
	ctrlGroups.SetItemText(item, 1, dlg.checked ? CTSTRING(YES) : CTSTRING(NO));
	changeHubsGroup(oldName, newName);
	return 0;
}

LRESULT FavHubGroupsDlg::onDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = reinterpret_cast<NMITEMACTIVATE*>(pnmh);
	if (item->iItem >= 0)
		PostMessage(WM_COMMAND, IDC_EDIT, 0);
	else if (item->iItem == -1)
		PostMessage(WM_COMMAND, IDC_ADD, 0);
	return 0;
}
