#include "stdafx.h"
#include "RangesPage.h"
#include "WinUtil.h"

#include "../client/IpGuard.h"
#include "../client/IpTrust.h"
#include "../client/File.h"
#include "../client/CFlylinkDBManager.h"

static const WinUtil::TextItem texts1[] =
{
	{ IDC_ENABLE_IPGUARD,       ResourceManager::IPGUARD_ENABLE },
	{ IDC_INTRO_IPGUARD,        ResourceManager::IPGUARD_INTRO  },
	{ IDC_CAPTION_IPGUARD_MODE, ResourceManager::IPGUARD_MODE   },
	{ 0,                        ResourceManager::Strings()      }
};

static const WinUtil::TextItem texts2[] =
{
	{ IDC_ENABLE_IPTRUST, ResourceManager::SETTINGS_ENABLE_IPTRUST },
	{ 0,                  ResourceManager::Strings()               }
};

static const WinUtil::TextItem texts3[] =
{
	{ IDC_ENABLE_P2P_GUARD,         ResourceManager::P2P_GUARD_ENABLE             },
	{ IDC_P2P_GUARD_DESC,           ResourceManager::P2P_GUARD_DESC               },
	{ IDC_CAPTION_MANUAL_P2P_GUARD, ResourceManager::SETTINGS_MANUALLY_BLOCKED_IP },
	{ IDC_REMOVE,                   ResourceManager::REMOVE_SELECTED              },
	{ 0,                            ResourceManager::Strings()                    }
};

#define ADD_TAB(name, type, text) \
	tcItem.pszText = const_cast<TCHAR*>(CTSTRING(text)); \
	name.reset(new type); \
	tcItem.lParam = reinterpret_cast<LPARAM>(name.get()); \
	name->Create(m_hWnd, type::IDD); \
	ctrlTabs.InsertItem(n++, &tcItem);

LRESULT RangesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlTabs.Attach(GetDlgItem(IDC_TABS));
	TCITEM tcItem;
	tcItem.mask = TCIF_TEXT | TCIF_PARAM;
	tcItem.iImage = -1;

	int n = 0;
	ADD_TAB(pageIpGuard, RangesPageIPGuard, IPGUARD);
	ADD_TAB(pageIpTrust, RangesPageIPTrust, SETTINGS_IPTRUST);
	ADD_TAB(pageP2PGuard, RangesPageP2PGuard, P2P_GUARD);

	ctrlTabs.SetCurSel(0);
	changeTab();
	
	return TRUE;
}

void RangesPage::changeTab()
{
	int pos = ctrlTabs.GetCurSel();
	pageIpGuard->ShowWindow(SW_HIDE);
	pageIpTrust->ShowWindow(SW_HIDE);
	pageP2PGuard->ShowWindow(SW_HIDE);

	CRect rc;
	ctrlTabs.GetClientRect(&rc);
	ctrlTabs.AdjustRect(FALSE, &rc);
	ctrlTabs.MapWindowPoints(m_hWnd, &rc);
	
	switch (pos)
	{
		case 0:
			pageIpGuard->MoveWindow(&rc);
			pageIpGuard->ShowWindow(SW_SHOW);
			break;
		
		case 1:
			pageIpTrust->MoveWindow(&rc);
			pageIpTrust->ShowWindow(SW_SHOW);
			break;
	
		case 2:
			pageP2PGuard->MoveWindow(&rc);
			pageP2PGuard->ShowWindow(SW_SHOW);
			break;
	}
}

void RangesPage::write()
{
	pageIpGuard->write();
	pageIpTrust->write();
	pageP2PGuard->write();
}

LRESULT RangesPageIPGuard::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	WinUtil::translate(*this, texts1);

	ctrlMode.Attach(GetDlgItem(IDC_IPGUARD_MODE));
	ctrlMode.AddString(CTSTRING(IPGUARD_WHITE_LIST));
	ctrlMode.AddString(CTSTRING(IPGUARD_BLACK_LIST));
	ctrlMode.SetCurSel(BOOLSETTING(IPGUARD_DEFAULT_DENY) ? 0 : 1);

	ipGuardEnabled = BOOLSETTING(ENABLE_IPGUARD);
	CButton(GetDlgItem(IDC_ENABLE_IPGUARD)).SetCheck(ipGuardEnabled ? BST_CHECKED : BST_UNCHECKED);
	try
	{
		ipGuardPath = IpGuard::getFileName();
		ipGuardData = File(ipGuardPath, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_IPGUARD_DATA, Text::toT(ipGuardData).c_str());
	}
	catch (const FileException&)
	{
		SetDlgItemText(IDC_IPGUARD_DATA, _T(""));
	}
	fixControls();
	return TRUE;
}

void RangesPageIPGuard::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_ENABLE_IPGUARD) != BST_UNCHECKED;
	GetDlgItem(IDC_IPGUARD_DATA).EnableWindow(state);
	ctrlMode.EnableWindow(state);
}

