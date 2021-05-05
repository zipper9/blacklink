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

#include "../client/UserCommand.h"
#include "../client/NmdcHub.h"
#include "CommandDlg.h"
#include "WinUtil.h"

enum
{
	TYPE_SEPARATOR,
	TYPE_RAW,
	TYPE_CHAT,
	TYPE_PM
};

static const WinUtil::TextItem texts[] =
{
	{ IDOK, ResourceManager::OK },
	{ IDCANCEL, ResourceManager::CANCEL },
	{ IDC_SETTINGS_NAME, ResourceManager::USER_CMD_COMMAND_NAME },
	{ IDC_SETTINGS_NAME_HINT, ResourceManager::USER_CMD_COMMAND_HINT },
	{ IDC_SETTINGS_TYPE, ResourceManager::USER_CMD_TYPE },
	{ IDC_SETTINGS_TYPE, ResourceManager::USER_CMD_TYPE },
	{ IDC_SETTINGS_CONTEXT, ResourceManager::USER_CMD_CONTEXT },
	{ IDC_SETTINGS_HUB_MENU, ResourceManager::USER_CMD_CONTEXT_HUB },
	{ IDC_SETTINGS_USER_MENU, ResourceManager::USER_CMD_CONTEXT_USER },
	{ IDC_SETTINGS_SEARCH_MENU, ResourceManager::USER_CMD_CONTEXT_SEARCH },
	{ IDC_SETTINGS_FILELIST_MENU, ResourceManager::USER_CMD_CONTEXT_FILELIST },
	{ IDC_SETTINGS_HUB, ResourceManager::USER_CMD_HUB },
	{ IDC_SETTINGS_HUB_HINT, ResourceManager::USER_CMD_HUB_HINT },
	{ IDC_SETTINGS_PARAMETERS, ResourceManager::USER_CMD_PARAMETERS },
	{ IDC_SETTINGS_COMMAND, ResourceManager::USER_CMD_COMMAND_TEXT },
	{ IDC_SETTINGS_TO, ResourceManager::USER_CMD_TO },
	{ IDC_SETTINGS_ONCE, ResourceManager::USER_CMD_ONCE },
	{ IDC_USER_CMD_EXAMPLE, ResourceManager::USER_CMD_PREVIEW },
	{ IDC_USER_CMD_PARAM_HINT, ResourceManager::USER_CMD_PARAM_NOTE },
	{ 0, ResourceManager::Strings() }
};

#define ATTACH(id, var) var.Attach(GetDlgItem(id))

