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

#ifndef MAGNET_DLG_H
#define MAGNET_DLG_H

#include <atldlgs.h>
#include <atlcrack.h>
#include "resource.h"
#include "WinUtil.h"

#ifdef OSVER_WIN_XP
class ClassicMagnetDlg : public CDialogImpl<ClassicMagnetDlg>
{
	public:
		enum { IDD = IDD_MAGNET };
		
		ClassicMagnetDlg(const TTHValue& hash, const tstring& fileName, const int64_t fileSize, const int64_t dirSize, bool dclst) :
			hash(hash), fileName(fileName), fileSize(fileSize), dirSize(dirSize), dclst(dclst), action(WinUtil::MA_DEFAULT)
		{}
		
		BEGIN_MSG_MAP(ClassicMagnetDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_MAGNET_OPEN, onRadioButton)
		COMMAND_ID_HANDLER(IDC_MAGNET_QUEUE, onRadioButton)
		COMMAND_ID_HANDLER(IDC_MAGNET_SEARCH, onRadioButton)
		COMMAND_ID_HANDLER(IDC_MAGNET_SAVEAS, onSaveAs)
		END_MSG_MAP();
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		WinUtil::DefinedMagnetAction getAction() const { return action; }
		const tstring& getFileName() const { return fileName; }

	private:
		const TTHValue hash;
		tstring fileName;
		const int64_t fileSize;
		const int64_t dirSize;
		const bool dclst;
		WinUtil::DefinedMagnetAction action;
};
#endif

namespace MagnetDlg
{

	WinUtil::DefinedMagnetAction showDialog(HWND hWndParent, const TTHValue& hash, tstring& fileName, int64_t fileSize, int64_t dirSize, bool dclst);

}

#endif // !defined(MAGNET_DLG_H)
