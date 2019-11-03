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
#include "SpeedVolDlg.h"
#include "WinUtil.h"
#include "../client/Util.h"

LRESULT SpeedVolDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (limit < minValue) limit = minValue;
	if (limit > maxValue) limit = maxValue;
	
	SetDlgItemText(IDOK, CTSTRING(OK));
	/* TODO
	SetDlgItemText(IDC_SPEED_STATIC, CTSTRING(K));
	*/
	
	CPoint pt;
	GetCursorPos(&pt);
	CRect rcWindow;
	GetWindowRect(rcWindow);
	ScreenToClient(rcWindow);
	SetWindowPos(NULL, pt.x, pt.y - (rcWindow.bottom - rcWindow.top), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	
	trackBar.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_SLIDER));
	edit.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_EDIT));
	
	edit.SetWindowTextW(Util::toStringW(limit).c_str());
	trackBar.SetRange(maxValue / -24, minValue / -24, TRUE);
	trackBar.SetTicFreq(24);
	trackBar.SetPos(limit / -24);
	trackBar.SetFocus();
	
	return FALSE;
}

LRESULT SpeedVolDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{		
		tstring buf;
		WinUtil::getWindowText(edit, buf);
		limit = Util::toInt(buf);
		if (limit < minValue) limit = minValue;
		if (limit > maxValue) limit = maxValue;
	}
	EndDialog(wID);
	return FALSE;
}

LRESULT SpeedVolDlg::OnChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	const int pos = trackBar.GetPos() * -24;
	edit.SetWindowTextW(Util::toStringW(pos).c_str());
	return FALSE;
}
