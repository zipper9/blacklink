/*
 * Copyright (C) 2011-2013 Alexey Solomin, a.rainman on gmail pt com
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

#include "stdinc.h"
#include "CompatibilityManager.h"
#include "SysVersion.h"
#include "Text.h"
#include <shlwapi.h>
#include <winsock2.h>

#  if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
#    include <math.h> /* needed for _set_FMA3_enable */
#  endif

LONG CompatibilityManager::comCtlVersion = 0;
FINDEX_INFO_LEVELS CompatibilityManager::findFileLevel = FindExInfoStandard;
// http://msdn.microsoft.com/ru-ru/library/windows/desktop/aa364415%28v=vs.85%29.aspx
// FindExInfoBasic
//     The FindFirstFileEx function does not query the short file name, improving overall enumeration speed.
//     The data is returned in a WIN32_FIND_DATA structure, and the cAlternateFileName member is always a NULL string.
//     Windows Server 2008, Windows Vista, Windows Server 2003, and Windows XP:  This value is not supported until Windows Server 2008 R2 and Windows 7.
DWORD CompatibilityManager::findFileFlags = 0;
// http://msdn.microsoft.com/ru-ru/library/windows/desktop/aa364419%28v=vs.85%29.aspx
// Uses a larger buffer for directory queries, which can increase performance of the find operation.
// Windows Server 2008, Windows Vista, Windows Server 2003, and Windows XP:  This value is not supported until Windows Server 2008 R2 and Windows 7.

#if defined(OSVER_WIN_XP) || defined(OSVER_WIN_VISTA)
DWORD CompatibilityManager::compareFlags = 0;
#endif

string CompatibilityManager::getDefaultPath()
{
	const char* homePath = SysVersion::isWine() ? getenv("HOME") : getenv("SystemDrive");
	string defaultPath = homePath ? homePath : (SysVersion::isWine() ? "\\tmp" : "C:"); // FIXME
	defaultPath += '\\';
	return defaultPath;
}

static LONG getComCtlVersionFromOS()
{
	typedef HRESULT (CALLBACK* DLL_GET_VERSION)(DLLVERSIONINFO *);
	LONG result = 0;
	HMODULE comctl32dll = LoadLibrary(_T("comctl32.dll"));
	if (comctl32dll)
	{
		DLL_GET_VERSION dllGetVersion = (DLL_GET_VERSION) GetProcAddress(comctl32dll, "DllGetVersion");
		if (dllGetVersion)
		{
			DLLVERSIONINFO dvi;
			memset(&dvi, 0, sizeof(dvi));
			dvi.cbSize = sizeof(dvi);
			if (SUCCEEDED(dllGetVersion(&dvi)))
				result = MAKELONG(dvi.dwMinorVersion, dvi.dwMajorVersion);
		}
		else
			result = MAKELONG(0, 4);
		FreeLibrary(comctl32dll);
	}
	return result;
}

void CompatibilityManager::init()
{
#ifdef _WIN64
	// https://code.google.com/p/chromium/issues/detail?id=425120
	// FMA3 support in the 2013 CRT is broken on Vista and Windows 7 RTM (fixed in SP1). Just disable it.
	// fix crash https://drdump.com/Problem.aspx?ProblemID=102616
	//           https://drdump.com/Problem.aspx?ProblemID=102601
#if _MSC_VER >= 1800 && _MSC_VER < 1900
	_set_FMA3_enable(0);
#endif
#endif
	SysVersion::initialize();

	WSADATA wsaData = {0};
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	comCtlVersion = getComCtlVersionFromOS();
	if (SysVersion::isOsWin7Plus())
	{
		findFileLevel = FindExInfoBasic;
		findFileFlags = FIND_FIRST_EX_LARGE_FETCH;
#if defined(OSVER_WIN_XP) || defined(OSVER_WIN_VISTA)
		compareFlags = SORT_DIGITSASNUMBERS;
#endif
	}
}

bool CompatibilityManager::setThreadName(HANDLE h, const char* name)
{
	typedef HRESULT (WINAPI *fnSetThreadDescription)(HANDLE, const WCHAR*);
	static bool resolved;
	static fnSetThreadDescription pSetThreadDescription;
	if (!resolved)
	{
		HMODULE kernel32lib = GetModuleHandle(_T("kernel32"));
		if (kernel32lib) pSetThreadDescription = (fnSetThreadDescription) GetProcAddress(kernel32lib, "SetThreadDescription");
		resolved = true;
	}
	if (!pSetThreadDescription) return false;
	wstring wsName = Text::utf8ToWide(name);
	return SUCCEEDED(pSetThreadDescription(h, wsName.c_str()));
}
