#include "stdafx.h"
#include "TextHostImpl.h"
#include "../client/Text.h"
#include "Fonts.h"
#include <assert.h>

const IID IID_ITextServices = { 0x8D33F740, 0xCF58, 0x11CE, { 0xA8, 0x9D, 0x00, 0xAA, 0x00, 0x6C, 0xAD, 0xC5 }};
const IID IID_ITextHost = { 0xC5BDD8D0, 0xD26E, 0x11CE, { 0xA8, 0x9E, 0x00, 0xAA, 0x00, 0x6C, 0xAD, 0xC5 }};
const IID IID_ITextHost2 = { 0x13E670F5, 0x1A5A, 0x11CF, { 0xAB, 0xEB, 0x00, 0xAA, 0x00, 0xB6, 0x5E, 0xA1 }};
const IID IID_ITextDocument = { 0x8CC497C0, 0xA1DF, 0x11CE, { 0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D }};

#ifndef HIMETRIC_PER_INCH
#define HIMETRIC_PER_INCH 2540
#endif

static const unsigned DEFAULT_STYLE = 0;

enum
{
	SB_FLAG_HORZ = 1,
	SB_FLAG_VERT = 2
};

enum
{
	FLAG_RICH           = 0x001,
	FLAG_TRANSPARENT    = 0x002,
	FLAG_AUTO_WORD_SEL  = 0x004,
	FLAG_WORD_WRAP      = 0x008,
	FLAG_ALLOW_BEEP     = 0x010,
	FLAG_SAVE_SELECTION = 0x020,
	FLAG_CAPTURED       = 0x040,
	FLAG_SHOW_CARET     = 0x080,
	FLAG_LARGE_SB_RANGE = 0x100
};

TextHostImpl::TextHostImpl()
{
	refs = 1;
	hWnd = nullptr;
	rcWindow = rcClient = {};
	textServices = nullptr;
	sbFlags = 0;
	flags = FLAG_RICH | FLAG_TRANSPARENT;
	sbWidth = GetSystemMetrics(SM_CXVSCROLL);
	sbHeight = GetSystemMetrics(SM_CYHSCROLL);
	winStyle = DEFAULT_STYLE;
	textMargins = { 1, 1, 1, 1 };
	extent = {};
#ifdef _UNICODE
	passwordChar = 0x2022;
#else
	passwordChar = '*';
#endif
	selBarWidth = 0;
	caretSize = {};
	maxTextLength = 32767;
	LOGFONT lf;
	GetObject(Fonts::g_systemFont, sizeof(lf), &lf);
	initCharFormat(&charFormat, lf);
	initParaFormat(&paraFormat);
}

TextHostImpl::~TextHostImpl()
{
	if (textServices)
	{
		textServices->OnTxInPlaceDeactivate();
		textServices->Release();
	}
}

bool TextHostImpl::init(HWND hWnd, CREATESTRUCT& cs)
{
	this->hWnd = hWnd;
	if (textServices) // already initialized
		return true;

	winStyle = cs.style;
	cs.style &= ~(WS_HSCROLL | WS_VSCROLL | WS_BORDER);

	if (!(winStyle & (ES_AUTOHSCROLL | WS_HSCROLL)))
		flags |= FLAG_WORD_WRAP;

	IUnknown* pUnk = nullptr;
	HRESULT hr = E_FAIL;
	HMODULE hModule = LoadLibrary(_T("msftedit.dll"));
	if (hModule)
	{
		PCreateTextServices pCreateTextServices = (PCreateTextServices) GetProcAddress(hModule, "CreateTextServices");
		if (pCreateTextServices)
			hr = pCreateTextServices(nullptr, this, &pUnk);
	}

	if (FAILED(hr)) goto err;

	hr = pUnk->QueryInterface(IID_ITextServices, (void**) &textServices);
	pUnk->Release();
	pUnk = nullptr;

	if (FAILED(hr)) goto err;

	if (cs.lpszName)
	{
#ifdef _UNICODE
		textServices->TxSetText(cs.lpszName);
#else
		wstring wt = Text::acpToWide(cs.lpszName);
		textServices->TxSetText(wt.c_str());
#endif
	}

	textServices->OnTxInPlaceActivate(nullptr);
	return true;

err:
	if (pUnk) pUnk->Release();
	if (textServices)
	{
		textServices->Release();
		textServices = nullptr;
	}
	return false;
}

