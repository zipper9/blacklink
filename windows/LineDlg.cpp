/*
 *
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
#include "LineDlg.h"
#include "WinUtil.h"
#include "UserMessages.h"
#include "ConfUI.h"
#include "../client/ResourceManager.h"
#include "../client/SettingsManager.h"

#ifdef _UNICODE
static const TCHAR passwordChar = TCHAR(0x25CF);
#else
static const TCHAR passwordChar = '*';
#endif

LRESULT LineDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (icon != -1)
	{
		HICON dialogIcon = g_iconBitmaps.getIcon(icon, 0);
		SetIcon(dialogIcon, FALSE);
		SetIcon(dialogIcon, TRUE);
	}

	ctrlLine.Attach(GetDlgItem(IDC_LINE));
	ctrlLine.SetFocus();
	ctrlLine.SetWindowText(line.c_str());
	ctrlLine.SetSelAll(TRUE);
	if (limitText) ctrlLine.SetLimitText(limitText);
	SetDlgItemText(IDOK, CTSTRING(OK));
	SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));

	CButton ctrlCheckBox(GetDlgItem(IDC_SAVE_PASSWORD));
	if (password)
	{
		ctrlLine.SetWindowLongPtr(GWL_STYLE, ctrlLine.GetWindowLongPtr(GWL_STYLE) | ES_PASSWORD);
		ctrlLine.SetPasswordChar(passwordChar);
		if (checkBox)
		{
			ctrlCheckBox.ShowWindow(SW_SHOW);
			ctrlCheckBox.SetWindowText(CTSTRING(SAVE_PASSWORD));
		}
	}
	else if (checkBox)
	{
		ctrlCheckBox.ShowWindow(SW_SHOW);
		ctrlCheckBox.SetWindowTextW(CTSTRING_I(checkBoxText));
	}

	if (checkBox && checked)
		ctrlCheckBox.SetCheck(BST_CHECKED);

	ctrlDescription.Attach(GetDlgItem(IDC_DESCRIPTION));
	ctrlDescription.SetWindowText(description.c_str());

	SetWindowText(title.c_str());

	if (disabled)
	{
		CButton ctrlOK(GetDlgItem(IDOK));
		CButton ctrlCancel(GetDlgItem(IDCANCEL));
		RECT rc1, rc2;
		ctrlCancel.GetClientRect(&rc1);
		ctrlCancel.MapWindowPoints(m_hWnd, &rc1);
		ctrlOK.GetClientRect(&rc2);
		rc1.left = rc1.right - rc2.right;
		ctrlCancel.ShowWindow(SW_HIDE);
		ctrlOK.MoveWindow(&rc1);
		SetForegroundWindow(m_hWnd);
	}

	if (!allowEmpty && line.empty())
		GetDlgItem(IDOK).EnableWindow(FALSE);

	if (notifyMainFrame)
		::SendMessage(WinUtil::g_mainWnd, WMU_DIALOG_CREATED, 0, (LPARAM) m_hWnd);

	CenterWindow(GetParent());
	return FALSE;
}

LRESULT LineDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(ctrlLine, line);
		checked = IsDlgButtonChecked(IDC_SAVE_PASSWORD) == BST_CHECKED;
		if (validator)
		{
			tstring errorMsg;
			if (!validator(*this, errorMsg))
			{
				if (errorMsg.empty()) errorMsg = TSTRING(INVALID_INPUT);
				WinUtil::showInputError(ctrlLine, errorMsg);
				return 0;
			}
		}
	}
	EndDialog(wID);
	return 0;
}

LRESULT LineDlg::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!allowEmpty)
	{
		WinUtil::getWindowText(ctrlLine, line);
		GetDlgItem(IDOK).EnableWindow(!line.empty());
	}
	return 0;
}

tstring KickDlg::lastMsg;

LRESULT KickDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::FINGER, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	const auto ss = SettingsManager::instance.getUiSettings();
	const int count = _countof(recent);
	for (int i = 0; i < count; ++i)
		recent[i] = Text::toT(ss->getString(Conf::KICK_MSG_RECENT_01 + i));

	ctrlLine.Attach(GetDlgItem(IDC_LINE));
	ctrlLine.SetFocus();

	line.clear();
	for (int i = 0; i < count; i++)
		if (!recent[i].empty())
			ctrlLine.AddString(recent[i].c_str());
	ctrlLine.SetWindowText(lastMsg.c_str());

	ctrlDescription.Attach(GetDlgItem(IDC_DESCRIPTION));
	ctrlDescription.SetWindowText(description.c_str());

	SetWindowText(title.c_str());

	CenterWindow(GetParent());
	return FALSE;
}

LRESULT KickDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(ctrlLine, line);
		lastMsg = line;
		int i, j;

		const int count = _countof(recent);
		for (i = 0; i < count; i++)
			if (line == recent[i])
			{
				i++;
				break;
			}

		for (j = i - 1; j > 0; j--)
			recent[j] = recent[j - 1];

		recent[0] = line;

		auto ss = SettingsManager::instance.getUiSettings();
		for (i = 0; i < count; ++i)
			ss->setString(Conf::KICK_MSG_RECENT_01 + i, Text::fromT(recent[i]));
	}

	EndDialog(wID);
	return 0;
}
