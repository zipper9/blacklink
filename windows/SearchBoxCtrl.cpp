#include "stdafx.h"
#include "SearchBoxCtrl.h"
#include "GdiUtil.h"
#include "BackingStore.h"
#include "WinUtil.h"
#include "UxThemeLib.h"
#include <vssym32.h>

#define UX UxThemeLib::instance

using namespace VistaAnimations;

static const int TIMER_UPDATE_ANIMATION = 5;
static const int TIMER_CLEANUP = 6;
static const int TIMER_CHECK_MOUSE = 7;
static const int CLEANUP_TIME = 15000;

static inline int64_t getHighResFrequency()
{
	LARGE_INTEGER x;
	if (!QueryPerformanceFrequency(&x)) return 0;
	return x.QuadPart;
}

static inline int64_t getHighResTimestamp()
{
	LARGE_INTEGER x;
	if (!QueryPerformanceCounter(&x)) return 0;
	return x.QuadPart;
}

LRESULT SearchBoxEdit::onKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	switch (wParam)
	{
		case VK_RETURN:
			if (parent) parent->returnPressed();
			break;

		case VK_TAB:
			if (isInDialog && parent) parent->tabPressed();
			break;

		case VK_ESCAPE:
			if (isInDialog && parent) parent->escapePressed();
			break;

		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT SearchBoxEdit::onGetDlgCode(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (lParam) isInDialog = true;
	return DLGC_WANTALLKEYS;
}

SearchBoxCtrl::SearchBoxCtrl()
{
	edit.parent = this;
	hFont = nullptr;
	hCursor[0] = hCursor[1] = nullptr;
	memset(&margins, 0, sizeof(margins));
	border.cxLeftWidth = border.cxRightWidth = border.cyTopHeight = border.cyBottomHeight = 3;
	for (int i = 0; i < 2; i++)
	{
		hBitmap[i] = nullptr;
		bitmapSize[i].cx = bitmapSize[i].cy = 0;
	}
	textHeight = -1;
	iconSpace = 4;
	backingStore = nullptr;
	flags = FLAG_ENABLE_ANIMATION;
	notifMask = NOTIF_RETURN | NOTIF_TAB | NOTIF_ESCAPE;
	for (int i = 0; i < MAX_BRUSHES; i++)
		hBrush[i] = nullptr;
	borderType = BORDER_TYPE_FRAME;
	borderCurrentState = ETS_NORMAL;
	borderTrans = nullptr;
	animationDuration = -1;
	colors[CLR_NORMAL_BORDER] = RGB(122, 122, 122);
	colors[CLR_HOT_BORDER] = RGB(16, 16, 16);
	colors[CLR_SELECTED_BORDER] = RGB(0, 120, 215);
	colors[CLR_BACKGROUND] = RGB(255, 255, 255);
	colors[CLR_TEXT] = RGB(0, 0, 0);
	colors[CLR_HINT] = RGB(87, 87, 87);
}

LRESULT SearchBoxCtrl::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	initTheme();
	return 0;
}

LRESULT SearchBoxCtrl::onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	tstring text(reinterpret_cast<const TCHAR*>(lParam));
	setText(text);
	return TRUE;
}

LRESULT SearchBoxCtrl::onSetFont(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	hFont = reinterpret_cast<HFONT>(wParam);
	if (edit.m_hWnd) edit.SendMessage(WM_SETFONT, (WPARAM) hFont, 0);
	return 0;
}

LRESULT SearchBoxCtrl::onGetFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return reinterpret_cast<LRESULT>(hFont);
}

LRESULT SearchBoxCtrl::onPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	RECT rc;
	GetClientRect(&rc);

	bool drawn = false;
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);

	if (textHeight == -1) updateTextHeight(hdc);
	if (!backingStore) backingStore = BackingStore::getBackingStore();
	if (backingStore)
	{
		HDC hMemDC = backingStore->getCompatibleDC(hdc, rc.right, rc.bottom);
		if (hMemDC)
		{
			draw(hMemDC, rc);
			BitBlt(hdc, 0, 0, rc.right, rc.bottom, hMemDC, 0, 0, SRCCOPY);
			drawn = true;
		}
	}
	if (!drawn) draw(hdc, rc);

	EndPaint(&ps);
	return 0;
}

