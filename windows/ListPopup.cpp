#include "stdafx.h"
#include "ListPopup.h"
#include "BackingStore.h"
#include "GdiUtil.h"
#include "UxThemeLib.h"
#include <vssym32.h>
#include <algorithm>

#undef min
#undef max

#define UX UxThemeLib::instance

#if !(defined(NTDDI_VERSION) && NTDDI_VERSION >= 0x0A00000C)
#define MENU_POPUPITEM_FOCUSABLE 27
#endif

static inline int totalWidth(const MARGINS& m)
{
	return m.cxLeftWidth + m.cxRightWidth;
}

static inline int totalHeight(const MARGINS& m)
{
	return m.cyTopHeight + m.cyBottomHeight;
}

ListPopup::ListPopup()
{
	maxVisibleItems = 18;
	maxTextWidthUnscaled = maxTextWidth = 220;

	data = -1;
	flags = FLAG_SHOW_CHECKS;
	itemHeight = 0;
	iconSize = 0;

	hotIndex = -1;
	topIndex = 0;
	pressedIndex = -1;
	maxIconWidth = maxIconHeight = 0;

	hFont = hFontBold = nullptr;
	hWndScroll = nullptr;
	hWndNotif = nullptr;

	for (int i = 0; i < _countof(checkBmp); i++)
		checkBmp[i] = nullptr;

	initFallbackMetrics();
	backingStore = nullptr;
}

ListPopup::~ListPopup()
{
	clearFonts();
	clearBitmaps();
	if (backingStore) backingStore->release();
}

void ListPopup::addItem(const tstring& text, HBITMAP icon, uintptr_t data, int flags)
{
	items.push_back(Item{text, icon, data, -1, -1, flags});
	if (icon) flags |= FLAG_UPDATE_ICON_SIZE;
	if (m_hWnd) updateScrollBar();
}

void ListPopup::clearItems()
{
	items.clear();
	flags |= FLAG_UPDATE_ICON_SIZE;
	if (m_hWnd) updateScrollBar();
}

void ListPopup::updateIconSize()
{
	maxIconWidth = maxIconHeight = 0;
	BITMAP bitmap;
	for (Item& item : items)
		if (item.iconWidth < 0)
		{
			item.iconWidth = item.iconHeight = 0;
			if (item.icon && GetObject(item.icon, sizeof(bitmap), &bitmap))
			{
				item.iconWidth = bitmap.bmWidth;
				item.iconHeight = bitmap.bmHeight;
			}
			if (item.iconWidth > maxIconWidth) maxIconWidth = item.iconWidth;
			if (item.iconHeight > maxIconHeight) maxIconHeight = item.iconHeight;
		}
	flags &= ~FLAG_UPDATE_ICON_SIZE;
}

void ListPopup::updateItemHeight(HDC hdc)
{
	itemHeight = 0;
	int textHeight = 0;
	HGDIOBJ oldFont = SelectObject(hdc, hFont);
	TEXTMETRIC tm = {};
	if (GetTextMetrics(hdc, &tm)) textHeight = tm.tmHeight;
	SelectObject(hdc, oldFont);

	if (textHeight) itemHeight = textHeight + totalHeight(marginText);

	if (maxIconHeight)
	{
		int bitmapHeight = maxIconHeight + totalHeight(marginBitmap);
		if (bitmapHeight > itemHeight) itemHeight = bitmapHeight;
	}
	if (flags & FLAG_SHOW_CHECKS)
	{
		int checkHeight = sizeCheck.cy + totalHeight(marginCheckBackground) + totalHeight(marginCheck);
		if (checkHeight > itemHeight) itemHeight = checkHeight;
	}
}

