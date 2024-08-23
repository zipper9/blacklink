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
#include "PropPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

struct SettingsState
{
	enum
	{
		LOCK_NONE,
		LOCK_READ,
		LOCK_WRITE
	};

	Settings* ss;
	int locked;

	SettingsState() : ss(nullptr), locked(LOCK_NONE) {}

	void release()
	{
		if (locked == LOCK_NONE) return;
		if (locked == LOCK_READ)
			ss->unlockRead();
		else
			ss->unlockWrite();
		locked = LOCK_NONE;
	}

	void setRead(Settings* ss)
	{
		if (this->ss) return;
		this->ss = ss;
		ss->lockRead();
		locked = LOCK_READ;
	}

	void setWrite(Settings* ss)
	{
		if (this->ss) return;
		this->ss = ss;
		ss->lockWrite();
		locked = LOCK_WRITE;
	}
};

static inline bool isUiSetting(int id)
{
	return id >= 1024;
}

void PropPage::initControls(HWND page, const Item* items)
{
	for (const Item* i = items; i->type != T_END; i++)
	{
		BaseSettingsImpl* settings;
		if (isUiSetting(i->setting))
			settings = SettingsManager::instance.getUiSettings();
		else
			settings = SettingsManager::instance.getCoreSettings();
		if (i->type == T_INT && (i->flags & FLAG_CREATE_SPIN))
		{
			int minVal, maxVal;
			if (!settings->getIntRange(i->setting, minVal, maxVal) || minVal == INT_MIN || maxVal == INT_MAX)
			{
				dcassert(0);
				continue;
			}
			HWND hWnd = GetDlgItem(page, i->itemID);
			if (!hWnd)
			{
				dcassert(0);
				continue;
			}
			CUpDownCtrl spin;
			spin.Create(page, 0, nullptr, WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS);
			spin.SetRange32(minVal, maxVal);
			spin.SetBuddy(hWnd);
		}
		else if (i->type == T_STR && (i->flags & FLAG_DEFAULT_AS_HINT))
		{
			HWND hWnd = GetDlgItem(page, i->itemID);
			if (!hWnd)
			{
				dcassert(0);
				continue;
			}
			string text = settings->getStringDefault(i->setting);
			wstring ws;
			Text::utf8ToWide(text, ws);
			CEdit(hWnd).SetCueBannerText(ws.c_str());
		}
	}
}

