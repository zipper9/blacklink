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
#include "../client/FavoriteManager.h"
#include "../client/SimpleStringTokenizer.h"
#include "ListViewArrows.h"

class ColumnInfo
{
	public:
		ColumnInfo(const tstring &aName, int aPos, uint16_t aFormat, int aWidth): m_name(aName), m_pos(aPos), m_width(aWidth),
			m_format(aFormat), m_is_visible(true), m_is_owner_draw(false)
		{}
		~ColumnInfo() {}
		tstring m_name;
		uint16_t m_format;
		int16_t  m_width;
		int8_t   m_pos;
		bool m_is_visible;
		bool m_is_owner_draw;
};

template<class T, int ctrlId>
class TypedListViewCtrl : public CWindowImpl<TypedListViewCtrl<T, ctrlId>, CListViewCtrl, CControlWinTraits>,
	public ListViewArrows<TypedListViewCtrl<T, ctrlId> >
{
	public:
		TypedListViewCtrl() : sortColumn(-1), sortAscending(true), hBrBg(Colors::g_bgBrush), leftMargin(0)
		{
		}
		~TypedListViewCtrl()
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
					ListView_SetItemState(m_hWnd, i, LVIS_SELECTED, LVIS_SELECTED);
					
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
					if (di->item.mask & LVIF_IMAGE) // http://support.microsoft.com/KB/141834
					{
						/*
						#ifdef _DEBUG
						        static int g_count = 0;
						        dcdebug("onGetDispInfo  count = %d di->item.iItem = %d di->item.iSubItem = %d, di->item.iIndent = %d, di->item.lParam = %d "
						                "mask = %d "
						                "state = %d "
						                "stateMask = %d "
						                "pszText = %d "
						                "cchTextMax = %d "
						                "iGroupId = %d "
						                "cColumns = %d "
						                "puColumns = %d "
						                "hdr.code = %d "
						                "hdr.hwndFrom = %d "
						                "hdr.idFrom = %d\n"
						                ,++g_count, di->item.iItem, di->item.iSubItem, di->item.iIndent, di->item.lParam,
						    di->item.mask,
						    di->item.state,
						    di->item.stateMask,
						    di->item.pszText,
						    di->item.cchTextMax,
						//    di->item.iImage,
						    di->item.iGroupId,
						    di->item.cColumns,
						    di->item.puColumns,
						    di->hdr.code,
						    di->hdr.hwndFrom,
						    di->hdr.idFrom
						                );
						#endif
						*/
						//[?] di->item.mask |= LVIF_DI_SETITEM;
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
				if(columnList[sortColumn].m_is_owner_draw) //TODO - проверить сортировку
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
		int insertItemLast(const T* item, int image, int p_position)
		{
			return insertItem(p_position, item, image);
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
			return GetSelectedCount();    // !SMT!-S
		}
		void forEachSelectedParam(void (T::*func)(void*), void *param)   // !SMT!-S
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(param);
		}
		
		void forEachSelectedParam(void (T::*func)(const string&, const tstring&), const string& hubHint, const tstring& p_message)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(hubHint, p_message);
		}
		void forEachSelectedParam(void (T::*func)(const string&, uint64_t), const string& hubHint, uint64_t p_data)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(hubHint, p_data);
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
			//CFlyLockWindowUpdate lockUpdate(m_hWnd);
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				T* item_data = getItemData(i);
				if (item_data)
					(item_data->*func)();
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
				if (columnList[j].m_is_owner_draw == false)
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
			else
			{
				//dcassert(i != -1);
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
				headerMenu.AppendMenu(MF_STRING, IDC_HEADER_MENU, i->m_name.c_str());
				if (i->m_is_visible)
					headerMenu.CheckMenuItem(j, MF_BYPOSITION | MF_CHECKED);
			}
			headerMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			return 1; // AirDC++
			/*          bHandled = FALSE;
			            if (!leftMargin || !hBrBg)
			                return 0;
			
			            dcassert(hBrBg);
			            if (!hBrBg) return 0;
			
			            bHandled = TRUE;
			            HDC dc = (HDC)wParam;
			            const int n = GetItemCount();
			            RECT r = {0, 0, 0, 0}, full;
			            GetClientRect(&full);
			
			            if (n > 0)
			            {
			                GetItemRect(0, &r, LVIR_BOUNDS);
			                r.bottom = r.top + ((r.bottom - r.top) * n);
			            }
			
			            RECT full2 = full; // Keep a backup
			
			
			            full.bottom = r.top;
			            FillRect(dc, &full, hBrBg);
			
			            full = full2; // Revert from backup
			            full.right = r.left + leftMargin; // state image
			            //full.left = 0;
			            FillRect(dc, &full, hBrBg);
			
			            full = full2; // Revert from backup
			            full.left = r.right;
			            FillRect(dc, &full, hBrBg);
			
			            full = full2; // Revert from backup
			            full.top = r.bottom;
			            full.right = r.right;
			            FillRect(dc, &full, hBrBg);
			            return S_OK;
			*/
		}
		void setFlickerFree(HBRUSH flickerBrush)
		{
			hBrBg = flickerBrush;
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
			ci.m_is_visible = !ci.m_is_visible;
			{
				CLockRedraw<true> lockRedraw(m_hWnd);
				if (!ci.m_is_visible)
				{
					removeColumn(ci);
				}
				else
				{
					if (ci.m_width == 0)
					{
						ci.m_width = 80;
					}
					CListViewCtrl::InsertColumn(ci.m_pos, ci.m_name.c_str(), ci.m_format, ci.m_width, static_cast<int>(wParam));
					LVCOLUMN lvcl = {0};
					lvcl.mask = LVCF_ORDER;
					lvcl.iOrder = ci.m_pos;
					SetColumn(ci.m_pos, &lvcl);
					updateAllImages(true); //[+]PPA
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
				lvc.cchTextMax = 512;
				lvc.pszText = buf;
				buf[0] = 0;
				GetColumn(i, &lvc);
				for (auto j = columnList.begin(); j != columnList.end(); ++j)
				{
					if (_tcscmp(buf, j->m_name.c_str()) == 0)
					{
						j->m_pos = lvc.iOrder;
						j->m_width = lvc.cx;
						break;
					}
				}
			}
			for (auto i = columnList.begin(); i != columnList.end(); ++i)
			{
				if (!visible.empty()) visible += ',';
				if (i->m_is_visible)
				{
					visible += '1';
				}
				else
				{
					i->m_pos = size++;
					visible += '0';
				}
				if (!order.empty()) order += ',';
				order += Util::toString(i->m_pos);
				if (!widths.empty()) widths += ',';
				widths += Util::toString(i->m_width);
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
					columnList[i].m_is_visible = false;
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
					lvc.iOrder = columnList[i].m_pos = columns[i];
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
		void setColumnOwnerDraw(const uint8_t p_col)
		{
			columnList[p_col].m_is_owner_draw = true;
		}

		bool ownsItemData = true;
		bool destroyingItems = false;

	private:
		int sortColumn;
		bool sortAscending;
		int leftMargin;
		HBRUSH hBrBg;
		CMenu headerMenu;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = T::compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn()); // https://www.box.net/shared/043aea731a61c46047fe
			return (t->sortAscending ? result : -result);
		}
		
		vector<ColumnInfo> columnList;
		vector<uint8_t> columnIndexes;
		
		void updateAllImages(bool p_updateItems = false)
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
				if (p_updateItems)
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
				if (_tcscmp(ci.m_name.c_str(), lvcl.pszText) == 0)
				{
					ci.m_width = lvcl.cx;
					ci.m_pos = lvcl.iOrder;
					
					int itemCount = GetHeader().GetItemCount();
					if (itemCount >= 0 && sortColumn > itemCount - 2)
						setSortColumn(0);
						
					if (sortColumn == ci.m_pos)
						setSortColumn(0);
						
					DeleteColumn(k);
					updateAllImages(); //[+]PPA
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
					if (stricmp(columnList[j].m_name.c_str(), lvcl.pszText) == 0)
					{
						columnIndexes.push_back(static_cast<uint8_t>(j));
						break;
					}
				}
			}
		}
	public:
		// TODO: remove this function
		void SetItemFilled(const LPNMLVCUSTOMDRAW p_cd, const CRect& p_rc2, COLORREF p_textColor = Colors::g_textColor, COLORREF p_textColorUnfocus = Colors::g_textColor)
		{
			COLORREF color;
			if (GetItemState((int)p_cd->nmcd.dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED)
			{
				if (m_hWnd == ::GetFocus())
				{
					// TODO: GetSysColor break the color of the theme for FlylinkDC? Use color from WinUtil to themed here?
					color = GetSysColor(COLOR_HIGHLIGHT);
					::SetBkColor(p_cd->nmcd.hdc, color);
					::SetTextColor(p_cd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
				}
				else
				{
					color = Colors::getAlternativBkColor(p_cd); // [!] IRainman
					::SetBkColor(p_cd->nmcd.hdc, color); // [!] IRainman
					::SetTextColor(p_cd->nmcd.hdc, p_textColorUnfocus);
				}
			}
			else
			{
				color = Colors::getAlternativBkColor(p_cd); // [!] IRainman
				::SetBkColor(p_cd->nmcd.hdc, color); // [!] IRainman
				::SetTextColor(p_cd->nmcd.hdc, p_textColor);
			}
			HGDIOBJ oldpen = ::SelectObject(p_cd->nmcd.hdc, CreatePen(PS_SOLID, 0, color));
			HGDIOBJ oldbr = ::SelectObject(p_cd->nmcd.hdc, CreateSolidBrush(color));
			Rectangle(p_cd->nmcd.hdc, p_rc2.left + 1, p_rc2.top, p_rc2.right, p_rc2.bottom);
			DeleteObject(::SelectObject(p_cd->nmcd.hdc, oldpen));
			DeleteObject(::SelectObject(p_cd->nmcd.hdc, oldbr));
		}
};

// Copyright (C) 2005-2009 Big Muscle, StrongDC++
template<class T, int ctrlId, class KValue>
class TypedTreeListViewCtrl : public TypedListViewCtrl<T, ctrlId>
{
	public:
	
		TypedTreeListViewCtrl() : uniqueParent(false) // [cppcheck] Member variable 'TypedTreeListViewCtrl<T,ctrlId,K,hashFunc,equalKey>::uniqueParent' is not initialized in the constructor.
		{
		}
		~TypedTreeListViewCtrl()
		{
			states.Destroy();
		}
		
		typedef TypedTreeListViewCtrl<T, ctrlId, KValue> thisClass;
		typedef TypedListViewCtrl<T, ctrlId> baseClass;
		
		struct ParentPair
		{
			T* parent;
			vector<T*> children;
		};
		
		typedef std::pair<KValue, ParentPair> ParentMapPair;
		typedef std::unordered_map<KValue, ParentPair> ParentMap;
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButton)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP();
		
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			ResourceLoader::LoadImageList(IDR_STATE, states, 16, 16);
			SetImageList(states, LVSIL_STATE);
			
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			CPoint pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			
			LVHITTESTINFO lvhti;
			lvhti.pt = pt;
			
			int pos = SubItemHitTest(&lvhti);
			if (pos != -1)
			{
				CRect rect;
				GetItemRect(pos, rect, LVIR_ICON);
				
				if (pt.x < rect.left)
				{
					T* i = getItemData(pos);
					if (i->parent == NULL)
					{
						if (i->collapsed)
						{
							Expand(i, pos);
						}
						else
						{
							Collapse(i, pos);
						}
					}
				}
			}
			
			bHandled = false;
			return 0;
		}
		
		void Collapse(T* parent, int itemPos)
		{
			SetRedraw(false);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			for (auto i = children.cbegin(); i != children.cend(); ++i)
			{
				deleteItem(*i);
			}
			parent->collapsed = true;
			SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			SetRedraw(true);
		}
		
		void Expand(T* parent, int itemPos)
		{
			SetRedraw(false);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			if (children.size() > (size_t)(uniqueParent ? 1 : 0))
			{
				parent->collapsed = false;
				for (auto i = children.cbegin(); i != children.cend(); ++i)
				{
					insertChild(*i, itemPos + 1);
				}
				SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
				resort();
			}
			SetRedraw(true);
		}
		
		void insertChild(const T* item, int idx)
		{
			LV_ITEM lvi;
			lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_INDENT;
			lvi.iItem = idx;
			lvi.iSubItem = 0;
			lvi.iIndent = 1;
			lvi.pszText = LPSTR_TEXTCALLBACK;
			lvi.iImage = item->getImageIndex();
			lvi.lParam = (LPARAM)item;
			lvi.state = 0;
			lvi.stateMask = 0;
			InsertItem(&lvi);
		}
		
		
		static const vector<T*> g_emptyVector;
		const vector<T*>& findChildren(const KValue& groupCond) const
		{
			dcassert(!destroyingItems);
			ParentMap::const_iterator i = parents.find(groupCond);
			if (i != parents.end())
			{
				return  i->second.children;
			}
			else
			{
				return g_emptyVector;
			}
		}
		
		ParentPair* findParentPair(const KValue& groupCond)
		{
			ParentMap::iterator i = parents.find(groupCond);
			if (i != parents.end())
			{
				return &i->second;
			}
			else
			{
				return nullptr;
			}
		}
		
		int insertChildNonVisual(T* item, ParentPair* pp, bool autoExpand, bool useVisual, bool useImageCallback)
		{
			dcassert(!destroyingItems);
			T* parent = nullptr;
			int pos = -1;
			if (pp->children.empty())
			{
				T* oldParent = pp->parent;
				parent = oldParent->createParent();
				if (parent != oldParent)
				{
					uniqueParent = true;
					parents.erase(oldParent->getGroupCond());
					deleteItem(oldParent);
					
					ParentPair newPP = { parent };
					pp = &(parents.insert(ParentMapPair(parent->getGroupCond(), newPP)).first->second);
					
					parent->parent = nullptr; // ensure that parent of this item is really NULL
					oldParent->parent = parent;
					pp->children.push_back(oldParent); // mark old parent item as a child
					parent->hits++;
					if (useVisual)
						pos = insertItem(getSortPos(parent), parent, useImageCallback ? I_IMAGECALLBACK : parent->getImageIndex());
				}
				else
				{
					uniqueParent = false;
					if (useVisual)
						pos = findItem(parent);
				}
				
				if (pos != -1)
				{
					if (autoExpand)
					{
						if (useVisual)
							SetItemState(pos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
						parent->collapsed = false;
					}
					else
					{
						if (useVisual)
							SetItemState(pos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
					}
				}
			}
			else
			{
				parent = pp->parent;
				if (useVisual)
					pos = findItem(parent);
			}
			
			pp->children.push_back(item);
			parent->hits++;
			item->parent = parent;
			if (pos != -1 && useVisual)
			{
				if (!parent->collapsed)
				{
					insertChild(item, pos + static_cast<int>(pp->children.size()));
				}
				updateItem(pos); // TODO - упростить?
			}
			
			return pos;
		}
		
		int insertGroupedItem(T* item, bool autoExpand, bool extra, bool useImageCallback)
		{
			T* parent = nullptr;
			ParentPair* pp = nullptr;
			
			if (!extra)
				pp = findParentPair(item->getGroupCond());
				
			int pos = -1;
			
			if (pp == nullptr)
			{
				parent = item;
				
				ParentPair newPP = { parent };
				dcassert(!destroyingItems);
				parents.insert(ParentMapPair(parent->getGroupCond(), newPP));
				
				parent->parent = nullptr; // ensure that parent of this item is really NULL
				pos = insertItem(getSortPos(parent), parent, useImageCallback ? I_IMAGECALLBACK : parent->getImageIndex());
				return pos;
			}
			else
			{
				pos = insertChildNonVisual(item, pp, autoExpand, true, useImageCallback);
			}
			return pos;
		}
		
		void removeParent(T* parent)
		{
			dcassert(!destroyingItems);
			CFlyBusyBool busy(destroyingItems);
			ParentPair* pp = findParentPair(parent->getGroupCond());
			if (pp)
			{
				for (auto i = pp->children.cbegin(); i != pp->children.cend(); ++i)
				{
					deleteItem(*i);
					if (ownsItemData)
						delete *i;
				}
				pp->children.clear();
				parents.erase(parent->getGroupCond());
			}
			deleteItem(parent);
		}
		
		void removeGroupedItem(T* item, bool removeFromMemory = true)
		{
			if (!item->parent)
			{
				removeParent(item);
			}
			else
			{
				dcassert(!destroyingItems);
				CFlyBusyBool busy(destroyingItems);
				T* parent = item->parent;
				ParentPair* pp = findParentPair(parent->getGroupCond());
				
				const auto l_id = deleteItem(item); // TODO - разобраться почему тут не удаляет.
				if (pp)
				{
					const auto n = find(pp->children.begin(), pp->children.end(), item);
					if (n != pp->children.end())
					{
						pp->children.erase(n);
						pp->parent->hits--;
					}
					
					if (uniqueParent)
					{
						dcassert(!pp->children.empty());
						if (pp->children.size() == 1)
						{
							const T* oldParent = parent;
							parent = pp->children.front();
							
							deleteItem(oldParent);
							parents.erase(oldParent->getGroupCond());
							if (ownsItemData)
								delete oldParent;
								
							ParentPair newPP = { parent };
							parents.insert(ParentMapPair(parent->getGroupCond(), newPP));
							
							parent->parent = nullptr; // ensure that parent of this item is really NULL
							deleteItem(parent);
							insertItem(getSortPos(parent), parent, parent->getImageIndex());
						}
					}
					else
					{
						if (pp->children.empty())
						{
							SetItemState(findItem(parent), INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
						}
					}
				}
				updateItem(parent);
			}
			
			if (removeFromMemory && ownsItemData)
				delete item;
		}
		
		void deleteAllNoLock()
		{
			// HACK: ugly hack but at least it doesn't crash and there's no memory leak
			DeleteAllItems();
			if (ownsItemData)
				for (auto i = parents.cbegin(); i != parents.cend(); ++i)
				{
					T* ti = i->second.parent;
					for (auto j : i->second.children)
						delete j;
					delete ti;
				}
			parents.clear();
		}

		void deleteAll()
		{
			dcassert(!destroyingItems);
			CFlyBusyBool busy(destroyingItems);
			CLockRedraw<> lockRedraw(m_hWnd);
			deleteAllNoLock();
		}
		
		LRESULT onColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
			if (l->iSubItem != getSortColumn())
			{
				setAscending(true);
				setSortColumn(l->iSubItem);
			}
			else if (isAscending())
			{
				setAscending(false);
			}
			else
			{
				setSortColumn(-1);
			}
			resort();
			return 0;
		}
		
		void resort()
		{
			dcassert(!destroyingItems);
			if (!destroyingItems)
			{
				if (getSortColumn() != -1)
				{
					SortItems(&compareFunc, (LPARAM)this);
				}
			}
		}
		
		int getSortPos(const T* a)
		{
			int high = GetItemCount();
			if ((getSortColumn() == -1) || (high == 0))
				return high;
				
			high--;
			
			int low = 0;
			int mid = 0;
			T* b = nullptr;
			int comp = 0;
			while (low <= high)
			{
				mid = (low + high) / 2;
				b = getItemData(mid);
				comp = compareItems(a, b, static_cast<uint8_t>(getSortColumn()));  // https://www.box.net/shared/9411c0b86a2a66b073af
				
				if (comp == 0)
					return mid;

				if (!isAscending())
					comp = -comp;
					
				if (comp < 0)
					high = mid - 1;
				else
					low = mid + 1;
			}
			
			comp = compareItems(a, b, static_cast<uint8_t>(getSortColumn()));
			if (!isAscending())
				comp = -comp;
			if (comp > 0)
				mid++;
				
			return mid;
		}
		ParentMap& getParents()
		{
			return parents;
		}
	private:
	
		/** map of all parent items with their associated children */
		ParentMap parents;
		/** +/- images */
		CImageList states;
		
		/** is extra item needed for parent items? */
		bool uniqueParent;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn());
			
			if (result == 2)
				result = (t->isAscending() ? 1 : -1);
			else if (result == -2)
				result = (t->isAscending() ? -1 : 1);
				
			return (t->isAscending() ? result : -result);
		}
		
		static int compareItems(const T* a, const T* b, uint8_t col)
		{
			// Copyright (C) Liny, RevConnect
			
			// both are children
			if (a->parent && b->parent)
			{
				// different parent
				if (a->parent != b->parent)
					return compareItems(a->parent, b->parent, col);
			}
			else
			{
				if (a->parent == b)
					return 2;  // a should be displayed below b
					
				if (b->parent == a)
					return -2; // b should be displayed below a
					
				if (a->parent)
					return compareItems(a->parent, b, col);
					
				if (b->parent)
					return compareItems(a, b->parent, col);
			}
			
			return T::compareItems(a, b, col);
		}
};

