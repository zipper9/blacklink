/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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
#include "PreviewDlg.h"
#include "WinUtil.h"
#include <boost/algorithm/string.hpp>

LRESULT PreviewDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SETTINGS_PREVIEW_DLG));
	SetDlgItemText(IDC_PREV_NAME2, CTSTRING(SETTINGS_NAME2));
	SetDlgItemText(IDC_PREV_APPLICATION, CTSTRING(SETTINGS_PREVIEW_DLG_APPLICATION));
	SetDlgItemText(IDC_PREV_ARG, CTSTRING(SETTINGS_PREVIEW_DLG_ARGUMENTS));
	SetDlgItemText(IDC_PREV_EXT, CTSTRING(SETTINGS_PREVIEW_DLG_EXT));
	
	//SetDlgItemText(IDC_PREVIEW_BROWSE, CTSTRING(BROWSE)); // [~] JhaoDa, not necessary any more
	SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));
	SetDlgItemText(IDOK, CTSTRING(OK));
	
	ctrlName.Attach(GetDlgItem(IDC_PREVIEW_NAME));
	ctrlApplication.Attach(GetDlgItem(IDC_PREVIEW_APPLICATION));
	ctrlArguments.Attach(GetDlgItem(IDC_PREVIEW_ARGUMENTS));
	ctrlExtensions.Attach(GetDlgItem(IDC_PREVIEW_EXTENSION));
	
	ctrlName.SetWindowText(name.c_str());
	ctrlApplication.SetWindowText(application.c_str());
	ctrlArguments.SetWindowText(arguments.c_str());
	ctrlExtensions.SetWindowText(extensions.c_str());
	
	CenterWindow(GetParent());
	return 0;
}

LRESULT PreviewDlg::OnBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
{
	tstring x;
	WinUtil::getWindowText(ctrlApplication, x);
	
	if (WinUtil::browseFile(x, m_hWnd, false, Util::emptyStringT, _T("Application\0*.exe\0\0")) == IDOK)  // TODO translate
		ctrlApplication.SetWindowText(x.c_str());
	
	return 0;
}

LRESULT PreviewDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(ctrlName, name);
		WinUtil::getWindowText(ctrlApplication, application);
		if (name.empty() || application.empty())
		{
			MessageBox(CTSTRING(NAME_COMMAND_EMPTY), getAppNameVerT().c_str(), MB_ICONWARNING|MB_OK);
			return 0;
		}
		WinUtil::getWindowText(ctrlArguments, arguments);
		WinUtil::getWindowText(ctrlExtensions, extensions);
		boost::replace_all(extensions, " ", "");
		boost::replace_all(extensions, ",", ";");
	}
	EndDialog(wID);
	return 0;
}
