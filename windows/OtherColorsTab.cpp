#include "stdafx.h"
#include "OtherColorsTab.h"
#include "DialogLayout.h"
#include "BarShader.h"
#include "ColorUtil.h"
#include "Fonts.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/Text.h"

static OtherColorsTab* instance;

#define CTLID_VALUE_RED   0x2C2
#define CTLID_VALUE_GREEN 0x2C3
#define CTLID_VALUE_BLUE  0x2C4

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CHANGE_COLOR,                FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SET_DEFAULT,                 FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_USE_CUSTOM_MENU,             FLAG_TRANSLATE, AUTO,   UNSPEC },
	{ IDC_SETTINGS_ODC_MENUBAR_USETWO, FLAG_TRANSLATE, AUTO,   UNSPEC },
	{ IDC_SETTINGS_ODC_MENUBAR_BUMPED, FLAG_TRANSLATE, AUTO,   UNSPEC },
	{ IDC_SETTINGS_ODC_MENUBAR_LEFT,   FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SETTINGS_ODC_MENUBAR_RIGHT,  FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_MENU_SET_DEFAULT,            FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

struct ColorSettings
{
	ResourceManager::Strings name;
	int setting;
	COLORREF value;
};

static ColorSettings colors[] =
{
	{ ResourceManager::COLOR_WND,                  Conf::BACKGROUND_COLOR,         0 },
	{ ResourceManager::COLOR_TEXT,                 Conf::TEXT_COLOR,               0 },
	{ ResourceManager::COLOR_ERROR,                Conf::ERROR_COLOR,              0 },
	// Progress bars
	{ ResourceManager::PROGRESS_BACK,              Conf::PROGRESS_BACK_COLOR,      0 },
	{ ResourceManager::PROGRESS_SEGMENT,           Conf::PROGRESS_SEGMENT_COLOR,   0 },
	{ ResourceManager::PROGRESS_DOWNLOADED,        Conf::COLOR_DOWNLOADED,         0 },
	{ ResourceManager::PROGRESS_RUNNING,           Conf::COLOR_RUNNING,            0 },
	{ ResourceManager::PROGRESS_RUNNING_COMPLETED, Conf::COLOR_RUNNING_COMPLETED,  0 },
	// Tabs
	{ ResourceManager::TABS_INACTIVE_BACKGROUND_COLOR,       Conf::TABS_INACTIVE_BACKGROUND_COLOR,       0 },
	{ ResourceManager::TABS_ACTIVE_BACKGROUND_COLOR,         Conf::TABS_ACTIVE_BACKGROUND_COLOR,         0 },
	{ ResourceManager::TABS_INACTIVE_TEXT_COLOR,             Conf::TABS_INACTIVE_TEXT_COLOR,             0 },
	{ ResourceManager::TABS_ACTIVE_TEXT_COLOR,               Conf::TABS_ACTIVE_TEXT_COLOR,               0 },
	{ ResourceManager::TABS_OFFLINE_BACKGROUND_COLOR,        Conf::TABS_OFFLINE_BACKGROUND_COLOR,        0 },
	{ ResourceManager::TABS_OFFLINE_ACTIVE_BACKGROUND_COLOR, Conf::TABS_OFFLINE_ACTIVE_BACKGROUND_COLOR, 0 },
	{ ResourceManager::TABS_UPDATED_BACKGROUND_COLOR,        Conf::TABS_UPDATED_BACKGROUND_COLOR,        0 },
	{ ResourceManager::TABS_BORDER_COLOR,                    Conf::TABS_BORDER_COLOR,                    0 },
	// File status
	{ ResourceManager::COLOR_SHARED,               Conf::FILE_SHARED_COLOR,        0 },
	{ ResourceManager::COLOR_DOWNLOADED,           Conf::FILE_DOWNLOADED_COLOR,    0 },
	{ ResourceManager::COLOR_CANCELED,             Conf::FILE_CANCELED_COLOR,      0 },
	{ ResourceManager::COLOR_FOUND,                Conf::FILE_FOUND_COLOR,         0 },
	{ ResourceManager::COLOR_QUEUED,               Conf::FILE_QUEUED_COLOR,        0 }
};

struct MenuOption
{
	int idc;
	int setting;
	bool OtherColorsTab::*ptr;
};

static const MenuOption menuOptions[] =
{
	{ IDC_USE_CUSTOM_MENU,             Conf::USE_CUSTOM_MENU,    &OtherColorsTab::useCustomMenu },
	{ IDC_SETTINGS_ODC_MENUBAR_USETWO, Conf::MENUBAR_TWO_COLORS, &OtherColorsTab::menuTwoColors },
	{ IDC_SETTINGS_ODC_MENUBAR_BUMPED, Conf::MENUBAR_BUMPED,     &OtherColorsTab::menuBumped    }
};

void OtherColorsTab::loadSettings(const BaseSettingsImpl* ss)
{
	for (int i = 0; i < _countof(colors); i++)
		colors[i].value = ss->getInt(colors[i].setting);
	menuLeftColor = ss->getInt(Conf::MENUBAR_LEFT_COLOR);
	menuRightColor = ss->getInt(Conf::MENUBAR_RIGHT_COLOR);
	menuTwoColors = ss->getBool(Conf::MENUBAR_TWO_COLORS);
	menuBumped = ss->getBool(Conf::MENUBAR_BUMPED);
	useCustomMenu = ss->getBool(Conf::USE_CUSTOM_MENU);
}

void OtherColorsTab::saveSettings(BaseSettingsImpl* ss) const
{
	for (int i = 0; i < _countof(colors); i++)
		ss->setInt(colors[i].setting, (int) colors[i].value);
	ss->setInt(Conf::MENUBAR_LEFT_COLOR, (int) menuLeftColor);
	ss->setInt(Conf::MENUBAR_RIGHT_COLOR, (int) menuRightColor);
	ss->setBool(Conf::MENUBAR_TWO_COLORS, menuTwoColors);
	ss->setBool(Conf::MENUBAR_BUMPED, menuBumped);
	ss->setBool(Conf::USE_CUSTOM_MENU, useCustomMenu);
}

void OtherColorsTab::getValues(SettingsStore& ss) const
{
	for (int i = 0; i < _countof(colors); i++)
		ss.setIntValue(colors[i].setting, colors[i].value);
	ss.setIntValue(Conf::MENUBAR_LEFT_COLOR, menuLeftColor);
	ss.setIntValue(Conf::MENUBAR_RIGHT_COLOR, menuRightColor);
	ss.setIntValue(Conf::MENUBAR_TWO_COLORS, menuTwoColors);
	ss.setIntValue(Conf::MENUBAR_BUMPED, menuBumped);
	ss.setIntValue(Conf::USE_CUSTOM_MENU, useCustomMenu);
}

void OtherColorsTab::setValues(const SettingsStore& ss)
{
	int val;
	for (int i = 0; i < _countof(colors); i++)
		if (ss.getIntValue(colors[i].setting, val)) colors[i].value = val;
	if (ss.getIntValue(Conf::MENUBAR_LEFT_COLOR, val))
		menuLeftColor = val;
	if (ss.getIntValue(Conf::MENUBAR_RIGHT_COLOR, val))
		menuRightColor = val;
	ss.getBoolValue(Conf::MENUBAR_TWO_COLORS, menuTwoColors);
	ss.getBoolValue(Conf::MENUBAR_BUMPED, menuBumped);
	ss.getBoolValue(Conf::USE_CUSTOM_MENU, useCustomMenu);
}

void OtherColorsTab::updateTheme()
{
	showCurrentColor();
	for (int i = 0; i < _countof(menuOptions); i++)
		CButton(GetDlgItem(menuOptions[i].idc)).SetCheck(this->*menuOptions[i].ptr ? BST_CHECKED : BST_UNCHECKED);
	updateMenuOptions();
	ctrlMenubar.Invalidate();
}

void OtherColorsTab::setColor(int index, COLORREF color)
{
	if (colors[index].value != color)
	{
		colors[index].value = color;
		if (ctrlTabList.GetCurSel() == index)
			setCurrentColor(color);
	}
}

void OtherColorsTab::showCurrentColor()
{
	COLORREF color = colors[ctrlTabList.GetCurSel()].value;
	setCurrentColor(color);
}

LRESULT OtherColorsTab::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	ctrlSample.Attach(GetDlgItem(IDC_SAMPLE_TAB_COLOR));

	if (hFont) ::DeleteObject(hFont);
	LOGFONT lf;
	GetObject(Fonts::g_systemFont, sizeof(lf), &lf);
	lf.lfHeight = -14;
	hFont = CreateFontIndirect(&lf);

	ctrlTabList.Attach(GetDlgItem(IDC_TABCOLOR_LIST));
	ctrlTabList.ResetContent();
	for (int i = 0; i < _countof(colors); i++)
		ctrlTabList.AddString(Text::toT(ResourceManager::getString(colors[i].name)).c_str());

	ctrlTabList.SetCurSel(0);
	showCurrentColor();

	ctrlMenubar.Attach(GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_COLOR));
	for (int i = 0; i < _countof(menuOptions); i++)
		CButton(GetDlgItem(menuOptions[i].idc)).SetCheck(this->*menuOptions[i].ptr ? BST_CHECKED : BST_UNCHECKED);
	updateMenuOptions();

	return TRUE;
}