SIZE ListPopup::getPrefSize(HDC hdc)
{
	if (flags & FLAG_UPDATE_ICON_SIZE) updateIconSize();
	updateItemHeight(hdc);

	SIZE size = {};
	int count = (int) items.size() - topIndex;
	if (count > maxVisibleItems) count = maxVisibleItems;
	if (count <= 0) return size;
	size.cy = count * itemHeight;

	int textWidth = 0;
	HGDIOBJ oldFont = nullptr;
	HFONT currentFont = nullptr;
	for (const auto& item : items)
	{
		if (textWidth < maxTextWidth)
		{
			SIZE textSize;
			HFONT hFontItem = (item.flags & IF_DEFAULT) ? hFontBold : hFont;
			if (hFontItem != currentFont)
			{
				HGDIOBJ result = SelectObject(hdc, hFontItem);
				currentFont = hFontItem;
				if (!oldFont) oldFont = result;
			}
			GetTextExtentPoint(hdc, item.text.c_str(), (int) item.text.length(), &textSize);
			if (textSize.cx > textWidth)
			{
				textWidth = textSize.cx;
				if (textWidth > maxTextWidth) textWidth = maxTextWidth;
			}
		}
	}
	if (oldFont) SelectObject(hdc, oldFont);

	size.cx = textWidth + totalWidth(marginText) + totalWidth(marginItem);
	if ((flags & FLAG_SHOW_CHECKS) || maxIconWidth)
	{
		if (hTheme)
		{
			int bitmapWidth = sizeCheck.cx + totalWidth(marginCheck) + marginCheckBackground.cxLeftWidth;
			if (maxIconWidth > bitmapWidth) bitmapWidth = maxIconWidth;
			size.cx += bitmapWidth + marginCheckBackground.cxRightWidth;
		}
		else
		{
			int gapSize = sizeCheck.cx + totalWidth(marginCheck);
			if (maxIconWidth)
			{
				int bitmapWidth = maxIconWidth + totalWidth(marginBitmap);
				if (bitmapWidth > gapSize) gapSize = bitmapWidth;
			}
			size.cx += gapSize;
		}
	}
	if (flags & FLAG_HAS_SCROLLBAR) size.cx += scrollBarSize;
	size.cx += 2*cxBorder;
	size.cy += 2*cyBorder;

	return size;
}

SIZE ListPopup::getPrefSize()
{
	SIZE size = {};
	HDC hdc = GetDC();
	if (hdc)
	{
		size = getPrefSize(hdc);
		ReleaseDC(hdc);
	}
	return size;
}

void ListPopup::clearFonts()
{
	if (hFont)
	{
		DeleteObject(hFont);
		hFont = nullptr;
	}
	if (hFontBold)
	{
		DeleteObject(hFontBold);
		hFontBold = nullptr;
	}
}

void ListPopup::updateFonts()
{
	clearFonts();
	LOGFONT lf;
	HRESULT hr = E_FAIL;
	if (hTheme) hr = UX.pGetThemeSysFont(hTheme, TMT_MENUFONT, &lf);
	if (FAILED(hr))
	{
		NONCLIENTMETRICS ncm = { offsetof(NONCLIENTMETRICS, lfMessageFont) + sizeof(NONCLIENTMETRICS::lfMessageFont) };
		if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) return;
		memcpy(&lf, &ncm.lfMenuFont, sizeof(lf));
	}
	hFont = CreateFontIndirect(&lf);
	lf.lfWeight = FW_SEMIBOLD;
	hFontBold = CreateFontIndirect(&lf);
}

void ListPopup::updateScrollBar()
{
	topIndex = 0;
	int numItems = (int) items.size();
	int maxScroll = numItems - maxVisibleItems;
	if (maxScroll > 0)
	{
		flags |= FLAG_HAS_SCROLLBAR;
		if (!hWndScroll)
		{
			hWndScroll = ::CreateWindow(WC_SCROLLBAR, nullptr, WS_CHILD | WS_CLIPSIBLINGS | SBS_VERT, 0, 0, 0, 0, m_hWnd, nullptr, nullptr, nullptr);
			assert(hWndScroll);
		}
		SCROLLINFO si;
		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		si.nMin = 0;
		si.nMax = numItems - 1;
		si.nPage = maxVisibleItems;
		si.nPos = 0;
		si.nTrackPos = 0;
		::SetScrollInfo(hWndScroll, SB_CTL, &si, TRUE);
	}
	else
	{
		flags &= ~FLAG_HAS_SCROLLBAR;
		if (hWndScroll) ::ShowWindow(hWndScroll, SW_HIDE);
	}
}

