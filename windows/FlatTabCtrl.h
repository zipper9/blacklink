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

#ifndef FLAT_TAB_CTRL_H
#define FLAT_TAB_CTRL_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlcrack.h>

#include "TabPreviewWindow.h"
#include "WinUtil.h"
#include "ConfUI.h"
#include <vector>
#include <list>
#include <algorithm>
#include "resource.h"

#ifdef IRAINMAN_INCLUDE_GDI_OLE
void setActiveMDIWindow(HWND hwnd);
#endif

enum
{
	FT_FIRST = WM_APP + 700,
	FTM_SELECTED, // This will be sent when the user presses a tab. WPARAM = HWND
	FTM_ROWS_CHANGED, // The number of rows changed
	FTM_SETACTIVE, // Set currently active tab to the HWND pointed by WPARAM
	FTM_CONTEXTMENU, // Display context menu and return TRUE, or return FALSE for the default one
	FTM_GETOPTIONS, // Get tab options from client
	WMU_REALLY_CLOSE
};

struct FlatTabOptions
{
	HICON icons[2];
	bool isHub;
};

class FlatTabCtrl : public CWindowImpl<FlatTabCtrl, CWindow, CControlWinTraits>
{
public:
	enum
	{
		TEXT_LEN_MIN = 7,
		TEXT_LEN_MAX = 80,
		TEXT_LEN_FULL = 96,
		CLOSE_BUTTON_SIZE = 16,
		ICON_SIZE = 16
	};

	FlatTabCtrl();
	~FlatTabCtrl() { cleanup(); }

	static LPCTSTR GetWndClassName()
	{
		return _T("FlatTabCtrl");
	}

	void addTab(HWND hWnd);
	void removeTab(HWND hWnd);
	void startSwitch();
	void endSwitch();
	HWND getNext();
	HWND getPrev();
	void setActive(HWND hWnd);
	bool isActive(HWND hWnd) const;
	HWND getActive() const;
	void setTop(HWND hWnd);
	void setDirty(HWND hWnd);
	void setIcon(HWND hWnd, HICON icon, bool disconnected);
	void setDisconnected(HWND hWnd, bool disconnected);
	void setTooltipText(HWND hWnd, const tstring& text);
	void updateText(HWND hWnd, LPCTSTR text);
	void setCountMessages(HWND hWnd, int messageCount);
	int getTabsPosition() const { return tabsPosition; }
	int getContextMenuAlign() const
	{
		return getTabsPosition() == Conf::TABS_TOP ? TPM_TOPALIGN : TPM_BOTTOMALIGN;
	}
	int getTabCount() const { return (int) tabs.size(); }

	bool updateSettings(bool invalidate);

	BEGIN_MSG_MAP(thisClass)
	MESSAGE_HANDLER(WM_SIZE, onSize)
	MESSAGE_HANDLER(WM_CREATE, onCreate)
	MESSAGE_HANDLER(WM_DESTROY, onDestroy)
	MESSAGE_HANDLER(WM_PAINT, onPaint)
	MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
	MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
	MESSAGE_HANDLER(WM_LBUTTONUP, onLButtonUp)
	MESSAGE_HANDLER(WM_MBUTTONUP, onMButtonUp)
	MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
	MESSAGE_HANDLER(WM_MOUSELEAVE, onMouseLeave)
	MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
	MESSAGE_HANDLER(WM_CAPTURECHANGED, onCaptureChanged)
	COMMAND_ID_HANDLER(IDC_CHEVRON, onChevron)
	COMMAND_RANGE_HANDLER(IDC_SELECT_WINDOW, IDC_SELECT_WINDOW + tabs.size(), onSelectWindow)
	END_MSG_MAP()

	LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);

	void switchTo(bool next = true);

	int getTabHeight() const { return height; }
	int getHeight() const { return getRows() * getTabHeight() + 2*edgeHeight; }
	int getFill() const { return (getTabHeight() + 1) / 2; }
	int getRows() const { return rows; }
	int getEdgeHeight() const { return edgeHeight; }

	LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
	LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
	LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
	LRESULT onChevron(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
	LRESULT onSelectWindow(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
	LRESULT onCaptureChanged(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & /*bHandled*/);

	void processTooltipPop(HWND hWnd);

private:
	class TabInfo
	{
	public:
		typedef std::vector<TabInfo *> List;
		typedef List::iterator ListIter;

		TabInfo(HWND hWnd, unsigned tabChars)
		: hWnd(hWnd), len(0), xpos(0), row(0), dirty(false), disconnected(false),
		  maxLength(std::min<unsigned>(TEXT_LEN_MAX, std::max<unsigned>(tabChars, TEXT_LEN_MIN)))
		{
			size.cx = size.cy = 0;
			boldSize.cx = boldSize.cy = 0;
			hIcons[0] = hIcons[1] = nullptr;
			update();
		}

		unsigned maxLength;
		HWND hWnd;
		size_t len;
		tstring text;
		tstring textEllip;
		tstring tooltipText;
		SIZE size;
		SIZE boldSize;
		int xpos;
		int row;
		bool dirty;
		bool disconnected;

		HICON hIcons[2];

		bool update();
		void updateSize(HDC hdc);
		bool updateText(LPCTSTR str);
		void setMaxLength(unsigned maxLength, HDC hdc);
		HICON getIcon() const;
	};

	int getWidth(const TabInfo* t) const;
	int getRowFromPos(int yPos) const;
	void calcRows(bool invalidate = true);
	void moveTabs(TabInfo* tabToMove, const TabInfo* t, bool after);
	void setMoving(bool flag, POINT pt = POINT{}, RECT* tabRect = nullptr);

	bool showIcons;
	bool showCloseButton;
	bool useBoldNotif;
	bool nonHubsFirst;
	int tabsPosition;
	unsigned tabChars;
	int maxRows;

	enum
	{
		INACTIVE_BACKGROUND_COLOR,
		ACTIVE_BACKGROUND_COLOR,
		INACTIVE_TEXT_COLOR,
		ACTIVE_TEXT_COLOR,
		OFFLINE_BACKGROUND_COLOR,
		OFFLINE_ACTIVE_BACKGROUND_COLOR,
		UPDATED_BACKGROUND_COLOR,
		BORDER_COLOR,
		INSERTION_MARK_COLOR,
		MAX_COLORS
	};

	COLORREF colors[MAX_COLORS];

	CButton chevron;
	CToolTipCtrl tooltip;
	CImageList closeButtonImages;
	CMenu menu;
	class BackingStore* backingStore;
	TabPreviewWindow preview;

	int rows;
	int height;
	int textHeight;
	int edgeHeight;
	int startMargin;
	int horizIconSpace;
	int horizPadding;
	int chevronWidth;

	TabInfo::List tabs;

	TabInfo* active;
	TabInfo* pressed;
	const TabInfo *hoverTab;
	const TabInfo *closeButtonPressedTab;
	const TabInfo *insertionTab;
	bool closeButtonHover;
	bool insertAfter;
	bool moving;
	tstring tooltipText;

	std::list<HWND> viewOrder;
	std::list<HWND>::iterator nextTab;

	bool inTab;

	TabInfo *getTabInfo(HWND hWnd) const;
	int getTopPos(const TabInfo *tab) const;
	void getCloseButtonRect(const TabInfo* t, RECT& rc) const;

	static COLORREF getLighterColor(COLORREF color);
	static COLORREF getDarkerColor(COLORREF color);

	enum
	{
		DRAW_TAB_ACTIVE       = 1,
		DRAW_TAB_FIRST_IN_ROW = 2,
		DRAW_TAB_LAST_IN_ROW  = 4
	};

	void drawTab(HDC hdc, const TabInfo *tab, int options);
	void drawInsertionMark(HDC hdc, int xpos, int ypos);
	void cleanup();
};

#endif // !defined(FLAT_TAB_CTRL_H)
