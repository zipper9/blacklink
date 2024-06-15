#include "stdafx.h"
#include "DirectoryListingFrm.h"
#include "Fonts.h"
#include "GdiUtil.h"
#include "../client/SysVersion.h"

DirectoryListingNavWnd::DirectoryListingNavWnd() : navBarHeight(-1), isAppThemed(false), isVisible(true)
{
	int dpi = WinUtil::getDisplayDpi();
	navBarMinWidth = 160 * dpi / 96;
	space = 4 * dpi / 96;
}

LRESULT DirectoryListingNavWnd::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	isAppThemed = IsAppThemed() != FALSE && SysVersion::isOsVistaPlus();
	navBar.setFont(Fonts::g_systemFont, false);
	navBar.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE);

	ctrlNavigation.Create(m_hWnd, rcDefault, NULL,
		WS_CHILD | WS_VISIBLE | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_FLAT,
		0);
	ctrlNavigation.SetImageList(g_navigationImage.getIconList());

	ctrlButtons.Create(m_hWnd, rcDefault, NULL,
		WS_CHILD | WS_VISIBLE | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_LIST | TBSTYLE_FLAT,
		TBSTYLE_EX_MIXEDBUTTONS);

	return 0;
}

LRESULT DirectoryListingNavWnd::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	return 0;
}

LRESULT DirectoryListingNavWnd::onEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	RECT rc;
	GetClientRect(&rc);
	FillRect((HDC) wParam, &rc, GetSysColorBrush(isAppThemed ? COLOR_WINDOW : COLOR_BTNFACE));
	return 1;
}

LRESULT DirectoryListingNavWnd::onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	updateNavBarHeight();

	RECT rcParent;
	GetClientRect(&rcParent);

	SIZE size1, size2;
	ctrlNavigation.GetMaxSize(&size1);
	ctrlButtons.GetMaxSize(&size2);
	if (size1.cx + navBarMinWidth + space > rcParent.right)
	{
		hideControl(ctrlNavigation);
		hideControl(navBar);
		hideControl(ctrlButtons);
		isVisible = false;
		return 0;
	}

	isVisible = true;
	int buttonsSize = size2.cx + 2 * space;
	if (size1.cx + navBarMinWidth + size2.cx + 3 * space > rcParent.right)
	{
		buttonsSize = 0;
		hideControl(ctrlButtons);
	}

	RECT rc;
	rc.left = rcParent.left;
	rc.top = (rcParent.top + rcParent.bottom - size1.cy) / 2;
	rc.right = rc.left + size1.cx;
	rc.bottom = rc.top + size1.cy;
	ctrlNavigation.SetWindowPos(nullptr, &rc, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);

	rc.left = rc.right + space;
	rc.right = rcParent.right - buttonsSize;
	rc.top = (rcParent.top + rcParent.bottom - navBarHeight) / 2;
	rc.bottom = rc.top + navBarHeight;
	navBar.SetWindowPos(nullptr, &rc, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);

	if (buttonsSize)
	{
		rc.left = rc.right + space;
		rc.right = rcParent.right;
		rc.top = (rcParent.top + rcParent.bottom - size2.cy) / 2;
		rc.bottom = rc.top + size2.cy;
		ctrlButtons.SetWindowPos(nullptr, &rc, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
	}

	return 0;
}

LRESULT DirectoryListingNavWnd::onCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (lParam && ((HWND) lParam == ctrlButtons.m_hWnd || (HWND) lParam == ctrlNavigation.m_hWnd))
		GetParent().SendMessage(WM_COMMAND, wParam, lParam);
	return 0;
}

LRESULT DirectoryListingNavWnd::onThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	isAppThemed = IsAppThemed() != FALSE && SysVersion::isOsVistaPlus();
	navBarHeight = -1;
	return 0;
}

int DirectoryListingNavWnd::updateNavBarHeight()
{
	if (navBarHeight != -1) return navBarHeight;
	HDC hdc = GetDC();
	if (!hdc) return -1;
	navBarHeight = NavigationBar::getPrefHeight(hdc, Fonts::g_systemFont);
	ReleaseDC(hdc);
	return navBarHeight;
}

