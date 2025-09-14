#include "stdafx.h"
#include "TabPreviewWindow.h"
#include "GdiUtil.h"
#include "Fonts.h"
#include "DwmApiLib.h"
#include "WinUtil.h"
#include <algorithm>

static const int ICON_SIZE = 16;

#undef min
#undef max

#define DWM DwmApiLib::instance

TabPreviewWindow::TabPreviewWindow()
{
	tabHeight = 0;
	tabWidth = 0;
	startMargin = 0;
	innerMargin = 0;
	horizIconSpace = 0;
	horizPadding = 0;
	previewWidth = 400;
	previewHeight = 300;
	textColor = RGB(0, 0, 0);
	backgroundColor = RGB(255, 255, 255);
	borderColor = RGB(157, 157, 161);
	maxPreviewWidth = 0;
	maxPreviewHeight = 0;
	pos = TABS_TOP;
	ptGrab = {};
}

void TabPreviewWindow::init(HICON icon, const tstring& text, int pos)
{
	this->icon = icon;
	this->text = text;
	this->pos = pos;
}

void TabPreviewWindow::clearPreview()
{
	if (preview)
	{
		DeleteObject(preview);
		preview = nullptr;
	}
}

void TabPreviewWindow::setTextColor(COLORREF color)
{
	textColor = color;
}

void TabPreviewWindow::setBackgroundColor(COLORREF color)
{
	backgroundColor = color;
}

void TabPreviewWindow::setBorderColor(COLORREF color)
{
	borderColor = color;
}

void TabPreviewWindow::setGrabPoint(POINT pt)
{
	ptGrab = pt;
}

void TabPreviewWindow::setMaxPreviewSize(int width, int height)
{
	maxPreviewWidth = width;
	maxPreviewHeight = height;
}

void TabPreviewWindow::draw(HDC hdc)
{
	RECT rc;
	GetClientRect(&rc);
	RECT rc2 = rc;
	if (pos == TABS_TOP)
		rc2.top += tabHeight;
	else
		rc2.bottom -= tabHeight;
	HBRUSH borderBrush = CreateSolidBrush(borderColor);
	HBRUSH backgroundBrush = CreateSolidBrush(backgroundColor);
	WinUtil::drawFrame(hdc, rc2, 1, 1, borderBrush);
	rc2.left++; rc2.top++; rc2.right--; rc2.bottom--;
	FillRect(hdc, &rc2, backgroundBrush);
	if (pos == TABS_TOP)
	{
		rc2.top = rc.top;
		rc2.bottom = rc2.top + tabHeight + 1;
	}
	else
	{
		rc2.bottom = rc.bottom;
		rc2.top = rc2.bottom - tabHeight - 1;
	}
	rc2.left = rc.left + startMargin;
	rc2.right = rc2.left + tabWidth;
	WinUtil::drawFrame(hdc, rc2, 1, 1, borderBrush);
	int xpos = rc2.left + horizPadding;
	RECT rc3 = rc2;
	rc3.left++; rc3.right--;
	if (pos == TABS_TOP)
		rc3.top++;
	else
		rc3.bottom--;
	FillRect(hdc, &rc3, backgroundBrush);
	if (icon)
	{
		DrawIconEx(hdc, xpos, rc2.top + (rc2.bottom - rc2.top - ICON_SIZE) / 2, icon, ICON_SIZE, ICON_SIZE, 0, nullptr, DI_NORMAL | DI_COMPAT);
		xpos += ICON_SIZE + horizIconSpace;
	}
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, textColor);
	HGDIOBJ oldFont = SelectObject(hdc, Fonts::g_systemFont);
	rc2.left = xpos;
	rc2.right = rc2.right + 1 - horizPadding;
	DrawText(hdc, text.c_str(), (int) text.length(), &rc2, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
	SelectObject(hdc, oldFont);
	DeleteObject(backgroundBrush);
	DeleteObject(borderBrush);

	if (preview)
	{
		if (pos == TABS_TOP) rc.top += tabHeight;
		rc.left += innerMargin;
		rc.top += innerMargin;
		HDC hdcBitmap = CreateCompatibleDC(hdc);
		HGDIOBJ oldBitmap = SelectObject(hdcBitmap, preview);
		BitBlt(hdc, rc.left, rc.top, previewWidth, previewHeight, hdcBitmap, 0, 0, SRCCOPY);
		SelectObject(hdcBitmap, oldBitmap);
		DeleteDC(hdcBitmap);
	}
}

