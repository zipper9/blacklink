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

#ifndef ABOUT_DLG_H
#define ABOUT_DLG_H

#include "wtl_flylinkdc.h"

class AboutDlg : public CDialogImpl<AboutDlg>
{
	public:
		enum { IDD = IDD_ABOUTBOX };
		
		BEGIN_MSG_MAP(AboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
			SetDlgItemText(IDC_VERSION, T_VERSIONSTRING);
			::SetWindowText(GetDlgItem(IDC_UPDATE_VERSION_CURRENT_LBL), (TSTRING(CURRENT_VERSION) + _T(":")).c_str());
			return TRUE;
		}
};

#endif // !defined(ABOUT_DLG_H)
