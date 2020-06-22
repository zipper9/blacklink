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

#ifndef _INTEGRATION_PAGE_H_
#define _INTEGRATION_PAGE_H_

#include "PropPage.h"

class IntegrationPage : public CPropertyPage<IDD_INTEGRATION_PAGE>, public PropPage
{
	public:
		explicit IntegrationPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_INTEGRATION_PROP)),
			shellIntEnabled(false),
			autostartEnabled(false),
			shellIntAvailable(false)
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP_EX(IntegrationPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
#ifdef SSA_SHELL_INTEGRATION
		COMMAND_ID_HANDLER(IDC_SHELL_INT_BUTTON, onClickedShellInt)
#endif
		COMMAND_ID_HANDLER(IDC_AUTOSTART_BUTTON, onClickedAutostart)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#ifdef SSA_SHELL_INTEGRATION
		LRESULT onClickedShellInt(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onClickedAutostart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_VIDEO1; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
#ifdef SSA_SHELL_INTEGRATION
		void checkShellInt();
		void updateShellIntState();
#endif
		void checkAutostart();
		void updateAutostartState();
		
		bool shellIntAvailable;
		bool shellIntEnabled;
		bool autostartEnabled;
		
		CListViewCtrl ctrlList;
};

#endif //_INTEGRATION_PAGE_H_
