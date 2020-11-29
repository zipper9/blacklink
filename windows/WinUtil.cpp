/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#include "../client/File.h"
#include "Resource.h"

#include <atlwin.h>
#include <shlobj.h>
#include <mmsystem.h>

#define COMPILE_MULTIMON_STUBS 1
#include <MultiMon.h>
#include <powrprof.h>

#include "WinUtil.h"
#include "PrivateFrame.h"
#include "MainFrm.h"
#include "LineDlg.h"

#include "../client/StringTokenizer.h"
#include "../client/SimpleStringTokenizer.h"
#include "../client/ShareManager.h"
#include "../client/UploadManager.h"
#include "../client/HashManager.h"
#include "../client/File.h"
#include "../client/DownloadManager.h"
#include "../client/ParamExpander.h"
#include "../client/MagnetLink.h"
#include "MagnetDlg.h"
#include "BarShader.h"
#include "HTMLColors.h"
#include "DirectoryListingFrm.h"

#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

string UserInfoGuiTraits::g_hubHint;
UserPtr UserInfoBaseHandlerTraitsUser<UserPtr>::g_user = nullptr;
OnlineUserPtr UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::g_user = nullptr;

const TCHAR* g_file_list_type = L"All Lists\0*.xml.bz2;*.dcls;*.dclst;*.torrent\0Torrent files\0*.torrent\0FileLists\0*.xml.bz2\0DCLST metafiles\0*.dcls;*.dclst\0All Files\0*.*\0\0";

HBRUSH Colors::g_bgBrush = nullptr;
COLORREF Colors::g_textColor = 0;
COLORREF Colors::g_bgColor = 0;

HFONT Fonts::g_font = nullptr;
int Fonts::g_fontHeight = 0;
int Fonts::g_fontHeightPixl = 0;
HFONT Fonts::g_boldFont = nullptr;
HFONT Fonts::g_systemFont = nullptr;

CMenu WinUtil::g_mainMenu;

OMenu WinUtil::g_copyHubMenu;
OMenu UserInfoGuiTraits::copyUserMenu;
OMenu UserInfoGuiTraits::grantMenu;
OMenu UserInfoGuiTraits::speedMenu;
OMenu UserInfoGuiTraits::userSummaryMenu;
OMenu UserInfoGuiTraits::privateMenu;

OMenu Preview::g_previewMenu;
int Preview::g_previewAppsSize = 0;
dcdrun(bool Preview::_debugIsActivated = false;)
dcdrun(bool Preview::_debugIsClean = true;)

HIconWrapper WinUtil::g_banIconOnline(IDR_BANNED_ONLINE);
HIconWrapper WinUtil::g_banIconOffline(IDR_BANNED_OFF);
HIconWrapper WinUtil::g_hFirewallIcon(IDR_ICON_FIREWALL);
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
HIconWrapper WinUtil::g_hClockIcon(IDR_ICON_CLOCK);
#endif

std::unique_ptr<HIconWrapper> WinUtil::g_HubOnIcon;
std::unique_ptr<HIconWrapper> WinUtil::g_HubOffIcon;

TStringList LastDir::dirs;
HWND WinUtil::g_mainWnd = nullptr;
HWND WinUtil::g_mdiClient = nullptr;
FlatTabCtrl* WinUtil::g_tabCtrl = nullptr;
HHOOK WinUtil::g_hook = nullptr;
bool WinUtil::hubUrlHandlersRegistered = false;
bool WinUtil::magnetHandlerRegistered = false;
bool WinUtil::dclstHandlerRegistered = false;
bool WinUtil::g_isAppActive = false;
CHARFORMAT2 Colors::g_TextStyleTimestamp;
CHARFORMAT2 Colors::g_ChatTextGeneral;
CHARFORMAT2 Colors::g_ChatTextOldHistory;
CHARFORMAT2 Colors::g_TextStyleMyNick;
CHARFORMAT2 Colors::g_ChatTextMyOwn;
CHARFORMAT2 Colors::g_ChatTextServer;
CHARFORMAT2 Colors::g_ChatTextSystem;
CHARFORMAT2 Colors::g_TextStyleBold;
CHARFORMAT2 Colors::g_TextStyleFavUsers;
CHARFORMAT2 Colors::g_TextStyleFavUsersBan;
CHARFORMAT2 Colors::g_TextStyleOPs;
CHARFORMAT2 Colors::g_TextStyleURL;
CHARFORMAT2 Colors::g_ChatTextPrivate;
CHARFORMAT2 Colors::g_ChatTextLog;

HLSCOLOR RGB2HLS(COLORREF rgb)
{
	unsigned char minval = min(GetRValue(rgb), min(GetGValue(rgb), GetBValue(rgb)));
	unsigned char maxval = max(GetRValue(rgb), max(GetGValue(rgb), GetBValue(rgb)));
	float mdiff  = float(maxval) - float(minval);
	float msum   = float(maxval) + float(minval);
	
	float luminance = msum / 510.0f;
	float saturation = 0.0f;
	float hue = 0.0f;
	
	if (maxval != minval)
	{
		float rnorm = (maxval - GetRValue(rgb)) / mdiff;
		float gnorm = (maxval - GetGValue(rgb)) / mdiff;
		float bnorm = (maxval - GetBValue(rgb)) / mdiff;
		
		saturation = (luminance <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));
		
		if (GetRValue(rgb) == maxval) hue = 60.0f * (6.0f + bnorm - gnorm);
		if (GetGValue(rgb) == maxval) hue = 60.0f * (2.0f + rnorm - bnorm);
		if (GetBValue(rgb) == maxval) hue = 60.0f * (4.0f + gnorm - rnorm);
		if (hue > 360.0f) hue = hue - 360.0f;
	}
	return HLS((hue * 255) / 360, luminance * 255, saturation * 255);
}

static BYTE _ToRGB(float rm1, float rm2, float rh)
{
	if (rh > 360.0f) rh -= 360.0f;
	else if (rh <   0.0f) rh += 360.0f;
	
	if (rh <  60.0f) rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;
	else if (rh < 180.0f) rm1 = rm2;
	else if (rh < 240.0f) rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;
	
	return (BYTE)(rm1 * 255);
}

static inline bool EqualD(double a, double b)
{
	return fabs(a - b) <= 1e-6;
}

COLORREF HLS2RGB(HLSCOLOR hls)
{
	float hue        = ((int)HLS_H(hls) * 360) / 255.0f;
	float luminance  = HLS_L(hls) / 255.0f;
	float saturation = HLS_S(hls) / 255.0f;
	
	if (EqualD(saturation, 0))
	{
		return RGB(HLS_L(hls), HLS_L(hls), HLS_L(hls));
	}
	float rm1, rm2;
	
	if (luminance <= 0.5f) rm2 = luminance + luminance * saturation;
	else                     rm2 = luminance + saturation - luminance * saturation;
	rm1 = 2.0f * luminance - rm2;
	BYTE red   = _ToRGB(rm1, rm2, hue + 120.0f);
	BYTE green = _ToRGB(rm1, rm2, hue);
	BYTE blue  = _ToRGB(rm1, rm2, hue - 120.0f);
	
	return RGB(red, green, blue);
}

COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S)
{
	HLSCOLOR hls = RGB2HLS(rgb);
	BYTE h = HLS_H(hls);
	BYTE l = HLS_L(hls);
	BYTE s = HLS_S(hls);
	
	if (percent_L > 0)
	{
		l = BYTE(l + ((255 - l) * percent_L) / 100);
	}
	else if (percent_L < 0)
	{
		l = BYTE((l * (100 + percent_L)) / 100);
	}
	if (percent_S > 0)
	{
		s = BYTE(s + ((255 - s) * percent_S) / 100);
	}
	else if (percent_S < 0)
	{
		s = BYTE((s * (100 + percent_S)) / 100);
	}
	return HLS2RGB(HLS(h, l, s));
}

void WinUtil::initThemeIcons()
{
	g_HubOnIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB));
	g_HubOffIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB_OFF));
}

dcdrun(bool WinUtil::g_staticMenuUnlinked = true;)
void WinUtil::unlinkStaticMenus(OMenu& menu)
{
	dcdrun(g_staticMenuUnlinked = true;)
	MENUITEMINFO mif = { sizeof MENUITEMINFO };
	mif.fMask = MIIM_SUBMENU;
	for (int i = menu.GetMenuItemCount()-1; i >= 0; i--)
	{
		menu.GetMenuItemInfo(i, TRUE, &mif);
		if (UserInfoGuiTraits::isUserInfoMenus(mif.hSubMenu) ||
		    mif.hSubMenu == g_copyHubMenu.m_hMenu ||
		    Preview::isPreviewMenu(mif.hSubMenu))
		{
			menu.RemoveMenu(i, MF_BYPOSITION);
		}
	}
}

void WinUtil::escapeMenu(tstring& text)
{
	string::size_type i = 0;
	while ((i = text.find(_T('&'), i)) != string::npos)
	{
		text.insert(i, 1, _T('&'));
		i += 2;
	}
}

const tstring& WinUtil::escapeMenu(const tstring& text, tstring& tmp)
{
	string::size_type i = text.find(_T('&'));
	if (i == string::npos) return text;
	tmp = text;
	tmp.insert(i, 1, _T('&'));
	i += 2;
	while ((i = tmp.find(_T('&'), i)) != string::npos)
	{
		tmp.insert(i, 1, _T('&'));
		i += 2;
	}
	return tmp;
}

