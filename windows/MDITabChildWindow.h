#ifndef MDI_TAB_CHILD_WINDOW_H_
#define MDI_TAB_CHILD_WINDOW_H_

#include <atlframe.h>
#include "MDIUtil.h"
#include "FlatTabCtrl.h"
#include "OMenu.h"
#include "UserMessages.h"

template <class T, class TBase = CMDIWindow, class TWinTraits = CMDIChildWinTraits>
class ATL_NO_VTABLE MDITabChildWindowImpl : public CMDIChildWindowImpl<T, TBase, TWinTraits>
{
	public:
	MDITabChildWindowImpl() : created(false), closed(false), closing(false)
	{
	}

	FlatTabCtrl *getTab()
	{
		return WinUtil::tabCtrl;
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
	MESSAGE_HANDLER(WMU_REALLY_CLOSE, onReallyClose)
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

		//if (this->m_hMenu == NULL) this->m_hMenu = MenuHelper::mainMenu;

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

		if (this->MDIGetActive(&bMaximized) == NULL) bMaximized = WinUtil::useMDIMaximized();

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
		//if (this->m_hMenu == MenuHelper::mainMenu) this->m_hMenu = NULL;
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
		this->PostMessage(WMU_REALLY_CLOSE);
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
		::SendMessage(WinUtil::mdiClient, WM_MDIREFRESHMENU, 0, 0);
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

#endif // MDI_TAB_CHILD_WINDOW_H_
