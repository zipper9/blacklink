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

#ifndef TREE_PROPERTY_SHEET_H
#define TREE_PROPERTY_SHEET_H

#include <atldlgs.h>
#include "resource.h"
#include "StatusLabelCtrl.h"
#include "TimerHelper.h"
#include "UserMessages.h"
#include "../client/SettingsManager.h"

class TreePropertySheet : public CPropertySheetImpl<TreePropertySheet>, protected TimerHelper
{
	public:
		enum { TAB_MESSAGE_MAP = 13 };

		typedef CPropertySheetImpl<TreePropertySheet> baseClass;

		TreePropertySheet(ATL::_U_STRINGorID title = (LPCTSTR)NULL, UINT uStartPage = 0, HWND hWndParent = NULL) :
			CPropertySheetImpl<TreePropertySheet>(title, uStartPage, hWndParent)
			, tabContainer(WC_TABCONTROL, this, TAB_MESSAGE_MAP)
			, TimerHelper(m_hWnd)
			, icon(NULL)
		{
		
			m_psh.pfnCallback = propSheetProc;
			m_psh.dwFlags |= PSH_RTLREADING;
		}

		BEGIN_MSG_MAP(TreePropertySheet)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_COMMAND, baseClass::OnCommand)
		MESSAGE_HANDLER(WMU_USER_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WMU_RESTART_REQUIRED, onRestartRequired)
		NOTIFY_HANDLER(IDC_PAGE, TVN_SELCHANGED, onSelChanged)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(TAB_MESSAGE_MAP)
		MESSAGE_HANDLER(TCM_SETCURSEL, onSetCurSel)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSetCurSel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onSelChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onRestartRequired(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			showStatusText();
			return 0;
		}

		static int CALLBACK propSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam);

		TreePropertySheet(const TreePropertySheet &) = delete;
		TreePropertySheet& operator= (const TreePropertySheet &) = delete;

	protected:
		virtual void onTimerSec() {}
		virtual int getItemImage(int page) const { return 0; }
		virtual void pageChanged(int oldPage, int newPage) {}
		
		HICON icon;

	private:
		enum
		{
			SPACE_MID = 1,
			SPACE_TOP = 10,
			SPACE_BOTTOM = 1,
			SPACE_LEFT = 10,
			SPACE_RIGHT = 6,
			TREE_WIDTH = 245
		};
		
		static const int MAX_NAME_LENGTH = 256;

		void hideTab();
		void addTree();
		void fillTree();
		void showStatusText();

		HTREEITEM addItem(const tstring& str, HTREEITEM parent, int page, int image);
		HTREEITEM findItem(const tstring& str, HTREEITEM start);
		HTREEITEM findItem(int page, HTREEITEM start);
		
		CImageList treeIcons;
		CTreeViewCtrl ctrlTree;
		CContainedWindow tabContainer;
		StatusLabelCtrl ctrlStatus;
};

#endif // !defined(TREE_PROPERTY_SHEET_H)
