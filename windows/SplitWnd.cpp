#include "stdafx.h"
#include "SplitWnd.h"
#include <algorithm>
#include <string.h>

static inline void clearRect(RECT& rc)
{
	rc.left = rc.right = rc.top = rc.bottom = 0;
}

static inline bool equalRect(const RECT& rc1, const RECT& rc2)
{
	return rc1.left == rc2.left &&
	       rc1.right == rc2.right &&
	       rc1.top == rc2.top &&
	       rc1.bottom == rc2.bottom;
}

static HBRUSH createHalftoneBrush()
{
	uint16_t pattern[8];
	for (int i = 0; i < 8; i++)
		pattern[i] = 0x5555 << (i & 1);
	HBITMAP hBitmap = CreateBitmap(8, 8, 1, 1, pattern);
	if (!hBitmap) return nullptr;
	HBRUSH hBrush = CreatePatternBrush(hBitmap);
	DeleteObject(hBitmap);
	return hBrush;
}

SplitWndBase::SplitWndBase()
{
	callback = nullptr;
	memset(&margins, 0, sizeof(margins));
	hotIndex = -1;
	halftoneBrush = nullptr;
	fullDragMode = FULL_DRAG_DEFAULT;
	ghostBarPos = INT_MIN;
	singlePane = -1;
	options = OPT_PAINT_MARGINS;
}

SplitWndBase::~SplitWndBase()
{
	cleanup();
}

void SplitWndBase::cleanup()
{
	if (halftoneBrush)
	{
		DeleteObject(halftoneBrush);
		halftoneBrush = nullptr;
	}
}

void SplitWndBase::updateLayout()
{
	if (sp.empty()) return;
	if (singlePane != -1)
	{
		int index = singlePane >> 1, which = singlePane & 1;
		RECT rc;
		getRootRect(rc);
		PaneInfo& pane = sp[index].panes[which];
		if (!equalRect(rc, pane.rc))
		{
			pane.rc = rc;
			paneRectUpdated(index, which);
		}
		return;
	}
	if (sp[0].flags & FLAG_PROPORTIONAL)
		sp[0].state |= STATE_APPLY_PROPORTIONAL_POS;
	changePosition(0, sp[0].position);
}

void SplitWndBase::addSplitter(int pane, uint16_t flags, int position, int thickness)
{
	if (thickness < 0)
	{
		thickness = getDefaultThickness(flags);
		flags &= ~FLAG_SET_THICKNESS;
	}
	else
		flags |= FLAG_SET_THICKNESS;
	SplitterInfo info;
	info.flags = flags;
	info.state = 0;
	info.prev = pane;
	info.position = position;
	info.propPos = position;
	info.thickness = thickness;
	info.colorType = COLOR_TYPE_SYSCOLOR;
	info.color = COLOR_BTNFACE;
	for (int i = 0; i < 2; ++i)
	{
		clearRect(info.panes[i].rc);
		info.panes[i].hWnd = nullptr;
		info.panes[i].flags = 0;
	}
	if (flags & FLAG_PROPORTIONAL)
		info.state |= STATE_FORCE_UPDATE | STATE_APPLY_PROPORTIONAL_POS;
	sp.push_back(info);
}

void SplitWndBase::setPaneWnd(int pane, HWND hWnd)
{
	int index = pane >> 1, which = pane & 1;
	PaneInfo& pi = sp[index].panes[which];
	if (pi.hWnd != hWnd)
	{
		pi.hWnd = hWnd;
		clearRect(pi.rc);
		if (pi.flags & FLAG_WND_HIDDEN)
		{
			if (hWnd)
				ShowWindow(hWnd, SW_HIDE);
			else
				pi.flags ^= FLAG_WND_HIDDEN;
		}
	}
}

void SplitWndBase::setSinglePaneMode(int pane)
{
	if (pane < -1) pane = -1;
	if (pane == singlePane) return;
	for (int i = 0; i < (int) sp.size(); ++i)
		for (int j = 0; j < 2; ++j)
		{
			PaneInfo& pi = sp[i].panes[j];
			if (!pi.hWnd) continue;
			if ((i << 1) + j == pane || pane == -1)
			{
				if (pi.flags & FLAG_WND_HIDDEN)
				{
					ShowWindow(pi.hWnd, SW_SHOWNA);
					pi.flags ^= FLAG_WND_HIDDEN;
				}
			}
			else
			{
				if (!(pi.flags & FLAG_WND_HIDDEN))
				{
					ShowWindow(pi.hWnd, SW_HIDE);
					pi.flags |= FLAG_WND_HIDDEN;
				}
			}
		}
	singlePane = pane;
}