void WinUtil::appendSeparator(HMENU hMenu)
{
	CMenuHandle menu(hMenu);
	int count = menu.GetMenuItemCount();
	if (!count) return;
	UINT state = menu.GetMenuState(count-1, MF_BYPOSITION);
	if ((state & MF_POPUP) || !(state & MF_SEPARATOR))
		menu.AppendMenu(MF_SEPARATOR);
}

void WinUtil::appendSeparator(OMenu& menu)
{
	int count = menu.GetMenuItemCount();
	if (!count) return;
	UINT state = menu.GetMenuState(count-1, MF_BYPOSITION);
	if ((state & MF_POPUP) || !(state & MF_SEPARATOR))
		menu.AppendMenu(MF_SEPARATOR);
}

static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
		if (WinUtil::g_tabCtrl) //[+]PPA
			if (wParam == VK_CONTROL && LOWORD(lParam) == 1)
			{
				if (lParam & 0x80000000)
				{
					WinUtil::g_tabCtrl->endSwitch();
				}
				else
				{
					WinUtil::g_tabCtrl->startSwitch();
				}
			}
	}
	return CallNextHookEx(WinUtil::g_hook, code, wParam, lParam);
}

void WinUtil::init(HWND hWnd)
{
	g_mainWnd = hWnd;
	
	Preview::init();
	
	g_mainMenu.CreateMenu();
	
	CMenuHandle file;
	file.CreatePopupMenu();
	
	file.AppendMenu(MF_STRING, IDC_OPEN_FILE_LIST, CTSTRING(MENU_OPEN_FILE_LIST));
	file.AppendMenu(MF_STRING, IDC_ADD_MAGNET, CTSTRING(MENU_ADD_MAGNET));
#ifdef FLYLINKDC_USE_TORRENT
	file.AppendMenu(MF_STRING, IDC_OPEN_TORRENT_FILE, CTSTRING(MENU_OPEN_TORRENT_FILE));
#endif
	file.AppendMenu(MF_STRING, IDC_OPEN_MY_LIST, CTSTRING(MENU_OPEN_OWN_LIST));
	file.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST, CTSTRING(MENU_REFRESH_FILE_LIST));
	file.AppendMenu(MF_STRING, IDC_MATCH_ALL, CTSTRING(MENU_OPEN_MATCH_ALL));
#if 0
	file.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST_PURGE, CTSTRING(MENU_REFRESH_FILE_LIST_PURGE));
	file.AppendMenu(MF_STRING, IDC_CONVERT_TTH_HISTORY, CTSTRING(MENU_CONVERT_TTH_HISTORY_INTO_LEVELDB));
#endif
	file.AppendMenu(MF_STRING, IDC_OPEN_DOWNLOADS, CTSTRING(MENU_OPEN_DOWNLOADS_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, IDC_OPEN_LOGS, CTSTRING(MENU_OPEN_LOGS_DIR));
	file.AppendMenu(MF_STRING, IDC_OPEN_CONFIGS, CTSTRING(MENU_OPEN_CONFIG_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_GET_TTH, CTSTRING(MENU_TTH));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_FILE_RECONNECT, CTSTRING(MENU_RECONNECT));
	file.AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED));
	file.AppendMenu(MF_STRING, IDC_FOLLOW, CTSTRING(MENU_FOLLOW_REDIRECT));
	file.AppendMenu(MF_STRING, ID_FILE_QUICK_CONNECT, CTSTRING(MENU_QUICK_CONNECT));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_FILE_SETTINGS, CTSTRING(MENU_SETTINGS));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_APP_EXIT, CTSTRING(MENU_EXIT));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)file, CTSTRING(MENU_FILE));
	
	CMenuHandle view;
	view.CreatePopupMenu();
	view.AppendMenu(MF_STRING, ID_FILE_CONNECT, CTSTRING(MENU_PUBLIC_HUBS));
	view.AppendMenu(MF_STRING, IDC_RECENTS, CTSTRING(MENU_FILE_RECENT_HUBS));
	view.AppendMenu(MF_STRING, IDC_FAVORITES, CTSTRING(MENU_FAVORITE_HUBS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, IDC_FAVUSERS, CTSTRING(MENU_FAVORITE_USERS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, ID_FILE_SEARCH, CTSTRING(MENU_SEARCH));
	view.AppendMenu(MF_STRING, IDC_FILE_ADL_SEARCH, CTSTRING(MENU_ADL_SEARCH));
	view.AppendMenu(MF_STRING, IDC_SEARCH_SPY, CTSTRING(MENU_SEARCH_SPY));
	view.AppendMenu(MF_SEPARATOR);
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
	view.AppendMenu(MF_STRING, IDC_CDMDEBUG_WINDOW, CTSTRING(MENU_CDMDEBUG_MESSAGES));
#endif
	view.AppendMenu(MF_STRING, IDC_NOTEPAD, CTSTRING(MENU_NOTEPAD));
	view.AppendMenu(MF_STRING, IDC_HASH_PROGRESS, CTSTRING(MENU_HASH_PROGRESS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, IDC_TOPMOST, CTSTRING(MENU_TOPMOST));
	view.AppendMenu(MF_STRING, ID_VIEW_TOOLBAR, CTSTRING(MENU_TOOLBAR));
	view.AppendMenu(MF_STRING, ID_VIEW_STATUS_BAR, CTSTRING(MENU_STATUS_BAR));
	view.AppendMenu(MF_STRING, ID_VIEW_TRANSFER_VIEW, CTSTRING(MENU_TRANSFER_VIEW));
	view.AppendMenu(MF_STRING, ID_TOGGLE_TOOLBAR, CTSTRING(TOGGLE_TOOLBAR));
	view.AppendMenu(MF_STRING, ID_TOGGLE_QSEARCH, CTSTRING(TOGGLE_QSEARCH));
	view.AppendMenu(MF_STRING, ID_VIEW_TRANSFER_VIEW_TOOLBAR, CTSTRING(MENU_TRANSFER_VIEW_TOOLBAR));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)view, CTSTRING(MENU_VIEW));
	
	CMenuHandle transfers;
	transfers.CreatePopupMenu();
	
	transfers.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(MENU_DOWNLOAD_QUEUE));
	transfers.AppendMenu(MF_STRING, IDC_FINISHED, CTSTRING(MENU_FINISHED_DOWNLOADS));
	transfers.AppendMenu(MF_SEPARATOR);
	transfers.AppendMenu(MF_STRING, IDC_UPLOAD_QUEUE, CTSTRING(MENU_WAITING_USERS));
	transfers.AppendMenu(MF_STRING, IDC_FINISHED_UL, CTSTRING(MENU_FINISHED_UPLOADS));
	transfers.AppendMenu(MF_SEPARATOR);
	transfers.AppendMenu(MF_STRING, IDC_NET_STATS, CTSTRING(MENU_NETWORK_STATISTICS));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)transfers, CTSTRING(MENU_TRANSFERS));
	
	CMenuHandle window;
	window.CreatePopupMenu();
	
	window.AppendMenu(MF_STRING, ID_WINDOW_CASCADE, CTSTRING(MENU_CASCADE));
	window.AppendMenu(MF_STRING, ID_WINDOW_TILE_HORZ, CTSTRING(MENU_HORIZONTAL_TILE));
	window.AppendMenu(MF_STRING, ID_WINDOW_TILE_VERT, CTSTRING(MENU_VERTICAL_TILE));
	window.AppendMenu(MF_STRING, ID_WINDOW_ARRANGE, CTSTRING(MENU_ARRANGE));
	window.AppendMenu(MF_STRING, ID_WINDOW_MINIMIZE_ALL, CTSTRING(MENU_MINIMIZE_ALL));
	window.AppendMenu(MF_STRING, ID_WINDOW_RESTORE_ALL, CTSTRING(MENU_RESTORE_ALL));
	window.AppendMenu(MF_SEPARATOR);
	window.AppendMenu(MF_STRING, IDC_CLOSE_DISCONNECTED, CTSTRING(MENU_CLOSE_DISCONNECTED));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_HUBS, CTSTRING(MENU_CLOSE_ALL_HUBS));
	window.AppendMenu(MF_STRING, IDC_CLOSE_HUBS_BELOW, CTSTRING(MENU_CLOSE_HUBS_BELOW));
	window.AppendMenu(MF_STRING, IDC_CLOSE_HUBS_NO_USR, CTSTRING(MENU_CLOSE_HUBS_EMPTY));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_PM, CTSTRING(MENU_CLOSE_ALL_PM));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_PM, CTSTRING(MENU_CLOSE_ALL_OFFLINE_PM));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_DIR_LIST));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_SEARCH_FRAME, CTSTRING(MENU_CLOSE_ALL_SEARCHFRAME));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)window, CTSTRING(MENU_WINDOW));
	
	CMenuHandle help;
	help.CreatePopupMenu();
	
	help.AppendMenu(MF_STRING, ID_APP_ABOUT, CTSTRING(MENU_ABOUT));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)help, CTSTRING(MENU_HLP));
	
	g_fileImage.init();
	g_videoImage.init();
	g_flagImage.init();
	g_userImage.init();
	g_userStateImage.init();
	g_trackerImage.init();
	g_genderImage.init();
	
	Colors::init();
	
	Fonts::init();
	
	if (BOOLSETTING(REGISTER_URL_HANDLER))
		registerHubUrlHandlers();
	
	if (BOOLSETTING(REGISTER_MAGNET_HANDLER))
		registerMagnetHandler();

	if (BOOLSETTING(REGISTER_DCLST_HANDLER))
		registerDclstHandler();
	
	g_hook = SetWindowsHookEx(WH_KEYBOARD, &KeyboardProc, NULL, GetCurrentThreadId());
	
	g_copyHubMenu.CreatePopupMenu();
	g_copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBNAME, CTSTRING(HUB_NAME));
	g_copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBADDRESS, CTSTRING(HUB_ADDRESS));
	g_copyHubMenu.InsertSeparatorFirst(TSTRING(COPY));
	
	UserInfoGuiTraits::init();
}

