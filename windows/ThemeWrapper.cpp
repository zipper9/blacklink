#include "stdafx.h"
#include "ThemeWrapper.h"
#include <tchar.h>

ThemeWrapper::ThemeWrapper()
{
	hModule = nullptr;
	hTheme = nullptr;
	moduleInitialized = false;
	themeInitialized = false;
	clearPointers();
}

ThemeWrapper::~ThemeWrapper()
{
	closeTheme();
	freeLibrary();
}

void ThemeWrapper::clearPointers()
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

void ThemeWrapper::loadLibrary()
{
	if (hModule || moduleInitialized) return;
	moduleInitialized = true;
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

void ThemeWrapper::freeLibrary()
{
	if (hModule)
	{
		FreeLibrary(hModule);
		hModule = nullptr;
	}
	moduleInitialized = false;
}

bool ThemeWrapper::openTheme(HWND hWnd, LPCWSTR name)
{
	if (hTheme || themeInitialized) return true;
	loadLibrary();
	if (!hModule)
	{
		clearPointers();
		freeLibrary();
		return false;
	}
	hTheme = pOpenThemeData(hWnd, name);
	themeInitialized = true;
	return hTheme != nullptr;
}

void ThemeWrapper::closeTheme()
{
	if (hTheme)
	{
		pCloseThemeData(hTheme);
		hTheme = nullptr;
	}
	themeInitialized = false;
}
