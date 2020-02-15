/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#ifndef SHARE_MISC_PAGE_H
#define SHARE_MISC_PAGE_H

#include <atlcrack.h>
#include "PropPage.h"

class ShareMiscPage : public CPropertyPage<IDD_SHARE_MISC_PAGE>, public PropPage
{
	public:
		explicit ShareMiscPage() : PropPage(TSTRING(SETTINGS_UPLOADS) + _T('\\') + TSTRING(SETTINGS_ADVANCED))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP(ShareMiscPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TTH_IN_STREAM, onFixControls)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_UPLOAD_ADVANCED; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		void fixControls();
};

#endif //SHARE_MISC_PAGE_H
