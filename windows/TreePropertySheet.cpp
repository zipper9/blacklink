/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#include "Resource.h"

#include "TreePropertySheet.h"
#include "ResourceLoader.h"
#include "WinUtil.h"
#include "../client/ResourceManager.h"

static const TCHAR SEPARATOR = _T('\\');

int TreePropertySheet::PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
	if (uMsg == PSCB_INITIALIZED)
	{
		::PostMessage(hwndDlg, WM_USER_INITDIALOG, 0, 0);
	}
	
	return CPropertySheet::PropSheetCallback(hwndDlg, uMsg, lParam);
}

LRESULT TreePropertySheet::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	destroyTimer();
	treeIcons.Destroy();
	return 0;
}

LRESULT TreePropertySheet::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	onTimerSec();
	return 0;
}

LRESULT TreePropertySheet::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
{
	if (icon)
	{
		SetIcon(icon, FALSE);
		SetIcon(icon, TRUE);		
	}
	
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
	if (BOOLSETTING(SETTINGS_WINDOW_TRANSP))
	{
		sliderPos = 255;
		setTransparency(sliderPos);
	}
#endif
	ResourceLoader::LoadImageList(IDR_SETTINGS_ICONS, treeIcons, 16, 16);
	hideTab();
	CenterWindow(GetParent());
	addTree();
	fillTree();
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
	if (BOOLSETTING(SETTINGS_WINDOW_TRANSP))
		addTransparencySlider();
#endif
	return 0;
}

#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
void TreePropertySheet::addTransparencySlider()
{
	CRect rectOk, rectWin;
	GetDlgItem(IDOK).GetWindowRect(rectOk);
	GetWindowRect(rectWin);
	ScreenToClient(rectOk);
	ScreenToClient(rectWin);
	rectOk.left = rectWin.left + 7;
	rectOk.right = rectOk.left + 80;
	slider.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_BOTH | TBS_NOTICKS, 0, IDC_PROPPAGE_TRANSPARENCY);
	slider.MoveWindow(rectOk);
	slider.SetRange(50, 255, TRUE);
	slider.SetPos(sliderPos);
	tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(tooltip.IsWindow());
	tooltip.AddTool(slider, ResourceManager::TRANSPARENCY_PROPPAGE);
	tooltip.Activate(TRUE);
}

void TreePropertySheet::setTransparency(int value)
{
	typedef bool (CALLBACK * LPFUNC)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
	LPFUNC _d_SetLayeredWindowAttributes = (LPFUNC)GetProcAddress(LoadLibrary(_T("user32")), "SetLayeredWindowAttributes");
	if (_d_SetLayeredWindowAttributes)
	{
		SetWindowLongPtr(GWL_EXSTYLE, GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYERED /*| WS_EX_TRANSPARENT*/);
		_d_SetLayeredWindowAttributes(m_hWnd, 0, value, LWA_ALPHA);
	}
}

LRESULT TreePropertySheet::onTranspChanged(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	setTransparency(slider.GetPos());
	return 0;
}
#endif // SCALOLAZ_PROPPAGE_TRANSPARENCY

void TreePropertySheet::hideTab()
{
	CRect rcClient, rcTab, rcPage, rcWindow;
	CWindow tab = GetTabControl();
	CWindow page = IndexToHwnd(0);
	GetClientRect(&rcClient);
	tab.GetWindowRect(&rcTab);
	tab.ShowWindow(SW_HIDE);
	if (page.IsWindow())
	{
		page.GetClientRect(&rcPage);
		page.MapWindowPoints(m_hWnd, &rcPage);
	}
	GetWindowRect(&rcWindow);
	::MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rcTab, 2);
	
	ScrollWindow(SPACE_LEFT + TREE_WIDTH + SPACE_MID - rcPage.left, SPACE_TOP - rcPage.top);
	rcWindow.right += SPACE_LEFT + TREE_WIDTH + SPACE_MID - rcPage.left - (rcClient.Width() - rcTab.right) + SPACE_RIGHT;
	rcWindow.bottom += SPACE_TOP - rcPage.top - SPACE_BOTTOM;
	
	MoveWindow(&rcWindow, TRUE);
	
	tabContainer.SubclassWindow(tab.m_hWnd);
}

void TreePropertySheet::addTree()
{
	// Insert the space to the left
	CRect rcPage;
	
	const HWND page = IndexToHwnd(0);
	::GetWindowRect(page, &rcPage);
	::MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rcPage, 2);
	
	CRect rc(SPACE_LEFT, rcPage.top, TREE_WIDTH, rcPage.bottom);
	ctrlTree.Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_EX_LAYERED |
	                WS_TABSTOP | WinUtil::getTreeViewStyle(), WS_EX_CLIENTEDGE, IDC_PAGE);
	WinUtil::setExplorerTheme(ctrlTree);
	ctrlTree.SetImageList(treeIcons, TVSIL_NORMAL);
}

