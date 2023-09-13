#ifndef EMOTICON_PACKS_DLG_H_
#define EMOTICON_PACKS_DLG_H_

#ifdef BL_UI_FEATURE_EMOTICONS

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "../client/typedefs.h"

class EmoticonPacksDlg : public CDialogImpl<EmoticonPacksDlg>
{
	public:
		enum { IDD = IDD_EMOTICON_PACKS };

		BEGIN_MSG_MAP(EmoticonPacksDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_LIST_UP, BN_CLICKED, onMoveUp)
		COMMAND_HANDLER(IDC_LIST_DOWN, BN_CLICKED, onMoveDown)
		COMMAND_ID_HANDLER(IDC_ENABLE, onEnable)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		NOTIFY_HANDLER(IDC_LIST1, LVN_ITEMCHANGED, onListItemChanged)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onEnable(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onMoveUp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */)
		{
			moveSelection(-1);
			return 0;
		}
		LRESULT onMoveDown(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */)
		{
			moveSelection(1);
			return 0;
		}
		LRESULT onListItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

	public:
		struct Item
		{
			tstring name;
			bool enabled;
		};

		bool enabled = true;
		vector<Item> items;

	private:
		CListViewCtrl ctrlList;
		CButton ctrlEnable;
		CButton ctrlUp;
		CButton ctrlDown;
		int initList = 0;

		void updateButtons(BOOL enable);
		void moveSelection(int direction);
};

#endif // BL_UI_FEATURE_EMOTICONS

#endif // EMOTICON_PACKS_DLG_H_
