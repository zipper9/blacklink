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

#  if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
#    include <math.h> /* needed for _set_FMA3_enable */
#  endif

#include <winnt.h>
#include <shlwapi.h>
#ifdef OSVER_WIN_VISTA
#define PSAPI_VERSION 1
#endif
#include <psapi.h>
#include "SysVersion.h"
#include "FormatUtil.h"
#include "Socket.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CompatibilityManager.h"
#include "DatabaseManager.h"
#include "ClientManager.h"
#include "ShareManager.h"

#ifndef GR_GDIOBJECTS_PEAK
#define GR_GDIOBJECTS_PEAK  2
#endif

#ifndef GR_USEROBJECTS_PEAK
#define GR_USEROBJECTS_PEAK 4
#endif


string CompatibilityManager::g_incopatibleSoftwareList;
string CompatibilityManager::g_startupInfo;
DWORDLONG CompatibilityManager::g_TotalPhysMemory;
DWORDLONG CompatibilityManager::g_FreePhysMemory;
LONG CompatibilityManager::g_comCtlVersion = 0;
DWORD CompatibilityManager::g_oldPriorityClass = 0;
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

	g_comCtlVersion = getComCtlVersionFromOS();
	generateSystemInfoForApp();
	if (SysVersion::isOsWin7Plus())
	{
		findFileLevel = FindExInfoBasic;
		findFileFlags = FIND_FIRST_EX_LARGE_FETCH;
#if defined(OSVER_WIN_XP) || defined(OSVER_WIN_VISTA)
		compareFlags = SORT_DIGITSASNUMBERS;
#endif
	}
}

struct DllInfo
{
	const TCHAR* dllName;
	const char* info;
};

// TODO - config
static const DllInfo g_IncompatibleDll[] =
{
	_T("nvappfilter.dll"), "NVIDIA ActiveArmor (NVIDIA Firewall)",
	_T("nvlsp.dll"), "NVIDIA Application Filter",
	_T("nvlsp64.dll"), "NVIDIA Application Filter",
	_T("chtbrkg.dll"), "Adskip or Internet Download Manager integration",
	_T("adguard.dll"), "Adguard", // Latest verion 5.1 not bad.
	_T("pcapwsp.dll"), "ProxyCap",
	_T("NetchartFilter.dll"), "NetchartFilter",
	_T("vlsp.dll"), "Venturi Wireless Software",
	_T("radhslib.dll"), "Naomi web filter",
	_T("AmlMaple.dll"), "Aml Maple",
	_T("sdata.dll"), "Trojan-PSW.Win32.LdPinch.ajgw",
	_T("browsemngr.dll"), "Browser Manager",
	_T("owexplorer_10616.dll"), "Overwolf Overlay",
	_T("owexplorer_10615.dll"), "Overwolf Overlay",
	_T("owexplorer_20125.dll"), "Overwolf Overlay",
	_T("owexplorer_2006.dll"), "Overwolf Overlay",
	_T("owexplorer_20018.dll"), "Overwolf Overlay",
	_T("owexplorer_2000.dll"), "Overwolf Overlay",
	_T("owexplorer_20018.dll"), "Overwolf Overlay",
	_T("owexplorer_20015.dll"), "Overwolf Overlay",
	_T("owexplorer_1069.dll"), "Overwolf Overlay",
	_T("owexplorer_10614.dll"), "Overwolf Overlay",
	_T("am32_33707.dll"), "Ad Muncher",
	_T("am32_32562.dll"), "Ad Muncher",
	_T("am64_33707.dll"), "Ad Muncher x64",
	_T("am64_32130.dll"), "Ad Muncher x64",
	_T("browserprotect.dll"), "Browserprotect",
	_T("searchresultstb.dll"), "DTX Toolbar",
	_T("networx.dll"), "NetWorx",
	nullptr, nullptr
};

void CompatibilityManager::detectIncompatibleSoftware()
{
	const DllInfo* currentDllInfo = g_IncompatibleDll;
	// TODO http://stackoverflow.com/questions/420185/how-to-get-the-version-info-of-a-dll-in-c
	for (; currentDllInfo->dllName; ++currentDllInfo)
	{
		const HMODULE hIncompatibleDll = GetModuleHandle(currentDllInfo->dllName);
		if (hIncompatibleDll)
		{
			g_incopatibleSoftwareList += '\n';
			const auto l_dll = Text::fromT(currentDllInfo->dllName);
			g_incopatibleSoftwareList += l_dll;
			if (currentDllInfo->info)
			{
				g_incopatibleSoftwareList += " - ";
				g_incopatibleSoftwareList += currentDllInfo->info;
			}
		}
	}
	g_incopatibleSoftwareList.shrink_to_fit();
}

