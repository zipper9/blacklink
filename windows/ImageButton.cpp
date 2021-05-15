#include "stdafx.h"

#ifdef OSVER_WIN_XP
#include "ImageButton.h"

static const int BUTTON_PADDING = 3;

ImageButton::ImageButton() : icon(nullptr), initialized(false)
{
	hTheme = nullptr;
	imageWidth = -1;
	imageHeight = -1;
}

ImageButton::~ImageButton()
{
	cleanup();
}

void ImageButton::cleanup()
{
	if (hTheme)
	{
		CloseThemeData(hTheme);
		hTheme = nullptr;
	}
}

LRESULT ImageButton::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	cleanup();
	return 0;
}

LRESULT ImageButton::onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!initialized)
	{
		initialized = true;
		hTheme = OpenThemeData(m_hWnd, L"BUTTON");
	}
	CRect rect;
	GetClientRect(&rect);
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);
	draw(hdc, rect.Width(), rect.Height());
	EndPaint(&ps);
	return 0;
}


LRESULT ImageButton::onErase(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	return 1;
}

LRESULT ImageButton::onThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (hTheme) CloseThemeData(hTheme);
	hTheme = OpenThemeData(m_hWnd, L"BUTTON");
	Invalidate();
	return 0;
}

LRESULT ImageButton::onSetImage(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == IMAGE_ICON)
	{
		icon = (HICON) lParam;
		imageWidth = imageHeight = -1;
	}
	bHandled = FALSE;
	return 0;
}

void ImageButton::drawBackground(HDC hdc, int width, int height)
{
	UINT state = GetState();
	RECT rect = { 0, 0, width, height };
	FillRect(hdc, &rect, GetSysColorBrush(COLOR_BTNFACE));
	if (hTheme)
	{
		int flags;
		if (state & BST_PUSHED) flags = PBS_PRESSED;
		else if (state & BST_HOT) flags = PBS_HOT;
		else flags = PBS_NORMAL;
		DrawThemeBackground(hTheme, hdc, BP_PUSHBUTTON, flags, &rect, nullptr);
	}
	else
	{
		UINT flags = DFCS_BUTTONPUSH;
		if (state & BST_PUSHED) flags |= DFCS_PUSHED;
		if (state & BST_HOT) flags |= DFCS_HOT;
		DrawFrameControl(hdc, &rect, DFC_BUTTON, flags);
	}
	if (state & BST_FOCUS)
	{
		InflateRect(&rect, -BUTTON_PADDING, -BUTTON_PADDING);
		DrawFocusRect(hdc, &rect);
	}
}

void ImageButton::drawImage(HDC hdc, int width, int height)
{
	if (!icon) return;
	if (imageWidth < 0 || imageHeight < 0)
	{
		ICONINFO info;
		if (!GetIconInfo(icon, &info)) return;
		BITMAP bm;
		if (!GetObject(info.hbmColor, sizeof(bm), &bm)) return;
		imageWidth = bm.bmWidth;
		imageHeight = bm.bmHeight;
	}
	int x = (width - imageWidth) / 2;
	int y = (height - imageHeight) / 2;
	DrawIconEx(hdc, x, y, icon, 0, 0, 0, nullptr, DI_NORMAL);
}

void ImageButton::draw(HDC hdc, int width, int height)
{
	drawBackground(hdc, width, height);
	drawImage(hdc, width, height);
}
#endif
