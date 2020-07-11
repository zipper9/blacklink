#include <stdafx.h>
#include "RichTextLabel.h"
#include <atlmisc.h>
#include "StrUtil.h"
#include "BaseUtil.h"
#include "WinUtil.h"

enum
{
	MARGIN_LEFT,
	MARGIN_TOP,
	MARGIN_RIGHT,
	MARGIN_BOTTOM
};

static const int UNDERLINE_OFFSET_Y = 1;
static const int UNDERLINE_OFFSET_X = 0;

static const int MIN_FONT_SIZE = 6;
static const int MAX_FONT_SIZE = 72;

RichTextLabel::~RichTextLabel()
{
	cleanup();
	clearStyles();
}

BOOL RichTextLabel::SubclassWindow(HWND hWnd)
{
	ATLASSERT(m_hWnd == NULL);
	ATLASSERT(::IsWindow(hWnd));
	if (!CWindowImpl<RichTextLabel>::SubclassWindow(hWnd)) return FALSE;
	WinUtil::getWindowText(hWnd, text);
	initialize();
	return TRUE;
}

LRESULT RichTextLabel::onCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
	initialize();
	return lRes;
}

LRESULT RichTextLabel::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	cleanup();
	clearStyles();
	return 0;
}

LRESULT RichTextLabel::onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	text.assign(reinterpret_cast<const TCHAR*>(lParam));
	initialize();
	return TRUE;
}

void RichTextLabel::drawUnderline(HDC dc, int xStart, int xEnd, int y, int yBottom, bool isLink) const
{
	y += UNDERLINE_OFFSET_Y;
	if (y >= yBottom) return;
	HPEN hPen = CreatePen(PS_SOLID, 1, isLink ? colorLinkHover : colorText);
	HGDIOBJ oldPen = SelectObject(dc, hPen);
	MoveToEx(dc, xStart - UNDERLINE_OFFSET_X, y, nullptr);
	LineTo(dc, xEnd, y);
	SelectObject(dc, oldPen);
	DeleteObject(hPen);
}