void ListPopup::updateScrollBarPos(int width, int height)
{
	if (flags & FLAG_HAS_SCROLLBAR)
		::SetWindowPos(hWndScroll, nullptr,
			width - scrollBarSize - cxBorder, cyBorder,
			scrollBarSize, height - 2*cyBorder,
			SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void ListPopup::changeTheme()
{
	if (!m_hWnd) return;
	closeTheme();
	clearBitmaps();
	initTheme();
	updateFonts();
}

void ListPopup::initFallbackMetrics()
{
	partId = 0;
	maxTextWidth = maxTextWidthUnscaled * WinUtil::getDisplayDpi() / 96;
	scrollBarSize = GetSystemMetrics(SM_CXVSCROLL);
	cxBorder = 3*GetSystemMetrics(SM_CXBORDER);
	cyBorder = 3*GetSystemMetrics(SM_CYBORDER);

	int cxEdge = GetSystemMetrics(SM_CXEDGE);
	int cyEdge = GetSystemMetrics(SM_CYEDGE);
	marginCheck.cxLeftWidth = marginCheck.cxRightWidth = 0;
	marginCheck.cyTopHeight = marginCheck.cyBottomHeight = 0;
	sizeCheck.cx = GetSystemMetrics(SM_CXMENUCHECK);
	sizeCheck.cy = GetSystemMetrics(SM_CYMENUCHECK);

	marginText.cxLeftWidth = cxEdge;
	marginText.cxRightWidth = std::max(cxEdge, 4);
	marginText.cyTopHeight = marginText.cyBottomHeight = cyEdge;

	marginBitmap.cxLeftWidth = marginBitmap.cxRightWidth = std::max(3, cxEdge);
	marginBitmap.cyTopHeight = marginBitmap.cyBottomHeight = std::max(3, cyEdge);

	memset(&marginCheckBackground, 0, sizeof(marginCheckBackground));
	memset(&marginItem, 0, sizeof(marginItem));
	marginCheck.cxLeftWidth = cxEdge;
}

void ListPopup::initTheme()
{
	initFallbackMetrics();
	flags &= ~FLAG_APP_THEMED;

#ifndef DISABLE_THEME
	openTheme(m_hWnd, VSCLASS_MENU);
	if (UX.pIsAppThemed && UX.pIsAppThemed()) flags |= FLAG_APP_THEMED;
#endif
	if (!hTheme) return;

	partId = MENU_POPUPITEM;
	if (IsThemePartDefined(hTheme, MENU_POPUPITEM_FOCUSABLE, MPI_HOT))
		partId = MENU_POPUPITEM_FOCUSABLE;
	UX.pGetThemeMargins(hTheme, nullptr, MENU_POPUPCHECK, 0, TMT_CONTENTMARGINS, nullptr, &marginCheck);
	UX.pGetThemeMargins(hTheme, nullptr, MENU_POPUPCHECKBACKGROUND, 0, TMT_CONTENTMARGINS, nullptr, &marginCheckBackground);
	UX.pGetThemeMargins(hTheme, nullptr, partId, 0, TMT_CONTENTMARGINS, nullptr, &marginItem);
	UX.pGetThemePartSize(hTheme, nullptr, MENU_POPUPCHECK, 0, nullptr, TS_TRUE, &sizeCheck);

	SIZE size;
	UX.pGetThemePartSize(hTheme, nullptr, MENU_POPUPBORDERS, 0, nullptr, TS_TRUE, &size);
	marginBitmap.cxLeftWidth = marginBitmap.cxRightWidth = size.cx;
	marginBitmap.cyTopHeight = marginBitmap.cyBottomHeight = size.cy;

	int popupBorderSize, popupBackgroundBorderSize;
	UX.pGetThemeInt(hTheme, partId, 0, TMT_BORDERSIZE, &popupBorderSize);
	UX.pGetThemeInt(hTheme, MENU_POPUPBACKGROUND, 0, TMT_BORDERSIZE, &popupBackgroundBorderSize);

	marginText.cxLeftWidth = popupBackgroundBorderSize;
	marginText.cxRightWidth = popupBorderSize;
	marginText.cyTopHeight = marginItem.cyTopHeight;
	marginText.cyBottomHeight = marginItem.cyBottomHeight;
}

void ListPopup::trackMouseEvent(bool cancel)
{
	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(tme);
	tme.dwFlags = TME_LEAVE | (cancel ? TME_CANCEL : 0);
	tme.hwndTrack = m_hWnd;
	tme.dwHoverTime = 0;
	TrackMouseEvent(&tme);
	if (cancel)
		flags &= ~FLAG_MOUSE_TRACKING;
	else
		flags |= FLAG_MOUSE_TRACKING;
}

void ListPopup::sendNotification(int index)
{
	if (!hWndNotif) return;
	uintptr_t data = index == -1 ? 0 : items[index].data;
	::SendMessage(hWndNotif, WMU_LIST_POPUP_RESULT, (WPARAM) index, (LPARAM) data);
}

void ListPopup::drawBorder(HDC hdc, const RECT& rc)
{
	if (hTheme && SUCCEEDED(UX.pDrawThemeBackground(hTheme, hdc, MENU_POPUPBORDERS, 0, &rc, nullptr)))
		return;

	int cxBaseBorder = GetSystemMetrics(SM_CXBORDER);
	int cyBaseBorder = GetSystemMetrics(SM_CYBORDER);

	RECT rc2 = rc;
	if ((flags & FLAG_APP_THEMED)/* || isFlatMenu()*/)
	{
		WinUtil::drawFrame(hdc, rc2, cxBaseBorder, cyBaseBorder, GetSysColorBrush(COLOR_BTNSHADOW));
		InflateRect(&rc2, -cxBaseBorder, -cyBaseBorder);
		WinUtil::drawFrame(hdc, rc2, 2*cxBaseBorder, 2*cyBaseBorder, GetSysColorBrush(COLOR_MENU));
	}
	else
	{
		DrawEdge(hdc, &rc2, EDGE_RAISED, (BF_RECT | BF_ADJUST));
		WinUtil::drawFrame(hdc, rc2, cxBaseBorder, cyBaseBorder, GetSysColorBrush(COLOR_MENU));
	}
}

void ListPopup::drawBackground(HDC hdc, const RECT& rc)
{
	if (hTheme)
		UX.pDrawThemeBackground(hTheme, hdc, MENU_POPUPBACKGROUND, 0, &rc, nullptr);
#if 0
	else
		FillRect(hdc, &rc, GetSysColorBrush(COLOR_MENU));
#endif
}

void ListPopup::drawItem(HDC hdc, const RECT& rc, int index)
{
	RECT rcDraw;
	const Item& item = items[index];
	if (hTheme)
	{
		int stateId = index == hotIndex ? MPI_HOT : MPI_NORMAL;
		int xCheck = rc.left;
		int xText = rc.left + marginItem.cxLeftWidth;
		if ((flags & FLAG_SHOW_CHECKS) || maxIconWidth)
		{
			int gapSize = sizeCheck.cx + totalWidth(marginCheck) + marginCheckBackground.cxLeftWidth;
			int bitmapSize = maxIconWidth ? maxIconWidth + totalWidth(marginBitmap) : 0;
			if (bitmapSize > gapSize) gapSize = bitmapSize;
			xCheck += marginItem.cxLeftWidth + gapSize;
			RECT rcGutter;
			rcGutter.left = rc.left;
			rcGutter.top = rc.top;
			rcGutter.right = xCheck + marginCheckBackground.cxRightWidth;
			rcGutter.bottom = rc.bottom;
			UX.pDrawThemeBackground(hTheme, hdc, MENU_POPUPGUTTER, 0, &rcGutter, nullptr);
			xText = rcGutter.right;
		}

		rcDraw = rc;
		rcDraw.left += marginItem.cxLeftWidth;
		rcDraw.right -= marginItem.cxRightWidth;
		UX.pDrawThemeBackground(hTheme, hdc, partId, stateId, &rcDraw, nullptr);

		if ((flags & FLAG_SHOW_CHECKS) && (item.flags & IF_CHECKED))
		{
			int checkWidth = sizeCheck.cx + totalWidth(marginCheck);
			int checkHeight = sizeCheck.cy + totalHeight(marginCheck);
			rcDraw.left = xCheck - checkWidth;
			rcDraw.right = xCheck;
			rcDraw.top = (rc.top + rc.bottom - checkHeight) / 2;
			rcDraw.bottom = rcDraw.top + checkHeight;
			UX.pDrawThemeBackground(hTheme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL, &rcDraw, nullptr);

			rcDraw.left += marginCheck.cxLeftWidth;
			rcDraw.top += marginCheck.cyTopHeight;
			rcDraw.right -= marginCheck.cxRightWidth;
			rcDraw.bottom -= marginCheck.cyBottomHeight;
			UX.pDrawThemeBackground(hTheme, hdc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL, &rcDraw, nullptr);
		}
		else if (item.icon && item.iconWidth && item.iconHeight)
		{
			int x = xCheck - marginBitmap.cxRightWidth - item.iconWidth;
			int y = (rc.top + rc.bottom - item.iconHeight) / 2;
			WinUtil::drawAlphaBitmap(hdc, item.icon, x, y, item.iconWidth, item.iconHeight);
		}

		rcDraw.left = xText + marginText.cxLeftWidth;
		rcDraw.top = rc.top + marginText.cyTopHeight;
		rcDraw.bottom = rc.bottom - marginText.cyBottomHeight;
		rcDraw.right = rc.right - marginText.cxRightWidth;
		HGDIOBJ oldFont = SelectObject(hdc, (item.flags & IF_DEFAULT) ? hFontBold : hFont);
		UX.pDrawThemeText(hTheme, hdc, partId, stateId, item.text.c_str(), (int) item.text.length(),
			DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_END_ELLIPSIS, 0, &rcDraw);
		SelectObject(hdc, oldFont);
	}
	else
	{
		if (index == hotIndex)
		{
#if 0 // This is the old style "flat" menu
			FillRect(hdc, &rc, GetSysColorBrush(COLOR_MENUHILIGHT));
			FrameRect(hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
#else
			FillRect(hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
#endif
		}
		else
			FillRect(hdc, &rc, GetSysColorBrush(COLOR_MENU));

		int xText = rc.left + marginItem.cxLeftWidth;
		if ((flags & FLAG_SHOW_CHECKS) || maxIconWidth)
		{
			int gapSize = 0;
			if (flags & FLAG_SHOW_CHECKS) gapSize = totalWidth(marginCheck) + sizeCheck.cx;
			if (maxIconWidth)
			{
				int bitmapSize = maxIconWidth + totalWidth(marginBitmap);
				if (bitmapSize > gapSize) gapSize = bitmapSize;
			}
			xText += gapSize;
		}

		COLORREF textColor = GetSysColor(index == hotIndex ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);
		if ((flags & FLAG_SHOW_CHECKS) && (item.flags & IF_CHECKED))
		{
			if (!checkBmp[0])
				checkBmp[0] = WinUtil::createFrameControlBitmap(hdc, sizeCheck.cx, sizeCheck.cy, DFC_MENU, DFCS_MENUCHECK);
			if (checkBmp[0])
				WinUtil::drawMonoBitmap(hdc, checkBmp[0],
					rc.left + marginItem.cxLeftWidth + marginCheck.cxLeftWidth,
					rc.top + (rc.bottom - rc.top - sizeCheck.cy) / 2,
					sizeCheck.cx, sizeCheck.cy, textColor);
		}
		else if (item.icon && item.iconWidth && item.iconHeight)
		{
			int x = rc.left + marginBitmap.cxLeftWidth;
			int y = (rc.top + rc.bottom - item.iconHeight) / 2;
			WinUtil::drawAlphaBitmap(hdc, item.icon, x, y, item.iconWidth, item.iconHeight);
		}

		rcDraw.left = xText + marginText.cxLeftWidth;
		rcDraw.top = rc.top + marginText.cyTopHeight;
		rcDraw.bottom = rc.bottom - marginText.cyBottomHeight;
		rcDraw.right = rc.right - marginText.cxRightWidth;
		HGDIOBJ oldFont = SelectObject(hdc, (item.flags & IF_DEFAULT) ? hFontBold : hFont);
		int oldMode = SetBkMode(hdc, TRANSPARENT);
		COLORREF oldColor = SetTextColor(hdc, textColor);
		DrawText(hdc, item.text.c_str(), (int) item.text.length(),
			&rcDraw, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_END_ELLIPSIS);
		SelectObject(hdc, oldFont);
		SetBkMode(hdc, oldMode);
		SetTextColor(hdc, oldColor);
	}
}

void ListPopup::draw(HDC hdc, const RECT& rc)
{
	drawBackground(hdc, rc);
	RECT rcItem = { rc.left, rc.top, rc.right, rc.top + itemHeight };

	int index = topIndex;
	int count = (int) items.size() - topIndex;
	if (count > maxVisibleItems) count = maxVisibleItems;
	while (count)
	{
		drawItem(hdc, rcItem, index);
		rcItem.top += itemHeight;
		rcItem.bottom += itemHeight;
		index++;
		count--;
	}
}

int ListPopup::hitTest(POINT pt, int& index) const
{
	CRect rc;
	GetClientRect(&rc);
	if (flags & FLAG_HAS_SCROLLBAR)
	{
		CRect rcScroll = rc;
		rcScroll.left = rcScroll.right - scrollBarSize;
		if (rcScroll.PtInRect(pt))
		{
			index = -1;
			return HT_SCROLL;
		}
	}
	if (!rc.PtInRect(pt) || itemHeight <= 0)
	{
		index = -1;
		return HT_EMPTY;
	}
	index = (pt.y - rc.top) / itemHeight + topIndex;
	if (index >= 0 && index < (int) items.size())
	{
		// ...
		return HT_ITEM;
	}
	index = -1;
	return HT_EMPTY;
}

void ListPopup::ensureVisible(int index, bool moveUp)
{
	int totalItems = (int) items.size();
	int count = totalItems - topIndex;
	if (count > maxVisibleItems) count = maxVisibleItems;
	if (index >= topIndex && index < topIndex + count) return;
	if (moveUp)
	{
		topIndex = index;
		if (topIndex + maxVisibleItems > totalItems) topIndex = totalItems - maxVisibleItems;
	}
	else
		topIndex = index - (maxVisibleItems - 1);
	if (topIndex < 0) topIndex = 0;
}

void ListPopup::clearBitmaps()
{
	for (int i = 0; i < _countof(checkBmp); i++)
		if (checkBmp[i])
		{
			DeleteObject(checkBmp[i]);
			checkBmp[i] = nullptr;
		}
}

LRESULT ListPopup::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	initTheme();
	updateFonts();
	updateScrollBar();
	return 0;
}

LRESULT ListPopup::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
{
	//ATLTRACE("ListPopup: onDestroy\n");
	closeTheme();
	clearFonts();
	clearBitmaps();
	return 0;
}

LRESULT ListPopup::onPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	CRect rc;
	GetClientRect(&rc);

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);

	if (!backingStore) backingStore = BackingStore::getBackingStore();
	if (backingStore)
	{
		int width = rc.right;
		int height = rc.bottom;
		HDC hMemDC = backingStore->getCompatibleDC(hdc, width, height);
		if (hMemDC)
		{
			drawBorder(hMemDC, rc);
			InflateRect(&rc, -cxBorder, -cyBorder);
			if (flags & FLAG_HAS_SCROLLBAR) rc.right -= scrollBarSize;
			draw(hMemDC, rc);
			BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);
		}
	}
	else
	{
		drawBorder(hdc, rc);
		InflateRect(&rc, -cxBorder, -cyBorder);
		if (flags & FLAG_HAS_SCROLLBAR) rc.right -= scrollBarSize;
		draw(hdc, rc);
	}

	EndPaint(&ps);

	return 0;
}

