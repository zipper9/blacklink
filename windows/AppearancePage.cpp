/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "AppearancePage.h"
#include "WinUtil.h"
#include "UserMessages.h"
#include "ConfUI.h"
#include "../client/File.h"
#include "../client/AppPaths.h"
#include "../client/SettingsManager.h"
#include "../client/BusyCounter.h"
#include "../client/ConfCore.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_SETTINGS_APPEARANCE_OPTIONS, ResourceManager::SETTINGS_OPTIONS },
	{ IDC_SETTINGS_TIME_STAMPS_FORMAT, ResourceManager::SETTINGS_TIME_STAMPS_FORMAT },
	{ IDC_THEME, ResourceManager::THEME },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_TIME_STAMPS_FORMAT, Conf::TIME_STAMPS_FORMAT, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::MINIMIZE_ON_STARTUP, ResourceManager::SETTINGS_MINIMIZE_ON_STARTUP },
	{ Conf::MINIMIZE_ON_CLOSE, ResourceManager::MINIMIZE_ON_CLOSE },
	{ Conf::MINIMIZE_TRAY, ResourceManager::SETTINGS_MINIMIZE_TRAY },
	{ Conf::SHOW_CURRENT_SPEED_IN_TITLE, ResourceManager::SHOW_CURRENT_SPEED_IN_TITLE },
	{ Conf::SORT_FAVUSERS_FIRST, ResourceManager::SETTINGS_SORT_FAVUSERS_FIRST },
	{ Conf::SHOW_HIDDEN_USERS, ResourceManager::SETTINGS_SHOW_HIDDEN_USERS },
	{ Conf::FILTER_ENTER, ResourceManager::SETTINGS_FILTER_ENTER },
	{ Conf::USE_SYSTEM_ICONS, ResourceManager::SETTINGS_USE_SYSTEM_ICONS },
	{ Conf::SHOW_SHELL_MENU, ResourceManager::SETTINGS_SHOW_SHELL_MENU },
	{ Conf::SHOW_INFOTIPS, ResourceManager::SETTINGS_SHOW_INFO_TIPS },
	{ Conf::SHOW_GRIDLINES, ResourceManager::VIEW_GRIDCONTROLS },
	{ Conf::FILTER_MESSAGES, ResourceManager::SETTINGS_FILTER_MESSAGES },
	{ Conf::UC_SUBMENU, ResourceManager::UC_SUBMENU },
	{ Conf::ENABLE_COUNTRY_FLAG, ResourceManager::ENABLE_COUNTRYFLAG },
	{ Conf::APP_DPI_AWARE, ResourceManager::SETTINGS_APP_DPI_AWARE },
	{ 0, ResourceManager::Strings() }
};

void AppearancePage::write()
{
	PropPage::write(*this, items, listItems, ctrlList);

	string themeFile;
	int sel = ctrlTheme.GetCurSel();
	if (sel >= 0 && sel < (int) themes.size()) themeFile = themes[sel].name;
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getString(Conf::THEME_MANAGER_THEME_DLL_NAME) != themeFile)
	{
		ss->setString(Conf::THEME_MANAGER_THEME_DLL_NAME, themeFile);
		if (themes.size() != 1)
			MessageBox(CTSTRING(THEME_CHANGE_THEME_INFO), CTSTRING(THEME_CHANGE_THEME), MB_OK | MB_ICONEXCLAMATION);
	}
}

LRESULT AppearancePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	BusyCounter<int> busyFlag(initializing);
	ctrlList.Attach(GetDlgItem(IDC_APPEARANCE_BOOLEANS));
	WinUtil::setExplorerTheme(ctrlList);

	WinUtil::translate(*this, texts);
	PropPage::read(*this, items, listItems, ctrlList);	

	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO));
	getThemeList();

	const auto* ss = SettingsManager::instance.getUiSettings();
	const string& selectedTheme = ss->getString(Conf::THEME_MANAGER_THEME_DLL_NAME);
	int index = 0;
	int sel = -1;
	for (const auto& ti : themes)
	{
		ctrlTheme.AddString(ti.description.c_str());
		if (ti.name == selectedTheme) sel = index;
		index++;
	}

	ctrlTheme.SetCurSel(sel);
	return TRUE;
}

LRESULT AppearancePage::onClickedHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	MessageBox(CTSTRING(TIMESTAMP_HELP), CTSTRING(TIMESTAMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
	return 0;
}

LRESULT AppearancePage::onListItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMLISTVIEW* l = reinterpret_cast<const NMLISTVIEW*>(pnmh);
	if (!initializing && (l->uChanged & LVIF_STATE))
	{
		const auto* ss = SettingsManager::instance.getUiSettings();
		int setting = 0;
		if (l->iItem >= 0 && l->iItem < _countof(listItems))
			setting = listItems[l->iItem].setting;
		if (setting == Conf::APP_DPI_AWARE &&
		    (bool) CListViewCtrl(l->hdr.hwndFrom).GetCheckState(l->iItem) != ss->getBool(Conf::APP_DPI_AWARE))
		{
			GetParent().SendMessage(WMU_RESTART_REQUIRED);
		}
	}
	return 0;
}

void AppearancePage::getThemeList()
{
	if (themes.empty())
	{
		typedef  void (WINAPIV ResourceName)(wchar_t*, size_t);
		string fileFindPath = Util::getThemesPath() + "*.dll";
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			string name  = i->getFileName();
			string fullPath = Util::getThemesPath() + name;
			if (name.empty() || i->isDirectory() || i->getSize() == 0
			        || WinUtil::getDllPlatform(fullPath) !=
#if defined(_WIN64)
			        IMAGE_FILE_MACHINE_AMD64
#elif defined(_WIN32)
			        IMAGE_FILE_MACHINE_I386
#endif
			   )
			{
				continue;
			}
			name.erase(name.length() - 4);
			if (Text::isAsciiSuffix2<string>(name, "_x64")) name.erase(name.length() - 4);
			HMODULE hModule = NULL;
			try
			{
				hModule =::LoadLibrary(Text::toT(fullPath).c_str());
				if (hModule != NULL)
				{
					ResourceName* resourceName = (ResourceName*)::GetProcAddress(hModule, "ResourceName");
					if (resourceName)
					{
						wchar_t buf[256];
						resourceName(buf, 256);
						if (buf[0])
						{
							wstring wName = buf;
							wName +=  L" (" + Text::utf8ToWide(name) + L')';
							themes.push_back({ wName, name });
						}
					}
					::FreeLibrary(hModule);
				}
			}
			catch (...)
			{
				if (hModule)
					::FreeLibrary(hModule);
				hModule = NULL;
			}
		}
		sort(themes.begin(), themes.end(),
			[](const ThemeInfo& l, const ThemeInfo& r) { return stricmp(l.description, r.description) < 0; });
		themes.insert(themes.begin(), { TSTRING(THEME_DEFAULT_NAME), Util::emptyString });
	}
}
