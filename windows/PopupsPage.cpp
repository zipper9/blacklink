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
#include "MainFrm.h"
#include "PopupManager.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_POPUP_ENABLE, ResourceManager::ENABLE_POPUPS },
	{ IDC_POPUP_AWAY, ResourceManager::POPUP_ONLY_WHEN_AWAY },
	{ IDC_POPUP_MINIMIZED, ResourceManager::POPUP_ONLY_WHEN_MINIMIZED },
	{ IDC_POPUPGROUP, ResourceManager::SETTINGS_POPUPS },
	{ IDC_PREVIEW, ResourceManager::PREVIEW_MENU },
	{ IDC_POPUPTYPE, ResourceManager::POPUP_TYPE },
	{ IDC_POPUP_TIME_STR, ResourceManager::POPUP_TIME },
	{ IDC_S, ResourceManager::S },
	{ IDC_POPUP_BACKCOLOR, ResourceManager::POPUP_BACK_COLOR },
	{ IDC_POPUP_FONT, ResourceManager::POPUP_FONT },
	{ IDC_POPUP_TITLE_FONT, ResourceManager::POPUP_TITLE_FONT },
	{ IDC_MAX_MSG_LENGTH_STR, ResourceManager::MAX_MSG_LENGTH },
	{ IDC_POPUP_W_STR, ResourceManager::WIDTH },
	{ IDC_POPUP_H_STR, ResourceManager::HEIGHT },
	{ IDC_POPUP_TRANSP_STR, ResourceManager::TRANSPARENCY },
	{ IDC_POPUP_IMAGE_GP, ResourceManager::POPUP_IMAGE },
	{ IDC_POPUP_COLORS, ResourceManager::POPUP_COLORS },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_POPUP_TIME, SettingsManager::POPUP_TIME, PropPage::T_INT },
	{ IDC_POPUPFILE, SettingsManager::POPUP_IMAGE_FILE, PropPage::T_STR },
	{ IDC_MAX_MSG_LENGTH, SettingsManager::POPUP_MAX_LENGTH, PropPage::T_INT },
	{ IDC_POPUP_W, SettingsManager::POPUP_WIDTH, PropPage::T_INT },
	{ IDC_POPUP_H, SettingsManager::POPUP_HEIGHT, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::POPUP_ON_HUB_CONNECTED, ResourceManager::POPUP_HUB_CONNECTED },
	{ SettingsManager::POPUP_ON_HUB_DISCONNECTED, ResourceManager::POPUP_HUB_DISCONNECTED },
	{ SettingsManager::POPUP_ON_FAVORITE_CONNECTED, ResourceManager::POPUP_FAVORITE_CONNECTED },
	{ SettingsManager::POPUP_ON_FAVORITE_DISCONNECTED, ResourceManager::POPUP_FAVORITE_DISCONNECTED },
	{ SettingsManager::POPUP_ON_CHEATING_USER, ResourceManager::POPUP_CHEATING_USER },
	{ SettingsManager::POPUP_ON_CHAT_LINE, ResourceManager::POPUP_CHAT_LINE },
	{ SettingsManager::POPUP_ON_DOWNLOAD_STARTED, ResourceManager::POPUP_DOWNLOAD_START },
	{ SettingsManager::POPUP_ON_DOWNLOAD_FAILED, ResourceManager::POPUP_DOWNLOAD_FAILED },
	{ SettingsManager::POPUP_ON_DOWNLOAD_FINISHED, ResourceManager::POPUP_DOWNLOAD_FINISHED },
	{ SettingsManager::POPUP_ON_UPLOAD_FINISHED, ResourceManager::POPUP_UPLOAD_FINISHED },
	{ SettingsManager::POPUP_ON_PM, ResourceManager::POPUP_PM },
	{ SettingsManager::POPUP_ON_NEW_PM, ResourceManager::POPUP_NEW_PM },
	{ SettingsManager::POPUP_PM_PREVIEW, ResourceManager::PM_PREVIEW },
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
	{ SettingsManager::POPUP_ON_SEARCH_SPY, ResourceManager::POPUP_SEARCH_SPY },
