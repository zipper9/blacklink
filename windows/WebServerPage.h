#ifndef WebServerPage_H
#define WebServerPage_H

#include "PropPage.h"

class WebServerPage : public CPropertyPage<IDD_WEBSERVER_PAGE>, public PropPage
{
	public:
		explicit WebServerPage() : PropPage(TSTRING(SETTINGS_RC) + _T('\\') + TSTRING(WEBSERVER))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP(WebServerPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_SERVER_EX; }
		void write();
		void cancel()
		{
			cancel_check();
		}
};

#endif //WebServerPage_H
