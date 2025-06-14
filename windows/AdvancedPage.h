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

#ifndef ADVANCED_PAGE_H
#define ADVANCED_PAGE_H

#include "../client/typedefs.h"
#include "PropPage.h"

class AdvancedPage : public CPropertyPage<IDD_ADVANCED_PAGE>, public PropPage
{
	public:
		explicit AdvancedPage() : PropPage(TSTRING(SETTINGS_ADVANCED)), curSel(0)
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP_EX(AdvancedPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_WINAMP_HELP, BN_CLICKED, onClickedWinampHelp)
		COMMAND_HANDLER(IDC_PLAYER_COMBO, CBN_SELCHANGE, onSelChange)
		COMMAND_HANDLER(IDC_RATIOMSG_HELP, BN_CLICKED, onClickedRatioMsgHelp)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedWinampHelp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onClickedRatioMsgHelp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);//[+] WhiteD. Custom ratio message
		LRESULT onSelChange(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_SETTINGS; }
		void write();

	protected:
		CComboBox ctrlPlayer;
		CEdit ctrlFormat;
		
		tstring WinampStr;
		tstring WMPlayerStr;
		tstring iTunesStr;
		tstring MPCStr;
		tstring JAStr;
		tstring QCDQMPStr;
		int curSel;

		CListViewCtrl ctrlList;

		void updateFormatString();
};

#endif // !defined(ADVANCED_PAGE_H)
