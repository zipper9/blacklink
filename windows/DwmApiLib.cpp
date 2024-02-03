#include "stdafx.h"
#include "DwmApiLib.h"
#include <tchar.h>

DwmApiLib DwmApiLib::instance;

void DwmApiLib::clearPointers()
{
	pDwmSetWindowAttribute = nullptr;
}

#define RESOLVE(f) p ## f = (fn ## f) GetProcAddress(hModule, #f); if (!p ## f) result = false;

void DwmApiLib::init()
{
	if (hModule || initialized) return;
	initialized = true;
	hModule = LoadLibrary(_T("dwmapi.dll"));
	if (!hModule) return;

	bool result = true;
	RESOLVE(DwmSetWindowAttribute);
}

void DwmApiLib::uninit()
{
	if (hModule)
	{
		FreeLibrary(hModule);
		hModule = nullptr;
		clearPointers();
	}
	initialized = false;
}
