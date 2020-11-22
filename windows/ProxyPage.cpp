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

#include "Resource.h"
#include "ProxyPage.h"
#include "WinUtil.h"
#include "../client/Socket.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_SETTINGS_OUTGOING,         ResourceManager::SETTINGS_OUTGOING             },
	{ IDC_DIRECT_OUT,                ResourceManager::SETTINGS_DIRECT               },
	{ IDC_SOCKS5,                    ResourceManager::SETTINGS_SOCKS5               },
	{ IDC_SETTINGS_SOCKS5_IP,        ResourceManager::SETTINGS_SOCKS5_IP            },
	{ IDC_SETTINGS_SOCKS5_PORT,      ResourceManager::SETTINGS_SOCKS5_PORT          },
	{ IDC_SETTINGS_SOCKS5_USERNAME,  ResourceManager::SETTINGS_SOCKS5_USERNAME      },
	{ IDC_SETTINGS_SOCKS5_PASSWORD,  ResourceManager::PASSWORD                      },
	{ IDC_SOCKS_RESOLVE,             ResourceManager::SETTINGS_SOCKS5_RESOLVE       },
	{ IDC_CAPTION_PORT_TEST_URL,     ResourceManager::SETTINGS_PORT_TEST_SERVER_URL },
	{ IDC_CAPTION_DHT_BOOTSTRAP_URL, ResourceManager::SETTINGS_DHT_BOOTSTRAP        },
	{ 0,                             ResourceManager::Strings()                     }
	
};

static const PropPage::Item items[] =
{
	{ IDC_SOCKS_SERVER,      SettingsManager::SOCKS_SERVER,      PropPage::T_STR  },
	{ IDC_SOCKS_PORT,        SettingsManager::SOCKS_PORT,        PropPage::T_INT  },
	{ IDC_SOCKS_USER,        SettingsManager::SOCKS_USER,        PropPage::T_STR  },
	{ IDC_SOCKS_PASSWORD,    SettingsManager::SOCKS_PASSWORD,    PropPage::T_STR  },
	{ IDC_SOCKS_RESOLVE,     SettingsManager::SOCKS_RESOLVE,     PropPage::T_BOOL },
	{ IDC_PORT_TEST_URL,     SettingsManager::URL_PORT_TEST,     PropPage::T_STR  },
	{ IDC_DHT_BOOTSTRAP_URL, SettingsManager::URL_DHT_BOOTSTRAP, PropPage::T_STR  },
	{ 0,                     0,                                  PropPage::T_END  }
};

LRESULT ProxyPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	
	switch (SETTING(OUTGOING_CONNECTIONS))
	{
		case SettingsManager::OUTGOING_SOCKS5:
			CheckDlgButton(IDC_SOCKS5, BST_CHECKED);
			break;
		default:
			CheckDlgButton(IDC_DIRECT_OUT, BST_CHECKED);
			break;
	}
	
	PropPage::read(*this, items);
	
	fixControls();
	
	CEdit(GetDlgItem(IDC_SOCKS_SERVER)).LimitText(250);
	CEdit(GetDlgItem(IDC_SOCKS_PORT)).LimitText(5);
	CEdit(GetDlgItem(IDC_SOCKS_USER)).LimitText(250);
	CEdit(GetDlgItem(IDC_SOCKS_PASSWORD)).LimitText(250);
	CEdit(GetDlgItem(IDC_PORT_TEST_URL)).LimitText(280);
	CEdit(GetDlgItem(IDC_DHT_BOOTSTRAP_URL)).LimitText(280);
	
	return TRUE;
}

void ProxyPage::write()
{
	tstring x;
	WinUtil::getWindowText(GetDlgItem(IDC_SOCKS_SERVER), x);
	tstring::size_type i;
	
	while ((i = x.find(' ')) != string::npos)
		x.erase(i, 1);
	SetDlgItemText(IDC_SOCKS_SERVER, x.c_str());
	
	WinUtil::getWindowText(GetDlgItem(IDC_SERVER), x);
	
	while ((i = x.find(' ')) != string::npos)
		x.erase(i, 1);
		
	SetDlgItemText(IDC_SERVER, x.c_str());
	
	PropPage::write(*this, items);
	
	int ct = SettingsManager::OUTGOING_DIRECT;
	
	if (IsDlgButtonChecked(IDC_SOCKS5))
		ct = SettingsManager::OUTGOING_SOCKS5;
		
	if (SETTING(OUTGOING_CONNECTIONS) != ct)
	{
		g_settings->set(SettingsManager::OUTGOING_CONNECTIONS, ct);
		Socket::ProxyConfig proxy;
		if (Socket::getProxyConfig(proxy))
			Socket::socksUpdated(&proxy);
		else
			Socket::socksUpdated(nullptr);
	}
}

void ProxyPage::fixControls()
{
	BOOL socks = IsDlgButtonChecked(IDC_SOCKS5);
	::EnableWindow(GetDlgItem(IDC_SOCKS_SERVER), socks);
	::EnableWindow(GetDlgItem(IDC_SOCKS_PORT), socks);
	::EnableWindow(GetDlgItem(IDC_SOCKS_USER), socks);
	::EnableWindow(GetDlgItem(IDC_SOCKS_PASSWORD), socks);
	::EnableWindow(GetDlgItem(IDC_SOCKS_RESOLVE), socks);
}

LRESULT ProxyPage::onClickedDirect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}
