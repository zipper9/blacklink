#include "stdafx.h"

#ifdef _USRDLL
#include "resource.h"
#include "GdiOle_i.h"
#include "dllmain.h"

CGdiOleModule _AtlModule;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	hInstance;
	return _AtlModule.DllMain(dwReason, lpReserved);
}
#endif
