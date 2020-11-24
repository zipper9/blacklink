#ifndef SPLASH_WINDOW_H_
#define SPLASH_WINDOW_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "../client/typedefs.h"
#include "../client/Thread.h"

class SplashWindow : public CWindowImpl<SplashWindow>
{
	public:
		SplashWindow();
		~SplashWindow();

		SplashWindow(const SplashWindow&) = delete;
		SplashWindow& operator= (const SplashWindow&) = delete;

		static const int WIDTH = 400;
		static const int HEIGHT = 254;

		void setProgressText(const tstring& text);

	private:
		BEGIN_MSG_MAP(SplashWindow)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return 0; }

	private:
		HDC memDC;
		HFONT font;
		HBITMAP dibSect[2];
		void* dibBuf[2];
		uint8_t* maskBuf[3];
		int frameIndex;
		int filterIndex;
		int maskIndex;
		bool useEffect;
		tstring versionText;
		tstring progressText;
		CriticalSection csProgressText;

		void drawNextFrame();
		void drawText(HDC hdc);
		void cleanup();
};

#endif // SPLASH_WINDOW_H_