void RangesPageIPGuard::write()
{
	bool enable = IsDlgButtonChecked(IDC_ENABLE_IPGUARD) != BST_UNCHECKED;
	g_settings->set(SettingsManager::ENABLE_IPGUARD, enable);
	g_settings->set(SettingsManager::IPGUARD_DEFAULT_DENY, !ctrlMode.GetCurSel());
	
	tstring ts;
	WinUtil::getWindowText(GetDlgItem(IDC_IPGUARD_DATA), ts);
	string strGuard = Text::fromT(ts);
	bool changed = false;
	if (strGuard != ipGuardData)
	{
		try
		{
			File fout(ipGuardPath, File::WRITE, File::CREATE | File::TRUNCATE);
			fout.write(strGuard);
			fout.close();
			ipGuardData = std::move(strGuard);
			changed = true;
		}
		catch (const FileException&)
		{
		}
	}
	if (enable)
	{
		if (changed || !ipGuardEnabled)
			ipGuard.load();
	}
	else
		ipGuard.clear();
}

LRESULT RangesPageIPTrust::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	WinUtil::translate(*this, texts2);

	ipTrustEnabled = BOOLSETTING(ENABLE_IPTRUST);
	CButton(GetDlgItem(IDC_ENABLE_IPTRUST)).SetCheck(ipTrustEnabled ? BST_CHECKED : BST_UNCHECKED);
	try
	{
		ipTrustPath = IpTrust::getFileName();
		ipTrustData = File(ipTrustPath, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_IPTRUST_DATA, Text::toT(ipTrustData).c_str());
	}
	catch (const FileException&)
	{
		SetDlgItemText(IDC_IPTRUST_DATA, _T(""));
	}
	fixControls();
	return TRUE;
}

void RangesPageIPTrust::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_ENABLE_IPTRUST) != BST_UNCHECKED;
	GetDlgItem(IDC_IPTRUST_DATA).EnableWindow(state);
}

void RangesPageIPTrust::write()
{
	bool enable = IsDlgButtonChecked(IDC_ENABLE_IPTRUST) != BST_UNCHECKED;
	g_settings->set(SettingsManager::ENABLE_IPTRUST, enable);

	tstring ts;
	WinUtil::getWindowText(GetDlgItem(IDC_IPTRUST_DATA), ts);
	string strTrust = Text::fromT(ts);
	bool changed = false;
	if (strTrust != ipTrustData)
	{
		try
		{
			File fout(ipTrustPath, File::WRITE, File::CREATE | File::TRUNCATE);
			fout.write(strTrust);
			fout.close();
			ipTrustData = std::move(strTrust);
			changed = true;
		}
		catch (const FileException&)
		{
		}
	}
	if (enable)
	{
		if (changed || !ipTrustEnabled)
			ipTrust.load();
	}
	else
		ipTrust.clear();
}

LRESULT RangesPageP2PGuard::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	WinUtil::translate(*this, texts3);

	checkBox.Attach(GetDlgItem(IDC_ENABLE_P2P_GUARD));
	checkBox.SetCheck(BOOLSETTING(ENABLE_P2P_GUARD) ? BST_CHECKED : BST_UNCHECKED);
	listBox.Attach(GetDlgItem(IDC_MANUAL_P2P_GUARD));
	loadBlocked();	
	fixControls();
	return TRUE;
}

void RangesPageP2PGuard::fixControls()
{
	BOOL state = checkBox.GetCheck() != BST_UNCHECKED;
	listBox.EnableWindow(state);
	BOOL unused;
	onSelChange(0, 0, nullptr, unused);
}

void RangesPageP2PGuard::loadBlocked()
{
	vector<P2PGuardBlockedIP> result;
	CFlylinkDBManager::getInstance()->loadManuallyBlockedIPs(result);
	for (const auto& v : result)
		listBox.AddString(Text::toT(boost::asio::ip::make_address_v4(v.ip).to_string() + '\t' + v.note).c_str());
	BOOL unused;
	onSelChange(0, 0, nullptr, unused);
}

LRESULT RangesPageP2PGuard::onSelChange(WORD, WORD, HWND, BOOL&)
{
	BOOL enable = checkBox.GetCheck() == BST_CHECKED;
	if (!enable)
	{
		int n = listBox.GetSelCount();
		if (n)
		{
			std::vector<int> aiSel;
			aiSel.resize(n);
			listBox.GetSelItems(n, aiSel.data());
			for (int i = 0; i < n; ++i)
				listBox.SetSel(aiSel[i], FALSE);
		}
	}
	CButton(GetDlgItem(IDC_REMOVE)).EnableWindow(enable && listBox.GetSelCount() != 0);
	return 0;
}

LRESULT RangesPageP2PGuard::onRemoveBlocked(WORD, WORD, HWND, BOOL&)
{
	int n = listBox.GetSelCount();
	if (n)
	{
		std::vector<int> aiSel;
		aiSel.resize(n);
		listBox.GetSelItems(n, aiSel.data());
		for (int i = 0; i < n; ++i)
		{
			int nLen = listBox.GetTextLen(aiSel[i]);
			if (nLen > 0)
			{
				tstring str;
				str.resize(nLen + 2);
				listBox.GetText(aiSel[i], &str[0]);
				auto pos = str.find(_T('\t'));
				if (pos != tstring::npos)
				{
					str.erase(pos);
					boost::system::error_code ec;
					auto ip = boost::asio::ip::address_v4::from_string(Text::fromT(str), ec);
					if (!ec)
						CFlylinkDBManager::getInstance()->removeManuallyBlockedIP(ip.to_uint());
				}
			}
		}
	}
	listBox.ResetContent();
	loadBlocked();
	return 0;
}

void RangesPageP2PGuard::write()
{
	g_settings->set(SettingsManager::ENABLE_P2P_GUARD, checkBox.GetCheck() != BST_UNCHECKED);
}
