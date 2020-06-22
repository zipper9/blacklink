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

#ifndef TABS_PAGE_H
#define TABS_PAGE_H

#include "PropPage.h"

class TabsPage : public CPropertyPage<IDD_TABS_PAGE>, public PropPage
{
	public:
		explicit TabsPage() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TABS))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP(TabsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_TABS; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	protected:
		CListViewCtrl ctrlOption;
		CListViewCtrl ctrlBold;
};

#endif // TABS_PAGE_H
