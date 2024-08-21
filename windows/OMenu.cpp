/*
 * Copyright (C) 2003-2004 "Opera", <opera at home dot se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"
#include "OMenu.h"
#include "WinUtil.h"
#include "ColorUtil.h"
#include "BarShader.h"
#include "GdiUtil.h"
#include "DwmApiLib.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/SysVersion.h"

#define THEME_NAME L"MENU"
#define USE_DRAW_THEME_TEXT
#define DWM DwmApiLib::instance

static const size_t MAX_CAPTION_LEN = 40;

#ifndef DWMWA_COLOR_DEFAULT

enum
{
    DWMWA_WINDOW_CORNER_PREFERENCE = 33,
    DWMWA_BORDER_COLOR,
    DWMWA_CAPTION_COLOR,
    DWMWA_TEXT_COLOR,
    DWMWA_VISIBLE_FRAME_BORDER_THICKNESS
};

typedef enum
{
    DWMWCP_DEFAULT = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND = 2,
    DWMWCP_ROUNDSMALL = 3

} DWM_WINDOW_CORNER_PREFERENCE;

#define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#define DWMWA_COLOR_NONE    0xFFFFFFFE

#endif

#if !(defined(NTDDI_VERSION) && NTDDI_VERSION >= 0x0A00000C)
#define MENU_POPUPITEM_FOCUSABLE 27
#endif

static COLORREF getColorFromTheme(HTHEME hTheme, int width, int height, int partId, int stateId)
{
	COLORREF color = CLR_INVALID;
	HDC hDCDisplay = CreateIC(_T("DISPLAY"), nullptr, nullptr, nullptr);
	BITMAPINFO bi = { sizeof(BITMAPINFOHEADER) };
	bi.bmiHeader.biWidth = width;
	bi.bmiHeader.biHeight = height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP hBitmap = CreateDIBSection(hDCDisplay, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
	HDC hdc = CreateCompatibleDC(hDCDisplay);
	HGDIOBJ oldBitmap = SelectObject(hdc, hBitmap);
	RECT rc = { 0, 0, width, height };
	if (SUCCEEDED(DrawThemeBackground(hTheme, hdc, partId, stateId, &rc, nullptr)))
	{
		GdiFlush();
		int x = width / 2;
		int y = height / 2;
		uint8_t* p = (uint8_t*) bits + ((width * y + x) << 2);
		color = RGB(p[2], p[1], p[0]);
	}
	SelectObject(hdc, oldBitmap);
	DeleteObject(hBitmap);
	DeleteDC(hdc);
	DeleteDC(hDCDisplay);
	return color;
}

struct OMenuItem
{
	OMenuItem(const OMenuItem&) = delete;
	OMenuItem& operator= (const OMenuItem&) = delete;

	OMenuItem(OMenu* parent) : parent(parent), accelPos(-1), bitmap(nullptr), grayBitmap(nullptr)
	{
		sizeText.cx = sizeText.cy = 0;
		sizeBitmap.cx = sizeBitmap.cy = 0;
	}

	~OMenuItem()
	{
		if (grayBitmap) DeleteObject(grayBitmap);
	}

	OMenu* const parent;
	tstring text;
	void*   data;
	UINT    type;
	UINT    extType;
	SIZE    sizeText;
	int     accelPos;
	HBITMAP bitmap;
	mutable HBITMAP grayBitmap;
	SIZE    sizeBitmap;

	int getTextLength() const
	{
		return accelPos == -1 ? static_cast<int>(text.length()) : accelPos;
	}

	HBITMAP getGrayBitmap(HDC hdc) const;
};

enum
{
	EXT_TYPE_SUBMENU      = 1,
	EXT_TYPE_HEADER       = 2,
	EXT_TYPE_DEFAULT_ITEM = 4
};

OMenu::OMenu() :
	ownerDrawMode(OD_DEFAULT),
	themeInitialized(false), partId(0), hTheme(nullptr),
	fontNormal(nullptr), fontBold(nullptr), bgBrush(nullptr),
	textMeasured(false), updateMenuWindowFlag(false)
{
	marginCheck = { 0 };
	marginCheckBackground = { 0 };
	marginItem = { 0 };
	marginText = { 0 };
	marginSubmenu = { 0 };
	sizeCheck = { 0 };
	sizeSeparator = { 0 };
	sizeSubmenu = { 0 };
	marginHeader = { 0 };
	maxTextWidth = maxAccelWidth = maxBitmapWidth = 0;
	minBitmapWidth = minBitmapHeight = 0;
	accelSpace = avgCharWidth = 0;
	for (int i = 0; i < _countof(bitmaps); ++i)
		bitmaps[i] = nullptr;
}

OMenu::~OMenu()
{
	if (::IsMenu(m_hMenu))
		DestroyMenu();
	else
		dcassert(!m_hMenu);
	dcassert(items.empty());
	for (auto i = items.cbegin(); i != items.cend(); ++i)
		delete *i;
	destroyResources();
}

void OMenu::destroyResources()
{
	if (fontNormal)
	{
		DeleteObject(fontNormal);
		fontNormal = nullptr;
	}
	if (fontBold)
	{
		DeleteObject(fontBold);
		fontBold = nullptr;
	}
	if (hTheme)
	{
		CloseThemeData(hTheme);
		hTheme = nullptr;
	}
	for (int i = 0; i < _countof(bitmaps); ++i)
		if (bitmaps[i])
		{
			DeleteObject(bitmaps[i]);
			bitmaps[i] = nullptr;
		}
	if (bgBrush)
	{
		DeleteObject(bgBrush);
		bgBrush = nullptr;
	}
}

void OMenu::SetOwnerDraw(int mode)
{
	if (ownerDrawMode != mode && m_hMenu && GetMenuItemCount())
	{
		dcassert(0);
		return;
	}
	ownerDrawMode = mode;
}

BOOL OMenu::CreatePopupMenu()
{
	if (ownerDrawMode == OD_DEFAULT)
	{
		if (SettingsManager::instance.getUiSettings()->getBool(Conf::USE_CUSTOM_MENU))
			ownerDrawMode = OD_ALWAYS;
		else
			ownerDrawMode = OD_NEVER;
	}
	if (!CMenu::CreatePopupMenu())
		return FALSE;
	if (ownerDrawMode != OD_NEVER && SysVersion::isOsWin11Plus())
		updateBackgroundBrush();
	return TRUE;
}

BOOL OMenu::InsertSeparator(UINT uItem, BOOL byPosition, const tstring& caption)
{
	if (ownerDrawMode == OD_NEVER)
		return FALSE;
	OMenuItem* omi = new OMenuItem(this);
	omi->text = caption.length() > MAX_CAPTION_LEN ? caption.substr(0, MAX_CAPTION_LEN) : caption;
	omi->data = nullptr;
	omi->extType = EXT_TYPE_HEADER;
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	mii.fMask = MIIM_FTYPE | MIIM_DATA;
	mii.fType = MFT_OWNERDRAW | MFT_SEPARATOR;
	mii.dwItemData = (ULONG_PTR) omi;
	omi->type = mii.fType;
	if (!CMenu::InsertMenuItem(uItem, byPosition, &mii))
	{
		dcassert(0);
		delete omi;
		return FALSE;
	}
	items.push_back(omi);
	return TRUE;
}

void OMenu::checkOwnerDrawOnRemove(UINT uItem, BOOL byPosition)
{
	if (ownerDrawMode == OD_NEVER)
		return;
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	mii.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_SUBMENU;
	GetMenuItemInfo(uItem, byPosition, &mii);

	if (mii.dwItemData != NULL)
	{
		OMenuItem* mi = (OMenuItem*) mii.dwItemData;
		auto i = find(items.begin(), items.end(), mi);
		if (i != items.end())
			items.erase(i);
		else
			dcassert(0);
		delete mi;
	}
}

BOOL OMenu::InsertMenuItem(UINT uItem, BOOL bByPosition, LPMENUITEMINFO lpmii)
{
	if (ownerDrawMode == OD_NEVER)
		return CMenu::InsertMenuItem(uItem, bByPosition, lpmii);
	OMenuItem* omi = new OMenuItem(this);
	if (lpmii->dwTypeData) omi->text = lpmii->dwTypeData;
	omi->data = nullptr;
	if (lpmii->fMask & MIIM_DATA)
		omi->data = (void*) lpmii->dwItemData;
	omi->extType = 0;
	MENUITEMINFO mii;
	memcpy(&mii, lpmii, sizeof(MENUITEMINFO));
	mii.fMask |= MIIM_DATA;
	if (ownerDrawMode == OD_ALWAYS)
		mii.fType |= MFT_OWNERDRAW;
	if (!(mii.fMask & (MIIM_TYPE | MIIM_FTYPE)))
		mii.fMask |= MIIM_FTYPE;
	if ((mii.fMask & MIIM_SUBMENU) && mii.hSubMenu)
		omi->extType |= EXT_TYPE_SUBMENU;
	mii.dwItemData = (ULONG_PTR) omi;
	omi->type = mii.fType;
	if (!CMenu::InsertMenuItem(uItem, bByPosition, &mii))
	{
		dcassert(0);
		delete omi;
		return FALSE;
	}
	items.push_back(omi);
	return TRUE;
}

BOOL OMenu::AppendMenu(UINT nFlags, UINT_PTR nIDNewItem, LPCTSTR lpszNewItem, HBITMAP hBitmap)
{
	if (ownerDrawMode == OD_NEVER)
	{
		int index = hBitmap ? GetMenuItemCount() : 0;
		if (!CMenu::AppendMenu(nFlags, nIDNewItem, lpszNewItem)) return FALSE;
		if (hBitmap) SetBitmap(index, TRUE, hBitmap);
		return TRUE;
	}
	dcassert(::IsMenu(m_hMenu));
	int pos = GetMenuItemCount();
	OMenuItem* omi = new OMenuItem(this);
	if (lpszNewItem) omi->text = lpszNewItem;
	omi->data = nullptr;
	omi->extType = 0;
	MENUITEMINFO mii = { sizeof(mii) };
	mii.dwItemData = (ULONG_PTR) omi;
	mii.fMask = MIIM_DATA | MIIM_STRING | MIIM_FTYPE | MIIM_STATE;
	mii.dwTypeData = const_cast<LPTSTR>(lpszNewItem);
	if (nFlags & (MF_DISABLED | MF_GRAYED))
	{
		nFlags &= ~(MF_DISABLED | MF_GRAYED);
		mii.fState |= MFS_DISABLED;
	}
	if (nFlags & MF_CHECKED)
	{
		nFlags &= ~MF_CHECKED;
		mii.fState |= MFS_CHECKED;
	}
	if (nFlags & MF_POPUP)
	{
		nFlags &= ~MF_POPUP;
		mii.fMask |= MIIM_SUBMENU;
		mii.hSubMenu = (HMENU) nIDNewItem;
		omi->extType |= EXT_TYPE_SUBMENU;
	}
	else
	{
		mii.fMask |= MIIM_ID;
		mii.wID = nIDNewItem;
	}
	if (ownerDrawMode == OD_ALWAYS)
		mii.fType |= MFT_OWNERDRAW;
	mii.fType |= nFlags;
	omi->type = mii.fType;
	omi->bitmap = hBitmap;
	if (!CMenu::InsertMenuItem(pos, TRUE, &mii))
	{
		dcassert(0);
		delete omi;
		return FALSE;
	}
	items.push_back(omi);
	updateBitmapHeight(hBitmap);
	return TRUE;
}

BOOL OMenu::SetMenuDefaultItem(UINT id)
{
	if (ownerDrawMode == OD_NEVER)
	{
		dcassert(items.empty());
		return CMenu::SetMenuDefaultItem(id);
	}
	for (OMenuItem* omi : items)
		omi->extType &= ~EXT_TYPE_DEFAULT_ITEM;
	if (!CMenu::SetMenuDefaultItem(id))
		return FALSE;
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_DATA;
	if (!GetMenuItemInfo(id, FALSE, &mii))
		return FALSE;
	if (mii.dwItemData)
	{
		((OMenuItem*) mii.dwItemData)->extType |= EXT_TYPE_DEFAULT_ITEM;
		textMeasured = false;
	}
	return TRUE;
}

bool OMenu::RenameItem(UINT id, const tstring& text)
{
	int count = GetMenuItemCount();
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA;
	for (int i = 0; i < count; ++i)
	{
		if (GetMenuItemInfo(i, TRUE, &mii) && mii.wID == id)
		{
			if (mii.fType & MFT_OWNERDRAW)
			{
				OMenuItem* omi = (OMenuItem*) mii.dwItemData;
				omi->text = text;
				omi->parent->textMeasured = false;

				// force WM_MEASUREITEM
				ModifyMenu(i, MF_BYPOSITION | MF_STRING, id, text.c_str());
				mii.fMask |= MIIM_STRING;
				mii.dwTypeData = const_cast<TCHAR*>(text.c_str());
				SetMenuItemInfo(i, TRUE, &mii);
			}
			else
				ModifyMenu(i, MF_BYPOSITION | MF_STRING, id, text.c_str());
			return true;
		}
	}
	return false;
}

void* OMenu::GetItemData(UINT id) const
{
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_FTYPE | MIIM_DATA;
	if (!GetMenuItemInfo(id, FALSE, &mii) || !mii.dwItemData)
		return nullptr;
	if (ownerDrawMode == OD_NEVER)
		return (void *) mii.dwItemData;
	return ((const OMenuItem* ) mii.dwItemData)->data;
}

struct FindMenuWindowInfo
{
	HWND hWndResult;
	HMENU hMenu;
};

static BOOL CALLBACK enumProc(HWND hWnd, LPARAM lParam)
{
	if (GetClassLong(hWnd, GCW_ATOM) == 0x8000)
	{
		HMENU hMenu = (HMENU) SendMessage(hWnd, MN_GETHMENU, 0, 0);
		if (hMenu)
		{
			FindMenuWindowInfo* info = reinterpret_cast<FindMenuWindowInfo*>(lParam);
			if (info->hMenu == hMenu)
			{
				info->hWndResult = hWnd;
				return FALSE;
			}
		}
	}
	return TRUE;
}

static HWND findMenuWindow(HMENU hMenu)
{
	FindMenuWindowInfo info;
	info.hWndResult = NULL;
	info.hMenu = hMenu;
	EnumThreadWindows(GetCurrentThreadId(), enumProc, reinterpret_cast<LPARAM>(&info));
	return info.hWndResult;
}

static void updateMenuDecorations(HWND hWnd, HTHEME hTheme, int partId)
{
	DWM.init();
	if (DWM.pDwmSetWindowAttribute)
	{
		DWM_WINDOW_CORNER_PREFERENCE preference = partId == MENU_POPUPITEM_FOCUSABLE ? DWMWCP_ROUNDSMALL : DWMWCP_ROUND;
		DWM.pDwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
		COLORREF color;
		if (hTheme && SUCCEEDED(GetThemeColor(hTheme, MENU_POPUPBORDERS, 0, TMT_FILLCOLORHINT, &color)))
			DWM.pDwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &color, sizeof(color));
	}
}

LRESULT OMenu::onInitMenuPopup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = TRUE;
	HMENU menu = (HMENU) wParam;
	::DefWindowProc(hWnd, uMsg, wParam, lParam);
	int count = ::GetMenuItemCount(menu);
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	mii.fMask = MIIM_FTYPE | MIIM_DATA;
	OMenu* parent = nullptr;
	for (int i = 0; i < count; ++i)
	{
		if (!::GetMenuItemInfo(menu, i, TRUE, &mii))
		{
			dcassert(0);
			continue;
		}
		if ((mii.fType & MFT_OWNERDRAW) && mii.dwItemData)
		{
			OMenuItem* omi = (OMenuItem*) mii.dwItemData;
			parent = omi->parent;
			parent->textMeasured = false;
			if (!parent->themeInitialized)
			{
				parent->openTheme(hWnd);
				parent->themeInitialized = true;
			}
		}
	}
	if (parent && SysVersion::isOsWin11Plus())
	{
		HWND hWndMenu = findMenuWindow(menu);
		if (hWndMenu)
		{
			updateMenuDecorations(hWndMenu, parent->hTheme, parent->partId);
			parent->updateMenuWindowFlag = false;
		}
		else
			parent->updateMenuWindowFlag = true;
	}
	return FALSE;
}

void OMenu::updateMenuWindow()
{
	if (SysVersion::isOsWin11Plus())
	{
		HWND hWndMenu = findMenuWindow(m_hMenu);
		if (hWndMenu) updateMenuDecorations(hWndMenu, hTheme, partId);
	}
}

bool OMenu::getMenuFont(LOGFONT& lf) const
{
	if (hTheme)
	{
		HRESULT hr = GetThemeSysFont(hTheme, TMT_MENUFONT, &lf);
		if (SUCCEEDED(hr)) return true;
	}
	NONCLIENTMETRICS ncm = { offsetof(NONCLIENTMETRICS, lfMessageFont) + sizeof(NONCLIENTMETRICS::lfMessageFont) };
	if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) return false;
	memcpy(&lf, &ncm.lfMenuFont, sizeof(lf));
	return true;
}

void OMenu::createNormalFont()
{
	if (fontNormal) return;
	LOGFONT lf;
	if (!getMenuFont(lf))
	{
		dcassert(0);
		return;
	}
	fontNormal = CreateFontIndirect(&lf);
}

void OMenu::createBoldFont()
{
	if (fontBold) return;
	LOGFONT lf;
	if (!getMenuFont(lf))
	{
		dcassert(0);
		return;
	}
	lf.lfWeight = FW_SEMIBOLD;
	fontBold = CreateFontIndirect(&lf);
}

void OMenu::updateFontMetrics(HWND hwnd)
{
	dcassert(fontNormal);
	dcassert(hwnd);
	HDC hdc = GetDC(hwnd);
	if (!hdc) return;
	HGDIOBJ oldFont = SelectObject(hdc, fontNormal);
	int dx, dy;
	if (WinUtil::getDialogUnits(hdc, dx, dy))
	{
		avgCharWidth = dx;
		accelSpace = hTheme ? avgCharWidth * 2 : avgCharWidth;
	}
	SelectObject(hdc, oldFont);
	ReleaseDC(hwnd, hdc);
}

static inline int totalWidth(const MARGINS& m)
{
	return m.cxLeftWidth + m.cxRightWidth;
}

static inline int totalHeight(const MARGINS& m)
{
	return m.cyTopHeight + m.cyBottomHeight;
}

void OMenu::measureText(HDC hdc)
{
	RECT rc;
	HFONT font;
	HGDIOBJ oldFont;
	maxTextWidth = maxAccelWidth = 0;
	maxBitmapWidth = minBitmapWidth;

	for (OMenuItem* omi : items)
	{
		rc.left = rc.top = rc.right = rc.bottom = 0;
		if (omi->type & MFT_SEPARATOR)
		{
			if (omi->extType & EXT_TYPE_HEADER)
			{
				createBoldFont();
				oldFont = SelectObject(hdc, fontBold);
				DrawText(hdc, omi->text.c_str(), omi->text.length(), &rc, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX | DT_NOCLIP);
				SelectObject(hdc, oldFont);
				omi->sizeText.cx = rc.right - rc.left;
				omi->sizeText.cy = rc.bottom - rc.top;
			}
		}
		else
		{
			int len = static_cast<int>(omi->text.length());
			omi->accelPos = omi->text.find(_T('\t'));
			if (omi->extType & EXT_TYPE_DEFAULT_ITEM)
			{
				createBoldFont();
				font = fontBold;
			}
			else
			{
				createNormalFont();
				font = fontNormal;
			}
			oldFont = SelectObject(hdc, font);
			if (omi->accelPos != -1)
			{
				len = omi->accelPos;
				DrawText(hdc, omi->text.c_str() + omi->accelPos + 1, -1, &rc, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX | DT_NOCLIP);
				int accelWidth = rc.right - rc.left;
				if (accelWidth > maxAccelWidth) maxAccelWidth = accelWidth;
				rc.left = rc.top = rc.right = rc.bottom = 0;
			}
			DrawText(hdc, omi->text.c_str(), len, &rc, DT_SINGLELINE | DT_CALCRECT | DT_HIDEPREFIX | DT_NOCLIP);
			SelectObject(hdc, oldFont);
			omi->sizeText.cx = rc.right - rc.left;
			omi->sizeText.cy = rc.bottom - rc.top;
			if (omi->sizeText.cx > maxTextWidth) maxTextWidth = omi->sizeText.cx;
			omi->sizeBitmap.cx = omi->sizeBitmap.cy = 0;
			if (omi->bitmap)
			{
				BITMAP bitmap;
				if (GetObject(omi->bitmap, sizeof(bitmap), &bitmap))
				{
					omi->sizeBitmap.cx = bitmap.bmWidth;
					omi->sizeBitmap.cy = bitmap.bmHeight;
					if (bitmap.bmWidth > maxBitmapWidth) maxBitmapWidth = bitmap.bmWidth;
				}
			}
		}
	}
	textMeasured = true;
}

LRESULT OMenu::onMeasureItem(HWND hWnd, UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	if (!wParam)
	{
		MEASUREITEMSTRUCT* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
		if (mis->CtlType == ODT_MENU)
		{
			OMenuItem* omi = (OMenuItem*) mis->itemData;
			if (omi)
			{
				OMenu* parent = omi->parent;
				if (!parent->themeInitialized)
				{
					parent->themeInitialized = true;
					parent->openTheme(hWnd);
				}
				if (!parent->textMeasured)
				{
					HDC hdc = GetDC(hWnd);
					if (hdc)
					{
						parent->measureText(hdc);
						ReleaseDC(hWnd, hdc);
					}
				}
				bHandled = TRUE;
				int height = 0;
				int width = 0;
				if (omi->type & MFT_SEPARATOR)
				{
					if (omi->extType & EXT_TYPE_HEADER)
					{
						height = omi->sizeText.cy + totalHeight(parent->marginText);
						int checkHeight = parent->sizeCheck.cy + totalHeight(parent->marginCheckBackground) + totalHeight(parent->marginCheck);
						if (checkHeight > height) height = checkHeight;
						height += totalHeight(parent->marginItem);
						width = omi->sizeText.cx + totalWidth(parent->marginItem) + totalWidth(parent->marginText);
					}
					else
					{
						height = parent->sizeSeparator.cy + totalHeight(parent->marginItem);
						width = parent->sizeCheck.cx + totalWidth(parent->marginItem);
					}
				}
				else
				{
					if (parent->hTheme)
					{
						width = parent->sizeCheck.cx + totalWidth(parent->marginCheck) + parent->marginCheckBackground.cxLeftWidth;
						if (parent->maxBitmapWidth)
						{
							int maxBitmapWidth = parent->maxBitmapWidth + totalWidth(parent->marginBitmap);
							if (maxBitmapWidth > width) width = maxBitmapWidth;
						}
						width += parent->marginCheckBackground.cxRightWidth;
					}
					else
					{
						width = parent->sizeCheck.cx + totalWidth(parent->marginCheck);
						if (parent->maxBitmapWidth)
						{
							int maxBitmapWidth = parent->maxBitmapWidth + totalWidth(parent->marginBitmap);
							if (maxBitmapWidth > width) width = maxBitmapWidth;
						}
					}
					height = parent->sizeCheck.cy + totalHeight(parent->marginCheckBackground) + totalHeight(parent->marginCheck);
					width += parent->maxTextWidth + totalWidth(parent->marginText);
					int partHeight = omi->sizeText.cy + totalHeight(parent->marginText);
					if (partHeight > height) height = partHeight;
					partHeight = omi->sizeBitmap.cy + totalHeight(parent->marginBitmap);
					if (partHeight > height) height = partHeight;
					width += totalWidth(parent->marginItem);
					width += totalWidth(parent->marginSubmenu) + parent->sizeSubmenu.cx;
					if (parent->maxAccelWidth) width += parent->maxAccelWidth + parent->accelSpace;
					if (parent->minBitmapHeight > 0)
					{
						int bitmapHeight = parent->minBitmapHeight + totalHeight(parent->marginBitmap);
						if (bitmapHeight > height) height = bitmapHeight;
					}
				}
				mis->itemWidth = width - 2 * parent->avgCharWidth;
				mis->itemHeight = height;
				return TRUE;
			}
		}
	}
	return FALSE;
}

static inline POPUPITEMSTATES toItemStateId(UINT state)
{
	if (state & (ODS_INACTIVE | ODS_DISABLED))
		return (state & (ODS_HOTLIGHT | ODS_SELECTED)) ? MPI_DISABLEDHOT : MPI_DISABLED;
	return (state & (ODS_HOTLIGHT | ODS_SELECTED)) ? MPI_HOT : MPI_NORMAL;
}

LRESULT OMenu::onDrawItem(HWND hWnd, UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	if (!wParam)
	{
		const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
		if (dis->CtlType == ODT_MENU)
		{
			const OMenuItem* omi = (const OMenuItem*) dis->itemData;
			if (omi)
			{
				OMenu* parent = omi->parent;
				if (!parent->themeInitialized)
				{
					parent->themeInitialized = true;
					parent->openTheme(hWnd);
				}
				if (parent->updateMenuWindowFlag)
				{
					parent->updateMenuWindowFlag = false;
					parent->updateMenuWindow();
				}

				CRect rc(dis->rcItem);

				if ((omi->type & MFT_SEPARATOR) && (omi->extType & EXT_TYPE_HEADER))
				{
					const auto ss = SettingsManager::instance.getUiSettings();
					bHandled = TRUE;
					rc.left += parent->marginHeader.cxLeftWidth;
					rc.right -= parent->marginHeader.cxRightWidth;
					rc.top += parent->marginHeader.cyTopHeight;
					rc.bottom -= parent->marginHeader.cyBottomHeight;
					if (ss->getBool(Conf::MENUBAR_TWO_COLORS))
						OperaColors::drawBar(dis->hDC, rc.left, rc.top, rc.right, rc.bottom,
							ss->getInt(Conf::MENUBAR_LEFT_COLOR),
							ss->getInt(Conf::MENUBAR_RIGHT_COLOR),
							ss->getBool(Conf::MENUBAR_BUMPED));
					else
					{
						COLORREF clrOld = SetBkColor(dis->hDC, ss->getInt(Conf::MENUBAR_LEFT_COLOR));
						ExtTextOut(dis->hDC, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
						SetBkColor(dis->hDC, clrOld);
					}
					SetBkMode(dis->hDC, TRANSPARENT);
					SetTextColor(dis->hDC, ColorUtil::textFromBackground(ss->getInt(Conf::MENUBAR_LEFT_COLOR)));
					HGDIOBJ prevFont = SelectObject(dis->hDC, parent->fontBold);
					DrawText(dis->hDC, omi->text.c_str(), omi->text.length(), rc, DT_CENTER | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
					SelectObject(dis->hDC, prevFont);
				}
				else if (parent->hTheme)
				{
					bHandled = TRUE;
					POPUPITEMSTATES stateId = toItemStateId(dis->itemState);
					if (IsThemeBackgroundPartiallyTransparent(parent->hTheme, parent->partId, stateId))
						DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPBACKGROUND, 0, &rc, nullptr);

					int gapSize = parent->sizeCheck.cx + totalWidth(parent->marginCheck) + parent->marginCheckBackground.cxLeftWidth;
					if (parent->maxBitmapWidth)
					{
						int bitmapSize = parent->maxBitmapWidth + totalWidth(parent->marginBitmap);
						if (bitmapSize > gapSize) gapSize = bitmapSize;
					}
					int xCheck = rc.left + parent->marginItem.cxLeftWidth + gapSize;

					RECT rcGutter;
					rcGutter.left = rc.left;
					rcGutter.top = rc.top;
					rcGutter.right = xCheck + parent->marginCheckBackground.cxRightWidth;
					rcGutter.bottom = rc.bottom;
					DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPGUTTER, 0, &rcGutter, nullptr);

					if (omi->type & MFT_SEPARATOR)
					{
						RECT rcSep;
						rcSep.left = (parent->partId == MENU_POPUPITEM_FOCUSABLE ? rc.left : rcGutter.right) + parent->marginItem.cxLeftWidth + parent->sizeSeparator.cx;
						rcSep.right = rc.right - parent->marginItem.cxRightWidth - parent->sizeSeparator.cx;
						rcSep.top = (rc.top + rc.bottom - parent->sizeSeparator.cy) / 2;
						rcSep.bottom = rcSep.top + parent->sizeSeparator.cy;
						DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPSEPARATOR, 0, &rcSep, nullptr);
					}
					else
					{
						RECT rcDraw = rc;
						rcDraw.left += parent->marginItem.cxLeftWidth;
						rcDraw.right -= parent->marginItem.cxRightWidth;
						DrawThemeBackground(parent->hTheme, dis->hDC, parent->partId, stateId, &rcDraw, nullptr);

						HBITMAP bitmap = omi->bitmap;
						if (bitmap)
						{
							if (dis->itemState & ODS_GRAYED) bitmap = omi->getGrayBitmap(dis->hDC);
							int x = xCheck - parent->marginBitmap.cxRightWidth - omi->sizeBitmap.cx;
							int y = (rc.top + rc.bottom - omi->sizeBitmap.cy) / 2;
							WinUtil::drawAlphaBitmap(dis->hDC, bitmap, x, y, omi->sizeBitmap.cx, omi->sizeBitmap.cy);
						}

						if (dis->itemState & ODS_CHECKED)
						{
							int checkWidth = parent->sizeCheck.cx + totalWidth(parent->marginCheck);
							int checkHeight = parent->sizeCheck.cy + totalHeight(parent->marginCheck);
							rcDraw.left = xCheck - checkWidth;
							rcDraw.right = xCheck;
							rcDraw.top = (rc.top + rc.bottom - checkHeight) / 2;
							rcDraw.bottom = rcDraw.top + checkHeight;
							POPUPCHECKBACKGROUNDSTATES bgCheckStateId =
								(stateId == MPI_DISABLED || stateId == MPI_DISABLEDHOT) ? MCB_DISABLED : MCB_NORMAL;
							DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPCHECKBACKGROUND, bgCheckStateId, &rcDraw, nullptr);

							rcDraw.left += parent->marginCheck.cxLeftWidth;
							rcDraw.top += parent->marginCheck.cyTopHeight;
							rcDraw.right -= parent->marginCheck.cxRightWidth;
							rcDraw.bottom -= parent->marginCheck.cyBottomHeight;
							POPUPCHECKSTATES checkStateId;
							if (omi->type & MFT_RADIOCHECK)
								checkStateId = (stateId == MPI_DISABLED || stateId == MPI_DISABLEDHOT) ? MC_BULLETDISABLED : MC_BULLETNORMAL;
							else
								checkStateId = (stateId == MPI_DISABLED || stateId == MPI_DISABLEDHOT) ? MC_CHECKMARKDISABLED : MC_CHECKMARKNORMAL;
							DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPCHECK, checkStateId, &rcDraw, nullptr);
						}

						rcDraw.left = rcGutter.right + parent->marginText.cxLeftWidth;
						rcDraw.top = (rc.top + rc.bottom - omi->sizeText.cy) / 2;
						rcDraw.bottom = rcDraw.top + omi->sizeText.cy;
						rcDraw.right = rcDraw.left + omi->sizeText.cx;
						DWORD flags = DT_SINGLELINE | DT_LEFT;
						if (dis->itemState & ODS_NOACCEL) flags |= DT_HIDEPREFIX;
						HFONT hFont = (omi->extType & EXT_TYPE_DEFAULT_ITEM) ? parent->fontBold : parent->fontNormal;
						HGDIOBJ oldFont = SelectObject(dis->hDC, hFont);
#ifdef USE_DRAW_THEME_TEXT
						DrawThemeText(parent->hTheme, dis->hDC, parent->partId, stateId,
							omi->text.c_str(), omi->getTextLength(), flags, 0, &rcDraw);
#else
						int oldBkMode = SetBkMode(dis->hDC, TRANSPARENT);
						DrawText(dis->hDC, omi->text.c_str(), omi->getTextLength(), &rcDraw, flags);
#endif
						if (omi->accelPos != -1)
						{
							rcDraw.right = rc.right - parent->marginItem.cxRightWidth - totalWidth(parent->marginSubmenu) - parent->sizeSubmenu.cx;
							rcDraw.left = rcDraw.right - parent->maxAccelWidth;
#ifdef USE_DRAW_THEME_TEXT
							DrawThemeText(parent->hTheme, dis->hDC, parent->partId, stateId,
								omi->text.c_str() + omi->accelPos + 1, -1, DT_SINGLELINE | DT_RIGHT | DT_NOPREFIX, 0, &rcDraw);
#else
							DrawText(dis->hDC, omi->text.c_str() + omi->accelPos + 1, -1, &rcDraw, DT_SINGLELINE | DT_RIGHT | DT_NOPREFIX);
#endif
						}
#ifndef USE_DRAW_THEME_TEXT
						SetBkMode(dis->hDC, oldBkMode);
#endif
						SelectObject(dis->hDC, oldFont);

						if (omi->extType & EXT_TYPE_SUBMENU)
						{
							POPUPSUBMENUSTATES submenuStateId = (dis->itemState & (ODS_INACTIVE | ODS_DISABLED)) ? MSM_DISABLED : MSM_NORMAL;
							rcDraw.right = rc.right - parent->marginItem.cxRightWidth - parent->marginSubmenu.cxRightWidth;
							rcDraw.left = rcDraw.right - parent->sizeSubmenu.cx;
							rcDraw.top = (rc.top + rc.bottom - parent->sizeSubmenu.cy) / 2;
							rcDraw.bottom = rcDraw.top + parent->sizeSubmenu.cy;
							DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPSUBMENU, submenuStateId, &rcDraw, nullptr);
						}
					}
				}
				else
				{
					static const int FLAG_SHOW_CHECKS = 1;
					static const int flags = FLAG_SHOW_CHECKS;

					if (omi->type & MFT_SEPARATOR)
					{
						// Non-themed separator has 1px left and right padding
						RECT rcSep;
						rcSep.left = rc.left + parent->marginItem.cxLeftWidth + 1;
						rcSep.right = rc.right - parent->marginItem.cxRightWidth - 1;
						rcSep.top = (rc.top + rc.bottom - 2) / 2;
						rcSep.bottom = rcSep.top + 2;
						DrawEdge(dis->hDC, &rcSep, EDGE_ETCHED, BF_TOP);
					}
					else
					{
						bool isSelected = (dis->itemState & (ODS_HOTLIGHT | ODS_SELECTED)) != 0;
						int bgSysColor = isSelected ? COLOR_HIGHLIGHT : COLOR_MENU;
						FillRect(dis->hDC, &rc, GetSysColorBrush(bgSysColor));

						int xText = rc.left + parent->marginItem.cxLeftWidth;
						int gapSize = 0;
						if ((flags & FLAG_SHOW_CHECKS) || parent->maxBitmapWidth)
						{
							if (flags & FLAG_SHOW_CHECKS)
								gapSize = totalWidth(parent->marginCheck) + parent->sizeCheck.cx;
							if (parent->maxBitmapWidth)
							{
								int bitmapSize = parent->maxBitmapWidth + totalWidth(parent->marginBitmap);
								if (bitmapSize > gapSize) gapSize = bitmapSize;
							}
							xText += gapSize;
						}

						COLORREF textColor;
						if (dis->itemState & ODS_GRAYED)
						{
							textColor = GetSysColor(COLOR_GRAYTEXT);
							if (textColor == GetSysColor(bgSysColor))
								textColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
						}
						else
							textColor = GetSysColor(isSelected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);
						if ((flags & FLAG_SHOW_CHECKS) && (dis->itemState & ODS_CHECKED))
						{
							int y = rc.top + (rc.bottom - rc.top - parent->sizeCheck.cy) / 2;
							int x = rc.left + parent->marginItem.cxLeftWidth + parent->marginCheck.cxLeftWidth;
#ifndef USE_EXACT_LOOKS
							if (parent->maxBitmapWidth)
								x = rc.left + parent->marginItem.cxLeftWidth + (gapSize - parent->sizeCheck.cx) / 2;
#endif
							parent->drawCompatBitmap(dis->hDC,
								x, y, parent->sizeCheck,
								(omi->type & MFT_RADIOCHECK) ? DFCS_MENUBULLET : DFCS_MENUCHECK,
								textColor);
						}

						HBITMAP bitmap = omi->bitmap;
						if (bitmap)
						{
							if (dis->itemState & ODS_GRAYED) bitmap = omi->getGrayBitmap(dis->hDC);
							int x = rc.left + parent->marginItem.cxLeftWidth + parent->marginBitmap.cxLeftWidth;
							int y = (rc.top + rc.bottom - omi->sizeBitmap.cy) / 2;
							WinUtil::drawAlphaBitmap(dis->hDC, bitmap, x, y, omi->sizeBitmap.cx, omi->sizeBitmap.cy);
						}

						RECT rcDraw;
						rcDraw.left = xText + parent->marginText.cxLeftWidth;
						rcDraw.top = rc.top + parent->marginText.cyTopHeight;
						rcDraw.bottom = rc.bottom - parent->marginText.cyBottomHeight;
						rcDraw.right = rc.right - parent->marginText.cxRightWidth;
						HGDIOBJ oldFont = SelectObject(dis->hDC,
							(omi->extType & EXT_TYPE_DEFAULT_ITEM) ? parent->fontBold : parent->fontNormal);
						int oldMode = SetBkMode(dis->hDC, TRANSPARENT);
						COLORREF oldColor = SetTextColor(dis->hDC, textColor);
						DWORD drawTextFlags = DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOCLIP;
						if (dis->itemState & ODS_NOACCEL) drawTextFlags |= DT_HIDEPREFIX;
						DrawText(dis->hDC, omi->text.c_str(), omi->getTextLength(), &rcDraw, drawTextFlags);

						if (omi->accelPos != -1)
						{
							rcDraw.right = rc.right - totalWidth(parent->marginSubmenu) - parent->sizeSubmenu.cx;
							rcDraw.left = rcDraw.right - parent->maxAccelWidth;
#ifdef USE_EXACT_LOOKS
							drawTextFlags = DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX;
#else
							drawTextFlags = DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX;
#endif
							DrawText(dis->hDC, omi->text.c_str() + omi->accelPos + 1, -1, &rcDraw, drawTextFlags);
						}

						SelectObject(dis->hDC, oldFont);
						SetBkMode(dis->hDC, oldMode);
						SetTextColor(dis->hDC, oldColor);

						if (omi->extType & EXT_TYPE_SUBMENU)
						{
							int right = rc.right - parent->marginSubmenu.cxRightWidth;
							int left = right - parent->sizeSubmenu.cx;
							int top = (rc.top + rc.bottom - parent->sizeSubmenu.cy) / 2;
							parent->drawCompatBitmap(dis->hDC,
								left, top, parent->sizeSubmenu,
								DFCS_MENUARROW, textColor);
						}
					}
				}
				ExcludeClipRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom);
				return TRUE;
			}
		}
	}
	return FALSE;
}

void OMenu::openTheme(HWND hwnd)
{
	partId = MENU_POPUPITEM;
#ifndef DISABLE_UXTHEME
	hTheme = OpenThemeData(hwnd, THEME_NAME);
#else
	hTheme = nullptr;
#endif
	createNormalFont();
	updateFontMetrics(hwnd);
	if (!hTheme)
	{
		initFallbackParams();
	}
	else
	{
		if (IsThemePartDefined(hTheme, MENU_POPUPITEM_FOCUSABLE, MPI_HOT))
			partId = MENU_POPUPITEM_FOCUSABLE;
		GetThemeMargins(hTheme, NULL, MENU_POPUPCHECK, 0, TMT_CONTENTMARGINS, nullptr, &marginCheck);
		GetThemeMargins(hTheme, NULL, MENU_POPUPCHECKBACKGROUND, 0, TMT_CONTENTMARGINS, nullptr, &marginCheckBackground);
		GetThemeMargins(hTheme, NULL, partId, 0, TMT_CONTENTMARGINS, nullptr, &marginItem);
		GetThemeMargins(hTheme, NULL, MENU_POPUPSUBMENU, 0, TMT_CONTENTMARGINS, NULL, &marginSubmenu);
		GetThemePartSize(hTheme, NULL, MENU_POPUPCHECK, 0, nullptr, TS_TRUE, &sizeCheck);
		GetThemePartSize(hTheme, NULL, MENU_POPUPSEPARATOR, 0, nullptr, TS_TRUE, &sizeSeparator);
		GetThemePartSize(hTheme, NULL, MENU_POPUPSUBMENU, 0, NULL, TS_TRUE, &sizeSubmenu);
		MARGINS margins;
		HRESULT hr = GetThemeMargins(hTheme, NULL, MENU_POPUPBORDERS, 0, TMT_CONTENTMARGINS, nullptr, &margins);
		SIZE size;
		hr = GetThemePartSize(hTheme, NULL, MENU_POPUPBORDERS, 0, NULL, TS_TRUE, &size);
		marginBitmap.cxLeftWidth = marginBitmap.cxRightWidth = size.cx;
		marginBitmap.cyTopHeight = marginBitmap.cyBottomHeight = size.cy;

		int popupBorderSize, popupBackgroundBorderSize;
		GetThemeInt(hTheme, partId, 0, TMT_BORDERSIZE,  &popupBorderSize);
		GetThemeInt(hTheme, MENU_POPUPBACKGROUND, 0, TMT_BORDERSIZE, &popupBackgroundBorderSize);

		marginText.cxLeftWidth = popupBackgroundBorderSize;
		marginText.cxRightWidth = popupBorderSize; // FIXME
		marginText.cyTopHeight = marginItem.cyTopHeight;
		marginText.cyBottomHeight = marginItem.cyBottomHeight;

		if (SysVersion::isOsWin11Plus())
		{
			marginHeader.cxLeftWidth = marginItem.cxLeftWidth + marginBitmap.cxLeftWidth;
			marginHeader.cxRightWidth = marginItem.cxRightWidth + marginBitmap.cxRightWidth;
			marginHeader.cyTopHeight = marginItem.cyTopHeight;
			marginHeader.cyBottomHeight = marginItem.cyBottomHeight;
		}
		else
			memset(&marginHeader, 0, sizeof(marginHeader));
	
		sizeSeparator.cx = 0;
		if (partId == MENU_POPUPITEM_FOCUSABLE &&
		    SUCCEEDED(GetThemeMargins(hTheme, NULL, MENU_POPUPITEM, 0, TMT_CONTENTMARGINS, nullptr, &margins)))
		{
			sizeSeparator.cx = std::max(margins.cxLeftWidth, margins.cxRightWidth);
		}
	}
}

#ifndef OBM_MNARROW
#define OBM_MNARROW 32739
#endif

void OMenu::initFallbackParams()
{
	int cxEdge = GetSystemMetrics(SM_CXEDGE);
	int cyEdge = GetSystemMetrics(SM_CYEDGE);
	marginCheck.cxLeftWidth = marginCheck.cxRightWidth = 0;
	marginCheck.cyTopHeight = marginCheck.cyBottomHeight = 0;
	sizeCheck.cx = GetSystemMetrics(SM_CXMENUCHECK);
	sizeCheck.cy = GetSystemMetrics(SM_CYMENUCHECK);

	HBITMAP hOemBitmap = LoadBitmap(NULL, (LPCTSTR) OBM_MNARROW);
	BITMAP bm;
	if (hOemBitmap && GetObject(hOemBitmap, sizeof(bm), &bm))
	{
		sizeSubmenu.cx = bm.bmWidth;
		sizeSubmenu.cy = bm.bmHeight;
	}

	marginText.cxLeftWidth = marginText.cxRightWidth = cxEdge;
	marginText.cyTopHeight = marginText.cyBottomHeight = cyEdge;

#ifdef USE_EXACT_LOOKS
	int cyBorder = GetSystemMetrics(SM_CYBORDER);
	marginBitmap.cxLeftWidth = cxEdge;
	marginBitmap.cxRightWidth = 0;
	marginBitmap.cyTopHeight = marginBitmap.cyBottomHeight = cyBorder;
#else
	marginBitmap.cxLeftWidth = std::max(3, cxEdge);
	marginBitmap.cxRightWidth = cxEdge;
	marginBitmap.cyTopHeight = marginBitmap.cyBottomHeight = std::max(3, cyEdge);
#endif

	memset(&marginCheckBackground, 0, sizeof(marginCheckBackground));
	memset(&marginItem, 0, sizeof(marginItem));
	//marginItem.cxLeftWidth = marginItem.cxRightWidth = 0;
	//marginItem.cyTopHeight = marginItem.cyBottomHeight = 1;
	marginCheck.cxLeftWidth = cxEdge;

	sizeSeparator.cx = 0;
	sizeSeparator.cy = GetSystemMetrics(SM_CYMENUSIZE) / 2;

	marginSubmenu.cxLeftWidth = avgCharWidth;
	memset(&marginHeader, 0, sizeof(marginHeader));
}

void OMenu::updateBackgroundBrush()
{
	HTHEME hTheme = OpenThemeData(nullptr, THEME_NAME);
	if (hTheme)
	{
		COLORREF color;
		if (FAILED(GetThemeColor(hTheme, MENU_POPUPBACKGROUND, 0, TMT_FILLCOLOR, &color)))
			color = getColorFromTheme(hTheme, 5, 5, MENU_POPUPBORDERS, 0);
		CloseThemeData(hTheme);
		if (color != CLR_INVALID)
		{
			if (bgBrush) DeleteObject(bgBrush);
			bgBrush = CreateSolidBrush(color);
			MENUINFO mi = { sizeof(mi) };
			mi.fMask = MIM_BACKGROUND;
			mi.hbrBack = bgBrush;
			::SetMenuInfo(m_hMenu, &mi);
		}
	}
}

void OMenu::updateBitmapHeight(HBITMAP hBitmap)
{
#ifndef USE_EXACT_LOOKS
	if (!minBitmapHeight && hBitmap)
	{
		BITMAP bm;
		if (GetObject(hBitmap, sizeof(bm), &bm))
			minBitmapHeight = abs(bm.bmHeight);
	}
#endif
}

bool OMenu::SetBitmap(UINT item, BOOL byPosition, HBITMAP hBitmap)
{
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	if (ownerDrawMode == OD_NEVER || ownerDrawMode == OD_IF_NEEDED)
	{
#ifdef OSVER_WIN_XP
		if (!SysVersion::isOsVistaPlus()) return false;
#endif
		mii.fMask = MIIM_BITMAP;
		mii.hbmpItem = hBitmap;
		return SetMenuItemInfo(item, byPosition, &mii) != FALSE;
	}
	if (ownerDrawMode == OD_ALWAYS)
	{
		mii.fMask = MIIM_DATA;
		if (!GetMenuItemInfo(item, byPosition, &mii)) return false;
		if (mii.dwItemData)
		{
			OMenuItem* mi = (OMenuItem*) mii.dwItemData;
			mi->bitmap = hBitmap;
			updateBitmapHeight(hBitmap);
			return true;
		}
	}
	return false;
}

HBITMAP OMenuItem::getGrayBitmap(HDC hdc) const
{
	if (grayBitmap || !bitmap) return grayBitmap;
	BITMAP bm;
	if (!GetObject(bitmap, sizeof(bm), &bm) || bm.bmBitsPixel != 32) return bitmap;

	int width = bm.bmWidth;
	int height = bm.bmHeight;
	unsigned size = width * height << 2;
	BITMAPINFO bmi;
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = size;
	bmi.bmiHeader.biXPelsPerMeter = 0;
	bmi.bmiHeader.biYPelsPerMeter = 0;
	bmi.bmiHeader.biClrUsed = 0;
	bmi.bmiHeader.biClrImportant = 0;

	BYTE* bits = new BYTE[size];
	if (!GetDIBits(hdc, bitmap, 0, height, bits, &bmi, DIB_RGB_COLORS)) return bitmap;
	BYTE* p = bits;
	for (int y = 0; y < height; y++)
		for(int x = 0; x < width; x++)
		{
			int alpha = p[3]/2;
			p[0] = p[1] = p[2] = BYTE((p[0] + 6*p[1] + 3*p[2])*alpha/2550);
			p[3] = BYTE(alpha);
			p += 4;
		}

	grayBitmap = CreateCompatibleBitmap(hdc, width, height);
	SetDIBits(hdc, grayBitmap, 0, height, bits, &bmi, DIB_RGB_COLORS);
	delete[] bits;
	return grayBitmap;
}

void OMenu::drawCompatBitmap(HDC hdc, int x, int y, SIZE size, int flags, COLORREF color)
{
	int index = 0;
	if (flags == DFCS_MENUARROW)
		index = 2;
	else if (flags == DFCS_MENUBULLET)
		index = 1;
	if (!bitmaps[index])
	{
		bitmaps[index] = WinUtil::createFrameControlBitmap(hdc, size.cx, size.cy, DFC_MENU, flags);
		if (!bitmaps[index]) return;
	}
	WinUtil::drawMonoBitmap(hdc, bitmaps[index], x, y, size.cx, size.cy, color);
}
