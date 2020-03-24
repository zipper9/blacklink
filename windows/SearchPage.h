/*
 * FlylinkDC++ // Search Page
 */

#ifndef SEARCH_PAGE_H
#define SEARCH_PAGE_H

#include "ExListViewCtrl.h"
#include "PropPage.h"
#include "wtl_flylinkdc.h"

class SearchPage : public CPropertyPage<IDD_SEARCH_PAGE>, public PropPage
{
	public:
		explicit SearchPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_EXPERTS_ONLY) + _T('\\') + TSTRING(SEARCH))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~SearchPage()
		{
			ctrlList.Detach();
		}
		
		BEGIN_MSG_MAP(SearchPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_SEARCH_FORGET, onFixControls)
		NOTIFY_HANDLER(IDC_ADVANCED_BOOLEANS, NM_CUSTOMDRAW, ctrlList.onCustomDraw)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_SEARCH; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		void fixControls();
		ExListViewCtrl ctrlList;
};

#endif //SEARCH_PAGE_H
