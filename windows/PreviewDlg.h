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

#ifndef PREVIEW_DLG_H_
#define PREVIEW_DLG_H_

#include <atlcrack.h>
#include "../client/Text.h"

class PreviewDlg : public CDialogImpl<PreviewDlg>
{
		const bool newItem;
		CEdit ctrlName;
		CEdit ctrlApplication;
		CEdit ctrlArguments;
		CEdit ctrlExtensions;
		
	public:
		PreviewDlg(bool newItem) : newItem(newItem), arguments(_T("%[file]")) {}
		
		tstring name;
		tstring application;
		tstring arguments;
		tstring extensions;
		
		enum { IDD = IDD_PREVIEW };
		
		BEGIN_MSG_MAP_EX(PreviewDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_PREVIEW_BROWSE, onBrowse);
		COMMAND_HANDLER(IDC_PREVIEW_NAME, EN_CHANGE, onChange)
		COMMAND_HANDLER(IDC_PREVIEW_APPLICATION, EN_CHANGE, onChange)
		END_MSG_MAP()
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlName.SetFocus();
			return FALSE;
		}
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};

#endif // PREVIEW_DLG_H_