LRESULT SearchBoxCtrl::onSize(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	if (edit.m_hWnd)
	{
		RECT rcClient, rc;
		GetClientRect(&rcClient);
		calcEditRect(rcClient, rc);
		edit.SetWindowPos(nullptr,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
			SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	return 0;
}

LRESULT SearchBoxCtrl::onSetFocus(UINT, WPARAM wParam, LPARAM, BOOL&)
{
	setFocusToEdit();
	Invalidate();
	return 0;
}

LRESULT SearchBoxCtrl::onLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	if ((flags & (FLAG_HAS_EDIT_TEXT | FLAG_EDIT_VISIBLE)) == (FLAG_HAS_EDIT_TEXT | FLAG_EDIT_VISIBLE)
		&& hBitmap[1])
	{
		RECT rc;
		GetClientRect(&rc);
		calcCloseButtonRect(rc, rc);
		if (PtInRect(&rc, pt))
		{
			edit.SetWindowText(_T(""));
			setFocusToEdit();
			Invalidate();
			return 0;
		}
	}
	setFocusToEdit();
	Invalidate();
	return 0;
}

LRESULT SearchBoxCtrl::onSetCursor(UINT, WPARAM, LPARAM, BOOL&)
{
	int index = 0;
	LPCTSTR id = IDC_IBEAM;
	if ((flags & FLAG_HAS_EDIT_TEXT) && hBitmap[1])
	{
		RECT rc;
		GetClientRect(&rc);
		calcCloseButtonRect(rc, rc);
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);
		if (PtInRect(&rc, pt))
		{
			index = 1;
			id = IDC_HAND;
		}
	}
	if (!hCursor[index]) hCursor[index] = LoadCursor(nullptr, id);
	SetCursor(hCursor[index]);
	return TRUE;
}

LRESULT SearchBoxCtrl::onCtlColorEdit(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
	if (flags & FLAG_CUSTOM_COLORS)
	{
		HDC hdc = (HDC) wParam;
		SetTextColor(hdc, colors[CLR_TEXT]);
		SetBkColor(hdc, colors[CLR_BACKGROUND]);
		if (!hBrush[CLR_BACKGROUND])
			hBrush[CLR_BACKGROUND] = CreateSolidBrush(colors[CLR_BACKGROUND]);
		return (LRESULT) hBrush[CLR_BACKGROUND];
	}
	return GetParent().SendMessage(WM_CTLCOLOREDIT, wParam, lParam);
}

LRESULT SearchBoxCtrl::onEditSetFocus(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	if (!(flags & FLAG_FOCUSED))
	{
		flags |= FLAG_FOCUSED;
		updateCheckMouseTimer();
		setBorderState(ETS_SELECTED);
		Invalidate();
	}
	return 0;
}

LRESULT SearchBoxCtrl::onEditKillFocus(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	tstring ts;
	WinUtil::getWindowText(edit.m_hWnd, ts);
	if (ts.empty())
	{
		edit.ShowWindow(SW_HIDE);
		flags &= ~FLAG_EDIT_VISIBLE;
	}
	flags &= ~FLAG_FOCUSED;
	updateCheckMouseTimer();
	setBorderState((flags & FLAG_HOT) ? ETS_HOT : ETS_NORMAL);
	Invalidate();
	return 0;
}

LRESULT SearchBoxCtrl::onEditChange(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	int oldFlags = flags;
	if (edit.GetWindowTextLength())
		flags |= FLAG_HAS_EDIT_TEXT;
	else
		flags &= ~FLAG_HAS_EDIT_TEXT;
	if (flags != oldFlags) Invalidate();
	sendWmCommand(EN_CHANGE);
	return 0;
}

LRESULT SearchBoxCtrl::onMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	int oldFlags = flags;
	flags |= FLAG_HOT;
	if (flags != oldFlags && !(flags & FLAG_FOCUSED))
	{
		updateCheckMouseTimer();
		setBorderState(ETS_HOT);
		Invalidate();
	}
	return 0;
}

LRESULT SearchBoxCtrl::onTimer(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	if (wParam == TIMER_UPDATE_ANIMATION)
		updateAnimationState();
	else if (wParam == TIMER_CLEANUP)
		cleanupAnimationState();
	else if (wParam == TIMER_CHECK_MOUSE)
		checkMouse();
	else
		bHandled = FALSE;
	return 0;
}

