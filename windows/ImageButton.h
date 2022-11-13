#ifndef IMAGE_BUTTON_H_
#define IMAGE_BUTTON_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>

class ImageButton : public CWindowImpl<ImageButton, CButton>
{
		HICON icon;
		bool initialized;
		int imageWidth;
		int imageHeight;
		HTHEME hTheme;

	public:
		BEGIN_MSG_MAP(ImageButton)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, onErase)
		MESSAGE_HANDLER(WM_ENABLE, onEnable)
		MESSAGE_HANDLER(WM_THEMECHANGED, onThemeChanged)
		MESSAGE_HANDLER(BM_SETIMAGE, onSetImage)
		END_MSG_MAP()
		
		ImageButton();
		~ImageButton();

		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onErase(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onEnable(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSetImage(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	private:
		void drawBackground(HDC hdc, int width, int height);
		void drawImage(HDC hdc, int width, int height);
		void draw(HDC hdc, int width, int height);
		void cleanup();
};

#endif // IMAGE_BUTTON_H_
