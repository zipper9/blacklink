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
#include "UsersFrame.h"
#include "DirectoryListingFrm.h"
#include "RecentsFrm.h"
#include "FavoritesFrm.h"
#include "NotepadFrame.h"
#include "QueueFrame.h"
#include "SpyFrame.h"
#include "FinishedFrame.h"
#include "ADLSearchFrame.h"
#include "FinishedULFrame.h"
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
#include "Winamp.h"
#include "Players.h"
#include "iTunesCOMInterface.h"
#include "ToolbarManager.h"
#include "AboutDlgIndex.h"
#include "AddMagnet.h" // [+] NightOrion
#include "CheckTargetDlg.h" // [+] SSA
#ifdef IRAINMAN_INCLUDE_SMILE
# include "../GdiOle/GDIImage.h"
#endif
#include "../client/ConnectionManager.h"
#include "../client/ConnectivityManager.h"
#include "../client/UploadManager.h"
#include "../client/DownloadManager.h"
#include "../client/SimpleXML.h"
#include "../client/LogManager.h"
#include "../client/WebServerManager.h"
#include "../client/ThrottleManager.h"
#include "../client/CryptoManager.h"
#ifdef FLYLINKDC_USE_CUSTOM_MENU
#include "../FlyFeatures/CustomMenuManager.h" //[+] //SSA
#endif
#include "../client/MappingManager.h"
#include "../client/Text.h"
#include "../client/NmdcHub.h"
#include "HIconWrapper.h"
#ifdef SSA_WIZARD_FEATURE
# include "Wizards/FlyWizard.h"
#endif
#include "PrivateFrame.h"
#include "PublicHubsFrm.h"

#ifdef SCALOLAZ_SPEEDLIMIT_DLG
# include "SpeedVolDlg.h"
#endif
#include "../FlyFeatures/CProgressDlg.h"
#include "../FlyFeatures/flyfeatures.h" // [+] SSA
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
#include "CFlyLocationDlg.h"
#endif
#include "ExMessageBox.h"

#define FLYLINKDC_CALC_MEMORY_USAGE // TODO: move to CompatibilityManager
#  ifdef FLYLINKDC_CALC_MEMORY_USAGE
#   ifdef FLYLINKDC_SUPPORT_WIN_VISTA
#    define PSAPI_VERSION 1
#   endif
#   include <psapi.h>
#   pragma comment(lib, "psapi.lib")
#endif // FLYLINKDC_CALC_MEMORY_USAGE


int HashProgressDlg::g_is_execute = 0;

bool g_TabsCloseButtonEnabled;
DWORD g_GDI_count = 0;
int   g_RAM_WorkingSetSize = 0;
int   g_RAM_PeakWorkingSetSize = 0;

#define FLYLINKDC_USE_TASKBUTTON_PROGRESS

MainFrame* MainFrame::g_anyMF = nullptr;
bool MainFrame::g_isHardwareShutdown = false;
uint64_t MainFrame::g_CurrentShutdownTime = 0;
bool MainFrame::g_isShutdownStatus = false;
CComboBox MainFrame::QuickSearchBox;
bool MainFrame::g_bDisableAutoComplete = false;
bool MainFrame::g_bAppMinimized = false;
int MainFrame::g_CountSTATS = 0; //[+]PPA


// [+] IRainman Speedmeter
uint64_t MainFrame::g_lastUpdate = 0;
int64_t MainFrame::g_updiff = 0;
int64_t MainFrame::g_downdiff = 0;
// [~] IRainman Speedmeter

const char* g_magic_password = "LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ";
MainFrame::MainFrame() :
	CSplitterImpl(false),
	TimerHelper(m_hWnd),
	m_second_count(60),
	m_trayMessage(0),
	m_tbButtonMessage(0), // [+] InfinitySky.
	m_is_maximized(false),
	m_lastUp(0),
	m_lastMove(0),
	m_lastDown(0), // [+] IRainman Speedmeter
	m_is_tbarcreated(false),
	m_is_wtbarcreated(false),
	m_is_qtbarcreated(false),
	m_is_end_session(false),
	m_oldshutdown(false),
	m_stopperThread(nullptr),
	m_bHashProgressVisible(false),
	m_isOpenHubFrame(false),
	m_closing(false),
	m_menuclose(false), // [+] InfinitySky.
	m_is_missedAutoConnect(false),
#ifdef IRAINMAN_IP_AUTOUPDATE
	m_elapsedMinutesFromlastIPUpdate(0),
#endif
	m_index_new_version_menu_item(0),
	m_appIcon(nullptr),
	m_trayIcon(nullptr),
	m_bTrayIcon(false),
	m_bIsPM(false),
	m_count_status_change(0),
	QuickSearchBoxContainer(WC_COMBOBOX, this, QUICK_SEARCH_MAP),
	QuickSearchEditContainer(WC_EDIT, this, QUICK_SEARCH_MAP),
#ifdef SSA_WIZARD_FEATURE
	m_is_wizard(false),
#endif
	m_is_start_autoupdate(false),
	m_numberOfReadBytes(0),
	m_maxnumberOfReadBytes(100),
	statusContainer(STATUSCLASSNAME, this, STATUS_MESSAGE_MAP),
	m_diff(GET_TICK()), // [!] IRainman fix.
	m_stopexit(false)
{
	m_bUpdateProportionalPos = false; // Исправил залипание сплиттера в верхней части
	g_anyMF = this;
	memzero(m_statusSizes, sizeof(m_statusSizes));
}

bool MainFrame::isAppMinimized(HWND p_hWnd)
{
	return g_bAppMinimized && WinUtil::g_tabCtrl && WinUtil::g_tabCtrl->isActive(p_hWnd);
	
}

MainFrame::~MainFrame()
{
	LogManager::g_mainWnd = nullptr;
	m_CmdBar.m_hImageList = NULL;
	smallImages.Destroy();
	largeImages.Destroy();
	largeImagesHot.Destroy();
	winampImages.Destroy();
	winampImagesHot.Destroy();
	m_ShutdownIcon.reset();
	
#ifdef IRAINMAN_INCLUDE_SMILE
	CAGEmotionSetup::destroyEmotionSetup();
#endif
	WinUtil::uninit();
}

unsigned int WINAPI MainFrame::stopper(void* p)
{
	MainFrame* mf = (MainFrame*)p;
	HWND wnd = nullptr;
	HWND wnd2 = nullptr;
	boost::unordered_map<HWND, int> l_wm_close_message;
	while (mf->m_hWndMDIClient && (wnd =::GetWindow(mf->m_hWndMDIClient, GW_CHILD)) != NULL)
	{
		auto& l_result = l_wm_close_message[wnd];
		++l_result;
		if (wnd == wnd2)
		{
#ifdef _DEBUG
			LogManager::message("MainFrame::stopper Sleep(10) wnd = " + Util::toHexString((long) wnd));
#endif
			Sleep(10);
			if (l_result > 1000)
			{
				//dcassert(0);
				LogManager::message("MainFrame::stopper Sleep(10) wnd = " + Util::toHexString((long) wnd) + " count > 1000!");
				break;
			}
		}
		else
		{
			if (l_result > 1)
			{
				LogManager::message("MainFrame::stopper duplicate ::PostMessage wnd = " + Util::toHexString((long) wnd));
				dcassert(0);
			}
			const auto l_post_result = ::PostMessage(wnd, WM_CLOSE, 0, 0);
			if (l_post_result == 0)
			{
				dcassert(0);
				LogManager::message("MainFrame::stopper ::PostMessage(wnd, WM_CLOSE, 0, 0) == 0[!] wnd = " + Util::toString((long) wnd));
			}
#ifdef _DEBUG
			LogManager::message("MainFrame::stopper ::PostMessage(wnd, WM_CLOSE, 0, 0) wnd = " + Util::toHexString((long) wnd));
#endif
			wnd2 = wnd;
		}
	}
	
	mf->PostMessage(WM_CLOSE);
	return 0;
}

LRESULT MainFrame::onMatchAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	QueueManager::getInstance()->matchAllFileLists(); // [!] IRainman fix.
	return 0;
}

