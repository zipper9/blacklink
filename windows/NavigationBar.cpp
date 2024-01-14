#include "stdafx.h"
#include "NavigationBar.h"
#include "BackingStore.h"
#include "GdiUtil.h"
#include "WinUtil.h"
#include <vsstyle.h>
#include <vssym32.h>

#undef min
#undef max

using std::vector;

enum
{
	SBP_ABBACKGROUND = 1,
	SBB_NORMAL       = 1,
	SBB_HOT          = 2,
	SBB_DISABLED     = 3,
	SBB_FOCUSED      = 4
};

enum
{
	RESOURCE_ARROWS   = 1,
	RESOURCE_CHEVRON  = 2,
	RESOURCE_DROPDOWN = 4
};

static const unsigned BUTTON_DROPDOWN = 1;

static const int HEIGHT_EXTRA = 8; // dialog units
static const int POPUP_DATA_CHEVRON = -16;
static const int IMAGE_LIST_ICON_SIZE = 16;
static const int TIMER_UPDATE_ANIMATION = 5;
static const int TIMER_CLEANUP = 6;
static const int CLEANUP_TIME = 15000;

static void drawMonoBitmap(HDC hdc, HBITMAP hBitmap, const RECT& rc, COLORREF color);

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

template<typename T>
void clampValue(T& val, T minVal, T maxVal)
{
	if (val < minVal)
		val = minVal;
	else if (val > maxVal)
		val = maxVal;
}

static inline bool operator== (const RECT& r1, const RECT& r2)
{
	return r1.left == r2.left && r1.right == r2.right && r1.top == r2.top && r1.bottom == r2.bottom;
}

LRESULT NavBarEditBox::onChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
#ifdef DEBUG_NAV_BAR
	ATLTRACE("NavBarEditBox: onChar 0x%X\n", wParam);
#endif
	if (wParam == VK_ESCAPE)
	{
		::SendMessage(notifWnd, WMU_EXIT_EDIT_MODE, 0, 0);
		return 0;
	}
	if (wParam == VK_RETURN)
	{
		::SendMessage(notifWnd, WMU_EXIT_EDIT_MODE, 1, 0);
		return 0;
	}
	bHandled = FALSE;
	return 1;
}

NavigationBar::NavigationBar()
{
	flags = 0;
	iconSize = 0;
	buttonSize = 24;
	glyphSize = 16;
	paddingLeft = paddingRight = iconPaddingLeft = iconPaddingRight = 0;
	iconBitmapWidth = iconBitmapHeight = 0;
	hotIndex = popupIndex = -1;
	hotType = pressedType = HT_EMPTY;
	margins.cxLeftWidth = margins.cxRightWidth = margins.cyTopHeight = margins.cyBottomHeight = 1;
	hFont = nullptr;
	iconBmp = nullptr;
	bmpThemedDropDown = nullptr;
	memset(bmpNonThemed, 0, sizeof(bmpNonThemed));
	callback = nullptr;
	backingStore = nullptr;
	historyState = 0;
	animationEnabled = true;
	animationDuration = -1;

	int dpi = WinUtil::getDisplayDpi();
	minItemWidth = 55 * dpi / 96;
	popupPosOffset = 34 * dpi / 96;
	comboDropDownHeight = 160 * dpi / 96;

	chevron.data = 0;
	chevron.autoWidth = chevron.width = glyphSize;
	chevron.xpos = 0;
	chevron.flags = BF_CHEVRON;
	chevron.currentState = -1;
	chevron.trans = nullptr;

	addToolbarButton(BUTTON_DROPDOWN);
}

NavigationBar::~NavigationBar()
{
	cleanup();
	if (backingStore) backingStore->release();
}

int NavigationBar::getPrefHeight(HDC hdc, HFONT hFont)
{
	int xdu, ydu;
	HGDIOBJ oldFont = SelectObject(hdc, hFont);
	WinUtil::getDialogUnits(hdc, xdu, ydu);
	SelectObject(hdc, oldFont);
	return WinUtil::dialogUnitsToPixelsY(HEIGHT_EXTRA, ydu) + ydu;
}

void NavigationBar::clampItemWidth(int& width, int autoWidth) const
{
	if (width > autoWidth)
		width = autoWidth;
	else
	{
		int minWidth = std::min(minItemWidth, autoWidth);
		if (width < minWidth)
			width = minWidth;
	}
}

void NavigationBar::layout(const RECT& rc)
{
	flags &= ~FLAG_WANT_LAYOUT;
	int xpos = rc.left;
	int width = rc.right - rc.left;
	int buttonsWidth = buttonSize * (int) buttons.size();
	width -= buttonsWidth;
	if (flags & FLAG_HAS_ICON)
	{
		width -= iconSize;
		xpos += iconSize;
	}
	width -= chevron.width;
	if (width < 0)
	{
		for (Item& item : crumbs)
			item.width = 0;
		for (Item& item : buttons)
			item.width = 0;
		flags |= FLAG_NO_ROOM;
		cancelStateTransitions(true);
		return;
	}

	RECT rcButton = rc;
	flags &= ~FLAG_NO_ROOM;
	int visibleCount = 0;
	if (!crumbs.empty())
	{
		for (Item& item : crumbs)
			item.width = 0;
		int totalSize = 0;
		int numItems = crumbs.size();
		for (int i = numItems-1; i >= 0; --i)
		{
			totalSize += crumbs[i].autoWidth;
			if (totalSize > width) break;
			crumbs[i].width = crumbs[i].autoWidth;
			visibleCount++;
		}
		if (visibleCount < 2)
		{
			visibleCount = 1;
			int width0 = width;
			Item& b0 = crumbs[numItems-1];
			if (crumbs.size() > 1)
			{
				Item& b1 = crumbs[numItems-2];
				int width1 = width - b0.autoWidth;
				clampItemWidth(width1, b1.autoWidth);
				width0 -= width1;
				clampItemWidth(width0, b0.autoWidth);
				if (width0 + width1 > width)
				{
					width0 = width;
					width1 = 0;
				}
				else
					visibleCount = 2;
				b1.width = width1;
			}
			if (width0 > b0.autoWidth)
				width0 = b0.autoWidth;
			else
			{
				int minWidth = std::min(minItemWidth, b0.autoWidth);
				if (width0 < minWidth)
				{
					width0 = 0;
					flags |= FLAG_NO_ROOM;
				}
			}
			b0.width = width0;
		}
		if (visibleCount < numItems)
		{
			flags |= FLAG_HAS_CHEVRON;
			xpos += chevron.width;
		}
		else
			flags &= ~FLAG_HAS_CHEVRON;
		for (Item& item : crumbs)
		{
			item.xpos = xpos;
			if (item.trans)
			{
				rcButton.left = xpos;
				rcButton.right = xpos + item.width;
				if (!(rcButton == item.trans->rc))
					removeStateTransition(item);
			}
			xpos += item.width;
		}
	}
	if (!buttons.empty())
	{
		xpos = rc.right - buttonsWidth;
		for (Item& item : buttons)
		{
			item.width = buttonSize;
			item.xpos = xpos;
			if (item.trans)
			{
				rcButton.left = xpos;
				rcButton.right = xpos + item.width;
				if (!(rcButton == item.trans->rc))
					removeStateTransition(item);
			}
			xpos += buttonSize;
		}
	}
	if (!(flags & FLAG_HAS_CHEVRON))
		removeStateTransition(chevron);
	//ATLTRACE("visibleCount: %d\n", visibleCount);
}

