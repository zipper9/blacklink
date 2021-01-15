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
#include "Resource.h"
#include "MainFrm.h"
#include "HubFrame.h"
#include "SearchFrm.h"
#include "PropertiesDlg.h"
#include "NetworkPage.h"
#include "UsersFrame.h"
#include "DirectoryListingFrm.h"
#include "RecentsFrm.h"
#include "FavoritesFrm.h"
#include "NotepadFrame.h"
#include "QueueFrame.h"
#include "SpyFrame.h"
#include "FinishedDLFrame.h"
#include "FinishedULFrame.h"
#include "ADLSearchFrame.h"
#ifdef FLYLINKDC_USE_STATS_FRAME
# include "StatsFrame.h"
#endif
#include "WaitingUsersFrame.h"
#include "LineDlg.h"
#include "HashProgressDlg.h"
#include "PrivateFrame.h"
#include "WinUtil.h"
#include "CDMDebugFrame.h"
#include "FileHashDlg.h"
#include "PopupManager.h"
#include "ResourceLoader.h"
#include "Toolbar.h"
#include "AGEmotionSetup.h"
#include "Players.h"
#include "Winamp.h"
#include "JAControl.h"
#include "iTunesCOMInterface.h"
#include "ToolbarManager.h"
#include "AboutDlgIndex.h"
#include "AddMagnet.h"
#include "CheckTargetDlg.h"
#ifdef IRAINMAN_INCLUDE_SMILE
# include "../GdiOle/GDIImage.h"
#endif
#include "../client/ConnectionManager.h"
#include "../client/ConnectivityManager.h"
#include "../client/UploadManager.h"
#include "../client/DownloadManager.h"
#include "../client/HashManager.h"
#include "../client/LogManager.h"
#include "../client/WebServerManager.h"
#include "../client/ThrottleManager.h"
#include "../client/Text.h"
#include "../client/NmdcHub.h"
#include "../client/SimpleStringTokenizer.h"
#include "../client/dht/DHT.h"
#include "HIconWrapper.h"
#include "PrivateFrame.h"
#include "PublicHubsFrm.h"

#ifdef SCALOLAZ_SPEEDLIMIT_DLG
# include "SpeedVolDlg.h"
#endif

#include "ExMessageBox.h"
#include "CommandLine.h"

#define FLYLINKDC_CALC_MEMORY_USAGE // TODO: move to CompatibilityManager
#  ifdef FLYLINKDC_CALC_MEMORY_USAGE
#   ifdef FLYLINKDC_SUPPORT_WIN_VISTA
#    define PSAPI_VERSION 1
#   endif
#   include <psapi.h>
#   pragma comment(lib, "psapi.lib")
#endif // FLYLINKDC_CALC_MEMORY_USAGE

extern ParsedCommandLine cmdLine;

bool g_TabsCloseButtonEnabled;
DWORD g_GDI_count = 0;
int   g_RAM_WorkingSetSize = 0;
int   g_RAM_PeakWorkingSetSize = 0;

#define FLYLINKDC_USE_TASKBUTTON_PROGRESS

MainFrame* MainFrame::instance = nullptr;
bool MainFrame::appMinimized = false;

static const int STATUS_PART_PADDING = 12;

enum
{
	TRAY_ICON_NONE,
	TRAY_ICON_NORMAL,
	TRAY_ICON_PM
};

static const string emptyStringHash("LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ");

static bool hasPasswordTray()
{
	if (!SETTING(PROTECT_TRAY)) return false;
	const string& password = SETTING(PASSWORD);
	return !password.empty() && password != emptyStringHash;
}

static bool hasPasswordClose()
{
	if (!SETTING(PROTECT_CLOSE)) return false;
	const string& password = SETTING(PASSWORD);
	return !password.empty() && password != emptyStringHash;
}

MainFrame::MainFrame() :
	CSplitterImpl(false),
	TimerHelper(m_hWnd),
	statusContainer(STATUSCLASSNAME, this, STATUS_MESSAGE_MAP),
	hashProgressVisible(false),
	updateStatusBar(0),
	useTrayIcon(true),
	hasPM(false),
	trayIcon(TRAY_ICON_NONE),
	lastTickMouseMove(0),
	quickSearchBoxContainer(WC_COMBOBOX, this, QUICK_SEARCH_MAP),
	quickSearchEditContainer(WC_EDIT, this, QUICK_SEARCH_MAP),
	disableAutoComplete(false),
	messageIdTaskbarCreated(0),
	messageIdTaskbarButtonCreated(0),
	wasMaximized(false),
	quitFromMenu(false),
	closing(false),
	processingStats(false),
	endSession(false),
	autoConnectFlag(true),
	updateLayoutFlag(false),
	secondsCounter(60),
	shutdownEnabled(false),
	shutdownTime(0),
	shutdownStatusDisplayed(false),
	passwordDlg(nullptr),
	stopperThread(nullptr)
{
	m_bUpdateProportionalPos = false;
	memset(statusSizes, 0, sizeof(statusSizes));
	auto tick = GET_TICK();
	timeUsersCleanup = tick + Util::rand(3, 10)*60000;
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	timeFlushRatio = tick + Util::rand(3, 10)*60000;
#endif
	instance = this;
}

bool MainFrame::isAppMinimized(HWND hWnd)
{
	return appMinimized && WinUtil::g_tabCtrl && WinUtil::g_tabCtrl->isActive(hWnd);
}

MainFrame::~MainFrame()
{
	LogManager::g_mainWnd = nullptr;
	ctrlCmdBar.m_hImageList = NULL;
	smallImages.Destroy();
	largeImages.Destroy();
	largeImagesHot.Destroy();
	winampImages.Destroy();
	winampImagesHot.Destroy();
	settingsImages.Destroy();
	
#ifdef IRAINMAN_INCLUDE_SMILE
	CAGEmotionSetup::destroyEmotionSetup();
#endif
	WinUtil::uninit();
}

// Send WM_CLOSE to each MDI child window
unsigned int WINAPI MainFrame::stopper(void* p)
{
	MainFrame* mf = (MainFrame*)p;
	HWND wnd = nullptr;
	HWND lastWnd = nullptr;
	boost::unordered_map<HWND, int> m;
	while (mf->m_hWndMDIClient && (wnd = ::GetWindow(mf->m_hWndMDIClient, GW_CHILD)) != nullptr)
	{
		int& counter = m[wnd];
		++counter;
		if (wnd == lastWnd)
		{
			if (counter > 1000)
			{
				LogManager::message("Forced shutdown, hwnd=0x" + Util::toHexString(wnd));
				break;
			}
			Sleep(10);
		}
		else
		{
			::PostMessage(wnd, WM_CLOSE, 0, 0);
			lastWnd = wnd;
		}
	}
	
	mf->PostMessage(WM_CLOSE);
	return 0;
}

LRESULT MainFrame::onMatchAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	QueueManager::getInstance()->matchAllFileLists();
	return 0;
}

void MainFrame::createMainMenu(void)
{
	// Loads images and creates command bar window
	ctrlCmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	ctrlCmdBar.SetImageSize(16, 16);
	m_hMenu = WinUtil::g_mainMenu;
	
	ctrlCmdBar.AttachMenu(m_hMenu);
	
	CImageList tmp;
	
	ResourceLoader::LoadImageList(IDR_TOOLBAR_MINI, smallImages, 16, 16);
	ResourceLoader::LoadImageList(IDR_PLAYERS_CONTROL_MINI, tmp, 16, 16);
	
	int imageCount = tmp.GetImageCount();
	for (int i = 0; i < imageCount; i++)
	{
		HICON icon = tmp.GetIcon(i);
		smallImages.AddIcon(icon);
		DestroyIcon(icon);
	}
	
	tmp.Destroy();
	
	ctrlCmdBar.m_hImageList = smallImages;
	
	for (size_t i = 0; g_ToolbarButtons[i].id; i++)
		ctrlCmdBar.m_arrCommand.Add(g_ToolbarButtons[i].id);
	
	// Add menu icons that are not used in toolbar
	for (size_t i = 0; g_MenuImages[i].id; i++)
		ctrlCmdBar.m_arrCommand.Add(g_MenuImages[i].id);
	
	for (size_t i = 0; g_WinampToolbarButtons[i].id; i++)
		ctrlCmdBar.m_arrCommand.Add(g_WinampToolbarButtons[i].id);
	
#if _WTL_CMDBAR_VISTA_MENUS
	// Use Vista-styled menus for Windows Vista and later.
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (CompatibilityManager::isOsVistaPlus())
#endif
	{
		ctrlCmdBar._AddVistaBitmapsFromImageList(0, ctrlCmdBar.m_arrCommand.GetSize());
	}
#endif
	
	SetMenu(NULL);  // remove old menu
}

void MainFrame::createTrayMenu()
{
	trayMenu.CreatePopupMenu();
	trayMenu.AppendMenu(MF_STRING, IDC_TRAY_SHOW, CTSTRING(MENU_SHOW));
	trayMenu.AppendMenu(MF_STRING, IDC_OPEN_DOWNLOADS, CTSTRING(MENU_OPEN_DOWNLOADS_DIR));
	
	trayMenu.AppendMenu(MF_SEPARATOR);
	trayMenu.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST, CTSTRING(MENU_REFRESH_FILE_LIST));
	trayMenu.AppendMenu(MF_STRING, IDC_TRAY_LIMITER, CTSTRING(TRAY_LIMITER));
	trayMenu.AppendMenu(MF_SEPARATOR);
	trayMenu.AppendMenu(MF_STRING, ID_FILE_SETTINGS, CTSTRING(MENU_SETTINGS));
	
	trayMenu.AppendMenu(MF_SEPARATOR);
	trayMenu.AppendMenu(MF_STRING, ID_APP_ABOUT, CTSTRING(MENU_ABOUT));
	trayMenu.AppendMenu(MF_SEPARATOR);
#ifdef SCALOLAZ_MANY_MONITORS
	trayMenu.AppendMenu(MF_STRING, IDC_SETMASTERMONITOR, CTSTRING(RESTORE_WINDOW_POS));
#endif
	trayMenu.AppendMenu(MF_STRING, ID_APP_EXIT, CTSTRING(MENU_EXIT));
	trayMenu.SetMenuDefaultItem(IDC_TRAY_SHOW);
}