void MainFrame::createMainMenu(void) // [+]Drakon. Enlighting functions.
{
	// Loads images and creates command bar window
	m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	m_CmdBar.SetImageSize(16, 16); // [+] IRainman fix picture size.
	m_hMenu = WinUtil::g_mainMenu;
	
	m_CmdBar.AttachMenu(m_hMenu);
	
	CImageList tmp;
	
#ifdef _DEBUG
	size_t iMainToolbarImages = 0;
	size_t iWinampToolbarImages = 0;
	
	for (size_t i = 0; g_ToolbarButtons[i].id; ++i)
	{
		iMainToolbarImages++;
	}
	
	for (size_t i = 0; g_WinampToolbarButtons[i].id; ++i)
	{
		iWinampToolbarImages++;
	}
#endif
	
	ResourceLoader::LoadImageList(IDR_TOOLBAR_MINI, smallImages, 16, 16);
	ResourceLoader::LoadImageList(IDR_PLAYERS_CONTROL_MINI, tmp, 16, 16);
	
	//dcassert(images.GetImageCount() == iMainToolbarImages);
	//dcassert(winampimages.GetImageCount() == iWinampToolbarImages);
	
	int imageCount = tmp.GetImageCount();
	for (int i = 0; i < imageCount; i++)
	{
		HICON icon = tmp.GetIcon(i);
		smallImages.AddIcon(icon);
		DestroyIcon(icon);
	}
	
	tmp.Destroy();
	
	m_CmdBar.m_hImageList = smallImages;
#ifdef _DEBUG
	int iImageInd = 0;
#endif
	
#ifdef FLYLINKDC_USE_DEAD_CODE
	// TODO
	// вот этот цикл ниразу не выполняется. пока не понял зачем сделали там - даже в 4xx это есть
	// в ToolbarButtons[0].image лежит всегда 0
	// Add fake items for unused icons, to save icons ids order
	for (int i = 0; i < ToolbarButtons[0].image; i++)
	{
		m_CmdBar.m_arrCommand.Add(0);
#ifdef _DEBUG
		iImageInd++;
#endif
	}
#endif
	
	for (size_t i = 0; g_ToolbarButtons[i].id; i++)
	{
		// If fell assert, check comment for
		// ToolbarButtons array in definition.
		dcassert(g_ToolbarButtons[i].image == iImageInd);
		
		m_CmdBar.m_arrCommand.Add(g_ToolbarButtons[i].id);
#ifdef _DEBUG
		iImageInd++; //-V127
#endif
	}
	
	// Add menu icons that are not used in toolbar
	for (size_t i = 0; g_MenuImages[i].id; i++)
	{
		// If fell assert, check comment for
		// ToolbarButtons and MenuImages arrays in definition.
		dcassert(g_MenuImages[i].image == iImageInd);
		
		m_CmdBar.m_arrCommand.Add(g_MenuImages[i].id);
#ifdef _DEBUG
		iImageInd++; //-V127
#endif
	}
	
	// If felt here, recheck WinampToolbarButtons, icons bitmap
	// and if it's all ok - uncomment follow block
	dcassert(g_WinampToolbarButtons[0].image == 0);
	
#ifdef _DEBUG
	iImageInd = g_WinampToolbarButtons[0].image;
#endif
	for (size_t i = 0; g_WinampToolbarButtons[i].id; i++)
	{
		// If fell assert, check comment for
		// ToolbarButtons array in definition.
		dcassert(g_WinampToolbarButtons[i].image == iImageInd);
		
		m_CmdBar.m_arrCommand.Add(g_WinampToolbarButtons[i].id);
#ifdef _DEBUG
		iImageInd++; //-V127
#endif
	}
	
#if _WTL_CMDBAR_VISTA_MENUS
	// Use Vista-styled menus for Windows Vista and later.
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (CompatibilityManager::isOsVistaPlus())
#endif
	{
		m_CmdBar._AddVistaBitmapsFromImageList(0, m_CmdBar.m_arrCommand.GetSize());
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

LRESULT MainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	g_fly_server_stat.startTick(CFlyServerStatistics::TIME_START_GUI);
#endif // FLYLINKDC_USE_GATHER_STATISTICS
	
	if (CompatibilityManager::isIncompatibleSoftwareFound())
	{
		if (CFlylinkDBManager::getInstance()->get_registry_variable_string(e_IncopatibleSoftwareList) != CompatibilityManager::getIncompatibleSoftwareList())
		{
			CFlylinkDBManager::getInstance()->set_registry_variable_string(e_IncopatibleSoftwareList, CompatibilityManager::getIncompatibleSoftwareList());
			LogManager::message("CompatibilityManager::detectUncompatibleSoftware = " + CompatibilityManager::getIncompatibleSoftwareList());
			if (MessageBox(Text::toT(CompatibilityManager::getIncompatibleSoftwareMessage()).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)
			{
				//WinUtil::openLink(WinUtil::GetWikiLink() + _T("incompatiblesoftware"));
			}
		}
	}
#ifdef FLYLINKDC_USE_CHECK_OLD_OS
	if (BOOLSETTING(REPORT_TO_USER_IF_OUTDATED_OS_DETECTED) && CompatibilityManager::runningAnOldOS())
	{
		SET_SETTING(REPORT_TO_USER_IF_OUTDATED_OS_DETECTED, false);
		if (MessageBox(CTSTRING(OUTDATED_OS_DETECTED), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)
		{
			//WinUtil::openLink(WinUtil::GetWikiLink() + _T("outdatedoperatingsystem"));
		}
	}
	// [~] IRainman
#endif
	
#ifdef NIGHTORION_USE_STATISTICS_REQUEST
	if (BOOLSETTING(SETTINGS_STATISTICS_ASK))
	{
		MessageBox(CTSTRING(TEXT_STAT_INFO), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONINFORMATION | MB_DEFBUTTON1 | MB_TOPMOST);
		SET_SETTING(USE_FLY_SERVER_STATICTICS_SEND, true);
		SET_SETTING(SETTINGS_STATISTICS_ASK, false);
	}
#endif // NIGHTORION_USE_STATISTICS_REQUEST
	
	QueueManager::getInstance()->addListener(this);
	WebServerManager::getInstance()->addListener(this);
	UserManager::getInstance()->addListener(this); // [+] IRainman
	
	// [+] SSA Wizard. Проверяем - есть ли ник
	if (SETTING(NICK).empty()
#ifdef SSA_WIZARD_FEATURE
	        || m_is_wizard
#endif
	   )
	{
#ifdef SSA_WIZARD_FEATURE
		if (ShowSetupWizard() == IDOK)
		{
			// Wizard OK
		}
#endif
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
			MessageBox(Text::toT(e.getError()).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_ICONSTOP | MB_OK);
		}
	}
	
	LogManager::g_mainWnd = m_hWnd;
	LogManager::g_LogMessageID = STATUS_MESSAGE;
	LogManager::g_isLogSpeakerEnabled = true;
	WinUtil::init(m_hWnd);
	
	m_trayMessage = RegisterWindowMessage(_T("TaskbarCreated"));
	dcassert(m_trayMessage);
	
	// [+] InfinitySky. Taskbar buttons on Win7.
// [+] InfinitySky.
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (m_trayMessage && CompatibilityManager::isOsVistaPlus())
#endif
	{
		HMODULE l_user32lib = LoadLibrary(_T("user32"));
		LPFUNC l_d_ChangeWindowMessageFilter = (LPFUNC)GetProcAddress(l_user32lib, "ChangeWindowMessageFilter");
		dcassert(l_d_ChangeWindowMessageFilter);
		if (l_d_ChangeWindowMessageFilter)
		{
			// 1 == MSGFLT_ADD
			l_d_ChangeWindowMessageFilter(m_trayMessage, 1);
			l_d_ChangeWindowMessageFilter(WMU_WHERE_ARE_YOU, 1);
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
			if (CompatibilityManager::isWin7Plus())
#endif
			{
				m_tbButtonMessage = RegisterWindowMessage(_T("TaskbarButtonCreated"));
				l_d_ChangeWindowMessageFilter(m_tbButtonMessage, 1);
				l_d_ChangeWindowMessageFilter(WM_COMMAND, 1);
			}
			if (l_user32lib)
				FreeLibrary(l_user32lib);
		}
	}
	
	TimerManager::getInstance()->start(0, "TimerManager");
	SetWindowText(getFlylinkDCAppCaptionWithVersionT().c_str());
	createMainMenu();
	
	// [!] TODO убрать флажки нафиг! Достаточно валидность указателя проверять, я уже молчу про то, что этот флажёк не гарнтирует вообще ничего.
	m_is_tbarcreated = false;
	m_is_wtbarcreated = false;
	m_is_qtbarcreated = false;
	// [!] TODO
	
	HWND hWndToolBar = createToolbar();
	HWND hWndQuickSearchBar = createQuickSearchBar();
	HWND hWndWinampBar = createWinampToolbar();
	
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	
	AddSimpleReBarBand(m_CmdBar);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);
		
	AddSimpleReBarBand(hWndQuickSearchBar, NULL, FALSE, 200, TRUE);//  ,780 //[+]PPA
	AddSimpleReBarBand(hWndWinampBar, NULL, TRUE);
	
	CreateSimpleStatusBar();
	
	m_rebar = m_hWndToolBar;
	ToolbarManager::applyTo(m_hWndToolBar, "MainToolBar");
	
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.SetSimple(FALSE); // https://www.box.net/shared/6d96012d9690dc892187
	int w[STATUS_PART_LAST - 1] = {0};
	ctrlStatus.SetParts(STATUS_PART_LAST - 1, w);
	m_statusSizes[0] = WinUtil::getTextWidth(TSTRING(AWAY_STATUS), ctrlStatus.m_hWnd);
	
	statusContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	RECT rect = {0};
	ctrlHashProgress.Create(ctrlStatus, &rect, L"Hashing", WS_CHILD | PBS_SMOOTH, 0, IDC_STATUS_HASH_PROGRESS);
	ctrlHashProgress.SetRange(0, HashManager::GetMaxProgressValue());
	ctrlHashProgress.SetStep(1);
	
	tabAWAYMenu.CreatePopupMenu();  // [+] add context menu on DHT area in status bar
	tabAWAYMenu.AppendMenu(MF_STRING, IDC_STATUS_AWAY_ON_OFF, CTSTRING(AWAY));
	
	ctrlLastLines.Create(ctrlStatus, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	ctrlLastLines.SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	ctrlLastLines.AddTool(ctrlStatus.m_hWnd);
	ctrlLastLines.SetDelayTime(TTDT_AUTOPOP, 15000);
	
	CreateMDIClient();
	m_CmdBar.SetMDIClient(m_hWndMDIClient);
	WinUtil::g_mdiClient = m_hWndMDIClient;
	ctrlTab.setOptions(SETTING(TABS_POS), SETTING(MAX_TAB_ROWS), SETTING(TAB_SIZE), true, BOOLSETTING(TABS_CLOSEBUTTONS), BOOLSETTING(TABS_BOLD), BOOLSETTING(NON_HUBS_FRONT), false);
	ctrlTab.Create(m_hWnd, rcDefault);
	WinUtil::g_tabCtrl = &ctrlTab;
	
	m_transferView.Create(m_hWnd);
	ViewTransferView(BOOLSETTING(SHOW_TRANSFERVIEW));
	
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
	
	
	// SSA - create WinAmp toolbar Menu
	winampMenu.CreatePopupMenu();
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::WinAmp, CTSTRING(MEDIA_MENU_WINAMP));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::WinMediaPlayer, CTSTRING(MEDIA_MENU_WMP));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::iTunes, CTSTRING(MEDIA_MENU_ITUNES));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::WinMediaPlayerClassic, CTSTRING(MEDIA_MENU_WPC));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + SettingsManager::JetAudio, CTSTRING(MEDIA_MENU_JA));
	
	try
	{
		File::ensureDirectory(SETTING(LOG_DIRECTORY));
	}
	catch (const FileException&) {}
	
	
	if (SETTING(PROTECT_START) && SETTING(PASSWORD) != g_magic_password && !SETTING(PASSWORD).empty())
	{
		INT_PTR l_do_modal_result;
		if (getPasswordInternal(l_do_modal_result) == false &&
		        l_do_modal_result == IDOK)
		{
			ExitProcess(1); // Опасная функция ExitProcess - http://blog.not-a-kernel-guy.com/2007/07/15/210
		}
	}
	
	
	openDefaultWindows();
	
	ConnectivityManager::getInstance()->setupConnections();
	
	PostMessage(WM_SPEAKER, PARSE_COMMAND_LINE);
	
	HICON l_trayIcon = NULL;
	m_normalicon = l_trayIcon ? std::unique_ptr<HIconWrapper>(new HIconWrapper(l_trayIcon)) : std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_MAINFRAME)) ;
	m_pmicon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_TRAY_AND_TASKBAR_PM));
	m_emptyicon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_TRAY_AND_TASKBAR_NO_PM));//[+]IRainman
	
	updateTray(true); // [~] InfinitySky. Обновлять иконку в трее (и на панеле задач Win7). From ApexDC++. // [-] updateTray(BOOLSETTING(MINIMIZE_TRAY));
	
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
	
	m_jaControl = unique_ptr<JAControl>(new JAControl((HWND)(*this)));
	
	// We want to pass this one on to the splitter...hope it get's there...
	bHandled = FALSE;
	
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	g_fly_server_stat.stopTick(CFlyServerStatistics::TIME_START_GUI);
#endif // FLYLINKDC_USE_GATHER_STATISTICS
	createTimer(1000, 3);
	m_transferView.UpdateLayout();
	
	if (BOOLSETTING(IPUPDATE))
	{
		m_threadedUpdateIP.updateIP(BOOLSETTING(IPUPDATE));
	}
#ifdef FLYLINKDC_USE_TORRENT
	DownloadManager::getInstance()->init_torrent();
#endif
	return 0;
}
bool g_is_auto_open_queue = false; // Флаг выставляется в потоке если он заканичвается раньше построения окна.
void MainFrame::openDefaultWindows()
{
	if (BOOLSETTING(OPEN_FAVORITE_HUBS)) PostMessage(WM_COMMAND, IDC_FAVORITES);
	if (BOOLSETTING(OPEN_FAVORITE_USERS)) PostMessage(WM_COMMAND, IDC_FAVUSERS);
	if (g_is_auto_open_queue && BOOLSETTING(OPEN_QUEUE))
	{
		PostMessage(WM_COMMAND, IDC_QUEUE);
	}
	if (BOOLSETTING(OPEN_FINISHED_DOWNLOADS)) PostMessage(WM_COMMAND, IDC_FINISHED);
	if (BOOLSETTING(OPEN_WAITING_USERS)) PostMessage(WM_COMMAND, IDC_UPLOAD_QUEUE);
	if (BOOLSETTING(OPEN_FINISHED_UPLOADS)) PostMessage(WM_COMMAND, IDC_FINISHED_UL);
	if (BOOLSETTING(OPEN_SEARCH_SPY)) PostMessage(WM_COMMAND, IDC_SEARCH_SPY);
	if (BOOLSETTING(OPEN_NETWORK_STATISTICS)) PostMessage(WM_COMMAND, IDC_NET_STATS);
	if (BOOLSETTING(OPEN_NOTEPAD)) PostMessage(WM_COMMAND, IDC_NOTEPAD);
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
	if (BOOLSETTING(OPEN_CDMDEBUG)) PostMessage(WM_COMMAND, IDC_CDMDEBUG_WINDOW);
#endif
	if (!BOOLSETTING(SHOW_STATUSBAR)) PostMessage(WM_COMMAND, ID_VIEW_STATUS_BAR);
	if (!BOOLSETTING(SHOW_TOOLBAR)) PostMessage(WM_COMMAND, ID_VIEW_TOOLBAR);
	if (!BOOLSETTING(SHOW_TRANSFERVIEW))
	{
		//  PostMessage(WM_COMMAND, ID_VIEW_TRANSFER_VIEW);
	}
	if (!BOOLSETTING(SHOW_PLAYER_CONTROLS)) PostMessage(WM_COMMAND, ID_TOGGLE_TOOLBAR);
	if (!BOOLSETTING(SHOW_QUICK_SEARCH)) PostMessage(WM_COMMAND, ID_TOGGLE_QSEARCH);
#ifdef IRAINMAN_INCLUDE_RSS
	if (BOOLSETTING(OPEN_RSS)) PostMessage(WM_COMMAND, IDC_RSS); // [+] SSA
#endif
}

