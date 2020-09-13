#ifndef FIND_DUPLICATES_DLG_H
#define FIND_DUPLICATES_DLG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "HIconWrapper.h"

class FindDuplicatesDlg : public CDialogImpl<FindDuplicatesDlg>
{
	public:
		enum { IDD = IDD_FIND_DUPLICATES };

		struct Options
		{
			int64_t sizeMin;
			int sizeUnit;

			Options()
			{
				sizeMin = -1;
				sizeUnit = 2;
			}
		};
		
		FindDuplicatesDlg(Options& options): options(options) {}

		BEGIN_MSG_MAP(FindDuplicatesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	
	private:
		Options& options;
		HIconWrapper dialogIcon;
		CEdit ctrlMinSize;
		CComboBox ctrlSizeUnit;
};

#endif // FIND_DUPLICATES_DLG_H
