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

#ifndef SHARE_PAGE_H
#define SHARE_PAGE_H

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"
#include "WinUtil.h"
#include "FolderTree.h"
#include "../client/SettingsManager.h"

class SharePage : public CPropertyPage<IDD_SHARE_PAGE>, public PropPage, public FolderTree::OnChangeListener
{
	public:
		explicit SharePage() : PropPage(TSTRING(SETTINGS_UPLOADS)), contDirectories(WC_LISTVIEW, this, 1)
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~SharePage()
		{
			ctrlDirectories.Detach();
		}
		
		BEGIN_MSG_MAP_EX(SharePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_DIRECTORIES, LVN_ITEMCHANGED, onItemChangedDirectories)
		NOTIFY_HANDLER(IDC_DIRECTORIES, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_DBLCLK, onDoubleClick)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_CUSTOMDRAW, ctrlDirectories.onCustomDraw)
		COMMAND_HANDLER(IDC_SHOW_TREE, BN_CLICKED, onClickedShowTree)
		COMMAND_ID_HANDLER(IDC_ADD, onClickedAdd)
		COMMAND_ID_HANDLER(IDC_REMOVE, onClickedRemove)
		COMMAND_ID_HANDLER(IDC_RENAME, onClickedRename)
		REFLECT_NOTIFICATIONS()
		ALT_MSG_MAP(1)
		MESSAGE_HANDLER(WM_DROPFILES, onDropFiles)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDropFiles(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onItemChangedDirectories(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onClickedAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedShowTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_UPLOAD; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	protected:
		CContainedWindow contDirectories;
		ExListViewCtrl ctrlDirectories;
		CStatic ctrlTotalSize;
		CStatic ctrlTotalFiles;
		FolderTree ft;
		bool showTree;
		bool listInitialized;
		bool updateTree;
		bool updateList;
		bool hasExcludeGroup;
		bool renamingItem;

		virtual void onChange() override;

	private:
		void addDirectory(const tstring& path);
		void insertDirectoryItem(int groupId, const tstring& virtualPath, const tstring& realPath, int64_t size);
		void insertGroup(int groupId, const wstring& name);
		void insertListItems();
		void toggleView();
		void showInfo();
};

#endif // !defined(SHARE_PAGE_H)