int NavigationBar::hitTest(POINT pt, int& index) const
{
	CRect rc;
	GetClientRect(&rc);
	index = -1;
	if (!rc.PtInRect(pt)) return HT_OUTSIDE;
	int xpos = 0;
	if (flags & FLAG_HAS_ICON)
	{
		xpos += iconSize;
		if (pt.x < xpos) return HT_ICON;
	}
	if (flags & FLAG_HAS_CHEVRON)
	{
		xpos += chevron.width;
		if (pt.x < xpos) return HT_CHEVRON;
	}
	int numItems = crumbs.size();
	for (int i = 0; i < numItems; ++i)
	{
		const Item& item = crumbs[i];
		if (pt.x >= item.xpos && pt.x < item.xpos + item.width)
		{
			index = i;
			if ((item.flags & BF_ARROW) && pt.x >= item.xpos + item.width - glyphSize) return HT_ITEM_DD;
			return HT_ITEM;
		}
	}
	int numButtons = buttons.size();
	for (int i = 0; i < numButtons; ++i)
	{
		const Item& item= buttons[i];
		if (pt.x >= item.xpos && pt.x < item.xpos + item.width)
		{
			index = i;
			return HT_BUTTON;
		}
	}
	return HT_EMPTY;
}

void NavigationBar::updateAutoWidth(HDC hdc)
{
	HGDIOBJ oldFont = SelectObject(hdc, hFont);
	for (Item& item : crumbs)
		if (item.autoWidth < 0)
		{
			if (!item.text.empty())
			{
				CRect rc(0, 0, 0, 0);
				DrawText(hdc, item.text.c_str(), item.text.length(), &rc, DT_SINGLELINE | DT_CALCRECT);
				item.autoWidth = rc.Width() + paddingLeft + paddingRight;
				if (item.flags & BF_ARROW) item.autoWidth += glyphSize;
			}
			else
				item.autoWidth = glyphSize;
			flags |= FLAG_WANT_LAYOUT;
		}
	SelectObject(hdc, oldFont);
	flags &= ~FLAG_UPDATE_WIDTH;
}

void NavigationBar::updateIconSize()
{
	iconBitmapWidth = iconBitmapHeight = 0;
	BITMAP bitmap;
	if (iconBmp && GetObject(iconBmp, sizeof(bitmap), &bitmap))
	{
		iconBitmapWidth = bitmap.bmWidth;
		iconBitmapHeight = abs(bitmap.bmHeight);
	}
	iconSize = iconBitmapWidth + iconPaddingLeft + iconPaddingRight;
}

void NavigationBar::cleanup()
{
	clearFont();
	for (int i = 0; i < _countof(bmpNonThemed); ++i)
		if (bmpNonThemed[i])
		{
			DeleteObject(bmpNonThemed[i]);
			bmpNonThemed[i] = nullptr;
		}
	if (bmpThemedDropDown)
	{
		DeleteObject(bmpThemedDropDown);
		bmpThemedDropDown = nullptr;
	}
	removeStateTransitions(crumbs);
	removeStateTransitions(buttons);
	removeStateTransition(chevron);
}

void NavigationBar::clearFont()
{
	if (hFont && (flags & FLAG_OWN_FONT))
	{
		DeleteObject(hFont);
		hFont = nullptr;
	}
	flags &= ~FLAG_OWN_FONT;
}

void NavigationBar::createFont()
{
	dcassert(!hFont);
	NONCLIENTMETRICS ncm = { offsetof(NONCLIENTMETRICS, lfMessageFont) + sizeof(NONCLIENTMETRICS::lfMessageFont) };
	LOGFONT lf;
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		memcpy(&lf, &ncm.lfMessageFont, sizeof(LOGFONT));
	else
		GetObject((HFONT) GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
	hFont = CreateFontIndirect(&lf);
	flags |= FLAG_OWN_FONT;
}

void NavigationBar::setFont(HFONT hFont, bool ownFont)
{
	clearFont();
	this->hFont = hFont;
	if (ownFont) flags |= FLAG_OWN_FONT;
	flags |= FLAG_UPDATE_WIDTH;
}

void NavigationBar::drawBackground(HDC hdc, const RECT& rc)
{
	if (themeAddressBar.hTheme)
	{
		FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));
		int stateId = (flags & (FLAG_UNDER_MOUSE | FLAG_POPUP_VISIBLE)) ? SBB_FOCUSED : SBB_NORMAL;
		themeAddressBar.pDrawThemeParentBackground(m_hWnd, hdc, &rc);
		themeAddressBar.pDrawThemeBackground(themeAddressBar.hTheme, hdc, SBP_ABBACKGROUND, stateId, &rc, nullptr);
	}
	else
	{
		FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));
		//DrawEdge(hdc, &rc, BDR_SUNKENOUTER, BF_RECT);
		WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNSHADOW), GetSysColorBrush(COLOR_BTNHILIGHT));
	}
}

int NavigationBar::getButtonState(int flags) const
{
	int stateId = TS_NORMAL;
	if (flags & BF_PRESSED)
		stateId = TS_PRESSED;
	else if (flags & BF_HOT)
	{
		stateId = TS_HOT;
		if ((flags & (BF_TEXT | BF_PUSH)) == BF_TEXT && hotType == HT_ITEM_DD)
			stateId = TS_OTHERSIDEHOT;
	}
	return stateId;
}

void NavigationBar::drawThemeButton(HDC hdc, const RECT& rcButton, const Item& item, int stateId)
{
	RECT rc = rcButton;
	if (item.flags & BF_CHEVRON)
	{
		if (themeBreadcrumbs.hTheme)
			themeBreadcrumbs.pDrawThemeBackground(themeBreadcrumbs.hTheme, hdc, 1, stateId, &rc, nullptr);
		return;
	}
	if (item.flags & BF_PUSH)
	{
		themeToolbar.pDrawThemeBackground(themeToolbar.hTheme, hdc, TP_BUTTON, stateId, &rc, nullptr);
		if (item.data == BUTTON_DROPDOWN)
		{
			if (flags & FLAG_INIT_DD_BITMAP)
			{
				flags ^= FLAG_INIT_DD_BITMAP;
				if (bmpThemedDropDown) DeleteObject(bmpThemedDropDown);
				bmpThemedDropDown = getThemeDropDownBitmap();
				if (!bmpThemedDropDown) createNonThemeResources(RESOURCE_DROPDOWN);
			}
			HBITMAP bitmap = bmpThemedDropDown ? bmpThemedDropDown : bmpNonThemed[BNT_DROPDOWN];
			BITMAP bm;
			if (bitmap && GetObject(bitmap, sizeof(bm), &bm))
			{
				int width = bm.bmWidth;
				int height = abs(bm.bmHeight);
				if (bm.bmBitsPixel != 1) height /= 4;
				int x = rc.left + (rc.right - rc.left - width) / 2;
				int y = rc.top + (rc.bottom - rc.top - height) / 2;
				if (bm.bmBitsPixel == 1)
					WinUtil::drawMonoBitmap(hdc, bitmap, x, y, width, height, GetSysColor(COLOR_BTNTEXT));
				else
					WinUtil::drawAlphaBitmap(hdc, bitmap, x, y, width, height);
			}
		}
	}
	else if (item.flags & BF_TEXT)
	{
		int partId = TP_BUTTON;
		if (item.flags & BF_ARROW)
		{
			rc.right -= glyphSize;
			partId = TP_SPLITBUTTON;
		}
		themeToolbar.pDrawThemeBackground(themeToolbar.hTheme, hdc, partId, stateId, &rc, nullptr);
		HGDIOBJ oldFont = SelectObject(hdc, hFont);
		RECT rcText = rc;
		rcText.left += paddingLeft;
		rcText.right -= paddingRight;
		if (item.flags & BF_PRESSED)
		{
			int xoffset = GetSystemMetrics(SM_CXBORDER);
			int yoffset = GetSystemMetrics(SM_CYBORDER);
			rcText.left += xoffset;
			rcText.top += yoffset;
			rcText.right += xoffset;
			rcText.bottom += yoffset;
		}
		themeToolbar.pDrawThemeText(themeToolbar.hTheme, hdc, partId, stateId, item.text.c_str(), item.text.length(),
			DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS, 0, &rcText);
		SelectObject(hdc, oldFont);
		if (item.flags & BF_ARROW)
		{
			rc.left = rc.right;
			rc.right = rcButton.right;
		}
	}
	if (item.flags & BF_ARROW)
	{
		if (stateId == TS_OTHERSIDEHOT) stateId = TS_HOT;
		themeToolbar.pDrawThemeBackground(themeToolbar.hTheme, hdc, TP_SPLITBUTTONDROPDOWN, stateId, &rc, nullptr);
	}
}

