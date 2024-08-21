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
#include "Emoticons.h"
#include "Players.h"
#include "Winamp.h"
#include "JAControl.h"
#include "iTunesCOMInterface.h"
#include "ToolbarManager.h"
#include "AboutDlgIndex.h"
#include "AddMagnet.h"
#include "CheckTargetDlg.h"
#include "DclstGenDlg.h"
#ifdef BL_UI_FEATURE_EMOTICONS
#include "../GdiOle/GDIImage.h"
#include "../client/SimpleStringTokenizer.h"
#endif
#include "../client/ConnectionManager.h"
#include "../client/ConnectivityManager.h"
#include "../client/UploadManager.h"
#include "../client/DownloadManager.h"
#include "../client/HashManager.h"
#include "../client/UserManager.h"
#include "../client/LogManager.h"
#include "../client/WebServerManager.h"
#include "../client/ThrottleManager.h"
#include "../client/CryptoManager.h"
#include "../client/Random.h"
#include "../client/NmdcHub.h"
#include "../client/DCPlusPlus.h"
#include "../client/HttpClient.h"
#include "../client/SocketPool.h"
#include "../client/AntiFlood.h"
#include "../client/IpTest.h"
#include "../client/CompatibilityManager.h"
#include "../client/SysVersion.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include "../client/FormatUtil.h"
#include "../client/SettingsUtil.h"
#include "../client/dht/DHT.h"
#include "../client/ConfCore.h"
#include "HIconWrapper.h"
#include "PrivateFrame.h"
#include "PublicHubsFrm.h"
#include "LimitEditDlg.h"
#include "ExMessageBox.h"
#include "CommandLine.h"
#include "BrowseFile.h"
#include "WinSysHandlers.h"
#include "StylesPage.h"
#include "NotifUtil.h"

#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
#include "TextFrame.h"
#endif

#define FLYLINKDC_CALC_MEMORY_USAGE // TODO: move to CompatibilityManager
#  ifdef FLYLINKDC_CALC_MEMORY_USAGE
#   ifdef OSVER_WIN_VISTA
#    define PSAPI_VERSION 1
#   endif
#   include <psapi.h>
#   pragma comment(lib, "psapi.lib")
#endif // FLYLINKDC_CALC_MEMORY_USAGE

extern ParsedCommandLine cmdLine;
extern IpBans tcpBans, udpBans;

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
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (!ss->getBool(Conf::PROTECT_TRAY)) return false;
	const string& password = ss->getString(Conf::PASSWORD);
	return !password.empty() && password != emptyStringHash;
}

static bool hasPasswordClose()
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (!ss->getBool(Conf::PROTECT_CLOSE)) return false;
	const string& password = ss->getString(Conf::PASSWORD);
	return !password.empty() && password != emptyStringHash;
}

MainFrame::MainFrame() :
	CSplitterImpl(false),
	TimerHelper(m_hWnd),
	statusContainer(STATUSCLASSNAME, this, STATUS_MESSAGE_MAP),
	hashProgressState(HASH_PROGRESS_HIDDEN),
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
	autoAway(false),
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
	stopperThread(nullptr),
	statusHistory(20),
	toolbarImageSize(0),
	fileListVersion(0),
	visToolbar(TRUE), visWinampBar(TRUE), visQuickSearch(TRUE)
{
	m_bUpdateProportionalPos = false;
	memset(statusSizes, 0, sizeof(statusSizes));
	auto tick = GET_TICK();
	timeDbCleanup = tick + 60000;
	timeUsersCleanup = tick + Util::rand(3, 10)*60000;
#ifdef BL_FEATURE_IP_DATABASE
	timeFlushRatio = tick + Util::rand(3, 10)*60000;
#endif
	g_ipTest.setCommandCallback(this);
	httpClient.setCommandCallback(this);
	updateSettings();
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
	toolbar16.Destroy();
	toolbarHot16.Destroy();
	toolbar24.Destroy();
	toolbarHot24.Destroy();
	mediaToolbar.Destroy();
	mediaToolbarHot.Destroy();
	settingsImages.Destroy();
	
	UserInfoGuiTraits::uninit();
	MenuHelper::uninit();
	WinUtil::uninit();
}

void MainFrame::updateSettings()
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	optToggleActiveWindow = ss->getBool(Conf::TOGGLE_ACTIVE_WINDOW);
	optAutoAway = ss->getBool(Conf::AUTO_AWAY);
	optMinimizeTray = ss->getBool(Conf::MINIMIZE_TRAY);
	optReducePriority = ss->getBool(Conf::REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY);
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
				tstring title;
				WinUtil::getWindowText(wnd, title);
				LogManager::message("Forced shutdown, hwnd=0x" + Util::toHexString(wnd) + " (" + Text::fromT(title) + ")");
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
	m_hMenu = MenuHelper::mainMenu;
	
	ctrlCmdBar.AttachMenu(m_hMenu);
	
	CImageList tmp;
	
	ResourceLoader::LoadImageList(IDR_TOOLBAR_SMALL, toolbar16, 16, 16);
	ResourceLoader::LoadImageList(IDR_MEDIA_TOOLBAR_SMALL, tmp, 16, 16);
	
	int imageCount = tmp.GetImageCount();
	for (int i = 0; i < imageCount; i++)
	{
		HICON icon = tmp.GetIcon(i);
		toolbar16.AddIcon(icon);
		DestroyIcon(icon);
	}
	
	tmp.Destroy();
	
	ctrlCmdBar.m_hImageList = toolbar16;
	
	for (size_t i = 0; g_ToolbarButtons[i].id; i++)
		ctrlCmdBar.m_arrCommand.Add(g_ToolbarButtons[i].id);
	
	// Add menu icons that are not used in toolbar
	for (size_t i = 0; g_MenuImages[i].id; i++)
		ctrlCmdBar.m_arrCommand.Add(g_MenuImages[i].id);
	
	for (size_t i = 0; g_WinampToolbarButtons[i].id; i++)
		ctrlCmdBar.m_arrCommand.Add(g_WinampToolbarButtons[i].id);
	
#if _WTL_CMDBAR_VISTA_MENUS
	// Use Vista-styled menus for Windows Vista and later.
#ifdef OSVER_WIN_XP
	if (SysVersion::isOsVistaPlus())
#endif
	{
		ctrlCmdBar._AddVistaBitmapsFromImageList(0, ctrlCmdBar.m_arrCommand.GetSize());
	}
#endif
	
	SetMenu(NULL);  // remove old menu
}

void MainFrame::updateFileListMenu()
{
	static const int MENU_POS = 2;
	if (ctrlCmdBar.m_bMenuActive) return; // Don't modify menu when it's visible
	auto sm = ShareManager::getInstance();
	int64_t version = sm->getFileListVersion();
	if (fileListVersion == version) return;
	fileListVersion = version;
	vector<ShareManager::ShareGroupInfo> sg;
	sm->getShareGroups(sg);
	if (sg.empty() && otherFileLists.empty()) return;
	otherFileLists.clear();
	CMenuHandle file(::GetSubMenu(m_hMenu, 0));
	file.RemoveMenu(MENU_POS, MF_BYPOSITION);
	if (!sg.empty())
	{
		tstring hotkey;
		tstring title = TSTRING(MENU_OPEN_OWN_LIST);
		auto pos = title.find('\t');
		if (pos != tstring::npos)
		{
			hotkey = title.substr(pos);
			title.erase(pos);
		}
		CMenuHandle menu;
		menu.CreateMenu();
		tstring defaultName = TSTRING(SHARE_GROUP_DEFAULT) + hotkey;
		menu.AppendMenu(MF_STRING, IDC_OPEN_MY_LIST, defaultName.c_str());
		int id = IDC_OPEN_MY_LIST;
		int count = 0;
		for (const auto& item : sg)
		{
			menu.AppendMenu(MF_STRING, ++id, Text::toT(item.name).c_str());
			otherFileLists.push_back(item.id);
			if (++count == 50) break;
		}
		CMenuItemInfo mi;
		mi.fMask = MIIM_TYPE | MIIM_SUBMENU;
		mi.fType = MFT_STRING;
		mi.dwTypeData = const_cast<TCHAR*>(title.c_str());
		mi.hSubMenu = menu.m_hMenu;
		file.InsertMenuItem(MENU_POS, TRUE, &mi);
	}
	else
		file.InsertMenu(MENU_POS, MF_BYPOSITION, IDC_OPEN_MY_LIST, CTSTRING(MENU_OPEN_OWN_LIST));
}

