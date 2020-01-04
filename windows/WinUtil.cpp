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

#include "atlwin.h"
#include <shlobj.h>

#define COMPILE_MULTIMON_STUBS 1
#include <MultiMon.h>
#include <powrprof.h>

#include "WinUtil.h"
#include "PrivateFrame.h"
#include "MainFrm.h"

#include "../client/StringTokenizer.h"
#include "../client/ShareManager.h"
#include "../client/UploadManager.h"
#include "../client/HashManager.h"
#include "../client/File.h"
#include "../client/DownloadManager.h"
#include "MagnetDlg.h"
// AirDC++
#include "winamp.h"
#include "WMPlayerRemoteApi.h"
#include "iTunesCOMInterface.h"
#include "QCDCtrlMsgs.h"
// AirDC++
#include <control.h>
#include <strmif.h> // error with missing ddraw.h, get it from MS DirectX SDK
#include "BarShader.h"
#include "HTMLColors.h"
#include "DirectoryListingFrm.h"

//[!]IRainman moved from Network Page
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
//[~]IRainman moved from Network Page

// [+] IRainman opt: use static object.
string UserInfoGuiTraits::g_hubHint;
UserPtr UserInfoBaseHandlerTraitsUser<UserPtr>::g_user = nullptr;
OnlineUserPtr UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::g_user = nullptr;
// [~] IRainman opt.

const TCHAR* g_file_list_type = L"All Lists\0*.xml.bz2;*.dcls;*.dclst;*.torrent\0Torrent files\0*.torrent\0FileLists\0*.xml.bz2\0DCLST metafiles\0*.dcls;*.dclst\0All Files\0*.*\0\0";

HBRUSH Colors::g_bgBrush = nullptr;
COLORREF Colors::g_textColor = 0;
COLORREF Colors::g_bgColor = 0;

HFONT Fonts::g_font = nullptr;
int Fonts::g_fontHeight = 0;
int Fonts::g_fontHeightPixl = 0;
HFONT Fonts::g_boldFont = nullptr;
HFONT Fonts::g_systemFont = nullptr;
HFONT Fonts::g_halfFont = nullptr;

CMenu WinUtil::g_mainMenu;

OMenu WinUtil::g_copyHubMenu; // [+] IRainman fix.
OMenu UserInfoGuiTraits::copyUserMenu; // [+] IRainman fix.
OMenu UserInfoGuiTraits::grantMenu;
OMenu UserInfoGuiTraits::speedMenu; // !SMT!-S
OMenu UserInfoGuiTraits::userSummaryMenu; // !SMT!-UI
OMenu UserInfoGuiTraits::privateMenu; // !SMT!-PSW
// [+] IRainman fix.
OMenu Preview::g_previewMenu;
int Preview::g_previewAppsSize = 0;
dcdrun(bool Preview::_debugIsActivated = false;)
dcdrun(bool Preview::_debugIsClean = true;)
// [~] IRainman fix.
HIconWrapper WinUtil::g_banIconOnline(IDR_BANNED_ONLINE); // !SMT!-UI
HIconWrapper WinUtil::g_banIconOffline(IDR_BANNED_OFF); // !SMT!-UI
HIconWrapper WinUtil::g_hMedicalIcon(IDR_ICON_MEDICAL_BAG);
HIconWrapper WinUtil::g_hFirewallIcon(IDR_ICON_FIREWALL);
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
HIconWrapper WinUtil::g_hClockIcon(IDR_ICON_CLOCK);
#endif

std::unique_ptr<HIconWrapper> WinUtil::g_HubOnIcon;
std::unique_ptr<HIconWrapper> WinUtil::g_HubOffIcon;
std::unique_ptr<HIconWrapper> WinUtil::g_HubDDoSIcon;
HIconWrapper WinUtil::g_hThermometerIcon(IDR_ICON_THERMOMETR_BAG);

//static WinUtil::ShareMap WinUtil::UsersShare; // !SMT!-UI
TStringList LastDir::g_dirs;
HWND WinUtil::g_mainWnd = nullptr;
HWND WinUtil::g_mdiClient = nullptr;
FlatTabCtrl* WinUtil::g_tabCtrl = nullptr;
HHOOK WinUtil::g_hook = nullptr;
//[-]PPA tstring WinUtil::tth;
//StringPairList WinUtil::initialDirs; [-] IRainman merge.
//tstring WinUtil::exceptioninfo;
bool WinUtil::urlDcADCRegistered = false;
bool WinUtil::urlMagnetRegistered = false;
bool WinUtil::DclstRegistered = false;
bool WinUtil::g_isAppActive = false;
//DWORD WinUtil::comCtlVersion = 0; [-] IRainman: please use CompatibilityManager::getComCtlVersion()
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