LRESULT RichTextLabel::onPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	PAINTSTRUCT ps;
	HDC paintDC = BeginPaint(&ps);

	CRect rc;
	GetClientRect(&rc);
	int width = rc.Width();
	int height = rc.Height();
	if (width <= 0 || height <= 0)
	{
		EndPaint(&ps);
		return 0;
	}

	if (width != bitmapWidth || height != bitmapHeight)
	{
		cleanup();
		memDC = CreateCompatibleDC(paintDC);
		memBitmap = CreateCompatibleBitmap(paintDC, width, height);
		bitmapWidth = width;
		bitmapHeight = height;
		oldBitmap = (HBITMAP) SelectObject(memDC, memBitmap);
	}

	if (calcSizeFlag)
		calcSize(memDC);
	if (layoutFlag)
		layout(memDC, width, height);

	if (!bgBrush) bgBrush = CreateSolidBrush(colorBackground);
	FillRect(memDC, &rc, bgBrush);
	SetBkMode(memDC, TRANSPARENT);

	HRGN hrgn = nullptr;
	int bottom = rc.bottom;
	if (margins[MARGIN_LEFT] || margins[MARGIN_TOP] || margins[MARGIN_RIGHT] || margins[MARGIN_BOTTOM])
	{
		RECT rcClip = { margins[MARGIN_LEFT], margins[MARGIN_TOP], width - margins[MARGIN_RIGHT], height - margins[MARGIN_BOTTOM] };
		hrgn = CreateRectRgnIndirect(&rcClip);
		SelectClipRgn(memDC, hrgn);
		bottom -= margins[MARGIN_BOTTOM];
	}

	int underlinePos = -1;
	HFONT oldFont = nullptr;
	for (size_t i = 0; i < fragments.size(); ++i)
	{
		const Fragment& fragment = fragments[i];
		if (fragment.y >= bottom) break;
		if (fragment.text[0] == _T('\n')) continue;
		const StyleInstance& style = styles[fragment.style];
		HFONT prevFont = (HFONT) SelectObject(memDC, style.hFont);
		if (!oldFont) oldFont = prevFont;
		COLORREF color = colorText;
		if (fragment.link != -1)
			color = fragment.link == hoverLink ? colorLinkHover : colorLink;
		SetTextColor(memDC, color);
		ExtTextOut(memDC, fragment.x, fragment.y, ETO_CLIPPED, &rc, fragment.text.c_str(), static_cast<UINT>(fragment.text.length()), nullptr);
		if ((style.font.lfUnderline || (fragment.link != -1 && fragment.link == hoverLink)) && underlinePos == -1)
			underlinePos = static_cast<int>(i);
	}
	if (underlinePos != -1)
	{
		const Fragment& fragment = fragments[underlinePos];
		const StyleInstance& style = styles[fragment.style];
		int y = fragment.y + style.metric.tmAscent;
		int xStart = fragment.x;
		int xEnd = 0;
		bool isLink = fragment.link != -1;
		for (size_t i = underlinePos; i < fragments.size(); ++i)
		{
			const Fragment& fragment = fragments[i];
			const StyleInstance& style = styles[fragment.style];
			if (style.font.lfUnderline || (fragment.link != -1 && fragment.link == hoverLink))
			{
				int yBaseLine = fragment.y + style.metric.tmAscent;
				if (xStart == -1)
				{
					y = yBaseLine;
					xStart = fragment.x;
					xEnd = xStart + fragment.width - fragment.spaceWidth;
					isLink = fragment.link != -1;
				}
				else if (yBaseLine == y)
				{
					xEnd = fragment.x + fragment.width - fragment.spaceWidth;
				}
				else
				{
					if (fragment.x > xEnd) xEnd = fragment.x;
					if (y >= bottom) break;
					drawUnderline(memDC, xStart, xEnd, y, bottom, isLink);
					y = yBaseLine;
					xStart = fragment.x;
					xEnd = fragment.x + fragment.width - fragment.spaceWidth;
					isLink = fragment.link != -1;
				}
			}
			else if (xStart != -1)
			{
				if (y >= bottom) break;
				drawUnderline(memDC, xStart, xEnd, y, bottom, isLink);
				xStart = -1;
			}
		}
		if (xStart != -1 && y < bottom)
		{
			const Fragment& fragment = fragments.back();
			drawUnderline(memDC, xStart, fragment.x + fragment.width - fragment.spaceWidth,
				y, bottom, isLink);
		}
	}

	if (hrgn)
	{
		SelectClipRgn(memDC, nullptr);
		DeleteObject(hrgn);
	}
	if (oldFont) SelectObject(memDC, oldFont);
	BitBlt(paintDC, rc.left, rc.top, width, height, memDC, 0, 0, SRCCOPY);

	EndPaint(&ps);
	return 0;
}

LRESULT RichTextLabel::onSize(UINT, WPARAM, LPARAM, BOOL&)
{
	layoutFlag = true;
	return 0;
}

LRESULT RichTextLabel::onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);

	CRect rc;
	GetClientRect(&rc);

	int newLink = -1;
	int bottom = rc.bottom - margins[MARGIN_BOTTOM];
	for (size_t i = 0; i < fragments.size(); ++i)
	{
		const Fragment& fragment = fragments[i];
		if (fragment.y >= bottom) break;
		if (fragment.text[0] == _T('\n')) continue;
		if (fragment.link == -1) continue;
		if (x >= fragment.x && x < fragment.x + fragment.width &&
		    y >= fragment.y && y < fragment.y + fragment.height)
		{
			newLink = fragment.link;
			break;
		}
	}

	if (newLink != hoverLink)
	{
		hoverLink = newLink;
		clickLink = -1;
		SendMessage(WM_SETCURSOR, (WPARAM) m_hWnd, 0);
		Invalidate();
		if (hoverLink != -1)
		{
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = m_hWnd;
			_TrackMouseEvent(&tme);
		}
	}

	return TRUE;
}

LRESULT RichTextLabel::onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	clickLink = -1;
	if (hoverLink != -1)
	{
		hoverLink = -1;
		Invalidate();
	}
	return TRUE;
}

LRESULT RichTextLabel::onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
{
	clickLink = hoverLink;
	return 0;
}

LRESULT RichTextLabel::onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
{
	if (clickLink >= 0 && clickLink < static_cast<int>(links.size()))
	{
		HWND parent = GetParent();
		::SendMessage(parent, WMU_LINK_ACTIVATED, 0, (LPARAM) links[clickLink].c_str());
		clickLink = -1;
	}
	return 0;
}

