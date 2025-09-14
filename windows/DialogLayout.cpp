#include "stdafx.h"
#include "DialogLayout.h"
#include "ControlTypes.h"
#include "WinUtil.h"
#include "../client/Text.h"
#include <algorithm>

using namespace DialogLayout;

enum
{
	FLAG_WIDTH   = 1,
	FLAG_HEIGHT  = 2,
	FLAG_LEFT    = 4,
	FLAG_RIGHT   = 8,
	FLAG_TOP     = 16,
	FLAG_BOTTOM  = 32,
	FLAG_RC_ORIG = 64
};

struct ItemInfo
{
	HWND hwnd;
	int flags;
	int type;
	int drawFlags;
	int width;
	int height;
	RECT rc;
	RECT rcOrig;
};

class DialogInfo
{
	HWND hwnd;
	int xdu, ydu;
	int dlgWidth, dlgHeight;
	int checkBoxGap;
	SIZE checkBoxSize;
	SIZE radioSize;
	int comboBoxHeight;
	bool hasTheme;
	const Options* const options;
	HFONT hFont;

	void getDialogUnits()
	{
		if (!WinUtil::getDialogUnits(hwnd, nullptr, xdu, ydu))
		{
			dcassert(0);
			xdu = 1;
			ydu = 1;
		}
	}

	void getDialogSize()
	{
		if (options && options->width && options->height)
		{
			dlgWidth = getXSize(options->width);
			dlgHeight = getYSize(options->height);
			return;
		}
		RECT rc;
		GetClientRect(hwnd, &rc);
		dlgWidth = rc.right;
		dlgHeight = rc.bottom;
	}

	void getTextSize(HDC hdc, HWND ctrlHwnd, int drawFlags, SIZE& size);

public:
	DialogInfo(HWND hwnd, const Options* options) : hwnd(hwnd), xdu(-1), ydu(-1),
		dlgWidth(-1), dlgHeight(-1), checkBoxGap(-1), comboBoxHeight(-1), hasTheme(true),
		options(options), hFont(nullptr) {}

	int getXDialogUnits()
	{
		if (xdu < 0) getDialogUnits();
		return xdu;
	}

	int getYDialogUnits()
	{
		if (ydu < 0) getDialogUnits();
		return ydu;
	}

	int getXSize(int size)
	{
		int unit = size & 1;
		size >>= 1;
		if (unit == 1) size = getXSizeDU(size);
		return size;
	}

	int getYSize(int size)
	{
		int unit = size & 1;
		size >>= 1;
		if (unit == 1) size = getYSizeDU(size);
		return size;
	}

	int getXSizeDU(int size)
	{
		int du = getXDialogUnits();
		return (size * du + 2) / 4;
	}

	int getYSizeDU(int size)
	{
		int du = getYDialogUnits();
		return (size * du + 4) / 8;
	}

	int getDialogWidth()
	{
		if (dlgWidth < 0) getDialogSize();
		return dlgWidth;
	}

	int getDialogHeight()
	{
		if (dlgHeight < 0) getDialogSize();
		return dlgHeight;
	}

	bool getSizeFromContent(HWND hwnd, int type, int drawFlags, SIZE& size);
	void getOrigRect(ItemInfo& ii) const;
};

static int getControlType(HWND hwnd, int& drawFlags)
{
	TCHAR name[64];
	if (!GetClassName(hwnd, name, _countof(name))) return WinUtil::CTRL_UNKNOWN;
	int style = GetWindowLong(hwnd, GWL_STYLE);
	int type = WinUtil::CTRL_UNKNOWN;
	drawFlags = DT_SINGLELINE;
	if (!_tcsicmp(name, _T("static")))
	{
		switch (style & SS_TYPEMASK)
		{
			case SS_LEFT:
			case SS_CENTER:
			case SS_RIGHT:
			case SS_SIMPLE:
			case SS_LEFTNOWORDWRAP:
				type = WinUtil::CTRL_TEXT;
				break;
			case SS_ICON:
			case SS_BITMAP:
				type = WinUtil::CTRL_IMAGE;
				break;
			default:
				return WinUtil::CTRL_UNKNOWN;
		}
		if (style & SS_NOPREFIX) drawFlags |= DT_NOPREFIX;
	}
	else if (!_tcsicmp(name, _T("button")))
	{
		switch (style & BS_TYPEMASK)
		{
			case BS_CHECKBOX:
			case BS_AUTOCHECKBOX:
			case BS_3STATE:
			case BS_AUTO3STATE:
				type = (style & BS_PUSHLIKE) ? WinUtil::CTRL_BUTTON : WinUtil::CTRL_CHECKBOX;
				break;
			case BS_RADIOBUTTON:
			case BS_AUTORADIOBUTTON:
				type = (style & BS_PUSHLIKE) ? WinUtil::CTRL_BUTTON : WinUtil::CTRL_RADIO;
				break;
			case BS_GROUPBOX:
				type = WinUtil::CTRL_GROUPBOX;
				break;
			default:
				type = WinUtil::CTRL_BUTTON;
		}
		if (style & BS_MULTILINE) drawFlags &= ~DT_SINGLELINE;
	}
	else if (!_tcsicmp(name, _T("combobox")))
	{
		if (style & CBS_DROPDOWN)
			type = WinUtil::CTRL_COMBOBOX;
	}
	else if (!_tcsicmp(name, _T("edit")))
	{
		type = WinUtil::CTRL_EDIT;
	}
	return type;
}

