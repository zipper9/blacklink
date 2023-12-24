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
#include "NotepadFrame.h"
#include "WinUtil.h"
#include "Colors.h"
#include "Fonts.h"
#include "../client/LogManager.h"
#include "../client/File.h"
#include "../client/AppPaths.h"

static int textUnderCursor(POINT p, CEdit& ctrl, tstring& x)
{
	const int i = ctrl.CharFromPos(p);
	const int line = ctrl.LineFromChar(i);
	const int c = LOWORD(i) - ctrl.LineIndex(line);
	const int len = ctrl.LineLength(i) + 1;
	if (len < 3)
		return 0;

	x.resize(len + 1);
	x.resize(ctrl.GetLine(line, &x[0], len + 1));

	string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
	if (start == string::npos)
		start = 0;
	else
		start++;

	return start;
}

LRESULT NotepadFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlPad.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	               WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL, WS_EX_CLIENTEDGE);
	               
	ctrlPad.LimitText(0);
	ctrlPad.SetFont(Fonts::g_font);
	string tmp;
	try
	{
		tmp = File(Util::getNotepadFile(), File::READ, File::OPEN).read();
	}
	catch (const FileException&)
	{
		//LogManager::message("Error read " + Util::getNotepadFile() + " Error = " + e.getError());
	}

	ctrlPad.SetWindowText(Text::toT(tmp).c_str());
	ctrlPad.EmptyUndoBuffer();
	ctrlClientContainer.SubclassWindow(ctrlPad.m_hWnd);
	SettingsManager::getInstance()->addListener(this);
	bHandled = FALSE;
	return 1;
}

LRESULT NotepadFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (hWnd == ctrlPad.m_hWnd)
		return Colors::setColor(hDC);
	bHandled = FALSE;
	return FALSE;
}

LRESULT NotepadFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::NOTEPAD, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT NotepadFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		closed = true;
		SettingsManager::getInstance()->removeListener(this);
		if (ctrlPad.GetModify())
		{
			tstring tmp;
			WinUtil::getWindowText(ctrlPad, tmp);
			if (!tmp.empty())
			{
				try
				{
					File(Util::getNotepadFile(), File::WRITE, File::CREATE | File::TRUNCATE).write(Text::fromT(tmp));
				}
				catch (const FileException& e)
				{
					LogManager::message("Error writing " + Util::getNotepadFile() + ": " + e.getError());
				}
			}
			else
				File::deleteFile(Util::getNotepadFile());
		}
		
		setButtonPressed(IDC_NOTEPAD, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		bHandled = FALSE;
		return 0;
	}
}

void NotepadFrame::UpdateLayout(BOOL /*bResizeBars*/ /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	CRect rc;
	
	GetClientRect(rc);
	
	rc.bottom -= 1;
	//rc.top += 1; [~] Sergey Shushkanov
	//rc.left += 1; [~] Sergey Shushkanov
	//rc.right -= 1; [~] Sergey Shushkanov
	ctrlPad.MoveWindow(rc);
	
}

LRESULT NotepadFrame::onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	HWND focus = GetFocus();
	bHandled = false;
	if (focus == ctrlPad.m_hWnd)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		tstring x;
		tstring::size_type start = (tstring::size_type) textUnderCursor(pt, ctrlPad, x);
		tstring::size_type end = x.find(_T(' '), start);
		
		if (end == string::npos)
			end = x.length();
			
		bHandled = WinUtil::openLink(x.substr(start, end - start));
	}
	return 0;
}

void NotepadFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

CFrameWndClassInfo& NotepadFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("NotepadFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::NOTEPAD, 0);

	return wc;
}
