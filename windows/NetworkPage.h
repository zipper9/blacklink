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

#ifndef NETWORK_PAGE_H
#define NETWORK_PAGE_H

#include "PropPage.h"
#include "../client/NetworkUtil.h"

class NetworkPage;

struct NetworkSettings
{
	int portTCP;
	int portTLS;
	int portUDP;
	bool enableV6;
	bool autoDetect[2];
	int incomingConn[2];
	string bindAddr[2];
	string bindDev[2];
	int bindOptions[2];
	string mapper[2];

	void get();
	bool compare(const NetworkSettings& other) const;
};

class NetworkIPTab : public CDialogImpl<NetworkIPTab>
{
	public:
		enum { IDD = IDD_IP_TAB };

		NetworkIPTab(bool v6, NetworkPage* parent) : parent(parent), v6(v6) {}

		BEGIN_MSG_MAP(NetworkIPTab)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_EXTERNAL_IP, EN_KILLFOCUS, onKillFocusExternalIp)
		COMMAND_ID_HANDLER(IDC_ENABLE, onEnable)
		COMMAND_ID_HANDLER(IDC_CONNECTION_DETECTION, onChange)
		COMMAND_ID_HANDLER(IDC_DIRECT, onChange)
		COMMAND_ID_HANDLER(IDC_FIREWALL_PASSIVE, onChange)
		COMMAND_ID_HANDLER(IDC_FIREWALL_UPNP, onChange)
		COMMAND_ID_HANDLER(IDC_FIREWALL_NAT, onChange)
		COMMAND_ID_HANDLER(IDC_WAN_IP_MANUAL, onChange)
		COMMAND_ID_HANDLER(IDC_GETIP, onTestPorts)
		COMMAND_ID_HANDLER(IDC_OPTIONS, onOptions)
		COMMAND_HANDLER(IDC_BIND_ADDRESS, CBN_EDITCHANGE, onEditChange)
		COMMAND_HANDLER(IDC_BIND_ADDRESS, CBN_SELCHANGE, onSelChange)
		END_MSG_MAP()

		int getConnectionType() const;
		void updateState();
		void updatePortNumbers();
		void updateTLSOption();
		void copyPortSettings(NetworkIPTab& copyTo) const;
		void writePortSettings();
		void writeOtherSettings();
		void getPortSettingsFromUI(NetworkSettings& settings) const;
		void getOtherSettingsFromUI(NetworkSettings& settings) const;

	private:
		void fixControls();

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onKillFocusExternalIp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			fixControls();
			return 0;
		}
		LRESULT onEnable(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTestPorts(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		string getDeviceName(const vector<Util::AdapterInfo>& adapters) const;
		void updateOptionsButton();
		void updateOptionsButton(const tstring& ts);

		NetworkPage* const parent;
		const bool v6;
		int options;
		vector<Util::AdapterInfo> adapters;
		CComboBox bindCombo;
};

class NetworkFirewallTab : public CDialogImpl<NetworkFirewallTab>
{
	public:
		enum { IDD = IDD_FIREWALL_TAB };

		bool testWinFirewall();

		BEGIN_MSG_MAP(NetworkFirewallTab)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ADD_FIREWALL_EXCEPTION, onAddWinFirewallException)
		END_MSG_MAP()

	private:
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onAddWinFirewallException(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);

		CButton ctrlButton;
		CStatic ctrlText;
		CStatic ctrlIcon;
		tstring appPath;
};

class NetworkUrlsTab: public CDialogImpl<NetworkUrlsTab>
{
	public:
		enum { IDD = IDD_URLS_TAB };

		BEGIN_MSG_MAP(NetworkUrlsTab)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		END_MSG_MAP()

		void writeSettings();

	private:
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
};

class NetworkPage : public CPropertyPage<IDD_NETWORK_PAGE>, public PropPage
{
		friend class NetworkIPTab;
	
	public:
		explicit NetworkPage() : PropPage(TSTRING(SETTINGS_NETWORK)),
			useTLS(false), applyingSettings(false), tabIP{{false, this}, {true, this}}, prevTab(-1)
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}

		BEGIN_MSG_MAP(NetworkPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, onChangeTab)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onChangeTab(int idCtrl, LPNMHDR pnmh, BOOL& bHandled) { changeTab(); return 1; }

		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) *this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_CONNECTION; }
		void onShow() override;
		void write();
		void onTimer() override { updatePortState(); }
		void testPorts();
		static void setPrevSettings(NetworkSettings* settings)
		{
			prevSettings = settings;
		}

	private:
		bool useTLS;
		bool applyingSettings;
		static NetworkSettings* prevSettings;
		CTabCtrl ctrlTabs;
		NetworkIPTab tabIP[2];
		NetworkFirewallTab tabFirewall;
		NetworkUrlsTab tabUrls;

		int prevTab;

		void changeTab();
		bool runPortTest();
		bool runIpTest();
		void getFromUI(NetworkSettings& settings) const;
		void updatePortState();
};

#endif // !defined(NETWORK_PAGE_H)
