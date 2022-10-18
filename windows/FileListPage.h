#ifndef FILE_LIST_PAGE_H
#define FILE_LIST_PAGE_H

#include "PropPage.h"

class FileListPage : public CPropertyPage<IDD_FILELIST_PAGE>, public PropPage
{
	public:
		explicit FileListPage() : PropPage(TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(SETTINGS_FILE_LIST))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}

		BEGIN_MSG_MAP(FileListPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

		PROPSHEETPAGE* getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_FILE_LIST; }
		void write();
};

#endif // FILE_LIST_PAGE_H
