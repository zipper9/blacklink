#include "stdafx.h"
#include "SearchDlg.h"
#include "../client/Text.h"
#include "../client/Util.h"
#include "WinUtil.h"

LRESULT SearchDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SEARCH));
	SetDlgItemText(IDC_CAPTION_SEARCH_STRING, CTSTRING(SEARCH_STRING));
	SetDlgItemText(IDC_CAPTION_FILE_TYPE, CTSTRING(FILE_TYPE));
	SetDlgItemText(IDC_CAPTION_MIN_FILE_SIZE, CTSTRING(MIN_FILE_SIZE));
	SetDlgItemText(IDC_CAPTION_MAX_FILE_SIZE, CTSTRING(MAX_FILE_SIZE));
	SetDlgItemText(IDC_CAPTION_SIZE_TYPE, CTSTRING(SIZE_TYPE));
	SetDlgItemText(IDC_MATCH_CASE, CTSTRING(MATCH_CASE));
	SetDlgItemText(IDC_REGEXP, CTSTRING(REGULAR_EXPRESSION));
	SetDlgItemText(IDC_NEW_WINDOW, CTSTRING(OPEN_RESULTS_IN_NEW_WINDOW));
	SetDlgItemText(IDC_CAPTION_SHARED_AT_LAST, CTSTRING(SHARED_AT_LAST));
	SetDlgItemText(IDC_CAPTION_DAYS, CTSTRING(SHARED_DAYS));

	dialogIcon = HIconWrapper(IDR_SEARCH);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);	
	
	ctrlText.Attach(GetDlgItem(IDC_SEARCH_STRING));
	ctrlText.SetWindowText(Text::toT(options.text).c_str());

	ctrlMatchCase.Attach(GetDlgItem(IDC_MATCH_CASE));
	ctrlMatchCase.SetCheck(options.matchCase ? BST_CHECKED : BST_UNCHECKED);

	ctrlRegExp.Attach(GetDlgItem(IDC_REGEXP));
	ctrlRegExp.SetCheck(options.regExp ? BST_CHECKED : BST_UNCHECKED);

	ctrlFileType.Attach(GetDlgItem(IDC_FILE_TYPE));
	ctrlFileType.AddString(CTSTRING(ANY));
	ctrlFileType.AddString(CTSTRING(AUDIO));
	ctrlFileType.AddString(CTSTRING(COMPRESSED));
	ctrlFileType.AddString(CTSTRING(DOCUMENT));
	ctrlFileType.AddString(CTSTRING(EXECUTABLE));
	ctrlFileType.AddString(CTSTRING(PICTURE));
	ctrlFileType.AddString(CTSTRING(VIDEO_AND_SUBTITLES));
	ctrlFileType.AddString(CTSTRING(DIRECTORY));
	ctrlFileType.AddString(_T("TTH"));
	ctrlFileType.AddString(CTSTRING(CD_DVD_IMAGES));
	ctrlFileType.AddString(CTSTRING(COMICS));
	ctrlFileType.AddString(CTSTRING(BOOK));
	ctrlFileType.SetCurSel(options.fileType);

	ctrlMinSize.Attach(GetDlgItem(IDC_MIN_FILE_SIZE));
	if (options.sizeMin > 0) ctrlMinSize.SetWindowText(Util::toStringW(options.sizeMin).c_str());
	
	ctrlMaxSize.Attach(GetDlgItem(IDC_MAX_FILE_SIZE));
	if (options.sizeMax > 0) ctrlMaxSize.SetWindowText(Util::toStringW(options.sizeMax).c_str());

	ctrlSizeUnit.Attach(GetDlgItem(IDC_SIZE_TYPE));
	ctrlSizeUnit.AddString(CTSTRING(B));
	ctrlSizeUnit.AddString(CTSTRING(KB));
	ctrlSizeUnit.AddString(CTSTRING(MB));
	ctrlSizeUnit.AddString(CTSTRING(GB));
	ctrlSizeUnit.SetCurSel(options.sizeUnit);

	ctrlSharedDays.Attach(GetDlgItem(IDC_SHARED_DAYS));
	if (options.sharedDays > 0) ctrlSharedDays.SetWindowText(Util::toStringW(options.sharedDays).c_str());

	ctrlNewWindow.Attach(GetDlgItem(IDC_NEW_WINDOW));
	ctrlNewWindow.SetCheck(options.newWindow ? BST_CHECKED : BST_UNCHECKED);

	CenterWindow(GetParent());

	return 0;
}

LRESULT SearchDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		tstring ts;
		WinUtil::getWindowText(ctrlText, ts);
		options.text = Text::fromT(ts);
		options.matchCase = ctrlMatchCase.GetCheck() == BST_CHECKED;
		options.regExp = ctrlRegExp.GetCheck() == BST_CHECKED;
		options.fileType = ctrlFileType.GetCurSel();

		WinUtil::getWindowText(ctrlMinSize, ts);
		options.sizeMin = ts.empty() ? -1 : Util::toInt64(ts);

		WinUtil::getWindowText(ctrlMaxSize, ts);
		options.sizeMax = ts.empty() ? -1 : Util::toInt64(ts);

		options.sizeUnit = ctrlSizeUnit.GetCurSel();

		WinUtil::getWindowText(ctrlSharedDays, ts);
		options.sharedDays = ts.empty() ? -1 : Util::toInt(ts);

		options.newWindow = ctrlNewWindow.GetCheck() == BST_CHECKED;
	}
	EndDialog(wID);
	return 0;
}

LRESULT SearchDlg::OnClearResults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	clearResultsFlag = true;
	EndDialog(IDOK);
	return 0;
}