void TextHostImpl::initCharFormat(CHARFORMAT2* cf, const LOGFONT& lf)
{
	HDC hdc = CreateIC(_T("DISPLAY"), nullptr, nullptr, nullptr);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	DeleteDC(hdc);

	memset(cf, 0, sizeof(*cf));
	cf->cbSize = sizeof(CHARFORMAT2W);
	cf->dwMask = CFM_SIZE | CFM_OFFSET | CFM_FACE | CFM_CHARSET | CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
	cf->crTextColor = RGB(0, 0, 0);
	cf->yOffset = 0;
	cf->yHeight = abs(lf.lfHeight) * 1440 / dpi;
	cf->bCharSet = lf.lfCharSet;
	cf->dwEffects = 0;
	if (lf.lfWeight >= FW_BOLD)
		cf->dwEffects |= CFE_BOLD;
	if (lf.lfItalic)
		cf->dwEffects |= CFE_ITALIC;
	if (lf.lfUnderline)
		cf->dwEffects |= CFE_UNDERLINE;
	cf->bPitchAndFamily = lf.lfPitchAndFamily;
	_tcscpy(cf->szFaceName, lf.lfFaceName);
}

void TextHostImpl::initParaFormat(PARAFORMAT2* pf)
{
	memset(pf, 0, sizeof(*pf));
	pf->cbSize = sizeof(PARAFORMAT2);
	pf->dwMask = PFM_ALL;
	pf->wAlignment = PFA_LEFT;
	pf->cTabCount = 1;
	pf->rgxTabs[0] = lDefaultTab;
}

void TextHostImpl::updateVScroll()
{
	assert(ctrlVScroll.m_hWnd);
	LONG minVal, maxVal, pos, page;
	textServices->TxGetVScroll(&minVal, &maxVal, &pos, &page, nullptr);
	//ATLTRACE("updateVScroll: minVal=%d, maxVal=%d, pos=%d\n", minVal, maxVal, pos);
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = minVal;
	si.nMax = maxVal;
	si.nPage = page;
	si.nPos = si.nTrackPos = pos;
	ctrlVScroll.SetScrollInfo(&si);
}

void TextHostImpl::updateHScroll()
{
	assert(ctrlHScroll.m_hWnd);
	LONG minVal, maxVal, pos, page;
	textServices->TxGetHScroll(&minVal, &maxVal, &pos, &page, nullptr);
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = minVal;
	si.nMax = maxVal;
	si.nPage = page;
	si.nPos = si.nTrackPos = pos;
	ctrlHScroll.SetScrollInfo(&si);
}

HRESULT TextHostImpl::QueryInterface(REFIID riid, void **ppvObject)
{
	if (riid == IID_IUnknown || riid == IID_ITextHost || riid == IID_ITextHost2)
	{
		++refs;
		*ppvObject = this;
		return S_OK;
	}
	*ppvObject = nullptr;
	return E_NOINTERFACE;
}

ULONG TextHostImpl::AddRef()
{
	assert(refs > 0);
	return ++refs;
}

ULONG TextHostImpl::Release()
{
	assert(refs > 0);
	ULONG result = --refs;
	if (!result) delete this;
	return result;
}

// ITextHost

HDC TextHostImpl::TxGetDC()
{
	return GetDC(hWnd);
}

int TextHostImpl::TxReleaseDC(HDC hdc)
{
	return ReleaseDC(hWnd, hdc);
}

BOOL TextHostImpl::TxShowScrollBar(int fnBar, BOOL fShow)
{
	//ATLTRACE("TxShowScrollBar: fnBar=%d, fShow=%d\n", fnBar, (int) fShow);
	int oldFlags = sbFlags;
	if (fnBar == SB_VERT || fnBar == SB_BOTH)
	{
		if (fShow)
			sbFlags |= SB_FLAG_VERT;
		else
			sbFlags &= ~SB_FLAG_VERT;
		if (ctrlVScroll.m_hWnd)
			ctrlVScroll.ShowWindow(fShow ? SW_SHOW : SW_HIDE);
		else if (fShow)
			ctrlVScroll.Create(hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | SB_VERT);
	}
	if (fnBar == SB_HORZ || fnBar == SB_BOTH)
	{
		if (fShow)
			sbFlags |= SB_FLAG_HORZ;
		else
			sbFlags &= ~SB_FLAG_HORZ;
		if (ctrlHScroll.m_hWnd)
			ctrlHScroll.ShowWindow(fShow ? SW_SHOW : SW_HIDE);
		else if (fShow)
			ctrlHScroll.Create(hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | SB_HORZ);
	}
	if (sbFlags != oldFlags)
		setWindowRect(rcWindow);
	return TRUE;
}