LRESULT RichTextLabel::onSetCursor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetCursor(hoverLink != -1 ? linkCursor : defaultCursor);
	return TRUE;
}

static inline bool isWhiteSpace(TCHAR c)
{
	return c == _T(' ') || c == _T('\t') || c == _T('\n') || c == _T('\r');
}

inline bool styleEq(const RichTextLabel::Style& s1, const RichTextLabel::Style& s2)
{
	return s1.font.lfWeight == s2.font.lfWeight &&
		s1.font.lfHeight == s2.font.lfHeight &&
		s1.font.lfItalic == s2.font.lfItalic &&
		s1.font.lfUnderline == s2.font.lfUnderline;
}

int RichTextLabel::getStyle()
{
	if (!initialStyle.font.lfHeight)
	{
		HFONT font = GetFont();
		GetObject(font, sizeof(LOGFONT), &initialStyle.font);
		if (fontHeight) initialStyle.font.lfHeight = fontHeight;
	}

	int style = -1;
	const Style& currentStyle = tagStack.empty() ? initialStyle : tagStack.back().style;
	for (vector<StyleInstance>::size_type i = 0; i < styles.size(); ++i)
		if (styleEq(currentStyle, styles[i]))
		{
			style = static_cast<int>(i);
			break;
		}
	if (style < 0)
	{
		StyleInstance newStyle;
		newStyle.font = currentStyle.font;
		newStyle.hFont = nullptr;
		style = static_cast<int>(styles.size());
		styles.push_back(newStyle);
	}
	return style;
}

void RichTextLabel::addFragment(tstring::size_type start, tstring::size_type end, bool whiteSpace)
{
	int style = getStyle();
	Fragment fragment;
	fragment.text = text.substr(start, end - start);
	unescape(fragment.text);
	if (whiteSpace)
		fragment.text += _T(' ');
	if (!fragments.empty())
	{
		Fragment& lastFragment = fragments.back();
		if (lastFragment.style == style && lastFragment.link == parsingLink &&
		    lastFragment.text.back() != _T(' '))
		{
			lastFragment.text += fragment.text;
			return;
		}
	}
	fragment.style = style;
	fragment.x = fragment.y = 0;
	fragment.width = fragment.height = fragment.spaceWidth = 0;
	fragment.link = parsingLink;
	fragments.push_back(fragment);
	lastFragment = static_cast<int>(fragments.size()) - 1;
}

void RichTextLabel::initialize()
{
	if (!linkCursor)
		linkCursor = LoadCursor(nullptr, IDC_HAND);
	if (!defaultCursor)
		defaultCursor = LoadCursor(nullptr, IDC_ARROW);

	memset(&initialStyle, 0, sizeof(initialStyle));
	tagStack.clear();
	fragments.clear();
	links.clear();
	clearStyles();
	parsingLink = -1;
	lastFragment = -1;

	tstring::size_type start = tstring::npos;
	for (tstring::size_type i = 0; i < text.length(); ++i)
	{
		if (isWhiteSpace(text[i]))
		{
			if (lastFragment != -1 && fragments[lastFragment].text.back() != _T(' '))
				fragments[lastFragment].text += _T(' ');
			else if (!fragments.empty() && start == tstring::npos)
			{
				addFragment(i, i, true);
				continue;
			}
			if (start == tstring::npos) continue;
			addFragment(start, i, true);
			start = tstring::npos;
			continue;
		}
		if (text[i] == _T('<'))
		{
			if (start != tstring::npos)
			{
				addFragment(start, i, false);
				start = tstring::npos;
			}
			auto tagEnd = text.find(_T('>'), i + 1);
			if (tagEnd == tstring::npos) break;
			processTag(text.substr(i + 1, tagEnd - (i + 1)));
			i = tagEnd;
			lastFragment = -1;
			continue;
		}
		if (start == tstring::npos)
			start = i;
	}
	if (start != tstring::npos)
		addFragment(start, text.length(), false);
	calcSizeFlag = layoutFlag = true;
	tagStack.clear();
}

void RichTextLabel::getTextSize(HDC hdc, const tstring& text, int len, SIZE& size, const RichTextLabel::StyleInstance& style)
{
	if (len == 0)
	{
		size.cx = size.cy = 0;
		return;
	}
	GetTextExtentPoint32(hdc, text.c_str(), len, &size);
}

