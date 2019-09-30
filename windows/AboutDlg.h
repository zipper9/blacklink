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

#if !defined(ABOUT_DLG_H)
#define ABOUT_DLG_H

#include "HIconWrapper.h"
#include "wtl_flylinkdc.h"

class AboutDlg : public CDialogImpl<AboutDlg>
{
	public:
		enum { IDD = IDD_ABOUTBOX };
		
		AboutDlg() { }
		~AboutDlg() {}
		
		BEGIN_MSG_MAP(AboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			SetDlgItemText(IDC_VERSION, T_VERSIONSTRING);
			
			::SetWindowText(GetDlgItem(IDC_UPDATE_VERSION_CURRENT_LBL), (TSTRING(CURRENT_VERSION) + _T(":")).c_str());
			
//[-]PPA    SetDlgItemText(IDC_TTH, WinUtil::tth.c_str());

			char l_full_version[64];
			_snprintf(l_full_version, _countof(l_full_version), "%d", _MSC_FULL_VER);
			
			//CenterWindow(GetParent());
			return TRUE;
		}
};

#endif // !defined(ABOUT_DLG_H)
