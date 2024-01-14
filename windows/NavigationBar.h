#ifndef NAVIGATION_BAR_H_
#define NAVIGATION_BAR_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlcrack.h>

#include <vector>
#include "ThemeWrapper.h"
#include "AutoImageList.h"
#include "ListPopup.h"

class BackingStore;

class NavBarEditBox : public CWindowImpl<NavBarEditBox, CEdit>
{
	public:
		DECLARE_WND_SUPERCLASS(_T("NavBarEditBox"), WC_EDIT);

		NavBarEditBox() {}

		BEGIN_MSG_MAP(NavBarEditBox)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		END_MSG_MAP()

		HWND notifWnd;

	private:
		LRESULT onChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
};

class NavigationBar : public CWindowImpl<NavigationBar>
{
	public:
		class Callback
		{
			public:
				// same as ListPopup flags
				enum
				{
					IF_CHECKED = 1,
					IF_DEFAULT = 2
				};

				struct Item
				{
					tstring text;
					HBITMAP icon;
					uintptr_t data;
					uint16_t flags;
				};

				struct HistoryItem
				{
					tstring text;
					HBITMAP icon;
				};

				virtual void selectItem(int index) = 0;
				virtual void getPopupItems(int index, std::vector<Item>& res) const = 0;
				virtual void selectPopupItem(int index, const tstring& text, uintptr_t itemData) = 0;
				virtual tstring getCurrentPath() const = 0;
				virtual HBITMAP getCurrentPathIcon() const = 0;
				virtual bool setCurrentPath(const tstring& path) = 0;
				virtual uint64_t getHistoryState() const = 0;
				virtual void getHistoryItems(std::vector<HistoryItem>& res) const = 0;
				virtual tstring getHistoryItem(int index) const = 0;
				virtual HBITMAP getChevronMenuImage(int index, uintptr_t itemData) = 0;
		};

		NavigationBar();
		~NavigationBar();

		NavigationBar(const NavigationBar&) = delete;
		NavigationBar& operator= (const NavigationBar&) = delete;

		void addTextItem(const tstring& text, uintptr_t data, bool hasArrow = true);
		void addArrowItem(uintptr_t data);
		void setIcon(HBITMAP hBitmap);
		void setCallback(Callback* cb) { callback = cb; }
		void setEditMode(bool enable);
		void removeAllItems();
		int getPopupIndex() const { return popupIndex; }
		void setAnimationEnabled(bool flag) { animationEnabled = flag; }
		void setAnimationDuration(int value) { animationDuration = value; } // -1 = use default for this theme
		void setFont(HFONT hFont, bool ownFont);
		static int getPrefHeight(HDC hdc, HFONT hFont);
		bool isEditMode() const { return (flags & FLAG_EDIT_MODE) != 0; }
		HWND getEditCtrl() const { return editBox.m_hWnd; }