void OtherColorsTab::setCurrentColor(COLORREF cr)
{
	currentColor = cr;
	HBRUSH newBrush = CreateSolidBrush(cr);
	if (hBrush) DeleteObject(hBrush);
	hBrush = newBrush;
	ctrlSample.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASENOW | RDW_UPDATENOW | RDW_FRAME);
}

void OtherColorsTab::cleanup()
{
	if (hBrush)
	{
		DeleteObject(hBrush);
		hBrush = nullptr;
	}
	if (hFont)
	{
		DeleteObject(hFont);
		hFont = nullptr;
	}
}

LRESULT OtherColorsTab::onSetDefaultColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const auto ss = SettingsManager::instance.getUiSettings();
	int index = ctrlTabList.GetCurSel();
	COLORREF color = ss->getIntDefault(colors[index].setting);
	if (colors[index].value != color)
	{
		colors[index].value = color;
		setCurrentColor(colors[index].value);
		if (callback)
		{
			callback->settingChanged(colors[index].setting);
			callback->intSettingChanged(colors[index].setting, colors[index].value);
		}
	}
	return 0;
}

LRESULT OtherColorsTab::onChooseColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CColorDialog d(colors[ctrlTabList.GetCurSel()].value, CC_FULLOPEN, *this);
	if (d.DoModal() == IDOK)
	{
		int index = ctrlTabList.GetCurSel();
		if (colors[index].value != d.GetColor())
		{
			colors[index].value = d.GetColor();
			setCurrentColor(d.GetColor());
			if (callback)
			{
				callback->settingChanged(colors[index].setting);
				callback->intSettingChanged(colors[index].setting, colors[index].value);
			}
		}
	}
	return 0;
}

