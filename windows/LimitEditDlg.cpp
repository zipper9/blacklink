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
#include "LimitEditDlg.h"
#include "WinUtil.h"
#include "../client/Util.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_KBPS, ResourceManager::KBPS      },
	{ IDOK,     ResourceManager::OK        },
	{ IDCANCEL, ResourceManager::CANCEL    },
	{ 0,        ResourceManager::Strings() }
};

LRESULT LimitEditDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(upload ? CTSTRING(UPLOAD_SPEED_LIMIT) : CTSTRING(DOWNLOAD_SPEED_LIMIT));
	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::LIMIT, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	BOOL enable = limit > 0;
	if (limit <= 0) limit = maxVal <= 1024 ? 256 : 1024;

	WinUtil::translate(*this, texts);
	tstring text;
	if (!nick.empty())
		text = TSTRING_F(SET_UL_SPEED_LIMIT_USER, nick);
	else
		text = upload ? TSTRING(SET_UL_SPEED_LIMIT) : TSTRING(SET_DL_SPEED_LIMIT);
	enableLimit.Attach(GetDlgItem(IDC_SPEED_STR));
	enableLimit.SetWindowText(text.c_str());
	
	trackBar.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_SLIDER));
	edit.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_EDIT));

	enableLimit.SetCheck(enable ? BST_CHECKED : BST_UNCHECKED);
	edit.EnableWindow(enable);
	trackBar.EnableWindow(enable);
	
	edit.SetWindowText(Util::toStringT(limit).c_str());
	edit.LimitText(5);
	trackBar.SetRange(minVal, maxVal, TRUE);
	trackBar.SetTicFreq(maxVal <= 1024 ? 64 : 512);
	trackBar.SetPos(limit);
	
	return FALSE;
}

LRESULT LimitEditDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		if (enableLimit.GetCheck() == BST_CHECKED)
		{
			tstring buf;
			WinUtil::getWindowText(edit, buf);
			limit = Util::toInt(buf);
		}
		else
			limit = 0;
	}
	EndDialog(wID);
	return FALSE;
}

LRESULT LimitEditDlg::onChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	trackBarMoved = true;
	int pos = trackBar.GetPos();
	edit.SetWindowText(Util::toStringT(pos).c_str());
	trackBarMoved = false;
	return FALSE;
}

LRESULT LimitEditDlg::onEnableLimit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (enableLimit.GetCheck() == BST_CHECKED)
	{
		edit.EnableWindow(TRUE);
		trackBar.EnableWindow(TRUE);
		checkEditText();
	}
	else
	{
		edit.EnableWindow(FALSE);
		trackBar.EnableWindow(FALSE);
		GetDlgItem(IDOK).EnableWindow(TRUE);
	}
	return FALSE;
}

LRESULT LimitEditDlg::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!trackBarMoved)
	{
		tstring buf;
		WinUtil::getWindowText(edit, buf);
		int value = Util::toInt(buf);
		GetDlgItem(IDOK).EnableWindow(value >= minVal);
		if (value >= minVal)
		{
			if (value > maxVal) value = maxVal;
			trackBar.SetPos(value);
		}
	}
	return FALSE;
}

void LimitEditDlg::checkEditText()
{
	tstring buf;
	WinUtil::getWindowText(edit, buf);
	limit = Util::toInt(buf);
	GetDlgItem(IDOK).EnableWindow(limit >= minVal);
}