void NavigationBar::drawButton(HDC hdc, const RECT& rcClient, Item& item)
{
	RECT rc;
	rc.top = rcClient.top;
	rc.bottom = rcClient.bottom;
	rc.left = item.xpos;
	rc.right = rc.left + item.width;
	int stateId = getButtonState(item.flags);
	if (themeToolbar.hTheme)
	{
		if (item.currentState == -1)
			item.currentState = stateId;
		else
			updateStateTransition(item, stateId);
		if (item.trans && item.trans->running && item.trans->currentAlpha != -1)
			item.trans->draw(hdc);
		else
			drawThemeButton(hdc, rc, item, stateId);
	}
	else
	{
		if (stateId == TS_OTHERSIDEHOT) stateId = TS_HOT;
		COLORREF textColor = GetSysColor(COLOR_BTNTEXT);
		int rightPos = rc.right;
		if (item.flags & BF_PUSH)
		{
			if (stateId == TS_PRESSED)
				WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNSHADOW), GetSysColorBrush(COLOR_BTNHILIGHT));
			else if (stateId == TS_HOT)
				WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNHILIGHT), GetSysColorBrush(COLOR_BTNSHADOW));
			if (item.data == BUTTON_DROPDOWN)
			{
				createNonThemeResources(RESOURCE_DROPDOWN);
				RECT rcImage = rc;
				if (item.flags & BF_PRESSED)
				{
					int xoffset = GetSystemMetrics(SM_CXBORDER);
					int yoffset = GetSystemMetrics(SM_CYBORDER);
					rcImage.left += xoffset;
					rcImage.top += yoffset;
					rcImage.right += xoffset;
					rcImage.bottom += yoffset;
				}
				drawMonoBitmap(hdc, bmpNonThemed[BNT_DROPDOWN], rcImage, textColor);
			}
		}
		else if (item.flags & BF_TEXT)
		{
			if (item.flags & BF_ARROW)
				rc.right -= glyphSize;
			//if (stateId != TS_NORMAL)
			//	DrawEdge(hdc, &rc, stateId == TS_PRESSED ? BDR_SUNKENINNER : BDR_RAISEDINNER, BF_RECT);
			if (stateId == TS_PRESSED)
				WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNSHADOW), GetSysColorBrush(COLOR_BTNHILIGHT));
			else if (stateId == TS_HOT) // Note: mode 0 is probably better here...
				WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNHILIGHT), GetSysColorBrush(COLOR_BTNSHADOW));
			HGDIOBJ oldFont = SelectObject(hdc, hFont);
			SetTextColor(hdc, textColor);
			SetBkMode(hdc, TRANSPARENT);
			RECT rcText = rc;
			rcText.left += paddingLeft;
			rcText.right -= paddingRight;
			if (item.flags & BF_PRESSED)
			{
				int xoffset = GetSystemMetrics(SM_CXBORDER);
				int yoffset = GetSystemMetrics(SM_CYBORDER);
				rcText.left += xoffset;
				rcText.top += yoffset;
				rcText.right += xoffset;
				rcText.bottom += yoffset;
			}
			DrawText(hdc, item.text.c_str(), item.text.length(), &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
			SelectObject(hdc, oldFont);
			if (item.flags & BF_ARROW)
			{
				rc.left = rc.right;
				rc.right = rightPos;
			}
		}
		if (item.flags & BF_ARROW)
		{
			createNonThemeResources(RESOURCE_ARROWS);
			//if (stateId != TS_NORMAL)
			//	DrawEdge(hdc, &rc, stateId == TS_PRESSED ? BDR_SUNKENINNER : BDR_RAISEDINNER, BF_RECT);
			if (stateId == TS_PRESSED)
				WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNSHADOW), GetSysColorBrush(COLOR_BTNHILIGHT));
			else if (stateId == TS_HOT)
				WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNHILIGHT), GetSysColorBrush(COLOR_BTNSHADOW));
			drawMonoBitmap(hdc, bmpNonThemed[stateId == TS_PRESSED ? BNT_V_ARROW : BNT_H_ARROW], rc, textColor);
		}
	}
}

void NavigationBar::drawChevron(HDC hdc, const RECT& rcClient)
{
	RECT rc = rcClient;
	if (flags & FLAG_HAS_ICON) rc.left += iconSize;
	rc.right = rc.left + chevron.width;
	chevron.xpos = rc.left;
	int stateId = TS_NORMAL;
	if (hotType == HT_CHEVRON)
	{
		if (pressedType == HT_CHEVRON)
			stateId = TS_PRESSED;
		else
			stateId = TS_HOT;
	}
	if (themeBreadcrumbs.hTheme)
	{
		if (chevron.currentState == -1)
			chevron.currentState = stateId;
		else
			updateStateTransition(chevron, stateId);
		if (chevron.trans && chevron.trans->running && chevron.trans->currentAlpha != -1)
			chevron.trans->draw(hdc);
		else
			drawThemeButton(hdc, rc, chevron, stateId);
	}
	else
	{
		createNonThemeResources(RESOURCE_CHEVRON);
		if (stateId == TS_PRESSED)
			WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNSHADOW), GetSysColorBrush(COLOR_BTNHILIGHT));
		else if (stateId == TS_HOT)
			WinUtil::drawEdge(hdc, rc, 1, GetSysColorBrush(COLOR_BTNHILIGHT), GetSysColorBrush(COLOR_BTNSHADOW));
		drawMonoBitmap(hdc, bmpNonThemed[BNT_CHEVRON], rc, GetSysColor(COLOR_BTNTEXT));
	}
}

void NavigationBar::drawIcon(HDC hdc, const RECT& rcClient)
{
	dcassert(flags & FLAG_HAS_ICON);
	int x = rcClient.left + iconPaddingLeft;
	int y = rcClient.top + (rcClient.bottom - rcClient.top - iconBitmapHeight) / 2;
	if (pressedType == HT_ICON || pressedType == HT_CHEVRON ||
	   (pressedType == HT_ITEM_DD && !hotIndex && !(crumbs[0].flags & BF_TEXT)))
	{
		x += GetSystemMetrics(SM_CXBORDER);
		y += GetSystemMetrics(SM_CYBORDER);
	}
	WinUtil::drawAlphaBitmap(hdc, iconBmp, x, y, iconBitmapWidth, iconBitmapHeight);
}

void NavigationBar::draw(HDC hdc, const RECT& rcClient)
{
	drawBackground(hdc, rcClient);

	RECT rc = { rcClient.left + margins.cxLeftWidth, rcClient.top + margins.cyTopHeight, rcClient.right - margins.cxRightWidth, rcClient.bottom - margins.cyBottomHeight };

	if (flags & FLAG_UPDATE_WIDTH) updateAutoWidth(hdc);
	if (flags & FLAG_WANT_LAYOUT) layout(rc);

	if (flags & FLAG_HAS_ICON) drawIcon(hdc, rc);
	if (flags & FLAG_HAS_CHEVRON) drawChevron(hdc, rc);

	for (Item& item : crumbs)
		if (item.width > 0) drawButton(hdc, rc, item);
	for (Item& item : buttons)
		if (item.width > 0) drawButton(hdc, rc, item);
}

void NavigationBar::addTextItem(const tstring& text, uintptr_t data, bool hasArrow)
{
	Item item;
	item.data = data;
	item.autoWidth = -1;
	item.xpos = 0;
	item.width = 0;
	item.flags = hasArrow ? BF_ARROW : 0;
	item.currentState = -1;
	item.trans = nullptr;
	if (!text.empty())
	{
		item.text = text;
		item.flags |= BF_TEXT;
		flags |= FLAG_UPDATE_WIDTH;
	}
	if (!(item.flags & (BF_TEXT | BF_ARROW)))
		item.flags |= BF_ARROW;
	crumbs.push_back(item);
	flags |= FLAG_WANT_LAYOUT;
}

