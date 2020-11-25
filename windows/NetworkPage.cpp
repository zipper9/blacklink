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

#include <comdef.h>
#include "Resource.h"

#include "NetworkPage.h"
#include "WinUtil.h"
#include "../client/CryptoManager.h"
#include "../client/ConnectivityManager.h"
#include "../client/DownloadManager.h"
#include "../client/PortTest.h"
#include "../client/webrtc/talk/base/winfirewall.h"

extern bool g_DisableTestPort;
extern int g_tlsOption;

enum
{
	IconFailure = 0,
	IconSuccess,
	IconWaiting,
	IconWarning,
	IconUnknown,
	IconQuestion,
	IconDisabled
};

static const PropPage::TextItem texts[] =
{
	{ IDC_CONNECTION_DETECTION,        ResourceManager::CONNECTION_DETECTION           },
	{ IDC_DIRECT,                      ResourceManager::SETTINGS_DIRECT                },
	{ IDC_FIREWALL_UPNP,               ResourceManager::SETTINGS_FIREWALL_UPNP         },
	{ IDC_FIREWALL_NAT,                ResourceManager::SETTINGS_FIREWALL_NAT          },
	{ IDC_FIREWALL_PASSIVE,            ResourceManager::SETTINGS_FIREWALL_PASSIVE      },
	{ IDC_WAN_IP_MANUAL,               ResourceManager::SETTINGS_WAN_IP_MANUAL         },
	{ IDC_NO_IP_OVERRIDE,              ResourceManager::SETTINGS_NO_IP_OVERRIDE        },
	{ IDC_SETTINGS_PORTS,              ResourceManager::SETTINGS_PORTS                 },
	{ IDC_SETTINGS_IP,                 ResourceManager::SETTINGS_EXTERNAL_IP           },
	{ IDC_SETTINGS_PORT_TCP,           ResourceManager::SETTINGS_TCP_PORT              },
	{ IDC_SETTINGS_PORT_UDP,           ResourceManager::SETTINGS_UDP_PORT              },
	{ IDC_SETTINGS_PORT_TLS,           ResourceManager::SETTINGS_TLS_PORT              },
	{ IDC_SETTINGS_PORT_TORRENT,       ResourceManager::SETTINGS_TORRENT_PORT          },
	{ IDC_SETTINGS_INCOMING,           ResourceManager::SETTINGS_INCOMING              },
	{ IDC_SETTINGS_BIND_ADDRESS,       ResourceManager::SETTINGS_BIND_ADDRESS          },
	{ IDC_SETTINGS_BIND_ADDRESS_HELP,  ResourceManager::SETTINGS_BIND_ADDRESS_HELP     },
	{ IDC_NATT,                        ResourceManager::ALLOW_NAT_TRAVERSAL            },
	{ IDC_AUTO_TEST_PORTS,             ResourceManager::TEST_PORTS_AUTO                },
	{ IDC_IPUPDATE,                    ResourceManager::UPDATE_IP                      },
	{ IDC_SETTINGS_UPDATE_IP_INTERVAL, ResourceManager::UPDATE_IP_INTERVAL             },
	{ IDC_SETTINGS_USE_TORRENT,        ResourceManager::USE_TORRENT_SEARCH_TEXT        },
	{ IDC_GETIP,                       ResourceManager::TEST_PORTS_AND_GET_IP          },
	{ IDC_ADD_FIREWALL_EXCEPTION,      ResourceManager::ADD_FIREWALL_EXCEPTION         },
	{ IDC_STATIC_GATEWAY,              ResourceManager::SETTINGS_GATEWAY               },
	{ IDC_CAPTION_MAPPER,              ResourceManager::PREFERRED_MAPPER               },
	{ IDC_USE_DHT,                     ResourceManager::SETTINGS_USE_DHT               },
	{ 0,                               ResourceManager::Strings()                      }
};