void DirectoryListingNavWnd::updateNavBar(const DirectoryListing::Directory* dir, const string& path, const DirectoryListingFrame* frame)
{
	navBar.setEditMode(false);
	int icon = IconBitmaps::FOLDER;
	if (!dir->getParent())
		icon = frame->getDclstFlag() ? IconBitmaps::DCLST : IconBitmaps::FILELIST;
	navBar.removeAllItems();
	navBar.setIcon(g_iconBitmaps.getBitmap(icon, 0));
	navBar.addArrowItem(PATH_ITEM_ROOT);
	navBar.addTextItem(Text::toT(frame->getNick()), PATH_ITEM_ROOT, !dir->directories.empty());
	string::size_type pos = 0;
	bool hasArrow = true;
	while (pos < path.length())
	{
		string::size_type nextPos = path.find('\\', pos);
		if (nextPos == string::npos) break;
		if (nextPos == path.length() - 1) hasArrow = !dir->directories.empty();
		navBar.addTextItem(Text::toT(path.substr(pos, nextPos - pos)), PATH_ITEM_FOLDER, hasArrow);
		pos = nextPos + 1;
	}
	navBar.Invalidate();
	enableNavigationButton(IDC_NAVIGATION_UP, dir->getParent() != nullptr);
}

void DirectoryListingNavWnd::enableNavigationButton(int idc, bool enable)
{
	ctrlNavigation.EnableButton(idc, (BOOL) enable);
}

void DirectoryListingNavWnd::enableControlButton(int idc, bool enable)
{
	ctrlButtons.EnableButton(idc, (BOOL) enable);
}

void DirectoryListingNavWnd::initToolbars(const DirectoryListingFrame* frame)
{
	TBBUTTON tbb = {};
	tbb.iBitmap = 0;
	tbb.idCommand = IDC_NAVIGATION_BACK;
	tbb.fsState = TBSTATE_ENABLED;
	tbb.fsStyle = BTNS_BUTTON;
	ctrlNavigation.AddButton(&tbb);
	tbb.iBitmap = 1;
	tbb.idCommand = IDC_NAVIGATION_FORWARD;
	ctrlNavigation.AddButton(&tbb);
	tbb.iBitmap = 2;
	tbb.idCommand = IDC_NAVIGATION_UP;
	ctrlNavigation.AddButton(&tbb);

	ctrlButtons.SetImageList(g_navigationImage.getIconList());
	tbb.iBitmap = 3;
	tbb.idCommand = IDC_FIND;
	tbb.iString = (INT_PTR) CTSTRING(FIND);
	tbb.fsStyle = BTNS_SHOWTEXT | BTNS_AUTOSIZE;
	ctrlButtons.AddButton(&tbb);
	tbb.iBitmap = I_IMAGENONE;
	tbb.idCommand = IDC_PREV;
	tbb.iString = (INT_PTR) CTSTRING(PREV);
	ctrlButtons.AddButton(&tbb);
	tbb.idCommand = IDC_NEXT;
	tbb.iString = (INT_PTR) CTSTRING(NEXT);
	ctrlButtons.AddButton(&tbb);
	tbb.fsStyle = BTNS_SEP;
	ctrlButtons.AddButton(&tbb);
	tbb.fsStyle = BTNS_SHOWTEXT | BTNS_AUTOSIZE;
	tbb.idCommand = IDC_MATCH_QUEUE;
	tbb.iString = (INT_PTR) (frame->isOwnList() ? CTSTRING(FIND_DUPLICATES) : CTSTRING(MATCH_QUEUE));
	ctrlButtons.AddButton(&tbb);
	tbb.idCommand = IDC_FILELIST_DIFF;
	tbb.iString = (INT_PTR) CTSTRING(FILE_LIST_DIFF);
	ctrlButtons.AddButton(&tbb);

	enableNavigationButton(IDC_NAVIGATION_BACK, false);
	enableNavigationButton(IDC_NAVIGATION_FORWARD, false);
	enableNavigationButton(IDC_NAVIGATION_UP, false);
	enableControlButton(IDC_PREV, false);
	enableControlButton(IDC_NEXT, false);
}

void DirectoryListingNavWnd::hideControl(HWND hWnd)
{
	if (::GetWindowLong(hWnd, GWL_STYLE) & WS_VISIBLE)
		::ShowWindow(hWnd, SW_HIDE);
}