BOOL TextHostImpl::TxEnableScrollBar(int fuSBFlags, int fuArrowflags)
{
	//ATLTRACE("TxEnableScrollBar: fuSBFlags=%d, fuArrowflags=%d\n", fuSBFlags, fuArrowflags);
	int oldFlags = sbFlags;
	if (fuSBFlags == SB_VERT || fuSBFlags == SB_BOTH)
	{
		if (fuArrowflags == ESB_DISABLE_BOTH)
			sbFlags &= ~SB_FLAG_VERT;
		else
			sbFlags |= SB_FLAG_VERT;
		if ((sbFlags & SB_FLAG_VERT) && !ctrlVScroll.m_hWnd)
			ctrlVScroll.Create(hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | SB_VERT);
	}
	if (fuSBFlags == SB_HORZ || fuSBFlags == SB_BOTH)
	{
		if (fuArrowflags == ESB_DISABLE_BOTH)
			sbFlags &= ~SB_FLAG_HORZ;
		else
			sbFlags |= SB_FLAG_HORZ;
		if ((sbFlags & SB_FLAG_HORZ) && !ctrlHScroll.m_hWnd)
			ctrlHScroll.Create(hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | SB_HORZ);
	}
	if (sbFlags != oldFlags)
		setWindowRect(rcWindow);
	return TRUE;
}

BOOL TextHostImpl::TxSetScrollRange(int fnBar, LONG nMinPos, int nMaxPos, BOOL fRedraw)
{
	//ATLTRACE("TxSetScrollRange: min=%d, max=%d\n", nMinPos, nMaxPos);
	if (fnBar == SB_VERT)
	{
		if (nMaxPos > 0xFFFF)
			flags |= FLAG_LARGE_SB_RANGE;
		else
			flags &= ~FLAG_LARGE_SB_RANGE;
		if (ctrlVScroll.m_hWnd) updateVScroll();
	}
	else if (fnBar == SB_HORZ)
	{
		if (ctrlHScroll.m_hWnd) updateHScroll();
	}
	return TRUE;
}

BOOL TextHostImpl::TxSetScrollPos(int fnBar, int nPos, BOOL fRedraw)
{
	//ATLTRACE("TxSetScrollPos: nPos=%d\n", nPos);
	if (fnBar == SB_VERT)
	{
		if (ctrlVScroll.m_hWnd) updateVScroll();
	}
	else if (fnBar == SB_HORZ)
	{
		if (ctrlHScroll.m_hWnd) updateHScroll();
	}
	return TRUE;
}

void TextHostImpl::TxInvalidateRect(LPCRECT prc, BOOL fMode)
{
	InvalidateRect(hWnd, prc, fMode);
}

void TextHostImpl::TxViewChange(BOOL fUpdate)
{
	if (fUpdate) InvalidateRect(hWnd, nullptr, TRUE);
}

BOOL TextHostImpl::TxCreateCaret(HBITMAP hbmp, int xWidth, int yHeight)
{
	//ATLTRACE("CreateCaret: w=%d, h=%d\n", xWidth, yHeight);
	caretSize.cx = xWidth;
	caretSize.cy = yHeight;
	return CreateCaret(hWnd, hbmp, xWidth, yHeight);
}

BOOL TextHostImpl::TxShowCaret(BOOL fShow)
{
	BOOL result;
	if (fShow)
	{
		result = ShowCaret(hWnd);
		flags |= FLAG_SHOW_CARET;
	}
	else
	{
		result = HideCaret(hWnd);
		flags &= ~FLAG_SHOW_CARET;
	}
	//ATLTRACE("ShowCaret: %d, result = %d\n", (int) fShow, (int) result);
	return result;
}

BOOL TextHostImpl::TxSetCaretPos(int x, int y)
{
	//ATLTRACE("SetCaretPos: %d, %d\n", x, y);
	POINT ptCaret = {};
	GetCaretPos(&ptCaret);
	RECT rcCaret;
	rcCaret.left = ptCaret.x;
	rcCaret.top = ptCaret.y;
	rcCaret.right = rcCaret.left + caretSize.cx;
	rcCaret.bottom = rcCaret.top + caretSize.cy;
	InvalidateRect(hWnd, &rcCaret, FALSE);

	rcCaret.left = x;
	rcCaret.top = y;
	rcCaret.right = rcCaret.left + caretSize.cx;
	rcCaret.bottom = rcCaret.top + caretSize.cy;
	InvalidateRect(hWnd, &rcCaret, FALSE);

	return SetCaretPos(x, y);
}

