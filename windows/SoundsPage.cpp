/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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
#include "SoundsPage.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_SOUND_ENABLE, ResourceManager::ENABLE_SOUNDS },
	{ IDC_PRIVATE_MESSAGE_BEEP, ResourceManager::SETTINGS_PM_BEEP },
	{ IDC_PRIVATE_MESSAGE_BEEP_OPEN, ResourceManager::SETTINGS_PM_BEEP_OPEN },
	{ IDC_CZDC_SOUND, ResourceManager::SETTINGS_SOUNDS },
	//{ IDC_BROWSE, ResourceManager::BROWSE }, // [~] JhaoDa, not necessary any more
	{ IDC_PLAY, ResourceManager::PLAY },
	{ IDC_NONE, ResourceManager::NONE },
	{ IDC_DEFAULT, ResourceManager::DEFAULT },
	{ IDC_SOUNDS, ResourceManager::SOUND_THEME },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_PRIVATE_MESSAGE_BEEP, SettingsManager::PRIVATE_MESSAGE_BEEP, PropPage::T_BOOL },
	{ IDC_PRIVATE_MESSAGE_BEEP_OPEN, SettingsManager::PRIVATE_MESSAGE_BEEP_OPEN, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

struct
{
	ResourceManager::Strings name;
	int setting;
	string value;
} static currentSounds[] =
{
	{ ResourceManager::SOUND_DOWNLOAD_BEGINS,   SettingsManager::SOUND_BEGINFILE       },
	{ ResourceManager::SOUND_DOWNLOAD_FINISHED, SettingsManager::SOUND_FINISHFILE      },
	{ ResourceManager::SOUND_SOURCE_ADDED,      SettingsManager::SOUND_SOURCEFILE      },
	{ ResourceManager::SOUND_UPLOAD_FINISHED,   SettingsManager::SOUND_UPLOADFILE      },
	{ ResourceManager::SOUND_FAKER_FOUND,       SettingsManager::SOUND_FAKERFILE       },
	{ ResourceManager::SETCZDC_PRIVATE_SOUND,   SettingsManager::SOUND_BEEPFILE        },
	{ ResourceManager::MYNICK_IN_CHAT,          SettingsManager::SOUND_CHATNAMEFILE    },
	{ ResourceManager::SOUND_TTH_INVALID,       SettingsManager::SOUND_TTH             },
	{ ResourceManager::HUB_CONNECTED,           SettingsManager::SOUND_HUBCON          },
	{ ResourceManager::HUB_DISCONNECTED,        SettingsManager::SOUND_HUBDISCON       },
	{ ResourceManager::FAVUSER_ONLINE,          SettingsManager::SOUND_FAVUSER         },
	{ ResourceManager::FAVUSER_OFFLINE,         SettingsManager::SOUND_FAVUSER_OFFLINE },
	{ ResourceManager::SOUND_TYPING_NOTIFY,     SettingsManager::SOUND_TYPING_NOTIFY   },
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
	{ ResourceManager::SOUND_SEARCHSPY,         SettingsManager::SOUND_SEARCHSPY       }
#endif
};

static const TCHAR* defaultSounds[] =
{
	_T("DownloadBegins.wav"),
	_T("DownloadFinished.wav"),
	_T("AltSourceAdded.wav"),
	_T("UploadFinished.wav"),
	_T("FakerFound.wav"),
	_T("PrivateMessage.wav"),
	_T("MyNickInMainChat.wav"),
	_T("FileCorrupted.wav"),
	_T("HubConnected.wav"),
	_T("HubDisconnected.wav"),
	_T("FavUser.wav"),
	_T("FavUserDisconnected.wav"),
	_T("TypingNotify.wav"),
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
	_T("SearchSpy.wav")
#endif
};

LRESULT Sounds::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	
	ctrlSoundTheme.Attach(GetDlgItem(IDC_SOUNDS_COMBO));
	
	getSoundThemeList();
	for (auto i = soundThemes.cbegin(); i != soundThemes.cend(); ++i)
		ctrlSoundTheme.AddString(i->first.c_str());
		
	const int ind = WinUtil::getIndexFromMap(soundThemes, SETTING(THEME_MANAGER_SOUNDS_THEME_NAME));
	if (ind < 0)
	{
		ctrlSoundTheme.SetCurSel(0);  // Если когда-то был выбран пакет, но его удалили физически, ставим по умолчанию
		setAllToDefault();
	}
	else
		ctrlSoundTheme.SetCurSel(ind);
	
	CheckDlgButton(IDC_SOUND_ENABLE, SETTING(SOUNDS_DISABLED) ? BST_UNCHECKED : BST_CHECKED);
	
	ctrlSounds.Attach(GetDlgItem(IDC_SOUNDLIST));
	CRect rc;
	ctrlSounds.GetClientRect(rc);
	setListViewExtStyle(ctrlSounds, BOOLSETTING(SHOW_GRIDLINES), false);
	SET_LIST_COLOR_IN_SETTING(ctrlSounds);

	ctrlSounds.InsertColumn(0, CTSTRING(SETTINGS_SOUNDS), LVCFMT_LEFT, (rc.Width() / 3) * 1, 0);
	ctrlSounds.InsertColumn(1, CTSTRING(FILENAME), LVCFMT_LEFT, (rc.Width() / 3) * 2, 1);
	
	for (int i = 0; i < _countof(currentSounds); i++)
	{
		int j = ctrlSounds.insert(i, Text::toT(ResourceManager::getString(currentSounds[i].name)).c_str());
		currentSounds[i].value = SettingsManager::get((SettingsManager::StrSetting) currentSounds[i].setting, true);
		ctrlSounds.SetItemText(j, 1, Text::toT(currentSounds[i].value).c_str());
	}
	
	fixControls();
	return TRUE;
}

