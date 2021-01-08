/*
 * Copyright (C) 2001-2019 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef PUBLIC_HUBS_LIST_DLG_H
#define PUBLIC_HUBS_LIST_DLG_H

#include "ExListViewCtrl.h"
#include "../client/HublistManager.h"
#include "../client/Text.h"
#include "resource.h"

class PublicHubsListDlg : public CDialogImpl<PublicHubsListDlg>
{
	public:
		enum { IDD = IDD_HUB_LIST };

		PublicHubsListDlg(const vector<HublistManager::HubListInfo> &hubLists): hubLists(hubLists) {}

		~PublicHubsListDlg()
		{
			ctrlList.Detach();
		}

		BEGIN_MSG_MAP(PublicHubsListDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_LIST_ADD, onAdd);
		COMMAND_ID_HANDLER(IDC_LIST_UP, onMoveUp);
		COMMAND_ID_HANDLER(IDC_LIST_DOWN, onMoveDown);
		COMMAND_ID_HANDLER(IDC_LIST_EDIT, onEdit);
		COMMAND_ID_HANDLER(IDC_LIST_REMOVE, onRemove);
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		NOTIFY_HANDLER(IDC_LIST_LIST, LVN_ITEMCHANGED, onItemchangedDirectories)
		NOTIFY_HANDLER(IDC_LIST_LIST, NM_DBLCLK, onDblClick)
		END_MSG_MAP();

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
		LRESULT onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled);
		LRESULT onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled);
		LRESULT onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled);
		LRESULT onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled);
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL & /*bHandled*/);
		LRESULT onDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

	private:
		ExListViewCtrl ctrlList;
		const vector<HublistManager::HubListInfo> &hubLists;

		void updateButtonState(BOOL state);
};

#endif // !defined(PUBLIC_HUBS_LIST_DLG_H)
