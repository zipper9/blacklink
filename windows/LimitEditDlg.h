/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef _LIMIT_EDIT_DLG_H_
#define _LIMIT_EDIT_DLG_H_

#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include "../client/typedefs.h"
#include "resource.h"

class LimitEditDlg: public CDialogImpl<LimitEditDlg>
{
	public:
		enum { IDD = IDD_SPEEDLIMIT_DLG };
		
		BEGIN_MSG_MAP(LimitEditDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_HANDLER(IDC_SPEED_STR, BN_CLICKED, onEnableLimit)
		COMMAND_HANDLER(IDC_SPEEDLIMITDLG_EDIT, EN_CHANGE, onChange)
		MESSAGE_HANDLER(WM_HSCROLL, onChangeSliderScroll);
		END_MSG_MAP()

		LimitEditDlg(bool upload, const tstring& nick, int limit, int minVal, int maxVal) :
			upload(upload), nick(nick), limit(limit), minVal(minVal), maxVal(maxVal), trackBarMoved(false) {}

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEnableLimit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		int getLimit() const { return limit; }

	protected:
		const bool upload;
		const tstring nick;
		const int minVal;
		const int maxVal;
		int limit;
		CTrackBarCtrl trackBar;
		CEdit edit;
		CButton enableLimit;
		bool trackBarMoved;

		void checkEditText();
};

#endif // _LIMIT_EDIT_DLG_H_
