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
#include "WinUtil.h"
#include "ExMessageBox.h"

LRESULT FavHubGroupsDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlGroups.Attach(GetDlgItem(IDC_GROUPS));
	
	uint32_t width;
	{
		CRect rc;
		ctrlGroups.GetClientRect(rc);
		width = rc.Width() - 20; // for scroll
	}
	
	// Translate dialog
	SetWindowText(CTSTRING(MANAGE_GROUPS));
	SetDlgItemText(IDC_ADD, CTSTRING(ADD));
	SetDlgItemText(IDC_REMOVE, CTSTRING(REMOVE));
	SetDlgItemText(IDC_SAVE, CTSTRING(SAVE));
	SetDlgItemText(IDCANCEL, CTSTRING(CLOSE));
	SetDlgItemText(IDC_NAME_STATIC, CTSTRING(NAME));
	
	SetDlgItemText(IDC_PRIVATE, CTSTRING(GROUPS_PRIVATE_CHECKBOX));
	SetDlgItemText(IDC_GROUP_PROPERTIES, CTSTRING(GROUPS_PROPERTIES_FIELD));
	
	ctrlGroups.InsertColumn(0, CTSTRING(NAME), LVCFMT_LEFT, WinUtil::percent(width, 70), 0);
	ctrlGroups.InsertColumn(1, CTSTRING(GROUPS_PRIVATE), LVCFMT_LEFT, WinUtil::percent(width, 15), 0);
	ctrlGroups.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	setListViewColors(ctrlGroups);
	WinUtil::setExplorerTheme(ctrlGroups);
	
	{
		FavoriteManager::LockInstanceHubs lock(FavoriteManager::getInstance(), false);
		const FavHubGroups& groups = lock.getFavHubGroups();
		for (auto i = groups.cbegin(); i != groups.cend(); ++i)
			addItem(Text::toT(i->first), i->second.priv);
	}
	updateSelectedGroup(true);
	return 0;
}

LRESULT FavHubGroupsDlg::onClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	save();
	EndDialog(FALSE);
	return 0;
}

LRESULT FavHubGroupsDlg::onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	updateSelectedGroup();
	return 0;
}

void FavHubGroupsDlg::save()
{
	FavHubGroups groups;
	string name;
	FavHubGroupProperties group;
	
	for (int i = 0; i < ctrlGroups.GetItemCount(); ++i)
	{
		//@todo: fixme
		/*group.first = Text::fromT(getText(0, i));
		group.second.priv = getText(1, i) == _T("Yes");
		group.second.connect = getText(2, i) == _T("Yes");
		groups.insert(group);*/
		name = Text::fromT(getText(0, i));
		group.priv = getText(1, i) == TSTRING(YES);
		groups.insert(make_pair(name, group));
	}
	auto fm = FavoriteManager::getInstance();
	fm->setFavHubGroups(groups);
	fm->saveFavorites();
}

int FavHubGroupsDlg::findGroup(LPCTSTR name)
{
	for (int i = 0; i < ctrlGroups.GetItemCount(); ++i)
	{
		if (wcscmp(ctrlGroups.ExGetItemTextT(i, 0).c_str(), name) == 0)
		{
			return i;
		}
	}
	return -1;
}

void FavHubGroupsDlg::addItem(const tstring& name, bool priv, bool select /*= false*/)
{
	int item = ctrlGroups.InsertItem(ctrlGroups.GetItemCount(), name.c_str());
	ctrlGroups.SetItemText(item, 1, priv ? CTSTRING(YES) : CTSTRING(NO));
	if (select)
		ctrlGroups.SelectItem(item);
}

