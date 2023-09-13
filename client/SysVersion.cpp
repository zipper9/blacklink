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
#include "SysVersion.h"
#include "StrUtil.h"
#include "Text.h"

#ifdef _WIN32
#include "winprod.h"

unsigned SysVersion::flags = 0;
OSVERSIONINFOEX SysVersion::osvi = {};
SYSTEM_INFO SysVersion::sysInfo = {};

struct
{
	DWORD type;
	const char* name;
} static const productNames[] =
{
	{ PRODUCT_ULTIMATE,                     "Ultimate Edition"                            },
	{ PRODUCT_PROFESSIONAL,                 "Professional"                                },
	{ PRODUCT_PROFESSIONAL_N,               "Professional N"                              },
	{ PRODUCT_HOME_PREMIUM,                 "Home Premium Edition"                        },
	{ PRODUCT_HOME_BASIC,                   "Home Basic Edition"                          },
	{ PRODUCT_ENTERPRISE,                   "Enterprise Edition"                          },
	{ PRODUCT_BUSINESS,                     "Business Edition"                            },
	{ PRODUCT_STARTER,                      "Starter Edition"                             },
	{ PRODUCT_CLUSTER_SERVER,               "Cluster Server Edition"                      },
	{ PRODUCT_DATACENTER_SERVER,            "Datacenter Edition"                          },
	{ PRODUCT_DATACENTER_SERVER_CORE,       "Datacenter Edition (core installation)"      },
	{ PRODUCT_ENTERPRISE_SERVER,            "Enterprise Edition"                          },
	{ PRODUCT_ENTERPRISE_SERVER_CORE,       "Enterprise Edition (core installation)"      },
	{ PRODUCT_ENTERPRISE_SERVER_IA64,       "Server Enterprise for Itanium-based Systems" },
	{ PRODUCT_SMALLBUSINESS_SERVER,         "Small Business Server"                       },
	{ PRODUCT_SMALLBUSINESS_SERVER_PREMIUM, "Small Business Server Premium Edition"       },
	{ PRODUCT_STANDARD_SERVER,              "Standard Edition"                            },
	{ PRODUCT_STANDARD_SERVER_CORE,         "Standard Edition (core installation)"        },
	{ PRODUCT_WEB_SERVER,                   "Web Server Edition"                          },
	{ PRODUCT_IOTUAP,                       "IoT Core"                                    },
	{ PRODUCT_EDUCATION,                    "Education"                                   },
	{ PRODUCT_ENTERPRISE_S,                 "Enterprise 2015 LTSB"                        },
	{ PRODUCT_ENTERPRISE_S_N,               "Enterprise 2015 LTSB N"                      },
	{ PRODUCT_CORE,                         "Home"                                        },
	{ PRODUCT_CORE_SINGLELANGUAGE,          "Home Single Language"                        },
	{ PRODUCT_UNLICENSED,                   "Unlicensed"                                  }
};
#endif

#ifdef _WIN32
bool SysVersion::isWow64Process()
{
	typedef BOOL (WINAPI* IS_WOW64_PROCESS)(HANDLE, PBOOL);
	static IS_WOW64_PROCESS isWow64Process;
	static bool resolved;
	if (!resolved)
	{
		HMODULE kernel32lib = GetModuleHandle(_T("kernel32"));
		if (kernel32lib) isWow64Process = (IS_WOW64_PROCESS) GetProcAddress(kernel32lib, "IsWow64Process");
		resolved = true;
	}
	BOOL isWow64;
	if (isWow64Process && isWow64Process(GetCurrentProcess(), &isWow64))
		return isWow64 != FALSE;
	return false;
}

bool SysVersion::detectWine()
{
	HMODULE module = GetModuleHandle(_T("ntdll.dll"));
	if (!module)
		return false;

	bool result = GetProcAddress(module, "wine_server_call") != nullptr;
	FreeLibrary(module);
	return result;
}

const char* SysVersion::getProcArchString()
{
	switch (sysInfo.wProcessorArchitecture)
	{
		case PROCESSOR_ARCHITECTURE_AMD64:
			return "x86-x64";
		case PROCESSOR_ARCHITECTURE_INTEL:
			return "x86";
		case PROCESSOR_ARCHITECTURE_IA64:
			return "Itanium";
#ifdef PROCESSOR_ARCHITECTURE_ARM
		case PROCESSOR_ARCHITECTURE_ARM:
			return "ARM";
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM64
		case PROCESSOR_ARCHITECTURE_ARM64:
			return "ARM64";
#endif
	};
	return "Unknown";
}

void SysVersion::setWine(bool flag)
{
	if (flag)
		flags |= IS_WINE;
	else
		flags &= ~IS_WINE;
}
#endif

string SysVersion::getFormattedOsVerNum()
{
#ifdef _WIN32
	string s = Util::toString(osvi.dwMajorVersion) + '.' + Util::toString(osvi.dwMinorVersion);
	if (osvi.wProductType == VER_NT_SERVER)
		s += " Server";
	if (osvi.wServicePackMajor)
		s += " SP" + Util::toString(osvi.wServicePackMajor);
	if (isWine())
		s += " Wine";
	if (isWow64())
		s += " WOW64";
	return s;
#else
	return "";
#endif
}