void Colors::getUserColor(bool p_is_op, const UserPtr& user, COLORREF &fg, COLORREF &bg, unsigned short& p_flag_mask, const OnlineUserPtr& onlineUser)
{
	bool l_is_favorites = false;
	bg = g_bgColor;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	if (SETTING(ENABLE_AUTO_BAN))
	{
		if ((p_flag_mask & IS_AUTOBAN) == IS_AUTOBAN)
		{
			// BUG? l_is_favorites is false here
			if (onlineUser && user->hasAutoBan(&onlineUser->getClient(), l_is_favorites) != User::BAN_NONE)
				p_flag_mask = (p_flag_mask & ~IS_AUTOBAN) | IS_AUTOBAN_ON;
			else
				p_flag_mask = (p_flag_mask & ~IS_AUTOBAN);
			if (l_is_favorites)
				p_flag_mask = (p_flag_mask & ~IS_FAVORITE) | IS_FAVORITE_ON;
			else
				p_flag_mask = (p_flag_mask & ~IS_FAVORITE);
		}
		if (p_flag_mask & IS_AUTOBAN)
		{
			bg = SETTING(BAN_COLOR);
		}
	}
#endif // IRAINMAN_ENABLE_AUTO_BAN
#ifdef FLYLINKDC_USE_DETECT_CHEATING
	if (p_is_op && onlineUser) // Возможно фикс https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=38000
	{
	
		const auto fc = onlineUser->getIdentity().getFakeCard();
		if (fc & Identity::BAD_CLIENT)
		{
			fg = SETTING(BAD_CLIENT_COLOR);
			return;
		}
		else if (fc & Identity::BAD_LIST)
		{
			fg = SETTING(BAD_FILELIST_COLOR);
			return;
		}
		else if (fc & Identity::CHECKED && BOOLSETTING(SHOW_SHARE_CHECKED_USERS))
		{
			fg = SETTING(FULL_CHECKED_COLOR);
			return;
		}
	}
#endif // FLYLINKDC_USE_DETECT_CHEATING
#ifdef _DEBUG
	//LogManager::message("Colors::getUserColor, user = " + user->getLastNick() + " color = " + Util::toString(fg));
#endif
	dcassert(user);
	// [!] IRainman fix todo: https://crash-server.com/SearchResult.aspx?ClientID=guest&Stack=Colors::getUserColor , https://crash-server.com/SearchResult.aspx?ClientID=guest&Stack=WinUtil::getUserColor
	if ((p_flag_mask & IS_IGNORED_USER) == IS_IGNORED_USER)
	{
		if (UserManager::getInstance()->isInIgnoreList(onlineUser ? onlineUser->getIdentity().getNick() : user->getLastNick()))
			p_flag_mask = (p_flag_mask & ~IS_IGNORED_USER) | IS_IGNORED_USER_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_IGNORED_USER);
	}
	if ((p_flag_mask & IS_RESERVED_SLOT) == IS_RESERVED_SLOT)
	{
		if (UploadManager::getReservedSlotTime(user))
			p_flag_mask = (p_flag_mask & ~IS_RESERVED_SLOT) | IS_RESERVED_SLOT_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_RESERVED_SLOT);
	}
	if ((p_flag_mask & IS_FAVORITE) == IS_FAVORITE)
	{
		bool l_is_ban = false;
		l_is_favorites = FavoriteManager::isFavoriteUser(user, l_is_ban);
		if (l_is_favorites)
			p_flag_mask = (p_flag_mask & ~IS_FAVORITE) | IS_FAVORITE_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_FAVORITE);
		if (l_is_ban)
			p_flag_mask = (p_flag_mask & ~IS_BAN) | IS_BAN_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_BAN);
	}
	if (p_flag_mask & IS_FAVORITE)
	{
		if (p_flag_mask & IS_BAN)
			fg = SETTING(TEXT_ENEMY_FORE_COLOR);
		else
			fg = SETTING(FAVORITE_COLOR);
	}
	else if (onlineUser && onlineUser->getIdentity().isOp())
	{
		fg = SETTING(OP_COLOR);
	}
	else if (p_flag_mask & IS_RESERVED_SLOT)
	{
		fg = SETTING(RESERVED_SLOT_COLOR);
	}
	else if (p_flag_mask & IS_IGNORED_USER)
	{
		fg = SETTING(IGNORED_COLOR);
	}
	else if (user->isSet(User::FIREBALL))
	{
		fg = SETTING(FIREBALL_COLOR);
	}
	else if (user->isSet(User::SERVER))
	{
		fg = SETTING(SERVER_COLOR);
	}
	else if (onlineUser && !onlineUser->getIdentity().isTcpActive()) // [!] IRainman opt.
	{
		fg = SETTING(PASSIVE_COLOR);
	}
	else
	{
		fg = SETTING(NORMAL_COLOR);
	}
}

void WinUtil::initThemeIcons()
{
	g_HubOnIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB));
	g_HubOffIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB_OFF));
	
	g_HubDDoSIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_ICON_MEDICAL_BAG));
}

// !SMT!-UI
dcdrun(bool WinUtil::g_staticMenuUnlinked = true;)
void WinUtil::unlinkStaticMenus(CMenu &menu)
{
	dcdrun(g_staticMenuUnlinked = true;)
	MENUITEMINFO mif = { sizeof MENUITEMINFO };
	mif.fMask = MIIM_SUBMENU;
	for (int i = menu.GetMenuItemCount(); i; i--)
	{
		menu.GetMenuItemInfo(i - 1, true, &mif);
		if (UserInfoGuiTraits::isUserInfoMenus(mif.hSubMenu) ||
		        mif.hSubMenu == g_copyHubMenu.m_hMenu || // [+] IRainman fix.
		        Preview::isPreviewMenu(mif.hSubMenu) // [+] IRainman fix.
		   )
		{
			menu.RemoveMenu(i - 1, MF_BYPOSITION);
		}
	}
}

int WinUtil::GetMenuItemPosition(const CMenu &p_menu, UINT_PTR p_IDItem)
{
	for (int i = 0; i < p_menu.GetMenuItemCount(); ++i)
	{
		if (p_menu.GetMenuItemID(i) == p_IDItem)
			return i;
	}
	return -1;
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
	file.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST, CTSTRING(MENU_REFRESH_FILE_LIST));// [~] changed position Sergey Shushkanov
	file.AppendMenu(MF_STRING, IDC_MATCH_ALL, CTSTRING(MENU_OPEN_MATCH_ALL));
//	file.AppendMenu(MF_STRING, IDC_FLYLINK_DISCOVER, _T("Flylink Discover…"));
	file.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST_PURGE, CTSTRING(MENU_REFRESH_FILE_LIST_PURGE)); // https://www.box.net/shared/cw9agvj2n3fbypdcls46
	file.AppendMenu(MF_STRING, IDC_CONVERT_TTH_HISTORY, CTSTRING(MENU_CONVERT_TTH_HISTORY_INTO_LEVELDB));
	file.AppendMenu(MF_STRING, IDC_OPEN_DOWNLOADS, CTSTRING(MENU_OPEN_DOWNLOADS_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, IDC_OPEN_LOGS, CTSTRING(MENU_OPEN_LOGS_DIR));
	file.AppendMenu(MF_STRING, IDC_OPEN_CONFIGS, CTSTRING(MENU_OPEN_CONFIG_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_GET_TTH, CTSTRING(MENU_TTH));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_FILE_RECONNECT, CTSTRING(MENU_RECONNECT));
	file.AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED)); // [~] InfinitySky. Moved from "window."
	file.AppendMenu(MF_STRING, IDC_FOLLOW, CTSTRING(MENU_FOLLOW_REDIRECT));
	file.AppendMenu(MF_STRING, ID_FILE_QUICK_CONNECT, CTSTRING(MENU_QUICK_CONNECT));
	file.AppendMenu(MF_SEPARATOR);
#ifdef SSA_WIZARD_FEATURE
	file.AppendMenu(MF_STRING, ID_FILE_SETTINGS_WIZARD, CTSTRING(MENU_SETTINGS_WIZARD));
#endif
	file.AppendMenu(MF_STRING, ID_FILE_SETTINGS, CTSTRING(MENU_SETTINGS));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_APP_EXIT, CTSTRING(MENU_EXIT));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)file, CTSTRING(MENU_FILE));
	
	CMenuHandle view;
	view.CreatePopupMenu();
	view.AppendMenu(MF_STRING, ID_FILE_CONNECT, CTSTRING(MENU_PUBLIC_HUBS));
	view.AppendMenu(MF_STRING, IDC_RECENTS, CTSTRING(MENU_FILE_RECENT_HUBS));
	//view.AppendMenu(MF_SEPARATOR); [-] Sergey Shushkanov
	view.AppendMenu(MF_STRING, IDC_FAVORITES, CTSTRING(MENU_FAVORITE_HUBS));
	view.AppendMenu(MF_SEPARATOR); // [+] Sergey Shushkanov
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
	//view.AppendMenu(MF_SEPARATOR); [-] Sergey Shushkanov
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
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)help, CTSTRING(MENU_HLP)); // [~] Drakon
	
