/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#ifndef ABOUT_DLG_INDEX_H
#define ABOUT_DLG_INDEX_H

#include "AboutDlg.h"
#include "AboutCmdsDlg.h"
#include "AboutStatDlg.h"
#include "wtl_flylinkdc.h"
#include "../client/CompiledDateTime.h"

#if _MSC_VER >= 1921
#define MSC_RELEASE 2019
#elif _MSC_VER >= 1910
#define MSC_RELEASE 2017
#elif _MSC_VER >= 1900
#define MSC_RELEASE 2015
#elif _MSC_VER >= 1800
#define MSC_RELEASE 2013
#elif _MSC_VER >= 1700
#define MSC_RELEASE 2012
#elif _MSC_VER >= 1600
#define MSC_RELEASE 2010
#else
#define MSC_RELEASE 1970
#endif

class AboutDlgIndex : public CDialogImpl<AboutDlgIndex>
{
	public:
		enum { IDD = IDD_ABOUTTABS };
		enum
		{
			ABOUT,
			CHAT_COMMANDS,
			STATISTICS,
			VERSION_HISTORY,
		};
		AboutDlgIndex(): m_pTabDialog(0)
		{
		}
		
		BEGIN_MSG_MAP(AboutDlgIndex)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		NOTIFY_HANDLER(IDC_ABOUTTAB, TCN_SELCHANGE, OnSelchangeTabs)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			char fullVersion[64];
			_snprintf(fullVersion, sizeof(fullVersion), "%d (%d)", MSC_RELEASE, _MSC_FULL_VER);
			
			SetDlgItemText(IDC_COMPT, (TSTRING(COMPILED_ON) + _T(' ') + getCompileDate() + _T(' ') + getCompileTime(_T("%H:%M:%S"))
			                           + _T(", Visual C++ ") + Text::toT(fullVersion)).c_str());
			SetWindowText(CTSTRING(MENU_ABOUT));
			ctrlTab.Attach(GetDlgItem(IDC_ABOUTTAB));
			TCITEM tcItem;
			tcItem.mask = TCIF_TEXT | TCIF_PARAM;
			tcItem.iImage = -1;
			
			// Page List. May be optimized to massive
			tcItem.pszText = (LPWSTR) CTSTRING(MENU_ABOUT);
			m_Page1 = std::unique_ptr<AboutDlg>(new AboutDlg());
			tcItem.lParam = (LPARAM)&m_Page1;
			ctrlTab.InsertItem(0, &tcItem);
			m_Page1->Create(ctrlTab.m_hWnd, AboutDlg::IDD);
			
			tcItem.pszText = (LPWSTR) CTSTRING(CHAT_COMMANDS);
			m_Page2 = std::unique_ptr<AboutCmdsDlg>(new AboutCmdsDlg());
			tcItem.lParam = (LPARAM)&m_Page2;
			ctrlTab.InsertItem(1, &tcItem);
			m_Page2->Create(ctrlTab.m_hWnd, AboutCmdsDlg::IDD);
			
			tcItem.pszText = (LPWSTR) CTSTRING(ABOUT_STATISTICS);
			m_Page3 = std::unique_ptr<AboutStatDlg>(new AboutStatDlg());
			tcItem.lParam = (LPARAM)&m_Page3;
			ctrlTab.InsertItem(2, &tcItem);
			m_Page3->Create(ctrlTab.m_hWnd, AboutStatDlg::IDD);
			
			ctrlTab.SetCurSel(m_pTabDialog);
			
			CenterWindow(GetParent());
			
			NMHDR hdr;  // Give PENDEL to View First Tab (from param m_pTabDialog, for start with 2 or other open Tab) :)
			hdr.code = TCN_SELCHANGE;
			hdr.hwndFrom = ctrlTab.m_hWnd;
			hdr.idFrom = IDC_ABOUTTAB;
			::SendMessage(ctrlTab.m_hWnd, WM_NOTIFY, hdr.idFrom, (LPARAM)&hdr);
			
			return TRUE;
		}
		
		LRESULT OnSelchangeTabs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
		{
			int pos = ctrlTab.GetCurSel(); // Selected Tab #
			// Hide All Dialogs
			m_Page1->ShowWindow(SW_HIDE);
			m_Page2->ShowWindow(SW_HIDE);
			m_Page3->ShowWindow(SW_HIDE);
			m_pTabDialog = pos;
			
			CRect rc;
			ctrlTab.GetClientRect(&rc); // Get work Area Rect
			ctrlTab.AdjustRect(FALSE, &rc);    // Clear Tab line from Rect. Do not "TRUE"
			rc.left -= 3;
			
			switch (pos)
			{
				case ABOUT: // About
				{
					m_Page1->MoveWindow(&rc);
					m_Page1->ShowWindow(SW_SHOW);
					break;
				}
				case CHAT_COMMANDS: // Chat Commands
				{
					m_Page2->MoveWindow(&rc);
					m_Page2->ShowWindow(SW_SHOW);
					break;
				}
				case STATISTICS: // UDP, TCP, TLS etc statistics
				{
					m_Page3->MoveWindow(&rc);
					m_Page3->ShowWindow(SW_SHOW);
					break;
				}
			}
			return 1;
		}
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			EndDialog(wID);
			return 0;
		}

	private:
		CTabCtrl ctrlTab;
		int m_pTabDialog;
		
		std::unique_ptr<AboutDlg> m_Page1;
		std::unique_ptr<AboutCmdsDlg> m_Page2;
		std::unique_ptr<AboutStatDlg> m_Page3;
};

#endif // !defined(ABOUT_DLG_INDEX_H)