LRESULT MainFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (CompatibilityManager::isIncompatibleSoftwareFound())
	{
		if (DatabaseManager::getInstance()->getRegistryVarString(e_IncopatibleSoftwareList) != CompatibilityManager::getIncompatibleSoftwareList())
		{
			DatabaseManager::getInstance()->setRegistryVarString(e_IncopatibleSoftwareList, CompatibilityManager::getIncompatibleSoftwareList());
			LogManager::message("CompatibilityManager: " + CompatibilityManager::getIncompatibleSoftwareList());
			if (MessageBox(Text::toT(CompatibilityManager::getIncompatibleSoftwareMessage()).c_str(), getAppNameVerT().c_str(), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)
			{
				//WinUtil::openLink(WinUtil::GetWikiLink() + _T("incompatiblesoftware"));
			}
		}
	}
	
	QueueManager::getInstance()->addListener(this);
	WebServerManager::getInstance()->addListener(this);
	UserManager::getInstance()->addListener(this);
	FinishedManager::getInstance()->addListener(this);
	
	if (SETTING(NICK).empty())
	{
		ShowWindow(SW_RESTORE);
	}
	if (BOOLSETTING(WEBSERVER))
	{
		try
		{
			WebServerManager::getInstance()->Start();
		}
		catch (const Exception& e)
		{
			MessageBox(Text::toT(e.getError()).c_str(), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		}
	}
	
	LogManager::g_mainWnd = m_hWnd;
	LogManager::g_LogMessageID = STATUS_MESSAGE;
	LogManager::g_isLogSpeakerEnabled = true;
	WinUtil::init(m_hWnd);
	
	messageIdTaskbarCreated = RegisterWindowMessage(_T("TaskbarCreated"));
	dcassert(messageIdTaskbarCreated);
	
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (messageIdTaskbarCreated && CompatibilityManager::isOsVistaPlus())
#endif
	{
		typedef BOOL (CALLBACK* LPFUNC)(UINT message, DWORD dwFlag);
		HMODULE module = GetModuleHandle(_T("user32.dll"));
		if (module)
		{
			LPFUNC changeWindowMessageFilter = (LPFUNC) GetProcAddress(module, "ChangeWindowMessageFilter");
			if (changeWindowMessageFilter)
			{
				// 1 == MSGFLT_ADD
				changeWindowMessageFilter(messageIdTaskbarCreated, 1);
				changeWindowMessageFilter(WMU_WHERE_ARE_YOU, 1);
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
				if (CompatibilityManager::isWin7Plus())
#endif
				{
					messageIdTaskbarButtonCreated = RegisterWindowMessage(_T("TaskbarButtonCreated"));
					changeWindowMessageFilter(messageIdTaskbarButtonCreated, 1);
					changeWindowMessageFilter(WM_COMMAND, 1);
				}
			}
		}
	}
	
	TimerManager::getInstance()->start(0, "TimerManager");
	SetWindowText(getAppNameVerT().c_str());
	createMainMenu();

	ResourceLoader::LoadImageList(IDR_SETTINGS_ICONS, settingsImages, 16, 16);
	
	HWND hWndToolBar = createToolbar();
	HWND hWndQuickSearchBar = createQuickSearchBar();
	HWND hWndWinampBar = createWinampToolbar();
	
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	
	AddSimpleReBarBand(ctrlCmdBar);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);
		
	AddSimpleReBarBand(hWndQuickSearchBar, NULL, FALSE, 200, TRUE);
	AddSimpleReBarBand(hWndWinampBar, NULL, TRUE);
	
	CreateSimpleStatusBar();
	
	ctrlRebar = m_hWndToolBar;
	ToolbarManager::getInstance()->applyTo(m_hWndToolBar, "MainToolBar");
	
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.SetSimple(FALSE); // https://www.box.net/shared/6d96012d9690dc892187
	int w[STATUS_PART_LAST - 1] = {0};
	ctrlStatus.SetParts(STATUS_PART_LAST - 1, w);
	statusSizes[0] = WinUtil::getTextWidth(TSTRING(AWAY_STATUS), ctrlStatus) + STATUS_PART_PADDING;
	
	statusContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	RECT rect = {0};
	ctrlHashProgress.Create(ctrlStatus, &rect, _T("Hashing"), WS_CHILD | PBS_SMOOTH, 0, IDC_STATUS_HASH_PROGRESS);
	ctrlHashProgress.SetRange(0, HashProgressDlg::MAX_PROGRESS_VALUE);
	ctrlHashProgress.SetStep(1);
	
	tabAwayMenu.CreatePopupMenu();
	tabAwayMenu.AppendMenu(MF_STRING, IDC_STATUS_AWAY_ON_OFF, CTSTRING(AWAY));
	
	ctrlLastLines.Create(ctrlStatus, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	ctrlLastLines.SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	ctrlLastLines.AddTool(ctrlStatus.m_hWnd);
	ctrlLastLines.SetDelayTime(TTDT_AUTOPOP, 15000);
	
	CreateMDIClient();
	ctrlCmdBar.SetMDIClient(m_hWndMDIClient);
	WinUtil::g_mdiClient = m_hWndMDIClient;
	ctrlTab.setOptions(SETTING(TABS_POS), SETTING(MAX_TAB_ROWS), SETTING(TAB_SIZE), true, BOOLSETTING(TABS_CLOSEBUTTONS), BOOLSETTING(TABS_BOLD), BOOLSETTING(NON_HUBS_FRONT), false);
	ctrlTab.Create(m_hWnd, rcDefault);
	WinUtil::g_tabCtrl = &ctrlTab;
	
	transferView.Create(m_hWnd);
	toggleTransferView(BOOLSETTING(SHOW_TRANSFERVIEW));
	
	tuneTransferSplit();
	
	UIAddToolBar(hWndToolBar);
	UIAddToolBar(hWndWinampBar);
	UIAddToolBar(hWndQuickSearchBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	UISetCheck(ID_VIEW_TRANSFER_VIEW, BOOLSETTING(SHOW_TRANSFERVIEW));
	UISetCheck(ID_VIEW_TRANSFER_VIEW_TOOLBAR, BOOLSETTING(SHOW_TRANSFERVIEW_TOOLBAR));
	UISetCheck(ID_TOGGLE_TOOLBAR, 1);
	UISetCheck(ID_TOGGLE_QSEARCH, 1);
	
	UISetCheck(IDC_TRAY_LIMITER, BOOLSETTING(THROTTLE_ENABLE));
	UISetCheck(IDC_TOPMOST, BOOLSETTING(TOPMOST));
	UISetCheck(IDC_LOCK_TOOLBARS, BOOLSETTING(LOCK_TOOLBARS));
	
	if (BOOLSETTING(TOPMOST))
	{
		toggleTopmost();
	}
	if (BOOLSETTING(LOCK_TOOLBARS))
	{
		toggleLockToolbars();
	}
	
	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);
	
	createTrayMenu();
	
	tbMenu.CreatePopupMenu();
	tbMenu.AppendMenu(MF_STRING, ID_VIEW_TOOLBAR, CTSTRING(MENU_TOOLBAR));
	tbMenu.AppendMenu(MF_STRING, ID_TOGGLE_TOOLBAR, CTSTRING(TOGGLE_TOOLBAR));
	tbMenu.AppendMenu(MF_STRING, ID_TOGGLE_QSEARCH, CTSTRING(TOGGLE_QSEARCH));
	
	tbMenu.AppendMenu(MF_SEPARATOR);
	tbMenu.AppendMenu(MF_STRING, IDC_LOCK_TOOLBARS, CTSTRING(LOCK_TOOLBARS));
	
	winampMenu.CreatePopupMenu();
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::WinAmp, CTSTRING(MEDIA_MENU_WINAMP));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::WinMediaPlayer, CTSTRING(MEDIA_MENU_WMP));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::iTunes, CTSTRING(MEDIA_MENU_ITUNES));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::WinMediaPlayerClassic, CTSTRING(MEDIA_MENU_WPC));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::JetAudio, CTSTRING(MEDIA_MENU_JA));
	
	File::ensureDirectory(SETTING(LOG_DIRECTORY));
	
#if 0
	if (SETTING(PROTECT_START) && SETTING(PASSWORD) != emptyStringHash && !SETTING(PASSWORD).empty())
	{
		INT_PTR result;
		if (getPasswordInternal(result, m_hWnd) && result == IDOK)
		{
			ExitProcess(1); // ???
		}
	}
#endif	
	
	openDefaultWindows();
	
	ConnectivityManager::getInstance()->setupConnections();
	
	PostMessage(WM_SPEAKER, PROCESS_COMMAND_LINE);
	
	mainIcon = HIconWrapper(IDR_MAINFRAME);
	pmIcon = HIconWrapper(IDR_TRAY_AND_TASKBAR_PM);
	
	if (useTrayIcon) setTrayIcon(TRAY_ICON_NORMAL);
	
	Util::setAway(BOOLSETTING(AWAY), true);
	
	ctrlToolbar.CheckButton(IDC_AWAY, BOOLSETTING(AWAY));
	ctrlToolbar.CheckButton(IDC_LIMITER, BOOLSETTING(THROTTLE_ENABLE));
	ctrlToolbar.CheckButton(IDC_DISABLE_SOUNDS, BOOLSETTING(SOUNDS_DISABLED));
	ctrlToolbar.CheckButton(IDC_DISABLE_POPUPS, BOOLSETTING(POPUPS_DISABLED));
	ctrlToolbar.CheckButton(ID_TOGGLE_TOOLBAR, BOOLSETTING(SHOW_PLAYER_CONTROLS));
	
	if (SETTING(NICK).empty())
	{
		PostMessage(WM_COMMAND, ID_FILE_SETTINGS);
	}
	
	jaControl = unique_ptr<JAControl>(new JAControl((HWND)(*this)));
	
	// We want to pass this one on to the splitter...hope it get's there...
	bHandled = FALSE;
	
	transferView.UpdateLayout();

#ifdef FLYLINKDC_USE_TORRENT
	DownloadManager::getInstance()->init_torrent();
#endif
	
#ifdef IRAINMAN_INCLUDE_SMILE
	CAGEmotionSetup::reCreateEmotionSetup();
#endif

	if (!PopupManager::isValidInstance())
		PopupManager::newInstance();

	PostMessage(WMU_UPDATE_LAYOUT);
	createTimer(1000, 3);

	return 0;
}

void MainFrame::openDefaultWindows()
{
	static const struct
	{
		SettingsManager::IntSetting setting;
		int command;
		bool value;
	} openSettings[] =
	{
		{ SettingsManager::OPEN_FAVORITE_HUBS,      IDC_FAVORITES,       true  },
		{ SettingsManager::OPEN_FAVORITE_USERS,     IDC_FAVUSERS,        true  },
		{ SettingsManager::OPEN_PUBLIC_HUBS,        IDC_PUBLIC_HUBS,     true  },
		{ SettingsManager::OPEN_QUEUE,              IDC_QUEUE,           true  },
		{ SettingsManager::OPEN_FINISHED_DOWNLOADS, IDC_FINISHED,        true  },
		{ SettingsManager::OPEN_WAITING_USERS,      IDC_UPLOAD_QUEUE,    true  },
		{ SettingsManager::OPEN_FINISHED_UPLOADS,   IDC_FINISHED_UL,     true  },
		{ SettingsManager::OPEN_SEARCH_SPY,         IDC_SEARCH_SPY,      true  },
		{ SettingsManager::OPEN_NETWORK_STATISTICS, IDC_NET_STATS,       true  },
		{ SettingsManager::OPEN_NOTEPAD,            IDC_NOTEPAD,         true  },
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		{ SettingsManager::OPEN_CDMDEBUG,           IDC_CDMDEBUG_WINDOW, true  },
#endif
		{ SettingsManager::SHOW_STATUSBAR,          ID_VIEW_STATUS_BAR,  false },
		{ SettingsManager::SHOW_TOOLBAR,            ID_VIEW_TOOLBAR,     false },
		{ SettingsManager::SHOW_PLAYER_CONTROLS,    ID_TOGGLE_TOOLBAR,   false },
		{ SettingsManager::SHOW_QUICK_SEARCH,       ID_TOGGLE_QSEARCH,   false }
	};
	for (int i = 0; i < _countof(openSettings); ++i)
		if (g_settings->getBool(openSettings[i].setting) == openSettings[i].value)
			PostMessage(WM_COMMAND, openSettings[i].command);
}

int MainFrame::tuneTransferSplit()
{
	int splitSize = SETTING(TRANSFER_FRAME_SPLIT);
	m_nProportionalPos = splitSize;
	if (m_nProportionalPos < 3000 || m_nProportionalPos > 9400)
	{
		m_nProportionalPos = 9100; // TODO - пофиксить
	}
	SET_SETTING(TRANSFER_FRAME_SPLIT, m_nProportionalPos);
	SetSplitterPanes(m_hWndMDIClient, transferView.m_hWnd);
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
	return m_nProportionalPos;
}

LRESULT MainFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	
	if (closing)
		return 0;

	const uint64_t tick = GET_TICK();
	if (--secondsCounter == 0)
	{
		secondsCounter = 60;
		onMinute(tick);
	}
	if (ClientManager::isStartup())
		return 0;

	const uint64_t currentUp   = Socket::g_stats.tcp.uploaded + Socket::g_stats.ssl.uploaded;
	const uint64_t currentDown = Socket::g_stats.tcp.downloaded + Socket::g_stats.ssl.downloaded;
	if (useTrayIcon && hasPM)
		setTrayIcon(((tick / 1000) & 1) ? TRAY_ICON_NORMAL : TRAY_ICON_PM);

	PROCESS_MEMORY_COUNTERS pmc = { 0 };
	BOOL memoryInfoResult = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	if (memoryInfoResult)
	{
		g_GDI_count = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
		g_RAM_WorkingSetSize = pmc.WorkingSetSize >> 20;
		g_RAM_PeakWorkingSetSize = pmc.PeakWorkingSetSize >> 20;
	}
	if (!appMinimized || IsWindowVisible())
	{
		if (updateStatusBar)
		{
			UpdateLayout(TRUE);
			updateStatusBar = 0;
		}
		
#ifdef FLYLINKDC_CALC_MEMORY_USAGE
		if (!CompatibilityManager::isWine())
		{
			if ((tick / 1000) % 5 == 0)
			{
				TCHAR buf[128];
				if (memoryInfoResult)
				{
					CompatibilityManager::caclPhysMemoryStat();
					_sntprintf(buf, _countof(buf), _T(" [RAM: %dM / %dM][Free: %dM][GDI: %d]"),
					           g_RAM_WorkingSetSize,
					           g_RAM_PeakWorkingSetSize,
					           int(CompatibilityManager::getFreePhysMemory() >> 20),
					           int(g_GDI_count));
				}
				tstring title = getAppNameVerT();
				title += buf;
				SetWindowText(title.c_str());
			}
		}
#else
		if (BOOLSETTING(SHOW_CURRENT_SPEED_IN_TITLE))
		{
			const tstring dlstr = Util::formatBytesT(DownloadManager::getRunningAverage());
			const tstring ulstr = Util::formatBytesT(UploadManager::getRunningAverage());
			tstring title = TSTRING(DL) + _T(' ') + dlstr + _T(" / ") + TSTRING(UP) + _T(' ') + ulstr + _T("  -  ");
			title += getAppNameVerT();
			SetWindowText(title.c_str());
		}
#endif // FLYLINKDC_CALC_MEMORY_USAGE
		
		if (!processingStats)
		{
			dcassert(!ClientManager::isStartup());
			const tstring dlstr = Util::formatBytesT(DownloadManager::getRunningAverage());
			const tstring ulstr = Util::formatBytesT(UploadManager::getRunningAverage());
			TStringList* stats = new TStringList();
			stats->push_back(Util::getAway() ? TSTRING(AWAY_STATUS) : Util::emptyStringT);
			unsigned normal, registered, op;
			Client::getCounts(normal, registered, op);
			TCHAR hubCounts[64];
			_sntprintf(hubCounts, _countof(hubCounts), _T(" %u/%u/%u"), normal, registered, op);
			stats->push_back(TSTRING(SHARED) + _T(": ") + Util::formatBytesT(ShareManager::getInstance()->getTotalSharedSize()));
			stats->push_back(TSTRING(H) + hubCounts);
			stats->push_back(TSTRING(SLOTS) + _T(": ") + Util::toStringT(UploadManager::getFreeSlots()) + _T('/') + Util::toStringT(UploadManager::getSlots())
			                 + _T(" (") + Util::toStringT(UploadManager::getInstance()->getFreeExtraSlots()) + _T('/') + Util::toStringT(SETTING(EXTRA_SLOTS)) + _T(")"));
			stats->push_back(TSTRING(D) + _T(' ') + Util::formatBytesT(currentDown));
			stats->push_back(TSTRING(U) + _T(' ') + Util::formatBytesT(currentUp));
			const bool throttleEnabled = BOOLSETTING(THROTTLE_ENABLE);
			stats->push_back(TSTRING(D) + _T(" [") + Util::toStringT(DownloadManager::getDownloadCount()) + _T("][")
			                 + ((!throttleEnabled|| ThrottleManager::getInstance()->getDownloadLimitInKBytes() == 0) ?
			                    TSTRING(N) : Util::toStringT((int)ThrottleManager::getInstance()->getDownloadLimitInKBytes()) + TSTRING(KILO)) + _T("] ")
			                 + dlstr + _T('/') + TSTRING(S));
			stats->push_back(TSTRING(U) + _T(" [") + Util::toStringT(UploadManager::getUploadCount()) + _T("][")
			                 + ((!throttleEnabled || ThrottleManager::getInstance()->getUploadLimitInKBytes() == 0) ?
			                    TSTRING(N) : Util::toStringT((int)ThrottleManager::getInstance()->getUploadLimitInKBytes()) + TSTRING(KILO)) + _T("] ")
			                 + ulstr + _T('/') + TSTRING(S));
			processingStats = true;
			if (!PostMessage(WM_SPEAKER, MAIN_STATS, (LPARAM) stats))
			{
				dcassert(0);
				processingStats = false;
				delete stats;
			}
		}
	}
	return 0;
}

