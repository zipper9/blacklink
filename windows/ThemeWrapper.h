#ifndef THEME_WRAPPER_H_
#define THEME_WRAPPER_H_

#include "../client/w.h"
#include <UxTheme.h>

class ThemeWrapper
{
	public:
		HMODULE hModule;
		HTHEME hTheme;

		typedef BOOL(STDAPICALLTYPE *fnIsAppThemed)();
		typedef HTHEME (STDAPICALLTYPE *fnOpenThemeData)(HWND, LPCWSTR);
		typedef HRESULT (STDAPICALLTYPE *fnCloseThemeData)(HTHEME);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemePartSize)(HTHEME, HDC, int, int, const RECT*, THEMESIZE, SIZE*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeTextExtent)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, const RECT*, RECT*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeBackgroundContentRect)(HTHEME, HDC, int, int, const RECT*, RECT*);
		typedef BOOL (STDAPICALLTYPE *fnIsThemeBackgroundPartiallyTransparent)(HTHEME, int, int);
		typedef HRESULT (STDAPICALLTYPE *fnDrawThemeBackground)(HTHEME, HDC, int, int, const RECT*, const RECT*);
		typedef HRESULT (STDAPICALLTYPE *fnDrawThemeText)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, DWORD, const RECT*);
		typedef HRESULT (STDAPICALLTYPE *fnDrawThemeTextEx)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, RECT*, const DTTOPTS*);
		typedef HRESULT (STDAPICALLTYPE *fnDrawThemeParentBackground)(HWND, HDC, const RECT*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeColor)(HTHEME, int, int, int, COLORREF*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeMetric)(HTHEME, HDC, int, int, int, int*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeBool)(HTHEME, int, int, int, BOOL*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeInt)(HTHEME, int, int, int, int*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeMargins)(HTHEME, HDC, int, int, int, const RECT*, MARGINS*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeBitmap)(HTHEME, int, int, int, ULONG, HBITMAP*);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeSysFont)(HTHEME, int, LOGFONTW*);
		typedef HANIMATIONBUFFER (STDAPICALLTYPE *fnBeginBufferedAnimation)(HWND, HDC, const RECT*, BP_BUFFERFORMAT, BP_PAINTPARAMS*, BP_ANIMATIONPARAMS*, HDC*, HDC*);
		typedef HRESULT (STDAPICALLTYPE *fnEndBufferedAnimation)(HANIMATIONBUFFER, BOOL);
		typedef BOOL (STDAPICALLTYPE *fnBufferedPaintRenderAnimation)(HWND, HDC);
		typedef HRESULT (STDAPICALLTYPE *fnBufferedPaintStopAllAnimations)(HWND);
		typedef HRESULT (STDAPICALLTYPE *fnGetThemeTransitionDuration)(HTHEME, int, int, int, int, DWORD*);
		typedef HRESULT (STDAPICALLTYPE *fnEnableThemeDialogTexture)(HWND, DWORD);
		typedef BOOL (STDAPICALLTYPE *fnIsThemeDialogTextureEnabled)(HWND);

		fnIsAppThemed pIsAppThemed;
		fnOpenThemeData pOpenThemeData;
		fnCloseThemeData pCloseThemeData;
		fnGetThemePartSize pGetThemePartSize;
		fnGetThemeTextExtent pGetThemeTextExtent;
		fnGetThemeBackgroundContentRect pGetThemeBackgroundContentRect;
		fnIsThemeBackgroundPartiallyTransparent pIsThemeBackgroundPartiallyTransparent;
		fnDrawThemeBackground pDrawThemeBackground;
		fnDrawThemeText pDrawThemeText;
		fnDrawThemeTextEx pDrawThemeTextEx;
		fnDrawThemeParentBackground pDrawThemeParentBackground;
		fnGetThemeColor pGetThemeColor;
		fnGetThemeMetric pGetThemeMetric;
		fnGetThemeBool pGetThemeBool;
		fnGetThemeInt pGetThemeInt;
		fnGetThemeMargins pGetThemeMargins;
		fnGetThemeBitmap pGetThemeBitmap;
		fnGetThemeSysFont pGetThemeSysFont;
		fnBeginBufferedAnimation pBeginBufferedAnimation;
		fnEndBufferedAnimation pEndBufferedAnimation;
		fnBufferedPaintRenderAnimation pBufferedPaintRenderAnimation;
		fnBufferedPaintStopAllAnimations pBufferedPaintStopAllAnimations;
		fnGetThemeTransitionDuration pGetThemeTransitionDuration;
		fnEnableThemeDialogTexture pEnableThemeDialogTexture;
		fnIsThemeDialogTextureEnabled pIsThemeDialogTextureEnabled;

		ThemeWrapper();
		~ThemeWrapper();
		ThemeWrapper(const ThemeWrapper&) = delete;
		ThemeWrapper& operator= (const ThemeWrapper&) = delete;

		void loadLibrary();
		bool openTheme(HWND hWnd, LPCWSTR name);
		void closeTheme();

	private:
		void freeLibrary();
		void clearPointers();

		bool moduleInitialized;
		bool themeInitialized;
};

#endif // THEME_WRAPPER_H_
