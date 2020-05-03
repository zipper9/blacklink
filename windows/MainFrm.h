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

#ifndef MAIN_FRM_H
#define MAIN_FRM_H

#define SCALOLAZ_MANY_MONITORS

#include "HubFrame.h"
#include "../client/FavoriteManager.h"
#include "../client/LogManager.h"
#include "../client/ShareManager.h"
#include "../client/WebServerManager.h"
#include "../client/AdlSearch.h"
#include "../client/UserManager.h"
#include "../client/FinishedManagerListener.h"
#include "SingleInstance.h"
#include "TransferView.h"
#include "JAControl.h"
#include "TimerHelper.h"

#define QUICK_SEARCH_MAP 20
#define STATUS_MESSAGE_MAP 9

#define WM_ANIM_CHANGE_FRAME WM_USER + 1

class HIconWrapper;
class MainFrame : public CMDIFrameWindowImpl<MainFrame>, public CUpdateUI<MainFrame>,
	public CMessageFilter, public CIdleHandler, public CSplitterImpl<MainFrame>,
	private QueueManagerListener,
	private WebServerListener,
	private UserManagerListener,
	private FinishedManagerListener,
	private TimerHelper
{
	public:
		MainFrame();
		~MainFrame();
		DECLARE_FRAME_WND_CLASS(_T("FlylinkDC++"), IDR_MAINFRAME) // APPNAME
		
		CMDICommandBarCtrl m_CmdBar;
		
		struct Popup
		{
			tstring Title;
			tstring Message;
			int Icon;
		};
		
		enum
		{
			DOWNLOAD_LISTING,
			BROWSE_LISTING,
			MAIN_STATS,
			PARSE_COMMAND_LINE,
			VIEW_FILE_AND_DELETE,
			SET_STATUSTEXT,
			STATUS_MESSAGE,
			SHOW_POPUP_MESSAGE,
			REMOVE_POPUP,
			SET_PM_TRAY_ICON
		};
		
		BOOL PreTranslateMessage(MSG* pMsg)
		{
			if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
			{
				ctrlLastLines.RelayEvent(pMsg);
			}
			
			if (!IsWindow())
				return FALSE;
				
			if (CMDIFrameWindowImpl<MainFrame>::PreTranslateMessage(pMsg))
				return TRUE;
				
			HWND hWnd = MDIGetActive();
			if (hWnd != NULL && (BOOL)::SendMessage(hWnd, WM_FORWARDMSG, 0, (LPARAM)pMsg))
			{
				return TRUE;
			}
			return FALSE;
		}
		
		BOOL OnIdle()
		{
			UIUpdateToolBar();
			return FALSE;
		}
		typedef CSplitterImpl<MainFrame> splitterBase;
		BEGIN_MSG_MAP(MainFrame)
		MESSAGE_HANDLER(WM_PARENTNOTIFY, onParentNotify)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_SPEAKER_AUTO_CONNECT, onSpeakerAutoConnect)
		MESSAGE_HANDLER(FTM_SELECTED, onSelected)
		MESSAGE_HANDLER(FTM_ROWS_CHANGED, onRowsChanged)
		MESSAGE_HANDLER(WM_APP + 242, onTrayIcon) // TODO небезопасный код!
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_ENDSESSION, onEndSession)
		MESSAGE_HANDLER(m_trayMessage, onTray)
		MESSAGE_HANDLER(m_tbButtonMessage, onTaskbarButton); // [+] InfinitySky.
		MESSAGE_HANDLER(WM_COPYDATA, onCopyData)
		MESSAGE_HANDLER(WMU_WHERE_ARE_YOU, onWhereAreYou)
		MESSAGE_HANDLER(WM_ACTIVATEAPP, onActivateApp)
		MESSAGE_HANDLER(WM_APPCOMMAND, onAppCommand)
		MESSAGE_HANDLER(IDC_REBUILD_TOOLBAR, OnCreateToolbar)
		MESSAGE_HANDLER(WEBSERVER_SOCKET_MESSAGE, onWebServerSocket)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_MENUSELECT, OnMenuSelect)
#ifdef IRAINMAN_INCLUDE_SMILE
		MESSAGE_HANDLER(WM_ANIM_CHANGE_FRAME, OnAnimChangeFrame) // [2] https://www.box.net/shared/2ab8bc29f2f90df352ca
