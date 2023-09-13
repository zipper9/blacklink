#include "stdafx.h"
#include "WebServerPage.h"
#include "../client/SettingsManager.h"
#include "../client/File.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include "WinUtil.h"
#include "DialogLayout.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 4, DialogLayout::SIDE_LEFT, U_DU(6) };
static const DialogLayout::Align align2 = { 3, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { 5, DialogLayout::SIDE_LEFT, U_DU(12) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_ENABLE_WEBSERVER, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SETTINGS_BIND_ADDRESS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_BIND_ADDRESS_HELP, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_PORT, 0, UNSPEC, UNSPEC },
	{ IDC_CAPTION_PORT, FLAG_TRANSLATE, AUTO, UNSPEC, 0, nullptr, &align1 },
	{ IDC_BIND_ADDRESS, 0, UNSPEC, UNSPEC, 0, &align2, &align3 },
	{ IDC_SETTINGS_NORMAL_USER, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_NORMAL_USER, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_NORMAL_PASSWORD, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_POWER_USER, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_POWER_USER, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_POWER_PASSWORD, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_TOGGLE_PASSWORDS, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

static const PropPage::Item items[] =
{
	{ IDC_ENABLE_WEBSERVER, SettingsManager::WEBSERVER, PropPage::T_BOOL },
	{ IDC_PORT, SettingsManager::WEBSERVER_PORT, PropPage::T_INT },
	{ IDC_NORMAL_USER, SettingsManager::WEBSERVER_USER, PropPage::T_STR },
	{ IDC_NORMAL_PASSWORD, SettingsManager::WEBSERVER_PASS, PropPage::T_STR },
	{ IDC_POWER_USER, SettingsManager::WEBSERVER_POWER_USER, PropPage::T_STR },
	{ IDC_POWER_PASSWORD, SettingsManager::WEBSERVER_POWER_PASS, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

static bool checkPath(const char* dir)
{
	string path = Util::getWebServerPath();
	if (path.empty()) return false;
	Util::appendPathSeparator(path);
	path += dir;
	FileAttributes attr;
	return File::getAttributes(path, attr) && attr.isDirectory();
}

static bool checkPaths()
{
	return checkPath("static") && checkPath("templates") && checkPath("themes");
}

LRESULT WebServerPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	hasFiles = checkPaths();

	if (!hasFiles)
	{
		static const int ids[] =
		{
			IDC_NORMAL_USER, IDC_NORMAL_PASSWORD, IDC_POWER_USER, IDC_POWER_PASSWORD
		};
		for (int i = 1; i < _countof(layoutItems); ++i)
			GetDlgItem(layoutItems[i].id).ShowWindow(SW_HIDE);
		for (int i = 0; i < _countof(ids); ++i)
			GetDlgItem(ids[i]).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_ENABLE_WEBSERVER).EnableWindow(FALSE);
		CStatic placeholder(GetDlgItem(IDC_DESCRIPTION));
		RECT rc;
		placeholder.GetWindowRect(&rc);
		placeholder.DestroyWindow();
		ScreenToClient(&rc);
		label.Create(m_hWnd, &rc, nullptr, WS_CHILD | WS_VISIBLE);
		label.setText(TSTRING(WEBSERVER_NO_FILES));
		label.setImage(IconBitmaps::WARNING, 0);
		return TRUE;
	}

	PropPage::read(*this, items);
	toggleControls(BOOLSETTING(WEBSERVER));
	CComboBox bindCombo(GetDlgItem(IDC_BIND_ADDRESS));
	vector<Util::AdapterInfo> adapters;
	WinUtil::getAdapterList(AF_INET, adapters, Util::GNA_ALLOW_LOOPBACK);
	WinUtil::fillAdapterList(AF_INET, adapters, bindCombo, SETTING(WEBSERVER_BIND_ADDRESS), 0);
	return TRUE;
}

void WebServerPage::write()
{
	if (!hasFiles) return;
	PropPage::write(*this, items);
	CComboBox bindCombo(GetDlgItem(IDC_BIND_ADDRESS));
	g_settings->set(SettingsManager::WEBSERVER_BIND_ADDRESS, WinUtil::getSelectedAdapter(bindCombo));
}

void WebServerPage::toggleControls(bool enable)
{
	static const int ids[] =
	{
		IDC_BIND_ADDRESS, IDC_PORT,
		IDC_NORMAL_USER, IDC_NORMAL_PASSWORD, IDC_POWER_USER, IDC_POWER_PASSWORD,
		IDC_TOGGLE_PASSWORDS
	};
	if (!enable && passwordsVisible)
		showPasswords(false);
	for (int i = 0; i < _countof(ids); ++i)
		GetDlgItem(ids[i]).EnableWindow(enable);
}

void WebServerPage::showPasswords(bool show)
{
	static const int ids[] =
	{
		IDC_NORMAL_PASSWORD, IDC_POWER_PASSWORD
	};
	if (show && MessageBox(CTSTRING(CONFIRM_SHOW_PASSWORDS), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;
	passwordsVisible = show;
	for (int i = 0; i < _countof(ids); ++i)
	{
		CEdit wnd(GetDlgItem(ids[i]));
		if (!passwordChar) passwordChar = wnd.GetPasswordChar();
		wnd.SetPasswordChar(passwordsVisible ? 0 : passwordChar);
		wnd.Invalidate();
	}
	GetDlgItem(IDC_TOGGLE_PASSWORDS).SetWindowText(passwordsVisible ? CTSTRING(SETTINGS_HIDE_PASSWORDS) : CTSTRING(SETTINGS_SHOW_PASSWORDS));
}

LRESULT WebServerPage::onToggleControls(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	toggleControls(CButton(hWndCtl).GetCheck() == BST_CHECKED);
	return 0;
}

LRESULT WebServerPage::onShowPasswords(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	showPasswords(!passwordsVisible);
	return 0;
}
