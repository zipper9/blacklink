/*
 * FlylinkDC++ // Search Page
 */

#ifndef SEARCH_PAGE_H
#define SEARCH_PAGE_H

#include "PropPage.h"

class SearchPage : public CPropertyPage<IDD_SEARCH_PAGE>, public PropPage
{
	public:
		explicit SearchPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_EXPERTS_ONLY) + _T('\\') + TSTRING(SEARCH))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP(SearchPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_SAVE_SEARCH, onFixControls)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_SEARCH; }
		void write();

	private:
		void fixControls();
		CListViewCtrl ctrlList;
};

#endif //SEARCH_PAGE_H