#ifdef FLYLINKDC_USE_EXT_JSON
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
	CMenuHandle l_menu_flylinkdc_location;
	l_menu_flylinkdc_location.CreatePopupMenu();
	l_menu_flylinkdc_location.AppendMenu(MF_STRING, IDC_FLYLINKDC_LOCATION, CTSTRING(MENU_CHANGE_FLYLINKDC_LOCATION)); //  _T("Change FlylinkDC++ location!")
	const string l_text_flylinkdc_location = "|||| " + SETTING(LOCATION_COUNTRY) +
	                                         " - " + SETTING(LOCATION_CITY) + " - " + SETTING(LOCATION_ISP) + " ||||";
	g_mainMenu.AppendMenu(MF_STRING, l_menu_flylinkdc_location, Text::toT(l_text_flylinkdc_location).c_str());
#endif
	
#endif // FLYLINKDC_USE_EXT_JSON
	g_fileImage.init();
	
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
	g_videoImage.init();
#endif
	
	g_flagImage.init();
	
	g_userImage.init();
	g_userStateImage.init();
	g_trackerImage.init();
	g_genderImage.init();
	
	Colors::init();
	
	Fonts::init();
	
	if (BOOLSETTING(REGISTER_URL_HANDLER))
	{
		registerDchubHandler();
		registerNMDCSHandler();
		registerADChubHandler();
		registerADCShubHandler();
		urlDcADCRegistered = true;
	}
	
	if (BOOLSETTING(REGISTER_MAGNET_HANDLER))
	{
		registerMagnetHandler();
		urlMagnetRegistered = true;
	}

	if (BOOLSETTING(REGISTER_DCLST_HANDLER))
	{
		registerDclstHandler();
		DclstRegistered = true;
	}
	
	/* [-] IRainman move to CompatibilityManager
	DWORD dwMajor = 0, dwMinor = 0;
	if (SUCCEEDED(ATL::AtlGetCommCtrlVersion(&dwMajor, &dwMinor)))
	{
	    comCtlVersion = MAKELONG(dwMinor, dwMajor);
	}
	*/
	
	g_hook = SetWindowsHookEx(WH_KEYBOARD, &KeyboardProc, NULL, GetCurrentThreadId());
	
	g_copyHubMenu.CreatePopupMenu();
	g_copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBNAME, CTSTRING(HUB_NAME));
	g_copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBADDRESS, CTSTRING(HUB_ADDRESS));
	g_copyHubMenu.InsertSeparatorFirst(TSTRING(COPY));
	
	UserInfoGuiTraits::init();
}

void Fonts::init()
{
	LOGFONT lf[2] = {0};
	::GetObject((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(lf[0]), &lf[0]);
	// SettingsManager::setDefault(SettingsManager::TEXT_FONT, Text::fromT(encodeFont(lf))); // !SMT!-F
	
	lf[0].lfWeight = FW_BOLD;
	g_boldFont = ::CreateFontIndirect(&lf[0]);

	lf[1] = lf[0];
	lf[1].lfHeight += 3;
	lf[1].lfWeight = FW_NORMAL;
	g_halfFont = ::CreateFontIndirect(&lf[1]);
	
	decodeFont(Text::toT(SETTING(TEXT_FONT)), lf[0]);
	//::GetObject((HFONT)GetStockObject(ANSI_FIXED_FONT), sizeof(lf[1]), &lf[1]);
	
	//lf[1].lfHeight = lf[0].lfHeight;
	//lf[1].lfWeight = lf[0].lfWeight;
	//lf[1].lfItalic = lf[0].lfItalic;
	
	g_font = ::CreateFontIndirect(&lf[0]);
	g_fontHeight = WinUtil::getTextHeight(WinUtil::g_mainWnd, g_font);
	g_systemFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	{
		HDC hDC = ::GetDC(NULL);
		const int l_pix = ::GetDeviceCaps(hDC, LOGPIXELSY);
		g_fontHeightPixl = -::MulDiv(lf[0].lfHeight, l_pix, 72);
		int l_res = ReleaseDC(NULL, hDC);
		dcassert(l_res);
	}
}

void Colors::init()
{
	g_textColor = SETTING(TEXT_COLOR);
	g_bgColor = SETTING(BACKGROUND_COLOR);
	
	g_bgBrush = CreateSolidBrush(Colors::g_bgColor); // Leak
	
	CHARFORMAT2 cf;
	memzero(&cf, sizeof(CHARFORMAT2));
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
		g_ChatTextMyOwn.dwEffects       |= CFE_BOLD;
	if (SETTING(TEXT_MYOWN_ITALIC))
		g_ChatTextMyOwn.dwEffects       |= CFE_ITALIC;
		
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
	g_ISPImage.uninit(); // TODO - позже
	g_TransferTreeImage.uninit();
	g_flagImage.uninit();
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
	g_videoImage.uninit();
#endif
	Fonts::uninit();
	Colors::uninit();
	
	g_mainMenu.DestroyMenu();
	g_copyHubMenu.DestroyMenu();// [+] IRainman fix.
	
	// !SMT!-UI
	UserInfoGuiTraits::uninit();
}

void Fonts::decodeFont(const tstring& setting, LOGFONT &dest)
{
	const StringTokenizer<tstring, TStringList> st(setting, _T(','));
	const auto& sl = st.getTokens();
	
	::GetObject((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(dest), &dest);
	tstring face;
	if (sl.size() == 4)
	{
		face = sl[0];
		dest.lfHeight = Util::toInt(sl[1]);
		dest.lfWeight = Util::toInt(sl[2]);
		dest.lfItalic = (BYTE)Util::toInt(sl[3]);
	}
	
	if (!face.empty())
	{
		memzero(dest.lfFaceName, sizeof(dest.lfFaceName));
		// [!] PVS V512 A call of the 'memset' function will lead to underflow of the buffer 'dest.lfFaceName'. flylinkdc   winutil.cpp 845 False
		_tcscpy_s(dest.lfFaceName, face.c_str());
	}
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
	AutoArray <TCHAR> buf(FULL_MAX_PATH);
	BROWSEINFO bi = {0};
	bi.hwndOwner = owner;
	bi.pszDisplayName = buf;
	bi.lpszTitle = CTSTRING(CHOOSE_FOLDER);
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
	bi.lParam = (LPARAM)target.c_str();
	bi.lpfn = &browseCallbackProc;
	if (LPITEMIDLIST pidl = SHBrowseForFolder(&bi))
	{
		SHGetPathFromIDList(pidl, buf);
		target = buf;
		
		Util::appendPathSeparator(target);
		WinUtil::safe_sh_free(pidl);
		return true;
	}
	return false;
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
	res += L',';
	res += Util::toStringW(font.lfHeight);
	res += L',';
	res += Util::toStringW(font.lfWeight);
	res += L',';
	res += Util::toStringW(font.lfItalic);
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
//[+] FlylinkDC++ Team
void WinUtil::splitTokensWidth(int* p_array, const string& p_tokens, int p_maxItems) noexcept
{
	splitTokens(p_array, p_tokens, p_maxItems);
	for (int k = 0; k < p_maxItems; ++k)
		if (p_array[k] <= 0 || p_array[k] > 2000)
			p_array[k] = 10;
}
//[~] FlylinkDC++ Team
void WinUtil::splitTokens(int* p_array, const string& p_tokens, int p_maxItems) noexcept
{
	dcassert(p_maxItems > 0); //[+] FlylinkDC++ Team
	const StringTokenizer<string> t(p_tokens, ',');
	const StringList& l = t.getTokens();
	int k = 0;
	for (auto i = l.cbegin(); i != l.cend() && k < p_maxItems; ++i, ++k)
	{
		p_array[k] = Util::toInt(*i);
	}
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
	SearchFrame::openWindow(Text::toT(file), 0, Search::SIZE_DONTCARE, FILE_TYPE_ANY);
}

void WinUtil::searchHash(const TTHValue& hash)
{
	SearchFrame::openWindow(Text::toT(hash.toBase32()), 0, Search::SIZE_DONTCARE, FILE_TYPE_TTH);
}

void WinUtil::registerDchubHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	Buf[0] = 0;
	
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCHUB));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}
static void internalDeleteRegistryKey(const tstring& p_key)
{
	tstring l_key = _T("SOFTWARE\\Classes\\") + p_key;
	if (SHDeleteKey(HKEY_CURRENT_USER, l_key.c_str()) != ERROR_SUCCESS)
	{
		LogManager::message("Erorr Delete key " + Text::fromT(l_key) + " " + Util::translateError());
	}
	
}
void WinUtil::unRegisterDchubHandler()
{
	internalDeleteRegistryKey(_T("dchub"));
}

