#include "stdafx.h"
#include "FlatTabCtrl.h"
#include "ColorUtil.h"
#include "Fonts.h"
#include "BackingStore.h"

FlatTabCtrl::FlatTabCtrl() :
	closing(nullptr),
	active(nullptr), moving(nullptr), inTab(false),
	tabsPosition(Conf::TABS_TOP), tabChars(16), maxRows(7),
	showIcons(true), showCloseButton(true), useBoldNotif(false), nonHubsFirst(true),
	closeButtonPressedTab(nullptr), closeButtonHover(false), hoverTab(nullptr),
	rows(1), height(0),
	textHeight(0), edgeHeight(0), horizIconSpace(0), horizPadding(0), chevronWidth(0),
	backingStore(nullptr)
{
}

void FlatTabCtrl::cleanup()
{
	if (backingStore)
	{
		backingStore->release();
		backingStore = nullptr;
	}
}

COLORREF FlatTabCtrl::getLighterColor(COLORREF color)
{
	return HLS_TRANSFORM(color, 30, 0);
}

COLORREF FlatTabCtrl::getDarkerColor(COLORREF color)
{
	return HLS_TRANSFORM(color, -45, 0);
}

void FlatTabCtrl::addTab(HWND hWnd)
{
	TabInfo *t = new TabInfo(hWnd, tabChars);
	dcassert(getTabInfo(hWnd) == nullptr);
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
	if (tabs.size() == 1 && !IsWindowVisible())
		::PostMessage(WinUtil::g_mainWnd, WMU_UPDATE_LAYOUT, 0, 0);
	else
		Invalidate();
}

void FlatTabCtrl::removeTab(HWND hWnd)
{
	TabInfo::ListIter i;
	for (i = tabs.begin(); i != tabs.end(); ++i)
		if ((*i)->hWnd == hWnd) break;

	dcassert(i != tabs.end());

	if (i == tabs.end()) return;

	TabInfo *ti = *i;
	if (active == ti) active = nullptr;
	if (hoverTab == ti) hoverTab = nullptr;
	if (closeButtonPressedTab == ti) closeButtonPressedTab = nullptr;
	delete ti;
	tabs.erase(i);
	viewOrder.remove(hWnd);
	nextTab = viewOrder.end();
	if (!viewOrder.empty()) --nextTab;

	calcRows(false);
	if (tabs.empty())
		::PostMessage(WinUtil::g_mainWnd, WMU_UPDATE_LAYOUT, 0, 0);
	else
		Invalidate();
}

void FlatTabCtrl::startSwitch()
{
	if (viewOrder.empty()) return;
	nextTab = --viewOrder.end();
	inTab = true;
}

void FlatTabCtrl::endSwitch()
{
	inTab = false;
	if (active) setTop(active->hWnd);
}

HWND FlatTabCtrl::getNext()
{
	if (viewOrder.empty()) return nullptr;
	if (nextTab == viewOrder.begin())
		nextTab = --viewOrder.end();
	else
		--nextTab;
	return *nextTab;
}

HWND FlatTabCtrl::getPrev()
{
	if (viewOrder.empty()) return nullptr;
	nextTab++;
	if (nextTab == viewOrder.end())
		nextTab = viewOrder.begin();
	return *nextTab;
}

void FlatTabCtrl::setActive(HWND hWnd)
{
#ifdef IRAINMAN_INCLUDE_GDI_OLE
	setActiveMDIWindow(hWnd);
#endif
	if (!inTab) setTop(hWnd);

	auto ti = getTabInfo(hWnd);
	if (!ti) return;

	if (active != ti)
	{
		active = ti;
		ti->dirty = false;
		calcRows(false);
		Invalidate();
	}
}

bool FlatTabCtrl::getActive(HWND hWnd) const
{
	auto ti = getTabInfo(hWnd);
	if (!ti) return false;
	return active == ti;
}

bool FlatTabCtrl::isActive(HWND hWnd) const
{
	return active && active->hWnd == hWnd;
}

void FlatTabCtrl::setTop(HWND hWnd)
{
	viewOrder.remove(hWnd);
	viewOrder.push_back(hWnd);
	nextTab = --viewOrder.end();
}