#endif
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_SETTINGS, OnFileSettings)
		COMMAND_ID_HANDLER(IDC_MATCH_ALL, onMatchAll)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_VIEW_TRANSFER_VIEW, OnViewTransferView)
		COMMAND_ID_HANDLER(ID_VIEW_TRANSFER_VIEW_TOOLBAR, OnViewTransferViewToolBar)
		COMMAND_ID_HANDLER(ID_GET_TTH, onGetTTH)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(ID_WINDOW_CASCADE, OnWindowCascade)
		COMMAND_ID_HANDLER(ID_WINDOW_TILE_HORZ, OnWindowTile)
		COMMAND_ID_HANDLER(ID_WINDOW_TILE_VERT, OnWindowTileVert)
		COMMAND_ID_HANDLER(ID_WINDOW_ARRANGE, OnWindowArrangeIcons)
		COMMAND_ID_HANDLER(IDC_RECENTS, onOpenWindows)
		COMMAND_ID_HANDLER(ID_FILE_CONNECT, onOpenWindows)
		COMMAND_ID_HANDLER(ID_FILE_SEARCH, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_FAVORITES, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_FAVUSERS, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_NOTEPAD, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_QUEUE, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_ADD_MAGNET, onAddMagnet)
		COMMAND_ID_HANDLER(IDC_SEARCH_SPY, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_FILE_ADL_SEARCH, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_NET_STATS, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_FINISHED, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_FINISHED_UL, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_UPLOAD_QUEUE, onOpenWindows)
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		COMMAND_ID_HANDLER(IDC_CDMDEBUG_WINDOW, onOpenWindows)
#endif
		COMMAND_ID_HANDLER(IDC_AWAY, onAway)
		COMMAND_ID_HANDLER(IDC_LIMITER, onLimiter)
		COMMAND_ID_HANDLER(IDC_OPEN_FILE_LIST, onOpenFileList)
		COMMAND_ID_HANDLER(IDC_OPEN_TORRENT_FILE, onOpenFileList)
		COMMAND_ID_HANDLER(IDC_OPEN_MY_LIST, onOpenFileList)
		COMMAND_ID_HANDLER(IDC_TRAY_SHOW, onAppShow)
#ifdef SCALOLAZ_MANY_MONITORS
		COMMAND_ID_HANDLER(IDC_SETMASTERMONITOR, onSetDefaultPosition)
#endif
		COMMAND_ID_HANDLER(ID_WINDOW_MINIMIZE_ALL, onWindowMinimizeAll)
		COMMAND_ID_HANDLER(ID_WINDOW_RESTORE_ALL, onWindowRestoreAll)
		COMMAND_ID_HANDLER(IDC_SHUTDOWN, onShutDown)
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
		COMMAND_ID_HANDLER(IDC_FLYLINKDC_LOCATION, onChangeLocation)
#endif
		COMMAND_ID_HANDLER(IDC_FLYLINKDC_FOUND_NEW_VERSION, onFoundNewVersion)
		
		COMMAND_ID_HANDLER(IDC_DISABLE_SOUNDS, onDisableSounds)
		COMMAND_ID_HANDLER(IDC_DISABLE_POPUPS, onDisablePopups)
		COMMAND_ID_HANDLER(IDC_CLOSE_DISCONNECTED, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_PM, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_OFFLINE_PM, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_DIR_LIST, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_SEARCH_FRAME, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_RECONNECT_DISCONNECTED, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_HUBS, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_HUBS_BELOW, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_HUBS_NO_USR, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_OPEN_DOWNLOADS, onOpenDownloads)
		COMMAND_ID_HANDLER(IDC_OPEN_LOGS, onOpenLogs)
		COMMAND_ID_HANDLER(IDC_OPEN_CONFIGS, onOpenConfigs)
		COMMAND_ID_HANDLER(IDC_REFRESH_FILE_LIST, onRefreshFileList)
		COMMAND_ID_HANDLER(IDC_REFRESH_FILE_LIST_PURGE, onRefreshFileListPurge)
		COMMAND_ID_HANDLER(ID_FILE_QUICK_CONNECT, onQuickConnect)
		COMMAND_ID_HANDLER(IDC_HASH_PROGRESS, onHashProgress)
		COMMAND_ID_HANDLER(IDC_TRAY_LIMITER, onLimiter) //[-] NightOrion - Double of MainFrame::onTrayLimiter
		COMMAND_ID_HANDLER(ID_TOGGLE_TOOLBAR, OnViewWinampBar)
		COMMAND_ID_HANDLER(ID_TOGGLE_QSEARCH, OnViewQuickSearchBar)
		COMMAND_ID_HANDLER(IDC_TOPMOST, OnViewTopmost)
		COMMAND_ID_HANDLER(IDC_LOCK_TOOLBARS, onLockToolbars)
		COMMAND_RANGE_HANDLER(IDC_WINAMP_BACK, IDC_WINAMP_VOL_HALF, onWinampButton)
		COMMAND_RANGE_HANDLER(ID_MEDIA_MENU_WINAMP_START, ID_MEDIA_MENU_WINAMP_END, onMediaMenu)
		COMMAND_ID_HANDLER(IDC_STATUS_AWAY_ON_OFF, onAway)
