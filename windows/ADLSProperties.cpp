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
#include "ADLSProperties.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "../client/ADLSearch.h"
#include "../client/FavoriteManager.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 13, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 10, DialogLayout::SIDE_RIGHT, 0 };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_IS_ACTIVE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_SEARCH_STRING, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_MATCH_CASE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_REGEXP, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_FILE_TYPE, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_MIN_FILE_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_MAX_FILE_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_SIZE_TYPE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_ADLSP_DESTINATION, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_DEST_DIR, 0, UNSPEC, UNSPEC },
	{ IDC_AUTOQUEUE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_IS_FORBIDDEN, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_ADLSEARCH_ACTION, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_ADLSEARCH_RAW_ACTION, 0, UNSPEC, UNSPEC, 0, &align1, &align2 },
	{ IDOK, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDCANCEL, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

LRESULT ADLSProperties::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	SetWindowText(CTSTRING(ADLS_PROPERTIES));
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::ADL_SEARCH, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	auto commands = FavoriteManager::getInstance()->getUserCommands();
	userCommands.clear();

	ctrlUserCommand.Attach(GetDlgItem(IDC_ADLSEARCH_RAW_ACTION));
	ctrlUserCommand.AddString(_T("---"));
	int selIndex = 0;
	int index = 1;
	for (const UserCommand& uc : commands)
		if ((uc.isChat() || uc.isRaw()) && (uc.getCtx() & UserCommand::CONTEXT_FILELIST))
		{
			userCommands.push_back(uc);
			tstring name = Text::toT(uc.getName());
			ctrlUserCommand.AddString(name.c_str());
			if (search->userCommand == uc.getName()) selIndex = index;
			index++;
		}
	ctrlUserCommand.SetCurSel(selIndex);

	ctrlSearch.Attach(GetDlgItem(IDC_SEARCH_STRING));
	ctrlDestDir.Attach(GetDlgItem(IDC_DEST_DIR));
	ctrlMinSize.Attach(GetDlgItem(IDC_MIN_FILE_SIZE));
	ctrlMaxSize.Attach(GetDlgItem(IDC_MAX_FILE_SIZE));
	ctrlActive.Attach(GetDlgItem(IDC_IS_ACTIVE));
	ctrlAutoQueue.Attach(GetDlgItem(IDC_AUTOQUEUE));
	ctrlFlagFile.Attach(GetDlgItem(IDC_IS_FORBIDDEN));
	ctrlMatchCase.Attach(GetDlgItem(IDC_MATCH_CASE));
	ctrlRegEx.Attach(GetDlgItem(IDC_REGEXP));

	ctrlSearchType.Attach(GetDlgItem(IDC_FILE_TYPE));
	ctrlSearchType.AddString(CTSTRING(ADLS_FILE_NAME));
	ctrlSearchType.AddString(CTSTRING(ADLS_DIRECTORY_NAME));
	ctrlSearchType.AddString(CTSTRING(ADLS_FULL_PATH));
	ctrlSearchType.AddString(CTSTRING(TTH));

	ctrlSizeType.Attach(GetDlgItem(IDC_SIZE_TYPE));
	ctrlSizeType.AddString(CTSTRING(B));
	ctrlSizeType.AddString(CTSTRING(KB));
	ctrlSizeType.AddString(CTSTRING(MB));
	ctrlSizeType.AddString(CTSTRING(GB));

	ctrlSearch.SetWindowText(Text::toT(search->searchString).c_str());
	ctrlDestDir.SetWindowText(Text::toT(search->destDir).c_str());
	if (search->minFileSize >= 0)
		ctrlMinSize.SetWindowText(Util::toStringT(search->minFileSize).c_str());
	if (search->maxFileSize >= 0)
		ctrlMaxSize.SetWindowText(Util::toStringT(search->maxFileSize).c_str());
	ctrlActive.SetCheck(search->isActive ? BST_CHECKED : BST_UNCHECKED);
	ctrlSearchType.SetCurSel(search->sourceType);
	ctrlSizeType.SetCurSel(search->typeFileSize);
	ctrlMatchCase.SetCheck(search->isCaseSensitive ? BST_CHECKED : BST_UNCHECKED);
	ctrlRegEx.SetCheck(search->isRegEx ? BST_CHECKED : BST_UNCHECKED);
	ctrlAutoQueue.SetCheck(search->isAutoQueue ? BST_CHECKED : BST_UNCHECKED);
	ctrlFlagFile.SetCheck(search->isForbidden ? BST_CHECKED : BST_UNCHECKED);
	GetDlgItem(IDOK).EnableWindow(!search->searchString.empty());

	ctrlSearch.SetFocus();
	CenterWindow(GetParent());

	return 0;
}

LRESULT ADLSProperties::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		ADLSearch::SourceType sourceType = (ADLSearch::SourceType) ctrlSearchType.GetCurSel();
		bool isRegEx = ctrlRegEx.GetCheck() == BST_CHECKED;
		bool isCaseSensitive = ctrlMatchCase.GetCheck() == BST_CHECKED;
		tstring buf;

		WinUtil::getWindowText(ctrlSearch, buf);
		if (sourceType == ADLSearch::TTH)
		{
			if (!Util::isTigerHashString(buf))
			{
				WinUtil::showInputError(ctrlSearch, TSTRING(INVALID_TTH));
				return 0;
			}
			isRegEx = false;
			isCaseSensitive = false;
		}

		string searchString = Text::fromT(buf);
		if (isRegEx)
		{
			try
			{
				std::regex(searchString, isCaseSensitive ? std::regex::flag_type() : std::regex_constants::icase);
			}
			catch (...)
			{
				WinUtil::showInputError(ctrlSearch, TSTRING(INVALID_REG_EXP));
				return 0;
			}
		}

		WinUtil::getWindowText(ctrlDestDir, buf);
		search->destDir = Text::fromT(buf);

		WinUtil::getWindowText(ctrlMinSize, buf);
		search->minFileSize = buf.empty() ? -1 : Util::toInt64(buf);
		WinUtil::getWindowText(ctrlMaxSize, buf);
		search->maxFileSize = buf.empty() ? -1 : Util::toInt64(buf);

		search->isActive = ctrlActive.GetCheck() == BST_CHECKED;
		search->isCaseSensitive = isCaseSensitive;
		search->isRegEx = isRegEx;
		search->isAutoQueue = ctrlAutoQueue.GetCheck() == BST_CHECKED;
		search->isForbidden = ctrlFlagFile.GetCheck() == BST_CHECKED;

		search->sourceType = sourceType;
		search->typeFileSize = (ADLSearch::SizeType) ctrlSizeType.GetCurSel();
		search->searchString = std::move(searchString);
		int cmd = ctrlUserCommand.GetCurSel();
		if (cmd > 0 && cmd - 1 < (int) userCommands.size())
			search->userCommand = userCommands[cmd-1].getName();
		else
			search->userCommand.clear();
	}
	EndDialog(wID);
	return 0;
}

void ADLSProperties::checkTTH(const tstring& str)
{
	if (Util::isTigerHashString(str))
	{
		ctrlSearchType.SetCurSel(ADLSearch::TTH);
		autoSwitchToTTH = true;
	}
	else if (autoSwitchToTTH)
	{
		ctrlSearchType.SetCurSel(ADLSearch::OnlyFile);
		autoSwitchToTTH = false;
	}
}

LRESULT ADLSProperties::onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring str;
	WinUtil::getWindowText(ctrlSearch, str);
	checkTTH(str);
	GetDlgItem(IDOK).EnableWindow(!str.empty());
	return 0;
}