void FlatTabCtrl::setDirty(HWND hWnd)
{
	auto ti = getTabInfo(hWnd);
	dcassert(ti != nullptr);
	if (!ti) return;

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

void FlatTabCtrl::setIcon(HWND hWnd, HICON icon, bool disconnected)
{
	auto ti = getTabInfo(hWnd);
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

void FlatTabCtrl::setDisconnected(HWND hWnd, bool disconnected)
{
	auto ti = getTabInfo(hWnd);
	if (ti && ti->disconnected != disconnected)
	{
		ti->disconnected = disconnected;
		Invalidate();
	}
}

void FlatTabCtrl::setTooltipText(HWND hWnd, const tstring& text)
{
	auto ti = getTabInfo(hWnd);
	if (ti)
		ti->tooltipText = text;
}

void FlatTabCtrl::updateText(HWND hWnd, LPCTSTR text)
{
	TabInfo *ti = getTabInfo(hWnd);
	if (ti && ti->updateText(text))
	{
		calcRows(false);
		Invalidate();
	}
}

// FIXME: currently does nothing
void FlatTabCtrl::setCountMessages(HWND hWnd, int messageCount)
{
	/*
	if (TabInfo* ti = getTabInfo(hWnd))
	{
		ti->m_count_messages = messageCount;
		ti->m_dirty = false;
	}
	*/
}

bool FlatTabCtrl::updateSettings(bool invalidate)
{
	static const int colorOptions[] =
	{
		{ Conf::TABS_INACTIVE_BACKGROUND_COLOR       },
		{ Conf::TABS_ACTIVE_BACKGROUND_COLOR         },
		{ Conf::TABS_INACTIVE_TEXT_COLOR             },
		{ Conf::TABS_ACTIVE_TEXT_COLOR               },
		{ Conf::TABS_OFFLINE_BACKGROUND_COLOR        },
		{ Conf::TABS_OFFLINE_ACTIVE_BACKGROUND_COLOR },
		{ Conf::TABS_UPDATED_BACKGROUND_COLOR        },
		{ Conf::TABS_BORDER_COLOR                    }
	};

	bool needInval = false;
	const auto* ss = SettingsManager::instance.getUiSettings();
	for (int i = 0; i < MAX_COLORS; i++)
	{
		COLORREF color = ss->getInt(colorOptions[i]);
		if (colors[i] != color)
		{
			needInval = true;
			colors[i] = color;
		}
	}

	int tabsPosition = ss->getInt(Conf::TABS_POS);
	int maxRows = ss->getInt(Conf::MAX_TAB_ROWS);
	unsigned tabChars = ss->getInt(Conf::TAB_SIZE);
	bool showCloseButton = ss->getBool(Conf::TABS_CLOSEBUTTONS);
	bool useBoldNotif = ss->getBool(Conf::TABS_BOLD);
	this->nonHubsFirst = ss->getBool(Conf::NON_HUBS_FRONT);
	if (this->tabsPosition != tabsPosition)
	{
		this->tabsPosition = tabsPosition;
		hoverTab = closeButtonPressedTab = nullptr;
		closeButtonHover = false;
	}
	bool widthChanged = this->tabChars != tabChars;
	if (widthChanged || this->showCloseButton != showCloseButton ||
	    this->useBoldNotif != useBoldNotif || this->maxRows != maxRows)
	{
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

LRESULT FlatTabCtrl::onLButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & /*bHandled*/)
{
	int xPos = GET_X_LPARAM(lParam);
	int yPos = GET_Y_LPARAM(lParam);
	int row = getRowFromPos(yPos);
	bool invalidate = false;

	for (auto t : tabs)
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
	if (invalidate) Invalidate();
	return 0;
}

LRESULT FlatTabCtrl::onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
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

LRESULT FlatTabCtrl::onMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
{
	int xPos = GET_X_LPARAM(lParam);
	int yPos = GET_Y_LPARAM(lParam);
	int row = getRowFromPos(yPos);

	for (auto t : tabs)
		if (row == t->row && xPos >= t->xpos && xPos < t->xpos + getWidth(t))
		{
			auto hWnd = GetParent();
			if (hWnd)
				::SendMessage(t->hWnd, WM_CLOSE, 0, 0);
			break;
		}
	return 0;
}

LRESULT FlatTabCtrl::onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
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
					const auto* ss = SettingsManager::instance.getUiSettings();
					if (ss->getBool(Conf::TABS_SHOW_INFOTIPS))
					{
						CToolInfo info(TTF_SUBCLASS, m_hWnd, 0, nullptr, const_cast<TCHAR*>(tooltipText.c_str()));
						tooltip.AddTool(&info);
						tooltip.Activate(TRUE);
						showTooltip = true;
					}
				}
				else
					tooltip.Activate(FALSE);
			}
			else
				showTooltip = true;
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

LRESULT FlatTabCtrl::onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	closeButtonPressedTab = nullptr;
	if (hoverTab)
	{
		hoverTab = nullptr;
		Invalidate();
	}
	return TRUE;
}