void NavigationBar::addArrowItem(uintptr_t data)
{
	Item item;
	item.data = data;
	item.autoWidth = -1;
	item.xpos = 0;
	item.width = 0;
	item.flags = BF_ARROW;
	item.currentState = -1;
	item.trans = nullptr;
	crumbs.push_back(item);
	flags |= FLAG_WANT_LAYOUT;
}

void NavigationBar::removeAllItems()
{
	if (!crumbs.empty())
	{
		removeStateTransitions(crumbs);
		crumbs.clear();
		flags |= FLAG_WANT_LAYOUT;
		hotIndex = -1;
		hotType = pressedType = HT_EMPTY;
	}
}

void NavigationBar::addToolbarButton(uintptr_t data)
{
	Item item;
	item.autoWidth = -1;
	item.xpos = 0;
	item.width = 0;
	item.flags = BF_PUSH;
	item.data = data;
	item.currentState = -1;
	item.trans = nullptr;
	buttons.push_back(item);
	flags |= FLAG_WANT_LAYOUT;
}

void NavigationBar::setIcon(HBITMAP hBitmap)
{
	iconBmp = hBitmap;
	if (!iconBmp)
	{
		if (flags & FLAG_HAS_ICON) flags = (flags ^ FLAG_HAS_ICON) | FLAG_WANT_LAYOUT;
		iconBitmapWidth = iconBitmapHeight = iconSize = 0;
		return;
	}
	updateIconSize();
	flags |= FLAG_HAS_ICON | FLAG_WANT_LAYOUT;
}

void NavigationBar::setEditMode(bool enable)
{
	if (enable)
	{
		if (!(flags & FLAG_EDIT_MODE)) enterEditMode(false);
	}
	else
	{
		if (flags & FLAG_EDIT_MODE) exitEditMode();
	}
}

void NavigationBar::trackMouseEvent()
{
	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(tme);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = m_hWnd;
	tme.dwHoverTime = 0;
	TrackMouseEvent(&tme);
	flags |= FLAG_MOUSE_TRACKING;
}

LRESULT NavigationBar::onCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (!hFont) createFont();
	if (animationEnabled && !WinUtil::hasFastBlend()) animationEnabled = false;
	return 0;
}

LRESULT NavigationBar::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	if (popup.m_hWnd) popup.DestroyWindow();
	if (comboBox.m_hWnd) comboBox.DestroyWindow();
	imageList.destroy();
	cleanup();
	cancelStateTransitions(true);
	themeToolbar.closeTheme();
	themeAddressBar.closeTheme();
	themeBreadcrumbs.closeTheme();
	return 0;
}

void NavigationBar::initTheme()
{
#ifndef DISABLE_THEME
	if (themeToolbar.hTheme && themeAddressBar.hTheme) return;
#if 0
	OSVERSIONINFOEX ver;
	memset(&ver, 0, sizeof(OSVERSIONINFOEX));
	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((OSVERSIONINFO*) &ver);
	if (ver.dwMajorVersion >= 6)
#endif
	{
		themeToolbar.openTheme(m_hWnd, L"BBComposited::Toolbar"); // BB / BBComposited
		//themeToolbar.openTheme(m_hWnd, L"Toolbar");
		themeAddressBar.openTheme(m_hWnd, L"AB::AddressBand"); // AB / ABComposited
		themeBreadcrumbs.openTheme(m_hWnd, L"BreadcrumbBar");
	}
#if 0
	else
	{
		themeToolbar.openTheme(m_hWnd, L"Toolbar");
		themeAddressBar.openTheme(m_hWnd, L"Toolbar");
		themeBreadcrumbs.closeTheme();
	}
#endif
#endif
	flags |= FLAG_INIT_METRICS | FLAG_INIT_DD_BITMAP;
}

void NavigationBar::initMetrics(HDC hdc)
{
	buttonSize = GetSystemMetrics(SM_CXVSCROLL);
	glyphSize = GetSystemMetrics(SM_CYMENUCHECK);
	paddingLeft = paddingRight = iconPaddingLeft = iconPaddingRight = GetSystemMetrics(SM_CXEDGE);
	if (flags & FLAG_HAS_ICON) updateIconSize();
	if (themeToolbar.hTheme)
	{
		static const int WIDTH = 100;
		static const int HEIGHT = 80;
		RECT rc = { 0, 0, WIDTH, HEIGHT };
		if (SUCCEEDED(themeToolbar.pGetThemeBackgroundContentRect(themeToolbar.hTheme, hdc, TP_SPLITBUTTON, TS_NORMAL, &rc, &rc)))
		{
			paddingLeft += rc.left;
			paddingRight += WIDTH - rc.right;
		}
	}
	else
	{
		paddingLeft += 2;
		paddingRight += 2;
	}
	if (themeAddressBar.hTheme)
	{
		static const int WIDTH = 100;
		static const int HEIGHT = 80;
		RECT rc = { 0, 0, WIDTH, HEIGHT };
		//themeAddressBar.GetThemeMargins();
		if (SUCCEEDED(themeAddressBar.pGetThemeBackgroundContentRect(themeAddressBar.hTheme, hdc, SBP_ABBACKGROUND, SBB_FOCUSED, &rc, &rc)))
		{
			margins.cxLeftWidth = rc.left;
			margins.cxRightWidth = WIDTH - rc.right;
			margins.cyTopHeight = rc.top;
			margins.cyBottomHeight = HEIGHT - rc.bottom + 1;
		}
	}
	else
		margins.cxLeftWidth = margins.cxRightWidth = margins.cyTopHeight = margins.cyBottomHeight = 1;
	flags &= ~FLAG_INIT_METRICS;
}

LRESULT NavigationBar::onPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	initTheme();

	CRect rc;
	GetClientRect(&rc);

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);

	if (flags & FLAG_INIT_METRICS) initMetrics(hdc);

	if (!(flags & FLAG_EDIT_MODE))
	{
		if (!backingStore) backingStore = BackingStore::getBackingStore();
		if (backingStore)
		{
			HDC hMemDC = backingStore->getCompatibleDC(hdc, rc.right, rc.bottom);
			if (hMemDC)
			{
				draw(hMemDC, rc);
				BitBlt(hdc, 0, 0, rc.right, rc.bottom, hMemDC, 0, 0, SRCCOPY);
			}
		}
		else
			draw(hdc, rc);
	}

	EndPaint(&ps);

	return 0;
}

LRESULT NavigationBar::onSize(UINT, WPARAM, LPARAM, BOOL&)
{
	flags |= FLAG_WANT_LAYOUT;
	if (flags & FLAG_EDIT_MODE)
	{
		RECT rc;
		GetClientRect(&rc);
		rc.bottom = comboDropDownHeight;
		comboBox.SetWindowPos(nullptr, &rc, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
	}
	return 0;
}

LRESULT NavigationBar::onThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	popup.changeTheme();
	themeToolbar.closeTheme();
	themeAddressBar.closeTheme();
	themeBreadcrumbs.closeTheme();
	if (bmpThemedDropDown)
	{
		DeleteObject(bmpThemedDropDown);
		bmpThemedDropDown = nullptr;
	}
	for (Item& item : crumbs)
		item.autoWidth = -1;
	for (Item& item : buttons)
		item.autoWidth = -1;
	flags |= FLAG_WANT_LAYOUT | FLAG_UPDATE_WIDTH;
	return 0;
}

LRESULT NavigationBar::onTimer(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	if (wParam == TIMER_UPDATE_ANIMATION)
	{
		updateAnimationState();
		return 0;
	}
	if (wParam == TIMER_CLEANUP)
	{
		cleanupAnimationState();
		return 0;
	}
	bHandled = FALSE;
	return 0;
}

