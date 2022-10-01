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

/*
 * Automatic Directory Listing Search
 * Henrik Engström, henrikengstrom on home point se
 */


#ifndef ADL_SEARCH_FRAME_H
#define ADL_SEARCH_FRAME_H

#include "FlatTabCtrl.h"
#include "StaticFrame.h"
#include "ExListViewCtrl.h"
#include "../client/ADLSearch.h"

class ADLSearchFrame : public MDITabChildWindowImpl<ADLSearchFrame>,
	public StaticFrame<ADLSearchFrame, ResourceManager::ADL_SEARCH, IDC_FILE_ADL_SEARCH>,
	private SettingsManagerListener,
	public CMessageFilter
{
	public:
		typedef MDITabChildWindowImpl<ADLSearchFrame> baseClass;

		ADLSearchFrame(): xdu(0), ydu(0), setCheckState(0), dlgHelp(nullptr) {}

		ADLSearchFrame(const ADLSearchFrame&) = delete;
		ADLSearchFrame& operator= (const ADLSearchFrame&) = delete;
		
		static CFrameWndClassInfo& GetWndClassInfo();
		
		BEGIN_MSG_MAP(ADLSearchFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		MESSAGE_HANDLER(WMU_DIALOG_CLOSED, onHelpDialogClosed)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_ADD, onAdd)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_ADLS_HELP, onHelp)
		COMMAND_ID_HANDLER(IDC_MOVE_UP, onMoveUp)
		COMMAND_ID_HANDLER(IDC_MOVE_DOWN, onMoveDown)
		NOTIFY_HANDLER(IDC_ADLLIST, NM_CUSTOMDRAW, ctrlList.onCustomDraw)
		NOTIFY_HANDLER(IDC_ADLLIST, NM_DBLCLK, onDoubleClickList)
		NOTIFY_HANDLER(IDC_ADLLIST, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_ADLLIST, LVN_KEYDOWN, onKeyDown)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		// Message handlers
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onHelp(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onMoveUp(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onMoveDown(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDoubleClickList(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onHelpDialogClosed(UINT, WPARAM, LPARAM lParam, BOOL&);
		
		// Update colors
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		// Update control layouts
		void UpdateLayout(BOOL bResizeBars = TRUE);

		virtual BOOL PreTranslateMessage(MSG* pMsg) override;

	private:
		void getItemText(TStringList& sl, const ADLSearch& s) const;
		void load();
		void update(const ADLSearchManager::SearchCollection& collection, const vector<int>& selection);
		void showItem(int index, const ADLSearch& s);

		ExListViewCtrl ctrlList;
		int setCheckState;
		int xdu, ydu;
		int buttonWidth, buttonHeight, buttonSpace;
		int vertMargin, horizMargin;
		class HelpTextDlg* dlgHelp;

		CButton ctrlAdd;
		CButton ctrlEdit;
		CButton ctrlRemove;
		CButton ctrlMoveUp;
		CButton ctrlMoveDown;
		CButton ctrlHelp;
		OMenu contextMenu;

		// Column order
		enum
		{
			COLUMN_FIRST = 0,
			COLUMN_ACTIVE_SEARCH_STRING = COLUMN_FIRST,
			COLUMN_SOURCE_TYPE,
			COLUMN_DEST_DIR,
			COLUMN_MIN_FILE_SIZE,
			COLUMN_MAX_FILE_SIZE,
			COLUMN_LAST
		};
		
		// Column parameters
		static int columnIndexes[];
		static int columnSizes[];
		void on(SettingsManagerListener::Repaint) override;
};

#endif // !defined(ADL_SEARCH_FRAME_H)
