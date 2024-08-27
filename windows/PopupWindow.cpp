#include "stdafx.h"
#include "PopupWindow.h"
#include "GdiUtil.h"
#include "Fonts.h"
#include "UserMessages.h"

static const int TIMER_UPDATE_ANIMATION = 1;

enum
{
	FLAG_FONT_TEXT  = 1,
	FLAG_FONT_TITLE = 2,
	FLAG_ANIM_TIMER = 4
};

enum
{
	ANIM_SHOW = 1,
	ANIM_HIDE
};

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

PopupWindow::PopupWindow()
{
	memset(&logFont, 0, sizeof(logFont));
	memset(&logFontTitle, 0, sizeof(logFontTitle));
	flags = 0;
	hFont = Fonts::g_font;
	hFontTitle = Fonts::g_boldFont;

	int dpi = WinUtil::getDisplayDpi();
	borderSize = dpi / 96;
	padding = 16 * dpi / 96;
	titleSpace = 20 * dpi / 96;

	backgroundColor = RGB(217, 234, 247);
	textColor = RGB(0, 0, 0);
	titleColor = borderColor = RGB(21, 67, 101);

	animDuration = 600;
	currentAnimation = 0;
	currentAnimDuration = 0;
	animStart = 0;

	removeTime = 0;
	hNotifWnd = nullptr;
}

void PopupWindow::cleanup()
{
	if (flags & FLAG_FONT_TEXT)
	{
		DeleteObject(hFont);
		hFont = nullptr;
		flags ^= FLAG_FONT_TEXT;
	}
	if (flags & FLAG_FONT_TITLE)
	{
		DeleteObject(hFontTitle);
		hFontTitle = nullptr;
		flags ^= FLAG_FONT_TITLE;
	}
}

void PopupWindow::setFont(const LOGFONT& f)
{
	if (flags & FLAG_FONT_TEXT)
	{
		DeleteObject(hFont);
		flags ^= FLAG_FONT_TEXT;
	}
	logFont = f;
	hFont = nullptr;
}

void PopupWindow::setTitleFont(const LOGFONT& f)
{
	if (flags & FLAG_FONT_TITLE)
	{
		DeleteObject(hFontTitle);
		flags ^= FLAG_FONT_TITLE;
	}
	logFontTitle = f;
	hFontTitle = nullptr;
}

double PopupWindow::getAnimValue() const
{
	int64_t now = getHighResTimestamp();
	double elapsed = double(now - animStart) * 1000 / double(getHighResFrequency());
	double value = elapsed / currentAnimDuration;
	if (value < 0)
		value = 0;
	else if (value > 1)
		value = 1;
	return value;
}

void PopupWindow::startAnimation(int type)
{
	if (currentAnimation == type || !type) return;
	updateWindowStyle();
	if (currentAnimation)
		currentAnimDuration = (int) (currentAnimDuration * getAnimValue());
	else
		currentAnimDuration = animDuration;
	if (!currentAnimDuration)
	{
		onAnimationFinished(type);
		return;
	}
	currentAnimation = type;
	animStart = getHighResTimestamp();
	if (!(flags & FLAG_ANIM_TIMER))
	{
		SetTimer(TIMER_UPDATE_ANIMATION, 10);
		flags |= FLAG_ANIM_TIMER;
	}
}

LRESULT PopupWindow::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
{
	cleanup();
	return 0;
}

LRESULT PopupWindow::onPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);

	RECT rc;
	GetClientRect(&rc);

	if (!hFont)
	{
		hFont = CreateFontIndirect(&logFont);
		if (hFont) flags |= FLAG_FONT_TEXT;
	}
	if (!hFontTitle)
	{
		hFontTitle = CreateFontIndirect(&logFontTitle);
		if (hFontTitle) flags |= FLAG_FONT_TITLE;
	}

	HBRUSH brushBack = CreateSolidBrush(backgroundColor);
	HBRUSH brushBorder = CreateSolidBrush(borderColor);

	FillRect(hdc, &rc, brushBack);
	WinUtil::drawFrame(hdc, rc, borderSize, borderSize, brushBorder);

	rc.left += padding;
	rc.right -= padding;
	RECT rcText = rc;

	HGDIOBJ oldFont = SelectObject(hdc, hFontTitle);
	DrawText(hdc, title.c_str(), (int) title.length(), &rcText, DT_CENTER|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS|DT_CALCRECT);
	int titleHeight = rcText.bottom - rcText.top;
	rcText = rc;
	SelectObject(hdc, hFont);
	DrawText(hdc, text.c_str(), (int) text.length(), &rcText, DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK|DT_CALCRECT);
	int textHeight = rcText.bottom - rcText.top;

	rcText = rc;
	rcText.top = (rc.bottom - (titleHeight + textHeight + titleSpace)) / 2;
	SetBkMode(hdc, TRANSPARENT);
	SelectObject(hdc, hFontTitle);
	SetTextColor(hdc, titleColor);
	DrawText(hdc, title.c_str(), (int) title.length(), &rcText, DT_CENTER|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
	rcText.top += titleHeight + titleSpace;
	rcText.bottom -= padding;
	SelectObject(hdc, hFont);
	SetTextColor(hdc, textColor);
	DrawText(hdc, text.c_str(), (int) text.length(), &rcText, DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK);
	SelectObject(hdc, oldFont);

	DeleteObject(brushBack);
	DeleteObject(brushBorder);

	EndPaint(&ps);
	return 0;
}

LRESULT PopupWindow::onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	removeTime = 0;
	startAnimation(ANIM_HIDE);
	return 0;
}

LRESULT PopupWindow::onTimer(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	if (wParam == TIMER_UPDATE_ANIMATION)
	{
		double value = getAnimValue();
		if (value == 1)
		{
			onAnimationFinished(currentAnimation);
			return 0;
		}
		int alpha = (int) (255 * value);
		if (currentAnimation == ANIM_HIDE) alpha = 255 - alpha;
		SetLayeredWindowAttributes(m_hWnd, 0, alpha, LWA_ALPHA);
		return 0;
	}
	bHandled = FALSE;
	return 0;
}

LRESULT PopupWindow::onShowWindow(UINT, WPARAM wParam, LPARAM, BOOL&)
{
	if (wParam) startAnimation(ANIM_SHOW);
	return 0;
}

void PopupWindow::onAnimationFinished(int type)
{
	if (flags & FLAG_ANIM_TIMER)
	{
		KillTimer(TIMER_UPDATE_ANIMATION);
		flags ^= FLAG_ANIM_TIMER;
	}
	if (currentAnimation == ANIM_HIDE)
	{
		ShowWindow(SW_HIDE);
		if (hNotifWnd) ::PostMessage(hNotifWnd, WMU_REMOVE_POPUP, 0, (LPARAM) m_hWnd);
	}
	currentAnimation = 0;
}

void PopupWindow::updateWindowStyle()
{
	LONG_PTR oldStyle = GetWindowLongPtr(GWL_EXSTYLE);
	LONG_PTR newStyle = oldStyle | WS_EX_LAYERED;
	if (oldStyle != newStyle) SetWindowLongPtr(GWL_EXSTYLE, newStyle);
}

void PopupWindow::hide()
{
	if (m_hWnd) startAnimation(ANIM_HIDE);
}