int MainFrame::tuneTransferSplit()
{
	int l_split_size = SETTING(TRANSFER_FRAME_SPLIT);
	m_nProportionalPos = l_split_size;
	if (m_nProportionalPos < 3000 || m_nProportionalPos > 9400)
	{
		m_nProportionalPos = 9100; // TODO - пофиксить
	}
	SET_SETTING(TRANSFER_FRAME_SPLIT, m_nProportionalPos);
	SetSplitterPanes(m_hWndMDIClient, m_transferView.m_hWnd);
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
	
	if (m_closing)
		return 0;

	const uint64_t aTick = GET_TICK();
	if (--m_second_count == 0)
	{
		m_second_count = 60;
		onMinute(aTick);
	}
	if (ClientManager::isStartup())
	{
		return 0;
	}
// [+]IRainman Speedmeter
	m_diff = (/*(lastUpdate == 0) ? aTick - 1000 :*/ aTick - g_lastUpdate); // [!] IRainman fix.
	const uint64_t l_CurrentUp   = Socket::g_stats.m_tcp.totalUp;
	const uint64_t l_CurrentDown = Socket::g_stats.m_tcp.totalDown;
	if (m_Stats.size() > SPEED_APPROXIMATION_INTERVAL_S) // Averaging interval in seconds
		m_Stats.pop_front();
		
	//dcassert(m_diff > 0);
	m_Stats.push_back(Sample(m_diff, UpAndDown(l_CurrentUp - m_lastUp, l_CurrentDown - m_lastDown)));
	g_updiff = 0;
	g_downdiff = 0;
	uint64_t period = 1;
	for (auto i = m_Stats.cbegin(); i != m_Stats.cend(); ++i)
	{
		g_updiff += i->second.first;
		g_downdiff += i->second.second;
		period += i->first;
	}
	//dcassert(period);
	if (period) // fix https://crash-server.com/Problem.aspx?ProblemID=22552
	{
		g_updiff *= 1000I64;
		g_downdiff *= 1000I64;
		g_updiff /= period;
		g_downdiff /= period;
	}
	g_lastUpdate = aTick;
	m_lastUp     = l_CurrentUp;
	m_lastDown   = l_CurrentDown;
// [~]IRainman Speedmeter

	if (m_bTrayIcon && m_bIsPM)
	{
		setIcon(((aTick / 1000) & 1) ? *m_normalicon : *m_pmicon); // !SMT!-UI
	}
	PROCESS_MEMORY_COUNTERS pmc = { 0 };
	BOOL memoryInfoResult = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	if (memoryInfoResult)
	{
		g_GDI_count = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
		g_RAM_WorkingSetSize = pmc.WorkingSetSize >> 20;
		g_RAM_PeakWorkingSetSize = pmc.PeakWorkingSetSize >> 20;
	}
	if (!g_bAppMinimized || !BOOLSETTING(MINIMIZE_TRAY) /* [-] IRainman opt: not need to update the window title when it is minimized to tray. || BOOLSETTING(SHOW_CURRENT_SPEED_IN_TITLE)*/)
	{
		if (m_count_status_change)
		{
			UpdateLayout(TRUE);
			m_count_status_change = 0;
		}
		
#ifdef FLYLINKDC_CALC_MEMORY_USAGE
		if (!CompatibilityManager::isWine())
		{
			if ((aTick / 1000) % 5 == 0)
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
				tstring title = getFlylinkDCAppCaptionWithVersionT();
				title += buf;
				SetWindowText(title.c_str());
			}
		}
#else
		const wstring l_dlstr = Util::formatBytesW(g_downdiff);
		const wstring l_ulstr = Util::formatBytesW(g_updiff);
		if (BOOLSETTING(SHOW_CURRENT_SPEED_IN_TITLE))
		{
			tstring title = TSTRING(DL) + _T(' ') + (l_dlstr) + _T(" / ") + TSTRING(UP) + _T(' ') + (l_ulstr) + _T("  -  ");
			title += getFlylinkDCAppCaptionWithVersionT();
			SetWindowText(title.c_str());
		}
#endif // FLYLINKDC_CALC_MEMORY_USAGE
		
		if (g_CountSTATS == 0) // Генерируем статистику только когда предыдущая порция обработана
		{
			dcassert(!ClientManager::isStartup());
			TStringList* Stats = new TStringList();
			Stats->push_back(Util::getAway() ? TSTRING(AWAY_STATUS) : Util::emptyStringT);
#ifdef FLYLINKDC_CALC_MEMORY_USAGE
			const wstring l_dlstr = Util::formatBytesW(g_downdiff);
			const wstring l_ulstr = Util::formatBytesW(g_updiff);
#endif
			unsigned normal, registered, op;
			Client::getCounts(normal, registered, op);
			TCHAR hubCounts[64];
			_sntprintf(hubCounts, _countof(hubCounts), _T(" %u/%u/%u"), normal, registered, op);
			Stats->push_back(TSTRING(SHARED) + _T(": ") + Util::formatBytesW(ShareManager::getShareSize()));
			Stats->push_back(TSTRING(H) + hubCounts);
			Stats->push_back(TSTRING(SLOTS) + _T(": ") + Util::toStringW(UploadManager::getFreeSlots()) + _T('/') + Util::toStringW(UploadManager::getSlots())
			                 + _T(" (") + Util::toStringW(UploadManager::getInstance()->getFreeExtraSlots()) + _T('/') + Util::toStringW(SETTING(EXTRA_SLOTS)) + _T(")"));
			Stats->push_back(TSTRING(D) + _T(' ') + Util::formatBytesW(l_CurrentDown));
			Stats->push_back(TSTRING(U) + _T(' ') + Util::formatBytesW(l_CurrentUp));
			const bool l_ThrottleEnable = BOOLSETTING(THROTTLE_ENABLE);
			Stats->push_back(TSTRING(D) + _T(" [") + Util::toStringW(DownloadManager::getDownloadCount()) + _T("][")
			                 + ((!l_ThrottleEnable || ThrottleManager::getInstance()->getDownloadLimitInKBytes() == 0) ?
			                    TSTRING(N) : Util::toStringW((int)ThrottleManager::getInstance()->getDownloadLimitInKBytes()) + TSTRING(KILO)) + _T("] ")
			                 + l_dlstr + _T('/') + TSTRING(S));
			Stats->push_back(TSTRING(U) + _T(" [") + Util::toStringW(UploadManager::getUploadCount()) + _T("][") + ((!l_ThrottleEnable || ThrottleManager::getInstance()->getUploadLimitInKBytes() == 0) ? TSTRING(N) : Util::toStringW((int)ThrottleManager::getInstance()->getUploadLimitInKBytes()) + TSTRING(KILO)) + _T("] ") + l_ulstr + _T('/') + TSTRING(S));
			g_CountSTATS++;
			if (!PostMessage(WM_SPEAKER, MAIN_STATS, (LPARAM)Stats))
			{
				dcassert(0);
				LogManager::message("Error PostMessage(WM_SPEAKER, MAIN_STATS, (LPARAM)Stats)");
				g_CountSTATS--;
				delete Stats;
			}
		}
		else
		{
			// dcassert(0); // Тик пришел раньше чем успело обработать предыдущее событие.
		}
	}
	return 0;
}

void MainFrame::onMinute(uint64_t aTick)
{
#ifdef IRAINMAN_IP_AUTOUPDATE
	const auto interval = SETTING(IPUPDATE_INTERVAL);
	if (BOOLSETTING(IPUPDATE) && interval != 0)
	{
		m_elapsedMinutesFromlastIPUpdate++;
		if (m_elapsedMinutesFromlastIPUpdate >= interval)
		{
			m_elapsedMinutesFromlastIPUpdate = 0;
			m_threadedUpdateIP.updateIP(BOOLSETTING(IPUPDATE));
		}
	}
#endif
	HublistManager::getInstance()->removeUnusedConnections();
	LogManager::closeOldFiles();
}


void MainFrame::fillToolbarButtons(CToolBarCtrl& toolbar, const string& setting, const ToolbarButton* buttons, int buttonCount)
{
	ctrlToolbar.SetButtonStructSize();
	const StringTokenizer<string> t(setting, ',');
	const StringList& l = t.getTokens();
	for (auto k = l.cbegin(); k != l.cend(); ++k)
	{
		const int i = Util::toInt(*k);
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
					tbb.iBitmap = buttons[i].image;
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
	if (!m_is_tbarcreated)
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
		ctrlToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS); // [+] PNG.
		ctrlToolbar.SetImageList(largeImages);
		ctrlToolbar.SetHotImageList(largeImagesHot);
		
		m_is_tbarcreated = true;
	}
	

	while (ctrlToolbar.GetButtonCount() > 0)
		ctrlToolbar.DeleteButton(0);

	fillToolbarButtons(ctrlToolbar, SETTING(TOOLBAR), g_ToolbarButtons, g_ToolbarButtonsCount);
	ctrlToolbar.AutoSize();
	if (m_rebar.IsWindow())   // resize of reband to fix position of chevron
	{
		const int nCount = m_rebar.GetBandCount();
		for (int i = 0; i < nCount; i++)
		{
			REBARBANDINFO rbBand = {0};
			rbBand.cbSize = sizeof(REBARBANDINFO);
			rbBand.fMask = RBBIM_IDEALSIZE | RBBIM_CHILD;
			m_rebar.GetBandInfo(i, &rbBand);
			if (rbBand.hwndChild == ctrlToolbar.m_hWnd)
			{
				RECT rect = { 0, 0, 0, 0 };
				ctrlToolbar.GetItemRect(ctrlToolbar.GetButtonCount() - 1, &rect);
				rbBand.cxIdeal = rect.right;
				m_rebar.SetBandInfo(i, &rbBand);
			}
		}
	}
	return ctrlToolbar.m_hWnd;
}

HWND MainFrame::createWinampToolbar()
{
	if (!m_is_wtbarcreated)
	{
		ResourceLoader::LoadImageList(IDR_PLAYERS_CONTROL, winampImages, 24, 24);
		ResourceLoader::LoadImageList(IDR_PLAYERS_CONTROL_HL, winampImagesHot, 24, 24);
		
		ctrlWinampToolbar.Create(m_hWnd, NULL, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
		ctrlWinampToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);  // [+] Drakon // [+] SSA
		ctrlWinampToolbar.SetImageList(winampImages);
		ctrlWinampToolbar.SetHotImageList(winampImagesHot);
		
		fillToolbarButtons(ctrlWinampToolbar, SETTING(WINAMPTOOLBAR), g_WinampToolbarButtons, g_WinampToolbarButtonsCount);
		ctrlWinampToolbar.AutoSize();
		m_is_wtbarcreated = true;
	}
	return ctrlWinampToolbar.m_hWnd;
}

