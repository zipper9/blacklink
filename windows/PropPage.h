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
#include "../client/typedefs.h"
#include "resource.h"
#include "PropPageIcons.h"
#include "../client/ResourceManager.h"

class PropPage
{
	public:
		PropPage(const tstring& title) : m_title(title)
		{
		}
		virtual ~PropPage()
		{
		}
		
		virtual PROPSHEETPAGE *getPSP() = 0;
		virtual int getPageIcon() const = 0;
		virtual void write() = 0;
		virtual void cancel() {}
		virtual void onHide() {}
		virtual void onShow() {}
		virtual void onTimer() {}

		enum Type { T_STR, T_INT, T_BOOL, T_END };
		
		enum
		{
			FLAG_INVERT          = 1,
			FLAG_CREATE_SPIN     = 2,
			FLAG_DEFAULT_AS_HINT = 4
		};

		struct Item
		{
			WORD itemID;
			int setting;
			Type type;
			int flags;
		};

		struct ListItem
		{
			int setting;
			ResourceManager::Strings desc;
		};

		PropPage(const PropPage &) = delete;
		PropPage& operator= (const PropPage &) = delete;

		static void initControls(HWND page, const Item* items);
		static void read(HWND page, const Item* items, const ListItem* listItems = nullptr, HWND list = NULL);
		static void write(HWND page, const Item* items, const ListItem* listItems = nullptr, HWND list = NULL);

	protected:
		tstring m_title;
		void cancel(HWND page);
		bool getBoolSetting(const ListItem* listItems, HWND list, int setting);
};

#endif // !defined(PROP_PAGE_H)