string CompatibilityManager::getIncompatibleSoftwareMessage()
{
	if (isIncompatibleSoftwareFound())
	{
		string temp;
		temp.resize(32768);
		temp.resize(sprintf_s(&temp[0], temp.size() - 1, CSTRING(INCOMPATIBLE_SOFTWARE_FOUND), g_incopatibleSoftwareList.c_str()));
		return temp;
	}
	return Util::emptyString;
}

void CompatibilityManager::generateSystemInfoForApp()
{
	g_startupInfo = getAppNameVer();
	g_startupInfo += " is starting\n"
	                 "\tNumber of processors: " + Util::toString(SysVersion::getCPUCount()) + ".\n" +
	                 + "\tProcessor type: ";
	g_startupInfo += SysVersion::getProcArchString();
	g_startupInfo += ".\n";

	g_startupInfo += getGlobalMemoryStatusMessage();
	g_startupInfo += "\n";

	g_startupInfo += "\tRunning in ";
#ifndef _WIN64
	if (SysVersion::isWow64())
		g_startupInfo += "Windows WOW64\n\tPlease consider using the x64 version!";
	else
#endif
		g_startupInfo += "Windows native ";
	g_startupInfo += "\n\t";

	g_startupInfo += SysVersion::getFormattedOsName();
	g_startupInfo += " (" + SysVersion::getFormattedOsVerNum() + ")";
	g_startupInfo += "\n\n";
	Util::convertToDos(g_startupInfo);
}

LONG CompatibilityManager::getComCtlVersionFromOS()
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

bool CompatibilityManager::getGlobalMemoryStatus(MEMORYSTATUSEX* status)
{
	typedef BOOL (WINAPI* GLOBAL_MEMORY_STATUS_EX)(MEMORYSTATUSEX*);
	static GLOBAL_MEMORY_STATUS_EX globalMemoryStatusEx;
	static bool resolved;
	if (!resolved)
	{
		HMODULE kernel32lib = GetModuleHandle(_T("kernel32"));
		if (kernel32lib) globalMemoryStatusEx = (GLOBAL_MEMORY_STATUS_EX) GetProcAddress(kernel32lib, "GlobalMemoryStatusEx");
		resolved = true;
	}
	if (globalMemoryStatusEx)
		return globalMemoryStatusEx(status) != FALSE;
	return false;
}

string CompatibilityManager::getGlobalMemoryStatusMessage()
{
	MEMORYSTATUSEX curMemoryInfo = {0};
	curMemoryInfo.dwLength = sizeof(curMemoryInfo);

	if (getGlobalMemoryStatus(&curMemoryInfo))
	{
		g_TotalPhysMemory = curMemoryInfo.ullTotalPhys;
		string memoryInfo = "\tMemory info:\n";
		memoryInfo += "\t\tMemory usage:\t" + Util::toString(curMemoryInfo.dwMemoryLoad) + "%\n";
		memoryInfo += "\t\tPhysical memory total:\t" + Util::formatBytes(curMemoryInfo.ullTotalPhys) + "\n";
		memoryInfo += "\t\tPhysical memory free:\t" + Util::formatBytes(curMemoryInfo.ullAvailPhys) + " \n";
		return memoryInfo;
	}
	return Util::emptyString;
}

string CompatibilityManager::generateFullSystemStatusMessage()
{
	return
	    getStartupInfo() +
	    STRING(CURRENT_SYSTEM_STATE) + ":\n" + getGlobalMemoryStatusMessage() +
	    getIncompatibleSoftwareMessage() + "\n" +
	    DatabaseManager::getInstance()->getDBInfo();
}

string CompatibilityManager::getNetworkStats()
{
	char buf[1024];
	snprintf(buf, sizeof(buf),
		"TCP received / sent\t%s / %s\n"
		"UDP received / sent\t%s / %s\n"
		"TLS received / sent\t%s / %s\n",
		Util::formatBytes(Socket::g_stats.tcp.downloaded).c_str(), Util::formatBytes(Socket::g_stats.tcp.uploaded).c_str(),
		Util::formatBytes(Socket::g_stats.udp.downloaded).c_str(), Util::formatBytes(Socket::g_stats.udp.uploaded).c_str(),
		Util::formatBytes(Socket::g_stats.ssl.downloaded).c_str(), Util::formatBytes(Socket::g_stats.ssl.uploaded).c_str());
	return buf;
}

