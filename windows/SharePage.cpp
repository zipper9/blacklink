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
#include "SharePage.h"
#include "LineDlg.h"
#include "BrowseFile.h"
#include "ConfUI.h"
#include "../client/PathUtil.h"
#include "../client/FormatUtil.h"
#include "../client/ShareManager.h"
#include "../client/ConfCore.h"

#ifdef OSVER_WIN_XP
#include "../client/SysVersion.h"
#endif

enum
{
	GROUP_NORMAL,
	GROUP_EXCLUDED
};

static const WinUtil::TextItem texts[] =
{
	{ IDC_SHOW_TREE, ResourceManager::SETTINGS_USE_SHARE_TREE },
	{ IDC_REMOVE, ResourceManager::REMOVE },
	{ IDC_ADD, ResourceManager::SETTINGS_ADD_FOLDER },
	{ IDC_RENAME, ResourceManager::RENAME },
	{ IDC_SETTINGS_ONLY_HASHED, ResourceManager::SETTINGS_ONLY_HASHED },
	{ IDC_SETTINGS_SKIPLIST, ResourceManager::SETTINGS_SKIPLIST_SHARE },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_SKIPLIST_SHARE, Conf::SKIPLIST_SHARE, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

LRESULT SharePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WinUtil::translate((HWND)(*this), texts);

	PropPage::read(*this, items);
	const auto* ss = SettingsManager::instance.getUiSettings();
	showTree = !ss->getBool(Conf::USE_OLD_SHARING_UI);
	CButton ctrlShowTree(GetDlgItem(IDC_SHOW_TREE));
	ctrlShowTree.SetCheck(showTree ? BST_CHECKED : BST_UNCHECKED);

	listInitialized = false;
	updateTree = updateList = true;
	hasExcludeGroup = false;
	renamingItem = false;

	ctrlDirectories.Attach(GetDlgItem(IDC_DIRECTORIES));
	ctrlDirectories.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlDirectories);
	contDirectories.SubclassWindow(ctrlDirectories);

	ft.SubclassWindow(GetDlgItem(IDC_TREE));
	WinUtil::setExplorerTheme(ft);
	ft.SetOnChangeListener(this);

	ctrlTotalSize.Attach(GetDlgItem(IDC_SETTINGS_SHARE_SIZE));
	ctrlTotalFiles.Attach(GetDlgItem(IDC_SETTINGS_SHARE_FILES));
	toggleView();

	return TRUE;
}

void SharePage::toggleView()
{
	if (showTree)
	{
		if (updateTree)
		{
			ft.PopulateTree();
			updateTree = false;
		}
	}
	else if (!listInitialized || updateList)
	{
		if (!listInitialized)
		{
			ctrlDirectories.EnableGroupView(TRUE);
			insertGroup(GROUP_NORMAL, WSTRING(SETTINGS_SHARED_FOLDERS));
			ctrlDirectories.InsertColumn(0, CTSTRING(VIRTUAL_NAME), LVCFMT_LEFT, 80, 0);
			ctrlDirectories.InsertColumn(1, CTSTRING(DIRECTORY), LVCFMT_LEFT, 197, 1);
			ctrlDirectories.InsertColumn(2, CTSTRING(SIZE), LVCFMT_RIGHT, 90, 2);
			listInitialized = true;
		}
		insertListItems();
		updateList = false;
	}
	
	ctrlDirectories.ShowWindow(showTree ? SW_HIDE : SW_SHOW);
	ft.ShowWindow(showTree ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_ADD).ShowWindow(showTree ? SW_HIDE: SW_SHOW);
	GetDlgItem(IDC_REMOVE).ShowWindow(showTree ? SW_HIDE : SW_SHOW);
	GetDlgItem(IDC_RENAME).ShowWindow(showTree ? SW_HIDE : SW_SHOW);
	showInfo();
}

void SharePage::showInfo()
{
	auto sm = ShareManager::getInstance();
	bool changed = sm->changed();
	tstring str = TSTRING(TOTAL_SIZE) + Util::formatBytesT(sm->getTotalSharedSize());
	if (changed) str += _T('*');
	ctrlTotalSize.SetWindowText(str.c_str());	
	str = TSTRING(TOTAL_FILES) + Util::toStringT(sm->getTotalSharedFiles());
	if (changed) str += _T('*');
	ctrlTotalFiles.SetWindowText(str.c_str());
}

LRESULT SharePage::onDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HDROP drop = (HDROP)wParam;
	unique_ptr<TCHAR[]> buf(new TCHAR[FULL_MAX_PATH]);
	UINT nrFiles;
	
	nrFiles = DragQueryFile(drop, (UINT) -1, NULL, 0);
	
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

void SharePage::write()
{
	PropPage::write(*this, items);
	CButton ctrlShowTree(GetDlgItem(IDC_SHOW_TREE));
	auto ss = SettingsManager::instance.getUiSettings();
	ss->setBool(Conf::USE_OLD_SHARING_UI,
		ctrlShowTree.GetCheck() == BST_CHECKED ? false : true);
}

