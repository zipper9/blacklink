﻿/*
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

#ifndef GENERAL_PAGE_H
#define GENERAL_PAGE_H

#include "PropPage.h"

class GeneralPage : public CPropertyPage<IDD_GENERAL_PAGE>, public PropPage
{
	public:
		explicit GeneralPage() : PropPage(TSTRING(SETTINGS_GENERAL))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~GeneralPage()
		{
			imageListGender.Destroy();
		}
		
		BEGIN_MSG_MAP_EX(GeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_NICK, EN_CHANGE, onTextChanged)
		COMMAND_HANDLER(IDC_EMAIL, EN_CHANGE, onTextChanged)
		COMMAND_HANDLER(IDC_DESCRIPTION, EN_CHANGE, onTextChanged)
		COMMAND_ID_HANDLER(IDC_CLIENT_ID, onChangeId)
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
		COMMAND_ID_HANDLER(IDC_CHECK_ADD_TO_DESCRIPTION, onClickedActive)
#endif
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChangeId(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_USER_INFO; }
		void write();

	private:
		struct LanguageInfo
		{
			string filename;
			string language;
		};

		vector<LanguageInfo> languageList;
		CComboBoxEx ctrlGender;
		CImageList imageListGender;

		void fixControls();
		void getLangList();
		void addGenderItem(const TCHAR* text, int imageIndex, int index);
};

#endif // !defined(GENERAL_PAGE_H)
