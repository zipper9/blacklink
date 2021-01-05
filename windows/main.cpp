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
#include <delayimp.h>
#include <atlgdiraii.h>

#include "Resource.h"
#include "MainFrm.h"
#include "ResourceLoader.h"
#include "PopupManager.h"
#include "ToolbarManager.h"
#include "ThemeManager.h"
#include "../client/DCPlusPlus.h"
#include "../client/MappingManager.h"
#include "../client/CompatibilityManager.h"
#include "../client/ThrottleManager.h"
#include "../client/HashManager.h"
#include "SplashWindow.h"
#include "CommandLine.h"

#ifndef _DEBUG
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
	if (result == WMU_WHERE_ARE_YOU) //-V104
	{
		// found it
		HWND *target = (HWND *)lParam;
		*target = hWnd;
		return FALSE;
	}
	return TRUE;
}

static SplashWindow splash;

void splashTextCallBack(void*, const tstring& text)
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
	splashThread.start();
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

#ifdef SCALOLAZ_MANY_MONITORS
int ObtainMonitors() // Count of a Display Devices
{
	DWORD i = 0;
	int count = 0;
	//DWORD j = 0;
	DISPLAY_DEVICE dc = {0};
	
	dc.cb = sizeof(dc);
	while (EnumDisplayDevices(NULL, i, &dc, EDD_GET_DEVICE_INTERFACE_NAME) != 0)
		// Read More: https://msdn.microsoft.com/ru-ru/library/windows/desktop/dd162609(v=vs.85).aspx
	{
		if ((dc.StateFlags & DISPLAY_DEVICE_ACTIVE) && !(dc.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
		{
			// Read More: https://msdn.microsoft.com/ru-ru/library/windows/desktop/dd183569(v=vs.85).aspx
			/*  // Этот кусок до счётчика Реальных Дисплеев нам не нужен, но оставлю. Вдруг приспичит разрешение дисплеев и их имена брать.
			DEVMODE dm;
			j = 0;
			while (EnumDisplaySettings(dc.DeviceName, j, &dm) != 0)
			{
			    //Запоминаем DEVMODE dm, чтобы потом мы могли его найти и использовать
			    //в ChangeDisplaySettings, когда будем инициализировать окно
			    ++j;
			}
			*/
			++count;    // Count for REAL Devices
		}
		++i;
	}
	return count;
}
#endif
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
	
	startup(splashTextCallBack, nullptr, GuiInit, nullptr);
	ThemeManager::getInstance()->load();
	static int nRet;
	{
		// !SMT!-fix this will ensure that GUI (wndMain) destroyed before client library shutdown (gui objects may call lib)
		MainFrame wndMain;
		
		CRect rc = wndMain.rcDefault;
		
		if ((SETTING(MAIN_WINDOW_POS_X) != CW_USEDEFAULT) &&
		        (SETTING(MAIN_WINDOW_POS_Y) != CW_USEDEFAULT) &&
		        (SETTING(MAIN_WINDOW_SIZE_X) != CW_USEDEFAULT) &&
		        (SETTING(MAIN_WINDOW_SIZE_Y) != CW_USEDEFAULT))
		{
		
			/*
			
			Пока не работает на мульти-мониках
			            RECT l_desktop = {0,0,1024,600};
			            GetWindowRect(GetDesktopWindow(), &l_desktop);
			            rc.left = SETTING(MAIN_WINDOW_POS_X);
			            bool l_is_skip = false;
			            if (rc.left < 0 || rc.left >= l_desktop.left - 50)
			            {
			                rc.left = 0;
			                rc.right = l_desktop.right;
			                l_is_skip = true;
			            }
			            rc.top = SETTING(MAIN_WINDOW_POS_Y);
			            if (rc.top < 0 || rc.top >= l_desktop.top - 50)
			            {
			                rc.top = 0;
			                rc.bottom = l_desktop.bottom;
			                l_is_skip = true;
			            }
			            if (l_is_skip == false)
			            {
			                rc.right = rc.left + SETTING(MAIN_WINDOW_SIZE_X);
			                if (rc.right < 0 || rc.right >= l_desktop.right - l_desktop.left)
			                    rc.right = l_desktop.right;
			                rc.bottom = rc.top + SETTING(MAIN_WINDOW_SIZE_Y);
			                if (rc.bottom < 0 || rc.bottom >= l_desktop.bottom - l_desktop.top)
			                    rc.bottom = l_desktop.bottom;
			            }
			*/
			rc.left = SETTING(MAIN_WINDOW_POS_X);
			rc.top = SETTING(MAIN_WINDOW_POS_Y);
			rc.right = rc.left + SETTING(MAIN_WINDOW_SIZE_X);
			rc.bottom = rc.top + SETTING(MAIN_WINDOW_SIZE_Y);
		}
		
		// Now, let's ensure we have sane values here...
#ifdef SCALOLAZ_MANY_MONITORS
		const int l_mons = ObtainMonitors();    // Get  Phisical Display Drives Count: 1 or more
#endif
		if (((rc.left < -10) || (rc.top < -10) || (rc.right - rc.left < 500) || ((rc.bottom - rc.top) < 300))
#ifdef SCALOLAZ_MANY_MONITORS
		        && l_mons < 2
		        || ((rc.left < -4000) || (rc.right > 5000) || (rc.top < -10) || (rc.bottom > 4000))
#endif
		   )
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
			if (BOOLSETTING(STARTUP_BACKUP))
			{
				splashTextCallBack(nullptr, TSTRING(STARTUP_BACKUP_SETTINGS));
				Util::backupSettings();
			}
			
			if (/*SETTING(PROTECT_PRIVATE) && */SETTING(PROTECT_PRIVATE_RND))
				SET_SETTING(PM_PASSWORD, Util::getRandomNick()); // Generate a random PM password
				
			SetForegroundWindow(wndMain);
			DestroySplash();

			wndMain.ShowWindow((nCmdShow == SW_SHOWDEFAULT || nCmdShow == SW_SHOWNORMAL) ? SETTING(MAIN_WINDOW_STATE) : nCmdShow);
			if (BOOLSETTING(MINIMIZE_ON_STARTUP))
				wndMain.ShowWindow(SW_SHOWMINIMIZED);

			nRet = theLoop.Run();
			_Module.RemoveMessageLoop();
		}
	}
	
	shutdown(GuiUninit, NULL/*, true*/);
#if defined(__PROFILER_ENABLED__)
	Profiler::dumphtml();
#endif
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif
	return nRet;
}