void RichTextLabel::calcSize(HDC hdc)
{
	HFONT oldFont = nullptr;
	for (auto& fragment : fragments)
	{
		StyleInstance& style = styles[fragment.style];
		bool getMetrics = false;
		if (!style.hFont)
		{
			getMetrics = true;
			auto underline = style.font.lfUnderline;
			style.font.lfUnderline = FALSE;
			style.hFont = CreateFontIndirect(&style.font);
			style.font.lfUnderline = underline;
		}
		if (fragment.text[0] == _T('\n')) continue;
		HFONT prevFont = (HFONT) SelectObject(hdc, style.hFont);
		if (!oldFont) oldFont = prevFont;
		if (getMetrics) GetTextMetrics(hdc, &style.metric);
		SIZE size;
		int len = static_cast<int>(fragment.text.length());
		getTextSize(hdc, fragment.text, len, size, style);
		fragment.width = size.cx;
		fragment.height = size.cy;
		if (fragment.text.back() == _T(' '))
		{
			while (len && fragment.text[len-1] == _T(' ')) len--;
			getTextSize(hdc, fragment.text, len, size, style);
			fragment.spaceWidth = fragment.width - size.cx;
		}
		else
			fragment.spaceWidth = 0;
	}
	if (oldFont) SelectObject(hdc, oldFont);
	calcSizeFlag = false;
	layoutFlag = true;
}

void RichTextLabel::layout(HDC hdc, int width, int height)
{
	width -= margins[MARGIN_RIGHT];
	int x = margins[MARGIN_LEFT];
	int y = margins[MARGIN_TOP];
	int maxAscent = 0;
	int maxDescent = 0;
	size_t lineStart = 0;
	for (size_t i = 0; i < fragments.size(); ++i)
	{
		Fragment& fragment = fragments[i];
		const StyleInstance& style = styles[fragment.style];
		bool lineBreak = fragment.text[0] == _T('\n');
		if (lineBreak || (lineStart < i && x + fragment.width - fragment.spaceWidth > width))
		{
			if (lineBreak)
				updatePosition(style, maxAscent, maxDescent);
			for (size_t j = lineStart; j < i; ++j)
			{
				Fragment& fragment = fragments[j];
				int ascent = styles[fragment.style].metric.tmAscent;
				fragment.y = y + maxAscent - ascent;
			}
			if (centerFlag) centerLine(lineStart, i, width);
			x = margins[MARGIN_LEFT];
			y += maxAscent + maxDescent;
			maxAscent = maxDescent = 0;
			lineStart = i;
			if (lineBreak)
			{
				lineStart++;
				fragment.x = margins[MARGIN_LEFT];
				continue;
			}
		}
		updatePosition(style, maxAscent, maxDescent);
		fragment.x = x;
		x += fragment.width;
	}
	for (size_t j = lineStart; j < fragments.size(); ++j)
	{
		Fragment& fragment = fragments[j];
		const StyleInstance& style = styles[fragment.style];
		int ascent = style.metric.tmAscent;
		fragment.y = y + maxAscent - ascent;
	}
	if (centerFlag) centerLine(lineStart, fragments.size(), width);
	layoutFlag = false;
}

void RichTextLabel::updatePosition(const RichTextLabel::StyleInstance& style, int& maxAscent, int& maxDescent) const
{
	int top = style.metric.tmAscent;
	if (top > maxAscent) maxAscent = top;
	int bottom = style.metric.tmDescent;
	if (bottom > maxDescent) maxDescent = bottom;
}

void RichTextLabel::centerLine(size_t start, size_t end, int width)
{
	if (start >= end) return;
	const Fragment& fragment = fragments[end-1];
	int offset = (width - (fragment.x + fragment.width - fragment.spaceWidth)) / 2;
	for (size_t i = start; i < end; ++i)
		fragments[i].x += offset;
}

struct Attribute
{
	string name;
	string value;
};

