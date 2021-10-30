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

SettingsManager * g_settings;

void PropPage::read(HWND page, const Item* items, const ListItem* listItems /* = nullptr */, HWND list /* = 0 */)
{
#ifdef _DEBUG
	m_check_read_write++;
#endif
	dcassert(page != NULL);
	
	if (items)
	{
		const bool useDef = true;
		for (const Item* i = items; i->type != T_END; i++)
		{
			switch (i->type)
			{
				case T_STR:
					if (GetDlgItem(page, i->itemID) == NULL)
					{
						dcassert(0);
						break;
					}
					::SetDlgItemText(page, i->itemID,
					                 Text::toT(g_settings->get((SettingsManager::StrSetting) i->setting, useDef)).c_str());
					break;

				case T_INT:			
					if (GetDlgItem(page, i->itemID) == NULL)
					{
						dcassert(0);
						break;
					}
					::SetDlgItemInt(page, i->itemID,
					                g_settings->get((SettingsManager::IntSetting) i->setting, useDef), TRUE);
					break;

				case T_BOOL:
					if (GetDlgItem(page, i->itemID) == NULL)
					{
						dcassert(0);
						break;
					}
					if (SettingsManager::getBool((SettingsManager::IntSetting) i->setting, useDef))
						::CheckDlgButton(page, i->itemID, BST_CHECKED);
					else
						::CheckDlgButton(page, i->itemID, BST_UNCHECKED);
					break;

				default:
					dcassert(false);
					break;
			}
		}
	}

	if (listItems)
	{
		CListViewCtrl ctrl(list);
		CRect rc;
		ctrl.GetClientRect(rc);
		ctrl.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
		SET_LIST_COLOR_IN_SETTING(ctrl);
		WinUtil::setExplorerTheme(ctrl);
		ctrl.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
		
		LVITEM lvi = {0};
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 0;
		
		for (int i = 0; listItems[i].setting != 0; i++)
		{
			lvi.iItem = i;
			lvi.pszText = const_cast<TCHAR*>(CTSTRING_I(listItems[i].desc));
			ctrl.InsertItem(&lvi);
			ctrl.SetCheckState(i, SettingsManager::getBool((SettingsManager::IntSetting) listItems[i].setting, true));
		}
		ctrl.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	}
}

void PropPage::write(HWND page, const Item* items, const ListItem* listItems /* = nullptr */, HWND list /* = NULL */)
{
#ifdef _DEBUG
	m_check_read_write--;
#endif
	dcassert(page != NULL);
	
	bool showWarning = false;
	
	if (items)
	{
		for (const Item* i = items; i->type != T_END; ++i)
		{
			tstring buf;
			switch (i->type)
			{
				case T_STR:
				{
					HWND dlgItem = GetDlgItem(page, i->itemID);
					if (!dlgItem)
					{
						dcassert(0);
						break;
					}
					WinUtil::getWindowText(dlgItem, buf);
					showWarning |= g_settings->set((SettingsManager::StrSetting) i->setting, Text::fromT(buf));
					// Crash https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=78416
					break;
				}

				case T_INT:
				{
					HWND dlgItem = GetDlgItem(page, i->itemID);
					if (!dlgItem)
					{
						dcassert(0);
						break;
					}
					WinUtil::getWindowText(dlgItem, buf);
					showWarning |= g_settings->set((SettingsManager::IntSetting) i->setting, Util::toInt(buf));
					break;
				}

				case T_BOOL:
				{
					if (!GetDlgItem(page, i->itemID))
					{
						dcassert(0);
						break;
					}
					bool value = ::IsDlgButtonChecked(page, i->itemID) == BST_CHECKED;
					showWarning |= g_settings->set((SettingsManager::IntSetting) i->setting, value);
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
			showWarning |= SET_SETTING(IntSetting(listItems[i].setting), ctrl.GetCheckState(i));
	}
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
	dcassert(page != NULL);
	cancel_check();
}
