/*
 * Copyright (C) 2010 Crise, crise<at>mail.berlios.de
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

// @todo Smarter width calculation for Vista+ in CheckMessageBoxProc

#include "stdafx.h"

#include "ExMessageBox.h"
#include "WinUtil.h"

#ifdef FLYLINKDC_SUPPORT_WIN_XP
#include "../client/CompatibilityManager.h"
#endif

ExMessageBox::MessageBoxValues ExMessageBox::mbv = {0};

static const UINT IDC_CHECK_BOX = 2025;

struct CheckBoxUserData
{
	const TCHAR* text;
	UINT state;
};

LRESULT CALLBACK ExMessageBox::CbtHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	switch (nCode)
	{
		case HCBT_CREATEWND:
		{
			LPCBT_CREATEWND lpCbtCreate = (LPCBT_CREATEWND)lParam;
			// MSDN says that lpszClass can't be trusted but this seems to be an exception
			if (lpCbtCreate->lpcs->lpszClass == WC_DIALOG && _tcscmp(lpCbtCreate->lpcs->lpszName, mbv.lpName) == 0)
			{
				mbv.hWnd = (HWND) wParam;
				mbv.lpMsgBoxProc = (WNDPROC) SetWindowLongPtr(mbv.hWnd, GWLP_WNDPROC, (LONG_PTR)mbv.lpMsgBoxProc);
			}
		}
		break;
	
		case HCBT_DESTROYWND:
		{
			if (mbv.hWnd == (HWND) wParam)
				mbv.lpMsgBoxProc = (WNDPROC) SetWindowLongPtr(mbv.hWnd, GWLP_WNDPROC, (LONG_PTR)mbv.lpMsgBoxProc);
		}
		break;
	}

	return CallNextHookEx(mbv.hHook, nCode, wParam, lParam);
}

// Helper function for CheckMessageBoxProc
static BOOL WINAPI ScreenToClient(HWND hWnd, LPRECT lpRect)
{
	if (!::ScreenToClient(hWnd, (LPPOINT)lpRect))
		return FALSE;
	return ::ScreenToClient(hWnd, ((LPPOINT)lpRect) + 1);
}

/**
 * Below CheckMessageBoxProc adds everyones favorite "don't show again" checkbox to the dialog
 * much of the layout code (especially for XP and older windows versions) is copied with changes
 * from a GPL'ed project emabox at SourceForge.
 **/
static LRESULT CALLBACK CheckMessageBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_COMMAND:
		{
			if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_CHECK_BOX)
			{
				const LRESULT res = SendMessage((HWND)lParam, BM_GETSTATE, 0, 0);
				bool checkedAfter = (res & BST_CHECKED) == 0;
				
				// Update user data
				CheckBoxUserData* ud = static_cast<CheckBoxUserData*>(ExMessageBox::GetUserData());
				ud->state = checkedAfter ? BST_CHECKED : BST_UNCHECKED;
				
				SendMessage((HWND)lParam, BM_SETCHECK, checkedAfter ? BST_CHECKED : BST_UNCHECKED, 0);
			}
		}
		break;

		case WM_ERASEBKGND:
		{
#ifdef FLYLINKDC_SUPPORT_WIN_XP
			if (!CompatibilityManager::isOsVistaPlus()) break;
#endif
			RECT rc = {0};
			HDC dc = (HDC)wParam;
				
			// Fill the entire dialog
			GetClientRect(hWnd, &rc);
			FillRect(dc, &rc, GetSysColorBrush(COLOR_WINDOW));
				
			// Calculate strip height
			RECT rcButton = {0};
			GetWindowRect(FindWindowEx(hWnd, nullptr, _T("BUTTON"), nullptr), &rcButton);
			int stripHeight = (rcButton.bottom - rcButton.top) + 24;
				
			// Fill the strip
			rc.top += (rc.bottom - rc.top) - stripHeight;
			FillRect(dc, &rc, GetSysColorBrush(COLOR_3DFACE));

#if 0
			// Make a line
			HGDIOBJ oldPen = SelectObject(dc, CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DLIGHT)));
			MoveToEx(dc, rc.left - 1, rc.top, (LPPOINT)NULL);
			LineTo(dc, rc.right, rc.top);
			DeleteObject(SelectObject(dc, oldPen));
