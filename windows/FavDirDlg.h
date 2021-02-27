/**
* Dialog "Add / Change Favorite Directory".
*/

#ifndef FAV_DIR_DLG_H
#define FAV_DIR_DLG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "../client/typedefs.h"

class FavDirDlg : public CDialogImpl<FavDirDlg>
{
	public:
		FavDirDlg(bool newItem) : newItem(newItem) {}

		tstring name;
		tstring dir;
		tstring extensions;
		
		enum { IDD = IDD_FAVORITEDIR };
		
		BEGIN_MSG_MAP_EX(FavDirDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_FAVDIR_BROWSE, onBrowse);
		COMMAND_HANDLER(IDC_FAVDIR_NAME, EN_CHANGE, onChange)
		COMMAND_HANDLER(IDC_FAVDIR, EN_CHANGE, onChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
	private:
		const  bool newItem;
		CEdit ctrlName;
		CEdit ctrlDirectory;
		CEdit ctrlExtensions;
};

#endif // !defined(FAV_DIR_DLG_H)
