#ifndef RemoteControlPage_H
#define RemoteControlPage_H

#include "PropPage.h"

class RemoteControlPage : public CPropertyPage<IDD_REMOTE_CONTROL_PAGE>, public PropPage
{
	public:
		explicit RemoteControlPage() : PropPage(TSTRING(SETTINGS_RC))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~RemoteControlPage()
		{
		}
		
		BEGIN_MSG_MAP(RemoteControlPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_SERVER; }
		void write();
};

#endif //RemoteControlPage_H
