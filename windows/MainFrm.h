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

#include "../client/HubEntry.h"
#include "../client/LogManager.h"
#include "../client/WebServerManager.h"
#include "../client/UserManager.h"
#include "../client/FinishedManagerListener.h"
#include "SingleInstance.h"
#include "TransferView.h"
#include "TimerHelper.h"
#include "UserMessages.h"

#define QUICK_SEARCH_MAP 20
#define STATUS_MESSAGE_MAP 9

class JAControl;
struct ParsedCommandLine;

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
		DECLARE_FRAME_WND_CLASS(_T(APPNAME), IDR_MAINFRAME)
		
		enum
		{
			DOWNLOAD_LISTING,
			BROWSE_LISTING,
			MAIN_STATS,
			PROCESS_COMMAND_LINE,
			VIEW_FILE_AND_DELETE,
			SET_STATUSTEXT,
			STATUS_MESSAGE,
			SHOW_POPUP_MESSAGE,
			REMOVE_POPUP,
			SET_PM_TRAY_ICON
		};

		struct ListenerError
		{
			const char* type;
			int errorCode;
		};

		typedef CSplitterImpl<MainFrame> splitterBase;
		BEGIN_MSG_MAP(MainFrame)
		MESSAGE_HANDLER(WM_PARENTNOTIFY, onParentNotify)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBackground)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WMU_LISTENER_INIT, onListenerInit)
		MESSAGE_HANDLER(WMU_UPDATE_LAYOUT, onUpdateLayout);
		MESSAGE_HANDLER(FTM_SELECTED, onSelected)
		MESSAGE_HANDLER(FTM_ROWS_CHANGED, onRowsChanged)
		MESSAGE_HANDLER(WMU_TRAY_ICON, onTrayIcon)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_ENDSESSION, onEndSession)
		MESSAGE_HANDLER(messageIdTaskbarCreated, onTaskbarCreated)
		MESSAGE_HANDLER(messageIdTaskbarButtonCreated, onTaskbarButtonCreated);
		MESSAGE_HANDLER(WM_COPYDATA, onCopyData)
		MESSAGE_HANDLER(WMU_WHERE_ARE_YOU, onWhereAreYou)
		MESSAGE_HANDLER(WMU_DIALOG_CREATED, onLineDlgCreated)
		MESSAGE_HANDLER(WM_ACTIVATEAPP, onActivateApp)
		MESSAGE_HANDLER(WM_APPCOMMAND, onAppCommand)
		MESSAGE_HANDLER(IDC_REBUILD_TOOLBAR, onCreateToolbar)
		MESSAGE_HANDLER(WEBSERVER_SOCKET_MESSAGE, onWebServerSocket)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_MENUSELECT, onMenuSelect)
#ifdef IRAINMAN_INCLUDE_SMILE
		MESSAGE_HANDLER(WM_ANIM_CHANGE_FRAME, onAnimChangeFrame)