static void parseTag(const string& s, string& tag, vector<Attribute>& attr)
{
	attr.clear();
	tag.clear();
	const char* c = s.c_str();
	bool getValue = false;
	Attribute a;
	string::size_type i = 0;
	while (i < s.length())
	{
		if (isWhiteSpace(c[i]))
		{
			++i;
			continue;
		}
		if (tag.empty())
		{
			string::size_type j = i + 1;
			while (j < s.length() && !isWhiteSpace(c[j])) ++j;
			tag = s.substr(i, j - i);
			Text::asciiMakeLower(tag);
			i = j;
			continue;
		}
		if (c[i] == '=' && !getValue && !attr.empty())
		{
			getValue = true;
			++i;
			continue;
		}
		if (getValue)
		{
			string::size_type j;
			if (c[i] == '\'')
			{
				j = s.find('\'', ++i);
				if (j == string::npos) break;
			}
			else if (c[i] == '"')
			{
				j = s.find('"', ++i);
				if (j == string::npos) break;
			}
			else
			{
				j = i;
				while (j < s.length() && !isWhiteSpace(c[j])) ++j;
			}
			attr.back().value = s.substr(i, j - i);
			getValue = false;
			i = j + 1;
			continue;
		}
		string::size_type j = i;
		while (j < s.length() && !isWhiteSpace(c[j]) && c[j] != '=') ++j;
		a.name = s.substr(i, j - i);
		if (a.name.empty()) break;
		Text::asciiMakeLower(a.name);
		attr.push_back(a);
		i = j;
		if (c[j] == '=')
		{
			getValue = true;
			++i;
		}
	}
}

static const string& getAttribValue(const vector<Attribute>& attr, const string& name)
{
	for (const auto& v : attr)
		if (v.name == name) return v.value;
	return Util::emptyString;
}

static void changeFontSize(LOGFONT& lf, int delta)
{
	int val = abs(lf.lfHeight) + delta;
	if (val < MIN_FONT_SIZE) val = MIN_FONT_SIZE; else
	if (val > MAX_FONT_SIZE) val = MAX_FONT_SIZE;
	if (val < 8) val = 8;
	if (lf.lfHeight < 0) val = -val;
	lf.lfHeight = val;
}

static void setFontSize(LOGFONT& lf, int val)
{
	if (val < MIN_FONT_SIZE) val = MIN_FONT_SIZE; else
	if (val > MAX_FONT_SIZE) val = MAX_FONT_SIZE;
	lf.lfHeight = -val;
}

void RichTextLabel::processTag(const tstring& data)
{
	string tag;
	vector<Attribute> attr;
	parseTag(Text::fromT(data), tag, attr);
	if (tag.empty()) return;
	if (tag == "br")
	{
		Fragment fragment;
		fragment.text = _T('\n');
		fragment.style = getStyle();
		fragment.x = fragment.y = 0;
		fragment.width = fragment.height = fragment.spaceWidth = 0;
		fragments.push_back(fragment);
		return;
	}
	if (tag[0] == '/')
	{
		tag.erase(0, 1);
		if (tagStack.empty() || tagStack.back().tag != tag) return;
		tagStack.pop_back();
		parsingLink = tagStack.empty() ? -1 : tagStack.back().link;
		return;
	}
	if (tag == "b")
	{
		TagStackItem item = newTagStackItem(tag);
		item.style.font.lfWeight = FW_BOLD;
		tagStack.push_back(item);
		return;
	}
	if (tag == "u")
	{
		TagStackItem item = newTagStackItem(tag);
		item.style.font.lfUnderline = TRUE;
		tagStack.push_back(item);
		return;
	}
	if (tag == "i" || tag == "em")
	{
		TagStackItem item = newTagStackItem(tag);
		item.style.font.lfItalic = TRUE;
		tagStack.push_back(item);
		return;
	}
	if (tag == "font")
	{
		TagStackItem item = newTagStackItem(tag);
		tagStack.push_back(item);
		const string& sizeValue = getAttribValue(attr, "size");
		if (!sizeValue.empty())
		{
			if (sizeValue[0] == '+')
			{
				auto val = Util::toUInt32(sizeValue.c_str() + 1);
				changeFontSize(tagStack.back().style.font, val);
			}
			else if (sizeValue[0] == '-')
			{
				auto val = Util::toUInt32(sizeValue.c_str() + 1);
				changeFontSize(tagStack.back().style.font, -(int) val);
			}
			else
			{
				auto val = Util::toUInt32(sizeValue);
				if (val) setFontSize(tagStack.back().style.font, val);
			}
		}
		return;
	}
	if (tag == "a")
	{
		TagStackItem item = newTagStackItem(tag);
		tagStack.push_back(item);
		const string& hrefValue = getAttribValue(attr, "href");
		parsingLink = static_cast<int>(links.size());
		tstring href = Text::toT(hrefValue);
		unescape(href);
		links.push_back(href);
		tagStack.back().link = parsingLink;
		return;
	}
}