		BEGIN_MSG_MAP(NavigationBar)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, onLButtonUp)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, onMouseLeave)
		MESSAGE_HANDLER(WM_THEMECHANGED, onThemeChanged)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WMU_LIST_POPUP_RESULT, onPopupResult)
		MESSAGE_HANDLER(WMU_EXIT_EDIT_MODE, onExitEditMode)
		COMMAND_CODE_HANDLER(CBN_KILLFOCUS, onComboKillFocus)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onComboSelChange)
		END_MSG_MAP()

	private:
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return 1; }
		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSize(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onTimer(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onPopupResult(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onExitEditMode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onComboKillFocus(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onComboSelChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	private:
		enum
		{
			BF_PUSH    = 0x01,
			BF_TEXT    = 0x02,
			BF_ARROW   = 0x04,
			BF_PRESSED = 0x08,
			BF_HOT     = 0x10,
			BF_CHEVRON = 0x20
		};

		enum
		{
			FLAG_HAS_CHEVRON     = 0x0001,
			FLAG_HAS_ICON        = 0x0002,
			FLAG_PRESSED         = 0x0004,
			FLAG_UNDER_MOUSE     = 0x0008,
			FLAG_EDIT_MODE       = 0x0010,
			FLAG_POPUP_VISIBLE   = 0x0020,
			FLAG_WANT_LAYOUT     = 0x0040,
			FLAG_INIT_METRICS    = 0x0080,
			FLAG_INIT_DD_BITMAP  = 0x0100,
			FLAG_UPDATE_WIDTH    = 0x0200,
			FLAG_NO_ROOM         = 0x0400,
			FLAG_OWN_FONT        = 0x0800,
			FLAG_MOUSE_TRACKING  = 0x1000,
			FLAG_HIDING_POPUP    = 0x2000,
			FLAG_TIMER_ANIMATION = 0x4000,
			FLAG_TIMER_CLEANUP   = 0x8000
		};

		enum
		{
			HT_OUTSIDE,
			HT_EMPTY,
			HT_ITEM,
			HT_ITEM_DD,
			HT_BUTTON,
			HT_ICON,
			HT_CHEVRON
		};

		struct Item;

		struct StateTransition
		{
			HBITMAP bitmaps[3];
			uint8_t* bits[3];
			int states[2];
			int nextState;
			RECT rc;
			int64_t startTime;
			double currentValue;
			double startValue;
			double endValue;
			int currentAlpha;
			int duration;
			bool running;

			StateTransition();
			~StateTransition() { cleanup(); }

			void cleanup();
			bool update(int64_t time, int64_t frequency);
			void updateAlpha();
			void updateImage();
			void draw(HDC hdc);
			void createBitmaps(NavigationBar* navBar, HDC hdc, const RECT& rcClient, const Item& item);
			void start(int64_t time, int duration);
			void reverse(int64_t time, int totalDuration);
			bool isForward() const { return startValue < endValue; }
			bool nextTransition();
			int getCompletedState() const;
		};

		struct Item
		{
			tstring text;
			uintptr_t data;
			int autoWidth;
			int xpos;
			int width;
			int flags;
			int currentState;
			StateTransition* trans;
		};

		struct UpdateAnimationState
		{
			HDC hdc;
			RECT rc;
			int64_t timestamp;
			int64_t frequency;
			bool update;
			bool running;
		};

		Callback* callback;
		std::vector<Item> crumbs;
		std::vector<Item> buttons;
		Item chevron;

		int flags;
		int iconSize;
		int glyphSize;
		int buttonSize;
		int popupPosOffset;
		int comboDropDownHeight;
		int minItemWidth;
		int paddingLeft, paddingRight;
		int iconPaddingLeft, iconPaddingRight;
		int hotIndex;
		int hotType;
		int pressedType;
		int popupIndex;
		MARGINS margins;
		bool animationEnabled;
		int animationDuration;

		ThemeWrapper themeToolbar;
		ThemeWrapper themeAddressBar;
		ThemeWrapper themeBreadcrumbs;

		HFONT hFont;

		HBITMAP iconBmp;
		int iconBitmapWidth, iconBitmapHeight;
		AutoImageList imageList;
		HBITMAP bmpThemedDropDown;

		enum
		{
			BNT_H_ARROW,
			BNT_V_ARROW,
			BNT_CHEVRON,
			BNT_DROPDOWN,
			MAX_NT_BITMAPS
		};

		HBITMAP bmpNonThemed[MAX_NT_BITMAPS];

		BackingStore* backingStore;

		ListPopup popup;
		CComboBoxEx comboBox;
		NavBarEditBox editBox;

		uint64_t historyState;

		void addToolbarButton(uintptr_t data);
		void layout(const RECT& rc);
		void clampItemWidth(int &width, int autoWidth) const;
		int hitTest(POINT pt, int& index) const;
		int getButtonState(int flags) const;
		void clearHotItem();
		void updateAutoWidth(HDC hdc);
		void updateIconSize();
		void cleanup();
		void clearFont();
		void createFont();
		void draw(HDC hdc, const RECT& rcClient);
		void drawBackground(HDC hdc, const RECT& rcClient);
		void drawButton(HDC hdc, const RECT& rcClient, Item& item);
		void drawThemeButton(HDC hdc, const RECT& rcButton, const Item& item, int stateId);
		void drawChevron(HDC hdc, const RECT& rcClient);
		void drawIcon(HDC hdc, const RECT& rcClient);
		void trackMouseEvent();
		void initTheme();
		void initMetrics(HDC hdc);
		void showPopupWindow(int xpos);
		int chevronPopupIndexToItemIndex(int chevronIndex) const;
		void showPopup(int index);
		void hidePopup();
		void showChevronPopup();
		void enterEditMode(bool showDropDown);
		void exitEditMode();
		void fillHistoryComboBox();
		void createNonThemeResources(int flags);
		HBITMAP getThemeDropDownBitmap();
		void updateAnimationState();
		void cleanupAnimationState();
		void updateStateTransition(Item& item, int newState);
		void cancelStateTransitions(std::vector<Item>& v);
		void cancelStateTransition(Item& item);
		void removeStateTransitions(std::vector<Item>& v);
		void removeStateTransition(Item& item);
		void cancelStateTransitions(bool remove);
		void cleanupAnimationState(Item& item, UpdateAnimationState& uas, int64_t delay);
		void updateAnimationState(Item& item, UpdateAnimationState& uas);
		int getTransitionDuration(int oldState, int newState) const;
		void startTimer(int id, int flag, int time);
		void stopTimer(int id, int flag);
};

#endif // NAVIGATION_BAR_H_
