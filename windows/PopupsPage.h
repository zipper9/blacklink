/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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

#ifndef POPUPS_PAGE_H_
#define POPUPS_PAGE_H_

#include "PropPage.h"

class PopupsPage : public CPropertyPage<IDD_POPUPS_PAGE>, public PropPage
{
	public:
		PopupsPage();

		BEGIN_MSG_MAP(PopupsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_PREVIEW, BN_CLICKED, onPreview)
		COMMAND_ID_HANDLER(IDC_POPUP_FONT, onFont)
		COMMAND_ID_HANDLER(IDC_POPUP_TITLE_FONT, onTitleFont)
		COMMAND_ID_HANDLER(IDC_POPUP_BACKCOLOR, onBackColor)
		COMMAND_ID_HANDLER(IDC_POPUP_BORDER_COLOR, onBorderColor)
		COMMAND_ID_HANDLER(IDC_POPUP_TYPE, onTypeChanged)
		COMMAND_ID_HANDLER(IDC_POPUP_ENABLE, onFixControls)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onPreview(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onBackColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBorderColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTitleFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTypeChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		PROPSHEETPAGE *getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_POPUPS; }
		void write();

	private:
		void fixControls();
		void updateControls(int popupType);
		void changeColor(int index);
		void changeFont(int fontIndex, int colorIndex);
		void applySettings();
		void restoreSettings();

		CListViewCtrl ctrlPopups;
		CComboBox ctrlPopupType;

		struct ColorSetting
		{
			COLORREF color, oldColor;
			bool changed;
			int setting;
		};

		struct FontSetting
		{
			string font, oldFont;
			bool changed;
			int setting;
		};

		ColorSetting colorSettings[4];
		FontSetting fontSettings[2];
};

#endif // POPUPS_PAGE_H_