RichTextLabel::TagStackItem RichTextLabel::newTagStackItem(const string& tag)
{
	TagStackItem item;
	item.tag = tag;
	item.link = -1;
	if (tagStack.empty())
	{
		if (!initialStyle.font.lfHeight)
		{
			HFONT font = GetFont();
			GetObject(font, sizeof(LOGFONT), &initialStyle.font);
			if (fontHeight) initialStyle.font.lfHeight = fontHeight;
		}
		item.style = initialStyle;
	}
	else
		item.style = tagStack.back().style;
	return item;
}

void RichTextLabel::unescape(tstring& text)
{
	tstring::size_type i = 0;
	while ((i = text.find(_T('&'), i)) != tstring::npos)
	{
		if (text.compare(i + 1, 3, _T("lt;"), 3) == 0)
			text.replace(i, 4, 1, _T('<'));
		else if (text.compare(i + 1, 4, _T("amp;"), 4) == 0)
			text.replace(i, 5, 1, _T('&'));
		else if (text.compare(i + 1, 3, _T("gt;"), 3) == 0)
			text.replace(i, 4, 1, _T('>'));
		else if (text.compare(i + 1, 5, _T("apos;"), 5) == 0)
			text.replace(i, 6, 1, _T('\''));
		else if (text.compare(i + 1, 5, _T("quot;"), 5) == 0)
			text.replace(i, 6, 1, _T('"'));
		i++;
	}
}

void RichTextLabel::cleanup()
{
	if (memDC)
	{
		if (oldBitmap) SelectObject(memDC, oldBitmap);
		DeleteDC(memDC);
		memDC = nullptr;
	}
	if (memBitmap)
	{
		DeleteObject(memBitmap);
		memBitmap = nullptr;
	}
	if (bgBrush)
	{
		DeleteObject(bgBrush);
		bgBrush = nullptr;
	}
}

void RichTextLabel::clearStyles()
{
	for (auto& style : styles)
		if (style.hFont) DeleteObject(style.hFont);
	styles.clear();
}

void RichTextLabel::setTextColor(COLORREF color)
{
	if (color == colorText) return;
	colorText = color;
	if (m_hWnd) Invalidate();
}

void RichTextLabel::setBackgroundColor(COLORREF color)
{
	if (color == colorBackground) return;
	colorBackground = color;
	if (bgBrush)
	{
		DeleteObject(bgBrush);
		bgBrush = nullptr;
	}
	if (m_hWnd) Invalidate();
}

void RichTextLabel::setLinkColor(COLORREF color, COLORREF colorHover)
{
	if (color == colorLink && colorHover == colorLinkHover) return;
	colorLink = color;
	colorLinkHover = colorHover;
	if (m_hWnd) Invalidate();
}

void RichTextLabel::setMargins(int left, int top, int right, int bottom)
{
	bool update = false;
	if (margins[MARGIN_LEFT] != left)
	{
		margins[MARGIN_LEFT] = left;
		update = true;
	}
	if (margins[MARGIN_RIGHT] != right)
	{
		margins[MARGIN_RIGHT] = right;
		update = true;
	}
	if (margins[MARGIN_TOP] != top)
	{
		margins[MARGIN_TOP] = top;
		update = true;
	}
	if (margins[MARGIN_BOTTOM] != bottom)
	{
		margins[MARGIN_BOTTOM] = bottom;
		update = true;
	}
	if (!update) return;
	layoutFlag = true;
	if (m_hWnd) Invalidate();
}

void RichTextLabel::setCenter(bool center)
{
	if (center == centerFlag) return;
	centerFlag = center;
	layoutFlag = true;
	if (m_hWnd) Invalidate();
}

void RichTextLabel::setTextHeight(int height)
{
	int value = 0;
	if (height)
	{
		value = abs(height);
		if (value < MIN_FONT_SIZE) value = MIN_FONT_SIZE; else
		if (value > MAX_FONT_SIZE) value = MAX_FONT_SIZE;
		if (height < 0) value = -value;
	}
	if (fontHeight == value) return;
	fontHeight = value;
	initialize();
	if (m_hWnd) Invalidate();
}
