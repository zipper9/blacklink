#include "stdafx.h"
#include "ThemeWrapper.h"
#include "UxThemeLib.h"

ThemeWrapper::ThemeWrapper()
{
	hTheme = nullptr;
	initialized = false;
}

ThemeWrapper::~ThemeWrapper()
{
	closeTheme();
}

bool ThemeWrapper::openTheme(HWND hWnd, LPCWSTR name)
{
	if (hTheme || initialized) return true;
	UxThemeLib::instance.init();
	if (!UxThemeLib::instance.pOpenThemeData) return false;
	hTheme = UxThemeLib::instance.pOpenThemeData(hWnd, name);
	initialized = true;
	return hTheme != nullptr;
}

void ThemeWrapper::closeTheme()
{
	if (hTheme)
	{
		UxThemeLib::instance.pCloseThemeData(hTheme);
		hTheme = nullptr;
	}
	initialized = false;
}
