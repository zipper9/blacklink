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

#ifndef LIST_VIEW_ARROWS_H
#define LIST_VIEW_ARROWS_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>

#ifdef OSVER_WIN_XP
#include "../client/CompatibilityManager.h"
#endif

extern CAppModule _Module;

template<class T>
class ListViewArrows
{
	public:
		typedef ListViewArrows<T> thisClass;

		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SETTINGCHANGE, onSettingChange)
		END_MSG_MAP()
		
		void updateArrow()
		{
			T* pThis = (T*)this;
			
#ifdef OSVER_WIN_XP
			if (CompatibilityManager::getComCtlVersion() >= MAKELONG(0, 6))
#endif
			{
				CHeaderCtrl headerCtrl = pThis->GetHeader();
				const int itemCount = headerCtrl.GetItemCount();
				for (int i = 0; i < itemCount; ++i)
				{
					HDITEM item = {0};
					item.mask = HDI_FORMAT;
					headerCtrl.GetItem(i, &item);
					
					//clear the previous state
					item.fmt &=  ~(HDF_SORTUP | HDF_SORTDOWN);
					
					if (i == pThis->getSortColumn())
					{
						item.fmt |= (pThis->isAscending() ? HDF_SORTUP : HDF_SORTDOWN);
					}
					
					headerCtrl.SetItem(i, &item);
				}
			}
		}
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			T* pThis = (T*)this;
			_Module.AddSettingChangeNotify(pThis->m_hWnd);
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			T* pThis = (T*)this;
			_Module.RemoveSettingChangeNotify(pThis->m_hWnd);
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onSettingChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			return 1;
		}
};

#endif // !defined(LIST_VIEW_ARROWS_H)