LRESULT ListPopup::onSize(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	int width = GET_X_LPARAM(lParam);
	int height = GET_Y_LPARAM(lParam);
	updateScrollBarPos(width, height);
#if 0
	if (!(flags & FLAG_MOUSE_CAPTURE) && width && height)
	{
		SetCapture();
		flags |= FLAG_MOUSE_CAPTURE;
	}
#endif
	return 0;
}

LRESULT ListPopup::onShowWindow(UINT, WPARAM wParam, LPARAM, BOOL&)
{
	RECT rc;
	GetClientRect(&rc);
	updateScrollBarPos(rc.right - rc.left, rc.bottom - rc.top);
#ifdef CAPTURE_MOUSE
	if (wParam)
	{
		if (!(flags & FLAG_MOUSE_CAPTURE))
		{
			SetCapture();
			flags |= FLAG_MOUSE_CAPTURE;
		}
	}
	else
	{
		if (flags & FLAG_MOUSE_CAPTURE)
		{
			flags ^= FLAG_MOUSE_CAPTURE;
			ReleaseCapture();
		}
	}
#endif
	return 0;
}

LRESULT ListPopup::onLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	int index;
	int ht = hitTest(pt, index);
	if (index >= 0)
	{
		if (!(flags & FLAG_PRESSED))
		{
			pressedIndex = index;
			flags |= FLAG_PRESSED;
		}
		if (index != hotIndex)
		{
			hotIndex = index;
			Invalidate();
		}
	}
	else if (ht == HT_SCROLL)
		return ::SendMessage(hWndScroll, uMsg, wParam, lParam);
	else
		sendNotification(-1);
	return 0;
}

