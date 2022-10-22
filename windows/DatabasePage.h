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
			currentJournalMode = -1;
		}

		BEGIN_MSG_MAP(DatabasePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_JOURNAL_MODE, CBN_SELCHANGE, onSetJournalMode)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSetJournalMode(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		PROPSHEETPAGE* getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_DATABASE; }
		void write();

	private:
		CComboBox ctrlJournal;
		bool defaultJournalMode;
		int currentJournalMode;
};

#endif // DATABASE_PAGE_H
