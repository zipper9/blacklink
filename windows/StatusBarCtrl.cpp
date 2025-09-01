#include "stdafx.h"
#include "StatusBarCtrl.h"
#include "BackingStore.h"
#include "GdiUtil.h"
#include "UxThemeLib.h"
#include <algorithm>
#include <vssym32.h>

#define UX UxThemeLib::instance

#undef min
#undef max

static const int DEFAULT_HORIZ_PADDING = 4;
static const int DEFAULT_VERT_PADDING  = 1;

static inline bool isAppThemed()
{
	UX.init();
	return UX.pIsAppThemed && UX.pIsAppThemed();
}

StatusBarCtrl::StatusBarCtrl()
{
	paneStyle = PANE_STYLE_DEFAULT;
	padding.left = padding.right = DEFAULT_HORIZ_PADDING;
	padding.top = padding.bottom = DEFAULT_VERT_PADDING;
	iconSpace = 3;
	hFont = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
	flags = FLAG_UPDATE_HEIGHT | FLAG_USE_THEME | FLAG_UPDATE_THEME | FLAG_USE_STYLE_METRICS | FLAG_AUTO_REDRAW;
	minHeight = 0;
	fontHeight = 0;
	backgroundType = COLOR_TYPE_SYSCOLOR;
	backgroundColor = COLOR_BTNFACE;
	separatorType = COLOR_TYPE_SYSCOLOR;
	separatorColor = COLOR_WINDOWTEXT;
	gripperSize = 0;
	backingStore = nullptr;
	callback = nullptr;
	hWndParent = nullptr;
	margins = {};
}

void StatusBarCtrl::initStyleMetrics()
{
	static const int borderSize = 1;
	margins.topSpace = GetSystemMetrics(SM_CYEDGE);
	margins.bottomSpace = 0;
	padding.left = padding.right = DEFAULT_HORIZ_PADDING;
	padding.top = padding.bottom = DEFAULT_VERT_PADDING;
	if (resolveStyle() == PANE_STYLE_BEVEL)
	{
		margins.horizSpace = GetSystemMetrics(SM_CXEDGE);
		padding.left += borderSize;
		padding.right += borderSize;
		padding.top += borderSize;
		padding.bottom += borderSize;
	}
	else
	{
		margins.horizSpace = 0;
		padding.right += borderSize;
		padding.top += borderSize;
		padding.bottom += borderSize;
	}
	flags &= ~FLAG_INIT_STYLE_METRICS;
}

void StatusBarCtrl::init()
{
	if (flags & FLAG_UPDATE_THEME)
	{
		updateTheme();
		if (flags & FLAG_USE_STYLE_METRICS) flags |= FLAG_INIT_STYLE_METRICS;
	}
	if (flags & FLAG_INIT_STYLE_METRICS)
	{
		initStyleMetrics();
		flags |= FLAG_UPDATE_LAYOUT;
	}
}

int StatusBarCtrl::resolveStyle() const
{
	if (paneStyle != PANE_STYLE_DEFAULT) return paneStyle;
	return (flags & FLAG_USE_THEME) && isAppThemed() ? PANE_STYLE_LINE : PANE_STYLE_BEVEL;
}

void StatusBarCtrl::cleanup()
{
	if (backingStore)
	{
		backingStore->release();
		backingStore = nullptr;
	}
	if (flags & FLAG_OWN_FONT)
	{
		DeleteObject(hFont);
		hFont = nullptr;
		flags ^= FLAG_OWN_FONT;
	}
}

void StatusBarCtrl::setPanes(int count, const PaneInfo pi[])
{
	panes.clear();
	panes.reserve(count);
	for (int i = 0; i < count; i++)
		addPane(pi[i]);
}