BOOL TextHostImpl::TxSetTimer(UINT idTimer, UINT uTimeout)
{
	//ATLTRACE("TxSetTimer: id=%d\n", idTimer);
	return SetTimer(hWnd, idTimer, uTimeout, nullptr) != 0;
}

void TextHostImpl::TxKillTimer(UINT idTimer)
{
	//ATLTRACE("TxKillTimer: id=%d\n", idTimer);
	KillTimer(hWnd, idTimer);
}

void TextHostImpl::TxScrollWindowEx(int dx, int dy, LPCRECT lprcScroll,	LPCRECT lprcClip, HRGN hrgnUpdate, LPRECT lprcUpdate, UINT fuScroll)
{
	// Called on scroll when FLAG_TRANSPARENT is not set
	//ATLTRACE("TxScrollWindowEx: dx=%d, dy=%d\n", dx, dy);
}

void TextHostImpl::TxSetCapture(BOOL fCapture)
{
	//ATLTRACE("TxSetCapture: %d\n", (int) fCapture);
	if (fCapture)
	{
		SetCapture(hWnd);
		flags |= FLAG_CAPTURED;
	}
	else
	{
		ReleaseCapture();
		flags &= ~FLAG_CAPTURED;
	}
}

void TextHostImpl::TxSetFocus()
{
	//ATLTRACE("TxSetFocus\n");
	SetFocus(hWnd);
}

void TextHostImpl::TxSetCursor(HCURSOR hcur, BOOL fText)
{
	//ATLTRACE("TxSetCursor: %p\n", hcur);
	SetCursor(hcur);
}

BOOL TextHostImpl::TxScreenToClient(LPPOINT lppt)
{
	return ScreenToClient(hWnd, lppt);
}

BOOL TextHostImpl::TxClientToScreen(LPPOINT lppt)
{
	return ClientToScreen(hWnd, lppt);
}

HRESULT TextHostImpl::TxActivate(LONG *plOldState)
{
	return S_OK;
}

HRESULT TextHostImpl::TxDeactivate(LONG lNewState)
{
	return S_OK;
}

HRESULT TextHostImpl::TxGetClientRect(LPRECT prc)
{
	*prc = rcClient;
	return S_OK;
}

HRESULT TextHostImpl::TxGetViewInset(LPRECT prc)
{
	prc->left = prc->right = prc->top = prc->bottom = 0;
	return S_OK;
}

HRESULT TextHostImpl::TxGetCharFormat(const CHARFORMATW **ppCF)
{
	*ppCF = &charFormat;
	return S_OK;
}

HRESULT TextHostImpl::TxGetParaFormat(const PARAFORMAT **ppPF)
{
	*ppPF = &paraFormat;
	return S_OK;
}

COLORREF TextHostImpl::TxGetSysColor(int nIndex)
{
	return ::GetSysColor(nIndex);
}

HRESULT TextHostImpl::TxGetBackStyle(TXTBACKSTYLE *pstyle)
{
	*pstyle = (flags & FLAG_TRANSPARENT) ? TXTBACK_TRANSPARENT : TXTBACK_OPAQUE;
	return S_OK;
}

HRESULT TextHostImpl::TxGetMaxLength(DWORD *pLength)
{
	*pLength = maxTextLength;
	return S_OK;
}

HRESULT TextHostImpl::TxGetScrollBars(DWORD *pdwScrollBar)
{
	*pdwScrollBar = winStyle & (WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_DISABLENOSCROLL);
	return S_OK;
}

HRESULT TextHostImpl::TxGetPasswordChar(TCHAR *pch)
{
	*pch = passwordChar;
	return S_OK;
}

HRESULT TextHostImpl::TxGetAcceleratorPos(LONG *pcp)
{
	*pcp = -1;
	return S_OK;
}

HRESULT TextHostImpl::OnTxCharFormatChange(const CHARFORMATW *pcf)
{
	return S_OK;
}

HRESULT TextHostImpl::OnTxParaFormatChange(const PARAFORMAT *ppf)
{
	return S_OK;
}