void Fonts::init()
{
	LOGFONT lf;
	NONCLIENTMETRICS ncm = { sizeof(ncm) };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		memcpy(&lf, &ncm.lfMessageFont, sizeof(LOGFONT));
	else
		GetObject((HFONT) GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);

	g_systemFont = CreateFontIndirect(&lf);
	
	lf.lfWeight = FW_BOLD;
	g_boldFont = CreateFontIndirect(&lf);
	
	decodeFont(Text::toT(SETTING(TEXT_FONT)), lf);
	
	g_font = ::CreateFontIndirect(&lf);
	g_fontHeight = WinUtil::getTextHeight(WinUtil::g_mainWnd, g_font);

	HDC hDC = CreateIC(_T("DISPLAY"), nullptr, nullptr, nullptr);
	g_fontHeightPixl = -MulDiv(lf.lfHeight, GetDeviceCaps(hDC, LOGPIXELSY), 72); // FIXME
	DeleteDC(hDC);
}

void Colors::init()
{
	g_textColor = SETTING(TEXT_COLOR);
	g_bgColor = SETTING(BACKGROUND_COLOR);
	
	if (g_bgBrush) DeleteObject(g_bgBrush);
	g_bgBrush = CreateSolidBrush(Colors::g_bgColor);
	
	CHARFORMAT2 cf;
	memset(&cf, 0, sizeof(CHARFORMAT2));
	cf.cbSize = sizeof(cf);
	cf.dwReserved = 0;
	cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD | CFM_ITALIC;
	cf.dwEffects = 0;
	cf.crBackColor = SETTING(BACKGROUND_COLOR);
	cf.crTextColor = SETTING(TEXT_COLOR);
	
	g_TextStyleTimestamp = cf;
	g_TextStyleTimestamp.crBackColor = SETTING(TEXT_TIMESTAMP_BACK_COLOR);
	g_TextStyleTimestamp.crTextColor = SETTING(TEXT_TIMESTAMP_FORE_COLOR);
	if (SETTING(TEXT_TIMESTAMP_BOLD))
		g_TextStyleTimestamp.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_TIMESTAMP_ITALIC))
		g_TextStyleTimestamp.dwEffects |= CFE_ITALIC;
		
	g_ChatTextGeneral = cf;
	g_ChatTextGeneral.crBackColor = SETTING(TEXT_GENERAL_BACK_COLOR);
	g_ChatTextGeneral.crTextColor = SETTING(TEXT_GENERAL_FORE_COLOR);
	if (SETTING(TEXT_GENERAL_BOLD))
		g_ChatTextGeneral.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_GENERAL_ITALIC))
		g_ChatTextGeneral.dwEffects |= CFE_ITALIC;
		
	g_ChatTextOldHistory = cf;
	g_ChatTextOldHistory.crBackColor = SETTING(TEXT_GENERAL_BACK_COLOR);
	g_ChatTextOldHistory.crTextColor = SETTING(TEXT_GENERAL_FORE_COLOR);
	g_ChatTextOldHistory.yHeight = 5;
	
	g_TextStyleBold = g_ChatTextGeneral;
	g_TextStyleBold.dwEffects = CFE_BOLD;
	
	g_TextStyleMyNick = cf;
	g_TextStyleMyNick.crBackColor = SETTING(TEXT_MYNICK_BACK_COLOR);
	g_TextStyleMyNick.crTextColor = SETTING(TEXT_MYNICK_FORE_COLOR);
	if (SETTING(TEXT_MYNICK_BOLD))
		g_TextStyleMyNick.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_MYNICK_ITALIC))
		g_TextStyleMyNick.dwEffects |= CFE_ITALIC;
		
	g_ChatTextMyOwn = cf;
	g_ChatTextMyOwn.crBackColor = SETTING(TEXT_MYOWN_BACK_COLOR);
	g_ChatTextMyOwn.crTextColor = SETTING(TEXT_MYOWN_FORE_COLOR);
	if (SETTING(TEXT_MYOWN_BOLD))
		g_ChatTextMyOwn.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_MYOWN_ITALIC))
		g_ChatTextMyOwn.dwEffects |= CFE_ITALIC;
		
	g_ChatTextPrivate = cf;
	g_ChatTextPrivate.crBackColor = SETTING(TEXT_PRIVATE_BACK_COLOR);
	g_ChatTextPrivate.crTextColor = SETTING(TEXT_PRIVATE_FORE_COLOR);
	if (SETTING(TEXT_PRIVATE_BOLD))
		g_ChatTextPrivate.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_PRIVATE_ITALIC))
		g_ChatTextPrivate.dwEffects |= CFE_ITALIC;
		
	g_ChatTextSystem = cf;
	g_ChatTextSystem.crBackColor = SETTING(TEXT_SYSTEM_BACK_COLOR);
	g_ChatTextSystem.crTextColor = SETTING(TEXT_SYSTEM_FORE_COLOR);
	if (SETTING(TEXT_SYSTEM_BOLD))
		g_ChatTextSystem.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_SYSTEM_ITALIC))
		g_ChatTextSystem.dwEffects |= CFE_ITALIC;
		
	g_ChatTextServer = cf;
	g_ChatTextServer.crBackColor = SETTING(TEXT_SERVER_BACK_COLOR);
	g_ChatTextServer.crTextColor = SETTING(TEXT_SERVER_FORE_COLOR);
	if (SETTING(TEXT_SERVER_BOLD))
		g_ChatTextServer.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_SERVER_ITALIC))
		g_ChatTextServer.dwEffects |= CFE_ITALIC;
		
	g_ChatTextLog = g_ChatTextGeneral;
	g_ChatTextLog.crTextColor = OperaColors::blendColors(SETTING(TEXT_GENERAL_BACK_COLOR), SETTING(TEXT_GENERAL_FORE_COLOR), 0.4);
	
	g_TextStyleFavUsers = cf;
	g_TextStyleFavUsers.crBackColor = SETTING(TEXT_FAV_BACK_COLOR);
	g_TextStyleFavUsers.crTextColor = SETTING(TEXT_FAV_FORE_COLOR);
	if (SETTING(TEXT_FAV_BOLD))
		g_TextStyleFavUsers.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_FAV_ITALIC))
		g_TextStyleFavUsers.dwEffects |= CFE_ITALIC;
		
	g_TextStyleFavUsersBan = cf;
	g_TextStyleFavUsersBan.crBackColor = SETTING(TEXT_ENEMY_BACK_COLOR);
	g_TextStyleFavUsersBan.crTextColor = SETTING(TEXT_ENEMY_FORE_COLOR);
	if (SETTING(TEXT_ENEMY_BOLD))
		g_TextStyleFavUsersBan.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_ENEMY_ITALIC))
		g_TextStyleFavUsersBan.dwEffects |= CFE_ITALIC;
		
	g_TextStyleOPs = cf;
	g_TextStyleOPs.crBackColor = SETTING(TEXT_OP_BACK_COLOR);
	g_TextStyleOPs.crTextColor = SETTING(TEXT_OP_FORE_COLOR);
	if (SETTING(TEXT_OP_BOLD))
		g_TextStyleOPs.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_OP_ITALIC))
		g_TextStyleOPs.dwEffects |= CFE_ITALIC;
		
	g_TextStyleURL = cf;
	g_TextStyleURL.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_BACKCOLOR | CFM_LINK | CFM_UNDERLINE;
	g_TextStyleURL.crBackColor = SETTING(TEXT_URL_BACK_COLOR);
	g_TextStyleURL.crTextColor = SETTING(TEXT_URL_FORE_COLOR);
	g_TextStyleURL.dwEffects = CFE_LINK | CFE_UNDERLINE;
	if (SETTING(TEXT_URL_BOLD))
		g_TextStyleURL.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_URL_ITALIC))
		g_TextStyleURL.dwEffects |= CFE_ITALIC;
}

void WinUtil::uninit()
{
	UnhookWindowsHookEx(g_hook);
	g_hook = nullptr;
	
	g_tabCtrl = nullptr;
	g_mainWnd = nullptr;
	g_fileImage.uninit();
	g_userImage.uninit();
	g_trackerImage.uninit();
	g_userStateImage.uninit();
	g_genderImage.uninit();
	g_TransferTreeImage.uninit();
	g_flagImage.uninit();
	g_videoImage.uninit();

	Fonts::uninit();
	Colors::uninit();
	
	g_mainMenu.DestroyMenu();
	g_copyHubMenu.DestroyMenu();
	
	UserInfoGuiTraits::uninit();
}

