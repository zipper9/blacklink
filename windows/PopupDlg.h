/*
 * Copyright (C) 2003-2005 Pär Björklund, per.bjorklund@gmail.com
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

#ifndef POPUPWND_H
#define POPUPWND_H

#include "Resource.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "ConfUI.h"
#include "../client/TimeUtil.h"
#include "../client/SettingsManager.h"

class PopupWnd : public CWindowImpl<PopupWnd, CWindow>
{
	public:
		DECLARE_WND_CLASS(_T("Popup"));

		BEGIN_MSG_MAP(PopupWnd)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		END_MSG_MAP()

		PopupWnd(const tstring& text, const tstring& title, CRect rc, uint32_t id, HBITMAP hBmp): id(id), msg(text), title(title), m_bmp(hBmp), height(0)
		{
			timeCreated = GET_TICK();
			memset(&logFont, 0, sizeof(logFont));
			memset(&myFont, 0, sizeof(myFont));
			const auto* ss = SettingsManager::instance.getUiSettings();
			int popupType = ss->getInt(Conf::POPUP_TYPE);
			if (popupType == BALLOON || popupType == SPLASH)
				Create(NULL, rc, NULL, WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_TOOLWINDOW);
			else if (popupType == CUSTOM && m_bmp != NULL)
				Create(NULL, rc, NULL, WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_TOOLWINDOW);
			else
				Create(NULL, rc, NULL, WS_CAPTION | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_TOOLWINDOW);

			Fonts::decodeFont(Text::toT(ss->getString(Conf::POPUP_FONT)), logFont);
			font = ::CreateFontIndirect(&logFont);

			Fonts::decodeFont(Text::toT(ss->getString(Conf::POPUP_TITLE_FONT)), myFont);
			titlefont = ::CreateFontIndirect(&myFont);
		}
		
		~PopupWnd()
		{
			DeleteObject(font);
			DeleteObject(titlefont);
		}
		
		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			::PostMessage(WinUtil::g_mainWnd, WM_SPEAKER, WM_CLOSE, (LPARAM)id);
			bHandled = TRUE;
			return 0;
		}
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			const auto* ss = SettingsManager::instance.getUiSettings();
			int popupType = ss->getInt(Conf::POPUP_TYPE);
			if (m_bmp != NULL && popupType == CUSTOM)
			{
				bHandled = FALSE;
				return 1;
			}
			
			::SetClassLongPtr(m_hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)::GetSysColorBrush(COLOR_INFOTEXT));
			CRect rc;
			GetClientRect(rc);
			
			rc.top += 1;
			rc.left += 1;
			rc.right -= 1;
			if (popupType == BALLOON || popupType == CUSTOM || popupType == SPLASH)
				rc.bottom /= 3;
			else
				rc.bottom /= 4;
				
			label.Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			             SS_CENTER | SS_NOPREFIX);
			             
			rc.top += rc.bottom - 1;
			if (popupType == BALLOON || popupType == CUSTOM || popupType == SPLASH)
				rc.bottom *= 3;
			else
				rc.bottom = (rc.bottom * 4) + 1;
				
			label1.Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			              SS_CENTER | SS_NOPREFIX);

			if (popupType == BALLOON)
			{
				label.SetFont(Fonts::g_boldFont);
				label.SetWindowText(title.c_str());
				label1.SetFont(Fonts::g_font);
				label1.SetWindowText(msg.c_str());
				bHandled = FALSE;
				return 1;
			}
			if (popupType == CUSTOM || popupType == SPLASH)
			{
				label.SetFont(Fonts::g_boldFont);
				label.SetWindowText(title.c_str());
			}
			else
				SetWindowText(title.c_str());
				
			label1.SetFont(font);
			label1.SetWindowText(msg.c_str());
			
			bHandled = FALSE;
			return 1;
		}
		
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			const auto* ss = SettingsManager::instance.getUiSettings();
			int popupType = ss->getInt(Conf::POPUP_TYPE);
			if (m_bmp == NULL || popupType != CUSTOM)
			{
				if (label) label.DestroyWindow();
				if (label1) label1.DestroyWindow();
			}
			DestroyWindow();

			bHandled = FALSE;
			return 1;
		}
		
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			const HWND hWnd = (HWND)lParam;
			const HDC hDC = (HDC)wParam;
			const auto* ss = SettingsManager::instance.getUiSettings();
			::SetBkColor(hDC, ss->getInt(Conf::POPUP_BACKCOLOR));
			::SetTextColor(hDC, ss->getInt(Conf::POPUP_TEXTCOLOR));
			if (hWnd == label1.m_hWnd)
				::SelectObject(hDC, font);
			return (LRESULT) CreateSolidBrush(ss->getInt(Conf::POPUP_BACKCOLOR));
		}

		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			const auto* ss = SettingsManager::instance.getUiSettings();
			int popupType = ss->getInt(Conf::POPUP_TYPE);
			if (m_bmp == NULL || popupType != CUSTOM)
			{
				bHandled = FALSE;
				return 0;
			}

			PAINTSTRUCT ps;
			HDC hdc = ::BeginPaint(m_hWnd, &ps);

			HDC hdcMem = CreateCompatibleDC(NULL);
			HBITMAP hbmT = (HBITMAP)::SelectObject(hdcMem, m_bmp);

			BITMAP bm = {0};
			GetObject(m_bmp, sizeof(bm), &bm);
			
			//Move selected bitmap to the background
			BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);
			
			SelectObject(hdcMem, hbmT);
			DeleteDC(hdcMem);
			
			//Cofigure border
			::SetBkMode(hdc, TRANSPARENT);
			
			int xBorder = bm.bmWidth / 10;
			int yBorder = bm.bmHeight / 10;
			CRect rc(xBorder, yBorder, bm.bmWidth - xBorder, bm.bmHeight - yBorder);
			
			//Draw the Title and Message with selected font and color
			const tstring pmsg = _T("\r\n\r\n") + msg;
			HFONT oldTitleFont = (HFONT)SelectObject(hdc, titlefont);
			::SetTextColor(hdc, ss->getInt(Conf::POPUP_TITLE_TEXTCOLOR));
			::DrawText(hdc, title.c_str(), static_cast<int>(title.length()), rc, DT_SINGLELINE | DT_TOP | DT_CENTER);
			
			HFONT oldFont = (HFONT)SelectObject(hdc, font);
			::SetTextColor(hdc, ss->getInt(Conf::POPUP_TEXTCOLOR));
			::DrawText(hdc, pmsg.c_str(), static_cast<int>(pmsg.length()), rc, DT_LEFT | DT_WORDBREAK);
			
			SelectObject(hdc, oldTitleFont);
			SelectObject(hdc, oldFont);
			::EndPaint(m_hWnd, &ps);
			
			return 0;
		}
		
		uint32_t id;
		uint64_t timeCreated;
		uint16_t height;
		
		enum { BALLOON, CUSTOM, SPLASH, WINDOW };
		
	private:
		tstring  msg, title;
		CStatic label, label1;
		LOGFONT logFont;
		HFONT   font;
		LOGFONT myFont;
		HFONT   titlefont;
		HBITMAP m_bmp;
};

#endif