void SplitWndBase::setSplitterColor(int index, int colorType, COLORREF color)
{
	sp[index].colorType = colorType;
	sp[index].color = color;
}

void SplitWndBase::setSplitterPos(int index, int pos, bool proportional)
{
	if (index < 0 || index >= (int) sp.size()) return;
	if (proportional)
	{
		sp[index].state |= STATE_APPLY_PROPORTIONAL_POS;
		sp[index].propPos = pos;
	}
	changePosition(index, pos);
}

void SplitWndBase::setMargins(const MARGINS& margins)
{
	this->margins = margins;
}

void SplitWndBase::setFullDragMode(int mode)
{
	if (mode >= FULL_DRAG_DEFAULT && mode <= FULL_DRAG_DISABLED)
		fullDragMode = mode;
}

void SplitWndBase::getRootRect(RECT& rc) const
{
	rc.left = rcClient.left + margins.cxLeftWidth;
	rc.right = rcClient.right - margins.cxRightWidth;
	rc.top = rcClient.top + margins.cyTopHeight;
	rc.bottom = rcClient.bottom - margins.cyBottomHeight;
	if (rc.right <= rc.left || rc.bottom <= rc.top) clearRect(rc);
}

void SplitWndBase::getPaneRect(int pane, RECT& rc) const
{
	if (pane < 0)
	{
		getRootRect(rc);
		return;
	}
	int index = pane >> 1, which = pane & 1;
	rc = sp[index].panes[which].rc;
}

static void clampPos(int& pos, int minVal, int maxVal)
{
	if (pos < minVal)
		pos = minVal;
	else if (pos > maxVal)
		pos = maxVal;
}

void SplitWndBase::updatePosition(int index, int& newPos, RECT newRect[]) const
{
	const SplitterInfo& info = sp[index];
	int minSize[] = { 0, 0 };
	if (callback)
	{
		int pane = index << 1;
		minSize[0] = getMinPaneSize(pane);
		minSize[1] = getMinPaneSize(pane + 1);
	}
	if (newPos < 0) newPos = 0;
	RECT rc;
	getPaneRect(info.prev, rc);
	int size = (info.flags & FLAG_HORIZONTAL) ? rc.right - rc.left : rc.bottom - rc.top;
	if (size <= minSize[0] + minSize[1] + info.thickness)
	{
		clearRect(newRect[0]);
		clearRect(newRect[1]);
		return;
	}
	int newPos1, newPos2;
	if (info.flags & FLAG_ALIGN_OTHER)
	{
		clampPos(newPos, minSize[1], size - (minSize[0] + info.thickness));
		newPos2 = size - newPos;
		newPos1 = newPos2 - info.thickness;
	}
	else
	{
		clampPos(newPos, minSize[0], size - (minSize[1] + info.thickness));
		newPos1 = newPos;
		newPos2 = newPos1 + info.thickness;
	}
	newRect[0] = newRect[1] = rc;
	if (info.flags & FLAG_HORIZONTAL)
	{
		newRect[0].right = rc.left + newPos1;
		newRect[1].left = rc.left + newPos2;
	}
	else
	{
		newRect[0].bottom = rc.top + newPos1;
		newRect[1].top = rc.top + newPos2;
	}
}

bool SplitWndBase::setRect(PaneInfo& info, const RECT& rc)
{
	if (rc.left == info.rc.left && rc.right == info.rc.right && rc.top == info.rc.top && rc.bottom == info.rc.bottom)
		return false;
	if (rc.right - rc.left != info.rc.right - info.rc.left)
		info.flags |= FLAG_WIDTH_CHANGED;
	if (rc.bottom - rc.top != info.rc.bottom - info.rc.top)
		info.flags |= FLAG_HEIGHT_CHANGED;
	info.rc = rc;
	return true;
}

bool SplitWndBase::setRects(SplitterInfo& info, const RECT newRect[])
{
	info.panes[0].flags = info.panes[1].flags = 0;
	bool r1 = setRect(info.panes[0], newRect[0]);
	bool r2 = setRect(info.panes[1], newRect[1]);
	return r1 || r2;
}