void MainFrame::onMinute(uint64_t tick)
{
	HublistManager::getInstance()->removeUnusedConnections();
	if (tick >= timeUsersCleanup)
	{
		ClientManager::usersCleanup();
		timeUsersCleanup = tick + Util::rand(3, 10)*60000;
	}
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	if (tick >= timeFlushRatio)
	{
		ClientManager::flushRatio();
		timeFlushRatio = tick + Util::rand(3, 10)*60000;
	}
#endif
	LogManager::closeOldFiles(tick);
}

void MainFrame::fillToolbarButtons(CToolBarCtrl& toolbar, const string& setting, const ToolbarButton* buttons, int buttonCount)
{
	ctrlToolbar.SetButtonStructSize();
	SimpleStringTokenizer<char> t(setting, ',');
	string tok;
	while (t.getNextToken(tok))
	{
		const int i = Util::toInt(tok);
		if (i < buttonCount)
		{
				TBBUTTON tbb = {0};
				if (i < 0)
				{
					tbb.fsStyle = TBSTYLE_SEP;
				}
				else
				{
					if (buttons[i].id < 0) continue;
					tbb.iBitmap = i;
					tbb.idCommand = buttons[i].id;
					tbb.fsState = TBSTATE_ENABLED;
					tbb.fsStyle = buttons[i].check ? TBSTYLE_CHECK : TBSTYLE_BUTTON;
					tbb.iString = (INT_PTR)(CTSTRING_I(buttons[i].tooltip));
					dcassert(tbb.iString != -1);
					if (tbb.idCommand  == IDC_WINAMP_SPAM)
						tbb.fsStyle |= TBSTYLE_DROPDOWN;
				}
				toolbar.AddButtons(1, &tbb);
		}
	}
}

HWND MainFrame::createToolbar()
{
	if (!ctrlToolbar)
	{
		if (SETTING(TOOLBARIMAGE).empty())
		{
			ResourceLoader::LoadImageList(IDR_TOOLBAR, largeImages, 24, 24);
		}
		else
		{
			const int size = SETTING(TB_IMAGE_SIZE);
			ResourceLoader::LoadImageList(Text::toT(SETTING(TOOLBARIMAGE)).c_str(), largeImages, size, size);
		}
		if (SETTING(TOOLBARHOTIMAGE).empty())
		{
			ResourceLoader::LoadImageList(IDR_TOOLBAR_HL, largeImagesHot, 24, 24);
		}
		else
		{
			const int size = SETTING(TB_IMAGE_SIZE_HOT);
			ResourceLoader::LoadImageList(Text::toT(SETTING(TOOLBARHOTIMAGE)).c_str(), largeImagesHot, size, size);
		}
		ctrlToolbar.Create(m_hWnd, NULL, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR); // [~]Drakon. Fix with toolbar.
		ctrlToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);
		ctrlToolbar.SetImageList(largeImages);
		ctrlToolbar.SetHotImageList(largeImagesHot);
	}

	while (ctrlToolbar.GetButtonCount() > 0)
		ctrlToolbar.DeleteButton(0);

	fillToolbarButtons(ctrlToolbar, SETTING(TOOLBAR), g_ToolbarButtons, g_ToolbarButtonsCount);
	ctrlToolbar.AutoSize();
	if (ctrlRebar.IsWindow())   // resize of reband to fix position of chevron
	{
		const int nCount = ctrlRebar.GetBandCount();
		for (int i = 0; i < nCount; i++)
		{
			REBARBANDINFO rbBand = {0};
			rbBand.cbSize = sizeof(REBARBANDINFO);
			rbBand.fMask = RBBIM_IDEALSIZE | RBBIM_CHILD;
			ctrlRebar.GetBandInfo(i, &rbBand);
			if (rbBand.hwndChild == ctrlToolbar.m_hWnd)
			{
				RECT rect = { 0, 0, 0, 0 };
				ctrlToolbar.GetItemRect(ctrlToolbar.GetButtonCount() - 1, &rect);
				rbBand.cxIdeal = rect.right;
				ctrlRebar.SetBandInfo(i, &rbBand);
			}
		}
	}
	return ctrlToolbar.m_hWnd;
}

HWND MainFrame::createWinampToolbar()
{
	if (!ctrlWinampToolbar)
	{
		ResourceLoader::LoadImageList(IDR_PLAYERS_CONTROL, winampImages, 24, 24);
		ResourceLoader::LoadImageList(IDR_PLAYERS_CONTROL_HL, winampImagesHot, 24, 24);
		
		ctrlWinampToolbar.Create(m_hWnd, NULL, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
		ctrlWinampToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);
		ctrlWinampToolbar.SetImageList(winampImages);
		ctrlWinampToolbar.SetHotImageList(winampImagesHot);
		
		fillToolbarButtons(ctrlWinampToolbar, SETTING(WINAMPTOOLBAR), g_WinampToolbarButtons, g_WinampToolbarButtonsCount);
		ctrlWinampToolbar.AutoSize();
	}
	return ctrlWinampToolbar.m_hWnd;
}

HWND MainFrame::createQuickSearchBar()
{
	if (!ctrlQuickSearchBar)
	{
		static const int WIDTH = 200;
		ctrlQuickSearchBar.Create(m_hWnd, NULL, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 0, ATL_IDW_TOOLBAR);

		TBBUTTON tb = {0};
		
		tb.iBitmap = WIDTH;
		tb.fsStyle = TBSTYLE_SEP;
		
		ctrlQuickSearchBar.SetButtonStructSize();
		ctrlQuickSearchBar.AddButtons(1, &tb);
		ctrlQuickSearchBar.AutoSize();
		
		CRect rect;
		ctrlQuickSearchBar.GetItemRect(0, &rect);
		
		rect.bottom += 100;
		rect.left += 2;
		
		quickSearchBox.Create(ctrlQuickSearchBar.m_hWnd, rect, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                      WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL, 0);
		quickSearchBox.GetWindowRect(&rect);
		ctrlQuickSearchBar.SetButtonSize(WIDTH, rect.bottom - rect.top);

		updateQuickSearches();
		
		quickSearchBoxContainer.SubclassWindow(quickSearchBox.m_hWnd);
		quickSearchBox.SetExtendedUI();
		quickSearchBox.SetFont(Fonts::g_systemFont, FALSE);
		
		POINT pt;
		pt.x = 10;
		pt.y = 10;
		
		HWND hWnd = quickSearchBox.ChildWindowFromPoint(pt);
		if (hWnd != NULL && !quickSearchEdit.IsWindow() && hWnd != quickSearchBox.m_hWnd)
		{
			quickSearchEdit.Attach(hWnd);
			quickSearchEdit.SetCueBannerText(CTSTRING(QSEARCH_STR));
			quickSearchEditContainer.SubclassWindow(quickSearchEdit.m_hWnd);
		}
	}
	return ctrlQuickSearchBar.m_hWnd;
}

LRESULT MainFrame::onWinampButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (SETTING(MEDIA_PLAYER) == SettingsManager::WinAmp)
	{
		HWND hwndWinamp = FindWindow(_T("Winamp v1.x"), NULL);
		if (::IsWindow(hwndWinamp))
		{
			switch (wID)
			{
				case IDC_WINAMP_BACK:
					SendMessage(hwndWinamp, WM_COMMAND, WINAMP_BUTTON1, 0);
					break;
				case IDC_WINAMP_PLAY:
					SendMessage(hwndWinamp, WM_COMMAND, WINAMP_BUTTON2, 0);
					break;
				case IDC_WINAMP_STOP:
					SendMessage(hwndWinamp, WM_COMMAND, WINAMP_BUTTON4, 0);
					break;
				case IDC_WINAMP_PAUSE:
					SendMessage(hwndWinamp, WM_COMMAND, WINAMP_BUTTON3, 0);
					break;
				case IDC_WINAMP_NEXT:
					SendMessage(hwndWinamp, WM_COMMAND, WINAMP_BUTTON5, 0);
					break;
				case IDC_WINAMP_VOL_UP:
					SendMessage(hwndWinamp, WM_COMMAND, WINAMP_VOLUMEUP, 0);
					break;
				case IDC_WINAMP_VOL_DOWN:
					SendMessage(hwndWinamp, WM_COMMAND, WINAMP_VOLUMEDOWN, 0);
					break;
				case IDC_WINAMP_VOL_HALF:
					SendMessage(hwndWinamp, WM_WA_IPC, 255 / 2, IPC_SETVOLUME);
					break;
			}
		}
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::WinMediaPlayer)
	{
		HWND hwndWMP = FindWindow(_T("WMPlayerApp"), NULL);
		if (::IsWindow(hwndWMP))
		{
			switch (wID)
			{
				case IDC_WINAMP_BACK:
					SendMessage(hwndWMP, WM_COMMAND, WMP_PREV, 0);
					break;
				case IDC_WINAMP_PLAY:
				case IDC_WINAMP_PAUSE:
					SendMessage(hwndWMP, WM_COMMAND, WMP_PLAY, 0);
					break;
				case IDC_WINAMP_STOP:
					SendMessage(hwndWMP, WM_COMMAND, WMP_STOP, 0);
					break;
				case IDC_WINAMP_NEXT:
					SendMessage(hwndWMP, WM_COMMAND, WMP_NEXT, 0);
					break;
				case IDC_WINAMP_VOL_UP:
					SendMessage(hwndWMP, WM_COMMAND, WMP_VOLUP, 0);
					break;
				case IDC_WINAMP_VOL_DOWN:
					SendMessage(hwndWMP, WM_COMMAND, WMP_VOLDOWN, 0);
					break;
				case IDC_WINAMP_VOL_HALF:
					SendMessage(hwndWMP, WM_COMMAND, WMP_MUTE, 0);
					break;
			}
		}
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::iTunes)
	{
		// Since i couldn't find out the appropriate window messages, we doing this а la COM
		HWND hwndiTunes = FindWindow(_T("iTunes"), _T("iTunes"));
		if (::IsWindow(hwndiTunes))
		{
			IiTunes *iITunes;
			CoInitialize(NULL);
			if (SUCCEEDED(::CoCreateInstance(CLSID_iTunesApp, NULL, CLSCTX_LOCAL_SERVER, IID_IiTunes, (PVOID *)&iITunes)))
			{
				long currVol;
				switch (wID)
				{
					case IDC_WINAMP_BACK:
						iITunes->PreviousTrack();
						break;
					case IDC_WINAMP_PLAY:
						iITunes->Play();
						break;
					case IDC_WINAMP_STOP:
						iITunes->Stop();
						break;
					case IDC_WINAMP_PAUSE:
						iITunes->Pause();
						break;
					case IDC_WINAMP_NEXT:
						iITunes->NextTrack();
						break;
					case IDC_WINAMP_VOL_UP:
						iITunes->get_SoundVolume(&currVol);
						iITunes->put_SoundVolume(currVol + 10);
						break;
					case IDC_WINAMP_VOL_DOWN:
						iITunes->get_SoundVolume(&currVol);
						iITunes->put_SoundVolume(currVol - 10);
						break;
					case IDC_WINAMP_VOL_HALF:
						iITunes->put_SoundVolume(50);
						break;
				}
			}
			safe_release(iITunes);
			CoUninitialize();
		}
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::WinMediaPlayerClassic)
	{
		HWND hwndMPC = FindWindow(_T("MediaPlayerClassicW"), NULL);
		if (::IsWindow(hwndMPC))
		{
			switch (wID)
			{
				case IDC_WINAMP_BACK:
					SendMessage(hwndMPC, WM_COMMAND, MPC_PREV, 0);
					break;
				case IDC_WINAMP_PLAY:
					SendMessage(hwndMPC, WM_COMMAND, MPC_PLAY, 0);
					break;
				case IDC_WINAMP_STOP:
					SendMessage(hwndMPC, WM_COMMAND, MPC_STOP, 0);
					break;
				case IDC_WINAMP_PAUSE:
					SendMessage(hwndMPC, WM_COMMAND, MPC_PAUSE, 0);
					break;
				case IDC_WINAMP_NEXT:
					SendMessage(hwndMPC, WM_COMMAND, MPC_NEXT, 0);
					break;
				case IDC_WINAMP_VOL_UP:
					SendMessage(hwndMPC, WM_COMMAND, MPC_VOLUP, 0);
					break;
				case IDC_WINAMP_VOL_DOWN:
					SendMessage(hwndMPC, WM_COMMAND, MPC_VOLDOWN, 0);
					break;
				case IDC_WINAMP_VOL_HALF:
					SendMessage(hwndMPC, WM_COMMAND, MPC_MUTE, 0);
					break;
			}
		}
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::JetAudio)
	{
		if (jaControl.get() && jaControl.get()->isJARunning())
		{
			jaControl.get()->JAUpdateAllInfo();
			switch (wID)
			{
				case IDC_WINAMP_BACK:
					jaControl.get()->JAPrevTrack();
					break;
				case IDC_WINAMP_PLAY:
				{
					if (jaControl.get()->isJAPaused())
						jaControl.get()->JASetPause();
					else if (jaControl.get()->isJAStopped())
						jaControl.get()->JASetPlay(0);
				}
				break;
				case IDC_WINAMP_STOP:
					jaControl.get()->JASetStop();
					break;
				case IDC_WINAMP_PAUSE:
					jaControl.get()->JASetPause();
					break;
				case IDC_WINAMP_NEXT:
					jaControl.get()->JANextTrack();
					break;
				case IDC_WINAMP_VOL_UP:
					jaControl.get()->JAVolumeUp();
					break;
				case IDC_WINAMP_VOL_DOWN:
					jaControl.get()->JAVolumeDown();
					break;
				case IDC_WINAMP_VOL_HALF:
					jaControl.get()->JASetVolume(50);
					break;
			}
		}
	}
	return 0;
}

