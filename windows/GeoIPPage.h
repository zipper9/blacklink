#ifndef GEOIP_PAGE_H
#define GEOIP_PAGE_H

#include "PropPage.h"

class GeoIPPage : public CPropertyPage<IDD_GEOIP_PAGE>, public PropPage
{
	public:
		explicit GeoIPPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_GEOIP_DATABASE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}

		BEGIN_MSG_MAP(GeoIPPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_GEOIP_AUTO_DOWNLOAD, onAutoDownload)
		COMMAND_ID_HANDLER(IDC_GEOIP_DOWNLOAD, onDownload)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onAutoDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			fixControls();
			return 0;
		}
		LRESULT onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		PROPSHEETPAGE* getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_GEOIP; }
		void write();
		void cancel() { cancel_check(); }
		void onTimer() { updateButtonState(); }

	private:
		void fixControls();
		void updateButtonState();
};

#endif // GEOIP_PAGE_H