void StatusBarCtrl::addPane(const PaneInfo& pi)
{
	Pane p;
	p.icon = nullptr;
	p.width = pi.minWidth;
	p.minWidth = pi.minWidth;
	p.maxWidth = pi.maxWidth;
	p.weight = pi.weight;
	p.iconWidth = p.iconHeight = 0;
	p.textWidth = 0;
	p.align = pi.align;
	p.flags = pi.flags & VALID_FLAGS_MASK;
	panes.push_back(p);
	flags |= FLAG_UPDATE_LAYOUT;
}

void StatusBarCtrl::setPaneInfo(int index, const PaneInfo& pi)
{
	Pane& p = panes[index];
	p.minWidth = pi.minWidth;
	p.maxWidth = pi.maxWidth;
	p.weight = pi.weight;
	p.align = pi.align;
	p.flags = (p.flags & ~VALID_FLAGS_MASK) | (pi.flags & VALID_FLAGS_MASK);
	flags |= FLAG_UPDATE_LAYOUT;
}

void StatusBarCtrl::getPaneInfo(int index, PaneInfo& pi) const
{
	const Pane& p = panes[index];
	pi.minWidth = p.minWidth;
	pi.maxWidth = p.maxWidth;
	pi.weight = p.weight;
	pi.align = p.align;
	pi.flags = p.flags;
}

int StatusBarCtrl::getPaneWidth(int index) const
{
	return panes[index].width;
}

void StatusBarCtrl::updateContentSize(Pane& p, HDC hdc)
{
	if (p.flags & PANE_FLAG_PRIV_UPDATE_TEXT)
	{
		RECT rc = {};
		DrawText(hdc, p.text.c_str(), (int) p.text.length(), &rc, DT_SINGLELINE | DT_CALCRECT);
		p.textWidth = (uint16_t) (rc.right - rc.left);
		p.flags ^= PANE_FLAG_PRIV_UPDATE_TEXT;
	}
	if (p.flags & PANE_FLAG_PRIV_UPDATE_ICON)
	{
		BITMAP bitmap;
		if (p.icon && GetObject(p.icon, sizeof(bitmap), &bitmap))
		{
			p.iconWidth = (uint16_t) bitmap.bmWidth;
			p.iconHeight = (uint16_t) abs(bitmap.bmHeight);
			if (bitmap.bmBitsPixel == 32)
				p.flags |= PANE_FLAG_PRIV_ALPHA_BITMAP;
			else
				p.flags &= ~PANE_FLAG_PRIV_ALPHA_BITMAP;
		}
		else
			p.iconWidth = p.iconHeight = 0;
		p.flags ^= PANE_FLAG_PRIV_UPDATE_ICON;
	}
}

void StatusBarCtrl::updateWidth(Pane& p, HDC hdc)
{
	if (p.minWidth == p.maxWidth)
	{
		p.width = p.minWidth + padding.left + padding.right;
		return;
	}
	p.width = p.textWidth + p.iconWidth;
	if (p.textWidth && p.iconWidth) p.width += iconSpace;
	if (p.width < p.minWidth)
		p.width = p.minWidth;
	else if (p.width > p.maxWidth)
		p.width = p.maxWidth;
	p.width += padding.left + padding.right;
}