LRESULT MainFrame::onQuickSearchChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (uMsg == WM_CHAR)
		if (wParam == VK_BACK)
			disableAutoComplete = true;
		else
			disableAutoComplete = false;
			
	switch (wParam)
	{
		case VK_DELETE:
			if (uMsg == WM_KEYDOWN)
			{
				disableAutoComplete = true;
			}
			bHandled = FALSE;
			break;
		case VK_RETURN:
			if (WinUtil::isShift() || WinUtil::isCtrlOrAlt())
			{
				bHandled = FALSE;
			}
			else
			{
				if (uMsg == WM_KEYDOWN)
				{
					tstring s;
					WinUtil::getWindowText(quickSearchEdit, s);
					SearchFrame::openWindow(s);
				}
			}
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT MainFrame::onQuickSearchColor(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	const HDC hDC = (HDC)wParam;
	return Colors::setColor(hDC);
}

LRESULT MainFrame::onParentNotify(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT res = 0;
	bHandled = FALSE;
	
	if (LOWORD(wParam) == WM_LBUTTONDOWN)
	{
		POINT pt = {LOWORD(lParam), HIWORD(lParam)};
		RECT rect;
		ctrlHashProgress.GetWindowRect(&rect);
		ScreenToClient(&rect);
		if (PtInRect(&rect, pt))
		{
			PostMessage(WM_COMMAND, IDC_HASH_PROGRESS);
			bHandled = TRUE;
		}
	}
	
	return res;
}

LRESULT MainFrame::onQuickSearchEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	uint32_t nTextLen = 0;
	HWND hWndCombo = quickSearchBox.m_hWnd;
	DWORD dwStartSel = 0, dwEndSel = 0;
		
	// Get the text length from the combobox, then copy it into a newly allocated buffer.
	nTextLen = ::SendMessage(hWndCombo, WM_GETTEXTLENGTH, NULL, NULL);
	_TCHAR *pEnteredText = new _TCHAR[nTextLen + 1];
	::SendMessage(hWndCombo, WM_GETTEXT, (WPARAM)nTextLen + 1, (LPARAM)pEnteredText);
	::SendMessage(hWndCombo, CB_GETEDITSEL, (WPARAM)&dwStartSel, (LPARAM)&dwEndSel);
		
	// Check to make sure autocompletion isn't disabled due to a backspace or delete
	// Also, the user must be typing at the end of the string, not somewhere in the middle.
	if (!disableAutoComplete && dwStartSel == dwEndSel && dwStartSel == nTextLen)
	{
		// Try and find a string that matches the typed substring.  If one is found,
		// set the text of the combobox to that string and set the selection to mask off
		// the end of the matched string.
		int nMatch = ::SendMessage(hWndCombo, CB_FINDSTRING, (WPARAM) - 1, (LPARAM)pEnteredText);
		if (nMatch != CB_ERR)
		{
			uint32_t nMatchedTextLen = ::SendMessage(hWndCombo, CB_GETLBTEXTLEN, (WPARAM)nMatch, 0);
			if (nMatchedTextLen != CB_ERR)
			{
				// Since the user may be typing in the same string, but with different case (e.g. "/port --> /PORT")
				// we copy whatever the user has already typed into the beginning of the matched string,
				// then copy the whole shebang into the combobox.  We then set the selection to mask off
				// the inferred portion.
				_TCHAR * pStrMatchedText = new _TCHAR[nMatchedTextLen + 1];
				::SendMessage(hWndCombo, CB_GETLBTEXT, (WPARAM)nMatch, (LPARAM)pStrMatchedText);
				memcpy((void*)pStrMatchedText, (void*)pEnteredText, nTextLen * sizeof(_TCHAR));
				::SendMessage(hWndCombo, WM_SETTEXT, 0, (WPARAM)pStrMatchedText);
				::SendMessage(hWndCombo, CB_SETEDITSEL, 0, MAKELPARAM(nTextLen, -1));
				delete[] pStrMatchedText;
			}
		}
	}
	
	delete[] pEnteredText;
	bHandled = TRUE;
	return 0;
}

void MainFrame::updateQuickSearches(bool clear /*= false*/)
{
	quickSearchBox.ResetContent();
	if (!clear)
	{
		if (SearchFrame::g_lastSearches.empty())
			SearchFrame::loadSearchHistory();
		for (auto& str : SearchFrame::g_lastSearches)
			quickSearchBox.AddString(str.c_str());
	}
	if (BOOLSETTING(CLEAR_SEARCH))
		quickSearchBox.SetWindowText(_T(""));
}

LRESULT MainFrame::onListenerInit(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam)
	{
		ListenerError* error = reinterpret_cast<ListenerError*>(lParam);
		MessageBox(CTSTRING_F(LISTENING_SOCKET_ERROR, error->type % error->errorCode),
			getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
		delete error;
		ClientManager::stopStartup();
		autoConnectFlag = false;
		return 0;
	}
	if (!autoConnectFlag) return 0;
	autoConnectFlag = false;
	std::vector<FavoriteHubEntry> hubs;
	{
		FavoriteManager::LockInstanceHubs lock(FavoriteManager::getInstance(), false);		
		for (const FavoriteHubEntry* entry : lock.getFavoriteHubs())
			if (entry->getAutoConnect())
				hubs.push_back(FavoriteHubEntry(*entry));
	}
	autoConnect(hubs);
	if (BOOLSETTING(OPEN_DHT))
		HubFrame::openHubWindow(dht::NetworkName);
	return 0;
}

LRESULT MainFrame::onUpdateLayout(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	updateLayoutFlag = true;
	UpdateLayout();
	updateLayoutFlag = false;
	return 0;
}

LRESULT MainFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == MAIN_STATS)
	{
		processingStats = false;
		std::unique_ptr<TStringList> pstr(reinterpret_cast<TStringList*>(lParam));
		if (ClientManager::isBeforeShutdown() || ClientManager::isStartup())
		{
			return 0;
		}
		
		TStringList& str = *pstr;
		if (ctrlStatus.IsWindow())
		{
			bool update = false;
			HashManager::Info info;
			auto hm = HashManager::getInstance();
			hm->getInfo(info);
			if (info.filesLeft)
			{
				int progressValue;
				if (info.sizeHashed >= info.sizeToHash)
					progressValue = HashProgressDlg::MAX_PROGRESS_VALUE;
				else
					progressValue = (info.sizeHashed * HashProgressDlg::MAX_PROGRESS_VALUE) / info.sizeToHash;
#ifdef FLYLINKDC_SUPPORT_WIN_XP
				if (CompatibilityManager::isOsVistaPlus())
#endif
					ctrlHashProgress.SetState(hm->getHashSpeed() < 0 ? PBST_PAUSED : PBST_NORMAL);
				ctrlHashProgress.SetPos(progressValue);
				if (!hashProgressVisible)
				{
					ctrlHashProgress.ShowWindow(SW_SHOW);
					update = true;
					hashProgressVisible = true;
				}
			}
			else if (hashProgressVisible)
			{
				ctrlHashProgress.ShowWindow(SW_HIDE);
				update = true;
				hashProgressVisible = false;
				ctrlHashProgress.SetPos(0);
			}
			
			if (statusText[0] != str[0])
			{
				statusText[0] = std::move(str[0]);
				ctrlStatus.SetText(1, statusText[0].c_str());
			}
			const size_t count = str.size();
			dcassert(count < STATUS_PART_LAST);
			for (size_t i = 1; i < count; i++)
			{
				if (statusText[i] != str[i])
				{
					statusText[i] = std::move(str[i]);
					int w = WinUtil::getTextWidth(statusText[i], ctrlStatus) + STATUS_PART_PADDING;
					if (i < STATUS_PART_LAST && statusSizes[i] < w)
					{
						statusSizes[i] = w;
						update = true;
					}
					ctrlStatus.SetText(i + 1, statusText[i].c_str());
				}
			}
			
			if (isShutDown())
			{
				const uint64_t second = GET_TICK() / 1000;
				if (!shutdownStatusDisplayed)
				{
					HICON shutdownIcon = g_iconBitmaps.getIcon(IconBitmaps::SHUTDOWN, 0);
					ctrlStatus.SetIcon(STATUS_PART_SHUTDOWN_TIME, shutdownIcon);
					shutdownStatusDisplayed = true;
				}
				if (DownloadManager::getDownloadCount() > 0)
				{
					shutdownTime = second;
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
				}
				else
				{
					const int timeout = SETTING(SHUTDOWN_TIMEOUT);
					const int64_t timeLeft = timeout - (second - shutdownTime);
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, (_T(' ') + Util::formatSecondsT(timeLeft, timeLeft < 3600)).c_str(), SBT_POPOUT);
					if (shutdownTime + timeout <= second)
					{
						// We better not try again. It WON'T work...
						shutdownEnabled = false;
						
						bool shutdownResult = WinUtil::shutDown(SETTING(SHUTDOWN_ACTION));
						if (shutdownResult)
						{
							// Should we go faster here and force termination?
							// We "could" do a manual shutdown of this app...
						}
						else
						{
							ctrlStatus.SetText(STATUS_PART_MESSAGE, CTSTRING(FAILED_TO_SHUTDOWN));
							ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
						}
					}
				}
			}
			else
			{
				if (shutdownStatusDisplayed)
				{
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
					ctrlStatus.SetIcon(STATUS_PART_SHUTDOWN_TIME, NULL);
					shutdownStatusDisplayed = false;
				}
			}
			
			if (update)
				updateStatusBar++;
		}
	}
	else if (wParam == STATUS_MESSAGE)
	{
		LogManager::g_isLogSpeakerEnabled = true;
		char* msg = reinterpret_cast<char*>(lParam);
		if (!ClientManager::isShutdown() && !closing && ctrlStatus.IsWindow())
		{
			string msgStr = Util::formatDateTime("[%H:%M:%S] ", GET_TIME());
			msgStr.append(msg);
			const tstring line = Text::toT(msgStr);
			ctrlStatus.SetText(STATUS_PART_MESSAGE, line.c_str());
			
			const tstring::size_type rpos = line.find(_T('\r'));
			if (rpos == tstring::npos)
				lastLinesList.push_back(line);
			else
				lastLinesList.push_back(line.substr(0, rpos));

			while (lastLinesList.size() > MAX_CLIENT_LINES)
				lastLinesList.erase(lastLinesList.begin());
		}
		delete[] msg;
	}
	else if (wParam == DOWNLOAD_LISTING)
	{
		std::unique_ptr<QueueManager::DirectoryListInfo> i(reinterpret_cast<QueueManager::DirectoryListInfo*>(lParam));
		if (!ClientManager::isBeforeShutdown())
		{
			DirectoryListingFrame::openWindow(Text::toT(i->file), Text::toT(i->dir), i->hintedUser, i->speed, i->isDclst);
		}
	}
	else if (wParam == BROWSE_LISTING)
	{
		std::unique_ptr<DirectoryBrowseInfo> i(reinterpret_cast<DirectoryBrowseInfo*>(lParam));
		if (!ClientManager::isBeforeShutdown())
		{
			DirectoryListingFrame::openWindow(i->hintedUser, i->text, 0);
		}
	}
	else if (wParam == VIEW_FILE_AND_DELETE)
	{
		std::unique_ptr<tstring> file(reinterpret_cast<tstring*>(lParam));
		if (!ClientManager::isBeforeShutdown())
		{
			ShellExecute(NULL, NULL, file->c_str(), NULL, NULL, SW_SHOW); // FIXME
		}
	}
	else if (wParam == PROCESS_COMMAND_LINE)
	{
		processCommandLine(cmdLine);
	}
	else if (wParam == SHOW_POPUP_MESSAGE)
	{
		Popup* msg = (Popup*)lParam;
		if (!ClientManager::isBeforeShutdown())
		{
			dcassert(PopupManager::isValidInstance());
			if (PopupManager::isValidInstance())
			{
				PopupManager::getInstance()->Show(msg->message, msg->title, msg->icon);
			}
		}
		delete msg;
	}
	else if (wParam == REMOVE_POPUP)
	{
		dcassert(PopupManager::isValidInstance());
		if (PopupManager::isValidInstance())
		{
			PopupManager::getInstance()->AutoRemove();
		}
	}
	else if (wParam == SET_PM_TRAY_ICON)
	{
		if (!ClientManager::isBeforeShutdown() && !hasPM && (!WinUtil::g_isAppActive || appMinimized))
		{
			hasPM = true;
			if (taskbarList)
				taskbarList->SetOverlayIcon(m_hWnd, pmIcon, nullptr);
			setTrayIcon(useTrayIcon ? TRAY_ICON_PM : TRAY_ICON_NONE);
		}
	}
	else if (wParam == WM_CLOSE)
	{
		dcassert(PopupManager::isValidInstance());
		if (PopupManager::isValidInstance())
		{
			PopupManager::getInstance()->Remove((int)lParam);
		}
	}
	else
	{
		dcassert(0);
	}
	return 0;
}

