#ifndef TEXT_HOST_CTRL_H_
#define TEXT_HOST_CTRL_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "TextHostImpl.h"
#include "ThemeWrapper.h"

class BackingStore;

class TextHostCtrl : public CWindowImpl<TextHostCtrl>, public ThemeWrapper
{
	public:
		TextHostCtrl();
		~TextHostCtrl() { cleanup(); }

		void SetBackgroundColor(COLORREF color);
		void SetSel(int start, int end);
		int SetSel(CHARRANGE& cr);
		void GetSel(int& startChar, int& endChar) const;
		void GetSel(CHARRANGE& cr) const;
		void ReplaceSel(const WCHAR* text, BOOL canUndo = FALSE);
		BOOL SetCharFormat(CHARFORMAT2& cf, WORD flags);
		DWORD GetSelectionCharFormat(CHARFORMAT2& cf) const;
		BOOL SetParaFormat(PARAFORMAT& pf);
		int GetSelText(TCHAR* buf) const;
		int GetTextRange(int startChar, int endChar, TCHAR* text) const;
		void LimitText(int chars = 0);
		int GetLineCount() const;
		int LineIndex(int line = -1) const;
		int LineLength(int line = -1) const;
		int LineFromChar(int index) const;
		POINT PosFromChar(int chr) const;
		int CharFromPos(POINT pt) const;
		int GetTextLengthEx(GETTEXTLENGTHEX* gtl) const;
		int GetTextLengthEx(DWORD flags = GTL_DEFAULT, UINT codepage = CP_ACP) const;
		int GetTextLength() const;
		int GetLine(int index, TCHAR* buf, int maxLength) const;
		DWORD FindWordBreak(int code, int startChar);
		void SetAutoURLDetect(BOOL autoDetect);
		UINT SetUndoLimit(UINT undoLimit);
		DWORD SetEventMask(DWORD eventMask);
		DWORD GetEventMask() const;
		int FindText(DWORD flags, FINDTEXTEX& ft) const;
		IRichEditOle* GetOleInterface() const;
		BOOL SetOleCallback(IRichEditOleCallback* pCallback);

	protected:
		void cleanup();
		void updateTheme();
		void drawBackground(HDC hDC, const RECT& rc);

	private:
		TextHostImpl* textHost;
		BackingStore* backingStore;
		int flags;
		int borderWidth, borderHeight;
		HBRUSH backBrush;
		COLORREF backColor;

		BEGIN_MSG_MAP(TextHostCtrl)
		MESSAGE_HANDLER(WM_NCCREATE, onNcCreate)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_THEMECHANGED, onThemeChanged)
		MESSAGE_HANDLER(WM_SETTEXT, onCtrlMessage)
		MESSAGE_HANDLER(WM_GETTEXT, onCtrlMessage)
		MESSAGE_HANDLER(WM_GETTEXTLENGTH, onCtrlMessage)
		MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, onCtrlMessage)
		MESSAGE_RANGE_HANDLER(WM_KEYFIRST, WM_KEYLAST, onCtrlMessage)
		MESSAGE_HANDLER(WM_IME_CHAR, onCtrlMessage)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_KILLFOCUS, onKillFocus)
		MESSAGE_HANDLER(WM_VSCROLL, onVScroll)
		MESSAGE_HANDLER(WM_HSCROLL, onCtrlMessage)
		MESSAGE_HANDLER(WM_TIMER, onCtrlMessage)
		MESSAGE_HANDLER(WM_GETDLGCODE, onGetDlgCode)
		MESSAGE_HANDLER(WM_SETCURSOR, onSetCursor)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onNcCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return 1; }
		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onSize(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onCtrlMessage(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onKillFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onGetDlgCode(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};

#endif // TEXT_HOST_CTRL_H_