void StatusBarCtrl::calcWidth(int width, HDC hdc)
{
	HGDIOBJ prevFont = SelectObject(hdc, hFont);
	do
	{
		int fixedWidth = 0;
		unsigned weightSum = 0;
		for (size_t i = 0; i < panes.size(); ++i)
		{
			Pane& p = panes[i];
			if ((p.flags & PANE_FLAG_HIDE_EMPTY) && isPaneEmpty(i))
			{
				p.width = 0;
				p.flags |= PANE_FLAG_PRIV_WIDTH_SET;
				continue;
			}
			if (p.flags & PANE_FLAG_PRIV_WIDTH_SET)
			{
				fixedWidth += p.width;
				continue;
			}
			updateContentSize(p, hdc);
			if (p.weight)
			{
				weightSum += p.weight;
				continue;
			}
			updateWidth(p, hdc);
			fixedWidth += p.width;
			p.flags |= PANE_FLAG_PRIV_WIDTH_SET;
		}
		if (!weightSum) break;
		int availWidth = width - fixedWidth;
		if (availWidth < 0) availWidth = 0;
		Pane* lastPane = nullptr;
		int propWidthSum = 0;
		bool recalcWidth = false;
		for (Pane& p : panes)
		{
			if ((p.flags & PANE_FLAG_PRIV_WIDTH_SET) || !p.weight) continue;
			if (lastPane) propWidthSum += lastPane->width;
			p.width = p.weight * availWidth / weightSum;
			if (p.width < p.minWidth)
			{
				p.width = p.minWidth;
				recalcWidth = true;
			}
			else if (p.width > p.maxWidth)
			{
				p.width = p.maxWidth;
				recalcWidth = true;
			}
			if (recalcWidth)
			{
				p.flags |= PANE_FLAG_PRIV_WIDTH_SET;
				break;
			}
			lastPane = &p;
		}
		if (recalcWidth) continue;
		if (lastPane) lastPane->width = availWidth - propWidthSum;
	} while (0);
	SelectObject(hdc, prevFont);
}

void StatusBarCtrl::setPaneText(int index, const tstring& text)
{
	Pane& p = panes[index];
	if (p.text != text)
	{
		p.text = text;
		p.flags |= PANE_FLAG_PRIV_UPDATE_TEXT;
		if (p.minWidth != p.maxWidth) flags |= FLAG_UPDATE_LAYOUT;
		if ((flags & FLAG_AUTO_REDRAW) && m_hWnd) Invalidate(FALSE);
	}
}

const tstring& StatusBarCtrl::getPaneText(int index) const
{
	return panes[index].text;
}

void StatusBarCtrl::setPaneIcon(int index, HBITMAP hBitmap)
{
	Pane& p = panes[index];
	if (p.icon != hBitmap)
	{
		p.icon = hBitmap;
		p.flags |= PANE_FLAG_PRIV_UPDATE_ICON;
		if (p.minWidth != p.maxWidth) flags |= FLAG_UPDATE_LAYOUT;
		if ((flags & FLAG_AUTO_REDRAW) && m_hWnd) Invalidate(FALSE);
	}
}

HBITMAP StatusBarCtrl::getPaneIcon(int index) const
{
	return panes[index].icon;
}

void StatusBarCtrl::updateLayout(HDC hdc)
{
	flags &= ~FLAG_UPDATE_LAYOUT;
	gripperSize = (flags & FLAG_SHOW_GRIPPER) ? GetSystemMetrics(SM_CXVSCROLL) : 0;
	RECT rc;
	GetClientRect(&rc);
	int numVisible = getNumVisiblePanes();
	if (numVisible)
	{
		int width = rc.right - rc.left - (numVisible - 1) * margins.horizSpace;
		if (gripperSize) width -= gripperSize + margins.horizSpace;
		if (width < 0) width = 0;
		for (Pane& p : panes)
			p.flags &= ~PANE_FLAG_PRIV_WIDTH_SET;
		calcWidth(width, hdc);
	}
	if (flags & FLAG_AUTO_REDRAW)
		Invalidate(FALSE);
}