static void alignHorizontal(DialogInfo& di, ItemInfo* ii, const RECT* groups, int i, const Align* align, int side)
{
	dcassert(align->side == SIDE_LEFT || align->side == SIDE_RIGHT);
	int x;
	int j = align->index;
	if (j == 0)
	{
		if (align->side == SIDE_LEFT) x = 0; else x = di.getDialogWidth();
	}
	else if (j > 0)
	{
		if (j & INDEX_RELATIVE)
			j = i - (j ^ INDEX_RELATIVE);
		else
			--j;
		dcassert(j < i);
		if (align->side == SIDE_LEFT) x = ii[j].rc.left; else x = ii[j].rc.right;
	}
	else
	{
		if (align->side == SIDE_LEFT) x = groups[-j-1].left; else x = groups[-j-1].right;
	}
	int offset = di.getXSize(align->offset);
	if (side == SIDE_LEFT)
	{
		ii[i].rc.left = x + offset;
		ii[i].flags |= FLAG_LEFT;
	}
	else
	{
		ii[i].rc.right = x - offset;
		ii[i].flags |= FLAG_RIGHT;
	}
}

static void alignVertical(DialogInfo& di, ItemInfo* ii, const RECT* groups, int i, const Align* align, int side)
{
	dcassert(align->side == SIDE_TOP || align->side == SIDE_BOTTOM);
	int y;
	int j = align->index;
	if (j == 0)
	{
		if (align->side == SIDE_TOP) y = 0; else y = di.getDialogHeight();
	}
	else if (j > 0)
	{
		if (j & INDEX_RELATIVE)
			j = i - (j ^ INDEX_RELATIVE);
		else
			--j;
		dcassert(j < i);
		if (align->side == SIDE_TOP) y = ii[j].rc.top; else y = ii[j].rc.bottom;
	}
	else
	{
		if (align->side == SIDE_TOP) y = groups[-j-1].top; else y = groups[-j-1].bottom;
	}
	int offset = di.getYSize(align->offset);
	if (side == SIDE_TOP)
	{
		ii[i].rc.top = y + offset;
		ii[i].flags |= FLAG_TOP;
	}
	else
	{
		ii[i].rc.bottom = y - offset;
		ii[i].flags |= FLAG_BOTTOM;
	}
}

void DialogInfo::getOrigRect(ItemInfo& ii) const
{
	if (!(ii.flags & FLAG_RC_ORIG))
	{
		GetWindowRect(ii.hwnd, &ii.rcOrig);
		POINT* pt = (POINT*) &ii.rcOrig;
		ScreenToClient(hwnd, pt);
		ScreenToClient(hwnd, pt + 1);
		ii.flags |= FLAG_RC_ORIG;
	}
}