void MainFrame::createTrayMenu()
{
	trayMenu.SetOwnerDraw(OMenu::OD_NEVER);
	trayMenu.CreatePopupMenu();
	trayMenu.AppendMenu(MF_STRING, IDC_TRAY_SHOW, CTSTRING(MENU_SHOW));
	trayMenu.AppendMenu(MF_STRING, IDC_OPEN_DOWNLOADS, CTSTRING(MENU_OPEN_DOWNLOADS_DIR), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOADS_DIR, 0));
	trayMenu.AppendMenu(MF_SEPARATOR);
	trayMenu.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST, CTSTRING(MENU_REFRESH_FILE_LIST), g_iconBitmaps.getBitmap(IconBitmaps::REFRESH_SHARE, 0));
	trayMenu.AppendMenu(MF_STRING, IDC_TRAY_LIMITER, CTSTRING(SETCZDC_ENABLE_LIMITING), g_iconBitmaps.getBitmap(IconBitmaps::LIMIT, 0));
	trayMenu.AppendMenu(MF_STRING, ID_FILE_SETTINGS, CTSTRING(MENU_SETTINGS), g_iconBitmaps.getBitmap(IconBitmaps::SETTINGS, 0));
	trayMenu.AppendMenu(MF_STRING, ID_APP_ABOUT, CTSTRING(MENU_ABOUT), g_iconBitmaps.getBitmap(IconBitmaps::ABOUT, 0));
	trayMenu.AppendMenu(MF_STRING, IDC_TRAY_RESTORE_POS, CTSTRING(RESTORE_WINDOW_POS));
	trayMenu.AppendMenu(MF_SEPARATOR);
	trayMenu.AppendMenu(MF_STRING, ID_APP_EXIT, CTSTRING(MENU_EXIT));
	trayMenu.SetMenuDefaultItem(IDC_TRAY_SHOW);
}

LRESULT MainFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	LogManager::message("Main window created (Thread: " + Util::toString(BaseThread::getCurrentThreadId()) + ')', false);

	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::DETECT_PREVIEW_APPS))
	{
		ss->setBool(Conf::DETECT_PREVIEW_APPS, false);
		PreviewApplication::List appList;
		PreviewMenu::detectApps(appList);
		LogManager::message(Util::toString(appList.size()) + " preview app(s) found");
		FavoriteManager::getInstance()->addPreviewApps(appList, false);
	}

	if (FavoriteManager::getInstance()->getSearchUrls().empty())
		addWebSearchUrls();

	QueueManager::getInstance()->addListener(this);
	WebServerManager::getInstance()->addListener(this);
	UserManager::getInstance()->addListener(this);
	FinishedManager::getInstance()->addListener(this);
	FavoriteManager::getInstance()->addListener(this);
	SearchManager::getInstance()->addListener(this);

	if (ClientManager::isNickEmpty())
		ShowWindow(SW_RESTORE);

	auto cs = SettingsManager::instance.getCoreSettings();
	cs->lockRead();
	bool enableWebServer = cs->getBool(Conf::ENABLE_WEBSERVER);
	bool away = cs->getBool(Conf::AWAY);
	cs->unlockRead();

	if (enableWebServer)
	{
		try
		{
			WebServerManager::getInstance()->start();
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
	MenuHelper::createMenus();
	UserInfoGuiTraits::init();
	
	messageIdTaskbarCreated = RegisterWindowMessage(_T("TaskbarCreated"));
	dcassert(messageIdTaskbarCreated);
	
#ifdef OSVER_WIN_XP
	if (messageIdTaskbarCreated && SysVersion::isOsVistaPlus())
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
#ifdef OSVER_WIN_VISTA
				if (SysVersion::isWin7Plus())
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

	int imageSize = ss->getInt(Conf::TB_IMAGE_SIZE);
	createToolbar(imageSize);
	createQuickSearchBar();
	createWinampToolbar(imageSize);
	toolbarImageSize = imageSize;
	
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	ctrlRebar = m_hWndToolBar;
	
	AddSimpleReBarBandCtrl(ctrlRebar, ctrlCmdBar);
	AddSimpleReBarBandCtrl(ctrlRebar, ctrlToolbar, 0, NULL, TRUE);
		
	AddSimpleReBarBandCtrl(ctrlRebar, ctrlQuickSearchBar, 0, NULL, FALSE, 200, TRUE);
	AddSimpleReBarBandCtrl(ctrlRebar, ctrlWinampToolbar, 0, NULL, TRUE);
	
	CreateSimpleStatusBar();

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
	CToolInfo ti(TTF_SUBCLASS, ctrlStatus, 0, nullptr, LPSTR_TEXTCALLBACK);
	ctrlLastLines.AddTool(&ti);
	ctrlLastLines.SetDelayTime(TTDT_AUTOPOP, 15000);
	
	CreateMDIClient();
	ctrlCmdBar.SetMDIClient(m_hWndMDIClient);
	WinUtil::g_mdiClient = m_hWndMDIClient;
	ctrlTab.updateSettings(false);
	ctrlTab.Create(m_hWnd, rcDefault);
	WinUtil::g_tabCtrl = &ctrlTab;
	
	bool showTransferView = ss->getBool(Conf::SHOW_TRANSFERVIEW);
	transferView.Create(m_hWnd);
	toggleTransferView(showTransferView);

	initTransfersSplitter();

	UIAddToolBar(ctrlToolbar);
	UIAddToolBar(ctrlWinampToolbar);
	UIAddToolBar(ctrlQuickSearchBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	UISetCheck(ID_VIEW_TRANSFER_VIEW, showTransferView);
	UISetCheck(ID_VIEW_MEDIA_TOOLBAR, 1);
	UISetCheck(ID_VIEW_QUICK_SEARCH, 1);

	bool optTopmost = ss->getBool(Conf::TOPMOST);
	bool optLockToolbars = ss->getBool(Conf::LOCK_TOOLBARS);
	UISetCheck(IDC_TOPMOST, optTopmost);
	UISetCheck(IDC_LOCK_TOOLBARS, optLockToolbars);

	if (optTopmost) toggleTopmost();
	if (optLockToolbars) toggleLockToolbars(TRUE);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);
	
	createTrayMenu();
	
	tbMenu.CreatePopupMenu();
	tbMenu.AppendMenu(MF_STRING, ID_VIEW_TOOLBAR, CTSTRING(MENU_TOOLBAR));
	tbMenu.AppendMenu(MF_STRING, ID_VIEW_MEDIA_TOOLBAR, CTSTRING(TOGGLE_TOOLBAR));
	tbMenu.AppendMenu(MF_STRING, ID_VIEW_QUICK_SEARCH, CTSTRING(TOGGLE_QSEARCH));
	
	tbMenu.AppendMenu(MF_SEPARATOR);
	tbMenu.AppendMenu(MF_STRING, IDC_LOCK_TOOLBARS, CTSTRING(LOCK_TOOLBARS));
	
	winampMenu.CreatePopupMenu();
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + Conf::WinAmp, CTSTRING(MEDIA_MENU_WINAMP));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + Conf::WinMediaPlayer, CTSTRING(MEDIA_MENU_WMP));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + Conf::iTunes, CTSTRING(MEDIA_MENU_ITUNES));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + Conf::WinMediaPlayerClassic, CTSTRING(MEDIA_MENU_WPC));
	winampMenu.AppendMenu(MF_STRING, ID_MEDIA_MENU_WINAMP_START + Conf::JetAudio, CTSTRING(MEDIA_MENU_JA));
	
	//File::ensureDirectory(SETTING(LOG_DIRECTORY));
	
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

	updateFileListMenu();
	openDefaultWindows();

	ConnectivityManager::getInstance()->setupConnections();

	PostMessage(WM_SPEAKER, PROCESS_COMMAND_LINE);
	
	mainIcon = HIconWrapper(IDR_MAINFRAME);
	pmIcon = HIconWrapper(IDR_TRAY_AND_TASKBAR_PM);
	
	if (useTrayIcon) setTrayIcon(TRAY_ICON_NORMAL);

	Util::setAway(away, true);

	checkToolbarButtons();
	ctrlToolbar.CheckButton(ID_VIEW_MEDIA_TOOLBAR, visWinampBar);

	if (ClientManager::isNickEmpty())
		PostMessage(WM_COMMAND, ID_FILE_SETTINGS);

	jaControl = unique_ptr<JAControl>(new JAControl((HWND)(*this)));

	// We want to pass this one on to the splitter...hope it get's there...
	bHandled = FALSE;

	transferView.UpdateLayout();

#ifdef BL_UI_FEATURE_EMOTICONS
	StringList emoticonPacks;
	const string& emoticonPack = ss->getString(Conf::EMOTICONS_FILE);
	if (!emoticonPack.empty() && emoticonPack != "Disabled")
	{
		emoticonPacks.push_back(emoticonPack);
		SimpleStringTokenizer<char> st(ss->getString(Conf::ADDITIONAL_EMOTICONS), ';');
		string token;
		while (st.getNextNonEmptyToken(token))
			emoticonPacks.push_back(token);
	}
	emoticonPackList.setConfig(emoticonPacks);
	chatTextParser.setEmoticonPackList(&emoticonPackList);
#endif
	chatTextParser.initSearch();

	if (!PopupManager::isValidInstance())
		PopupManager::newInstance();

	PostMessage(WMU_UPDATE_LAYOUT);
	createTimer(1000, 3);

	return 0;
}

void MainFrame::checkToolbarButtons()
{
	ctrlToolbar.CheckButton(IDC_AWAY, Util::getAway());
	const auto* ss = SettingsManager::instance.getUiSettings();
	ctrlToolbar.CheckButton(IDC_DISABLE_SOUNDS, ss->getBool(Conf::SOUNDS_DISABLED));
	ctrlToolbar.CheckButton(IDC_DISABLE_POPUPS, ss->getBool(Conf::POPUPS_DISABLED));
	ctrlToolbar.CheckButton(IDC_SHUTDOWN, isShutDown());
	checkLimitsButton();
}

