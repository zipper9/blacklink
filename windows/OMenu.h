/*
 * Copyright (C) 2003-2004 "Opera", <opera at home dot se>
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

#ifndef __OMENU_H
#define __OMENU_H

#include <atlapp.h>
#include <atluser.h>
#include "typedefs.h"

class OMenu;
struct OMenuItem;

class OMenu final : private CMenu
{
	public:
		enum
		{
			OD_DEFAULT,
			OD_IF_NEEDED,
			OD_ALWAYS,
			OD_NEVER
		};

		OMenu();
		~OMenu();
		
		OMenu(const OMenu&) = delete;
		OMenu& operator= (const OMenu&) = delete;
		
		BOOL CreatePopupMenu();
		
		BOOL InsertSeparator(UINT uItem, BOOL byPosition, const tstring& caption);
		BOOL InsertSeparatorFirst(const tstring& caption)
		{
			return InsertSeparator(0, TRUE, caption);
		}
		BOOL InsertSeparatorLast(const tstring& caption)
		{
			return InsertSeparator(GetMenuItemCount(), TRUE, caption);
		}
		
		void RemoveFirstItem()
		{
			RemoveMenu(0, MF_BYPOSITION);
		}

		BOOL DeleteMenu(UINT nPosition, UINT nFlags)
		{
			checkOwnerDrawOnRemove(nPosition, nFlags & MF_BYPOSITION);
			return CMenu::DeleteMenu(nPosition, nFlags);
		}

		BOOL RemoveMenu(UINT nPosition, UINT nFlags)
		{
			checkOwnerDrawOnRemove(nPosition, nFlags & MF_BYPOSITION);
			return CMenu::RemoveMenu(nPosition, nFlags);
		}

		void ClearMenu()
		{
			for (int i = GetMenuItemCount() - 1; i >= 0; --i)
				DeleteMenu(i, MF_BYPOSITION);
		}

		BOOL DestroyMenu()
		{
			int count = GetMenuItemCount();
			for (int i = 0; i < count; i++)
				checkOwnerDrawOnRemove(i, MF_BYPOSITION);
			return CMenu::DestroyMenu();
		}
		
		BOOL InsertMenuItem(UINT uItem, BOOL bByPosition, LPMENUITEMINFO lpmii);
		
		BOOL AppendMenu(UINT nFlags, UINT_PTR nIDNewItem = 0, LPCTSTR lpszNewItem = nullptr, HBITMAP hBitmap = nullptr);

		BOOL AppendMenu(UINT nFlags, HMENU hSubMenu, LPCTSTR lpszNewItem, HBITMAP hBitmap = nullptr)
		{
			ATLASSERT(::IsMenu(hSubMenu));
			return AppendMenu(nFlags | MF_POPUP, (UINT_PTR) hSubMenu, lpszNewItem, hBitmap);
		}

		BOOL SetMenuDefaultItem(UINT id);

		bool SetBitmap(UINT item, BOOL byPosition, HBITMAP hBitmap);

		bool RenameItem(UINT id, const tstring& text);
		void* GetItemData(UINT id) const;

		void SetOwnerDraw(int mode);

		static LRESULT onInitMenuPopup(HWND hWnd, UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		static LRESULT onMeasureItem(HWND hWnd, UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		static LRESULT onDrawItem(HWND hWnd, UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		
		using CMenu::m_hMenu;
		using CMenu::operator HMENU;
		using CMenu::CheckMenuItem;
		using CMenu::EnableMenuItem;
		using CMenu::TrackPopupMenu;
		using CMenu::GetMenuItemCount;
		using CMenu::GetMenuItemInfo;
		using CMenu::GetMenuItemID;
		using CMenu::GetMenuState;

	private:
		int ownerDrawMode;
		vector<OMenuItem*> items;

		bool    themeInitialized;
		HTHEME  hTheme;
		MARGINS marginCheck;
		MARGINS marginCheckBackground;
		MARGINS marginItem;
		MARGINS marginText;
		MARGINS marginAccelerator;
		MARGINS marginSubmenu;
		MARGINS marginBitmap;
		SIZE    sizeCheck;
		SIZE    sizeSeparator;
		SIZE    sizeSubmenu;

		HFONT   fontNormal;
		HFONT   fontBold;

		bool    textMeasured;
		int     maxTextWidth;
		int     maxAccelWidth;
		int     maxBitmapWidth;
		
		void checkOwnerDrawOnRemove(UINT uItem, BOOL byPosition);
		void openTheme(HWND hwnd);
		void initFallbackParams();
		bool getMenuFont(LOGFONT& lf) const;
		void createNormalFont();
		void createBoldFont();
		void measureText(HDC hdc);
};

#define MESSAGE_HANDLER_HWND(msg, func) \
	if(uMsg == msg) \
	{ \
		bHandled = TRUE; \
		lResult = func(hWnd, uMsg, wParam, lParam, bHandled); \
		if(bHandled) \
			return TRUE; \
	}

#endif // __OMENU_H
