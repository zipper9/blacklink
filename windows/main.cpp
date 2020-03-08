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
#include "../client/MappingManager.h"
#include "../client/CompatibilityManager.h"
#include "../client/ThrottleManager.h"
#include "../client/HashManager.h"
#include "../FlyFeatures/flyfeatures.h"

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
	COPYDATASTRUCT cpd = {0};
	cpd.cbData = sizeof(TCHAR) * (cmdLine.length() + 1);
	cpd.lpData = (void *)cmdLine.c_str();
	SendMessage(hOther, WM_COPYDATA, NULL, (LPARAM) & cpd);
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

static tstring g_sSplashText;
static int g_splash_size_x = 347;
static int g_splash_size_y = 93;
ExCImage* g_splash_png = nullptr;

LRESULT CALLBACK splashCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_PAINT && g_splash_png)
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);

		RECT rc;
		GetWindowRect(hwnd, &rc);
		OffsetRect(&rc, -rc.left, -rc.top);
		RECT rc2 = rc;
		RECT rc3 = rc;
		rc2.top = 2;
		rc2.right = rc2.right - 5;
		SetBkMode(ps.hdc, TRANSPARENT);
		rc3.top = rc3.bottom - 15;
		rc3.left = rc3.right - 120;
		
		HDC memDC = CreateCompatibleDC(ps.hdc);
		HGDIOBJ oldBitmap = SelectObject(memDC, *g_splash_png);
		BitBlt(ps.hdc, 0, 0, g_splash_size_x, g_splash_size_y, memDC, 0, 0, SRCCOPY);
		SelectObject(memDC, oldBitmap);
		DeleteDC(memDC);

		LOGFONT logFont = {0};
		GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(logFont), &logFont);
		_tcscpy(logFont.lfFaceName, _T("Tahoma"));
		logFont.lfHeight = 11;
		const HFONT hFont = CreateFontIndirect(&logFont);
		
		HGDIOBJ oldFont = SelectObject(ps.hdc, hFont);
		SetTextColor(ps.hdc, RGB(0, 0, 0));
		const tstring progress = g_sSplashText;
		DrawText(ps.hdc, progress.c_str(), progress.length(), &rc2, DT_CENTER | DT_NOPREFIX | DT_SINGLELINE);
		const tstring version = T_VERSIONSTRING;
		DrawText(ps.hdc, version.c_str(), version.length(), &rc3, DT_CENTER | DT_NOPREFIX | DT_SINGLELINE);
		SelectObject(ps.hdc, oldFont);
		DeleteObject(hFont);

		EndPaint(hwnd, &ps);
		return 0;
	}
	
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void splashTextCallBack(void* wnd, const tstring& text)
{
	g_sSplashText = text;
	if (wnd) RedrawWindow((HWND) wnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

CEdit g_dummy;
CWindow g_splash;
bool g_DisableSplash  = false;
bool g_DisableGDIPlus = false;
#ifdef _DEBUG
bool g_DisableTestPort = false;
#else
bool g_DisableTestPort = false;
#endif
#ifdef SSA_WIZARD_FEATURE
bool g_ShowWizard = false;
#endif

void CreateSplash()
{
	if (!g_DisableSplash)
	{
		CRect rc;
		rc.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
		rc.top = (rc.bottom / 2) - 80;
		
		rc.right = GetSystemMetrics(SM_CXFULLSCREEN);
		rc.left = rc.right / 2 - 85;
		
		g_dummy.Create(NULL, rc, getFlylinkDCAppCaptionWithVersionT().c_str(), WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		               ES_CENTER | ES_READONLY, WS_EX_STATICEDGE);
		g_splash.Create(_T("Static"), GetDesktopWindow(), g_splash.rcDefault, NULL, WS_POPUP | WS_VISIBLE | SS_USERITEM | WS_EX_TOOLWINDOW);
		
		if (!g_splash_png)
		{
			g_splash_png = new ExCImage;
			const string l_custom_splash = Util::getModuleCustomFileName("splash.png");
			if (File::isExist(l_custom_splash))
				g_splash_png->Load(Text::toT(l_custom_splash).c_str());
			else
				g_splash_png->LoadFromResource(IDR_SPLASH, _T("PNG"), _Module.get_m_hInst());
		}
		g_splash_size_x = g_splash_png->GetWidth();
		g_splash_size_y = g_splash_png->GetHeight();
		
		g_splash.SetFont((HFONT)GetStockObject(DEFAULT_GUI_FONT));
		
		rc.right = rc.left + g_splash_size_x;
		rc.bottom = rc.top + g_splash_size_y;

		g_splash.HideCaret();
		g_splash.SetWindowPos(NULL, &rc, SWP_SHOWWINDOW);
		g_splash.SetWindowLongPtr(GWLP_WNDPROC, (LONG_PTR)&splashCallback);
		g_splash.CenterWindow();
		g_splash.SetFocus();
		g_splash.RedrawWindow();
	}
}

void DestroySplash()
{
	safe_delete(g_splash_png);
	if (!g_DisableSplash && g_splash)
	{
		DestroyAndDetachWindow(g_splash);
		DestroyAndDetachWindow(g_dummy);
		g_sSplashText.clear();
	}
}

void GuiInit(void*)
{
	createFlyFeatures(); // [+] SSA
}

void GuiUninit(void*)
{
	deleteFlyFeatures(); // [+] SSA
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
	
	startup(splashTextCallBack, g_DisableSplash ? (void*)0 : (void*)g_splash.m_hWnd, GuiInit, NULL);
	startupFlyFeatures(splashTextCallBack, g_DisableSplash ? (void*)0 : (void*)g_splash.m_hWnd);
	WinUtil::initThemeIcons();
	static int nRet;
	{
		// !SMT!-fix this will ensure that GUI (wndMain) destroyed before client library shutdown (gui objects may call lib)
		MainFrame wndMain;
#ifdef SSA_WIZARD_FEATURE
		if (g_ShowWizard)
		{
			wndMain.SetWizardMode();
		}
#endif
		
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
			// nCmdShow - по идее, команда при запуске флая с настройками. Но мы её заюзаем как флаг
			// ДА для установки дефолтного стиля для окна.
			// ниже по коду видно, что если в этой команде есть что-то, то будет заюзан стиль из Настроек клиента.
			if (!nCmdShow)
				nCmdShow = SW_SHOWDEFAULT;  // SW_SHOWNORMAL
		}
		const int rtl = /*ResourceManager::getInstance()->isRTL() ? WS_EX_RTLREADING :*/ 0; // [!] IRainman fix.
		if (wndMain.CreateEx(NULL, rc, 0, rtl | WS_EX_APPWINDOW | WS_EX_WINDOWEDGE) == NULL)
		{
			ATLTRACE(_T("Main window creation failed!\n"));
			DestroySplash();
			nRet = 0; // [!] IRainman fix: correct unload.
		}
		else // [!] IRainman fix: correct unload.
		{
			// Backup & Archive Settings at Starup!!! Written by NightOrion.
			if (BOOLSETTING(STARTUP_BACKUP))
			{
				Util::backupSettings();
			}
			// End of BackUp...
			
			if (/*SETTING(PROTECT_PRIVATE) && */SETTING(PROTECT_PRIVATE_RND))
				SET_SETTING(PM_PASSWORD, Util::getRandomNick()); // Generate a random PM password
				
			wndMain.ShowWindow(((nCmdShow == SW_SHOWDEFAULT) || (nCmdShow == SW_SHOWNORMAL)) ? SETTING(MAIN_WINDOW_STATE) : nCmdShow);
			if (BOOLSETTING(MINIMIZE_ON_STARTUP))
			{
				wndMain.ShowWindow(SW_SHOWMINIMIZED);
			}
			
			DestroySplash();
			
			nRet = theLoop.Run(); // [2] https://www.box.net/shared/e198e9df5044db2a40f4
			
			_Module.RemoveMessageLoop();
		}  // [!] IRainman fix: correct unload.
	} // !SMT!-fix
	
	shutdown(GuiUninit, NULL/*, true*/);
#if defined(__PROFILER_ENABLED__)
	Profiler::dumphtml();
#endif
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	// TODO - flush file
#endif
#ifdef FLYLINKDC_USE_PROFILER_CS
	CFlyLockProfiler::print_stat();
#endif
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	CompatibilityManager::init();
	
#ifndef _DEBUG
	SingleInstance dcapp(_T("{BLDC-C8052503-235C-486A-A7A2-1D614A9A4242}"));
#else
	SingleInstance dcapp(_T("{BLDC-C8052503-235C-486A-A7A2-1D614A9A4241}"));
#endif
	bool l_is_multipleInstances = false;
	bool l_is_magnet = false;
	bool l_is_delay = false;
	bool l_is_openfile = false;
	bool l_is_sharefolder = false;
#ifdef _DEBUG
	// [!] brain-ripper
	// Dear developer, if you don't want to see Splash screen in debug version,
	// please specify parameter "/nologo" in project's
	// "Properties/Configuration Properties/Debugging/Command Arguments"
	
	//g_DisableSplash = true;
#endif
	extern bool g_DisableSQLJournal;
	extern bool g_UseWALJournal;
	extern bool g_EnableSQLtrace;
	extern bool g_UseSynchronousOff;
	extern bool g_UseCSRecursionLog;
	extern bool g_DisableUserStat;
	if (_tcsstr(lpstrCmdLine, _T("/nowal")) != NULL)
		g_DisableSQLJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_use_memory")) != NULL)
		g_DisableSQLJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_use_wal")) != NULL)
		g_UseWALJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_synchronous_off")) != NULL)
		g_UseSynchronousOff = true;
#if 0
	if (_tcsstr(lpstrCmdLine, _T("/hfs_ignore_file_size")) != NULL)
		ShareManager::setIgnoreFileSizeHFS();
#endif
	//if (_tcsstr(lpstrCmdLine, _T("/disable_users_stats")) != NULL)
	//  g_DisableUserStat = true;
	
	if (_tcsstr(lpstrCmdLine, _T("/sqltrace")) != NULL)
		g_EnableSQLtrace = true;
	if (_tcsstr(lpstrCmdLine, _T("/nologo")) != NULL)
		g_DisableSplash = true;
	if (_tcsstr(lpstrCmdLine, _T("/nogdiplus")) != NULL)
		g_DisableGDIPlus = true;
	if (_tcsstr(lpstrCmdLine, _T("/notestport")) != NULL)
		g_DisableTestPort = true;
	if (_tcsstr(lpstrCmdLine, _T("/q")) != NULL)
		l_is_multipleInstances = true;
	if (_tcsstr(lpstrCmdLine, _T("/nowine")) != NULL)
		CompatibilityManager::setWine(false);
	if (_tcsstr(lpstrCmdLine, _T("/forcewine")) != NULL)
		CompatibilityManager::setWine(true);
	if (_tcsstr(lpstrCmdLine, _T("/magnet")) != NULL)
		l_is_magnet = true;
		
	if (_tcsstr(lpstrCmdLine, _T("/c")) != NULL)
	{
		l_is_multipleInstances = true;
		l_is_delay = true;
	}
#ifdef SSA_WIZARD_FEATURE
	if (_tcsstr(lpstrCmdLine, _T("/wizard")) != NULL)
		g_ShowWizard = true;
#endif
	if (_tcsstr(lpstrCmdLine, _T("/open")) != NULL)
		l_is_openfile = true;
	if (_tcsstr(lpstrCmdLine, _T("/share")) != NULL)
		l_is_sharefolder = true;
#ifdef SSA_SHELL_INTEGRATION
	if (_tcsstr(lpstrCmdLine, _T("/installShellExt")) != NULL)
	{
		WinUtil::makeShellIntegration(false);
		return 0;
	}
	if (_tcsstr(lpstrCmdLine, _T("/uninstallShellExt")) != NULL)
	{
		WinUtil::makeShellIntegration(true);
		return 0;
	}
#endif
	if (_tcsstr(lpstrCmdLine, _T("/installStartup")) != NULL)
	{
		WinUtil::AutoRunShortCut(true);
		return 0;
	}
	if (_tcsstr(lpstrCmdLine, _T("/uninstallStartup")) != NULL)
	{
		WinUtil::AutoRunShortCut(false);
		return 0;
	}
	
	Util::initialize();
	ThrottleManager::newInstance();
	
	// First, load the settings! Any code running before will not get the value of SettingsManager!
	SettingsManager::newInstance();
	SettingsManager::getInstance()->load();
	const bool l_is_create_wide = SettingsManager::LoadLanguage();
	ResourceManager::startup(l_is_create_wide);
	SettingsManager::getInstance()->setDefaults(); // !SMT!-S: allow localized defaults in string settings
	LogManager::init();
	CreateSplash(); //[+]PPA
	
	TimerManager::newInstance();
	ClientManager::newInstance();
	CompatibilityManager::detectUncompatibleSoftware();
	ThrottleManager::getInstance()->startup(); // [+] IRainman fix.
	CompatibilityManager::caclPhysMemoryStat();
	if (dcapp.IsAnotherInstanceRunning())
	{
		// Allow for more than one instance...
		bool multiple = false;
		if (l_is_multipleInstances == false && l_is_magnet == false && l_is_openfile == false && l_is_sharefolder == false)
		{
			if (MessageBox(NULL, CTSTRING(ALREADY_RUNNING), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)   // [~] Drakon.
			{
				multiple = true;
			}
		}
		else
		{
			multiple = true;
		}
		
		if (l_is_delay == true)
		{
			Thread::sleep(2500);        // let's put this one out for a break
		}
		
		if (multiple == false || l_is_magnet == true || l_is_openfile == true || l_is_sharefolder == true)
		{
			HWND hOther = NULL;
			EnumWindows(searchOtherInstance, (LPARAM)&hOther);
			
			if (hOther != NULL)
			{
				// pop up
				::SetForegroundWindow(hOther);
				
				/*if( IsIconic(hOther)) {
				    //::ShowWindow(hOther, SW_RESTORE); // !SMT!-f - disable, it unlocks password-protected instance
				}*/
				sendCmdLine(hOther, lpstrCmdLine);
			}
			return FALSE;
		}
	}
	
	// For SHBrowseForFolder
	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	ATLASSERT(SUCCEEDED(hRes)); // [+] IRainman
	
	
#ifdef _DEBUG
	// It seamed not to be working for stack-overflow catch'n'process.
	// I'll research it further
	
	// [+] brain-ripper
	// Try to reserve some space for stack on stack-overflow event,
	// to successfully make crash-dump.
	// Note SetThreadStackGuarantee available on Win2003+ (including WinXP x64)
	HMODULE hKernelLib = LoadLibrary(L"kernel32.dll");
	
	if (hKernelLib)
	{
		typedef BOOL (WINAPI * SETTHREADSTACKGUARANTEE)(PULONG StackSizeInBytes);
		SETTHREADSTACKGUARANTEE SetThreadStackGuarantee = (SETTHREADSTACKGUARANTEE)GetProcAddress(hKernelLib, "SetThreadStackGuarantee");
		
		if (SetThreadStackGuarantee)
		{
			ULONG len = 1024 * 8;
			SetThreadStackGuarantee(&len);
		}
		
		FreeLibrary(hKernelLib);
	}
#endif
	
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
