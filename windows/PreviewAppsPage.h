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

#ifndef PREVIEW_APPS_PAGE_H_
#define PREVIEW_APPS_PAGE_H_

#include "PropPage.h"
#include "ExListViewCtrl.h"

class PreviewApplication;

class PreviewAppsPage : public CPropertyPage<IDD_PREVIEW_APPS_PAGE>, public PropPage
{
	public:
		explicit PreviewAppsPage() : PropPage(TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(SETTINGS_AVIPREVIEW))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~PreviewAppsPage()
		{
			ctrlCommands.Detach();
		}

		BEGIN_MSG_MAP_EX(PreviewAppsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ADD_MENU, onAddMenu)
		COMMAND_ID_HANDLER(IDC_REMOVE_MENU, onRemoveMenu)
		COMMAND_ID_HANDLER(IDC_CHANGE_MENU, onChangeMenu)
		COMMAND_ID_HANDLER(IDC_DETECT, onDetect)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, NM_DBLCLK, onDblClick)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, LVN_ITEMCHANGED, onItemchangedDirectories)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, NM_CUSTOMDRAW, ctrlCommands.onCustomDraw)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onAddMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChangeMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDetect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);

		LRESULT onDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;

			if (item->iItem >= 0)
			{
				PostMessage(WM_COMMAND, IDC_CHANGE_MENU, 0);
			}
			else if (item->iItem == -1)
			{
				PostMessage(WM_COMMAND, IDC_ADD_MENU, 0);
			}

			return 0;
		}

		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_MEDIA; }
		void write() {}

	protected:
		ExListViewCtrl ctrlCommands;
		void addEntry(const PreviewApplication* pa, int pos);
		void insertAppList();
		void checkMenu();
};

#endif // PREVIEW_APPS_PAGE_H_
