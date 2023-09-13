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

#ifndef ABOUT_DLG_H
#define ABOUT_DLG_H

#include "SplashWindow.h"
#include "RichTextLabel.h"
#include "WinUtil.h"
#include "CompiledDateTime.h"

#if _MSC_VER >= 1921
#define MSC_RELEASE 2019
#elif _MSC_VER >= 1910
#define MSC_RELEASE 2017
#elif _MSC_VER >= 1900
#define MSC_RELEASE 2015
#elif _MSC_VER >= 1800
#define MSC_RELEASE 2013
#elif _MSC_VER >= 1700
#define MSC_RELEASE 2012
#elif _MSC_VER >= 1600
#define MSC_RELEASE 2010
#else
#define MSC_RELEASE 1970
#endif

class AboutDlg : public CDialogImpl<AboutDlg>
{
	public:
		enum { IDD = IDD_ABOUTBOX };
		
		BEGIN_MSG_MAP(AboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WMU_LINK_ACTIVATED, onLinkActivated)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
			RECT rc;
			GetClientRect(&rc);
			rc.left = (rc.right - rc.left - SplashWindow::WIDTH) / 2;
			rc.right = rc.left + SplashWindow::WIDTH;
			rc.top = 20;
			rc.bottom = rc.top + SplashWindow::HEIGHT;
			splash.setUseDialogBackground(true);
			splash.Create(m_hWnd, rc, nullptr, WS_CHILD | WS_VISIBLE);

			TCHAR compilerVersion[64];
			_sntprintf(compilerVersion, _countof(compilerVersion), _T("%d (%d)"), MSC_RELEASE, _MSC_FULL_VER);
			
			tstring str = TSTRING(COMPILED_ON);
			str += getCompileDate();
			str += _T(' ');
			str += getCompileTime(_T("%H:%M:%S"));
			str += _T(", Visual C++ ");
			str += compilerVersion;
			str += _T("<br>");
			str += TSTRING(ABOUT_SOURCE);

			infoLabel.setUseDialogBackground(true);
			infoLabel.setUseSystemColors(true);
			infoLabel.SubclassWindow(GetDlgItem(IDC_INFO_TEXT));
			infoLabel.setCenter(true);
			infoLabel.SetWindowText(str.c_str());

			return TRUE;
		}

		LRESULT onLinkActivated(UINT, WPARAM, LPARAM lParam, BOOL&)
		{
			auto text = reinterpret_cast<const TCHAR*>(lParam);
			WinUtil::openFile(text);
			return 0;
		}

	private:
		SplashWindow splash;
		RichTextLabel infoLabel;
};

#endif // !defined(ABOUT_DLG_H)