void MainFrame::processCommandLine(const ParsedCommandLine& cmd)
{
	if (!cmd.openMagnet.empty())
		WinUtil::parseMagnetUri(cmd.openMagnet);
	if (!cmd.openHub.empty())
		WinUtil::parseDchubUrl(cmd.openHub);
	if (!cmd.openFile.empty() && File::isExist(cmd.openFile))
		WinUtil::openFileList(cmd.openFile);
	if (!cmd.shareFolder.empty() && File::isExist(cmd.shareFolder))
		shareFolderFromShell(cmd.shareFolder);
}

LRESULT MainFrame::onCopyData(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	const COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
	if (jaControl.get()->ProcessCopyData(cds))
		return TRUE;
	
	if (!getPassword())
		return FALSE;

	if (IsIconic())
		ShowWindow(SW_RESTORE);

	wstring commandLine = Util::getModuleFileName();
	commandLine += L' ';
	commandLine += static_cast<WCHAR*>(cds->lpData);
	ParsedCommandLine pcl;
	if (parseCommandLine(pcl, commandLine.c_str()))
		processCommandLine(pcl);
	return TRUE;
}

LRESULT MainFrame::onHashProgress(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HashProgressDlg dlg(false, false, g_iconBitmaps.getIcon(IconBitmaps::HASH_PROGRESS, 0));
	dlg.DoModal(m_hWnd);
	return 0;
}

LRESULT MainFrame::onAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AboutDlgIndex dlg;
	dlg.DoModal(m_hWnd);
	return 0;
}

LRESULT MainFrame::onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case ID_FILE_SEARCH:
			SearchFrame::openWindow();
			break;
		case IDC_PUBLIC_HUBS:
			PublicHubsFrame::toggleWindow();
			break;
		case IDC_FAVORITES:
			FavoriteHubsFrame::toggleWindow();
			break;
		case IDC_FAVUSERS:
			UsersFrame::toggleWindow();
			break;
		case IDC_NOTEPAD:
			NotepadFrame::toggleWindow();
			break;
		case IDC_QUEUE:
			QueueFrame::toggleWindow();
			break;
		case IDC_SEARCH_SPY:
			SpyFrame::toggleWindow();
			break;
		case IDC_FILE_ADL_SEARCH:
			ADLSearchFrame::toggleWindow();
			break;
#ifdef FLYLINKDC_USE_STATS_FRAME
		case IDC_NET_STATS:
			StatsFrame::toggleWindow();
			break;
#endif
		case IDC_FINISHED:
			FinishedDLFrame::toggleWindow();
			break;
		case IDC_FINISHED_UL:
			FinishedULFrame::toggleWindow();
			break;
		case IDC_UPLOAD_QUEUE:
			WaitingUsersFrame::toggleWindow();
			break;
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		case IDC_CDMDEBUG_WINDOW:
			CDMDebugFrame::toggleWindow();
			break;
#endif
		case IDC_RECENTS:
			RecentHubsFrame::toggleWindow();
			break;
		default:
			dcassert(0);
			break;
	}
	return 0;
}

LRESULT MainFrame::onToggleDHT(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HubFrame* frame = HubFrame::findHubWindow(dht::NetworkName);
	if (frame)
		frame->PostMessage(WM_CLOSE);
	else
		HubFrame::openHubWindow(dht::NetworkName);
	return 0;
}

LRESULT MainFrame::onSettings(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!PropertiesDlg::g_is_create)
	{
		PropertiesDlg dlg(m_hWnd, g_iconBitmaps.getIcon(IconBitmaps::SETTINGS, 0));
		
		NetworkPage::Settings prevNetworkSettings;
		prevNetworkSettings.get();

		bool prevSortFavUsersFirst = BOOLSETTING(SORT_FAVUSERS_FIRST);
		bool prevRegisterURLHandler = BOOLSETTING(REGISTER_URL_HANDLER);
		bool prevRegisterMagnetHandler = BOOLSETTING(REGISTER_MAGNET_HANDLER);
		bool prevRegisterDCLSTHandler = BOOLSETTING(REGISTER_DCLST_HANDLER);
		bool prevDHT = BOOLSETTING(USE_DHT);
		bool prevHubUrlInTitle = BOOLSETTING(HUB_URL_IN_TITLE);
		string prevDownloadDir = SETTING(TEMP_DOWNLOAD_DIRECTORY);
		
		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			SettingsManager::getInstance()->save();
			transferView.setButtonState();

			NetworkPage::Settings currentNetworkSettings;
			currentNetworkSettings.get();
			if (ConnectionManager::getInstance()->getPort() == 0 || !currentNetworkSettings.compare(prevNetworkSettings))
				ConnectivityManager::getInstance()->setupConnections();

			bool useDHT = BOOLSETTING(USE_DHT);
			if (useDHT != prevDHT)
			{
				dht::DHT* d = dht::DHT::getInstance();
				if (useDHT)
					d->start();
				else
					d->stop();
			}

			if (BOOLSETTING(REGISTER_URL_HANDLER) != prevRegisterURLHandler)
			{
				if (BOOLSETTING(REGISTER_URL_HANDLER))
					WinUtil::registerHubUrlHandlers();
				else if (WinUtil::hubUrlHandlersRegistered)
					WinUtil::unregisterHubUrlHandlers();
			}

			if (BOOLSETTING(REGISTER_MAGNET_HANDLER) != prevRegisterMagnetHandler)
			{
				if (BOOLSETTING(REGISTER_MAGNET_HANDLER))
					WinUtil::registerMagnetHandler();
				else if (WinUtil::magnetHandlerRegistered)
					WinUtil::unregisterMagnetHandler();
			}

			if (BOOLSETTING(REGISTER_DCLST_HANDLER) != prevRegisterDCLSTHandler)
			{
				if (BOOLSETTING(REGISTER_DCLST_HANDLER))
					WinUtil::registerDclstHandler();
				else if (WinUtil::dclstHandlerRegistered)
					WinUtil::unregisterDclstHandler();
			}

			if (BOOLSETTING(SORT_FAVUSERS_FIRST) != prevSortFavUsersFirst)
				HubFrame::resortUsers();

			if (BOOLSETTING(HUB_URL_IN_TITLE) != prevHubUrlInTitle)
				HubFrame::updateAllTitles();

			if (SETTING(TEMP_DOWNLOAD_DIRECTORY) != prevDownloadDir)
				QueueItem::checkTempDir = true;

			MainFrame::setLimiterButton(BOOLSETTING(THROTTLE_ENABLE));

			ctrlToolbar.CheckButton(IDC_DISABLE_SOUNDS, BOOLSETTING(SOUNDS_DISABLED));
			ctrlToolbar.CheckButton(IDC_DISABLE_POPUPS, BOOLSETTING(POPUPS_DISABLED));
			ctrlToolbar.CheckButton(IDC_AWAY, Util::getAway());
			ctrlToolbar.CheckButton(IDC_SHUTDOWN, isShutDown());

			bool needUpdateLayout = ctrlTab.getTabsPosition() != SETTING(TABS_POS);
			bool needInvalidateTabs = ctrlTab.setOptions(SETTING(TABS_POS), SETTING(MAX_TAB_ROWS), SETTING(TAB_SIZE), true, BOOLSETTING(TABS_CLOSEBUTTONS), BOOLSETTING(TABS_BOLD), BOOLSETTING(NON_HUBS_FRONT), false);

			if (needUpdateLayout)
			{
				UpdateLayout();
				needInvalidateTabs = true;
			}

			if (needInvalidateTabs)
				ctrlTab.Invalidate();
			
			if (!BOOLSETTING(SHOW_CURRENT_SPEED_IN_TITLE))
				SetWindowText(getAppNameVerT().c_str());
			
			ShareManager::getInstance()->refreshShareIfChanged();
			ClientManager::infoUpdated(true);
		}
		else
		{
			transferView.setButtonState();
		}
	}
	return 0;
}

// FIXME
LRESULT MainFrame::onWebServerSocket(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WebServerManager::getInstance()->getServerSocket().incoming();
	return 0;
}

LRESULT MainFrame::onTooltipPop(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	ctrlTab.processTooltipPop(pnmh->hwndFrom);
	return 0;
}

LRESULT MainFrame::onGetToolTip(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMTTDISPINFO pDispInfo = (LPNMTTDISPINFO)pnmh;
	pDispInfo->szText[0] = 0;
	
	if (idCtrl != 0 && !(pDispInfo->uFlags & TTF_IDISHWND))
	{
		int stringId = -1;
		switch (idCtrl)
		{
			case IDC_WINAMP_BACK:
				stringId = ResourceManager::WINAMP_BACK;
				break;
			case IDC_WINAMP_PLAY:
				stringId = ResourceManager::WINAMP_PLAY;
				break;
			case IDC_WINAMP_PAUSE:
				stringId = ResourceManager::WINAMP_PAUSE;
				break;
			case IDC_WINAMP_NEXT:
				stringId = ResourceManager::WINAMP_NEXT;
				break;
			case IDC_WINAMP_STOP:
				stringId = ResourceManager::WINAMP_STOP;
				break;
			case IDC_WINAMP_VOL_UP:
				stringId = ResourceManager::WINAMP_VOL_UP;
				break;
			case IDC_WINAMP_VOL_HALF:
				stringId = ResourceManager::WINAMP_VOL_HALF;
				break;
			case IDC_WINAMP_VOL_DOWN:
				stringId = ResourceManager::WINAMP_VOL_DOWN;
				break;
			case IDC_WINAMP_SPAM:
				stringId = ResourceManager::WINAMP_SPAM;
				break;
		}
		
		for (int i = 0; g_ToolbarButtons[i].id; ++i)
		{
			if (g_ToolbarButtons[i].id == idCtrl)
			{
				stringId = g_ToolbarButtons[i].tooltip;
				break;
			}
		}
		if (stringId != -1)
		{
			_tcsncpy(pDispInfo->lpszText, CTSTRING_I((ResourceManager::Strings)stringId), 79);
			pDispInfo->uFlags |= TTF_DI_SETITEM;
		}
	}
	else   // if we're really in the status bar, this should be detected intelligently
	{
		lastLines.clear();
		for (size_t i = 0; i < lastLinesList.size(); ++i)
		{
			lastLines += lastLinesList[i];
			if (i != lastLinesList.size()-1)
				lastLines += _T("\r\n");
		}
		pDispInfo->lpszText = const_cast<TCHAR*>(lastLines.c_str());
	}
	return 0;
}

void MainFrame::autoConnect(const std::vector<FavoriteHubEntry>& hubs)
{
	CFlyLockWindowUpdate l(WinUtil::g_mdiClient);
	HubFrame* lastFrame = nullptr;
	{
		for (const FavoriteHubEntry& entry : hubs)
		{
			if (!entry.getNick().empty())
			{
				RecentHubEntry r;
				r.setName(entry.getName());
				r.setDescription(entry.getDescription());
				r.setServer(entry.getServer());
				RecentHubEntry* recent = FavoriteManager::getInstance()->addRecent(r);
				if (recent)
					recent->setAutoOpen(true);
				lastFrame = HubFrame::openHubWindow(entry.getServer(),
				                                    entry.getName(),
				                                    entry.getRawCommands(),
				                                    entry.getWindowPosX(),
				                                    entry.getWindowPosY(),
				                                    entry.getWindowSizeX(),
				                                    entry.getWindowSizeY(),
				                                    entry.getWindowType(),
				                                    entry.getChatUserSplit(),
				                                    entry.getHideUserList(),
				                                    entry.getSuppressChatAndPM());
			}
		}
		if (BOOLSETTING(OPEN_RECENT_HUBS))
		{
			const auto& recents = FavoriteManager::getRecentHubs();
			for (const RecentHubEntry* recent : recents)
			{
				if (!recent->getAutoOpen() && recent->getOpenTab() == "+")
					lastFrame = HubFrame::openHubWindow(recent->getServer(), recent->getName());
			}
		}
	}
	ClientManager::stopStartup();
	if (lastFrame)
		lastFrame->createMessagePanel();
}

