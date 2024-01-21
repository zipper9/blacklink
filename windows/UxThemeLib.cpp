#include "stdafx.h"
#include "UxThemeLib.h"
#include <tchar.h>

UxThemeLib UxThemeLib::instance;

void UxThemeLib::clearPointers()
{
	pIsAppThemed = nullptr;
	pOpenThemeData = nullptr;
	pCloseThemeData = nullptr;
	pGetThemePartSize = nullptr;
	pGetThemeTextExtent = nullptr;
	pGetThemeBackgroundContentRect = nullptr;
	pIsThemeBackgroundPartiallyTransparent = nullptr;
	pDrawThemeBackground = nullptr;
	pDrawThemeText = nullptr;
	pDrawThemeTextEx = nullptr;
	pDrawThemeParentBackground = nullptr;
	pGetThemeColor = nullptr;
	pGetThemeMetric = nullptr;
	pGetThemeBool = nullptr;
	pGetThemeInt = nullptr;
	pGetThemeMargins = nullptr;
	pGetThemeBitmap = nullptr;
	pGetThemeSysFont = nullptr;
	pBeginBufferedAnimation = nullptr;
	pEndBufferedAnimation = nullptr;
	pBufferedPaintRenderAnimation = nullptr;
	pBufferedPaintStopAllAnimations = nullptr;
	pGetThemeTransitionDuration = nullptr;
	pEnableThemeDialogTexture = nullptr;
	pIsThemeDialogTextureEnabled = nullptr;
}

#define RESOLVE(f)     p ## f = (fn ## f) GetProcAddress(hModule, #f); if (!p ## f) result = false;
#define RESOLVE_OPT(f) p ## f = (fn ## f) GetProcAddress(hModule, #f);

void UxThemeLib::init()
{
	if (hModule || initialized) return;
	initialized = true;
	hModule = LoadLibrary(_T("UxTheme.dll"));
	if (!hModule) return;
	bool result = true;
	RESOLVE(IsAppThemed);
	RESOLVE(OpenThemeData);
	RESOLVE(CloseThemeData);
	RESOLVE(GetThemePartSize);
	RESOLVE(GetThemeTextExtent);
	RESOLVE(GetThemeBackgroundContentRect);
	RESOLVE(IsThemeBackgroundPartiallyTransparent);
	RESOLVE(DrawThemeBackground);
	RESOLVE(DrawThemeText);
	RESOLVE(DrawThemeParentBackground);
	RESOLVE(GetThemeColor);
	RESOLVE(GetThemeMetric);
	RESOLVE(GetThemeBool);
	RESOLVE(GetThemeInt);
	RESOLVE(GetThemeMargins);
	RESOLVE(GetThemeBitmap);
	RESOLVE(GetThemeSysFont);
	if (!result) return;
	RESOLVE_OPT(DrawThemeTextEx);
	RESOLVE_OPT(BeginBufferedAnimation);
	RESOLVE_OPT(EndBufferedAnimation);
	RESOLVE_OPT(BufferedPaintRenderAnimation);
	RESOLVE_OPT(BufferedPaintStopAllAnimations);
	RESOLVE_OPT(GetThemeTransitionDuration);
	RESOLVE_OPT(EnableThemeDialogTexture);
	RESOLVE_OPT(IsThemeDialogTextureEnabled);
}

void UxThemeLib::uninit()
{
	if (hModule)
	{
		FreeLibrary(hModule);
		hModule = nullptr;
		clearPointers();
	}
	initialized = false;
}
