#include "stdafx.h"
#include "ProgressBarTab.h"
#include "DialogLayout.h"
#include "PropPage.h"
#include "BarShader.h"
#include "ColorUtil.h"
#include "../client/SearchManager.h"

static ProgressBarTab* instance;

#define CTLID_VALUE_RED   0x2C2
#define CTLID_VALUE_GREEN 0x2C3
#define CTLID_VALUE_BLUE  0x2C4

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 5,  DialogLayout::SIDE_RIGHT, U_DU(6)  };
static const DialogLayout::Align align2 = { 17, DialogLayout::SIDE_RIGHT, U_DU(6)  };
static const DialogLayout::Align align3 = { 18, DialogLayout::SIDE_RIGHT, U_DU(16) };
static const DialogLayout::Align align4 = { 19, DialogLayout::SIDE_RIGHT, U_DU(6)  };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SHOW_PROGRESSBARS,   FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_PROGRESSBAR_ODC,     FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_PROGRESS_BUMPED,     FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_PROGRESSBAR_CLASSIC, FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_CAPTION_DEPTH,       FLAG_TRANSLATE, AUTO,   UNSPEC             },
    { IDC_DEPTH,               0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_PROGRESS_OVERRIDE,   FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_PROGRESS_OVERRIDE2,  FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_SET_DL_BACKGROUND,   FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_SET_DL_TEXT,         FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_SET_DL_DEFAULT,      FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_SET_UL_BACKGROUND,   FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_SET_UL_TEXT,         FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_SET_UL_DEFAULT,      FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_SHOW_SPEED_ICON,     FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_USE_CUSTOM_SPEED,    FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_CAPTION_DL_SPEED,    FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_DL_SPEED,            0,              UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_CAPTION_UL_SPEED,    FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align3 },
	{ IDC_UL_SPEED,            0,              UNSPEC, UNSPEC, 0, &align4 }
};