void TreePropertySheet::fillTree()
{
	CTabCtrl tab = GetTabControl();
	const int pages = tab.GetItemCount();
	LocalArray<TCHAR, MAX_NAME_LENGTH> buf;
	TCITEM item = {0};
	item.mask = TCIF_TEXT;
	item.pszText = buf.data();
	item.cchTextMax = MAX_NAME_LENGTH - 1;
	
	HTREEITEM first = NULL;
	for (int i = 0; i < pages; ++i)
	{
		tab.GetItem(i, &item);
		int image = getItemImage(i);
		if (i == 0)
			first = addItem(buf.data(), TVI_ROOT, i, image);
		else
			addItem(buf.data(), TVI_ROOT, i, image);
	}
	if (SETTING(REMEMBER_SETTINGS_PAGE))
		ctrlTree.SelectItem(findItem(SETTING(SETTINGS_PAGE), ctrlTree.GetRootItem()));
	else
		ctrlTree.SelectItem(first);
	createTimer(1000);
}

HTREEITEM TreePropertySheet::addItem(const tstring& str, HTREEITEM parent, int page, int image)
{
	TVINSERTSTRUCT tvi = {0};
	tvi.hInsertAfter = TVI_LAST;
	tvi.hParent = parent;
	
	const HTREEITEM first = (parent == TVI_ROOT) ? ctrlTree.GetRootItem() : ctrlTree.GetChildItem(parent);
	
	string::size_type i = str.find(SEPARATOR);
	if (i == string::npos)
	{
		// Last dir, the actual page
		HTREEITEM item = findItem(str, first);
		if (item == NULL)
		{
			// Doesn't exist, add
			tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
			tvi.item.pszText = const_cast<LPTSTR>(str.c_str());
			tvi.item.lParam = page;
			item = ctrlTree.InsertItem(&tvi);
			ctrlTree.SetItemImage(item, image, image);
			ctrlTree.Expand(parent);
			return item;
		}
		else
		{
			// Update page
			if (ctrlTree.GetItemData(item) == -1)
				ctrlTree.SetItemData(item, page);
			return item;
		}
	}
	else
	{
		tstring name = str.substr(0, i);
		HTREEITEM item = findItem(name, first);
		if (!item) item = first;
		ctrlTree.Expand(parent);
		return addItem(str.substr(i + 1), item, page, image);
	}
}

HTREEITEM TreePropertySheet::findItem(const tstring& str, HTREEITEM start)
{
	LocalArray<TCHAR, MAX_NAME_LENGTH> buf;
	while (start != NULL)
	{
		ctrlTree.GetItemText(start, buf.data(), MAX_NAME_LENGTH - 1);
		if (lstrcmp(str.c_str(), buf.data()) == 0) // TODO PVS
		{
			return start;
		}
		start = ctrlTree.GetNextSiblingItem(start);
	}
	return start;
}

HTREEITEM TreePropertySheet::findItem(int page, HTREEITEM start)
{
	while (start != NULL)
	{
		if (((int)ctrlTree.GetItemData(start)) == page)
			return start;
		const HTREEITEM ret = findItem(page, ctrlTree.GetChildItem(start));
		if (ret != NULL)
			return ret;
		start = ctrlTree.GetNextSiblingItem(start);
	}
	return NULL;
}

LRESULT TreePropertySheet::onSelChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /* bHandled */)
{
	NMTREEVIEW* nmtv = (NMTREEVIEW*)pnmh;
	int page = nmtv->itemNew.lParam;
	if (page == -1)
	{
		HTREEITEM next = ctrlTree.GetChildItem(nmtv->itemNew.hItem);
		if (next == nullptr)
		{
			next = ctrlTree.GetNextSiblingItem(nmtv->itemNew.hItem);
			if (next == nullptr)
			{
				next = ctrlTree.GetParentItem(nmtv->itemNew.hItem);
				if (next != nullptr)
				{
					next = ctrlTree.GetNextSiblingItem(next);
				}
			}
		}
		if (next != nullptr)
			ctrlTree.SelectItem(next);
	}
	else
	{
		int oldPage = HwndToIndex(GetActivePage());
		if (oldPage != page)
		{
			SetActivePage(page);
			pageChanged(oldPage, page);
			if (SETTING(REMEMBER_SETTINGS_PAGE))
				SET_SETTING(SETTINGS_PAGE, page);
		}
	}
	return 0;
}

LRESULT TreePropertySheet::onSetCurSel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlTree.SelectItem(findItem((int)wParam, ctrlTree.GetRootItem()));
	bHandled = FALSE;
	return 0;
}
