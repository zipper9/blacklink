#ifndef FAV_USER_DLG_H_
#define FAV_USER_DLG_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "../client/Text.h"
#include "../client/CID.h"

class FavUserDlg: public CDialogImpl<FavUserDlg>
{
	public:
		enum { IDD = IDD_FAVUSER };

		BEGIN_MSG_MAP(FavUserDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_UPLOAD_SPEED, CBN_SELCHANGE, onSelChange)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	
	private:
		CEdit ctrlDesc;
		CButton ctrlAutoGrant;
		CComboBox ctrlUpload;
		CEdit ctrlSpeedValue;
		CComboBox ctrlShareGroup;
		CComboBox ctrlPMHandling;

		void updateSpeedCtrl();

		struct ShareGroupInfo
		{
			CID id;
			tstring name;
			int def;
		};

		vector<ShareGroupInfo> shareGroups;

	public:
		tstring title;
		tstring description;
		int speedLimit = 0;
		uint32_t flags = 0;
		CID shareGroup;
};

#endif // FAV_USER_DLG_H_
