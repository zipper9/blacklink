
/*
 * ApexDC speedmod (c) SMT 2007
 */

#ifndef MESSAGES_PAGE_H
#define MESSAGES_PAGE_H

#include "PropPage.h"

class MessagesPage : public CPropertyPage<IDD_MESSAGES_PAGE>, public PropPage
{
	public:
		explicit MessagesPage() : PropPage(TSTRING(SETTINGS_MESSAGES))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP(MessagesPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TIME_AWAY, onFixControls)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_USER_WAITING; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	protected:
		void fixControls();
};

#endif //MESSAGES_PAGE_H
