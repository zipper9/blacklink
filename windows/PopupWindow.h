#ifndef POPUP_WINDOW_H_
#define POPUP_WINDOW_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "../client/typedefs.h"

class PopupWindow : public CWindowImpl<PopupWindow, CWindow, CWinTraits<WS_POPUP, WS_EX_NOACTIVATE | WS_EX_NOPARENTNOTIFY | WS_EX_TOPMOST>>
{
	public:
		DECLARE_WND_CLASS_EX(_T("PopupWindow"), CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW);

		PopupWindow();
		~PopupWindow() { cleanup(); }

		void setRemoveTime(uint64_t tick) { removeTime = tick; }
		bool shouldRemove(uint64_t tick) const { return removeTime && tick > removeTime; }
		void setText(const tstring& s) { text = s; }
		void setTitle(const tstring& s) { title = s; }
		void setFont(const LOGFONT& f);
		void setTitleFont(const LOGFONT& f);
		void setBorderSize(int value) { borderSize = value; }
		void setPadding(int value) { padding = value; }
		void setTitleSpace(int value) { titleSpace = value; }
		void setBackgroundColor(COLORREF color) { backgroundColor = color; }
		void setTextColor(COLORREF color) { textColor = color; }
		void setTitleColor(COLORREF color) { titleColor = color; }
		void setBorderColor(COLORREF color) { borderColor = color; }
		void setNotifWnd(HWND hWnd) { hNotifWnd = hWnd; }
		void hide();

	private:
		BEGIN_MSG_MAP(PopupWindow)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SHOWWINDOW, onShowWindow);
		MESSAGE_HANDLER(WM_MOUSEACTIVATE, onMouseActivate);
		END_MSG_MAP()

		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return 1; }
		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT, WPARAM wParam, LPARAM, BOOL& bHandled);
		LRESULT onShowWindow(UINT, WPARAM wParam, LPARAM, BOOL&);
		LRESULT onMouseActivate(UINT, WPARAM wParam, LPARAM, BOOL&) { return MA_NOACTIVATE; }

		void cleanup();
		void startAnimation(int type);
		double getAnimValue() const;
		void onAnimationFinished(int type);
		void updateWindowStyle();

		int flags;
		tstring text;
		tstring title;
		uint64_t removeTime;
		HWND hNotifWnd;

		LOGFONT logFont;
		LOGFONT logFontTitle;

		HFONT hFont;
		HFONT hFontTitle;

		COLORREF backgroundColor;
		COLORREF textColor;
		COLORREF titleColor;
		COLORREF borderColor;

		int borderSize;
		int padding;
		int titleSpace;

		int animDuration;
		int currentAnimation;
		int currentAnimDuration;
		int64_t animStart;
};

#endif // POPUP_WINDOW_H_
