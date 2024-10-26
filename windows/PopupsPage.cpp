/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"
#include "PopupsPage.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "PopupManager.h"
#include "DialogLayout.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/AdcHub.h"
#include "../client/Random.h"
#include "../client/Util.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const PropPage::Item items[] =
{
	{ IDC_POPUP_ENABLE, Conf::POPUPS_DISABLED, PropPage::T_BOOL, PropPage::FLAG_INVERT },
	{ IDC_POPUP_AWAY, Conf::POPUP_ONLY_WHEN_AWAY, PropPage::T_BOOL },
	{ IDC_POPUP_MINIMIZED, Conf::POPUP_ONLY_WHEN_MINIMIZED, PropPage::T_BOOL },
	{ IDC_POPUP_TIME, Conf::POPUP_TIME, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_MAX_MSG_LENGTH, Conf::POPUP_MAX_LENGTH, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_POPUP_W, Conf::POPUP_WIDTH, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ IDC_POPUP_H, Conf::POPUP_HEIGHT, PropPage::T_INT, PropPage::FLAG_CREATE_SPIN },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::POPUP_ON_HUB_CONNECTED, ResourceManager::POPUP_HUB_CONNECTED },
	{ Conf::POPUP_ON_HUB_DISCONNECTED, ResourceManager::POPUP_HUB_DISCONNECTED },
	{ Conf::POPUP_ON_FAVORITE_CONNECTED, ResourceManager::POPUP_FAVORITE_CONNECTED },
	{ Conf::POPUP_ON_FAVORITE_DISCONNECTED, ResourceManager::POPUP_FAVORITE_DISCONNECTED },
	{ Conf::POPUP_ON_CHEATING_USER, ResourceManager::POPUP_CHEATING_USER },
	{ Conf::POPUP_ON_CHAT_LINE, ResourceManager::POPUP_CHAT_LINE },
	{ Conf::POPUP_ON_DOWNLOAD_STARTED, ResourceManager::POPUP_DOWNLOAD_START },
	{ Conf::POPUP_ON_DOWNLOAD_FAILED, ResourceManager::POPUP_DOWNLOAD_FAILED },
	{ Conf::POPUP_ON_DOWNLOAD_FINISHED, ResourceManager::POPUP_DOWNLOAD_FINISHED },
	{ Conf::POPUP_ON_UPLOAD_FINISHED, ResourceManager::POPUP_UPLOAD_FINISHED },
	{ Conf::POPUP_ON_PM, ResourceManager::POPUP_PM },
	{ Conf::POPUP_ON_NEW_PM, ResourceManager::POPUP_NEW_PM },
	{ Conf::POPUP_PM_PREVIEW, ResourceManager::PM_PREVIEW },
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
	{ Conf::POPUP_ON_SEARCH_SPY, ResourceManager::POPUP_SEARCH_SPY },
#endif
//	{ Conf::POPUP_ON_FOLDER_SHARED, ResourceManager::POPUP_NEW_FOLDERSHARE },
	{ 0, ResourceManager::Strings() }
};

