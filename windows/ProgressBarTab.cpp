#include "stdafx.h"
#include "ProgressBarTab.h"
#include "DialogLayout.h"
#include "PropPage.h"
#include "Colors.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/Random.h"
#include "../client/StrUtil.h"

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
	{ IDC_SHOW_PROGRESSBARS, Conf::SHOW_PROGRESS_BARS, PropPage::T_BOOL },
	{ IDC_DL_SPEED, Conf::TOP_DL_SPEED, PropPage::T_INT },
	{ IDC_UL_SPEED, Conf::TOP_UL_SPEED, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

void ProgressBarTab::loadSettings(const BaseSettingsImpl* ss)
{
	progressBackground[0] = ss->getInt(Conf::DOWNLOAD_BAR_COLOR);
	progressBackground[1] = ss->getInt(Conf::UPLOAD_BAR_COLOR);
	progressText[0] = ss->getInt(Conf::PROGRESS_TEXT_COLOR_DOWN);
	progressText[1] = ss->getInt(Conf::PROGRESS_TEXT_COLOR_UP);
	progressOdc = ss->getBool(Conf::PROGRESSBAR_ODC_STYLE);
	progressBumped = ss->getBool(Conf::PROGRESSBAR_ODC_BUMPED);
	progressDepth = ss->getInt(Conf::PROGRESS_3DDEPTH);
	progressOverrideBackground = ss->getBool(Conf::PROGRESS_OVERRIDE_COLORS);
	progressOverrideText = ss->getBool(Conf::PROGRESS_OVERRIDE_COLORS2);
	speedIconEnable = ss->getBool(Conf::STEALTHY_STYLE_ICO);
	speedIconCustom = ss->getBool(Conf::STEALTHY_STYLE_ICO_SPEEDIGNORE);
	windowBackground = ss->getInt(Conf::BACKGROUND_COLOR);
	emptyBarBackground = ss->getInt(Conf::PROGRESS_BACK_COLOR);
}

void ProgressBarTab::saveSettings(BaseSettingsImpl* ss) const
{
	PropPage::write(m_hWnd, items);
	ss->setInt(Conf::DOWNLOAD_BAR_COLOR, (int) progressBackground[0]);
	ss->setInt(Conf::UPLOAD_BAR_COLOR, (int) progressBackground[1]);
	ss->setInt(Conf::PROGRESS_TEXT_COLOR_DOWN, (int) progressText[0]);
	ss->setInt(Conf::PROGRESS_TEXT_COLOR_UP, (int) progressText[1]);
	ss->setBool(Conf::PROGRESSBAR_ODC_STYLE, progressOdc);
	ss->setBool(Conf::PROGRESSBAR_ODC_BUMPED, progressBumped);
	ss->setInt(Conf::PROGRESS_3DDEPTH, progressDepth);
	ss->setBool(Conf::PROGRESS_OVERRIDE_COLORS, progressOverrideBackground);
	ss->setBool(Conf::PROGRESS_OVERRIDE_COLORS2, progressOverrideText);
	ss->setBool(Conf::STEALTHY_STYLE_ICO, speedIconEnable);
	ss->setBool(Conf::STEALTHY_STYLE_ICO_SPEEDIGNORE, speedIconCustom);
}

void ProgressBarTab::getValues(SettingsStore& ss) const
{
	ss.setIntValue(Conf::DOWNLOAD_BAR_COLOR, progressBackground[0]);
	ss.setIntValue(Conf::UPLOAD_BAR_COLOR, progressBackground[1]);
	ss.setIntValue(Conf::PROGRESS_TEXT_COLOR_DOWN, progressText[0]);
	ss.setIntValue(Conf::PROGRESS_TEXT_COLOR_UP, progressText[1]);
	ss.setIntValue(Conf::PROGRESSBAR_ODC_STYLE, progressOdc);
	ss.setIntValue(Conf::PROGRESSBAR_ODC_BUMPED, progressBumped);
	ss.setIntValue(Conf::PROGRESS_3DDEPTH, progressDepth);
	ss.setIntValue(Conf::PROGRESS_OVERRIDE_COLORS, progressOverrideBackground);
	ss.setIntValue(Conf::PROGRESS_OVERRIDE_COLORS2, progressOverrideText);
#if 0
	ss.setIntValue(Conf::STEALTHY_STYLE_ICO, speedIconEnable);
	ss.setIntValue(Conf::STEALTHY_STYLE_ICO_SPEEDIGNORE, speedIconCustom);
#endif
}

void ProgressBarTab::setValues(const SettingsStore& ss)
{
	int val;
	if (ss.getIntValue(Conf::DOWNLOAD_BAR_COLOR, val)) progressBackground[0] = val;
	if (ss.getIntValue(Conf::UPLOAD_BAR_COLOR, val)) progressBackground[1] = val;
	if (ss.getIntValue(Conf::PROGRESS_TEXT_COLOR_DOWN, val)) progressText[0] = val;
	if (ss.getIntValue(Conf::PROGRESS_TEXT_COLOR_UP, val)) progressText[1] = val;
	ss.getBoolValue(Conf::PROGRESSBAR_ODC_STYLE, progressOdc);
	ss.getBoolValue(Conf::PROGRESSBAR_ODC_BUMPED, progressBumped);
	if (ss.getIntValue(Conf::PROGRESS_3DDEPTH, val) && val >= 1 && val <= 5) progressDepth = val;
	ss.getBoolValue(Conf::PROGRESS_OVERRIDE_COLORS, progressOverrideBackground);
	ss.getBoolValue(Conf::PROGRESS_OVERRIDE_COLORS2, progressOverrideText);
#if 0
	ss.getBoolValue(Conf::STEALTHY_STYLE_ICO, speedIconEnable);
	ss.getBoolValue(Conf::STEALTHY_STYLE_ICO_SPEEDIGNORE, speedIconCustom);
#endif
	if (ss.getIntValue(Conf::BACKGROUND_COLOR, val)) windowBackground = val;
	if (ss.getIntValue(Conf::PROGRESS_BACK_COLOR, val)) emptyBarBackground = val;
}

void ProgressBarTab::redrawBars()
{
	GetDlgItem(IDC_PROGRESS_COLOR_DOWN_SHOW).Invalidate();
	GetDlgItem(IDC_PROGRESS_COLOR_UP_SHOW).Invalidate();
}

void ProgressBarTab::updateTheme()
{
	updateControls();
	applySettings(0, true);
	applySettings(1, true);
	redrawBars();
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
	const auto ss = SettingsManager::instance.getUiSettings();
	setEnabled(ss->getBool(Conf::SHOW_PROGRESS_BARS));
	initializing = false;
}

void ProgressBarTab::setBackgroundColor(COLORREF clr)
{
	if (windowBackground != clr)
	{
		windowBackground = clr;
		progress[0].setWindowBackground(windowBackground);
		progress[1].setWindowBackground(windowBackground);
		if (progressOdc) redrawBars();
	}
}

void ProgressBarTab::setEmptyBarBackground(COLORREF clr)
{
	if (emptyBarBackground != clr)
	{
		emptyBarBackground = clr;
		progress[0].setEmptyBarBackground(emptyBarBackground);
		progress[1].setEmptyBarBackground(emptyBarBackground);
		if (!progressOdc) redrawBars();
	}
}

void ProgressBarTab::applySettings(int index, bool check)
{
	ProgressBar::Settings settings;
	settings.odcStyle = progressOdc;
	settings.odcBumped = progressBumped;
	settings.setTextColor = progressOverrideText;
	settings.depth = progressDepth;
	settings.clrBackground = progressOverrideBackground ? progressBackground[index] : GetSysColor(COLOR_HIGHLIGHT);
	settings.clrText = progressText[index];
	settings.clrEmptyBackground = emptyBarBackground;
	if (!check || progress[index].get() != settings)
		progress[index].set(settings);
	progress[index].setWindowBackground(windowBackground);
}

void ProgressBarTab::notifyColorChange(int id, int value)
{
	if (callback)
	{
		callback->settingChanged(id);
		callback->intSettingChanged(id, value);
	}
}

LRESULT ProgressBarTab::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	CUpDownCtrl spin(GetDlgItem(IDC_DEPTH_SPIN));
	spin.SetRange(1, 5);
	spin.SetBuddy(GetDlgItem(IDC_DEPTH));

	updateControls();
	applySettings(0, false);
	applySettings(1, false);
	return TRUE;
}