void WinUtil::registerNMDCSHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	Buf[0] = 0;
	
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCHUB));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}

void WinUtil::unRegisterNMDCSHandler()
{
	internalDeleteRegistryKey(_T("nmdcs"));
}

void WinUtil::registerADChubHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_ADC));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}

void WinUtil::unRegisterADChubHandler()
{
	internalDeleteRegistryKey(_T("adc"));
}

void WinUtil::registerADCShubHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_ADC));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}

void WinUtil::unRegisterADCShubHandler()
{
	internalDeleteRegistryKey(_T("adcs"));
}

void WinUtil::registerMagnetHandler()
{
	HKEY hk = nullptr;
	tstring openCmd;
	tstring appName = Util::getModuleFileName();
	const auto l_comman_key_path = _T("SOFTWARE\\Classes\\magnet\\shell\\open\\command");
	{
		LocalArray<TCHAR, 1024> l_buf;
		l_buf[0] = 0;
		
		// what command is set up to handle magnets right now?
		if (::RegOpenKeyEx(HKEY_CURRENT_USER, l_comman_key_path, 0, KEY_READ, &hk) == ERROR_SUCCESS)
		{
			DWORD l_bufLen = l_buf.size();
			::RegQueryValueEx(hk, NULL, NULL, NULL, (LPBYTE)l_buf.data(), &l_bufLen);
			::RegCloseKey(hk);
		}
		openCmd = l_buf.data();
	}
	
	// (re)register the handler if FlylinkDC.exe isn't the default, or if DC++ is handling it
	if (BOOLSETTING(REGISTER_MAGNET_HANDLER))
	{
		const tstring l_qAppName = _T('\"') + appName + _T("\"");
		if (openCmd.empty() || strnicmp(openCmd, l_qAppName, l_qAppName.size()) != 0)
		{
			internalDeleteRegistryKey(_T("magnet"));
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\magnet"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_MAGNET));
				return;
			}
			const tstring l_MagnetShellDesc = CTSTRING(MAGNET_SHELL_DESC);
			if (!l_MagnetShellDesc.empty())
			{
				::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)l_MagnetShellDesc.c_str(), sizeof(TCHAR) * l_MagnetShellDesc.length() + 1);
			}
			::RegSetValueEx(hk, _T("URL Protocol"), NULL, REG_SZ, NULL, NULL);
			::RegCloseKey(hk);
			::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\magnet\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
			appName = l_qAppName + _T(" /magnet \"%1\"");
			::RegCreateKeyEx(HKEY_CURRENT_USER, l_comman_key_path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
		}
	}
}

void WinUtil::unRegisterMagnetHandler()
{
	internalDeleteRegistryKey(_T("magnet"));
}

void WinUtil::registerDclstHandler()
{
	// [!] SSA - тут нужно добавить ссылку и открытие dclst файлов с диска
	
	// [HKEY_CURRENT_USER\Software\Classes\.dcls]
	// @="DCLST metafile"
	// [HKEY_CURRENT_USER\Software\Classes\.dclst]
	// @="DCLST metafile"
	
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile]
	// @="DCLST metafile download shortcut"
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\DefaultIcon]
	// @="\"C:\\Program Files\\FlylinkDC++\\FlylinkDC.exe\""
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\Shell]
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\Shell\Open]
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\Shell\Open\Command]
	// @="\"C:\\Program Files\\FlylinkDC++\\FlylinkDC\" \"%1\""
	
	
	const tstring l_comman_key_path = _T("SOFTWARE\\Classes\\DCLST metafile\\shell\\open\\command");
	
	HKEY hk = nullptr;
	tstring openCmd;
	tstring appName = Util::getModuleFileName();
	
	{
		LocalArray<TCHAR, 1024> l_buf;
		l_buf[0] = 0;
		
		// what command is set up to handle magnets right now?
		if (::RegOpenKeyEx(HKEY_CURRENT_USER, l_comman_key_path.c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS)
		{
			DWORD l_bufLen = l_buf.size();
			::RegQueryValueEx(hk, NULL, NULL, NULL, (LPBYTE)l_buf.data(), &l_bufLen);
			::RegCloseKey(hk);
		}
		openCmd = l_buf.data();
	}
	
	// (re)register the handler if FlylinkDC.exe isn't the default, or if DC++ is handling it
	if (BOOLSETTING(REGISTER_DCLST_HANDLER))
	{
		const tstring l_qAppName = _T('\"') + appName + _T("\"");
		if (openCmd.empty() || strnicmp(openCmd, l_qAppName, l_qAppName.size()) != 0)
		{
			// Add Class Ext
			static const tstring dclstMetafile = _T("DCLST metafile");
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\.dcls"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
				return;
			}
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)dclstMetafile.c_str(), sizeof(TCHAR) * (dclstMetafile.length() + 1));
			::RegCloseKey(hk);
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\.dclst"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
				return;
			}
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)dclstMetafile.c_str(), sizeof(TCHAR) * (dclstMetafile.length() + 1));
			::RegCloseKey(hk);
			
			
			internalDeleteRegistryKey(_T("DCLST metafile"));
			
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\DCLST metafile"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
				return;
			}
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)CTSTRING(DCLST_SHELL_DESC), sizeof(TCHAR) * TSTRING(MAGNET_SHELL_DESC).length() + 1);
			::RegCloseKey(hk);
			::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\DCLST metafile\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
			appName = l_qAppName + _T(" /open \"%1\"");
			::RegCreateKeyEx(HKEY_CURRENT_USER, l_comman_key_path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
		}
	}
}

