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
#include "AdvancedPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/ConfCore.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 5, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_GROUP_TEMPLATES, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_MAGNET_URL_TEMPLATE, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CZDC_RATIOMSG, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CZDC_WINAMP, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_THOLD_STR, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_THOLD, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align1 }
};

static const PropPage::Item items[] =
{
	{ IDC_EWMAGNET_TEMPL, Conf::WMLINK_TEMPLATE, PropPage::T_STR},
	{ IDC_RATIOMSG, Conf::RATIO_MESSAGE, PropPage::T_STR},
	{ IDC_THOLD, Conf::USER_THRESHOLD, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::AUTO_FOLLOW, R_(SETTINGS_AUTO_FOLLOW) },
	{ Conf::STARTUP_BACKUP, R_(STARTUP_BACKUP) },
	{ Conf::AUTO_KICK, R_(SETTINGS_AUTO_KICK) },
	{ Conf::AUTO_KICK_NO_FAVS, R_(SETTINGS_AUTO_KICK_NO_FAVS) },
	{ Conf::AUTO_CHANGE_NICK, R_(SETTINGS_AUTO_CHANGE_NICK) },
	{ Conf::USE_MEMORY_MAPPED_FILES, R_(SETTINGS_USE_MM_FILES) },
	{ Conf::REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY, R_(REDUCE_PRIORITY_IF_MINIMIZED) },
	{ Conf::USE_MAGNETS_IN_PLAYERS_SPAM, R_(USE_MAGNETS_IN_PLAYERS_SPAM) },
	{ Conf::USE_BITRATE_FIX_FOR_SPAM, R_(USE_BITRATE_FIX_FOR_SPAM) },
	{ 0, R_INVALID }
};

LRESULT AdvancedPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));

	const auto* ss = SettingsManager::instance.getUiSettings();
	curSel = ss->getInt(Conf::MEDIA_PLAYER);
	WMPlayerStr = Text::toT(ss->getString(Conf::WMP_FORMAT));
	WinampStr = Text::toT(ss->getString(Conf::WINAMP_FORMAT));
	iTunesStr = Text::toT(ss->getString(Conf::ITUNES_FORMAT));
	MPCStr = Text::toT(ss->getString(Conf::MPLAYERC_FORMAT));
	JAStr = Text::toT(ss->getString(Conf::JETAUDIO_FORMAT));
	QCDQMPStr = Text::toT(ss->getString(Conf::QCDQMP_FORMAT));

	ctrlList.Attach(GetDlgItem(IDC_ADVANCED_BOOLEANS));
	ctrlFormat.Attach(GetDlgItem(IDC_WINAMP));

	static const ResourceManager::Strings playerStrings[] =
	{
		R_(MEDIA_MENU_WINAMP), R_(MEDIA_MENU_WMP), R_(MEDIA_MENU_ITUNES), R_(MEDIA_MENU_WPC),
		R_(MEDIA_MENU_JA), R_(MEDIA_MENU_QCDQMP), R_INVALID
	};
	ctrlPlayer.Attach(GetDlgItem(IDC_PLAYER_COMBO));
	WinUtil::fillComboBoxStrings(ctrlPlayer, playerStrings);
	ctrlPlayer.SetCurSel(curSel);

	switch (curSel)
	{
		case Conf::WinAmp:
			SetDlgItemText(IDC_WINAMP, WinampStr.c_str());
			break;
		case Conf::WinMediaPlayer:
			SetDlgItemText(IDC_WINAMP, WMPlayerStr.c_str());
			break;
		case Conf::iTunes:
			iTunesStr = SetDlgItemText(IDC_WINAMP, iTunesStr.c_str());
			break;
		case Conf::WinMediaPlayerClassic:
			SetDlgItemText(IDC_WINAMP, MPCStr.c_str());
			break;
		case Conf::JetAudio:
			SetDlgItemText(IDC_WINAMP, JAStr.c_str());
			break;
		case Conf::QCDQMP:
			SetDlgItemText(IDC_WINAMP, QCDQMPStr.c_str());
			break;
		default:
		{
			SetDlgItemText(IDC_WINAMP, CTSTRING(NO_MEDIA_SPAM));
			::EnableWindow(GetDlgItem(IDC_WINAMP), false);
			::EnableWindow(GetDlgItem(IDC_WINAMP_HELP), false);
		}
		break;
	}
	return TRUE;
}

void AdvancedPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));

	tstring buf;
	WinUtil::getWindowText(ctrlFormat, buf);

	switch (curSel)
	{
		case Conf::WinAmp:
			WinampStr = buf;
			break;
		case Conf::WinMediaPlayer:
			WMPlayerStr = buf;
			break;
		case Conf::iTunes:
			iTunesStr = buf;
			break;
		case Conf::WinMediaPlayerClassic:
			MPCStr = buf;
			break;
		case Conf::JetAudio:
			JAStr = buf;
			break;
		case Conf::QCDQMP:
			QCDQMPStr = buf;
			break;
	}
	
	auto ss = SettingsManager::instance.getUiSettings();
	ss->setInt(Conf::MEDIA_PLAYER, ctrlPlayer.GetCurSel());
	ss->setString(Conf::WINAMP_FORMAT, Text::fromT(WinampStr).c_str());
	ss->setString(Conf::WMP_FORMAT, Text::fromT(WMPlayerStr).c_str());
	ss->setString(Conf::ITUNES_FORMAT, Text::fromT(iTunesStr).c_str());
	ss->setString(Conf::MPLAYERC_FORMAT, Text::fromT(MPCStr).c_str());
	ss->setString(Conf::JETAUDIO_FORMAT, Text::fromT(JAStr).c_str());
	ss->setString(Conf::QCDQMP_FORMAT, Text::fromT(QCDQMPStr).c_str());
}

LRESULT AdvancedPage::onClickedWinampHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	switch (curSel)
	{
		case Conf::WinAmp:
			MessageBox(CTSTRING(WINAMP_HELP), CTSTRING(WINAMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case Conf::WinMediaPlayer:
			MessageBox(CTSTRING(WMP_HELP_STR), CTSTRING(WMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case Conf::iTunes:
			MessageBox(CTSTRING(ITUNES_HELP), CTSTRING(ITUNES_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case Conf::WinMediaPlayerClassic:
			MessageBox(CTSTRING(MPC_HELP), CTSTRING(MPC_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case Conf::JetAudio:
			MessageBox(CTSTRING(JA_HELP), CTSTRING(JA_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case Conf::QCDQMP:
			MessageBox(CTSTRING(QCDQMP_HELP), CTSTRING(QCDQMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
	}
	
	return S_OK;
}

LRESULT AdvancedPage::onClickedRatioMsgHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	MessageBox(CTSTRING(RATIO_MSG_HELP), CTSTRING(RATIO_MSG_HELP_DESC), MB_OK | MB_ICONINFORMATION);
	return S_OK;
}

LRESULT AdvancedPage::onSelChange(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	tstring buf;
	WinUtil::getWindowText(ctrlFormat, buf);
	
	switch (curSel)
	{
		case Conf::WinAmp:
			WinampStr = buf;
			break;
		case Conf::WinMediaPlayer:
			WMPlayerStr = buf;
			break;
		case Conf::iTunes:
			iTunesStr = buf;
			break;
		case Conf::WinMediaPlayerClassic:
			MPCStr = buf;
			break;
		case Conf::JetAudio:
			JAStr = buf;
			break;
		case Conf::QCDQMP:
			QCDQMPStr = buf;
			break;
	}

	curSel = ctrlPlayer.GetCurSel();
	switch (curSel)
	{
		case Conf::WinAmp:
			SetDlgItemText(IDC_WINAMP, WinampStr.c_str());
			break;
		case Conf::WinMediaPlayer:
			SetDlgItemText(IDC_WINAMP, WMPlayerStr.c_str());
			break;
		case Conf::iTunes:
			SetDlgItemText(IDC_WINAMP, iTunesStr.c_str());
			break;
		case Conf::WinMediaPlayerClassic:
			SetDlgItemText(IDC_WINAMP, MPCStr.c_str());
			break;
		case Conf::JetAudio:
			SetDlgItemText(IDC_WINAMP, JAStr.c_str());
			break;
		case Conf::QCDQMP:
			SetDlgItemText(IDC_WINAMP, QCDQMPStr.c_str());
			break;
		default:
			SetDlgItemText(IDC_WINAMP, CTSTRING(NO_MEDIA_SPAM));
			break;
	}

	BOOL isPlayerSelected = (curSel >= Conf::WinAmp && curSel < Conf::NumPlayers) ? TRUE : FALSE;

	::EnableWindow(GetDlgItem(IDC_WINAMP), isPlayerSelected);
	::EnableWindow(GetDlgItem(IDC_WINAMP_HELP), isPlayerSelected);
	
	return 0;
}