bool SplitWndBase::changePosition(int index, int newPos)
{
	bool proportional = (sp[index].state & STATE_APPLY_PROPORTIONAL_POS) != 0;
	RECT newRect[2];
	if (proportional)
	{
		sp[index].state ^= STATE_APPLY_PROPORTIONAL_POS;
		clampPos(sp[index].propPos, 0, 10000);
		getPaneRect(sp[index].prev, newRect[0]);
		newPos = getProportionalPos(sp[index], newRect[0]);
	}
	updatePosition(index, newPos, newRect);
	if (!setRects(sp[index], newRect))
		return false;
	sp[index].position = newPos;
	if (!proportional && (sp[index].flags & FLAG_PROPORTIONAL))
		updateProportionalPos(sp[index]);
	sp[index].state |= STATE_POSITION_CHANGED;

	for (int i = index + 1; i < (int) sp.size(); ++i)
	{
		int prev1 = sp[i].prev >> 1;
		int prev2 = sp[i].prev & 1;
		bool forceUpdate = (sp[i].state & STATE_FORCE_UPDATE) != 0;
		if (!(sp[prev1].state & STATE_POSITION_CHANGED) && !forceUpdate) continue;
		uint16_t flags = sp[i].flags;
		newPos = sp[i].position;
		if (flags & FLAG_PROPORTIONAL)
		{
			uint16_t flag = (flags & FLAG_HORIZONTAL) ? FLAG_WIDTH_CHANGED : FLAG_HEIGHT_CHANGED;
			if ((sp[prev1].panes[prev2].flags & flag) || forceUpdate)
				newPos = getProportionalPos(sp[i], sp[prev1].panes[prev2].rc);
		}
		int savedPos = newPos;
		updatePosition(i, newPos, newRect);
		if (setRects(sp[i], newRect))
		{
			sp[i].position = newPos;
			if ((flags & FLAG_PROPORTIONAL) && savedPos != newPos)
				updateProportionalPos(sp[i]);
			sp[i].state |= STATE_POSITION_CHANGED;
		}
	}
	for (int i = index; i < (int) sp.size(); ++i)
	{
		if (sp[i].state & STATE_POSITION_CHANGED)
		{
			if (callback) callback->splitterMoved(i);
			paneRectUpdated(i, 0);
			paneRectUpdated(i, 1);
			sp[i].state ^= STATE_POSITION_CHANGED;
		}
		sp[i].state &= ~STATE_FORCE_UPDATE;
	}
	return true;
}