HWND MainFrame::createQuickSearchBar()
{
	if (!m_is_qtbarcreated)
	{
	
		ctrlQuickSearchBar.Create(m_hWnd, NULL, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 0, ATL_IDW_TOOLBAR);
		
		TBBUTTON tb = {0};
		
		tb.iBitmap = 200;
		tb.fsStyle = TBSTYLE_SEP;
		
		ctrlQuickSearchBar.SetButtonStructSize();
		ctrlQuickSearchBar.AddButtons(1, &tb);
		ctrlQuickSearchBar.AutoSize();
		
		CRect rect;
		ctrlQuickSearchBar.GetItemRect(0, &rect);
		
		rect.bottom += 100;
		rect.left += 2;
		
		QuickSearchBox.Create(ctrlQuickSearchBar.m_hWnd, rect, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                      WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL, 0);
		                      
		updateQuickSearches();
		
		QuickSearchBoxContainer.SubclassWindow(QuickSearchBox.m_hWnd);
		QuickSearchBox.SetExtendedUI();
		QuickSearchBox.SetFont(Fonts::g_systemFont, FALSE);
		
		POINT pt;
		pt.x = 10;
		pt.y = 10;
		
		HWND hWnd = QuickSearchBox.ChildWindowFromPoint(pt);
		if (hWnd != NULL && !QuickSearchEdit.IsWindow() && hWnd != QuickSearchBox.m_hWnd)
		{
			QuickSearchEdit.Attach(hWnd);
			QuickSearchEdit.SetCueBannerText(CTSTRING(QSEARCH_STR));
			QuickSearchEditContainer.SubclassWindow(QuickSearchEdit.m_hWnd);
		}
		m_is_qtbarcreated = true;
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
		if (m_jaControl.get() && m_jaControl.get()->isJARunning())
		{
			m_jaControl.get()->JAUpdateAllInfo();
			switch (wID)
			{
				case IDC_WINAMP_BACK:
					m_jaControl.get()->JAPrevTrack();
					break;
				case IDC_WINAMP_PLAY:
				{
					if (m_jaControl.get()->isJAPaused())
						m_jaControl.get()->JASetPause();
					else if (m_jaControl.get()->isJAStopped())
						m_jaControl.get()->JASetPlay(0);
				}
				break;
				case IDC_WINAMP_STOP:
					m_jaControl.get()->JASetStop();
					break;
				case IDC_WINAMP_PAUSE:
					m_jaControl.get()->JASetPause();
					break;
				case IDC_WINAMP_NEXT:
					m_jaControl.get()->JANextTrack();
					break;
				case IDC_WINAMP_VOL_UP:
					m_jaControl.get()->JAVolumeUp();
					break;
				case IDC_WINAMP_VOL_DOWN:
					m_jaControl.get()->JAVolumeDown();
					break;
				case IDC_WINAMP_VOL_HALF:
					m_jaControl.get()->JASetVolume(50);
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
			g_bDisableAutoComplete = true;
		else
			g_bDisableAutoComplete = false;
			
	switch (wParam)
	{
		case VK_DELETE:
			if (uMsg == WM_KEYDOWN)
			{
				g_bDisableAutoComplete = true;
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
					WinUtil::getWindowText(QuickSearchEdit, s);
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

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
LRESULT MainFrame::onDeviceChange(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = TRUE;
	HashManager::getInstance()->OnDeviceChange(lParam, wParam);
	
	return TRUE;
}
#endif

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
	//if (BOOLSETTING(AUTO_COMPLETE_SEARCH)) //[-] IRainman
	{
		uint32_t nTextLen = 0;
		HWND hWndCombo = QuickSearchBox.m_hWnd;
		DWORD dwStartSel = 0, dwEndSel = 0;
		
		// Get the text length from the combobox, then copy it into a newly allocated buffer.
		nTextLen = ::SendMessage(hWndCombo, WM_GETTEXTLENGTH, NULL, NULL);
		_TCHAR *pEnteredText = new _TCHAR[nTextLen + 1];
		::SendMessage(hWndCombo, WM_GETTEXT, (WPARAM)nTextLen + 1, (LPARAM)pEnteredText);
		::SendMessage(hWndCombo, CB_GETEDITSEL, (WPARAM)&dwStartSel, (LPARAM)&dwEndSel);
		
		// Check to make sure autocompletion isn't disabled due to a backspace or delete
		// Also, the user must be typing at the end of the string, not somewhere in the middle.
		if (! g_bDisableAutoComplete && (dwStartSel == dwEndSel) && (dwStartSel == nTextLen))
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
					delete [] pStrMatchedText;
				}
			}
		}
		
		delete [] pEnteredText;
		bHandled = TRUE;
	}
	return 0;
}

void MainFrame::updateQuickSearches(bool p_clean /*= false*/)
{
	//if (BOOLSETTING(AUTO_COMPLETE_SEARCH))//[-]IRainman always is true!
	{
		QuickSearchBox.ResetContent();
		if (!p_clean)
		{
			//[+]IRainman
			if (SearchFrame::g_lastSearches.empty())
				CFlylinkDBManager::getInstance()->load_registry(SearchFrame::g_lastSearches, e_SearchHistory);
				
			for (auto i = SearchFrame::g_lastSearches.cbegin(); i != SearchFrame::g_lastSearches.cend(); ++i)//[~]IRainman
			{
				QuickSearchBox.InsertString(0, i->c_str());
			}
		}
	}
	if (BOOLSETTING(CLEAR_SEARCH))
	{
		QuickSearchBox.SetWindowText(_T(""));
	}
}

void MainFrame::getTaskbarState() // MainFrm: The event handler TaskBar Button Color in ONE FUNCTION
{
#ifdef FLYLINKDC_USE_TASKBUTTON_PROGRESS
	if (m_taskbarList) // [!] IRainman fix.
	{
		m_taskbarList->SetProgressState(m_hWnd, TBPF_NORMAL);
	}
#endif
}

LRESULT MainFrame::onSpeakerAutoConnect(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
#ifdef IRAINMAN_USE_NON_RECURSIVE_BEHAVIOR
	FavoriteHubEntryList tmp;
	{
		FavoriteManager::LockInstanceHubs lockedInstanceHubs;
		tmp = lockedInstanceHubs.getFavoriteHubs();
	}
	autoConnect(tmp);
#else
	FavoriteManager::LockInstanceHubs lockedInstanceHubs;
	autoConnect(lockedInstanceHubs.getFavoriteHubs());
#endif
	return 0;
}

LRESULT MainFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == MAIN_STATS)
	{
		std::unique_ptr<TStringList> pstr(reinterpret_cast<TStringList*>(lParam));
		if (--g_CountSTATS)
		{
			return 0; // [+] PPA Исключем лишнее обновление статусной строки и таскбара.
		}
		if (ClientManager::isBeforeShutdown() || ClientManager::isStartup())
		{
			return 0;
		}
		getTaskbarState();
		
		const TStringList& str = *pstr;
		if (ctrlStatus.IsWindow())
		{
			bool update = false;
			if (HashManager::getInstance()->IsHashing())
			{
				ctrlHashProgress.SetPos(HashManager::getInstance()->GetProgressValue());
				if (!m_bHashProgressVisible)
				{
					ctrlHashProgress.ShowWindow(SW_SHOW);
					update = true;
					m_bHashProgressVisible = true;
				}
			}
			else if (m_bHashProgressVisible)
			{
				ctrlHashProgress.ShowWindow(SW_HIDE);
				update = true;
				m_bHashProgressVisible = false;
				ctrlHashProgress.SetPos(0);
			}
			
			{
				//CLockRedraw<true> l_lock_draw(ctrlStatus.m_hWnd);
				BOOL result;
				if (m_statusText[0] != str[0])
				{
					m_statusText[0] = str[0];
					result = ctrlStatus.SetText(1, str[0].c_str()); // TODO никогда не срабатывает...
					dcassert(result);
				}
				const uint8_t count = str.size();
				dcassert(count < STATUS_PART_LAST);
				for (uint8_t i = 1; i < count; i++)
				{
					if (m_statusText[i] != str[i])
					{
						m_statusText[i] = str[i];
						const uint8_t w = WinUtil::getTextWidth(str[i], ctrlStatus.m_hWnd);
						if (i < STATUS_PART_LAST && m_statusSizes[i] < w)
						{
							m_statusSizes[i] = w;
							update = true;
						}
						//dcdebug("ctrlStatus.SetText[%d] = [%s]\n", int(i), Text::fromT(str[i]).c_str());
						
						result = ctrlStatus.SetText(i + 1, str[i].c_str()); // https://www.crash-server.com/DumpGroup.aspx?ClientID=guest&Login=Guest&DumpGroupID=127864
						dcassert(result);
						// https://www.crash-server.com/UploadedReport.aspx?DumpID=1474566
					}
					else
					{
						//dcdebug("!!! Duplicate ctrlStatus.SetText[%d] = [%s]\n", int(i), Text::fromT(str[i]).c_str());
					}
				}
			}
			
			if (isShutDown())
			{
				const uint64_t iSec = GET_TICK() / 1000;
				if (!g_isShutdownStatus)
				{
					if (!m_ShutdownIcon)
					{
						m_ShutdownIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_SHUTDOWN));
					}
					ctrlStatus.SetIcon(STATUS_PART_SHUTDOWN_TIME, *m_ShutdownIcon);
					g_isShutdownStatus = true;
				}
				if (DownloadManager::getDownloadCount() > 0)
				{
					g_CurrentShutdownTime = iSec;
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
				}
				else
				{
					const int l_timeout = SETTING(SHUTDOWN_TIMEOUT);
					const int64_t l_timeLeft = l_timeout - (iSec - g_CurrentShutdownTime);
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, (_T(' ') + Util::formatSecondsW(l_timeLeft, l_timeLeft < 3600)).c_str(), SBT_POPOUT);
					if (g_CurrentShutdownTime + l_timeout <= iSec)
					{
						// We better not try again. It WON'T work...
						g_isHardwareShutdown = false;
						
						const bool bDidShutDown = WinUtil::shutDown(SETTING(SHUTDOWN_ACTION));
						if (bDidShutDown)
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
				if (g_isShutdownStatus)
				{
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
					ctrlStatus.SetIcon(STATUS_PART_SHUTDOWN_TIME, NULL);
					g_isShutdownStatus = false;
				}
			}
			
			if (update)
			{
				m_count_status_change++;
			}
		}
	}
	else if (wParam == STATUS_MESSAGE)
	{
		LogManager::g_isLogSpeakerEnabled = true;
		char* msg = reinterpret_cast<char*>(lParam);
		if (!ClientManager::isShutdown() && !m_closing && ctrlStatus.IsWindow())
		{
			string msgStr = Util::formatDigitalClock("[%H:%M:%S] ", GET_TIME(), false);
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
			ShellExecute(NULL, NULL, file->c_str(), NULL, NULL, SW_SHOW); // !SMT!-UI
		}
	}
	else if (wParam == PARSE_COMMAND_LINE)
	{
		parseCommandLine(GetCommandLine());
	}
	else if (wParam == SHOW_POPUP_MESSAGE)
	{
		Popup* msg = (Popup*)lParam;
		if (!ClientManager::isBeforeShutdown())
		{
			dcassert(PopupManager::isValidInstance());
			if (PopupManager::isValidInstance())
			{
				PopupManager::getInstance()->Show(msg->Message, msg->Title, msg->Icon);
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
	else if (wParam == SET_PM_TRAY_ICON) // Установка иконки о получении сообщения.
	{
		if (!ClientManager::isBeforeShutdown())
		{
			if (m_bIsPM == false && (!WinUtil::g_isAppActive || g_bAppMinimized)) // [!] InfinitySky. Будет лучше менять иконку при получении сообщения всегда, если эта иконка не установлена и если окно не активно (как в Skype).
			{
				m_bIsPM = true; // Иконка о получении лички установлена.
				
				if (m_taskbarList) // [+] InfinitySky. Если есть поддержка системой taskbarList.
					m_taskbarList->SetOverlayIcon(m_hWnd, *m_pmicon, NULL); // Устанавливается мини-иконка на панели задач о получении сообщения.
					
				if (m_bTrayIcon == true)
					setIcon(*m_pmicon);
			}
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

void MainFrame::parseCommandLine(const tstring& cmdLine)
{
	const string::size_type i = 0;
	string::size_type j;
	tstring l_cmdLine = cmdLine;
	int l_e = cmdLine.size() - 1;
	
	if (!cmdLine.empty() && cmdLine[l_e] == '"')
	{
		string::size_type l_b = cmdLine.rfind('"', l_e - 1);
		if (l_b != tstring::npos)
		{
			l_cmdLine = cmdLine.substr(0, l_e);
			l_cmdLine.erase(l_b, 1);
		}
	}
//[+] FlylinkDC fix

	if ((j = l_cmdLine.find(_T("magnet:?"), i)) != string::npos)
	{
		WinUtil::parseMagnetUri(l_cmdLine.substr(j)); // [1] https://www.box.net/shared/6e7a194cff59e3057d5d
	}
	else if ((j = l_cmdLine.find(_T("dchub://"), i)) != string::npos ||
	         (j = l_cmdLine.find(_T("nmdcs://"), i)) != string::npos ||
	         (j = l_cmdLine.find(_T("adc://"), i)) != string::npos ||
	         (j = l_cmdLine.find(_T("adcs://"), i)) != string::npos)
	{
		WinUtil::parseDchubUrl(l_cmdLine.substr(j));
	}
	// H:\Projects\flylinkdc5xx\compiled\flylinkdc_Debug.exe /open "H:\Torrent\boilsoft video splitter 5.21.dcls"
	static const tstring openKey = _T("/open ");
	if ((j = l_cmdLine.find(openKey, i)) != string::npos) // [+] SSA dclst support
	{
		// get file, and view it
		const tstring fileName = cmdLine.substr(j + openKey.length());
		const tstring openFileName = WinUtil::getFilenameFromString(fileName);
		if (File::isExist(openFileName))
		{
			// [!] IRainman fix: don't use long path here. File and FileFindIter classes is auto correcting path string.
			WinUtil::OpenFileList(openFileName);
			/*
			AutoArray<TCHAR> Buf(FULL_MAX_PATH);
			tstring longOpenFileName = openFileName;
			DWORD resSize = ::GetLongPathName(openFileName.c_str(), Buf, FULL_MAX_PATH - 1);
			if (resSize && resSize <= FULL_MAX_PATH)
			    longOpenFileName = Buf;
			WinUtil::OpenFileList(longOpenFileName);
			*/
		}
	}
	static const tstring sharefolder = _T("/share ");
	if ((j = l_cmdLine.find(sharefolder, i)) != string::npos) // [+] SSA
	{
		// get file, and share it
		const tstring fileName = l_cmdLine.substr(j + sharefolder.length());
		const tstring shareFolderName = WinUtil::getFilenameFromString(fileName);
		if (File::isExist(shareFolderName))
		{
			// [!] IRainman fix: don't use long path here. File and FileFindIter classes is auto correcting path string.
			AddFolderShareFromShell(shareFolderName);
			/*
			AutoArray<TCHAR> Buf(FULL_MAX_PATH);
			tstring longOpenFileName = shareFolderName;
			DWORD resSize = ::GetLongPathName(shareFolderName.c_str(), Buf, FULL_MAX_PATH - 1);
			if (resSize && resSize <= FULL_MAX_PATH)
			    longOpenFileName = Buf;
			AddFolderShareFromShell(longOpenFileName);
			*/
		}
	}
}

LRESULT MainFrame::onCopyData(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	// [~]SSA - JetAudioControl
	if (m_jaControl.get()->ProcessCopyData((PCOPYDATASTRUCT) lParam))
	{
		return true;
	}
	
	if (!getPassword())
		return false;

	const tstring cmdLine = (LPCTSTR)(((COPYDATASTRUCT *)lParam)->lpData);
	if (IsIconic())
	{
		if (!Util::isTorrentLink(cmdLine))
			ShowWindow(SW_RESTORE);
	}
	parseCommandLine(Util::getModuleFileName() + L' ' + cmdLine);
	return true;
}

LRESULT MainFrame::onHashProgress(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HashProgressDlg(false).DoModal(m_hWnd);
	return 0;
}

LRESULT MainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AboutDlgIndex dlg;
	dlg.DoModal(m_hWnd);
	return 0;
}

LRESULT MainFrame::onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
#ifdef FLYLINKDC_USE_CUSTOM_MENU
		// [+]  SSA: Custom menu support.
		if (wID >= IDC_CUSTOM_MENU && wID <= IDC_CUSTOM_MENU100)
		{
			const string& strURL = CustomMenuManager::getInstance()->GetUrlByID(wID - IDC_CUSTOM_MENU);
			if (!strURL.empty())
			{
				WinUtil::openLink(Text::toT(strURL));
			}
		}
		else // [~]  SSA: Custom menu support.
#endif
			switch (wID)
			{
				case ID_FILE_SEARCH:
					SearchFrame::openWindow();
					break;
				case ID_FILE_CONNECT:
					PublicHubsFrame::openWindow();
					break;
				case IDC_FAVORITES:
					FavoriteHubsFrame::openWindow();
					break;
				case IDC_FAVUSERS:
					UsersFrame::openWindow();
					break;
				case IDC_NOTEPAD:
					NotepadFrame::openWindow();
					break;
				case IDC_QUEUE:
					QueueFrame::openWindow();
					break;
				case IDC_SEARCH_SPY:
					SpyFrame::openWindow();
					break;
				case IDC_FILE_ADL_SEARCH:
					ADLSearchFrame::openWindow();
					break;
#ifdef FLYLINKDC_USE_STATS_FRAME
				case IDC_NET_STATS:
					StatsFrame::openWindow();
					break;
#endif
				case IDC_FINISHED:
					FinishedFrame::openWindow();
					break;
				case IDC_FINISHED_UL:
					FinishedULFrame::openWindow();
					break;
				case IDC_UPLOAD_QUEUE:
					WaitingUsersFrame::openWindow();
					break;
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
				case IDC_CDMDEBUG_WINDOW:
					CDMDebugFrame::openWindow();
					break;
#endif
				case IDC_RECENTS:
					RecentHubsFrame::openWindow();
					break;
#ifdef IRAINMAN_INCLUDE_RSS
				case IDC_RSS:
					RSSNewsFrame::openWindow();
					break;
#endif
				default:
					dcassert(0);
					break;
			}
	return 0;
}

LRESULT MainFrame::OnFileSettings(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!PropertiesDlg::g_is_create)
	{
		HICON icon = smallImages.GetIcon(15);
		PropertiesDlg dlg(m_hWnd, icon);
		
		bool prevAutoDetect = BOOLSETTING(AUTO_DETECT_CONNECTION);
		int prevConn = SETTING(INCOMING_CONNECTIONS);
		int prevTCPPort = SETTING(TCP_PORT);
		int prevUDPPort = SETTING(UDP_PORT);
		int prevTLSPort = CryptoManager::TLSOk()? SETTING(TLS_PORT) : -1;
		string prevBindAddr = SETTING(BIND_ADDRESS);
		string prevMapper = SETTING(MAPPER);

		bool prevSortFavUsersFirst = BOOLSETTING(SORT_FAVUSERS_FIRST);
		bool prevRegisterURLHandler = BOOLSETTING(REGISTER_URL_HANDLER);
		bool prevRegisterMagnetHandler = BOOLSETTING(REGISTER_MAGNET_HANDLER);
		bool prevRegisterDCLSTHandler = BOOLSETTING(REGISTER_DCLST_HANDLER);		
		
		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			SettingsManager::getInstance()->save();
			m_transferView.setButtonState();
			if (m_is_missedAutoConnect && !SETTING(NICK).empty())
				PostMessage(WM_SPEAKER_AUTO_CONNECT, 0);

			bool currentAutoDetect = BOOLSETTING(AUTO_DETECT_CONNECTION);
			int currentConn = SETTING(INCOMING_CONNECTIONS);
			int currentTCPPort = SETTING(TCP_PORT);
			int currentUDPPort = SETTING(UDP_PORT);
			int currentTLSPort = CryptoManager::TLSOk()? SETTING(TLS_PORT) : -1;
			string currentBindAddr = SETTING(BIND_ADDRESS);
			string currentMapper = SETTING(MAPPER);

			if (currentAutoDetect != prevAutoDetect ||
			    currentConn != prevConn ||
			    currentTCPPort != prevTCPPort ||
			    currentUDPPort != prevUDPPort ||
			    currentTLSPort != prevTLSPort ||
			    currentBindAddr != prevBindAddr ||
			    currentMapper != prevMapper)
			{
				ConnectivityManager::getInstance()->setupConnections();
			}
			                                                      
			if (BOOLSETTING(SORT_FAVUSERS_FIRST) != prevSortFavUsersFirst)
				HubFrame::resortUsers();
				
			if (BOOLSETTING(REGISTER_URL_HANDLER) != prevRegisterURLHandler)
			{
				if (BOOLSETTING(REGISTER_URL_HANDLER))
				{
					WinUtil::registerDchubHandler();
					WinUtil::registerNMDCSHandler();
					WinUtil::registerADChubHandler();
					WinUtil::registerADCShubHandler();
					WinUtil::urlDcADCRegistered = true;
				}
				else if (WinUtil::urlDcADCRegistered)
				{
					WinUtil::unRegisterDchubHandler();
					WinUtil::unRegisterNMDCSHandler();
					WinUtil::unRegisterADChubHandler();
					WinUtil::unRegisterADCShubHandler();
					WinUtil::urlDcADCRegistered = false;
				}
			}
			
			if (BOOLSETTING(REGISTER_MAGNET_HANDLER) != prevRegisterMagnetHandler)
			{
				if (BOOLSETTING(REGISTER_MAGNET_HANDLER))
				{
					WinUtil::registerMagnetHandler();
					WinUtil::urlMagnetRegistered = true;
				}
				else if (WinUtil::urlMagnetRegistered)
				{
					WinUtil::unRegisterMagnetHandler();
					WinUtil::urlMagnetRegistered = false;
				}
			}

			if (BOOLSETTING(REGISTER_DCLST_HANDLER) != prevRegisterDCLSTHandler)
			{
				if (BOOLSETTING(REGISTER_DCLST_HANDLER))
				{
					WinUtil::registerDclstHandler();
					WinUtil::DclstRegistered = true;
				}
				else if (WinUtil::DclstRegistered)
				{
					WinUtil::unRegisterDclstHandler();
					WinUtil::DclstRegistered = false;
				}
			}
			
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
				SetWindowText(getFlylinkDCAppCaptionWithVersionT().c_str());
			
			// TODO move this call to kernel.
			ClientManager::infoUpdated(true); // Для fly-server шлем принудительно
		}
		else
		{
			m_transferView.setButtonState();
		}
		DestroyIcon(icon);
	}
	return 0;
}

#ifdef IRAINMAN_IP_AUTOUPDATE
void MainFrame::getIPupdate()
{
	string l_external_ip;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	std::vector<unsigned short> l_udp_port, l_tcp_port;
	static bool g_is_first;
	if (g_is_first == false)
	{
		g_is_first = true;
		if (!BOOLSETTING(AUTO_DETECT_CONNECTION))
		{
			l_udp_port.push_back(SETTING(UDP_PORT));
			l_tcp_port.push_back(SETTING(TCP_PORT));
			l_tcp_port.push_back(SETTING(TLS_PORT));
		}
	}
	bool l_is_udp_port_send = CFlyServerJSON::pushTestPort(l_udp_port, l_tcp_port, l_external_ip, SETTING(IPUPDATE_INTERVAL), "Get external IP");
	if (l_is_udp_port_send && !l_external_ip.empty())
	{
		if (!BOOLSETTING(WAN_IP_MANUAL))
		{
			SET_SETTING(EXTERNAL_IP, l_external_ip);
			LogManager::message(STRING(IP_AUTO_UPDATE) + ' ' + l_external_ip + " ");
		}
	}
	else
#endif
	{
		if (!BOOLSETTING(WAN_IP_MANUAL))
		{
			const auto& l_url = SETTING(URL_GET_IP);
			if (Util::isHttpLink(l_url))
			{
				l_external_ip = Util::getWANIP(l_url);
				if (!l_external_ip.empty())
				{
					SET_SETTING(EXTERNAL_IP, l_external_ip);
					LogManager::message(STRING(IP_AUTO_UPDATE) + ' ' + l_external_ip);
				}
				else
				{
					LogManager::message("Error IP AutoUpdate from URL: " + l_url);
				}
			}
			else
			{
				LogManager::message("Error IP AutoUpdate. Invalid URL: " + l_url); // TODO translate
			}
		}
	}
}
#endif
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

void MainFrame::autoConnect(const FavoriteHubEntry::List& fl)
{
//    PROFILE_THREAD_SCOPED()
	const bool l_settingsNickExist = !SETTING(NICK).empty();
	m_is_missedAutoConnect = false;
	CFlyLockWindowUpdate l(WinUtil::g_mdiClient);
	HubFrame* frm_current = nullptr;
	HubFrame* frm_last = nullptr;
	{
#if 0 // ???
		int l_count_sec = 0;
		while (ConnectionManager::g_is_test_tcp_port == false ||
		        ConnectionManager::g_is_test_tcp_port == true && CFlyServerJSON::isTestPortOK(SETTING(TCP_PORT), "tcp") == false)
		{
			if (++l_count_sec > 50)
			{
				break;
			}
			sleep(100);
		}
#endif
		extern bool g_isStartupProcess;
		CFlyBusyBool l_busy_1(g_isStartupProcess);
		for (auto i = fl.cbegin(); i != fl.cend(); ++i)
		{
			const FavoriteHubEntry* entry = *i;
			if (entry->getConnect())
			{
				if (!entry->getNick().empty() || l_settingsNickExist)
				{
					RecentHubEntry r;
					r.setName(entry->getName());
					r.setDescription(entry->getDescription());
					r.setServer(entry->getServer());
					auto l_resent_hub = FavoriteManager::getInstance()->addRecent(r);
					if (l_resent_hub)
					{
						l_resent_hub->setAutoOpen(true);
					}
					frm_current = HubFrame::openHubWindow(entry->getServer(),
					                                      entry->getName(),
					                                      entry->getRawOne(),
					                                      entry->getRawTwo(),
					                                      entry->getRawThree(),
					                                      entry->getRawFour(),
					                                      entry->getRawFive(),
					                                      entry->getWindowPosX(),
					                                      entry->getWindowPosY(),
					                                      entry->getWindowSizeX(),
					                                      entry->getWindowSizeY(),
					                                      entry->getWindowType(),
					                                      entry->getChatUserSplit(),
					                                      entry->getUserListState(),
					                                      entry->getSuppressChatAndPM()
					                                     );
					if (frm_current)
					{
						frm_last = frm_current;
					}
				}
				else
				{
					m_is_missedAutoConnect = true;
				}
			}
		}
		// Откроем ранее открытые хабы, но не помещенные в избранные
		if (BOOLSETTING(OPEN_RECENT_HUBS))
		{
			for (auto j = FavoriteManager::getRecentHubs().cbegin(); j != FavoriteManager::getRecentHubs().cend(); ++ j)
			{
				if ((*j)->getAutoOpen() == false && (*j)->getOpenTab() == "+")
				{
					frm_current = HubFrame::openHubWindow((*j)->getServer(), (*j)->getName());
					if (frm_current)
						frm_last = frm_current;
						
				}
			}
		}
		// Создаем смайлы в конец
#ifdef IRAINMAN_INCLUDE_SMILE
		CAGEmotionSetup::reCreateEmotionSetup();
#endif
	}
	UpdateLayout(true);
	if (frm_last)
	{
		frm_last->createMessagePanel();
	}
	if (!PopupManager::isValidInstance())
	{
		PopupManager::newInstance();
	}
}

void MainFrame::updateTray(bool add /* = true */)
{
	if (m_normalicon) //[+]PPA
	{
		if (add)
		{
			if (!m_bTrayIcon)
			{
				NOTIFYICONDATA nid = { 0 };
				nid.cbSize = sizeof(NOTIFYICONDATA);
				nid.hWnd = m_hWnd;
				nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
				nid.uCallbackMessage = WM_APP + 242;// TODO отрефакторить! это источник потенциальных ошибок!
				nid.hIcon = *m_normalicon;
				_tcsncpy(nid.szTip, getFlylinkDCAppCaptionT().c_str(), 64);
				nid.szTip[63] = '\0';
				m_lastMove = GET_TICK() - 1000;
				m_bTrayIcon = ::Shell_NotifyIcon(NIM_ADD, &nid) != FALSE;// [~] InfinitySky. Code from Apex 1.3.8.
			}
		}
		else
		{
			if (m_bTrayIcon)
			{
				NOTIFYICONDATA nid = { 0 };
				nid.cbSize = sizeof(NOTIFYICONDATA);
				nid.hWnd = m_hWnd;
				::Shell_NotifyIcon(NIM_DELETE, &nid);
				m_bTrayIcon = false;
			}
		}
	}
}
void MainFrame::SetOverlayIcon()
{
	if (m_taskbarList) // Если есть поддержка системой taskbarList.
	{
		m_taskbarList->SetOverlayIcon(m_hWnd, *m_emptyicon, NULL); // [!] IRainman. Прозрачная пустая иконка.
	}
}

void MainFrame::setTrayAndTaskbarIcons() // [+] IRainman: copy-past fix.
{
	if (m_bIsPM/*[-] IRainman && m_normalicon*/) // Если иконка о получении лички была установлена.
	{
		m_bIsPM = false; // Иконка о получении лички не установлена.
		SetOverlayIcon();
		if (m_bTrayIcon == true)
		{
			setIcon(*m_normalicon);
		}
	}
}

void setAwayByMinimized() // [+] IRainman fix.
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
	// [!] IRainman fix.
	if (wParam == SIZE_MINIMIZED)
	{
		if (!g_bAppMinimized)
		{
			g_bAppMinimized = true;
			if (BOOLSETTING(MINIMIZE_TRAY) != WinUtil::isShift())
			{
				ShowWindow(SW_HIDE);
				if (BOOLSETTING(REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY))
					CompatibilityManager::reduceProcessPriority();
			}
			setAwayByMinimized();
		}
		m_is_maximized = IsZoomed() > 0;
	}
	else if ((wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED))
	{
		if (g_bAppMinimized)
		{
			g_bAppMinimized = false;
			setAwayByMinimized();
			CompatibilityManager::restoreProcessPriority();
			setTrayAndTaskbarIcons();
		}
		if (wParam == SIZE_RESTORED)
		{
			if (SETTING(MAIN_WINDOW_STATE) == SW_SHOWMAXIMIZED)
			{
				//SET_SETTING(MAIN_WINDOW_STATE, SW_SHOWMAXIMIZED);
				//ShowWindow(SW_SHOWMAXIMIZED);
			}
		}
	}
	// [~] IRainman fix.
	bHandled = FALSE;
	return 0;
}

