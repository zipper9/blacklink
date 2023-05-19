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
#include "QueuePage.h"
#include "DialogLayout.h"
#include "../client/SettingsManager.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { -2, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align align3 = { 10, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align4 = { 0, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align5 = { 13, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align6 = { 14, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_MULTISOURCE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_AUTOSEGMENT, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_DONTBEGIN, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_CHUNKCOUNT, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_AUTO_SEARCH_EDIT, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_BEGIN_EDIT, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_SEG_NUMBER, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_MINUTES, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_KBPS, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_CAPTION_TARGET_EXISTS, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_DOWNLOAD_ASK_COMBO, 0, UNSPEC, UNSPEC, 0, &align3, &align4 },
	{ IDC_SKIP_EXISTING, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_COPY_FILE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SETTINGS_COPY_FILE, 0, UNSPEC, UNSPEC, 0, &align5 },
	{ IDC_SETTINGS_MB, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align6 }
};

static const PropPage::Item items[] =
{
	{ IDC_MULTISOURCE, SettingsManager::ENABLE_MULTI_CHUNK, PropPage::T_BOOL },
	{ IDC_AUTOSEGMENT, SettingsManager::AUTO_SEARCH, PropPage::T_BOOL },
	{ IDC_DONTBEGIN, SettingsManager::DONT_BEGIN_SEGMENT, PropPage::T_BOOL },
	{ IDC_BEGIN_EDIT, SettingsManager::DONT_BEGIN_SEGMENT_SPEED, PropPage::T_INT },
	{ IDC_AUTO_SEARCH_EDIT, SettingsManager::AUTO_SEARCH_TIME, PropPage::T_INT },
	{ IDC_CHUNKCOUNT, SettingsManager::SEGMENTS_MANUAL, PropPage::T_BOOL },
	{ IDC_SEG_NUMBER, SettingsManager::NUMBER_OF_SEGMENTS, PropPage::T_INT },
	{ IDC_SKIP_EXISTING, SettingsManager::SKIP_EXISTING, PropPage::T_BOOL },
	{ IDC_SETTINGS_COPY_FILE, SettingsManager::COPY_EXISTING_MAX_SIZE, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem optionItems[] =
{
	{ SettingsManager::AUTO_SEARCH_DL_LIST, ResourceManager::SETTINGS_AUTO_SEARCH_AUTO_MATCH },
	{ SettingsManager::SKIP_ZERO_BYTE, ResourceManager::SETTINGS_SKIP_ZERO_BYTE },
#if 0
	{ SettingsManager::DONT_DL_ALREADY_SHARED, ResourceManager::SETTINGS_DONT_DL_ALREADY_SHARED },
#endif
	{ SettingsManager::OVERLAP_CHUNKS, ResourceManager::OVERLAP_CHUNKS },
	{ SettingsManager::REPORT_ALTERNATES, ResourceManager::REPORT_ALTERNATES },
	{ SettingsManager::SEARCH_MAGNET_SOURCES, ResourceManager::SETTINGS_SEARCH_MAGNET_SOURCES },
	{ 0, ResourceManager::Strings() }
};

LRESULT QueuePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_OTHER_QUEUE_OPTIONS));
	ctrlActionIfExists.Attach(GetDlgItem(IDC_DOWNLOAD_ASK_COMBO));

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, optionItems, ctrlList);

	CUpDownCtrl spin1(GetDlgItem(IDC_SEG_NUMBER_SPIN));
	spin1.SetRange32(1, 200);
	spin1.SetBuddy(GetDlgItem(IDC_SEG_NUMBER));
	
	CUpDownCtrl spin2(GetDlgItem(IDC_AUTO_SEARCH_SPIN));
	spin2.SetRange32(1, 60);
	spin2.SetBuddy(GetDlgItem(IDC_AUTO_SEARCH_EDIT));

	CUpDownCtrl spin3(GetDlgItem(IDC_BEGIN_SPIN));
	spin3.SetRange32(2, 100000);
	spin3.SetBuddy(GetDlgItem(IDC_BEGIN_EDIT));

	ctrlActionIfExists.AddString(CTSTRING(TE_ACTION_ASK));
	ctrlActionIfExists.AddString(CTSTRING(TE_ACTION_REPLACE));
	ctrlActionIfExists.AddString(CTSTRING(TE_ACTION_RENAME));
	ctrlActionIfExists.AddString(CTSTRING(TE_ACTION_SKIP));
	
	int index = 0;
	switch (SETTING(TARGET_EXISTS_ACTION))
	{
		case SettingsManager::TE_ACTION_ASK:
			index = 0;
			break;

		case SettingsManager::TE_ACTION_REPLACE:
			index = 1;
			break;

		case SettingsManager::TE_ACTION_RENAME:
			index = 2;
			break;

		case SettingsManager::TE_ACTION_SKIP:
			index = 3;
			break;
	}
	ctrlActionIfExists.SetCurSel(index);
	
	fixControls();
	return TRUE;
}

void QueuePage::fixControls()
{
	const BOOL isChecked = IsDlgButtonChecked(IDC_MULTISOURCE) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_AUTOSEGMENT), isChecked);
	::EnableWindow(GetDlgItem(IDC_DONTBEGIN), isChecked);
	::EnableWindow(GetDlgItem(IDC_CHUNKCOUNT), isChecked);
	::EnableWindow(GetDlgItem(IDC_AUTO_SEARCH_SPIN), isChecked);
	::EnableWindow(GetDlgItem(IDC_AUTO_SEARCH_EDIT), isChecked);
	::EnableWindow(GetDlgItem(IDC_BEGIN_EDIT), isChecked);
	::EnableWindow(GetDlgItem(IDC_MINUTES), isChecked);
	::EnableWindow(GetDlgItem(IDC_KBPS), isChecked);
	::EnableWindow(GetDlgItem(IDC_SEG_NUMBER), isChecked);
}

void QueuePage::write()
{
	PropPage::write(*this, items, optionItems, ctrlList);
	
	int ct = SettingsManager::TE_ACTION_ASK;
	switch (ctrlActionIfExists.GetCurSel())
	{
		case 0:
			ct = SettingsManager::TE_ACTION_ASK;
			break;

		case 1:
			ct = SettingsManager::TE_ACTION_REPLACE;
			break;

		case 2:
			ct = SettingsManager::TE_ACTION_RENAME;
			break;

		case 3:
			ct = SettingsManager::TE_ACTION_SKIP;
			break;
	}
	g_settings->set(SettingsManager::TARGET_EXISTS_ACTION, ct);
}
