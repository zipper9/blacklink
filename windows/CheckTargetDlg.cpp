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
#include "../client/CompatibilityManager.h"

#ifdef FLYLINKDC_SUPPORT_WIN_XP
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
	{ IDOK,                      ResourceManager::OK                    },
	{ 0,                         ResourceManager::Strings()             }
};

LRESULT ClassicCheckTargetDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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

LRESULT ClassicCheckTargetDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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

LRESULT ClassicCheckTargetDlg::onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
	::EnableWindow(GetDlgItem(IDC_REPLACE_APPLY), IsDlgButtonChecked(IDC_REPLACE_RENAME) != BST_CHECKED);
	*/
	return 0;
}
#endif

void CheckTargetDlg::showDialog(HWND hWndParent, const string& fullPath, int64_t sizeNew, int64_t sizeExisting, time_t timeExisting, int& option, bool& applyForAll)
{
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (!CompatibilityManager::isOsVistaPlus())
	{
		ClassicCheckTargetDlg dlg(fullPath, sizeNew, sizeExisting, timeExisting, option);
		dlg.DoModal(hWndParent);
		option = dlg.getOption();
		applyForAll = dlg.isApplyForAll();
		return;
	}
#endif

	tstring newName = Text::toT(Util::getFileName(Util::getFilenameForRenaming(fullPath)));
	tstring fileName = Text::toT(Util::getFileName(fullPath));
	CTaskDialog taskDlg;

	TASKDIALOG_BUTTON buttons[3];
	buttons[0].nButtonID = IDC_REPLACE_REPLACE;
	buttons[0].pszButtonText = CWSTRING(REPLACE_REPLACE);
	buttons[1].nButtonID = IDC_REPLACE_RENAME;
	wstring textRename = WSTRING(REPLACE_RENAME);
	textRename += L'\n';
	textRename += WSTRING_F(REPLACE_RENAME_NOTE, newName);
	buttons[1].pszButtonText = textRename.c_str();
	buttons[2].nButtonID = IDC_REPLACE_SKIP;
	wstring textSkip = WSTRING(REPLACE_SKIP);
	textSkip += L'\n';
	textSkip += WSTRING(REPLACE_SKIP_NOTE);
	buttons[2].pszButtonText = textSkip.c_str();

	wstring oldSize = Util::toStringW(sizeExisting) + L" (";
	oldSize += Util::formatBytesW(sizeExisting);
	oldSize += L')';

	wstring newSize = Util::toStringW(sizeNew) + L" (";
	newSize += Util::formatBytesW(sizeNew);
	newSize += L')';

	wstring oldDate = Text::utf8ToWide(Util::formatDateTime(timeExisting));

	wstring details = WSTRING_F(REPLACE_DETAILS, fileName % oldSize % oldDate % newSize);
	taskDlg.SetExpandedInformationText(details.c_str());

	taskDlg.SetButtons(buttons, _countof(buttons));
	taskDlg.SetWindowTitle(CWSTRING(REPLACE_DLG_TITLE));
	taskDlg.SetMainInstructionText(CWSTRING(REPLACE_DLG_TITLE));
	taskDlg.SetContentText(CWSTRING(REPLACE_CAPTION));
	taskDlg.SetMainIcon(g_iconBitmaps.getIcon(IconBitmaps::DOWNLOAD_QUEUE, 1));
	taskDlg.SetVerificationText(CWSTRING(REPLACE_APPLY));
	taskDlg.ModifyFlags(0, TDF_USE_COMMAND_LINKS | TDF_EXPANDED_BY_DEFAULT);

	BOOL flagChecked = FALSE;
	int id = 0;
	taskDlg.DoModal(hWndParent, &id, nullptr, &flagChecked);
	switch (id)
	{
		case IDC_REPLACE_REPLACE:
			option = SettingsManager::TE_ACTION_REPLACE;
			break;
		case IDC_REPLACE_RENAME:
			option = SettingsManager::TE_ACTION_RENAME;
			break;
		case IDC_REPLACE_SKIP:
			option = SettingsManager::TE_ACTION_SKIP;
			break;
		default:
			option = SettingsManager::TE_ACTION_ASK;
	}
	applyForAll = flagChecked;
}