static const PropPage::Item items[] =
{
	{ IDC_CONNECTION_DETECTION,  SettingsManager::AUTO_DETECT_CONNECTION, PropPage::T_BOOL },
	{ IDC_PORT_TCP,              SettingsManager::TCP_PORT,               PropPage::T_INT  },
	{ IDC_PORT_UDP,              SettingsManager::UDP_PORT,               PropPage::T_INT  },
	{ IDC_PORT_TLS,              SettingsManager::TLS_PORT,               PropPage::T_INT  },
	{ IDC_NO_IP_OVERRIDE,        SettingsManager::NO_IP_OVERRIDE,         PropPage::T_BOOL },
//	{ IDC_IP_GET_IP,             SettingsManager::URL_GET_IP,             PropPage::T_STR  },
	{ IDC_IPUPDATE,              SettingsManager::IPUPDATE,               PropPage::T_BOOL },
	{ IDC_WAN_IP_MANUAL,         SettingsManager::WAN_IP_MANUAL,          PropPage::T_BOOL },
	{ IDC_UPDATE_IP_INTERVAL,    SettingsManager::IPUPDATE_INTERVAL,      PropPage::T_INT  },
	{ IDC_AUTO_TEST_PORTS,       SettingsManager::AUTO_TEST_PORTS,        PropPage::T_BOOL },
	{ IDC_NATT,                  SettingsManager::ALLOW_NAT_TRAVERSAL,    PropPage::T_BOOL },
	{ IDC_PORT_TORRENT,          SettingsManager::DHT_PORT,               PropPage::T_INT  },
	{ IDC_SETTINGS_USE_TORRENT,  SettingsManager::USE_TORRENT_SEARCH,     PropPage::T_BOOL },
	{ IDC_USE_DHT,               SettingsManager::USE_DHT,                PropPage::T_BOOL },
	{ 0,                         0,                                       PropPage::T_END  }
};

LRESULT NetworkPage::OnKillFocusExternalIp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring tmp;
	CWindow externalIp(GetDlgItem(IDC_EXTERNAL_IP));
	WinUtil::getWindowText(externalIp, tmp);
	string ipStr = Text::fromT(tmp);
	if (!ipStr.empty())
	{
		boost::system::error_code ec;
		const auto ip = boost::asio::ip::address_v4::from_string(ipStr, ec);
		if (ec)
		{
			ipStr = SETTING(EXTERNAL_IP);
			MessageBox(CTSTRING(BAD_IP_ADDRESS), getAppNameVerT().c_str(), MB_OK | MB_ICONWARNING);
			externalIp.SetWindowText(Text::toT(ipStr).c_str());
		}
	}
	return 0;
}

int NetworkPage::getConnectionType() const
{
	int ct = SettingsManager::INCOMING_DIRECT;
	if (IsDlgButtonChecked(IDC_FIREWALL_UPNP))
		ct = SettingsManager::INCOMING_FIREWALL_UPNP;
	else if (IsDlgButtonChecked(IDC_FIREWALL_NAT))
		ct = SettingsManager::INCOMING_FIREWALL_NAT;
	else if (IsDlgButtonChecked(IDC_FIREWALL_PASSIVE))
		ct = SettingsManager::INCOMING_FIREWALL_PASSIVE;
	return ct;
}

void NetworkPage::write()
{
	PropPage::write(*this, items);
	
	CWindow externalIp(GetDlgItem(IDC_EXTERNAL_IP));
	if (externalIp.IsWindowEnabled())
	{
		tstring str;
		WinUtil::getWindowText(externalIp, str);
		g_settings->set(SettingsManager::EXTERNAL_IP, Text::fromT(str));
	}
	else
		g_settings->set(SettingsManager::EXTERNAL_IP, Util::emptyString);
	
	g_settings->set(SettingsManager::BIND_ADDRESS, WinUtil::getSelectedAdapter(CComboBox(GetDlgItem(IDC_BIND_ADDRESS))));
	g_settings->set(SettingsManager::INCOMING_CONNECTIONS, getConnectionType());

	CComboBox mapperCombo(GetDlgItem(IDC_MAPPER));
	int selIndex = mapperCombo.GetCurSel();
	StringList mappers = ConnectivityManager::getInstance()->getMapperV4().getMappers();
	if (selIndex >= 0 && selIndex < (int) mappers.size())
		g_settings->set(SettingsManager::MAPPER, mappers[selIndex]);
}

LRESULT NetworkPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	
#ifndef IRAINMAN_IP_AUTOUPDATE
	::EnableWindow(GetDlgItem(IDC_IPUPDATE), FALSE);
#endif
#ifdef FLYLINKDC_USE_TORRENT
	SET_SETTING(DHT_PORT, DownloadManager::getInstance()->listen_torrent_port());