template<class T, int ctrlId, class KValue>
const vector<T*> TypedTreeListViewCtrl<T, ctrlId, KValue>::g_emptyVector;

#ifdef FLYLINKDC_USE_TREEE_LIST_VIEW_WITHOUT_POINTER
///////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, int ctrlId, class KValue>
class TypedTreeListViewCtrlSafe : public TypedListViewCtrl<T, ctrlId>
{
	public:
	
		TypedTreeListViewCtrlSafe() : destroyingItems(false), uniqueParent(false)
		{
		}
		~TypedTreeListViewCtrlSafe()
		{
			states.Destroy();
		}
		
		typedef TypedTreeListViewCtrlSafe<T, ctrlId, KValue> thisClass;
		typedef TypedListViewCtrl<T, ctrlId> baseClass;
		
		struct ParentPair
		{
			T* parent;
			vector<T*> children;
		};
		
		typedef std::pair<KValue, ParentPair> ParentMapPair;
		typedef std::unordered_map<KValue, ParentPair> ParentMap;
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButton)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP();
		
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			ResourceLoader::LoadImageList(IDR_STATE, states, 16, 16);
			SetImageList(states, LVSIL_STATE);
			
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			CPoint pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			
			LVHITTESTINFO lvhti;
			lvhti.pt = pt;
			
			int pos = SubItemHitTest(&lvhti);
			if (pos != -1)
			{
				CRect rect;
				GetItemRect(pos, rect, LVIR_ICON);
				
				if (pt.x < rect.left)
				{
					T* i = getItemData(pos);
					if (i->parent == NULL)
					{
						if (i->collapsed)
						{
							Expand(i, pos);
						}
						else
						{
							Collapse(i, pos);
						}
					}
				}
			}
			
			bHandled = false;
			return 0;
		}
		
		void Collapse(T* parent, int itemPos)
		{
			SetRedraw(false);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			for (auto i = children.cbegin(); i != children.cend(); ++i)
			{
				deleteItem(*i);
			}
			parent->collapsed = true;
			SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			SetRedraw(true);
		}
		
		void Expand(T* parent, int itemPos)
		{
			SetRedraw(false);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			if (children.size() > (size_t)(uniqueParent ? 1 : 0))
			{
				parent->collapsed = false;
				for (auto i = children.cbegin(); i != children.cend(); ++i)
				{
					insertChild(*i, itemPos + 1);
				}
				SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
				resort();
			}
			SetRedraw(true);
		}
		
		void insertChild(const T* item, int idx)
		{
			LV_ITEM lvi;
			lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_INDENT;
			lvi.iItem = idx;
			lvi.iSubItem = 0;
			lvi.iIndent = 1;
			lvi.pszText = LPSTR_TEXTCALLBACK;
			lvi.iImage = item->getImageIndex();
			lvi.lParam = (LPARAM)item;
			lvi.state = 0;
			lvi.stateMask = 0;
			InsertItem(&lvi);
		}
		
		static const vector<T*> g_emptyVector;
		const vector<T*>& findChildren(const KValue& groupCond) const
		{
			ParentMap::const_iterator i = parents.find(groupCond);
			if (i != parents.end())
			{
				return  i->second.children;
			}
			else
			{
				return g_emptyVector;
			}
		}
		
		ParentPair* findParentPair(const KValue& groupCond)
		{
			auto i = parents.find(groupCond);
			if (i != parents.end())
			{
				return &i->second;
			}
			else
			{
				return nullptr;
			}
		}
		
		int insertChildNonVisual(T* item, ParentPair* pp, bool p_auto_expand, bool p_use_visual, bool p_use_image_callback)
		{
			T* parent = nullptr;
			int pos = -1;
			if (pp->children.empty())
			{
				T* oldParent = pp->parent;
				parent = oldParent->createParent();
				if (parent != oldParent)
				{
					uniqueParent = true;
					parents.erase(oldParent->getGroupCond());
					deleteItem(oldParent);
					
					ParentPair newPP = { parent };
					pp = &(parents.insert(ParentMapPair(parent->getGroupCond(), newPP)).first->second);
					
					parent->parent = nullptr; // ensure that parent of this item is really NULL
					oldParent->parent = parent;
					pp->children.push_back(oldParent); // mark old parent item as a child
					parent->hits++;
					if (p_use_visual)
					{
						pos = insertItem(getSortPos(parent), parent, p_use_image_callback ? I_IMAGECALLBACK : parent->getImageIndex());
					}
				}
				else
				{
					uniqueParent = false;
					if (p_use_visual)
					{
						pos = findItem(parent);
					}
				}
				
				if (pos != -1)
				{
					if (p_auto_expand)
					{
						if (p_use_visual)
							SetItemState(pos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
						parent->collapsed = false;
					}
					else
					{
						if (p_use_visual)
							SetItemState(pos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
					}
				}
			}
			else
			{
				parent = pp->parent;
				if (p_use_visual)
					pos = findItem(parent);
			}
			
			pp->children.push_back(item);
			parent->hits++;
			item->parent = parent;
			if (pos != -1 && p_use_visual)
			{
				if (!parent->collapsed)
				{
					insertChild(item, pos + static_cast<int>(pp->children.size()));
				}
				updateItem(pos); // TODO - упростить?
			}
			
			return pos;
		}
		
		int insertGroupedItem(T* item, bool autoExpand, bool extra, bool p_use_image_callback)
		{
			T* parent = nullptr;
			ParentPair* pp = nullptr;
			
			if (!extra)
				pp = findParentPair(item->getGroupCond());
				
			int pos = -1;
			
			if (pp == NULL)
			{
				parent = item;
				
				ParentPair newPP = { parent };
				parents.insert(ParentMapPair(parent->getGroupCond(), newPP));
				
				parent->parent = nullptr; // ensure that parent of this item is really NULL
				pos = insertItem(getSortPos(parent), parent, p_use_image_callback ? I_IMAGECALLBACK : parent->getImageIndex());
				return pos;
			}
			else
			{
				pos = insertChildNonVisual(item, pp, autoExpand, true, p_use_image_callback);
			}
			return pos;
		}
		
		void removeParent(T* parent)
		{
			ParentPair* pp = findParentPair(parent->getGroupCond());
			if (pp)
			{
				for (auto i = pp->children.cbegin(); i != pp->children.cend(); ++i)
				{
					deleteItem(*i);
					delete *i;
				}
				pp->children.clear();
				parents.erase(parent->getGroupCond());
			}
			deleteItem(parent);
		}
		
		void removeGroupedItem(T* item, bool removeFromMemory = true)
		{
			if (!item->parent)
			{
				removeParent(item);
			}
			else
			{
				T* parent = item->parent;
				ParentPair* pp = findParentPair(parent->getGroupCond());
				
				deleteItem(item); // TODO - разобраться почему тут не удаляет.
				
				const auto n = find(pp->children.begin(), pp->children.end(), item);
				if (n != pp->children.end())
				{
					pp->children.erase(n);
					pp->parent->hits--;
				}
				
				if (uniqueParent)
				{
					dcassert(!pp->children.empty());
					if (pp->children.size() == 1)
					{
						const T* oldParent = parent;
						parent = pp->children.front();
						
						deleteItem(oldParent);
						parents.erase(oldParent->getGroupCond());
						delete oldParent;
						
						ParentPair newPP = { parent };
						parents.insert(ParentMapPair(parent->getGroupCond(), newPP));
						
						parent->parent = nullptr; // ensure that parent of this item is really NULL
						deleteItem(parent);
						insertItem(getSortPos(parent), parent, parent->getImageIndex());
					}
				}
				else
				{
					if (pp->children.empty())
					{
						SetItemState(findItem(parent), INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
					}
				}
				
				updateItem(parent);
			}
			
			if (removeFromMemory)
				delete item;
		}
		
		void DeleteAndClearAllItems() // [!] IRainman Dear BM: please use actual name!
		{
			CLockRedraw<> lockRedraw(m_hWnd);
			// HACK: ugly hack but at least it doesn't crash and there's no memory leak
			for (auto i = parents.cbegin(); i != parents.cend(); ++i)
			{
				T* ti = i->second.parent;
				for (auto j = i->second.children.cbegin(); j != i->second.children.cend(); ++j)
				{
					deleteItem(*j);
					delete *j;
				}
				deleteItem(ti);
				delete ti;
			}
			const int l_Count = GetItemCount();
			//dcassert(l_Count == 0)
			for (int i = 0; i < l_Count; i++)
			{
				T* si = getItemData(i);
				delete si; // https://drdump.com/DumpGroup.aspx?DumpGroupID=358387
			}
			
			parents.clear();
			DeleteAllItems();
		}
		
		LRESULT onColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
			if (l->iSubItem != getSortColumn())
			{
				setAscending(true);
				setSortColumn(l->iSubItem);
			}
			else if (isAscending())
			{
				setAscending(false);
			}
			else
			{
				setSortColumn(-1);
			}
			resort();
			return 0;
		}
		
		void resort()
		{
			if (getSortColumn() != -1)
			{
				SortItems(&compareFunc, (LPARAM)this);
			}
		}
		
		int getSortPos(const T* a)
		{
			int high = GetItemCount();
			if ((getSortColumn() == -1) || (high == 0))
				return high;
				
			high--;
			
			int low = 0;
			int mid = 0;
			T* b = nullptr;
			int comp = 0;
			while (low <= high)
			{
				mid = (low + high) / 2;
				b = getItemData(mid);
				comp = compareItems(a, b, static_cast<uint8_t>(getSortColumn()));  // https://www.box.net/shared/9411c0b86a2a66b073af
				
				if (!isAscending())
					comp = -comp;
					
				if (comp == 0)
				{
					return mid;
				}
				else if (comp < 0)
				{
					high = mid - 1;
				}
				else if (comp > 0)
				{
					low = mid + 1;
				}
				else if (comp == 2)
				{
					if (isAscending())
						low = mid + 1;
					else
						high = mid - 1;
				}
				else if (comp == -2)
				{
					if (!isAscending())
						low = mid + 1;
					else
						high = mid - 1;
				}
			}
			
			comp = compareItems(a, b, static_cast<uint8_t>(getSortColumn()));
			if (!isAscending())
				comp = -comp;
			if (comp > 0)
				mid++;
				
			return mid;
		}
		ParentMap& getParents()
		{
			return parents;
		}
		
	private:
	
		/** map of all parent items with their associated children */
		ParentMap parents;
		
		/** +/- images */
		CImageList states;
		
		/** is extra item needed for parent items? */
		bool uniqueParent;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn());
			
			if (result == 2)
				result = (t->isAscending() ? 1 : -1);
			else if (result == -2)
				result = (t->isAscending() ? -1 : 1);
				
			return (t->isAscending() ? result : -result);
		}
		
		static int compareItems(const T* a, const T* b, uint8_t col)
		{
			// Copyright (C) Liny, RevConnect
			
			// both are children
			if (a->parent && b->parent)
			{
				// different parent
				if (a->parent != b->parent)
					return compareItems(a->parent, b->parent, col);
			}
			else
			{
				if (a->parent == b)
					return 2;  // a should be displayed below b
					
				if (b->parent == a)
					return -2; // b should be displayed below a
					
				if (a->parent)
					return compareItems(a->parent, b, col);
					
				if (b->parent)
					return compareItems(a, b->parent, col);
			}
			
			return T::compareItems(a, b, col);
		}
};

template<class T, int ctrlId, class KValue>
const vector<T*> TypedTreeListViewCtrlSafe<T, ctrlId, KValue>::g_emptyVector;
#endif //  FLYLINKDC_USE_TREEE_LIST_VIEW_WITHOUT_POINTER
///////////////////////////////////////////////////////////////////////////////////////////////////


#endif // !defined(TYPED_LIST_VIEW_CTRL_H)