void MainFrame::storeWindowsPos()
{
	if (m_is_start_autoupdate == false)
	{
		WINDOWPLACEMENT wp = { 0 };
		wp.length = sizeof(wp);
		if (GetWindowPlacement(&wp))
		{
			// Состояние окна отдельно от координат! Там мы могли не попадать в условие, при MAXIMIZED
			if (wp.showCmd == SW_SHOWNORMAL || wp.showCmd == SW_SHOW ||
			        wp.showCmd == SW_SHOWMAXIMIZED || wp.showCmd == SW_SHOWMINIMIZED ||
			        wp.showCmd == SW_MAXIMIZE)
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
			if (GetWindowRect(rc) && (wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWNORMAL))
			{
				SET_SETTING(MAIN_WINDOW_POS_X, rc.left);
				SET_SETTING(MAIN_WINDOW_POS_Y, rc.top);
				SET_SETTING(MAIN_WINDOW_SIZE_X, rc.Width());
				SET_SETTING(MAIN_WINDOW_SIZE_Y, rc.Height());
			}
#ifdef _DEBUG
			else {
				dcdebug("MainFrame:: WINDOW  GetWindowRect(rc) -> NULL rc OR is SW_MAXIMIZED, SW_MINIMIZED, etc...\n");
				// Вывести окно с этим сообщением
			}
#endif
		}
#ifdef _DEBUG
		else {
			dcdebug("MainFrame:: WINDOW  GetWindowPlacement(&wp) -> NULL data !!!\n");
			// вывести окно с этим сообщением
		}
#endif
	}
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
	m_is_end_session = true;
	FavoriteManager::saveFavorites();
	//SettingsManager::getInstance()->save();
	return 0;
}

