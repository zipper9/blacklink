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

#ifndef COMMAND_DLG_H
#define COMMAND_DLG_H

#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include "RichTextLabel.h"
#include "resource.h"

class CommandDlg : public CDialogImpl<CommandDlg>
{
		CEdit ctrlName;
		CComboBox ctrlType;
		CEdit ctrlCommand;
		CEdit ctrlHub;
		CEdit ctrlNick;
		CButton ctrlContextHub;
		CButton ctrlContextUser;
		CButton ctrlContextSearch;
		CButton ctrlContextFilelist;
		CButton ctrlOnce;
		CEdit ctrlResult;
		RichTextLabel ctrlNote;

	public:
		int type;
		int ctx;
		tstring name;
		tstring command;
		tstring hub;
		const bool newCommand;
		
		enum { IDD = IDD_USER_COMMAND };
		
		BEGIN_MSG_MAP(CommandDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_HELP_TYPE, onTypeHint)
		COMMAND_HANDLER(IDC_COMMAND, EN_CHANGE, onChange)
		COMMAND_HANDLER(IDC_NICK, EN_CHANGE, onChange)
		COMMAND_HANDLER(IDC_HUB, EN_CHANGE, onHub);
		COMMAND_HANDLER(IDC_COMMAND_TYPE, CBN_SELCHANGE, onChangeType)
		MESSAGE_HANDLER(WMU_LINK_ACTIVATED, onLinkActivated)
		END_MSG_MAP()
		
		CommandDlg(bool newCommand) : type(0), ctx(0), newCommand(newCommand) { }
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlName.SetFocus();
			return FALSE;
		}
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onChangeType(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onHub(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTypeHint(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLinkActivated(UINT, WPARAM, LPARAM lParam, BOOL&);

	private:
		void updateCommand();
		void updateHub();
		void updateControls();
		void updateContext();
};

#endif // !defined(COMMAND_DLG_H)
