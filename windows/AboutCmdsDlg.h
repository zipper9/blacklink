/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#if !defined(ABOUT_CMDS_DLG_H)
#define ABOUT_CMDS_DLG_H

#pragma once

#include "wtl_flylinkdc.h"
#include "Commands.h"

class AboutCmdsDlg : public CDialogImpl<AboutCmdsDlg>
{
	public:
		enum { IDD = IDD_ABOUTCMDS };
		
		AboutCmdsDlg() {}
		~AboutCmdsDlg() {}

		AboutCmdsDlg(const AboutCmdsDlg&) = delete;
		AboutCmdsDlg& operator= (const AboutCmdsDlg&) = delete;
		
		BEGIN_MSG_MAP(AboutCmdsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			tstring help = Commands::helpForCEdit();
			
			CEdit ctrlThanks(GetDlgItem(IDC_THANKS));
			ctrlThanks.FmtLines(TRUE);
			ctrlThanks.AppendText(help.c_str(), TRUE);
			ctrlThanks.Detach();
			
			//CenterWindow(GetParent());
			return TRUE;
		}
};

#endif // !defined(ABOUT_CMDS_DLG_H)