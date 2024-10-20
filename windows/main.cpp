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
#include "ResourceLoader.h"
#include "PopupManager.h"
#include "ToolbarManager.h"
#include "ThemeManager.h"
#include "WinFirewall.h"
#include "BackingStore.h"
#include "UxThemeLib.h"
#include "DwmApiLib.h"
#include "../client/DCPlusPlus.h"
#include "../client/MappingManager.h"
#include "../client/CompatibilityManager.h"
#include "../client/ThrottleManager.h"
#include "../client/HashManager.h"
#include "../client/AppPaths.h"
#include "../client/FormatUtil.h"
#include "../client/SysVersion.h"
#include "../client/AppStats.h"
#include "../client/ProfileLocker.h"
#include "../client/PathUtil.h"
#include "../client/SettingsUtil.h"
#include "../client/ConfCore.h"
#include "SplashWindow.h"
#include "CommandLine.h"

#ifdef BL_UI_FEATURE_EMOTICONS
#include "Emoticons.h"
#ifdef DEBUG_GDI_IMAGE
#include "../GdiOle/GDIImage.h"
#endif
#endif

#if !defined(_DEBUG) && (defined(_M_IX86) || defined(_M_X64))
#define USE_CRASH_HANDLER
#endif

#ifdef USE_CRASH_HANDLER
extern LONG __stdcall DCUnhandledExceptionFilter(LPEXCEPTION_POINTERS e);
#endif

CAppModule _Module;

static void sendCmdLine(HWND hOther, LPTSTR lpstrCmdLine)
{
	const tstring cmdLine = lpstrCmdLine;
	COPYDATASTRUCT cpd;
	cpd.dwData = 0;
	cpd.cbData = sizeof(TCHAR) * (cmdLine.length() + 1);
	cpd.lpData = const_cast<TCHAR*>(cmdLine.c_str());
	SendMessage(hOther, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cpd));
}

BOOL CALLBACK searchOtherInstance(HWND hWnd, LPARAM lParam)
{
	DWORD_PTR result;
	LRESULT ok = ::SendMessageTimeout(hWnd, WMU_WHERE_ARE_YOU, 0, 0,
	                                  SMTO_BLOCK | SMTO_ABORTIFHUNG, 5000, &result);
	if (ok == 0)
		return TRUE;
	if (result == WMU_WHERE_ARE_YOU)
	{
		// found it
		HWND *target = (HWND *)lParam;
		*target = hWnd;
		return FALSE;
	}
	return TRUE;
}

static void dbErrorCallback(const string& message, bool forceExit)
{
	static int guard;
	if (!guard)
	{
		guard++;
		MessageBox(NULL, Text::toT(message).c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
		guard--;
	}
}

static SplashWindow splash;

static void splashTextCallBack(void*, const tstring& text)
{
	if (splash.m_hWnd)
	{
		splash.setProgressText(text);
		splash.Invalidate();
	}
}

static bool g_DisableSplash  = false;
static bool g_DisableGDIPlus = false;
#ifdef _DEBUG
bool g_DisableTestPort = false;
#else
bool g_DisableTestPort = false;
#endif

class SplashThread : public Thread
{
	public:
		SplashThread() { initEvent.create(); }
		void waitInitialized() { initEvent.wait(); }

	protected:
		virtual int run()
		{
			RECT rc;
			rc.left = (GetSystemMetrics(SM_CXFULLSCREEN) - SplashWindow::WIDTH) / 2;
			rc.top = (GetSystemMetrics(SM_CYFULLSCREEN) - SplashWindow::HEIGHT) / 2;
			rc.right = rc.left + SplashWindow::WIDTH;
			rc.bottom = rc.top + SplashWindow::HEIGHT;
			splash.setHasBorder(true);
			splash.Create(GetDesktopWindow(), rc, getAppNameVerT().c_str(), WS_POPUP | WS_VISIBLE);
			initEvent.notify();
			while (true)
			{
				MSG msg;
				BOOL ret = GetMessage(&msg, nullptr, 0, 0);
				if (ret == FALSE || ret == -1) break;
				DispatchMessage(&msg);
			}
			splash.DestroyWindow();
			return 0;
		}

		WinEvent<FALSE> initEvent;
};

static SplashThread splashThread;

void CreateSplash()
{
	if (g_DisableSplash) return;
	splashThread.start(0, "SplashThread");
	splashThread.waitInitialized();
}

void DestroySplash()
{
	if (!g_DisableSplash && splashThread.isRunning())
	{
		PostMessage(splash, WM_QUIT, 0, 0);
		splashThread.join();
	}
}

void GuiInit(void*)
{
	ThemeManager::newInstance();
}

void GuiUninit(void*)
{
	ThemeManager::deleteInstance();
	PopupManager::deleteInstance();
}

static int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	// Initialize GDI+.
	static Gdiplus::GdiplusStartupInput g_gdiplusStartupInput;
	static ULONG_PTR g_gdiplusToken = 0;
	Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);