LRESULT OtherColorsTab::onMenuDefaults(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const auto ss = SettingsManager::instance.getUiSettings();
	COLORREF color = ss->getIntDefault(Conf::MENUBAR_LEFT_COLOR);
	if (color != menuLeftColor)
	{
		menuLeftColor = color;
		if (callback) callback->settingChanged(Conf::MENUBAR_LEFT_COLOR);
	}
	color = ss->getIntDefault(Conf::MENUBAR_RIGHT_COLOR);
	if (color != menuRightColor)
	{
		menuRightColor = color;
		if (callback) callback->settingChanged(Conf::MENUBAR_RIGHT_COLOR);
	}
	for (int i = 0; i < _countof(menuOptions); i++)
	{
		bool bval = ss->getIntDefault(menuOptions[i].setting) != 0;
		if (this->*menuOptions[i].ptr != bval)
		{
			this->*menuOptions[i].ptr = bval;
			CButton(GetDlgItem(menuOptions[i].idc)).SetCheck(bval ? BST_CHECKED : BST_UNCHECKED);
			if (callback)
				callback->settingChanged(menuOptions[i].setting);
		}
	}
	updateMenuOptions();
	ctrlMenubar.Invalidate();
	return 0;
}

void OtherColorsTab::updateMenuOptions()
{
	GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_USETWO).EnableWindow(useCustomMenu);
	GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_BUMPED).EnableWindow(useCustomMenu && menuTwoColors);
	GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_LEFT).EnableWindow(useCustomMenu);
	GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_RIGHT).EnableWindow(useCustomMenu && menuTwoColors);
	GetDlgItem(IDC_MENU_SET_DEFAULT).EnableWindow(useCustomMenu);
}