HRESULT TextHostImpl::TxGetPropertyBits(DWORD dwMask, DWORD *pdwBits)
{
	DWORD prop = 0;
	if (flags & FLAG_RICH)
		prop = TXTBIT_RICHTEXT;
	if (winStyle & ES_MULTILINE)
		prop |= TXTBIT_MULTILINE;
	if (winStyle & ES_READONLY)
		prop |= TXTBIT_READONLY;
	if (winStyle & ES_PASSWORD)
		prop |= TXTBIT_USEPASSWORD;
	if (!(winStyle & ES_NOHIDESEL))
		prop |= TXTBIT_HIDESELECTION;
	if (flags & FLAG_AUTO_WORD_SEL)
		prop |= TXTBIT_AUTOWORDSEL;
	if (flags & FLAG_WORD_WRAP)
		prop |= TXTBIT_WORDWRAP;
	if (flags & FLAG_ALLOW_BEEP)
		prop |= TXTBIT_ALLOWBEEP;
	if (flags & FLAG_SAVE_SELECTION)
		prop |= TXTBIT_SAVESELECTION;
	*pdwBits = prop & dwMask;
	return S_OK;
}

HRESULT TextHostImpl::TxNotify(DWORD iNotify, void *pv)
{
	// TODO: Possibly add other notification codes
	// such as EN_CHANGE, EN_SELCHANGE, etc
	//ATLTRACE("TxNotify: 0x%X\n", iNotify);
	if (iNotify == EN_LINK && pv)
	{
		int id = GetWindowLong(hWnd, GWL_ID);
		NMHDR* hdr = static_cast<NMHDR*>(pv);
		hdr->hwndFrom = hWnd;
		hdr->idFrom = id;
		hdr->code = iNotify;
		SendMessage(hWnd, WM_NOTIFY, (WPARAM) id, (LPARAM) pv);
	}
	return S_OK;
}

HIMC TextHostImpl::TxImmGetContext()
{
	return nullptr;
}

void TextHostImpl::TxImmReleaseContext(HIMC himc)
{
}

HRESULT TextHostImpl::TxGetExtent(LPSIZEL lpExtent)
{
	//ATLTRACE("TxGetExtent: w=%d, h=%d\n", extent.cx, extent.cy);
	*lpExtent = extent;
	return S_OK;
}

HRESULT	TextHostImpl::TxGetSelectionBarWidth(LONG *plSelBarWidth)
{
	*plSelBarWidth = selBarWidth;
	return S_OK;
}

// ITextHost2

BOOL TextHostImpl::TxIsDoubleClickPending()
{
	return FALSE;
}

HRESULT TextHostImpl::TxGetWindow(HWND *phwnd)
{
	*phwnd = hWnd;
	return S_OK;
}

HRESULT TextHostImpl::TxSetForegroundWindow()
{
	return S_OK;
}

HPALETTE TextHostImpl::TxGetPalette()
{
	return nullptr;
}

HRESULT TextHostImpl::TxGetEastAsianFlags(LONG *pFlags)
{
	// TODO
	if (pFlags) *pFlags = 0;
	return S_OK;
}

HCURSOR TextHostImpl::TxSetCursor2(HCURSOR hcur, BOOL bText)
{
	return SetCursor(hcur);
}

void TextHostImpl::TxFreeTextServicesNotification()
{
}

HRESULT TextHostImpl::TxGetEditStyle(DWORD dwItem, DWORD *pdwData)
{
	// TODO
	if (pdwData) *pdwData = 0;
	return S_OK;
}

HRESULT TextHostImpl::TxGetWindowStyles(DWORD *pdwStyle, DWORD *pdwExStyle)
{
	if (pdwStyle) *pdwStyle = winStyle;
	if (pdwExStyle) *pdwExStyle = 0;
	return S_OK;
}

HRESULT TextHostImpl::TxShowDropCaret(BOOL fShow, HDC hdc, LPCRECT prc)
{
	return E_NOTIMPL;
}

HRESULT TextHostImpl::TxDestroyCaret()
{
	return E_NOTIMPL;
}

HRESULT TextHostImpl::TxGetHorzExtent(LONG *plHorzExtent)
{
	return E_NOTIMPL;
}

