#ifndef STATUS_BAR_CTRL_H_
#define STATUS_BAR_CTRL_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>

#include <vector>
#include "../client/tstring.h"
#include "ThemeWrapper.h"

class BackingStore;

class StatusBarCtrl : public CWindowImpl<StatusBarCtrl>, private ThemeWrapper
{
	public:
		enum
		{
			FLAG_OWN_FONT           = 0x001,
			FLAG_RIGHT_TO_LEFT      = 0x002,
			FLAG_UPDATE_LAYOUT      = 0x004,
			FLAG_UPDATE_HEIGHT      = 0x008,
			FLAG_USE_THEME          = 0x010,
			FLAG_UPDATE_THEME       = 0x020,
			FLAG_SHOW_GRIPPER       = 0x040,
			FLAG_AUTO_GRIPPER       = 0x080,
			FLAG_USE_STYLE_METRICS  = 0x100,
			FLAG_INIT_STYLE_METRICS = 0x200,
			FLAG_AUTO_REDRAW        = 0x400
		};

		enum
		{
			MOUSE_BUTTON_LEFT,
			MOUSE_BUTTON_RIGHT
		};

	private:
		enum
		{
			PANE_FLAG_PRIV_UPDATE_TEXT  = 0x100,
			PANE_FLAG_PRIV_UPDATE_ICON  = 0x200,
			PANE_FLAG_PRIV_WIDTH_SET    = 0x400,
			PANE_FLAG_PRIV_ALPHA_BITMAP = 0x800
		};

		struct Pane
		{
			tstring text;
			HBITMAP icon;
			int width;
			int minWidth;
			int maxWidth;
			uint16_t weight;
			uint16_t iconWidth;
			uint16_t iconHeight;
			uint32_t textWidth;
			uint16_t align;
			uint16_t flags;
		};

	public:
		class Callback
		{
		public:
			virtual void statusPaneClicked(int pane, int button, POINT pt) = 0;
			virtual void drawStatusPane(int pane, HDC hdc, const RECT& rc) = 0;
			virtual bool isStatusPaneEmpty(int pane) const = 0;
		};

		struct PaneMargins
		{
			int horizSpace;
			int topSpace;
			int bottomSpace;
		};

		struct Margins
		{
			int left;
			int top;
			int right;
			int bottom;
		};

		StatusBarCtrl();
		~StatusBarCtrl() { cleanup(); }

		enum
		{
			PANE_FLAG_MOUSE_CLICKS = 1,
			PANE_FLAG_NO_DECOR     = 2,
			PANE_FLAG_HIDE_EMPTY   = 4,
			PANE_FLAG_CUSTOM_DRAW  = 8,
			VALID_FLAGS_MASK = PANE_FLAG_MOUSE_CLICKS | PANE_FLAG_NO_DECOR | PANE_FLAG_HIDE_EMPTY | PANE_FLAG_CUSTOM_DRAW
		};

		enum
		{
			ALIGN_LEFT  = 0,
			ALIGN_RIGHT = 1
		};

		enum
		{
			PANE_STYLE_DEFAULT,
			PANE_STYLE_BEVEL,
			PANE_STYLE_LINE,
		};

		enum
		{
			COLOR_TYPE_TRANSPARENT,
			COLOR_TYPE_RGB,
			COLOR_TYPE_SYSCOLOR
		};

		struct PaneInfo
		{
			int minWidth;
			int maxWidth;
			uint16_t weight;
			uint16_t align;
			uint16_t flags;
		};

		int getNumPanes() const { return (int) panes.size(); }
		void addPane(const PaneInfo& pi);
		void setPanes(int count, const PaneInfo pi[]);
		void setPaneText(int index, const tstring& text);
		const tstring& getPaneText(int index) const;
		void setPaneIcon(int index, HBITMAP hBitmap);
		HBITMAP getPaneIcon(int index) const;
		void setPaneInfo(int index, const PaneInfo& pi);
		void getPaneInfo(int index, PaneInfo& pi) const;
		int getPaneWidth(int index) const;
		void updateLayout(HDC hdc);
		void setFont(HFONT hFont, bool ownFont);
		int getPrefHeight(HDC hdc);
		int getPaneContentWidth(HDC hdc, int index, bool includePadding);
		void setCallback(Callback* p) { callback = p; }
		void setUseTheme(bool flag);
		void setShowGripper(bool flag);
		void setAutoGripper(bool flag);
		void setRightToLeft(bool flag);
		void setAutoRedraw(bool flag);
		void setMinHeight(int height);
		void setIconSpace(int space);
		void setPanePadding(const Margins& p);
		void setPaneMargins(const PaneMargins& p);
		void getPaneRect(int index, RECT& rc) const;
		int getFlags() const { return flags; }
		int getNumVisiblePanes() const;
		int getFirstVisibleIndex() const;
		int getLastVisibleIndex() const;
		void forceUpdate();
		int resolveStyle() const;

	protected:
		void init();
		void initStyleMetrics();
		void cleanup();
		void draw(HDC hdc, const RECT& rcClient);
		bool isPaneEmpty(int index) const;
		void updateContentSize(Pane& p, HDC hdc);
		void updateWidth(Pane& p, HDC hdc);
		void updateTheme();
		void calcWidth(int availWidth, HDC hdc);
		void drawSeparator(HDC hdc, const RECT& rc);
		void drawContent(HDC hdc, const RECT& rc, const Pane& p);
		int findPane(POINT pt) const;
		void updateFontHeight(HDC hdc);
		void updateGripperState();
		void setFlag(bool enable, int flag);
		void handleClick(LPARAM lParam, int button);

	private:
		std::vector<Pane> panes;
		int minHeight;
		int fontHeight;
		int paneStyle;
		int iconSpace;
		Margins padding;
		PaneMargins margins;
		int backgroundType;
		COLORREF backgroundColor;
		int separatorType;
		COLORREF separatorColor;
		int textType;
		COLORREF textColor;
		HFONT hFont;
		int gripperSize;
		int flags;
		HWND hWndParent;
		BackingStore* backingStore;
		Callback* callback;

		BEGIN_MSG_MAP(StatusBarCtrl)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_RBUTTONDOWN, onRButtonDown)
		MESSAGE_HANDLER(WM_THEMECHANGED, onThemeChanged)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return 1; }
		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSize(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onRButtonDown(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
};

#endif // STATUS_BAR_CTRL_H_
