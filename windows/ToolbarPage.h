/*
 * Copyright (C) 2003 Twink,  spm7@waikato.ac.nz
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

#ifndef TOOLBARPAGE_H
#define TOOLBARPAGE_H

#include "../client/typedefs.h"
#include "PropPage.h"

class ToolbarPage : public CPropertyPage<IDD_TOOLBAR_PAGE>, public PropPage
{
	public:
		explicit ToolbarPage() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TOOLBAR))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}

		BEGIN_MSG_MAP(ToolbarPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_TOOLBAR_ADD, BN_CLICKED, onAdd)
		COMMAND_HANDLER(IDC_TOOLBAR_REMOVE, BN_CLICKED, onRemove)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

		PROPSHEETPAGE *getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_TOOLBAR; }
		void write();

	protected:
		CListViewCtrl ctrlCommands;
		CListViewCtrl ctrlToolbar;
		CComboBox ctrlIconSize;

		void makeItem(LVITEM* lvi, int item, tstring& tmp);
};

#endif //TOOLBARPAGE_H
