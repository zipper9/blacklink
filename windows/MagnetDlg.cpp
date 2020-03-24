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

#include "../client/ResourceManager.h"
#include "Resource.h"
#include "MagnetDlg.h"
#include "WinUtil.h"

LRESULT MagnetDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(MAGNET_DLG_TITLE));
	CenterWindow(GetParent());
	
	// fill in dialog bits
	SetDlgItemText(IDC_MAGNET_HASH, CTSTRING(MAGNET_DLG_HASH));
	SetDlgItemText(IDC_MAGNET_NAME, CTSTRING(MAGNET_DLG_FILE));
	SetDlgItemText(IDC_MAGNET_SIZE, CTSTRING(MAGNET_DLG_SIZE));
	if (isDCLST())
	{
		SetDlgItemText(IDC_MAGNET_QUEUE, CTSTRING(MAGNET_DLG_QUEUE_DCLST));
		// for Custom Themes Bitmap
		mImg.LoadFromResourcePNG(IDR_DCLST);
	}
	else
	{
		SetDlgItemText(IDC_MAGNET_QUEUE, CTSTRING(MAGNET_DLG_QUEUE));
		// for Custom Themes Bitmap
		mImg.LoadFromResourcePNG(IDR_MAGNET_PNG);
	}
	GetDlgItem(IDC_MAGNET_PIC).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)mImg));
	SetDlgItemText(IDC_MAGNET_SEARCH, CTSTRING(MAGNET_DLG_SEARCH));
	SetDlgItemText(IDC_MAGNET_NOTHING, CTSTRING(MAGNET_DLG_NOTHING));
	SetDlgItemText(IDC_MAGNET_REMEMBER, CTSTRING(MAGNET_DLG_REMEMBER));
	SetDlgItemText(IDC_MAGNET_OPEN, isDCLST() ? CTSTRING(MAGNET_DLG_OPEN_DCLST) : CTSTRING(MAGNET_DLG_OPEN_FILE));
	SetDlgItemText(IDC_MAGNET_SAVEAS, CTSTRING(MAGNET_DLG_SAVEAS));
	if (mSize <= 0 || mFileName.length() <= 0)
	{
		::ShowWindow(GetDlgItem(IDC_MAGNET_QUEUE), FALSE);
		::ShowWindow(GetDlgItem(IDC_MAGNET_REMEMBER), FALSE);
	}
	// Enable only for DCLST files
	//GetDlgItem(IDC_MAGNET_OPEN).EnableWindow(isDCLST());
	
	SetDlgItemText(IDC_MAGNET_TEXT, CTSTRING(MAGNET_DLG_TEXT_GOOD));
	
	// file details
	SetDlgItemText(IDC_MAGNET_DISP_HASH, Text::toT(mHash.toBase32()).c_str());
	
	// handling UTF8 input text
	{
		const string strFileName = Text::wideToAcp(mFileName);
		if (Text::validateUtf8(strFileName))
			mFileName = Text::toT(strFileName);
	}
	
	SetDlgItemText(IDC_MAGNET_DISP_NAME, mFileName.length() > 0 ? mFileName.c_str() : _T("N-A"));
	
	if (mSize <= 0)
	{
		SetDlgItemText(IDC_MAGNET_DISP_SIZE, _T("N-A"));
	}
	else
	{
		std::string sizeString = Util::formatBytes(mSize);      // toString(mSize)
		if (mdSize > 0)
			sizeString += " / " + Util::formatBytes(mdSize);
			
		SetDlgItemText(IDC_MAGNET_DISP_SIZE, mSize > 0 ? Text::toT(sizeString).c_str() : _T("N-A"));
	}
	//char buf[256];
	//_i64toa_s(mSize, buf, _countof(buf), 10);
	//search->minFileSize > 0 ? _i64toa(search->minFileSize, buf, 10) : ""
	
	// radio button
	CheckRadioButton(IDC_MAGNET_QUEUE, IDC_MAGNET_NOTHING, IDC_MAGNET_SEARCH);

	// focus
	CEdit focusThis;
	focusThis.Attach(GetDlgItem(IDC_MAGNET_SEARCH));
	focusThis.SetFocus();
	
	::EnableWindow(GetDlgItem(IDC_MAGNET_SAVEAS), IsDlgButtonChecked(IDC_MAGNET_QUEUE) == BST_CHECKED); // !SMT!-UI
	return 0;
}

