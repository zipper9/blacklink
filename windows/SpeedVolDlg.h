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

#ifndef _SPEED_VOL_DLG_H_
#define _SPEED_VOL_DLG_H_

#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>

class SpeedVolDlg: public CDialogImpl<SpeedVolDlg>
{
	public:
		enum { IDD = IDD_SPEEDLIMIT_DLG };
		
		BEGIN_MSG_MAP(SpeedVolDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		MESSAGE_HANDLER(WM_VSCROLL, OnChangeSliderScroll)
		MESSAGE_HANDLER(WM_NCACTIVATE, OnNCActivate)
		END_MSG_MAP()
		
		SpeedVolDlg(int limit = 0, int min = 0, int max = 6144) : limit(limit), minValue(min), maxValue(max) {}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		int GetLimit() const
		{
			return limit;
		}
		
		LRESULT OnNCActivate(UINT /* uMsg */, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (!wParam)
			{
				BOOL unused;
				OnCloseCmd(0, IDOK, NULL, unused);
				return FALSE;
			}
			return 0;
		}
		
	protected:
		int limit;
		int minValue;
		int maxValue;
		
		CTrackBarCtrl trackBar;
		CEdit edit;
};

#endif // _SPEED_VOL_DLG_H_