bool CompatibilityManager::updatePhysMemoryStats()
{
	// Total RAM
	MEMORYSTATUSEX curMem = {0};
	curMem.dwLength = sizeof(curMem);
	g_FreePhysMemory = 0;
	g_TotalPhysMemory = 0;
	bool result = getGlobalMemoryStatus(&curMem);
	if (result)
	{
		g_TotalPhysMemory = curMem.ullTotalPhys;
		g_FreePhysMemory = curMem.ullAvailPhys;
	}
	return result;
}

string CompatibilityManager::getStats() // moved from WinUtil
{
	char buf[2048];
	string s;
#ifdef BL_FEATURE_IP_DATABASE
	if (DatabaseManager::isValidInstance())
	{
		auto dm = DatabaseManager::getInstance();
		dm->loadGlobalRatio();
		const auto& r = dm->getGlobalRatio();
		snprintf(buf, sizeof(buf),
			"Total downloaded\t%s\n"
			"Total uploaded\t%s\n"
			"Upload/download ratio\t%.2f\n",
			Util::formatBytes(r.download).c_str(),
			Util::formatBytes(r.upload).c_str(),
			r.download > 0 ? (double) r.upload / (double) r.download : 0);
		s = buf;
	}
#endif
	if (ShareManager::isValidInstance())
	{
		auto sm = ShareManager::getInstance();
		snprintf(buf, sizeof(buf),
			"Shared size\t%s\n"
			"Shared files\t%u\n",
			Util::formatBytes(sm->getTotalSharedSize()).c_str(),
			static_cast<unsigned>(sm->getTotalSharedFiles()));
		s += buf;
	}

	snprintf(buf, sizeof(buf),
		"Total users\t%u (on %u hubs)\n",
		static_cast<unsigned>(ClientManager::getTotalUsers()),
		Client::getTotalCounts());
	s += buf;
	s += "OS version\t";
	s += SysVersion::getFormattedOsName();
	s += '\n';

	HANDLE currentProcess = GetCurrentProcess();
	if (updatePhysMemoryStats())
	{
		snprintf(buf, sizeof(buf),
			"Memory (free / total)\t%s / %s\n",
			Util::formatBytes(g_FreePhysMemory).c_str(),
			Util::formatBytes(g_TotalPhysMemory).c_str());
		s += buf;
	}
	snprintf(buf, sizeof(buf),
		"System uptime\t%s\n"
		"Program uptime\t%s\n",
		Util::formatTime(getSysUptime()).c_str(),
		Util::formatTime(Util::getUpTime()).c_str());
	s += buf;

	static bool psapiLoaded = false;
	typedef BOOL (CALLBACK *LPFUNC)(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);
	static LPFUNC ptrGetProcessMemoryInfo = nullptr;
	if (!psapiLoaded)
	{
		HINSTANCE hInstPsapi = LoadLibrary(_T("psapi"));
		if (hInstPsapi)
			ptrGetProcessMemoryInfo = (LPFUNC) GetProcAddress(hInstPsapi, "GetProcessMemoryInfo");
		psapiLoaded = true;
	}
	if (ptrGetProcessMemoryInfo)
	{
		PROCESS_MEMORY_COUNTERS pmc = {0};
		pmc.cb = sizeof(pmc);
		ptrGetProcessMemoryInfo(currentProcess, &pmc, sizeof(pmc));
		if (ptrGetProcessMemoryInfo(currentProcess, &pmc, sizeof(pmc)))
		{
#if 0
			extern int g_RAM_PeakWorkingSetSize;
			extern int g_RAM_WorkingSetSize;
			g_RAM_WorkingSetSize = pmc.WorkingSetSize >> 20;
			g_RAM_PeakWorkingSetSize = pmc.PeakWorkingSetSize >> 20;
#endif
			snprintf(buf, sizeof(buf),
				"Working set size (peak)\t%s (%s)\n"
				"Virtual memory (peak)\t%s (%s)\n",
				Util::formatBytes((uint64_t) pmc.WorkingSetSize).c_str(),
				Util::formatBytes((uint64_t) pmc.PeakWorkingSetSize).c_str(),
				Util::formatBytes((uint64_t) pmc.PagefileUsage).c_str(),
				Util::formatBytes((uint64_t) pmc.PeakPagefileUsage).c_str());
			s += buf;
		}
	}

	snprintf(buf, sizeof(buf),
		"Win32 GDI objects (peak)\t%d (%d)\n"
		"Win32 User objects (peak)\t%d (%d)\n",
		GetGuiResources(currentProcess, GR_GDIOBJECTS),
		GetGuiResources(currentProcess, GR_GDIOBJECTS_PEAK),
		GetGuiResources(currentProcess, GR_USEROBJECTS),
		GetGuiResources(currentProcess, GR_USEROBJECTS_PEAK));
	s += buf;

	s += getNetworkStats();
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	s += "Sockets\t" + Util::toString(BufferedSocket::getSocketCount()) + '\n';
#endif
	return s;
}

