/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#ifndef ABOUT_CMDS_DLG_H
#define ABOUT_CMDS_DLG_H

#include "wtl_flylinkdc.h"
#include "Commands.h"

class AboutCmdsDlg : public CDialogImpl<AboutCmdsDlg>
{
	public:
		enum { IDD = IDD_ABOUTCMDS };
		
		BEGIN_MSG_MAP(AboutCmdsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
			tstring help = Commands::helpForCEdit();
			
			CEdit ctrlThanks(GetDlgItem(IDC_THANKS));
			ctrlThanks.FmtLines(TRUE);
			ctrlThanks.AppendText(help.c_str(), TRUE);
			
			return TRUE;
		}
};

#endif // !defined(ABOUT_CMDS_DLG_H)