#endif
			return TRUE;
		}
		break;

		case WM_CTLCOLORSTATIC:
		{
			// Vista+ has grey strip
			if ((HWND) lParam == GetDlgItem(hWnd, IDC_CHECK_BOX))
			{
				HDC hdc = (HDC)wParam;
				SetBkMode(hdc, TRANSPARENT);
				return (LRESULT)GetSysColorBrush(COLOR_3DFACE);
			}
		}
		break;

		case WM_INITDIALOG:
		{
			RECT rc = {0};
			const CheckBoxUserData* ud = static_cast<CheckBoxUserData*>(ExMessageBox::GetUserData());
			
			GetClientRect(hWnd, &rc);
			int clientHeightBefore = rc.bottom - rc.top;
			
			// Create checkbox (resized and moved later)
			HWND check = CreateWindow(_T("BUTTON"), ud->text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_VCENTER | BS_CHECKBOX,
			                          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			                          hWnd, (HMENU)(uintptr_t) IDC_CHECK_BOX, GetModuleHandle(nullptr), nullptr);
			                         
			SendMessage(check, BM_SETCHECK, ud->state, 0);
			
			// Apply default font
			const int cyMenuSize = GetSystemMetrics(SM_CYMENUSIZE);
			const int cxMenuSize = GetSystemMetrics(SM_CXMENUSIZE);
			const HFONT defaultFont = (HFONT) SendMessage(hWnd, WM_GETFONT, 0, 0);
			HFONT hOldFont;
			SIZE size;
			
			SendMessage(check, WM_SETFONT, (WPARAM) defaultFont, TRUE);
			
			// Get the size of the checkbox
			HDC hdc = GetDC(hWnd);
			hOldFont = (HFONT) SelectObject(hdc, defaultFont);
			GetTextExtentPoint32(hdc, ud->text, _tcslen(ud->text), &size);
			SelectObject(hdc, hOldFont);
			ReleaseDC(hWnd, hdc);
			
			// Checkbox dimensions
			int checkboxWidth = cxMenuSize + size.cx + 1;
			int checkboxHeight = cyMenuSize > size.cy ? cyMenuSize : size.cy;

			GetWindowRect(hWnd, &rc);
			int windowWidthBefore = rc.right - rc.left;
			int windowHeightBefore = rc.bottom - rc.top;
			int windowTopBefore = rc.top;
			int windowLeftBefore = rc.left;

			const int checkboxLeft = 16;
			const int checkboxSpace = 16;

			int leftButtonPos = windowWidthBefore;
			HWND current = nullptr;
			while ((current = FindWindowEx(hWnd, current, _T("BUTTON"), nullptr)) != nullptr)
			{
				if (current == check) continue;
				GetWindowRect(current, &rc);
				ScreenToClient(hWnd, &rc);
				if (rc.left < leftButtonPos) leftButtonPos = rc.left;
			}

			int offset = checkboxLeft + checkboxWidth + checkboxSpace - leftButtonPos;

			// Resize and re-center dialog
			int windowWidthAfter = windowWidthBefore + offset;
			int windowLeftAfter = windowLeftBefore + (windowWidthBefore - windowWidthAfter) / 2;
			MoveWindow(hWnd, windowLeftAfter, windowTopBefore, windowWidthAfter, windowHeightBefore, TRUE);

			// Align checkbox with buttons (approximately)
			int checkboxTop = clientHeightBefore - int(checkboxHeight * 1.70);
			MoveWindow(check, checkboxLeft, checkboxTop, checkboxWidth, checkboxHeight, FALSE);
				
			// Go through the buttons and move them
			current = nullptr;
			while ((current = FindWindowEx(hWnd, current, _T("BUTTON"), nullptr)) != nullptr)
			{
				if (current == check) continue;
				GetWindowRect(current, &rc);
				ScreenToClient(hWnd, &rc);
				MoveWindow(current, rc.left + offset, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
			}
		}
		break;
	}
	
	return CallWindowProc(ExMessageBox::GetMessageBoxProc(), hWnd, uMsg, wParam, lParam);
}

int ExMessageBox::Show(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType, WNDPROC wndProc)
{
	mbv.hHook = nullptr;
	mbv.hWnd = nullptr;
	mbv.lpMsgBoxProc = wndProc;
	mbv.typeFlags = uType;
	mbv.lpName = lpCaption;

	// Let's set up a CBT hook for this thread, and then call the standard MessageBox
	mbv.hHook = SetWindowsHookEx(WH_CBT, CbtHookProc, GetModuleHandle(nullptr), GetCurrentThreadId());

	int nRet = MessageBox(hWnd, lpText, lpCaption, uType);

	// And we're done
	UnhookWindowsHookEx(mbv.hHook);

	return nRet;
}

int MessageBoxWithCheck(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, LPCTSTR lpQuestion, UINT uType, UINT& uCheck)
{
	CheckBoxUserData ud = { lpQuestion, uCheck };
	ExMessageBox::SetUserData(&ud);
	int nRet = ExMessageBox::Show(hWnd, lpText, lpCaption, uType, CheckMessageBoxProc);
	uCheck = ud.state;
	ExMessageBox::SetUserData(nullptr);
	return nRet;
}