void SplitWndBase::paneRectUpdated(int index, int which)
{
	PaneInfo& pi = sp[index].panes[which];
	if (callback)
		callback->setPaneRect((index << 1) + which, pi.rc);
	if (pi.hWnd)
		::SetWindowPos(pi.hWnd, nullptr,
			pi.rc.left, pi.rc.top, pi.rc.right - pi.rc.left, pi.rc.bottom - pi.rc.top,
			SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
	pi.flags &= ~(FLAG_WIDTH_CHANGED | FLAG_HEIGHT_CHANGED);
}

void SplitWndBase::updateProportionalPos(SplitterInfo& info)
{
	dcassert(info.flags & FLAG_PROPORTIONAL);
	RECT rc;
	getPaneRect(info.prev, rc);
	int size = (info.flags & FLAG_HORIZONTAL) ? rc.right - rc.left : rc.bottom - rc.top;
	size -= info.thickness;
	if (size > 0)
		info.propPos = info.position * 10000 / size;
	else
		info.propPos = 0;
}

int SplitWndBase::getProportionalPos(const SplitterInfo& info, const RECT& rc) const
{
	int size = (info.flags & FLAG_HORIZONTAL) ? rc.right - rc.left : rc.bottom - rc.top;
	size -= info.thickness;
	if (size <= 0) return 0;
	return info.propPos * size / 10000;
}

void SplitWndBase::getSplitterBarRect(int index, RECT& rc) const
{
	if (sp[index].flags & FLAG_HORIZONTAL)
	{
		rc.left = sp[index].panes[0].rc.right;
		rc.right = sp[index].panes[1].rc.left;
		rc.top = sp[index].panes[0].rc.top;
		rc.bottom = sp[index].panes[0].rc.bottom;
	}
	else
	{
		rc.top = sp[index].panes[0].rc.bottom;
		rc.bottom = sp[index].panes[1].rc.top;
		rc.left = sp[index].panes[0].rc.left;
		rc.right = sp[index].panes[0].rc.right;
	}
}

void SplitWndBase::getSplitterBarRect(int index, int pos, RECT& rc) const
{
	RECT newRect[2];
	updatePosition(index, pos, newRect);
	if (sp[index].flags & FLAG_HORIZONTAL)
	{
		rc.left = newRect[0].right;
		rc.right = newRect[1].left;
		rc.top = newRect[0].top;
		rc.bottom = newRect[0].bottom;
	}
	else
	{
		rc.top = newRect[0].bottom;
		rc.bottom = newRect[1].top;
		rc.left = newRect[0].left;
		rc.right = newRect[0].right;
	}
}

int SplitWndBase::findSplitterBar(POINT pt) const
{
	RECT rc;
	for (int i = 0; i < (int) sp.size(); ++i)
	{
		getSplitterBarRect(i, rc);
		if (PtInRect(&rc, pt))
		{
			if (sp[i].flags & FLAG_INTERACTIVE) return i;
			break;
		}
	}
	return -1;
}

void SplitWndBase::updateHotIndex(POINT pt)
{
	hotIndex = singlePane == -1 ? findSplitterBar(pt) : -1;
	HCURSOR hCursor;
	if (hotIndex != -1)
		hCursor = LoadCursor(nullptr, (sp[hotIndex].flags & FLAG_HORIZONTAL) ? IDC_SIZEWE : IDC_SIZENS);
	else
		hCursor = LoadCursor(nullptr, IDC_ARROW);
	SetCursor(hCursor);
}

void SplitWndBase::drawFrame(HDC hdc) const
{
	HBRUSH hBrush = GetSysColorBrush(COLOR_BTNFACE);
	RECT rc;
	if (margins.cxLeftWidth)
	{
		rc.left = 0;
		rc.top = 0;
		rc.right = margins.cxLeftWidth;
		rc.bottom = rcClient.bottom;
		FillRect(hdc, &rc, hBrush);
	}
	if (margins.cxRightWidth)
	{
		rc.left = rcClient.right - margins.cxRightWidth;
		rc.top = 0;
		rc.right = rcClient.right;
		rc.bottom = rcClient.bottom;
		FillRect(hdc, &rc, hBrush);
	}
	if (margins.cyTopHeight)
	{
		rc.left = margins.cxLeftWidth;
		rc.top = 0;
		rc.right = rcClient.right - margins.cxRightWidth;
		rc.bottom = margins.cyTopHeight;
		FillRect(hdc, &rc, hBrush);
	}
	if (margins.cyBottomHeight)
	{
		rc.left = margins.cxLeftWidth;
		rc.top = rcClient.bottom - margins.cyBottomHeight;
		rc.right = rcClient.right - margins.cxRightWidth;
		rc.bottom = rcClient.bottom;
		FillRect(hdc, &rc, hBrush);
	}
}

void SplitWndBase::drawGhostBar(HWND hWnd)
{
	if (!halftoneBrush)
	{
		halftoneBrush = createHalftoneBrush();
		if (!halftoneBrush) return;
	}

	RECT rc, rcWnd;
	getSplitterBarRect(hotIndex, ghostBarPos, rc);
	GetWindowRect(hWnd, &rcWnd);
	MapWindowPoints(nullptr, hWnd, (POINT *) &rcWnd, 2);
	OffsetRect(&rc, -rcWnd.left, -rcWnd.top);

	HDC hdc = GetWindowDC(hWnd);
	HGDIOBJ oldBrush = SelectObject(hdc, halftoneBrush);
	PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATINVERT);
	SelectObject(hdc, oldBrush);
	ReleaseDC(hWnd, hdc);
}

void SplitWndBase::draw(HDC hdc)
{
	if (options & OPT_PAINT_MARGINS) drawFrame(hdc);
	RECT rc;
	for (int i = 0; i < (int) sp.size(); ++i)
		if (sp[i].colorType != COLOR_TYPE_TRANSPARENT && sp[i].thickness > 0)
		{
			getSplitterBarRect(i, rc);
			HBRUSH hBrush;
			if (sp[i].colorType == COLOR_TYPE_RGB)
				hBrush = CreateSolidBrush(sp[i].color);
			else
				hBrush = GetSysColorBrush(sp[i].color);
			FillRect(hdc, &rc, hBrush);
			if (sp[i].colorType == COLOR_TYPE_RGB)
				DeleteObject(hBrush);
		}
}

