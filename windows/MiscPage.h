/*
 * Copyright (C) 2006-2013 Crise, crise<at>mail.berlios.de
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

#ifndef MISC_PAGE_H
#define MISC_PAGE_H

#include "PropPage.h"
#include "ExListViewCtrl.h"

class MiscPage : public CPropertyPage<IDD_MISC_PAGE>, public PropPage
{
	public:
		explicit MiscPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_MISC)), ignoreListChanged(false)
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~MiscPage()
		{
			ignoreListCtrl.Detach();
		}
		
		BEGIN_MSG_MAP_EX(MiscPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_IGNORE_ADD, onIgnoreAdd)
		COMMAND_ID_HANDLER(IDC_IGNORE_REMOVE, onIgnoreRemove)
		COMMAND_ID_HANDLER(IDC_IGNORE_CLEAR, onIgnoreClear)
		NOTIFY_HANDLER(IDC_IGNORELIST, NM_CUSTOMDRAW, ignoreListCtrl.onCustomDraw)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onIgnoreAdd(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreRemove(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreClear(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_DISPLAY; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		StringSet ignoreList;
		bool ignoreListChanged;
		ExListViewCtrl ignoreListCtrl;
};

#endif // !defined(MISC_PAGE_H)