#else
	GetDlgItem(IDC_SETTINGS_PORT_TORRENT).ShowWindow(SW_HIDE);
	GetDlgItem(IDC_PORT_TORRENT).ShowWindow(SW_HIDE);
	GetDlgItem(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO).ShowWindow(SW_HIDE);
	GetDlgItem(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO_UPNP).ShowWindow(SW_HIDE);
	GetDlgItem(IDC_SETTINGS_USE_TORRENT).ShowWindow(SW_HIDE);
#endif
	
	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
			CheckDlgButton(IDC_DIRECT, BST_CHECKED);
			break;
		case SettingsManager::INCOMING_FIREWALL_UPNP:
			CheckDlgButton(IDC_FIREWALL_UPNP, BST_CHECKED);
			break;
		case SettingsManager::INCOMING_FIREWALL_NAT:
			CheckDlgButton(IDC_FIREWALL_NAT, BST_CHECKED);
			break;
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
			CheckDlgButton(IDC_FIREWALL_PASSIVE, BST_CHECKED);
			break;
		default:
			CheckDlgButton(IDC_DIRECT, BST_CHECKED);
			break;
	}
	
	PropPage::read(*this, items);
	useTLS = BOOLSETTING(USE_TLS);
	GetDlgItem(IDC_EXTERNAL_IP).SetWindowText(Text::toT(SETTING(EXTERNAL_IP)).c_str());
	
	fixControls();
	
	CEdit(GetDlgItem(IDC_PORT_TCP)).LimitText(5);
	CEdit(GetDlgItem(IDC_PORT_UDP)).LimitText(5);
	CEdit(GetDlgItem(IDC_PORT_TLS)).LimitText(5);
	
	CComboBox bindCombo(GetDlgItem(IDC_BIND_ADDRESS));
	WinUtil::fillAdapterList(false, bindCombo, SETTING(BIND_ADDRESS));

	CComboBox mapperCombo(GetDlgItem(IDC_MAPPER));
	StringList mappers = ConnectivityManager::getInstance()->getMapperV4().getMappers();
	int selIndex = 0;
	for (size_t i = 0; i < mappers.size(); ++i)
	{
		mapperCombo.AddString(Text::toT(mappers[i]).c_str());
		if (mappers[i] == SETTING(MAPPER))
			selIndex = i;
	}
	mapperCombo.SetCurSel(selIndex);

	updatePortState();
	//::SendMessage(m_hWnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, IDC_ADD_FLYLINKDC_WINFIREWALL, true);
	//SetButtonElevationRequiredState(IDC_ADD_FLYLINKDC_WINFIREWALL,);
	
	string gateway = Socket::getDefaultGateway();
	//MappingManager::setDefaultGatewayIP(gateway);
	GetDlgItem(IDC_DEFAULT_GATEWAY_IP).SetWindowText(Text::toT(gateway).c_str());
	return TRUE;
}

static int getIconForState(boost::logic::tribool state)
{
	if (boost::logic::indeterminate(state)) return IconUnknown;
	if (state) return IconSuccess;
	return IconFailure;
}

void NetworkPage::onShow()
{
	if (!g_tlsOption) return;
	useTLS = g_tlsOption == 1;
	const BOOL autoDetect = IsDlgButtonChecked(IDC_CONNECTION_DETECTION) == BST_CHECKED;
	::EnableWindow(GetDlgItem(IDC_PORT_TLS), !autoDetect && useTLS);
	updatePortState();
}

void NetworkPage::fixControls()
{
	const BOOL autoDetect = IsDlgButtonChecked(IDC_CONNECTION_DETECTION) == BST_CHECKED;
	//const BOOL direct = IsDlgButtonChecked(IDC_DIRECT) == BST_CHECKED;
	const BOOL upnp = IsDlgButtonChecked(IDC_FIREWALL_UPNP) == BST_CHECKED;
	const BOOL nat = IsDlgButtonChecked(IDC_FIREWALL_NAT) == BST_CHECKED;
	//const BOOL nat_traversal = IsDlgButtonChecked(IDC_NATT) == BST_CHECKED;
	
	const BOOL passive = IsDlgButtonChecked(IDC_FIREWALL_PASSIVE) == BST_CHECKED;	
	const BOOL manualIP = IsDlgButtonChecked(IDC_WAN_IP_MANUAL) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_DIRECT), !autoDetect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_UPNP), !autoDetect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_NAT), !autoDetect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_PASSIVE), !autoDetect);
	