LRESULT ListPopup::onLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	int index;
	int ht = hitTest(pt, index);
	if (flags & FLAG_PRESSED)
	{
		flags ^= FLAG_PRESSED;
		if (index == pressedIndex) sendNotification(index);
		pressedIndex = -1;
	}
	else if (ht == HT_SCROLL)
		return ::SendMessage(hWndScroll, uMsg, wParam, lParam);
	return 0;
}

LRESULT ListPopup::onMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	//ATLTRACE("onMouseMove: hWnd=0x%X, x=%d, y=%d\n", m_hWnd, pt.x, pt.y);
	int newIndex;
	int ht = hitTest(pt, newIndex);
	if (ht == HT_SCROLL)
		return ::SendMessage(hWndScroll, uMsg, wParam, lParam);
	if (newIndex != hotIndex)
	{
		hotIndex = newIndex;
		Invalidate();
	}
	if (!(flags & FLAG_MOUSE_TRACKING)) trackMouseEvent(false);
#ifdef CAPTURE_MOUSE
	if ((flags & FLAG_MOUSE_CAPTURE) && hWndNotif)
	{
		ClientToScreen(&pt);
		::ScreenToClient(hWndNotif, &pt);
		::PostMessage(hWndNotif, WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));
	}
#endif
	return 0;
}

