/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#ifndef ABOUT_STAT_DLG_H
#define ABOUT_STAT_DLG_H

#include "wtl_flylinkdc.h"
#include "resource.h"
#include "TimerHelper.h"

class AboutStatDlg : public CDialogImpl<AboutStatDlg>, private TimerHelper
{
	public:
		enum { IDD = IDD_ABOUTSTAT };

		AboutStatDlg() : TimerHelper(m_hWnd)
		{
		}

		BEGIN_MSG_MAP(AboutStatDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		COMMAND_HANDLER(IDC_AUTO_REFRESH, BN_CLICKED, onAutoRefresh)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onAutoRefresh(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onTimer(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);

	private:
		void showText();
};

#endif // !defined(ABOUT_STAT_DLG_H)
