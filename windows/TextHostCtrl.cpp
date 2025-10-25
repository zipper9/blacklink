#include "stdafx.h"
#include "TextHostCtrl.h"
#include "BackingStore.h"
#include "UxThemeLib.h"
#include "GdiUtil.h"
#include <algorithm>
#include <vssym32.h>

#define UX UxThemeLib::instance

#undef min
#undef max

enum
{
	FLAG_APP_THEMED   = 1,
	FLAG_UPDATE_THEME = 2,
	FLAG_FOCUSED      = 4
};

static inline bool isAppThemed()
{
	UX.init();
	return UX.pIsAppThemed && UX.pIsAppThemed();
}

TextHostCtrl::TextHostCtrl() :
	textHost(nullptr), backingStore(nullptr), flags(0),
	borderWidth(0), borderHeight(0), backBrush(nullptr), backColor(0)
{
}

void TextHostCtrl::cleanup()
{
	if (textHost)
	{
		textHost->Release();
		textHost = nullptr;
	}
	if (backBrush)
	{
		DeleteObject(backBrush);
		backBrush = nullptr;
	}
	closeTheme();
	flags |= FLAG_UPDATE_THEME;
}

LRESULT TextHostCtrl::onNcCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
	int oldStyle = cs->style;
	if (oldStyle & WS_BORDER)
	{
		borderWidth = 2;
		borderHeight = 2;
	}
	if (!textHost)
	{
		TextHostImpl* impl = new TextHostImpl;
		if (!impl->init(m_hWnd, *cs))
		{
			delete impl;
			return -1;
		}
		textHost = impl;
	}
	if (cs->style != oldStyle) SetWindowLong(GWL_STYLE, cs->style);
	backColor = GetSysColor(COLOR_WINDOW);
	bHandled = FALSE;
	return 0;
}

LRESULT TextHostCtrl::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	flags |= FLAG_UPDATE_THEME;
	return 0;
}

LRESULT TextHostCtrl::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	cleanup();
	return 0;
}

LRESULT TextHostCtrl::onPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	RECT rcClient;
	GetClientRect(&rcClient);
	if (flags & FLAG_UPDATE_THEME) updateTheme();
	bool drawn = false;
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);
	RECT& rc = ps.rcPaint;
	if (!backingStore) backingStore = BackingStore::getBackingStore();
	if (backingStore)
	{
		HDC hMemDC = backingStore->getCompatibleDC(hdc, rcClient.right, rcClient.bottom);
		if (hMemDC)
		{
			drawBackground(hMemDC, rcClient);
			InflateRect(&rcClient, -borderWidth, -borderHeight);
			if (textHost) textHost->draw(hMemDC, rc);
			BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hMemDC, rc.left, rc.top, SRCCOPY);
			drawn = true;
		}
	}
	if (!drawn)
	{
		drawBackground(hdc, rcClient);
		InflateRect(&rcClient, -borderWidth, -borderHeight);
		if (textHost) textHost->draw(hdc, rcClient);
	}
	EndPaint(&ps);
	return 0;
}

LRESULT TextHostCtrl::onSize(UINT, WPARAM, LPARAM, BOOL&)
{
	if (textHost)
	{
		RECT rc;
		GetClientRect(&rc);
		InflateRect(&rc, -borderWidth, -borderHeight);
		textHost->setWindowRect(rc);
	}
	return 0;
}

LRESULT TextHostCtrl::onThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	closeTheme();
	flags |= FLAG_UPDATE_THEME;
	return 0;
}

LRESULT TextHostCtrl::onCtrlMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (textHost)
	{
		LRESULT lRes;
		HRESULT hr = textHost->sendMessage(uMsg, wParam, lParam, &lRes);
		if (SUCCEEDED(hr) && hr != S_FALSE) return lRes;
	}
	bHandled = FALSE;
	return 0;
}