#endif // IRAINMAN_INCLUDE_GDI_INIT

	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	startup(splashTextCallBack, nullptr, GuiInit, nullptr, dbErrorCallback);
	ThemeManager::getInstance()->load();
	static int nRet;
	{
		auto ss = SettingsManager::instance.getUiSettings();
		// !SMT!-fix this will ensure that GUI (wndMain) destroyed before client library shutdown (gui objects may call lib)
		MainFrame wndMain;

		CRect rc = wndMain.rcDefault;
		int xpos = ss->getInt(Conf::MAIN_WINDOW_POS_X);
		int ypos = ss->getInt(Conf::MAIN_WINDOW_POS_Y);
		int width = ss->getInt(Conf::MAIN_WINDOW_SIZE_X);
		int height = ss->getInt(Conf::MAIN_WINDOW_SIZE_Y);
		if (xpos != CW_USEDEFAULT && ypos != CW_USEDEFAULT && width != CW_USEDEFAULT && height != CW_USEDEFAULT)
		{
			rc.left = xpos;
			rc.top = ypos;
			rc.right = rc.left + xpos;
			rc.bottom = rc.top + ypos;
		}

		// Now, let's ensure we have sane values here...
		if (rc.left < -10 || rc.top < -10 || rc.right - rc.left < 800 || rc.bottom - rc.top < 600)
		{
			rc = wndMain.rcDefault;
			if (!nCmdShow)
				nCmdShow = SW_SHOWDEFAULT;  // SW_SHOWNORMAL
		}
		if (wndMain.CreateEx(NULL, rc, 0, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE) == NULL)
		{
			ATLTRACE(_T("Main window creation failed!\n"));
			DestroySplash();
			nRet = 0;
		}
		else
		{
			if (ss->getBool(Conf::STARTUP_BACKUP))
			{
				splashTextCallBack(nullptr, TSTRING(STARTUP_BACKUP_SETTINGS));
				Util::backupSettings();
			}

			SetForegroundWindow(wndMain);
			DestroySplash();

			wndMain.ShowWindow((nCmdShow == SW_SHOWDEFAULT || nCmdShow == SW_SHOWNORMAL) ? ss->getInt(Conf::MAIN_WINDOW_STATE) : nCmdShow);
			if (ss->getBool(Conf::MINIMIZE_ON_STARTUP))
				wndMain.ShowWindow(SW_SHOWMINIMIZED);

			nRet = theLoop.Run();
			_Module.RemoveMessageLoop();
		}
	}

	shutdown(GuiUninit, NULL/*, true*/);
#ifdef BL_UI_FEATURE_EMOTICONS
	emoticonPackList.clear();
#ifdef DEBUG_GDI_IMAGE
	if (CGDIImage::getImageCount()) BaseThread::sleep(1000);
	CGDIImage::stopTimers();
	dcdebug("CGDIImage instance count: %d\n", (int) CGDIImage::getImageCount());
#endif
#endif
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif
	return nRet;
}

ParsedCommandLine cmdLine;

