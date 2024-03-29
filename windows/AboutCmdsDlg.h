/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#ifndef ABOUT_CMDS_DLG_H
#define ABOUT_CMDS_DLG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atldlgs.h>
#include <atlcrack.h>
#include "resource.h"

class AboutCmdsDlg : public CDialogImpl<AboutCmdsDlg>
{
	public:
		enum { IDD = IDD_ABOUTCMDS };

		BEGIN_MSG_MAP(AboutCmdsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()

		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
};

#endif // !defined(ABOUT_CMDS_DLG_H)