LRESULT TextHostCtrl::onVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!textHost) return 0;
	LRESULT lRes;
	if (textHost->isLargeSBRange() && (LOWORD(wParam) == SB_THUMBTRACK || LOWORD(wParam) == SB_THUMBPOSITION))
	{
		CScrollBar& scroll = textHost->getVScroll();
		SCROLLINFO si;
		si.cbSize = sizeof(si);
		si.fMask = SIF_TRACKPOS;
		scroll.GetScrollInfo(&si);

		POINT pt;
		textHost->sendMessage(EM_GETSCROLLPOS, 0, (LPARAM) &pt, &lRes);
		pt.y = si.nTrackPos;
		textHost->sendMessage(EM_SETSCROLLPOS, 0, (LPARAM) &pt, &lRes);
	}
	else
		textHost->sendMessage(uMsg, wParam, lParam, &lRes);
	return 0;
}

LRESULT TextHostCtrl::onSetFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	int oldFlags = flags;
	flags |= FLAG_FOCUSED;
	if (oldFlags != flags) Invalidate();
	return onCtrlMessage(uMsg, wParam, lParam, bHandled);
}

LRESULT TextHostCtrl::onKillFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	int oldFlags = flags;
	flags &= ~FLAG_FOCUSED;
	if (oldFlags != flags) Invalidate();
	return onCtrlMessage(uMsg, wParam, lParam, bHandled);
}

LRESULT TextHostCtrl::onGetDlgCode(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	//if (lParam) isInDialog = true;
	//return DLGC_WANTALLKEYS;
	return DLGC_WANTARROWS | DLGC_WANTCHARS;
}

LRESULT TextHostCtrl::onSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if ((HWND) wParam == m_hWnd && textHost)
	{
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);
		if (!textHost->setCursor(pt)) bHandled = FALSE;
	}
	else
		bHandled = FALSE;
	return 0;
}

void TextHostCtrl::SetBackgroundColor(COLORREF color)
{
	if (backColor != color)
	{
		if (backBrush)
		{
			DeleteObject(backBrush);
			backBrush = nullptr;
		}
		backColor = color;
	}
}

void TextHostCtrl::SetSel(int start, int end)
{
	CHARRANGE cr;
	cr.cpMin = start;
	cr.cpMax = end;
	SetSel(cr);
}

int TextHostCtrl::SetSel(CHARRANGE &cr)
{
	LRESULT lResult = 0;
	if (textHost)
		textHost->sendMessage(EM_EXSETSEL, 0, (LPARAM) &cr, &lResult);
	return (int) lResult;
}

void TextHostCtrl::ReplaceSel(const WCHAR* text, BOOL canUndo)
{
	if (!textHost) return;
	LRESULT lResult;
	textHost->sendMessage(EM_REPLACESEL, (WPARAM) canUndo, (LPARAM) text, &lResult);
}

void TextHostCtrl::GetSel(int& startChar, int& endChar) const
{
	CHARRANGE cr;
	GetSel(cr);
	startChar = cr.cpMin;
	endChar = cr.cpMax;
}

void TextHostCtrl::GetSel(CHARRANGE &cr) const
{
	if (!textHost)
	{
		cr.cpMin = cr.cpMax = -1;
		return;
	}
	LRESULT lResult;
	textHost->sendMessage(EM_EXGETSEL, 0, (LPARAM) &cr, &lResult);
}

BOOL TextHostCtrl::SetCharFormat(CHARFORMAT2& cf, WORD flags)
{
	if (!textHost) return FALSE;
	cf.cbSize = sizeof(CHARFORMAT2);
	LRESULT lResult;
	return SUCCEEDED(textHost->sendMessage(EM_SETCHARFORMAT, flags, (LPARAM) &cf, &lResult));
}

DWORD TextHostCtrl::GetSelectionCharFormat(CHARFORMAT2& cf) const
{
	if (!textHost) return 0;
	cf.cbSize = sizeof(CHARFORMAT2);
	LRESULT lResult;
	textHost->sendMessage(EM_GETCHARFORMAT, 1, (LPARAM) &cf, &lResult);
	return (DWORD) lResult;
}