void TabPreviewWindow::setPreview(HWND hWnd)
{
	clearPreview();

	RECT rc;
	::GetClientRect(hWnd, &rc);

	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	if (maxPreviewWidth > 1 && maxPreviewHeight > 1)
	{
		double scale1 = width > maxPreviewWidth ? double(maxPreviewWidth) / width : 1.0;
		double scale2 = height > maxPreviewHeight ? double(maxPreviewHeight) / height : 1.0;
		double scale = std::min(scale1, scale2);
		previewWidth = (int) (scale * width);
		previewHeight = (int) (scale * height);
	}
	else
	{
		previewWidth = width;
		previewHeight = height;
	}

	if (previewWidth < 1 || previewHeight < 1)
	{
		previewWidth = previewHeight = 0;
		return;
	}

	bool result = false;
	bool updateStyle = false;
	LONG exStyle = 0;
	if (!DwmApiLib::instance.isCompositionEnabled())
	{
		exStyle = ::GetWindowLong(hWnd, GWL_EXSTYLE);
		if (!(exStyle & (WS_EX_COMPOSITED | WS_EX_LAYERED)))
		{
			exStyle |= WS_EX_COMPOSITED;
			::SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);
			::UpdateWindow(hWnd);
			updateStyle = true;
		}
	}

	HDC hdcFrame = ::GetDC(hWnd);
	if (hdcFrame)
	{
		HDC hdc = CreateCompatibleDC(hdcFrame);
		preview = CreateCompatibleBitmap(hdcFrame, previewWidth, previewHeight);
		if (preview)
		{
			HGDIOBJ oldBitmap = SelectObject(hdc, preview);
			if (width == previewWidth && height == previewHeight)
				result = BitBlt(hdc, 0, 0, width, height, hdcFrame, 0, 0, SRCCOPY) != FALSE;
			else
			{
				SetStretchBltMode(hdc, HALFTONE);
				result = StretchBlt(hdc, 0, 0, previewWidth, previewHeight, hdcFrame, 0, 0, width, height, SRCCOPY) != FALSE;
			}
			SelectObject(hdc, oldBitmap);
			DeleteDC(hdc);
		}
		::ReleaseDC(hWnd, hdcFrame);
	}

	if (updateStyle)
	{
		exStyle &= ~WS_EX_COMPOSITED;
		::SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);
	}

	if (!result && preview)
	{
		DeleteObject(preview);
		preview = nullptr;
	}
}

void TabPreviewWindow::updateSize(HDC hdc)
{
	CDCHandle dc(hdc);
	HFONT oldfont = dc.SelectFont(Fonts::g_systemFont);
	int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	horizPadding = 5 * dpi / 96;
	startMargin = 8 * dpi / 96;
	int vertExtraSpace = 10 * dpi / 96;
	horizIconSpace = 2 * dpi / 96;
	innerMargin = 3 * dpi / 96;
	int textHeight = WinUtil::getTextHeight(dc);
	tabHeight = std::max<int>(textHeight, ICON_SIZE) + vertExtraSpace;
	SIZE textSize;
	dc.GetTextExtent(text.c_str(), (int) text.length(), &textSize);
	tabWidth = textSize.cx + 2 * horizPadding;
	if (icon) tabWidth += ICON_SIZE + horizIconSpace;
	dc.SelectFont(oldfont);
}

SIZE TabPreviewWindow::getSize() const
{
	SIZE size;
	size.cx = std::max<int>(tabWidth + 2 * startMargin, previewWidth + 2 * innerMargin);
	size.cy = previewHeight + 2* innerMargin + tabHeight;
	return size;
}

POINT TabPreviewWindow::getOffset() const
{
	POINT pt;
	pt.x = -startMargin;
	pt.y = pos == TABS_TOP ? 0 : -getSize().cy;
	pt.x += ptGrab.x;
	pt.y += ptGrab.y;
	return pt;
}

static void clearAlpha(uint8_t* data, int stride, int x, int y, int width, int height)
{
	if (width <= 0 || height <= 0) return;
	data += y * stride;
	while (height)
	{
		uint8_t* p = data + x * 4 + 3;
		for (int i = 0; i < width * 4; i += 4)
			p[i] = 0;
		data += stride;
		height--;
	}
}

LRESULT TabPreviewWindow::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
{
	RECT rc;
	GetClientRect(&rc);
	SIZE size;
	size.cx = rc.right - rc.left;
	size.cy = rc.bottom - rc.top;

	HDC hdcScreen = ::GetDC(nullptr);
	HDC hdcBackBuffer = CreateCompatibleDC(hdcScreen);

	void* bitmapData = nullptr;
	BITMAPINFOHEADER bmi = {};
	bmi.biWidth = size.cx;
	bmi.biHeight = -size.cy;
	bmi.biBitCount = 32;
	bmi.biCompression = BI_RGB;
	bmi.biPlanes = 1;
	bmi.biSize = sizeof(bmi);
	bmi.biSizeImage = size.cx * size.cy * 4;
	HBITMAP hBitmap = CreateDIBSection(hdcScreen, (BITMAPINFO*) &bmi, DIB_RGB_COLORS, &bitmapData, nullptr, 0);
	memset(bitmapData, 0, bmi.biSizeImage);

	HGDIOBJ hbmOld = SelectObject(hdcBackBuffer, hBitmap);
	draw(hdcBackBuffer);
	GdiFlush();

	uint8_t* p = (uint8_t*) bitmapData;
	for (int i = 3; i < (int) bmi.biSizeImage; i += 4)
		p[i] = 0xFF;
	int stride = size.cx * 4;
	int ypos = pos == TABS_TOP ? 0 : size.cy - tabHeight;
	clearAlpha(p, stride, 0, ypos, startMargin, tabHeight);
	clearAlpha(p, stride, startMargin + tabWidth, ypos, size.cx - (startMargin + tabWidth), tabHeight);

	POINT ptSrc = {};
	BLENDFUNCTION bf;
	bf.AlphaFormat = AC_SRC_ALPHA;
	bf.SourceConstantAlpha = 220;
	bf.BlendFlags = 0;
	bf.BlendOp = AC_SRC_OVER;

	::ReleaseDC(nullptr, hdcScreen);
	UpdateLayeredWindow(m_hWnd, nullptr, nullptr, &size, hdcBackBuffer, &ptSrc, 0, &bf, ULW_ALPHA);
	SelectObject(hdcBackBuffer, hbmOld);
	DeleteObject(hBitmap);
	DeleteDC(hdcBackBuffer);

	return 0;
}

LRESULT TabPreviewWindow::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
{
	clearPreview();
	return 0;
}
