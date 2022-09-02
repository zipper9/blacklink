#ifndef HELP_TEXT_DLG_H_
#define HELP_TEXT_DLG_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "RichTextLabel.h"

class HelpTextDlg : public CDialogImpl<HelpTextDlg>
{
	public:
		HelpTextDlg() : notifWnd(nullptr) {}

		enum { IDD = IDD_HELP_TEXT };

		BEGIN_MSG_MAP_EX(HelpTextDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			CRect rc;
			GetClientRect(&rc);
			int xdu, ydu;
			WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
			int hmargin = WinUtil::dialogUnitsToPixelsX(8, xdu);
			int vmargin = WinUtil::dialogUnitsToPixelsY(6, ydu);
			ctrlText.setMargins(hmargin, vmargin, hmargin, vmargin);
			ctrlText.Create(m_hWnd, rc);
			ctrlText.SetFont(Fonts::g_systemFont);
			ctrlText.SetWindowText(CTSTRING(ADL_BRIEF));
			SetWindowText(CTSTRING(ADL_TITLE));
			return 0;
		}

		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (notifWnd) ::PostMessage(notifWnd, WMU_DIALOG_CLOSED, 0, 0);
			return 0;
		}

		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (notifWnd) ::PostMessage(notifWnd, WMU_DIALOG_CLOSED, 0, 0);
			return 0;
		}

		void setNotifWnd(HWND hWnd)
		{
			notifWnd = hWnd;
		}

	private:
		RichTextLabel ctrlText;
		HWND notifWnd;
};

#endif // HELP_TEXT_DLG_H_
