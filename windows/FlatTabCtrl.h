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
#include "MenuHelper.h"
#include "UserMessages.h"
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
	virtual ~FlatTabCtrl() { cleanup(); }

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
	bool getActive(HWND hWnd) const;
	bool isActive(HWND hWnd) const;
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
		return getTabsPosition() == SettingsManager::TABS_TOP ? TPM_TOPALIGN : TPM_BOTTOMALIGN;
	}
	int getTabCount() const { return tabs.size(); }

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
	COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
	COMMAND_ID_HANDLER(IDC_CHEVRON, onChevron)
	COMMAND_RANGE_HANDLER(IDC_SELECT_WINDOW, IDC_SELECT_WINDOW + tabs.size(), onSelectWindow)
	END_MSG_MAP()

	LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/);

	void switchTo(bool next = true);

	int getTabHeight() const { return height; }
	int getHeight() const { return getRows() * getTabHeight() + 2*edgeHeight; }
	int getFill() const { return (getTabHeight() + 1) / 2; }
	int getRows() const { return rows; }

	LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
	LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
	LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
	LRESULT onChevron(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
	LRESULT onSelectWindow(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/);

	void processTooltipPop(HWND hWnd);

	private:
	class TabInfo
	{
		public:
		typedef vector<TabInfo *> List;
		typedef List::iterator ListIter;

		TabInfo(HWND hWnd, unsigned tabChars)
		: hWnd(hWnd), len(0), xpos(0), row(0), dirty(false), disconnected(false),
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

		bool update();
		void updateSize(HDC hdc);
		bool updateText(LPCTSTR str);
		void setMaxLength(unsigned maxLength, HDC hdc);
	};

	int getWidth(const TabInfo* t) const;
	int getRowFromPos(int yPos) const;
	void calcRows(bool invalidate = true);
	void moveTabs(TabInfo* t, bool after);

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
		MAX_COLORS
	};

	COLORREF colors[MAX_COLORS];

	HWND closing;
	CButton chevron;
	CToolTipCtrl tooltip;
	CImageList closeButtonImages;
	CMenu menu;
	class BackingStore* backingStore;

	int rows;
	int height;
	int xdu, ydu;
	int textHeight;
	int edgeHeight;
	int horizIconSpace;
	int horizPadding;
	int chevronWidth;

	TabInfo::List tabs;

	TabInfo *active;
	TabInfo *moving;
	const TabInfo *hoverTab;
	const TabInfo *closeButtonPressedTab;
	bool closeButtonHover;
	tstring tooltipText;

	list<HWND> viewOrder;
	list<HWND>::iterator nextTab;

	bool inTab;

	TabInfo *getTabInfo(HWND hWnd) const;
	int getTopPos(const TabInfo *tab) const;
	void getCloseButtonRect(const TabInfo* t, CRect& rc) const;

	static COLORREF getLighterColor(COLORREF color);
	static COLORREF getDarkerColor(COLORREF color);

	enum
	{
		DRAW_TAB_ACTIVE       = 1,
		DRAW_TAB_FIRST_IN_ROW = 2,
		DRAW_TAB_LAST_IN_ROW  = 4,
		DRAW_TAB_LAST_ROW     = 8
	};

	void drawTab(HDC hdc, const TabInfo *tab, int options);
	void cleanup();
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

	virtual void onDeactivate() {}
	virtual void onActivate() {}

	typedef MDITabChildWindowImpl<T, TBase, TWinTraits> thisClass;
	typedef CMDIChildWindowImpl<T, TBase, TWinTraits> baseClass;

	BEGIN_MSG_MAP(thisClass)
	MESSAGE_HANDLER(WM_CLOSE, onClose)
	MESSAGE_HANDLER(WM_SYSCOMMAND, onSysCommand)
	MESSAGE_HANDLER(WM_CREATE, onCreate)
	MESSAGE_HANDLER(WM_MDIACTIVATE, onMDIActivate)
	MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, onWindowPosChanging)
	MESSAGE_HANDLER(WM_DESTROY, onDestroy)
	MESSAGE_HANDLER(WM_SETTEXT, onSetText)
	MESSAGE_HANDLER(WM_REALLY_CLOSE, onReallyClose)
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
		ATOM atom = T::GetWndClassInfo().Register(&this->m_pfnSuperWindowProc);

		if (nMenuID != 0)
			this->m_hMenu = ::LoadMenu(ATL::_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(nMenuID));

		if (this->m_hMenu == NULL) this->m_hMenu = MenuHelper::mainMenu;

		dwStyle = T::GetWndStyle(dwStyle);
		dwExStyle = T::GetWndExStyle(dwExStyle);

		dwExStyle |= WS_EX_MDICHILD; // force this one
		this->m_pfnSuperWindowProc = ::DefMDIChildProc;
		this->m_hWndMDIClient = hWndParent;
		ATLASSERT(::IsWindow(this->m_hWndMDIClient));

		if (rect.m_lpRect == NULL) rect.m_lpRect = &TBase::rcDefault;

		// If the currently active MDI child is maximized, we want to create this one maximized too
		ATL::CWindow wndParent = hWndParent;
		BOOL bMaximized = FALSE;

		if (this->MDIGetActive(&bMaximized) == NULL) bMaximized = SETTING(MDI_MAXIMIZED);

		if (bMaximized) wndParent.SetRedraw(FALSE);

		HWND hWnd = CFrameWindowImplBase<TBase, TWinTraits>::Create(hWndParent, rect.m_lpRect,
			szWindowName, dwStyle, dwExStyle, (UINT) 0, atom, lpCreateParam);

		if (bMaximized)
		{
			// Maximize and redraw everything
			if (hWnd) this->MDIMaximize(hWnd);
			wndParent.SetRedraw(TRUE);
			wndParent.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
			::SetFocus(this->GetMDIFrame()); // focus will be set back to this window
		}
		else if (hWnd && ::IsWindowVisible(this->m_hWnd) && !::IsChild(hWnd, ::GetFocus()))
		{
			::SetFocus(hWnd);
		}

		return hWnd;
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

	LRESULT onCreate(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM lParam, BOOL &bHandled)
	{
		bHandled = FALSE;
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		if (cs->lpszName) currentTitle = cs->lpszName;
		dcassert(getTab());
		getTab()->addTab(this->m_hWnd);
		created = true;
		return 0;
	}

	LRESULT onMDIActivate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
	{
		dcassert(getTab());
		//dcdebug("onMDIActivate: wParam = 0x%X, lParam = 0x%X\n", wParam, lParam);
		if (this->m_hWnd == (HWND) wParam)
			onDeactivate();
		if (this->m_hWnd == (HWND) lParam)
		{
			getTab()->setActive(this->m_hWnd);
			onActivate();
		}
		bHandled = FALSE;
		return 1;
	}

	LRESULT onWindowPosChanging(UINT /*uMsg*/, WPARAM /*wParam */, LPARAM lParam, BOOL &bHandled)
	{
		const WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
		//dcdebug("onWindowPosChanging: hWnd = 0x%X, zoomed = %d, flags = 0x%X\n", m_hWnd, IsZoomed(), wp->flags);
		if (!(wp->flags & SWP_HIDEWINDOW))
		{
			BOOL maximized = this->IsZoomed();
			BOOL visible = getTab()->IsWindowVisible();
			if (visible ^ maximized)
			{
				//dcdebug("onWindowPosChanging: WMU_UPDATE_LAYOUT\n");
				::PostMessage(WinUtil::g_mainWnd, WMU_UPDATE_LAYOUT, 0, 0);
			}
		}
		bHandled = FALSE;
		return 0;
	}

	LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
	{
		bHandled = FALSE;
		dcassert(getTab());
		getTab()->removeTab(this->m_hWnd);
		if (this->m_hMenu == MenuHelper::mainMenu) this->m_hMenu = NULL;
		return 0;
	}

	LRESULT onReallyClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
	{
		this->MDIDestroy(this->m_hWnd);
		return 0;
	}

	LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled */)
	{
		closing = true;
		this->PostMessage(WM_REALLY_CLOSE);
		return 0;
	}

	LRESULT onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL &bHandled)
	{
		bHandled = FALSE;
		dcassert(getTab());
		if (created)
			getTab()->updateText(this->m_hWnd, reinterpret_cast<LPCTSTR>(lParam));
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
		getTab()->setDirty(this->m_hWnd);
	}

	void setCountMessages(int messageCount)
	{
		dcassert(getTab());
		getTab()->setCountMessages(this->m_hWnd, messageCount);
	}

	bool getActive()
	{
		dcassert(getTab());
		return getTab()->getActive(this->m_hWnd);
	}

	void setDisconnected(bool dis = false)
	{
		dcassert(getTab());
		getTab()->setDisconnected(this->m_hWnd, dis);
	}

	void setIcon(HICON icon, bool disconnected = false)
	{
		dcassert(getTab());
		getTab()->setIcon(this->m_hWnd, icon, disconnected);
	}

	void setTooltipText(const tstring& text)
	{
		dcassert(getTab());
		getTab()->setTooltipText(this->m_hWnd, text);
	}

	void setWindowTitle(const tstring& text)
	{
		if (currentTitle == text) return;
		currentTitle = text;
		this->SetWindowText(currentTitle.c_str());
		::SendMessage(WinUtil::g_mdiClient, WM_MDIREFRESHMENU, 0, 0);
	}

	private:
	bool created;
	tstring currentTitle;

	protected:
	bool closed;
	bool closing;
	bool isClosedOrShutdown() const
	{
		return closing || closed || ClientManager::isBeforeShutdown();
	}
};

#endif // !defined(FLAT_TAB_CTRL_H)