void StatusBarCtrl::draw(HDC hdc, const RECT& rcClient)
{
	RECT rcPanes = rcClient;
	if (gripperSize)
	{
		if (flags & FLAG_RIGHT_TO_LEFT)
			rcPanes.left += gripperSize + margins.horizSpace;
		else
			rcPanes.right -= gripperSize + margins.horizSpace;
	}

	if (backgroundType != COLOR_TYPE_TRANSPARENT)
	{
		if (hTheme)
		{
			UX.pDrawThemeBackground(hTheme, hdc, 0, 0, &rcClient, nullptr);
		}
		else
		{
			HBRUSH brush = backgroundType == COLOR_TYPE_SYSCOLOR ? GetSysColorBrush((int) backgroundColor) : CreateSolidBrush(backgroundColor);
			FillRect(hdc, &rcClient, brush);
			if (backgroundType != COLOR_TYPE_SYSCOLOR) DeleteObject(brush);
		}
	}

	int style = resolveStyle();
	RECT rc = rcPanes;
	rc.top += margins.topSpace;
	rc.bottom -= margins.bottomSpace;
	int lastIndex = (flags & FLAG_RIGHT_TO_LEFT) ? getFirstVisibleIndex() : getLastVisibleIndex();
	HGDIOBJ prevFont = SelectObject(hdc, hFont);
	int prevMode = SetBkMode(hdc, TRANSPARENT);
	for (size_t i = 0; i < panes.size(); i++)
	{
		if (!panes[i].width && (panes[i].flags & PANE_FLAG_HIDE_EMPTY)) continue;
		if (flags & FLAG_RIGHT_TO_LEFT)
			rc.left = std::max(rc.right - panes[i].width, rcPanes.left);
		else
			rc.right = std::min(rc.left + panes[i].width, rcPanes.right);
		if (!(panes[i].flags & PANE_FLAG_NO_DECOR))
		{
			if (style == PANE_STYLE_BEVEL)
				WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNSHADOW), GetSysColorBrush(COLOR_BTNHILIGHT));
			else if (i != lastIndex)
				drawSeparator(hdc, rc);
		}
		if (panes[i].flags & PANE_FLAG_CUSTOM_DRAW)
		{
			if (callback) callback->drawStatusPane((int) i, hdc, rc);
		}
		else
			drawContent(hdc, rc, panes[i]);
		if (flags & FLAG_RIGHT_TO_LEFT)
		{
			rc.right = rc.left - margins.horizSpace;
			if (rc.right < rcPanes.left) break;
		}
		else
		{
			rc.left = rc.right + margins.horizSpace;
			if (rc.left >= rcPanes.right) break;
		}
	}
	SelectObject(hdc, prevFont);
	SetBkMode(hdc, prevMode);
	if (gripperSize)
	{
		RECT rcGripper;
		rcGripper.left = (flags & FLAG_RIGHT_TO_LEFT) ? rcClient.left : rcClient.right - gripperSize;
		rcGripper.top = rcClient.bottom - gripperSize;
		rcGripper.right = rcGripper.left + gripperSize;
		rcGripper.bottom = rcClient.bottom;
		if (hTheme)
			UX.pDrawThemeBackground(hTheme, hdc, SP_GRIPPER, (flags & FLAG_RIGHT_TO_LEFT) ? 2 : 1, &rcGripper, nullptr);
		else
			DrawFrameControl(hdc, &rcGripper, DFC_SCROLL, (flags & FLAG_RIGHT_TO_LEFT) ? DFCS_SCROLLSIZEGRIPRIGHT : DFCS_SCROLLSIZEGRIP);
	}
}

void StatusBarCtrl::drawSeparator(HDC hdc, const RECT& rc)
{
	if (separatorType == COLOR_TYPE_TRANSPARENT) return;
	if (hTheme)
	{
		UX.pDrawThemeBackground(hTheme, hdc, SP_PANE, 0, &rc, nullptr);
		return;
	}
	HBRUSH brush = separatorType == COLOR_TYPE_SYSCOLOR ? GetSysColorBrush((int) separatorColor) : CreateSolidBrush(separatorColor);
	RECT rcSep = rc;
	rcSep.left = rc.right - 1;
	FillRect(hdc, &rcSep, brush);
	if (separatorType != COLOR_TYPE_SYSCOLOR) DeleteObject(brush);
}

