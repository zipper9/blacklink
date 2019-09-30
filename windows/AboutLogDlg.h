/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#if !defined(ABOUT_LOG_DLG_H)
#define ABOUT_LOG_DLG_H


#pragma once


#include "wtl_flylinkdc.h"
#include "ExListViewCtrl.h"

class AboutLogDlg : public CDialogImpl<AboutLogDlg>
{
	public:
		enum { IDD = IDD_ABOUTCMDS };
		
		AboutLogDlg() {}
		~AboutLogDlg() {}
		
		BEGIN_MSG_MAP(AboutLogDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		NOTIFY_CODE_HANDLER(EN_LINK, OnLink)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			LRESULT lResult = GetDlgItem(IDC_THANKS).SendMessage(EM_GETEVENTMASK, 0, 0);
			GetDlgItem(IDC_THANKS).SendMessage(EM_SETEVENTMASK, 0, lResult | ENM_LINK);
			GetDlgItem(IDC_THANKS).SendMessage(EM_AUTOURLDETECT, TRUE, 0);
			
			return TRUE;
		}
		
		LRESULT OnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
		{
			const ENLINK* pL = (const ENLINK*)pnmh;
			if (WM_LBUTTONDOWN == pL->msg)
			{
				LocalArray<TCHAR, MAX_PATH> buf;
				CRichEditCtrl(pL->nmhdr.hwndFrom).GetTextRange(pL->chrg.cpMin, pL->chrg.cpMax, buf.data());
				WinUtil::openFile(buf.data());
			}
			else
				bHandled = FALSE;
			return 0;
		}
};

#endif // !defined(ABOUT_LOG_DLG_H)
