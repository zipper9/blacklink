#include "stdafx.h"
#include "FindDuplicatesDlg.h"
#include "WinUtil.h"
#include "ResourceLoader.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_CAPTION_MIN_FILE_SIZE, R_(MIN_FILE_SIZE) },
	{ IDC_CAPTION_SIZE_TYPE,     R_(SIZE_TYPE)     },
	{ IDOK,                      R_(OK)            },
	{ IDCANCEL,                  R_(CANCEL)        },
	{ 0,                         R_INVALID         }
};

static const ResourceManager::Strings sizeUnitStrings[] =
{
	R_(B), R_(KB), R_(MB), R_(GB), R_INVALID
};

LRESULT FindDuplicatesDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(FIND_DUPLICATES));
	WinUtil::translate(*this, texts);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::SEARCH, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);	
	
	ctrlMinSize.Attach(GetDlgItem(IDC_MIN_FILE_SIZE));
	if (options.sizeMin > 0) ctrlMinSize.SetWindowText(Util::toStringT(options.sizeMin).c_str());
	
	ctrlSizeUnit.Attach(GetDlgItem(IDC_SIZE_TYPE));
	WinUtil::fillComboBoxStrings(ctrlSizeUnit, sizeUnitStrings);
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
