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
#include <atlgdiraii.h>
#include "OMenu.h"
#include "WinUtil.h"
#include "BarShader.h"

struct OMenuItem
{
	OMenuItem(const OMenuItem&) = delete;
	OMenuItem& operator= (const OMenuItem&) = delete;

	OMenuItem(OMenu* parent) : parent(parent), accelPos(-1) {}

	OMenu* const parent;
	tstring text;
	void*   data;
	UINT    type;
	UINT    extType;
	SIZE    sizeText;
	int     accelPos;

	int getTextLength() const
	{
		return accelPos == -1 ? static_cast<int>(text.length()) : accelPos;
	}
};

enum
{
	EXT_TYPE_SUBMENU      = 1,
	EXT_TYPE_HEADER       = 2,
	EXT_TYPE_DEFAULT_ITEM = 4
};

OMenu::OMenu() :
	ownerDrawMode(OD_DEFAULT),
	themeInitialized(false), hTheme(nullptr),
	fontNormal(nullptr), fontBold(nullptr),
	textMeasured(false)
{
	marginCheck = { 0 };
	marginCheckBackground = { 0 };
	marginItem = { 0 };
	marginText = { 0 };
	marginAccelerator = { 0 };
	marginSubmenu = { 0 };
	sizeCheck = { 0 };
	sizeSeparator = { 0 };
	sizeSubmenu = { 0 };
	maxTextWidth = maxAccelWidth = 0;
}

OMenu::~OMenu()
{
	if (::IsMenu(m_hMenu))
	{
		DestroyMenu();
	}
	else
	{
		dcassert(items.empty());
		for (auto i = items.cbegin(); i != items.cend(); ++i)
			delete *i;
	}
	if (fontNormal) DeleteObject(fontNormal);
	if (fontBold) DeleteObject(fontBold);
	if (hTheme) CloseThemeData(hTheme);
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
		ownerDrawMode = IsAppThemed() ? OD_ALWAYS : OD_IF_NEEDED;
	return CMenu::CreatePopupMenu();
}

BOOL OMenu::InsertSeparator(UINT uItem, BOOL byPosition, const tstring& caption)
{
	if (ownerDrawMode == OD_NEVER)
		return FALSE;
	OMenuItem* omi = new OMenuItem(this);
	omi->text = caption;
	omi->sizeText = { 0, 0 };
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
	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_TYPE | MIIM_DATA | MIIM_SUBMENU;
	GetMenuItemInfo(uItem, byPosition, &mii);
	
	if (mii.dwItemData != NULL)
	{
		OMenuItem* mi = (OMenuItem*) mii.dwItemData;
		if (mii.fType & MFT_OWNERDRAW)
		{
			auto i = find(items.begin(), items.end(), mi);
			if (i != items.end())
				items.erase(i);
			else
				dcassert(0);
		}
		delete mi;
	}
}

BOOL OMenu::InsertMenuItem(UINT uItem, BOOL bByPosition, LPMENUITEMINFO lpmii)
{
	if (ownerDrawMode == OD_NEVER)
		return CMenu::InsertMenuItem(uItem, bByPosition, lpmii);
	OMenuItem* omi = new OMenuItem(this);
	if (lpmii->dwTypeData) omi->text = lpmii->dwTypeData;
	omi->sizeText = { 0, 0 };
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

BOOL OMenu::AppendMenu(UINT nFlags, UINT_PTR nIDNewItem, LPCTSTR lpszNewItem)
{
	if (ownerDrawMode == OD_NEVER)
		return CMenu::AppendMenu(nFlags, nIDNewItem, lpszNewItem);
	ATLASSERT(::IsMenu(m_hMenu));
	int pos = GetMenuItemCount();
	OMenuItem* omi = new OMenuItem(this);
	if (lpszNewItem) omi->text = lpszNewItem;
	omi->sizeText = { 0, 0 };
	omi->data = nullptr;
	omi->extType = 0;
	MENUITEMINFO mii = { sizeof(mii) };
	mii.dwItemData = (ULONG_PTR) omi;
	mii.fMask = MIIM_DATA | MIIM_STRING | MIIM_FTYPE | MIIM_STATE;
	mii.dwTypeData = const_cast<LPTSTR>(lpszNewItem);
	if (nFlags & (MF_DISABLED | MF_GRAYED))
	{
		nFlags &= ~(MF_DISABLED | MF_GRAYED);
		//mii.fState |= MFS_DISABLED;
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
	if (!CMenu::InsertMenuItem(pos, TRUE, &mii))
	{
		dcassert(0);
		delete omi;
		return FALSE;
	}
	items.push_back(omi);
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

LRESULT OMenu::onInitMenuPopup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = TRUE;
	HMENU menu = (HMENU) wParam;
	::DefWindowProc(hWnd, uMsg, wParam, lParam);
	int count = ::GetMenuItemCount(menu);
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	mii.fMask = MIIM_TYPE | MIIM_DATA;
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
			omi->parent->textMeasured = false;
			if (!omi->parent->themeInitialized)
			{
				omi->parent->openTheme(hWnd);
				omi->parent->themeInitialized = true;
			}
		}
	}
	return FALSE;
}

