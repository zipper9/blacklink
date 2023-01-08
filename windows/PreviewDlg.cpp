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

#include "stdafx.h"

#include "Resource.h"
#include "PreviewDlg.h"
#include "WinUtil.h"
#include "BrowseFile.h"
#include <boost/algorithm/string.hpp>

static const WinUtil::TextItem texts[] =
{
	{ IDC_PREV_NAME,        ResourceManager::SETTINGS_NAME                    },
	{ IDC_PREV_APPLICATION, ResourceManager::SETTINGS_PREVIEW_DLG_APPLICATION },
	{ IDC_PREV_ARG,         ResourceManager::SETTINGS_PREVIEW_DLG_ARGUMENTS   },
	{ IDC_PREV_ARG2,        ResourceManager::SETTINGS_PREVIEW_DLG_ARG_INFO    },
	{ IDC_PREV_EXT,         ResourceManager::SETTINGS_EXTENSION_LIST          },
	{ IDC_PREV_EXT2,        ResourceManager::SETTINGS_EXT_LIST_EXAMPLE        },
	{ IDOK,                 ResourceManager::OK                               },
	{ IDCANCEL,             ResourceManager::CANCEL                           },
	{ 0,                    ResourceManager::Strings()                        }
};

LRESULT PreviewDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(newItem ? CTSTRING(SETTINGS_ADD_PREVIEW_APP) : CTSTRING(SETTINGS_EDIT_PREVIEW_APP));
	WinUtil::translate(*this, texts);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::PREVIEW, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	ctrlName.Attach(GetDlgItem(IDC_PREVIEW_NAME));
	ctrlApplication.Attach(GetDlgItem(IDC_PREVIEW_APPLICATION));
	ctrlArguments.Attach(GetDlgItem(IDC_PREVIEW_ARGUMENTS));
	ctrlExtensions.Attach(GetDlgItem(IDC_PREVIEW_EXTENSION));
	
	ctrlName.SetWindowText(name.c_str());
	ctrlApplication.SetWindowText(application.c_str());
	ctrlArguments.SetWindowText(arguments.c_str());
	ctrlExtensions.SetWindowText(extensions.c_str());
	
	CenterWindow(GetParent());
	return 0;
}

LRESULT PreviewDlg::onBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
{
	tstring x;
	WinUtil::getWindowText(ctrlApplication, x);

	static const WinUtil::FileMaskItem types[] =
	{
		{ ResourceManager::FILEMASK_APPLICATION, _T("*.exe") },
		{ ResourceManager::Strings(),            nullptr     }
	};
	if (WinUtil::browseFile(x, m_hWnd, false, Util::emptyStringT, WinUtil::getFileMaskString(types).c_str()) == IDOK)
		ctrlApplication.SetWindowText(x.c_str());

	return 0;
}

LRESULT PreviewDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(ctrlName, name);
		WinUtil::getWindowText(ctrlApplication, application);
		boost::trim(name);
		boost::trim(application);
		if (name.empty() || application.empty())
		{
			MessageBox(CTSTRING(NAME_COMMAND_EMPTY), getAppNameVerT().c_str(), MB_ICONWARNING|MB_OK);
			return 0;
		}
		WinUtil::getWindowText(ctrlArguments, arguments);
		WinUtil::getWindowText(ctrlExtensions, extensions);
		boost::replace_all(extensions, _T(" "), _T(""));
		boost::replace_all(extensions, _T(","), _T(";"));
	}
	EndDialog(wID);
	return 0;
}

LRESULT PreviewDlg::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring s;
	WinUtil::getWindowText(ctrlName, s);
	boost::trim(s);
	if (s.empty())
	{
		GetDlgItem(IDOK).EnableWindow(FALSE);
		return 0;
	}
	WinUtil::getWindowText(ctrlApplication, s);
	boost::trim(s);
	GetDlgItem(IDOK).EnableWindow(!s.empty());
	return 0;
}