#ifdef SSA_WIZARD_FEATURE
		COMMAND_ID_HANDLER(ID_FILE_SETTINGS_WIZARD, OnFileSettingsWizard)
#endif
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, onGetToolTip)
		NOTIFY_CODE_HANDLER(TTN_POP, onTooltipPop)
		NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnToolbarDropDown)
		CHAIN_MDI_CHILD_COMMANDS()
		CHAIN_MSG_MAP(CUpdateUI<MainFrame>)
		CHAIN_MSG_MAP(CMDIFrameWindowImpl<MainFrame>)
		CHAIN_MSG_MAP(splitterBase);
		ALT_MSG_MAP(QUICK_SEARCH_MAP)
		MESSAGE_HANDLER(WM_CHAR, onQuickSearchChar)
		MESSAGE_HANDLER(WM_KEYDOWN, onQuickSearchChar)
		MESSAGE_HANDLER(WM_KEYUP, onQuickSearchChar)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onQuickSearchColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onQuickSearchColor)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onQuickSearchColor)
		COMMAND_CODE_HANDLER(EN_CHANGE, onQuickSearchEditChange)
		ALT_MSG_MAP(STATUS_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_LBUTTONUP, onContextMenuL)
		END_MSG_MAP()
		
		BEGIN_UPDATE_UI_MAP(MainFrame)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_TRANSFER_VIEW, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_TRANSFER_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_TOGGLE_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_TOGGLE_QSEARCH, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDC_TRAY_LIMITER, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDC_TOPMOST, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDC_LOCK_TOOLBARS, UPDUI_MENUPOPUP)
		END_UPDATE_UI_MAP()
		
		
		LRESULT onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSpeakerAutoConnect(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onHashProgress(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onEndSession(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onGetTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnFileSettings(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMatchAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenFileList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTrayIcon(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnViewTransferView(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnViewTransferViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetToolTip(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onTooltipPop(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT OnToolbarDropDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onCopyData(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onCloseWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRefreshFileList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRefreshFileListPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		#if 0
		LRESULT onConvertTTHHistory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		#endif
		LRESULT onQuickConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onActivateApp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onWebServerSocket(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onAppCommand(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onAway(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLimiter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
		LRESULT onChangeLocation(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onFoundNewVersion(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDisableSounds(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDisablePopups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+] InfinitySky.
		LRESULT onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMediaMenu(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		//LRESULT onTrayLimiter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);//[-] NightOrion - Double of MainFrame::onLimiter
		LRESULT OnViewWinampBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onWinampButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT OnViewTopmost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onContextMenuL(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT OnMenuSelect(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
		void QuerySpeedLimit(const SettingsManager::IntSetting l_limit_normal, int l_min_lim, int l_max_lim);
#endif
		LRESULT onLockToolbars(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onQuickSearchChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onQuickSearchColor(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDeviceChange(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onParentNotify(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onQuickSearchEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);
		LRESULT OnViewQuickSearchBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef IRAINMAN_INCLUDE_SMILE
		LRESULT OnAnimChangeFrame(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
#endif
		LRESULT onAddMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef SSA_WIZARD_FEATURE
		LRESULT OnFileSettingsWizard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		void ViewTransferView(BOOL bVisible);
		void onAwayPush();
		void getTaskbarState();
		static unsigned int WINAPI stopper(void* p);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void onLimiter(const bool l_currentLimiter = BOOLSETTING(THROTTLE_ENABLE)) // [+] IRainman fix
		{
			Util::setLimiter(!l_currentLimiter);
			setLimiterButton(!l_currentLimiter);
		}
		void parseCommandLine(const tstring& cmdLine);
		
		LRESULT onWhereAreYou(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return WMU_WHERE_ARE_YOU; //-V109
		}
		
		LRESULT onTray(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			m_bTrayIcon = false;
			updateTray(true);
			return 0;
		}
		
		void setTrayAndTaskbarIcons(); // [+] IRainman: copy-past fix.
		LRESULT onTaskbarButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		LRESULT onRowsChanged(UINT /*uMsg*/, WPARAM /* wParam */, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (ClientManager::isStartup() == false)
			{
				UpdateLayout();
				Invalidate();
			}
			return 0;
		}
		
		LRESULT onSelected(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			HWND hWnd = (HWND)wParam;
			if (MDIGetActive() != hWnd)
			{
				MDIActivate(hWnd);
			}
			else if (BOOLSETTING(TOGGLE_ACTIVE_WINDOW) && !::IsIconic(hWnd))
			{
				::SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
				MDINext(hWnd);
				hWnd = MDIGetActive();
			}
			if (::IsIconic(hWnd))
				::ShowWindow(hWnd, SW_RESTORE);
			return 0;
		}
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 0;
		}
		
		LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			m_menuclose = true; // [+] InfinitySky. «акрытие через меню.
			PostMessage(WM_CLOSE);
			return 0;
		}
		void openDirs(const string& p_dir)
		{
			// [+] brain-ripper
			// ensure that directory exist. if not - Explorer won't open this dir of course.
			File::ensureDirectory(p_dir);
			WinUtil::openFile(Text::toT(p_dir));
		}
		LRESULT onOpenConfigs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			openDirs(Util::getConfigPath());
			return 0;
		}
		LRESULT onOpenLogs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			openDirs(SETTING(LOG_DIRECTORY));
			return 0;
		}
		LRESULT onOpenDownloads(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			openDirs(SETTING(DOWNLOAD_DIRECTORY));
			return 0;
		}
		
		LRESULT OnWindowCascade(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MDICascade();
			return 0;
		}
		
		LRESULT OnWindowTile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MDITile();
			return 0;
		}
		
		LRESULT OnWindowTileVert(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MDITile(MDITILE_VERTICAL);
			return 0;
		}
		
		LRESULT OnWindowArrangeIcons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MDIIconArrange();
			return 0;
		}
		
		LRESULT onWindowMinimizeAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			HWND tmpWnd = GetWindow(GW_CHILD); //getting client window
			tmpWnd = ::GetWindow(tmpWnd, GW_CHILD); //getting first child window
			while (tmpWnd != NULL)
			{
				::CloseWindow(tmpWnd);
				tmpWnd = ::GetWindow(tmpWnd, GW_HWNDNEXT);
			}
			return 0;
		}
		LRESULT onWindowRestoreAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			HWND tmpWnd = GetWindow(GW_CHILD); //getting client window
			HWND ClientWnd = tmpWnd; //saving client window handle
			tmpWnd = ::GetWindow(tmpWnd, GW_CHILD); //getting first child window
			BOOL bmax;
			while (tmpWnd != NULL)
			{
				::ShowWindow(tmpWnd, SW_RESTORE);
				::SendMessage(ClientWnd, WM_MDIGETACTIVE, NULL, (LPARAM)&bmax);
				if (bmax)
				{
					break; //bmax will be true if active child
				}
				//window is maximized, so if bmax then break
				tmpWnd = ::GetWindow(tmpWnd, GW_HWNDNEXT);
			}
			return 0;
		}
		
		LRESULT onShutDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(!isShutDown());
			setShutDown(!isShutDown());
			return S_OK;
		}
		LRESULT OnCreateToolbar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			createToolbar();
			return S_OK;
		}
		
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		uint8_t m_second_count;
		void onMinute(uint64_t aTick);
		
		static MainFrame* getMainFrame()
		{
			return g_anyMF;
		}
		static bool isAppMinimized(HWND p_hWnd);
		static bool isAppMinimized()
		{
			return g_bAppMinimized;
		}
		CToolBarCtrl& getToolBar()
		{
			return ctrlToolbar;
		}
		
		static void setShutDown(bool b)
		{
			if (b)
			{
				g_CurrentShutdownTime = TimerManager::getTick() / 1000;
			}
			g_isHardwareShutdown = b;
		}
		static bool isShutDown()
		{
			return g_isHardwareShutdown;
		}
		
		static void setAwayButton(bool check)
		{
			if (g_anyMF)
			{
				g_anyMF->ctrlToolbar.CheckButton(IDC_AWAY, check);
			}
		}
		
		static void setLimiterButton(bool check)
		{
			if (g_anyMF)
			{
				g_anyMF->ctrlToolbar.CheckButton(IDC_LIMITER, check);
				g_anyMF->UISetCheck(IDC_TRAY_LIMITER, check);
			}
		}
		
		static void ShowBalloonTip(const tstring& message, const tstring& title, int infoFlags = NIIF_INFO);
		
		CImageList smallImages, largeImages, largeImagesHot, winampImages, winampImagesHot;
		
#ifdef IRAINMAN_IP_AUTOUPDATE
		static void getIPupdate();
		int m_elapsedMinutesFromlastIPUpdate;
#endif
		static void updateQuickSearches(bool p_clean = false);
		
		// SSA
		JAControl* getJAControl()
		{
			return m_jaControl.get();
		}
		
#ifdef SSA_WIZARD_FEATURE
		void SetWizardMode()
		{
			m_is_wizard = true;
		}
#endif
	private:
	
		unique_ptr<JAControl> m_jaControl;
		
		HICON m_appIcon;
		HICON m_trayIcon;
		
		std::unique_ptr<HIconWrapper> m_normalicon;
		std::unique_ptr<HIconWrapper> m_pmicon;
		std::unique_ptr<HIconWrapper> m_emptyicon;//[+]IRainman
		
		CReBarCtrl m_rebar;
		unsigned m_index_new_version_menu_item;
		
		bool getPassword(); // !SMT!-f
		bool getPasswordInternal(INT_PTR& p_do_modal_result);
		
		class DirectoryBrowseInfo
		{
			public:
				DirectoryBrowseInfo(const HintedUser& user, const string& text) : hintedUser(user), text(text) { }
				const HintedUser hintedUser;
				const string text;
		};
		
		TransferView m_transferView;
		static MainFrame* g_anyMF;
		
		enum { MAX_CLIENT_LINES = 10 }; // TODO copy-paste
		enum DefinedStatusParts
		{
			STATUS_PART_MESSAGE,
			STATUS_PART_1,
			STATUS_PART_SHARED_SIZE,
			STATUS_PART_3,
			STATUS_PART_SLOTS,
			STATUS_PART_DOWNLOAD,
			STATUS_PART_UPLOAD,
			STATUS_PART_7,
			STATUS_PART_8,
			STATUS_PART_SHUTDOWN_TIME,
			STATUS_PART_HASH_PROGRESS,
			STATUS_PART_LAST
		};
		
		TStringList lastLinesList;
		tstring lastLines;
		CFlyToolTipCtrl ctrlLastLines;
		CStatusBarCtrl ctrlStatus;
		CContainedWindow statusContainer;
		CProgressBarCtrl ctrlHashProgress;
		bool m_bHashProgressVisible;
		FlatTabCtrl ctrlTab;
		// FlylinkDC Team TODO: needs?
		static int g_CountSTATS; //[+]PPA
		CToolBarCtrl ctrlToolbar;
		CToolBarCtrl ctrlWinampToolbar;
		
		CToolBarCtrl ctrlQuickSearchBar;
		static CComboBox QuickSearchBox;
		CEdit QuickSearchEdit;
		CContainedWindow QuickSearchBoxContainer;
		CContainedWindow QuickSearchEditContainer;
		static bool g_bDisableAutoComplete;
		
		bool m_is_tbarcreated;
		bool m_is_wtbarcreated; // [+]Drakon
		bool m_is_qtbarcreated; // [+]Drakon
		bool m_is_end_session;
		
		bool m_bTrayIcon;
		int tuneTransferSplit();
		void openDefaultWindows();
		bool m_bIsPM;
		static bool g_bAppMinimized;
		
		static bool g_isHardwareShutdown;
		static uint64_t g_CurrentShutdownTime;
		std::unique_ptr<HIconWrapper> m_ShutdownIcon;
		static bool g_isShutdownStatus;
		unsigned m_count_status_change;
		
		CMenu trayMenu;
		CMenu tbMenu;
		CMenu tabAWAYMenu;
		CMenu winampMenu;
		RECT  m_tabAWAYRect;
		
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
		RECT tabSPEED_INRect;
		RECT tabSPEED_OUTRect;
#endif
		
		UINT m_trayMessage;
		UINT m_tbButtonMessage; // [+] InfinitySky.
		
		typedef BOOL (CALLBACK* LPFUNC)(UINT message, DWORD dwFlag); // [+] InfinitySky.
		
		CComPtr<ITaskbarList3> m_taskbarList; // [+] InfinitySky.
		
		/** Was the window maximized when minimizing it? */
		bool m_is_maximized;
		void ShowWindowMax()
		{
			ShowWindow(SW_SHOW);
			ShowWindow(m_is_maximized ? SW_MAXIMIZE : SW_RESTORE);
		}
		
		uint64_t m_lastMove;
// [+]IRainman Speedmeter
	public:
		static uint64_t getLastUpdateTick()
		{
			return g_lastUpdate;
		}
		static int64_t getLastUploadSpeed()
		{
			return g_updiff;
		}
		static int64_t getLastDownloadSpeed()
		{
			return g_downdiff;
		}
	private:
		static uint64_t g_lastUpdate;
		static int64_t g_updiff;
		static int64_t g_downdiff;
		int64_t m_lastUp;
		int64_t m_lastDown;
		int64_t m_diff;
		typedef std::pair<uint64_t, uint64_t> UpAndDown;
		typedef std::pair<uint64_t, UpAndDown> Sample;
		deque<Sample> m_Stats;
		tstring lastTTHdir;
		bool m_oldshutdown;
		bool m_stopexit;
		bool m_menuclose; // [+] InfinitySky.
		void SetOverlayIcon();
		bool m_closing;
		uint8_t m_statusSizes[STATUS_PART_LAST];
		tstring m_statusText[STATUS_PART_LAST];
		HANDLE m_stopperThread;
		bool m_is_missedAutoConnect;
		void fillToolbarButtons(CToolBarCtrl& toolbar, const string& setting, const struct ToolbarButton* buttons, int buttonCount);
		HWND createToolbar();
		HWND createWinampToolbar();
		HWND createQuickSearchBar();
		void updateTray(bool add = true);
		void toggleTopmost() const;
		void toggleLockToolbars() const;
		int  m_numberOfReadBytes;
		int  m_maxnumberOfReadBytes;
		bool m_isOpenHubFrame;
#ifdef SSA_WIZARD_FEATURE
		bool m_is_wizard;
#endif
		bool m_is_start_autoupdate;
		
		LRESULT onAppShow(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/);
#ifdef SCALOLAZ_MANY_MONITORS
		LRESULT onSetDefaultPosition(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/);
#endif
		void autoConnect(const std::vector<FavoriteHubEntry>& hubs);
		
		void setIcon(HICON newIcon); // !SMT!-UI
		void storeWindowsPos();
		
		// WebServerListener
		void on(WebServerListener::Setup) noexcept override;
		void on(WebServerListener::ShutdownPC, int) noexcept override;
		
		// QueueManagerListener
		void on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string& dir, const DownloadPtr& download) noexcept override;
		void on(QueueManagerListener::PartialList, const HintedUser& aUser, const string& text) noexcept override;
		void on(QueueManagerListener::TryAdding, const string& fileName, int64_t newSize, int64_t existingSize, time_t existingTime, int& option) noexcept override;
		void on(QueueManagerListener::SourceAdded) noexcept override;

		// UserManagerListener
		void on(UserManagerListener::OutgoingPrivateMessage, const UserPtr& to, const string& hubHint, const tstring& message) noexcept override;
		void on(UserManagerListener::OpenHub, const string& url) noexcept override;
		void on(UserManagerListener::CollectSummaryInfo, const UserPtr& user, const string& hubHint) noexcept override;
		
		// FinishedManagerListener
		void on(FinishedManagerListener::AddedDl, bool isFile, const FinishedItemPtr&) noexcept;
		void on(FinishedManagerListener::AddedUl, bool isFile, const FinishedItemPtr&) noexcept;
		
		void createTrayMenu();
		void createMainMenu();
#ifdef SSA_WIZARD_FEATURE
		UINT ShowSetupWizard();
#endif
		void shareFolderFromShell(const tstring& folder);
		
#ifdef IRAINMAN_IP_AUTOUPDATE
		class CFlyIPUpdater : public Thread
		{
				bool m_is_running;
				bool m_is_ip_update;
			private:
				int run()
				{
					getIPupdate();
					return 0;
				}
			public:
				CFlyIPUpdater() : m_is_running(false), m_is_ip_update(false)
				{
				}
				void updateIP(bool p_ip_update)
				{
					dcassert(!m_is_running)
					if (m_is_running == false)
					{
						m_is_ip_update = p_ip_update;
						CFlyBusyBool l_busy(m_is_running);
						try
						{
							start(128);
						}
						catch (const ThreadException& e)
						{
							LogManager::message(e.getError());
							// TODO - сохранить такие вещи и скинуть как ошибку
						}
					}
				}
		} m_threadedUpdateIP;
#endif

};

#endif // !defined(MAIN_FRM_H)
