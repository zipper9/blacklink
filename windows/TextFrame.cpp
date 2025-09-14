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

#include "stdafx.h"

// TODO: delete temporary file after closing the window

#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
#include "TextFrame.h"
#include "Colors.h"
#include "Fonts.h"
#include "../client/File.h"
#include "../client/PathUtil.h"

TextFrame::TextFrame(const tstring& fileName) : file(fileName)
{
	SettingsManager::instance.addListener(this);
}

void TextFrame::openWindow(const tstring& fileName)
{
	TextFrame* frame = new TextFrame(fileName);
	frame->Create(WinUtil::mdiClient);
}

DWORD CALLBACK TextFrame::editStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	DWORD dwRet = 0xffffffff;
	File *pFile = reinterpret_cast<File *>(dwCookie);

	if (pFile)
	{
		try
		{
			size_t len = cb;
			*pcb = (LONG)pFile->read(pbBuff, len);
			dwRet = 0;
		}
		catch (const FileException& /*e*/)
		{
		}
	}

	return dwRet;
}

LRESULT TextFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlPad.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	               WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY, WS_EX_CLIENTEDGE);

	ctrlPad.LimitText(0);
	ctrlPad.SetFont(Fonts::g_font);

	try
	{
		File f(file, File::READ, File::OPEN);
		uint8_t buf[4];
		size_t size = sizeof(buf);
		memset(buf, 0, sizeof(buf));
		f.read(buf, size);

		static const uint8_t bUTF8signature[] = {0xef, 0xbb, 0xbf};
		static const uint8_t bUnicodeSignature[] = {0xff, 0xfe};
		int nFormat = SF_TEXT;
		if (memcmp(buf, bUTF8signature, sizeof(bUTF8signature)) == 0)
		{
			nFormat = SF_TEXT | SF_USECODEPAGE | (CP_UTF8 << 16);
			f.setPos(sizeof(bUTF8signature));
		}
		else if (memcmp(buf, bUnicodeSignature, sizeof(bUnicodeSignature)) == 0)
		{
			nFormat = SF_TEXT | SF_UNICODE;
			f.setPos(sizeof(bUnicodeSignature));
		}
		else
		{
			nFormat = SF_TEXT;
			f.setPos(0);
		}

		EDITSTREAM es =
		{
			(DWORD_PTR) &f,
			0,
			editStreamCallback
		};
		ctrlPad.StreamIn(nFormat, es);

		ctrlPad.EmptyUndoBuffer();
		setWindowTitle(Util::getFileName(file));
	}
	catch (const FileException& e)
	{
		tstring errorText = Util::getFileName(file) + _T(": ") + Text::toT(e.getError());
		setWindowTitle(errorText);
	}

	bHandled = FALSE;
	return 1;
}

LRESULT TextFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	SettingsManager::instance.removeListener(this);
	bHandled = FALSE;
	return 0;
}

LRESULT TextFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HWND hWnd = (HWND) lParam;
	HDC hDC = (HDC) wParam;
	if (hWnd == ctrlPad.m_hWnd)
		return Colors::setColor(hDC);
	bHandled = FALSE;
	return FALSE;
}

LRESULT TextFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::NOTEPAD, 0);
	opt->isHub = false;
	return TRUE;
}

void TextFrame::UpdateLayout(BOOL /*bResizeBars*/ /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	CRect rc;

	GetClientRect(rc);

	rc.bottom -= 1;
	rc.top += 1;
	rc.left += 1;
	rc.right -= 1;
	ctrlPad.MoveWindow(rc);
}

void TextFrame::on(SettingsManagerListener::ApplySettings)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

CFrameWndClassInfo& TextFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("TextFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::NOTEPAD, 0);

	return wc;
}

#endif