LRESULT NavigationBar::onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	//ATLTRACE("NavigationBar::onLButtonDown\n");
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	bool update = false;
	int index;
	int ht = hitTest(pt, index);
	if (index >= 0)
	{
		clearHotItem();
		if (ht == HT_ITEM || ht == HT_ITEM_DD)
			crumbs[index].flags |= BF_PRESSED | BF_HOT;
		else
			buttons[index].flags |= BF_PRESSED | BF_HOT;
		if (ht == HT_ITEM_DD)
		{
			if ((flags & FLAG_POPUP_VISIBLE) && popupIndex == index)
			{
				hidePopup();
				popupIndex = -1;
			}
			else
			{
				showPopup(index);
				if (!(flags & FLAG_POPUP_VISIBLE)) return 0;
			}
		}
		hotIndex = index;
		hotType = pressedType = ht;
		flags |= FLAG_PRESSED;
		update = true;
	}
	else if (ht == HT_ICON)
	{
		clearHotItem();
		hotType = pressedType = HT_ICON;
		flags |= FLAG_PRESSED;
		update = true;
	}
	else if (ht == HT_CHEVRON)
	{
		clearHotItem();
		hotType = pressedType = HT_CHEVRON;
		flags |= FLAG_PRESSED;
		showChevronPopup();
		update = true;
	}
	else if (ht == HT_EMPTY)
	{
		if (!(flags & FLAG_EDIT_MODE)) enterEditMode(false);
	}
	if (update) Invalidate();
	return 0;
}

LRESULT NavigationBar::onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (!(flags & FLAG_PRESSED)) return 0;
	flags &= ~FLAG_PRESSED;
	if (flags & FLAG_POPUP_VISIBLE) return 0;
	clearHotItem();
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	int index;
	int ht = hitTest(pt, index);
	if (ht != pressedType) pressedType = HT_EMPTY;
	hotType = ht;
	if (index >= 0)
	{
		hotIndex = index;
		if (ht == HT_ITEM || ht == HT_ITEM_DD)
		{
			crumbs[hotIndex].flags |= BF_HOT;
			if (callback && pressedType == HT_ITEM)
				callback->selectItem(hotIndex);
		}
		else
		{
			buttons[hotIndex].flags |= BF_HOT;
			if (pressedType == HT_BUTTON && buttons[hotIndex].data == BUTTON_DROPDOWN && !(flags & FLAG_EDIT_MODE))
				enterEditMode(true);
		}
	}
	else if (pressedType == HT_ICON)
	{
		if (!(flags & FLAG_EDIT_MODE))
			enterEditMode(false);
	}
	pressedType = HT_EMPTY;
	Invalidate();
	return 0;
}

LRESULT NavigationBar::onMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (flags & FLAG_EDIT_MODE) return 0;
	POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	//ATLTRACE("onMouseMove: hWnd=0x%X, x=%d, y=%d\n", m_hWnd, pt.x, pt.y);
	bool update = false;
	int newIndex;
	int ht = hitTest(pt, newIndex);
	//ATLTRACE("onMouseMove: index=%d, ht=%d\n", newIndex, ht);
	if (ht == HT_OUTSIDE)
	{
		if (flags & FLAG_UNDER_MOUSE)
		{
			flags ^= FLAG_UNDER_MOUSE;
			update = true;
		}
		ht = HT_EMPTY;
	}
	else if (!(flags & FLAG_UNDER_MOUSE))
	{
		flags |= FLAG_UNDER_MOUSE;
		update = true;
	}
	if (flags & FLAG_POPUP_VISIBLE)
	{
		if (ht == HT_BUTTON)
		{
			ht = HT_EMPTY;
			newIndex = -1;
		}
		if ((newIndex >= 0 || ht == HT_CHEVRON) && newIndex != hotIndex)
		{
			clearHotItem();
			if (newIndex >= 0) crumbs[newIndex].flags |= BF_HOT | BF_PRESSED;
			hotType = pressedType = ht == HT_CHEVRON ? HT_CHEVRON : HT_ITEM_DD;
			hotIndex = newIndex;
			update = true;
			if (hotType == HT_CHEVRON)
				showChevronPopup();
			else
				showPopup(hotIndex);
		}
	}
	else if (newIndex != hotIndex || ht != hotType)
	{
		clearHotItem();
		if (ht == HT_ITEM || ht == HT_ITEM_DD)
			crumbs[newIndex].flags |= BF_HOT;
		else if (ht == HT_BUTTON)
			buttons[newIndex].flags |= BF_HOT;
		hotIndex = newIndex;
		hotType = ht;
		update = true;
	}
	if (!(flags & FLAG_MOUSE_TRACKING)) trackMouseEvent();
	if (update) Invalidate();
	return 0;
}

LRESULT NavigationBar::onMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	bool update = false;
	flags &= ~FLAG_MOUSE_TRACKING;
	if (flags & FLAG_UNDER_MOUSE)
	{
		flags ^= FLAG_UNDER_MOUSE;
		update = true;
	}
	if (!(flags & FLAG_POPUP_VISIBLE))
	{
		if (hotType != HT_EMPTY || pressedType != HT_EMPTY) update = true;
		pressedType = HT_EMPTY;
		clearHotItem();
	}
	if (update) Invalidate();
	return 0;
}

LRESULT NavigationBar::onPopupResult(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
#ifdef DEBUG_NAV_BAR
	ATLTRACE("NavigationBar::onPopupResult\n");
#endif
	if (flags & FLAG_HIDING_POPUP)
	{
		// Ignore ListPopup's WM_KILLFOCUS when we are hiding popup manually
		flags ^= FLAG_HIDING_POPUP;
		return 0;
	}
	hidePopup();
	int index = (int) wParam;
	if (index != -1 && callback)
	{
		if (popup.getData() == POPUP_DATA_CHEVRON)
		{
			index = chevronPopupIndexToItemIndex(index);
			if (index != -1) callback->selectItem(index);
			return 0;
		}
		popupIndex = popup.getData();
		callback->selectPopupItem(index, popup.getItemText(index), popup.getItemData(index));
	}
	popupIndex = -1;
	return 0;
}

LRESULT NavigationBar::onComboKillFocus(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (flags & FLAG_EDIT_MODE) exitEditMode();
	return 0;
}

LRESULT NavigationBar::onComboSelChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = comboBox.GetCurSel();
	if (index >= 0 && callback)
	{
		tstring text = callback->getHistoryItem(index);
		if (!text.empty() && callback->setCurrentPath(text)) exitEditMode();
	}
	return 0;
}

LRESULT NavigationBar::onExitEditMode(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (flags & FLAG_EDIT_MODE)
	{
		if (wParam == 1 && callback)
		{
			tstring text;
			WinUtil::getWindowText(editBox, text);
			if (!callback->setCurrentPath(text)) return 0;
		}
		exitEditMode();
	}
	return 0;
}

void NavigationBar::clearHotItem()
{
	if (hotIndex != -1)
	{
		if (hotType == HT_ITEM || hotType == HT_ITEM_DD)
			crumbs[hotIndex].flags &= ~(BF_HOT | BF_PRESSED);
		else if (hotType == HT_BUTTON)
			buttons[hotIndex].flags &= ~(BF_HOT | BF_PRESSED);
		hotIndex = -1;
	}
	hotType = HT_EMPTY;
}

void NavigationBar::showPopupWindow(int xpos)
{
	if (!popup)
	{
		popup.Create(m_hWnd, 0, nullptr, WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_TOOLWINDOW);
		if (!popup)
		{
			dcassert(0);
			return;
		}
		popup.setNotifWindow(m_hWnd);
	}
	SIZE size = popup.getPrefSize();
	CRect rc;
	GetClientRect(&rc);
	CPoint pt(xpos, rc.bottom);
	ClientToScreen(&pt);
	// Don't "optimize" by adding SWP_SHOWWINDOW flag to SetWindowPos, WM_SHOWWINDOW is not called in that case
	popup.SetWindowPos(nullptr, pt.x - popupPosOffset, pt.y, size.cx, size.cy, 0);
	popup.ShowWindow(SW_SHOW);
	flags = (flags | FLAG_POPUP_VISIBLE) & ~FLAG_HIDING_POPUP;
}