bool FavHubGroupsDlg::getItem(tstring& name, bool& priv, bool checkSel)
{
	WinUtil::getWindowText(GetDlgItem(IDC_NAME), name);
	if (name.empty())
	{
		MessageBox(CTSTRING(ENTER_GROUP_NAME), CTSTRING(MANAGE_GROUPS), MB_ICONERROR);
		return false;
	}
	else
	{
		int pos = findGroup(name.c_str());
		if (pos != -1 && (checkSel == false || pos != ctrlGroups.GetSelectedIndex()))
		{
			MessageBox(CTSTRING(ITEM_EXIST), CTSTRING(MANAGE_GROUPS), MB_ICONERROR);
			return false;
		}
	}
	
	CButton wnd;
	wnd.Attach(GetDlgItem(IDC_PRIVATE));
	priv = wnd.GetCheck() == 1;
	wnd.Detach();
	
	return true;
}

tstring FavHubGroupsDlg::getText(const int column, const int item /*= -1*/)
{
	const int selection = item == -1 ? ctrlGroups.GetSelectedIndex() : item;
	if (selection >= 0)
	{
		return ctrlGroups.ExGetItemTextT(selection, column);
	}
	return Util::emptyStringT;
}

void FavHubGroupsDlg::updateSelectedGroup(bool forceClean /*= false*/)
{
	tstring name;
	bool priv = false;
	BOOL enableButtons = FALSE;
	
	if (ctrlGroups.GetSelectedIndex() != -1)
	{
		if (!forceClean)
		{
			name = getText(0);
			priv = getText(1) == TSTRING(YES);
		}
		enableButtons = TRUE;
	}
	
	GetDlgItem(IDC_REMOVE).EnableWindow(enableButtons);
	GetDlgItem(IDC_SAVE).EnableWindow(enableButtons);
	SetDlgItemText(IDC_NAME, name.c_str());
	CButton(GetDlgItem(IDC_PRIVATE)).SetCheck(priv ? BST_CHECKED : BST_UNCHECKED);
}

LRESULT FavHubGroupsDlg::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring name;
	bool priv;
	if (getItem(name, priv, false))
	{
		addItem(name, priv, true);
		updateSelectedGroup(true);
	}
	return 0;
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
	if (!hubIds.empty())
	{
		tstring msg;
		msg += TSTRING(GROUPS_GROUP);
		msg += _T(" '") + nameT + _T("' ");
		msg += TSTRING(GROUPS_CONTAINS) + _T(' ');
		msg += Util::toStringT(hubIds.size());
		msg += TSTRING(GROUPS_REMOVENOTIFY);
		int remove = MessageBox(msg.c_str(), CTSTRING(GROUPS_REMOVEGROUP), MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON1);
			
		bool save = false;
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
			{
				FavoriteManager::LockInstanceHubs lock(fm, true);
				auto& hubs = lock.getFavoriteHubs();
				for (FavoriteHubEntry* hub : hubs)
					if (hub->getGroup() == name)
					{
						hub->setGroup(Util::emptyString);
						save = true;
					}
				break;
			}
		}
		if (save)
			fm->saveFavorites();
	}
	ctrlGroups.DeleteItem(pos);
	updateSelectedGroup(true);
	return 0;
}

LRESULT FavHubGroupsDlg::onUpdate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int item = ctrlGroups.GetSelectedIndex();
	if (item >= 0)
	{
		tstring newNameT;
		tstring oldNameT = getText(0);
		bool priv;
		if (getItem(newNameT, priv, true))
		{
			if (oldNameT != newNameT)
			{
				const string oldName = Text::fromT(oldNameT);
				const string newName = Text::fromT(newNameT);
				auto fm = FavoriteManager::getInstance();
				FavoriteManager::LockInstanceHubs lock(fm, true);
				FavoriteHubEntryList& hubs = lock.getFavoriteHubs();
				for (auto i = hubs.begin(); i != hubs.end(); ++i)
				{
					FavoriteHubEntry* fhe = *i;
					if (fhe->getGroup() == oldName)
						fhe->setGroup(newName);
				}
			}
			ctrlGroups.DeleteItem(item);
			addItem(newNameT, priv, true);
			updateSelectedGroup();
		}
	}
	return 0;
}
