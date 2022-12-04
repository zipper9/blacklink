/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef DCPLUSPLUS_WTL_FLYLINKDC_H
#define DCPLUSPLUS_WTL_FLYLINKDC_H

#include "../client/w.h"
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include "../client/ResourceManager.h"

class CFlyToolTipCtrl : public CToolTipCtrl
{
	public:
		CFlyToolTipCtrl() {}
		CFlyToolTipCtrl(const CFlyToolTipCtrl&) = delete;
		CFlyToolTipCtrl& operator= (const CFlyToolTipCtrl&) = delete;	

		void AddTool(HWND hWnd, const wchar_t* p_Text = LPSTR_TEXTCALLBACK)
		{
			CToolInfo l_ti(TTF_SUBCLASS, hWnd, 0, nullptr, (LPWSTR) p_Text);
			ATLVERIFY(CToolTipCtrl::AddTool(&l_ti));
		}
		void AddTool(HWND p_hWnd, ResourceManager::Strings p_ID)
		{
			AddTool(p_hWnd, ResourceManager::getStringW(p_ID).c_str());
		}
};

template<bool needsInvalidate = false>
class CLockRedraw
{
	public:
		explicit CLockRedraw(const HWND p_hWnd) noexcept :
			m_hWnd(p_hWnd)
		{
			if (m_hWnd)
			{
				ATLASSERT(::IsWindow(m_hWnd));
				::SendMessage(m_hWnd, WM_SETREDRAW, (WPARAM)FALSE, 0);
			}
		}
		~CLockRedraw() noexcept
		{
			if (m_hWnd)
			{
				::SendMessage(m_hWnd, WM_SETREDRAW, (WPARAM)TRUE, 0);
				if (needsInvalidate)
				{
					::InvalidateRect(m_hWnd, nullptr, TRUE);
				}
			}
		}

		CLockRedraw(const CLockRedraw&) = delete;
		CLockRedraw& operator= (const CLockRedraw&) = delete;	
	
	private:
		const HWND m_hWnd;
};

#endif // DCPLUSPLUS_WTL_FLYLINKDC_H