string SysVersion::getFormattedOsName()
{
#ifdef _WIN32
	string text = "Microsoft Windows ";

	// Test for the specific product.
	// https://msdn.microsoft.com/ru-ru/library/windows/desktop/ms724833(v=vs.85).aspx
	unsigned major = osvi.dwMajorVersion;
	unsigned minor = osvi.dwMinorVersion;
	if (major == 6 || major == 10)
	{
		if (major == 10)
		{
			if (minor == 0)
			{
				if (osvi.dwBuildNumber >= 22000)
					text += osvi.wProductType == VER_NT_WORKSTATION ? "11" : "Server 2022";
				else
					text += osvi.wProductType == VER_NT_WORKSTATION ? "10" : "Server 2016";
			}
			// check type for Win10: Desktop, Mobile, etc...
		}
		if (major == 6)
		{
			if (minor == 0)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
					text += "Vista";
				else
					text += "Server 2008";
			}
			if (minor == 1)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
					text += "7";
				else
					text += "Server 2008 R2";
			}
			if (minor == 2)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
					text += "8";
				else
					text += "Server 2012";
			}
			if (minor == 3)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
					text += "8.1";
				else
					text += "Server 2012 R2";
			}
		}
		// Product Info  https://msdn.microsoft.com/en-us/library/windows/desktop/ms724358(v=vs.85).aspx
		typedef BOOL (WINAPI* GET_PRODUCT_INFO)(DWORD, DWORD, DWORD, DWORD, PDWORD);
		GET_PRODUCT_INFO getProductInfo = (GET_PRODUCT_INFO) GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetProductInfo");
		if (getProductInfo)
		{
			DWORD dwType;
			getProductInfo(major, minor, 0, 0, &dwType);
			const char* name = nullptr;
			for (size_t i = 0; i < _countof(productNames); ++i)
				if (productNames[i].type == dwType)
				{
					name = productNames[i].name;
					break;
				}
			if (name)
			{
				text += ' ';
				text += name;
			}
		}
	}
	if (major == 5)
	{
		if (minor == 2)
		{
			if (GetSystemMetrics(SM_SERVERR2))
				text += "Server 2003 R2, ";
			else if (osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER)
				text += "Storage Server 2003";
			else if (osvi.wSuiteMask & VER_SUITE_WH_SERVER)
				text += "Home Server";
			else if (osvi.wProductType == VER_NT_WORKSTATION && sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
				text += "XP Professional x64 Edition";
			else
				text += "Server 2003, ";  // Test for the server type.
			if (osvi.wProductType != VER_NT_WORKSTATION)
			{
				if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64)
				{
					if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
						text += "Datacenter Edition for Itanium-based Systems";
					else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
						text += "Enterprise Edition for Itanium-based Systems";
				}
				else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
				{
					if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
						text += "Datacenter x64 Edition";
					else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
						text += "Enterprise x64 Edition";
					else
						text += "Standard x64 Edition";
				}
				else
				{
					if (osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER)
						text += "Compute Cluster Edition";
					else if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
						text += "Datacenter Edition";
					else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
						text += "Enterprise Edition";
					else if (osvi.wSuiteMask & VER_SUITE_BLADE)
						text += "Web Edition";
					else
						text += "Standard Edition";
				}
			}
		}
		if (minor == 1)
		{
			text += "XP ";
			if (osvi.wSuiteMask & VER_SUITE_PERSONAL)
				text += "Home Edition";
			else
				text += "Professional";
		}
		if (minor == 0)
		{
			text += "2000 ";
			if (osvi.wProductType == VER_NT_WORKSTATION)
			{
				text += "Professional";
			}
			else
			{
				if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
					text += "Datacenter Server";
				else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
					text += "Advanced Server";
				else
					text += "Server";
			}
		}
	}
	// Include service pack (if any) and build number.
	if (osvi.szCSDVersion[0])
	{
		text += ' ' + Text::fromT(osvi.szCSDVersion);
	}
	text += " (build " + Util::toString(osvi.dwBuildNumber) + ")";
	if (osvi.dwMajorVersion >= 6)
	{
		if ((sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) || isWow64())
			text += ", 64-bit";
		else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
			text += ", 32-bit";
	}
	return text;
#elif defined(__linux__) || defined(linux)
	return "Linux";
#elif defined(__FreeBSD__)
	return "FreeBSD";
#else
	return "POSIX";
#endif
}

void SysVersion::initialize()
{
#ifdef _WIN32
	flags = 0;

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	if (!GetVersionEx((OSVERSIONINFO*) &osvi))
		memset(&osvi, 0, sizeof(osvi));

	unsigned version;
	if (osvi.dwMajorVersion >= 10)
	{
		version = osvi.dwBuildNumber >= 22000 ? OS_WINDOWS11_PLUS : OS_WINDOWS10_PLUS;
	}
	else
	{
		uint32_t v = osvi.dwMajorVersion << 16 | osvi.dwMinorVersion;
		if (v >= 0x60002)
			version = OS_WINDOWS8_PLUS;
		else if (v >= 0x60001)
			version = OS_WINDOWS7_PLUS;
		else if (v >= 0x60000)
			version = OS_VISTA_PLUS;
		else if (v >= 0x50001)
			version = OS_XP_PLUS;
		else
			version = 0;
	}
	while (version >= OS_XP_PLUS)
	{
		flags |= version;
		version >>= 1;
	}

	setWine(detectWine());

	if (!isWine() && isWow64Process())
		flags |= IS_WOW64;

	GetSystemInfo(&sysInfo);
#endif
}