void StatusBarCtrl::drawContent(HDC hdc, const RECT& rc, const Pane& p)
{
	int xleft = rc.left + padding.left;
	int xright = rc.right - padding.right;
	if (p.iconWidth)
	{
		int x = (p.align & ALIGN_RIGHT) ? xright : xleft;
		int y = rc.top + (rc.bottom - rc.top - p.iconHeight) / 2;
		int xsrc = 0, ysrc = 0;
		int width = p.iconWidth;
		int height = p.iconHeight;
		if (p.align & ALIGN_RIGHT)
		{
			x -= width;
			if (x < xleft)
			{
				xsrc = xleft - x;
				x = xleft;
				width -= xsrc;
			}
		}
		else if (x + width > xright)
			width = xright - x;
		if (width <= 0)
			return;
		if (y < rc.top)
		{
			ysrc = rc.top - y;
			y = rc.top;
			height -= ysrc;
		}
		if (y + height > rc.bottom)
			height = rc.bottom - y;
		if (p.flags & PANE_FLAG_PRIV_ALPHA_BITMAP)
			WinUtil::drawAlphaBitmap(hdc, p.icon, x, y, xsrc, ysrc, width, height);
		else
			WinUtil::drawBitmap(hdc, p.icon, x, y, xsrc, ysrc, width, height);
		if (width < p.iconWidth)
			return;
		if (p.align & ALIGN_RIGHT)
			xright = x - iconSpace;
		else
			xleft = x + width + iconSpace;
	}
	if (p.textWidth)
	{
		RECT rcDraw;
		rcDraw.left = xleft;
		rcDraw.right = xright;
		rcDraw.top = rc.top;
		rcDraw.bottom = rc.bottom;
		int flags = DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX | ((p.align & ALIGN_RIGHT) ? DT_RIGHT : DT_LEFT);
		if (xright - xleft < (int) p.textWidth)
			flags |= DT_END_ELLIPSIS;
		else
			flags |= DT_NOCLIP;
		DrawText(hdc, p.text.c_str(), (int) p.text.length(), &rcDraw, flags);
	}
}

int StatusBarCtrl::findPane(POINT pt) const
{
	RECT rc;
	GetClientRect(&rc);

	rc.top += margins.topSpace;
	rc.bottom -= margins.bottomSpace;
	if (pt.y < rc.top || pt.y >= rc.bottom) return -1;

	for (size_t i = 0; i < panes.size(); i++)
	{
		if (!panes[i].width && (panes[i].flags & PANE_FLAG_HIDE_EMPTY)) continue;
		if (flags & FLAG_RIGHT_TO_LEFT)
		{
			if (pt.x >= rc.right) break;
			rc.left = rc.right - panes[i].width;
			if (pt.x >= rc.left) return (int) i;
			rc.right = rc.left - margins.horizSpace;
		}
		else
		{
			if (pt.x < rc.left) break;
			rc.right = rc.left + panes[i].width;
			if (pt.x < rc.right) return (int) i;
			rc.left = rc.right + margins.horizSpace;
		}
	}
	return -1;
}

void StatusBarCtrl::updateFontHeight(HDC hdc)
{
	HGDIOBJ prevFont = SelectObject(hdc, hFont);
	TEXTMETRIC tm;
	GetTextMetrics(hdc, &tm);
	fontHeight = tm.tmHeight + tm.tmInternalLeading;
	if (!tm.tmInternalLeading) fontHeight += 2;
	SelectObject(hdc, prevFont);
	flags &= ~FLAG_UPDATE_HEIGHT;
}

void StatusBarCtrl::setFont(HFONT hFont, bool ownFont)
{
	HFONT oldFont = this->hFont;
	this->hFont = hFont;
	if (oldFont && (flags & FLAG_OWN_FONT))
	{
		DeleteObject(oldFont);
		flags ^= FLAG_OWN_FONT;
	}
	if (ownFont) flags |= FLAG_OWN_FONT;
	flags |= FLAG_UPDATE_HEIGHT;
	for (Pane& p : panes)
		p.flags |= PANE_FLAG_PRIV_UPDATE_TEXT;
}