void MainFrame::checkLimitsButton()
{
	bool limiterEnabled = ThrottleManager::getInstance()->isEnabled();
	UISetCheck(IDC_TRAY_LIMITER, limiterEnabled);
	ctrlToolbar.CheckButton(IDC_LIMITER, limiterEnabled);
}

void MainFrame::openDefaultWindows()
{
	static const struct
	{
		int setting;
		int command;
		bool value;
	} openSettings[] =
	{
		{ Conf::OPEN_FAVORITE_HUBS,      IDC_FAVORITES,         true  },
		{ Conf::OPEN_FAVORITE_USERS,     IDC_FAVUSERS,          true  },
		{ Conf::OPEN_PUBLIC_HUBS,        IDC_PUBLIC_HUBS,       true  },
		{ Conf::OPEN_QUEUE,              IDC_QUEUE,             true  },
		{ Conf::OPEN_FINISHED_DOWNLOADS, IDC_FINISHED,          true  },
		{ Conf::OPEN_WAITING_USERS,      IDC_UPLOAD_QUEUE,      true  },
		{ Conf::OPEN_FINISHED_UPLOADS,   IDC_FINISHED_UL,       true  },
		{ Conf::OPEN_SEARCH_SPY,         IDC_SEARCH_SPY,        true  },
		{ Conf::OPEN_NETWORK_STATISTICS, IDC_NET_STATS,         true  },
		{ Conf::OPEN_NOTEPAD,            IDC_NOTEPAD,           true  },
		{ Conf::OPEN_ADLSEARCH,          IDC_FILE_ADL_SEARCH,   true  },
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		{ Conf::OPEN_CDMDEBUG,           IDC_CDMDEBUG_WINDOW,   true  },
#endif
		{ Conf::SHOW_STATUSBAR,          ID_VIEW_STATUS_BAR,    false },
		{ Conf::SHOW_TOOLBAR,            ID_VIEW_TOOLBAR,       false },
		{ Conf::SHOW_PLAYER_CONTROLS,    ID_VIEW_MEDIA_TOOLBAR, false },
		{ Conf::SHOW_QUICK_SEARCH,       ID_VIEW_QUICK_SEARCH,  false }
	};
	const auto* ss = SettingsManager::instance.getUiSettings();
	for (int i = 0; i < _countof(openSettings); ++i)
		if (ss->getBool(openSettings[i].setting) == openSettings[i].value)
			PostMessage(WM_COMMAND, openSettings[i].command);
}

void MainFrame::initTransfersSplitter()
{
	auto ss = SettingsManager::instance.getUiSettings();
	int splitSize = ss->getInt(Conf::TRANSFER_FRAME_SPLIT);
	if (splitSize < 3000 || splitSize > 9400)
	{
		splitSize = 9100;
		ss->setInt(Conf::TRANSFER_FRAME_SPLIT, splitSize);
	}
	SetSplitterPanes(m_hWndMDIClient, transferView.m_hWnd);
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
	m_nProportionalPos = splitSize;
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
	if (tick >= timeUsersCleanup)
	{
		ClientManager::usersCleanup();
		timeUsersCleanup = tick + Util::rand(3, 10)*60000;
	}
#ifdef BL_FEATURE_IP_DATABASE
	if (tick >= timeFlushRatio)
	{
		ClientManager::flushRatio();
		timeFlushRatio = tick + Util::rand(3, 10)*60000;
	}
#endif
	if (tick >= timeDbCleanup)
	{
		DatabaseManager::getInstance()->processTimer(tick);
		timeDbCleanup = tick + 60000;
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
		if (!SysVersion::isWine())
		{
			if ((tick / 1000) % 5 == 0)
			{
				TCHAR buf[128];
				tstring title = getAppNameVerT();
				if (memoryInfoResult && CompatibilityManager::updatePhysMemoryStats())
				{
					_sntprintf(buf, _countof(buf), _T(" [RAM: %dM / %dM][Free: %dM][GDI: %d]"),
					           g_RAM_WorkingSetSize,
					           g_RAM_PeakWorkingSetSize,
					           int(CompatibilityManager::getFreePhysMemory() >> 20),
					           int(g_GDI_count));
					title += buf;
				}
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
			auto dm = DownloadManager::getInstance();
			auto um = UploadManager::getInstance();
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
			                 + _T(" (") + Util::toStringT(um->getFreeExtraSlots()) + _T('/') + Util::toStringT(Util::getConfInt(Conf::EXTRA_SLOTS)) + _T(")"));
			stats->push_back(TSTRING(D) + _T(' ') + Util::formatBytesT(currentDown));
			stats->push_back(TSTRING(U) + _T(' ') + Util::formatBytesT(currentUp));
			auto tm = ThrottleManager::getInstance();
			size_t downLimit = tm->getDownloadLimitInKBytes();
			size_t upLimit = tm->getUploadLimitInKBytes();
			stats->push_back(TSTRING(D) + _T(" [") + Util::toStringT(dm->getDownloadCount()) + _T("][")
			                 + (downLimit == 0 ?
			                    TSTRING(N) : Util::toStringT(downLimit) + TSTRING(KILO)) + _T("] ")
			                 + dlstr + _T('/') + TSTRING(S));
			stats->push_back(TSTRING(U) + _T(" [") + Util::toStringT(um->getUploadCount()) + _T("][")
			                 + (upLimit == 0 ?
			                    TSTRING(N) : Util::toStringT(upLimit) + TSTRING(KILO)) + _T("] ")
			                 + ulstr + _T('/') + TSTRING(S));
			processingStats = true;
			if (!WinUtil::postSpeakerMsg(m_hWnd, MAIN_STATS, stats))
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
	httpClient.removeUnusedConnections();
	socketPool.removeExpired(tick);
	tcpBans.removeExpired(tick);
	udpBans.removeExpired(tick);
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool geoIpAutoUpdate = ss->getBool(Conf::GEOIP_AUTO_UPDATE);
	string geoIpUrl = ss->getString(Conf::URL_GEOIP);
	ss->unlockRead();
	if (geoIpAutoUpdate)
		DatabaseManager::getInstance()->downloadGeoIPDatabase(tick, false, geoIpUrl);
	ADLSearchManager::getInstance()->saveOnTimer(tick);
	WebServerManager::getInstance()->removeExpired();
	CryptoManager::getInstance()->checkExpiredCert();
	LogManager::closeOldFiles(tick);
}

void MainFrame::createToolbar(int imageSize)
{
	uint8_t* checkState = nullptr;
	if (!ctrlToolbar)
	{
		ctrlToolbar.Create(m_hWnd, NULL, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
		ctrlToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);
	}
	else
	{
		checkState = new uint8_t[g_ToolbarButtonsCount];
		memset(checkState, 0, g_ToolbarButtonsCount);
		TBBUTTON tbb;
		int count = ctrlToolbar.GetButtonCount();
		while (count)
		{
			int index = --count;
			ctrlToolbar.GetButton(index, &tbb);
			if (tbb.fsState & TBSTATE_CHECKED)
			{
				for (int i = 0; i < g_ToolbarButtonsCount; ++i)
					if (g_ToolbarButtons[i].id == tbb.idCommand)
					{
						checkState[i] = TBSTATE_CHECKED;
						break;
					}
			}
			ctrlToolbar.DeleteButton(index);
		}
	}

	if (toolbarImageSize != imageSize)
	{
		if (imageSize == 16)
		{
			checkImageList(toolbarHot16, IDR_TOOLBAR_SMALL_HOT, 16);
			ctrlToolbar.SetImageList(toolbar16);
			ctrlToolbar.SetHotImageList(toolbarHot16);
		}
		else
		{
			checkImageList(toolbar24, IDR_TOOLBAR, 24);
			checkImageList(toolbarHot24, IDR_TOOLBAR_HOT, 24);
			ctrlToolbar.SetImageList(toolbar24);
			ctrlToolbar.SetHotImageList(toolbarHot24);
		}
	}

	const auto* ss = SettingsManager::instance.getUiSettings();
	fillToolbarButtons(ctrlToolbar, ss->getString(Conf::TOOLBAR), g_ToolbarButtons, g_ToolbarButtonsCount, checkState);
	delete[] checkState;
	ctrlToolbar.AutoSize();
}

void MainFrame::checkImageList(CImageList& imageList, int resource, int size)
{
	if (!imageList.m_hImageList)
		ResourceLoader::LoadImageList(resource, imageList, size, size);
}

CImageList& MainFrame::getToolbarImages()
{
	checkImageList(toolbar24, IDR_TOOLBAR, 24);
	return toolbar24;
}

CImageList& MainFrame::getToolbarHotImages()
{
	checkImageList(toolbarHot24, IDR_TOOLBAR_HOT, 24);
	return toolbarHot24;
}

void MainFrame::createWinampToolbar(int imageSize)
{
	if (!ctrlWinampToolbar)
	{
		ctrlWinampToolbar.Create(m_hWnd, NULL, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
		ctrlWinampToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);
	}
	else
	{
		int count = ctrlWinampToolbar.GetButtonCount();
		while (count) ctrlWinampToolbar.DeleteButton(--count);
	}

	if (toolbarImageSize != imageSize)
	{
		if (mediaToolbar.m_hImageList) mediaToolbar.Destroy();
		if (mediaToolbarHot.m_hImageList) mediaToolbarHot.Destroy();
		if (imageSize == 16)
		{
			ResourceLoader::LoadImageList(IDR_MEDIA_TOOLBAR_SMALL, mediaToolbar, 16, 16);
			ResourceLoader::LoadImageList(IDR_MEDIA_TOOLBAR_SMALL_HOT, mediaToolbarHot, 16, 16);
			ctrlWinampToolbar.SetImageList(mediaToolbar);
			ctrlWinampToolbar.SetHotImageList(mediaToolbarHot);
		}
		else
		{
			ResourceLoader::LoadImageList(IDR_MEDIA_TOOLBAR, mediaToolbar, 24, 24);
			ResourceLoader::LoadImageList(IDR_MEDIA_TOOLBAR_HOT, mediaToolbarHot, 24, 24);
			ctrlWinampToolbar.SetImageList(mediaToolbar);
			ctrlWinampToolbar.SetHotImageList(mediaToolbarHot);
		}
	}

	const auto* ss = SettingsManager::instance.getUiSettings();
	fillToolbarButtons(ctrlWinampToolbar, ss->getString(Conf::WINAMPTOOLBAR), g_WinampToolbarButtons, g_WinampToolbarButtonsCount, nullptr);
	ctrlWinampToolbar.AutoSize();
}

void MainFrame::createQuickSearchBar()
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

		COMBOBOXINFO inf = { sizeof(inf) };
		quickSearchBox.GetComboBoxInfo(&inf);
		if (inf.hwndItem && !quickSearchEdit.IsWindow() && inf.hwndItem != quickSearchBox.m_hWnd)
		{
			quickSearchEdit.Attach(inf.hwndItem);
			quickSearchEdit.SetCueBannerText(CTSTRING(QSEARCH_STR));
			quickSearchEditContainer.SubclassWindow(quickSearchEdit.m_hWnd);
		}
	}
}