void NavigationBar::showPopup(int index)
{
#ifdef DEBUG_NAV_BAR
	ATLTRACE("showPopup: %d, visible: %d\n", index, flags & FLAG_POPUP_VISIBLE);
#endif
	if (!callback) return;
	vector<Callback::Item> res;
	callback->getPopupItems(index, res);
	if (res.empty())
	{
		hidePopup();
		popupIndex = -1;
		return;
	}
	popup.setData(index);
	popup.clearItems();
	for (const auto& item : res)
		popup.addItem(item.text, item.icon, item.data, item.flags);
	int xpos = crumbs[index].xpos + crumbs[index].width;
	// If we move ListPopup without hiding it first, it loses its CS_DROPSHADOW overlay
	if (flags & FLAG_POPUP_VISIBLE)
	{
		flags = (flags | FLAG_HIDING_POPUP) & ~FLAG_POPUP_VISIBLE;
		popup.ShowWindow(SW_HIDE);
	}
	showPopupWindow(xpos);
	popupIndex = index;
}

void NavigationBar::hidePopup()
{
	bool update = false;
	if (flags & FLAG_POPUP_VISIBLE)
	{
		if (hotIndex != -1)
		{
			crumbs[hotIndex].flags &= ~(BF_HOT | BF_PRESSED);
			hotIndex = -1;
			update = true;
		}
		if (pressedType != HT_EMPTY || hotType != HT_EMPTY)
		{
			pressedType = hotType = HT_EMPTY;
			update = true;
		}
		flags &= ~FLAG_POPUP_VISIBLE;
		popup.ShowWindow(SW_HIDE);
	}
	if (update) Invalidate();
}

void NavigationBar::showChevronPopup()
{
	dcassert(flags & FLAG_HAS_CHEVRON);
	int itemCount = 0;
	popup.clearItems();
	for (int index = 0; index < (int) crumbs.size(); ++index)
	{
		const Item& item = crumbs[index];
		if ((item.flags & BF_TEXT) && !item.width)
		{
			HBITMAP bitmap = callback ? callback->getChevronMenuImage(index, item.data) : nullptr;
			popup.addItem(item.text, bitmap, item.data);
			itemCount++;
		}
	}
	if (!itemCount) return;
	int xpos = 0;
	if (flags & FLAG_HAS_ICON) xpos += iconSize;
	if (popup.m_hWnd)
	{
		flags |= FLAG_HIDING_POPUP;
		popup.ShowWindow(SW_HIDE);
	}
	popup.setData(POPUP_DATA_CHEVRON);
	showPopupWindow(xpos);
}

int NavigationBar::chevronPopupIndexToItemIndex(int chevronIndex) const
{
	for (int index = 0; index < (int) crumbs.size(); ++index)
	{
		const Item& item = crumbs[index];
		if ((item.flags & BF_TEXT) && !item.width)
		{
			if (!chevronIndex) return index;
			--chevronIndex;
		}
	}
	return -1;
}

