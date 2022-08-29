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

#ifndef ADLS_PROPERTIES_H_
#define ADLS_PROPERTIES_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "../client/typedefs.h"

class ADLSearch;

class ADLSProperties : public CDialogImpl<ADLSProperties>
{
	public:
		explicit ADLSProperties(ADLSearch* search) : search(search), autoSwitchToTTH(false) {}

		enum { IDD = IDD_ADLS_PROPERTIES };

		BEGIN_MSG_MAP(ADLSProperties)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_HANDLER(IDC_SEARCH_STRING, EN_CHANGE, onEditChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	private:
		ADLSearch* search;
		bool autoSwitchToTTH;

		CEdit ctrlSearch;
		CEdit ctrlDestDir;
		CEdit ctrlMinSize;
		CEdit ctrlMaxSize;
		CButton ctrlActive;
		CButton ctrlMatchCase;
		CButton ctrlRegEx;
		CButton ctrlAutoQueue;
		CButton ctrlFlagFile;
		CComboBox ctrlSearchType;
		CComboBox ctrlSizeType;
		CComboBox ctrlRaw;

		void checkTTH(const tstring& str);
};

#endif // ADLS_PROPERTIES_H_