LRESULT CommandDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(newCommand ? CTSTRING(USER_CMD_ADD_TITLE) : CTSTRING(USER_CMD_EDIT_TITLE));
	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::COMMANDS, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	ctrlNote.setUseLinkTooltips(false);
	ctrlNote.setUseSystemColors(true);
	ctrlNote.SubclassWindow(GetDlgItem(IDC_USER_CMD_PARAM_HINT));
	WinUtil::translate(*this, texts);

	ATTACH(IDC_RESULT, ctrlResult);
	ATTACH(IDC_NAME, ctrlName);
	ATTACH(IDC_COMMAND_TYPE, ctrlType);
	ATTACH(IDC_HUB, ctrlHub);
	ATTACH(IDC_SETTINGS_HUB_MENU, ctrlContextHub);
	ATTACH(IDC_SETTINGS_USER_MENU, ctrlContextUser);
	ATTACH(IDC_SETTINGS_SEARCH_MENU, ctrlContextSearch);
	ATTACH(IDC_SETTINGS_FILELIST_MENU, ctrlContextFilelist);
	ATTACH(IDC_NICK, ctrlNick);
	ATTACH(IDC_COMMAND, ctrlCommand);
	ATTACH(IDC_SETTINGS_ONCE, ctrlOnce);

	int selType;
	ctrlType.AddString(CTSTRING(USER_CMD_SEPARATOR));
	ctrlType.AddString(CTSTRING(USER_CMD_RAW));
	ctrlType.AddString(CTSTRING(USER_CMD_CHAT));
	ctrlType.AddString(CTSTRING(USER_CMD_PM));

	if (type == UserCommand::TYPE_SEPARATOR)
	{
		selType = TYPE_SEPARATOR;
	}
	else
	{
		// More difficult, determine type by what it seems to be...
		if ((_tcsncmp(command.c_str(), _T("$To: "), 5) == 0) &&
		    (command.find(_T(" From: %[myNI] $<%[myNI]> ")) != string::npos ||
		    command.find(_T(" From: %[mynick] $<%[mynick]> ")) != string::npos) &&
		    command.find(_T('|')) == command.length() - 1) // if it has | anywhere but the end, it is raw
		{
			string::size_type i = command.find(_T(' '), 5);
			dcassert(i != string::npos);
			const tstring to = command.substr(5, i - 5);
			string::size_type pos = command.find(_T('>'), 5) + 2;
			const tstring cmd = Text::toT(NmdcHub::validateMessage(Text::fromT(command.substr(pos, command.length() - pos - 1)), true));
			selType = TYPE_PM;
			ctrlNick.SetWindowText(to.c_str());
			ctrlCommand.SetWindowText(cmd.c_str());
		}
		else if (((_tcsncmp(command.c_str(), _T("<%[mynick]> "), 12) == 0) ||
		          (_tcsncmp(command.c_str(), _T("<%[myNI]> "), 10) == 0)) &&
		           command[command.length() - 1] == '|')
		{
			// Looks like a chat thing...
			string::size_type pos = command.find(_T('>')) + 2;
			tstring cmd = Text::toT(NmdcHub::validateMessage(Text::fromT(command.substr(pos, command.length() - pos - 1)), true));
			selType = TYPE_CHAT;
			ctrlCommand.SetWindowText(cmd.c_str());
		}
		else
		{
			tstring cmd = command;
			selType = TYPE_RAW;
			ctrlCommand.SetWindowText(cmd.c_str());
		}
		if (type == UserCommand::TYPE_RAW_ONCE)
			ctrlOnce.SetCheck(BST_CHECKED);
	}

	ctrlHub.SetWindowText(hub.c_str());
	ctrlName.SetWindowText(name.c_str());
	ctrlType.SetCurSel(selType);

	if (ctx & UserCommand::CONTEXT_HUB)
		ctrlContextHub.SetCheck(BST_CHECKED);
	if (ctx & UserCommand::CONTEXT_USER)
		ctrlContextUser.SetCheck(BST_CHECKED);
	if (ctx & UserCommand::CONTEXT_SEARCH)
		ctrlContextSearch.SetCheck(BST_CHECKED);
	if (ctx & UserCommand::CONTEXT_FILELIST)
		ctrlContextFilelist.SetCheck(BST_CHECKED);

	type = selType;
	updateControls();
	updateCommand();
	ctrlResult.SetWindowText(command.c_str());

	CenterWindow(GetParent());
	return FALSE;
}

LRESULT CommandDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		updateContext();
		WinUtil::getWindowText(ctrlName, name);
		boost::trim(name);
		if (type && name.empty())
		{
			ctrlName.SetFocus();
			MessageBox(CTSTRING(USER_CMD_NAME_EMPTY), getAppNameVerT().c_str(), MB_OK | MB_ICONWARNING);
			return 0;
		}

		tstring text;
		WinUtil::getWindowText(ctrlCommand, text);
		if (type && text.empty())
		{
			ctrlCommand.SetFocus();
			MessageBox(CTSTRING(USER_CMD_TEXT_EMPTY), getAppNameVerT().c_str(), MB_OK | MB_ICONWARNING);
			return 0;
		}

		if (type == TYPE_PM)
		{
			WinUtil::getWindowText(ctrlNick, text);
			boost::trim(text);
			if (text.empty())
			{
				ctrlNick.SetFocus();
				MessageBox(CTSTRING(USER_CMD_NICK_EMPTY), getAppNameVerT().c_str(), MB_OK | MB_ICONWARNING);
				return 0;
			}
		}

		WinUtil::getWindowText(ctrlHub, hub);
		boost::trim(hub);

		if (type)
			type = (ctrlOnce.GetCheck() == BST_CHECKED) ? UserCommand::TYPE_RAW_ONCE : UserCommand::TYPE_RAW;
	}
	EndDialog(wID);
	return 0;
}