void Fonts::decodeFont(const tstring& setting, LOGFONT &dest)
{
	const StringTokenizer<tstring, TStringList> st(setting, _T(','));
	const auto& sl = st.getTokens();
	
	NONCLIENTMETRICS ncm = { sizeof(ncm) };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		memcpy(&dest, &ncm.lfMessageFont, sizeof(LOGFONT));
	else
		GetObject((HFONT) GetStockObject(DEFAULT_GUI_FONT), sizeof(dest), &dest);

	tstring face;
	if (sl.size() == 4)
	{
		face = sl[0];
		dest.lfHeight = Util::toInt(sl[1]);
		dest.lfWeight = Util::toInt(sl[2]);
		dest.lfItalic = (BYTE)Util::toInt(sl[3]);
	}
	
	if (!face.empty() && face.length() < LF_FACESIZE)
		_tcscpy(dest.lfFaceName, face.c_str());
}

int CALLBACK WinUtil::browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lp*/, LPARAM pData)
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
			break;
	}
	return 0;
}

bool WinUtil::browseDirectory(tstring& target, HWND owner /* = NULL */)
{
	TCHAR buf[MAX_PATH];
	BROWSEINFO bi = {0};
	bi.hwndOwner = owner;
	bi.pszDisplayName = buf;
	bi.lpszTitle = CTSTRING(CHOOSE_FOLDER);
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
	bi.lParam = (LPARAM)target.c_str();
	bi.lpfn = &browseCallbackProc;
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (!pidl) return false;
	bool result = false;
	if (SHGetPathFromIDList(pidl, buf))
	{
		target = buf;
		Util::appendPathSeparator(target);
		result = true;
	}
	CoTaskMemFree(pidl);
	return result;
}

bool WinUtil::browseFile(tstring& target, HWND owner /* = NULL */, bool save /* = true */, const tstring& initialDir /* = Util::emptyString */, const TCHAR* types /* = NULL */, const TCHAR* defExt /* = NULL */)
{
	OPENFILENAME ofn = { 0 }; // common dialog box structure
	target = Text::toT(Util::validateFileName(Text::fromT(target)));
	AutoArray <TCHAR> l_buf(FULL_MAX_PATH);
	_tcscpy_s(l_buf, FULL_MAX_PATH, target.c_str());
	// Initialize OPENFILENAME
	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner = owner;
	ofn.lpstrFile = l_buf.data();
	ofn.lpstrFilter = types;
	ofn.lpstrDefExt = defExt;
	ofn.nFilterIndex = 1;
	
	if (!initialDir.empty())
	{
		ofn.lpstrInitialDir = initialDir.c_str();
	}
	ofn.nMaxFile = FULL_MAX_PATH;
	ofn.Flags = (save ? 0 : OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);
	
	// Display the Open dialog box.
	if ((save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn)) != FALSE)
	{
		target = ofn.lpstrFile;
		return true;
	}
	else
	{
		dcdebug("Error WinUtil::browseFile CommDlgExtendedError() = %x\n", CommDlgExtendedError());
	}
	return false;
}

tstring WinUtil::encodeFont(const LOGFONT& font)
{
	tstring res(font.lfFaceName);
	res += _T(',');
	res += Util::toStringT(font.lfHeight);
	res += _T(',');
	res += Util::toStringT(font.lfWeight);
	res += _T(',');
	res += Util::toStringT(font.lfItalic);
	return res;
}

void WinUtil::setClipboard(const tstring& str)
{
	if (!::OpenClipboard(g_mainWnd))
	{
		return;
	}
	
	EmptyClipboard();
	
	// Allocate a global memory object for the text.
	HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (str.size() + 1) * sizeof(TCHAR));
	if (hglbCopy == NULL)
	{
		CloseClipboard();
		return;
	}
	
	// Lock the handle and copy the text to the buffer.
	TCHAR* lptstrCopy = (TCHAR*)GlobalLock(hglbCopy);
	_tcscpy(lptstrCopy, str.c_str());
	GlobalUnlock(hglbCopy);
	
	// Place the handle on the clipboard.
	SetClipboardData(CF_UNICODETEXT, hglbCopy);
	
	CloseClipboard();
}

int WinUtil::splitTokensWidth(int* result, const string& tokens, int maxItems, int defaultValue) noexcept
{
	int count = splitTokens(result, tokens, maxItems);
	for (int k = 0; k < count; ++k)
		if (result[k] <= 0 || result[k] > 2000)
			result[k] = defaultValue;
	return count;
}

int WinUtil::splitTokens(int* result, const string& tokens, int maxItems) noexcept
{
	SimpleStringTokenizer<char> t(tokens, ',');
	string tok;
	int k = 0;
	while (k < maxItems && t.getNextToken(tok))
		result[k++] = Util::toInt(tok);
	return k;
}

bool WinUtil::getUCParams(HWND parent, const UserCommand& uc, StringMap& sm)
{
	string::size_type i = 0;
	StringMap done;
	
	while ((i = uc.getCommand().find("%[line:", i)) != string::npos)
	{
		i += 7;
		string::size_type j = uc.getCommand().find(']', i);
		if (j == string::npos)
			break;
			
		string name = uc.getCommand().substr(i, j - i);
		if (done.find(name) == done.end())
		{
			LineDlg dlg;
			dlg.title = Text::toT(Util::toString(" > ", uc.getDisplayName()));
			dlg.description = Text::toT(name);
			dlg.line = Text::toT(sm["line:" + name]);

			if (uc.isSet(UserCommand::FLAG_FROM_ADC_HUB))
			{
				Util::replace(_T("\\\\"), _T("\\"), dlg.description);
				Util::replace(_T("\\s"), _T(" "), dlg.description);
			}

			if (dlg.DoModal(parent) != IDOK) return false;

			string str = Text::fromT(dlg.line);
			sm["line:" + name] = str;
			done[name] = str;
		}
		i = j + 1;
	}
	i = 0;
	while ((i = uc.getCommand().find("%[kickline:", i)) != string::npos)
	{
		i += 11;
		string::size_type j = uc.getCommand().find(']', i);
		if (j == string::npos)
			break;
			
		string name = uc.getCommand().substr(i, j - i);
		if (done.find(name) == done.end())
		{
			KickDlg dlg;
			dlg.title = Text::toT(Util::toString(" > ", uc.getDisplayName()));
			dlg.description = Text::toT(name);

			if (uc.isSet(UserCommand::FLAG_FROM_ADC_HUB))
			{
				Util::replace(_T("\\\\"), _T("\\"), dlg.description);
				Util::replace(_T("\\s"), _T(" "), dlg.description);
			}
			
			if (dlg.DoModal(parent) != IDOK) return false;

			string str = Text::fromT(dlg.line);
			sm["kickline:" + name] = str;
			done[name] = str;
		}
		i = j + 1;
	}
	return true;
}

void WinUtil::copyMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	if (!aFile.empty())
	{
		setClipboard(Text::toT(Util::getMagnet(aHash, aFile, aSize)));
	}
}

void WinUtil::searchFile(const string& file)
{
	SearchFrame::openWindow(Text::toT(file), 0, SIZE_DONTCARE, FILE_TYPE_ANY);
}

void WinUtil::searchHash(const TTHValue& hash)
{
	SearchFrame::openWindow(Text::toT(hash.toBase32()), 0, SIZE_DONTCARE, FILE_TYPE_TTH);
}

static bool regReadString(HKEY key, const TCHAR* name, tstring& value)
{
	TCHAR buf[512];
	DWORD size = sizeof(buf);
	DWORD type;
	if (RegQueryValueEx(key, name, nullptr, &type, (BYTE *) buf, &size) != ERROR_SUCCESS ||
	    (type != REG_SZ && type != REG_EXPAND_SZ))
	{
		value.clear();
		return false;
	}
	size /= sizeof(TCHAR);
	if (size && buf[size-1] == 0) size--;
	value.assign(buf, size);
	return true;
}

static bool regWriteString(HKEY key, const TCHAR* name, const TCHAR* value, DWORD len)
{
	return RegSetValueEx(key, name, 0, REG_SZ, (const BYTE *) value, (len + 1) * sizeof(TCHAR)) == ERROR_SUCCESS;
}

static inline bool regWriteString(HKEY key, const TCHAR* name, const tstring& str)
{
	return regWriteString(key, name, str.c_str(), str.length());
}

static bool registerUrlHandler(const TCHAR* proto, const TCHAR* description)
{
	HKEY key = nullptr;
	tstring value;
	tstring exePath = Util::getModuleFileName();
	tstring app = _T('\"') + exePath + _T("\" /magnet \"%1\"");
	tstring pathProto = _T("SOFTWARE\\Classes\\");
	pathProto += proto;
	tstring pathCommand = pathProto + _T("\\Shell\\Open\\Command");
	
	if (RegOpenKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		regReadString(key, nullptr, value);
		RegCloseKey(key);
	}
	
	if (stricmp(app, value) == 0)
		return true;

	if (RegCreateKeyEx(HKEY_CURRENT_USER, pathProto.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
		return false;
		
	bool result = false;
	do
	{
		if (!regWriteString(key, nullptr, description, _tcslen(description))) break;
		if (!regWriteString(key, _T("URL Protocol"), Util::emptyStringT)) break;
		RegCloseKey(key);
		key = nullptr;
		
		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, app)) break;
		RegCloseKey(key);
		key = nullptr;
		
		tstring pathIcon = pathProto + _T("\\DefaultIcon");
		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathIcon.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, exePath)) break;
		result = true;
	} while (0);

	if (key) RegCloseKey(key);
	return result;
}

