/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"
#include "IntegrationPage.h"
#include "WinUtil.h"
#include "ShellExt.h"
#include "ConfUI.h"
#include "../client/AppPaths.h"
#include "../client/File.h"
#include "../client/Util.h"

#ifdef OSVER_WIN_XP
#include "../client/SysVersion.h"
#endif

static const PropPage::ListItem listItems[] =
{
	{ Conf::REGISTER_URL_HANDLER, ResourceManager::SETTINGS_URL_HANDLER },
	{ Conf::REGISTER_MAGNET_HANDLER, ResourceManager::SETCZDC_MAGNET_URI_HANDLER },
	{ Conf::REGISTER_DCLST_HANDLER, ResourceManager::INSTALL_DCLST_HANDLER },
#ifdef SSA_SHELL_INTEGRATION
	{ Conf::POPUP_ON_FOLDER_SHARED, ResourceManager::POPUP_NEW_FOLDERSHARE },
#endif
	{ 0, ResourceManager::Strings() }
};

LRESULT IntegrationPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_INTEGRATION_BOOLEANS));
	
	PropPage::read(*this, nullptr, listItems, ctrlList);
	
#ifdef SSA_SHELL_INTEGRATION
	checkShellInt();
	updateShellIntState();
#else
	GetDlgItem(IDC_SHELL_INT_BUTTON).ShowWindow(SW_HIDE);
	GetDlgItem(IDC_SHELL_INT_TEXT).ShowWindow(SW_HIDE);
#endif
	checkAutostart();
	updateAutostartState();

#ifdef OSVER_WIN_XP
	if (SysVersion::isOsVistaPlus())
#endif
	{
		GetDlgItem(IDC_SHELL_INT_BUTTON).SendMessage(BCM_FIRST + 0x000C, 0, 0xFFFFFFFF);
		GetDlgItem(IDC_AUTOSTART_BUTTON).SendMessage(BCM_FIRST + 0x000C, 0, 0xFFFFFFFF);
	}
	
	return TRUE;
}

void IntegrationPage::write()
{
	PropPage::write(*this, nullptr, listItems, ctrlList);
}

#ifdef SSA_SHELL_INTEGRATION
LRESULT IntegrationPage::onClickedShellInt(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool oldState = shellIntEnabled;
	bool result = WinUtil::registerShellExt(shellIntEnabled);
	if (!result)
	{
		checkShellInt();
		if (oldState == shellIntEnabled
#ifdef OSVER_WIN_XP
		        && SysVersion::isOsVistaPlus()
#endif
		   )
		{
			WinUtil::runElevated(NULL, Util::getModuleFileName().c_str(), shellIntEnabled ? _T("/uninstallShellExt") : _T("/installShellExt"));
			BaseThread::sleep(1000);
		}
	}
	checkShellInt();
	if (oldState == shellIntEnabled)
		MessageBox(CTSTRING(INTEGRATION_SHELL_ERROR), getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
	updateShellIntState();	
	return FALSE;
}
#endif // SSA_SHELL_INTEGRATION

LRESULT IntegrationPage::onClickedAutostart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool oldState = autostartEnabled;
	bool result =  WinUtil::autoRunShortcut(!autostartEnabled);
	if (result)
	{
		checkAutostart();
		if (oldState == autostartEnabled
#ifdef OSVER_WIN_XP
		        && SysVersion::isOsVistaPlus()
#endif
		   )
		{
			WinUtil::runElevated(NULL, Util::getModuleFileName().c_str(), shellIntEnabled ? _T("/uninstallStartup") : _T("/installStartup"));
			BaseThread::sleep(1000);
		}
	}
	checkAutostart();
	if (oldState == autostartEnabled)
		MessageBox(CTSTRING(INTEGRATION_AUTOSTART_ERROR), getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
	updateAutostartState();
	return FALSE;
}

#ifdef SSA_SHELL_INTEGRATION
void IntegrationPage::checkShellInt()
{
	shellIntAvailable = false;
	shellIntEnabled = false;
	const auto filePath = WinUtil::getShellExtDllPath();
	if (!File::isExist(filePath))
		return;

	shellIntAvailable = true;
	OLECHAR bufCLSID[40] = {0};
	StringFromGUID2(CLSID_ShellExt, bufCLSID, _countof(bufCLSID));
	tstring path = _T("CLSID\\");
	path += W2T(bufCLSID);
	
	HKEY key = nullptr;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, path.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		shellIntEnabled = true;
		RegCloseKey(key);
	}
}

void IntegrationPage::updateShellIntState()
{
	CStatic text(GetDlgItem(IDC_SHELL_INT_TEXT));
	CButton button(GetDlgItem(IDC_SHELL_INT_BUTTON));
	if (!shellIntAvailable)
	{
		text.SetWindowText(CTSTRING(INTEGRATION_SHELL_UNAVAILABLE));
		button.EnableWindow(FALSE);
	}
	else
	{
		text.SetWindowText(shellIntEnabled ? CTSTRING(INTEGRATION_SHELL_ENABLED) : CTSTRING(INTEGRATION_SHELL_DISABLED));
		button.EnableWindow(TRUE);
	}
	button.SetWindowText(shellIntEnabled ? CTSTRING(INTEGRATION_SHELL_REMOVE) : CTSTRING(INTEGRATION_SHELL_ADD));
}
#endif // SSA_SHELL_INTEGRATION

void IntegrationPage::checkAutostart()
{
	autostartEnabled = WinUtil::isAutoRunShortcutExists();
}

void IntegrationPage::updateAutostartState()
{
	if (autostartEnabled)
	{
		SetDlgItemText(IDC_AUTOSTART_TEXT, CTSTRING(INTEGRATION_AUTOSTART_ENABLED));
		SetDlgItemText(IDC_AUTOSTART_BUTTON, CTSTRING(INTEGRATION_AUTOSTART_REMOVE));
	}
	else
	{
		SetDlgItemText(IDC_AUTOSTART_TEXT, CTSTRING(INTEGRATION_AUTOSTART_DISABLED));
		SetDlgItemText(IDC_AUTOSTART_BUTTON, CTSTRING(INTEGRATION_AUTOSTART_ADD));
	}
}