void StatusBarCtrl::setUseTheme(bool flag)
{
	int newFlags = flags;
	if (flag)
		newFlags |= FLAG_USE_THEME;
	else
		newFlags &= ~FLAG_USE_THEME;
	if (flags != newFlags)
	{
		flags = newFlags | FLAG_UPDATE_THEME;
		closeTheme();
	}
}

void StatusBarCtrl::setFlag(bool enable, int flag)
{
	int newFlags;
	if (enable)
		newFlags = flags | flag;
	else
		newFlags = flags & ~flag;
	if (flags != newFlags)
		flags = newFlags | FLAG_UPDATE_LAYOUT;
}

void StatusBarCtrl::setShowGripper(bool flag)
{
	setFlag(flag, FLAG_SHOW_GRIPPER);
}

void StatusBarCtrl::setAutoGripper(bool flag)
{
	setFlag(flag, FLAG_AUTO_GRIPPER);
}

void StatusBarCtrl::setRightToLeft(bool flag)
{
	setFlag(flag, FLAG_RIGHT_TO_LEFT);
}

void StatusBarCtrl::setAutoRedraw(bool flag)
{
	if (flag)
	{
		flags |= FLAG_AUTO_REDRAW;
		if ((flags & FLAG_UPDATE_LAYOUT) && m_hWnd) Invalidate(FALSE);
	}
	else
		flags &= ~FLAG_AUTO_REDRAW;
}

int StatusBarCtrl::getPrefHeight(HDC hdc)
{
	init();
	if (flags & FLAG_UPDATE_HEIGHT) updateFontHeight(hdc);
	int height = fontHeight;
	HGDIOBJ prevFont = SelectObject(hdc, hFont);
	for (Pane& p : panes)
	{
		updateContentSize(p, hdc);
		if (p.iconHeight > height) height = p.iconHeight;
	}
	SelectObject(hdc, prevFont);
	height += padding.top + padding.bottom + margins.topSpace + margins.bottomSpace;
	if (height < minHeight) height = minHeight;
	return height;
}

void StatusBarCtrl::setMinHeight(int height)
{
	minHeight = height;
}

void StatusBarCtrl::setIconSpace(int space)
{
	if (iconSpace != space)
	{
		iconSpace = space;
		flags |= FLAG_UPDATE_LAYOUT;
	}
}

void StatusBarCtrl::setPanePadding(const Margins& p)
{
	if (padding.left != p.left || padding.right != p.right)
		flags |= FLAG_UPDATE_LAYOUT;
	padding = p;
	flags &= ~FLAG_USE_STYLE_METRICS;
}

void StatusBarCtrl::setPaneMargins(const PaneMargins& m)
{
	if (margins.horizSpace != m.horizSpace)
		flags |= FLAG_UPDATE_LAYOUT;
	margins = m;
	flags &= ~FLAG_USE_STYLE_METRICS;
}

void StatusBarCtrl::updateTheme()
{
	if (flags & FLAG_USE_THEME)
	{
		if (!hTheme) openTheme(m_hWnd, L"STATUS");
	}
	else
		closeTheme();
	flags &= ~FLAG_UPDATE_THEME;
}

void StatusBarCtrl::updateGripperState()
{
	int oldFlags = flags;
	if (::IsZoomed(hWndParent))
		flags &= ~FLAG_SHOW_GRIPPER;
	else
		flags |= FLAG_SHOW_GRIPPER;
	if (flags != oldFlags)
		flags |= FLAG_UPDATE_LAYOUT;
}