void SearchBoxCtrl::checkMouse()
{
	POINT pt;
	GetCursorPos(&pt);
	RECT rc;
	GetClientRect(&rc);
	ScreenToClient(&pt);
	int oldFlags = flags;
	if (PtInRect(&rc, pt))
		flags |= FLAG_HOT;
	else
		flags &= ~FLAG_HOT;
	if (flags != oldFlags && !(flags & FLAG_FOCUSED))
	{
		setBorderState((flags & FLAG_HOT) ? ETS_HOT : ETS_NORMAL);
		Invalidate();
	}
	updateCheckMouseTimer();
}

void SearchBoxCtrl::setFocusToEdit()
{
	if (!edit.m_hWnd) createEdit();
	if (!(flags & FLAG_EDIT_VISIBLE))
	{
		edit.ShowWindow(SW_SHOW);
		flags |= FLAG_EDIT_VISIBLE;
	}
	edit.SetFocus();
	flags |= FLAG_FOCUSED;
	updateCheckMouseTimer();
	setBorderState(ETS_SELECTED);
	Invalidate();
}

void SearchBoxCtrl::sendWmCommand(int code)
{
	HWND hWndParent = GetParent();
	if (hWndParent)
	{
		int id = GetDlgCtrlID() & 0xFFFF;
		::SendMessage(hWndParent, WM_COMMAND, code<<16 | id, (LPARAM) m_hWnd);
	}
}

void SearchBoxCtrl::updateTextHeight(HDC hdc)
{
	HGDIOBJ prevFont = SelectObject(hdc, hFont ? hFont : GetStockObject(DEFAULT_GUI_FONT));
	SIZE sz;
	GetTextExtentPoint32(hdc, _T("0"), 1, &sz);
	textHeight = sz.cy;
	SelectObject(hdc, prevFont);
}

void SearchBoxCtrl::setBitmap(HBITMAP hBitmap, int index)
{
	BITMAP bitmap;
	bitmapSize[index].cx = bitmapSize[index].cy = 0;
	if (hBitmap && GetObject(hBitmap, sizeof(bitmap), &bitmap))
	{
		bitmapSize[index].cx = bitmap.bmWidth;
		bitmapSize[index].cy = abs(bitmap.bmHeight);
	}
	this->hBitmap[index] = hBitmap;
}

void SearchBoxCtrl::setBitmap(HBITMAP hBitmap)
{
	setBitmap(hBitmap, 0);
}

void SearchBoxCtrl::setCloseBitmap(HBITMAP hBitmap)
{
	setBitmap(hBitmap, 1);
}

void SearchBoxCtrl::setText(const TCHAR* text)
{
	if (text && text[0])
	{
		if (!edit.m_hWnd) createEdit();
		edit.SetWindowText(text);
		if (!(flags & FLAG_EDIT_VISIBLE))
		{
			edit.ShowWindow(SW_SHOW);
			flags |= FLAG_EDIT_VISIBLE;
		}
	}
	else if (flags & FLAG_EDIT_VISIBLE)
	{
		flags &= ~FLAG_EDIT_VISIBLE;
		if (edit.m_hWnd) edit.ShowWindow(SW_HIDE);
	}
}

tstring SearchBoxCtrl::getText() const
{
	tstring s;
	if (flags & FLAG_EDIT_VISIBLE) WinUtil::getWindowText(edit.m_hWnd, s);
	return s;
}

void SearchBoxCtrl::setHint(const tstring& s)
{
	hintText = s;
}

void SearchBoxCtrl::setMargins(const MARGINS& margins)
{
	this->margins = margins;
}