#ifdef FLYLINKDC_USE_TORRENT
	const BOOL torrent = IsDlgButtonChecked(IDC_SETTINGS_USE_TORRENT) == BST_CHECKED;
	::EnableWindow(GetDlgItem(IDC_SETTINGS_PORT_TORRENT), torrent);
	::EnableWindow(GetDlgItem(IDC_PORT_TORRENT), torrent);
#endif
	
	::EnableWindow(GetDlgItem(IDC_EXTERNAL_IP), manualIP);
	
	::EnableWindow(GetDlgItem(IDC_SETTINGS_IP), !autoDetect);
	
	//::EnableWindow(GetDlgItem(IDC_IP_GET_IP), !autoDetect && (upnp || nat) && !m_is_manual);
	::EnableWindow(GetDlgItem(IDC_NO_IP_OVERRIDE), FALSE); // !autoDetect && (direct || upnp || nat || nat_traversal));
#ifdef IRAINMAN_IP_AUTOUPDATE
	::EnableWindow(GetDlgItem(IDC_IPUPDATE), upnp || nat);
#endif
	const BOOL ipupdate = (upnp || nat) && (IsDlgButtonChecked(IDC_IPUPDATE) == BST_CHECKED);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_UPDATE_IP_INTERVAL), ipupdate);
	::EnableWindow(GetDlgItem(IDC_UPDATE_IP_INTERVAL), ipupdate);
	const BOOL portEnabled = !autoDetect;// && (upnp || nat);
	::EnableWindow(GetDlgItem(IDC_PORT_TCP), portEnabled);
	::EnableWindow(GetDlgItem(IDC_PORT_UDP), portEnabled);
	::EnableWindow(GetDlgItem(IDC_PORT_TLS), portEnabled && useTLS);
	::EnableWindow(GetDlgItem(IDC_BIND_ADDRESS), !autoDetect);
	//::EnableWindow(GetDlgItem(IDC_SETTINGS_BIND_ADDRESS_HELP), !autoDetect);
	//::EnableWindow(GetDlgItem(IDC_SETTINGS_PORTS_UPNP), upnp);
	
	testWinFirewall();
#ifdef FLYLINKDC_USE_TORRENT
	setIcon(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO_UPNP, getIconForState(SettingsManager::g_upnpTorrentLevel));
#endif
}

LRESULT NetworkPage::onWANIPManualClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

LRESULT NetworkPage::onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

static int getIconForPortState(int state)
{
	if (g_DisableTestPort) return IconDisabled;
	if (state == PortTest::STATE_SUCCESS) return IconSuccess;
	if (state == PortTest::STATE_FAILURE) return IconFailure;
	if (state == PortTest::STATE_RUNNING) return IconWaiting;
	return IconQuestion;
}

static int getIconForMappingState(int state)
{
	if (state == MappingManager::STATE_SUCCESS) return IconSuccess;
	if (state == MappingManager::STATE_FAILURE) return IconFailure;
	if (state == MappingManager::STATE_RENEWAL_FAILURE) return IconWarning;
	if (state == MappingManager::STATE_RUNNING) return IconWaiting;
	return IconQuestion;
}

static int getControlIDForProto(int proto)
{
	switch (proto)
	{
		case PortTest::PORT_UDP: return IDC_NETWORK_TEST_PORT_UDP_ICO;
		case PortTest::PORT_TCP: return IDC_NETWORK_TEST_PORT_TCP_ICO;
		case PortTest::PORT_TLS: return IDC_NETWORK_TEST_PORT_TLS_TCP_ICO;
	}
	return -1;
}

static int getControlIDForUPnP(int type)
{
	switch (type)
	{
		case MappingManager::PORT_UDP: return IDC_NETWORK_TEST_PORT_UDP_ICO_UPNP;
		case MappingManager::PORT_TCP: return IDC_NETWORK_TEST_PORT_TCP_ICO_UPNP;
		case MappingManager::PORT_TLS: return IDC_NETWORK_TEST_PORT_TLS_TCP_ICO_UPNP;
	}
	return -1;
}

