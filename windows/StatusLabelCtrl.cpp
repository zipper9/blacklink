#include <stdafx.h>
#include "StatusLabelCtrl.h"
#include "GdiUtil.h"
#include "BackingStore.h"
#include "ImageLists.h"
#include "WinUtil.h"

static const int ICON_SPACE = 2; // dialog units

LRESULT StatusLabelCtrl::onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	text.assign(reinterpret_cast<const TCHAR*>(lParam));
	textWidth = textHeight = -1;
	return TRUE;
}

LRESULT StatusLabelCtrl::onSetFont(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	font = reinterpret_cast<HFONT>(wParam);
	return 0;
}

LRESULT StatusLabelCtrl::onGetFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return reinterpret_cast<LRESULT>(font);
}

LRESULT StatusLabelCtrl::onPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	RECT rc;
	GetClientRect(&rc);

	bool drawn = false;
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);

	if (textWidth == -1) updateTextSize(hdc);
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

void StatusLabelCtrl::updateTextSize(HDC hdc)
{
	HGDIOBJ prevFont = SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
	SIZE sz = { 0, 0 };
	if (!text.empty()) GetTextExtentPoint32(hdc, text.c_str(), text.length(), &sz);
	textWidth = sz.cx;
	textHeight = sz.cy;
	int xdu, ydu;
	WinUtil::getDialogUnits(hdc, xdu, ydu);
	SelectObject(hdc, prevFont);
	iconSpace = WinUtil::dialogUnitsToPixelsX(ICON_SPACE, xdu);
}

void StatusLabelCtrl::setImage(int index, int size)
{
	hBitmap = index < 0 ? nullptr : g_iconBitmaps.getBitmap(index, 0);
	if (hBitmap)
		bitmapWidth = bitmapHeight = 16;
	else
		bitmapWidth = bitmapHeight = 0;
}

SIZE StatusLabelCtrl::getIdealSize(HDC hdc)
{
	if (textWidth == -1) updateTextSize(hdc);
	int width = textWidth;
	if (hBitmap) width += iconSpace + bitmapWidth;
	return SIZE{width, std::max(textHeight, bitmapHeight)};
}

void StatusLabelCtrl::setText(const tstring& s)
{
	text = s;
	textWidth = textHeight = -1;
}

void StatusLabelCtrl::draw(HDC hdc, const RECT& rc)
{
	LRESULT br = GetParent().SendMessage(WM_CTLCOLORSTATIC, (WPARAM) hdc, (LPARAM) m_hWnd);
	if (br) FillRect(hdc, &rc, (HBRUSH) br);
	int x = rc.left;
	if (hBitmap)
	{
		WinUtil::drawAlphaBitmap(hdc, hBitmap, x, (rc.top + rc.bottom - bitmapHeight) / 2, bitmapWidth, bitmapHeight);
		x += bitmapWidth + iconSpace;
	}
	HGDIOBJ prevFont = SelectObject(hdc, font ? font : (HFONT) GetStockObject(DEFAULT_GUI_FONT));
	int prevMode = SetBkMode(hdc, TRANSPARENT);
	ExtTextOut(hdc, x, rc.top + (rc.bottom - rc.top - textHeight) / 2, ETO_CLIPPED, &rc, text.c_str(), text.length(), nullptr);
	SelectObject(hdc, prevFont);
	SetBkMode(hdc, prevMode);
}

void StatusLabelCtrl::cleanup()
{
	if (backingStore)
	{
		backingStore->release();
		backingStore = nullptr;
	}
}