void SearchBoxCtrl::draw(HDC hdc, const RECT& rc)
{
	RECT rc2 = rc;
	if (borderTrans && borderTrans->running)
	{
		borderTrans->draw(hdc);
	}
	else
	{
		drawBackground(hdc, rc);
		drawBorder(hdc, rc2, borderCurrentState);
	}

	rc2.left += margins.cxLeftWidth + border.cxLeftWidth;
	rc2.right -= margins.cxRightWidth + border.cxRightWidth;
	rc2.top += margins.cyTopHeight + border.cyTopHeight;
	rc2.bottom -= margins.cyBottomHeight + border.cyBottomHeight;

	if (hBitmap[0])
	{
		WinUtil::drawAlphaBitmap(hdc, hBitmap[0], rc2.left, (rc2.top + rc2.bottom - bitmapSize[0].cy) / 2, bitmapSize[0].cx, bitmapSize[0].cy);
		rc2.left += bitmapSize[0].cx + iconSpace;
	}
	if ((flags & FLAG_HAS_EDIT_TEXT) && hBitmap[1])
	{
		int x = rc2.right - bitmapSize[1].cx;
		WinUtil::drawAlphaBitmap(hdc, hBitmap[1], x, (rc2.top + rc2.bottom - bitmapSize[1].cy) / 2, bitmapSize[1].cx, bitmapSize[1].cy);
		rc2.right = x - iconSpace;
	}
	if (!(flags & FLAG_EDIT_VISIBLE) && !hintText.empty() && rc2.left < rc2.right)
	{
		HGDIOBJ prevFont = SelectObject(hdc, hFont ? hFont : (HFONT) GetStockObject(DEFAULT_GUI_FONT));
		int prevMode = SetBkMode(hdc, TRANSPARENT);
		COLORREF prevColor = SetTextColor(hdc, colors[CLR_HINT]);
		ExtTextOut(hdc, rc2.left, (rc2.top + rc2.bottom - textHeight) / 2, ETO_CLIPPED, &rc2, hintText.c_str(), hintText.length(), nullptr);
		SetTextColor(hdc, prevColor);
		SelectObject(hdc, prevFont);
		SetBkMode(hdc, prevMode);
	}
}

void SearchBoxCtrl::drawBackground(HDC hdc, const RECT& rc)
{
	if (flags & FLAG_CUSTOM_COLORS)
	{
		if (!hBrush[CLR_BACKGROUND])
			hBrush[CLR_BACKGROUND] = CreateSolidBrush(colors[CLR_BACKGROUND]);
		FillRect(hdc, &rc, hBrush[CLR_BACKGROUND]);
		return;
	}
	HBRUSH br = (HBRUSH) GetParent().SendMessage(WM_CTLCOLOREDIT, (WPARAM) hdc, (LPARAM) m_hWnd);
	if (!br) br = (HBRUSH) GetStockObject(WHITE_BRUSH);
	FillRect(hdc, &rc, br);
}

void SearchBoxCtrl::drawBorder(HDC hdc, RECT& rc, int stateId)
{
	if (borderType == BORDER_TYPE_FRAME)
	{
		int index;
		switch (stateId)
		{
			case ETS_SELECTED:
				index = CLR_SELECTED_BORDER;
				break;
			case ETS_HOT:
				index = CLR_HOT_BORDER;
				break;
			default:
				index = CLR_NORMAL_BORDER;
		}
		if (!hBrush[index])
			hBrush[index] = CreateSolidBrush(colors[index]);
		WinUtil::drawFrame(hdc, rc, 1, 1, hBrush[index]);
		return;
	}
	initTheme();
	if (hTheme)
		UX.pDrawThemeBackground(hTheme, hdc, EP_EDITBORDER_NOSCROLL, stateId, &rc, nullptr);
	else
		DrawEdge(hdc, &rc, EDGE_SUNKEN, BF_RECT);
}

void SearchBoxCtrl::cleanup()
{
	if (backingStore)
	{
		backingStore->release();
		backingStore = nullptr;
	}
	for (int i = 0; i < MAX_BRUSHES; i++)
		if (hBrush[i])
		{
			DeleteObject(hBrush[i]);
			hBrush[i] = nullptr;
		}
	for (int i = 0; i < _countof(hCursor); i++)
		if (hCursor[i])
		{
			DestroyCursor(hCursor[i]);
			hCursor[i] = nullptr;
		}
	delete borderTrans;
	borderTrans = nullptr;
	closeTheme();
}

void SearchBoxCtrl::initTheme()
{
	if (!hTheme) openTheme(m_hWnd, L"Edit");
}

void SearchBoxCtrl::createEdit()
{
	dcassert(!edit.m_hWnd);
	RECT rcClient, rc;
	GetClientRect(&rcClient);
	calcEditRect(rcClient, rc);
	edit.Create(m_hWnd, rc, nullptr, WS_CHILD | ES_LEFT | ES_AUTOHSCROLL);
	if (hFont) edit.SendMessage(WM_SETFONT, (WPARAM) hFont, 0);
	edit.SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
}

