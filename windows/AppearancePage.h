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

#ifndef APPEARANCE_PAGE_H
#define APPEARANCE_PAGE_H

#include "../client/typedefs.h"
#include "PropPage.h"

class AppearancePage : public CPropertyPage<IDD_APPEARANCE_PAGE>, public PropPage
{
	public:
		explicit AppearancePage() : PropPage(TSTRING(SETTINGS_APPEARANCE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP_EX(AppearancePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_TIMESTAMP_HELP, BN_CLICKED, onClickedHelp)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedHelp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_WINDOW; }
		void write();

	protected:
		CListViewCtrl ctrlList;
		
		struct ThemeInfo
		{
			wstring description;
			string name;
		};
		
		CComboBox ctrlTheme;
		vector<ThemeInfo> themes;
		
		void getThemeList();	
};

#endif // !defined(APPEARANCE_PAGE_H)