void MainFrame::setTrayIcon(int newIcon)
{
	if (trayIcon == newIcon) return;
	if (trayIcon == TRAY_ICON_NONE)
	{
		NOTIFYICONDATA nid = {};
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = m_hWnd;
		nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
		nid.uCallbackMessage = WMU_TRAY_ICON;
		nid.hIcon = newIcon == TRAY_ICON_PM ? pmIcon : mainIcon;
		_tcsncpy(nid.szTip, getAppNameT().c_str(), 64);
		nid.szTip[63] = '\0';
		lastTickMouseMove = GET_TICK() - 1000;
		Shell_NotifyIcon(NIM_ADD, &nid);
	}
	else if (newIcon == TRAY_ICON_NONE)
	{
		NOTIFYICONDATA nid = {};
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = m_hWnd;
		Shell_NotifyIcon(NIM_DELETE, &nid);
	}
	else
	{
		NOTIFYICONDATA nid = {};
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = m_hWnd;
		nid.uFlags = NIF_ICON;
		nid.hIcon = newIcon == TRAY_ICON_PM ? pmIcon : mainIcon;
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
	trayIcon = newIcon;
}

void MainFrame::clearPMStatus()
{
	if (hasPM)
	{
		hasPM = false;
		if (taskbarList)
			taskbarList->SetOverlayIcon(m_hWnd, nullptr, nullptr);
		setTrayIcon(useTrayIcon ? TRAY_ICON_NORMAL : TRAY_ICON_NONE);
	}
}

// FIXME
void setAwayByMinimized()
{
	static bool g_awayByMinimized = false;
	
	auto invertAwaySettingIfNeeded = [&]()
	{
		const auto curInvertedAway = !Util::getAway();
		if (g_awayByMinimized != curInvertedAway)
		{
			g_awayByMinimized = curInvertedAway;
			Util::setAway(curInvertedAway);
			MainFrame::setAwayButton(curInvertedAway);
		}
	};
	
	if (g_awayByMinimized)
	{
		invertAwaySettingIfNeeded();
	}
	else if (MainFrame::isAppMinimized() && BOOLSETTING(AUTO_AWAY))
	{
		invertAwaySettingIfNeeded();
	}
}

LRESULT MainFrame::onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam == SIZE_MINIMIZED)
	{
		if (!appMinimized)
		{
			appMinimized = true;
			if (BOOLSETTING(MINIMIZE_TRAY) != WinUtil::isShift())
			{
				ShowWindow(SW_HIDE);
				if (BOOLSETTING(REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY))
					CompatibilityManager::reduceProcessPriority();
			}
			setAwayByMinimized();
		}
		wasMaximized = IsZoomed() > 0;
	}
	else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
	{
		if (appMinimized)
		{
			appMinimized = false;
			setAwayByMinimized();
			CompatibilityManager::restoreProcessPriority();
			clearPMStatus();
		}
#if 0
		if (wParam == SIZE_RESTORED)
		{
			if (SETTING(MAIN_WINDOW_STATE) == SW_SHOWMAXIMIZED)
			{
				//SET_SETTING(MAIN_WINDOW_STATE, SW_SHOWMAXIMIZED);
				//ShowWindow(SW_SHOWMAXIMIZED);
			}
		}
#endif
	}
	bHandled = FALSE;
	return 0;
}

void MainFrame::storeWindowsPos()
{
	WINDOWPLACEMENT wp = { 0 };
	wp.length = sizeof(wp);
	if (GetWindowPlacement(&wp))
	{
		// Состояние окна отдельно от координат! Там мы могли не попадать в условие, при MAXIMIZED
		if (wp.showCmd == SW_SHOWNORMAL || wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWMAXIMIZED || wp.showCmd == SW_SHOWMINIMIZED)
		{
			SET_SETTING(MAIN_WINDOW_STATE, (int)wp.showCmd);
		}
		else
		{
			dcassert(0);
		}
		// Координаты окна
		CRect rc;
		// СОХРАНИМ координаты, только если окно в нормальном состоянии!!! Иначе - пусть будут последние
		if ((wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWNORMAL) && GetWindowRect(rc))
		{
			SET_SETTING(MAIN_WINDOW_POS_X, rc.left);
			SET_SETTING(MAIN_WINDOW_POS_Y, rc.top);
			SET_SETTING(MAIN_WINDOW_SIZE_X, rc.Width());
			SET_SETTING(MAIN_WINDOW_SIZE_Y, rc.Height());
		}
	}
#ifdef _DEBUG
	else
	{
		dcdebug("MainFrame:: WINDOW  GetWindowPlacement(&wp) -> NULL data !!!\n");
	}
#endif
}

#ifdef SCALOLAZ_MANY_MONITORS
LRESULT MainFrame::onSetDefaultPosition(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/)
{
	CRect rc;
	GetWindowRect(rc);
	rc.left = 0;
	rc.top = 0;
	rc.right = rc.left + SETTING(MAIN_WINDOW_SIZE_X);
	rc.bottom = rc.top + SETTING(MAIN_WINDOW_SIZE_Y);
	if ((rc.right - rc.left) < 600 || (rc.bottom - rc.top) < 400)
	{
		rc.right = rc.left + 600;
		rc.bottom = rc.top + 400;
	}
	MoveWindow(rc);
	CenterWindow(GetParent());
	storeWindowsPos();       // Хз как лучше - сразу сохранить новые значения, или ждём закрытия программы и там сохраним как обычно.
	return 0;
}
#endif

LRESULT MainFrame::onEndSession(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	endSession = true;
	FavoriteManager::getInstance()->saveFavorites();
	//SettingsManager::getInstance()->save();
	return 0;
}

LRESULT MainFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (BOOLSETTING(MINIMIZE_ON_CLOSE) && !quitFromMenu)
	{
		ShowWindow(SW_MINIMIZE);
	}
	else
	{
		if (!closing)
		{
#ifdef _DEBUG
			dcdebug("MainFrame::OnClose first - User::g_user_counts = %d\n", int(User::g_user_counts)); // [!] IRainman fix: Issue 1037 иногда теряем объект User?
#endif
			bool dontQuit = false;
			if (!endSession && hasPasswordClose())
			{
				INT_PTR result;
				if (!getPasswordInternal(result, m_hWnd))
				{
					dontQuit = true;
					quitFromMenu = false;
				}
				else
					dontQuit = result != IDOK;
			}
			
			bool bForceNoWarning = false;
			
			if (!dontQuit)
			{
				// [+] brain-ripper
				// check if hashing pending,
				// and display hashing progress
				// if any
				
				if (HashManager::getInstance()->isHashing())
				{
					bool bForceStopExit = false;
					if (!HashProgressDlg::instanceCounter)
					{
						HashProgressDlg dlg(true, true, g_iconBitmaps.getIcon(IconBitmaps::SETTINGS, 0));
						bForceStopExit = dlg.DoModal() != IDC_BTN_EXIT_ON_DONE;
					}
					
					// User take decision in dialog,
					//so avoid one more warning message
					bForceNoWarning = true;
					
					if (HashManager::getInstance()->isHashing() || bForceStopExit)
					{
						// User closed progress window, while hashing still in progress,
						// or user unchecked "exit on done" checkbox,
						// so let program to work
						
						dontQuit = true;
						quitFromMenu = false;
					}
				}
			}
			
			UINT checkState = BOOLSETTING(CONFIRM_EXIT) ? BST_CHECKED : BST_UNCHECKED;
			
			if ((endSession ||
			     SETTING(PROTECT_CLOSE) ||
			     checkState == BST_UNCHECKED ||
			     (bForceNoWarning ||
			     MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_EXIT), getAppNameVerT().c_str(),
			                         CTSTRING(ALWAYS_ASK), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES))
			     && !dontQuit)
			{
				storeWindowsPos();
					
				ClientManager::before_shutdown();
				LogManager::g_mainWnd = nullptr;
				closing = true;
				destroyTimer();
				ClientManager::stopStartup();
				NmdcHub::log_all_unknown_command();
				preparingCoreToShutdown();
					
				transferView.prepareClose();
				//dcassert(TransferView::ItemInfo::g_count_transfer_item == 0);
					
				WebServerManager::getInstance()->removeListener(this);
				FinishedManager::getInstance()->removeListener(this);
				UserManager::getInstance()->removeListener(this);
				QueueManager::getInstance()->removeListener(this);

				ConnectionManager::getInstance()->disconnect();
					
				ToolbarManager::getInstance()->getFrom(m_hWndToolBar, "MainToolBar");
					
				useTrayIcon = false;
				setTrayIcon(TRAY_ICON_NONE);
				if (m_nProportionalPos > 300)
					SET_SETTING(TRANSFER_FRAME_SPLIT, m_nProportionalPos);
				ShowWindow(SW_HIDE);
				stopperThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, &stopper, this, 0, nullptr));
			}
			else
			{
				quitFromMenu = false;
			}
			SET_SETTING(CONFIRM_EXIT, checkState != BST_UNCHECKED);
			bHandled = TRUE;
		}
		else
		{
#ifdef _DEBUG
			dcdebug("MainFrame::OnClose second - User::g_user_counts = %d\n", int(User::g_user_counts));
#endif
			// This should end immediately, as it only should be the stopper that sends another WM_CLOSE
			WaitForSingleObject(stopperThread, 60 * 1000);
			CloseHandle(stopperThread);
			stopperThread = nullptr;
			bHandled = FALSE;
#ifdef _DEBUG
			dcdebug("MainFrame::OnClose third - User::g_user_counts = %d\n", int(User::g_user_counts));
#endif
		}
	}
	return 0;
}

LRESULT MainFrame::onGetTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
	if (WinUtil::browseFile(file, m_hWnd, false, lastTTHdir))
	{
		FileHashDlg dlg(g_iconBitmaps.getIcon(IconBitmaps::TTH, 0));
		dlg.filename = std::move(file);
		dlg.lastDir = lastTTHdir;
		dlg.DoModal();
		lastTTHdir = std::move(dlg.lastDir);
	}
	return 0;
}

void MainFrame::UpdateLayout(BOOL resizeBars /* = TRUE */)
{
	if (!ClientManager::isStartup() || updateLayoutFlag)
	{
		RECT rect;
		GetClientRect(&rect);
		// position bars and offset their dimensions
		UpdateBarsPosition(rect, resizeBars);
		
		if (ctrlStatus.IsWindow() && ctrlLastLines.IsWindow())
		{
			CRect sr;
			int w[STATUS_PART_LAST];
			
			bool isHashing = HashManager::getInstance()->isHashing();
			ctrlStatus.GetClientRect(sr);
			if (isHashing)
			{
				w[STATUS_PART_HASH_PROGRESS] = sr.right - 20;
				w[STATUS_PART_SHUTDOWN_TIME] = w[STATUS_PART_HASH_PROGRESS] - 60;
			}
			else
				w[STATUS_PART_SHUTDOWN_TIME] = sr.right - 20;
				
			w[STATUS_PART_8] = w[STATUS_PART_SHUTDOWN_TIME] - 60;
#define setw(x) w[x] = max(w[x+1] - statusSizes[x], 0)
			setw(STATUS_PART_7);
			setw(STATUS_PART_UPLOAD);
			setw(STATUS_PART_DOWNLOAD);
			setw(STATUS_PART_SLOTS);
			setw(STATUS_PART_3);
			setw(STATUS_PART_SHARED_SIZE);
			setw(STATUS_PART_1);
			setw(STATUS_PART_MESSAGE);
			
			ctrlStatus.SetParts(STATUS_PART_LAST - 1 + (isHashing ? 1 : 0), w);
			ctrlLastLines.SetMaxTipWidth(max(w[4], 400));
			
			if (isHashing)
			{
				RECT rect;
				ctrlStatus.GetRect(STATUS_PART_HASH_PROGRESS, &rect);
				
				rect.right = w[STATUS_PART_HASH_PROGRESS] - 1;
				ctrlHashProgress.MoveWindow(&rect);
			}
			
			//tabDHTRect.right -= 2;
			ctrlStatus.GetRect(STATUS_PART_1, &tabAwayRect);
			
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
			ctrlStatus.GetRect(STATUS_PART_7, &tabDownSpeedRect);
			ctrlStatus.GetRect(STATUS_PART_8, &tabUpSpeedRect);
#endif
		}
		CRect rc  = rect;
		CRect rc2 = rect;
		
		switch (ctrlTab.getTabsPosition())
		{
			case SettingsManager::TABS_TOP:
				rc.bottom = rc.top + ctrlTab.getHeight();
				rc2.top = rc.bottom;
				break;
			default:
				rc.top = rc.bottom - ctrlTab.getHeight();
				rc2.bottom = rc.top;
				break;
		}
		
		if (ctrlTab.IsWindow())
			ctrlTab.MoveWindow(rc);
		SetSplitterRect(rc2);
	}
}

