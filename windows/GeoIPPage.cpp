#include "stdafx.h"
#include "GeoIPPage.h"
#include "DialogLayout.h"
#include "WinUtil.h"
#include "../client/SettingsManager.h"
#include "../client/DatabaseManager.h"
#include "../client/FormatUtil.h"
#include "../client/ConfCore.h"

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
	{ IDC_GEOIP_URL, Conf::URL_GEOIP, PropPage::T_STR, PropPage::FLAG_DEFAULT_AS_HINT },
	{ IDC_GEOIP_AUTO_DOWNLOAD, Conf::GEOIP_AUTO_UPDATE, PropPage::T_BOOL },
	{ IDC_GEOIP_CHECK, Conf::GEOIP_CHECK_HOURS, PropPage::T_INT },
	{ IDC_USE_CUSTOM_LOCATIONS, Conf::USE_CUSTOM_LOCATIONS, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

LRESULT GeoIPPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::initControls(*this, items);
	PropPage::read(*this, items);

	CStatic placeholder(GetDlgItem(IDC_STATUS));
	RECT rc;
	placeholder.GetWindowRect(&rc);
	placeholder.DestroyWindow();
	ScreenToClient(&rc);
	ctrlStatus.Create(m_hWnd, rc, nullptr, WS_CHILD | WS_VISIBLE, 0, IDC_STATUS);

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

	int icon;
	tstring text;
	switch (status)
	{
		case DatabaseManager::MMDB_STATUS_OK:
		{
			tstring date = Text::toT(Util::formatDateTime("%x", timestamp));
			text = TSTRING_F(GEOIP_DB_DATE, date);
			icon = IconBitmaps::STATUS_SUCCESS;
			break;
		}
		case DatabaseManager::MMDB_STATUS_MISSING:
			text = TSTRING(GEOIP_DB_MISSING);
			icon = IconBitmaps::STATUS_FAILURE;
			break;
		case DatabaseManager::MMDB_STATUS_DOWNLOADING:
			text = TSTRING(GEOIP_DB_DOWNLOADING);
			icon = -1;
			break;
		default:
			text = TSTRING(GEOIP_DB_FAIL);
			icon = IconBitmaps::STATUS_FAILURE;
	}

	if (text != ctrlStatus.getText())
	{
		ctrlStatus.setText(text);
		ctrlStatus.setImage(icon, 0);
		ctrlStatus.Invalidate();
	}
}