ParsedCommandLine cmdLine;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	CompatibilityManager::init();
	
#ifndef _DEBUG
	SingleInstance dcapp(_T("{BLDC-C8052503-235C-486A-A7A2-1D614A9A4242}"));
#else
	SingleInstance dcapp(_T("{BLDC-C8052503-235C-486A-A7A2-1D614A9A4241}"));
#endif
	extern bool g_DisableSQLJournal;
	extern bool g_UseWALJournal;
	extern bool g_EnableSQLtrace;
	extern bool g_UseSynchronousOff;
	extern bool g_UseCSRecursionLog;
	parseCommandLine(cmdLine, nullptr);
	if (cmdLine.sqliteNoWAL || cmdLine.sqliteUseMemory) g_DisableSQLJournal = true;
	if (cmdLine.sqliteUseWAL) g_UseWALJournal = true;
	if (cmdLine.sqliteSyncOff) g_UseSynchronousOff = true;
	if (cmdLine.sqliteTrace) g_EnableSQLtrace = true;
	g_DisableSplash = cmdLine.disableSplash;
	g_DisableGDIPlus = cmdLine.disableGDIPlus;
	g_DisableTestPort = cmdLine.disablePortTest;
	if (cmdLine.setWine) CompatibilityManager::setWine(cmdLine.setWine == 1);
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
	
	Util::initialize();
	ThrottleManager::newInstance();
	ToolbarManager::newInstance();
	
	SettingsManager::newInstance();
	SettingsManager::getInstance()->addListener(ToolbarManager::getInstance());
	SettingsManager::getInstance()->load();
	LogManager::init();

	SettingsManager::loadLanguage();
	SettingsManager::getInstance()->setDefaults(); // !SMT!-S: allow localized defaults in string settings
	
	CreateSplash();	
	
	TimerManager::newInstance();
	ClientManager::newInstance();
	CompatibilityManager::detectIncompatibleSoftware();
	ThrottleManager::getInstance()->startup();
	CompatibilityManager::caclPhysMemoryStat();
	if (dcapp.IsAnotherInstanceRunning())
	{
		// Allow for more than one instance...
		bool multiple = false;
		bool hasAction = !cmdLine.openFile.empty() || !cmdLine.openMagnet.empty() || !cmdLine.openHub.empty() || !cmdLine.shareFolder.empty();
		if (!cmdLine.multipleInstances && !hasAction)
		{
			if (MessageBox(NULL, CTSTRING(ALREADY_RUNNING), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)
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
			DestroySplash();
			return FALSE;
		}
	}
	
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