static const DialogLayout::Align align1 = { 5, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { -2, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align4 = { 11, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_POPUP_ENABLE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_POPUP_AWAY, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_POPUP_MINIMIZED, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_POPUPGROUP, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_POPUPTYPE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_POPUP_TYPE, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_POPUP_TIME_STR, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_MAX_MSG_LENGTH_STR, FLAG_TRANSLATE, AUTO, UNSPEC, 1 },
	{ IDC_POPUP_W_STR, FLAG_TRANSLATE, AUTO, UNSPEC, 2 },
	{ IDC_POPUP_H_STR, FLAG_TRANSLATE, AUTO, UNSPEC, 2 },
	{ IDC_POPUP_TIME, 0, UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_MAX_MSG_LENGTH, 0, UNSPEC, UNSPEC, 0, &align2 },
	{ IDC_POPUP_W, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_POPUP_H, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_S, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align4 },
	{ IDC_POPUP_COLORS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_POPUP_TITLE_FONT, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_POPUP_FONT, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_POPUP_BACKCOLOR, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_POPUP_BORDER_COLOR, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_PREVIEW, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

enum
{
	POPUP_COLOR_TITLE,
	POPUP_COLOR_MESSAGE,
	POPUP_COLOR_BACKGROUND,
	POPUP_COLOR_BORDER
};

enum
{
	POPUP_FONT_TITLE,
	POPUP_FONT_MESSAGE
};

PopupsPage::PopupsPage() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(POPUPS))
{
	SetTitle(m_title.c_str());
	m_psp.dwFlags |= PSP_RTLREADING;

	colorSettings[POPUP_COLOR_TITLE].setting = Conf::POPUP_TITLE_TEXT_COLOR;
	colorSettings[POPUP_COLOR_MESSAGE].setting = Conf::POPUP_TEXT_COLOR;
	colorSettings[POPUP_COLOR_BACKGROUND].setting = Conf::POPUP_BACKGROUND_COLOR;
	colorSettings[POPUP_COLOR_BORDER].setting = Conf::POPUP_BORDER_COLOR;
	fontSettings[POPUP_FONT_TITLE].setting = Conf::POPUP_TITLE_FONT;
	fontSettings[POPUP_FONT_MESSAGE].setting = Conf::POPUP_FONT;

	const auto* ss = SettingsManager::instance.getUiSettings();
	for (int i = 0; i < _countof(colorSettings); ++i)
	{
		colorSettings[i].oldColor = colorSettings[i].color = ss->getInt(colorSettings[i].setting);
		colorSettings[i].changed = false;
	}
	for (int i = 0; i < _countof(fontSettings); ++i)
	{
		fontSettings[i].oldFont = fontSettings[i].font = ss->getString(fontSettings[i].setting);
		fontSettings[i].changed = false;
	}
	if (fontSettings[POPUP_FONT_TITLE].font.empty())
		fontSettings[POPUP_FONT_TITLE].font = PopupManager::getInstance()->getDefaultTitleFont();
}

LRESULT PopupsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlPopups.Attach(GetDlgItem(IDC_POPUPLIST));
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::initControls(*this, items);
	PropPage::read(*this, items, listItems, ctrlPopups);

	const auto* ss = SettingsManager::instance.getUiSettings();
	int popupType = ss->getInt(Conf::POPUP_TYPE);
	ctrlPopupType.Attach(GetDlgItem(IDC_POPUP_TYPE));
	ctrlPopupType.AddString(CTSTRING(POPUP_BALLOON));
	ctrlPopupType.AddString(CTSTRING(POPUP_CUSTOM));
	ctrlPopupType.SetCurSel(popupType);

	if (popupType == PopupManager::TYPE_SYSTEM)
		updateControls(popupType);

	fixControls();
	return TRUE;
}

void PopupsPage::changeColor(int index)
{
	CColorDialog dlg(colorSettings[index].color, CC_FULLOPEN);
	if (dlg.DoModal() == IDOK)
	{
		colorSettings[index].color = dlg.GetColor();
		colorSettings[index].changed = true;
	}
}

void PopupsPage::changeFont(int fontIndex, int colorIndex)
{
	LOGFONT font;
	Fonts::decodeFont(Text::toT(fontSettings[fontIndex].font), font);
	CFontDialog dlg(&font, CF_EFFECTS | CF_SCREENFONTS | CF_FORCEFONTEXIST);
	dlg.m_cf.rgbColors = colorSettings[colorIndex].color;
	if (dlg.DoModal() == IDOK)
	{
		colorSettings[colorIndex].color = dlg.GetColor();
		colorSettings[colorIndex].changed = true;
		fontSettings[fontIndex].font = Text::fromT(Fonts::encodeFont(font));
		fontSettings[fontIndex].changed = true;
	}
}

LRESULT PopupsPage::onBackColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	changeColor(POPUP_COLOR_BACKGROUND);
	return 0;
}

LRESULT PopupsPage::onBorderColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	changeColor(POPUP_COLOR_BORDER);
	return 0;
}

LRESULT PopupsPage::onFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	changeFont(POPUP_FONT_MESSAGE, POPUP_COLOR_MESSAGE);
	return 0;
}