BOOL TextHostCtrl::SetParaFormat(PARAFORMAT& pf)
{
	if (!textHost) return FALSE;
	pf.cbSize = sizeof(PARAFORMAT);
	LRESULT lResult;
	textHost->sendMessage(EM_SETPARAFORMAT, 0, (LPARAM) &pf, &lResult);
	return (BOOL) lResult;
}

int TextHostCtrl::GetTextRange(int startChar, int endChar, TCHAR* text) const
{
	if (!textHost) return 0;
	TEXTRANGE tr;
	tr.chrg.cpMin = startChar;
	tr.chrg.cpMax = endChar;
	tr.lpstrText = text;
	LRESULT lResult;
	textHost->sendMessage(EM_GETTEXTRANGE, 0, (LPARAM) &tr, &lResult);
	return (int) lResult;
}

int TextHostCtrl::GetSelText(TCHAR* buf) const
{
	if (!textHost) return 0;
	LRESULT lResult;
	textHost->sendMessage(EM_GETSELTEXT, 0, (LPARAM) buf, &lResult);
	return (int) lResult;
}

void TextHostCtrl::LimitText(int chars)
{
	if (!textHost) return;
	LRESULT lResult;
	textHost->sendMessage(EM_EXLIMITTEXT, 0, chars, &lResult);
}

int TextHostCtrl::GetLineCount() const
{
	if (!textHost) return -1;
	LRESULT lResult;
	textHost->sendMessage(EM_GETLINECOUNT, 0, 0, &lResult);
	return (int) lResult;
}

int TextHostCtrl::LineIndex(int line) const
{
	if (!textHost) return -1;
	LRESULT lResult;
	textHost->sendMessage(EM_LINEINDEX, line, 0, &lResult);
	return (int) lResult;
}

int TextHostCtrl::LineLength(int line) const
{
	if (!textHost) return -1;
	LRESULT lResult;
	textHost->sendMessage(EM_LINELENGTH, line, 0, &lResult);
	return (int) lResult;
}

int TextHostCtrl::LineFromChar(int index) const
{
	if (!textHost) return -1;
	LRESULT lResult;
	textHost->sendMessage(EM_EXLINEFROMCHAR, 0, index, &lResult);
	return (int) lResult;
}

POINT TextHostCtrl::PosFromChar(int chr) const
{
	POINT pt = {};
	if (!textHost) return pt;
	LRESULT lResult;
	textHost->sendMessage(EM_POSFROMCHAR, (WPARAM) &pt, chr, &lResult);
	return pt;
}

int TextHostCtrl::CharFromPos(POINT pt) const
{
	if (!textHost) return -1;
	LRESULT lResult;
	textHost->sendMessage(EM_CHARFROMPOS, 0, (LPARAM) &pt, &lResult);
	return (int) lResult;
}

int TextHostCtrl::GetTextLengthEx(GETTEXTLENGTHEX* gtl) const
{
	if (!textHost) return -1;
	LRESULT lResult;
	textHost->sendMessage(EM_GETTEXTLENGTHEX, (WPARAM) gtl, 0, &lResult);
	return (int) lResult;
}

int TextHostCtrl::GetTextLengthEx(DWORD flags, UINT codepage) const
{
	GETTEXTLENGTHEX gtl;
	gtl.flags = flags;
	gtl.codepage = codepage;
	return GetTextLengthEx(&gtl);
}

int TextHostCtrl::GetTextLength() const
{
	GETTEXTLENGTHEX gtl;
	gtl.flags = GTL_NUMCHARS | GTL_PRECISE | GTL_USECRLF;
	gtl.codepage = 1200;
	return GetTextLengthEx(&gtl);
}

int TextHostCtrl::GetLine(int index, TCHAR* buf, int maxLength) const
{
	if (!textHost) return 0;
	*(DWORD*) buf = maxLength;
	LRESULT lResult;
	textHost->sendMessage(EM_GETLINE, index, (LPARAM) buf, &lResult);
	return (int) lResult;
}

