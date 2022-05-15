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

#include "DownloadPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "../client/File.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const PropPage::Item items[] =
{
	{ IDC_TEMP_DOWNLOAD_DIRECTORY, SettingsManager::TEMP_DOWNLOAD_DIRECTORY, PropPage::T_STR },
	{ IDC_DOWNLOAD_DIR, SettingsManager::DOWNLOAD_DIRECTORY, PropPage::T_STR },
	{ IDC_DOWNLOADS, SettingsManager::DOWNLOAD_SLOTS, PropPage::T_INT },
	{ IDC_FILES, SettingsManager::FILE_SLOTS, PropPage::T_INT },
	{ IDC_MAXSPEED, SettingsManager::MAX_DOWNLOAD_SPEED, PropPage::T_INT },
	{ IDC_EXTRA_DOWN_SLOT, SettingsManager::EXTRA_DOWNLOAD_SLOTS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { -2, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SETTINGS_DIRECTORIES, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_DOWNLOAD_DIRECTORY, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_UNFINISHED_DOWNLOAD_DIRECTORY, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_DIRECTORIES, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_FILES_MAX, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_DOWNLOADS_MAX, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_EXTRA_DOWNLOADS_MAX, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_SETTINGS_DOWNLOADS_SPEED_PAUSE, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_FILES, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_DOWNLOADS, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_EXTRA_DOWN_SLOT, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_MAXSPEED, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_SETTINGS_HINT1, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_SETTINGS_HINT2, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_SETTINGS_HINT3, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_SETTINGS_SPEEDS_NOT_ACCURATE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SETTINGS_DOWNLOAD_LIMITS, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

LRESULT DownloadPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items);
	
	CUpDownCtrl spin1(GetDlgItem(IDC_FILESPIN));
	spin1.SetRange32(0, 100);
	spin1.SetBuddy(GetDlgItem(IDC_FILES));

	CUpDownCtrl spin2(GetDlgItem(IDC_SLOTSSPIN));
	spin2.SetRange32(0, 100);
	spin2.SetBuddy(GetDlgItem(IDC_DOWNLOADS));

	CUpDownCtrl spin3(GetDlgItem(IDC_SPEEDSPIN));
	spin3.SetRange32(0, 10000);	
	spin3.SetBuddy(GetDlgItem(IDC_MAXSPEED));

	CUpDownCtrl spin4(GetDlgItem(IDC_EXTRASLOTSSPIN));
	spin4.SetRange32(0, 100);
	spin4.SetBuddy(GetDlgItem(IDC_EXTRA_DOWN_SLOT));

	return TRUE;
}

void DownloadPage::write()
{
	PropPage::write(*this, items);
	const tstring dir = Text::toT(SETTING(TEMP_DOWNLOAD_DIRECTORY));
	if (!dir.empty())
		File::ensureDirectory(dir);
}

LRESULT DownloadPage::onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
	if (WinUtil::browseDirectory(dir, m_hWnd))
	{
		Util::appendPathSeparator(dir);
		SetDlgItemText(IDC_DOWNLOAD_DIR, dir.c_str());
	}
	return 0;
}

LRESULT DownloadPage::onClickedBrowseTempDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir = Text::toT(SETTING(TEMP_DOWNLOAD_DIRECTORY));
	if (WinUtil::browseDirectory(dir, m_hWnd))
	{
		Util::appendPathSeparator(dir);
		SetDlgItemText(IDC_TEMP_DOWNLOAD_DIRECTORY, dir.c_str());
	}
	return 0;
}