// При закрытии.
LRESULT MainFrame::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// [+] InfinitySky. Выбор между сворачиванием и закрытием.
	if (BOOLSETTING(MINIMIZE_ON_CLOSE) && !m_menuclose) // [+] InfinitySky. Сворачивать при закрытии, если выбрана эта опция в настройках и закрытие не через меню.
	{
		ShowWindow(SW_MINIMIZE);
	}
	else
	{
		if (!m_closing)   // [+] SSA
		{
#ifdef _DEBUG
			dcdebug("MainFrame::OnClose first - User::g_user_counts = %d\n", int(User::g_user_counts)); // [!] IRainman fix: Issue 1037 иногда теряем объект User?
#endif
			m_stopexit = false;
			if (SETTING(PROTECT_CLOSE) && !m_oldshutdown && !m_is_end_session && SETTING(PASSWORD) != g_magic_password && !SETTING(PASSWORD).empty())
			{
				INT_PTR l_do_modal_result;
				if (getPasswordInternal(l_do_modal_result) == false)
				{
					m_stopexit = true;
					m_menuclose = false;
				}
				else
				{
					if (l_do_modal_result == IDOK)
						m_stopexit = false;
					else
						m_stopexit = true;
				}
			}
			
			bool bForceNoWarning = false;
			
			if (!m_stopexit)
			{
				// [+] brain-ripper
				// check if hashing pending,
				// and display hashing progress
				// if any
				
				if (HashManager::getInstance()->IsHashing())
				{
					bool bForceStopExit = false;
					if (!HashProgressDlg::g_is_execute)
					{
						bForceStopExit = HashProgressDlg(true, true).DoModal() != IDC_BTN_EXIT_ON_DONE;
					}
					
					// User take decision in dialog,
					//so avoid one more warning message
					bForceNoWarning = true;
					
					if (HashManager::getInstance()->IsHashing() || bForceStopExit)
					{
						// User closed progress window, while hashing still in progress,
						// or user unchecked "exit on done" checkbox,
						// so let program to work
						
						m_stopexit = true;
						m_menuclose = false; // [+] InfinitySky. Отключаем метку закрытия через меню, на случай, если в окне предупреждения о закрытии будет отмена закрытия.
					}
				}
			}
			
			UINT checkState = BOOLSETTING(CONFIRM_EXIT) ? BST_CHECKED : BST_UNCHECKED;
			
			if ((m_oldshutdown || m_is_end_session ||
			     SETTING(PROTECT_CLOSE) ||
			     checkState == BST_UNCHECKED ||
			     (bForceNoWarning ||
			     MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_EXIT), getFlylinkDCAppCaptionWithVersionT().c_str(),
			                         CTSTRING(ALWAYS_ASK), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES))
			     && !m_stopexit)
			{
				{
					storeWindowsPos();
					//BOOL l_res = ::DestroyIcon(m_appIcon);
					//auto l_error = GetLastError();
					//dcassert(l_res);
					//l_res = ::DestroyIcon(m_trayIcon);
					//dcassert(l_res);
					//l_error = GetLastError();
					
					ClientManager::before_shutdown();
					LogManager::g_mainWnd = nullptr;
					m_closing = true;
					destroyTimer();
					ClientManager::stopStartup();
					NmdcHub::log_all_unknown_command();
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
					CFlyTickDelta l_delta(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_SHUTDOWN_GUI]);
#endif
					preparingCoreToShutdown(); // [!] IRainman fix.
					
					m_transferView.prepareClose();
					//dcassert(TransferView::ItemInfo::g_count_transfer_item == 0);
					
					WebServerManager::getInstance()->removeListener(this);
					UserManager::getInstance()->removeListener(this); // [+] IRainman
					QueueManager::getInstance()->removeListener(this);
					
					ConnectionManager::getInstance()->disconnect();
					
					ToolbarManager::getFrom(m_hWndToolBar, "MainToolBar"); // Для сохранения позиций тулбара (SCALOlаz)
					
					updateTray(false);
					if (m_nProportionalPos > 300)
					{
						SET_SETTING(TRANSFER_FRAME_SPLIT, m_nProportionalPos);
						// FIXME: Flylink was setting a registry value here...
					}
					ShowWindow(SW_HIDE);
				}
				m_stopperThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, &stopper, this, 0, nullptr));
				
			}
			else
			{
				m_menuclose = false; // [+] InfinitySky. Отключаем метку закрытия через меню, на случай, если в окне предупреждения о закрытии будет отмена закрытия.
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
			WaitForSingleObject(m_stopperThread, 60 * 1000);
			CloseHandle(m_stopperThread);
			m_stopperThread = nullptr;
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
		HICON icon = smallImages.GetIcon(36);
		FileHashDlg dlg(icon);
		dlg.filename = std::move(file);
		dlg.lastDir = lastTTHdir;
		dlg.DoModal();
		lastTTHdir = std::move(dlg.lastDir);
		DestroyIcon(icon);
	}
	return 0;
}

void MainFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (ClientManager::isStartup() == false)
	{
		RECT l_rect;
		GetClientRect(&l_rect);
		// position bars and offset their dimensions
		UpdateBarsPosition(l_rect, bResizeBars);
		
		if (ctrlStatus.IsWindow() && ctrlLastLines.IsWindow())
		{
			CRect sr;
			int w[STATUS_PART_LAST];
			
			bool bIsHashing = HashManager::getInstance()->IsHashing();
			ctrlStatus.GetClientRect(sr);
			if (bIsHashing)
			{
				w[STATUS_PART_HASH_PROGRESS] = sr.right - 20;
				w[STATUS_PART_SHUTDOWN_TIME] = w[STATUS_PART_HASH_PROGRESS] - 60;
			}
			else
				w[STATUS_PART_SHUTDOWN_TIME] = sr.right - 20;
				
			w[STATUS_PART_8] = w[STATUS_PART_SHUTDOWN_TIME] - 60;
#define setw(x) w[x] = max(w[x+1] - m_statusSizes[x], 0)
			setw(STATUS_PART_7);
			setw(STATUS_PART_UPLOAD);
			setw(STATUS_PART_DOWNLOAD);
			setw(STATUS_PART_SLOTS);
			setw(STATUS_PART_3);
			setw(STATUS_PART_SHARED_SIZE);
			setw(STATUS_PART_1);
			setw(STATUS_PART_MESSAGE);
			
			ctrlStatus.SetParts(STATUS_PART_LAST - 1 + (bIsHashing ? 1 : 0), w);
			ctrlLastLines.SetMaxTipWidth(max(w[4], 400));
			
			if (bIsHashing)
			{
				RECT rect;
				ctrlStatus.GetRect(STATUS_PART_HASH_PROGRESS, &rect);
				
				rect.right = w[STATUS_PART_HASH_PROGRESS] - 1;
				ctrlHashProgress.MoveWindow(&rect);
			}
			
			//tabDHTRect.right -= 2;
			ctrlStatus.GetRect(STATUS_PART_1, &m_tabAWAYRect);    // Get AWAY Area Rect
			
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
			ctrlStatus.GetRect(STATUS_PART_7, &tabSPEED_INRect);
			ctrlStatus.GetRect(STATUS_PART_8, &tabSPEED_OUTRect);
#endif
		}
		CRect rc  = l_rect;
		CRect rc2 = l_rect;
		
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
	tstring l_file;
	static tstring g_last_torrent_file;
#ifdef FLYLINKDC_USE_TORRENT
	if (wID == IDC_OPEN_TORRENT_FILE)
	{
		if (WinUtil::browseFile(l_file, m_hWnd, false, g_last_torrent_file, L"All torrent file\0*.torrent\0\0"))
		{
			g_last_torrent_file = Util::getFilePath(l_file);
			DownloadManager::getInstance()->add_torrent_file(l_file, _T(""));
		}
		return 0;
	}
#endif
	if (wID == IDC_OPEN_MY_LIST)
	{
		const string& l_own_list_file = ShareManager::getInstance()->getOwnListFile();
		if (!l_own_list_file.empty())
		{
			DirectoryListingFrame::openWindow(Text::toT(l_own_list_file), Util::emptyStringT, HintedUser(ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks(), Util::emptyString), 0);
		}
		return 0;
	}
	
	if (WinUtil::browseFile(l_file, m_hWnd, false, Text::toT(Util::getListPath()), g_file_list_type))//FILE_LIST_TYPE.c_str()))
	{
		if (Util::isTorrentFile(l_file))
		{
			g_last_torrent_file = Util::getFilePath(l_file);
#ifdef FLYLINKDC_USE_TORRENT
			DownloadManager::getInstance()->add_torrent_file(l_file, _T(""));
#endif
		}
		else
		{
			WinUtil::OpenFileList(l_file); // [!] SSA dclst support.
			// UserPtr u = DirectoryListing::getUserFromFilename(Text::fromT(file));
			//if (u)
			//{
			//  DirectoryListingFrame::openWindow(file, HintedUser(u, Util::emptyString), 0, Util::isDclstFile(file));
			//}
			//else
			//{
			//  // [!] IRainman Support broken file lists and non-standard formats like that dcls
			//  //if (
			//  MessageBox(CTSTRING(INVALID_LISTNAME), getFlylinkDCAppCaptionWithVersionT().c_str());
			//}
		}
	}
	return 0;
}

#if 0
//LRESULT MainFrame::onFlylinkDiscover(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//{
//	ShellExecute(NULL, NULL, _T("FlylinkDiscover.exe"), NULL, NULL, SW_SHOWNORMAL);
//	return 0;
//}
// Эта функция выполняется в отдельном потоке
struct ThreadParams
{
};

static DWORD WINAPI converttthHistoryThreadFunc(void* params)
{
	// Получаем экземпляр класса ProgressParam<ThreadParams>,
	// который хранит указатель на IProgress и на параметры
	ProgressParam<ThreadParams> *pp = (ProgressParam<ThreadParams>*)params;
	
	// Получаем параметры, но в данном случае ничего с ними не делаем
	// ThreadParams* threadParams = (ThreadParams*)pp->pParams;
	
	// Устанавливаем диапазон изменения индикатора прогресса
	pp->pProgress->SetProgressRange(0, 100, 0);
	CFlyLog l_log("[Convert TTH History]");
	const auto l_count = CFlylinkDBManager::getInstance()->convert_tth_history();
	l_log.step(" Count: " + Util::toString(l_count));
	// Выполянем долгий цикл
	/*
	for (int i = 0; i < 100; ++i)
	{
	    Sleep (100);
	    pp->pProgress->SetProgress(i + 1);
	}
	*/
	return 0;
}

LRESULT MainFrame::onConvertTTHHistory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ThreadParams l_params;
	CProgressDlg <ThreadParams, IDD_FLY_PROGRESS, IDC_TIME > l_dlg(converttthHistoryThreadFunc, &l_params, CWSTRING(PLEASE_WAIT));
	l_dlg.Start();
	return 0;
}
#endif

LRESULT MainFrame::onRefreshFileListPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ShareManager::getInstance()->setDirty();
	ShareManager::getInstance()->setPurgeTTH();
	ShareManager::getInstance()->refresh_share(true);
	LogManager::message(STRING(PURGE_TTH_DATABASE));
	return 0;
}

LRESULT MainFrame::onRefreshFileList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ShareManager::getInstance()->setDirty();
	ShareManager::getInstance()->refresh_share(true);
	return 0;
}

bool MainFrame::getPasswordInternal(INT_PTR& p_do_modal_result)
{
	LineDlg dlg;
	dlg.description = TSTRING(PASSWORD_DESC);
	dlg.title = TSTRING(PASSWORD_TITLE);
	dlg.password = true;
	dlg.disabled = true;
	p_do_modal_result = dlg.DoModal(m_hWnd);
	if (p_do_modal_result == IDOK)
	{
		tstring tmp = dlg.line;
		TigerTree mytth(TigerTree::calcBlockSize(tmp.size(), 1));
		mytth.update(tmp.c_str(), tmp.size());
		mytth.finalize();
		return Text::toT(mytth.getRoot().toBase32().c_str()) == Text::toT(SETTING(PASSWORD));
	}
	else
		return false;
}

bool MainFrame::getPassword()
{
	if (m_is_maximized || !SETTING(PROTECT_TRAY) || SETTING(PASSWORD) == g_magic_password || SETTING(PASSWORD).empty())
		return true;
	INT_PTR l_do_modal_result;
	if (!getPasswordInternal(l_do_modal_result))
	{
		return false;
	}
	else
	{
		return true;
	}
	
}

LRESULT MainFrame::onTrayIcon(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (lParam == WM_LBUTTONUP)
	{
		if (g_bAppMinimized)
		{
			if (getPassword())
			{
				ShowWindowMax();
			}
		}
		else
		{
			//SetForegroundWindow(m_hWnd);// [-] Drakon
			ShowWindow(SW_MINIMIZE);// [+] Drakon
		}
	}
	else if (lParam == WM_MOUSEMOVE && ((m_lastMove + 1000) < GET_TICK()))
	{
		NOTIFYICONDATA nid = {0};
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = m_hWnd;
		nid.uFlags = NIF_TIP;
		_tcsncpy(nid.szTip, (
		             TSTRING(D) + _T(' ') + Util::formatBytesW(g_downdiff) + _T('/') + WSTRING(S) + _T(" (") +
		             Util::toStringW(DownloadManager::getDownloadCount()) + _T(")\r\n") +
		             TSTRING(U) + _T(' ') + Util::formatBytesW(g_updiff) + _T('/') + WSTRING(S) + _T(" (") +
		             Util::toStringW(UploadManager::getUploadCount()) + _T(")") + _T("\r\n") +
		             TSTRING(UPTIME) + _T(' ') + Util::formatSecondsW(Util::getUpTime())
		         ).c_str(), 63);
		         
		::Shell_NotifyIcon(NIM_MODIFY, &nid);
		m_lastMove = GET_TICK();
	}
	else if (lParam == WM_RBUTTONUP)
	{
		CPoint pt(GetMessagePos());
		SetForegroundWindow(m_hWnd);
		if ((!SETTING(PROTECT_TRAY) || !g_bAppMinimized) || (SETTING(PASSWORD) == g_magic_password || SETTING(PASSWORD).empty()))
			trayMenu.TrackPopupMenu(TPM_RIGHTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		PostMessage(WM_NULL, 0, 0);
	}
	return 0;
}

LRESULT MainFrame::onTaskbarButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)// [+] InfinitySky. Taskbar buttons on Win7. From ApexDC++.
{
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
	if (!CompatibilityManager::isWin7Plus())
		return 0;
#endif
		
	m_taskbarList.Release();
	if (m_taskbarList.CoCreateInstance(CLSID_TaskbarList) == S_OK)
	{
		if (m_normalicon)
		{
			SetOverlayIcon();
		}
		else
		{
			dcassert(0);
		}
	}
	THUMBBUTTON buttons[3]; // Количество кнопок.
	const int l_sizeTip = _countof(buttons[0].szTip) - 1;
	buttons[0].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
	buttons[0].iId = IDC_OPEN_DOWNLOADS;
	buttons[0].hIcon = smallImages.GetIcon(22);
	wcsncpy(buttons[0].szTip, CWSTRING(MENU_OPEN_DOWNLOADS_DIR), l_sizeTip);
	buttons[0].dwFlags = THBF_ENABLED;
	
	buttons[1].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
	buttons[1].iId = ID_FILE_SETTINGS;
	buttons[1].hIcon = smallImages.GetIcon(15);
	wcsncpy(buttons[1].szTip, CWSTRING(SETTINGS), l_sizeTip);
	buttons[1].dwFlags = THBF_ENABLED;
	
	buttons[2].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
	buttons[2].iId = IDC_REFRESH_FILE_LIST;
	buttons[2].hIcon = smallImages.GetIcon(23);
	wcsncpy(buttons[2].szTip, CWSTRING(CMD_SHARE_REFRESH), l_sizeTip);
	buttons[2].dwFlags = THBF_ENABLED;
	if (m_taskbarList)
		m_taskbarList->ThumbBarAddButtons(m_hWnd, _countof(buttons), buttons);
		
	for (size_t i = 0; i < _countof(buttons); ++i)
		DestroyIcon(buttons[i].hIcon);
		
	return 0;
}

LRESULT MainFrame::onAppShow(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/)
{
	if (::IsIconic(m_hWnd))
	{
		if (!m_is_maximized && SETTING(PROTECT_TRAY) && SETTING(PASSWORD) != g_magic_password && !SETTING(PASSWORD).empty())
		{
			const auto l_title_name = _T("Password required - FlylinkDC++");
			const HWND otherWnd = FindWindow(NULL, l_title_name);
			if (otherWnd == NULL)
			{
				LineDlg dlg;
				dlg.description = TSTRING(PASSWORD_DESC);
				dlg.title = l_title_name;
				dlg.password = true;
				dlg.disabled = true;
				if (dlg.DoModal(/*m_hWnd*/) == IDOK)
				{
					tstring tmp = dlg.line;
					TigerTree mytth(TigerTree::calcBlockSize(tmp.size(), 1));
					mytth.update(tmp.c_str(), tmp.size());
					mytth.finalize();
					if (mytth.getRoot().toBase32().c_str() == SETTING(PASSWORD))
					{
						ShowWindowMax();
					}
				}
			}
			else
			{
				::SetForegroundWindow(otherWnd);
			}
		}
		else
		{
			ShowWindowMax();
		}
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
		p->Title = title;
		p->Message = message;
		p->Icon = infoFlags;
		safe_post_message(*getMainFrame(), SHOW_POPUP_MESSAGE, p);
	}
}

LRESULT MainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;    // initially visible
	bVisible = !bVisible;
	CReBarCtrl l_rebar = m_hWndToolBar;
	int nBandIndex = l_rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);   // toolbar is 2nd added band
	l_rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_TOOLBAR, bVisible);
	ctrlToolbar.CheckButton(ID_VIEW_TOOLBAR, bVisible); // [+] InfinitySky.
	return 0;
}

LRESULT MainFrame::OnViewWinampBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;    // initially visible
	bVisible = !bVisible;
	CReBarCtrl l_rebar = m_hWndToolBar;
	int nBandIndex = l_rebar.IdToIndex(ATL_IDW_BAND_FIRST + 3);   // toolbar is 3nd added band
	l_rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_TOGGLE_TOOLBAR, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_PLAYER_CONTROLS, bVisible);
	ctrlToolbar.CheckButton(ID_TOGGLE_TOOLBAR, bVisible);
	return 0;
}

LRESULT MainFrame::OnViewQuickSearchBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;    // initially visible
	bVisible = !bVisible;
	CReBarCtrl l_rebar = m_hWndToolBar;
	int nBandIndex = l_rebar.IdToIndex(ATL_IDW_BAND_FIRST + 2);   // toolbar is 4th added band
	l_rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_TOGGLE_QSEARCH, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_QUICK_SEARCH, bVisible);
	return 0;
}

