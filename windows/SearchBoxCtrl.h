#ifndef SEARCH_BOX_CTRL_H_
#define SEARCH_BOX_CTRL_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "../client/tstring.h"
#include "ThemeWrapper.h"
#include "VistaAnimations.h"

class BackingStore;

#define WMU_RETURN (WM_USER + 96)

class SearchBoxEdit : public CWindowImpl<SearchBoxEdit, CEdit>
{
	public:
		DECLARE_WND_SUPERCLASS(_T("SearchBoxEdit"), WC_EDIT);

		SearchBoxEdit() {}

		class SearchBoxCtrl* parent = nullptr;

	private:
		BEGIN_MSG_MAP(SearchBoxEdit)
		MESSAGE_HANDLER(WM_KEYDOWN, onKeyDown)
		MESSAGE_HANDLER(WM_GETDLGCODE, onGetDlgCode)
		END_MSG_MAP()

		LRESULT onKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onGetDlgCode(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

		bool isInDialog = false;
};

class SearchBoxCtrl: public CWindowImpl<SearchBoxCtrl>, private ThemeWrapper
{
		friend class SearchBoxEdit;

	public:
		enum
		{
			BORDER_TYPE_FRAME,
			BORDER_TYPE_EDIT
		};

		enum
		{
			CLR_NORMAL_BORDER,
			CLR_HOT_BORDER,
			CLR_SELECTED_BORDER,
			CLR_BACKGROUND,
			CLR_TEXT,
			CLR_HINT,
			MAX_COLORS,
			MAX_BRUSHES = CLR_BACKGROUND + 1
		};

		SearchBoxCtrl();
		~SearchBoxCtrl() { cleanup(); }

		SearchBoxCtrl(const SearchBoxCtrl&) = delete;
		SearchBoxCtrl& operator= (const SearchBoxCtrl&) = delete;

		void setBitmap(HBITMAP hBitmap);
		void setCloseBitmap(HBITMAP hBitmap);
		void setText(const TCHAR* text);
		void setText(const tstring& text) { setText(text.c_str()); }
		tstring getText() const;
		void setHint(const tstring& s);
		void setMargins(const MARGINS& margins);
		void setColor(int what, COLORREF color);
		void setUseCustomColors(bool flag);
		void setBorderType(int type);
		void setAnimationEnabled(bool flag);
		void setAnimationDuration(int value) { animationDuration = value; } // -1 = use default for this theme

	private:
		BEGIN_MSG_MAP(SearchBoxCtrl)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SETTEXT, onSetText)
		MESSAGE_HANDLER(WM_SETFONT, onSetFont)
		MESSAGE_HANDLER(WM_GETFONT, onGetFont)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
		MESSAGE_HANDLER(WM_SETCURSOR, onSetCursor)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColorEdit)
		COMMAND_CODE_HANDLER(EN_SETFOCUS, onEditSetFocus)
		COMMAND_CODE_HANDLER(EN_KILLFOCUS, onEditKillFocus)
		COMMAND_CODE_HANDLER(EN_CHANGE, onEditChange)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { cleanup(); return 0; }
		LRESULT onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSetFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onGetFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return TRUE; }
		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSize(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSetFocus(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onMouseMove(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSetCursor(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onCtlColorEdit(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onTimer(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onEditSetFocus(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/);
		LRESULT onEditKillFocus(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/);

		enum
		{
			FLAG_EDIT_VISIBLE      = 0x001,
			FLAG_HAS_EDIT_TEXT     = 0x002,
			FLAG_FOCUSED           = 0x004,
			FLAG_HOT               = 0x008,
			FLAG_CUSTOM_COLORS     = 0x010,
			FLAG_ENABLE_ANIMATION  = 0x040,
			FLAG_TIMER_ANIMATION   = 0x100,
			FLAG_TIMER_CLEANUP     = 0x200,
			FLAG_TIMER_CHECK_MOUSE = 0x400
		};

		SearchBoxEdit edit;
		tstring hintText;
		HFONT hFont;
		HBITMAP hBitmap[2];
		SIZE bitmapSize[2];
		int textHeight;
		int iconSpace;
		MARGINS margins;
		MARGINS border;
		HBRUSH hBrush[MAX_BRUSHES];
		COLORREF colors[MAX_COLORS];
		HCURSOR hCursor[2];
		BackingStore* backingStore;
		int flags;
		int borderType;
		int animationDuration;
		int borderCurrentState;
		VistaAnimations::StateTransition* borderTrans;

		void updateTextHeight(HDC hdc);
		void draw(HDC hdc, const RECT& rc);
		void drawBackground(HDC hdc, const RECT& rc);
		void drawBorder(HDC hdc, RECT& rc, int stateId);
		void cleanup();
		void initTheme();
		void calcEditRect(const RECT& rcClient, RECT& rc);
		void calcCloseButtonRect(const RECT& rcClient, RECT& rc) const;
		void createEdit();
		void setFocusToEdit();
		void sendWmCommand(int code);
		void setBitmap(HBITMAP hBitmap, int index);
		int getTransitionDuration(int oldState, int newState) const;
		void initStateTransitionBitmaps(HDC hdc, const RECT& rcClient);
		void setBorderState(int newState);
		void updateAnimationState();
		void cleanupAnimationState();
		void checkMouse();
		void updateCheckMouseTimer();
		void startTimer(int id, int flag, int time);
		void stopTimer(int id, int flag);
		void returnPressed();
		void tabPressed();
		void escapePressed();
};

#endif // SEARCH_BOX_CTRL_H_
