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
#include "SoundsPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "BrowseFile.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/File.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include <mmsystem.h>

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Item layoutItems[] =
{	
	{ IDC_SOUND_ENABLE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SOUNDS, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_PLAY, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_NONE, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_DEFAULT, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_PRIVATE_MESSAGE_BEEP, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_PRIVATE_MESSAGE_BEEP_OPEN, FLAG_TRANSLATE, AUTO, UNSPEC }
};

static const PropPage::Item items[] =
{
	{ IDC_SOUND_ENABLE, Conf::SOUNDS_DISABLED, PropPage::T_BOOL, PropPage::FLAG_INVERT },
	{ IDC_PRIVATE_MESSAGE_BEEP, Conf::PRIVATE_MESSAGE_BEEP, PropPage::T_BOOL },
	{ IDC_PRIVATE_MESSAGE_BEEP_OPEN, Conf::PRIVATE_MESSAGE_BEEP_OPEN, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

struct
{
	ResourceManager::Strings name;
	int setting;
	string value;
} static currentSounds[] =
{
	{ ResourceManager::SOUND_DOWNLOAD_BEGINS,   Conf::SOUND_BEGINFILE       },
	{ ResourceManager::SOUND_DOWNLOAD_FINISHED, Conf::SOUND_FINISHFILE      },
	{ ResourceManager::SOUND_SOURCE_ADDED,      Conf::SOUND_SOURCEFILE      },
	{ ResourceManager::SOUND_UPLOAD_FINISHED,   Conf::SOUND_UPLOADFILE      },
	{ ResourceManager::SOUND_FAKER_FOUND,       Conf::SOUND_FAKERFILE       },
	{ ResourceManager::SETCZDC_PRIVATE_SOUND,   Conf::SOUND_BEEPFILE        },
	{ ResourceManager::MYNICK_IN_CHAT,          Conf::SOUND_CHATNAMEFILE    },
	{ ResourceManager::SOUND_TTH_INVALID,       Conf::SOUND_TTH             },
	{ ResourceManager::HUB_CONNECTED,           Conf::SOUND_HUBCON          },
	{ ResourceManager::HUB_DISCONNECTED,        Conf::SOUND_HUBDISCON       },
	{ ResourceManager::FAVUSER_ONLINE,          Conf::SOUND_FAVUSER         },
	{ ResourceManager::FAVUSER_OFFLINE,         Conf::SOUND_FAVUSER_OFFLINE },
	{ ResourceManager::SOUND_TYPING_NOTIFY,     Conf::SOUND_TYPING_NOTIFY   },
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
	{ ResourceManager::SOUND_SEARCHSPY,         Conf::SOUND_SEARCHSPY       }
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
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items);

	ctrlSoundTheme.Attach(GetDlgItem(IDC_SOUNDS_COMBO));

	getSoundThemeList();
	const auto* ss = SettingsManager::instance.getUiSettings();
	int sel = -1;
	const string& selTheme = ss->getString(Conf::THEME_MANAGER_SOUNDS_THEME_NAME);
	int index = 0;
	for (const auto& theme : themes)
	{
		ctrlSoundTheme.AddString(theme.name.c_str());
		if (theme.path == selTheme) sel = index;
		index++;
	}

	if (sel < 0)
	{
		ctrlSoundTheme.SetCurSel(0);
		setAllToDefault();
	}
	else
		ctrlSoundTheme.SetCurSel(sel);

	ctrlSounds.Attach(GetDlgItem(IDC_SOUNDLIST));
	CRect rc;
	ctrlSounds.GetClientRect(rc);
	ctrlSounds.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlSounds);

	ctrlSounds.InsertColumn(0, CTSTRING(SETTINGS_SOUNDS), LVCFMT_LEFT, (rc.Width() / 3) * 1, 0);
	ctrlSounds.InsertColumn(1, CTSTRING(FILENAME), LVCFMT_LEFT, (rc.Width() / 3) * 2, 1);

	for (int i = 0; i < _countof(currentSounds); i++)
	{
		int j = ctrlSounds.insert(i, Text::toT(ResourceManager::getString(currentSounds[i].name)).c_str());
		currentSounds[i].value = ss->getString(currentSounds[i].setting);
		ctrlSounds.SetItemText(j, 1, Text::toT(currentSounds[i].value).c_str());
	}

	fixControls();
	return TRUE;
}

void Sounds::write()
{
	PropPage::write(*this, items);

	auto ss = SettingsManager::instance.getUiSettings();
	for (int i = 0; i < _countof(currentSounds); i++)
		ss->setString(currentSounds[i].setting, ctrlSounds.ExGetItemText(i, 1));

	ss->setString(Conf::THEME_MANAGER_SOUNDS_THEME_NAME, getSelectedTheme());
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
			tstring themePath = Text::toT(Util::getSoundsPath() + getSelectedTheme());
			Util::appendPathSeparator(themePath);
			const tstring selectedSoundPath = themePath + defaultSounds[item.iItem];
			ctrlSounds.SetItemText(item.iItem, 1, selectedSoundPath.c_str());
		}
	}
	return 0;
}

string Sounds::getSelectedTheme() const
{
	int sel = ctrlSoundTheme.GetCurSel();
	if (sel >= 0 && sel < (int) themes.size()) return themes[sel].path;
	return string();
}

void Sounds::setAllToDefault()
{
	tstring themePath = Text::toT(Util::getSoundsPath() + getSelectedTheme());
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
	if (themes.empty())
	{
		const string fileFindPath = Util::getSoundsPath() + '*';
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			if (i->isDirectory())
			{
				const string& name = i->getFileName();
				if (!Util::isReservedDirName(name))
					themes.push_back({ Text::toT(name), name });
			}
		}
		sort(themes.begin(), themes.end(),
			[](const ThemeInfo& l, const ThemeInfo& r) { return stricmp(l.name, r.name) < 0; });
		themes.insert(themes.begin(), { TSTRING(SOUND_THEME_DEFAULT_NAME), Util::emptyString });
	}
}
