#ifndef LIMIT_PAGE_H_
#define LIMIT_PAGE_H_

#include "PropPage.h"

class LimitPage : public CPropertyPage<IDD_LIMIT_PAGE>, public PropPage
{
	public:
		explicit LimitPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_LIMIT))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP_EX(LimitPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TIME_LIMITING, onChangeCont)
		COMMAND_ID_HANDLER(IDC_THROTTLE_ENABLE, onChangeCont)
		COMMAND_ID_HANDLER(IDC_DISCONNECTING_ENABLE, onChangeCont)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onChangeCont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_SPEED; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		CComboBox timeCtrlBegin, timeCtrlEnd;
		
		void fixControls();
};

#endif // LIMIT_PAGE_H_
