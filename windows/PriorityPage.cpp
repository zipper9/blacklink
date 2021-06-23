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
#include "PriorityPage.h"
#include "DialogLayout.h"
#include "../client/SettingsManager.h"
#include "../client/QueueItem.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 3, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 6, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { 7, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_AUTOPRIORITY_FILENAME, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_AUTOPRIORITY_USE_PATTERNS, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_SET_PRIORITY1, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_AUTOPRIORITY_PATTERNS_PRIO, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_CAPTION_AUTOPRIORITY_FILESIZE, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_AUTOPRIORITY_USE_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_AUTOPRIORITY_SIZE, 0, UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_CAPTION_KIB, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align3 },
	{ IDC_CAPTION_SET_PRIORITY2, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_AUTOPRIORITY_SIZE_PRIO, 0, UNSPEC, UNSPEC, 0, &align1 }
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
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
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