void WinUtil::unRegisterDclstHandler()// [+] IRainman dclst support
{
	internalDeleteRegistryKey(_T("DCLST metafile"));
}

void WinUtil::openFile(const tstring& file)
{
	openFile(file.c_str());
}

void WinUtil::openFile(const TCHAR* file)
{
	::ShellExecute(NULL, _T("open"), file, NULL, NULL, SW_SHOWNORMAL);
}

bool WinUtil::openLink(const tstring& uri) // [!] IRainman opt: return status.
{
	// [!] IRainman opt.
	if (parseMagnetUri(uri) || parseDchubUrl(uri))
	{
		return true;
	}
	else
	{
		static const Tags g_ExtLinks[] =
		{
			EXT_URL_LIST(),
		};
		for (size_t i = 0; i < _countof(g_ExtLinks); ++i)
		{
			if (strnicmp(uri, g_ExtLinks[i].tag, g_ExtLinks[i].tag.size()))
			{
				translateLinkToextProgramm(uri);
				return true;
			}
		}
	}
	return false;
	// [~] IRainman opt.
}

void WinUtil::translateLinkToextProgramm(const tstring& url, const tstring& p_Extension /*= Util::emptyStringT*/, const tstring& p_openCmd /* = Util::emptyStringT*/)//[+]FlylinkDC
{
	// [!] IRainman
	tstring x;
	if (p_openCmd.empty())
	{
		if (p_Extension.empty())
		{
			tstring::size_type i = url.find(_T("://"));
			if (i != string::npos)
			{
				x = url.substr(0, i);
			}
			else
			{
				x = _T("http");
			}
		}
		else
		{
			//[+] IRainman
			x = p_Extension;
		}
		x += _T("\\shell\\open\\command");
	}
#ifdef FLYLINKDC_USE_TORRENT
	if (url.find(_T("magnet:?xt=urn:btih")) != tstring::npos)
	{
		DownloadManager::getInstance()->add_torrent_file(_T(""), url);
	}
#endif
	
	::ShellExecute(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool WinUtil::parseDchubUrl(const tstring& aUrl)
{
	if (Util::isDcppHub(aUrl) || Util::isNmdcHub(aUrl))
	{
		uint16_t port;
		string proto, host, file, query, fragment;
		const string formattedUrl = Util::formatDchubUrl(Text::fromT(aUrl));
		Util::decodeUrl(formattedUrl, proto, host, port, file, query, fragment);
		const string hostPort = host + ":" + Util::toString(port);
		if (!host.empty())
		{
			RecentHubEntry r;
			r.setServer(formattedUrl);
			FavoriteManager::getInstance()->addRecent(r);
			HubFrame::openHubWindow(formattedUrl);
		}
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
	return false;
}

bool WinUtil::parseMagnetUri(const tstring& aUrl, DefinedMagnetAction Action /* = MA_DEFAULT */)
{
	// official types that are of interest to us
	//  xt = exact topic
	//  xs = exact substitute
	//  as = acceptable substitute
	//  dn = display name
	//  xl = exact length
	//  kt = text for search
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
			string fname, fhash, type, param;
			
			const StringTokenizer<string> mag(url.substr(8), '&');
			typedef boost::unordered_map<string, string> MagMap;
			MagMap hashes;
			
			int64_t fsize = 0;
			int64_t dirsize = 0;
			for (auto idx = mag.getTokens().cbegin(); idx != mag.getTokens().cend(); ++idx)
			{
				// break into pairs
				string::size_type pos = idx->find('=');
				if (pos != string::npos)
				{
					type = Text::toLower(Util::encodeURI(idx->substr(0, pos), true));
					param = Util::encodeURI(idx->substr(pos + 1), true);
				}
				else
				{
					type = Util::encodeURI(*idx, true);
					param.clear();
				}
				// extract what is of value
				if (param.length() == 85 && strnicmp(param.c_str(), "urn:bitprint:", 13) == 0)
				{
					hashes[type] = param.substr(46);
				}
				else if (param.length() == 54 && strnicmp(param.c_str(), "urn:tree:tiger:", 15) == 0)
				{
					hashes[type] = param.substr(15);
				}
				else if (param.length() == 53 && strnicmp(param.c_str(), "urn:tigertree:", 14) == 0)   // used by nextpeer :(
				{
					hashes[type] = param.substr(14);
				}
				else if (param.length() == 55 && strnicmp(param.c_str(), "urn:tree:tiger/:", 16) == 0)
				{
					hashes[type] = param.substr(16);
				}
				else if (param.length() == 59 && strnicmp(param.c_str(), "urn:tree:tiger/1024:", 20) == 0)
				{
					hashes[type] = param.substr(20);
				}
				// Short URN versions
				else if (param.length() == 81 && strnicmp(param.c_str(), "bitprint:", 9) == 0)
				{
					hashes[type] = param.substr(42);
				}
				else if (param.length() == 50 && strnicmp(param.c_str(), "tree:tiger:", 11) == 0)
				{
					hashes[type] = param.substr(11);
				}
				else if (param.length() == 49 && strnicmp(param.c_str(), "tigertree:", 10) == 0)   // used by nextpeer :(
				{
					hashes[type] = param.substr(10);
				}
				else if (param.length() == 51 && strnicmp(param.c_str(), "tree:tiger/:", 12) == 0)
				{
					hashes[type] = param.substr(12);
				}
				else if (param.length() == 55 && strnicmp(param.c_str(), "tree:tiger/1024:", 16) == 0)
				{
					hashes[type] = param.substr(16);
				}
				// File name and size
				else if (type.length() == 2 && strnicmp(type.c_str(), "dn", 2) == 0)
				{
					fname = param;
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "xl", 2) == 0)
				{
					fsize = Util::toInt64(param);
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "dl", 2) == 0)
				{
					dirsize = Util::toInt64(param);
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "kt", 2) == 0)
				{
					fname = param;
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "xs", 2) == 0)
				{
					WinUtil::parseDchubUrl(Text::toT(param));
				}
			}
			// pick the most authoritative hash out of all of them.
			if (hashes.find("as") != hashes.end())
			{
				fhash = hashes["as"];
			}
			if (hashes.find("xs") != hashes.end())
			{
				fhash = hashes["xs"];
			}
			if (hashes.find("xt") != hashes.end())
			{
				fhash = hashes["xt"];
			}
			const bool l_isDCLST = Util::isDclstFile(fname);
			if (!fhash.empty() && Encoder::isBase32(fhash.c_str()))
			{
				// ok, we have a hash, and maybe a filename.
				
				if (Action == MA_DEFAULT)
				{
					if (fsize <= 0 || fname.empty())
						Action = MA_ASK;
					else
					{
						if (!l_isDCLST)
						{
							if (BOOLSETTING(MAGNET_ASK))
								Action = MA_ASK;
							else
							{
								switch (SETTING(MAGNET_ACTION))
								{
									case SettingsManager::MAGNET_AUTO_DOWNLOAD:
										Action = MA_DOWNLOAD;
										break;
									case SettingsManager::MAGNET_AUTO_SEARCH:
										Action = MA_SEARCH;
										break;
									case SettingsManager::MAGNET_AUTO_DOWNLOAD_AND_OPEN: // [!] FlylinkDC Team TODO: add support auto open file after download to gui.
										Action = MA_OPEN;
										break;
									default:
										Action = MA_ASK;
										break;
								}
							}
						}
						else
						{
							if (BOOLSETTING(DCLST_ASK))
								Action = MA_ASK;
							else
							{
								switch (SETTING(DCLST_ACTION))
								{
									case SettingsManager::MAGNET_AUTO_DOWNLOAD:
										Action = MA_DOWNLOAD;
										break;
									case SettingsManager::MAGNET_AUTO_SEARCH:
										Action = MA_SEARCH;
										break;
									case SettingsManager::MAGNET_AUTO_DOWNLOAD_AND_OPEN:
										Action = MA_OPEN;
										break;
									default:
										Action = MA_ASK;
										break;
								}
							}
						}
					}
				}
				
				switch (Action)
				{
					case MA_DOWNLOAD:
						try
						{
							// [!] SSA - Download Folder
							QueueManager::getInstance()->add(fname, fsize, TTHValue(fhash), HintedUser(),
							                                 l_isDCLST ? QueueItem::FLAG_DCLST_LIST :
							                                 0);
						}
						catch (const Exception& e)
						{
							LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
						}
						break;
						
					case MA_SEARCH:
						SearchFrame::openWindow(Text::toT(fhash), 0, Search::SIZE_DONTCARE, FILE_TYPE_TTH);
						break;
					case MA_OPEN:
					{
						try
						{
							// [!] SSA to do open here
							QueueManager::getInstance()->add(fname, fsize, TTHValue(fhash), HintedUser(), QueueItem::FLAG_CLIENT_VIEW | (l_isDCLST ? QueueItem::FLAG_DCLST_LIST : 0));
						}
						catch (const Exception& e)
						{
							LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
						}
						
					}
					break;
					case MA_ASK:
					{
						MagnetDlg dlg(TTHValue(fhash), Text::toT(Text::toUtf8(fname)), fsize, dirsize, l_isDCLST);
						dlg.DoModal(g_mainWnd);
					}
					break;
				};
			}
			else if (!fname.empty() && fhash.empty())
			{
				SearchFrame::openWindow(Text::toT(fname), fsize, (fsize == 0) ? Search::SIZE_DONTCARE : Search::SIZE_EXACT, FILE_TYPE_ANY);
			}
			else
			{
				MessageBox(g_mainWnd, CTSTRING(MAGNET_DLG_TEXT_BAD), CTSTRING(MAGNET_DLG_TITLE), MB_OK | MB_ICONEXCLAMATION);
			}
		}
		return true;
	}
	return false;
}