LRESULT FlatTabCtrl::onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
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

LRESULT FlatTabCtrl::onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
{
	if (::IsWindow(closing)) ::SendMessage(closing, WM_CLOSE, 0, 0);
	return 0;
}

void FlatTabCtrl::switchTo(bool next)
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

LRESULT FlatTabCtrl::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
{
	chevron.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_CHEVRON);
	chevron.SetWindowTextW(L"\u00bb");

	tooltip.Create(m_hWnd, rcDefault, nullptr, TTS_ALWAYSTIP | TTS_NOPREFIX);
	tooltip.SetMaxTipWidth(600);

	ResourceLoader::LoadImageList(IDR_CLOSE_PNG, closeButtonImages, CLOSE_BUTTON_SIZE, CLOSE_BUTTON_SIZE);
	menu.CreatePopupMenu();

	CDCHandle dc(::GetDC(m_hWnd));
	HFONT oldfont = dc.SelectFont(Fonts::g_systemFont);
	int xdu, ydu;
	WinUtil::getDialogUnits(dc, xdu, ydu);
	int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	edgeHeight = 2 * dpi / 96;
	horizPadding = 5 * dpi / 96;
	int vertExtraSpace = 10 * dpi / 96;
	horizIconSpace = 2 * dpi / 96;
	chevronWidth = WinUtil::dialogUnitsToPixelsX(8, xdu);
	textHeight = WinUtil::getTextHeight(dc);
	height = std::max<int>(textHeight, ICON_SIZE) + vertExtraSpace;
	dc.SelectFont(oldfont);
	::ReleaseDC(m_hWnd, dc);

	return 0;
}

LRESULT FlatTabCtrl::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
{
	cleanup();
	return 0;
}

LRESULT FlatTabCtrl::onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL & /*bHandled*/)
{
	calcRows();
	SIZE sz = { LOWORD(lParam), HIWORD(lParam) };
	chevron.MoveWindow(sz.cx - chevronWidth, edgeHeight + 1, chevronWidth, getHeight() - 2*edgeHeight - 2);
	return 0;
}

LRESULT FlatTabCtrl::onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
{
	return FALSE;
}

LRESULT FlatTabCtrl::onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
{
	CRect rc;
	CRect crc;
	GetClientRect(&crc);

	if (GetUpdateRect(&rc, FALSE))
	{
		PAINTSTRUCT ps;
		HDC paintDC = BeginPaint(&ps);
		HDC hdc = paintDC;
		if (!backingStore) backingStore = BackingStore::getBackingStore();
		if (backingStore)
		{
			HDC hMemDC = backingStore->getCompatibleDC(hdc, rc.Width(), rc.Height());
			if (hMemDC) hdc = hMemDC;
		}

		POINT org;
		SetWindowOrgEx(hdc, rc.left, rc.top, &org);

		CRect rcEdge, rcBackground;
		rcEdge.left = rcBackground.left = 0;
		rcEdge.right = rcBackground.right = crc.right;
		if (tabsPosition == Conf::TABS_TOP)
		{
			rcEdge.bottom = crc.bottom;
			rcEdge.top = rcEdge.bottom - edgeHeight;
			rcBackground.bottom = rcEdge.top;
			rcBackground.top = 0;
		}
		else
		{
			rcEdge.top = 0;
			rcEdge.bottom = edgeHeight;
			rcBackground.top = rcEdge.bottom;
			rcBackground.bottom = crc.bottom;
		}

		HBRUSH brush = CreateSolidBrush(colors[ACTIVE_BACKGROUND_COLOR]);
		FillRect(hdc, &rcEdge, brush);
		DeleteObject(brush);

		brush = CreateSolidBrush(colors[INACTIVE_BACKGROUND_COLOR]);
		FillRect(hdc, &rcBackground, brush);
		DeleteObject(brush);

		HGDIOBJ oldfont = SelectObject(hdc, Fonts::g_systemFont);

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
					drawTab(hdc, t, options);
			}
		}

		HGDIOBJ oldpen = SelectObject(hdc, ::CreatePen(PS_SOLID, 1, colors[BORDER_COLOR]));
		int ypos = edgeHeight;
		if (tabsPosition == Conf::TABS_TOP) ypos += getTabHeight()-1;
		for (int r = 0; r < rows; r++)
		{
			MoveToEx(hdc, rc.left, ypos, nullptr);
			LineTo(hdc, rc.right, ypos);
			ypos += getTabHeight();
		}
		DeleteObject(SelectObject(hdc, oldpen));

		if (drawActiveOpt & DRAW_TAB_ACTIVE)
		{
			dcassert(active);
			drawTab(hdc, active, drawActiveOpt);
		}

		SelectObject(hdc, oldfont);
		if (hdc != paintDC)
			BitBlt(paintDC, rc.left, rc.top, rc.Width(), rc.Height(), hdc, rc.left, rc.top, SRCCOPY);
		SetWindowOrgEx(hdc, org.x, org.y, nullptr);
		EndPaint(&ps);
	}
	return 0;
}