void SearchBoxCtrl::calcEditRect(const RECT& rcClient, RECT& rc)
{
	rc.left = rcClient.left + margins.cxLeftWidth + border.cxLeftWidth;
	rc.right = rcClient.right - (margins.cxRightWidth + border.cxRightWidth);
	rc.top = rcClient.top + margins.cyTopHeight + border.cyTopHeight;
	rc.bottom = rcClient.bottom - (margins.cyBottomHeight + border.cyBottomHeight);
	if (hBitmap[0]) rc.left += bitmapSize[0].cx + iconSpace;
	if (hBitmap[1]) rc.right -= bitmapSize[1].cx + iconSpace;
	if (textHeight < 0)
	{
		HDC hdc = GetDC();
		updateTextHeight(hdc);
		ReleaseDC(hdc);
	}
	int y = (rc.top + rc.bottom - textHeight) / 2;
	rc.top = y;
}

void SearchBoxCtrl::calcCloseButtonRect(const RECT& rcClient, RECT& rc) const
{
	rc.right = rcClient.right - (margins.cxRightWidth + border.cxRightWidth);
	rc.top = rcClient.top + margins.cyTopHeight + border.cyTopHeight;
	rc.bottom = rcClient.bottom - (margins.cyBottomHeight + border.cyBottomHeight);
	rc.left = rc.right - bitmapSize[1].cx;
	rc.top = (rc.top + rc.bottom - bitmapSize[1].cy) / 2;
}

void SearchBoxCtrl::setColor(int what, COLORREF color)
{
	if (what < 0 || what >= MAX_COLORS) return;
	colors[what] = color;
	if (what < _countof(hBrush) && hBrush[what])
	{
		DeleteObject(hBrush[what]);
		hBrush[what] = nullptr;
	}
}

void SearchBoxCtrl::setUseCustomColors(bool flag)
{
	if (flag)
		flags |= FLAG_CUSTOM_COLORS;
	else
		flags &= ~FLAG_CUSTOM_COLORS;
}

void SearchBoxCtrl::setBorderType(int type)
{
	if (type != BORDER_TYPE_FRAME && type != BORDER_TYPE_EDIT) return;
	borderType = type;
}

void SearchBoxCtrl::setAnimationEnabled(bool flag)
{
	if (flag)
		flags |= FLAG_ENABLE_ANIMATION;
	else
		flags &= ~FLAG_ENABLE_ANIMATION;
}

int SearchBoxCtrl::getTransitionDuration(int oldState, int newState) const
{
	if (animationDuration >= 0)
		return animationDuration;

	DWORD duration;
	if (hTheme && UX.pGetThemeTransitionDuration &&
	    SUCCEEDED(UX.pGetThemeTransitionDuration(hTheme, EP_EDITBORDER_NOSCROLL, oldState, newState, TMT_TRANSITIONDURATIONS, &duration)))
	{
#ifdef DEBUG_SEARCH_BOX
		ATLTRACE("Transition duration %d -> %d is %d\n", oldState, newState, duration);
#endif
		return duration;
	}
	return 0;
}

void SearchBoxCtrl::initStateTransitionBitmaps(HDC hdc, const RECT& rcClient)
{
	borderTrans->createBitmaps(hdc, rcClient);
	HDC bitmapDC = CreateCompatibleDC(hdc);
	for (int i = 0; i < 2; i++)
	{
		HGDIOBJ oldBitmap = SelectObject(bitmapDC, borderTrans->bitmaps[i]);
		drawBackground(bitmapDC, rcClient);
		RECT rc = rcClient;
		drawBorder(bitmapDC, rc, borderTrans->states[i]);
		GdiFlush();
		SelectObject(bitmapDC, oldBitmap);
	}
	DeleteDC(bitmapDC);
}