LRESULT MainFrame::onOpenFileList(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
	static tstring g_last_torrent_file;
#ifdef FLYLINKDC_USE_TORRENT
	if (wID == IDC_OPEN_TORRENT_FILE)
	{
		if (WinUtil::browseFile(file, m_hWnd, false, g_last_torrent_file, L"All torrent file\0*.torrent\0\0"))
		{
			g_last_torrent_file = Util::getFilePath(file);
			DownloadManager::getInstance()->add_torrent_file(file, _T(""));
		}
		return 0;
	}
#endif
	if (wID == IDC_OPEN_MY_LIST)
	{
		const auto myUser = std::make_shared<User>(ClientManager::getMyCID(), SETTING(NICK));
		myUser->setFlag(User::MYSELF);
		DirectoryListingFrame::openWindow(Text::toT(ShareManager::getInstance()->getBZXmlFile(CID())),
			Util::emptyStringT, HintedUser(myUser, Util::emptyString), 0);
		return 0;
	}
	
	if (WinUtil::browseFile(file, m_hWnd, false, Text::toT(Util::getListPath()), g_file_list_type))//FILE_LIST_TYPE.c_str()))
	{
		if (Util::isTorrentFile(file))
		{
			g_last_torrent_file = Util::getFilePath(file);
#ifdef FLYLINKDC_USE_TORRENT
			DownloadManager::getInstance()->add_torrent_file(file, _T(""));
#endif
		}
		else
		{
			WinUtil::openFileList(file);
		}
	}
	return 0;
}

LRESULT MainFrame::onRefreshFileListPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	return 0;
}

LRESULT MainFrame::onRefreshFileList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ShareManager::getInstance()->refreshShare();
	return 0;
}

LRESULT MainFrame::onLineDlgCreated(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	passwordDlg = (HWND) lParam;
	return 0;
}

bool MainFrame::getPasswordInternal(INT_PTR& result, HWND hwndParent)
{
	if (passwordDlg)
	{
		SetForegroundWindow(passwordDlg);
		return false;
	}

	LineDlg dlg;
	dlg.description = TSTRING(PASSWORD_DESC);
	dlg.title = TSTRING(PASSWORD_TITLE);
	dlg.password = true;
	dlg.disabled = true;
	dlg.notifyMainFrame = true;
	result = dlg.DoModal(hwndParent);
	passwordDlg = nullptr;
	if (result == IDOK)
	{
		string tmp = Text::fromT(dlg.line);
		TigerTree hash(1024);
		hash.update(tmp.c_str(), tmp.size());
		hash.finalize();
		return hash.getRoot().toBase32() == SETTING(PASSWORD);
	}
	return false;
}

bool MainFrame::getPassword()
{
	if (IsWindowVisible() || !hasPasswordTray()) return true;
	INT_PTR result;
	return getPasswordInternal(result, m_hWnd);
}

LRESULT MainFrame::onTrayIcon(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (lParam == WM_LBUTTONUP)
	{
		if (appMinimized)
		{
			if (getPassword())
				showWindow();
		}
		else
			ShowWindow(SW_MINIMIZE);
	}
	else if (lParam == WM_MOUSEMOVE)
	{
		uint64_t tick = GET_TICK();
		if (lastTickMouseMove + 1000 < tick)
		{
			NOTIFYICONDATA nid = {0};
			nid.cbSize = sizeof(NOTIFYICONDATA);
			nid.hWnd = m_hWnd;
			nid.uFlags = NIF_TIP;
			_tcsncpy(nid.szTip, (
				TSTRING(D) + _T(' ') + Util::formatBytesT(DownloadManager::getRunningAverage()) + _T('/') + TSTRING(S) + _T(" (") +
				Util::toStringT(DownloadManager::getDownloadCount()) + _T(")\r\n") +
				TSTRING(U) + _T(' ') + Util::formatBytesT(UploadManager::getRunningAverage()) + _T('/') + TSTRING(S) + _T(" (") +
				Util::toStringT(UploadManager::getUploadCount()) + _T(")") + _T("\r\n") +
				TSTRING(UPTIME) + _T(' ') + Util::formatSecondsT(Util::getUpTime())).c_str(), 63);
			::Shell_NotifyIcon(NIM_MODIFY, &nid);
			lastTickMouseMove = tick;
		}
	}
	else if (lParam == WM_RBUTTONUP)
	{
		CPoint pt(GetMessagePos());
		SetForegroundWindow(m_hWnd);
		if (IsWindowVisible() || !hasPasswordTray())
			trayMenu.TrackPopupMenu(TPM_RIGHTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		PostMessage(WM_NULL, 0, 0);
	}
	return 0;
}

LRESULT MainFrame::onTaskbarCreated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// When your taskbar application receives this message, it should assume that any taskbar icons it added have been removed and add them again. 
	int newIcon = TRAY_ICON_NONE;
	if (useTrayIcon)
		newIcon = hasPM ? TRAY_ICON_PM : TRAY_ICON_NORMAL;
	trayIcon = TRAY_ICON_NONE;
	setTrayIcon(newIcon);
	return 0;
}

LRESULT MainFrame::onTaskbarButtonCreated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
	if (!CompatibilityManager::isWin7Plus())
		return 0;
#endif

	taskbarList.Release();
	if (FAILED(taskbarList.CoCreateInstance(CLSID_TaskbarList)))
		return 0;

	THUMBBUTTON buttons[3];
	const int sizeTip = _countof(buttons[0].szTip) - 1;
	buttons[0].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
	buttons[0].iId = IDC_OPEN_DOWNLOADS;
	buttons[0].hIcon = g_iconBitmaps.getIcon(IconBitmaps::DOWNLOADS_DIR, 0);
	wcsncpy(buttons[0].szTip, CWSTRING(MENU_OPEN_DOWNLOADS_DIR), sizeTip);
	buttons[0].szTip[sizeTip] = 0;
	buttons[0].dwFlags = THBF_ENABLED;

	buttons[1].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
	buttons[1].iId = ID_FILE_SETTINGS;
	buttons[1].hIcon = g_iconBitmaps.getIcon(IconBitmaps::SETTINGS, 0);
	wcsncpy(buttons[1].szTip, CWSTRING(SETTINGS), sizeTip);
	buttons[1].szTip[sizeTip] = 0;
	buttons[1].dwFlags = THBF_ENABLED;

	buttons[2].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
	buttons[2].iId = IDC_REFRESH_FILE_LIST;
	buttons[2].hIcon = g_iconBitmaps.getIcon(IconBitmaps::REFRESH_SHARE, 0);
	wcsncpy(buttons[2].szTip, CWSTRING(CMD_SHARE_REFRESH), sizeTip);
	buttons[2].szTip[sizeTip] = 0;
	buttons[2].dwFlags = THBF_ENABLED;

	taskbarList->ThumbBarAddButtons(m_hWnd, _countof(buttons), buttons);
		
	return 0;
}

LRESULT MainFrame::onAppShow(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/)
{
	if (::IsIconic(m_hWnd))
	{
		if (!wasMaximized && hasPasswordTray())
		{
			INT_PTR result;
			if (getPasswordInternal(result, nullptr))
				showWindow();
		}
		else
			showWindow();
	}
	return 0;
}

void MainFrame::ShowBalloonTip(const tstring& message, const tstring& title, int infoFlags)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(getMainFrame());
	if (getMainFrame() && !ClientManager::isBeforeShutdown() && PopupManager::isValidInstance())
	{
		Popup* p = new Popup;
		p->title = title;
		p->message = message;
		p->icon = infoFlags;
		safe_post_message(*getMainFrame(), SHOW_POPUP_MESSAGE, p);
	}
}

LRESULT MainFrame::onViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;
	bVisible = !bVisible;
	CReBarCtrl rebar(m_hWndToolBar);
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_TOOLBAR, bVisible);
	ctrlToolbar.CheckButton(ID_VIEW_TOOLBAR, bVisible);
	return 0;
}

LRESULT MainFrame::onViewWinampBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;
	bVisible = !bVisible;
	CReBarCtrl rebar(m_hWndToolBar);
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 3);
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_TOGGLE_TOOLBAR, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_PLAYER_CONTROLS, bVisible);
	ctrlToolbar.CheckButton(ID_TOGGLE_TOOLBAR, bVisible);
	return 0;
}

LRESULT MainFrame::onViewQuickSearchBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;
	bVisible = !bVisible;
	CReBarCtrl rebar(m_hWndToolBar);
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 2);
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_TOGGLE_QSEARCH, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_QUICK_SEARCH, bVisible);
	return 0;
}

LRESULT MainFrame::onViewTopmost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UISetCheck(IDC_TOPMOST, !BOOLSETTING(TOPMOST));
	SET_SETTING(TOPMOST, !BOOLSETTING(TOPMOST));
	toggleTopmost();
	return 0;
}

LRESULT MainFrame::onViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_STATUSBAR, bVisible);
	return 0;
}

void MainFrame::toggleTransferView(BOOL bVisible)
{
	if (!bVisible)
	{
		if (GetSinglePaneMode() != SPLIT_PANE_TOP)
		{
			SetSinglePaneMode(SPLIT_PANE_TOP);
		}
	}
	else
	{
		if (GetSinglePaneMode() != SPLIT_PANE_NONE)
		{
			SetSinglePaneMode(SPLIT_PANE_NONE);
		}
	}
	
	UISetCheck(ID_VIEW_TRANSFER_VIEW, bVisible);
	UpdateLayout();
	ctrlToolbar.CheckButton(ID_VIEW_TRANSFER_VIEW, bVisible);
}

LRESULT MainFrame::onViewTransferView(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const BOOL bVisible = !BOOLSETTING(SHOW_TRANSFERVIEW);
	SET_SETTING(SHOW_TRANSFERVIEW, bVisible);
	toggleTransferView(bVisible);
	return 0;
}

LRESULT MainFrame::onViewTransferViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const BOOL bVisible = !BOOLSETTING(SHOW_TRANSFERVIEW_TOOLBAR);
	SET_SETTING(SHOW_TRANSFERVIEW_TOOLBAR, bVisible);
	ctrlToolbar.CheckButton(ID_VIEW_TRANSFER_VIEW_TOOLBAR, bVisible);
	UISetCheck(ID_VIEW_TRANSFER_VIEW_TOOLBAR, bVisible);
	transferView.UpdateLayout();
	return 0;
}

LRESULT MainFrame::onCloseWindows(WORD, WORD wID, HWND, BOOL&)
{
	switch (wID)
	{
		case IDC_CLOSE_DISCONNECTED:
			HubFrame::closeDisconnected();
			break;
		case IDC_CLOSE_ALL_HUBS:
			HubFrame::closeAll(0);
			break;
		case IDC_CLOSE_HUBS_BELOW:
			HubFrame::closeAll(SETTING(USER_THERSHOLD));
			break;
		case IDC_CLOSE_HUBS_NO_USR:
			HubFrame::closeAll(2);
			break;
		case IDC_CLOSE_ALL_PM:
			PrivateFrame::closeAll();
			break;
		case IDC_CLOSE_ALL_OFFLINE_PM:
			PrivateFrame::closeAllOffline();
			break;
		case IDC_CLOSE_ALL_DIR_LIST:
			DirectoryListingFrame::closeAll();
			break;
		case IDC_CLOSE_ALL_SEARCH_FRAME:
			SearchFrame::closeAll();
			break;
		case IDC_RECONNECT_DISCONNECTED:
			HubFrame::reconnectDisconnected();
			break;
	}
	return 0;
}

LRESULT MainFrame::onLimiter(WORD, WORD, HWND, BOOL&)
{
	onLimiter();
	return 0;
}

LRESULT MainFrame::onQuickConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		return 0;
	}

	LineDlg dlg;
	dlg.description = TSTRING(HUB_ADDRESS);
	dlg.title = TSTRING(QUICK_CONNECT);
	dlg.icon = IconBitmaps::QUICK_CONNECT;
	dlg.allowEmpty = false;
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		tstring tmp = dlg.line;
		// Strip out all the spaces
		string::size_type i;
		while ((i = tmp.find(' ')) != string::npos)
			tmp.erase(i, 1);
			
		if (!tmp.empty())
		{
			uint16_t port = 0;
			string proto, host, file, query, fragment;	
			Util::decodeUrl(Text::fromT(tmp), proto, host, port, file, query, fragment);
			if (!Util::getHubProtocol(proto))
			{
				MessageBox(CTSTRING_F(UNSUPPORTED_HUB_PROTOCOL, Text::toT(proto)), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
				return 0;
			}
			if (host.empty())
			{
				MessageBox(CTSTRING(INCOMPLETE_FAV_HUB), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
				return 0;
			}
			const string formattedUrl = Util::formatDchubUrl(proto, host, port);
			RecentHubEntry r;
			r.setServer(formattedUrl);
			FavoriteManager::getInstance()->addRecent(r);
			HubFrame::openHubWindow(formattedUrl);
		}
	}
	return 0;
}

void MainFrame::on(QueueManagerListener::PartialList, const HintedUser& aUser, const string& text) noexcept
{
	safe_post_message(*this, BROWSE_LISTING, new DirectoryBrowseInfo(aUser, text));
}

