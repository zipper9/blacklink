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

#include "../client/ResourceManager.h"
#include "../client/SettingsManager.h"

#include "OMenu.h"
#include "ShellContextMenu.h"
#include "ResourceLoader.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "resource.h"

#ifdef IRAINMAN_INCLUDE_GDI_OLE
void setActiveMDIWindow(HWND hwnd);
#endif

enum
{
	FT_FIRST = WM_APP + 700,
	/** This will be sent when the user presses a tab. WPARAM = HWND */
	FTM_SELECTED,
	/** The number of rows changed */
	FTM_ROWS_CHANGED,
	/** Set currently active tab to the HWND pointed by WPARAM */
	FTM_SETACTIVE,
	/** Display context menu and return TRUE, or return FALSE for the default one */
	FTM_CONTEXTMENU,
	/** Get tab options from client */
	FTM_GETOPTIONS,
	/** Close window with postmessage... */
	WM_REALLY_CLOSE
};

struct FlatTabOptions
{
	HICON icons[2];
	bool isHub;
};

template <class T, class TBase = CWindow, class TWinTraits = CControlWinTraits>
class ATL_NO_VTABLE FlatTabCtrlImpl : public CWindowImpl<T, TBase, TWinTraits>
{
	public:
	enum
	{
		TEXT_LEN_MIN = 7,
		TEXT_LEN_MAX = 80,
		TEXT_LEN_FULL = 96,
		EDGE_HEIGHT = 4,
		HORIZ_EXTRA_SPACE = 6,
		VERT_EXTRA_SPACE = 10,
		CLOSE_BUTTON_SIZE = 16,
		CHEVRON_WIDTH = 14,
		ICON_SIZE = 16
	};

	FlatTabCtrlImpl() :
		closing(NULL), rows(1), height(0),
		active(nullptr), moving(nullptr), inTab(false),
		tabsPosition(SettingsManager::TABS_TOP), tabChars(16), maxRows(7),
		showIcons(true), showCloseButton(true), useBoldNotif(false), nonHubsFirst(true),
		closeButtonPressedTab(nullptr), closeButtonHover(false), hoverTab(nullptr)
	{
		black.CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
	}

	virtual ~FlatTabCtrlImpl() {}

	static LPCTSTR GetWndClassName()
	{
		return _T("FlatTabCtrl");
	}

	void addTab(HWND hWnd)
	{
		TabInfo *t = new TabInfo(hWnd, tabChars);
		dcassert(getTabInfo(hWnd) == NULL);
		FlatTabOptions opt;
		bool isHub = false;
		if (::SendMessage(hWnd, FTM_GETOPTIONS, 0, reinterpret_cast<LPARAM>(&opt)))
		{
			t->hIcons[0] = opt.icons[0];
			t->hIcons[1] = opt.icons[1];
			isHub = opt.isHub;
		}
		if (nonHubsFirst && !isHub)
			tabs.insert(tabs.begin(), t);
		else
			tabs.push_back(t);
		viewOrder.push_back(hWnd);
		nextTab = --viewOrder.end();
		active = t;
		calcRows(false);
		Invalidate();
	}

	void removeTab(HWND aWnd)
	{
		typename TabInfo::ListIter i;
		for (i = tabs.begin(); i != tabs.end(); ++i)
			if ((*i)->hWnd == aWnd) break;

		dcassert(i != tabs.end());

		if (i == tabs.end()) return;

		TabInfo *ti = *i;
		if (active == ti) active = nullptr;
		if (hoverTab == ti) hoverTab = nullptr;
		if (closeButtonPressedTab == ti) closeButtonPressedTab = nullptr;
		delete ti;
		tabs.erase(i);
		viewOrder.remove(aWnd);
		nextTab = viewOrder.end();
		if (!viewOrder.empty()) --nextTab;

		calcRows(false);
		Invalidate();
	}

	void startSwitch()
	{
		if (viewOrder.empty()) return;
		nextTab = --viewOrder.end();
		inTab = true;
	}

	void endSwitch()
	{
		inTab = false;
		if (active) setTop(active->hWnd);
	}

	HWND getNext()
	{
		if (viewOrder.empty()) return NULL;
		if (nextTab == viewOrder.begin())
			nextTab = --viewOrder.end();
		else
			--nextTab;
		return *nextTab;
	}

	HWND getPrev()
	{
		if (viewOrder.empty()) return NULL;
		nextTab++;
		if (nextTab == viewOrder.end())
			nextTab = viewOrder.begin();
		return *nextTab;
	}

	void setActive(HWND aWnd)
	{
#ifdef IRAINMAN_INCLUDE_GDI_OLE
		setActiveMDIWindow(aWnd);
#endif
		if (!inTab) setTop(aWnd);

		auto ti = getTabInfo(aWnd);
		if (!ti) return;

		active = ti;
		ti->dirty = false;
		calcRows(false);
		Invalidate();
	}

	bool getActive(HWND aWnd) const
	{
		auto ti = getTabInfo(aWnd);
		if (ti == NULL) return false;

		return active == ti;
	}

	bool isActive(HWND aWnd) const
	{
		return active && active->hWnd == aWnd;
	}

	void setTop(HWND aWnd)
	{
		viewOrder.remove(aWnd);
		viewOrder.push_back(aWnd);
		nextTab = --viewOrder.end();
	}