WORD CompatibilityManager::getDllPlatform(const string& fullpath)
{
	#pragma pack(1)
	struct PE_START
	{
		uint32_t signature;
		uint16_t machine;
	};
	#pragma pack()

	WORD result = IMAGE_FILE_MACHINE_UNKNOWN;
	try
	{
		File f(fullpath, File::READ, File::OPEN);
		IMAGE_DOS_HEADER mz;
		size_t len = sizeof(mz);
		f.read(&mz, len);
		if (len == sizeof(mz) && mz.e_magic == IMAGE_DOS_SIGNATURE)
		{
			f.setPos(mz.e_lfanew);
			PE_START pe;
			len = sizeof(pe);
			f.read(&pe, len);
			if (len == sizeof(pe) && pe.signature == IMAGE_NT_SIGNATURE)
				result = pe.machine;
		}
	}
	catch (Exception&) {}
	return result;
}

void CompatibilityManager::reduceProcessPriority()
{
	if (!g_oldPriorityClass || g_oldPriorityClass == GetPriorityClass(GetCurrentProcess()))
	{
		// TODO: refactoring this code to use step-up and step-down of the process priority change.
		g_oldPriorityClass = GetPriorityClass(GetCurrentProcess());
		SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
		// [-] SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1); [-] IRainman: this memory "optimization" for lemmings only.
	}
}

void CompatibilityManager::restoreProcessPriority()
{
	if (g_oldPriorityClass)
		SetPriorityClass(GetCurrentProcess(), g_oldPriorityClass);
}

string CompatibilityManager::getSpeedInfo()
{
	string s = "Download speed\t";
	s += Util::formatBytes(DownloadManager::getRunningAverage()) + "/s  (";
	s += Util::toString(DownloadManager::getInstance()->getDownloadCount()) + " fls.)\n";
	s += "Upload speed\t";
	s += Util::formatBytes(UploadManager::getRunningAverage()) + "/s  (";
	s += Util::toString(UploadManager::getInstance()->getUploadCount()) + " fls.)\n";
	return s;
}

TStringList CompatibilityManager::findVolumes()
{
	TCHAR buf[MAX_PATH];
	buf[0] = 0;
	TStringList volumes;
	HANDLE hVol = FindFirstVolume(buf, MAX_PATH);
	if (hVol != INVALID_HANDLE_VALUE)
	{
		volumes.push_back(buf);
		BOOL found = FindNextVolume(hVol, buf, MAX_PATH);
		while (found)
		{
			volumes.push_back(buf);
			found = FindNextVolume(hVol, buf, MAX_PATH);
		}
		found = FindVolumeClose(hVol);
	}
	return volumes;
}

struct DiskInfo
{
	tstring mountPath;
	int64_t size;
	int64_t free;
	int type;
};

static void getDisks(std::vector<DiskInfo>& results)
{
	DiskInfo di;
	TStringList volumes = CompatibilityManager::findVolumes();
	for (const tstring& vol : volumes)
	{
		UINT type = GetDriveType(vol.c_str());
		if (type == DRIVE_CDROM/* || type == DRIVE_REMOVABLE*/)
			continue;
		TCHAR buf[MAX_PATH];
		buf[0] = 0;
		ULARGE_INTEGER size, free;
		if (GetVolumePathNamesForVolumeName(vol.c_str(), buf, MAX_PATH, nullptr) &&
		    GetDiskFreeSpaceEx(vol.c_str(), nullptr, &size, &free))
		{
			di.mountPath = buf;
			if (!di.mountPath.empty())
			{
				di.free = free.QuadPart;
				di.size = size.QuadPart;
				di.type = type;
				results.push_back(di);
			}
		}
	}

	// and a check for mounted Network drives, todo fix a better way for network space
	DWORD drives = GetLogicalDrives();
	TCHAR drive[3] = { _T('A'), _T(':'), _T('\0') };

	while (drives)
	{
		if ((drives & 1) && drive[0] >= _T('C'))
		{
			ULARGE_INTEGER size, free;
			UINT type = GetDriveType(drive);
			if (type == DRIVE_REMOTE && GetDiskFreeSpaceEx(drive, nullptr, &size, &free))
			{
				di.mountPath = drive;
				di.free = free.QuadPart;
				di.size = size.QuadPart;
				di.type = type;
				results.push_back(di);
			}
		}
		++drive[0];
		drives >>= 1;
	}
}