void MainFrame::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string& dir, const DownloadPtr& download) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (qi->isSet(QueueItem::FLAG_CLIENT_VIEW))
		{
			if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST))
			{
				// This is a file listing, show it...
				auto dirInfo = new QueueManager::DirectoryListInfo(download->getHintedUser(), qi->getListName(), dir, download->getRunningAverage(), qi->isSet(QueueItem::FLAG_DCLST_LIST));
				safe_post_message(*this, DOWNLOAD_LISTING, dirInfo);
			}
			else if (qi->isSet(QueueItem::FLAG_TEXT))
			{
				safe_post_message(*this, VIEW_FILE_AND_DELETE, new tstring(Text::toT(qi->getTarget())));
			}
			else
			{
				WinUtil::openFile(Text::toT(qi->getTarget()));
			}
		}
	}
}

LRESULT MainFrame::onActivateApp(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	WinUtil::g_isAppActive = wParam == TRUE;
	if (WinUtil::g_isAppActive)
		clearPMStatus();
	return 0;
}

LRESULT MainFrame::onAppCommand(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if (GET_APPCOMMAND_LPARAM(lParam) == APPCOMMAND_BROWSER_FORWARD)
	{
		ctrlTab.switchTo();
	}
	else if (GET_APPCOMMAND_LPARAM(lParam) == APPCOMMAND_BROWSER_BACKWARD)
	{
		ctrlTab.switchTo(false);
	}
	else
	{
		bHandled = FALSE;
		return FALSE;
	}
	
	return TRUE;
}

LRESULT MainFrame::onAway(WORD, WORD, HWND, BOOL&)
{
	setAway(!Util::getAway());
	return 0;
}

void MainFrame::setAway(bool flag)
{
	setAwayButton(flag);
	Util::setAway(flag);
}

LRESULT MainFrame::onDisableSounds(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool soundDisabled = ctrlToolbar.IsButtonChecked(IDC_DISABLE_SOUNDS) != FALSE;
	SET_SETTING(SOUNDS_DISABLED, soundDisabled);
	return 0;
}

LRESULT MainFrame::onDisablePopups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool popusDisabled = ctrlToolbar.IsButtonChecked(IDC_DISABLE_POPUPS) != FALSE;
	SET_SETTING(POPUPS_DISABLED, popusDisabled);
	return 0;
}

void MainFrame::on(WebServerListener::Setup) noexcept
{
	WSAAsyncSelect(WebServerManager::getInstance()->getServerSocket().getSock(), m_hWnd, WEBSERVER_SOCKET_MESSAGE, FD_ACCEPT);
}

void MainFrame::on(WebServerListener::ShutdownPC, int action) noexcept
{
	WinUtil::shutDown(action);
}

LRESULT MainFrame::onSelected(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HWND hWnd = (HWND)wParam;
	if (MDIGetActive() != hWnd)
	{
		WinUtil::activateMDIChild(hWnd);
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

LRESULT MainFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	useTrayIcon = false;
	setTrayIcon(TRAY_ICON_NONE);
	bHandled = FALSE;
	winampMenu.DestroyMenu();
	return 0;
}

LRESULT MainFrame::onLockToolbars(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UISetCheck(IDC_LOCK_TOOLBARS, !BOOLSETTING(LOCK_TOOLBARS));
	SET_SETTING(LOCK_TOOLBARS, !BOOLSETTING(LOCK_TOOLBARS));
	toggleLockToolbars();
	return 0;
}

LRESULT MainFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	if (reinterpret_cast<HWND>(wParam) == m_hWndToolBar)
	{
		tbMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	if (reinterpret_cast<HWND>(wParam) == m_hWndStatusBar)      // SCALOlaz : use menus on status
	{
		// Get m_hWnd Mode (maximized, normal)
		// for DHT area
		POINT ptClient = pt;
		::ScreenToClient(m_hWndStatusBar, &ptClient);
		
		// AWAY
		if (ptClient.x >= tabAwayRect.left && ptClient.x <= tabAwayRect.right)
		{
			tabAwayMenu.CheckMenuItem(IDC_STATUS_AWAY_ON_OFF, MF_BYCOMMAND | (SETTING(AWAY) ? MF_CHECKED : MF_UNCHECKED));
			tabAwayMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
		if (ptClient.x >= tabDownSpeedRect.left && ptClient.x <= tabDownSpeedRect.right)
		{
			setSpeedLimit(SettingsManager::MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, 0, 6144); // min, max ~6mbit
		}
		
		if (ptClient.x >= tabUpSpeedRect.left && ptClient.x <= tabUpSpeedRect.right)
		{
			setSpeedLimit(SettingsManager::MAX_UPLOAD_SPEED_LIMIT_NORMAL, 0, 6144);   //min, max ~6mbit
		}
		
		if ((ptClient.x >= tabDownSpeedRect.left && ptClient.x <= tabDownSpeedRect.right) || (ptClient.x >= tabUpSpeedRect.left && ptClient.x <= tabUpSpeedRect.right))
		{
			if (SETTING(MAX_UPLOAD_SPEED_LIMIT_NORMAL) == 0 && SETTING(MAX_DOWNLOAD_SPEED_LIMIT_NORMAL) == 0)
			{
				onLimiter(true);    // true = turn OFF throttle limiter
			}
		}
#endif
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT MainFrame::onContextMenuL(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const POINT ptClient = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	// AWAY area
	if (ptClient.x >= tabAwayRect.left && ptClient.x <= tabAwayRect.right)
		setAway(!Util::getAway());
	bHandled = FALSE;
	return 0;
}

#ifdef SCALOLAZ_SPEEDLIMIT_DLG
void MainFrame::setSpeedLimit(SettingsManager::IntSetting setting, int minValue, int maxValue)
{
	SpeedVolDlg dlg(SettingsManager::get(setting), minValue, maxValue);
	if (dlg.DoModal() == IDOK)
	{
		int value = dlg.GetLimit();
		if (SettingsManager::get(setting) != value)
		{
			SettingsManager::set(setting, value);
			onLimiter(false);
		}
	}
}
#endif

LRESULT MainFrame::onMenuSelect(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	// [+] brain-ripper
	// There is strange bug in WTL: when menu opened, status-bar is disappeared.
	// It is caused by WTL's OnMenuSelect handler (atlframe.h), by calling
	// ::SendMessage(m_hWndStatusBar, SB_SIMPLE, TRUE, 0L);
	// This supposed to switch status-bar to simple mode and show description of tracked menu-item,
	// but status-bar just disappears.
	//
	// Since we not provide description for menu-items in status-bar,
	// i just turn off this feature by manually handling OnMenuSelect event.
	// Do nothing in here, just mark as handled, to suppress WTL processing
	//
	// If decided to use menu-item descriptions in status-bar, then
	// remove this function and debug why status-bar is disappeared
	return FALSE;
}

void MainFrame::toggleTopmost()
{
	DWORD dwExStyle = (DWORD)GetWindowLongPtr(GWL_EXSTYLE);
	CRect rc;
	GetWindowRect(rc);
	HWND order = (dwExStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST;
	::SetWindowPos(m_hWnd, order, rc.left, rc.top, rc.Width(), rc.Height(), SWP_SHOWWINDOW);
}

void MainFrame::toggleLockToolbars()
{
	CReBarCtrl rebar(m_hWndToolBar);
	REBARBANDINFO rbi = {};
	rbi.cbSize = sizeof(rbi);
	rbi.fMask  = RBBIM_STYLE;
	int count = rebar.GetBandCount();
	for (int i = 0; i < count; i++)
	{
		rebar.GetBandInfo(i, &rbi);
		rbi.fStyle ^= RBBS_NOGRIPPER | RBBS_GRIPPERALWAYS;
		rebar.SetBandInfo(i, &rbi);
	}
}

#ifdef IRAINMAN_INCLUDE_SMILE
LRESULT MainFrame::onAnimChangeFrame(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if (!CGDIImage::isShutdown() && !MainFrame::isAppMinimized())
	{
		CGDIImage *pImage = (CGDIImage *)lParam;
		//dcdebug("OnAnimChangeFrame  pImage = %p\r\n", lParam);
		if (pImage)
		{
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
			const bool l_is_live_gdi = CGDIImage::isGDIImageLive(pImage);
			dcassert(l_is_live_gdi);
			if (l_is_live_gdi)
			{
				pImage->DrawFrame();
			}
			else
			{
				LogManager::message("Error in OnAnimChangeFrame CGDIImage::isGDIImageLive!");
			}
#else
			pImage->DrawFrame();
#endif
		}
	}
	return 0;
}
#endif // IRAINMAN_INCLUDE_SMILE

LRESULT MainFrame::onAddMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AddMagnet dlg;
	dlg.DoModal(m_hWnd);
	return 0;
}

void MainFrame::on(QueueManagerListener::TryAdding, const string& fileName, int64_t newSize, int64_t existingSize, time_t existingTime, int& option) noexcept
{
	bool applyForAll;
	CheckTargetDlg::showDialog(*this, fileName, newSize, existingSize, existingTime, option, applyForAll);
	if (applyForAll)
		SET_SETTING(TARGET_EXISTS_ACTION, option);
}

LRESULT MainFrame::onToolbarDropDown(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTOOLBAR* ptb = reinterpret_cast<NMTOOLBAR*>(pnmh);
	if (ptb && ptb->iItem == IDC_WINAMP_SPAM)
	{
		CToolBarCtrl toolbar(pnmh->hwndFrom);
		// Get the button rect
		CRect rect;
		toolbar.GetItemRect(toolbar.CommandToIndex(ptb->iItem), &rect);
		// Create a point
		CPoint pt(rect.left, rect.bottom);
		// Map the points
		toolbar.MapWindowPoints(HWND_DESKTOP, &pt, 1);
		// Load the menu
		int iCurrentMediaSelection = SETTING(MEDIA_PLAYER);
		for (int i = 0; i < SettingsManager::PlayersCount; i++)
			winampMenu.CheckMenuItem(ID_MEDIA_MENU_WINAMP_START + i, MF_BYCOMMAND | ((iCurrentMediaSelection == i) ? MF_CHECKED : MF_UNCHECKED));
		ctrlCmdBar.TrackPopupMenu(winampMenu, TPM_RIGHTBUTTON | TPM_VERTICAL, pt.x, pt.y);
	}
	return 0;
}

LRESULT MainFrame::onMediaMenu(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int selectedMenu = wID - ID_MEDIA_MENU_WINAMP_START;
	if (selectedMenu < SettingsManager::PlayersCount && selectedMenu > -1)
	{
		SET_SETTING(MEDIA_PLAYER, selectedMenu);
	}
	return 0;
}

void MainFrame::shareFolderFromShell(const tstring& infolder)
{
	tstring folder = infolder;
	Util::appendPathSeparator(folder);
	const string strFolder = Text::fromT(folder);
	vector<ShareManager::SharedDirInfo> directories;
	ShareManager::getInstance()->getDirectories(directories);
	bool found = false;
	for (auto j = directories.cbegin(); j != directories.cend(); ++j)
	{
		// Compare with
		if (!strFolder.compare(j->realPath))
		{
			found = true;
			break;
		}
	}
	
	if (!found)
	{
		// [!] SSA Need to add Dialog Question
		bool shareFolder = true;
		if (BOOLSETTING(CONFIRM_SHARE_FROM_SHELL))
		{
			tstring question = folder;
			question += _T("\r\n");
			question += TSTRING(SECURITY_SHARE_FROM_SHELL_QUESTION);
			shareFolder = (MessageBox(question.c_str(), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES);
		}
		if (shareFolder)
		{
			try
			{
				CWaitCursor waitCursor;
				tstring lastName = Util::getLastDir(folder);
				ShareManager::getInstance()->addDirectory(strFolder, Text::fromT(lastName));
				tstring mmessage = folder;
				mmessage += _T(" (");
				mmessage += lastName;
				mmessage += _T(')');
				SHOW_POPUP(POPUP_ON_FOLDER_SHARED, mmessage, TSTRING(SHARE_NEW_FOLDER_MESSAGE));
				LogManager::message(STRING(SHARE_NEW_FOLDER_MESSAGE) + ' ' + Text::fromT(mmessage));
			}
			catch (const Exception& ex)
			{
				LogManager::message(STRING(SHARE_NEW_FOLDER_ERROR) + " (" + Text::fromT(folder) + ") " + ex.getError());
			}
		}
	}
}

void MainFrame::on(UserManagerListener::OutgoingPrivateMessage, const UserPtr& to, const string& hint, const tstring& message) noexcept
{
	PrivateFrame::openWindow(nullptr, HintedUser(to, hint), Util::emptyString, message);
}

void MainFrame::on(UserManagerListener::OpenHub, const string& url) noexcept
{
	HubFrame::openHubWindow(url);
}

void MainFrame::on(UserManagerListener::CollectSummaryInfo, const UserPtr& user, const string& hubHint) noexcept
{
	UserInfoSimple(user, hubHint).addSummaryMenu();
}

void MainFrame::on(FinishedManagerListener::AddedDl, bool isFile, const FinishedItemPtr&) noexcept
{
	if (isFile)
	{
		PLAY_SOUND(SOUND_FINISHFILE);
	}
}

void MainFrame::on(FinishedManagerListener::AddedUl, bool isFile, const FinishedItemPtr&) noexcept
{
	if (isFile)
	{
		PLAY_SOUND(SOUND_UPLOADFILE);
	}
}

void MainFrame::on(QueueManagerListener::SourceAdded) noexcept
{
	PLAY_SOUND(SOUND_SOURCEFILE);
}

BOOL MainFrame::PreTranslateMessage(MSG* pMsg)
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
		return TRUE;
	return FALSE;
}
