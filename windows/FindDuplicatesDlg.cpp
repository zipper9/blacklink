#include "stdafx.h"
#include "FindDuplicatesDlg.h"
#include "WinUtil.h"
#include "ResourceLoader.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_CAPTION_MIN_FILE_SIZE, ResourceManager::MIN_FILE_SIZE },
	{ IDC_CAPTION_SIZE_TYPE,     ResourceManager::SIZE_TYPE     },
	{ IDOK,                      ResourceManager::OK            },
	{ IDCANCEL,                  ResourceManager::CANCEL        },
	{ 0,                         ResourceManager::Strings()     }
};

LRESULT FindDuplicatesDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(FIND_DUPLICATES));
	WinUtil::translate(*this, texts);

	dialogIcon = HIconWrapper(IDR_SEARCH);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);	
	
	ctrlMinSize.Attach(GetDlgItem(IDC_MIN_FILE_SIZE));
	if (options.sizeMin > 0) ctrlMinSize.SetWindowText(Util::toStringT(options.sizeMin).c_str());
	
	ctrlSizeUnit.Attach(GetDlgItem(IDC_SIZE_TYPE));
	ctrlSizeUnit.AddString(CTSTRING(B));
	ctrlSizeUnit.AddString(CTSTRING(KB));
	ctrlSizeUnit.AddString(CTSTRING(MB));
	ctrlSizeUnit.AddString(CTSTRING(GB));
	ctrlSizeUnit.SetCurSel(options.sizeUnit);

	CenterWindow(GetParent());

	return 0;
}

LRESULT FindDuplicatesDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		tstring ts;
		WinUtil::getWindowText(ctrlMinSize, ts);
		options.sizeMin = ts.empty() ? -1 : Util::toInt64(ts);
		options.sizeUnit = ctrlSizeUnit.GetCurSel();
	}
	EndDialog(wID);
	return 0;
}