	void setDirty(HWND aWnd)
	{
		auto ti = getTabInfo(aWnd);
		dcassert(ti != NULL);
		if (ti == NULL) return;

		bool invalidate = ti->update();

		if (active != ti)
		{
			if (!ti->dirty)
			{
				ti->dirty = true;
				invalidate = true;
			}
		}

		if (invalidate)
		{
			calcRows(false);
			Invalidate();
		}
	}

	void setIcon(HWND aWnd, HICON icon, bool disconnected)
	{
		auto ti = getTabInfo(aWnd);
		if (ti)
		{
			int iconIndex = disconnected ? 1 : 0;
			if (ti->hIcons[iconIndex] != icon)
			{
				ti->hIcons[iconIndex] = icon;
				Invalidate();
			}
		}
	}

	void setDisconnected(HWND aWnd, bool disconnected)
	{
		auto ti = getTabInfo(aWnd);
		if (ti && ti->disconnected != disconnected)
		{
			ti->disconnected = disconnected;
			Invalidate();
		}
	}

	void setTooltipText(HWND aWnd, const tstring& text)
	{
		auto ti = getTabInfo(aWnd);
		if (ti)
			ti->tooltipText = text;
	}

	void updateText(HWND aWnd, LPCTSTR text)
	{
		TabInfo *ti = getTabInfo(aWnd);
		if (ti && ti->updateText(text))
		{
			calcRows(false);
			Invalidate();
		}
	}

	// FIXME: currently does nothing
	void setCountMessages(HWND aWnd, int messageCount)
	{
		/*
		if (TabInfo* ti = getTabInfo(aWnd))
		{
			ti->m_count_messages = messageCount;
			ti->m_dirty = false;
		}
		*/
	}

	int getTabsPosition() const { return tabsPosition; }
	int getContextMenuAlign() const
	{
		return getTabsPosition() == SettingsManager::TABS_TOP ? TPM_TOPALIGN : TPM_BOTTOMALIGN;
	}

	bool setOptions(int tabsPosition, int maxRows, unsigned tabChars,
	                bool showIcons, bool showCloseButton, bool useBoldNotif, bool nonHubsFirst,
					bool invalidate)
	{
		bool needInval = false;
		this->nonHubsFirst = nonHubsFirst;
		if (this->tabsPosition != tabsPosition)
		{
			this->tabsPosition = tabsPosition;
			hoverTab = closeButtonPressedTab = nullptr;
			closeButtonHover = false;
		}
		bool widthChanged = this->tabChars != tabChars;
		if (widthChanged || this->showIcons != showIcons || this->showCloseButton != showCloseButton ||
		    this->useBoldNotif != useBoldNotif || this->maxRows != maxRows)
		{
			this->showIcons = showIcons;
			this->showCloseButton = showCloseButton;
			this->useBoldNotif = useBoldNotif;
			this->maxRows = maxRows;
			this->tabChars = tabChars;
			needInval = true;
			if (m_hWnd)
			{
				if (widthChanged)
				{
					HDC hdc = ::GetDC(m_hWnd);
					for (auto t : tabs) t->setMaxLength(tabChars, hdc);
					::ReleaseDC(m_hWnd, hdc);
				}
				calcRows(false);
			}
		}
		if (m_hWnd && needInval && invalidate) Invalidate();
		return needInval;
	}

	BEGIN_MSG_MAP(thisClass)
	MESSAGE_HANDLER(WM_SIZE, onSize)
	MESSAGE_HANDLER(WM_CREATE, onCreate)
	MESSAGE_HANDLER(WM_PAINT, onPaint)
	MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
	MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
	MESSAGE_HANDLER(WM_LBUTTONUP, onLButtonUp)
	MESSAGE_HANDLER(WM_MBUTTONUP, onMButtonUp)
	MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
	MESSAGE_HANDLER(WM_MOUSELEAVE, onMouseLeave)
	MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
	COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
	COMMAND_ID_HANDLER(IDC_CHEVRON, onChevron)
	COMMAND_RANGE_HANDLER(IDC_SELECT_WINDOW, IDC_SELECT_WINDOW + tabs.size(), onSelectWindow)
	END_MSG_MAP()

	LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & /*bHandled*/)
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		int row = getRowFromPos(yPos);
		bool invalidate = false;

		for (auto t : tabs)
		{
			if (row == t->row && xPos >= t->xpos && xPos < t->xpos + getWidth(t))
			{				
				if (showCloseButton)
				{
					CRect rc;
					getCloseButtonRect(t, rc);
					if (xPos >= rc.left && xPos < rc.right && yPos >= rc.top && yPos < rc.bottom)
					{
						hoverTab = closeButtonPressedTab = t;
						invalidate = true;
						moving = nullptr;
						break;
					}
				}
				HWND hWnd = GetParent();
				if (hWnd)
				{
					if (wParam & MK_SHIFT || wParam & MK_XBUTTON1)
						::SendMessage(t->hWnd, WM_CLOSE, 0, 0);
					else
						moving = t;
				}
				break;
			}
		}
		if (invalidate) Invalidate();
		return 0;
	}

	LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
	{
		if (moving)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			int row = getRowFromPos(yPos);

#if 1 // Selected tab must stay selected: don't swtich to previous tab!
			bool moveLast = true;
#endif
			for (auto t : tabs)
			{
				int tabWidth = getWidth(t);
				if (row == t->row && xPos >= t->xpos && xPos < t->xpos + tabWidth)
				{
					auto hWnd = GetParent();
					if (hWnd)
					{
						if (t == moving)
						{
							if (t != active)
								::SendMessage(hWnd, FTM_SELECTED, (WPARAM)t->hWnd, 0);
						}
						else
						{
							// check if the pointer is on the left or right half of the tab
							// to determine where to insert the tab
							moveTabs(t, xPos > t->xpos + tabWidth / 2);
						}
					}
					moveLast = false;
					break;
				}
			}
			if (moveLast) moveTabs(tabs.back(), true);
			moving = nullptr;
		}
		else if (closeButtonPressedTab)
		{
			if (closeButtonHover)
			{
				HWND hWnd = GetParent();
				if (hWnd) ::SendMessage(closeButtonPressedTab->hWnd, WM_CLOSE, 0, 0);

			}
			closeButtonPressedTab = nullptr;
			Invalidate();
		}
		return 0;
	}

	LRESULT onMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		int row = getRowFromPos(yPos);

		for (auto t : tabs)
		{
			if (row == t->row && xPos >= t->xpos && xPos < t->xpos + getWidth(t))
			{
				auto hWnd = GetParent();
				if (hWnd)
				{
					::SendMessage(t->hWnd, WM_CLOSE, 0, 0);
				}
				break;
			}
		}
		return 0;
	}

	LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		int row = getRowFromPos(yPos);
		bool showTooltip = false;
		bool closeButtonVisible = false;
		bool invalidate = false;
		closeButtonHover = false;

		for (auto t : tabs)
		{
			if (row == t->row && xPos >= t->xpos && xPos < t->xpos + getWidth(t))
			{
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = m_hWnd;
				_TrackMouseEvent(&tme);
						
				if (showCloseButton)
				{
					if (hoverTab != t)
					{
						hoverTab = t;
						invalidate = true;
					}
					CRect rc;
					getCloseButtonRect(t, rc);
					closeButtonHover = xPos >= rc.left && xPos < rc.right && yPos >= rc.top && yPos < rc.bottom;
					closeButtonVisible = true;
				}

				const tstring& tabTooltip = t->tooltipText.empty() ? t->text : t->tooltipText;
				if (tabTooltip != tooltipText)
				{
					tooltip.DelTool(m_hWnd);
					tooltipText = tabTooltip;
					if (!tooltipText.empty())
					{
						if (BOOLSETTING(TABS_SHOW_INFOTIPS))
						{
							CToolInfo info(TTF_SUBCLASS, m_hWnd, 0, nullptr, const_cast<TCHAR*>(tooltipText.c_str()));
							tooltip.AddTool(&info);
							tooltip.Activate(TRUE);
							showTooltip = true;
						}
					} else tooltip.Activate(FALSE);
				} else showTooltip = true;
				break;
			}
		}
		if (!closeButtonVisible && hoverTab)
		{
			hoverTab = nullptr;
			invalidate = true;
		}
		if (!showTooltip && !tooltipText.empty())
		{
			tooltip.Activate(FALSE);
			tooltipText.clear();
		}
		if (invalidate) Invalidate();
		return TRUE;
	}

	LRESULT onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		closeButtonPressedTab = nullptr;
		if (hoverTab)
		{
			hoverTab = nullptr;
			Invalidate();
		}
		return TRUE;
	}
		
	LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // location of mouse click

		ScreenToClient(&pt);
		int xPos = pt.x;
		int row = getRowFromPos(pt.y);

		for (auto t : tabs)
		{
			if (row == t->row && xPos >= t->xpos && xPos < t->xpos + getWidth(t))
			{
				if (!::SendMessage(t->hWnd, FTM_CONTEXTMENU, 0, lParam))
				{
					closing = t->hWnd;
					ClientToScreen(&pt);
					CMenu contextMenu;
					contextMenu.CreatePopupMenu();
					contextMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE));
					contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | getContextMenuAlign(), pt.x, pt.y, m_hWnd);
				}
				break;
			}
		}
		return 0;
	}

	LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
	{
		if (::IsWindow(closing)) ::SendMessage(closing, WM_CLOSE, 0, 0);
		return 0;
	}

	void switchTo(bool next = true)
	{
		auto i = tabs.begin();
		for (; i != tabs.end(); ++i)
		{
			if ((*i)->hWnd == active->hWnd)
			{
				if (next)
				{
					++i;
					if (i == tabs.end()) i = tabs.begin();
				}
				else
				{
					if (i == tabs.begin()) i = tabs.end();
					--i;
				}
				setActive((*i)->hWnd);
				::SetWindowPos((*i)->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
				break;
			}
		}
	}

	int getTabHeight() const
	{
		return height;
	}

	int getHeight() const
	{
		return getRows() * getTabHeight() + 2*EDGE_HEIGHT;
	}

	int getFill() const
	{
		return (getTabHeight() + 1) / 2;
	}

	int getRows() const
	{
		return rows;
	}

	LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
	{
		chevron.Create(m_hWnd, rcDefault, NULL,
					   WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_CHEVRON);
		chevron.SetWindowText(_T("\u00bb"));

		tooltip.Create(m_hWnd, rcDefault, NULL, TTS_ALWAYSTIP | TTS_NOPREFIX);
		tooltip.SetMaxTipWidth(600);
	
		ResourceLoader::LoadImageList(IDR_CLOSE_PNG, closeButtonImages, CLOSE_BUTTON_SIZE, CLOSE_BUTTON_SIZE);
		menu.CreatePopupMenu();

		CDCHandle dc(::GetDC(m_hWnd));
		HFONT oldfont = dc.SelectFont(Fonts::g_systemFont);
		height = WinUtil::getTextHeight(dc) + VERT_EXTRA_SPACE;
		dc.SelectFont(oldfont);
		::ReleaseDC(m_hWnd, dc);

		return 0;
	}

	LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
	{
		calcRows();
		SIZE sz = { LOWORD(lParam), HIWORD(lParam) };
		chevron.MoveWindow(sz.cx - CHEVRON_WIDTH, EDGE_HEIGHT + 1, CHEVRON_WIDTH, getHeight() - 2*EDGE_HEIGHT - 2);
		return 0;
	}

	LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
	{
		return FALSE;
	}

	LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
	{
		CRect rc;
		CRect crc;
		GetClientRect(&crc);

		if (GetUpdateRect(&rc, FALSE))
		{
			CPaintDC dc(m_hWnd);
			CDC memDC;
			memDC.CreateCompatibleDC(dc);
			CBitmap memBitmap;
			memBitmap.CreateCompatibleBitmap(dc, rc.Width(), rc.Height());
			HBITMAP oldBitmap = memDC.SelectBitmap(memBitmap);
			memDC.SetWindowOrg(rc.left, rc.top);

			CRect rcEdge, rcBackground;
			rcEdge.left = rcBackground.left = 0;
			rcEdge.right = rcBackground.right = crc.right;
			if (tabsPosition == SettingsManager::TABS_TOP)
			{
				rcEdge.bottom = crc.bottom;
				rcEdge.top = rcEdge.bottom - EDGE_HEIGHT;
				rcBackground.bottom = rcEdge.top;
				rcBackground.top = 0;
			}
			else
			{
				rcEdge.top = 0;
				rcEdge.bottom = EDGE_HEIGHT;
				rcBackground.top = rcEdge.bottom;
				rcBackground.bottom = crc.bottom;
			}

			HBRUSH brush = CreateSolidBrush(colorSelected);
			memDC.FillRect(&rcEdge, brush);
			DeleteObject(brush);

			brush = CreateSolidBrush(colorNormal);
			memDC.FillRect(&rcBackground, brush);
			DeleteObject(brush);
	
			HFONT oldfont = memDC.SelectFont(Fonts::g_systemFont);

			int drawActiveOpt = 0;
			int lastRow = tabs.empty() ? -1 : tabs.back()->row;
			for (TabInfo::List::size_type i = 0; i < tabs.size(); i++)
			{				
				const TabInfo *t = tabs[i];
				if (t->row != -1 && t->xpos < rc.right && t->xpos + getWidth(t) >= rc.left)
				{
					int options = 0;
					if (i == 0 || t->row != tabs[i-1]->row)
						options |= DRAW_TAB_FIRST_IN_ROW;
					if (i == tabs.size() - 1 || t->row != tabs[i+1]->row)
						options |= DRAW_TAB_LAST_IN_ROW;
					if (t->row == lastRow)
						options |= DRAW_TAB_LAST_ROW;
					if (t == active)
						drawActiveOpt = options | DRAW_TAB_ACTIVE;
					else
						drawTab(memDC, t, options);
				}
			}

			HPEN oldpen = memDC.SelectPen(::CreatePen(PS_SOLID, 1, colorBorder));
			int ypos = EDGE_HEIGHT;
			if (tabsPosition == SettingsManager::TABS_TOP) ypos += getTabHeight()-1;
			for (int r = 0; r < rows; r++)
			{
				memDC.MoveTo(rc.left, ypos);
				memDC.LineTo(rc.right, ypos);
				ypos += getTabHeight();
			}
			::DeleteObject(memDC.SelectPen(oldpen));

			if (drawActiveOpt & DRAW_TAB_ACTIVE)
			{
				dcassert(active);
				drawTab(memDC, active, drawActiveOpt);
			}
			
			memDC.SelectFont(oldfont);
			dc.BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), memDC, rc.left, rc.top, SRCCOPY);			
			memDC.SelectBitmap(oldBitmap);	
		}
		return 0;
	}

	LRESULT onChevron(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
	{
		while (menu.GetMenuItemCount() > 0)
			menu.RemoveMenu(0, MF_BYPOSITION);

		int n = 0;
		CRect rc;
		GetClientRect(&rc);
		CMenuItemInfo mi;
		mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA | MIIM_STATE;
		mi.fType = MFT_STRING | MFT_RADIOCHECK;

		for (auto ti : tabs)
		{
			if (ti->row == -1)
			{
				mi.dwTypeData = (LPTSTR) ti->text.c_str();
				mi.dwItemData = (ULONG_PTR) ti->hWnd;
				mi.fState = MFS_ENABLED | (ti->dirty ? MFS_CHECKED : 0);
				mi.wID = IDC_SELECT_WINDOW + n;
				menu.InsertMenuItem(n++, TRUE, &mi);
			}
		}

		POINT pt;
		chevron.GetClientRect(&rc);
		pt.x = rc.right - rc.left;
		pt.y = 0;
		chevron.ClientToScreen(&pt);

		menu.TrackPopupMenu(TPM_RIGHTALIGN | TPM_RIGHTBUTTON | getContextMenuAlign(), pt.x, pt.y, m_hWnd);
		return 0;
	}

	LRESULT onSelectWindow(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
	{
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;

		menu.GetMenuItemInfo(wID, FALSE, &mi);
		auto hWnd = GetParent();
		if (hWnd)
			SendMessage(hWnd, FTM_SELECTED, (WPARAM)mi.dwItemData, 0);
		return 0;
	}

	void redraw()
	{
		height = WinUtil::getTextHeight(GetParent(), WinUtil::font) + VERT_EXTRA_SPACE;
		for (auto t : tabs)
		{
			t->update();
			SendMessage(t->hWnd, WM_PAINT, 0, 0);
		}
		Invalidate();
	}

	void processTooltipPop(HWND hwnd)
	{
		if (tooltip.m_hWnd == hwnd)
			tooltipText.clear();
	}
	
	private:
	class TabInfo
	{
		public:
		typedef vector<TabInfo *> List;
		typedef typename List::iterator ListIter;

		TabInfo(HWND aWnd, unsigned tabChars)
		: hWnd(aWnd), len(0), xpos(0), row(0), dirty(false), disconnected(false),
		  maxLength(std::min<unsigned>(TEXT_LEN_MAX, std::max<unsigned>(tabChars, TEXT_LEN_MIN)))
		{
			size.cx = size.cy = 0;
			boldSize.cx = boldSize.cy = 0;
			hIcons[0] = hIcons[1] = NULL;
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

		bool update()
		{
			TCHAR name[TEXT_LEN_FULL+1];
			::GetWindowText(hWnd, name, TEXT_LEN_FULL+1);
			bool ret = updateText(name);
			return ret;
		}

		void updateSize(HDC hdc)
		{
			CDCHandle dc(hdc);
			HFONT f = dc.SelectFont(Fonts::g_systemFont);			
			dc.GetTextExtent(textEllip.c_str(), textEllip.length(), &size);
			dc.SelectFont(Fonts::g_boldFont);
			dc.GetTextExtent(textEllip.c_str(), textEllip.length(), &boldSize);
			dc.SelectFont(f);
		}

		bool updateText(LPCTSTR str)
		{
			size_t len = _tcslen(str);
			if (!len) return false;
			tstring newText, newTextEllip;
			if (len > TEXT_LEN_FULL)
			{
				newText.assign(str, TEXT_LEN_FULL-3);
				newText.append(_T("..."));
			}
			else
				newText.assign(str, len);
			bool updated = false;
			if (text != newText)
			{
				updated = true;
				text = std::move(newText);
			}
			if (text.length() > maxLength)
			{
				newTextEllip = text.substr(0, maxLength-3);
				newTextEllip.append(_T("..."));
			}
			else
				newTextEllip = text;
			if (textEllip != newTextEllip)
			{
				updated = true;
				textEllip = std::move(newTextEllip);
			}
			if (!updated) return false;
			HDC hdc = ::GetDC(hWnd);
			updateSize(hdc);
			::ReleaseDC(hWnd, hdc);
			return true;
		}

		void setMaxLength(unsigned maxLength, HDC hdc)
		{
			maxLength = std::min<unsigned>(TEXT_LEN_MAX, std::max<unsigned>(maxLength, TEXT_LEN_MIN));
			if (this->maxLength == maxLength) return;
			this->maxLength = maxLength;
			tstring newTextEllip;
			if (text.length() > maxLength)
			{
				newTextEllip = text.substr(0, maxLength-3);
				newTextEllip.append(_T("..."));
			}
			else
				newTextEllip = text;
			if (textEllip == newTextEllip) return;
			textEllip = std::move(newTextEllip);
			updateSize(hdc);
		}
	};

	int getWidth(const TabInfo* t) const
	{
		return ((t->dirty && useBoldNotif) ? t->boldSize.cx : t->size.cx) +
			   HORIZ_EXTRA_SPACE + (showIcons ? 20 : 0) + (showCloseButton ? CLOSE_BUTTON_SIZE + 1 : 0);
	}

	int getRowFromPos(int yPos) const
	{
		if (yPos < EDGE_HEIGHT || yPos >= getHeight() - EDGE_HEIGHT) return -1;
		yPos -= EDGE_HEIGHT;
		int row = yPos / getTabHeight();
		if (tabsPosition != SettingsManager::TABS_TOP) row = getRows() - 1 - row;
		return row;
	}

	void calcRows(bool invalidate = true)
	{
		CRect rc;
		GetClientRect(rc);
		int r = 1;
		int w = 0;
		bool notify = false;
		bool needInval = false;

		for (auto t : tabs)
		{
			int tabWidth = getWidth(t);
			if (r != 0 && w + tabWidth > rc.Width() - CHEVRON_WIDTH)
			{
				if (r >= maxRows)
				{
					notify |= rows != r;
					rows = r;
					r = 0;
					chevron.EnableWindow(TRUE);
				}
				else
				{
					r++;
					w = 0;
				}
			}
			t->xpos = w;
			if (t->row != r - 1)
			{
				t->row = r - 1;
				needInval = true;
			}
			w += tabWidth;
		}

		if (r != 0)
		{
			chevron.EnableWindow(FALSE);
			notify |= rows != r;
			rows = r;
		}

		if (notify)
		{
			::SendMessage(GetParent(), FTM_ROWS_CHANGED, 0, 0);
		}
		if (needInval && invalidate) Invalidate();
	}

	void moveTabs(TabInfo *aTab, bool after)
	{
		if (moving == NULL) return;

		typename TabInfo::ListIter i, j;
		// remove the tab we're moving
		for (j = tabs.begin(); j != tabs.end(); ++j)
		{
			if ((*j) == moving)
			{
				tabs.erase(j);
				break;
			}
		}

		// find the tab we're going to insert before or after
		for (i = tabs.begin(); i != tabs.end(); ++i)
		{
			if ((*i) == aTab)
			{
				if (after) ++i;
				break;
			}
		}

		tabs.insert(i, moving);

		calcRows(false);
		Invalidate();
	}

	bool showIcons;
	bool showCloseButton;
	bool useBoldNotif;
	bool nonHubsFirst;
	int tabsPosition;
	unsigned tabChars;
	int maxRows;

	static const COLORREF colorSelected = RGB(255,255,255);
	static const COLORREF colorNormal = RGB(220,220,220);
	static const COLORREF colorBorder = RGB(157,157,161);
	//static const COLORREF colorDisconnected = RGB(255,153,153);
	static const COLORREF colorDisconnected = RGB(230,148,148);
	static const COLORREF colorDisconnectedLight = RGB(255,183,183);
	static const COLORREF colorNotif = RGB(153,189,255);
	static const COLORREF colorInactiveText = RGB(82,82,82);
	static const COLORREF colorActiveText = RGB(0,0,0);

	HWND closing;
	CButton chevron;
	CToolTipCtrl tooltip;
	CImageList closeButtonImages;
	CMenu menu;

	int rows;
	int height;

	typename TabInfo::List tabs;

	TabInfo *active;
	TabInfo *moving;
	const TabInfo *hoverTab;
	const TabInfo *closeButtonPressedTab;
	bool closeButtonHover;
	tstring tooltipText;
	CPen black;

	typedef list<HWND> WindowList;
	typedef WindowList::iterator WindowIter;

	WindowList viewOrder;
	WindowIter nextTab;

	bool inTab;

	TabInfo *getTabInfo(HWND aWnd) const
	{
		for (auto t : tabs)
			if (t->hWnd == aWnd) return t;
		return nullptr;
	}

	int getTopPos(const TabInfo *tab) const
	{
		if (tabsPosition == SettingsManager::TABS_TOP)
			return tab->row * getTabHeight() + EDGE_HEIGHT;
		return (getRows() - 1 - tab->row) * getTabHeight() + EDGE_HEIGHT;
	}

	void getCloseButtonRect(const TabInfo* t, CRect& rc) const
	{
		rc.left = t->xpos + getWidth(t) - (CLOSE_BUTTON_SIZE + 1);
		rc.top = getTopPos(t);
		rc.top += 2;
		rc.right = rc.left + CLOSE_BUTTON_SIZE;
		rc.bottom = rc.top + CLOSE_BUTTON_SIZE;
	}

	static COLORREF getLighterColor(COLORREF color)
	{
		return HLS_TRANSFORM(color, 30, 0);
	}

	static COLORREF getDarkerColor(COLORREF color)
	{
		return HLS_TRANSFORM(color, -45, 0);
	}

	enum
	{
		DRAW_TAB_ACTIVE       = 1,
		DRAW_TAB_FIRST_IN_ROW = 2,
		DRAW_TAB_LAST_IN_ROW  = 4,
		DRAW_TAB_LAST_ROW     = 8
	};

	void drawTab(CDC &dc, const TabInfo *tab, int options)
	{
		int ypos = getTopPos(tab);
		int pos = tab->xpos;

		HPEN borderPen = ::CreatePen(PS_SOLID, 1, colorBorder);
		HPEN oldPen = dc.SelectPen(borderPen);

		CRect rc(pos, ypos, pos + getWidth(tab), ypos + getTabHeight());

		COLORREF bgColor, textColor;
		if (options & DRAW_TAB_ACTIVE)
		{
			bgColor = tab->disconnected ? colorDisconnectedLight : colorSelected;
			textColor = colorActiveText;
		}
		else
		if (tab->disconnected)
		{
			bgColor = colorDisconnected;
			textColor = getDarkerColor(colorDisconnected);
		}
		else
		if (tab->dirty && !useBoldNotif)
		{
			bgColor = colorNotif;
			textColor = getDarkerColor(colorNotif);
		}
		else
		{
			bgColor = colorNormal;
			textColor = colorInactiveText;
		}
		
		HBRUSH brBackground = CreateSolidBrush(bgColor);
		dc.FillRect(rc, brBackground);
		DeleteObject(brBackground);

		if (options & DRAW_TAB_ACTIVE)
		{
			if ((options & (DRAW_TAB_FIRST_IN_ROW | DRAW_TAB_LAST_ROW)) == (DRAW_TAB_FIRST_IN_ROW | DRAW_TAB_LAST_ROW))
			{
				if (tabsPosition == SettingsManager::TABS_TOP)
				{
					dc.MoveTo(rc.left, rc.top);
					dc.LineTo(rc.left, getHeight());
				} else
				{
					dc.MoveTo(rc.left, 0);
					dc.LineTo(rc.left, rc.bottom);
				}
			}
			else
			{
				dc.MoveTo(rc.left, rc.top);
				dc.LineTo(rc.left, rc.bottom);
			}

			dc.MoveTo(rc.right, rc.top);
			dc.LineTo(rc.right, rc.bottom);

			if (tabsPosition == SettingsManager::TABS_TOP)
			{
				dc.MoveTo(rc.left, rc.top-1);
				dc.LineTo(rc.right+1, rc.top-1);
			}
			else
			{
				dc.MoveTo(rc.left, rc.bottom);
				dc.LineTo(rc.right+1, rc.bottom);
			}
		}
		else
		{
			if (!(options & DRAW_TAB_FIRST_IN_ROW))
			{
				dc.MoveTo(rc.left, rc.top + 3);
				dc.LineTo(rc.left, rc.bottom - 3);
			}
			dc.MoveTo(rc.right, rc.top + 3);
			dc.LineTo(rc.right, rc.bottom - 3);
		}

		dc.SetBkMode(TRANSPARENT);
		COLORREF oldTextColor = dc.SetTextColor(textColor);

		pos += 3;

		if (showIcons)
		{
			int iconIndex = tab->disconnected ? 1 : 0;
			if (tab->hIcons[iconIndex])
				dc.DrawIconEx(pos, ypos + (getTabHeight() - ICON_SIZE) / 2, tab->hIcons[iconIndex], ICON_SIZE, ICON_SIZE, 0, NULL, DI_NORMAL | DI_COMPAT);
			pos += 20;
		}

		HFONT oldFont = NULL;
		if (tab->dirty && useBoldNotif)
			oldFont = dc.SelectFont(Fonts::g_boldFont);

		dc.TextOut(pos, ypos + 3, tab->textEllip.c_str(), tab->textEllip.length());

		if (showIcons && hoverTab == tab)
		{
			CRect rcButton;
			getCloseButtonRect(tab, rcButton);
			int image = 0;
			if (closeButtonPressedTab == tab) image = 1;
			closeButtonImages.Draw(dc, image, rcButton.left, rcButton.top, ILD_NORMAL);
		}

		dc.SetTextColor(oldTextColor);
		dc.SelectPen(oldPen);
		if (oldFont) dc.SelectFont(oldFont);

		DeleteObject(borderPen);
	}
};

class FlatTabCtrl : public FlatTabCtrlImpl<FlatTabCtrl>
{
	public:
	DECLARE_FRAME_WND_CLASS_EX(GetWndClassName(), 0, 0, COLOR_3DFACE);
};

template <class T, class TBase = CMDIWindow, class TWinTraits = CMDIChildWinTraits>
class ATL_NO_VTABLE MDITabChildWindowImpl : public CMDIChildWindowImpl<T, TBase, TWinTraits>
{
	public:
	MDITabChildWindowImpl() : created(false), closed(false), closing(false)
	{
	}
	FlatTabCtrl *getTab()
	{
		return WinUtil::g_tabCtrl;
	}

	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}

	virtual void onBeforeActiveTab(HWND aWnd) {}
	virtual void onAfterActiveTab(HWND aWnd) {}
	virtual void onInvalidateAfterActiveTab(HWND aWnd) {}

	typedef MDITabChildWindowImpl<T, TBase, TWinTraits> thisClass;
	typedef CMDIChildWindowImpl<T, TBase, TWinTraits> baseClass;
	BEGIN_MSG_MAP(thisClass)
	MESSAGE_HANDLER(WM_CLOSE, onClose)
	MESSAGE_HANDLER(WM_SYSCOMMAND, onSysCommand)
	MESSAGE_HANDLER(WM_CREATE, onCreate)
	MESSAGE_HANDLER(WM_MDIACTIVATE, onMDIActivate)
	MESSAGE_HANDLER(WM_DESTROY, onDestroy)
	MESSAGE_HANDLER(WM_SETTEXT, onSetText)
	MESSAGE_HANDLER(WM_REALLY_CLOSE, onReallyClose)
	MESSAGE_HANDLER(WM_NOTIFYFORMAT, onNotifyFormat)
	MESSAGE_HANDLER_HWND(WM_MEASUREITEM, OMenu::onMeasureItem)
	MESSAGE_HANDLER_HWND(WM_DRAWITEM, OMenu::onDrawItem)
	MESSAGE_HANDLER_HWND(WM_INITMENUPOPUP, OMenu::onInitMenuPopup)
	CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

	HWND Create(HWND hWndParent,
				ATL::_U_RECT rect = NULL,
				LPCTSTR szWindowName = NULL,
				DWORD dwStyle = 0,
				DWORD dwExStyle = 0,
				UINT nMenuID = 0,
				LPVOID lpCreateParam = NULL)
	{
		ATOM atom = T::GetWndClassInfo().Register(&m_pfnSuperWindowProc);

		if (nMenuID != 0)
			m_hMenu = ::LoadMenu(ATL::_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(nMenuID));

		if (m_hMenu == NULL) m_hMenu = WinUtil::g_mainMenu;

		dwStyle = T::GetWndStyle(dwStyle);
		dwExStyle = T::GetWndExStyle(dwExStyle);

		dwExStyle |= WS_EX_MDICHILD; // force this one
		m_pfnSuperWindowProc = ::DefMDIChildProc;
		m_hWndMDIClient = hWndParent;
		ATLASSERT(::IsWindow(m_hWndMDIClient));

		if (rect.m_lpRect == NULL) rect.m_lpRect = &TBase::rcDefault;

		// If the currently active MDI child is maximized, we want to create this one maximized too
		ATL::CWindow wndParent = hWndParent;
		BOOL bMaximized = FALSE;

		if (MDIGetActive(&bMaximized) == NULL) bMaximized = SETTING(MDI_MAXIMIZED);

		if (bMaximized) wndParent.SetRedraw(FALSE);

		HWND hWnd = CFrameWindowImplBase<TBase, TWinTraits>::Create(hWndParent, rect.m_lpRect,
			szWindowName, dwStyle, dwExStyle, (UINT) 0, atom, lpCreateParam);

		if (bMaximized)
		{
			// Maximize and redraw everything
			if (hWnd != NULL) MDIMaximize(hWnd);
			wndParent.SetRedraw(TRUE);
			wndParent.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
			::SetFocus(GetMDIFrame()); // focus will be set back to this window
		}
		else if (hWnd != NULL && ::IsWindowVisible(m_hWnd) && !::IsChild(hWnd, ::GetFocus()))
		{
			::SetFocus(hWnd);
		}

		return hWnd;
	}

	LRESULT onNotifyFormat(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
	{
#ifdef _UNICODE
		return NFR_UNICODE;
#else
		return NFR_ANSI;
#endif
	}

	LRESULT onSysCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL &bHandled)
	{
		if (wParam == SC_CLOSE)
		{
			closing = true;
		}
		else if (wParam == SC_NEXTWINDOW)
		{
			HWND next = getTab()->getNext();
			if (next != NULL)
			{
				WinUtil::activateMDIChild(next);
				return 0;
			}
		}
		else if (wParam == SC_PREVWINDOW)
		{
			HWND next = getTab()->getPrev();
			if (next != NULL)
			{
				WinUtil::activateMDIChild(next);
				return 0;
			}
		}
		bHandled = FALSE;
		return 0;
	}

	LRESULT onCreate(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
	{
		bHandled = FALSE;
		dcassert(getTab());
		getTab()->addTab(m_hWnd);
		created = true;
		return 0;
	}

	LRESULT onMDIActivate(UINT /*uMsg*/, WPARAM /*wParam */, LPARAM lParam, BOOL &bHandled)
	{
		dcassert(getTab());
		if (m_hWnd == (HWND) lParam)
		{
			onBeforeActiveTab(m_hWnd);
			getTab()->setActive(m_hWnd);
			onAfterActiveTab(m_hWnd);
		}
		onInvalidateAfterActiveTab(m_hWnd);
		bHandled = FALSE;
		return 1;
	}

	LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
	{
		bHandled = FALSE;
		dcassert(getTab());
		getTab()->removeTab(m_hWnd);
		if (m_hMenu == WinUtil::g_mainMenu) m_hMenu = NULL;

#ifdef FLYLINKDC_USE_MDI_MAXIMIZED
		BOOL bMaximized = FALSE;
		if (::SendMessage(m_hWndMDIClient, WM_MDIGETACTIVE, 0, (LPARAM)&bMaximized) != NULL)
			SettingsManager::getInstance()->set(SettingsManager::MDI_MAXIMIZED, (bMaximized > 0));
#endif
		return 0;
	}

	LRESULT onReallyClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
	{
		MDIDestroy(m_hWnd);
		return 0;
	}

	LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled */)
	{
		closing = true;
		PostMessage(WM_REALLY_CLOSE);
		return 0;
	}

	LRESULT onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL &bHandled)
	{
		bHandled = FALSE;
		dcassert(getTab());
		if (created)
			getTab()->updateText(m_hWnd, reinterpret_cast<LPCTSTR>(lParam));
		return 0;
	}

	LRESULT onKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
	{
		if (wParam == VK_CONTROL && LOWORD(lParam) == 1)
		{
			getTab()->startSwitch();
		}
		bHandled = FALSE;
		return 0;
	}

	LRESULT onKeyUp(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL &bHandled)
	{
		if (wParam == VK_CONTROL)
		{
			getTab()->endSwitch();
		}
		bHandled = FALSE;
		return 0;
	}

	void setDirty()
	{
		dcassert(getTab());
		getTab()->setDirty(m_hWnd);
	}

	void setCountMessages(int messageCount)
	{
		dcassert(getTab());
		getTab()->setCountMessages(m_hWnd, messageCount);
	}

	bool getActive()
	{
		dcassert(getTab());
		return getTab()->getActive(m_hWnd);
	}

	void setDisconnected(bool dis = false)
	{
		dcassert(getTab());
		getTab()->setDisconnected(m_hWnd, dis);
	}

	void setIcon(HICON icon, bool disconnected = false)
	{
		dcassert(getTab());
		getTab()->setIcon(m_hWnd, icon, disconnected);
	}

	void setTooltipText(const tstring& text)
	{
		dcassert(getTab());
		getTab()->setTooltipText(m_hWnd, text);
	}

	private:
	bool created;

	protected:
	bool closed;
	bool closing;
	bool isClosedOrShutdown() const
	{
		return closing || closed || ClientManager::isBeforeShutdown();
	}
};

#endif // !defined(FLAT_TAB_CTRL_H)
