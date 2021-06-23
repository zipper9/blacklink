#include "stdafx.h"
#include "Resource.h"
#include "LimitPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { -2, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align align3 = { 20, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align4 = { 21, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align align5 = { 22, DialogLayout::SIDE_RIGHT, U_DU(30) };
static const DialogLayout::Align align6 = { 23, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align7 = { 24, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align align8 = { 26, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align9 = { 27, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align10 = { 28, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_DISCONNECTING_ENABLE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SEGMENTED_ONLY, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CZDC_I_DOWN_SPEED, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_CZDC_TIME_DOWN, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_CZDC_H_DOWN_SPEED, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_CZDC_MIN_FILE_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_REMOVE_IF, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_I_DOWN_SPEED, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_TIME_DOWN, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_H_DOWN_SPEED, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_MIN_FILE_SIZE, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_REMOVE_IF_BELOW, 0, UNSPEC, UNSPEC, 2, &align1 },
	{ IDC_SETTINGS_KBPS5, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_SETTINGS_MINUTES, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_SETTINGS_KBPS6, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_SETTINGS_MB, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_SETTINGS_KBPS7, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_CZDC_TRANSFER_LIMITING, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_THROTTLE_ENABLE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CZDC_DW_SPEEED, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_MX_DW_SP_LMT_NORMAL, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_SETTINGS_KBPS1, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align4 },
	{ IDC_CZDC_UP_SPEEED, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align5 },
	{ IDC_MX_UP_SP_LMT_NORMAL, 0, UNSPEC, UNSPEC, 0, &align6 },
	{ IDC_SETTINGS_KBPS2, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align7 },
	{ IDC_TIME_LIMITING, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_BW_START_TIME, 0, UNSPEC, UNSPEC, 0, &align8 },
	{ IDC_CZDC_TO, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align9 },
	{ IDC_BW_END_TIME, 0, UNSPEC, UNSPEC, 0, &align10 },
	{ IDC_CZDC_DW_SPEEED1, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_MX_DW_SP_LMT_TIME, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_SETTINGS_KBPS3, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align4 },
	{ IDC_CZDC_UP_SPEEED1, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align5 },
	{ IDC_MX_UP_SP_LMT_TIME, 0, UNSPEC, UNSPEC, 0, &align6 },
	{ IDC_SETTINGS_KBPS4, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align7 },
	{ IDC_PER_USER_LIMIT_ENABLE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_UPLOADSPEED_USER, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_SETTINGS_KBPS8, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align4 }
};

static const PropPage::Item items[] =
{
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
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items);

	CUpDownCtrl spin1(GetDlgItem(IDC_I_DOWN_SPEED_SPIN));
	spin1.SetRange32(0, 99999);
	spin1.SetBuddy(GetDlgItem(IDC_I_DOWN_SPEED));

	CUpDownCtrl spin2(GetDlgItem(IDC_TIME_DOWN_SPIN));
	spin2.SetRange32(1, 99999);
	spin2.SetBuddy(GetDlgItem(IDC_TIME_DOWN));

	CUpDownCtrl spin3(GetDlgItem(IDC_H_DOWN_SPEED_SPIN));
	spin3.SetRange32(0, 99999);
	spin3.SetBuddy(GetDlgItem(IDC_H_DOWN_SPEED));

	CUpDownCtrl spin4(GetDlgItem(IDC_UPLOADSPEEDSPIN));
	spin4.SetRange32(0, 99999);
	spin4.SetBuddy(GetDlgItem(IDC_MX_UP_SP_LMT_NORMAL));

	CUpDownCtrl spin5(GetDlgItem(IDC_DOWNLOADSPEEDSPIN));
	spin5.SetRange32(0, 99999);
	spin5.SetBuddy(GetDlgItem(IDC_MX_DW_SP_LMT_NORMAL));

	CUpDownCtrl spin6(GetDlgItem(IDC_UPLOADSPEEDSPIN_TIME));
	spin6.SetRange32(0, 99999);
	spin6.SetBuddy(GetDlgItem(IDC_MX_UP_SP_LMT_TIME));

	CUpDownCtrl spin7(GetDlgItem(IDC_DOWNLOADSPEEDSPIN_TIME));
	spin7.SetRange32(0, 99999);
	spin7.SetBuddy(GetDlgItem(IDC_MX_DW_SP_LMT_TIME));

	CUpDownCtrl spin8(GetDlgItem(IDC_MIN_FILE_SIZE_SPIN));
	spin8.SetRange32(0, 99999);
	spin8.SetBuddy(GetDlgItem(IDC_MIN_FILE_SIZE));

	CUpDownCtrl spin9(GetDlgItem(IDC_REMOVE_SPIN));
	spin9.SetRange32(0, 99999);
	spin9.SetBuddy(GetDlgItem(IDC_REMOVE_IF_BELOW));

	CUpDownCtrl spin10(GetDlgItem(IDC_UPLOADSPEEDSPIN_USER));
	spin10.SetRange32(0, 10240);
	spin10.SetBuddy(GetDlgItem(IDC_UPLOADSPEED_USER));

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
	
	state &= IsDlgButtonChecked(IDC_TIME_LIMITING) == BST_CHECKED;
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
	GetDlgItem(IDC_SEGMENTED_ONLY).EnableWindow(state);

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
