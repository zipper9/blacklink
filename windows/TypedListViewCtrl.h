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


#ifndef TYPED_LIST_VIEW_CTRL_H
#define TYPED_LIST_VIEW_CTRL_H

#include "../client/ResourceManager.h"
#include "../client/SettingsManager.h"
#include "ListViewArrows.h"
#include "resource.h"

class ColumnInfo
{
	public:
		int      id;
		tstring  name;
		uint16_t format;
		int16_t  width;
		bool     isVisible;
		bool     isOwnerDraw;
};

class TypedListViewColumns
{
	public:
		vector<ColumnInfo> columnList;
		vector<int> subItemToColumn;
		vector<int> columnToSubItem;
		CMenu headerMenu;
		static const bool cantRemoveFirstColumn = true;

		void setColumns(int count, const int* ids, const ResourceManager::Strings* names, const int* widths);
		void insertColumns(CListViewCtrl& lv, const string& order, const string& widths, const string& visible);
		void insertDummyColumn(CListViewCtrl& lv);
		void saveSettings(const CListViewCtrl& lv, string& order, string& widths, string& visible) const;
		void showMenu(const POINT& pt, HWND hWnd);
		void toggleColumn(CListViewCtrl& lv, int index, int& sortColumn, bool& doResort);
		static void getInfoTip(CListViewCtrl& lv, NMLVGETINFOTIP* pInfoTip);
};