LRESULT FlatTabCtrl::onChevron(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
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

LRESULT FlatTabCtrl::onSelectWindow(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
{
	CMenuItemInfo mi;
	mi.fMask = MIIM_DATA;

	menu.GetMenuItemInfo(wID, FALSE, &mi);
	auto hWnd = GetParent();
	if (hWnd)
		SendMessage(hWnd, FTM_SELECTED, (WPARAM)mi.dwItemData, 0);
	return 0;
}

void FlatTabCtrl::processTooltipPop(HWND hWnd)
{
	if (tooltip.m_hWnd == hWnd)
		tooltipText.clear();
}

int FlatTabCtrl::getWidth(const TabInfo* t) const
{
	return ((t->dirty && useBoldNotif) ? t->boldSize.cx : t->size.cx) +
		   2 * horizPadding + (showIcons ? ICON_SIZE + horizIconSpace : 0) + (showCloseButton ? CLOSE_BUTTON_SIZE + 1 : 0);
}

int FlatTabCtrl::getRowFromPos(int yPos) const
{
	if (yPos < edgeHeight || yPos >= getHeight() - edgeHeight) return -1;
	yPos -= edgeHeight;
	int row = yPos / getTabHeight();
	if (tabsPosition != Conf::TABS_TOP) row = getRows() - 1 - row;
	return row;
}

void FlatTabCtrl::calcRows(bool invalidate)
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
		if (r != 0 && w + tabWidth > rc.Width() - chevronWidth)
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
		::SendMessage(GetParent(), FTM_ROWS_CHANGED, 0, 0);
	if (needInval && invalidate) Invalidate();
}

void FlatTabCtrl::moveTabs(TabInfo *aTab, bool after)
{
	if (moving == nullptr) return;

	TabInfo::ListIter i, j;
	// remove the tab we're moving
	for (j = tabs.begin(); j != tabs.end(); ++j)
		if ((*j) == moving)
		{
			tabs.erase(j);
			break;
		}

	// find the tab we're going to insert before or after
	for (i = tabs.begin(); i != tabs.end(); ++i)
		if ((*i) == aTab)
		{
			if (after) ++i;
			break;
		}

	tabs.insert(i, moving);

	calcRows(false);
	Invalidate();
}

FlatTabCtrl::TabInfo* FlatTabCtrl::getTabInfo(HWND hWnd) const
{
	for (auto t : tabs)
		if (t->hWnd == hWnd) return t;
	return nullptr;
}

int FlatTabCtrl::getTopPos(const TabInfo *tab) const
{
	if (tabsPosition == Conf::TABS_TOP)
		return tab->row * getTabHeight() + edgeHeight;
	return (getRows() - 1 - tab->row) * getTabHeight() + edgeHeight;
}

void FlatTabCtrl::getCloseButtonRect(const TabInfo* t, CRect& rc) const
{
	rc.left = t->xpos + getWidth(t) - (CLOSE_BUTTON_SIZE + horizPadding);
	rc.top = getTopPos(t);
	rc.top += (height - CLOSE_BUTTON_SIZE) / 2;
	rc.right = rc.left + CLOSE_BUTTON_SIZE;
	rc.bottom = rc.top + CLOSE_BUTTON_SIZE;
}

void FlatTabCtrl::drawTab(HDC hdc, const TabInfo *tab, int options)
{
	int ypos = getTopPos(tab);
	int pos = tab->xpos;

	HPEN borderPen = ::CreatePen(PS_SOLID, 1, colors[BORDER_COLOR]);
	HGDIOBJ oldPen = SelectObject(hdc, borderPen);

	CRect rc(pos, ypos, pos + getWidth(tab), ypos + getTabHeight());

	COLORREF bgColor, textColor;
	if (options & DRAW_TAB_ACTIVE)
	{
		bgColor = colors[tab->disconnected ? OFFLINE_ACTIVE_BACKGROUND_COLOR : ACTIVE_BACKGROUND_COLOR];
		textColor = colors[ACTIVE_TEXT_COLOR];
	}
	else
	if (tab->disconnected)
	{
		bgColor = colors[OFFLINE_BACKGROUND_COLOR];
		textColor = getDarkerColor(bgColor);
	}
	else
	if (tab->dirty && !useBoldNotif)
	{
		bgColor = colors[UPDATED_BACKGROUND_COLOR];
		textColor = getDarkerColor(bgColor);
	}
	else
	{
		bgColor = colors[INACTIVE_BACKGROUND_COLOR];
		textColor = colors[INACTIVE_TEXT_COLOR];
	}

	HBRUSH brBackground = CreateSolidBrush(bgColor);
	FillRect(hdc, rc, brBackground);
	DeleteObject(brBackground);

	if (options & DRAW_TAB_ACTIVE)
	{
		if ((options & (DRAW_TAB_FIRST_IN_ROW | DRAW_TAB_LAST_ROW)) == (DRAW_TAB_FIRST_IN_ROW | DRAW_TAB_LAST_ROW))
		{
			if (tabsPosition == Conf::TABS_TOP)
			{
				MoveToEx(hdc, rc.left, rc.top, nullptr);
				LineTo(hdc, rc.left, getHeight());
			}
			else
			{
				MoveToEx(hdc, rc.left, 0, nullptr);
				LineTo(hdc, rc.left, rc.bottom);
			}
		}
		else
		{
			MoveToEx(hdc, rc.left, rc.top, nullptr);
			LineTo(hdc, rc.left, rc.bottom);
		}

		MoveToEx(hdc, rc.right, rc.top, nullptr);
		LineTo(hdc, rc.right, rc.bottom);

		if (tabsPosition == Conf::TABS_TOP)
		{
			MoveToEx(hdc, rc.left, rc.top-1, nullptr);
			LineTo(hdc, rc.right+1, rc.top-1);
		}
		else
		{
			MoveToEx(hdc, rc.left, rc.bottom, nullptr);
			LineTo(hdc, rc.right+1, rc.bottom);
		}
	}
	else
	{
		if (!(options & DRAW_TAB_FIRST_IN_ROW))
		{
			MoveToEx(hdc, rc.left, rc.top + 3, nullptr);
			LineTo(hdc, rc.left, rc.bottom - 3);
		}
		MoveToEx(hdc, rc.right, rc.top + 3, nullptr);
		LineTo(hdc, rc.right, rc.bottom - 3);
	}

	SetBkMode(hdc, TRANSPARENT);
	COLORREF oldTextColor = SetTextColor(hdc, textColor);

	pos += horizPadding;

	if (showIcons)
	{
		int iconIndex = tab->disconnected ? 1 : 0;
		if (tab->hIcons[iconIndex])
		DrawIconEx(hdc, pos, ypos + (height - ICON_SIZE) / 2, tab->hIcons[iconIndex], ICON_SIZE, ICON_SIZE, 0, nullptr, DI_NORMAL | DI_COMPAT);
		pos += ICON_SIZE + horizIconSpace;
	}

	HGDIOBJ oldFont = nullptr;
	if (tab->dirty && useBoldNotif)
		oldFont = SelectObject(hdc, Fonts::g_boldFont);

	TextOut(hdc, pos, ypos + (height - textHeight) / 2, tab->textEllip.c_str(), tab->textEllip.length());

	if (hoverTab == tab)
	{
		CRect rcButton;
		getCloseButtonRect(tab, rcButton);
		int image = closeButtonPressedTab == tab ? 1 : 0;
		closeButtonImages.Draw(hdc, image, rcButton.left, rcButton.top, ILD_NORMAL);
	}

	SetTextColor(hdc, oldTextColor);
	SelectObject(hdc, oldPen);
	if (oldFont) SelectObject(hdc, oldFont);

	DeleteObject(borderPen);
}

bool FlatTabCtrl::TabInfo::update()
{
	TCHAR name[TEXT_LEN_FULL+1];
	::GetWindowText(hWnd, name, TEXT_LEN_FULL+1);
	bool ret = updateText(name);
	return ret;
}

void FlatTabCtrl::TabInfo::updateSize(HDC hdc)
{
	CDCHandle dc(hdc);
	HFONT f = dc.SelectFont(Fonts::g_systemFont);
	dc.GetTextExtent(textEllip.c_str(), textEllip.length(), &size);
	dc.SelectFont(Fonts::g_boldFont);
	dc.GetTextExtent(textEllip.c_str(), textEllip.length(), &boldSize);
	dc.SelectFont(f);
}

bool FlatTabCtrl::TabInfo::updateText(LPCTSTR str)
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

void FlatTabCtrl::TabInfo::setMaxLength(unsigned maxLength, HDC hdc)
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
