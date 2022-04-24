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

#ifndef PROP_PAGE_H
#define PROP_PAGE_H

#include <atlcrack.h>
#include "resource.h"
#include "PropPageIcons.h"

class SettingsManager;
#include "../client/ResourceManager.h"

extern SettingsManager *g_settings;
class PropPage
{
	public:
		PropPage(const wstring& p_title) : m_title(p_title)
#ifdef _DEBUG
			, m_check_read_write(0)
#endif
		{
		}
		virtual ~PropPage()
		{
#if 0
			dcassert(m_check_read_write == 0);
#endif
		}
		
		virtual PROPSHEETPAGE *getPSP() = 0;
		virtual int getPageIcon() const { return PROP_PAGE_ICON_EMPTY; }
		virtual void write() = 0;
		virtual void cancel() = 0;
		virtual void onHide() {}
		virtual void onShow() {}
		virtual void onTimer() {}

		enum Type { T_STR, T_INT, T_BOOL, T_END };
		
		struct Item
		{
			WORD itemID;
			int setting;
			Type type;
		};
		struct ListItem
		{
			int setting;
			ResourceManager::Strings desc;
		};

		PropPage(const PropPage &) = delete;
		PropPage& operator= (const PropPage &) = delete;

	protected:
		wstring m_title;
		void read(HWND page, const Item* items, const ListItem* listItems = nullptr, HWND list = NULL);
		void write(HWND page, const Item* items, const ListItem* listItems = nullptr, HWND list = NULL);
		void cancel(HWND page);
		void cancel_check()
		{
#if 0
			dcassert(m_check_read_write > 0);
			m_check_read_write = 0;
#endif
		}
		bool getBoolSetting(const ListItem* listItems, HWND list, int setting);

#ifdef _DEBUG
	protected:
		int m_check_read_write;
#endif
};

#endif // !defined(PROP_PAGE_H)