BOOL setProcessDPIAware()
{
	HMODULE hMod = GetModuleHandle(_T("user32.dll"));
	if (!hMod) return FALSE;
	BOOL (WINAPI *pSetProcessDPIAware)() = (BOOL (WINAPI *)()) GetProcAddress(hMod, "SetProcessDPIAware");
	if (pSetProcessDPIAware) return pSetProcessDPIAware();
	return FALSE;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	ASSERT_MAIN_THREAD_INIT();
	SetErrorMode(SEM_FAILCRITICALERRORS);
	CompatibilityManager::init();
	CompatibilityManager::setThreadName(GetCurrentThread(), "Main");

#ifndef _DEBUG
	SingleInstance dcapp(_T("{BLDC-C8052503-235C-486A-A7A2-1D614A9A4242}"));
#else
	SingleInstance dcapp(_T("{BLDC-C8052503-235C-486A-A7A2-1D614A9A4241}"));
#endif
	extern bool g_EnableSQLtrace;
	extern bool g_UseSynchronousOff;
	parseCommandLine(cmdLine, nullptr);
	if (cmdLine.sqliteSyncOff) g_UseSynchronousOff = true;
	if (cmdLine.sqliteTrace) g_EnableSQLtrace = true;
	g_DisableSplash = cmdLine.disableSplash;
	g_DisableGDIPlus = cmdLine.disableGDIPlus;
	g_DisableTestPort = cmdLine.disablePortTest;
	if (cmdLine.setWine) SysVersion::setWine(cmdLine.setWine == 1);
#ifdef SSA_SHELL_INTEGRATION
	if (cmdLine.installShellExt)
	{
		WinUtil::registerShellExt(cmdLine.installShellExt == 1);
		return 0;
	}
#endif
	if (cmdLine.installAutoRunShortcut)
	{
		WinUtil::autoRunShortcut(cmdLine.installAutoRunShortcut == 1);
		return 0;
	}
	if (cmdLine.addFirewallEx)
	{
		::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		const tstring appPath = Util::getModuleFileName();
		WinFirewall fw;
		HRESULT hr;
		fw.initialize(&hr);
		fw.addApplicationW(appPath, getAppNameT(), true, &hr);
		return 0;
	}

	Util::initialize();
	Util::initFormatParams();

	Conf::initCoreSettings();
	Conf::initUiSettings();

	ToolbarManager::newInstance();
	SettingsManager::instance.addListener(ToolbarManager::getInstance());

	SettingsManager::instance.loadSettings();

	LogManager::init();
	Util::loadLanguage();
	LogManager::message(AppStats::getStartupMessage());

	// Allow localized defaults in string settings
	Conf::updateCoreSettingsDefaults();
	Conf::updateUiSettingsDefaults();
	Conf::processCoreSettings();
	Conf::processUiSettings();

	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::APP_DPI_AWARE)) setProcessDPIAware();

	ProfileLocker profileLocker;
	profileLocker.setPath(Util::getConfigPath());
	if (!profileLocker.lock())
	{
		if (Util::isLocalMode())
		{
			tstring path = Text::toT(Util::getConfigPath());
			Util::removePathSeparator(path);
			MessageBox(NULL, CTSTRING_F(PROFILE_LOCKED, path), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
		}
		else
			MessageBox(NULL, CTSTRING(ALREADY_RUNNING_LOCKED), getAppNameVerT().c_str(), MB_ICONINFORMATION | MB_OK);
		SettingsManager::instance.removeListeners();
		return 1;
	}

	ThrottleManager::newInstance();
	TimerManager::newInstance();
	ClientManager::newInstance();
	ThrottleManager::getInstance()->startup();

	if (dcapp.IsAnotherInstanceRunning())
	{
		// Allow for more than one instance...
		bool multiple = false;
		bool hasAction = !cmdLine.openFile.empty() || !cmdLine.openMagnet.empty() || !cmdLine.openHub.empty() || !cmdLine.shareFolder.empty();
		if (!cmdLine.multipleInstances && !hasAction)
		{
			if (MessageBox(NULL, CTSTRING(ALREADY_RUNNING_NOT_LOCKED), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)
				multiple = true;
		}
		else
			multiple = true;

		if (cmdLine.delay)
			Thread::sleep(2500);        // let's put this one out for a break

		if (!multiple || hasAction)
		{
			HWND hOther = NULL;
			EnumWindows(searchOtherInstance, (LPARAM)&hOther);

			if (hOther != NULL)
			{
				// pop up
				::SetForegroundWindow(hOther);
				sendCmdLine(hOther, lpstrCmdLine);
			}
			SettingsManager::instance.removeListeners();
			return FALSE;
		}
	}

	CreateSplash();

	// For SHBrowseForFolder
	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	ATLASSERT(SUCCEEDED(hRes));

#ifdef USE_CRASH_HANDLER
	LPTOP_LEVEL_EXCEPTION_FILTER pOldSEHFilter = nullptr;
	pOldSEHFilter = SetUnhandledExceptionFilter(&DCUnhandledExceptionFilter);
#endif

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES |
	                      ICC_TAB_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES);   // add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	UxThemeLib::instance.init();
	if (SysVersion::isOsWin11Plus()) DwmApiLib::instance.init();
	BackingStore::globalInit();

	HINSTANCE hRichEditNew;
	hRichEditNew = ::LoadLibrary(_T("MSFTEDIT.DLL"));
/*
	hRichEditOld = ::LoadLibrary(_T("RICHED20.DLL"));
	if (!hRichEditOld)
		hRichEditOld = ::LoadLibrary(_T("RICHED32.DLL"));
*/
	const int nRet = Run(lpstrCmdLine, nCmdShow);

	if (hRichEditNew) ::FreeLibrary(hRichEditNew);
/*
	if (hRichEditOld) ::FreeLibrary(hRichEditOld);
*/
	_Module.Term();
	::CoUninitialize();
	DestroySplash();

#ifdef USE_CRASH_HANDLER
	if (pOldSEHFilter)
		SetUnhandledExceptionFilter(pOldSEHFilter);
#endif

	return nRet;
}
