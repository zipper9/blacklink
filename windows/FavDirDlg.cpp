#include "stdafx.h"
#include "FavDirDlg.h"
#include "WinUtil.h"
#include "BrowseFile.h"
#include "../client/PathUtil.h"
#include "../client/Util.h"
#include <boost/algorithm/string.hpp>

static const WinUtil::TextItem texts[] =
{
	{ IDC_PREV_NAME,         ResourceManager::SETTINGS_NAME             },
	{ IDC_GET_FAVORITE_DIR,  ResourceManager::SETTINGS_TARGET_DIR       },
	{ IDC_FAVORITE_DIR_EXT,  ResourceManager::SETTINGS_EXTENSION_LIST   },
	{ IDC_FAVORITE_DIR_EXT2, ResourceManager::SETTINGS_EXT_LIST_EXAMPLE },
	{ IDOK,                  ResourceManager::OK                        },
	{ IDCANCEL,              ResourceManager::CANCEL                    },
	{ 0,                     ResourceManager::Strings()                 }
};

LRESULT FavDirDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(newItem ? CTSTRING(SETTINGS_ADD_FAVORITE_DIR) : CTSTRING(SETTINGS_EDIT_FAVORITE_DIR));
	WinUtil::translate(*this, texts);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::FAVORITE_DIRS, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	ctrlName.Attach(GetDlgItem(IDC_FAVDIR_NAME));
	ctrlDirectory.Attach(GetDlgItem(IDC_FAVDIR));
	ctrlExtensions.Attach(GetDlgItem(IDC_FAVDIR_EXTENSION));

	ctrlName.SetWindowText(name.c_str());
	ctrlDirectory.SetWindowText(dir.c_str());
	ctrlExtensions.SetWindowText(extensions.c_str());
	ctrlName.SetFocus();

	CenterWindow(GetParent());
	return 0;
}

LRESULT FavDirDlg::onBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
{
	tstring target;
	if (!dir.empty())
		target = dir;
	if (WinUtil::browseDirectory(target, m_hWnd))
	{
		ctrlDirectory.SetWindowText(target.c_str());
		if (ctrlName.GetWindowTextLength() == 0)
			ctrlName.SetWindowText(Util::getLastDir(target).c_str());
	}
	return 0;
}

LRESULT FavDirDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(ctrlName, name);
		WinUtil::getWindowText(ctrlDirectory, dir);
		boost::trim(name);
		boost::trim(dir);
		if (name.empty() || dir.empty())
		{
			MessageBox(CTSTRING(NAME_OR_DIR_NOT_EMPTY), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
			return 0;
		}
		WinUtil::getWindowText(ctrlExtensions, extensions);
		boost::replace_all(extensions, _T(" "), _T(""));
		boost::replace_all(extensions, _T(","), _T(";"));
	}

	EndDialog(wID);
	return 0;
}

LRESULT FavDirDlg::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring s;
	WinUtil::getWindowText(ctrlName, s);
	boost::trim(s);
	if (s.empty())
	{
		GetDlgItem(IDOK).EnableWindow(FALSE);
		return 0;
	}
	WinUtil::getWindowText(ctrlDirectory, s);
	boost::trim(s);
	GetDlgItem(IDOK).EnableWindow(!s.empty());
	return 0;
}
