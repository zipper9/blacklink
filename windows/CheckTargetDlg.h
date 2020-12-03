/*
 * Copyright (C) 2010-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef _CHECK_TARGET_DLG_H_
#define _CHECK_TARGET_DLG_H_

#include <atlctrls.h>
#include <atldlgs.h>
#include "../client/Text.h"
#include "resource.h"

#ifdef FLYLINKDC_SUPPORT_WIN_XP
class ClassicCheckTargetDlg : public CDialogImpl<ClassicCheckTargetDlg>
{
	public:
		enum { IDD = IDD_CHECKTARGETDLG };
		
		ClassicCheckTargetDlg(const string& fileName, int64_t sizeNew, int64_t sizeExisting, time_t timeExisting, int option)
			: fileName(Text::toT(fileName)), sizeNew(sizeNew), sizeExisting(sizeExisting), timeExisting(timeExisting), option(option), applyForAll(false) { }
		
		BEGIN_MSG_MAP(CheckTargetDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_REPLACE_REPLACE, onRadioButton)
		COMMAND_ID_HANDLER(IDC_REPLACE_RENAME, onRadioButton)
		COMMAND_ID_HANDLER(IDC_REPLACE_SKIP, onRadioButton)
		END_MSG_MAP();
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		int getOption() const
		{
			return option;
		}
		bool isApplyForAll() const
		{
			return applyForAll;
		}

	private:
		tstring fileName;
		tstring newName;
		int64_t sizeNew;
		int64_t sizeExisting;
		time_t timeExisting;
		int option;
		bool applyForAll;
};
#endif

namespace CheckTargetDlg
{

	void showDialog(HWND hWndParent, const string& fileName, int64_t sizeNew, int64_t sizeExisting, time_t timeExisting, int& option, bool& applyForAll);

}

#endif // _CHECK_TARGET_DLG_H_
