#include "stdafx.h"
#include "SearchDlg.h"
#include "WinUtil.h"
#include "ResourceLoader.h"
#include "CustomDrawHelpers.h"
#include "../client/Util.h"
#include "../client/SearchManager.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_CAPTION_SEARCH_STRING,  ResourceManager::SEARCH_STRING              },
	{ IDC_CAPTION_FILE_TYPE,      ResourceManager::FILE_TYPE                  },
	{ IDC_CAPTION_MIN_FILE_SIZE,  ResourceManager::MIN_FILE_SIZE              },
	{ IDC_CAPTION_MAX_FILE_SIZE,  ResourceManager::MAX_FILE_SIZE              },
	{ IDC_CAPTION_SIZE_TYPE,      ResourceManager::SIZE_TYPE                  },
	{ IDC_MATCH_CASE,             ResourceManager::MATCH_CASE                 },
	{ IDC_REGEXP,                 ResourceManager::REGULAR_EXPRESSION         },
	{ IDC_ONLY_NEW_FILES,         ResourceManager::ONLY_FILES_I_DONT_HAVE     },
	{ IDC_NEW_WINDOW,             ResourceManager::OPEN_RESULTS_IN_NEW_WINDOW },
	{ IDC_CAPTION_SHARED_AT_LAST, ResourceManager::SHARED_AT_LAST             },
	{ IDC_CAPTION_DAYS,           ResourceManager::SHARED_DAYS                },
	{ IDOK,                       ResourceManager::OK                         },
	{ IDCANCEL,                   ResourceManager::CANCEL                     },
	{ 0,                          ResourceManager::Strings()                  }
};

SearchDlg::~SearchDlg()
{
	imgSearchTypes.Destroy();
}

LRESULT SearchDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SEARCH));
	WinUtil::translate(*this, texts);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::SEARCH, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);	
	
	ResourceLoader::LoadImageList(IDR_SEARCH_TYPES, imgSearchTypes, 16, 16);

	ctrlText.Attach(GetDlgItem(IDC_SEARCH_STRING));
	ctrlText.SetWindowText(Text::toT(options.text).c_str());

	ctrlMatchCase.Attach(GetDlgItem(IDC_MATCH_CASE));
	ctrlMatchCase.SetCheck(options.matchCase ? BST_CHECKED : BST_UNCHECKED);

	ctrlRegExp.Attach(GetDlgItem(IDC_REGEXP));
	ctrlRegExp.SetCheck(options.regExp ? BST_CHECKED : BST_UNCHECKED);

	ctrlFileType.Attach(GetDlgItem(IDC_FILE_TYPE));
	for (int i = 0; i < NUMBER_OF_FILE_TYPES; ++i)
		ctrlFileType.AddString(CTSTRING_I(SearchManager::getTypeStr(i)));
	ctrlFileType.SetCurSel(options.fileType);

	ctrlMinSize.Attach(GetDlgItem(IDC_MIN_FILE_SIZE));
	if (options.sizeMin > 0) ctrlMinSize.SetWindowText(Util::toStringT(options.sizeMin).c_str());
	
	ctrlMaxSize.Attach(GetDlgItem(IDC_MAX_FILE_SIZE));
	if (options.sizeMax > 0) ctrlMaxSize.SetWindowText(Util::toStringT(options.sizeMax).c_str());

	ctrlSizeUnit.Attach(GetDlgItem(IDC_SIZE_TYPE));
	ctrlSizeUnit.AddString(CTSTRING(B));
	ctrlSizeUnit.AddString(CTSTRING(KB));
	ctrlSizeUnit.AddString(CTSTRING(MB));
	ctrlSizeUnit.AddString(CTSTRING(GB));
	ctrlSizeUnit.SetCurSel(options.sizeUnit);

	ctrlSharedDays.Attach(GetDlgItem(IDC_SHARED_DAYS));
	if (options.enableSharedDays)
	{
		if (options.sharedDays > 0)
			ctrlSharedDays.SetWindowText(Util::toStringT(options.sharedDays).c_str());
	}
	else
		ctrlSharedDays.EnableWindow(FALSE);		

	ctrlNewWindow.Attach(GetDlgItem(IDC_NEW_WINDOW));
	ctrlNewWindow.SetCheck(options.newWindow ? BST_CHECKED : BST_UNCHECKED);

	ctrlOnlyNewFiles.Attach(GetDlgItem(IDC_ONLY_NEW_FILES));
	ctrlOnlyNewFiles.SetCheck(options.onlyNewFiles ? BST_CHECKED : BST_UNCHECKED);

	CenterWindow(GetParent());

	return 0;
}

inline static bool isTTHChar(char c)
{
	return (c >= '2' && c <= '7') || (c >= 'A' && c <= 'Z');
}

static bool isTTH(const string& str)
{
	if (str.length() != 39) return false;
	for (char c : str)
		if (!isTTHChar(c)) return false;
	return true;
}

LRESULT SearchDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		tstring ts;
		WinUtil::getWindowText(ctrlText, ts);
		string text = Text::fromT(ts);
		int fileType = ctrlFileType.GetCurSel();
		
		if (fileType == FILE_TYPE_TTH && !isTTH(text))
		{
			MessageBox(CTSTRING(INVALID_TTH), CTSTRING(SEARCH), MB_OK | MB_ICONWARNING);
			return 0;
		}

		options.text = std::move(text); 
		options.matchCase = ctrlMatchCase.GetCheck() == BST_CHECKED;
		options.regExp = ctrlRegExp.GetCheck() == BST_CHECKED;
		options.fileType = fileType;

		WinUtil::getWindowText(ctrlMinSize, ts);
		options.sizeMin = ts.empty() ? -1 : Util::toInt64(ts);

		WinUtil::getWindowText(ctrlMaxSize, ts);
		options.sizeMax = ts.empty() ? -1 : Util::toInt64(ts);

		options.sizeUnit = ctrlSizeUnit.GetCurSel();

		WinUtil::getWindowText(ctrlSharedDays, ts);
		options.sharedDays = ts.empty() ? -1 : Util::toInt(ts);

		options.newWindow = ctrlNewWindow.GetCheck() == BST_CHECKED;
		options.onlyNewFiles = ctrlOnlyNewFiles.GetCheck() == BST_CHECKED;
	}
	EndDialog(wID);
	return 0;
}

LRESULT SearchDlg::onClearResults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	clearResultsFlag = true;
	EndDialog(IDOK);
	return 0;
}

LRESULT SearchDlg::onDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == IDC_FILE_TYPE)
	{
		CustomDrawHelpers::drawComboBox(ctrlFileType, reinterpret_cast<const DRAWITEMSTRUCT*>(lParam), imgSearchTypes);
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT SearchDlg::onMeasureItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == IDC_FILE_TYPE)
	{
		auto mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
		mis->itemHeight = 16;
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}