void SearchBoxCtrl::setBorderState(int newState)
{
#ifdef DEBUG_SEARCH_BOX
	ATLTRACE("setBorderState: %d\n", newState);
#endif
	if (!(borderTrans && borderTrans->running) && borderCurrentState == newState)
		return;
	if (!(flags & FLAG_ENABLE_ANIMATION) || !hTheme)
	{
		borderCurrentState = newState;
		return;
	}

	int64_t timestamp = getHighResTimestamp();
	if (borderTrans)
	{
		if (borderTrans->running)
		{
			int duration = getTransitionDuration(borderTrans->states[0], borderTrans->states[1]);
			borderTrans->setNewState(timestamp, newState, duration);
			return;
		}
	}
	else
		borderTrans = new StateTransition;

#ifdef DEBUG_SEARCH_BOX
	ATLTRACE("New transition: %d -> %d\n", borderCurrentState, newState);
#endif
	int duration = getTransitionDuration(borderCurrentState, newState);
	borderTrans->states[0] = borderCurrentState;
	borderTrans->states[1] = newState;

	RECT rc;
	GetClientRect(&rc);
	HDC hdc = GetDC();
	initStateTransitionBitmaps(hdc, rc);
	ReleaseDC(hdc);
	borderTrans->start(timestamp, duration);
	startTimer(TIMER_UPDATE_ANIMATION, FLAG_TIMER_ANIMATION, 10);
	stopTimer(TIMER_CLEANUP, FLAG_TIMER_CLEANUP);
}

void SearchBoxCtrl::updateAnimationState()
{
	if (!borderTrans) return;

	UpdateParams up;
	up.hdc = nullptr;
	up.timestamp = getHighResTimestamp();
	up.frequency = getHighResFrequency();
	up.update = up.running = false;
	if (borderTrans->update(up.timestamp, up.frequency))
	{
		if (!borderTrans->running)
		{
			borderCurrentState = borderTrans->getCompletedState();
#ifdef DEBUG_SEARCH_BOX
			ATLTRACE("Transition %p: current state is now %d\n", borderTrans, borderCurrentState);
#endif
		}
		up.update = true;
	}
	if (!borderTrans->running && borderTrans->nextTransition())
	{
		GetClientRect(&up.rc);
		up.hdc = GetDC();
		initStateTransitionBitmaps(up.hdc, up.rc);
		borderTrans->start(up.timestamp, getTransitionDuration(borderTrans->states[0], borderTrans->states[1]));
	}
	if (borderTrans->running) up.running = true;
	if (up.hdc) ReleaseDC(up.hdc);
	if (up.update) Invalidate();
	if (!up.running)
	{
		stopTimer(TIMER_UPDATE_ANIMATION, FLAG_TIMER_ANIMATION);
		startTimer(TIMER_CLEANUP, FLAG_TIMER_CLEANUP, CLEANUP_TIME);
	}
}

void SearchBoxCtrl::cleanupAnimationState()
{
	if (!borderTrans) return;
	int64_t delay = 15 * getHighResFrequency();
	if (!borderTrans->running && getHighResTimestamp() - borderTrans->startTime > delay)
	{
		delete borderTrans;
		borderTrans = nullptr;
	}
}

void SearchBoxCtrl::startTimer(int id, int flag, int time)
{
	if (!(flags & flag))
	{
		SetTimer(id, time, nullptr);
		flags |= flag;
	}
}

void SearchBoxCtrl::stopTimer(int id, int flag)
{
	if (flags & flag)
	{
		KillTimer(id);
		flags ^= flag;
	}
}

void SearchBoxCtrl::updateCheckMouseTimer()
{
	if ((flags & (FLAG_HOT | FLAG_FOCUSED)) == FLAG_HOT)
		startTimer(TIMER_CHECK_MOUSE, FLAG_TIMER_CHECK_MOUSE, 100);
	else
		stopTimer(TIMER_CHECK_MOUSE, FLAG_TIMER_CHECK_MOUSE);
}

void SearchBoxCtrl::returnPressed()
{
	if (notifMask & NOTIF_RETURN)
		GetParent().SendMessage(WMU_RETURN);
}

void SearchBoxCtrl::tabPressed()
{
	if ((notifMask & NOTIF_TAB) && !(GetKeyState(VK_CONTROL) & 0x8000))
		GetParent().SendMessage(WM_NEXTDLGCTL, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
}

void SearchBoxCtrl::escapePressed()
{
	if (notifMask & NOTIF_ESCAPE)
		GetParent().PostMessage(WM_CLOSE);
}
