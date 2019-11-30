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
#include "PriorityPage.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_CAPTION_AUTOPRIORITY_FILENAME, ResourceManager::SETTINGS_AUTOPRIORITY_FILENAME },
	{ IDC_AUTOPRIORITY_USE_PATTERNS, ResourceManager::SETTINGS_AUTOPRIORITY_USE_PATTERNS },
	{ IDC_CAPTION_SET_PRIORITY1, ResourceManager::SETTINGS_AUTOPRIORITY_SET_PRIORITY },
	{ IDC_CAPTION_AUTOPRIORITY_FILESIZE, ResourceManager::SETTINGS_AUTOPRIORITY_FILESIZE },
	{ IDC_AUTOPRIORITY_USE_SIZE, ResourceManager::SETTINGS_AUTOPRIORITY_USE_SIZE },
	{ IDC_CAPTION_KIB, ResourceManager::KB },
	{ IDC_CAPTION_SET_PRIORITY2, ResourceManager::SETTINGS_AUTOPRIORITY_SET_PRIORITY },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_AUTOPRIORITY_USE_PATTERNS, SettingsManager::AUTO_PRIORITY_USE_PATTERNS, PropPage::T_BOOL },
	{ IDC_AUTOPRIORITY_PATTERNS, SettingsManager::AUTO_PRIORITY_PATTERNS, PropPage::T_STR },
	{ IDC_AUTOPRIORITY_USE_SIZE, SettingsManager::AUTO_PRIORITY_USE_SIZE, PropPage::T_BOOL },
	{ IDC_AUTOPRIORITY_SIZE, SettingsManager::AUTO_PRIORITY_SMALL_SIZE, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const ResourceManager::Strings prioText[] =
{
	ResourceManager::LOWEST,
	ResourceManager::LOWER,
	ResourceManager::LOW,
	ResourceManager::NORMAL,
	ResourceManager::HIGH,
	ResourceManager::HIGHER,
	ResourceManager::HIGHEST
};

static inline void clampPrio(int& prio)
{
	if (prio < QueueItem::LOWEST)
		prio = QueueItem::LOWEST;
	else
	if (prio > QueueItem::HIGHEST)
		prio = QueueItem::HIGHEST;
}

LRESULT PriorityPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	
	CComboBox cb1(GetDlgItem(IDC_AUTOPRIORITY_PATTERNS_PRIO));
	CComboBox cb2(GetDlgItem(IDC_AUTOPRIORITY_SIZE_PRIO));
	for (int i = 0; i < _countof(prioText); i++)
	{
		const TCHAR* text = CTSTRING_I(prioText[i]);
		cb1.AddString(text);
		cb2.AddString(text);
	}
	
	int prio = g_settings->get(SettingsManager::AUTO_PRIORITY_PATTERNS_PRIO);
	clampPrio(prio);
	cb1.SetCurSel(prio - QueueItem::LOWEST);

	prio = g_settings->get(SettingsManager::AUTO_PRIORITY_SMALL_SIZE_PRIO);
	clampPrio(prio);
	cb2.SetCurSel(prio - QueueItem::LOWEST);

	onChangeUsePatterns();
	onChangeUseSize();
	
	return TRUE;
}

void PriorityPage::write()
{
	PropPage::write(*this, items);

	CComboBox cb1(GetDlgItem(IDC_AUTOPRIORITY_PATTERNS_PRIO));
	g_settings->set(SettingsManager::AUTO_PRIORITY_PATTERNS_PRIO, cb1.GetCurSel() + QueueItem::LOWEST);

	CComboBox cb2(GetDlgItem(IDC_AUTOPRIORITY_SIZE_PRIO));
	g_settings->set(SettingsManager::AUTO_PRIORITY_SMALL_SIZE_PRIO, cb2.GetCurSel() + QueueItem::LOWEST);
}

void PriorityPage::onChangeUsePatterns()
{
	BOOL state = IsDlgButtonChecked(IDC_AUTOPRIORITY_USE_PATTERNS) == BST_CHECKED;
	GetDlgItem(IDC_AUTOPRIORITY_PATTERNS).EnableWindow(state);
	GetDlgItem(IDC_AUTOPRIORITY_PATTERNS_PRIO).EnableWindow(state);
}

void PriorityPage::onChangeUseSize()
{
	BOOL state = IsDlgButtonChecked(IDC_AUTOPRIORITY_USE_SIZE) == BST_CHECKED;
	GetDlgItem(IDC_AUTOPRIORITY_SIZE).EnableWindow(state);
	GetDlgItem(IDC_AUTOPRIORITY_SIZE_PRIO).EnableWindow(state);
}

LRESULT PriorityPage::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_AUTOPRIORITY_USE_PATTERNS:
			onChangeUsePatterns();
			break;
		case IDC_AUTOPRIORITY_USE_SIZE:
			onChangeUseSize();
			break;
	}
	return 0;
}