#endif
//	{ SettingsManager::POPUP_ON_FOLDER_SHARED, ResourceManager::POPUP_NEW_FOLDERSHARE },
	{ 0, ResourceManager::Strings() }
};

LRESULT Popups::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlPopups.Attach(GetDlgItem(IDC_POPUPLIST));

	WinUtil::translate(*this, texts);
	PropPage::read(*this, items, listItems, ctrlPopups);
	
	ctrlPopupType.Attach(GetDlgItem(IDC_POPUP_TYPE));
	ctrlPopupType.AddString(CTSTRING(POPUP_BALOON));
	ctrlPopupType.AddString(CTSTRING(POPUP_CUSTOM));
	ctrlPopupType.AddString(CTSTRING(POPUP_SPLASH));
	ctrlPopupType.AddString(CTSTRING(POPUP_WINDOW));
	ctrlPopupType.SetCurSel(SETTING(POPUP_TYPE));
	
	CUpDownCtrl spin;
	spin.Attach(GetDlgItem(IDC_POPUP_TIME_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_POPUP_TIME));
	spin.SetRange32(1, 15);
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_MAX_MSG_LENGTH_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_MAX_MSG_LENGTH));
	spin.SetRange32(1, 512);
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_POPUP_W_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_POPUP_W));
	spin.SetRange32(80, 599);
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_POPUP_H_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_POPUP_H));
	spin.SetRange32(50, 299);
	spin.Detach();
	
	slider = GetDlgItem(IDC_POPUP_TRANSP_SLIDER);
	slider.SetRangeMin(50, TRUE);
	slider.SetRangeMax(255, TRUE);
	slider.SetPos(SETTING(POPUP_TRANSPARENCY));
	
	SetDlgItemText(IDC_POPUPFILE, Text::toT(SETTING(POPUP_IMAGE_FILE)).c_str());
	
	if (SETTING(POPUP_TYPE) == BALLOON)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_W), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_W_SPIN), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_H), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_H_SPIN), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_TRANSP_SLIDER), FALSE);
	}
	else if (SETTING(POPUP_TYPE) == CUSTOM)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), FALSE);
	}
	else
	{
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), FALSE);
	}
	
	CheckDlgButton(IDC_POPUP_ENABLE, SETTING(POPUPS_DISABLED) ? BST_UNCHECKED : BST_CHECKED);
	CheckDlgButton(IDC_POPUP_AWAY, SETTING(POPUP_ONLY_WHEN_AWAY) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_POPUP_MINIMIZED, SETTING(POPUP_ONLY_WHEN_MINIMIZED) ? BST_CHECKED : BST_UNCHECKED);
	fixControls();
	
	return TRUE;
}

LRESULT Popups::onBackColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CColorDialog dlg(SETTING(POPUP_BACKCOLOR), CC_FULLOPEN);
	if (dlg.DoModal() == IDOK)
		SET_SETTING(POPUP_BACKCOLOR, (int)dlg.GetColor());
	return 0;
}

LRESULT Popups::onFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LOGFONT font  = {0};
	Fonts::decodeFont(Text::toT(SETTING(POPUP_FONT)), font);
	CFontDialog dlg(&font, CF_EFFECTS | CF_SCREENFONTS | CF_FORCEFONTEXIST);
	dlg.m_cf.rgbColors = SETTING(POPUP_TEXTCOLOR);
	if (dlg.DoModal() == IDOK)
	{
		SET_SETTING(POPUP_TEXTCOLOR, (int)dlg.GetColor());
		SET_SETTING(POPUP_FONT, Text::fromT(WinUtil::encodeFont(font)));
	}
	return 0;
}

