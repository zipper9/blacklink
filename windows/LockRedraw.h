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

#ifndef LOCK_REDRAW_H_
#define LOCK_REDRAW_H_

#include "../client/w.h"
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

template<bool INVALIDATE = false>
class CLockRedraw
{
	public:
		explicit CLockRedraw(HWND hWnd) noexcept : hWnd(hWnd)
		{
			if (hWnd)
			{
				ATLASSERT(::IsWindow(hWnd));
				::SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);
			}
		}

		~CLockRedraw() noexcept
		{
			if (hWnd)
			{
				::SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
				if (INVALIDATE) ::InvalidateRect(hWnd, nullptr, TRUE);
			}
		}

		CLockRedraw(const CLockRedraw&) = delete;
		CLockRedraw& operator= (const CLockRedraw&) = delete;

	private:
		const HWND hWnd;
};

#endif // LOCK_REDRAW_H_