static const PropPage::Item items[] =
{
	{ IDC_SHOW_PROGRESSBARS, SettingsManager::SHOW_PROGRESS_BARS, PropPage::T_BOOL },
	{ IDC_DL_SPEED, SettingsManager::TOP_DL_SPEED, PropPage::T_INT },
	{ IDC_UL_SPEED, SettingsManager::TOP_UL_SPEED, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

void ProgressBarTab::loadSettings()
{
	progressBackground[0] = g_settings->get(SettingsManager::DOWNLOAD_BAR_COLOR);
	progressBackground[1] = g_settings->get(SettingsManager::UPLOAD_BAR_COLOR);
	progressText[0] = g_settings->get(SettingsManager::PROGRESS_TEXT_COLOR_DOWN);
	progressText[1] = g_settings->get(SettingsManager::PROGRESS_TEXT_COLOR_UP);
	progressOdc = g_settings->getBool(SettingsManager::PROGRESSBAR_ODC_STYLE);
	progressBumped = g_settings->getBool(SettingsManager::PROGRESSBAR_ODC_BUMPED);
	progressDepth = g_settings->get(SettingsManager::PROGRESS_3DDEPTH);
	progressOverrideBackground = g_settings->getBool(SettingsManager::PROGRESS_OVERRIDE_COLORS);
	progressOverrideText = g_settings->getBool(SettingsManager::PROGRESS_OVERRIDE_COLORS2);
	speedIconEnable = g_settings->getBool(SettingsManager::STEALTHY_STYLE_ICO);
	speedIconCustom = g_settings->getBool(SettingsManager::STEALTHY_STYLE_ICO_SPEEDIGNORE);
}

void ProgressBarTab::saveSettings() const
{
	PropPage::write(m_hWnd, items);
	g_settings->set(SettingsManager::DOWNLOAD_BAR_COLOR, (int) progressBackground[0]);
	g_settings->set(SettingsManager::UPLOAD_BAR_COLOR, (int) progressBackground[1]);
	g_settings->set(SettingsManager::PROGRESS_TEXT_COLOR_DOWN, (int) progressText[0]);
	g_settings->set(SettingsManager::PROGRESS_TEXT_COLOR_UP, (int) progressText[1]);
	g_settings->set(SettingsManager::PROGRESSBAR_ODC_STYLE, progressOdc);
	g_settings->set(SettingsManager::PROGRESSBAR_ODC_BUMPED, progressBumped);
	g_settings->set(SettingsManager::PROGRESS_3DDEPTH, progressDepth);
	g_settings->set(SettingsManager::PROGRESS_OVERRIDE_COLORS, progressOverrideBackground);
	g_settings->set(SettingsManager::PROGRESS_OVERRIDE_COLORS2, progressOverrideText);
	g_settings->set(SettingsManager::STEALTHY_STYLE_ICO, speedIconEnable);
	g_settings->set(SettingsManager::STEALTHY_STYLE_ICO_SPEEDIGNORE, speedIconCustom);
}

void ProgressBarTab::getValues(SettingsStore& ss) const
{
	ss.setIntValue(SettingsManager::DOWNLOAD_BAR_COLOR, progressBackground[0]);
	ss.setIntValue(SettingsManager::UPLOAD_BAR_COLOR, progressBackground[1]);
	ss.setIntValue(SettingsManager::PROGRESS_TEXT_COLOR_DOWN, progressText[0]);
	ss.setIntValue(SettingsManager::PROGRESS_TEXT_COLOR_UP, progressText[1]);
	ss.setIntValue(SettingsManager::PROGRESSBAR_ODC_STYLE, progressOdc);
	ss.setIntValue(SettingsManager::PROGRESSBAR_ODC_BUMPED, progressBumped);
	ss.setIntValue(SettingsManager::PROGRESS_3DDEPTH, progressDepth);
	ss.setIntValue(SettingsManager::PROGRESS_OVERRIDE_COLORS, progressOverrideBackground);
	ss.setIntValue(SettingsManager::PROGRESS_OVERRIDE_COLORS2, progressOverrideText);
#if 0
	ss.setIntValue(SettingsManager::STEALTHY_STYLE_ICO, speedIconEnable);
	ss.setIntValue(SettingsManager::STEALTHY_STYLE_ICO_SPEEDIGNORE, speedIconCustom);
#endif
}

void ProgressBarTab::setValues(const SettingsStore& ss)
{
	int val;
	if (ss.getIntValue(SettingsManager::DOWNLOAD_BAR_COLOR, val)) progressBackground[0] = val;
	if (ss.getIntValue(SettingsManager::UPLOAD_BAR_COLOR, val)) progressBackground[1] = val;
	if (ss.getIntValue(SettingsManager::PROGRESS_TEXT_COLOR_DOWN, val)) progressText[0] = val;
	if (ss.getIntValue(SettingsManager::PROGRESS_TEXT_COLOR_UP, val)) progressText[1] = val;
	ss.getBoolValue(SettingsManager::PROGRESSBAR_ODC_STYLE, progressOdc);
	ss.getBoolValue(SettingsManager::PROGRESSBAR_ODC_BUMPED, progressBumped);
	if (ss.getIntValue(SettingsManager::PROGRESS_3DDEPTH, val) && val >= 1 && val <= 5) progressDepth = val;
	ss.getBoolValue(SettingsManager::PROGRESS_OVERRIDE_COLORS, progressOverrideBackground);
	ss.getBoolValue(SettingsManager::PROGRESS_OVERRIDE_COLORS2, progressOverrideText);
#if 0
	ss.getBoolValue(SettingsManager::STEALTHY_STYLE_ICO, speedIconEnable);
	ss.getBoolValue(SettingsManager::STEALTHY_STYLE_ICO_SPEEDIGNORE, speedIconCustom);
#endif
}

void ProgressBarTab::updateTheme()
{
	updateControls();
	GetDlgItem(IDC_PROGRESS_COLOR_DOWN_SHOW).Invalidate();
	GetDlgItem(IDC_PROGRESS_COLOR_UP_SHOW).Invalidate();
}

void ProgressBarTab::updateControls()
{
	initializing = true;
	PropPage::read(m_hWnd, items);
	CButton(GetDlgItem(IDC_PROGRESSBAR_ODC)).SetCheck(progressOdc ? BST_CHECKED : BST_UNCHECKED);
	CButton(GetDlgItem(IDC_PROGRESSBAR_CLASSIC)).SetCheck(progressOdc ? BST_UNCHECKED : BST_CHECKED);
	CButton(GetDlgItem(IDC_PROGRESS_BUMPED)).SetCheck(progressBumped ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemInt(IDC_DEPTH, progressDepth);
	CButton(GetDlgItem(IDC_PROGRESS_OVERRIDE)).SetCheck(progressOverrideBackground ? BST_CHECKED : BST_UNCHECKED);
	CButton(GetDlgItem(IDC_PROGRESS_OVERRIDE2)).SetCheck(progressOverrideText ? BST_CHECKED : BST_UNCHECKED);
	CButton(GetDlgItem(IDC_SHOW_SPEED_ICON)).SetCheck(speedIconEnable ? BST_CHECKED : BST_UNCHECKED);
	CButton(GetDlgItem(IDC_USE_CUSTOM_SPEED)).SetCheck(speedIconCustom ? BST_CHECKED : BST_UNCHECKED);
	setEnabled(g_settings->getBool(SettingsManager::SHOW_PROGRESS_BARS));
	initializing = false;
}

LRESULT ProgressBarTab::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	CUpDownCtrl spin(GetDlgItem(IDC_DEPTH_SPIN));
	spin.SetRange(1, 5);
	spin.SetBuddy(GetDlgItem(IDC_DEPTH));

	updateControls();
	return TRUE;
}

LRESULT ProgressBarTab::onSetDefaultColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int id, index;
	SettingsManager::IntSetting bgSetting, fgSetting;
	if (wID == IDC_SET_DL_DEFAULT)
	{
		index = 0;
		bgSetting = SettingsManager::DOWNLOAD_BAR_COLOR;
		fgSetting = SettingsManager::PROGRESS_TEXT_COLOR_DOWN;
		id = IDC_PROGRESS_COLOR_DOWN_SHOW;
	}
	else
	{
		index = 1;
		bgSetting = SettingsManager::UPLOAD_BAR_COLOR;
		fgSetting = SettingsManager::PROGRESS_TEXT_COLOR_UP;
		id = IDC_PROGRESS_COLOR_UP_SHOW;
	}
	bool update = false;
	COLORREF color = g_settings->getDefault(bgSetting);
	if (progressBackground[index] != color)
	{
		progressBackground[index] = color;
		if (callback) callback->settingChanged(bgSetting);
		update = true;
	}
	color = g_settings->getDefault(fgSetting);
	if (progressText[index] != color)
	{
		progressText[index] = color;
		if (callback) callback->settingChanged(fgSetting);
		update = true;
	}
	if (update) GetDlgItem(id).Invalidate();
	return 0;
}

LRESULT ProgressBarTab::onEnable(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	setEnabled(CButton(hWndCtl).GetCheck() == BST_CHECKED);
	return 0;
}

void ProgressBarTab::setEnabled(bool enable)
{
	static const UINT id[] =
	{
		IDC_PROGRESSBAR_ODC, IDC_PROGRESSBAR_CLASSIC, IDC_PROGRESS_OVERRIDE, IDC_SHOW_SPEED_ICON,
		IDC_PROGRESS_BUMPED, IDC_DEPTH, IDC_PROGRESS_OVERRIDE2,
		IDC_SET_DL_BACKGROUND, IDC_SET_DL_TEXT, IDC_SET_DL_DEFAULT,
		IDC_SET_UL_BACKGROUND, IDC_SET_UL_TEXT, IDC_SET_UL_DEFAULT,
		IDC_USE_CUSTOM_SPEED, IDC_DL_SPEED, IDC_UL_SPEED
	};
	if (enable)
	{
		for (int i = 0; i < 4; i++)
			GetDlgItem(id[i]).EnableWindow(TRUE);
		updateStyleOptions();
		updateSpeedIconOptions();
	}
	else
	{
		for (int i = 0; i < _countof(id); i++)
			GetDlgItem(id[i]).EnableWindow(FALSE);
	}
}

void ProgressBarTab::getStyleOptions()
{
	bool bval = IsDlgButtonChecked(IDC_PROGRESSBAR_ODC) == BST_CHECKED;
	if (progressOdc != bval)
	{
		progressOdc = bval;
		if (callback) callback->settingChanged(SettingsManager::PROGRESSBAR_ODC_STYLE);
	}
	bval = IsDlgButtonChecked(IDC_PROGRESS_BUMPED) == BST_CHECKED;
	if (progressBumped != bval)
	{
		progressBumped = bval;
		if (callback) callback->settingChanged(SettingsManager::PROGRESSBAR_ODC_BUMPED);
	}
	int ival = GetDlgItemInt(IDC_DEPTH);
	if (progressDepth != ival)
	{
		progressDepth = ival;
		if (callback) callback->settingChanged(SettingsManager::PROGRESS_3DDEPTH);
	}
	bval = IsDlgButtonChecked(IDC_PROGRESS_OVERRIDE) == BST_CHECKED;
	if (progressOverrideBackground != bval)
	{
		progressOverrideBackground = bval;
		if (callback) callback->settingChanged(SettingsManager::PROGRESS_OVERRIDE_COLORS);
	}
	bval = IsDlgButtonChecked(IDC_PROGRESS_OVERRIDE2) == BST_CHECKED;
	if (progressOverrideText != bval)
	{
		progressOverrideText = bval;
		if (callback) callback->settingChanged(SettingsManager::PROGRESS_OVERRIDE_COLORS2);
	}
}

void ProgressBarTab::updateStyleOptions()
{
	GetDlgItem(IDC_PROGRESS_BUMPED).EnableWindow(progressOdc);
	GetDlgItem(IDC_DEPTH).EnableWindow(!progressOdc);
	GetDlgItem(IDC_PROGRESS_OVERRIDE2).EnableWindow(progressOverrideBackground);
	if (progressOverrideBackground)
	{
		GetDlgItem(IDC_SET_DL_BACKGROUND).EnableWindow(TRUE);
		GetDlgItem(IDC_SET_DL_TEXT).EnableWindow(progressOverrideText);
		GetDlgItem(IDC_SET_DL_DEFAULT).EnableWindow(TRUE);
		GetDlgItem(IDC_SET_UL_BACKGROUND).EnableWindow(TRUE);
		GetDlgItem(IDC_SET_UL_TEXT).EnableWindow(progressOverrideText);
		GetDlgItem(IDC_SET_UL_DEFAULT).EnableWindow(TRUE);
	}
	else
	{
		GetDlgItem(IDC_SET_DL_BACKGROUND).EnableWindow(FALSE);
		GetDlgItem(IDC_SET_DL_TEXT).EnableWindow(FALSE);
		GetDlgItem(IDC_SET_DL_DEFAULT).EnableWindow(FALSE);
		GetDlgItem(IDC_SET_UL_BACKGROUND).EnableWindow(FALSE);
		GetDlgItem(IDC_SET_UL_TEXT).EnableWindow(FALSE);
		GetDlgItem(IDC_SET_UL_DEFAULT).EnableWindow(FALSE);
	}
}

LRESULT ProgressBarTab::onStyleOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!initializing)
	{
		getStyleOptions();
		updateStyleOptions();
		GetDlgItem(IDC_PROGRESS_COLOR_DOWN_SHOW).Invalidate();
		GetDlgItem(IDC_PROGRESS_COLOR_UP_SHOW).Invalidate();
	}
	return 0;
}