void NetworkPage::updatePortState()
{
	const MappingManager& mapperV4 = ConnectivityManager::getInstance()->getMapperV4();
	bool running = false;
	for (int type = 0; type < PortTest::MAX_PORTS; type++)
	{
		int port, portIcon, mappingIcon;
		int portState = g_portTest.getState(type, port, nullptr);
		int mappingState = mapperV4.getState(type);
		if (portState == PortTest::STATE_RUNNING) running = true;
		if (type == PortTest::PORT_TLS && !useTLS)
		{
			portIcon = IconDisabled;
			mappingIcon = IconDisabled;
		}
		else
		{
			portIcon = getIconForPortState(portState);
			mappingIcon = getIconForMappingState(mappingState);
			if (mappingIcon == IconFailure && portIcon == IconSuccess)
				mappingIcon = IconUnknown;
		}
		setIcon(getControlIDForProto(type), portIcon);
		setIcon(getControlIDForUPnP(type), mappingIcon);
	}

	CButton ctrl(GetDlgItem(IDC_GETIP));
	if (running)
	{
		ctrl.SetWindowText(CTSTRING(TESTING_PORTS));
		ctrl.EnableWindow(FALSE);
	}
	else if (ConnectivityManager::getInstance()->isSetupInProgress())
	{
		ctrl.SetWindowText(CTSTRING(APPLYING_SETTINGS));
		ctrl.EnableWindow(FALSE);
	}
	else
	{
		ctrl.SetWindowText(CTSTRING(TEST_PORTS_AND_GET_IP));
		ctrl.EnableWindow(TRUE);
	}

	CWindow externalIp(GetDlgItem(IDC_EXTERNAL_IP));
	if (!externalIp.IsWindowEnabled())
	{
		string ipAddr = ConnectivityManager::getInstance()->getReflectedIP();
		externalIp.SetWindowText(Text::toT(ipAddr).c_str());
	}
	
#ifdef FLYLINKDC_USE_TORRENT
	// Testing DHT port is not supported
	setIcon(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO, IconUnknown);
#endif
}