LRESULT OtherColorsTab::onMenuOption(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	for (int i = 0; i < _countof(menuOptions); i++)
		if (menuOptions[i].idc == wID)
		{
			bool newVal = CButton(hWndCtl).GetCheck() == BST_CHECKED;
			if (this->*menuOptions[i].ptr != newVal)
			{
				this->*menuOptions[i].ptr = newVal;
				if (callback)
					callback->settingChanged(menuOptions[i].setting);
			}
			updateMenuOptions();
			ctrlMenubar.Invalidate();
			break;
		}
	return 0;
}

LRESULT OtherColorsTab::onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if (dis->CtlType == ODT_STATIC)
	{
		if (dis->CtlID == IDC_SAMPLE_TAB_COLOR)
		{
			unsigned r = GetRValue(currentColor);
			unsigned g = GetGValue(currentColor);
			unsigned b = GetBValue(currentColor);
			TCHAR buf[64];
			_sntprintf(buf, _countof(buf), _T("#%02X%02X%02X"), r, g, b);
			CDCHandle dc(dis->hDC);
			CRect rc(dis->rcItem);
			dc.FrameRect(&rc, GetSysColorBrush(COLOR_GRAYTEXT));
			rc.InflateRect(-1, -1);
			dc.FrameRect(&rc, GetSysColorBrush(COLOR_WINDOW));
			rc.InflateRect(-1, -1);
			dc.FillRect(&rc, hBrush);
			dc.SetTextColor(ColorUtil::textFromBackground(currentColor));
			dc.SetBkMode(TRANSPARENT);
			HFONT prevFont = dc.SelectFont(hFont);
			dc.DrawText(buf, -1, rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			dc.SelectFont(prevFont);
		}
		else if (dis->CtlID == IDC_SETTINGS_ODC_MENUBAR_COLOR)
		{
			CDCHandle dc(dis->hDC);
			CRect rc(dis->rcItem);
			COLORREF backgroundColor;
			if (useCustomMenu)
			{
				if (menuTwoColors)
					OperaColors::drawBar(dc, rc.left, rc.top, rc.right, rc.bottom, menuLeftColor, menuRightColor, menuBumped);
				else
					dc.FillSolidRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, menuLeftColor);
				backgroundColor = menuLeftColor;
			}
			else
			{
				backgroundColor = GetSysColor(COLOR_GRAYTEXT);
				dc.FillSolidRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, backgroundColor);
			}
			dc.SetTextColor(ColorUtil::textFromBackground(backgroundColor));
			int oldMode = dc.SetBkMode(TRANSPARENT);
			dc.DrawText(CTSTRING(SETTINGS_MENUHEADER_EXAMPLE), -1, rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			dc.SetBkMode(oldMode);
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

UINT_PTR CALLBACK OtherColorsTab::hookProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
			instance->ctrlMenubar.Invalidate();
		}
	}
	return 0;
}

LRESULT OtherColorsTab::onChooseMenuColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	selColor = wID == IDC_SETTINGS_ODC_MENUBAR_LEFT ? &menuLeftColor : &menuRightColor;
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
				callback->settingChanged(wID == IDC_SETTINGS_ODC_MENUBAR_LEFT ? Conf::MENUBAR_LEFT_COLOR : Conf::MENUBAR_RIGHT_COLOR);
		}
	}
	else
		*selColor = savedColor;
	instance = nullptr;
	ctrlMenubar.Invalidate();
	return 0;
}