LRESULT ProgressBarTab::onSetDefaultColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int id, index;
	int bgSetting, fgSetting;
	if (wID == IDC_SET_DL_DEFAULT)
	{
		index = 0;
		bgSetting = Conf::DOWNLOAD_BAR_COLOR;
		fgSetting = Conf::PROGRESS_TEXT_COLOR_DOWN;
		id = IDC_PROGRESS_COLOR_DOWN_SHOW;
	}
	else
	{
		index = 1;
		bgSetting = Conf::UPLOAD_BAR_COLOR;
		fgSetting = Conf::PROGRESS_TEXT_COLOR_UP;
		id = IDC_PROGRESS_COLOR_UP_SHOW;
	}
	bool update = false;
	const auto ss = SettingsManager::instance.getUiSettings();
	COLORREF color = ss->getIntDefault(bgSetting);
	if (progressBackground[index] != color)
	{
		progressBackground[index] = color;
		notifyColorChange(bgSetting, color);
		update = true;
	}
	color = ss->getIntDefault(fgSetting);
	if (progressText[index] != color)
	{
		progressText[index] = color;
		notifyColorChange(fgSetting, color);
		update = true;
	}
	if (update)
	{
		applySettings(index, false);
		GetDlgItem(id).Invalidate();
	}
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
		if (callback) callback->settingChanged(Conf::PROGRESSBAR_ODC_STYLE);
	}
	bval = IsDlgButtonChecked(IDC_PROGRESS_BUMPED) == BST_CHECKED;
	if (progressBumped != bval)
	{
		progressBumped = bval;
		if (callback) callback->settingChanged(Conf::PROGRESSBAR_ODC_BUMPED);
	}
	int ival = GetDlgItemInt(IDC_DEPTH);
	if (progressDepth != ival)
	{
		progressDepth = ival;
		if (callback) callback->settingChanged(Conf::PROGRESS_3DDEPTH);
	}
	bval = IsDlgButtonChecked(IDC_PROGRESS_OVERRIDE) == BST_CHECKED;
	if (progressOverrideBackground != bval)
	{
		progressOverrideBackground = bval;
		if (callback) callback->settingChanged(Conf::PROGRESS_OVERRIDE_COLORS);
	}
	bval = IsDlgButtonChecked(IDC_PROGRESS_OVERRIDE2) == BST_CHECKED;
	if (progressOverrideText != bval)
	{
		progressOverrideText = bval;
		if (callback) callback->settingChanged(Conf::PROGRESS_OVERRIDE_COLORS2);
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
		applySettings(0, true);
		applySettings(1, true);
		redrawBars();
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
		bool prevEnable = speedIconEnable;
		getSpeedIconOptions();
		updateSpeedIconOptions();
		if (speedIconEnable != prevEnable)
			redrawBars();
	}
	return 0;
}

