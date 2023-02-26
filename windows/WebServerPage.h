#ifndef WebServerPage_H
#define WebServerPage_H

#include "PropPage.h"
#include "StatusLabelCtrl.h"

class WebServerPage : public CPropertyPage<IDD_WEBSERVER_PAGE>, public PropPage
{
	public:
		explicit WebServerPage() : PropPage(TSTRING(WEBSERVER))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
			hasFiles = passwordsVisible = false;
			passwordChar = 0;
		}

		BEGIN_MSG_MAP(WebServerPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_ENABLE_WEBSERVER, BN_CLICKED, onToggleControls)
		COMMAND_HANDLER(IDC_TOGGLE_PASSWORDS, BN_CLICKED, onShowPasswords)
		END_MSG_MAP()

		PROPSHEETPAGE *getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_SERVER_EX; }
		void write();

	private:
		bool hasFiles;
		bool passwordsVisible;
		TCHAR passwordChar;
		StatusLabelCtrl label;

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onToggleControls(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onShowPasswords(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

		void showPasswords(bool show);
		void toggleControls(bool enable);
};

#endif //WebServerPage_H