LRESULT PopupsPage::onTitleFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	changeFont(POPUP_FONT_TITLE, POPUP_COLOR_TITLE);
	return 0;
}

void PopupsPage::updateControls(int popupType)
{
	BOOL state = popupType == PopupManager::TYPE_CUSTOM;
	GetDlgItem(IDC_POPUP_W).EnableWindow(state);
	GetDlgItem(IDC_POPUP_H).EnableWindow(state);
	GetDlgItem(IDC_POPUP_TITLE_FONT).EnableWindow(state);
	GetDlgItem(IDC_POPUP_FONT).EnableWindow(state);
	GetDlgItem(IDC_POPUP_BACKCOLOR).EnableWindow(state);
	GetDlgItem(IDC_POPUP_BORDER_COLOR).EnableWindow(state);
}

LRESULT PopupsPage::onTypeChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int popupType = ctrlPopupType.GetCurSel();
	updateControls(popupType);
	return 0;
}

LRESULT PopupsPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void PopupsPage::write()
{
	PropPage::write(*this, items, listItems, ctrlPopups);
	applySettings();

	auto ss = SettingsManager::instance.getUiSettings();
	ss->setInt(Conf::POPUP_TYPE, ctrlPopupType.GetCurSel());
}

void PopupsPage::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_POPUP_ENABLE) ? TRUE : FALSE;
	ctrlPopups.EnableWindow(state);
	GetDlgItem(IDC_POPUP_MINIMIZED).EnableWindow(state);
	GetDlgItem(IDC_POPUP_AWAY).EnableWindow(state);
}

void PopupsPage::applySettings()
{
	auto ss = SettingsManager::instance.getUiSettings();
	for (int i = 0; i < _countof(colorSettings); ++i)
		if (colorSettings[i].changed)
			ss->setInt(colorSettings[i].setting, colorSettings[i].color);
	for (int i = 0; i < _countof(fontSettings); ++i)
		if (fontSettings[i].changed)
			ss->setString(fontSettings[i].setting, fontSettings[i].font);
}

void PopupsPage::restoreSettings()
{
	auto ss = SettingsManager::instance.getUiSettings();
	for (int i = 0; i < _countof(colorSettings); ++i)
		if (colorSettings[i].changed)
			ss->setInt(colorSettings[i].setting, colorSettings[i].oldColor);
	for (int i = 0; i < _countof(fontSettings); ++i)
		if (fontSettings[i].changed)
			ss->setString(fontSettings[i].setting, fontSettings[i].oldFont);
}

LRESULT PopupsPage::onPreview(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto ss = SettingsManager::instance.getUiSettings();
	int savedWidth = ss->getInt(Conf::POPUP_WIDTH);
	int savedHeight = ss->getInt(Conf::POPUP_HEIGHT);
	int savedType = ss->getInt(Conf::POPUP_TYPE);

	tstring buf;
	WinUtil::getWindowText(GetDlgItem(IDC_POPUP_W), buf);
	ss->setInt(Conf::POPUP_WIDTH, Util::toInt(buf));
	WinUtil::getWindowText(GetDlgItem(IDC_POPUP_H), buf);
	ss->setInt(Conf::POPUP_HEIGHT, Util::toInt(buf));
	ss->setInt(Conf::POPUP_TYPE, ctrlPopupType.GetCurSel());
	applySettings();

	const auto& ext1 = AdcHub::getSearchExts();
	const auto& ext2 = ext1[Util::rand(ext1.size())];
	string exampleFileName = Util::getRandomNick();
	exampleFileName += '.';
	exampleFileName += ext2[Util::rand(ext2.size())];

	PopupManager::getInstance()->show(TSTRING(FILE) + _T(": ") + Text::toT(exampleFileName) + _T('\n') +
	                                  TSTRING(USER) + _T(": ") + Text::toT(ClientManager::getDefaultNick()), TSTRING(DOWNLOAD_FINISHED_POPUP), NIIF_INFO, true);

	ss->setInt(Conf::POPUP_WIDTH, savedWidth);
	ss->setInt(Conf::POPUP_HEIGHT, savedHeight);
	ss->setInt(Conf::POPUP_TYPE, savedType);
	restoreSettings();
	return 0;
}