void WinUtil::registerHubUrlHandlers()
{
	if (registerUrlHandler(_T("dchub"), _T("URL:Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "dchub"));

	if (registerUrlHandler(_T("nmdcs"), _T("URL:Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "nmdcs"));

	if (registerUrlHandler(_T("adc"), _T("URL:Advanced Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "adc"));

	if (registerUrlHandler(_T("adcs"), _T("URL:Advanced Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "adcs"));
}

static void internalDeleteRegistryKey(const tstring& key)
{
	tstring path = _T("SOFTWARE\\Classes\\") + key;
	if (SHDeleteKey(HKEY_CURRENT_USER, path.c_str()) != ERROR_SUCCESS)
		LogManager::message(STRING_F(ERROR_DELETING_REGISTRY_KEY, Text::fromT(path) % Util::translateError()));
}

void WinUtil::unregisterHubUrlHandlers()
{
	internalDeleteRegistryKey(_T("dchub"));
	internalDeleteRegistryKey(_T("nmdcs"));
	internalDeleteRegistryKey(_T("adc"));
	internalDeleteRegistryKey(_T("adcs"));
	hubUrlHandlersRegistered = false;
}

void WinUtil::registerMagnetHandler()
{
	if (registerUrlHandler(_T("magnet"), _T("URL:Magnet Link")))
		magnetHandlerRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "magnet"));
}

void WinUtil::unregisterMagnetHandler()
{
	internalDeleteRegistryKey(_T("magnet"));
	magnetHandlerRegistered = false;
}

static bool registerFileHandler(const TCHAR* names[], const TCHAR* description)
{
	HKEY key = nullptr;
	tstring value;
	tstring exePath = Util::getModuleFileName();
	tstring app = _T('\"') + exePath + _T("\" /open \"%1\"");
	tstring pathExt = _T("SOFTWARE\\Classes\\");
	pathExt += names[0];
	tstring pathCommand = pathExt + _T("\\Shell\\Open\\Command");
	
	if (RegOpenKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		regReadString(key, nullptr, value);
		RegCloseKey(key);
	}
	
	if (stricmp(app, value) == 0)
		return true;

	if (RegCreateKeyEx(HKEY_CURRENT_USER, pathExt.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
		return false;
		
	bool result = false;
	do
	{
		if (!regWriteString(key, nullptr, description, _tcslen(description))) break;
		RegCloseKey(key);
		key = nullptr;
		
		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, app)) break;
		RegCloseKey(key);
		key = nullptr;
		
		tstring pathIcon = pathExt + _T("\\DefaultIcon");
		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathIcon.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, exePath)) break;
		RegCloseKey(key);
		key = nullptr;

		int i = 1;
		DWORD len = _tcslen(names[0]);
		while (true)
		{
			if (!names[i])
			{
				result = true;
				break;
			}
			tstring path = _T("SOFTWARE\\Classes\\");
			path += names[i];
			if (RegCreateKeyEx(HKEY_CURRENT_USER, path.c_str(), 0, nullptr,
			    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
			if (!regWriteString(key, nullptr, names[0], len)) break;
			RegCloseKey(key);
			key = nullptr;
			i++;
		}
	} while (0);

	if (key) RegCloseKey(key);
	return result;
}

static const TCHAR* dcLstNames[] = { _T("DcLst metafile"), _T(".dclst"), _T(".dcls"), nullptr };

void WinUtil::registerDclstHandler()
{
	if (registerFileHandler(dcLstNames, CTSTRING(DCLST_DESCRIPTION)))
		dclstHandlerRegistered = true;
	else
		LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
}

void WinUtil::unregisterDclstHandler()
{
	int i = 0;
	while (dcLstNames[i])
	{
		internalDeleteRegistryKey(dcLstNames[i]);
		i++;
	}
	dclstHandlerRegistered = false;
}

void WinUtil::playSound(const string& soundFile, bool beep /* = false */)
{
	if (!soundFile.empty())
		PlaySound(Text::toT(soundFile).c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
	else if (beep)
		MessageBeep(MB_OK);
}

void WinUtil::openFile(const tstring& file)
{
	openFile(file.c_str());
}

void WinUtil::openFile(const TCHAR* file)
{
	::ShellExecute(NULL, _T("open"), file, NULL, NULL, SW_SHOWNORMAL);
}

static void shellExecute(const tstring& url)
{
#ifdef FLYLINKDC_USE_TORRENT
	if (url.find(_T("magnet:?xt=urn:btih")) != tstring::npos)
	{
		DownloadManager::getInstance()->add_torrent_file(_T(""), url);
		return;
	}
#endif
	::ShellExecute(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool WinUtil::openLink(const tstring& uri)
{
	if (parseMagnetUri(uri) || parseDchubUrl(uri))
		return true;

	static const Tags g_ExtLinks[] =
	{
		EXT_URL_LIST(),
	};
	for (size_t i = 0; i < _countof(g_ExtLinks); ++i)
	{
		if (strnicmp(uri, g_ExtLinks[i].tag, g_ExtLinks[i].tag.length()) == 0)
		{
			shellExecute(uri);
			return true;
		}
	}
	return false;
}

bool WinUtil::parseDchubUrl(const tstring& url)
{
	uint16_t port;
	string proto, host, file, query, fragment;
	Util::decodeUrl(Text::fromT(url), proto, host, port, file, query, fragment);
	if (!Util::getHubProtocol(proto) || host.empty() || port == 0) return false;

	const string formattedUrl = Util::formatDchubUrl(proto, host, port);
	
	RecentHubEntry r;
	r.setServer(formattedUrl);
	FavoriteManager::getInstance()->addRecent(r);
	HubFrame::openHubWindow(formattedUrl);

	if (!file.empty())
	{
		if (file[0] == '/') // Remove any '/' in from of the file
			file = file.substr(1);
				
		string path;
		string nick;
		const string::size_type i = file.find('/', 0);
		if (i != string::npos)
		{
			path = file.substr(i);
			nick = file.substr(0, i);
		}
		if (!nick.empty())
		{
			const string hostPort = host + ":" + Util::toString(port);
			const UserPtr user = ClientManager::findLegacyUser(nick, hostPort);
			if (user)
			{
				try
				{
					QueueManager::getInstance()->addList(user, QueueItem::FLAG_CLIENT_VIEW, path);
				}
				catch (const Exception&)
				{
					// Ignore for now...
				}
			}
			// @todo else report error
		}
	}
	return true;
}

static WinUtil::DefinedMagnetAction getMagnetAction(int setting)
{
	switch (setting)
	{
		case SettingsManager::MAGNET_ACTION_DOWNLOAD:
			return WinUtil::MA_DOWNLOAD;
		case SettingsManager::MAGNET_ACTION_SEARCH:
			return WinUtil::MA_SEARCH;
		case SettingsManager::MAGNET_ACTION_DOWNLOAD_AND_OPEN:
			return WinUtil::MA_OPEN;
	}
	return WinUtil::MA_ASK;
}

bool WinUtil::parseMagnetUri(const tstring& aUrl, DefinedMagnetAction action /* = MA_DEFAULT */)
{
	if (Util::isMagnetLink(aUrl))
	{
#ifdef FLYLINKDC_USE_TORRENT
		if (Util::isTorrentLink(aUrl))
		{
			DownloadManager::getInstance()->add_torrent_file(_T(""), aUrl);
		}
		else
#endif
		{
			const string url = Text::fromT(aUrl);
			LogManager::message(STRING(MAGNET_DLG_TITLE) + ": " + url);
			MagnetLink magnet;
			const char* fhash;
			if (!magnet.parse(url) || (fhash = magnet.getTTH()) == nullptr)
			{
				MessageBox(g_mainWnd, CTSTRING(MAGNET_DLG_TEXT_BAD), CTSTRING(MAGNET_DLG_TITLE), MB_OK | MB_ICONEXCLAMATION);
				return false;
			}
			string fname = magnet.getFileName();
			if (!magnet.exactSource.empty())
				WinUtil::parseDchubUrl(Text::toT(magnet.exactSource));

			const bool isDclst = Util::isDclstFile(fname);
			if (action == MA_DEFAULT)
			{
				if (!isDclst)
				{
					if (BOOLSETTING(MAGNET_ASK))
						action = MA_ASK;
					else
						action = getMagnetAction(SETTING(MAGNET_ACTION));
				}
				else
				{
					if (BOOLSETTING(DCLST_ASK))
						action = MA_ASK;
					else
						action = getMagnetAction(SETTING(DCLST_ACTION));
				}
			}
			if (action == MA_ASK)
			{
				MagnetDlg dlg(TTHValue(fhash), Text::toT(fname), magnet.exactLength, magnet.dirSize, isDclst);
				dlg.DoModal(g_mainWnd);
				action = dlg.getAction();
				if (action == WinUtil::MA_DEFAULT) return true;
				fname = Text::fromT(dlg.getFileName());
			}
			switch (action)
			{
				case MA_DOWNLOAD:
				case MA_OPEN:
					try
					{
						bool getConnFlag = true;
						QueueItem::MaskType flags = isDclst ? QueueItem::FLAG_DCLST_LIST : 0;
						if (action == MA_OPEN)
							flags |= QueueItem::FLAG_CLIENT_VIEW;
						else if (isDclst)
							flags |= QueueItem::FLAG_DOWNLOAD_CONTENTS;
						QueueManager::getInstance()->add(fname, magnet.exactLength, TTHValue(fhash), HintedUser(),
							flags, QueueItem::DEFAULT, true, getConnFlag);
					}
					catch (const Exception& e)
					{
						LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
					}
					break;

				case MA_SEARCH:
					SearchFrame::openWindow(Text::toT(fhash), 0, SIZE_DONTCARE, FILE_TYPE_TTH);
					break;
			}
		}
		return true;
	}
	return false;
}

void WinUtil::openFileList(const tstring& filename, DefinedMagnetAction Action /* = MA_DEFAULT */)
{
	const UserPtr u = DirectoryListing::getUserFromFilename(Text::fromT(filename));
	DirectoryListingFrame::openWindow(filename, Util::emptyStringT, HintedUser(u, Util::emptyString), 0, Util::isDclstFile(filename));
}

int WinUtil::textUnderCursor(POINT p, CEdit& ctrl, tstring& x)
{
	const int i = ctrl.CharFromPos(p);
	const int line = ctrl.LineFromChar(i);
	const int c = LOWORD(i) - ctrl.LineIndex(line);
	const int len = ctrl.LineLength(i) + 1;
	if (len < 3)
	{
		return 0;
	}
	
	x.resize(len + 1);
	x.resize(ctrl.GetLine(line, &x[0], len + 1));
	
	string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
	if (start == string::npos)
		start = 0;
	else
		start++;
		
	return start;
}

// !SMT!-UI (todo: disable - this routine does not save column visibility)
void WinUtil::saveHeaderOrder(CListViewCtrl& ctrl, SettingsManager::StrSetting order,
                              SettingsManager::StrSetting widths, int n,
                              int* indexes, int* sizes) noexcept
{
	string tmp;
	
	ctrl.GetColumnOrderArray(n, indexes);
	int i;
	for (i = 0; i < n; ++i)
	{
		tmp += Util::toString(indexes[i]);
		tmp += ',';
	}
	tmp.erase(tmp.size() - 1, 1);
	SettingsManager::set(order, tmp);
	tmp.clear();
	const int nHeaderItemsCount = ctrl.GetHeader().GetItemCount();
	for (i = 0; i < n; ++i)
	{
		sizes[i] = ctrl.GetColumnWidth(i);
		if (i >= nHeaderItemsCount) // Not exist column
			sizes[i] = 0;
		tmp += Util::toString(sizes[i]);
		tmp += ',';
	}
	tmp.erase(tmp.size() - 1, 1);
	SettingsManager::set(widths, tmp);
}

static pair<tstring, bool> formatHubNames(const StringList& hubs)
{
	if (hubs.empty())
	{
		return pair<tstring, bool>(TSTRING(OFFLINE), false);
	}
	else
	{
		return pair<tstring, bool>(Text::toT(Util::toString(hubs)), true);
	}
}

pair<tstring, bool> WinUtil::getHubNames(const CID& cid, const string& hintUrl)
{
	return formatHubNames(ClientManager::getHubNames(cid, hintUrl));
}

pair<tstring, bool> WinUtil::getHubNames(const UserPtr& u, const string& hintUrl)
{
	return getHubNames(u->getCID(), hintUrl);
}

pair<tstring, bool> WinUtil::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
	return formatHubNames(ClientManager::getHubNames(cid, hintUrl, priv));
}

pair<tstring, bool> WinUtil::getHubNames(const HintedUser& user)
{
	return getHubNames(user.user, user.hint);
}

void WinUtil::getContextMenuPos(CListViewCtrl& aList, POINT& aPt)
{
	int pos = aList.GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
	if (pos >= 0)
	{
		CRect lrc;
		aList.GetItemRect(pos, &lrc, LVIR_LABEL);
		aPt.x = lrc.left;
		aPt.y = lrc.top + (lrc.Height() / 2);
	}
	else
	{
		aPt.x = aPt.y = 0;
	}
	aList.ClientToScreen(&aPt);
}

void WinUtil::getContextMenuPos(CTreeViewCtrl& aTree, POINT& aPt)
{
	CRect trc;
	HTREEITEM ht = aTree.GetSelectedItem();
	if (ht)
	{
		aTree.GetItemRect(ht, &trc, TRUE);
		aPt.x = trc.left;
		aPt.y = trc.top + (trc.Height() / 2);
	}
	else
	{
		aPt.x = aPt.y = 0;
	}
	aTree.ClientToScreen(&aPt);
}
void WinUtil::getContextMenuPos(CEdit& aEdit, POINT& aPt)
{
	CRect erc;
	aEdit.GetRect(&erc);
	aPt.x = erc.Width() / 2;
	aPt.y = erc.Height() / 2;
	aEdit.ClientToScreen(&aPt);
}

void WinUtil::openFolder(const tstring& file)
{
	/* TODO: needs test in last wine!
	if (CompatibilityManager::isWine()) // [+]IRainman
	    ::ShellExecute(NULL, NULL, Text::toT(Util::getFilePath(ii->entry->getTarget())).c_str(), NULL, NULL, SW_SHOWNORMAL);
	*/
	if (File::isExist(file))
		::ShellExecute(NULL, NULL, _T("explorer.exe"), (_T("/e, /select, \"") + file + _T('\"')).c_str(), NULL, SW_SHOWNORMAL);
	else
		::ShellExecute(NULL, NULL, _T("explorer.exe"), (_T("/e, \"") + Util::getFilePath(file) + _T('\"')).c_str(), NULL, SW_SHOWNORMAL);
}

void WinUtil::openLog(const string& dir, const StringMap& params, const tstring& noLogMessage)
{
	const auto file = Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatParams(dir, params, true)));
	if (File::isExist(file))
		WinUtil::openFile(file);
	else
		MessageBox(nullptr, noLogMessage.c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONINFORMATION);
}

void Preview::setupPreviewMenu(const string& target)
{
	dcassert(g_previewAppsSize == 0);
	g_previewAppsSize = 0;
	
	const auto targetLower = Text::toLower(target);
	
	const auto& lst = FavoriteManager::getPreviewApps();
	for (size_t i = 0; i < lst.size(); ++i)
	{
		const auto tok = Util::splitSettingAndLower(lst[i]->extension);
		if (tok.empty())
		{
			g_previewMenu.AppendMenu(MF_STRING, IDC_PREVIEW_APP + i, Text::toT(lst[i]->name).c_str());
			g_previewAppsSize++;
		}
		else
			for (auto si = tok.cbegin(); si != tok.cend(); ++si)
			{
				if (Util::checkFileExt(targetLower, *si))
				{
					g_previewMenu.AppendMenu(MF_STRING, IDC_PREVIEW_APP + i, Text::toT(lst[i]->name).c_str());
					g_previewAppsSize++;
					break;
				}
			}
	}
}

template <class string_type>
static void AppendQuotesToPath(string_type& path)
{
	if (path.length() < 1)
		return;
		
	if (path[0] != '"')
		path = '\"' + path;
		
	if (path.back() != '"')
		path += '\"';
}

void Preview::runPreviewCommand(WORD wID, const string& file)
{
	if (wID < IDC_PREVIEW_APP) return;
	wID -= IDC_PREVIEW_APP;
	const auto& lst = FavoriteManager::getPreviewApps();
	if (wID >= lst.size()) return;

	const auto& application = lst[wID]->application;
	const auto& arguments = lst[wID]->arguments;
	StringMap fileParams;
		
	string dir = Util::getFilePath(file);
	AppendQuotesToPath(dir);
	fileParams["dir"] = dir;

	string quotedFile = file;
	AppendQuotesToPath(quotedFile);
	fileParams["file"] = quotedFile;
	
	string expandedArguments = Util::formatParams(arguments, fileParams, false);
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("Running command: " + application + " " + expandedArguments, false);
	::ShellExecute(NULL, NULL, Text::toT(application).c_str(), Text::toT(expandedArguments).c_str(), Text::toT(dir).c_str(), SW_SHOWNORMAL);
}

bool WinUtil::shutDown(int action)
{
	// Prepare for shutdown
	
	// [-] IRainman old code:
	//UINT iForceIfHung = 0;
	//OSVERSIONINFO osvi = {0};
	//osvi.dwOSVersionInfoSize = sizeof(osvi);
	//if (GetVersionEx(&osvi) != 0 && osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
	//{
	UINT iForceIfHung = 0x00000010;
	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), &hToken);
	
	LUID luid;
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid);
	
	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tp.Privileges[0].Luid = luid;
	AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);
	CloseHandle(hToken);
	//}
	
	// Shutdown
	switch (action)
	{
		case 0:
		{
			action = EWX_POWEROFF;
			break;
		}
		case 1:
		{
			action = EWX_LOGOFF;
			break;
		}
		case 2:
		{
			action = EWX_REBOOT;
			break;
		}
		case 3:
		{
			SetSuspendState(false, false, false);
			return true;
		}
		case 4:
		{
			SetSuspendState(true, false, false);
			return true;
		}
		case 5:
		{
			typedef bool (CALLBACK * LPLockWorkStation)(void);
			LPLockWorkStation _d_LockWorkStation = (LPLockWorkStation)GetProcAddress(LoadLibrary(_T("user32")), "LockWorkStation");
			if (_d_LockWorkStation)
			{
				_d_LockWorkStation();
			}
			return true;
		}
	}
	
	if (ExitWindowsEx(action | iForceIfHung, 0) == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

int WinUtil::setButtonPressed(int nID, bool bPressed /* = true */)
{
	if (nID == -1 || !MainFrame::getMainFrame()->getToolBar().IsWindow())
		return -1;
		
	MainFrame::getMainFrame()->getToolBar().CheckButton(nID, bPressed);
	return 0;
}

void WinUtil::activateMDIChild(HWND hWnd)
{
	::SendMessage(g_mdiClient, WM_SETREDRAW, FALSE, 0);
	::SendMessage(g_mdiClient, WM_MDIACTIVATE, (WPARAM) hWnd, 0);
	::SendMessage(g_mdiClient, WM_SETREDRAW, TRUE, 0);
	::RedrawWindow(g_mdiClient, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

tstring WinUtil::getNicks(const CID& cid, const string& hintUrl)
{
	const auto nicks = ClientManager::getNicks(cid, hintUrl);
	if (nicks.empty())
		return Util::emptyStringT;
	else
		return Text::toT(Util::toString(nicks));
}

tstring WinUtil::getNicks(const UserPtr& u, const string& hintUrl)
{
	dcassert(u);
	if (u)
		return getNicks(u->getCID(), hintUrl);
	return Util::emptyStringT;
}

tstring WinUtil::getNicks(const CID& cid, const string& hintUrl, bool priv)
{
	const auto nicks = ClientManager::getNicks(cid, hintUrl, priv);
	if (nicks.empty())
		return Util::emptyStringT;
	else
		return Text::toT(Util::toString(nicks));
}

tstring WinUtil::getNicks(const HintedUser& user)
{
	return getNicks(user.user, user.hint);
}

void WinUtil::fillAdapterList(bool v6, CComboBox& bindCombo, const string& bindAddress)
{
	vector<Util::AdapterInfo> adapters;
	Util::getNetworkAdapters(v6, adapters);
	const string defaultAdapter("0.0.0.0");
	if (std::find_if(adapters.cbegin(), adapters.cend(),
		[&defaultAdapter](const auto &v) { return v.ip == defaultAdapter; }) == adapters.cend())
	{
		adapters.insert(adapters.begin(), Util::AdapterInfo(TSTRING(DEFAULT_ADAPTER), defaultAdapter, 0));
	}
	int selected = -1;
	for (size_t i = 0; i < adapters.size(); ++i)
	{
		tstring text = Text::toT(adapters[i].ip);
		if (!adapters[i].adapterName.empty())
		{
			text += _T(" (");
			text += adapters[i].adapterName;
			text += _T(')');
		}
		bindCombo.AddString(text.c_str());
		if (adapters[i].ip == bindAddress)
			selected = i;
	}
	if (bindAddress.empty()) selected = 0;
	if (selected == -1)
		selected = bindCombo.InsertString(-1, Text::toT(bindAddress).c_str());
	bindCombo.SetCurSel(selected);
}

string WinUtil::getSelectedAdapter(const CComboBox& bindCombo)
{
	tstring ts;
	WinUtil::getWindowText(bindCombo, ts);
	string str = Text::fromT(ts);
	boost::trim(str);
	string::size_type pos = str.find(' ');
	if (pos != string::npos) str.erase(pos);
	return str;
}

void WinUtil::fillCharsetList(CComboBox& comboBox, int selected, bool onlyUTF8, bool inFavs)
{
	static const ResourceManager::Strings charsetNames[Text::NUM_SUPPORTED_CHARSETS] =
	{
		ResourceManager::ENCODING_CP1250,
		ResourceManager::ENCODING_CP1251,
		ResourceManager::ENCODING_CP1252,
		ResourceManager::ENCODING_CP1253,
		ResourceManager::ENCODING_CP1254,
		ResourceManager::ENCODING_CP1255,
		ResourceManager::ENCODING_CP1256,
		ResourceManager::ENCODING_CP1257,
		ResourceManager::ENCODING_CP1258,
		ResourceManager::ENCODING_CP936,
		ResourceManager::ENCODING_CP950
	};

	int index, selIndex = -1;
	if (!onlyUTF8)
	{
		tstring str;
		if (inFavs)
		{
			int defaultCharset = Text::charsetFromString(SETTING(DEFAULT_CODEPAGE));
			if (defaultCharset == Text::CHARSET_SYSTEM_DEFAULT) defaultCharset = Text::getDefaultCharset();
			str = TSTRING_F(ENCODING_DEFAULT, defaultCharset);
		}
		else
			str = TSTRING_F(ENCODING_SYSTEM_DEFAULT, Text::getDefaultCharset());
		index = comboBox.AddString(str.c_str());
		if (selected == Text::CHARSET_SYSTEM_DEFAULT) selIndex = index;
		comboBox.SetItemData(index, Text::CHARSET_SYSTEM_DEFAULT);
	}
	index = comboBox.AddString(CTSTRING(ENCODING_UTF8));
	if (selected == Text::CHARSET_UTF8) selIndex = index;
	comboBox.SetItemData(index, Text::CHARSET_UTF8);	
	if (!onlyUTF8)
		for (int i = 0; i < Text::NUM_SUPPORTED_CHARSETS; i++)
		{
			int charset = Text::supportedCharsets[i];
			tstring str = TSTRING_I(charsetNames[i]);
			str += _T(" (");
			str += Util::toStringT(charset);
			str += _T(')');
			index = comboBox.AddString(str.c_str());
			if (selected == charset) selIndex = index;
			comboBox.SetItemData(index, charset);
		}

	if (selIndex < 0 || onlyUTF8) selIndex = 0;
	comboBox.SetCurSel(selIndex);
}

int WinUtil::getSelectedCharset(const CComboBox& comboBox)
{
	int selIndex = comboBox.GetCurSel();
	return comboBox.GetItemData(selIndex);
}

// [+] InfinitySky.
void WinUtil::GetTimeValues(CComboBox& p_ComboBox)
{
	const bool use12hrsFormat = BOOLSETTING(USE_12_HOUR_FORMAT);
	p_ComboBox.AddString(CTSTRING(MIDNIGHT));
	
	if (use12hrsFormat)
		for (int i = 1; i < 12; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(" AM")).c_str());
			
	else
		for (int i = 1; i < 12; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(":00")).c_str());
			
	p_ComboBox.AddString(CTSTRING(NOON));
	
	if (use12hrsFormat)
		for (int i = 1; i < 12; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(" PM")).c_str());
			
	else
		for (int i = 13; i < 24; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(":00")).c_str());
}

#ifdef SSA_SHELL_INTEGRATION
tstring WinUtil::getShellExtDllPath()
{
	static const auto filePath = Text::toT(Util::getExePath()) + _T("BLShellExt")
#if defined(_WIN64)
	                             _T("_x64")
#endif
	                             _T(".dll");
	                             
	return filePath;
}

bool WinUtil::registerShellExt(bool unregister)
{
	typedef HRESULT (WINAPIV *Registration)();
	
	bool result = false;
	HINSTANCE hModule = nullptr;
	try
	{
		const auto filePath = WinUtil::getShellExtDllPath();
		hModule = LoadLibrary(filePath.c_str());
		if (hModule != nullptr)
		{
			Registration reg = nullptr;
			reg = (Registration) GetProcAddress((HMODULE)hModule, unregister ? "DllUnregisterServer" : "DllRegisterServer");
			if (reg)
				result = SUCCEEDED(reg());
			FreeLibrary(hModule);
		}
	}
	catch (...)
	{
		if (hModule)
			FreeLibrary(hModule);
	}
	return result;
}
#endif // SSA_SHELL_INTEGRATION

bool WinUtil::runElevated(
    HWND    hwnd,
    LPCTSTR pszPath,
    LPCTSTR pszParameters, //   = NULL,
    LPCTSTR pszDirectory)  //   = NULL );
{

	SHELLEXECUTEINFO shex = { 0 };
	
	shex.cbSize         = sizeof(SHELLEXECUTEINFO);
	shex.fMask          = SEE_MASK_NOCLOSEPROCESS; // 0;
	shex.hwnd           = hwnd;
	shex.lpVerb         = _T("runas");
	shex.lpFile         = pszPath;
	shex.lpParameters   = pszParameters;
	shex.lpDirectory    = pszDirectory;
	shex.nShow          = SW_NORMAL;
	
	bool bRet = ::ShellExecuteEx(&shex) != FALSE;
	
	//if (shex.hProcess)
	//{
	//  BOOL result = FALSE;
	//  while (1)
	//  {
	//      if (::IsProcessInJob(shex.hProcess, NULL, &result))
	//      {
	//          if (!result)
	//              break;
	//          ::Sleep(10);
	//      }else
	//          break;
	//  }
	//}
	::Sleep(1000);
	return bRet;
}

bool WinUtil::createShortcut(const tstring& targetFile, const tstring& targetArgs,
                             const tstring& linkFile, const tstring& description,
                             int showMode, const tstring& workDir,
                             const tstring& iconFile, int iconIndex)
{
	if (targetFile.empty() || linkFile.empty()) return false;

	bool result = false;
	IShellLink*   pShellLink = nullptr;
	IPersistFile* pPersistFile = nullptr;

	do
	{
		if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**) &pShellLink))
			|| !pShellLink) break;

		if (FAILED(pShellLink->SetPath(targetFile.c_str()))) break;
		if (FAILED(pShellLink->SetArguments(targetArgs.c_str()))) break;
		if (!description.empty() && FAILED(pShellLink->SetDescription(description.c_str()))) break;
		if (showMode > 0 && FAILED(pShellLink->SetShowCmd(showMode))) break;
		if (!workDir.empty() && FAILED(pShellLink->SetWorkingDirectory(workDir.c_str()))) break;
		if (!iconFile.empty() && iconIndex >= 0 && FAILED(pShellLink->SetIconLocation(iconFile.c_str(), iconIndex))) break;

		if (FAILED(pShellLink->QueryInterface(IID_IPersistFile, (void**) &pPersistFile))
			|| !pPersistFile) break;
		if (FAILED(pPersistFile->Save(linkFile.c_str(), TRUE))) break;

		result = true;	
	} while (0);
	
	if (pPersistFile)
		pPersistFile->Release();
	if (pShellLink)
		pShellLink->Release();
	
	return result;
}

bool WinUtil::autoRunShortcut(bool create)
{
	tstring linkFile = getAutoRunShortcutName();
	if (create)
	{
		if (!File::isExist(linkFile))
		{
			tstring targetFile = Util::getModuleFileName();
			tstring workDir = Util::getFilePath(targetFile);
			tstring description = getAppNameVerT();
			Util::appendPathSeparator(workDir);
			return createShortcut(targetFile, _T(""), linkFile, description, 0, workDir, targetFile, 0);
		}
	}
	else
	{
		if (File::isExist(linkFile))
			return File::deleteFile(linkFile);
	}	
	return true;
}

bool WinUtil::isAutoRunShortcutExists()
{
	return File::isExist(getAutoRunShortcutName());
}

tstring WinUtil::getAutoRunShortcutName()
{
	// Name: {userstartup}\FlylinkDC++{code:Postfix| }; Filename: {app}\FlylinkDC{code:Postfix|_}.exe; Tasks: startup; WorkingDir: {app}
	// CSIDL_STARTUP
	TCHAR startupPath[MAX_PATH];
	if (!SHGetSpecialFolderPath(NULL, startupPath, CSIDL_STARTUP, TRUE))
		return Util::emptyStringT;
		
	tstring result = startupPath;
	Util::appendPathSeparator(result);
	result += getAppNameT();
#if defined(_WIN64)
	result += _T("_x64");
#endif
	result += _T(".lnk");
	
	return result;
}

void WinUtil::getWindowText(HWND hwnd, tstring& text)
{
	int len = GetWindowTextLength(hwnd);
	if (len <= 0)
	{
		text.clear();
		return;
	}
	text.resize(len + 1);
	len = GetWindowText(hwnd, &text[0], len + 1);
	text.resize(len);
}

static inline int fromHexChar(int ch)
{
	if (ch >= '0' && ch <= '9') return ch-'0';
	if (ch >= 'a' && ch <= 'f') return ch-'a'+10;
	if (ch >= 'A' && ch <= 'F') return ch-'A'+10;
	return -1;
}

bool Colors::getColorFromString(const tstring& colorText, COLORREF& color)
{
	if (colorText.empty()) return false;
	if (colorText[0] == _T('#'))
	{
		if (colorText.length() == 7)
		{
			uint32_t v = 0;
			for (tstring::size_type i = 1; i < colorText.length(); ++i)
			{
				int x = fromHexChar(colorText[i]);
				if (x < 0) return false;
				v = v << 4 | x;
			}
			color = (v >> 16) | ((v << 16) & 0xFF0000) | (v & 0x00FF00);
			return true;
		}
		if (colorText.length() == 4)
		{
			int r = fromHexChar(colorText[1]);
			int g = fromHexChar(colorText[2]);
			int b = fromHexChar(colorText[3]);
			if (r < 0 || g < 0 || b < 0) return false;
			color = r | r << 4 | g << 8 | g << 12 | b << 16 | b << 20;
			return true;
		}
		return false;
	}
	tstring colorTextLower;
	Text::toLower(colorText, colorTextLower);
	// Add constant colors http://www.computerhope.com/htmcolor.htm
	for (size_t i = 0; i < _countof(g_htmlColors); i++)
	{
		if (colorTextLower == g_htmlColors[i].tag)
		{
			color = g_htmlColors[i].color;
			return true;
		}
	}
	return false;
}

bool WinUtil::setExplorerTheme(HWND hWnd)
{
	if (!IsAppThemed()) return false;
	SetWindowTheme(hWnd, L"explorer", nullptr);
	return true;
}

unsigned WinUtil::getListViewExStyle(bool checkboxes)
{
	unsigned style = LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP;
	if (checkboxes) style |= LVS_EX_CHECKBOXES;
	if (BOOLSETTING(SHOW_GRIDLINES)) style |= LVS_EX_GRIDLINES;
	return style;
}

unsigned WinUtil::getTreeViewStyle()
{
	return TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP | TVS_HASBUTTONS | TVS_LINESATROOT;
}

#ifdef IRAINMAN_ENABLE_WHOIS
bool WinUtil::processWhoisMenu(WORD wID, const tstring& ip)
{
	if (!ip.empty())
	{
		tstring link;
		switch (wID)
		{
			case IDC_WHOIS_IP:
				link = _T("http://www.ripe.net/perl/whois?form_type=simple&full_query_string=&searchtext=") + ip;
				break;
			case IDC_WHOIS_IP2:
				link = _T("http://bgp.he.net/ip/") + ip+ _T("#_whois");
				break;
		}
		if (!link.empty())
		{
			WinUtil::openLink(link);
			return true;
		}
	}
	return false;
}

void WinUtil::appendWhoisMenu(OMenu& menu, const tstring& ip, bool useSubMenu)
{
	CMenu subMenu;
	if (useSubMenu)
	{
		subMenu.CreateMenu();
		menu.AppendMenu(MF_STRING, subMenu, CTSTRING(WHOIS_LOOKUP));
	}

	tstring text = TSTRING(WHO_IS) + _T(" Ripe.net  ") + ip;
	if (useSubMenu)
		subMenu.AppendMenu(MF_STRING, IDC_WHOIS_IP, text.c_str());
	else
		menu.AppendMenu(MF_STRING, IDC_WHOIS_IP, text.c_str());

	text = TSTRING(WHO_IS) + _T(" Bgp.He  ") + ip;
	if (useSubMenu)
		subMenu.AppendMenu(MF_STRING, IDC_WHOIS_IP2, text.c_str());
	else
		menu.AppendMenu(MF_STRING, IDC_WHOIS_IP2, text.c_str());
	subMenu.Detach();
}
#endif

void WinUtil::appendPrioItems(OMenu& menu, int idFirst)
{
	static const ResourceManager::Strings names[] =
	{
		ResourceManager::PAUSED,
		ResourceManager::LOWEST,
		ResourceManager::LOWER,
		ResourceManager::LOW,
		ResourceManager::NORMAL,
		ResourceManager::HIGH,
		ResourceManager::HIGHER,
		ResourceManager::HIGHEST
	};
	static_assert(_countof(names) == QueueItem::LAST, "priority list mismatch");
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID;
	mii.fType = MFT_RADIOCHECK;
	int index = menu.GetMenuItemCount();
	for (int i = 0; i < _countof(names); i++)
	{
		mii.wID = idFirst + i;
		mii.dwTypeData = const_cast<TCHAR*>(CTSTRING_I(names[i]));
		menu.InsertMenuItem(index + i, TRUE, &mii);
	}
}

void Preview::startMediaPreview(WORD wID, const QueueItemPtr& qi)
{
	const auto fileName = !qi->getTempTarget().empty() ? qi->getTempTarget() : Util::getFileName(qi->getTarget());
	runPreviewCommand(wID, fileName);
}

void Preview::startMediaPreview(WORD wID, const TTHValue& tth)
{
	string path;
	if (ShareManager::getInstance()->getFilePath(tth, path))
		startMediaPreview(wID, path);
}

void Preview::startMediaPreview(WORD wID, const string& target)
{
	if (!target.empty())
		runPreviewCommand(wID, target);
}

void Preview::clearPreviewMenu()
{
	g_previewMenu.ClearMenu();
	dcdrun(_debugIsClean = true; _debugIsActivated = false; g_previewAppsSize = 0;)
}

void PreviewBaseHandler::activatePreviewItems(OMenu& menu)
{
	dcassert(!_debugIsActivated);
	dcdrun(_debugIsActivated = true;)
			
	int count = menu.GetMenuItemCount();
	MENUITEMINFO mii = { sizeof(mii) };
	// Passing HMENU to EnableMenuItem doesn't work with owner-draw OMenus for some reason
	mii.fMask = MIIM_SUBMENU;
	for (int i = 0; i < count; ++i)
	if (menu.GetMenuItemInfo(i, TRUE, &mii) && mii.hSubMenu == (HMENU) g_previewMenu)
	{
		menu.EnableMenuItem(i, MF_BYPOSITION | (g_previewMenu.GetMenuItemCount() > 0 ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
		break;
	}
}

void InternetSearchBaseHandler::appendInternetSearchItems(OMenu& menu)
{
	CMenu subMenu;
	subMenu.CreateMenu();
	subMenu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_GOOGLE, CTSTRING(SEARCH_WITH_GOOGLE));
	subMenu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_YANDEX, CTSTRING(SEARCH_WITH_YANDEX));
	menu.AppendMenu(MF_STRING, subMenu, CTSTRING(SEARCH_FILE_ON_INTERNET));
	subMenu.Detach();
}

void InternetSearchBaseHandler::searchFileOnInternet(const WORD wID, const tstring& file)
{
	tstring url;
	switch (wID)
	{
		case IDC_SEARCH_FILE_IN_GOOGLE:
			url += _T("https://www.google.com/search?hl=") + Text::toT(Util::getLang()) + _T("&q=");
			break;
		case IDC_SEARCH_FILE_IN_YANDEX:
			url += _T("https://yandex.ru/yandsearch?text=");
			break;
		default:
			return;
	}
	url += file;
	WinUtil::openFile(url);
}
