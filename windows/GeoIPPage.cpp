#include "stdafx.h"
#include "GeoIPPage.h"
#include "DialogLayout.h"
#include "HIconWrapper.h"
#include "WinUtil.h"
#include "../client/SettingsManager.h"
#include "../client/DatabaseManager.h"

static HIconWrapper iconError(IDR_ICON_WARN_ICON);
static HIconWrapper iconSuccess(IDR_ICON_SUCCESS_ICON);
static const int ICON_SIZE = 16;

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 2, DialogLayout::SIDE_LEFT,  U_DU(0) };
static const DialogLayout::Align align3 = { 3, DialogLayout::SIDE_LEFT,  U_DU(2) };
static const DialogLayout::Align align4 = { 6, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align5 = { 7, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_GROUP, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CAPTION_GEOIP_URL, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_GEOIP_DOWNLOAD, FLAG_TRANSLATE, AUTO, UNSPEC, 0, nullptr, &align1 },
	{ IDC_GEOIP_URL, 0, UNSPEC, UNSPEC, 0, &align2, &align3 },
	{ IDC_GEOIP_AUTO_DOWNLOAD, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_GEOIP_CHECK, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_GEOIP_CHECK, 0, UNSPEC, UNSPEC, 0, &align4 },
	{ IDC_CAPTION_HOURS, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align5 },
	{ IDC_USE_CUSTOM_LOCATIONS, FLAG_TRANSLATE, AUTO, UNSPEC }
};

static const PropPage::Item items[] =
{
	{ IDC_GEOIP_URL, SettingsManager::URL_GEOIP, PropPage::T_STR },
	{ IDC_GEOIP_AUTO_DOWNLOAD, SettingsManager::GEOIP_AUTO_UPDATE, PropPage::T_BOOL },
	{ IDC_GEOIP_CHECK, SettingsManager::GEOIP_CHECK_HOURS, PropPage::T_INT },
	{ IDC_USE_CUSTOM_LOCATIONS, SettingsManager::USE_CUSTOM_LOCATIONS, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

LRESULT GeoIPPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items);

	CWindow ctrlIcon(GetDlgItem(IDC_STATUS_ICON));
	RECT rc;
	ctrlIcon.GetClientRect(&rc);
	ctrlIcon.MapWindowPoints(m_hWnd, &rc);
	int iconY = rc.top;
	iconX = rc.left;
	textX = iconX + ICON_SIZE + 6;
	CWindow ctrlText(GetDlgItem(IDC_STATUS));
	ctrlText.GetClientRect(&rc);
	textWidth = rc.right - rc.left;
	textHeight = rc.bottom - rc.top;
	textY = iconY + ICON_SIZE - textHeight;

	fixControls();
	updateState();
	return TRUE;
}

LRESULT GeoIPPage::onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring url;
	WinUtil::getWindowText(GetDlgItem(IDC_GEOIP_URL), url);
	DatabaseManager::getInstance()->downloadGeoIPDatabase(0, true, Text::fromT(url));
	updateState();
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

void GeoIPPage::updateState()
{
	uint64_t timestamp;
	int status = DatabaseManager::getInstance()->getGeoIPDatabaseStatus(timestamp);
	GetDlgItem(IDC_GEOIP_DOWNLOAD).EnableWindow(status == DatabaseManager::MMDB_STATUS_DOWNLOADING ? FALSE : TRUE);

	HICON icon;
	tstring text;
	switch (status)
	{
		case DatabaseManager::MMDB_STATUS_OK:
		{
			tstring date = Text::toT(Util::formatDateTime("%x", timestamp));
			text = TSTRING_F(GEOIP_DB_DATE, date);
			icon = iconSuccess;
			break;
		}
		case DatabaseManager::MMDB_STATUS_MISSING:
			text = TSTRING(GEOIP_DB_MISSING);
			icon = iconError;
			break;
		case DatabaseManager::MMDB_STATUS_DOWNLOADING:
			text = TSTRING(GEOIP_DB_DOWNLOADING);
			icon = nullptr;
			break;
		default:
			text = TSTRING(GEOIP_DB_FAIL);
			icon = iconError;
	}

	CWindow ctrlIcon(GetDlgItem(IDC_STATUS_ICON));	
	if (icon)
	{
		ctrlIcon.SendMessage(STM_SETICON, (WPARAM) icon, 0);
		ctrlIcon.MoveWindow(iconX, textY + textHeight - ICON_SIZE, ICON_SIZE, ICON_SIZE);
	}

	CWindow ctrlText(GetDlgItem(IDC_STATUS));
	ctrlText.SetWindowText(text.c_str());
	ctrlText.MoveWindow(icon ? textX : iconX, textY, textWidth, textHeight);
	ctrlIcon.ShowWindow(icon ? SW_SHOW : SW_HIDE);
}
