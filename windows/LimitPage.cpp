#include "stdafx.h"
#include "Resource.h"
#include "LimitPage.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_CZDC_TRANSFER_LIMITING, ResourceManager::SETCZDC_TRANSFER_LIMITING },
	{ IDC_THROTTLE_ENABLE, ResourceManager::SETCZDC_ENABLE_LIMITING },
	{ IDC_CZDC_UP_SPEEED, ResourceManager::UPLOAD },
	{ IDC_CZDC_UP_SPEEED1, ResourceManager::UPLOAD },
	{ IDC_SETTINGS_KBPS1, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS2, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS3, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS4, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS5, ResourceManager::KBPS_DISABLE },
	{ IDC_SETTINGS_KBPS6, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS7, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS8, ResourceManager::KBPS },
	{ IDC_SETTINGS_MINUTES, ResourceManager::DATETIME_SECONDS },
	{ IDC_CZDC_DW_SPEEED, ResourceManager::DOWNLOAD },
	{ IDC_CZDC_DW_SPEEED1, ResourceManager::DOWNLOAD },
	{ IDC_TIME_LIMITING, ResourceManager::SETCZDC_ALTERNATE_LIMITING },
	{ IDC_CZDC_TO, ResourceManager::SETCZDC_TO },
	{ IDC_CZDC_SLOW_DISCONNECT, ResourceManager::SETCZDC_SLOW_DISCONNECT },
	{ IDC_SEGMENTED_ONLY, ResourceManager::SETTINGS_AUTO_DROP_SEGMENTED_SOURCE },
	{ IDC_CZDC_I_DOWN_SPEED, ResourceManager::SETCZDC_I_DOWN_SPEED },
	{ IDC_CZDC_TIME_DOWN, ResourceManager::SETCZDC_TIME_DOWN },
	{ IDC_CZDC_H_DOWN_SPEED, ResourceManager::SETCZDC_H_DOWN_SPEED },
	{ IDC_DISCONNECTING_ENABLE, ResourceManager::SETCZDC_DISCONNECTING_ENABLE },
	{ IDC_CZDC_MIN_FILE_SIZE, ResourceManager::SETCZDC_MIN_FILE_SIZE },
	{ IDC_SETTINGS_MB, ResourceManager::MB },
	{ IDC_REMOVE_IF, ResourceManager::NEW_DISCONNECT },
	{ IDC_PER_USER_LIMIT_ENABLE, ResourceManager::SET_PER_USER_UL_LIMIT },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	// [!] IRainman SpeedLimiter: to work correctly, you must first set the upload speed, and only then download speed!
	{ IDC_MX_UP_SP_LMT_NORMAL, SettingsManager::MAX_UPLOAD_SPEED_LIMIT_NORMAL, PropPage::T_INT },
	{ IDC_MX_DW_SP_LMT_NORMAL, SettingsManager::MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, PropPage::T_INT },
	{ IDC_TIME_LIMITING, SettingsManager::TIME_DEPENDENT_THROTTLE, PropPage::T_BOOL },
	{ IDC_MX_UP_SP_LMT_TIME, SettingsManager::MAX_UPLOAD_SPEED_LIMIT_TIME, PropPage::T_INT },
	{ IDC_MX_DW_SP_LMT_TIME, SettingsManager::MAX_DOWNLOAD_SPEED_LIMIT_TIME, PropPage::T_INT },
	{ IDC_BW_START_TIME, SettingsManager::BANDWIDTH_LIMIT_START, PropPage::T_INT },
	{ IDC_BW_END_TIME, SettingsManager::BANDWIDTH_LIMIT_END, PropPage::T_INT },
	{ IDC_THROTTLE_ENABLE, SettingsManager::THROTTLE_ENABLE, PropPage::T_BOOL },
	{ IDC_I_DOWN_SPEED, SettingsManager::AUTO_DISCONNECT_SPEED, PropPage::T_INT },
	{ IDC_TIME_DOWN, SettingsManager::AUTO_DISCONNECT_TIME, PropPage::T_INT },
	{ IDC_H_DOWN_SPEED, SettingsManager::AUTO_DISCONNECT_FILE_SPEED, PropPage::T_INT },
	{ IDC_DISCONNECTING_ENABLE, SettingsManager::ENABLE_AUTO_DISCONNECT, PropPage::T_BOOL },
	{ IDC_SEGMENTED_ONLY, SettingsManager::AUTO_DISCONNECT_MULTISOURCE_ONLY, PropPage::T_BOOL },
	{ IDC_MIN_FILE_SIZE, SettingsManager::AUTO_DISCONNECT_MIN_FILE_SIZE, PropPage::T_INT },
	{ IDC_REMOVE_IF_BELOW, SettingsManager::AUTO_DISCONNECT_REMOVE_SPEED, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

LRESULT LimitPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	
	CUpDownCtrl(GetDlgItem(IDC_I_DOWN_SPEED_SPIN)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_TIME_DOWN_SPIN)).SetRange32(1, 99999);
	CUpDownCtrl(GetDlgItem(IDC_H_DOWN_SPEED_SPIN)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_UPLOADSPEEDSPIN)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_DOWNLOADSPEEDSPIN)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_UPLOADSPEEDSPIN_TIME)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_DOWNLOADSPEEDSPIN_TIME)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_MIN_FILE_SIZE_SPIN)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_REMOVE_SPIN)).SetRange32(0, 99999);
	CUpDownCtrl(GetDlgItem(IDC_UPLOADSPEEDSPIN_USER)).SetRange32(0, 10240);
	
	timeCtrlBegin.Attach(GetDlgItem(IDC_BW_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_BW_END_TIME));
	
	WinUtil::fillTimeValues(timeCtrlBegin);
	WinUtil::fillTimeValues(timeCtrlEnd);
	
	timeCtrlBegin.SetCurSel(SETTING(BANDWIDTH_LIMIT_START));
	timeCtrlEnd.SetCurSel(SETTING(BANDWIDTH_LIMIT_END));
	
