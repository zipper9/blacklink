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
#include "AppStats.h"
#include "SysVersion.h"
#include "SysInfo.h"
#include "DatabaseManager.h"
#include "ClientManager.h"
#include "ShareManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "Socket.h"
#include "Client.h"
#include "FormatUtil.h"
#include "Util.h"

#include <winnt.h>
#include <shlwapi.h>
#ifdef OSVER_WIN_VISTA
#define PSAPI_VERSION 1
#endif
#include <psapi.h>
#include "SysVersion.h"
#include "CompatibilityManager.h"

#ifndef GR_GDIOBJECTS_PEAK
#define GR_GDIOBJECTS_PEAK  2
#endif

#ifndef GR_USEROBJECTS_PEAK
#define GR_USEROBJECTS_PEAK 4
#endif

using SysInfo::DiskInfo;

string AppStats::getStats()
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
	MEMORYSTATUSEX ms;
	if (SysInfo::getMemoryInfo(&ms))
	{
		snprintf(buf, sizeof(buf),
			"Memory (free / total)\t%s / %s\n",
			Util::formatBytes(ms.ullAvailPhys).c_str(),
			Util::formatBytes(ms.ullTotalPhys).c_str());
		s += buf;
	}
	snprintf(buf, sizeof(buf),
		"System uptime\t%s\n"
		"Program uptime\t%s\n",
		Util::formatTime(SysInfo::getSysUptime()).c_str(),
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

string AppStats::getSpeedInfo()
{
	string s = "Download speed\t";
	s += Util::formatBytes(DownloadManager::getRunningAverage()) + "/s  (";
	s += Util::toString(DownloadManager::getInstance()->getDownloadCount()) + " fls.)\n";
	s += "Upload speed\t";
	s += Util::formatBytes(UploadManager::getRunningAverage()) + "/s  (";
	s += Util::toString(UploadManager::getInstance()->getUploadCount()) + " fls.)\n";
	return s;
}

string AppStats::getDiskInfo()
{
	std::vector<DiskInfo> results;
	SysInfo::getDisks(results);
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

string AppStats::getDiskSpaceInfo(bool onlyTotal /* = false */)
{
	std::vector<DiskInfo> results;
	SysInfo::getDisks(results);

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

string AppStats::getNetworkStats()
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

string AppStats::getGlobalMemoryStatusMessage()
{
	MEMORYSTATUSEX ms;
	if (!SysInfo::getMemoryInfo(&ms)) return Util::emptyString;
	string s = "\tMemory info:\n";
	s += "\t\tMemory usage:\t" + Util::toString(ms.dwMemoryLoad) + "%\n";
	s += "\t\tPhysical memory total:\t" + Util::formatBytes(ms.ullTotalPhys) + "\n";
	s += "\t\tPhysical memory free:\t" + Util::formatBytes(ms.ullAvailPhys) + " \n";
	return s;
}

string AppStats::getStartupMessage()
{
	string s = getAppNameVer();
	s += " is starting\n"
	     "\tNumber of processors: " + Util::toString(SysVersion::getCPUCount()) + ".\n"
	     "\tProcessor type: ";
	s += SysVersion::getProcArchString();
	s += ".\n";

	s += getGlobalMemoryStatusMessage();
	s += "\n";

	s += "\tRunning in ";
#ifndef _WIN64
	if (SysVersion::isWow64())
		s += "Windows WOW64\n\tPlease consider using the x64 version!";
	else
#endif
		s += "Windows native ";
	s += "\n\t";

	s += SysVersion::getFormattedOsName();
	s += " (" + SysVersion::getFormattedOsVerNum() + ")";
	s += "\n\n";
	return s;
}

string AppStats::getFullSystemStatusMessage()
{
	return getStartupMessage() + DatabaseManager::getInstance()->getDBInfo();
}
