#ifndef SLOT_PAGE_H
#define SLOT_PAGE_H

#include "PropPage.h"

class SlotPage : public CPropertyPage<IDD_SLOT_PAGE>, public PropPage
{
	public:
		explicit SlotPage() : PropPage(TSTRING(SLOTS))
#ifdef SSA_IPGRANT_FEATURE
			, m_isEnabledIPGrant(false)
#endif
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP_EX(SlotPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
#ifdef SSA_IPGRANT_FEATURE
		COMMAND_ID_HANDLER(IDC_EXTRA_SLOT_BY_IP, onFixControls)
#endif
		REFLECT_NOTIFICATIONS()
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#ifdef SSA_IPGRANT_FEATURE
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			fixControls();
			return 0;
		}
#endif
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_UPLOAD_EX; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	protected:
#ifdef SSA_IPGRANT_FEATURE
		void fixControls();
#endif
		
#ifdef SSA_IPGRANT_FEATURE
	private:
		string m_IPGrant;
		string m_IPGrantPATH;
		bool m_isEnabledIPGrant;
#endif
};

#endif // !defined(SLOT_PAGE_H)