int SplitWndBase::getMinPaneSize(int pane) const
{
	if (!callback) return 0;
	int size = callback->getMinPaneSize(pane);
	if (size == Callback::SIZE_USE_MINMAX_MSG)
	{
		int index = pane >> 1, which = pane & 1;
		HWND hWnd = sp[index].panes[which].hWnd;
		if (hWnd)
		{
			MINMAXINFO info;
			memset(&info, 0, sizeof(info));
			SendMessage(hWnd, WM_GETMINMAXINFO, 0, (LPARAM) &info);
			return (sp[index].flags & FLAG_HORIZONTAL) ? info.ptMinTrackSize.x : info.ptMinTrackSize.y;
		}
	}
	if (size < 0) size = 0;
	return size;
}

void SplitWndBase::redraw(HWND hWnd, int index)
{
	RECT rc;
	getPaneRect(sp[index].prev, rc);
	InvalidateRect(hWnd, &rc, TRUE);
	UpdateWindow(hWnd);
}

bool SplitWndBase::handleButtonPress(HWND hWnd, POINT pt)
{
	updateClientRect(hWnd);
	updateHotIndex(pt);
	if (hotIndex == -1) return false;
	if (fullDragMode == FULL_DRAG_DEFAULT)
	{
		BOOL flag;
		SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0, &flag, 0);
		fullDragMode = flag ? FULL_DRAG_ENABLED : FULL_DRAG_DISABLED;
	}
	if (fullDragMode == FULL_DRAG_DISABLED)
	{
		ghostBarPos = sp[hotIndex].position;
		drawGhostBar(hWnd);
	}
	sp[hotIndex].state |= STATE_DRAGGING;
	prevPos = pt;
	SetCapture(hWnd);
	return true;
}

void SplitWndBase::handleButtonRelease(HWND hWnd)
{
	if (hotIndex == -1) return;
	sp[hotIndex].state &= ~STATE_DRAGGING;
	if (fullDragMode == FULL_DRAG_DISABLED)
	{
		drawGhostBar(hWnd);
		if (changePosition(hotIndex, ghostBarPos)) redraw(hWnd, hotIndex);
		ghostBarPos = INT_MIN;
	}
	hotIndex = -1;
	ReleaseCapture();
}

void SplitWndBase::handleMouseMove(HWND hWnd, POINT pt)
{
	updateClientRect(hWnd);
	if (hotIndex != -1 && (sp[hotIndex].state & STATE_DRAGGING))
	{
		int delta;
		if (sp[hotIndex].flags & FLAG_HORIZONTAL)
			delta = pt.x - prevPos.x;
		else
			delta = pt.y - prevPos.y;
		if (sp[hotIndex].flags & FLAG_ALIGN_OTHER)
			delta = -delta;
		if (fullDragMode == FULL_DRAG_DISABLED)
		{
			int newPos = ghostBarPos + delta;
			RECT newRect[2];
			updatePosition(hotIndex, newPos, newRect);
			if (newPos != ghostBarPos)
			{
				prevPos = pt;
				drawGhostBar(hWnd);
				ghostBarPos = newPos;
				drawGhostBar(hWnd);
			}
			return;
		}
		if (changePosition(hotIndex, sp[hotIndex].position + delta))
		{
			prevPos = pt;
			redraw(hWnd, hotIndex);
		}
		return;
	}
	updateHotIndex(pt);
}

void SplitWndBase::resetCapture(HWND hWnd)
{
	if (hotIndex == -1) return;
	if (sp[hotIndex].state & STATE_DRAGGING)
	{
		sp[hotIndex].state ^= STATE_DRAGGING;
		if (fullDragMode == FULL_DRAG_DISABLED)
		{
			drawGhostBar(hWnd);
			ghostBarPos = INT_MIN;
		}
	}
	hotIndex = -1;
}

int SplitWndBase::getDefaultThickness(uint16_t flags)
{
	return GetSystemMetrics((flags & FLAG_HORIZONTAL) ? SM_CXSIZEFRAME : SM_CYSIZEFRAME);
}

int SplitWndBase::getSplitterPos(int index, bool proportional) const
{
	if (index >= 0 && index < (int) sp.size())
		return proportional ? sp[index].propPos : sp[index].position;
	return -1;
}

void SplitWndBase::updateClientRect(HWND hWnd)
{
	GetClientRect(hWnd, &rcClient);
}
