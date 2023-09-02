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
#include "../client/SettingsManager.h"
#include "../client/CryptoManager.h"
#include "CertificatesPage.h"
#include "WinUtil.h"
#include "BrowseFile.h"

int g_tlsOption = 0;

static const WinUtil::TextItem texts[] =
{
	{ IDC_STATIC1, ResourceManager::PRIVATE_KEY_FILE },
	{ IDC_STATIC2, ResourceManager::OWN_CERTIFICATE_FILE },
	{ IDC_STATIC3, ResourceManager::TRUSTED_CERTIFICATES_PATH },
	{ IDC_GENERATE_CERTS, ResourceManager::GENERATE_CERTIFICATES },
	{ IDC_SECURITY_GROUP, ResourceManager::SETTINGS_CERTIFICATES },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_TLS_CERTIFICATE_FILE, SettingsManager::TLS_CERTIFICATE_FILE, PropPage::T_STR },
	{ IDC_TLS_PRIVATE_KEY_FILE, SettingsManager::TLS_PRIVATE_KEY_FILE, PropPage::T_STR },
	{ IDC_TLS_TRUSTED_CERTIFICATES_PATH, SettingsManager::TLS_TRUSTED_CERTIFICATES_PATH, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::USE_TLS, ResourceManager::SETTINGS_USE_TLS },
	{ SettingsManager::ALLOW_UNTRUSTED_HUBS, ResourceManager::SETTINGS_ALLOW_UNTRUSTED_HUBS },
	{ SettingsManager::ALLOW_UNTRUSTED_CLIENTS, ResourceManager::SETTINGS_ALLOW_UNTRUSTED_CLIENTS },
	{ 0, ResourceManager::Strings() }
};

#if 0
static const PropPage::ListItem securityItems[] =
{
	{ SettingsManager::CONFIRM_SHARE_FROM_SHELL, ResourceManager::SECURITY_ASK_ON_SHARE_FROM_SHELL },
	{ 0, ResourceManager::Strings() }
};
#endif

LRESULT CertificatesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlList.Attach(GetDlgItem(IDC_TLS_OPTIONS));
	WinUtil::translate(*this, texts);
	PropPage::read(*this, items, listItems, ctrlList);
#if 0
	PropPage::read(*this, nullptr, securityItems, GetDlgItem(IDC_SECURITY_LIST));
#endif
	return TRUE;
}

void CertificatesPage::write()
{
	g_tlsOption = 0;
	PropPage::write(*this, items, listItems, ctrlList);
#if 0
	PropPage::write(*this, nullptr, securityItems, GetDlgItem(IDC_SECURITY_LIST));
#endif
}

void CertificatesPage::cancel()
{
	g_tlsOption = 0;
}

LRESULT CertificatesPage::onBrowsePrivateKey(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target = Text::toT(SETTING(TLS_PRIVATE_KEY_FILE));
	CEdit edt(GetDlgItem(IDC_TLS_PRIVATE_KEY_FILE));
	
	if (WinUtil::browseFile(target, m_hWnd, false, target))
		edt.SetWindowText(target.c_str());
	return 0;
}

LRESULT CertificatesPage::onBrowseCertificate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target = Text::toT(SETTING(TLS_CERTIFICATE_FILE));
	CEdit edt(GetDlgItem(IDC_TLS_CERTIFICATE_FILE));
	
	if (WinUtil::browseFile(target, m_hWnd, false, target))
		edt.SetWindowText(target.c_str());
	return 0;
}

LRESULT CertificatesPage::onBrowseTrustedPath(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target = Text::toT(SETTING(TLS_TRUSTED_CERTIFICATES_PATH));
	CEdit edt(GetDlgItem(IDC_TLS_TRUSTED_CERTIFICATES_PATH));
	
	if (WinUtil::browseDirectory(target, m_hWnd))
		edt.SetWindowText(target.c_str());
	return 0;
}

LRESULT CertificatesPage::onGenerateCerts(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	try
	{
		CryptoManager::getInstance()->generateNewKeyPair();
	}
	catch (const CryptoException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), CTSTRING(ERROR_GENERATING_CERTIFICATE), MB_OK | MB_ICONERROR);
		return 0;
	}
	return 0;
}

void CertificatesPage::onHide()
{
	g_tlsOption = getBoolSetting(listItems, ctrlList, SettingsManager::USE_TLS) ? 1 : -1;
}