template<class T, int ctrlId>
class TypedListViewCtrl : public CWindowImpl<TypedListViewCtrl<T, ctrlId>, CListViewCtrl, CControlWinTraits>,
	public ListViewArrows<TypedListViewCtrl<T, ctrlId> >
{
	public:
		TypedListViewCtrl() : sortColumn(-1), sortAscending(true), leftMargin(0)
		{
		}
		
		typedef TypedListViewCtrl<T, ctrlId> thisClass;
		typedef CListViewCtrl baseClass;
		typedef ListViewArrows<thisClass> arrowBase;
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_MENUCOMMAND, onHeaderMenu)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		CHAIN_MSG_MAP(arrowBase)
		END_MSG_MAP();
		
		bool isRedraw()
		{
			dcassert(!destroyingItems);
			bool refresh = false;
			if (GetBkColor() != Colors::g_bgColor)
			{
				SetBkColor(Colors::g_bgColor);
				SetTextBkColor(Colors::g_bgColor);
				refresh = true;
			}
			if (GetTextColor() != Colors::g_textColor)
			{
				SetTextColor(Colors::g_textColor);
				refresh = true;
			}
			return refresh;
		}
		
		LRESULT onChar(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			// https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1698
			if ((GetKeyState(VkKeyScan('A') & 0xFF) & 0xFF00) > 0 && (GetKeyState(VK_CONTROL) & 0xFF00) > 0)
			{
				const int cnt = GetItemCount();
				for (int i = 0; i < cnt; ++i)
					SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);

				return 0;
			}
			
			bHandled = FALSE;
			return 1;
		}
		
		void setText(LVITEM& i, const tstring &text)
		{
			_tcsncpy(i.pszText, text.c_str(), static_cast<size_t>(i.cchTextMax));
		}

		void setText(LVITEM& i, const TCHAR* text)
		{
			i.pszText = const_cast<TCHAR*>(text);
		}
		
		LRESULT onGetDispInfo(int /* idCtrl */, LPNMHDR pnmh, BOOL& /* bHandled */)
		{
			dcassert(!destroyingItems);
			NMLVDISPINFO* di = (NMLVDISPINFO*)pnmh;
			if (di && di->item.iSubItem >= 0)
			{
				if (di->item.lParam)
				{
					if (di->item.mask & LVIF_TEXT)
					{
						di->item.mask |= LVIF_DI_SETITEM;
						int column = findColumn(di->item.iSubItem);
						const auto& text = ((T*)di->item.lParam)->getText(column);
						setText(di->item, text);
					}
					if (di->item.mask & LVIF_IMAGE)
					{
						di->item.iImage = ((T*)di->item.lParam)->getImageIndex();
					}
					if (di->item.iSubItem == 0 && di->item.mask & LVIF_STATE)
					{
						di->item.state = INDEXTOSTATEIMAGEMASK(((T*)di->item.lParam)->getStateImageIndex());
					}
				}
			}
			return 0;
		}

		LRESULT onInfoTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			dcassert(!destroyingItems);
			TypedListViewColumns::getInfoTip(*this, (NMLVGETINFOTIP*) pnmh);
			return 0;
		}

		// Sorting
		LRESULT onColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			const NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
			if (l->iSubItem != sortColumn)
			{
				sortAscending = true;
				sortColumn = l->iSubItem;
			}
			else if (sortAscending)
			{
				sortAscending = false;
			}
			else
			{
				sortColumn = -1;
			}
			updateArrow();
			resort();
			return 0;
		}

		void resort()
		{
			dcassert(!destroyingItems);
			if (sortColumn != -1)
			{
				/*
				if(columnList[sortColumn].isOwnerDraw) //TODO - проверить сортировку
				                {
				                        const int l_item_count = GetItemCount();
				                        for(int i=0;i<l_item_count;++i)
				                        {
				                          // updateItem(i,sortColumn);
				                        }
				                }
				*/
				SortItems(&compareFunc, (LPARAM)this);
			}
		}

		int insertItemState(const T* item, int image, int state)
		{
			return insertItemState(getSortPos(item), item, image, INDEXTOSTATEIMAGEMASK(state));
		}

		int insertItemState(int i, const T* item, int image, int state)
		{
			return InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_STATE, i,
			                  LPSTR_TEXTCALLBACK, state, LVIS_STATEIMAGEMASK, image, (LPARAM)item); // TODO I_IMAGECALLBACK
		}

		int insertItem(const T* item, int image)
		{
			return insertItem(getSortPos(item), item, image);
		}

		int insertItem(int i, const T* item, int image)
		{
			return InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE, i,
			                  LPSTR_TEXTCALLBACK, 0, 0, image, (LPARAM)item); // TODO I_IMAGECALLBACK
		}
		
		T* getItemData(const int iItem) const
		{
			return (T*)GetItemData(iItem);
		}

	public:
		int getSelectedCount() const
		{
			return GetSelectedCount();
		}

		void forEachSelectedParam(void (T::*func)(void*), void *param)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(param);
		}
		
		void forEachSelectedParam(void (T::*func)(const string&, const tstring&), const string& hubHint, const tstring& message)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(hubHint, message);
		}

		void forEachSelectedParam(void (T::*func)(const string&, uint64_t), const string& hubHint, uint64_t data)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(hubHint, data);
		}
		
		int findItem(const T* item) const
		{
			LVFINDINFO fi = { 0 };
			fi.flags  = LVFI_PARAM;
			fi.lParam = (LPARAM)item;
			return FindItem(&fi, -1);
		}

		struct CompFirst
		{
			CompFirst() { }
			bool operator()(T& a, const tstring& b)
			{
				return stricmp(a.getText(0), b) < 0;
			}
		};

		int findItem(const tstring& b, int start = -1, bool aPartial = false) const
		{
			LVFINDINFO fi = { 0 };
			fi.flags  = aPartial ? LVFI_PARTIAL : LVFI_STRING;
			fi.psz = b.c_str();
			return FindItem(&fi, start);
		}

		void forEachSelected(void (T::*func)())
		{
			CLockRedraw<> lockRedraw(m_hWnd);
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				T* itemData = getItemData(i);
				if (itemData)
					(itemData->*func)();
			}
		}

		template<class _Function>
		_Function forEachT(_Function pred)
		{
			int cnt = GetItemCount();
			for (int i = 0; i < cnt; ++i)
			{
				T* itemData = getItemData(i);
				if (itemData)
					pred(itemData);
			}
			return pred;
		}

		template<class _Function>
		_Function forEachSelectedT(_Function pred)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				T* itemData = getItemData(i);
				if (itemData)
					pred(itemData);
			}
			return pred;
		}

		template<class _Function>
		_Function forFirstSelectedT(_Function pred)
		{
			int i = -1;
			if ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				T* itemData = getItemData(i);
				if (itemData)
					pred(itemData);
					
			}
			return pred;
		}

		void updateItem(int i)
		{
			const int cnt = GetHeader().GetItemCount();
			for (int j = 0; j < cnt; ++j)
				if (!columns.columnList[j].isOwnerDraw)
					SetItemText(i, j, LPSTR_TEXTCALLBACK);
		}
		
		void updateItem(int i, int column)
		{
			int index = columns.columnToSubItem[column];
			if (index >= 0)
				SetItemText(i, index, LPSTR_TEXTCALLBACK);
		}
		
		int updateItem(const T* item)
		{
			int i = findItem(item);
			if (i != -1)
			{
				updateItem(i);
			}
			else
			{
				dcassert(i != -1);
			}
			return i;
		}
		
		int deleteItem(const T* item)
		{
			int i = findItem(item);
			if (i != -1)
			{
				DeleteItem(i);
			}
			return i;
		}
		
		void deleteAllNoLock()
		{
			dcassert(!destroyingItems);
			if (ownsItemData)
			{
				const int count = GetItemCount();
				for (int i = 0; i < count; ++i)
				{
					T* si = getItemData(i);
					delete si;
				}
			}
			DeleteAllItems();
		}
		
		void deleteAll()
		{
			CLockRedraw<> lockRedraw(m_hWnd);
			deleteAllNoLock();
		}
		
		int getSortPos(const T* a) const
		{
			int high = GetItemCount();
			if (sortColumn == -1 || high == 0)
				return high;
				
			//PROFILE_THREAD_SCOPED()
			high--;
			
			int low = 0;
			int mid = 0;
			T* b = nullptr;
			int comp = 0;
			while (low <= high)
			{
				mid = (low + high) / 2;
				b = getItemData(mid);
				comp = T::compareItems(a, b, static_cast<uint8_t>(sortColumn));
				if (!sortAscending)
					comp = -comp;
					
				if (comp == 0)
					return mid;
				if (comp < 0)
					high = mid - 1;
				else
					low = mid + 1;
			}
			
			comp = T::compareItems(a, b, static_cast<uint8_t>(sortColumn));
			if (!sortAscending)
				comp = -comp;
			if (comp > 0)
				mid++;
				
			return mid;
		}
		
		int getSortColumn() const
		{
			return sortColumn;
		}

		void setSortColumn(int subItem)
		{
			sortColumn = subItem;
		}

		int getRealSortColumn() const
		{
			return findColumn(sortColumn);
		}

		bool isAscending() const
		{
			return sortAscending;
		}

		void setAscending(bool s)
		{
			sortAscending = s;
			updateArrow();
		}

		int getSortForSettings() const
		{
			int column = sortColumn + 1;
			if (!sortAscending) column = -column;
			return column;
		}

		void setSortFromSettings(int column, int defaultColumn = 0, bool defaultAscending = true)
		{
			if (!column || abs(column) > (int) columns.subItemToColumn.size())
			{
				sortColumn = findColumn(defaultColumn);
				sortAscending = defaultAscending;
			} else
			{
				sortAscending = column > 0;
				sortColumn = abs(column) - 1;
			}
			updateArrow();
		}
		
		void setColumns(int count, const int* ids, const ResourceManager::Strings* names, const int* widths)
		{
			columns.setColumns(count, ids, names, widths);
		}

		void setColumnFormat(int id, int format)
		{
			for (auto& c : columns.columnList)
				if (c.id == id)
				{
					c.format = format;
					break;
				}
		}

		void setColumnOwnerDraw(int id)
		{
			for (auto& c : columns.columnList)
				if (c.id == id)
				{
					c.isOwnerDraw = true;
					break;
				}
		}

		void insertColumns(const string& order, const string& widths, const string& visible)
		{
			columns.insertColumns(*this, order, widths, visible);
		}

		void insertColumns(SettingsManager::StrSetting order, SettingsManager::StrSetting widths, SettingsManager::StrSetting visible)
		{
			insertColumns(SettingsManager::get(order), SettingsManager::get(widths), SettingsManager::get(visible));
		}

		void insertDummyColumn()
		{
			columns.insertDummyColumn(*this);
		}

		void showMenu(const POINT &pt)
		{
			columns.showMenu(pt, m_hWnd);
		}
		
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			return TRUE;
		}

		LRESULT onContextMenu(UINT /*msg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			if (!enableHeaderMenu)
			{
				bHandled = FALSE;
				return 0;
			}

			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			//make sure we're not handling the context menu if it's invoked from the
			//keyboard
			if (pt.x == -1 && pt.y == -1)
			{
				bHandled = FALSE;
				return 0;
			}
			
			CRect rc;
			GetHeader().GetWindowRect(&rc);
			
			if (PtInRect(&rc, pt))
			{
				showMenu(pt);
				return 0;
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onHeaderMenu(UINT /*msg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			bool doResort;
			columns.toggleColumn(*this, wParam, sortColumn, doResort);
			if (doResort)
			{
				sortAscending = true;
				if (!columns.subItemToColumn.empty())
				{
					sortColumn = 0;
					resort();
				}
			}
			//updateAllImages(true);
			UpdateWindow();
			return 0;
		}
		
		void saveHeaderOrder(SettingsManager::StrSetting order, SettingsManager::StrSetting widths, SettingsManager::StrSetting visible)
		{
			string tmp, tmp2, tmp3;
			columns.saveSettings(*this, tmp, tmp2, tmp3);
			SettingsManager::set(order, tmp);
			SettingsManager::set(widths, tmp2);
			SettingsManager::set(visible, tmp3);
		}
		
		void saveHeaderOrder(string& order, string& widths, string& visible) noexcept
		{
			columns.saveSettings(*this, order, widths, visible);
		}
		
		int findColumn(int subItem) const
		{
			return columns.subItemToColumn[subItem];
		}
		
		T* getSelectedItem() const
		{
			return GetSelectedCount() > 0 ? getItemData(GetNextItem(-1, LVNI_SELECTED)) : nullptr;
		}

		bool enableHeaderMenu = true;
		bool ownsItemData = true;

	protected:
		bool destroyingItems = false;

	private:
		int sortColumn;
		bool sortAscending;
		int leftMargin;
		TypedListViewColumns columns;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = T::compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn());
			return (t->sortAscending ? result : -result);
		}
		
#if 0
		void updateAllImages(bool updateItems = false)
		{
			const int cnt = GetItemCount();
			for (int i = 0; i < cnt; ++i)
			{
				LVITEM lvItem = {0};
				lvItem.iItem = i;
				lvItem.iSubItem = 0;
				lvItem.mask = LVIF_PARAM | LVIF_IMAGE;
				GetItem(&lvItem);
				lvItem.iImage = ((T*)lvItem.lParam)->getImageIndex();
				SetItem(&lvItem);
				if (updateItems)
					updateItem(i);
			}
		}
#endif

		using CListViewCtrl::InsertColumn;
};

#endif // !defined(TYPED_LIST_VIEW_CTRL_H)