LRESULT MainFrame::OnViewTopmost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UISetCheck(IDC_TOPMOST, !BOOLSETTING(TOPMOST));
	SET_SETTING(TOPMOST, !BOOLSETTING(TOPMOST));
	toggleTopmost();
	return 0;
}

LRESULT MainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	SET_SETTING(SHOW_STATUSBAR, bVisible);
	return 0;
}

void MainFrame::ViewTransferView(BOOL bVisible)
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
	
	ctrlToolbar.CheckButton(ID_VIEW_TRANSFER_VIEW, bVisible); // [+] InfinitySky.
}

LRESULT MainFrame::OnViewTransferView(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const BOOL bVisible = !BOOLSETTING(SHOW_TRANSFERVIEW);
	SET_SETTING(SHOW_TRANSFERVIEW, bVisible);
	ViewTransferView(bVisible);
	return 0;
}

LRESULT MainFrame::OnViewTransferViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const BOOL bVisible = !BOOLSETTING(SHOW_TRANSFERVIEW_TOOLBAR);
	SET_SETTING(SHOW_TRANSFERVIEW_TOOLBAR, bVisible);
	ctrlToolbar.CheckButton(ID_VIEW_TRANSFER_VIEW_TOOLBAR, bVisible);
	UISetCheck(ID_VIEW_TRANSFER_VIEW_TOOLBAR, bVisible);
	m_transferView.UpdateLayout();
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
	onLimiter(); // [!] IRainman fix
	return 0;
}

LRESULT MainFrame::onQuickConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LineDlg dlg;
	dlg.description = TSTRING(HUB_ADDRESS);
	dlg.title = TSTRING(QUICK_CONNECT);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		if (SETTING(NICK).empty())
		{
			MessageBox(CTSTRING(ENTER_NICK), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_ICONSTOP | MB_OK);
			return 0;
		}
		
		tstring tmp = dlg.line;
		// Strip out all the spaces
		string::size_type i;
		while ((i = tmp.find(' ')) != string::npos)
			tmp.erase(i, 1);
			
		if (!tmp.empty())
		{
			RecentHubEntry r;
			const string formattedDcHubUrl = Util::formatDchubUrl(Text::fromT(tmp));
			r.setServer(formattedDcHubUrl);
			FavoriteManager::getInstance()->addRecent(r);
			HubFrame::openHubWindow(formattedDcHubUrl);
		}
	}
	return 0;
}

void MainFrame::on(QueueManagerListener::PartialList, const HintedUser& aUser, const string& text) noexcept
{
	safe_post_message(*this, BROWSE_LISTING, new DirectoryBrowseInfo(aUser, text));
}

void MainFrame::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string& dir, const DownloadPtr& p_download) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (qi->isSet(QueueItem::FLAG_CLIENT_VIEW))
		{
			if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST))
			{
				// This is a file listing, show it...
				auto dirInfo = new QueueManager::DirectoryListInfo(p_download->getHintedUser(), qi->getListName(), dir, p_download->getRunningAverage(), qi->isSet(QueueItem::FLAG_DCLST_LIST));
				safe_post_message(*this, DOWNLOAD_LISTING, dirInfo);
			}
			else if (qi->isSet(QueueItem::FLAG_TEXT))
			{
				safe_post_message(*this, VIEW_FILE_AND_DELETE, new tstring(Text::toT(qi->getTarget())));
			}
		}
		// [+] IRainman support auto open file after download.
		else if (qi->isSet(QueueItem::FLAG_OPEN_FILE))
		{
			WinUtil::openFile(Text::toT(qi->getTarget()));
		}
		// [~] IRainman support auto open file after download.
	}
}

LRESULT MainFrame::onActivateApp(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	WinUtil::g_isAppActive = (wParam == TRUE); // wParam == TRUE if window is activated, FALSE if deactivated
	if (WinUtil::g_isAppActive)
	{
		setTrayAndTaskbarIcons(); // [!] IRainman fix.
	}
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
	onAwayPush();
	return 0;
}

void MainFrame::onAwayPush()
{
	if (Util::getAway())
	{
		setAwayButton(false);
		Util::setAway(false);
	}
	else
	{
		setAwayButton(true);
		Util::setAway(true);
	}
}

LRESULT MainFrame::onFoundNewVersion(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	return 0;
}

#ifdef FLYLINKDC_USE_LOCATION_DIALOG
LRESULT MainFrame::onChangeLocation(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFlyLocationDlg l_dlg;
	if (l_dlg.DoModal() == IDOK)
	{
		if (!l_dlg.isEmpty())
		{
			SET_SETTING(LOCATION_COUNTRY, Text::fromT(l_dlg.m_Country));
			SET_SETTING(LOCATION_CITY, Text::fromT(l_dlg.m_City));
			SET_SETTING(LOCATION_ISP, Text::fromT(l_dlg.m_Provider));
			ClientManager::resend_ext_json();
		}
	}
	return S_OK;
}
#endif

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

LRESULT MainFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (m_bTrayIcon)
	{
		updateTray(false);
	}
	bHandled = FALSE;
	winampMenu.DestroyMenu();
	return 0;
}

void MainFrame::setIcon(HICON newIcon)
{
	NOTIFYICONDATA nid = {0};
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = m_hWnd;
	nid.uFlags = NIF_ICON;
	nid.hIcon = newIcon;
	::Shell_NotifyIcon(NIM_MODIFY, &nid);
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
		if (ptClient.x >= m_tabAWAYRect.left && ptClient.x <= m_tabAWAYRect.right)
		{
			if (GetMenuItemCount(tabAWAYMenu) != -1) // Is valid menulist
			{
				tabAWAYMenu.CheckMenuItem(IDC_STATUS_AWAY_ON_OFF, MF_BYCOMMAND | (SETTING(AWAY) ? MF_CHECKED : MF_UNCHECKED));
				tabAWAYMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}
		}
		
#ifdef SCALOLAZ_SPEEDLIMIT_DLG
		if (ptClient.x >= tabSPEED_INRect.left && ptClient.x <= tabSPEED_INRect.right)
		{
			QuerySpeedLimit(SettingsManager::MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, 0, 6144); // min, max ~6mbit
		}
		
		if (ptClient.x >= tabSPEED_OUTRect.left && ptClient.x <= tabSPEED_OUTRect.right)
		{
			QuerySpeedLimit(SettingsManager::MAX_UPLOAD_SPEED_LIMIT_NORMAL, 0, 6144);   //min, max ~6mbit
		}
		
		if ((ptClient.x >= tabSPEED_INRect.left && ptClient.x <= tabSPEED_INRect.right) || (ptClient.x >= tabSPEED_OUTRect.left && ptClient.x <= tabSPEED_OUTRect.right))
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
	if (ptClient.x >= m_tabAWAYRect.left && ptClient.x <= m_tabAWAYRect.right)
	{
		onAwayPush();
	}
	bHandled = FALSE;
	return 0;
}

#ifdef SCALOLAZ_SPEEDLIMIT_DLG
// Draw slider SpeedLimit for Upload & Download
void MainFrame::QuerySpeedLimit(const SettingsManager::IntSetting l_limit_normal, int l_min_lim, int l_max_lim)
{
	SpeedVolDlg dlg(SettingsManager::get(l_limit_normal), l_min_lim, l_max_lim);
	if (dlg.DoModal() == IDOK)
	{
		const int l_current_limit = dlg.GetLimit();
		if (SettingsManager::get(l_limit_normal) != l_current_limit)
		{
			SettingsManager::set(l_limit_normal, l_current_limit);
			onLimiter(false);
		}
	}
}
#endif

LRESULT MainFrame::OnMenuSelect(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
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

void MainFrame::toggleTopmost() const
{
	DWORD dwExStyle = (DWORD)GetWindowLongPtr(GWL_EXSTYLE);
	CRect rc;
	GetWindowRect(rc);
	HWND order = (dwExStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST;
	::SetWindowPos(m_hWnd, order, rc.left, rc.top, rc.Width(), rc.Height(), SWP_SHOWWINDOW);
}

void MainFrame::toggleLockToolbars() const
{
	CReBarCtrl l_rebar = m_hWndToolBar;
	REBARBANDINFO rbi = {0};
	rbi.cbSize = sizeof(rbi);
	rbi.fMask  = RBBIM_STYLE;
	int nCount = l_rebar.GetBandCount();
	for (int i  = 0; i < nCount; i++)
	{
		l_rebar.GetBandInfo(i, &rbi);
		rbi.fStyle ^= RBBS_NOGRIPPER | RBBS_GRIPPERALWAYS;
		l_rebar.SetBandInfo(i, &rbi);
	}
}

#ifdef IRAINMAN_INCLUDE_SMILE
LRESULT MainFrame::OnAnimChangeFrame(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
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

LRESULT MainFrame::onAddMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // [+]NightOrion
{
	AddMagnet dlg;
	dlg.DoModal(m_hWnd);
	return 0;
}

void MainFrame::on(QueueManagerListener::TryAdding, const string& fileName, int64_t newSize, int64_t existingSize, time_t existingTime, int option) noexcept
{
	CheckTargetDlg dlg(fileName, newSize, existingSize, existingTime, option);
	if (dlg.DoModal(*this) == IDOK)
	{
		option = dlg.GetOption();
		if (dlg.IsApplyForAll())
			SET_SETTING(TARGET_EXISTS_ACTION, option);
	}
	else
		option = SettingsManager::ON_DOWNLOAD_SKIP;
		
	QueueManager::getInstance()->setOnDownloadSetting(option);
}

#ifdef SSA_WIZARD_FEATURE
UINT MainFrame::ShowSetupWizard()
{
	try
	{
		FlyWizard wizard;
		const UINT l_result = wizard.DoModal();
		if (l_result == IDOK)
		{
			ShareManager::getInstance()->setDirty();
			ShareManager::getInstance()->refresh_share(true);
		}
		return l_result;
	}
	catch (Exception & e)
	{
		::MessageBox(NULL, Text::toT(e.getError()).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR); // [1] https://www.box.net/shared/tsdgrjdhgdfjrsz168r7
		return IDCLOSE;
	}
}

LRESULT MainFrame::OnFileSettingsWizard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ShowSetupWizard();
	return TRUE;
}
#endif // SSA_WIZARD_FEATURE

LRESULT MainFrame::OnToolbarDropDown(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
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
		m_CmdBar.TrackPopupMenu(winampMenu, TPM_RIGHTBUTTON | TPM_VERTICAL, pt.x, pt.y);
		
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

void MainFrame::AddFolderShareFromShell(const tstring& infolder)
{
	tstring folder = infolder;
	Util::appendPathSeparator(folder);
	const string l_folder = Text::fromT(folder);
	CFlyDirItemArray directories;
	ShareManager::getDirectories(directories);
	bool bFound = false;
	for (auto j = directories.cbegin(); j != directories.cend(); ++j)
	{
		// Compare with
		if (!l_folder.compare(j->m_path))
		{
			bFound = true;
			break;
		}
	}
	
	if (!bFound)
	{
		// [!] SSA Need to add Dialog Question
		bool shareFolder = true;
		if (BOOLSETTING(CONFIRM_SHARE_FROM_SHELL))
		{
			tstring question = folder;
			question += L"\r\n";
			question += TSTRING(SECURITY_SHARE_FROM_SHELL_QUESTION);
			shareFolder = (MessageBox(question.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES);
		}
		if (shareFolder)
		{
			try
			{
				CWaitCursor waitCursor;
				tstring lastName = Util::getLastDir(folder);
				ShareManager::getInstance()->addDirectory(l_folder, Text::fromT(lastName), true);
				tstring mmessage = folder;
				mmessage += L" (";
				mmessage += lastName;
				mmessage += L')';
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

void MainFrame::on(UserManagerListener::OutgoingPrivateMessage, const UserPtr& to, const string& hint, const tstring& message) noexcept // [+] IRainman
{
	PrivateFrame::openWindow(nullptr, HintedUser(to, hint), Util::emptyString, message);
}

void MainFrame::on(UserManagerListener::OpenHub, const string& url) noexcept
{
	HubFrame::openHubWindow(url);
}

void MainFrame::on(UserManagerListener::CollectSummaryInfo, const UserPtr& user, const string& hubHint) noexcept // [+] IRainman
{
	UserInfoSimple(user, hubHint).addSummaryMenu();
}
