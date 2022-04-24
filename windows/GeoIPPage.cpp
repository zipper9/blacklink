#include "stdafx.h"
#include "GeoIPPage.h"
#include "DialogLayout.h"
#include "../client/SettingsManager.h"
#include "../client/DatabaseManager.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 2, DialogLayout::SIDE_RIGHT, U_DU(2) };
static const DialogLayout::Align align2 = { 5, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { 6, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_GEOIP_URL, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_GEOIP_URL, 0, UNSPEC, UNSPEC },
	{ IDC_GEOIP_DOWNLOAD, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align1 },
	{ IDC_GEOIP_AUTO_DOWNLOAD, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_GEOIP_CHECK, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_GEOIP_CHECK, 0, UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_CAPTION_HOURS, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align3 }
};

static const PropPage::Item items[] =
{
	{ IDC_GEOIP_URL, SettingsManager::URL_GEOIP, PropPage::T_STR },
	{ IDC_GEOIP_AUTO_DOWNLOAD, SettingsManager::GEOIP_AUTO_UPDATE, PropPage::T_BOOL },
	{ IDC_GEOIP_CHECK, SettingsManager::GEOIP_CHECK_HOURS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

LRESULT GeoIPPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items);
	fixControls();
	updateButtonState();
	return TRUE;
}

LRESULT GeoIPPage::onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	DatabaseManager::getInstance()->downloadGeoIPDatabase(0, true);
	updateButtonState();
	return 0;
}

void GeoIPPage::write()
{
	PropPage::write(*this, items);
}

void GeoIPPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_GEOIP_AUTO_DOWNLOAD);
	GetDlgItem(IDC_GEOIP_CHECK).EnableWindow(state);
}

void GeoIPPage::updateButtonState()
{
	GetDlgItem(IDC_GEOIP_DOWNLOAD).EnableWindow(DatabaseManager::getInstance()->isDownloading() ? FALSE : TRUE);
}
