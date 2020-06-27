#include "stdafx.h"
#include "CustomDrawHelpers.h"
#include "ImageLists.h"
#include "BarShader.h"
#include "../client/SettingsManager.h"
#include "../client/LocationUtil.h"
#include "../client/LruCache.h"

static const int iconSize = 16;
static const int flagIconWidth = 25;
static const int margin1 = 4;
static const int margin2 = 2;
static const int topOffset = 2;

class GdiObjCache
{
	public:
		GdiObjCache()
		{
			cache.setDeleter(deleter);
		}
		HBRUSH getBrush(COLORREF color);

	private:
		struct Entry
		{
			COLORREF key;
			HBRUSH brush;
			Entry* next;
		};
		LruCache<Entry, COLORREF> cache;

		static void deleter(Entry& e)
		{
			DeleteObject(e.brush);
		}		
};
	
static GdiObjCache gdiCache;

HBRUSH GdiObjCache::getBrush(COLORREF color)
{
	const Entry* entry = cache.get(color);
	if (entry) return entry->brush;
	cache.removeOldest(200);
	Entry newEntry;
	newEntry.key = color;
	newEntry.brush = CreateSolidBrush(color);
	cache.add(newEntry);
	return newEntry.brush;
}

CustomDrawHelpers::CustomDrawState::CustomDrawState()
{
	flags = 0;
	currentItem = prevSubItem = -1;
	indent = 0;
}

void CustomDrawHelpers::startDraw(CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	CListViewCtrl lv(cd->nmcd.hdr.hwndFrom);
	if (GetFocus() == lv)
		state.flags |= FLAG_LV_FOCUSED;
	else
		state.flags &= ~FLAG_LV_FOCUSED;
	state.currentItem = -1;
	state.prevSubItem = -1;
	state.hImg = lv.GetImageList(LVSIL_SMALL);
	state.hImgState = lv.GetImageList(LVSIL_STATE);

	if (state.flags & FLAG_GET_COLFMT)
	{
		int columns = lv.GetHeader().GetItemCount();
		state.columnFormat.resize(columns);
		LVCOLUMN col;
		col.mask = LVCF_FMT;
		for (int i = 0; i < columns; ++i)
		{
			if (!lv.GetColumn(i, &col))
			{
				dcassert(0);
				col.fmt = 0;
			}
			state.columnFormat[i] = col.fmt;
		}
	}
}

void CustomDrawHelpers::startItemDraw(CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	state.currentItem = (int) cd->nmcd.dwItemSpec;
	state.prevSubItem = -1;
	state.drawCount = 0;
	state.flags &= ~(FLAG_SELECTED | FLAG_FOCUSED | FLAG_HOT);
	CListViewCtrl lv(cd->nmcd.hdr.hwndFrom);
	lv.GetItemRect(state.currentItem, &state.rcItem, LVIR_BOUNDS);
	UINT itemState = lv.GetItemState(state.currentItem, LVIS_SELECTED | LVIS_FOCUSED);
	if (itemState & LVIS_SELECTED)
		state.flags |= FLAG_SELECTED;
	if (itemState & LVIS_FOCUSED)
		state.flags |= FLAG_FOCUSED;
	if ((state.flags & FLAG_USE_HOT_ITEM) && lv.GetHotItem() == state.currentItem)
		state.flags |= FLAG_HOT;
}

void CustomDrawHelpers::drawFocusRect(CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	if ((state.flags & FLAG_FOCUSED) && state.drawCount)
		DrawFocusRect(cd->nmcd.hdc, &state.rcItem);
}

void CustomDrawHelpers::drawBackground(HTHEME hTheme, const CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	if (!hTheme) return;
	bool selected = (state.flags & FLAG_SELECTED) != 0;
	bool focused = (state.flags & FLAG_LV_FOCUSED) != 0;
	int stateId = focused ? (selected ? LISS_SELECTED : LISS_NORMAL) : (selected ? LISS_SELECTEDNOTFOCUS : LISS_NORMAL);
	DrawThemeBackground(hTheme, cd->nmcd.hdc, LVP_LISTITEM, stateId, &cd->nmcd.rc, nullptr);
}

bool CustomDrawHelpers::startSubItemDraw(CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	if (cd->nmcd.rc.left == 0 && cd->nmcd.rc.right == 0) return false;
	
	state.rc = cd->nmcd.rc;
	state.drawCount++;
	if ((state.rc.left != 0 || state.rc.right != 0) && state.rc.top == 0 && state.rc.bottom == 0)
	{
		CListViewCtrl lv(cd->nmcd.hdr.hwndFrom);
		lv.GetSubItemRect(state.currentItem, cd->iSubItem, LVIR_BOUNDS, &state.rc);		
	}
	state.clrForeground = cd->clrText;
	state.clrBackground = cd->clrTextBk;
	if ((state.flags & (FLAG_APP_THEMED | FLAG_SELECTED)) == FLAG_SELECTED)
	{
		if (state.flags & FLAG_LV_FOCUSED)
		{
			state.clrForeground = GetSysColor(COLOR_HIGHLIGHTTEXT);
			state.clrBackground = GetSysColor(COLOR_HIGHLIGHT);
		}
		else
		{
			state.clrForeground = GetSysColor(COLOR_BTNTEXT);
			state.clrBackground = GetSysColor(COLOR_BTNFACE);
		}
	}

	if (cd->iSubItem == 0)
		state.rc.left = state.prevSubItem < 0 ? state.rcItem.left : state.prevSubItemRect.right;

	fillBackground(state, cd);
	return true;
}