LRESULT MainFrame::onRebuildToolbar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	LockWindowUpdate(TRUE);
	const auto* ss = SettingsManager::instance.getUiSettings();
	int imageSize = ss->getInt(Conf::TB_IMAGE_SIZE);
	bool changeSize = imageSize != toolbarImageSize;
	if (changeSize)
	{
		ctrlRebar.DeleteBand(3);
		ctrlRebar.DeleteBand(2);
		ctrlRebar.DeleteBand(1);
	}
	createToolbar(imageSize);
	if (changeSize)
	{
		createWinampToolbar(imageSize);
		AddSimpleReBarBandCtrl(ctrlRebar, ctrlToolbar, 0, NULL, TRUE);
	}
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
	if (changeSize)
	{
		AddSimpleReBarBandCtrl(ctrlRebar, ctrlQuickSearchBar, 0, NULL, FALSE, 200, TRUE);
		AddSimpleReBarBandCtrl(ctrlRebar, ctrlWinampToolbar, 0, NULL, TRUE);
		toolbarImageSize = imageSize;
		toggleRebarBand(visToolbar, 1, ID_VIEW_TOOLBAR, Conf::SHOW_TOOLBAR);
		toggleRebarBand(visWinampBar, 3, ID_VIEW_MEDIA_TOOLBAR, Conf::SHOW_PLAYER_CONTROLS);
		toggleRebarBand(visQuickSearch, 2, ID_VIEW_QUICK_SEARCH, Conf::SHOW_QUICK_SEARCH);
		if (ss->getBool(Conf::LOCK_TOOLBARS)) toggleLockToolbars(TRUE);
		UpdateLayout();
	}
	LockWindowUpdate(FALSE);
	return 0;
}

LRESULT MainFrame::onWinampButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int mediaPlayer = SettingsManager::instance.getUiSettings()->getInt(Conf::MEDIA_PLAYER);
	if (mediaPlayer == Conf::WinAmp)
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
	else if (mediaPlayer == Conf::WinMediaPlayer)
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
	else if (mediaPlayer == Conf::iTunes)
	{
		// Since i couldn't find out the appropriate window messages, we doing this à la COM
		HWND hwndiTunes = FindWindow(_T("iTunes"), _T("iTunes"));
		if (::IsWindow(hwndiTunes))
		{
			IiTunes *iITunes = nullptr;
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
			if (iITunes) iITunes->Release();
			CoUninitialize();
		}
	}
	else if (mediaPlayer == Conf::WinMediaPlayerClassic)
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
	else if (mediaPlayer == Conf::JetAudio)
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
			if (WinUtil::isShift() || WinUtil::isCtrl() || WinUtil::isAlt())
			{
				bHandled = FALSE;
			}
			else if (uMsg == WM_KEYDOWN)
			{
				tstring s;
				WinUtil::getWindowText(quickSearchEdit, s);
				SearchFrame::openWindow(s);
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
	
	if (LOWORD(wParam) == WM_LBUTTONDOWN && hashProgressState != HASH_PROGRESS_HIDDEN)
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
		if (SearchFrame::lastSearches.empty())
			SearchFrame::lastSearches.load(e_SearchHistory);
		const auto& data = SearchFrame::lastSearches.getData();
		for (const tstring& s : data)
			quickSearchBox.AddString(s.c_str());
	}
	if (SettingsManager::instance.getUiSettings()->getBool(Conf::CLEAR_SEARCH))
		quickSearchBox.SetWindowText(_T(""));
}

