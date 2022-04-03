#include "stdafx.h"

#include "Resource.h"
#include "WebServerPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_STATIC0, ResourceManager::WEBSERVER },
	{ IDC_STATIC1, ResourceManager::PORT },
	{ IDC_STATIC8, ResourceManager::USERS },
	{ IDC_STATIC2, ResourceManager::USER },
	{ IDC_STATIC3, ResourceManager::PASSWORD },
	{ IDC_STATIC5, ResourceManager::POWER_USER },
	{ IDC_STATIC4, ResourceManager::PASSWORD },
	{ IDC_STATIC6, ResourceManager::WEBSERVER_SEARCHSIZE },
	{ IDC_STATIC7, ResourceManager::WEBSERVER_SEARCHPAGESIZE },
	{ IDC_CHECK1, ResourceManager::WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR },
	{ IDC_ALLOW_UPNP, ResourceManager::WEBSERVER_ALLOW_UPNP },
	{ IDC_SETTINGS_WEB_BIND_ADDRESS, ResourceManager::SETTINGS_BIND_ADDRESS },
	{ IDC_SETTINGS_WEB_BIND_ADDRESS_HELP, ResourceManager::WEBSERVER_BIND_INTERFACE },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_EDIT1,            SettingsManager::WEBSERVER_PORT, PropPage::T_INT },
	{ IDC_EDIT2,            SettingsManager::WEBSERVER_USER, PropPage::T_STR },
	{ IDC_EDIT3,            SettingsManager::WEBSERVER_PASS, PropPage::T_STR },
	{ IDC_EDIT5,            SettingsManager::WEBSERVER_POWER_USER, PropPage::T_STR },
	{ IDC_EDIT6,            SettingsManager::WEBSERVER_POWER_PASS, PropPage::T_STR },
	{ IDC_EDIT4,            SettingsManager::WEBSERVER_SEARCHSIZE, PropPage::T_INT },
	{ IDC_EDIT7,            SettingsManager::WEBSERVER_SEARCHPAGESIZE, PropPage::T_INT },
	{ IDC_CHECK1,           SettingsManager::WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR, PropPage::T_BOOL },
	{ IDC_ALLOW_UPNP,       SettingsManager::WEBSERVER_ALLOW_UPNP, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

LRESULT WebServerPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WinUtil::translate(*this, texts);
	PropPage::read(*this, items);
	
	CComboBox bindCombo(GetDlgItem(IDC_WEB_BIND_ADDRESS));
	WinUtil::fillAdapterList(AF_INET, bindCombo, SETTING(WEBSERVER_BIND_ADDRESS), 0);
	return TRUE;
}

void WebServerPage::write()
{
	PropPage::write(*this, items);
	CComboBox bindCombo(GetDlgItem(IDC_WEB_BIND_ADDRESS));
	g_settings->set(SettingsManager::WEBSERVER_BIND_ADDRESS, WinUtil::getSelectedAdapter(bindCombo));
}