LRESULT MagnetDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		// [!] TODO: PLEASE STOP COPY-PAST!
		if (IsDlgButtonChecked(IDC_MAGNET_REMEMBER) == BST_CHECKED)
		{
			isDCLST() ? SET_SETTING(DCLST_ASK, false) : SET_SETTING(MAGNET_ASK, false);
			if (IsDlgButtonChecked(IDC_MAGNET_QUEUE))
				isDCLST() ? SET_SETTING(DCLST_ACTION, SettingsManager::MAGNET_AUTO_DOWNLOAD) : SET_SETTING(MAGNET_ACTION, SettingsManager::MAGNET_AUTO_DOWNLOAD) ;
			else if (IsDlgButtonChecked(IDC_MAGNET_SEARCH))
				isDCLST() ? SET_SETTING(DCLST_ACTION, SettingsManager::MAGNET_AUTO_SEARCH) : SET_SETTING(MAGNET_ACTION, SettingsManager::MAGNET_AUTO_SEARCH) ;
			else if (IsDlgButtonChecked(IDC_MAGNET_OPEN))
				isDCLST() ? SET_SETTING(DCLST_ACTION, SettingsManager::MAGNET_AUTO_DOWNLOAD_AND_OPEN) : SET_SETTING(MAGNET_ACTION, SettingsManager::MAGNET_AUTO_DOWNLOAD_AND_OPEN) ;
		}
		if (IsDlgButtonChecked(IDC_MAGNET_SEARCH))
		{
			WinUtil::searchHash(mHash);
		}
		else if (IsDlgButtonChecked(IDC_MAGNET_QUEUE))
		{
			try
			{
				const string target = Text::fromT(mFileName);
				bool getConnFlag = true;
				QueueManager::getInstance()->add(target, mSize, mHash, HintedUser(),
				                                 isDCLST() ? QueueItem::FLAG_DCLST_LIST : 0,
				                                 true, getConnFlag);
			}
			catch (const Exception& e)
			{
				LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
			}
		}
		else if (IsDlgButtonChecked(IDC_MAGNET_OPEN))
		{
			try
			{
				bool getConnFlag = true;
				QueueManager::getInstance()->add(Text::fromT(mFileName), mSize, mHash, HintedUser(),
				                                 isDCLST() ? (QueueItem::FLAG_CLIENT_VIEW | QueueItem::FLAG_DCLST_LIST) : QueueItem::FLAG_OPEN_FILE,
				                                 true, getConnFlag);
			}
			catch (const Exception& e)
			{
				LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
			}
		}
	}
	EndDialog(wID);
	return 0;
}

LRESULT MagnetDlg::onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_MAGNET_OPEN:
		case IDC_MAGNET_QUEUE:
		case IDC_MAGNET_SEARCH:
			if (mSize > 0 && mFileName.length() > 0)
			{
				::EnableWindow(GetDlgItem(IDC_MAGNET_REMEMBER), true);
			}
			break;
		case IDC_MAGNET_NOTHING:
			if (IsDlgButtonChecked(IDC_MAGNET_REMEMBER) == BST_CHECKED)
			{
				::CheckDlgButton(m_hWnd, IDC_MAGNET_REMEMBER, FALSE);
			}
			::EnableWindow(GetDlgItem(IDC_MAGNET_REMEMBER), false);
			break;
	};
	::EnableWindow(GetDlgItem(IDC_MAGNET_SAVEAS), IsDlgButtonChecked(IDC_MAGNET_QUEUE) == BST_CHECKED); // !SMT!-UI
	return 0;
}

LRESULT MagnetDlg::onSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dst = mFileName;
	if (!WinUtil::browseFile(dst, m_hWnd, true, Util::emptyStringT, NULL, Util::getFileExtWithoutDot(mFileName).c_str())) return 0;
	mFileName = dst;
	SetDlgItemText(IDC_MAGNET_DISP_NAME, dst.c_str());
	return 0;
}