static inline int getProportionalWidth(int width, int pos, int size)
{
	if (pos >= size || !size) return width;
	return width*pos/size;
}

LRESULT ProgressBarTab::onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if (dis->CtlType == ODT_STATIC)
	{
		if (dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW || dis->CtlID == IDC_PROGRESS_COLOR_UP_SHOW)
		{
			const int width = dis->rcItem.right - dis->rcItem.left;
			const int height = dis->rcItem.bottom - dis->rcItem.top;
			CDC dc;
			dc.CreateCompatibleDC(dis->hDC);
			HBITMAP hBmp = CreateCompatibleBitmap(dis->hDC, width, height);
			HBITMAP oldBmp = dc.SelectBitmap(hBmp);
			int index = dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW ? 0 : 1;
			tstring text = dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW ? TSTRING(STYLES_DOWNLOAD_BAR) : TSTRING(STYLES_UPLOAD_BAR);
			text += _T(" (");
			text += Util::toStringT(percent[index]);
			text += _T("%)");
			progress[index].draw(dc, dis->rcItem, getProportionalWidth(width - (progressOdc ? 2 : 0), percent[index], 100),
				text, speedIconEnable ? speedIcon[index] : -1);
			BitBlt(dis->hDC, dis->rcItem.left, dis->rcItem.top, width, height, dc, 0, 0, SRCCOPY);
			dc.SelectBitmap(oldBmp);
			DeleteObject(hBmp);
		}
		else
			bHandled = FALSE;
	}
	else
		bHandled = FALSE;
	return 0;
}

LRESULT ProgressBarTab::onClickProgress(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	int index = wID == IDC_PROGRESS_COLOR_DOWN_SHOW ? 0 : 1;
	percent[index] = Util::rand(101);
	speedIcon[index] = Util::rand(5);
	CWindow(hWndCtl).Invalidate();
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
			instance->applySettings(instance->selIndex, true);
			int idc = instance->selIndex == 0 ? IDC_PROGRESS_COLOR_DOWN_SHOW : IDC_PROGRESS_COLOR_UP_SHOW;
			instance->GetDlgItem(idc).Invalidate();
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
			selIndex = 0;
			id = Conf::DOWNLOAD_BAR_COLOR;
			break;
		case IDC_SET_UL_BACKGROUND:
			selColor = &progressBackground[1];
			selIndex = 1;
			id = Conf::UPLOAD_BAR_COLOR;
			break;
		case IDC_SET_DL_TEXT:
			selColor = &progressText[0];
			selIndex = 0;
			id = Conf::PROGRESS_TEXT_COLOR_DOWN;
			break;
		case IDC_SET_UL_TEXT:
			selColor = &progressText[1];
			selIndex = 1;
			id = Conf::PROGRESS_TEXT_COLOR_UP;
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
			notifyColorChange(id, *selColor);
		}
	}
	else
		*selColor = savedColor;
	instance = nullptr;
	applySettings(selIndex, true);
	int idc = selIndex == 0 ? IDC_PROGRESS_COLOR_DOWN_SHOW : IDC_PROGRESS_COLOR_UP_SHOW;
	GetDlgItem(idc).Invalidate();
	return 0;
}
