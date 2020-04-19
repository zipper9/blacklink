#include "stdafx.h"
#include "Resource.h"
#include "RangesPage.h"

#include "../client/IpGuard.h"
#include "../client/IpTrust.h"
#include "../client/File.h"
#include "../client/CFlylinkDBManager.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_DEFAULT_POLICY_STR, ResourceManager::IPGUARD_DEFAULT_POLICY },
	{ IDC_DEFAULT_POLICY_EXCEPT_STR, ResourceManager::EXCEPT_SPECIFIED },
	{ IDC_ENABLE_IPGUARD, ResourceManager::IPGUARD_ENABLE },
	{ IDC_ENABLE_P2P_GUARD, ResourceManager::P2P_GUARD_ENABLE },
	{ IDC_ENABLE_P2P_GUARD_STR, ResourceManager::P2P_GUARD_ENABLE_STR },
	{ IDC_INTRO_IPGUARD, ResourceManager::IPGUARD_INTRO },
	{ IDC_FLYLINK_TRUST_IP_BOX, ResourceManager::SETTINGS_IPBLOCK },
	{ IDC_FLYLINK_TRUST_IP_URL_STR, ResourceManager::SETTINGS_IPBLOCK_DOWNLOAD_URL_STR },
	{ IDC_FLYLINK_MANUAL_P2P_GUARD_IP_LIST_REMOVE_BUTTON, ResourceManager::REMOVE2 },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_ENABLE_IPTRUST, SettingsManager::ENABLE_IPTRUST, PropPage::T_BOOL },
	{ IDC_ENABLE_IPGUARD, SettingsManager::ENABLE_IPGUARD, PropPage::T_BOOL },
	{ IDC_ENABLE_P2P_GUARD, SettingsManager::ENABLE_P2P_GUARD, PropPage::T_BOOL },
	{ IDC_FLYLINK_TRUST_IP_URL, SettingsManager::URL_IPTRUST, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

LRESULT RangesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	ipGuardEnabled = BOOLSETTING(ENABLE_IPGUARD);
	ipTrustEnabled = BOOLSETTING(ENABLE_IPTRUST);
	
	ctrlPolicy.Attach(GetDlgItem(IDC_DEFAULT_POLICY));
	ctrlPolicy.AddString(CTSTRING(DENY_ALL));
	ctrlPolicy.AddString(CTSTRING(ALLOW_ALL));
	ctrlPolicy.SetCurSel(BOOLSETTING(IPGUARD_DEFAULT_DENY) ? 0 : 1);
	
	try
	{
		ipGuardPath = IpGuard::getFileName();
		ipGuardData = File(ipGuardPath, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_FLYLINK_GUARD_IP, Text::toT(ipGuardData).c_str());
	}
	catch (const FileException&)
	{
		SetDlgItemText(IDC_FLYLINK_GUARD_IP, _T(""));
		// SetDlgItemText(IDC_FLYLINK_PATH, CTSTRING(ERR_IPFILTER));
	}
	fixControls();
	try
	{
		ipTrustPath = IpTrust::getFileName();
		ipTrustData = File(ipTrustPath, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_FLYLINK_TRUST_IP, Text::toT(ipTrustData).c_str());
		SetDlgItemText(IDC_FLYLINK_PATH, Text::toT(ipTrustPath).c_str());
	}
	catch (const FileException&)
	{
		SetDlgItemText(IDC_FLYLINK_PATH, CTSTRING(ERR_IPFILTER));
	}

	p2pGuardListBox.Attach(GetDlgItem(IDC_FLYLINK_MANUAL_P2P_GUARD_IP_LIST_BOX));
	loadManualP2PGuard();	
	return TRUE;
}

void RangesPage::loadManualP2PGuard()
{
	string data = CFlylinkDBManager::getInstance()->load_manual_p2p_guard();
	StringTokenizer<string> lines(data, "\r\n");
	for (auto i = 0; i < lines.getTokens().size(); ++i)
	{
		p2pGuardListBox.AddString(Text::toT(lines.getTokens()[i]).c_str());
	}
	
}
LRESULT RangesPage::onRemoveP2PManual(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int n = p2pGuardListBox.GetSelCount();
	if (n)
	{
		std::vector<int> aiSel;
		aiSel.resize(n);
		p2pGuardListBox.GetSelItems(n, aiSel.data());
		for (int i = 0; i < n; ++i)
		{
			int nLen = p2pGuardListBox.GetTextLen(aiSel[i]);
			if (nLen > 0)
			{
				tstring str;
				str.resize(nLen + 2);
				p2pGuardListBox.GetText(aiSel[i], &str[0]);
				CFlylinkDBManager::getInstance()->remove_manual_p2p_guard(Text::fromT(str));
			}
		}
	}
	p2pGuardListBox.ResetContent();
	loadManualP2PGuard();
	return 0;
}

LRESULT RangesPage::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_CHANGE), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_REMOVE), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT RangesPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

void RangesPage::write()
{
	PropPage::write(*this, items);
	g_settings->set(SettingsManager::IPGUARD_DEFAULT_DENY, !ctrlPolicy.GetCurSel());
	
	tstring ts;
	WinUtil::getWindowText(GetDlgItem(IDC_FLYLINK_GUARD_IP), ts);
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
	if (BOOLSETTING(ENABLE_IPGUARD))
	{
		if (changed || !ipGuardEnabled)
			ipGuard.load();
	}
	else
		ipGuard.clear();
	
	WinUtil::getWindowText(GetDlgItem(IDC_FLYLINK_TRUST_IP), ts);
	string strTrust = Text::fromT(ts);
	changed = false;
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
	if (BOOLSETTING(ENABLE_IPTRUST))
	{
		if (changed || !ipTrustEnabled)
			ipTrust.load();
	}
	else
		ipTrust.clear();
}

void RangesPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_ENABLE_IPGUARD) != BST_UNCHECKED;
	::EnableWindow(GetDlgItem(IDC_INTRO_IPGUARD), state);
	::EnableWindow(GetDlgItem(IDC_DEFAULT_POLICY_STR), state);
	::EnableWindow(GetDlgItem(IDC_DEFAULT_POLICY), state);
	::EnableWindow(GetDlgItem(IDC_DEFAULT_POLICY_EXCEPT_STR), state);
	::EnableWindow(GetDlgItem(IDC_FLYLINK_GUARD_IP), state);
	
	state = IsDlgButtonChecked(IDC_ENABLE_IPTRUST) != BST_UNCHECKED;
	::EnableWindow(GetDlgItem(IDC_FLYLINK_TRUST_IP), state);
	::EnableWindow(GetDlgItem(IDC_FLYLINK_TRUST_IP_URL), state);
}