LRESULT SharePage::onItemChangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (renamingItem) return 0;
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	if (lv->uNewState & LVIS_SELECTED)
	{
		LVITEM item = {};
		item.mask = LVIF_GROUPID;
		item.iItem = lv->iItem;
		ctrlDirectories.GetItem(&item);
		GetDlgItem(IDC_REMOVE).EnableWindow(TRUE);
		GetDlgItem(IDC_RENAME).EnableWindow(item.iGroupId == GROUP_NORMAL);
	}
	else
	{
		GetDlgItem(IDC_REMOVE).EnableWindow(FALSE);
		GetDlgItem(IDC_RENAME).EnableWindow(FALSE);
	}
	return 0;
}

LRESULT SharePage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
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

LRESULT SharePage::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	
	if (item->iItem >= 0)
	{
		PostMessage(WM_COMMAND, IDC_RENAME, 0);
	}
	else if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_ADD, 0);
	}
	
	return 0;
}

LRESULT SharePage::onClickedShowTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	showTree = CButton(hWndCtl).GetCheck() == BST_CHECKED;
	toggleView();
	return 0;
}

LRESULT SharePage::onClickedAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target;
	if (WinUtil::browseDirectory(target, m_hWnd))
	{
		addDirectory(target);
		/*
		if (!HashProgressDlg::g_is_execute)
		{
			HashProgressDlg(true).DoModal();
		}
		*/
	}
	
	return 0;
}

static bool isGroupEmpty(CListViewCtrl& lv, int groupId)
{
#ifdef OSVER_WIN_XP
	int count = lv.GetItemCount();
	LVITEM item = {};
	item.mask = LVIF_GROUPID;
	for (int i = 0; i < count; i++)
	{
		item.iItem = i;
		if (lv.GetItem(&item) && item.iGroupId == groupId) return false;
	}
	return true;
#else
	LVGROUP lg = {};
	lg.cbSize = sizeof(lg);
	lg.mask = LVGF_ITEMS;
	lv.GetGroupInfo(groupId, &lg);
	return lg.cItems == 0;
#endif
}

LRESULT SharePage::onClickedRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = ctrlDirectories.GetNextItem(-1, LVNI_SELECTED);
	if (i < 0) return 0;
	if (MessageBox(CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;

	unique_ptr<TCHAR[]> buf(new TCHAR[FULL_MAX_PATH]);
	LVITEM item = {0};
	item.mask = LVIF_GROUPID;
	item.iItem = i;
	ctrlDirectories.GetItem(&item);
	int groupId = item.iGroupId;
	item.mask = LVIF_TEXT;
	item.cchTextMax = FULL_MAX_PATH;
	item.pszText = buf.get();
	item.iSubItem = 1;
	ctrlDirectories.GetItem(&item);
	if (groupId == GROUP_EXCLUDED)
	{
		ShareManager::getInstance()->removeExcludeFolder(Text::fromT(buf.get()));
		ctrlDirectories.DeleteItem(i);
		if (isGroupEmpty(ctrlDirectories, GROUP_EXCLUDED))
		{
			ctrlDirectories.RemoveGroup(GROUP_EXCLUDED);
			hasExcludeGroup = false;
		}
		GetDlgItem(IDC_REMOVE).EnableWindow(FALSE);
		GetDlgItem(IDC_RENAME).EnableWindow(FALSE);
		showInfo();
		updateTree = true;
		return 0;
	}
	
	ShareManager::getInstance()->removeDirectory(Text::fromT(buf.get()));
	if (hasExcludeGroup)
		insertListItems();
	else
		ctrlDirectories.DeleteItem(i);
	GetDlgItem(IDC_REMOVE).EnableWindow(FALSE);
	GetDlgItem(IDC_RENAME).EnableWindow(FALSE);
	showInfo();
	updateTree = true;	
	return 0;
}

LRESULT SharePage::onClickedRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = ctrlDirectories.GetNextItem(-1, LVNI_SELECTED);
	if (i < 0) return 0;
	
	unique_ptr<TCHAR[]> buf(new TCHAR[FULL_MAX_PATH]);
	LVITEM item = {0};
	item.mask = LVIF_TEXT | LVIF_GROUPID;
	item.cchTextMax = FULL_MAX_PATH;
	item.pszText = buf.get();
	item.iItem = i;
	item.iSubItem = 0;
	ctrlDirectories.GetItem(&item);
	if (item.iGroupId != GROUP_NORMAL) return 0;
	tstring vName = buf.get();
	item.iSubItem = 1;
	ctrlDirectories.GetItem(&item);
	tstring rPath = buf.get();
	try
	{
		LineDlg virt;
		virt.title = TSTRING(VIRTUAL_NAME);
		virt.description = TSTRING(VIRTUAL_NAME_LONG);
		virt.line = vName;
		virt.allowEmpty = false;
		virt.icon = IconBitmaps::FINISHED_UPLOADS;
		if (virt.DoModal(m_hWnd) == IDOK)
		{
			if (stricmp(buf.get(), virt.line.c_str()) != 0)
			{
				ShareManager::getInstance()->renameDirectory(Text::fromT(rPath), Text::fromT(virt.line));
				renamingItem = true;
				ctrlDirectories.SetItemText(i, 0, virt.line.c_str());
				renamingItem = false;
				updateTree = true;
			}
			else
			{
				MessageBox(CTSTRING(SKIP_RENAME), getAppNameVerT().c_str(), MB_ICONINFORMATION | MB_OK);
			}
		}
	}
	catch (const ShareException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
	}
	
	return 0;
}