LRESULT MainFrame::onListenerInit(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam)
	{
		ListenerError* error = reinterpret_cast<ListenerError*>(lParam);
		string type = error->type;
		type += error->af == AF_INET6 ? "v6" : "v4";
		MessageBox(CTSTRING_F(LISTENING_SOCKET_ERROR, type.c_str() % Text::toT(error->errorText)),
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
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::OPEN_DHT))
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

		updateFileListMenu();
		TStringList& str = *pstr;
		if (ctrlStatus.IsWindow())
		{
			bool update = false;
			int newHashProgressState = HASH_PROGRESS_HIDDEN;
			int progressValue = 0;
			int smState = ShareManager::getInstance()->getState();
			if (!(smState == ShareManager::STATE_SCANNING_DIRS || smState == ShareManager::STATE_CREATING_FILELIST))
			{
				HashManager::Info info;
				auto hm = HashManager::getInstance();
				hm->getInfo(info);
				if (info.filesLeft)
				{
					newHashProgressState = hm->getHashSpeed() < 0 ? HASH_PROGRESS_PAUSED : HASH_PROGRESS_NORMAL;
					if (info.sizeHashed >= info.sizeToHash)
						progressValue = HashProgressDlg::MAX_PROGRESS_VALUE;
					else
						progressValue = (info.sizeHashed * HashProgressDlg::MAX_PROGRESS_VALUE) / info.sizeToHash;
				}
			}
			else
				newHashProgressState = HASH_PROGRESS_MARQUEE;

			if (newHashProgressState != hashProgressState)
			{
				if (newHashProgressState != HASH_PROGRESS_HIDDEN)
				{
#ifdef OSVER_WIN_XP
					if (SysVersion::isOsVistaPlus())
#endif
						ctrlHashProgress.SendMessage(PBM_SETSTATE, newHashProgressState == HASH_PROGRESS_PAUSED ? PBST_PAUSED : PBST_NORMAL);
					if (hashProgressState == HASH_PROGRESS_MARQUEE || newHashProgressState == HASH_PROGRESS_MARQUEE)
					{
						if (newHashProgressState == HASH_PROGRESS_MARQUEE)
						{
							ctrlHashProgress.ModifyStyle(0, PBS_MARQUEE);
							ctrlHashProgress.SetMarquee(TRUE);
						}
						else
						{
							ctrlHashProgress.SetMarquee(FALSE);
							ctrlHashProgress.ModifyStyle(PBS_MARQUEE, 0);
						}
					}
					if (newHashProgressState != HASH_PROGRESS_MARQUEE)
						ctrlHashProgress.SetPos(progressValue);
					ctrlHashProgress.ShowWindow(SW_SHOW);
				}
				else
				{
					ctrlHashProgress.ShowWindow(SW_HIDE);
					ctrlHashProgress.SetPos(0);
				}
				hashProgressState = newHashProgressState;
				update = true;
			}
			else if (hashProgressState == HASH_PROGRESS_NORMAL)
				ctrlHashProgress.SetPos(progressValue);

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

			if (shutdownEnabled)
			{
				const uint64_t second = GET_TICK() / 1000;
				if (!shutdownStatusDisplayed)
				{
					HICON shutdownIcon = g_iconBitmaps.getIcon(IconBitmaps::SHUTDOWN, 0);
					ctrlStatus.SetIcon(STATUS_PART_SHUTDOWN_TIME, shutdownIcon);
					shutdownStatusDisplayed = true;
				}
				if (DownloadManager::getInstance()->getDownloadCount() > 0)
				{
					shutdownTime = second;
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
				}
				else
				{
					const auto* ss = SettingsManager::instance.getUiSettings();
					const int timeout = ss->getInt(Conf::SHUTDOWN_TIMEOUT);
					const int64_t timeLeft = timeout - (second - shutdownTime);
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, (_T(' ') + Util::formatSecondsT(timeLeft, timeLeft < 3600)).c_str(), SBT_POPOUT);
					if (shutdownTime + timeout <= second)
					{
						// We better not try again. It WON'T work...
						shutdownEnabled = false;

						int action = ss->getInt(Conf::SHUTDOWN_ACTION);
						bool shutdownResult = WinUtil::shutDown(action);
						if (shutdownResult)
						{
							if (action == 5) setShutdownButton(false);
						}
						else
						{
							setShutdownButton(false);
							addStatusMessage(TSTRING(FAILED_TO_SHUTDOWN));
							ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
							ctrlStatus.SetIcon(STATUS_PART_SHUTDOWN_TIME, nullptr);
						}
					}
				}
			}
			else
			{
				if (shutdownStatusDisplayed)
				{
					ctrlStatus.SetText(STATUS_PART_SHUTDOWN_TIME, _T(""));
					ctrlStatus.SetIcon(STATUS_PART_SHUTDOWN_TIME, nullptr);
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
			addStatusMessage(Text::toT(msg));
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
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
	else if (wParam == VIEW_FILE_AND_DELETE)
	{
		std::unique_ptr<tstring> file(reinterpret_cast<tstring*>(lParam));
		if (!ClientManager::isBeforeShutdown())
		{
			TextFrame::openWindow(*file);
		}
	}
#endif
	else if (wParam == PROCESS_COMMAND_LINE)
	{
		processCommandLine(cmdLine);
	}
	else if (wParam == SOUND_NOTIF)
	{
		NotifUtil::playSound(lParam);
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
	else if (wParam == SAVE_RECENTS)
	{
		FavoriteManager::getInstance()->saveRecents();
	}
	else if (wParam == FILE_EXISTS_ACTION)
	{
		while (true)
		{
			csFileExistsActions.lock();
			if (fileExistsActions.empty())
			{
				csFileExistsActions.unlock();
				break;
			}
			auto data = std::move(fileExistsActions.front());
			fileExistsActions.pop_front();
			csFileExistsActions.unlock();

			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockRead();
			int option = ss->getInt(Conf::TARGET_EXISTS_ACTION);
			ss->unlockRead();
			if (option == Conf::TE_ACTION_ASK)
			{
				bool applyForAll;
				option = Conf::TE_ACTION_REPLACE;
				CheckTargetDlg::showDialog(*this, data.path, data.newSize, data.existingSize, data.existingTime, option, applyForAll);
				if (applyForAll)
				{
					ss->lockWrite();
					ss->setInt(Conf::TARGET_EXISTS_ACTION, option);
					ss->unlockWrite();
				}
			}
			string newPath;
			if (option == Conf::TE_ACTION_RENAME)
				newPath = Util::getNewFileName(data.path);
			QueueManager::getInstance()->processFileExistsQuery(data.path, option, newPath, data.priority);
		}
	}
	else if (wParam == FRAME_INFO_TEXT)
	{
		FrameInfoText* data = reinterpret_cast<FrameInfoText*>(lParam);
		int type = data->frameId & WinUtil::MASK_FRAME_TYPE;
		if (type == WinUtil::FRAME_TYPE_HUB)
		{
			HubFrame* frame = HubFrame::findFrameByID(data->frameId);
			if (frame)
				frame->addSystemMessage(data->text, Colors::TEXT_STYLE_SYSTEM_MESSAGE);
		}
		else if (type == WinUtil::FRAME_TYPE_PM)
		{
			PrivateFrame* frame = PrivateFrame::findFrameByID(data->frameId);
			if (frame)
				frame->addSystemMessage(data->text, Colors::TEXT_STYLE_SYSTEM_MESSAGE);
		}
		delete data;
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

void MainFrame::addStatusMessage(const tstring& msg)
{
	string timeStampsFmt = Util::getConfString(Conf::TIME_STAMPS_FORMAT);
	tstring line = Text::toT(Util::formatDateTime("[" + timeStampsFmt + "] ", GET_TIME()));
	line += msg;
	tstring::size_type rpos = line.find(_T('\r'));
	if (rpos != tstring::npos) line.erase(rpos);
	ctrlStatus.SetText(STATUS_PART_MESSAGE, line.c_str());
	statusHistory.addLine(line);
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
	HashProgressDlg dlg(false, false);
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

LRESULT MainFrame::onOpenConfigs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	openDirs(Util::getConfigPath());
	return 0;
}

LRESULT MainFrame::onOpenDownloads(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	openDirs(Util::getDownloadDir(UserPtr()));
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

struct WebServerSettings
{
	bool enabled;
	string bindAddress;
	int port;

	void get();
	bool compare(const WebServerSettings& other) const
	{
		return enabled == other.enabled && bindAddress == other.bindAddress && port == other.port;
	}
};

void WebServerSettings::get()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	enabled = ss->getBool(Conf::ENABLE_WEBSERVER);
	bindAddress = ss->getString(Conf::WEBSERVER_BIND_ADDRESS);
	port = ss->getInt(Conf::WEBSERVER_PORT);
	ss->unlockRead();
}

LRESULT MainFrame::onSettings(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!PropertiesDlg::instance)
	{
		PropertiesDlg dlg(m_hWnd, g_iconBitmaps.getIcon(IconBitmaps::SETTINGS, 0));

		NetworkSettings prevNetworkSettings;
		prevNetworkSettings.get();
		NetworkPage::setPrevSettings(&prevNetworkSettings);

		WebServerSettings prevWebServerSettings;
		auto cs = SettingsManager::instance.getCoreSettings();
		const auto* ss = SettingsManager::instance.getUiSettings();
		bool prevSortFavUsersFirst = ss->getBool(Conf::SORT_FAVUSERS_FIRST);
		bool prevHubUrlInTitle = ss->getBool(Conf::HUB_URL_IN_TITLE);
		int prevRegHandlerSettings = WinUtil::getRegHandlerSettings();
		prevWebServerSettings.get();
		cs->lockRead();
		bool prevDHT = cs->getBool(Conf::USE_DHT);
		string prevDownloadDir = cs->getString(Conf::TEMP_DOWNLOAD_DIRECTORY);
		cs->unlockRead();
		COLORREF prevTextColor = Colors::g_textColor;
		COLORREF prevBgColor = Colors::g_bgColor;

		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			SettingsManager::instance.saveSettings();

			NetworkSettings currentNetworkSettings;
			currentNetworkSettings.get();
			if (ConnectionManager::getInstance()->getPort() == 0 || !currentNetworkSettings.compare(prevNetworkSettings))
				ConnectivityManager::getInstance()->setupConnections();

			cs->lockRead();
			bool useDHT = cs->getBool(Conf::USE_DHT);
			string downloadDir = cs->getString(Conf::TEMP_DOWNLOAD_DIRECTORY);
			cs->unlockRead();

			if (useDHT != prevDHT)
			{
				dht::DHT* d = dht::DHT::getInstance();
				if (useDHT)
					d->start();
				else
					d->stop();
			}

			WebServerSettings webServerSettings;
			webServerSettings.get();
			if (!webServerSettings.compare(prevWebServerSettings))
			{
				WebServerManager::getInstance()->shutdown();
				if (webServerSettings.enabled)
					WebServerManager::getInstance()->start();
			}

			int regHandlerSettings = WinUtil::getRegHandlerSettings();
			int regHandlerMask = prevRegHandlerSettings ^ regHandlerSettings;
			if (regHandlerMask)
				WinUtil::applyRegHandlerSettings(regHandlerSettings, regHandlerMask);

			if (ss->getBool(Conf::SORT_FAVUSERS_FIRST) != prevSortFavUsersFirst)
				HubFrame::resortUsers();

			if (ss->getBool(Conf::HUB_URL_IN_TITLE) != prevHubUrlInTitle)
				HubFrame::updateAllTitles();

			if (downloadDir != prevDownloadDir)
				QueueItem::checkTempDir = true;

			Util::updateCoreSettings();
			updateSettings();
			g_fileImage.updateSettings();
			checkToolbarButtons();

			bool needUpdateLayout = ctrlTab.getTabsPosition() != ss->getInt(Conf::TABS_POS);
			bool needInvalidateTabs = ctrlTab.updateSettings(false);

			if (needUpdateLayout)
			{
				UpdateLayout();
				needInvalidateTabs = true;
			}

			if (Colors::g_textColor != prevTextColor || Colors::g_bgColor != prevBgColor)
				quickSearchEdit.Invalidate();

			if (needInvalidateTabs)
				ctrlTab.Invalidate();

			if (!ss->getBool(Conf::SHOW_CURRENT_SPEED_IN_TITLE))
				SetWindowText(getAppNameVerT().c_str());

			ShareManager::getInstance()->refreshShareIfChanged();
			ClientManager::infoUpdated(true);

			if (StylesPage::queryChatColorsChanged())
			{
				HubFrame::changeTheme();
				PrivateFrame::changeTheme();
			}
		}
	}
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
		statusHistory.getToolTip(pnmh);
	}
	return 0;
}

void MainFrame::autoConnect(const std::vector<FavoriteHubEntry>& hubs)
{
	::LockWindowUpdate(WinUtil::g_mdiClient);
	HubFrame* lastFrame = nullptr;
	HubFrame::Settings cs;
	for (const FavoriteHubEntry& entry : hubs)
	{
		if (!entry.getNick().empty())
		{
			RecentHubEntry r;
			r.setName(entry.getName());
			r.setDescription(entry.getDescription());
			r.setOpenTab("+");
			r.setServer(entry.getServer());
			RecentHubEntry* recent = FavoriteManager::getInstance()->addRecent(r);
			if (recent)
				recent->setAutoOpen(true);
			cs.copySettings(entry);
			lastFrame = HubFrame::openHubWindow(cs);
		}
	}
	if (SettingsManager::instance.getUiSettings()->getBool(Conf::OPEN_RECENT_HUBS))
	{
		HubFrame::Settings cs;
		const auto& recents = FavoriteManager::getInstance()->getRecentHubs();
		for (const RecentHubEntry* recent : recents)
		{
			if (!recent->getAutoOpen() && recent->getOpenTab() == "+")
			{
				cs.server = recent->getServer();
				cs.name = recent->getName();
				lastFrame = HubFrame::openHubWindow(cs);
			}
		}
	}
	ClientManager::stopStartup();
	if (lastFrame)
		lastFrame->showActiveFrame();
	::LockWindowUpdate(NULL);
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

LRESULT MainFrame::onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam == SIZE_MINIMIZED)
	{
		if (!appMinimized)
		{
			appMinimized = true;
			if (optMinimizeTray != WinUtil::isShift())
			{
				ShowWindow(SW_HIDE);
				if (optReducePriority)
					CompatibilityManager::reduceProcessPriority();
			}
			if (optAutoAway && !Util::getAway())
			{
				autoAway = true;
				Util::setAway(true);
				setAwayButton(true);
			}
		}
		wasMaximized = IsZoomed() > 0;
	}
	else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
	{
		if (appMinimized)
		{
			appMinimized = false;
			CompatibilityManager::restoreProcessPriority();
			clearPMStatus();
			if (Util::getAway() && autoAway)
			{
				Util::setAway(false);
				setAwayButton(false);
			}
			autoAway = false;
		}
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
		auto ss = SettingsManager::instance.getUiSettings();
		if (wp.showCmd == SW_SHOWNORMAL || wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWMAXIMIZED || wp.showCmd == SW_SHOWMINIMIZED)
			ss->setInt(Conf::MAIN_WINDOW_STATE, wp.showCmd);
		else
			dcassert(0);
		CRect rc;
		if ((wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWNORMAL) && GetWindowRect(rc))
		{
			ss->setInt(Conf::MAIN_WINDOW_POS_X, rc.left);
			ss->setInt(Conf::MAIN_WINDOW_POS_Y, rc.top);
			ss->setInt(Conf::MAIN_WINDOW_SIZE_X, rc.Width());
			ss->setInt(Conf::MAIN_WINDOW_SIZE_Y, rc.Height());
		}
	}
#ifdef _DEBUG
	else
	{
		dcdebug("MainFrame:: WINDOW  GetWindowPlacement(&wp) -> NULL data !!!\n");
	}
#endif
}

