/**
* Dialog "Add / Change Favorite Directory".
*/

#ifndef FAV_DIR_DLG_H
#define FAV_DIR_DLG_H

#include <atlcrack.h>
#include "../client/Util.h"
#include "WinUtil.h"
#include "resource.h"

class FavDirDlg : public CDialogImpl<FavDirDlg>
{
	public:
		tstring name;
		tstring dir;
		tstring extensions;
		
		enum { IDD = IDD_FAVORITEDIR };
		
		BEGIN_MSG_MAP_EX(FavDirDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_FAVDIR_BROWSE, OnBrowse);
		END_MSG_MAP()
		
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			SetWindowText(CTSTRING(SETTINGS_ADD_FAVORITE_DIRS));
			SetDlgItemText(IDC_FAV_NAME2, CTSTRING(SETTINGS_NAME2));
			SetDlgItemText(IDC_GET_FAVORITE_DIR, CTSTRING(SETTINGS_GET_FAVORITE_DIR));
			SetDlgItemText(IDC_FAVORITE_DIR_EXT, CTSTRING(SETTINGS_FAVORITE_DIR_EXT));
			SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));
			SetDlgItemText(IDOK, CTSTRING(OK));
			
			ctrlName.Attach(GetDlgItem(IDC_FAVDIR_NAME));
			ctrlDirectory.Attach(GetDlgItem(IDC_FAVDIR));
			ctrlExtensions.Attach(GetDlgItem(IDC_FAVDIR_EXTENSION));
			
			ctrlName.SetWindowText(name.c_str());
			ctrlDirectory.SetWindowText(dir.c_str());
			ctrlExtensions.SetWindowText(extensions.c_str());
			ctrlName.SetFocus();
			
			CenterWindow(GetParent());
			return 0;
		}
		
		LRESULT OnBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
		{
			tstring target;
			if (!dir.empty())
				target = dir;
			if (WinUtil::browseDirectory(target, m_hWnd))
			{
				ctrlDirectory.SetWindowText(target.c_str());
				if (ctrlName.GetWindowTextLength() == 0)
					ctrlName.SetWindowText(Util::getLastDir(target).c_str());
			}
			return 0;
		}
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (wID == IDOK)
			{
				if ((ctrlName.GetWindowTextLength() == 0) || (ctrlDirectory.GetWindowTextLength() == 0))
				{
					MessageBox(CTSTRING(NAME_OR_DIR_NOT_EMPTY), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
					return 0;
				}
				
				WinUtil::getWindowText(ctrlName, name);
				WinUtil::getWindowText(ctrlDirectory, dir);
				WinUtil::getWindowText(ctrlExtensions, extensions);
			}
			
			EndDialog(wID);
			return 0;
		}
		
	private:
		CEdit ctrlName;
		CEdit ctrlDirectory;
		CEdit ctrlExtensions;
};

#endif // !defined(FAV_DIR_DLG_H)
