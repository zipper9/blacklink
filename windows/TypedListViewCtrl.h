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

#include "../client/SettingsManager.h"
#include "../client/SimpleStringTokenizer.h"
#include "ListViewArrows.h"
#include "resource.h"

class ColumnInfo
{
	public:
		ColumnInfo(const tstring &name, int pos, uint16_t format, int width):
			name(name), pos(pos), width(width), format(format), isVisible(true), isOwnerDraw(false)
		{}

		tstring  name;
		uint16_t format;
		int16_t  width;
		int8_t   pos;
		bool     isVisible;
		bool     isOwnerDraw;
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
						int subItem = static_cast<size_t>(di->item.iSubItem);
						auto index = columnIndexes[subItem];
						const auto& text = ((T*)di->item.lParam)->getText(index);
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
			if (!BOOLSETTING(SHOW_INFOTIPS)) return 0;
			
			NMLVGETINFOTIP* pInfoTip = (NMLVGETINFOTIP*) pnmh;
			const BOOL NoColumnHeader = (BOOL)(GetWindowLongPtr(GWL_STYLE) & LVS_NOCOLUMNHEADER);
			static const size_t BUF_SIZE = 300;
			TCHAR buf[BUF_SIZE];
			const int columnCount = GetHeader().GetItemCount();
			std::vector<int> indices(columnCount);
			GetColumnOrderArray(columnCount, indices.data());
			size_t outLen = 0;
			for (int i = 0; i < columnCount; ++i)
			{
				size_t prevLen = outLen;
				if (!NoColumnHeader)
				{
					LV_COLUMN lvCol = {0};
					lvCol.mask = LVCF_TEXT;
					lvCol.pszText = buf;
					lvCol.cchTextMax = BUF_SIZE;
					if (GetColumn(indices[i], &lvCol))
					{
						size_t len = _tcslen(lvCol.pszText);
						if (outLen + len + 2 >= INFOTIPSIZE) break; // no room
						memcpy(pInfoTip->pszText + outLen, lvCol.pszText, len*sizeof(TCHAR));
						outLen += len;
						_tcscpy(pInfoTip->pszText + outLen, _T(": "));
						outLen += 2;
					}
				}
				LVITEM lvItem = {0};
				lvItem.iItem = pInfoTip->iItem;
				int dataLen = GetItemText(pInfoTip->iItem, indices[i], buf, BUF_SIZE);
				if (dataLen <= 0) // empty data, skip it
				{
					outLen = prevLen;
					continue;
				}
				if (outLen + dataLen + 2 >= INFOTIPSIZE) // no room, stop
				{
					outLen = prevLen;
					break;
				}
				memcpy(pInfoTip->pszText + outLen, buf, dataLen*sizeof(TCHAR));
				outLen += dataLen;
				_tcscpy(pInfoTip->pszText + outLen, _T("\r\n"));
				outLen += 2;
			}
			
			if (outLen > 2) outLen -= 2;
				
			pInfoTip->pszText[outLen] = 0;
			pInfoTip->cchTextMax = static_cast<int>(outLen);
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

		int insertItemLast(const T* item, int image, int position)
		{
			return insertItem(position, item, image);
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
			{
				if (columnList[j].isOwnerDraw == false)
				{
					SetItemText(i, j, LPSTR_TEXTCALLBACK);
				}
			}
		}
		
