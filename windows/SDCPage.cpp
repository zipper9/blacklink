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
#include "SDCPage.h"
#include "DialogLayout.h"
#include "KnownClients.h"
#include "../client/SettingsManager.h"

#ifdef OSVER_WIN_XP
#include "../client/SysVersion.h"
#endif

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 1,  DialogLayout::SIDE_RIGHT, U_DU(6)  };
static const DialogLayout::Align align2 = { 2,  DialogLayout::SIDE_RIGHT, U_DU(20) };
static const DialogLayout::Align align3 = { 3,  DialogLayout::SIDE_RIGHT, U_DU(6)  };
static const DialogLayout::Align align4 = { 4,  DialogLayout::SIDE_RIGHT, U_DU(4)  };
static const DialogLayout::Align align5 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6)  };
static const DialogLayout::Align align6 = { 9,  DialogLayout::SIDE_RIGHT, U_DU(4)  };
static const DialogLayout::Align align7 = { 16, DialogLayout::SIDE_RIGHT, U_DU(6)  };
static const DialogLayout::Align align8 = { -2, DialogLayout::SIDE_RIGHT, U_DU(6)  };
static const DialogLayout::Align align9 = { 21, DialogLayout::SIDE_RIGHT, U_DU(4)  };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_SHUTDOWN_ACTION,      FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_SHUTDOWN_ACTION,              0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_CAPTION_SHUTDOWN_TIMEOUT,     FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align2 },
	{ IDC_SHUTDOWN_TIMEOUT,             0,              UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_S2,                           FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align4 },
	{ IDC_SETTINGS_WRITE_BUFFER,        FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_SETTINGS_SOCKET_IN_BUFFER,    FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_SETTINGS_SOCKET_OUT_BUFFER,   FLAG_TRANSLATE, AUTO,   UNSPEC, 1          },
	{ IDC_BUFFERSIZE,                   0,              UNSPEC, UNSPEC, 0, &align5 },
	{ IDC_SOCKET_IN_BUFFER,             0,              UNSPEC, UNSPEC, 0, &align5 },
	{ IDC_SOCKET_OUT_BUFFER,            0,              UNSPEC, UNSPEC, 0, &align5 },
	{ IDC_SOCKET_OUT_BUFFER,            0,              UNSPEC, UNSPEC, 0, &align5 },
	{ IDC_SETTINGS_KB,                  FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align6 },
	{ IDC_B1,                           FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align6 },
	{ IDC_B2,                           FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align6 },
	{ IDC_CAPTION_MAX_COMPRESSION,      FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_MAX_COMPRESSION,              0,              UNSPEC, UNSPEC, 0, &align7 },
	{ IDC_CAPTION_DISABLE_COMP,         FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_CAPTION_MIN_MULTI_CHUNK_SIZE, FLAG_TRANSLATE, AUTO,   UNSPEC, 2          },
	{ IDC_CAPTION_DOWNCONN,             FLAG_TRANSLATE, AUTO,   UNSPEC, 2          },
	{ IDC_MIN_MULTI_CHUNK_SIZE,         0,              UNSPEC, UNSPEC, 0, &align8 },
	{ IDC_DOWNCONN,                     0,              UNSPEC, UNSPEC, 0, &align8 },
	{ IDC_SETTINGS_MB,                  FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align9 },
	{ IDC_CAPTION_HTTP_UA,              FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_ENABLE_ZLIB_COMP,             FLAG_TRANSLATE, AUTO,   UNSPEC             }
};