void CustomDrawHelpers::setColor(CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	HDC hdc = cd->nmcd.hdc;
	state.oldBkMode = GetBkMode(hdc);
	SetBkMode(hdc, TRANSPARENT);
	state.oldColor = SetTextColor(hdc, state.clrForeground);	
}

void CustomDrawHelpers::fillBackground(const CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	if (!(state.flags & FLAG_APP_THEMED) || !(state.flags & (FLAG_SELECTED | FLAG_HOT)))
		FillRect(cd->nmcd.hdc, &state.rc, gdiCache.getBrush(state.clrBackground));
}

void CustomDrawHelpers::endSubItemDraw(CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	HDC hdc = cd->nmcd.hdc;
	state.prevSubItem = cd->iSubItem;
	state.prevSubItemRect = cd->nmcd.rc;
	if (state.oldBkMode != TRANSPARENT) SetBkMode(hdc, state.oldBkMode);
	SetTextColor(hdc, state.oldColor);
}

UINT CustomDrawHelpers::getTextFlags(const CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd)
{
	UINT flags = DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS;
	if ((state.flags & FLAG_GET_COLFMT) && cd->iSubItem >= 0 && cd->iSubItem < (int) state.columnFormat.size())
	{
		int fmt = state.columnFormat[cd->iSubItem];
		if (fmt & LVCFMT_RIGHT)
			flags |= DT_RIGHT;
		else if (fmt & LVCFMT_CENTER)
			flags |= DT_CENTER;
	}
	return flags;
}

void CustomDrawHelpers::drawFirstSubItem(CustomDrawHelpers::CustomDrawState& state, const NMLVCUSTOMDRAW* cd, const tstring& text)
{
	CListViewCtrl lv(cd->nmcd.hdr.hwndFrom);
	LVITEM item;
	memset(&item, 0, sizeof(item));
	item.mask = LVIF_IMAGE | LVIF_INDENT | LVIF_STATE;	
	item.stateMask = LVIS_STATEIMAGEMASK;
	item.iItem = state.currentItem;
	if (!lv.GetItem(&item)) return;

	if (!startSubItemDraw(state, cd)) return;
	RECT rc = state.rc;
	rc.left += margin1;
	int stateImage = (item.state & LVIS_STATEIMAGEMASK) >> 12;

	if (state.hImgState)
	{
		if (stateImage)
		{
			IMAGELISTDRAWPARAMS dp = { sizeof(dp) };
			dp.cx = dp.cy = iconSize;
			dp.y = (rc.top + rc.bottom - iconSize) / 2;
			dp.x = rc.left;
			if (dp.x + iconSize > rc.right)
				dp.cx = rc.right - dp.x;
			if (dp.cx > 0)
			{
				dp.himl = state.hImgState;
				dp.i = stateImage - 1;
				dp.hdcDst = cd->nmcd.hdc;
				dp.rgbBk = CLR_NONE;
				dp.rgbFg = CLR_NONE;
				dp.dwRop = SRCCOPY;
				ImageList_DrawIndirect(&dp);
			}
		}
		rc.left += iconSize + 3;
	}
	rc.left += iconSize * (item.iIndent + state.indent);
	if (state.hImg && item.iImage >= 0)
	{
		IMAGELISTDRAWPARAMS dp = { sizeof(dp) };
		dp.cx = dp.cy = iconSize;
		dp.y = (rc.top + rc.bottom - iconSize) / 2;
		dp.x = rc.left;
		if (dp.x + iconSize > rc.right)
			dp.cx = rc.right - dp.x;
		rc.left = dp.x + iconSize + margin2;
		if (dp.cx > 0)
		{
			dp.himl = state.hImg;
			dp.i = item.iImage;
			dp.hdcDst = cd->nmcd.hdc;
			dp.rgbBk = CLR_NONE;
			dp.rgbFg = CLR_NONE;
			dp.dwRop = SRCCOPY;
			ImageList_DrawIndirect(&dp);
		}
	}
	setColor(state, cd);
	UINT textFlags = getTextFlags(state, cd);
	if (textFlags & DT_RIGHT)
		rc.right -= margin1 + margin2;	
	else
		rc.right -= margin2;
	if (!text.empty() && rc.left < rc.right)
	{
		rc.top += topOffset;
		DrawText(cd->nmcd.hdc, text.c_str(), text.length(), &rc, textFlags);
	}
	state.indent = 0;
	endSubItemDraw(state, cd);
}

