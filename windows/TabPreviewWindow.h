#ifndef TAB_PREVIEW_WINDOW_H_
#define TAB_PREVIEW_WINDOW_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "../client/tstring.h"

class TabPreviewWindow : public CWindowImpl<TabPreviewWindow, CWindow, CWinTraits<WS_POPUP | WS_VISIBLE, WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_NOPARENTNOTIFY | WS_EX_TOPMOST | WS_EX_TRANSPARENT>>
{
	public:
		enum
		{
			TABS_TOP,
			TABS_BOTTOM
		};

		TabPreviewWindow();
		~TabPreviewWindow() { clearPreview(); }
		void init(HICON icon, const tstring& text, int pos);
		void setPreview(HWND hWnd);
		void setTextColor(COLORREF color);
		void setBackgroundColor(COLORREF color);
		void setBorderColor(COLORREF color);
		void updateSize(HDC hdc);
		SIZE getSize() const;
		POINT getOffset() const;
		void setGrabPoint(POINT pt);
		void setMaxPreviewSize(int width, int height);

	private:
		BEGIN_MSG_MAP(TabPreviewWindow)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled);

		void clearPreview();
		void draw(HDC hdc);

		int maxPreviewWidth;
		int maxPreviewHeight;
		int pos;
		COLORREF textColor;
		COLORREF backgroundColor;
		COLORREF borderColor;
		int tabHeight;
		int tabWidth;
		int startMargin;
		int innerMargin;
		int horizIconSpace;
		int horizPadding;
		int previewWidth;
		int previewHeight;
		HBITMAP preview;
		HICON icon;
		tstring text;
		POINT ptGrab;
};

#endif // TAB_PREVIEW_WINDOW_H_
