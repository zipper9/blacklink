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

#ifndef HASH_PROGESS_DLG_H
#define HASH_PROGESS_DLG_H

#include <atlctrls.h>
#include <atldlgs.h>
#include "resource.h"
#include "TimerHelper.h"

class HashProgressDlg : public CDialogImpl<HashProgressDlg>, private TimerHelper
{
	public:
		enum { IDD = IDD_HASH_PROGRESS };
		static const int MAX_PROGRESS_VALUE = 8192;
		
		HashProgressDlg(bool autoClose, bool exitOnDone, HICON icon) :
			TimerHelper(m_hWnd), autoClose(autoClose), exitOnDone(exitOnDone), icon(icon),
			paused(false), tempHashSpeed(0), updatingEditBox(0)
		{
			dcassert(!instanceCounter);
			++instanceCounter;
		}
		~HashProgressDlg()
		{
			--instanceCounter;
		}
		
		BEGIN_MSG_MAP(HashProgressDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_BTN_ABORT, onClickedAbort)
		COMMAND_HANDLER(IDC_EDIT_MAX_HASH_SPEED, EN_CHANGE, onChangeMaxHashSpeed)
		COMMAND_ID_HANDLER(IDC_PAUSE, onPause)
		COMMAND_ID_HANDLER(IDC_BTN_REFRESH_FILELIST, onRefresh)
		MESSAGE_HANDLER(WM_HSCROLL, onSliderChangeMaxHashSpeed)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSliderChangeMaxHashSpeed(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedAbort(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChangeMaxHashSpeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		void updateStats();
		
	private:
		bool autoClose;
		bool exitOnDone;
		CProgressBarCtrl progress;
		CButton exitOnDoneButton;
		CTrackBarCtrl slider;
		HICON icon;
		CStatic currentFile;
		CStatic infoFiles;
		CStatic infoSpeed;
		CStatic infoTime;
		bool paused;
		int tempHashSpeed;
		int updatingEditBox;

	public:
		static int instanceCounter;
};

#endif // !defined(HASH_PROGRESS_DLG_H)
