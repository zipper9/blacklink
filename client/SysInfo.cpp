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
#include "SysInfo.h"
#include "StrUtil.h"
#include "Text.h"

#ifdef _WIN32
void SysInfo::getDisks(std::vector<DiskInfo>& results)
{
	DiskInfo di;
	TStringList volumes = getVolumes();
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

TStringList SysInfo::getVolumes()
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

string SysInfo::getCPUInfo()
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

bool SysInfo::getMemoryInfo(MEMORYSTATUSEX* status)
{
	typedef BOOL (WINAPI* GLOBAL_MEMORY_STATUS_EX)(MEMORYSTATUSEX*);
	static GLOBAL_MEMORY_STATUS_EX globalMemoryStatusEx;
	static bool resolved;
	memset(status, 0, sizeof(MEMORYSTATUSEX));
	status->dwLength = sizeof(MEMORYSTATUSEX);
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

uint64_t SysInfo::getTickCount()
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

#endif // _WIN32
