/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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

#include "MagnetDlg.h"
#include "../client/ResourceManager.h"
#include "../client/SettingsManager.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_MAGNET_HASH,     ResourceManager::MAGNET_DLG_HASH      },
	{ IDC_MAGNET_NAME,     ResourceManager::MAGNET_DLG_FILE      },
	{ IDC_MAGNET_SIZE,     ResourceManager::MAGNET_DLG_SIZE      },
	{ IDC_MAGNET_SEARCH,   ResourceManager::MAGNET_DLG_SEARCH    },
	{ IDC_MAGNET_REMEMBER, ResourceManager::MAGNET_DLG_REMEMBER  },
	{ IDC_MAGNET_SAVEAS,   ResourceManager::MAGNET_DLG_SAVEAS    },
	{ IDC_MAGNET_TEXT,     ResourceManager::MAGNET_DLG_TEXT_GOOD },
	{ IDOK,                ResourceManager::OK                   },
	{ IDCANCEL,            ResourceManager::CANCEL               },
	{ 0,                   ResourceManager::Strings()            }
};

LRESULT MagnetDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(MAGNET_DLG_TITLE));
	CenterWindow(GetParent());

	WinUtil::translate(*this, texts);
	if (dclst)
	{
		SetDlgItemText(IDC_MAGNET_QUEUE, CTSTRING(MAGNET_DLG_DOWNLOAD_DCLST));
		SetDlgItemText(IDC_MAGNET_OPEN, CTSTRING(MAGNET_DLG_OPEN_DCLST));
		image.LoadFromResourcePNG(IDR_DCLST);
	}
	else
	{
		SetDlgItemText(IDC_MAGNET_QUEUE, CTSTRING(MAGNET_DLG_DOWNLOAD_FILE));
		SetDlgItemText(IDC_MAGNET_OPEN, CTSTRING(MAGNET_DLG_OPEN_FILE));
		image.LoadFromResourcePNG(IDR_MAGNET_PNG);
	}
	GetDlgItem(IDC_MAGNET_PIC).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(HBITMAP) image);
	if (fileSize <= 0 || fileName.empty())
	{
		GetDlgItem(IDC_MAGNET_QUEUE).EnableWindow(FALSE);
		GetDlgItem(IDC_MAGNET_OPEN).EnableWindow(FALSE);
		GetDlgItem(IDC_MAGNET_REMEMBER).EnableWindow(FALSE);
	}

	SetDlgItemText(IDC_MAGNET_DISP_HASH, Text::toT(hash.toBase32()).c_str());	
	SetDlgItemText(IDC_MAGNET_DISP_NAME, fileName.empty() ? CTSTRING(NA) : fileName.c_str());

	if (fileSize <= 0)
	{
		SetDlgItemText(IDC_MAGNET_DISP_SIZE, CTSTRING(NA));
	}
	else
	{
		tstring sizeString = Util::formatBytesT(fileSize);
		if (dirSize > 0)
			sizeString += _T(" / ") + Util::formatBytesT(dirSize);

		SetDlgItemText(IDC_MAGNET_DISP_SIZE, sizeString.c_str());
	}

	CheckRadioButton(IDC_MAGNET_QUEUE, IDC_MAGNET_OPEN, IDC_MAGNET_SEARCH);

	GetDlgItem(IDC_MAGNET_SEARCH).SetFocus();
	CButton renameButton(GetDlgItem(IDC_MAGNET_SAVEAS));
	if (dclst)
		renameButton.ShowWindow(SW_HIDE);
	else
		renameButton.EnableWindow(IsDlgButtonChecked(IDC_MAGNET_QUEUE) == BST_CHECKED);
	return 0;
}

LRESULT MagnetDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		if (IsDlgButtonChecked(IDC_MAGNET_REMEMBER) == BST_CHECKED)
		{
			int action = -1;
			if (IsDlgButtonChecked(IDC_MAGNET_QUEUE))
				action = SettingsManager::MAGNET_ACTION_DOWNLOAD;
			else if (IsDlgButtonChecked(IDC_MAGNET_SEARCH))
				action = SettingsManager::MAGNET_ACTION_SEARCH;
			else if (IsDlgButtonChecked(IDC_MAGNET_OPEN))
				action = SettingsManager::MAGNET_ACTION_DOWNLOAD_AND_OPEN;
			if (action != -1)
			{
				SettingsManager::set(dclst ? SettingsManager::DCLST_ASK : SettingsManager::MAGNET_ASK, FALSE);
				SettingsManager::set(dclst ? SettingsManager::DCLST_ACTION : SettingsManager::MAGNET_ACTION, action);
			}
		}
		if (IsDlgButtonChecked(IDC_MAGNET_SEARCH))
		{
			action = WinUtil::MA_SEARCH;
		}
		else if (IsDlgButtonChecked(IDC_MAGNET_QUEUE))
		{
			if (fileSize > 0) action = WinUtil::MA_DOWNLOAD;
		}
		else if (IsDlgButtonChecked(IDC_MAGNET_OPEN))
		{
			if (fileSize > 0) action = WinUtil::MA_OPEN;
		}
	}
	EndDialog(wID);
	return 0;
}

LRESULT MagnetDlg::onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (fileSize > 0 && !fileName.empty())
		GetDlgItem(IDC_MAGNET_REMEMBER).EnableWindow(TRUE);
	GetDlgItem(IDC_MAGNET_SAVEAS).EnableWindow(IsDlgButtonChecked(IDC_MAGNET_QUEUE) == BST_CHECKED);
	return 0;
}

LRESULT MagnetDlg::onSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dst = fileName;
	if (!WinUtil::browseFile(dst, m_hWnd, true, Util::emptyStringT, NULL, Util::getFileExtWithoutDot(fileName).c_str())) return 0;
	SetDlgItemText(IDC_MAGNET_DISP_NAME, dst.c_str());
	fileName = std::move(dst);
	return 0;
}
