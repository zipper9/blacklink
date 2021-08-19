#ifndef IGNORED_USERS_WINDOW_H_
#define IGNORED_USERS_WINDOW_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "ExListViewCtrl.h"
#include "resource.h"

class IgnoredUsersWindow : public CWindowImpl<IgnoredUsersWindow>
{
	public:
		DECLARE_WND_CLASS_EX(_T("IgnoredUsersWindow"), CS_HREDRAW | CS_VREDRAW, COLOR_3DFACE);

		IgnoredUsersWindow() : xdu(0), ydu(0) {}
		~IgnoredUsersWindow() { images.Destroy(); }

		IgnoredUsersWindow(const IgnoredUsersWindow&) = delete;
		IgnoredUsersWindow& operator= (const IgnoredUsersWindow&) = delete;

		void updateUsers();
		int getCount() const;
		int getMinWidth();

	private:
		BEGIN_MSG_MAP(IgnoredUsersWindow)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		NOTIFY_HANDLER(IDC_IGNORELIST, NM_CUSTOMDRAW, ctrlIgnored.onCustomDraw)
		NOTIFY_HANDLER(IDC_IGNORELIST, LVN_ITEMCHANGED, onIgnoredItemChanged)
		COMMAND_ID_HANDLER(IDC_IGNORE_ADD, onIgnoreAdd)
		COMMAND_ID_HANDLER(IDC_IGNORE_REMOVE, onIgnoreRemove)
		COMMAND_ID_HANDLER(IDC_IGNORE_CLEAR, onIgnoreClear)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onIgnoreAdd(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreRemove(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreClear(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoredItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

		void updateLayout();
		void insertIgnoreList();
		void updateButtons();
		void initMetrics();

	private:
		ExListViewCtrl ctrlIgnored;
		CImageList images;
		CButton ctrlAdd;
		CButton ctrlRemove;
		CButton ctrlClear;
		tstring selectedIgnore;
		int xdu, ydu;
		int buttonWidth[3];
		int buttonHeight;
		int buttonSpace;
		int buttonSpaceLarge;
		int buttonVertSpace;
};

#endif // IGNORED_USERS_WINDOW_H_
