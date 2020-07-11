#ifndef RICH_TEXT_LABEL_H_
#define RICH_TEXT_LABEL_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "../client/Text.h"
#include "UserMessages.h"

class RichTextLabel: public CWindowImpl<RichTextLabel>
{
	public:
		~RichTextLabel();

		BEGIN_MSG_MAP(RichTextLabel)
		MESSAGE_HANDLER(WM_SETCURSOR, onSetCursor)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SETTEXT, onSetText)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, onMouseLeave)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, onLButtonUp)
		END_MSG_MAP()

		BOOL SubclassWindow(HWND hWnd);

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled);
		LRESULT onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 1;
		}

		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSize(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
		LRESULT onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
		LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
		LRESULT onSetCursor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

		void setTextColor(COLORREF color);
		void setBackgroundColor(COLORREF color);
		void setLinkColor(COLORREF color, COLORREF colorHover);
		void setMargins(int left, int top, int right, int bottom);
		void setCenter(bool center);
		void setTextHeight(int height);

	private:
		struct Fragment
		{
			tstring text;
			int style;
			int x, y;
			int width, height;
			int spaceWidth;
			int link;
		};

		std::vector<Fragment> fragments;

		struct Style
		{
			LOGFONT font;
		};

		struct StyleInstance : public Style
		{
			HFONT hFont;
			TEXTMETRIC metric;
		};

		std::vector<StyleInstance> styles;

		struct TagStackItem
		{
			string tag;
			Style style;
			int link;
		};

		std::vector<TagStackItem> tagStack;

		std::vector<tstring> links;

		tstring text;
		Style initialStyle;
		bool calcSizeFlag = false;
		bool layoutFlag = false;
		int parsingLink = -1;
		int lastFragment = -1;
		int hoverLink = -1;
		int clickLink = -1;
		int fontHeight = 0;
		int margins[4] = {};
		bool centerFlag = false;
		COLORREF colorBackground = RGB(255, 255, 255);
		COLORREF colorText = RGB(0, 0, 0);
		COLORREF colorLink = RGB(0, 0x66, 0xCC);
		COLORREF colorLinkHover = RGB(0, 0x86, 0xFF);
		HDC memDC = nullptr;
		HBITMAP memBitmap = nullptr;
		HBITMAP oldBitmap = nullptr;
		HBRUSH bgBrush = nullptr;
		int bitmapWidth = 0;
		int bitmapHeight = 0;
		HCURSOR linkCursor = nullptr;
		HCURSOR defaultCursor = nullptr;

		void initialize();
		int getStyle();
		void addFragment(tstring::size_type start, tstring::size_type end, bool whiteSpace);
		void updatePosition(const StyleInstance& style, int& maxAscent, int& maxDescent) const;
		void processTag(const tstring& tag);
		TagStackItem newTagStackItem(const string& tag);
		static void unescape(tstring& text);
		void calcSize(HDC hdc);
		void layout(HDC hdc, int width, int height);
		void centerLine(size_t start, size_t end, int width);
		void drawUnderline(HDC dc, int xStart, int xEnd, int y, int yBottom, bool isLink) const;
		void clearStyles();
		void cleanup();

		static void getTextSize(HDC hdc, const tstring& text, int len, SIZE& size, const StyleInstance& style);

		friend bool styleEq(const Style& s1, const Style& s2);
};

#endif // RICH_TEXT_LABEL_H_
