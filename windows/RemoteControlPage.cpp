#include "stdafx.h"

#include "RemoteControlPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_ENABLE_WEBSERVER, ResourceManager::SETTINGS_WEBSERVER },
	{ IDC_DESCRIPTION, ResourceManager::WEBSERVER_NO_FILES },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_ENABLE_WEBSERVER, SettingsManager::WEBSERVER, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

static bool checkPath()
{
	const string& path = Util::getWebServerPath();
	if (path.empty()) return false;
	FileAttributes attr;
	return File::getAttributes(path, attr) && attr.isDirectory();
}

LRESULT RemoteControlPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WinUtil::translate((HWND)(*this), texts);
	PropPage::read(*this, items);

	if (!checkPath())
	{
		CButton button(GetDlgItem(IDC_ENABLE_WEBSERVER));
		button.SetCheck(BST_UNCHECKED);
		button.EnableWindow(FALSE);
		GetDlgItem(IDC_DESCRIPTION).ShowWindow(SW_SHOW);
	}

	return TRUE;
}

void RemoteControlPage::write()
{
	PropPage::write(*this, items);
}