void Sounds::write()
{
	PropPage::write(*this, items);
	
	for (int i = 0; i < _countof(currentSounds); i++)
		g_settings->set(SettingsManager::StrSetting(currentSounds[i].setting), ctrlSounds.ExGetItemText(i, 1));
	
	SET_SETTING(SOUNDS_DISABLED, IsDlgButtonChecked(IDC_SOUND_ENABLE) == 1 ? false : true);
	
	const string themeFile = WinUtil::getDataFromMap(ctrlSoundTheme.GetCurSel(), soundThemes);
	if (SETTING(THEME_MANAGER_SOUNDS_THEME_NAME) != themeFile) // ???
		g_settings->set(SettingsManager::THEME_MANAGER_SOUNDS_THEME_NAME, themeFile);
}

LRESULT Sounds::onBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LVITEM item = {0};
	if (ctrlSounds.GetSelectedItem(&item))
	{
		tstring x;
		if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
			ctrlSounds.SetItemText(item.iItem, 1, x.c_str());
	}
	return 0;
}

LRESULT Sounds::onClickedNone(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LVITEM item = {0};
	if (ctrlSounds.GetSelectedItem(&item))
		ctrlSounds.SetItemText(item.iItem, 1, _T(""));	
	fixControls();
	return 0;
}

LRESULT Sounds::onPlay(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LVITEM item = {0};
	if (ctrlSounds.GetSelectedItem(&item))
		PlaySound(ctrlSounds.ExGetItemTextT(item.iItem, 1).c_str(), NULL, SND_FILENAME | SND_ASYNC);
	return 0;
}

LRESULT Sounds::onDefault(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LVITEM item = {0};
	if (ctrlSounds.GetSelectedItem(&item))
	{
		if (item.iItem >= 0 && item.iItem < _countof(defaultSounds))
		{
			tstring themePath = Text::toT(Util::getSoundPath() + SETTING(THEME_MANAGER_SOUNDS_THEME_NAME));
			Util::appendPathSeparator(themePath);
			const tstring selectedSoundPath = themePath + defaultSounds[item.iItem];
			ctrlSounds.SetItemText(item.iItem, 1, selectedSoundPath.c_str());
		}
	}
	return 0;
}

void Sounds::setAllToDefault()
{
	const tstring themeName = Text::toT(WinUtil::getDataFromMap(ctrlSoundTheme.GetCurSel(), soundThemes));

	tstring themePath = Text::toT(Util::getSoundPath()) + themeName;
	Util::appendPathSeparator(themePath);
	
	for (size_t i = 0; i < _countof(defaultSounds); ++i)
	{
		const tstring fullPath = themePath + defaultSounds[i];
		ctrlSounds.SetItemText(i, 1, fullPath.c_str());
	}
}

void Sounds::fixControls()
{
	BOOL enabled = IsDlgButtonChecked(IDC_SOUND_ENABLE) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_CZDC_SOUND), enabled);// TODO: make these interface elements gray when disabled
	::EnableWindow(GetDlgItem(IDC_SOUNDLIST), enabled);
	::EnableWindow(GetDlgItem(IDC_SOUNDS_COMBO), enabled);
	::EnableWindow(GetDlgItem(IDC_PLAY), enabled);
	::EnableWindow(GetDlgItem(IDC_DEFAULT), enabled);
	::EnableWindow(GetDlgItem(IDC_NONE), enabled);
	::EnableWindow(GetDlgItem(IDC_BROWSE), enabled);
	::EnableWindow(GetDlgItem(IDC_PRIVATE_MESSAGE_BEEP), enabled);
	::EnableWindow(GetDlgItem(IDC_PRIVATE_MESSAGE_BEEP_OPEN), enabled);
}

LRESULT Sounds::onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void Sounds::getSoundThemeList()
{
	if (soundThemes.empty())
	{
		soundThemes.insert(std::pair<tstring, string>(TSTRING(SOUND_THEME_DEFAULT_NAME), Util::emptyString));
		const string fileFindPath = Util::getSoundPath() + '*';
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			if (i->isDirectory())
			{
				const string& name = i->getFileName();
				if (name != Util::m_dot && name != Util::m_dot_dot)
				{
					const tstring wName = /*L"Theme '" + */Text::toT(name)/* + L"'"*/;
					soundThemes.insert(::pair<tstring, string>(wName, name));
				}
			}
		}
	}
}