void StatusBarCtrl::getPaneRect(int index, RECT& rc) const
{
	GetClientRect(&rc);
	rc.top += margins.topSpace;
	rc.bottom -= margins.bottomSpace;

	for (size_t i = 0; i < panes.size(); i++)
	{
		if (!panes[i].width && (panes[i].flags & PANE_FLAG_HIDE_EMPTY))
		{
			if (i == index)
			{
				rc.left = rc.top = rc.right = rc.bottom = 0;
				break;
			}
			continue;
		}
		if (flags & FLAG_RIGHT_TO_LEFT)
			rc.left = rc.right - panes[i].width;
		else
			rc.right = rc.left + panes[i].width;
		if (i == index) break;
		if (flags & FLAG_RIGHT_TO_LEFT)
			rc.right = rc.left - margins.horizSpace;
		else
			rc.left = rc.right + margins.horizSpace;
	}
}

int StatusBarCtrl::getNumVisiblePanes() const
{
	int hidden = 0;
	for (size_t i = 0; i < panes.size(); ++i)
		if ((panes[i].flags & PANE_FLAG_HIDE_EMPTY) && isPaneEmpty(i)) hidden++;
	return (int) panes.size() - hidden;
}

int StatusBarCtrl::getFirstVisibleIndex() const
{
	for (int i = 0; i < (int) panes.size(); ++i)
		if (panes[i].width || !(panes[i].flags & PANE_FLAG_HIDE_EMPTY))
			return i;
	return -1;
}

int StatusBarCtrl::getLastVisibleIndex() const
{
	for (int i = (int) panes.size() - 1; i >= 0; --i)
		if (panes[i].width || !(panes[i].flags & PANE_FLAG_HIDE_EMPTY))
			return i;
	return -1;
}

bool StatusBarCtrl::isPaneEmpty(int index) const
{
	const Pane& p = panes[index];
	if (!p.minWidth && !p.maxWidth) return true;
	if ((p.flags & PANE_FLAG_CUSTOM_DRAW) && callback) return callback->isStatusPaneEmpty(index);
	return p.text.empty() && !p.icon;
}

int StatusBarCtrl::getPaneContentWidth(HDC hdc, int index, bool includePadding)
{
	Pane& p = panes[index];
	HGDIOBJ prevFont = SelectObject(hdc, hFont);
	updateContentSize(p, hdc);
	SelectObject(hdc, prevFont);
	int width = p.textWidth + p.iconWidth;
	if (p.textWidth && p.iconWidth) width += iconSpace;
	if (includePadding) width += padding.left + padding.right;
	return width;
}

void StatusBarCtrl::handleClick(LPARAM lParam, int button)
{
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	int index = findPane(pt);
	if (index != -1 && (panes[index].flags & PANE_FLAG_MOUSE_CLICKS))
		callback->statusPaneClicked(index, button, pt);
}

void StatusBarCtrl::forceUpdate()
{
	flags |= FLAG_UPDATE_LAYOUT;
}

LRESULT StatusBarCtrl::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
	hWndParent = cs->hwndParent;
	return 0;
}

LRESULT StatusBarCtrl::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	cleanup();
	hWndParent = nullptr;
	return 0;
}

LRESULT StatusBarCtrl::onPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	if (flags & FLAG_AUTO_GRIPPER) updateGripperState();
	init();
	bool drawn = false;
	RECT rc;
	GetClientRect(&rc);
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);
	if (flags & FLAG_UPDATE_LAYOUT) updateLayout(hdc);
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

LRESULT StatusBarCtrl::onSize(UINT, WPARAM, LPARAM, BOOL&)
{
	if (flags & FLAG_AUTO_GRIPPER) updateGripperState();
	init();
	HDC hdc = GetDC();
	updateLayout(hdc);
	ReleaseDC(hdc);
	return 0;
}

LRESULT StatusBarCtrl::onLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	if (callback) handleClick(lParam, MOUSE_BUTTON_LEFT);
	return 0;
}

LRESULT StatusBarCtrl::onRButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	if (callback) handleClick(lParam, MOUSE_BUTTON_RIGHT);
	return 0;
}

LRESULT StatusBarCtrl::onThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	closeTheme();
	flags |= FLAG_UPDATE_THEME | FLAG_UPDATE_LAYOUT;
	return 0;
}