void WinUtil::OpenFileList(const tstring& filename, DefinedMagnetAction Action /* = MA_DEFAULT */) // [+] IRainman dclst support // [!] SSA
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

bool WinUtil::parseDBLClick(const tstring& aString, string::size_type start, string::size_type end)
{
	const tstring l_URI = aString.substr(start, end - start); // [+] IRainman opt.
	return openLink(l_URI); // [!] IRainman opt.
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
	const auto file = Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatParams(dir, params, false)));
	if (File::isExist(file))
		WinUtil::openFile(file);
	else
		MessageBox(nullptr, noLogMessage.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONINFORMATION);
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

/*
bool WinUtil::checkIsButtonPressed(int nID)//[+]IRainman
{
    if (nID == -1)
        return false;

    LPTBBUTTONINFO tbi = new TBBUTTONINFO;
//  tbi->fsState = TBSTATE_CHECKED;
    if (MainFrame::getMainFrame()->getToolBar().GetButtonInfo(nID, tbi) == -1)
    {
        delete tbi;
        return false;
    }
    else
    {
        delete tbi;
        return true;
    }
}
*/

tstring WinUtil::getNicks(const CID& cid, const string& hintUrl)
{
	const auto l_nicks = ClientManager::getNicks(cid, hintUrl);
	if (l_nicks.empty())
		return Util::emptyStringT;
	else
		return Text::toT(Util::toString(l_nicks));
}

tstring WinUtil::getNicks(const UserPtr& u, const string& hintUrl)
{
	dcassert(u);
	if (u)
	{
		return getNicks(u->getCID(), hintUrl);
	}
	else
		return Util::emptyStringT;
}