LRESULT CommandDlg::onChange(WORD, WORD, HWND, BOOL&)
{
	updateCommand();
	ctrlResult.SetWindowText(command.c_str());
	return 0;
}

LRESULT CommandDlg::onHub(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	updateHub();
	return 0;
}

LRESULT CommandDlg::onChangeType(WORD, WORD, HWND, BOOL&)
{
	type = ctrlType.GetCurSel();
	if (type == TYPE_PM)
	{
		tstring to;
		WinUtil::getWindowText(ctrlNick, to);
		boost::trim(to);
		if (to.empty())
			ctrlNick.SetWindowText(_T("%[userNI]"));
	}
	updateCommand();
	ctrlResult.SetWindowText(command.c_str());
	updateControls();
	return 0;
}

void CommandDlg::updateContext()
{
	ctx = 0;
	if (ctrlContextHub.GetCheck() & BST_CHECKED)
		ctx |= UserCommand::CONTEXT_HUB;
	if (ctrlContextUser.GetCheck() & BST_CHECKED)
		ctx |= UserCommand::CONTEXT_USER;
	if (ctrlContextSearch.GetCheck() & BST_CHECKED)
		ctx |= UserCommand::CONTEXT_SEARCH;
	if (ctrlContextFilelist.GetCheck() & BST_CHECKED)
		ctx |= UserCommand::CONTEXT_FILELIST;
}


void CommandDlg::updateCommand()
{
	if (type == TYPE_SEPARATOR)
	{
		command.clear();
	}
	else if (type == TYPE_RAW)
	{
		WinUtil::getWindowText(ctrlCommand, command);
	}
	else if (type == TYPE_CHAT)
	{
		WinUtil::getWindowText(ctrlCommand, command);
		command = Text::toT("<%[myNI]> " + NmdcHub::validateMessage(Text::fromT(command), false) + '|');
	}
	else if (type == TYPE_PM)
	{
		tstring to;
		WinUtil::getWindowText(ctrlNick, to);
		boost::trim(to);
		WinUtil::getWindowText(ctrlCommand, command);
		command = _T("$To: ") + to + _T(" From: %[myNI] $<%[myNI]> ") + Text::toT(NmdcHub::validateMessage(Text::fromT(command), false)) + _T('|');
	}
}

void CommandDlg::updateHub()
{
	WinUtil::getWindowText(ctrlHub, hub);
	updateCommand();
}

void CommandDlg::updateControls()
{
	switch (type)
	{
		case TYPE_SEPARATOR:
			ctrlName.EnableWindow(FALSE);
			ctrlCommand.EnableWindow(FALSE);
			ctrlNick.EnableWindow(FALSE);
			ctrlOnce.EnableWindow(FALSE);
			break;
		case TYPE_RAW:
		case TYPE_CHAT:
			ctrlName.EnableWindow(TRUE);
			ctrlCommand.EnableWindow(TRUE);
			ctrlNick.EnableWindow(FALSE);
			ctrlOnce.EnableWindow(TRUE);
			break;
		case TYPE_PM:
			ctrlName.EnableWindow(TRUE);
			ctrlCommand.EnableWindow(TRUE);
			ctrlNick.EnableWindow(TRUE);
			ctrlOnce.EnableWindow(TRUE);
			break;
	}
}

LRESULT CommandDlg::onTypeHint(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	MessageBox(CTSTRING(USER_CMD_TYPE_HINT), getAppNameVerT().c_str(), MB_OK | MB_ICONINFORMATION);
	return 0;
}

LRESULT CommandDlg::onLinkActivated(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	MessageBox(CTSTRING(USER_CMD_PARAM_HINT), getAppNameVerT().c_str(), MB_OK | MB_ICONINFORMATION);
	return 0;
}
