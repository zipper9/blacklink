#ifndef THEME_WRAPPER_H_
#define THEME_WRAPPER_H_

#include "../client/w.h"
#include <UxTheme.h>

class ThemeWrapper
{
	public:
		HTHEME hTheme;

		ThemeWrapper();
		~ThemeWrapper();

		ThemeWrapper(const ThemeWrapper&) = delete;
		ThemeWrapper& operator= (const ThemeWrapper&) = delete;

		bool openTheme(HWND hWnd, LPCWSTR name);
		void closeTheme();

	private:
		bool initialized;
};

#endif // THEME_WRAPPER_H_