tstring WinUtil::getNicks(const CID& cid, const string& hintUrl, bool priv)
{
	const auto l_nicks = ClientManager::getNicks(cid, hintUrl, priv);
	if (l_nicks.empty())
		return Util::emptyStringT;
	else
		return Text::toT(Util::toString(l_nicks));
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

void WinUtil::fillCharsetList(CComboBox& comboBox, int selected, bool onlyUTF8)
{
	int index, selIndex = -1;
	if (!onlyUTF8)
	{
		tstring str = TSTRING_F(ENCODING_SYSTEM_DEFAULT, Text::getDefaultCharset());
		index = comboBox.AddString(str.c_str());
		if (selected == Text::CHARSET_SYSTEM_DEFAULT) selIndex = index;
		comboBox.SetItemData(index, Text::CHARSET_SYSTEM_DEFAULT);
	}
	index = comboBox.AddString(CTSTRING(ENCODING_UTF8));
	if (selected == Text::CHARSET_UTF8) selIndex = index;
	comboBox.SetItemData(index, Text::CHARSET_UTF8);	
	static const ResourceManager::Strings charsets[] =
	{
		ResourceManager::ENCODING_CP1250,
		ResourceManager::ENCODING_CP1251,
		ResourceManager::ENCODING_CP1252,
		ResourceManager::ENCODING_CP1253,
		ResourceManager::ENCODING_CP1254,
		ResourceManager::ENCODING_CP1255,
		ResourceManager::ENCODING_CP1256,
		ResourceManager::ENCODING_CP1257,
		ResourceManager::ENCODING_CP1258
	};
	if (!onlyUTF8)
		for (int i = 0; i < _countof(charsets); i++)
		{
			int charset = Text::CHARSET_MIN_SUPPORTED + i;
			tstring str = TSTRING_I(charsets[i]);
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

DWORD CALLBACK WinUtil::EditStreamCallback(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb)
{
	userStreamIterator *it = (userStreamIterator*)dwCookie;
	if (it->position >= it->length) return (DWORD) - 1;
	*pcb = cb > (it->length - it->position) ? (it->length - it->position) : cb;
	memcpy(lpBuff, it->data.get() + it->position, *pcb);
	it->position += *pcb;
	return 0;
}

tstring WinUtil::getFilenameFromString(const tstring& filename)
{
	tstring strRet;
	tstring::size_type i = 0;
	for (; i < filename.length(); i++)
	{
		const tstring strtest = filename.substr(i, 1);
		const wchar_t* testLabel = strtest.c_str();
		if (testLabel[0] != _T(' ') && testLabel[0] != _T('\"'))
			break;
	}
	if (i < filename.length())
	{
		strRet = filename.substr(i);
		
		tstring::size_type j2Comma = strRet.find(_T('\"'), 0);
		if (j2Comma != string::npos && j2Comma > 0)
			strRet = strRet.substr(0, j2Comma);
			
		//tstring::size_type jSpace = strRet.find(_T(' '), 0);
		//if (jSpace != tstring::npos && jSpace > 0)
		//  strRet = strRet.substr(0, jSpace - 1);
	}
	
	return strRet;
}

#ifdef SSA_SHELL_INTEGRATION
wstring WinUtil::getShellExtDllPath()
{
	// [!] TODO: To fully integrate on Windows x64 need both libraries.
	static const auto filePath = Text::toT(Util::getExePath()) + _T("FlylinkShellExt")
#if defined(_WIN64)
	                             _T("_x64")
#endif
	                             _T(".dll");
	                             
	return filePath;
}

bool WinUtil::makeShellIntegration(bool isNeedUnregistred)
{
	typedef  HRESULT(WINAPIV Registration)(void);
	
	bool bResult = false;
	HINSTANCE hModule = nullptr;
	try
	{
		const auto filePath = WinUtil::getShellExtDllPath();
		hModule =::LoadLibrary(filePath.c_str());
		if (hModule != nullptr)
		{
			bResult = false;
			Registration* reg = nullptr;
			reg = (Registration*)::GetProcAddress((HMODULE)hModule, isNeedUnregistred ? "DllUnregisterServer" : "DllRegisterServer");
			if (reg != nullptr)
			{
				bResult = SUCCEEDED(reg());
			}
			::FreeLibrary(hModule);
		}
	}
	catch (...)
	{
		if (hModule)
			::FreeLibrary(hModule);
			
		bResult = false;
	}
	
	
	return bResult;
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


/*
-------------------------------------------------------------------
Description:
  Creates the actual 'lnk' file (assumes COM has been initialized).

Parameters:
  pszTargetfile    - File name of the link's target.
  pszTargetargs    - Command line arguments passed to link's target.
  pszLinkfile      - File name of the actual link file being created.
  pszDescription   - Description of the linked item.
  iShowmode        - ShowWindow() constant for the link's target.
  pszCurdir        - Working directory of the active link.
  pszIconfile      - File name of the icon file used for the link.
  iIconindex       - Index of the icon in the icon file.

Returns:
  HRESULT value >= 0 for success, < 0 for failure.
--------------------------------------------------------------------
*/
bool WinUtil::CreateShortCut(const tstring& pszTargetfile, const tstring& pszTargetargs,
                             const tstring& pszLinkfile, const tstring& pszDescription,
                             int iShowmode, const tstring& pszCurdir,
                             const tstring& pszIconfile, int iIconindex)
{
	HRESULT       hRes;                  /* Returned COM result code */
	IShellLink*   pShellLink;            /* IShellLink object pointer */
	IPersistFile* pPersistFile;          /* IPersistFile object pointer */
	
	hRes = E_INVALIDARG;
	if (pszTargetfile.length() > 0 && pszLinkfile.length() > 0
	
	        /*
	                && (pszTargetargs.length() > 0)
	                && (pszDescription.length() > 0)
	                && (iShowmode >= 0)
	                && (pszCurdir.length() > 0)
	                && (pszIconfile.length() > 0 )
	                && (iIconindex >= 0)
	        */
	   )
	{
		hRes = CoCreateInstance(
		           CLSID_ShellLink,     /* pre-defined CLSID of the IShellLink object */
		           NULL,                 /* pointer to parent interface if part of aggregate */
		           CLSCTX_INPROC_SERVER, /* caller and called code are in same process */
		           IID_IShellLink,      /* pre-defined interface of the IShellLink object */
		           (LPVOID*)&pShellLink);         /* Returns a pointer to the IShellLink object */
		if (SUCCEEDED(hRes))
		{
			/* Set the fields in the IShellLink object */
			// [!] PVS V519 The 'hRes' variable is assigned values twice successively. Perhaps this is a mistake. Check lines: 4536, 4537.
			hRes |= pShellLink->SetPath(pszTargetfile.c_str());  // [!] PVS thanks!
			hRes |= pShellLink->SetArguments(pszTargetargs.c_str()); // [!] PVS thanks!
			if (pszDescription.length() > 0)
				hRes |= pShellLink->SetDescription(pszDescription.c_str());// [!] PVS thanks!
			if (iShowmode > 0)
				hRes |= pShellLink->SetShowCmd(iShowmode);// [!] PVS thanks!
			if (pszCurdir.length() > 0)
				hRes |= pShellLink->SetWorkingDirectory(pszCurdir.c_str());// [!] PVS thanks!
			if (pszIconfile.length() > 0 && iIconindex >= 0)
				hRes |= pShellLink->SetIconLocation(pszIconfile.c_str(), iIconindex);// [!] PVS thanks!
				
			/* Use the IPersistFile object to save the shell link */
			hRes |= pShellLink->QueryInterface( /* [!] PVS thanks! */
			            IID_IPersistFile,         /* pre-defined interface of the IPersistFile object */
			            (LPVOID*)&pPersistFile);            /* returns a pointer to the IPersistFile object */
			if (SUCCEEDED(hRes))
			{
				hRes = pPersistFile->Save(pszLinkfile.c_str(), TRUE);
				safe_release(pPersistFile);
			}
			safe_release(pShellLink);
		}
		
	}
	return SUCCEEDED(hRes);// [!] PVS thanks!
}

bool WinUtil::AutoRunShortCut(bool bCreate)
{
	if (bCreate)
	{
		// Create
		if (!IsAutoRunShortCutExists())
		{
			const std::wstring targetF = Util::getModuleFileName();
			std::wstring pszCurdir =  Util::getFilePath(targetF);
			std::wstring pszDescr = getFlylinkDCAppCaptionT();
			pszDescr  += L' ' + T_VERSIONSTRING;
			Util::appendPathSeparator(pszCurdir);
			return CreateShortCut(targetF, L"", GetAutoRunShortCutName(), pszDescr, 0, pszCurdir, targetF, 0);
		}
	}
	else
	{
		// Remove
		if (IsAutoRunShortCutExists())
		{
			return File::deleteFileT(GetAutoRunShortCutName());
		}
	}
	
	return true;
}

bool WinUtil::IsAutoRunShortCutExists()
{
	return File::isExist(GetAutoRunShortCutName());
}

tstring WinUtil::GetAutoRunShortCutName()
{
	// Name: {userstartup}\FlylinkDC++{code:Postfix| }; Filename: {app}\FlylinkDC{code:Postfix|_}.exe; Tasks: startup; WorkingDir: {app}
	// CSIDL_STARTUP
	TCHAR startupPath[MAX_PATH];
	if (!SHGetSpecialFolderPath(NULL, startupPath, CSIDL_STARTUP, TRUE))
		return Util::emptyStringT; // [!] IRainman fix
		
	tstring autoRunShortCut = startupPath;
	Util::appendPathSeparator(autoRunShortCut);
	autoRunShortCut += getFlylinkDCAppCaptionT();
#if defined(_WIN64)
	autoRunShortCut += L"_x64";
#endif
	autoRunShortCut += L".lnk";
	
	return autoRunShortCut;
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

bool Colors::getColorFromString(const tstring& colorText, COLORREF& color)
{

//#define USE_CRUTH_FOR_GET_HTML_COLOR

	// TODO - add background color!!!
	int r = 0;
	int g = 0;
	int b = 0;
	tstring colorTextLower = Text::toLower(colorText);
#ifdef USE_CRUTH_FOR_GET_HTML_COLOR
	boost::trim(colorTextLower); // Crutch.
#endif
	if (colorTextLower.empty())
	{
		return false;
	}
	else if (colorTextLower[0]  == L'#') // #FF0000
	{
#ifdef USE_CRUTH_FOR_GET_HTML_COLOR
		if (colorTextLower.length() > 7)
			colorTextLower = colorTextLower.substr(0, 7); // Crutch.
		else for (int i = colorTextLower.length(); i < 7; i++)
				colorTextLower += _T('0');
#else
		if (colorTextLower.size() != 7)
			return false;
#endif
		// TODO: rewrite without copy of string.
		const tstring s1(colorTextLower, 1, 2);
		const tstring s2(colorTextLower, 3, 2);
		const tstring s3(colorTextLower, 5, 2);
		try
		{
			r = stoi(s1, NULL, 16);
			g = stoi(s2, NULL, 16);
			b = stoi(s3, NULL, 16);
		}
		catch (const std::invalid_argument& /*e*/)
		{
			return false;
		}
		color = RGB(r, g, b);
		return true;
	}
	else
	{
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
}

bool WinUtil::isUseExplorerTheme()
{
	return BOOLSETTING(USE_EXPLORER_THEME);
}

void WinUtil::SetWindowThemeExplorer(HWND p_hWnd)
{
// FIXME: doesn't seem to work
#if 0
	if (isUseExplorerTheme())
	{
		SetWindowTheme(p_hWnd, L"explorer", NULL);
	}
#endif
}

#ifdef IRAINMAN_ENABLE_WHOIS
void WinUtil::CheckOnWhoisIP(WORD wID, const tstring& whoisIP)
{
	if (!whoisIP.empty())
	{
		tstring m_link;
		switch (wID)
		{
			case IDC_WHOIS_IP:
				m_link = _T("http://www.ripe.net/perl/whois?form_type=simple&full_query_string=&searchtext=") + whoisIP;
				break;
			case IDC_WHOIS_IP2:
				m_link = _T("http://bgp.he.net/ip/") + whoisIP + _T("#_whois");
				break;
		}
		if (!m_link.empty())
			WinUtil::openLink(m_link);
	}
}

void WinUtil::AppendMenuOnWhoisIP(CMenu& p_menuname, const tstring& p_IP, bool p_inSubmenu)
{
	// ToDo::  if p_inSubmenu == true : create and append into SubMenu
	p_menuname.AppendMenu(MF_STRING, IDC_WHOIS_IP, (TSTRING(WHO_IS) + _T(" Ripe.net  ") + p_IP).c_str());
	p_menuname.AppendMenu(MF_STRING, IDC_WHOIS_IP2, (TSTRING(WHO_IS) + _T(" Bgp.He  ") + p_IP).c_str());
	p_menuname.AppendMenu(MF_STRING, IDC_WHOIS_IP4_INFO, tstring(_T(" IP v4 Info ") + p_IP).c_str());
	//p_menu.AppendMenu(MF_SEPARATOR);
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
	for (int i = 0; i < _countof(names); i++)
		menu.AppendMenu(MF_STRING, idFirst + i, CTSTRING_I(names[i]));
}

void Preview::startMediaPreview(WORD wID, const QueueItemPtr& qi)
{
	const auto fileName = !qi->getTempTarget().empty() ? qi->getTempTarget() : qi->getTargetFileName();
	runPreviewCommand(wID, fileName);
}

void Preview::startMediaPreview(WORD wID, const TTHValue& tth)
{
	startMediaPreview(wID, ShareManager::toRealPath(tth));
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

UINT Preview::getPreviewMenuIndex()
{
	return (UINT)(HMENU)g_previewMenu;
}