void TextHostImpl::setWindowRect(const RECT& rc)
{
	rcWindow = rc;
	rcClient = rc;
	if (sbFlags & SB_FLAG_VERT) rcClient.right -= sbWidth;
	if (sbFlags & SB_FLAG_HORZ) rcClient.bottom -= sbHeight;
	rcClient.left += textMargins.cxLeftWidth;
	rcClient.top += textMargins.cyTopHeight;
	rcClient.right -= textMargins.cxRightWidth;
	rcClient.bottom -= textMargins.cyBottomHeight;
	if (rcClient.right < rcClient.left) rcClient.right = rcClient.left;
	if (rcClient.bottom < rcClient.top) rcClient.bottom = rcClient.top;

	HDC hdc = GetDC(hWnd);
	LONG xdpi = GetDeviceCaps(hdc, LOGPIXELSX);
	LONG ydpi = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(hWnd, hdc);
	if (xdpi <= 0) xdpi = 96;
	if (ydpi <= 0) ydpi = 96;
	extent.cx = MulDiv(rcClient.right - rcClient.left, HIMETRIC_PER_INCH, xdpi);
	extent.cy = MulDiv(rcClient.bottom - rcClient.top, HIMETRIC_PER_INCH, ydpi);
	if (textServices)
		textServices->OnTxPropertyBitsChange(TXTBIT_VIEWINSETCHANGE, TXTBIT_VIEWINSETCHANGE);

	RECT rcBar;
	if (sbFlags & SB_FLAG_VERT)
	{
		rcBar.right = rcWindow.right;
		rcBar.left = rcBar.right - sbWidth;
		rcBar.top = rcWindow.top;
		rcBar.bottom = rcWindow.bottom;
		ctrlVScroll.SetWindowPos(nullptr, &rcBar, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSENDCHANGING);
	}
	if (sbFlags & SB_FLAG_HORZ)
	{
		rcBar.bottom = rcWindow.bottom;
		rcBar.top = rcBar.bottom - sbHeight;
		rcBar.left = rcWindow.left;
		rcBar.right = rcWindow.right;
		ctrlHScroll.SetWindowPos(nullptr, &rcBar, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSENDCHANGING);
	}
}

void TextHostImpl::setTextMargins(const MARGINS& m)
{
	if (textMargins.cxLeftWidth != m.cxLeftWidth ||
	    textMargins.cyTopHeight != m.cyTopHeight ||
	    textMargins.cxRightWidth != m.cxRightWidth ||
	    textMargins.cyBottomHeight != m.cyBottomHeight)
	{
		textMargins = m;
		setWindowRect(rcWindow);
	}
}

void TextHostImpl::setExtent(SIZEL value)
{
	extent = value;
	if (textServices)
		textServices->OnTxPropertyBitsChange(TXTBIT_EXTENTCHANGE, TXTBIT_EXTENTCHANGE);
}

void TextHostImpl::draw(HDC hDC, RECT& rcPaint)
{
	if (textServices)
	{
		RECT rc = rcClient;
		textServices->TxDraw(
			DVASPECT_CONTENT, // Draw Aspect
			-1,               // Lindex
			nullptr,          // Info for drawing optimazation
			nullptr,          // target device information
			hDC,              // Draw device HDC
			nullptr,          // Target device HDC
			(RECTL*) &rc,     // Bounding client rectangle
			nullptr,          // Clipping rectangle for metafiles
			&rcPaint,         // Update rectangle
			nullptr,          // Call back function
			0,                // Call back parameter
			TXTVIEW_ACTIVE);  // What view of the object
	}
}

bool TextHostImpl::setCursor(POINT pt)
{
	if (!textServices || !PtInRect(&rcClient, pt)) return false;
	textServices->OnTxSetCursor(DVASPECT_CONTENT, -1, nullptr, nullptr, nullptr, nullptr, nullptr, pt.x, pt.y);
	return true;
}

HRESULT TextHostImpl::sendMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *pRes)
{
	if (!textServices) return S_FALSE;
	return textServices->TxSendMessage(msg, wParam, lParam, pRes);
}

ITextDocument* TextHostImpl::queryTextDocument()
{
	if (!textServices) return nullptr;
	ITextDocument* textDoc = nullptr;
	textServices->QueryInterface(IID_ITextDocument, (void **) &textDoc);
	return textDoc;
}

IRichEditOle* TextHostImpl::queryRichEditOle()
{
	if (!textServices) return nullptr;
	IRichEditOle* richEdit = nullptr;
	textServices->QueryInterface(IID_IRichEditOle, (void **) &richEdit);
	return richEdit;
}

bool TextHostImpl::isLargeSBRange() const
{
	return (flags & FLAG_LARGE_SB_RANGE) != 0;
}
