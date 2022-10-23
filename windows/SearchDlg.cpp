#include "stdafx.h"
#include "SearchDlg.h"
#include "WinUtil.h"
#include "ResourceLoader.h"
#include "CustomDrawHelpers.h"
#include "DialogLayout.h"
#include "ExMessageBox.h"
#include "Fonts.h"
#include "../client/Util.h"
#include "../client/SearchManager.h"
#include "../client/BusyCounter.h"

#ifdef OSVER_WIN_XP
#include "../client/CompatibilityManager.h"
#endif

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

SearchHistory SearchDlg::lastSearches;

template<typename C>
bool isTTH(const std::basic_string<C>& s)
{
	if (s.length() != 39) return false;
	return Encoder::isBase32<C>(s.c_str());
}

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_SEARCH_STRING, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_MATCH_CASE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_REGEXP, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_FILE_TYPE, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_MIN_FILE_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_MAX_FILE_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_SIZE_TYPE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_SHARED_AT_LAST, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_DAYS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SKIP_EMPTY, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SKIP_OWNED, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SKIP_CANCELED, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_NEW_WINDOW, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CLEAR_RESULTS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDOK, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDCANCEL, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

SearchDlg::~SearchDlg()
{
	imgSearchTypes.Destroy();
}

LRESULT SearchDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	BusyCounter<bool> busy(initializing);
	SetWindowText(CTSTRING(SEARCH));
	DialogLayout::layout(*this, layoutItems, _countof(layoutItems));

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::SEARCH, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	ResourceLoader::LoadImageList(IDR_SEARCH_TYPES, imgSearchTypes, 16, 16);

	ctrlText.Attach(GetDlgItem(IDC_SEARCH_STRING));
	ctrlText.SetWindowText(Text::toT(options.text).c_str());

	ctrlPurge.Attach(GetDlgItem(IDC_PURGE));
#ifdef OSVER_WIN_XP
	if (!CompatibilityManager::isOsVistaPlus())
		ctrlPurgeSubclass.SubclassWindow(ctrlPurge);
#endif
	ctrlPurge.SetIcon(g_iconBitmaps.getIcon(IconBitmaps::CLEAR, 0));

	ctrlMatchCase.Attach(GetDlgItem(IDC_MATCH_CASE));
	ctrlMatchCase.SetCheck(options.matchCase ? BST_CHECKED : BST_UNCHECKED);

	ctrlRegExp.Attach(GetDlgItem(IDC_REGEXP));
	ctrlRegExp.SetCheck(options.regExp ? BST_CHECKED : BST_UNCHECKED);

	ctrlFileType.Attach(GetDlgItem(IDC_FILE_TYPE));
	for (int i = 0; i < NUMBER_OF_FILE_TYPES; ++i)
		ctrlFileType.AddString(CTSTRING_I(SearchManager::getTypeStr(i)));
	ctrlFileType.SetCurSel(options.fileType);
	if (options.fileType == FILE_TYPE_TTH) autoSwitchToTTH = true;

	ctrlMinSize.Attach(GetDlgItem(IDC_MIN_FILE_SIZE));
	if (options.sizeMin >= 0) ctrlMinSize.SetWindowText(Util::toStringT(options.sizeMin).c_str());

	ctrlMaxSize.Attach(GetDlgItem(IDC_MAX_FILE_SIZE));
	if (options.sizeMax >= 0) ctrlMaxSize.SetWindowText(Util::toStringT(options.sizeMax).c_str());

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

	ctrlSkipEmpty.Attach(GetDlgItem(IDC_SKIP_EMPTY));
	ctrlSkipEmpty.SetCheck(options.skipEmpty ? BST_CHECKED : BST_UNCHECKED);
	ctrlSkipOwned.Attach(GetDlgItem(IDC_SKIP_OWNED));
	ctrlSkipOwned.SetCheck(options.skipOwned ? BST_CHECKED : BST_UNCHECKED);
	ctrlSkipCanceled.Attach(GetDlgItem(IDC_SKIP_CANCELED));
	ctrlSkipCanceled.SetCheck(options.skipCanceled ? BST_CHECKED : BST_UNCHECKED);

	lastSearches.firstLoad(e_FileListSearchHistory);
	const auto& data = lastSearches.getData();
	for (const tstring& s : data)
		ctrlText.AddString(s.c_str());

	CenterWindow(GetParent());

	return 0;
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
			COMBOBOXINFO inf = { sizeof(inf) };
			ctrlText.GetComboBoxInfo(&inf);
			WinUtil::showInputError(inf.hwndItem, TSTRING(INVALID_TTH));
			return 0;
		}

		lastSearches.addItem(ts, SETTING(SEARCH_HISTORY));
		lastSearches.save(e_FileListSearchHistory);
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
		options.skipEmpty = ctrlSkipEmpty.GetCheck() == BST_CHECKED;
		options.skipOwned = ctrlSkipOwned.GetCheck() == BST_CHECKED;
		options.skipCanceled = ctrlSkipCanceled.GetCheck() == BST_CHECKED;
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

LRESULT SearchDlg::onPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool hasHistory = !lastSearches.empty();
	if (hasHistory && BOOLSETTING(CONFIRM_CLEAR_SEARCH_HISTORY))
	{
		UINT check = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(CONFIRM_CLEAR_SEARCH), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION, check) != IDYES)
			return 0;
		if (check == BST_CHECKED)
			SET_SETTING(CONFIRM_CLEAR_SEARCH_HISTORY, FALSE);
	}
	ctrlText.ResetContent();
	if (hasHistory)
		lastSearches.clear(e_FileListSearchHistory);
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
		CustomDrawHelpers::measureComboBox(reinterpret_cast<MEASUREITEMSTRUCT*>(lParam), Fonts::g_dialogFont);
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

void SearchDlg::checkTTH(const tstring& str)
{
	if (isTTH(str))
	{
		ctrlFileType.SetCurSel(FILE_TYPE_TTH);
		autoSwitchToTTH = true;
	}
	else if (autoSwitchToTTH)
	{
		ctrlFileType.SetCurSel(FILE_TYPE_ANY);
		autoSwitchToTTH = false;
	}
}

LRESULT SearchDlg::onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (initializing) return 0;
	tstring str;
	WinUtil::getWindowText(ctrlText, str);
	checkTTH(str);
	return 0;
}

LRESULT SearchDlg::onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = ctrlText.GetCurSel();
	checkTTH(WinUtil::getComboBoxItemText(ctrlText, index));
	return 0;
}
