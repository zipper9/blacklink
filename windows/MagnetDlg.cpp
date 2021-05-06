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
#include "../client/CompatibilityManager.h"
#include <boost/algorithm/string/replace.hpp>

#ifdef OSVER_WIN_XP
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

LRESULT ClassicMagnetDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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
	GetDlgItem(IDC_MAGNET_SAVEAS).EnableWindow(FALSE);
	return 0;
}

LRESULT ClassicMagnetDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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

LRESULT ClassicMagnetDlg::onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (fileSize > 0 && !fileName.empty())
		GetDlgItem(IDC_MAGNET_REMEMBER).EnableWindow(TRUE);
	GetDlgItem(IDC_MAGNET_SAVEAS).EnableWindow(IsDlgButtonChecked(IDC_MAGNET_QUEUE) == BST_CHECKED || IsDlgButtonChecked(IDC_MAGNET_OPEN) == BST_CHECKED);
	return 0;
}

LRESULT ClassicMagnetDlg::onSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dst = fileName;
	if (!WinUtil::browseFile(dst, m_hWnd, true, Util::emptyStringT, NULL, Util::getFileExtWithoutDot(fileName).c_str())) return 0;
	SetDlgItemText(IDC_MAGNET_DISP_NAME, dst.c_str());
	fileName = std::move(dst);
	return 0;
}
#endif

class MagnetTaskDialog : public CTaskDialogImpl<MagnetTaskDialog>
{
	tstring fileName;
	wstring fileNameText;
	wstring fileSizeText;
	wstring fileTTHText;
	wstring details;
	bool fileSizeUnavailable = false;

public:
	bool allowRename = true;

	wstring getDetails() const
	{
		return fileNameText + L'\n' + fileSizeText + L'\n' + fileTTHText;
	}

	void setFileName(const tstring& fileName)
	{
		this->fileName = fileName;
		fileNameText = WSTRING(MAGNET_DLG_FILE) + L' ';
		if (fileName.empty())
		{
			fileNameText += WSTRING(NA);
			return;
		}
		if (allowRename)
		{
			tstring linkText = fileName;
			boost::replace_all(linkText, _T("<"), _T("&lt;"));
			boost::replace_all(linkText, _T(">"), _T("&gt;"));
			fileNameText += L"<a href=\"#\">" + linkText + L"</a>";
		}
		else
			fileNameText += fileName;
	}

	void setFileSize(int64_t fileSize, int64_t dirSize)
	{
		fileSizeText = WSTRING(MAGNET_DLG_SIZE) + L' ';
		if (fileSize > 0)
		{
			fileSizeText += Util::formatBytesW(fileSize);
			if (dirSize > 0)
				fileSizeText += L" / " + Util::formatBytesW(dirSize);
		}
		else
		{
			fileSizeText += WSTRING(NA);
			fileSizeUnavailable = true;
		}
	}

	void setFileTTH(const TTHValue& hash)
	{
		fileTTHText = WSTRING(MAGNET_DLG_HASH) + L' ';
		fileTTHText += Text::utf8ToWide(hash.toBase32());
	}

	void setDetails()
	{
		details = getDetails();
		SetExpandedInformationText(details.c_str());
	}

	void OnDialogConstructed()
	{
		if (fileName.empty() || fileSizeUnavailable)
		{
			EnableButton(IDC_MAGNET_OPEN, FALSE);
			EnableButton(IDC_MAGNET_QUEUE, FALSE);
		}
	}

	BOOL OnButtonClicked(int)
	{
		return TRUE;
	}

	void OnHyperlinkClicked(LPCWSTR)
	{
		tstring dst = fileName;
		if (!WinUtil::browseFile(dst, m_hWnd, true, Util::emptyStringT, NULL, Util::getFileExtWithoutDot(fileName).c_str())) return;
		setFileName(dst);
		details = getDetails();
		SetElementText(TDE_EXPANDED_INFORMATION, details.c_str());
	}

	const tstring& getFileName() const { return fileName; }
};

WinUtil::DefinedMagnetAction MagnetDlg::showDialog(HWND hWndParent, const TTHValue& hash, tstring& fileName, int64_t fileSize, int64_t dirSize, bool dclst)
{
#ifdef OSVER_WIN_XP
	if (!CompatibilityManager::isOsVistaPlus())
	{
		ClassicMagnetDlg dlg(hash, fileName, fileSize, dirSize, dclst);
		if (dlg.DoModal(hWndParent) != IDOK) return WinUtil::MA_ASK;
		fileName = dlg.getFileName();
		return dlg.getAction();
	}
#endif

	MagnetTaskDialog taskDlg;
	taskDlg.setFileName(fileName);
	taskDlg.setFileSize(fileSize, dirSize);
	taskDlg.setFileTTH(hash);
	taskDlg.setDetails();

	TASKDIALOG_BUTTON buttons[3];
	buttons[0].nButtonID = IDC_MAGNET_OPEN;
	buttons[1].nButtonID = IDC_MAGNET_QUEUE;
	buttons[2].nButtonID = IDC_MAGNET_SEARCH;
	if (dclst)
	{
		buttons[0].pszButtonText = CWSTRING(MAGNET_DLG_OPEN_DCLST);
		buttons[1].pszButtonText = CWSTRING(MAGNET_DLG_DOWNLOAD_DCLST);
	}
	else
	{
		buttons[0].pszButtonText = CWSTRING(MAGNET_DLG_OPEN_FILE);
		buttons[1].pszButtonText = CWSTRING(MAGNET_DLG_DOWNLOAD_FILE);
	}
	buttons[2].pszButtonText = CWSTRING(MAGNET_DLG_SEARCH);
	taskDlg.SetButtons(buttons, _countof(buttons));
	taskDlg.SetCommonButtons(TDCBF_CANCEL_BUTTON);
	taskDlg.SetWindowTitle(CWSTRING(MAGNET_DLG_BRIEF_TITLE));
	taskDlg.SetMainInstructionText(CWSTRING(MAGNET_DLG_TITLE));
	taskDlg.SetContentText(CWSTRING(MAGNET_DLG_TEXT_GOOD));
	taskDlg.SetMainIcon(IDR_MAGNET);
	taskDlg.SetVerificationText(CWSTRING(MAGNET_DLG_REMEMBER));
	taskDlg.ModifyFlags(0, TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS);
	BOOL flagChecked = FALSE;
	int id = 0;
	taskDlg.DoModal(hWndParent, &id, nullptr, &flagChecked);
	WinUtil::DefinedMagnetAction result = WinUtil::MA_ASK;
	int setting = -1;
	switch (id)
	{
		case IDC_MAGNET_OPEN:
			result = WinUtil::MA_OPEN;
			setting = SettingsManager::MAGNET_ACTION_DOWNLOAD_AND_OPEN;
			break;
		case IDC_MAGNET_QUEUE:
			result = WinUtil::MA_DOWNLOAD;
			setting = SettingsManager::MAGNET_ACTION_DOWNLOAD;
			break;
		case IDC_MAGNET_SEARCH:
			result = WinUtil::MA_SEARCH;
			setting = SettingsManager::MAGNET_ACTION_SEARCH;
	}
	if (flagChecked && setting != -1)
	{
		SettingsManager::set(dclst ? SettingsManager::DCLST_ASK : SettingsManager::MAGNET_ASK, FALSE);
		SettingsManager::set(dclst ? SettingsManager::DCLST_ACTION : SettingsManager::MAGNET_ACTION, setting);
	}
	fileName = taskDlg.getFileName();
	return result;
}