void NavigationBar::enterEditMode(bool showDropDown)
{
	cancelStateTransitions(false);
	clearHotItem();
	flags &= ~FLAG_PRESSED;
	pressedType = HT_EMPTY;

	CRect rc;
	GetClientRect(&rc);
	if (themeAddressBar.hTheme) rc.InflateRect(-1, -1);

	int comboItemHeight = rc.bottom - rc.top -  2 * GetSystemMetrics(SM_CYFIXEDFRAME);
	//ATLTRACE("comboItemHeight: %d\n", comboItemHeight);
	rc.bottom = rc.top + comboDropDownHeight;
	if (!comboBox)
	{
		comboBox.Create(m_hWnd, &rc, nullptr, WS_VISIBLE | WS_BORDER | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | CBS_AUTOHSCROLL | CBS_DROPDOWN, WS_EX_TOOLWINDOW);
		comboBox.SendMessage(CBEM_SETEXTENDEDSTYLE, CBES_EX_NOSIZELIMIT | CBES_EX_CASESENSITIVE, CBES_EX_NOSIZELIMIT | CBES_EX_CASESENSITIVE);
		if (hFont) comboBox.SetFont(hFont);
		imageList.create(IMAGE_LIST_ICON_SIZE, IMAGE_LIST_ICON_SIZE);
		comboBox.SetImageList(imageList.getHandle());
		comboBox.SetItemHeight(-1, comboItemHeight);
		editBox.notifWnd = m_hWnd;
		editBox.SubclassWindow(comboBox.GetEditCtrl());
	}
	else
	{
		comboBox.SetItemHeight(-1, comboItemHeight);
		comboBox.SetWindowPos(nullptr, &rc, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
	}
	if (callback)
	{
		tstring path = callback->getCurrentPath();
		HBITMAP icon = callback->getCurrentPathIcon();
		int imageIndex = icon ? imageList.addBitmap(icon) : -1;
		COMBOBOXEXITEM item = {};
		item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
		item.iItem = -1;
		item.pszText = const_cast<TCHAR*>(path.c_str());
		item.iImage = item.iSelectedImage = item.iOverlay = imageIndex;
		comboBox.SetItem(&item);
	}
	comboBox.SetFocus();
	flags |= FLAG_EDIT_MODE;
	fillHistoryComboBox();
	if (showDropDown) comboBox.ShowDropDown(TRUE);
}

void NavigationBar::exitEditMode()
{
	if (comboBox)
	{
		comboBox.ShowDropDown(FALSE);
		comboBox.ShowWindow(SW_HIDE);
	}
	flags &= ~FLAG_EDIT_MODE;
}

void NavigationBar::fillHistoryComboBox()
{
	if (!callback) return;
	uint64_t state = callback->getHistoryState();
	if (state == historyState) return;
	historyState = state;

	vector<Callback::HistoryItem> items;
	callback->getHistoryItems(items);
	comboBox.ResetContent();
	COMBOBOXEXITEM item = {};
	item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
	for (size_t i = 0; i < items.size(); i++)
	{
		int imageIndex = imageList.addBitmap(items[i].icon);
		item.iItem = i;
		item.pszText = const_cast<TCHAR*>(items[i].text.c_str());
		item.iImage = item.iSelectedImage = item.iOverlay = imageIndex;
		comboBox.InsertItem(&item);
	}
}

HBITMAP NavigationBar::getThemeDropDownBitmap()
{
	ThemeWrapper themeComboBox;
	if (themeComboBox.openTheme(m_hWnd, L"Combobox") && themeComboBox.pGetThemeBitmap)
	{
		HBITMAP hBitmap;
		int values[] =
		{
			3, // unknown magic value (Windows 7+)
			TMT_GLYPHDIBDATA // Windows Vista
		};
		for (int i = 0; i < _countof(values); ++i)
			if (SUCCEEDED(themeComboBox.pGetThemeBitmap(themeComboBox.hTheme, CP_DROPDOWNBUTTON, CBXS_NORMAL, values[i], GBF_COPY, &hBitmap)))
			{
				BITMAP bm;
				if (GetObject(hBitmap, sizeof(bm), &bm) && !(abs(bm.bmHeight) % 4))
					return hBitmap;
				DeleteObject(hBitmap);
			}
	}
	return nullptr;
}

static HBITMAP createMonoBitmap(int width, int height, std::function<void(uint8_t*, int, int, int)> drawFunc)
{
	int rowSize = ((width + 15) & ~15) >> 3;
	int size = rowSize * height;
	uint8_t* buf = new uint8_t[size];
	memset(buf, 0xFF, size);
	drawFunc(buf, rowSize, width, height);

	BITMAP bm;
	bm.bmType = 0;
	bm.bmWidth = width;
	bm.bmHeight = height;
	bm.bmWidthBytes = rowSize;
	bm.bmPlanes = 1;
	bm.bmBitsPixel = 1;
	bm.bmBits = buf;

	HBITMAP hBitmap = CreateBitmapIndirect(&bm);
	delete[] buf;
	return hBitmap;
}

static void drawMonoBitmap(HDC hdc, HBITMAP hBitmap, const RECT& rc, COLORREF color)
{
	BITMAP bm;
	if (!GetObject(hBitmap, sizeof(bm), &bm)) return;
	int height = abs(bm.bmHeight);
	WinUtil::drawMonoBitmap(hdc, hBitmap,
		(rc.left + rc.right - bm.bmWidth) / 2,
		(rc.top + rc.bottom - height) / 2,
		bm.bmWidth, height, color);
}

static void drawHLine(uint8_t* buf, int rowSize, int x, int y, int len)
{
	uint8_t* ptr = buf + rowSize * y + (x >> 3);
	uint8_t mask = 0x80 >> (x & 7);
	while (len)
	{
		*ptr ^= mask;
		ptr += mask & 1;
		mask = (mask >> 1) | (mask << 7);
		len--;
	}
}

void NavigationBar::createNonThemeResources(int flags)
{
	int width = glyphSize / 2;
	if (!(width & 1)) width--;
	int height = (width + 1) / 2;
	if (!bmpNonThemed[BNT_V_ARROW] && (flags & RESOURCE_ARROWS))
	{
		bmpNonThemed[BNT_V_ARROW] = createMonoBitmap(width, height,
			[](uint8_t* buf, int rowSize, int width, int height)
			{
				int pos = 0;
				while (width > 0)
				{
					drawHLine(buf, rowSize, pos, pos, width);
					pos++;
					width -= 2;
				}
			});
	}
	if (!bmpNonThemed[BNT_DROPDOWN] && (flags & RESOURCE_DROPDOWN))
	{
		dcassert(buttonSize > 0);
		int width = buttonSize / 2;
		if (!(width & 1)) width--;
		int height = (width + 1) / 2;
		bmpNonThemed[BNT_DROPDOWN] = createMonoBitmap(width, height,
			[](uint8_t* buf, int rowSize, int width, int height)
			{
				int pos = 0;
				while (width > 0)
				{
					drawHLine(buf, rowSize, pos, pos, width);
					pos++;
					width -= 2;
				}
			});
	}
	if (!bmpNonThemed[BNT_H_ARROW] && (flags & RESOURCE_ARROWS))
	{
		std::swap(width, height);
		bmpNonThemed[BNT_H_ARROW] = createMonoBitmap(width, height,
			[](uint8_t* buf, int rowSize, int width, int height)
			{
				int y1 = 0;
				while (y1 < width)
				{
					drawHLine(buf, rowSize, 0, y1, y1 + 1);
					int y2 = height - 1 - y1;
					if (y2 <= y1) break;
					drawHLine(buf, rowSize, 0, y2, y1 + 1);
					y1++;
				}
			});
	}
	if (!bmpNonThemed[BNT_CHEVRON] && (flags & RESOURCE_CHEVRON))
	{
		int dpi = WinUtil::getDisplayDpi();
		int dist = 2;
		int len = dpi >= 192 ? 3 : 2;
		int halfHeight = dpi >= 144 ? 3 : 2;
		height = halfHeight * 2 + 1;
		width = halfHeight + 2 * len + dist;
		bmpNonThemed[BNT_CHEVRON] = createMonoBitmap(width, height,
			[halfHeight, dist, len](uint8_t* buf, int rowSize, int width, int height)
			{
				int x = halfHeight, delta = -1;
				for (int y = 0; y < height; ++y)
				{
					drawHLine(buf, rowSize, x, y, len);
					drawHLine(buf, rowSize, x + len + dist, y, len);
					x += delta;
					if (x < 0) x = delta = 1;
				}
			});
	}
}

void NavigationBar::updateAnimationState()
{
	UpdateAnimationState uas;
	uas.hdc = nullptr;
	uas.timestamp = getHighResTimestamp();
	uas.frequency = getHighResFrequency();
	uas.update = uas.running = false;
	for (Item& item : crumbs)
		updateAnimationState(item, uas);
	for (Item& item : buttons)
		updateAnimationState(item, uas);
	updateAnimationState(chevron, uas);
	if (uas.hdc) ReleaseDC(uas.hdc);
	if (uas.update) Invalidate();
	if (!uas.running)
	{
		stopTimer(TIMER_UPDATE_ANIMATION, FLAG_TIMER_ANIMATION);
		startTimer(TIMER_CLEANUP, FLAG_TIMER_CLEANUP, CLEANUP_TIME);
	}
}

void NavigationBar::startTimer(int id, int flag, int time)
{
	if (!(flags & flag))
	{
		SetTimer(id, time, nullptr);
		flags |= flag;
	}
}

void NavigationBar::stopTimer(int id, int flag)
{
	if (flags & flag)
	{
		KillTimer(id);
		flags ^= flag;
	}
}

void NavigationBar::updateStateTransition(Item& item, int newState)
{
	if (!(item.trans && item.trans->running) && item.currentState == newState)
		return;
	if (!animationEnabled || !themeToolbar.hTheme || !themeAddressBar.hTheme)
		return;

	int64_t timestamp = getHighResTimestamp();
	if (item.trans)
	{
		if (item.trans->running)
		{
			if (item.trans->states[0] == newState || item.trans->states[1] == newState)
			{
				int duration = getTransitionDuration(item.trans->states[0], item.trans->states[1]);
				item.trans->nextState = -1;
				if (item.trans->states[0] == newState)
				{
					if (item.trans->isForward())
						item.trans->reverse(timestamp, duration);
				}
				else
				{
					if (!item.trans->isForward())
						item.trans->reverse(timestamp, duration);
				}
			}
			else
				item.trans->nextState = newState;
			return;
		}
	}
	else
		item.trans = new StateTransition;

#ifdef DEBUG_NAV_BAR
	ATLTRACE("New transition: %d -> %d\n", item.currentState, newState);
#endif
	int duration = getTransitionDuration(item.currentState, newState);
	item.trans->states[0] = item.currentState;
	item.trans->states[1] = newState;

	RECT rc;
	GetClientRect(&rc);
	HDC hdc = GetDC();
	item.trans->createBitmaps(this, hdc, rc, item);
	ReleaseDC(hdc);
	item.trans->start(timestamp, duration);
	startTimer(TIMER_UPDATE_ANIMATION, FLAG_TIMER_ANIMATION, 10);
	stopTimer(TIMER_CLEANUP, FLAG_TIMER_CLEANUP);
}

void NavigationBar::cancelStateTransitions(vector<Item>& v)
{
	for (Item& item : v)
		cancelStateTransition(item);
}

void NavigationBar::cancelStateTransition(Item& item)
{
	if (item.trans)
	{
		item.trans->running = false;
		item.trans->nextState = -1;
		item.currentState = -1;
	}
}

void NavigationBar::removeStateTransition(Item& item)
{
	delete item.trans;
	item.trans = nullptr;
	item.currentState = -1;
}

void NavigationBar::removeStateTransitions(vector<Item>& v)
{
	for (Item& item : v)
		removeStateTransition(item);
}

void NavigationBar::cancelStateTransitions(bool remove)
{
	stopTimer(TIMER_UPDATE_ANIMATION, FLAG_TIMER_ANIMATION);
	if (remove)
	{
		removeStateTransitions(crumbs);
		removeStateTransitions(buttons);
		removeStateTransition(chevron);
		stopTimer(TIMER_CLEANUP, FLAG_TIMER_CLEANUP);
	}
	else
	{
		cancelStateTransitions(crumbs);
		cancelStateTransitions(buttons);
		cancelStateTransition(chevron);
		startTimer(TIMER_CLEANUP, FLAG_TIMER_CLEANUP, CLEANUP_TIME);
	}
}

void NavigationBar::cleanupAnimationState(Item& item, UpdateAnimationState& uas, int64_t delay)
{
	if (!item.trans) return;
	if (!item.trans->running && uas.timestamp - item.trans->startTime > delay)
	{
#ifdef DEBUG_NAV_BAR
		ATLTRACE("Removing transition state %p\n", item.trans);
#endif
		removeStateTransition(item);
	}
	else
		uas.running = true;
}

void NavigationBar::cleanupAnimationState()
{
	int64_t delay = 15 * getHighResFrequency();
	UpdateAnimationState uas;
	uas.running = false;
	uas.timestamp = getHighResTimestamp();
	for (Item& item : crumbs)
		cleanupAnimationState(item, uas, delay);
	for (Item& item : buttons)
		cleanupAnimationState(item, uas, delay);
	cleanupAnimationState(chevron, uas, delay);
	if (!uas.running)
		stopTimer(TIMER_CLEANUP, FLAG_TIMER_CLEANUP);
}

void NavigationBar::updateAnimationState(Item& item, UpdateAnimationState& uas)
{
	if (!item.trans) return;
	if (item.trans->update(uas.timestamp, uas.frequency))
	{
		if (!item.trans->running)
		{
			item.currentState = item.trans->getCompletedState();
#ifdef DEBUG_NAV_BAR
			ATLTRACE("Transition %p: current state is now %d\n", item.trans, item.currentState);
#endif
		}
		uas.update = true;
	}
	if (!item.trans->running && item.trans->nextTransition())
	{
		if (!uas.hdc)
		{
			GetClientRect(&uas.rc);
			uas.hdc = GetDC();
		}
		item.trans->createBitmaps(this, uas.hdc, uas.rc, item);
		item.trans->start(uas.timestamp, getTransitionDuration(item.trans->states[0], item.trans->states[1]));
	}
	if (item.trans->running) uas.running = true;
}

int NavigationBar::getTransitionDuration(int oldState, int newState) const
{
	// This transition is not animated
	if ((oldState == TS_HOT && newState == TS_OTHERSIDEHOT) ||
	    (oldState == TS_OTHERSIDEHOT && newState == TS_HOT)) return 0;

	if (animationDuration >= 0)
		return animationDuration;

	DWORD duration;
	if (themeToolbar.hTheme && themeToolbar.pGetThemeTransitionDuration &&
	    SUCCEEDED(themeToolbar.pGetThemeTransitionDuration(themeToolbar.hTheme, TP_SPLITBUTTON, oldState, newState, TMT_TRANSITIONDURATIONS, &duration)))
	{
#ifdef DEBUG_NAV_BAR
		ATLTRACE("Transition duration %d -> %d is %d\n", oldState, newState, duration);
#endif
		return duration;
	}
	return 0;
}

NavigationBar::StateTransition::StateTransition()
{
	for (int i = 0; i < _countof(bitmaps); i++)
	{
		bitmaps[i] = nullptr;
		bits[i] = 0;
	}
	rc.left = rc.right = rc.top = rc.bottom = 0;
	states[0] = states[1] = nextState = -1;
	startTime = 0;
	currentValue = 0;
	currentAlpha = -1;
	duration = 0;
	startValue = 0;
	endValue = 1;
	running = true;
}

void NavigationBar::StateTransition::cleanup()
{
	for (int i = 0; i < _countof(bitmaps); ++i)
		if (bitmaps[i])
		{
			DeleteObject(bitmaps[i]);
			bitmaps[i] = nullptr;
		}
}

bool NavigationBar::StateTransition::update(int64_t time, int64_t frequency)
{
	if (!running || time <= startTime || frequency <= 0) return false;
	double elapsed = double(time - startTime) * 1000 / double(frequency);
	if (elapsed <= duration && duration > 0)
	{
		currentValue = startValue + (endValue - startValue) * (elapsed / duration);
		if (endValue > startValue)
		{
			if (currentValue > endValue) running = false;
		}
		else
			if (currentValue < endValue) running = false;
	}
	else
	{
		currentValue = endValue;
		running = false;
	}
#ifdef DEBUG_NAV_BAR
	if (!running) ATLTRACE("Transition %p finished\n", this);
#endif
	updateAlpha();
	return true;
}

void NavigationBar::StateTransition::updateAlpha()
{
	int intVal = (int) (255 * currentValue);
	clampValue(intVal, 0, 255);
	if (currentAlpha == intVal) return;
	currentAlpha = intVal;
	if (currentAlpha != 0 && currentAlpha != 255) updateImage();
}

void NavigationBar::StateTransition::updateImage()
{
	dcassert(currentAlpha >= 0 && currentAlpha <= 255);
	unsigned width = rc.right - rc.left;
	unsigned height = rc.bottom - rc.top;
	WinUtil::blend32(bits[0], bits[1], bits[2], width * height, 255 - currentAlpha);
	GdiFlush();
}

void NavigationBar::StateTransition::draw(HDC hdc)
{
	int index = 2;
	if (currentAlpha == 0)
		index = 0;
	else if (currentAlpha == 255)
		index = 1;
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	HDC bitmapDC = CreateCompatibleDC(hdc);
	HGDIOBJ oldBitmap = SelectObject(bitmapDC, bitmaps[index]);
	BitBlt(hdc, rc.left, rc.top, width, height, bitmapDC, 0, 0, SRCCOPY);
	oldBitmap = SelectObject(bitmapDC, oldBitmap);
	SelectObject(bitmapDC, oldBitmap);
	DeleteDC(bitmapDC);
}

void NavigationBar::StateTransition::createBitmaps(NavigationBar* navBar, HDC hdc, const RECT& rcClient, const Item& item)
{
	RECT rcBackground = rcClient;
	RECT rcButton;
	rcButton.left = item.xpos;
	rcButton.right = rcButton.left + item.width;
	rcButton.top = rcClient.top + navBar->margins.cyTopHeight;
	rcButton.bottom = rcClient.bottom - navBar->margins.cyBottomHeight;

	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	if (width != rcButton.right - rcButton.left || height != rcButton.bottom - rcButton.top)
	{
		width = rcButton.right - rcButton.left;
		height = rcButton.bottom - rcButton.top;
		cleanup();
	}

	rc = rcButton;

	OffsetRect(&rcBackground, -rc.left, -rc.top);
	OffsetRect(&rcButton, -rc.left, -rc.top);

	BITMAPINFOHEADER bmi = {};
	bmi.biWidth = width;
	bmi.biHeight = -height;
	bmi.biBitCount = 32;
	bmi.biCompression = BI_RGB;
	bmi.biPlanes = 1;
	bmi.biSize = sizeof(bmi);
	bmi.biSizeImage = width * height << 2;

	HDC bitmapDC = CreateCompatibleDC(hdc);
	for (int i = 0; i < 3; i++)
	{
		if (!bitmaps[i])
			bitmaps[i] = CreateDIBSection(nullptr, (BITMAPINFO*) &bmi, DIB_RGB_COLORS, (void **) &bits[i], nullptr, 0);
		if (i < 2)
		{
			HGDIOBJ oldBitmap = SelectObject(bitmapDC, bitmaps[i]);
			navBar->drawBackground(bitmapDC, rcBackground);
			navBar->drawThemeButton(bitmapDC, rcButton, item, states[i]);
			GdiFlush();
			SelectObject(bitmapDC, oldBitmap);
		}
	}
	DeleteDC(bitmapDC);
}

void NavigationBar::StateTransition::start(int64_t time, int duration)
{
	startTime = time;
	currentValue = 0;
	startValue = 0;
	endValue = 1;
	currentAlpha = 0;
	nextState = -1;
	this->duration = duration;
	running = true;
#ifdef DEBUG_NAV_BAR
	ATLTRACE("Transition %p started\n", this);
#endif
}

void NavigationBar::StateTransition::reverse(int64_t time, int totalDuration)
{
	if (startValue < endValue)
	{
		endValue = 0;
		duration = (int) (totalDuration * currentValue);
	}
	else
	{
		endValue = 1;
		duration = (int) (totalDuration * (1 - currentValue));
	}
	startTime = time;
	startValue = currentValue;
#ifdef DEBUG_NAV_BAR
	ATLTRACE("Transition %p reversed: start=%f, end=%f\n", this, startValue, endValue);
#endif
}

bool NavigationBar::StateTransition::nextTransition()
{
	if (nextState == -1) return false;
	if (isForward()) states[0] = states[1];
#ifdef DEBUG_NAV_BAR
	ATLTRACE("Transition %p: next state is %d\n", this, nextState);
#endif
	states[1] = nextState;
	nextState = -1;
	return true;
}

int NavigationBar::StateTransition::getCompletedState() const
{
	return states[isForward() ? 1 : 0];
}