LRESULT Popups::onTitleFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LOGFONT tmp = myFont;
	Fonts::decodeFont(Text::toT(SETTING(POPUP_TITLE_FONT)), tmp);
	CFontDialog dlg(&tmp, CF_EFFECTS | CF_SCREENFONTS | CF_FORCEFONTEXIST); // !SMT!-F
	dlg.m_cf.rgbColors = SETTING(POPUP_TITLE_TEXTCOLOR);
	if (dlg.DoModal() == IDOK)
	{
		myFont = tmp;
		SET_SETTING(POPUP_TITLE_TEXTCOLOR, (int)dlg.GetColor());
		SET_SETTING(POPUP_TITLE_FONT, Text::fromT(WinUtil::encodeFont(myFont)));
	}
	return 0;
}

LRESULT Popups::onPopupBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring x;
	WinUtil::getWindowText(GetDlgItem(IDC_POPUPFILE), x);
	if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
	{
		SetDlgItemText(IDC_POPUPFILE, x.c_str());
		SET_SETTING(POPUP_IMAGE_FILE, Text::fromT(x));
	}
	return 0;
}

LRESULT Popups::onTypeChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SET_SETTING(POPUP_TYPE, ctrlPopupType.GetCurSel());
	if (ctrlPopupType.GetCurSel() == BALLOON)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_W), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_W_SPIN), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_H), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_H_SPIN), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_TRANSP_SLIDER), FALSE);
	}
	else if (ctrlPopupType.GetCurSel() == CUSTOM)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), TRUE);
	}
	else
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), FALSE);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), FALSE);
	}
	
	if (ctrlPopupType.GetCurSel() != BALLOON)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_W), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUP_W_SPIN), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUP_H), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUP_H_SPIN), TRUE);
		::EnableWindow(GetDlgItem(IDC_POPUP_TRANSP_SLIDER), TRUE);
	}
	return 0;
}

LRESULT Popups::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void Popups::write()
{
	PropPage::write(*this, items, listItems, ctrlPopups);
	
	SET_SETTING(POPUP_TYPE, ctrlPopupType.GetCurSel());
	SET_SETTING(POPUPS_DISABLED, IsDlgButtonChecked(IDC_POPUP_ENABLE) != BST_CHECKED);
	SET_SETTING(POPUP_ONLY_WHEN_AWAY, IsDlgButtonChecked(IDC_POPUP_AWAY) == BST_CHECKED);
	SET_SETTING(POPUP_ONLY_WHEN_MINIMIZED, IsDlgButtonChecked(IDC_POPUP_MINIMIZED) == BST_CHECKED);
	SET_SETTING(POPUP_TRANSPARENCY, slider.GetPos());
}

void Popups::fixControls()
{
	BOOL state = IsDlgButtonChecked(IDC_POPUP_ENABLE) ? TRUE : FALSE;
	ctrlPopups.EnableWindow(state);
	::EnableWindow(GetDlgItem(IDC_POPUP_MINIMIZED), state);
	::EnableWindow(GetDlgItem(IDC_POPUP_AWAY), state);
}

LRESULT Popups::onPreview(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int popupW = SETTING(POPUP_WIDTH);
	int popupH = SETTING(POPUP_HEIGHT);
	int popupTransp = SETTING(POPUP_TRANSPARENCY);
	tstring buf;
	WinUtil::getWindowText(GetDlgItem(IDC_POPUP_W), buf);
	SET_SETTING(POPUP_WIDTH, Text::fromT(buf));
	WinUtil::getWindowText(GetDlgItem(IDC_POPUP_H), buf);
	SET_SETTING(POPUP_HEIGHT, Text::fromT(buf));
	SET_SETTING(POPUP_TRANSPARENCY, slider.GetPos());      //set from TrackBar position
	SET_SETTING(POPUP_TYPE, ctrlPopupType.GetCurSel());
	
	PopupManager::getInstance()->Show(TSTRING(FILE) + _T(": FlylinkDC++.7z\n") +
	                                  TSTRING(USER) + _T(": ") + Text::toT(SETTING(NICK)), TSTRING(DOWNLOAD_FINISHED_IDLE), NIIF_INFO, true);
	                                  
	SET_SETTING(POPUP_WIDTH, popupW);
	SET_SETTING(POPUP_HEIGHT, popupH);
	SET_SETTING(POPUP_TRANSPARENCY, popupTransp);
	return 0;
}
