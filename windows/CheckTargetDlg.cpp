/*
 * Copyright (C) 2010-2017 FlylinkDC++ Team http://flylinkdc.com
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
#include "WinUtil.h"
#include "CheckTargetDlg.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_REPLACE_DESCR,         ResourceManager::REPLACE_CAPTION       },
	{ IDC_REPLACE_BORDER_EXISTS, ResourceManager::REPLACE_BORDER_EXISTS },
	{ IDC_REPLACE_NAME_EXISTS,   ResourceManager::MAGNET_DLG_FILE       },
	{ IDC_REPLACE_SIZE_EXISTS,   ResourceManager::REPLACE_FILE_SIZE     },
	{ IDC_REPLACE_DATE_EXISTS,   ResourceManager::REPLACE_FILE_DATE     },
	{ IDC_REPLACE_BORDER_NEW,    ResourceManager::REPLACE_BORDER_NEW    },
	{ IDC_REPLACE_NAME_NEW,      ResourceManager::MAGNET_DLG_FILE       },
	{ IDC_REPLACE_SIZE_NEW,      ResourceManager::REPLACE_FILE_SIZE     },
	{ IDC_REPLACE_REPLACE,       ResourceManager::REPLACE_REPLACE       },
	{ IDC_REPLACE_SKIP,          ResourceManager::REPLACE_SKIP          },
	{ IDC_REPLACE_APPLY,         ResourceManager::REPLACE_APPLY         },
	{ IDC_REPLACE_CHANGE_NAME,   ResourceManager::MAGNET_DLG_SAVEAS     },
	{ IDOK,                      ResourceManager::OK                    },
	{ 0,                         ResourceManager::Strings()             }
};

LRESULT CheckTargetDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	newName = Text::toT(Util::getFileName(Util::getFilenameForRenaming(Text::fromT(fileName))));
	fileName = Util::getFileName(fileName);
	
	SetWindowText(CTSTRING(REPLACE_DLG_TITLE));
	WinUtil::translate(*this, texts);
	SetDlgItemText(IDC_REPLACE_RENAME, CTSTRING_F(REPLACE_RENAME_FMT, newName));
	
	CenterWindow(GetParent());
	
	SetDlgItemText(IDC_REPLACE_DISP_NAME_EXISTS, fileName.c_str());
	
	tstring strSize = Util::toStringT(sizeExisting) + _T(" (");
	strSize += Util::formatBytesT(sizeExisting);
	strSize += _T(')');
	
	SetDlgItemText(IDC_REPLACE_DISP_SIZE_EXISTS, strSize.c_str());
	SetDlgItemText(IDC_REPLACE_DISP_DATE_EXISTS, Text::toT(Util::formatDateTime(timeExisting)).c_str());
	
	SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, fileName.c_str());
	
	strSize = Util::toStringT(sizeNew) + _T(" (");
	strSize += Util::formatBytesT(sizeNew);
	strSize += _T(')');
	SetDlgItemText(IDC_REPLACE_DISP_SIZE_NEW, strSize.c_str());
	
	switch (option)
	{
		case SettingsManager::TE_ACTION_REPLACE:
		{
			CheckDlgButton(IDC_REPLACE_REPLACE, BST_CHECKED);
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, fileName.c_str());
		}
		break;
		case SettingsManager::TE_ACTION_SKIP:
		{
			CheckDlgButton(IDC_REPLACE_SKIP, BST_CHECKED);
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, _T(""));
		}
		break;
		case SettingsManager::TE_ACTION_RENAME:
		default:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, newName.c_str());
			CheckDlgButton(IDC_REPLACE_RENAME, BST_CHECKED);
		}
		break;
	}
	
	return 0;
}

LRESULT CheckTargetDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		if (IsDlgButtonChecked(IDC_REPLACE_REPLACE))
		{
			option = SettingsManager::TE_ACTION_REPLACE;
		}
		else if (IsDlgButtonChecked(IDC_REPLACE_RENAME))
		{
			option = SettingsManager::TE_ACTION_RENAME;
		}
		else if (IsDlgButtonChecked(IDC_REPLACE_SKIP))
		{
			option = SettingsManager::TE_ACTION_SKIP;
		}
		else
			option = SettingsManager::TE_ACTION_ASK;
			
		applyForAll = IsDlgButtonChecked(IDC_REPLACE_APPLY) == TRUE;
		EndDialog(wID);
	}
	return 0;
}

LRESULT CheckTargetDlg::onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_REPLACE_REPLACE:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, fileName.c_str());
		}
		break;
		case IDC_REPLACE_RENAME:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, newName.c_str());
		}
		break;
		case IDC_REPLACE_SKIP:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, _T(""));
		}
		break;
	};
	
	/*
	::EnableWindow(GetDlgItem(IDC_REPLACE_CHANGE_NAME), IsDlgButtonChecked(IDC_REPLACE_RENAME) == BST_CHECKED); // !SMT!-UI
	::EnableWindow(GetDlgItem(IDC_REPLACE_APPLY), IsDlgButtonChecked(IDC_REPLACE_RENAME) != BST_CHECKED);
	*/
	return 0;
}


#if 0 // disabled
LRESULT CheckTargetDlg::onChangeName(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dst = fileName;
	if (!WinUtil::browseFile(dst, m_hWnd, true)) return 0;
	fileName = dst;
	SetDlgItemText(IDC_MAGNET_DISP_NAME, dst.c_str());
	return 0;
}
#endif