		void updateItem(int i, int col)
		{
			SetItemText(i, col, LPSTR_TEXTCALLBACK);
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
		
		void setSortColumn(int aSortColumn)
		{
			sortColumn = aSortColumn;
			updateArrow();
		}

		int getSortColumn() const
		{
			return sortColumn;
		}

		uint8_t getRealSortColumn() const
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

		void setSortFromSettings(int column)
		{
			if (!column)
			{
				sortColumn = 0;
				sortAscending = true;
			} else
			{
				sortAscending = column > 0;
				sortColumn = abs(column) - 1;
			}
			updateArrow();
		}
		
		int InsertColumn(uint8_t nCol, const tstring &columnHeading, uint16_t nFormat = LVCFMT_LEFT, int nWidth = -1, int nSubItem = -1)
		{
			if (nWidth <= 0)
				nWidth = 80;
			columnList.push_back(ColumnInfo(columnHeading, nCol, nFormat, nWidth));
			columnIndexes.push_back(nCol);
			return CListViewCtrl::InsertColumn(nCol, columnHeading.c_str(), nFormat, nWidth, nSubItem);
		}

		void showMenu(const POINT &pt)
		{
			headerMenu.DestroyMenu();
			headerMenu.CreatePopupMenu();
			MENUINFO inf;
			inf.cbSize = sizeof(MENUINFO);
			inf.fMask = MIM_STYLE;
			inf.dwStyle = MNS_NOTIFYBYPOS;
			headerMenu.SetMenuInfo(&inf);
			
			int j = 0;
			for (auto i = columnList.cbegin(); i != columnList.cend(); ++i, ++j)
			{
				headerMenu.AppendMenu(MF_STRING, IDC_HEADER_MENU, i->name.c_str());
				if (i->isVisible)
					headerMenu.CheckMenuItem(j, MF_BYPOSITION | MF_CHECKED);
			}
			headerMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			return TRUE;
		}

		LRESULT onContextMenu(UINT /*msg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
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
			ColumnInfo& ci = columnList[wParam];
			ci.isVisible = !ci.isVisible;
			{
				CLockRedraw<true> lockRedraw(m_hWnd);
				if (!ci.isVisible)
				{
					removeColumn(ci);
				}
				else
				{
					if (ci.width == 0) ci.width = 80;
					CListViewCtrl::InsertColumn(ci.pos, ci.name.c_str(), ci.format, ci.width, static_cast<int>(wParam));
					LVCOLUMN lvcl = {0};
					lvcl.mask = LVCF_ORDER;
					lvcl.iOrder = ci.pos;
					SetColumn(ci.pos, &lvcl);
					updateAllImages(true);
				}
				
				updateColumnIndexes();
			}
			
			UpdateWindow();
			
			return 0;
		}
		
		
		void saveHeaderOrder(SettingsManager::StrSetting order, SettingsManager::StrSetting widths,
		                     SettingsManager::StrSetting visible)
		{
			string tmp, tmp2, tmp3;
			
			saveHeaderOrder(tmp, tmp2, tmp3);
			
			SettingsManager::set(order, tmp);
			SettingsManager::set(widths, tmp2);
			SettingsManager::set(visible, tmp3);
		}
		
		void saveHeaderOrder(string& order, string& widths, string& visible) noexcept
		{
			TCHAR buf[512];
			int size = GetHeader().GetItemCount();
			for (int i = 0; i < size; ++i)
			{
				LVCOLUMN lvc = {0};
				lvc.mask = LVCF_TEXT | LVCF_ORDER | LVCF_WIDTH;
				lvc.cchTextMax = _countof(buf);
				lvc.pszText = buf;
				buf[0] = 0;
				GetColumn(i, &lvc);
				for (auto j = columnList.begin(); j != columnList.end(); ++j)
				{
					if (_tcscmp(buf, j->name.c_str()) == 0)
					{
						j->pos = lvc.iOrder;
						j->width = lvc.cx;
						break;
					}
				}
			}
			for (auto i = columnList.begin(); i != columnList.end(); ++i)
			{
				if (!visible.empty()) visible += ',';
				if (i->isVisible)
				{
					visible += '1';
				}
				else
				{
					i->pos = size++;
					visible += '0';
				}
				if (!order.empty()) order += ',';
				order += Util::toString(i->pos);
				if (!widths.empty()) widths += ',';
				widths += Util::toString(i->width);
			}
		}
		
		void setVisible(const string& vis)
		{
			SimpleStringTokenizer<char> st(vis, ',');
			string tok;
			size_t i = 0;
			CLockRedraw<> lockRedraw(m_hWnd);
			while (i < columnList.size() && st.getNextToken(tok))
			{			
				if (Util::toInt(tok) == 0)
				{
					columnList[i].isVisible = false;
					removeColumn(columnList[i]);
				}
				++i;
			}
			updateColumnIndexes();
		}
		
		void setColumnOrderArray(size_t iCount, const int* columns)
		{
			LVCOLUMN lvc = {0};
			lvc.mask = LVCF_ORDER;
			
			int j = 0;
			for (size_t i = 0; i < iCount;)
			{
				if (columns[i] == j)
				{
					lvc.iOrder = columnList[i].pos = columns[i];
					SetColumn(static_cast<int>(i), &lvc);
					
					j++;
					i = 0;
				}
				else
				{
					i++;
				}
			}
		}
		
		//find the original position of the column at the current position.
		uint8_t findColumn(uint8_t col) const
		{
			dcassert(col < columnIndexes.size());
			return columnIndexes[col];
		}
		
		T* getSelectedItem() const
		{
			return GetSelectedCount() > 0 ? getItemData(GetNextItem(-1, LVNI_SELECTED)) : nullptr;
		}

		void setColumnOwnerDraw(int col)
		{
			columnList[col].isOwnerDraw = true;
		}

		bool ownsItemData = true;
		bool destroyingItems = false;

	private:
		int sortColumn;
		bool sortAscending;
		int leftMargin;
		CMenu headerMenu;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = T::compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn()); // https://www.box.net/shared/043aea731a61c46047fe
			return (t->sortAscending ? result : -result);
		}
		
		vector<ColumnInfo> columnList;
		vector<uint8_t> columnIndexes;
		
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
		void removeColumn(ColumnInfo& ci)
		{
			TCHAR buf[512];
			buf[0] = 0;
			LVCOLUMN lvcl = {0};
			lvcl.mask = LVCF_TEXT | LVCF_ORDER | LVCF_WIDTH;
			lvcl.pszText = buf;
			lvcl.cchTextMax = _countof(buf);;
			for (int k = 0; k < GetHeader().GetItemCount(); ++k)
			{
				GetColumn(k, &lvcl);
				if (_tcscmp(ci.name.c_str(), lvcl.pszText) == 0)
				{
					ci.width = lvcl.cx;
					ci.pos = lvcl.iOrder;
					
					int itemCount = GetHeader().GetItemCount();
					if (itemCount >= 0 && sortColumn > itemCount - 2)
						setSortColumn(0);
						
					if (sortColumn == ci.pos)
						setSortColumn(0);
						
					DeleteColumn(k);
					updateAllImages();
					break;
				}
			}
		}
		
		void updateColumnIndexes()
		{
			columnIndexes.clear();
			
			const int columns = GetHeader().GetItemCount();
			
			columnIndexes.reserve(static_cast<size_t>(columns));
			
			TCHAR buf[128];
			buf[0] = 0;
			LVCOLUMN lvcl = {0};
			
			for (int i = 0; i < columns; ++i)
			{
				lvcl.mask = LVCF_TEXT;
				lvcl.pszText = buf;
				lvcl.cchTextMax = _countof(buf);
				GetColumn(i, &lvcl);
				
				for (size_t j = 0; j < columnList.size(); ++j)
				{
					if (stricmp(columnList[j].name.c_str(), lvcl.pszText) == 0)
					{
						columnIndexes.push_back(static_cast<uint8_t>(j));
						break;
					}
				}
			}
		}
};

#endif // !defined(TYPED_LIST_VIEW_CTRL_H)
