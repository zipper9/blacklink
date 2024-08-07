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

#ifndef PROPERTIES_DLG_H
#define PROPERTIES_DLG_H

#include "PropPage.h"
#include "TreePropertySheet.h"

class PropertiesDlg : public TreePropertySheet
{
	public:
		BEGIN_MSG_MAP(PropertiesDlg)
		COMMAND_ID_HANDLER(IDOK, onOK)
		COMMAND_ID_HANDLER(IDCANCEL, onCANCEL)
		CHAIN_MSG_MAP(TreePropertySheet)
		ALT_MSG_MAP(TreePropertySheet::TAB_MESSAGE_MAP)
		MESSAGE_HANDLER(TCM_SETCURSEL, TreePropertySheet::onSetCurSel)
		END_MSG_MAP()

		PropertiesDlg(HWND parent, HICON icon);
		~PropertiesDlg();

		static PropertiesDlg* instance;

		LRESULT onOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onCANCEL(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

	protected:
		void write();
		void cancel();

		virtual int getItemImage(int page) const override;
		virtual void pageChanged(int oldPage, int newPage) override;

		static const size_t numPages = 37;
		PropPage *pages[numPages];

	private:
		virtual void onTimerSec();
};

#endif // !defined(PROPERTIES_DLG_H)
