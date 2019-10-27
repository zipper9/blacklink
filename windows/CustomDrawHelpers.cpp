#include "stdafx.h"
#include "CustomDrawHelpers.h"
#include "ImageLists.h"
#include "BarShader.h"
#include "../client/SettingsManager.h"

void CustomDrawHelpers::drawLocation(CListViewCtrl& lv, bool lvFocused, const NMLVCUSTOMDRAW* cd, const Util::CustomNetworkIndex& cni)
{
	COLORREF clrForeground = cd->clrText;
	COLORREF clrBackground = cd->clrTextBk;
	int item = (int) cd->nmcd.dwItemSpec;
	if (lv.GetItemState(item, LVIS_SELECTED))
	{
		if (lvFocused)
		{
			clrForeground = GetSysColor(COLOR_HIGHLIGHTTEXT);
			clrBackground = GetSysColor(COLOR_HIGHLIGHT);
		} else
		{
			clrForeground = GetSysColor(COLOR_BTNTEXT);
			clrBackground = GetSysColor(COLOR_BTNFACE);
		}		
	}
	CRect rc;
	lv.GetSubItemRect(item, cd->iSubItem, LVIR_BOUNDS, rc);
	HDC hdc = cd->nmcd.hdc;
	HBRUSH brush = CreateSolidBrush(clrBackground);
	FillRect(hdc, &rc, brush);
	DeleteObject(brush);
	//POINT p = { rc.left, rc.top + 1 };
	CPoint p(rc.left, rc.top + (rc.Height() - 16) / 2);

#ifdef FLYLINKDC_USE_GEO_IP
	if (BOOLSETTING(ENABLE_COUNTRY_FLAG))
	{
		g_flagImage.DrawCountry(cd->nmcd.hdc, cni, p);
		p.x += 25;
	}
#endif
	if (cni.getFlagIndex() > 0)
	{
		g_flagImage.DrawLocation(cd->nmcd.hdc, cni, p);
		p.x += 25;
	}
	const auto& desc = cni.getDescription();
	if (!desc.empty())
	{
		CRect rcText = rc;
		int bkMode = GetBkMode(hdc);
		SetBkMode(hdc, TRANSPARENT);
		COLORREF oldColor = SetTextColor(hdc, clrForeground);
		rcText.left = p.x + 6;
		rcText.top += 2;
		DrawText(hdc, desc.c_str(), desc.length(), &rcText, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
		//ExtTextOut(hdc, p.x + 6, rc2.top + 2, ETO_CLIPPED, &rc2, desc.c_str(), desc.length(), nullptr);
		if (bkMode != TRANSPARENT) SetBkMode(hdc, bkMode);
		SetTextColor(hdc, oldColor);
	}
}

void CustomDrawHelpers::drawTextAndIcon(CListViewCtrl& lv, bool lvFocused, const NMLVCUSTOMDRAW* cd, BaseImageList& images, int icon, const tstring& text)
{
	COLORREF clrForeground = cd->clrText;
	COLORREF clrBackground = cd->clrTextBk;
	int item = (int) cd->nmcd.dwItemSpec;
	if (lv.GetItemState(item, LVIS_SELECTED))
	{
		if (lvFocused)
		{
			clrForeground = GetSysColor(COLOR_HIGHLIGHTTEXT);
			clrBackground = GetSysColor(COLOR_HIGHLIGHT);
		} else
		{
			clrForeground = GetSysColor(COLOR_BTNTEXT);
			clrBackground = GetSysColor(COLOR_BTNFACE);
		}		
	}
	CRect rc;
	lv.GetSubItemRect(item, cd->iSubItem, LVIR_BOUNDS, rc);
	HDC hdc = cd->nmcd.hdc;
	HBRUSH brush = CreateSolidBrush(clrBackground);
	FillRect(hdc, &rc, brush);
	DeleteObject(brush);

	CPoint p(rc.left, rc.top + (rc.Height() - 16) / 2);
	images.Draw(hdc, icon, p);
	p.x += 16;

	if (!text.empty())
	{
		CRect rcText = rc;
		int bkMode = GetBkMode(hdc);
		SetBkMode(hdc, TRANSPARENT);
		COLORREF oldColor = SetTextColor(hdc, clrForeground);
		rcText.left = p.x + 2;
		rcText.top += 2;
		DrawText(hdc, text.c_str(), text.length(), &rcText, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
		if (bkMode != TRANSPARENT) SetBkMode(hdc, bkMode);
		SetTextColor(hdc, oldColor);
	}
}

void CustomDrawHelpers::drawIPAddress(CListViewCtrl& lv, bool lvFocused, const NMLVCUSTOMDRAW* cd, bool isPhantomIP, const tstring& text)
{
	COLORREF clrForeground = cd->clrText;
	COLORREF clrBackground = cd->clrTextBk;
	int item = (int) cd->nmcd.dwItemSpec;
	if (lv.GetItemState(item, LVIS_SELECTED))
	{
		if (lvFocused)
		{
			clrForeground = GetSysColor(COLOR_HIGHLIGHTTEXT);
			clrBackground = GetSysColor(COLOR_HIGHLIGHT);
		} else
		{
			clrForeground = GetSysColor(COLOR_BTNTEXT);
			clrBackground = GetSysColor(COLOR_BTNFACE);
		}		
	} else
	{
		if (isPhantomIP)
			clrForeground = OperaColors::blendColors(clrForeground, clrBackground, 0.4);
	}
	CRect rc;
	lv.GetSubItemRect(item, cd->iSubItem, LVIR_BOUNDS, rc);
	HDC hdc = cd->nmcd.hdc;
	HBRUSH brush = CreateSolidBrush(clrBackground);
	FillRect(hdc, &rc, brush);
	DeleteObject(brush);
	if (!text.empty())
	{
		int bkMode = GetBkMode(hdc);
		SetBkMode(hdc, TRANSPARENT);
		COLORREF oldColor = SetTextColor(hdc, clrForeground);
		rc.top += 2;
		if (isPhantomIP)
		{
			tstring tmp = text + _T('*');
			DrawText(hdc, tmp.c_str(), tmp.length(), &rc, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
		} else
		{
			DrawText(hdc, text.c_str(), text.length(), &rc, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
		}
		if (bkMode != TRANSPARENT) SetBkMode(hdc, bkMode);
		SetTextColor(hdc, oldColor);
	}
}

bool CustomDrawHelpers::drawVideoResIcon(CListViewCtrl& lv, bool lvFocused, const LPNMLVCUSTOMDRAW cd, const tstring& text, unsigned width, unsigned height)
{
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
	if (text.empty()) return false;
	int imageIndex = VideoImage::getMediaVideoIcon(width, height);
	if (imageIndex == -1) return false;
	COLORREF clrForeground = cd->clrText;
	COLORREF clrBackground = cd->clrTextBk;
	int item = (int) cd->nmcd.dwItemSpec;
	if (lv.GetItemState(item, LVIS_SELECTED))
	{
		if (lvFocused)
		{
			clrForeground = GetSysColor(COLOR_HIGHLIGHTTEXT);
			clrBackground = GetSysColor(COLOR_HIGHLIGHT);
		} else
		{
			clrForeground = GetSysColor(COLOR_BTNTEXT);
			clrBackground = GetSysColor(COLOR_BTNFACE);
		}		
	}
	CRect rc, rc2;
	lv.GetSubItemRect(item, cd->iSubItem, LVIR_BOUNDS, rc);
	rc2 = rc;
	HDC hdc = cd->nmcd.hdc;
	HBRUSH brush = CreateSolidBrush(clrBackground);
	FillRect(hdc, &rc2, brush);
	DeleteObject(brush);
	rc.left = rc.right - 19;
	const POINT p = { rc.left, rc.top };
	g_videoImage.Draw(hdc, imageIndex, p);
	int bkMode = GetBkMode(hdc);
	SetBkMode(hdc, TRANSPARENT); // Why is it needed? ETO_OPAQUE is not set...
	COLORREF oldColor = SetTextColor(hdc, clrForeground);
	ExtTextOut(hdc, rc2.left + 6, rc2.top + 2, ETO_CLIPPED, &rc2, text.c_str(), text.length(), NULL);
	if (bkMode != TRANSPARENT) SetBkMode(hdc, bkMode);
	SetTextColor(hdc, oldColor);
	return true;
#else
	return false;
#endif
}

bool CustomDrawHelpers::parseVideoResString(const tstring& text, unsigned& width, unsigned& height)
{
	auto pos = text.find(_T('x'));
	if (pos == tstring::npos) return false;
	int x_size = _wtoi(text.c_str());
	if (x_size <= 0) return false;
	int y_size = _wtoi(text.c_str() + pos + 1);
	if (y_size <= 0) return false;
	width = x_size;
	height = y_size;
	return true;
}
