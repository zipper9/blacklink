#ifndef SEARCH_DLG_H
#define SEARCH_DLG_H

#include <atlctrls.h>
#include <atldlgs.h>
#include "Resource.h"
#include <string>

struct SearchOptions
{
	std::string text;
	bool matchCase;
	bool regExp;
	int fileType;
	int64_t sizeMin, sizeMax;
	int sizeUnit;
	int sharedDays;
	bool newWindow;

	SearchOptions()
	{
		matchCase = false;
		regExp = false;
		fileType = 0;
		sizeMin = sizeMax = -1;
		sizeUnit = 0;
		sharedDays = 0;
		newWindow = false;
	}
};

class SearchDlg : public CDialogImpl<SearchDlg>
{
	public:
		enum { IDD = IDD_FILELIST_SEARCH };

		SearchDlg(SearchOptions& options): options(options) {}

		BEGIN_MSG_MAP(SearchDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		END_MSG_MAP()

		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	private:
		SearchOptions& options;

		CEdit ctrlText;
		CButton ctrlMatchCase;
		CButton ctrlRegExp;
		CEdit ctrlMinSize;
		CEdit ctrlMaxSize;
		CComboBox ctrlFileType;
		CComboBox ctrlSizeUnit;
		CEdit ctrlSharedDays;
		CButton ctrlNewWindow;
};

#endif /* SEARCH_DLG_H */
