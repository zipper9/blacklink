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

#include "Resource.h"
#include "AppearancePage.h"
#include "../client/File.h"

#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_SETTINGS_APPEARANCE_OPTIONS, ResourceManager::SETTINGS_OPTIONS },
	{ IDC_SETTINGS_TIME_STAMPS_FORMAT, ResourceManager::SETTINGS_TIME_STAMPS_FORMAT },
	{ IDC_SETTINGS_REQUIRES_RESTART, ResourceManager::SETTINGS_REQUIRES_RESTART },
	{ IDC_THEME, ResourceManager::THEME },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_TIME_STAMPS_FORMAT, SettingsManager::TIME_STAMPS_FORMAT, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::MINIMIZE_ON_STARTUP, ResourceManager::SETTINGS_MINIMIZE_ON_STARTUP },
	{ SettingsManager::MINIMIZE_ON_CLOSE, ResourceManager::MINIMIZE_ON_CLOSE },
	{ SettingsManager::MINIMIZE_TRAY, ResourceManager::SETTINGS_MINIMIZE_TRAY },
	{ SettingsManager::SHOW_CURRENT_SPEED_IN_TITLE, ResourceManager::SHOW_CURRENT_SPEED_IN_TITLE },
	{ SettingsManager::SORT_FAVUSERS_FIRST, ResourceManager::SETTINGS_SORT_FAVUSERS_FIRST },
	{ SettingsManager::USE_SYSTEM_ICONS, ResourceManager::SETTINGS_USE_SYSTEM_ICONS },
	{ SettingsManager::USE_OLD_SHARING_UI, ResourceManager::SETTINGS_USE_OLD_SHARING_UI },
	{ SettingsManager::SHOW_INFOTIPS, ResourceManager::SETTINGS_SHOW_INFO_TIPS },
	{ SettingsManager::SHOW_GRIDLINES, ResourceManager::VIEW_GRIDCONTROLS },
	{ SettingsManager::FILTER_MESSAGES, ResourceManager::SETTINGS_FILTER_MESSAGES },
	{ SettingsManager::UC_SUBMENU, ResourceManager::UC_SUBMENU },
	{ SettingsManager::USE_EXPLORER_THEME, ResourceManager::USE_EXPLORER_THEME },
	{ SettingsManager::USE_12_HOUR_FORMAT, ResourceManager::USE_12_HOUR_FORMAT },
#ifdef SCALOLAZ_HUB_MODE
	{ SettingsManager::ENABLE_HUBMODE_PIC, ResourceManager::ENABLE_HUBMODE_PIC },
#endif
	{ SettingsManager::ENABLE_COUNTRY_FLAG, ResourceManager::ENABLE_COUNTRYFLAG },
	{ 0, ResourceManager::Strings() }
};

AppearancePage::~AppearancePage()
{
	ctrlList.Detach();
}

void AppearancePage::write()
{
	PropPage::write(*this, items, listItems, ctrlList);
	
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO));
	const string themeFile = WinUtil::getDataFromMap(ctrlTheme.GetCurSel(), themeList);
	if (SETTING(THEME_MANAGER_THEME_DLL_NAME) != themeFile)
	{
		g_settings->set(SettingsManager::THEME_MANAGER_THEME_DLL_NAME, themeFile);
		if (themeList.size() != 1)
			MessageBox(CTSTRING(THEME_CHANGE_THEME_INFO), CTSTRING(THEME_CHANGE_THEME), MB_OK | MB_ICONEXCLAMATION);
	}
	ctrlTheme.Detach();	
}

LRESULT AppearancePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_APPEARANCE_BOOLEANS));
	
	PropPage::translate(*this, texts);	
	PropPage::read(*this, items, listItems, ctrlList);	
	
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO));
	getThemeList();
	for (auto i = themeList.cbegin(); i != themeList.cend(); ++i)
		ctrlTheme.AddString(i->first.c_str());
		
	ctrlTheme.SetCurSel(WinUtil::getIndexFromMap(themeList, SETTING(THEME_MANAGER_THEME_DLL_NAME)));
	ctrlTheme.Detach();
	
	return TRUE;
}

LRESULT AppearancePage::onClickedHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	MessageBox(CTSTRING(TIMESTAMP_HELP), CTSTRING(TIMESTAMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
	return S_OK;
}

void AppearancePage::getThemeList()
{
	if (themeList.empty())
	{
		typedef  void (WINAPIV ResourceName)(wchar_t*, size_t);
		themeList.insert(ThemePair(TSTRING(THEME_DEFAULT_NAME), Util::emptyString));
		// Find in Theme folder .DLL's check'em
		string fileFindPath = Util::getThemesPath() + "*.dll";
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			string name  = i->getFileName();
			string fullPath = Util::getThemesPath() + name;
			if (name.empty() || i->isDirectory() || i->getSize() == 0
			        || CompatibilityManager::getDllPlatform(fullPath) !=
#if defined(_WIN64)
			        IMAGE_FILE_MACHINE_AMD64
#elif defined(_WIN32)
			        IMAGE_FILE_MACHINE_I386
#endif
			   )
			{
				continue;
			}
			// CheckName;
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
							wName +=  L" (" + Text::toT(name) + L')';
							themeList.insert(ThemePair(wName, name));
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
		
	}
}
