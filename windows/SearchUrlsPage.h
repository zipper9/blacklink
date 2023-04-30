#ifndef SEARCH_URLS_PAGE_H_
#define SEARCH_URLS_PAGE_H_

#include "PropPage.h"
#include "ExListViewCtrl.h"
#include "../client/SearchUrl.h"

class SearchUrlsPage : public CPropertyPage<IDD_SEARCH_URLS_PAGE>, public PropPage
{
	public:
		explicit SearchUrlsPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_WEB_SEARCH))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~SearchUrlsPage()
		{
			ctrlList.Detach();
		}

		BEGIN_MSG_MAP_EX(SearchUrlsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ADD_MENU, onAdd)
		COMMAND_ID_HANDLER(IDC_REMOVE_MENU, onRemove)
		COMMAND_ID_HANDLER(IDC_CHANGE_MENU, onChange)
		COMMAND_ID_HANDLER(IDC_MOVE_UP, onMoveUp)
		COMMAND_ID_HANDLER(IDC_MOVE_DOWN, onMoveDown)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, NM_DBLCLK, onDblClick)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_MENU_ITEMS, NM_CUSTOMDRAW, ctrlList.onCustomDraw)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			swapItems(-1);
			return 0;
		}
		LRESULT onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			swapItems(1);
			return 0;
		}
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);

		LRESULT onDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;

			if (item->iItem >= 0)
			{
				PostMessage(WM_COMMAND, IDC_CHANGE_MENU, 0);
			}
			else if (item->iItem == -1)
			{
				PostMessage(WM_COMMAND, IDC_ADD_MENU, 0);
			}

			return 0;
		}

		PROPSHEETPAGE *getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_WEB_SEARCH; }
		void write() {}

	protected:
		ExListViewCtrl ctrlList;
		SearchUrl::List data;

		void addEntry(const SearchUrl& url, int pos);
		void setEntry(const SearchUrl& url, int pos);
		void insertUrlList();
		void updateButtons();
		void swapItems(int delta);
};

#endif // SEARCH_URLS_PAGE_H_