bool OMenu::getMenuFont(LOGFONT& lf) const
{
	if (hTheme)
	{
		HRESULT hr = GetThemeSysFont(hTheme, TMT_MENUFONT, &lf);
		if (SUCCEEDED(hr)) return true;
	}
	NONCLIENTMETRICS ncm;
	memset(&ncm, 0, sizeof(ncm));
	ncm.cbSize = sizeof(ncm);
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
	lf.lfWeight = FW_BOLD;
	fontBold = CreateFontIndirect(&lf);
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
					height = parent->sizeCheck.cy + totalHeight(parent->marginCheckBackground) + totalHeight(parent->marginCheck);
					width = parent->sizeCheck.cx + totalWidth(parent->marginCheckBackground) + totalWidth(parent->marginCheck);
					width += parent->maxTextWidth + totalWidth(parent->marginText);
					if (parent->maxAccelWidth)
						width += parent->maxAccelWidth;
					int textHeight = omi->sizeText.cy + totalHeight(parent->marginText);
					if (textHeight > height) height = textHeight;
					width += totalWidth(parent->marginItem);
					width += totalWidth(parent->marginSubmenu) + parent->sizeSubmenu.cx;
				}
				mis->itemWidth = width;
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

				CRect rc(dis->rcItem);
				
				if ((omi->type & MFT_SEPARATOR) && (omi->extType & EXT_TYPE_HEADER))
				{
					bHandled = TRUE;
					/*
					rc.left += parent->marginItem.cxLeftWidth;
					rc.top += parent->marginItem.cyTopHeight;
					rc.right -= parent->marginItem.cxRightWidth;
					rc.bottom -= parent->marginItem.cyBottomHeight;
					*/
				
					CDC dc;
					dc.Attach(dis->hDC);
					if (BOOLSETTING(MENUBAR_TWO_COLORS))
						OperaColors::FloodFill(dc, rc.left, rc.top, rc.right, rc.bottom, SETTING(MENUBAR_LEFT_COLOR), SETTING(MENUBAR_RIGHT_COLOR), BOOLSETTING(MENUBAR_BUMPED));
					else
						dc.FillSolidRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SETTING(MENUBAR_LEFT_COLOR));
					
					dc.SetBkMode(TRANSPARENT);
					dc.SetTextColor(OperaColors::TextFromBackground(SETTING(MENUBAR_LEFT_COLOR)));
					{
						CSelectFont selectFont(dc, parent->fontBold);
						dc.DrawText(omi->text.c_str(), omi->text.length(), rc, DT_CENTER | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
					}
					dc.Detach();
				}
				else
				if (parent->hTheme)
				{
					bHandled = TRUE;
					POPUPITEMSTATES stateId = toItemStateId(dis->itemState);
					if (IsThemeBackgroundPartiallyTransparent(parent->hTheme, MENU_POPUPITEM, stateId))
						DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPBACKGROUND, 0, &rc, nullptr);

					RECT rcGutter;
					rcGutter.left = rc.left;
					rcGutter.top = rc.top;
					rcGutter.right = rcGutter.left + parent->sizeCheck.cx + totalWidth(parent->marginCheck) + totalWidth(parent->marginCheckBackground) + parent->marginItem.cxLeftWidth;
					rcGutter.bottom = rc.bottom;
					DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPGUTTER, 0, &rcGutter, nullptr);

					if (omi->type & MFT_SEPARATOR)
					{
						RECT rcSep;
						rcSep.left = rcGutter.right + parent->marginItem.cxLeftWidth + parent->marginItem.cxLeftWidth;
						rcSep.right = rc.right - parent->marginItem.cxRightWidth - parent->marginItem.cxRightWidth;
						rcSep.top = (rc.top + rc.bottom - parent->sizeSeparator.cy) / 2;
						rcSep.bottom = rcSep.top + parent->sizeSeparator.cy;
						DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPSEPARATOR, 0, &rcSep, nullptr);
					}
					else
					{
						RECT rcDraw = rc;
						rcDraw.left += parent->marginItem.cxLeftWidth;
						rcDraw.right -= parent->marginItem.cxRightWidth;
						DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPITEM, stateId, &rcDraw, nullptr);

						if (dis->itemState & ODS_CHECKED)
						{
							int checkWidth = parent->sizeCheck.cx + totalWidth(parent->marginCheck);
							int checkHeight = parent->sizeCheck.cy + totalHeight(parent->marginCheck);
							rcDraw.left = rc.left + parent->marginItem.cxLeftWidth;
							rcDraw.right = rcDraw.left + checkWidth;
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
						DrawThemeText(parent->hTheme, dis->hDC, MENU_POPUPITEM, stateId,
							omi->text.c_str(), omi->getTextLength(), flags, 0, &rcDraw);

						if (omi->accelPos != -1)
						{
							rcDraw.right = rc.right - totalWidth(parent->marginSubmenu) - parent->sizeSubmenu.cx;
							rcDraw.left = rcDraw.right - parent->maxAccelWidth;
							DrawThemeText(parent->hTheme, dis->hDC, MENU_POPUPITEM, stateId,
								omi->text.c_str() + omi->accelPos + 1, -1, DT_SINGLELINE | DT_RIGHT | DT_NOPREFIX, 0, &rcDraw);
						}

						if (omi->extType & EXT_TYPE_SUBMENU)
						{
							POPUPSUBMENUSTATES submenuStateId = (dis->itemState & (ODS_INACTIVE | ODS_DISABLED)) ? MSM_DISABLED : MSM_NORMAL;
							rcDraw.right = rc.right - parent->marginSubmenu.cxRightWidth;
							rcDraw.left = rcDraw.right - parent->sizeSubmenu.cx;
							rcDraw.top = (rc.top + rc.bottom - parent->sizeSubmenu.cy) / 2;
							rcDraw.bottom = rcDraw.top + parent->sizeSubmenu.cy;
							DrawThemeBackground(parent->hTheme, dis->hDC, MENU_POPUPSUBMENU, submenuStateId, &rcDraw, nullptr);
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
	hTheme = OpenThemeData(hwnd, VSCLASS_MENU);
	if (!hTheme)
	{
		initFallbackParams();
		return;
	}
	
	GetThemeMargins(hTheme, NULL, MENU_POPUPCHECK, 0, TMT_CONTENTMARGINS, nullptr, &marginCheck); 
	GetThemeMargins(hTheme, NULL, MENU_POPUPCHECKBACKGROUND, 0, TMT_CONTENTMARGINS, nullptr, &marginCheckBackground);
	GetThemeMargins(hTheme, NULL, MENU_POPUPITEM, 0, TMT_CONTENTMARGINS, nullptr, &marginItem);
	GetThemeMargins(hTheme, NULL, MENU_POPUPSUBMENU, 0, TMT_CONTENTMARGINS, NULL, &marginSubmenu); 
	GetThemePartSize(hTheme, NULL, MENU_POPUPCHECK, 0, nullptr, TS_TRUE, &sizeCheck);
	GetThemePartSize(hTheme, NULL, MENU_POPUPSEPARATOR, 0, nullptr,  TS_TRUE, &sizeSeparator);
	GetThemePartSize(hTheme, NULL, MENU_POPUPSUBMENU, 0, NULL, TS_TRUE, &sizeSubmenu);
	
	int popupBorderSize, popupBackgroundBorderSize;
	GetThemeInt(hTheme, MENU_POPUPITEM, 0, TMT_BORDERSIZE,  &popupBorderSize);
	GetThemeInt(hTheme, MENU_POPUPBACKGROUND, 0, TMT_BORDERSIZE, &popupBackgroundBorderSize);

	marginText.cxLeftWidth = popupBackgroundBorderSize;
	marginText.cxRightWidth = popupBorderSize;
	marginText.cyTopHeight = marginItem.cyTopHeight;
	marginText.cyBottomHeight = marginItem.cyBottomHeight;
}

void OMenu::initFallbackParams()
{
	marginCheck.cxLeftWidth = marginCheck.cxRightWidth = GetSystemMetrics(SM_CXEDGE);
	marginCheck.cyTopHeight = marginCheck.cyBottomHeight = GetSystemMetrics(SM_CYEDGE);
	sizeCheck.cx = GetSystemMetrics(SM_CXMENUCHECK);
	sizeCheck.cy = GetSystemMetrics(SM_CYMENUCHECK);

	marginText.cxLeftWidth = marginText.cxRightWidth = 8;
	marginText.cyTopHeight = marginText.cyBottomHeight = 4;
}
