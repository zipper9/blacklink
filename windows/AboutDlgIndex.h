/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#ifndef ABOUT_DLG_INDEX_H
#define ABOUT_DLG_INDEX_H

#include "AboutDlg.h"
#include "AboutCmdsDlg.h"
#include "AboutStatDlg.h"
#include "wtl_flylinkdc.h"

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
		AboutDlgIndex(): selectedTab(0)
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
			SetWindowText(CTSTRING(MENU_ABOUT));
			ctrlTab.Attach(GetDlgItem(IDC_ABOUTTAB));
			TCITEM tcItem;
			tcItem.mask = TCIF_TEXT | TCIF_PARAM;
			tcItem.iImage = -1;
			
			tcItem.pszText = (LPWSTR) CTSTRING(MENU_ABOUT);
			page1 = std::unique_ptr<AboutDlg>(new AboutDlg());
			tcItem.lParam = (LPARAM)&page1;
			ctrlTab.InsertItem(0, &tcItem);
			page1->Create(ctrlTab.m_hWnd, AboutDlg::IDD);
			
			tcItem.pszText = (LPWSTR) CTSTRING(CHAT_COMMANDS);
			page2 = std::unique_ptr<AboutCmdsDlg>(new AboutCmdsDlg());
			tcItem.lParam = (LPARAM)&page2;
			ctrlTab.InsertItem(1, &tcItem);
			page2->Create(ctrlTab.m_hWnd, AboutCmdsDlg::IDD);
			
			tcItem.pszText = (LPWSTR) CTSTRING(ABOUT_STATISTICS);
			page3 = std::unique_ptr<AboutStatDlg>(new AboutStatDlg());
			tcItem.lParam = (LPARAM)&page3;
			ctrlTab.InsertItem(2, &tcItem);
			page3->Create(ctrlTab.m_hWnd, AboutStatDlg::IDD);
			
			ctrlTab.SetCurSel(selectedTab);
			
			CenterWindow(GetParent());
			
			NMHDR hdr;
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
			page1->ShowWindow(SW_HIDE);
			page2->ShowWindow(SW_HIDE);
			page3->ShowWindow(SW_HIDE);
			selectedTab = pos;
			
			CRect rc;
			ctrlTab.GetClientRect(&rc);
			ctrlTab.AdjustRect(FALSE, &rc);
			rc.left -= 3;
			
			switch (pos)
			{
				case ABOUT: // About
				{
					page1->MoveWindow(&rc);
					page1->ShowWindow(SW_SHOW);
					break;
				}
				case CHAT_COMMANDS: // Chat Commands
				{
					page2->MoveWindow(&rc);
					page2->ShowWindow(SW_SHOW);
					break;
				}
				case STATISTICS: // UDP, TCP, TLS etc statistics
				{
					page3->MoveWindow(&rc);
					page3->ShowWindow(SW_SHOW);
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
		int selectedTab;
		
		std::unique_ptr<AboutDlg> page1;
		std::unique_ptr<AboutCmdsDlg> page2;
		std::unique_ptr<AboutStatDlg> page3;
};

#endif // !defined(ABOUT_DLG_INDEX_H)