void ProgressBarTab::getSpeedIconOptions()
{
	speedIconEnable = IsDlgButtonChecked(IDC_SHOW_SPEED_ICON) == BST_CHECKED;
	speedIconCustom = IsDlgButtonChecked(IDC_USE_CUSTOM_SPEED) == BST_CHECKED;
}

void ProgressBarTab::updateSpeedIconOptions()
{
	GetDlgItem(IDC_USE_CUSTOM_SPEED).EnableWindow(speedIconEnable);
	GetDlgItem(IDC_DL_SPEED).EnableWindow(speedIconEnable && speedIconCustom);
	GetDlgItem(IDC_UL_SPEED).EnableWindow(speedIconEnable && speedIconCustom);
}

LRESULT ProgressBarTab::onSpeedIconOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!initializing)
	{
		getSpeedIconOptions();
		updateSpeedIconOptions();
	}
	return 0;
}

LRESULT ProgressBarTab::onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if (dis->CtlType == ODT_STATIC)
	{
		if (dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW || dis->CtlID == IDC_PROGRESS_COLOR_UP_SHOW)
		{
			CDCHandle dc(dis->hDC);
			CRect rc(dis->rcItem);
			int index = dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW ? 0 : 1;
			COLORREF clr = progressBackground[index];
			COLORREF clrText = progressText[index];
			if (!progressOverrideBackground)
				clr = GetSysColor(COLOR_HIGHLIGHT);
			if (!progressOverrideBackground || !progressOverrideText)
				clrText = ColorUtil::textFromBackground(clr);
			if (progressOdc)
			{
				COLORREF a, b;
				OperaColors::EnlightenFlood(clr, a, b);
				OperaColors::FloodFill(dc, rc.left, rc.top, rc.right, rc.bottom, a, b, progressBumped);
			}
			else
			{
				CBarShader shader(rc.bottom - rc.top, rc.right - rc.left);
				shader.SetFileSize(16);
				shader.FillRange(0, 16, clr);
				shader.Draw(dc, rc.top, rc.left, progressDepth);
			}
			dc.SetTextColor(clrText);
			dc.DrawText(
				dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW ? CTSTRING(STYLES_DOWNLOAD_BAR) : CTSTRING(STYLES_UPLOAD_BAR),
				-1, rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
		}
		else
			bHandled = FALSE;
	}
	else
		bHandled = FALSE;
	return 0;
}