void DialogLayout::layout(HWND hWnd, const Item* items, int count, const Options* options)
{
	DialogInfo di(hWnd, options);
	ItemInfo* ii = new ItemInfo[count];

	int maxGroup = 0;
	RECT* groups = nullptr;
	for (int i = 0; i < count; i++)
		if (items[i].group > maxGroup) maxGroup = items[i].group;
	if (maxGroup)
	{
		groups = new RECT[maxGroup];
		for (int i = 0; i < maxGroup; i++)
		{
			groups[i].left = groups[i].top = INT_MAX;
			groups[i].right = groups[i].bottom = INT_MIN;
		}
	}

	for (int i = 0; i < count; i++)
	{
		ItemInfo& cur = ii[i];
		if (items[i].flags & FLAG_PLACEHOLDER)
		{
			cur.hwnd = nullptr;
			continue;
		}
		cur.hwnd = (items[i].flags & FLAG_HWND) ? (HWND) items[i].id : GetDlgItem(hWnd, items[i].id);
		dcassert(cur.hwnd);
		if (items[i].flags & FLAG_TRANSLATE)
		{
			tstring text;
			WinUtil::getWindowText(cur.hwnd, text);
			if (text.length() >= 2 && text[0] == _T('@'))
			{
				int stringId = ResourceManager::getStringByName(Text::fromT(text.substr(1)));
				if (stringId != -1)
					SetWindowText(cur.hwnd, CTSTRING_I((ResourceManager::Strings) stringId));
			}
		}
		cur.flags = 0;
		cur.type = getControlType(cur.hwnd, cur.drawFlags);
		if (cur.type == WinUtil::CTRL_COMBOBOX) di.getOrigRect(cur);
		if (items[i].width >= 0)
		{
			cur.width = di.getXSize(items[i].width);
			cur.flags |= FLAG_WIDTH;
		}
		if (items[i].height >= 0)
		{
			cur.height = di.getYSize(items[i].height);
			cur.flags |= FLAG_HEIGHT;
		}
		if (items[i].width == AUTO || items[i].height == AUTO)
		{
			SIZE size;
			di.getSizeFromContent(cur.hwnd, cur.type, cur.drawFlags, size);
			if (items[i].width == AUTO)
			{
				cur.width = size.cx;
				cur.flags |= FLAG_WIDTH;
			}
			if (items[i].height == AUTO)
			{
				cur.height = size.cy;
				cur.flags |= FLAG_HEIGHT;
			}
		}
		if (items[i].left)
			alignHorizontal(di, ii, groups, i, items[i].left, SIDE_LEFT);
		if (items[i].right)
			alignHorizontal(di, ii, groups, i, items[i].right, SIDE_RIGHT);
		if (items[i].top)
			alignVertical(di, ii, groups, i, items[i].top, SIDE_TOP);
		if (items[i].bottom)
			alignVertical(di, ii, groups, i, items[i].bottom, SIDE_BOTTOM);
		if ((cur.flags & (FLAG_LEFT | FLAG_RIGHT)) != (FLAG_LEFT | FLAG_RIGHT))
		{
			if (!(cur.flags & FLAG_WIDTH))
			{
				di.getOrigRect(cur);
				cur.width = cur.rcOrig.right - cur.rcOrig.left;
				cur.flags |= FLAG_WIDTH;
			}
			if (cur.flags & (FLAG_LEFT | FLAG_RIGHT))
			{
				if (cur.flags & FLAG_LEFT)
					cur.rc.right = cur.rc.left + cur.width;
				else
					cur.rc.left = cur.rc.right - cur.width;
			}
			else
			{
				di.getOrigRect(cur);
				cur.rc.left = cur.rcOrig.left;
				cur.rc.right = cur.rc.left + cur.width;
			}
		}
		if ((cur.flags & (FLAG_TOP | FLAG_BOTTOM)) != (FLAG_TOP | FLAG_BOTTOM))
		{
			if (!(cur.flags & FLAG_HEIGHT))
			{
				di.getOrigRect(cur);
				cur.height = cur.rcOrig.bottom - cur.rcOrig.top;
				cur.flags |= FLAG_HEIGHT;
			}
			if (cur.flags & (FLAG_TOP | FLAG_BOTTOM))
			{
				if (cur.flags & FLAG_TOP)
					cur.rc.bottom = cur.rc.top + cur.height;
				else
					cur.rc.top = cur.rc.bottom - cur.height;
			}
			else
			{
				di.getOrigRect(cur);
				cur.rc.top = cur.rcOrig.top;
				cur.rc.bottom = cur.rc.top + cur.height;
			}
		}
		if (items[i].group)
		{
			RECT& gr = groups[items[i].group-1];
			if (cur.rc.left < gr.left) gr.left = cur.rc.left;
			if (cur.rc.right > gr.right) gr.right = cur.rc.right;
			if (cur.rc.top < gr.top) gr.top = cur.rc.top;
			if (cur.rc.bottom > gr.bottom) gr.bottom = cur.rc.bottom;
		}
	}

	UINT flags = SWP_NOZORDER;
	if (options && options->show) flags |= SWP_SHOWWINDOW;
	HDWP dwp = BeginDeferWindowPos(count);
	for (int i = 0; i < count; i++)
	{
		ItemInfo& cur = ii[i];
		if (!cur.hwnd) continue;
		if ((cur.flags & FLAG_RC_ORIG) &&
		    cur.rc.left == cur.rcOrig.left && cur.rc.right == cur.rcOrig.right &&
		    cur.rc.top == cur.rcOrig.top && cur.rc.bottom == cur.rcOrig.bottom) continue;
		DeferWindowPos(dwp, cur.hwnd, nullptr, cur.rc.left, cur.rc.top,
			cur.rc.right - cur.rc.left,
			cur.type == WinUtil::CTRL_COMBOBOX ? cur.rcOrig.bottom - cur.rcOrig.top : cur.rc.bottom - cur.rc.top,
			flags);
	}
	EndDeferWindowPos(dwp);

	delete[] ii;
	delete[] groups;
}