static const PropPage::Item items[] =
{
	{ IDC_BUFFERSIZE, SettingsManager::BUFFER_SIZE_FOR_DOWNLOADS, PropPage::T_INT },
	{ IDC_SOCKET_IN_BUFFER, SettingsManager::SOCKET_IN_BUFFER, PropPage::T_INT },
	{ IDC_SOCKET_OUT_BUFFER, SettingsManager::SOCKET_OUT_BUFFER, PropPage::T_INT },
	{ IDC_SHUTDOWN_TIMEOUT, SettingsManager::SHUTDOWN_TIMEOUT, PropPage::T_INT },
	{ IDC_ENABLE_ZLIB_COMP, SettingsManager::COMPRESS_TRANSFERS, PropPage::T_BOOL },
	{ IDC_MAX_COMPRESSION, SettingsManager::MAX_COMPRESSION, PropPage::T_INT },
	{ IDC_COMPRESSED_PATTERN, SettingsManager::COMPRESSED_FILES, PropPage::T_STR },
	{ IDC_DOWNCONN, SettingsManager::DOWNCONN_PER_SEC, PropPage::T_INT },
	{ IDC_MIN_MULTI_CHUNK_SIZE, SettingsManager::MIN_MULTI_CHUNK_SIZE, PropPage::T_INT },
	{ IDC_HTTP_USER_AGENT, SettingsManager::HTTP_USER_AGENT, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

void SDCPage::setRange(int idcEdit, int idcSpin, int minVal, int maxVal)
{
	CUpDownCtrl spin(GetDlgItem(idcSpin));
	spin.SetRange32(minVal, maxVal);
	spin.SetBuddy(GetDlgItem(idcEdit));
}

LRESULT SDCPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	setRange(IDC_SHUTDOWN_TIMEOUT, IDC_SHUTDOWN_SPIN, 1, 3600);
	setRange(IDC_BUFFERSIZE, IDC_BUFFER_SPIN, 0, 1024  * 1024);
	setRange(IDC_SOCKET_IN_BUFFER, IDC_READ_SPIN, 1024, 128 * 1024);
	setRange(IDC_SOCKET_OUT_BUFFER, IDC_WRITE_SPIN, 1024, 128 * 1024);
	setRange(IDC_MAX_COMPRESSION, IDC_MAX_COMP_SPIN, 0, 9);
	setRange(IDC_MIN_MULTI_CHUNK_SIZE, IDC_MIN_MULTI_CHUNK_SIZE_SPIN, 0, 100);
	setRange(IDC_DOWNCONN, IDC_DOWNCONN_SPIN, 0, 100);

	CComboBox ctrlHttpUserAgent(GetDlgItem(IDC_HTTP_USER_AGENT));
	for (size_t i = 0; KnownClients::userAgents[i]; ++i)
		ctrlHttpUserAgent.AddString(Text::toT(KnownClients::userAgents[i]).c_str());

	ctrlShutdownAction.Attach(GetDlgItem(IDC_SHUTDOWN_ACTION));
	ctrlShutdownAction.AddString(CTSTRING(POWER_OFF));
	ctrlShutdownAction.AddString(CTSTRING(LOG_OFF));
	ctrlShutdownAction.AddString(CTSTRING(REBOOT));
	ctrlShutdownAction.AddString(CTSTRING(SUSPEND));
	ctrlShutdownAction.AddString(CTSTRING(HIBERNATE));
	ctrlShutdownAction.AddString(CTSTRING(LOCK_COMPUTER));

	PropPage::read(*this, items);
	ctrlShutdownAction.SetCurSel(SETTING(SHUTDOWN_ACTION));

#ifdef OSVER_WIN_XP
	BOOL enable = !SysVersion::isOsVistaPlus();
#else
	BOOL enable = FALSE;
#endif
	GetDlgItem(IDC_SOCKET_IN_BUFFER).EnableWindow(enable);
	GetDlgItem(IDC_SOCKET_OUT_BUFFER).EnableWindow(enable);
	fixControls();

	return TRUE;
}

LRESULT SDCPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void SDCPage::fixControls()
{
	BOOL enable = IsDlgButtonChecked(IDC_ENABLE_ZLIB_COMP) == BST_CHECKED;
	GetDlgItem(IDC_MAX_COMPRESSION).EnableWindow(enable);
	GetDlgItem(IDC_MAX_COMP_SPIN).EnableWindow(enable);
	GetDlgItem(IDC_COMPRESSED_PATTERN).EnableWindow(enable);
}

void SDCPage::write()
{
	PropPage::write(*this, items);
	SET_SETTING(SHUTDOWN_ACTION, ctrlShutdownAction.GetCurSel());
}