void PropPage::read(HWND page, const Item* items, const ListItem* listItems /* = nullptr */, HWND list /* = 0 */)
{
	dcassert(page != NULL);

	SettingsState coreSettings;
	SettingsState uiSettings;
	SettingsState* state;
	tstring s;

	if (items)
	{
		for (const Item* i = items; i->type != T_END; i++)
		{
			if (isUiSetting(i->setting))
			{
				state = &uiSettings;
				if (!state->ss) state->setRead(SettingsManager::instance.getUiSettings());
			}
			else
			{
				state = &coreSettings;
				if (!state->ss) state->setRead(SettingsManager::instance.getCoreSettings());
			}

			HWND hWnd = GetDlgItem(page, i->itemID);
			if (!hWnd)
			{
				dcassert(0);
				continue;
			}
			switch (i->type)
			{
				case T_STR:
					Text::toT(state->ss->getString(i->setting), s);
					SetWindowText(hWnd, s.c_str());
					break;

				case T_INT:
					s = Util::toStringT(state->ss->getInt(i->setting));
					SetWindowText(hWnd, s.c_str());
					break;

				case T_BOOL:
				{
					bool value = state->ss->getBool(i->setting);
					if (i->flags & FLAG_INVERT) value = !value;
					SendMessage(hWnd, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
					break;
				}

				default:
					dcassert(0);
					break;
			}
		}
	}

	if (listItems)
	{
		CListViewCtrl ctrl(list);
		if (!ctrl.GetHeader().GetItemCount())
		{
			CRect rc;
			ctrl.GetClientRect(rc);
			ctrl.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
			WinUtil::setExplorerTheme(ctrl);
			ctrl.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
		}

		LVITEM lvi = {0};
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 0;

		if (ctrl.GetItemCount())
			ctrl.DeleteAllItems();

		for (int i = 0; listItems[i].setting != 0; i++)
		{
			int setting = listItems[i].setting;
			if (isUiSetting(setting))
			{
				state = &uiSettings;
				if (!state->ss) state->setRead(SettingsManager::instance.getUiSettings());
			}
			else
			{
				state = &coreSettings;
				if (!state->ss) state->setRead(SettingsManager::instance.getCoreSettings());
			}
			lvi.iItem = i;
			lvi.pszText = const_cast<TCHAR*>(CTSTRING_I(listItems[i].desc));
			ctrl.InsertItem(&lvi);
			ctrl.SetCheckState(i, state->ss->getBool(setting));
		}
		ctrl.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	}

	coreSettings.release();
	uiSettings.release();
}

void PropPage::write(HWND page, const Item* items, const ListItem* listItems /* = nullptr */, HWND list /* = NULL */)
{
	dcassert(page != NULL);

	bool showWarning = false;
	SettingsState coreSettings;
	SettingsState uiSettings;
	SettingsState* state;
	tstring s;

	if (items)
	{
		for (const Item* i = items; i->type != T_END; ++i)
		{
			if (isUiSetting(i->setting))
			{
				state = &uiSettings;
				if (!state->ss) state->setWrite(SettingsManager::instance.getUiSettings());
			}
			else
			{
				state = &coreSettings;
				if (!state->ss) state->setWrite(SettingsManager::instance.getCoreSettings());
			}

			HWND hWnd = GetDlgItem(page, i->itemID);
			if (!hWnd)
			{
				dcassert(0);
				continue;
			}
			switch (i->type)
			{
				case T_STR:
				{
					WinUtil::getWindowText(hWnd, s);
					if (state->ss->setString(i->setting, Text::fromT(s), Settings::SET_FLAG_FIX_VALUE) == Settings::RESULT_UPDATED)
						showWarning = true;
					break;
				}

				case T_INT:
				{
					WinUtil::getWindowText(hWnd, s);
					if (state->ss->setInt(i->setting, Util::toInt(s), Settings::SET_FLAG_FIX_VALUE) == Settings::RESULT_UPDATED)
						showWarning = true;
					break;
				}

				case T_BOOL:
				{
					bool value = SendMessage(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
					if (i->flags & FLAG_INVERT) value = !value;
					state->ss->setBool(i->setting, value);
					break;
				}

				default:
					dcassert(0);
					break;
			}
		}
	}

	if (listItems)
	{
		CListViewCtrl ctrl(list);
		for (int i = 0; listItems[i].setting != 0; i++)
		{
			int setting = listItems[i].setting;
			if (isUiSetting(setting))
			{
				state = &uiSettings;
				if (!state->ss) state->setWrite(SettingsManager::instance.getUiSettings());
			}
			else
			{
				state = &coreSettings;
				if (!state->ss) state->setWrite(SettingsManager::instance.getCoreSettings());
			}
			bool value = ctrl.GetCheckState(i) != FALSE;
			state->ss->setBool(setting, value);
		}
	}

	coreSettings.release();
	uiSettings.release();

#ifdef _DEBUG
	if (showWarning)
		MessageBox(page, _T("Values of the changed settings are automatically adjusted"), CTSTRING(WARNING), MB_OK);
#endif
}

bool PropPage::getBoolSetting(const ListItem* listItems, HWND list, int setting)
{
	for (int i = 0; listItems[i].setting; i++)
		if (listItems[i].setting == setting)
		{
			int res = CListViewCtrl(list).GetCheckState(i);
			return res != FALSE;
		}
	return false;
}

void PropPage::cancel(HWND page)
{
}