void DialogInfo::getTextSize(HDC dc, HWND ctrlHwnd, int drawFlags, SIZE& size)
{
	tstring text;
	WinUtil::getWindowText(ctrlHwnd, text);
	RECT rc;
	if (!hFont)
	{
		hFont = (HFONT) SendMessage(hwnd, WM_GETFONT, 0, 0);
		if (!hFont)
			hFont = (HFONT) SendMessage(ctrlHwnd, WM_GETFONT, 0, 0);
	}
	HGDIOBJ prevFont = SelectObject(dc, hFont);
	DrawText(dc, text.c_str(), (int) text.length(), &rc, drawFlags | DT_NOCLIP | DT_CALCRECT);
	SelectObject(dc, prevFont);
	size.cx = rc.right - rc.left;
	size.cy = rc.bottom - rc.top;
}

bool DialogInfo::getSizeFromContent(HWND hwnd, int type, int drawFlags, SIZE& size)
{
	size.cx = size.cy = 0;
	switch (type)
	{
		case WinUtil::CTRL_TEXT:
		{
			HDC dc = GetDC(hwnd);
			if (!dc) return false;
			getTextSize(dc, hwnd, drawFlags, size);
			ReleaseDC(hwnd, dc);
			break;
		}
		case WinUtil::CTRL_IMAGE:
			size.cx = size.cy = 16;
			break;
		case WinUtil::CTRL_BUTTON:
		{
			HDC dc = GetDC(hwnd);
			if (!dc) return false;
			getTextSize(dc, hwnd, drawFlags, size);
			size.cx += getXSizeDU(12);
			if (drawFlags & DT_SINGLELINE)
				size.cy = getYSizeDU(14);
			else
				size.cy += getYSizeDU(6);
			break;
		}
		case WinUtil::CTRL_CHECKBOX:
		case WinUtil::CTRL_RADIO:
		{
			HDC dc = GetDC(hwnd);
			if (!dc) return false;
			getTextSize(dc, hwnd, drawFlags, size);
			if (checkBoxGap < 0)
			{
				HTHEME hTheme = OpenThemeData(hwnd, VSCLASS_BUTTON);
				if (!hTheme || FAILED(GetThemePartSize(hTheme, dc, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, nullptr, TS_TRUE, &checkBoxSize)))
					checkBoxSize.cx = checkBoxSize.cy = GetDeviceCaps(dc, LOGPIXELSX) / 8 + 1;
				if (!hTheme || FAILED(GetThemePartSize(hTheme, dc, BP_RADIOBUTTON, CBS_UNCHECKEDNORMAL, nullptr, TS_TRUE, &radioSize)))
					radioSize.cx = radioSize.cy = GetDeviceCaps(dc, LOGPIXELSX) / 8 + 1;
				if (hTheme) CloseThemeData(hTheme);
				SIZE szZero;
				HFONT hFont = (HFONT) SendMessage(this->hwnd, WM_GETFONT, 0, 0);
				HGDIOBJ prevFont = SelectObject(dc, hFont);
				TCHAR c = _T('0');
				GetTextExtentPoint32(dc, &c, 1, &szZero);
				SelectObject(dc, prevFont);
				checkBoxGap = szZero.cx / 2;
				if (!hTheme)
				{
					checkBoxGap += 2;
					hasTheme = false;
				}
			}
			ReleaseDC(hwnd, dc);
			size.cx += (type == WinUtil::CTRL_RADIO ? radioSize.cx : checkBoxSize.cx) + checkBoxGap;
			size.cy = std::max(size.cy, checkBoxSize.cy);
			if (!hasTheme) size.cx += 2;
			break;
		}
		case WinUtil::CTRL_COMBOBOX:
		{
			if (comboBoxHeight < 0)
			{
				HDC dc = GetDC(hwnd);
				if (!dc) return false;
				SIZE szZero;
				HFONT hFont = (HFONT) SendMessage(this->hwnd, WM_GETFONT, 0, 0);
				HGDIOBJ prevFont = SelectObject(dc, hFont);
				TCHAR c = _T('0');
				GetTextExtentPoint32(dc, &c, 1, &szZero);
				SelectObject(dc, prevFont);
				ReleaseDC(hwnd, dc);
				comboBoxHeight = szZero.cy + GetSystemMetrics(SM_CYEDGE) + 2*GetSystemMetrics(SM_CYFIXEDFRAME);
			}
			size.cx = getXSizeDU(48);
			size.cy = comboBoxHeight;
			break;
		}
		case WinUtil::CTRL_EDIT:
			size.cx = getXSizeDU(40);
			size.cy = getYSizeDU(14);
			break;
		default:
			return false;
	}
	return true;
}
