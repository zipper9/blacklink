/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "ShareMiscPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_SHARING_OPTIONS, ResourceManager::SETTINGS_SHARING_OPTIONS },
	{ IDC_SHAREHIDDEN, ResourceManager::SETTINGS_SHARE_HIDDEN },
	{ IDC_SHARESYSTEM, ResourceManager::SETTINGS_SHARE_SYSTEM },
	{ IDC_SHAREVIRTUAL, ResourceManager::SETTINGS_SHARE_VIRTUAL },
	{ IDC_SHARING_AUTO_REFRESH, ResourceManager::SETTINGS_AUTOMATIC_REFRESH },
	{ IDC_REFRESH_SHARE_ON_STARTUP, ResourceManager::SETTINGS_REFRESH_ON_STARTUP },
	{ IDC_CAPTION_REFRESH_SHARE, ResourceManager::SETTINGS_REFRESH_EVERY },
	{ IDC_CAPTION_MINUTES, ResourceManager::SETTINGS_MINUTES },
	{ IDC_CAPTION_0_TO_DISABLE, ResourceManager::SETTINGS_ZERO_TO_DISABLE },
	{ IDC_SHARING_HASHING, ResourceManager::SETTINGS_HASHING_OPTIONS },
	{ IDC_TTH_IN_STREAM, ResourceManager::SETTINGS_SAVE_TTH_IN_NTFS_FILESTREAM },
	{ IDC_CAPTION_MIN_SIZE, ResourceManager::SETTINGS_MIN_SIZE_SAVE_TREES },
	{ IDC_SETTINGS_MB, ResourceManager::MB },
	{ IDC_CAPTION_MAX_HASH_SPEED, ResourceManager::SETTINGS_MAX_HASH_SPEED },
	{ IDC_SETTINGS_MBS, ResourceManager::MBPS },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_SHAREHIDDEN, SettingsManager::SHARE_HIDDEN, PropPage::T_BOOL },
	{ IDC_SHARESYSTEM, SettingsManager::SHARE_SYSTEM, PropPage::T_BOOL },
	{ IDC_SHAREVIRTUAL, SettingsManager::SHARE_VIRTUAL, PropPage::T_BOOL },
	{ IDC_REFRESH_SHARE_ON_STARTUP, SettingsManager::AUTO_REFRESH_ON_STARTUP, PropPage::T_BOOL },
	{ IDC_AUTO_REFRESH_TIME, SettingsManager::AUTO_REFRESH_TIME, PropPage::T_INT },
	{ IDC_TTH_IN_STREAM, SettingsManager::SAVE_TTH_IN_NTFS_FILESTREAM, PropPage::T_BOOL },
	{ IDC_MIN_FILE_SIZE, SettingsManager::SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM, PropPage::T_INT},
	{ IDC_MAX_HASH_SPEED, SettingsManager::MAX_HASH_SPEED, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

LRESULT ShareMiscPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	
	CUpDownCtrl updown;
	SET_MIN_MAX(IDC_MIN_FILE_SIZE_SPIN, 1, 1000);
	//SET_MIN_MAX(IDC_REFRESH_SPIN, 0, 3000);
	SET_MIN_MAX(IDC_MAX_HASH_SPEED_SPIN, 0, 999);
	
	fixControls();
	return TRUE;
}

void ShareMiscPage::write()
{
	PropPage::write(*this, items);
}

LRESULT ShareMiscPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // [+]NightOrion
{
	fixControls();
	return 0;
}

void ShareMiscPage::fixControls()
{
	const BOOL state = IsDlgButtonChecked(IDC_TTH_IN_STREAM) != 0;
	::EnableWindow(GetDlgItem(IDC_CAPTION_MIN_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_MIN_FILE_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MB), state);
}
