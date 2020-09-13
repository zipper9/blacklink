#include "stdafx.h"
#include "DuplicateFilesDlg.h"
#include "WinUtil.h"
#include "ResourceLoader.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_GOTO,  ResourceManager::GOTO      },
	{ IDCANCEL,  ResourceManager::CLOSE     },
	{ 0,         ResourceManager::Strings() }
};

DuplicateFilesDlg::DuplicateFilesDlg(const DirectoryListing* dl, const std::list<DirectoryListing::File*>& files, const DirectoryListing::File* selFile) :
	dl(dl), selFile(selFile), goToFile(nullptr)
{
	this->files.reserve(files.size());
	for (auto& file : files) this->files.push_back(file);
}

LRESULT DuplicateFilesDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DlgResize_Init();
	SetWindowText(CTSTRING(DUPLICATE_FILES));
	WinUtil::translate(*this, texts);

	dialogIcon = HIconWrapper(IDR_FILE_LIST);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	ctrlList.Attach(GetDlgItem(IDC_LIST1));
	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlList);
	SET_LIST_COLOR_IN_SETTING(ctrlList);

	CRect rc;
	ctrlList.GetClientRect(rc);
	ctrlList.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);

	int count = 0;
	for (const DirectoryListing::File* file : files)
	{
		string path = dl->getPath(file->getParent()) + file->getName();
		ctrlList.InsertItem(count, Text::toT(path).c_str());
		if (file == selFile) ctrlList.SelectItem(count);
		count++;
	}
	
	CenterWindow(GetParent());

	return 0;
}

LRESULT DuplicateFilesDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT DuplicateFilesDlg::onSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CDialogResize<DuplicateFilesDlg>::OnSize(uMsg, wParam, lParam, bHandled);
	CRect rc;
	ctrlList.GetClientRect(rc);
	ctrlList.SetColumnWidth(0, rc.Width());
	return 0;
}

LRESULT DuplicateFilesDlg::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = reinterpret_cast<NM_LISTVIEW*>(pnmh);
	GetDlgItem(IDC_GOTO).EnableWindow(lv->uNewState & LVIS_FOCUSED);
	return 0;
}

LRESULT DuplicateFilesDlg::onGoTo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	goToSelectedFile();
	return 0;
}

LRESULT DuplicateFilesDlg::onDoubleClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	goToSelectedFile();
	return 0;
}

LRESULT DuplicateFilesDlg::onEnter(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	goToSelectedFile();
	return 0;
}

void DuplicateFilesDlg::goToSelectedFile()
{
	int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	if (i < 0 || i >= (int) files.size()) return;
	goToFile = files[i];
	EndDialog(IDOK);
}