#ifndef FLYLINKDC_USE_DROP_SLOW
	GetDlgItem(IDC_DISCONNECTING_ENABLE).EnableWindow(FALSE);
	GetDlgItem(IDC_SEGMENTED_ONLY).EnableWindow(FALSE);
#endif

	int limit = SETTING(PER_USER_UPLOAD_SPEED_LIMIT);
	CButton(GetDlgItem(IDC_PER_USER_LIMIT_ENABLE)).SetCheck(limit > 0 ? BST_CHECKED : BST_UNCHECKED);
	CButton(GetDlgItem(IDC_UPLOADSPEED_USER)).SetWindowText(Util::toStringT(limit > 0 ? limit : 256).c_str());
	
	fixControls();
	
	return TRUE;
}

void LimitPage::write()
{
	PropPage::write(*this, items);
	
	g_settings->set(SettingsManager::BANDWIDTH_LIMIT_START, timeCtrlBegin.GetCurSel());
	g_settings->set(SettingsManager::BANDWIDTH_LIMIT_END, timeCtrlEnd.GetCurSel());

	int limit = IsDlgButtonChecked(IDC_PER_USER_LIMIT_ENABLE) ? GetDlgItemInt(IDC_UPLOADSPEED_USER) : 0;
	g_settings->set(SettingsManager::PER_USER_UPLOAD_SPEED_LIMIT, limit);
}

void LimitPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_THROTTLE_ENABLE) == BST_CHECKED;
	GetDlgItem(IDC_TIME_LIMITING).EnableWindow(state);
	
	state = IsDlgButtonChecked(IDC_THROTTLE_ENABLE) == BST_CHECKED && IsDlgButtonChecked(IDC_TIME_LIMITING) == BST_CHECKED;
	GetDlgItem(IDC_BW_START_TIME).EnableWindow(state);
	GetDlgItem(IDC_BW_END_TIME).EnableWindow(state);
	
	state = IsDlgButtonChecked(IDC_DISCONNECTING_ENABLE) == BST_CHECKED;
	GetDlgItem(IDC_I_DOWN_SPEED).EnableWindow(state);
	GetDlgItem(IDC_I_DOWN_SPEED_SPIN).EnableWindow(state);
	GetDlgItem(IDC_TIME_DOWN).EnableWindow(state);
	GetDlgItem(IDC_TIME_DOWN_SPIN).EnableWindow(state);
	GetDlgItem(IDC_H_DOWN_SPEED).EnableWindow(state);
	GetDlgItem(IDC_H_DOWN_SPEED_SPIN).EnableWindow(state);
	GetDlgItem(IDC_MIN_FILE_SIZE).EnableWindow(state);
	GetDlgItem(IDC_MIN_FILE_SIZE_SPIN).EnableWindow(state);
	GetDlgItem(IDC_REMOVE_IF_BELOW).EnableWindow(state);
	GetDlgItem(IDC_REMOVE_SPIN).EnableWindow(state);

	state = IsDlgButtonChecked(IDC_PER_USER_LIMIT_ENABLE) == BST_CHECKED;
	GetDlgItem(IDC_UPLOADSPEED_USER).EnableWindow(state);
}

LRESULT LimitPage::onChangeCont(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_TIME_LIMITING:
		case IDC_THROTTLE_ENABLE:
		case IDC_DISCONNECTING_ENABLE:
		case IDC_PER_USER_LIMIT_ENABLE:
			fixControls();
			break;
	}
	return TRUE;
}
