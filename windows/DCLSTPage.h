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

#ifndef _DCLST_PAGE_H_
#define _DCLST_PAGE_H_

#include "PropPage.h"

class DCLSTPage : public CPropertyPage<IDD_DCLS_PAGE>, public PropPage
{
	public:
		explicit DCLSTPage() : PropPage(TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(SETTINGS_DCLS_PAGE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP_EX(DCLSTPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_DCLS_CREATE_IN_FOLDER, onClickedDCLSTFolder)
		COMMAND_ID_HANDLER(IDC_DCLS_ANOTHER_FOLDER, onClickedDCLSTFolder)
		COMMAND_ID_HANDLER(IDC_PREVIEW_BROWSE, onBrowseClick)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedDCLSTFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBrowseClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_FOLDERS; }
		void write();
		void cancel()
		{
			cancel_check();
		}
		void fixControls();
		
	protected:
		CComboBox magnetClick;
};

#endif // _DCLST_PAGE_H_
