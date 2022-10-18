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
#include "../client/SettingsManager.h"

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
	{ IDC_EWMAGNET_TEMPL, SettingsManager::WMLINK_TEMPLATE, PropPage::T_STR},
	{ IDC_RATIOMSG, SettingsManager::RATIO_MESSAGE, PropPage::T_STR},
	{ IDC_THOLD, SettingsManager::USER_THRESHOLD, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::AUTO_FOLLOW, ResourceManager::SETTINGS_AUTO_FOLLOW },
	{ SettingsManager::STARTUP_BACKUP, ResourceManager::STARTUP_BACKUP },
	{ SettingsManager::AUTO_KICK, ResourceManager::SETTINGS_AUTO_KICK },
	{ SettingsManager::AUTO_KICK_NO_FAVS, ResourceManager::SETTINGS_AUTO_KICK_NO_FAVS },
	{ SettingsManager::AUTO_CHANGE_NICK, ResourceManager::SETTINGS_AUTO_CHANGE_NICK },
	{ SettingsManager::USE_MEMORY_MAPPED_FILES, ResourceManager::SETTINGS_USE_MM_FILES },
	{ SettingsManager::REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY, ResourceManager::REDUCE_PRIORITY_IF_MINIMIZED },
	{ SettingsManager::USE_MAGNETS_IN_PLAYERS_SPAM, ResourceManager::USE_MAGNETS_IN_PLAYERS_SPAM },
	{ SettingsManager::USE_BITRATE_FIX_FOR_SPAM, ResourceManager::USE_BITRATE_FIX_FOR_SPAM },
	{ 0, ResourceManager::Strings() }
};

LRESULT AdvancedPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
	
	curSel = SETTING(MEDIA_PLAYER);
	WMPlayerStr = Text::toT(SETTING(WMP_FORMAT));
	WinampStr = Text::toT(SETTING(WINAMP_FORMAT));
	iTunesStr = Text::toT(SETTING(ITUNES_FORMAT));
	MPCStr = Text::toT(SETTING(MPLAYERC_FORMAT));
	JAStr = Text::toT(SETTING(JETAUDIO_FORMAT));
	QCDQMPStr = Text::toT(SETTING(QCDQMP_FORMAT));
	
	ctrlList.Attach(GetDlgItem(IDC_ADVANCED_BOOLEANS));
	ctrlFormat.Attach(GetDlgItem(IDC_WINAMP));
	
	ctrlPlayer.Attach(GetDlgItem(IDC_PLAYER_COMBO));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_WINAMP));//  _T("Winamp (AIMP)"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_WMP)); //_T("Windows Media Player"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_ITUNES)); //_T("iTunes"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_WPC)); //_T("Media Player Classic"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_JA)); // _T("jetAudio Player"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_QCDQMP));
	ctrlPlayer.SetCurSel(curSel);
	
	switch (curSel)
	{
		case SettingsManager::WinAmp:
			SetDlgItemText(IDC_WINAMP, WinampStr.c_str());
			break;
		case SettingsManager::WinMediaPlayer:
			SetDlgItemText(IDC_WINAMP, WMPlayerStr.c_str());
			break;
		case SettingsManager::iTunes:
			iTunesStr = SetDlgItemText(IDC_WINAMP, iTunesStr.c_str());
			break;
		case SettingsManager::WinMediaPlayerClassic:
			SetDlgItemText(IDC_WINAMP, MPCStr.c_str());
			break;
		case SettingsManager::JetAudio:
			SetDlgItemText(IDC_WINAMP, JAStr.c_str());
			break;
		case SettingsManager::QCDQMP:
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
		case SettingsManager::WinAmp:
			WinampStr = buf;
			break;
		case SettingsManager::WinMediaPlayer:
			WMPlayerStr = buf;
			break;
		case SettingsManager::iTunes:
			iTunesStr = buf;
			break;
		case SettingsManager::WinMediaPlayerClassic:
			MPCStr = buf;
			break;
		case SettingsManager::JetAudio:
			JAStr = buf;
			break;
		case SettingsManager::QCDQMP:
			QCDQMPStr = buf;
			break;
	}
	
	SET_SETTING(MEDIA_PLAYER, ctrlPlayer.GetCurSel());
	SET_SETTING(WINAMP_FORMAT, Text::fromT(WinampStr).c_str());
	SET_SETTING(WMP_FORMAT, Text::fromT(WMPlayerStr).c_str());
	SET_SETTING(ITUNES_FORMAT, Text::fromT(iTunesStr).c_str());
	SET_SETTING(MPLAYERC_FORMAT, Text::fromT(MPCStr).c_str());
	SET_SETTING(JETAUDIO_FORMAT, Text::fromT(JAStr).c_str());
	SET_SETTING(QCDQMP_FORMAT, Text::fromT(QCDQMPStr).c_str());
}

LRESULT AdvancedPage::onClickedWinampHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	switch (curSel)
	{
		case SettingsManager::WinAmp:
			MessageBox(CTSTRING(WINAMP_HELP), CTSTRING(WINAMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::WinMediaPlayer:
			MessageBox(CTSTRING(WMP_HELP_STR), CTSTRING(WMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::iTunes:
			MessageBox(CTSTRING(ITUNES_HELP), CTSTRING(ITUNES_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::WinMediaPlayerClassic:
			MessageBox(CTSTRING(MPC_HELP), CTSTRING(MPC_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::JetAudio:
			MessageBox(CTSTRING(JA_HELP), CTSTRING(JA_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::QCDQMP:
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
		case SettingsManager::WinAmp:
			WinampStr = buf;
			break;
		case SettingsManager::WinMediaPlayer:
			WMPlayerStr = buf;
			break;
		case SettingsManager::iTunes:
			iTunesStr = buf;
			break;
		case SettingsManager::WinMediaPlayerClassic:
			MPCStr = buf;
			break;
		case SettingsManager::JetAudio:
			JAStr = buf;
			break;
		case SettingsManager::QCDQMP:
			QCDQMPStr = buf;
			break;
	}
	
	curSel = ctrlPlayer.GetCurSel();
	switch (curSel)
	{
		case SettingsManager::WinAmp:
			SetDlgItemText(IDC_WINAMP, WinampStr.c_str());
			break;
		case SettingsManager::WinMediaPlayer:
			SetDlgItemText(IDC_WINAMP, WMPlayerStr.c_str());
			break;
		case SettingsManager::iTunes:
			SetDlgItemText(IDC_WINAMP, iTunesStr.c_str());
			break;
		case SettingsManager::WinMediaPlayerClassic:
			SetDlgItemText(IDC_WINAMP, MPCStr.c_str());
			break;
		case SettingsManager::JetAudio:
			SetDlgItemText(IDC_WINAMP, JAStr.c_str());
			break;
		case SettingsManager::QCDQMP:
			SetDlgItemText(IDC_WINAMP, QCDQMPStr.c_str());
			break;
		default:
		{
			SetDlgItemText(IDC_WINAMP, CTSTRING(NO_MEDIA_SPAM));
		}
		break;
	}
	
	BOOL isPlayerSelected = (curSel >= SettingsManager::WinAmp && curSel < SettingsManager::PlayersCount) ? TRUE : FALSE;
	
	::EnableWindow(GetDlgItem(IDC_WINAMP), isPlayerSelected);
	::EnableWindow(GetDlgItem(IDC_WINAMP_HELP), isPlayerSelected);
	
	return 0;
}