string CompatibilityManager::getDiskInfo()
{
	std::vector<DiskInfo> results;
	getDisks(results);
	sort(results.begin(), results.end(),
		[](const DiskInfo& a, const DiskInfo& b) -> bool { return a.mountPath < b.mountPath; });

	string s;
	int64_t totalFree = 0, totalSize = 0;
	for (const auto& di : results)
	{
		s += di.type == DRIVE_REMOTE ? "Network drive " : "Local drive ";
		s += Text::fromT(di.mountPath);
		s += '\t';
		s += Util::formatBytes(di.free);
		s += " of ";
		s += Util::formatBytes(di.size);
		s += '\n';
		totalSize += di.size;
		totalFree += di.free;
	}
	s += "All drives\t";
	s += Util::formatBytes(totalFree);
	s += " of ";
	s += Util::formatBytes(totalSize);
	s += '\n';
	return s;
}

string CompatibilityManager::getDiskSpaceInfo(bool onlyTotal /* = false */)
{
	std::vector<DiskInfo> results;
	getDisks(results);

	string s;
	int64_t localFree = 0, localSize = 0, netFree = 0, netSize = 0;
	for (const auto& di : results)
		if (di.type == DRIVE_REMOTE)
		{
			netSize += di.size;
			netFree += di.free;
		}
		else
		{
			localSize += di.size;
			localFree += di.free;
		}
	if (!onlyTotal)
	{
		s += "Local drives\t" + Util::formatBytes(localFree) + " of " + Util::formatBytes(localSize) + '\n';
		if (netSize)
		{
			s += "Network drives\t" + Util::formatBytes(netFree) + " of " + Util::formatBytes(netSize) + '\n';
			s += "Network + HDD space\t" + Util::formatBytes(netFree + localFree) + " of " + Util::formatBytes(netSize + localSize) + '\n';
		}
		s += '\n';
	}
	else
		s += Util::formatBytes(localFree) + " of " + Util::formatBytes(localSize);
	return s;
}

string CompatibilityManager::getCPUInfo()
{
	tstring result;
	HKEY key = nullptr;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Hardware\\Description\\System\\CentralProcessor\\0"), 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		TCHAR buf[256];
		buf[0] = 0;
		DWORD type;
		DWORD size = sizeof(buf);
		if (RegQueryValueEx(key, _T("ProcessorNameString"), nullptr, &type, (BYTE *) buf, &size) == ERROR_SUCCESS && type == REG_SZ)
		{
			size /= sizeof(TCHAR);
			if (size && buf[size-1] == 0) size--;
			result.assign(buf, size);
		}
		DWORD speed;
		size = sizeof(DWORD);
		if (RegQueryValueEx(key, _T("~MHz"), nullptr, &type, (BYTE *) &speed, &size) == ERROR_SUCCESS && type == REG_DWORD)
		{
			result += _T(" (");
			result += Util::toStringT(speed);
			result += _T(" MHz)");
		}
		RegCloseKey(key);
	}
	return result.empty() ? "Unknown" : Text::fromT(result);
}

uint64_t CompatibilityManager::getTickCount()
{
#ifdef OSVER_WIN_XP
	typedef ULONGLONG(CALLBACK *GET_TICK_COUNT64)(void);
	static bool resolved;
	static GET_TICK_COUNT64 getTickCount64;
	if (!resolved)
	{
		HMODULE kernel32lib = GetModuleHandle(_T("kernel32"));
		if (kernel32lib) getTickCount64 = (GET_TICK_COUNT64) GetProcAddress(kernel32lib, "GetTickCount64");
		resolved = true;
	}
	return getTickCount64 ? getTickCount64() : GetTickCount();
#else
	return GetTickCount64();
#endif
}