DWORD TextHostCtrl::FindWordBreak(int code, int startChar)
{
	if (!textHost) return 0;
	LRESULT lResult;
	textHost->sendMessage(EM_FINDWORDBREAK, code, startChar, &lResult);
	return (int) lResult;
}

void TextHostCtrl::SetAutoURLDetect(BOOL autoDetect)
{
	if (!textHost) return;
	LRESULT lResult;
	textHost->sendMessage(EM_AUTOURLDETECT, autoDetect, 0, &lResult);
}

UINT TextHostCtrl::SetUndoLimit(UINT undoLimit)
{
	if (!textHost) return 0;
	LRESULT lResult;
	textHost->sendMessage(EM_SETUNDOLIMIT, undoLimit, 0, &lResult);
	return (int) lResult;
}

DWORD TextHostCtrl::SetEventMask(DWORD eventMask)
{
	if (!textHost) return 0;
	LRESULT lResult;
	textHost->sendMessage(EM_SETEVENTMASK, 0, eventMask, &lResult);
	return (int) lResult;
}

DWORD TextHostCtrl::GetEventMask() const
{
	if (!textHost) return 0;
	LRESULT lResult;
	textHost->sendMessage(EM_GETEVENTMASK, 0, 0, &lResult);
	return (int) lResult;
}

int TextHostCtrl::FindText(DWORD flags, FINDTEXTEX& ft) const
{
	if (!textHost) return -1;
	LRESULT lResult;
#ifdef _UNICODE
	textHost->sendMessage(EM_FINDTEXTEXW, flags, (LPARAM) &ft, &lResult);
#else
	textHost->sendMessage(EM_FINDTEXTEX, flags, (LPARAM) &ft, &lResult);
#endif
	return (int) lResult;
}

IRichEditOle* TextHostCtrl::GetOleInterface() const
{
	return textHost ? textHost->queryRichEditOle() : nullptr;
}

BOOL TextHostCtrl::SetOleCallback(IRichEditOleCallback* pCallback)
{
	if (!textHost) return FALSE;
	LRESULT lResult;
	textHost->sendMessage(EM_SETOLECALLBACK, 0, (LPARAM) pCallback, &lResult);
	return (BOOL) lResult;
}

void TextHostCtrl::updateTheme()
{
	if (!hTheme)
	{
		if (isAppThemed())
		{
			flags |= FLAG_APP_THEMED;
			openTheme(m_hWnd, L"EDIT");
		}
		else
			flags &= ~FLAG_APP_THEMED;
	}
	flags &= ~FLAG_UPDATE_THEME;
}

void TextHostCtrl::drawBackground(HDC hDC, const RECT& rc)
{
	if (!backBrush) backBrush = CreateSolidBrush(backColor);
	if (!borderWidth || !borderHeight)
	{
		FillRect(hDC, &rc, backBrush);
		return;
	}
	RECT rc2;
	if (hTheme)
	{
		int stateId = (flags & FLAG_FOCUSED) ? ETS_SELECTED : ETS_NORMAL;
		UX.pDrawThemeBackground(hTheme, hDC, EP_EDITBORDER_NOSCROLL, stateId, &rc, nullptr);
		rc2.left = rc.left + borderWidth;
		rc2.top = rc.top + borderHeight;
		rc2.right = rc.right - borderWidth;
		rc2.bottom = rc.bottom - borderHeight;
	}
	else
	{
		WinUtil::drawEdge(hDC, rc, 1, GetSysColorBrush(COLOR_3DSHADOW), GetSysColorBrush(COLOR_3DHIGHLIGHT));
		rc2.left = rc.left + 1;
		rc2.top = rc.top + 1;
		rc2.right = rc.right - 1;
		rc2.bottom = rc.bottom - 1;
		WinUtil::drawEdge(hDC, rc2, 1, GetSysColorBrush(COLOR_3DDKSHADOW), GetSysColorBrush(COLOR_3DFACE));
		rc2.left++;
		rc2.top++;
		rc2.right--;
		rc2.bottom--;
	}
	FillRect(hDC, &rc2, backBrush);
}
