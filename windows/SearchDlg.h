#ifndef SEARCH_DLG_H
#define SEARCH_DLG_H

#include <atlctrls.h>
#include <atldlgs.h>
#include "Resource.h"
#include "HIconWrapper.h"
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
	bool onlyNewFiles;
	bool enableSharedDays;

	SearchOptions()
	{
		matchCase = false;
		regExp = false;
		fileType = 0;
		sizeMin = sizeMax = -1;
		sizeUnit = 0;
		sharedDays = 0;
		newWindow = false;
		onlyNewFiles = false;
		enableSharedDays = true;
	}
};

class SearchDlg : public CDialogImpl<SearchDlg>
{
	public:
		enum { IDD = IDD_FILELIST_SEARCH };

		SearchDlg(SearchOptions& options): options(options), clearResultsFlag(false) {}

		BEGIN_MSG_MAP(SearchDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_CLEAR_RESULTS, OnClearResults)
		END_MSG_MAP()

		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnClearResults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		bool clearResults() const { return clearResultsFlag; }
	
	private:
		SearchOptions& options;
		bool clearResultsFlag;

		HIconWrapper dialogIcon;
		CEdit ctrlText;
		CButton ctrlMatchCase;
		CButton ctrlRegExp;
		CEdit ctrlMinSize;
		CEdit ctrlMaxSize;
		CComboBox ctrlFileType;
		CComboBox ctrlSizeUnit;
		CEdit ctrlSharedDays;
		CButton ctrlNewWindow;
		CButton ctrlOnlyNewFiles;
};

#endif /* SEARCH_DLG_H */