void MainFrame::prepareNonMaximized()
{
	ctrlTab.ShowWindow(SW_HIDE);
	CRect rect;
	GetClientRect(&rect);
	UpdateBarsPosition(rect, TRUE);
	SetSplitterRect(&rect);
	HubFrame::prepareNonMaximized();
	PrivateFrame::prepareNonMaximized();
}

LRESULT MainFrame::onSetDefaultPosition(WORD /*wNotifyCode*/, WORD /*wParam*/, HWND, BOOL& /*bHandled*/)
{
	ShowWindow(SW_SHOWDEFAULT);
	CenterWindow(GetParent());
	return 0;
}

LRESULT MainFrame::onEndSession(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	endSession = true;
	FavoriteManager::getInstance()->saveFavorites();
	//SettingsManager::getInstance()->save();
	return 0;
}

LRESULT MainFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::MINIMIZE_ON_CLOSE) && !quitFromMenu)
	{
		ShowWindow(SW_MINIMIZE);
	}
	else
	{
		if (!closing)
		{
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
			
			bool forceNoWarning = false;
			
			if (!dontQuit)
			{
				// [+] brain-ripper
				// check if hashing pending,
				// and display hashing progress
				// if any
				
				if (HashManager::getInstance()->isHashing())
				{
					bool forceStopExit = false;
					if (!HashProgressDlg::instanceCounter)
					{
						HashProgressDlg dlg(true, true);
						forceStopExit = dlg.DoModal() != IDC_BTN_EXIT_ON_DONE;
					}
					
					// User take decision in dialog,
					//so avoid one more warning message
					forceNoWarning = true;
					
					if (HashManager::getInstance()->isHashing() || forceStopExit)
					{
						// User closed progress window, while hashing still in progress,
						// or user unchecked "exit on done" checkbox,
						// so let program to work
						
						dontQuit = true;
						quitFromMenu = false;
					}
				}
			}

			UINT checkState = ss->getBool(Conf::CONFIRM_EXIT) ? BST_CHECKED : BST_UNCHECKED;

			if ((endSession ||
			     ss->getBool(Conf::PROTECT_CLOSE) ||
			     checkState == BST_UNCHECKED ||
			     (forceNoWarning ||
			     MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_EXIT), getAppNameVerT().c_str(),
			                         CTSTRING(ALWAYS_ASK), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES))
			     && !dontQuit)
			{
				storeWindowsPos();

				ClientManager::beforeShutdown();
				LogManager::g_mainWnd = nullptr;
				closing = true;
				destroyTimer();
				ClientManager::stopStartup();
				preparingCoreToShutdown();

				transferView.prepareClose();

				WebServerManager::getInstance()->removeListener(this);
				SearchManager::getInstance()->removeListener(this);
				FavoriteManager::getInstance()->removeListener(this);
				FinishedManager::getInstance()->removeListener(this);
				UserManager::getInstance()->removeListener(this);
				QueueManager::getInstance()->removeListener(this);

				ToolbarManager::getInstance()->getFrom(m_hWndToolBar, "MainToolBar");

				useTrayIcon = false;
				setTrayIcon(TRAY_ICON_NONE);
				if (m_nProportionalPos > 300)
					ss->setInt(Conf::TRANSFER_FRAME_SPLIT, m_nProportionalPos);
				ShowWindow(SW_HIDE);
				stopperThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, &stopper, this, 0, nullptr));
			}
			else
			{
				quitFromMenu = false;
			}
			ss->setBool(Conf::CONFIRM_EXIT, checkState != BST_UNCHECKED);
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
	if (WinUtil::browseFile(file, m_hWnd, false, lastTTHdir, WinUtil::getFileMaskString(WinUtil::allFilesMask).c_str(), nullptr, &WinUtil::guidGetTTH))
	{
		FileHashDlg dlg;
		dlg.filename = std::move(file);
		dlg.lastDir = lastTTHdir;
		dlg.DoModal();
		lastTTHdir = std::move(dlg.lastDir);
	}
	return 0;
}

LRESULT MainFrame::onDcLstFromFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring directory;
	if (WinUtil::browseDirectory(directory, m_hWnd, &WinUtil::guidDcLstFromFolder))
	{
		DclstGenDlg dlg(Text::fromT(directory));
		dlg.DoModal();
	}
	return 0;
}

void MainFrame::UpdateLayout(BOOL resizeBars /* = TRUE */)
{
	if ((!ClientManager::isStartup() || updateLayoutFlag) && !closing)
	{
		RECT rect;
		GetClientRect(&rect);
		// position bars and offset their dimensions
		UpdateBarsPosition(rect, resizeBars);
		
		if (ctrlStatus.IsWindow() && ctrlLastLines.IsWindow())
		{
			CRect sr;
			int w[STATUS_PART_LAST];
			
			int smState = ShareManager::getInstance()->getState();
			bool isHashing = smState == ShareManager::STATE_SCANNING_DIRS || smState == ShareManager::STATE_CREATING_FILELIST
				|| HashManager::getInstance()->isHashing();
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
			
			ctrlStatus.GetRect(STATUS_PART_1, &tabAwayRect);
			ctrlStatus.GetRect(STATUS_PART_7, &tabDownSpeedRect);
			ctrlStatus.GetRect(STATUS_PART_8, &tabUpSpeedRect);
		}

		CRect rc  = rect;
		CRect rc2 = rect;

		BOOL maximized;
		MDIGetActive(&maximized);
		BOOL showTabs = maximized && WinUtil::g_tabCtrl->getTabCount() != 0;
		if (ctrlTab.IsWindowVisible() != showTabs)
			ctrlTab.ShowWindow(showTabs ? SW_SHOW : SW_HIDE);
		if (showTabs)
		{
			switch (ctrlTab.getTabsPosition())
			{
				case Conf::TABS_TOP:
					rc.bottom = rc.top + ctrlTab.getHeight();
					rc2.top = rc.bottom;
					break;
				default:
					rc.top = rc.bottom - ctrlTab.getHeight();
					rc2.bottom = rc.top;
					break;
			}
			ctrlTab.MoveWindow(rc);
		}
		SetSplitterRect(rc2);
	}
}

