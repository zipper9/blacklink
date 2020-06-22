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

#ifndef EX_LIST_VIEW_CTRL_H
#define EX_LIST_VIEW_CTRL_H

#include "../client/typedefs.h"
#include "ListViewArrows.h"

// FIXME: remove inefficient sorting/insertion

class ExListViewCtrl : public CWindowImpl<ExListViewCtrl, CListViewCtrl, CControlWinTraits>,
	public ListViewArrows<ExListViewCtrl>
{
		int sortColumn;
		int sortType;
		bool ascending;
		int (*fun)(LPARAM, LPARAM);
		
	public:
		enum
		{
			SORT_FUNC = 2,
			SORT_STRING,
			SORT_STRING_NOCASE,
			SORT_INT,
			SORT_FLOAT,
			SORT_BYTES
		};
		
		typedef ListViewArrows<ExListViewCtrl> arrowBase;
		
		BEGIN_MSG_MAP(ExListViewCtrl)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		CHAIN_MSG_MAP(arrowBase)
		END_MSG_MAP()
		bool isRedraw();
		void setSort(int aColumn, int aType, bool aAscending = true, int (*aFun)(LPARAM, LPARAM) = nullptr)
		{
			bool doUpdateArrow = (aColumn != sortColumn || aAscending != ascending);
			
			sortColumn = aColumn;
			sortType = aType;
			ascending = aAscending;
			fun = aFun;
			resort();
			if (doUpdateArrow)
				updateArrow();
		}
		
		void setSortFromSettings(int column, int type, int maxColumns)
		{
			bool ascending = column > 0;
			column = abs(column);
			if (column < 1 || column > maxColumns)
			{
				column = 0;
				ascending = true;
			} else column--;
			setSort(column, type, ascending);
		}

		int getSortForSettings() const
		{
			int column = sortColumn + 1;
			if (!ascending) column = -column;
			return column;
		}

		void resort()
		{
			if (sortColumn != -1)
			{
				SortItemsEx(&CompareFunc, (LPARAM)this);
			}
		}
		
		bool isAscending() const
		{
			return ascending;
		}
		
		void setAscending(bool s)
		{
			ascending = s;
			updateArrow();
		}
		int getSortColumn() const
		{
			return sortColumn;
		}
		int getSortType() const
		{
			return sortType;
		}
		
		int insert(int nItem, TStringList& aList, int iImage = 0, LPARAM lParam = 0);
		int insert(TStringList& aList, int iImage = 0, LPARAM lParam = 0);
		int insert(int nItem, const tstring& aString, int iImage = 0, LPARAM lParam = 0)
		{
			return InsertItem(LVIF_PARAM | LVIF_TEXT | LVIF_IMAGE, nItem, aString.c_str(), 0, 0, iImage, lParam);
		}
		
		int getItemImage(int aItem) const
		{
			LVITEM lvi = {0};
			lvi.iItem = aItem;
			lvi.iSubItem = 0;
			lvi.mask = LVIF_IMAGE;
			GetItem(&lvi);
			return lvi.iImage;
		}
		
		int find(LPARAM lParam, int aStart = -1) const
		{
			LV_FINDINFO fi;
			fi.flags = LVFI_PARAM;
			fi.lParam = lParam;
			return FindItem(&fi, aStart);
		}
		
		int find(const tstring& aText, int aStart = -1, bool aPartial = false) const
		{
			LV_FINDINFO fi;
			fi.flags = aPartial ? LVFI_PARTIAL : LVFI_STRING;
			fi.psz = aText.c_str();
			return FindItem(&fi, aStart);
		}
#if 0
		void deleteItem(const tstring& aItem, int col = 0)
		{
			const int l_cnt = GetItemCount();
			for (int i = 0; i < l_cnt; i++)
			{
				LocalArray<TCHAR, 256> buf;
				GetItemText(i, col, buf.data(), 256);
				if (aItem == buf.data())
				{
					DeleteItem(i);
					break;
				}
			}
		}
#endif
		int moveItem(int oldPos, int newPos);
		void setSortDirection(bool aAscending)
		{
			setSort(sortColumn, sortType, aAscending, fun);
		}
		
		static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
		LRESULT onChar(UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
		{
			return CDRF_DODEFAULT;
		}

		tstring ExGetItemTextT(int line, int col) const
		{
			TCHAR buf[256];
			GetItemText(line, col, buf, _countof(buf));
			return buf;
		}
		
		string ExGetItemText(int line, int col) const;
		
		template<class T> static int compare(const T& a, const T& b)
		{
			return (a < b) ? -1 : ((a == b) ? 0 : 1);
		}
		
		ExListViewCtrl() : sortType(SORT_STRING), ascending(true), sortColumn(-1), fun(nullptr) { }
		
		~ExListViewCtrl() { }
};

#endif // !defined(EX_LIST_VIEW_CTRL_H)
