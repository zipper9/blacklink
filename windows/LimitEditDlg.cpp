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

#include "stdafx.h"
#include "Resource.h"
#include "LimitEditDlg.h"
#include "WinUtil.h"
#include "../client/FavoriteUser.h"
#include "../client/Util.h"

static const int MAXIMAL_LIMIT_KBPS = 10 * 1024;

LRESULT LimitEditDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SPEED_LIMIT));

	SetDlgItemText(IDOK, CTSTRING(OK));
	/* TODO
	SetDlgItemText(IDC_SPEED_STATIC, CTSTRING(K));
	*/
	
	trackBar.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_SLIDER));
	edit.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_EDIT));
	
	edit.SetWindowTextW(Util::toStringW(limit).c_str());
	trackBar.SetRange(MAXIMAL_LIMIT_KBPS / -10, 0, TRUE);
	trackBar.SetTicFreq(MAXIMAL_LIMIT_KBPS / 100);
	trackBar.SetPos(limit / -10);
	trackBar.SetFocus();
	
	return FALSE;
}

LRESULT LimitEditDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		tstring buf;
		WinUtil::getWindowText(GetDlgItem(IDC_SPEEDLIMITDLG_EDIT), buf);
		limit = Util::toInt(buf);
	}
	EndDialog(wID);
	return FALSE;
}

LRESULT LimitEditDlg::OnChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	int pos = trackBar.GetPos() * -10;
	edit.SetWindowTextW(Util::toStringW(pos).c_str());	
	return FALSE;
}