LRESULT MainFrame::onOpenFileList(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
	if (wID >= IDC_OPEN_MY_LIST && wID <= IDC_OPEN_MY_LIST + 50)
	{
		CID cid;
		if (wID > IDC_OPEN_MY_LIST)
		{
			unsigned pos = wID - (IDC_OPEN_MY_LIST + 1);
			if (pos >= otherFileLists.size()) return 0;
			cid = otherFileLists[pos];
		}
		int64_t unused;
		string path = ShareManager::getInstance()->getBZXmlFile(cid, unused);
		if (path.empty()) return 0;
		const auto myUser = std::make_shared<User>(ClientManager::getMyCID(), ClientManager::getDefaultNick());
		myUser->setFlag(User::MYSELF);
		DirectoryListingFrame::openWindow(Text::toT(path),
			Util::emptyStringT, HintedUser(myUser, Util::emptyString), 0);
		return 0;
	}
	
	if (WinUtil::browseFile(file, m_hWnd, false, Text::toT(Util::getListPath()), WinUtil::getFileMaskString(WinUtil::fileListsMask).c_str()))
	{
		if (Util::isTorrentFile(file))
		{
			// do nothing
		}
		else
		{
			WinUtil::openFileList(file);
		}
	}
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
		return hash.getRoot().toBase32() == SettingsManager::instance.getUiSettings()->getString(Conf::PASSWORD);
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
				Util::toStringT(DownloadManager::getInstance()->getDownloadCount()) + _T(")\r\n") +
				TSTRING(U) + _T(' ') + Util::formatBytesT(UploadManager::getRunningAverage()) + _T('/') + TSTRING(S) + _T(" (") +
				Util::toStringT(UploadManager::getInstance()->getUploadCount()) + _T(")") + _T("\r\n") +
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
#ifdef OSVER_WIN_VISTA
	if (!SysVersion::isWin7Plus())
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
	wcsncpy(buttons[2].szTip, CWSTRING(HASH_REFRESH_FILE_LIST), sizeTip);
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

void MainFrame::toggleLockToolbars(BOOL state)
{
	REBARBANDINFO rbi = {};
	rbi.cbSize = sizeof(rbi);
	rbi.fMask  = RBBIM_STYLE;
	int count = ctrlRebar.GetBandCount();
	for (int i = 0; i < count; i++)
	{
		ctrlRebar.GetBandInfo(i, &rbi);
		if (state)
			rbi.fStyle |= RBBS_NOGRIPPER | RBBS_GRIPPERALWAYS;
		else
			rbi.fStyle &= ~(RBBS_NOGRIPPER | RBBS_GRIPPERALWAYS);
		ctrlRebar.SetBandInfo(i, &rbi);
	}
}

void MainFrame::toggleRebarBand(BOOL state, int bandNumber, int commandId, int setting)
{
	int bandIndex = ctrlRebar.IdToIndex(ATL_IDW_BAND_FIRST + bandNumber);
	ctrlRebar.ShowBand(bandIndex, state);
	UISetCheck(commandId, state);
	auto ss = SettingsManager::instance.getUiSettings();
	ss->setBool(setting, state != FALSE);
	ctrlToolbar.CheckButton(commandId, state);
}

LRESULT MainFrame::onViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	visToolbar = !visToolbar;
	toggleRebarBand(visToolbar, 1, ID_VIEW_TOOLBAR, Conf::SHOW_TOOLBAR);
	UpdateLayout();
	return 0;
}

LRESULT MainFrame::onViewWinampBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	visWinampBar = !visWinampBar;
	toggleRebarBand(visWinampBar, 3, ID_VIEW_MEDIA_TOOLBAR, Conf::SHOW_PLAYER_CONTROLS);
	UpdateLayout();
	return 0;
}

LRESULT MainFrame::onViewQuickSearchBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	visQuickSearch = !visQuickSearch;
	toggleRebarBand(visQuickSearch, 2, ID_VIEW_QUICK_SEARCH, Conf::SHOW_QUICK_SEARCH);
	UpdateLayout();
	return 0;
}

LRESULT MainFrame::onViewTopmost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto ss = SettingsManager::instance.getUiSettings();
	bool topmost = !ss->getBool(Conf::TOPMOST);
	UISetCheck(IDC_TOPMOST, topmost);
	ss->setBool(Conf::TOPMOST, topmost);
	toggleTopmost();
	return 0;
}

LRESULT MainFrame::onViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto ss = SettingsManager::instance.getUiSettings();
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	ss->setBool(Conf::SHOW_STATUSBAR, bVisible != FALSE);
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
	ctrlToolbar.CheckButton(ID_VIEW_TRANSFER_VIEW, bVisible);
	UpdateLayout();
}

LRESULT MainFrame::onViewTransferView(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto ss = SettingsManager::instance.getUiSettings();
	bool visible = !ss->getBool(Conf::SHOW_TRANSFERVIEW);
	ss->setBool(Conf::SHOW_TRANSFERVIEW, visible);
	toggleTransferView((BOOL) visible);
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
			HubFrame::closeAll(SettingsManager::instance.getUiSettings()->getInt(Conf::USER_THRESHOLD));
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
		case IDC_CLOSE_ALL_OFFLINE_DIR_LIST:
			DirectoryListingFrame::closeAllOffline();
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
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	bool state = !ss->getBool(Conf::THROTTLE_ENABLE);
	ss->setBool(Conf::THROTTLE_ENABLE, state);
	ss->unlockWrite();

	ThrottleManager::getInstance()->updateSettings();
	checkLimitsButton();
	return 0;
}

LRESULT MainFrame::onQuickConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ClientManager::isNickEmpty())
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
			Util::ParsedUrl url;
			Util::decodeUrl(Text::fromT(tmp), url);
			if (!Util::getHubProtocol(url.protocol))
			{
				MessageBox(CTSTRING_F(UNSUPPORTED_HUB_PROTOCOL, Text::toT(url.protocol)), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
				return 0;
			}
			if (url.host.empty())
			{
				MessageBox(CTSTRING(INCOMPLETE_FAV_HUB), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
				return 0;
			}
			const string query = std::move(url.query);
			const string formattedUrl = Util::formatDchubUrl(url);
			RecentHubEntry r;
			r.setServer(formattedUrl);
			r.setOpenTab("+");
			FavoriteManager::getInstance()->addRecent(r);
			HubFrame::openHubWindow(formattedUrl, Util::getQueryParam(query, "kp"));
		}
	}
	return 0;
}

void MainFrame::on(QueueManagerListener::PartialList, const HintedUser& user, const string& text) noexcept
{
	WinUtil::postSpeakerMsg(*this, BROWSE_LISTING, new DirectoryBrowseInfo(user, text));
}

void MainFrame::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string& dir, const DownloadPtr& download) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		auto extraFlags = qi->getExtraFlags();
		if (extraFlags & QueueItem::XFLAG_CLIENT_VIEW)
		{
			auto flags = qi->getFlags();
			if (flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST))
			{
				// This is a file listing, show it...
				auto dirInfo = new QueueManager::DirectoryListInfo(download->getHintedUser(), download->getPath() + qi->getListExt(), dir, download->getRunningAverage(), (flags & QueueItem::FLAG_DCLST_LIST) != 0);
				WinUtil::postSpeakerMsg(*this, DOWNLOAD_LISTING, dirInfo);
			}
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
			else if (extraFlags & QueueItem::XFLAG_TEXT_VIEW)
			{
				WinUtil::postSpeakerMsg(*this, VIEW_FILE_AND_DELETE, new tstring(Text::toT(qi->getTarget())));
			}
#endif
			else
			{
				WinUtil::openFile(Text::toT(qi->getTarget()));
			}
		}
	}
}

