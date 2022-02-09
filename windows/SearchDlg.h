#ifndef SEARCH_DLG_H
#define SEARCH_DLG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "SearchHistory.h"

#ifdef OSVER_WIN_XP
#include "ImageButton.h"
#endif

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
	bool skipEmpty;
	bool skipOwned;
	bool skipCanceled;
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
		skipEmpty = true;
		skipOwned = false;
		skipCanceled = false;
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
		COMMAND_ID_HANDLER(IDC_PURGE, onPurge)
		COMMAND_HANDLER(IDC_SEARCH_STRING, CBN_EDITCHANGE, onEditChange)
		COMMAND_HANDLER(IDC_SEARCH_STRING, CBN_SELCHANGE, onSelChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClearResults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onMeasureItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		bool clearResults() const { return clearResultsFlag; }

		static SearchHistory lastSearches;

	private:
		SearchOptions& options;
		bool clearResultsFlag;
		bool autoSwitchToTTH;
		bool initializing;

		CComboBox ctrlText;
		CButton ctrlMatchCase;
		CButton ctrlRegExp;
		CEdit ctrlMinSize;
		CEdit ctrlMaxSize;
		CComboBox ctrlFileType;
		CComboBox ctrlSizeUnit;
		CEdit ctrlSharedDays;
		CButton ctrlNewWindow;
		CButton ctrlSkipEmpty;
		CButton ctrlSkipOwned;
		CButton ctrlSkipCanceled;
		CButton ctrlPurge;
#ifdef OSVER_WIN_XP
		ImageButton ctrlPurgeSubclass;
#endif
		CImageList imgSearchTypes;

		void checkTTH(const tstring& str);
};

#endif /* SEARCH_DLG_H */