#endif
		COMMAND_ID_HANDLER(ID_APP_EXIT, onFileExit)
		COMMAND_ID_HANDLER(ID_FILE_SETTINGS, onFileSettings)
		COMMAND_ID_HANDLER(IDC_MATCH_ALL, onMatchAll)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, onViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, onViewStatusBar)
		COMMAND_ID_HANDLER(ID_VIEW_TRANSFER_VIEW, onViewTransferView)
		COMMAND_ID_HANDLER(ID_VIEW_TRANSFER_VIEW_TOOLBAR, onViewTransferViewToolBar)
		COMMAND_ID_HANDLER(ID_GET_TTH, onGetTTH)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, onAppAbout)
		COMMAND_ID_HANDLER(ID_WINDOW_CASCADE, onWindowCascade)
		COMMAND_ID_HANDLER(ID_WINDOW_TILE_HORZ, onWindowTile)
		COMMAND_ID_HANDLER(ID_WINDOW_TILE_VERT, onWindowTileVert)
		COMMAND_ID_HANDLER(ID_WINDOW_ARRANGE, onWindowArrangeIcons)
		COMMAND_ID_HANDLER(IDC_RECENTS, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_PUBLIC_HUBS, onOpenWindows)
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
		COMMAND_ID_HANDLER(IDC_DHT, onToggleDHT);
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
		COMMAND_ID_HANDLER(IDC_QUICK_CONNECT, onQuickConnect)
		COMMAND_ID_HANDLER(IDC_HASH_PROGRESS, onHashProgress)
		COMMAND_ID_HANDLER(IDC_TRAY_LIMITER, onLimiter)
		COMMAND_ID_HANDLER(ID_TOGGLE_TOOLBAR, onViewWinampBar)
		COMMAND_ID_HANDLER(ID_TOGGLE_QSEARCH, onViewQuickSearchBar)
		COMMAND_ID_HANDLER(IDC_TOPMOST, onViewTopmost)
		COMMAND_ID_HANDLER(IDC_LOCK_TOOLBARS, onLockToolbars)
		COMMAND_RANGE_HANDLER(IDC_WINAMP_BACK, IDC_WINAMP_VOL_HALF, onWinampButton)
		COMMAND_RANGE_HANDLER(ID_MEDIA_MENU_WINAMP_START, ID_MEDIA_MENU_WINAMP_END, onMediaMenu)
		COMMAND_ID_HANDLER(IDC_STATUS_AWAY_ON_OFF, onAway)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, onGetToolTip)
		NOTIFY_CODE_HANDLER(TTN_POP, onTooltipPop)
		NOTIFY_CODE_HANDLER(TBN_DROPDOWN, onToolbarDropDown)
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
		LRESULT onListenerInit(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onUpdateLayout(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onHashProgress(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onEndSession(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onGetTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onFileSettings(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMatchAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenFileList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTrayIcon(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onViewTransferView(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onViewTransferViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetToolTip(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onTooltipPop(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onToolbarDropDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onCopyData(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onLineDlgCreated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
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
		LRESULT onDisableSounds(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDisablePopups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onToggleDHT(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMediaMenu(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onViewWinampBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onWinampButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onViewTopmost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onContextMenuL(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onMenuSelect(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
		void setSpeedLimit(SettingsManager::IntSetting setting, int minValue, int maxValue);
#endif
		LRESULT onLockToolbars(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onQuickSearchChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onQuickSearchColor(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onParentNotify(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onQuickSearchEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);
		LRESULT onViewQuickSearchBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef IRAINMAN_INCLUDE_SMILE
		LRESULT onAnimChangeFrame(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
#endif
		LRESULT onAddMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		void toggleTransferView(BOOL bVisible);
		static unsigned int WINAPI stopper(void* p);
		void UpdateLayout(BOOL resizeBars = TRUE);
		void onLimiter(bool currentLimiter = BOOLSETTING(THROTTLE_ENABLE))
		{
			Util::setLimiter(!currentLimiter);
			setLimiterButton(!currentLimiter);
		}
		void processCommandLine(const ParsedCommandLine& cmd);
		
		BOOL PreTranslateMessage(MSG* pMsg);
		
		BOOL OnIdle()
		{
			UIUpdateToolBar();
			return FALSE;
		}

		LRESULT onWhereAreYou(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return WMU_WHERE_ARE_YOU; //-V109
		}
		
		LRESULT onTaskbarCreated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		LRESULT onTaskbarButtonCreated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		LRESULT onRowsChanged(UINT /*uMsg*/, WPARAM /* wParam */, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (!ClientManager::isStartup())
			{
				UpdateLayout();
				Invalidate();
			}
			return 0;
		}
		
		LRESULT onSelected(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 0;
		}
		
		LRESULT onFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			quitFromMenu = true;
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		void openDirs(const string& dir)
		{
			tstring tdir = Text::toT(dir);
			File::ensureDirectory(tdir);
			WinUtil::openFile(tdir);
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
		
		LRESULT onWindowCascade(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MDICascade();
			return 0;
		}
		
		LRESULT onWindowTile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MDITile();
			return 0;
		}
		
		LRESULT onWindowTileVert(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MDITile(MDITILE_VERTICAL);
			return 0;
		}
		
		LRESULT onWindowArrangeIcons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
			setShutDown(!isShutDown());
			return S_OK;
		}

		LRESULT onCreateToolbar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			createToolbar();
			return S_OK;
		}
		
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		void onMinute(uint64_t tick);
		
		static MainFrame* getMainFrame()
		{
			return instance;
		}
		
		static bool isAppMinimized(HWND hWnd);
		static bool isAppMinimized() { return appMinimized; }
		CToolBarCtrl& getToolBar() { return ctrlToolbar; }
		
		void setShutDown(bool flag)
		{
			if (flag)
				shutdownTime = TimerManager::getTick() / 1000;
			shutdownEnabled = flag;
		}

		bool isShutDown() const
		{
			return shutdownEnabled;
		}
		
		static void setAwayButton(bool check)
		{
			if (instance)
			{
				instance->ctrlToolbar.CheckButton(IDC_AWAY, check);
			}
		}
		
		static void setLimiterButton(bool check)
		{
			if (instance)
			{
				instance->ctrlToolbar.CheckButton(IDC_LIMITER, check);
				instance->UISetCheck(IDC_TRAY_LIMITER, check);
			}
		}
		
		static void ShowBalloonTip(const tstring& message, const tstring& title, int infoFlags = NIIF_INFO);

		void updateQuickSearches(bool clear = false);

		JAControl* getJAControl() { return jaControl.get(); }
		
		CImageList& getToolbarImages() { return largeImages; }
		CImageList& getToolbarHotImages() { return largeImagesHot; }
		CImageList& getSmallToolbarImages() { return smallImages; }
		CImageList& getSettingsImages() { return settingsImages; }

	private:
		static MainFrame* instance;

		struct Popup
		{
			tstring title;
			tstring message;
			int icon;
		};
		
		// Controls
		CMDICommandBarCtrl ctrlCmdBar;
		CReBarCtrl ctrlRebar;
		FlatTabCtrl ctrlTab;
		CToolBarCtrl ctrlToolbar;
		CToolBarCtrl ctrlWinampToolbar;
		TransferView transferView;

		// Images
		CImageList smallImages, largeImages, largeImagesHot, winampImages, winampImagesHot;
		CImageList settingsImages;
		HIconWrapper mainIcon;
		HIconWrapper pmIcon;

		class DirectoryBrowseInfo
		{
			public:
				DirectoryBrowseInfo(const HintedUser& user, const string& text) : hintedUser(user), text(text) { }
				const HintedUser hintedUser;
				const string text;
		};
		
		// Status bar
		enum { MAX_CLIENT_LINES = 10 };
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
		CStatusBarCtrl ctrlStatus;
		CFlyToolTipCtrl ctrlLastLines;
		int statusSizes[STATUS_PART_LAST];
		tstring statusText[STATUS_PART_LAST];
		RECT tabAwayRect;
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
		RECT tabDownSpeedRect;
		RECT tabUpSpeedRect;
#endif
		CContainedWindow statusContainer;
		CProgressBarCtrl ctrlHashProgress;
		bool hashProgressVisible;
		unsigned updateStatusBar;

		// Tray icon
		bool useTrayIcon;
		bool hasPM;
		int trayIcon;
		uint64_t lastTickMouseMove;
		
		// Quick search
		CToolBarCtrl ctrlQuickSearchBar;
		CComboBox quickSearchBox;
		CEdit quickSearchEdit;
		CContainedWindow quickSearchBoxContainer;
		CContainedWindow quickSearchEditContainer;
		bool disableAutoComplete;
		
		// Menu
		CMenu trayMenu;
		CMenu tbMenu;
		CMenu tabAwayMenu;
		CMenu winampMenu;
		
		// Taskbar
		UINT messageIdTaskbarCreated;
		UINT messageIdTaskbarButtonCreated;		
		CComPtr<ITaskbarList3> taskbarList;
		
		// Flags
		static bool appMinimized;
		bool wasMaximized; // Was the window maximized when minimizing it?
		bool quitFromMenu;
		bool closing;
		bool processingStats; // ???
		bool endSession;
		bool autoConnectFlag;
		bool updateLayoutFlag;

		// Timers
		int secondsCounter;
		uint64_t timeUsersCleanup;
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		uint64_t timeFlushRatio;
#endif

		// Shutdown
		bool shutdownEnabled;
		uint64_t shutdownTime;
		bool shutdownStatusDisplayed;
		
		// Misc
		unique_ptr<JAControl> jaControl;
		HWND passwordDlg;
		tstring lastTTHdir;
		HANDLE stopperThread;

		void showWindow()
		{
			ShowWindow(SW_SHOW);
			ShowWindow(wasMaximized ? SW_MAXIMIZE : SW_RESTORE);
		}

		void fillToolbarButtons(CToolBarCtrl& toolbar, const string& setting, const struct ToolbarButton* buttons, int buttonCount);
		HWND createToolbar();
		HWND createWinampToolbar();
		HWND createQuickSearchBar();
		void toggleTopmost();
		void toggleLockToolbars();
		
		LRESULT onAppShow(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/);
#ifdef SCALOLAZ_MANY_MONITORS
		LRESULT onSetDefaultPosition(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/);
#endif
		void autoConnect(const std::vector<FavoriteHubEntry>& hubs);
		void openDefaultWindows();
		int tuneTransferSplit();
		void setAway(bool flag);
		
		void setTrayIcon(int newIcon);
		void clearPMStatus();
		void storeWindowsPos();
		
		void createTrayMenu();
		void createMainMenu();

		bool getPassword();
		bool getPasswordInternal(INT_PTR& result, HWND hwndParent);

		void shareFolderFromShell(const tstring& folder);
		
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
};

#endif // !defined(MAIN_FRM_H)
