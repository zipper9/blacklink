#ifndef SEARCH_DLG_H
#define SEARCH_DLG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
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
		clear();
	}

	void clear()
	{
		text.clear();
		matchCase = false;
		regExp = false;
		fileType = 0;
		sizeMin = sizeMax = -1;
		sizeUnit = 2;
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

		SearchDlg(SearchOptions& options):
			options(options), clearResultsFlag(false), autoSwitchToTTH(false), initializing(true) {}
		~SearchDlg();

		BEGIN_MSG_MAP(SearchDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_DRAWITEM, onDrawItem)
		MESSAGE_HANDLER(WM_MEASUREITEM, onMeasureItem)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_CLEAR_RESULTS, onClearResults)
		COMMAND_HANDLER(IDC_SEARCH_STRING, EN_CHANGE, onEditChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClearResults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onMeasureItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		bool clearResults() const { return clearResultsFlag; }

	private:
		SearchOptions& options;
		bool clearResultsFlag;
		bool autoSwitchToTTH;
		bool initializing;

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
		CImageList imgSearchTypes;
};

#endif /* SEARCH_DLG_H */
