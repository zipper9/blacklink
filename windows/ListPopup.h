#ifndef LIST_POPUP_H_
#define LIST_POPUP_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>

#include <vector>
#include "../client/tstring.h"
#include "ThemeWrapper.h"
#include "UserMessages.h"

class BackingStore;

class ListPopup : public CWindowImpl<ListPopup>, private ThemeWrapper
{
	public:
		DECLARE_WND_CLASS_EX(_T("ListPopup"), CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW | CS_SAVEBITS, COLOR_WINDOW);

		ListPopup();
		~ListPopup();

		void addItem(const tstring& text, HBITMAP icon = nullptr, uintptr_t data = 0, int flags = 0);
		void clearItems();
		SIZE getPrefSize(HDC hdc);
		SIZE getPrefSize();
		void setNotifWindow(HWND hWnd) { hWndNotif = hWnd; }
		const tstring& getItemText(int index) const { return items[index].text; }
		uintptr_t getItemData(int index) const { return items[index].data; }
		void setData(int data) { this->data = data; }
		int getData() const { return data; }
		void changeTheme();

		BEGIN_MSG_MAP(ListPopup)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_SHOWWINDOW, onShowWindow);
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, onLButtonUp)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, onMouseLeave)
		MESSAGE_HANDLER(WM_VSCROLL, onScroll)
		MESSAGE_HANDLER(WM_KILLFOCUS, onKillFocus)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, onMouseWheel)
		MESSAGE_HANDLER(WM_KEYDOWN, onKeyDown)
		END_MSG_MAP()

	private:
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return 1; }
		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSize(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onShowWindow(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onKillFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onMouseWheel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onKeyDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);

	private:
		enum
		{
			IF_CHECKED = 1,
			IF_DEFAULT = 2
		};

		enum
		{
			FLAG_HAS_SCROLLBAR    = 0x001,
			FLAG_SHOW_CHECKS      = 0x002,
			FLAG_HAS_BORDER       = 0x004,
			FLAG_PRESSED          = 0x008,
			FLAG_UPDATE_ICON_SIZE = 0x010,
			FLAG_MOUSE_CAPTURE    = 0x080,
			FLAG_MOUSE_TRACKING   = 0x100,
			FLAG_APP_THEMED       = 0x200
		};

		enum
		{
			HT_EMPTY,
			HT_SCROLL,
			HT_ITEM,
			HT_ITEM_CHECK
		};

		struct Item
		{
			tstring text;
			HBITMAP icon;
			uintptr_t data;
			int iconWidth;
			int iconHeight;
			int flags;
		};

		std::vector<Item> items;
		int maxVisibleItems;
		int maxTextWidthUnscaled;
		int maxTextWidth;

		int flags;
		int itemHeight;
		int iconSize;
		int scrollBarSize;
		int cxBorder, cyBorder;

		MARGINS marginCheck;
		MARGINS marginCheckBackground;
		MARGINS marginItem;
		MARGINS marginText;
		MARGINS marginBitmap;
		SIZE sizeCheck;
		int partId;

		int hotIndex;
		int topIndex;
		int pressedIndex;
		int maxIconWidth;
		int maxIconHeight;
		int data;

		HFONT hFont;
		HFONT hFontBold;
		HWND hWndScroll;
		HWND hWndNotif;
		HBITMAP checkBmp[2];

		BackingStore* backingStore;

		void initTheme();
		void initFallbackMetrics();
		void updateIconSize();
		void updateItemHeight(HDC hdc);
		void updateScrollBar();
		void updateScrollBarPos(int width, int height);
		void clearFonts();
		void updateFonts();
		void clearBitmaps();
		int hitTest(POINT pt, int& index) const;
		void draw(HDC hdc, const RECT& rc);
		void drawBorder(HDC hdc, const RECT& rc);
		void drawBackground(HDC hdc, const RECT& rc);
		void drawItem(HDC hdc, const RECT& rc, int index);
		void trackMouseEvent(bool cancel);
		void sendNotification(int index);
		void ensureVisible(int index, bool moveUp);
		void updateScrollValue();
};

#endif // LIST_POPUP_H_