void CustomDrawHelpers::drawLocation(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, const IPInfo& ipInfo)
{
	if (!startSubItemDraw(state, cd)) return;
	setColor(state, cd);
	RECT rc = state.rc;
	POINT p = { rc.left, (rc.top + rc.bottom - iconSize) / 2 };
	p.x += margin1;
	if (BOOLSETTING(ENABLE_COUNTRY_FLAG) && ipInfo.countryImage > 0)
	{
		g_flagImage.DrawCountry(cd->nmcd.hdc, ipInfo, p);
		p.x += flagIconWidth;
	}
	if (ipInfo.locationImage > 0)
	{
		g_flagImage.DrawLocation(cd->nmcd.hdc, ipInfo, p);
		p.x += flagIconWidth;
	}
	setColor(state, cd);
	const string& str = Util::getDescription(ipInfo);
	if (!str.empty())
	{
		auto desc = Text::toT(str);
		rc.left = p.x + margin2;
		rc.top += topOffset;
		DrawText(cd->nmcd.hdc, desc.c_str(), desc.length(), &rc, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
	}
	endSubItemDraw(state, cd);
}

void CustomDrawHelpers::drawTextAndIcon(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, CImageList* images, int icon, const tstring& text, bool rightIcon)
{
	if (!startSubItemDraw(state, cd)) return;
	setColor(state, cd);
	RECT rc = state.rc;
	if (images && icon >= 0)
	{
		IMAGELISTDRAWPARAMS dp = { sizeof(dp) };
		dp.cx = dp.cy = iconSize;
		dp.y = (rc.top + rc.bottom - iconSize) / 2;
		if (rightIcon)
		{
			dp.x = rc.right - margin1 - iconSize;
			rc.right = dp.x - margin2;
			if (dp.x < rc.left)
			{
				dp.xBitmap = rc.left - dp.x;
				dp.cx = iconSize - dp.xBitmap;
				dp.x = rc.left;
			}
			rc.left += margin1 + margin2;
		}
		else
		{
			dp.x = rc.left + margin1;
			if (dp.x + iconSize > rc.right)
				dp.cx = rc.right - dp.x;
			rc.left = dp.x + iconSize + margin2;
			rc.right -= margin2;
		}
		if (dp.cx > 0)
		{
			dp.himl = images->m_hImageList;
			dp.i = icon;
			dp.hdcDst = cd->nmcd.hdc;
			dp.rgbBk = CLR_NONE;
			dp.rgbFg = CLR_NONE;
			//dp.fStyle = (state.flags & FLAG_SELECTED) ? ILD_BLEND50 : ILD_NORMAL;
			dp.dwRop = SRCCOPY;
			ImageList_DrawIndirect(&dp);
		}
	}
	else
	{
		rc.left += margin1 + margin2;
		rc.right -= margin1 + margin2;
	}
	if (!text.empty() && rc.left < rc.right)
	{
		rc.top += topOffset;
		DrawText(cd->nmcd.hdc, text.c_str(), text.length(), &rc, getTextFlags(state, cd));
	}
	endSubItemDraw(state, cd);
}

void CustomDrawHelpers::drawIPAddress(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, bool isPhantomIP, const tstring& text)
{
	if (!startSubItemDraw(state, cd)) return;
	if (isPhantomIP && (state.flags & (FLAG_APP_THEMED | FLAG_SELECTED)) != FLAG_SELECTED)
		state.clrForeground = OperaColors::blendColors(state.clrForeground, state.clrBackground, 0.4);
	setColor(state, cd);
	if (!text.empty())
	{
		RECT rc = state.rc;
		rc.left += margin1 + margin2;
		rc.top += topOffset;
		if (isPhantomIP)
		{
			tstring tmp = text + _T('*');
			DrawText(cd->nmcd.hdc, tmp.c_str(), tmp.length(), &rc, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
		} else
		{
			DrawText(cd->nmcd.hdc, text.c_str(), text.length(), &rc, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
		}
	}
	endSubItemDraw(state, cd);
}

void CustomDrawHelpers::drawVideoResIcon(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, const tstring& text, unsigned width, unsigned height)
{
	int imageIndex = VideoImage::getMediaVideoIcon(width, height);
	drawTextAndIcon(state, cd, &g_videoImage.getIconList(), imageIndex, text, true);
}

bool CustomDrawHelpers::parseVideoResString(const tstring& text, unsigned& width, unsigned& height)
{
	auto pos = text.find(_T('x'));
	if (pos == tstring::npos) return false;
	int x_size = _tstoi(text.c_str());
	if (x_size <= 0) return false;
	int y_size = _tstoi(text.c_str() + pos + 1);
	if (y_size <= 0) return false;
	width = x_size;
	height = y_size;
	return true;
}