LRESULT ListPopup::onMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	//ATLTRACE("onMouseLeave\n");
	if (flags & FLAG_MOUSE_TRACKING) trackMouseEvent(true);
	if (hotIndex != -1)
	{
		hotIndex = -1;
		Invalidate();
	}
	return 0;
}

void ListPopup::updateScrollValue()
{
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS;
	si.nPos = topIndex;
	::SetScrollInfo(hWndScroll, SB_CTL, &si, TRUE);
}

LRESULT ListPopup::onScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	int maxScroll = (int) items.size() - maxVisibleItems;
	if (maxScroll <= 0)
	{
		bHandled = FALSE;
		return 0;
	}
	SCROLLINFO si;
	int prevIndex = topIndex;
	switch (LOWORD(wParam))
	{
		case SB_TOP:
			topIndex = 0;
			break;
		case SB_BOTTOM:
			topIndex = maxScroll;
			break;
		case SB_ENDSCROLL:
			return 0;
		case SB_LINEDOWN:
			if (++topIndex > maxScroll) topIndex = maxScroll;
			break;
		case SB_LINEUP:
			if (--topIndex < 0) topIndex = 0;
			break;
		case SB_PAGEDOWN:
			si.cbSize = sizeof(si);
			si.fMask = SIF_PAGE;
			::GetScrollInfo(hWndScroll, SB_CTL, &si);
			topIndex += si.nPage;
			if (topIndex > maxScroll) topIndex = maxScroll;
			break;
		case SB_PAGEUP:
			si.cbSize = sizeof(si);
			si.fMask = SIF_PAGE;
			::GetScrollInfo(hWndScroll, SB_CTL, &si);
			topIndex -= si.nPage;
			if (topIndex < 0) topIndex = 0;
			break;
		case SB_THUMBTRACK:
			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS;
			::GetScrollInfo(hWndScroll, SB_CTL, &si);
			//ATLTRACE("SB_THUMBTRACK: %d\n", si.nTrackPos);
			topIndex = si.nTrackPos;
	}

	if (topIndex != prevIndex)
	{
		updateScrollValue();
		Invalidate();
	}
	return 0;
}

LRESULT ListPopup::onKillFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	//ATLTRACE("onKillFocus\n");
	sendNotification(-1);
	return 0;
}

LRESULT ListPopup::onMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	int delta = GET_WHEEL_DELTA_WPARAM(wParam);
	return onScroll(WM_VSCROLL, delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0, bHandled);
}

LRESULT ListPopup::onKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	int prevIndex = topIndex;
	switch (wParam)
	{
		case VK_ESCAPE:
			sendNotification(-1);
			break;
		case VK_RETURN:
			if (hotIndex != -1) sendNotification(hotIndex);
			break;
		case VK_UP:
			if (items.empty()) break;
			if (--hotIndex < 0) hotIndex = (int) items.size() - 1;
			ensureVisible(hotIndex, true);
			Invalidate();
			break;
		case VK_DOWN:
			if (items.empty()) break;
			if (++hotIndex >= (int) items.size()) hotIndex = 0;
			ensureVisible(hotIndex, false);
			Invalidate();
			break;
	}
	if (topIndex != prevIndex)
	{
		if (flags & FLAG_HAS_SCROLLBAR) updateScrollValue();
		Invalidate();
	}
	return 0;
}