static inline int clamp(int val)
{
	if (val < 0) return 0;
	if (val > 255) return 255;
	return val;
}

UINT_PTR CALLBACK ProgressBarTab::hookProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_COMMAND)
	{
		if (HIWORD(wParam) == EN_CHANGE &&
		    (LOWORD(wParam) == CTLID_VALUE_RED || LOWORD(wParam) == CTLID_VALUE_GREEN || LOWORD(wParam) == CTLID_VALUE_BLUE))
		{
			int r = clamp(::GetDlgItemInt(hWnd, CTLID_VALUE_RED, nullptr, TRUE));
			int g = clamp(::GetDlgItemInt(hWnd, CTLID_VALUE_GREEN, nullptr, TRUE));
			int b = clamp(::GetDlgItemInt(hWnd, CTLID_VALUE_BLUE, nullptr, TRUE));
			*instance->selColor = RGB(r, g, b);
			::InvalidateRect(instance->selHwnd, nullptr, TRUE);
		}
	}
	return 0;
}

LRESULT ProgressBarTab::onChooseColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int id;
	switch (wID)
	{
		case IDC_SET_DL_BACKGROUND:
			selColor = &progressBackground[0];
			selHwnd = GetDlgItem(IDC_PROGRESS_COLOR_DOWN_SHOW);
			id = SettingsManager::DOWNLOAD_BAR_COLOR;
			break;
		case IDC_SET_UL_BACKGROUND:
			selColor = &progressBackground[1];
			selHwnd = GetDlgItem(IDC_PROGRESS_COLOR_UP_SHOW);
			id = SettingsManager::UPLOAD_BAR_COLOR;
			break;
		case IDC_SET_DL_TEXT:
			selColor = &progressText[0];
			selHwnd = GetDlgItem(IDC_PROGRESS_COLOR_DOWN_SHOW);
			id = SettingsManager::PROGRESS_TEXT_COLOR_DOWN;
			break;
		case IDC_SET_UL_TEXT:
			selColor = &progressText[1];
			selHwnd = GetDlgItem(IDC_PROGRESS_COLOR_UP_SHOW);
			id = SettingsManager::PROGRESS_TEXT_COLOR_UP;
			break;
		default:
			return 0;
	}
	COLORREF savedColor = *selColor;
	CColorDialog d(savedColor, CC_FULLOPEN | CC_SOLIDCOLOR, m_hWnd);
	d.m_cc.lpfnHook = hookProc;
	instance = this;
	if (d.DoModal() == IDOK)
	{
		if (savedColor != d.GetColor())
		{
			*selColor = d.GetColor();
			if (callback)
				callback->settingChanged(id);
		}
	}
	else
		*selColor = savedColor;
	instance = nullptr;
	::InvalidateRect(selHwnd, nullptr, TRUE);
	selHwnd = nullptr;
	return 0;
}
