#include <stdafx.h>
#include "StatusLabelCtrl.h"
#include "ImageLists.h"
#include "WinUtil.h"
#include "Colors.h"

static const int ICON_SPACE = 2; // dialog units

LRESULT StatusLabelCtrl::onCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	return 0;
}

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
	CPaintDC hDC(m_hWnd);
	if (textWidth == -1) updateTextSize(hDC);
	RECT rc;
	GetClientRect(&rc);
	LRESULT br = GetParent().SendMessage(WM_CTLCOLORSTATIC, (WPARAM) hDC.m_hDC, (LPARAM) m_hWnd);
	if (br) hDC.FillRect(&rc, (HBRUSH) br);
	int x = rc.left;
	if (hIcon)
	{
		DrawIconEx(hDC, x, (rc.top + rc.bottom - bitmapHeight) / 2, hIcon, bitmapWidth, bitmapHeight, 0, nullptr, DI_NORMAL);
		x += bitmapWidth + iconSpace;
	}
	HFONT prevFont = hDC.SelectFont(font ? font : (HFONT) GetStockObject(DEFAULT_GUI_FONT));
	int prevMode = hDC.SetBkMode(TRANSPARENT);
	ExtTextOut(hDC, x, rc.top + (rc.bottom - rc.top - textHeight) / 2, ETO_CLIPPED, &rc, text.c_str(), text.length(), nullptr);
	hDC.SelectFont(prevFont);
	hDC.SetBkMode(prevMode);
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
	hIcon = g_iconBitmaps.getIcon(index, 0);
	if (hIcon)
		bitmapWidth = bitmapHeight = 16;
	else
		bitmapWidth = bitmapHeight = 0;
}

SIZE StatusLabelCtrl::getIdealSize(HDC hdc)
{
	if (textWidth == -1) updateTextSize(hdc);
	int width = textWidth;
	if (hIcon) width += iconSpace + bitmapWidth;
	return SIZE{width, std::max(textHeight, bitmapHeight)};
}

void StatusLabelCtrl::setText(const tstring& s)
{
	text = s;
	textWidth = textHeight = -1;
}