void SharePage::addDirectory(const tstring& path)
{
	tstring pathTmp = path;
	Util::appendPathSeparator(pathTmp);
	try
	{
		LineDlg virt;
		virt.title = TSTRING(VIRTUAL_NAME);
		virt.description = TSTRING(VIRTUAL_NAME_LONG);
		virt.line = Text::toT(ShareManager::validateVirtual(Util::getLastDir(Text::fromT(pathTmp))));
		virt.allowEmpty = false;
		virt.checkBox = virt.checked = true;
		virt.checkBoxText = ResourceManager::ADD_TO_DEFAULT_SHARE_GROUP;
		virt.icon = IconBitmaps::FINISHED_UPLOADS;
		if (virt.DoModal(m_hWnd) == IDOK)
		{
			CWaitCursor waitCursor;
			ShareManager* sm = ShareManager::getInstance();
			string realPath = Text::fromT(pathTmp);
			sm->addDirectory(realPath, Text::fromT(virt.line));
			insertDirectoryItem(GROUP_NORMAL, virt.line, pathTmp, -1);
			showInfo();
			if (virt.checked)
			{
				CID id;
				list<string> dirs;
				sm->getShareGroupDirectories(id, dirs);
				dirs.push_back(realPath);
				sm->updateShareGroup(id, Util::emptyString, dirs);
			}
		}
	}
	catch (const ShareException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
	}
}

void SharePage::insertDirectoryItem(int groupId, const tstring& virtualPath, const tstring& realPath, int64_t size)
{
	LVITEM item = {};
	item.mask = LVIF_TEXT | LVIF_GROUPID;
	item.iItem = ctrlDirectories.GetItemCount();
	item.pszText = const_cast<TCHAR*>(virtualPath.c_str());
	item.iGroupId = groupId;
	int i = ctrlDirectories.InsertItem(&item);
	ctrlDirectories.SetItemText(i, 1, realPath.c_str());
	if (size >= 0)
		ctrlDirectories.SetItemText(i, 2, Util::formatBytesT(size).c_str());
}

void SharePage::insertGroup(int groupId, const wstring& name)
{
	LVGROUP lg = {};
	lg.cbSize = sizeof(lg);
	lg.iGroupId = groupId;
	lg.state = LVGS_NORMAL |
#ifdef OSVER_WIN_XP
		(SysVersion::isOsVistaPlus() ? LVGS_COLLAPSIBLE : 0)
#else
		LVGS_COLLAPSIBLE
#endif
		;
	lg.mask = LVGF_GROUPID | LVGF_HEADER | LVGF_STATE | LVGF_ALIGN;
	lg.uAlign = LVGA_HEADER_LEFT;
	lg.pszHeader = const_cast<WCHAR*>(name.c_str());
	lg.cchHeader = static_cast<int>(name.length());
	ctrlDirectories.InsertGroup(groupId, &lg);
}

void SharePage::insertListItems()
{
	ctrlDirectories.DeleteAllItems();
	vector<ShareManager::SharedDirInfo> directories;
	ShareManager::getInstance()->getDirectories(directories);
		
	bool hasExcluded = false;
	for (size_t j = 0; j < directories.size(); ++j)
	{
		const ShareManager::SharedDirInfo& dir = directories[j];
		if (dir.isExcluded)
		{
			if (!hasExcludeGroup)
			{
				insertGroup(GROUP_EXCLUDED, WSTRING(SETTINGS_EXCLUDED_FOLDERS));
				hasExcludeGroup = true;
			}
			insertDirectoryItem(GROUP_EXCLUDED, tstring(), Text::toT(dir.realPath), -1);
			hasExcluded = true;
		}
		else
		{
			insertDirectoryItem(GROUP_NORMAL, Text::toT(dir.virtualPath), Text::toT(dir.realPath), dir.size);
		}
	}

	if (!hasExcluded && hasExcludeGroup)
	{
		ctrlDirectories.RemoveGroup(GROUP_EXCLUDED);
		hasExcludeGroup = false;
	}
}

void SharePage::onChange()
{
	updateList = true;
	showInfo();
}