void MainFrame::on(SearchManagerListener::SR, const SearchResult& sr) noexcept
{
	if (!sr.getToken())
	{
		SearchFrame::broadcastSearchResult(sr);
		return;
	}

	if (sr.isAutoToken()) return;
	uint64_t id = SearchTokenList::instance.getTokenOwner(sr.getToken());
	if (!id) return;
	SearchFrame* frame = SearchFrame::getFramePtr(id);
	if (frame)
	{
		frame->addSearchResult(sr);
		SearchFrame::releaseFramePtr(frame);
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

void MainFrame::setShutDown(bool flag)
{
	if (flag)
		shutdownTime = Util::getTick() / 1000;
	shutdownEnabled = flag;
	setShutdownButton(flag);
}

LRESULT MainFrame::onDisableSounds(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool soundDisabled = ctrlToolbar.IsButtonChecked(IDC_DISABLE_SOUNDS) != FALSE;
	SettingsManager::instance.getUiSettings()->setBool(Conf::SOUNDS_DISABLED, soundDisabled);
	return 0;
}

LRESULT MainFrame::onDisablePopups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool popusDisabled = ctrlToolbar.IsButtonChecked(IDC_DISABLE_POPUPS) != FALSE;
	SettingsManager::instance.getUiSettings()->setBool(Conf::POPUPS_DISABLED, popusDisabled);
	return 0;
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
	else if (optToggleActiveWindow && !::IsIconic(hWnd))
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
	auto ss = SettingsManager::instance.getUiSettings();
	bool state = !ss->getBool(Conf::LOCK_TOOLBARS);
	UISetCheck(IDC_LOCK_TOOLBARS, (BOOL) state);
	ss->setBool(Conf::LOCK_TOOLBARS, state);
	toggleLockToolbars((BOOL) state);
	return 0;
}

LRESULT MainFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	if (reinterpret_cast<HWND>(wParam) == m_hWndToolBar)
	{
		tbMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	if (reinterpret_cast<HWND>(wParam) == m_hWndStatusBar)
	{
		POINT ptClient = pt;
		::ScreenToClient(m_hWndStatusBar, &ptClient);

		// AWAY
		if (ptClient.x >= tabAwayRect.left && ptClient.x <= tabAwayRect.right)
		{
			tabAwayMenu.CheckMenuItem(IDC_STATUS_AWAY_ON_OFF, MF_BYCOMMAND | (Util::getAway() ? MF_CHECKED : MF_UNCHECKED));
			tabAwayMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		if (ptClient.x >= tabDownSpeedRect.left && ptClient.x <= tabDownSpeedRect.right)
		{
			if (setSpeedLimit(false, 32, 6144))
				checkLimitsButton();
		}
		if (ptClient.x >= tabUpSpeedRect.left && ptClient.x <= tabUpSpeedRect.right)
		{
			if (setSpeedLimit(true, 32, 6144))
				checkLimitsButton();
		}
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT MainFrame::onStatusBarClick(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const POINT ptClient = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	// AWAY area
	if (ptClient.x >= tabAwayRect.left && ptClient.x <= tabAwayRect.right)
		setAway(!Util::getAway());
	bHandled = FALSE;
	return 0;
}

bool MainFrame::setSpeedLimit(bool upload, int minValue, int maxValue)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	int setting = upload ? Conf::MAX_UPLOAD_SPEED_LIMIT_NORMAL : Conf::MAX_DOWNLOAD_SPEED_LIMIT_NORMAL;
	ss->lockRead();
	int oldValue = ss->getInt(setting);
	ss->unlockRead();
	LimitEditDlg dlg(upload, Util::emptyStringT, oldValue, minValue, maxValue);
	if (dlg.DoModal() != IDOK) return false;
	ss->lockWrite();
	ss->setInt(setting, dlg.getLimit());
	if (dlg.getLimit())
		ss->setBool(Conf::THROTTLE_ENABLE, true);
	else if (!ss->getInt(Conf::MAX_UPLOAD_SPEED_LIMIT_NORMAL) && !ss->getInt(Conf::MAX_DOWNLOAD_SPEED_LIMIT_NORMAL))
		ss->setBool(Conf::THROTTLE_ENABLE, false);
	ss->unlockWrite();
	ThrottleManager::getInstance()->updateSettings();
	return true;
}

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

#ifdef BL_UI_FEATURE_EMOTICONS
LRESULT MainFrame::onAnimChangeFrame(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	CGDIImage* pImage = reinterpret_cast<CGDIImage*>(lParam);
	if (!pImage) return 0;
#ifdef DEBUG_GDI_IMAGE
	if (!CGDIImage::checkImage(pImage))
	{
		LogManager::message("Error in onAnimChangeFrame: invalid image 0x" + Util::toHexString(pImage), false);
		return 0;
	}
#endif
	pImage->DrawFrame();
	pImage->Release();
	return 0;
}
#endif // BL_UI_FEATURE_EMOTICONS

LRESULT MainFrame::onAddMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AddMagnet dlg;
	dlg.DoModal(m_hWnd);
	return 0;
}

void MainFrame::on(QueueManagerListener::FileExistsAction, const string& fileName, int64_t newSize, int64_t existingSize, time_t existingTime, QueueItem::Priority priority) noexcept
{
	FileExistsAction data;
	data.path = fileName;
	data.newSize = newSize;
	data.existingSize = existingSize;
	data.existingTime = existingTime;
	data.priority = priority;
	csFileExistsActions.lock();
	bool sendMsg = fileExistsActions.empty();
	fileExistsActions.emplace_back(std::move(data));
	csFileExistsActions.unlock();
	if (sendMsg) PostMessage(WM_SPEAKER, FILE_EXISTS_ACTION);
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
		int mediaPlayer = SettingsManager::instance.getUiSettings()->getInt(Conf::MEDIA_PLAYER);
		for (int i = 0; i < Conf::NumPlayers; i++)
			winampMenu.CheckMenuItem(ID_MEDIA_MENU_WINAMP_START + i, MF_BYCOMMAND | ((mediaPlayer == i) ? MF_CHECKED : MF_UNCHECKED));
		ctrlCmdBar.TrackPopupMenu(winampMenu, TPM_RIGHTBUTTON | TPM_VERTICAL, pt.x, pt.y);
	}
	return 0;
}

LRESULT MainFrame::onMediaMenu(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int selectedMenu = wID - ID_MEDIA_MENU_WINAMP_START;
	if (selectedMenu < Conf::NumPlayers && selectedMenu > -1)
		SettingsManager::instance.getUiSettings()->setInt(Conf::MEDIA_PLAYER, selectedMenu);
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
		bool shareFolder = true;
		if (SettingsManager::instance.getUiSettings()->getBool(Conf::CONFIRM_SHARE_FROM_SHELL))
		{
			tstring question = folder;
			question += _T("\r\n");
			question += TSTRING(SECURITY_SHARE_FROM_SHELL_QUESTION);
			shareFolder = MessageBox(question.c_str(), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
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

struct DefaultSearchUrl
{
	const char* name;
	const char* url;
	SearchUrl::Type type;
};

static const DefaultSearchUrl defaultSearchUrls[] =
{
	{ "Google",                "https://www.google.com/search?q=%s",  SearchUrl::KEYWORD  },
	{ "DuckDuckGo",            "https://www.duckduckgo.com/?q=%s",    SearchUrl::KEYWORD  },
	{ "he.net",                "https://bgp.he.net/dns/%s",           SearchUrl::HOSTNAME },
	{ "he.net",                "https://bgp.he.net/ip/%s",            SearchUrl::IP4      },
	{ "he.net",                "https://bgp.he.net/ip/%s",            SearchUrl::IP6      },
	{ "whatismyipaddress.com", "https://whatismyipaddress.com/ip/%s", SearchUrl::IP4      },
	{ "whatismyipaddress.com", "https://whatismyipaddress.com/ip/%s", SearchUrl::IP6      }
};

void MainFrame::addWebSearchUrls()
{
	SearchUrl::List data;
	for (int i = 0; i < _countof(defaultSearchUrls); ++i)
	{
		string description = defaultSearchUrls[i].type == SearchUrl::KEYWORD ?
			STRING_F(WEB_SEARCH_DEFAULT1, defaultSearchUrls[i].name) :
			STRING_F(WEB_SEARCH_DEFAULT2, defaultSearchUrls[i].name);
		data.emplace_back(SearchUrl{defaultSearchUrls[i].url, description, defaultSearchUrls[i].type});
	}
	FavoriteManager::getInstance()->setSearchUrls(data);
}

void MainFrame::on(UserManagerListener::OutgoingPrivateMessage, const UserPtr& to, const string& hint, const tstring& message) noexcept
{
	PrivateFrame::openWindow(nullptr, HintedUser(to, hint), Util::emptyString, Text::fromT(message));
}

void MainFrame::on(UserManagerListener::OpenHub, const string& url, const UserPtr& user) noexcept
{
	HubFrame::Settings cs;
	cs.server = url;
	bool isNew;
	HubFrame* hubFrame = HubFrame::openHubWindow(cs, &isNew);
	if (!isNew && hubFrame && user)
		hubFrame->selectCID(user->getCID());
}

void MainFrame::on(FinishedManagerListener::AddedDl, bool isFile, const FinishedItemPtr&) noexcept
{
	if (isFile) PostMessage(WM_SPEAKER, SOUND_NOTIF, Conf::SOUND_FINISHFILE);

}

void MainFrame::on(FinishedManagerListener::AddedUl, bool isFile, const FinishedItemPtr&) noexcept
{
	if (isFile) PostMessage(WM_SPEAKER, SOUND_NOTIF, Conf::SOUND_UPLOADFILE);
}

void MainFrame::on(QueueManagerListener::SourceAdded) noexcept
{
	PostMessage(WM_SPEAKER, SOUND_NOTIF, Conf::SOUND_SOURCEFILE);
}

void MainFrame::on(FavoriteManagerListener::SaveRecents) noexcept
{
	PostMessage(WM_SPEAKER, SAVE_RECENTS);
}

void MainFrame::onCommandCompleted(int type, uint64_t frameId, const string& text) noexcept
{
	FrameInfoText* data = new FrameInfoText;
	data->frameId = frameId;
	data->text = Text::toT(text);
	WinUtil::postSpeakerMsg(m_hWnd, FRAME_INFO_TEXT, data);
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

	return FALSE;
}
