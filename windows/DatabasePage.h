#ifndef DATABASE_PAGE_H
#define DATABASE_PAGE_H

#include "PropPage.h"

class DatabasePage : public CPropertyPage<IDD_DATABASE_PAGE>, public PropPage
{
	public:
		explicit DatabasePage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_EXPERTS_ONLY) + _T('\\') + TSTRING(SETTINGS_DATABASE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
			defaultJournalMode = false;
		}

		BEGIN_MSG_MAP(DatabasePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

		PROPSHEETPAGE* getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_DATABASE; }
		void write();

	private:
		CComboBox ctrlJournal;
		bool defaultJournalMode;
};

#endif // DATABASE_PAGE_H