LRESULT NetworkPage::onAddWinFirewallException(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	const tstring appPath = Util::getModuleFileName();
	talk_base::WinFirewall fw;
	HRESULT hr;
	fw.Initialize(&hr);
	const auto res = fw.AddApplicationW(appPath.c_str(), getAppNameT().c_str(), true, &hr);
	if (res)
	{
		MessageBox(CTSTRING(FIREWALL_EXCEPTION_ADDED), getAppNameVerT().c_str(), MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		_com_error msg(hr);
		MessageBox(CTSTRING_F(FIREWALL_EXCEPTION_ERROR, msg.ErrorMessage() % Util::toHexStringT(hr)),
			getAppNameVerT().c_str(), MB_OK | MB_ICONERROR);
	}
	testWinFirewall();
	return 0;
}

void NetworkPage::testWinFirewall()
{
	CButton btn(GetDlgItem(IDC_ADD_FIREWALL_EXCEPTION));
	const tstring appPath = Util::getModuleFileName();
	
	talk_base::WinFirewall fw;
	HRESULT hr;
	fw.Initialize(&hr);
	if (!fw.Enabled())
	{
		btn.SetWindowTextW(CTSTRING(WINDOWS_FIREWALL_DISABLED));
		btn.EnableWindow(FALSE);
		GetDlgItem(IDC_NETWORK_WINFIREWALL_ICO).ShowWindow(SW_HIDE);
		return;
	}

	bool authorized = false;
	bool res = fw.QueryAuthorizedW(appPath.c_str(), &authorized);
	if (res)
	{
		if (authorized)
			setIcon(IDC_NETWORK_WINFIREWALL_ICO, IconSuccess);
		else
			setIcon(IDC_NETWORK_WINFIREWALL_ICO, IconFailure);
	}
	else
		setIcon(IDC_NETWORK_WINFIREWALL_ICO, IconQuestion);
	btn.SetWindowTextW(CTSTRING(ADD_FIREWALL_EXCEPTION));
	btn.EnableWindow(TRUE);
	GetDlgItem(IDC_NETWORK_WINFIREWALL_ICO).ShowWindow(SW_SHOW);
}

bool NetworkPage::runPortTest()
{
	int portTCP = SETTING(TCP_PORT);
	g_portTest.setPort(PortTest::PORT_TCP, portTCP);
	int portUDP = SETTING(UDP_PORT);
	g_portTest.setPort(PortTest::PORT_UDP, portUDP);
	int mask = 1<<PortTest::PORT_UDP | 1<<PortTest::PORT_TCP;
	if (useTLS)
	{
		int portTLS = SETTING(TLS_PORT);
		g_portTest.setPort(PortTest::PORT_TLS, portTLS);
		mask |= 1<<PortTest::PORT_TLS;
	}
	if (!g_portTest.runTest(mask)) return false;
	updatePortState();
	return true;
}

LRESULT NetworkPage::onTestPorts(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	testWinFirewall();
	updatePortState();
	Settings currentSettings;
	currentSettings.get();
	Settings newSettings;
	getFromUI(newSettings);
	if (!currentSettings.compare(newSettings))
	{
	    if (MessageBox(CTSTRING(NETWORK_SETTINGS_CHANGED), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
		write();
		ConnectivityManager::getInstance()->setupConnections(true);
	}
	runPortTest();
	return 0;
}

void NetworkPage::setIcon(int id, int stateIcon)
{
	static HIconWrapper g_hModeActiveIco(IDR_ICON_SUCCESS_ICON);
	static HIconWrapper g_hModePassiveIco(IDR_ICON_WARN_ICON);
	static HIconWrapper g_hModeQuestionIco(IDR_ICON_QUESTION_ICON);
	static HIconWrapper g_hModeFailIco(IDR_ICON_FAIL_ICON);
	static HIconWrapper g_hModePauseIco(IDR_ICON_PAUSE_ICON);
	static HIconWrapper g_hModeProcessIco(IDR_NETWORK_STATISTICS_ICON);
	//static HIconWrapper g_hModeDisableTestIco(IDR_SKULL_RED_ICO);
	
	HICON icon;
	switch (stateIcon)
	{
		case IconFailure:
			icon = (HICON) g_hModeFailIco;
			break;
		case IconSuccess:
			icon = (HICON) g_hModeActiveIco;
			break;
		case IconWarning:
			icon = (HICON) g_hModePassiveIco;
			break;
		case IconUnknown:
			icon = (HICON) g_hModePauseIco;
			break;
		case IconQuestion:
			icon = (HICON) g_hModeQuestionIco;
			break;
		case IconDisabled:
			icon = nullptr;
			break;
		case IconWaiting:
		default:
			icon = (HICON) g_hModeProcessIco;
	}
	CWindow wnd(GetDlgItem(id));
	if (icon)
	{
		wnd.SendMessage(STM_SETICON, (WPARAM) icon, 0);
		wnd.ShowWindow(SW_SHOW);
	}
	else
		wnd.ShowWindow(SW_HIDE);
}

void NetworkPage::getFromUI(Settings& settings) const
{
	settings.autoDetect = IsDlgButtonChecked(IDC_CONNECTION_DETECTION) == BST_CHECKED;
	settings.incomingConn = getConnectionType();
	tstring buf;
	WinUtil::getWindowText(GetDlgItem(IDC_PORT_TCP), buf);
	settings.portTCP = Util::toInt(buf);
	CButton wndTLS(GetDlgItem(IDC_PORT_TLS));
	if (wndTLS.IsWindowEnabled())
	{
		WinUtil::getWindowText(wndTLS, buf);
		settings.portTLS = Util::toInt(buf);
	}
	else
		settings.portTLS = -1;
	WinUtil::getWindowText(GetDlgItem(IDC_PORT_UDP), buf);
	settings.portUDP = Util::toInt(buf);
	settings.bindAddr = WinUtil::getSelectedAdapter(CComboBox(GetDlgItem(IDC_BIND_ADDRESS)));
	
	CComboBox mapperCombo(GetDlgItem(IDC_MAPPER));
	int selIndex = mapperCombo.GetCurSel();
	StringList mappers = ConnectivityManager::getInstance()->getMapperV4().getMappers();
	if (selIndex >= 0 && selIndex < (int) mappers.size())
		settings.mapper = mappers[selIndex];
}

void NetworkPage::Settings::get()
{
	autoDetect = BOOLSETTING(AUTO_DETECT_CONNECTION);
	incomingConn = SETTING(INCOMING_CONNECTIONS);
	portTCP = SETTING(TCP_PORT);
	portTLS = BOOLSETTING(USE_TLS) ? SETTING(TLS_PORT) : -1;
	portUDP = SETTING(UDP_PORT);
	bindAddr = SETTING(BIND_ADDRESS);
	mapper = SETTING(MAPPER);
}

bool NetworkPage::Settings::compare(const NetworkPage::Settings& other) const
{
	return autoDetect == other.autoDetect &&
		incomingConn == other.incomingConn &&
		portTCP == other.portTCP &&
		portTLS == other.portTLS &&
		portUDP == other.portUDP &&
		bindAddr == other.bindAddr &&
		mapper == other.mapper;
}
