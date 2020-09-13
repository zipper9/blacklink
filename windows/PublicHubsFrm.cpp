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
#include "PublicHubsFrm.h"
#include "PublicHubsListDlg.h"
#include "HubFrame.h"
#include "WinUtil.h"
#include "CountryList.h"

HIconWrapper PublicHubsFrame::frameIcon(IDR_INTERNET_HUBS);

int PublicHubsFrame::columnIndexes[] =
{
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_USERS,
	COLUMN_SERVER,
	COLUMN_COUNTRY,
	COLUMN_SHARED,
	COLUMN_MINSHARE,
	COLUMN_MINSLOTS,
	COLUMN_MAXHUBS,
	COLUMN_MAXUSERS,
	COLUMN_RELIABILITY,
	COLUMN_RATING,
	COLUMN_LAST
};

int PublicHubsFrame::columnSizes[] = { 200, 290, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100 };

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::HUB_NAME,
	ResourceManager::DESCRIPTION,
	ResourceManager::USERS,
	ResourceManager::HUB_ADDRESS,
	ResourceManager::COUNTRY,
	ResourceManager::SHARED,
	ResourceManager::MIN_SHARE,
	ResourceManager::MIN_SLOTS,
	ResourceManager::MAX_HUBS,
	ResourceManager::MAX_USERS,
	ResourceManager::RELIABILITY,
	ResourceManager::RATING
};

static inline int getColumnSortType(int column)
{
	switch (column)
	{
		case PublicHubsFrame::COLUMN_USERS:
		case PublicHubsFrame::COLUMN_MINSLOTS:
		case PublicHubsFrame::COLUMN_MAXHUBS:
		case PublicHubsFrame::COLUMN_MAXUSERS:
			return ExListViewCtrl::SORT_INT;
		case PublicHubsFrame::COLUMN_RELIABILITY:
			return ExListViewCtrl::SORT_FLOAT;
		case PublicHubsFrame::COLUMN_SHARED:
		case PublicHubsFrame::COLUMN_MINSHARE:
			return ExListViewCtrl::SORT_BYTES;
		default:
			return ExListViewCtrl::SORT_STRING_NOCASE;
	}
}

LRESULT PublicHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	int w[3] = { 0, 0, 0 };
	ctrlStatus.SetParts(3, w);
	
	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
	                WS_EX_CLIENTEDGE, IDC_HUBLIST);
	ctrlHubs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlHubs);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(PUBLIC_HUBS_FRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(PUBLIC_HUBS_FRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_USERS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlHubs.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlHubs.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	
	setListViewColors(ctrlHubs);
	
	ctrlHubs.SetImageList(g_flagImage.getIconList(), LVSIL_SMALL);
	
	/*  extern HIconWrapper g_hOfflineIco;
	  extern HIconWrapper g_hOnlineIco;
	    m_onlineStatusImg.Create(16, 16, ILC_COLOR32 | ILC_MASK,  0, 2);
	    m_onlineStatusImg.AddIcon(g_hOnlineIco);
	    m_onlineStatusImg.AddIcon(g_hOfflineIco);
	  ctrlHubs.SetImageList(m_onlineStatusImg, LVSIL_SMALL);
	*/
	ClientManager::getOnlineClients(onlineHubs);
	
	ctrlHubs.SetFocus();
	
	ctrlConfigure.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_PUB_LIST_CONFIG);
	ctrlConfigure.SetWindowText(CTSTRING(CONFIGURE));
	ctrlConfigure.SetFont(Fonts::g_systemFont);

	ctrlRefresh.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_REFRESH);
	ctrlRefresh.SetWindowText(CTSTRING(REFRESH));
	ctrlRefresh.SetFont(Fonts::g_systemFont);

	ctrlLists.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_GROUPBOX, WS_EX_TRANSPARENT);
	ctrlLists.SetFont(Fonts::g_systemFont);
	ctrlLists.SetWindowText(CTSTRING(CONFIGURED_HUB_LISTS));

	ctrlPubLists.Create(m_hWnd, rcDefault, NULL,
	                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
	                    WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE, IDC_PUB_LIST_DROPDOWN);
	ctrlPubLists.SetFont(Fonts::g_systemFont, FALSE);

	ctrlFilter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
	filterContainer.SubclassWindow(ctrlFilter.m_hWnd);
	ctrlFilter.SetFont(Fonts::g_systemFont);

	ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlFilterSel.SetFont(Fonts::g_systemFont, FALSE);

	// populate the filter list with the column names
	for (int j = 0; j < COLUMN_LAST; j++)
		ctrlFilterSel.AddString(CTSTRING_I(columnNames[j]));

	ctrlFilterSel.AddString(CTSTRING(ANY));
	ctrlFilterSel.SetCurSel(COLUMN_LAST);

	ctrlFilterDesc.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_GROUPBOX, WS_EX_TRANSPARENT);
	ctrlFilterDesc.SetWindowText(CTSTRING(FILTER));
	ctrlFilterDesc.SetFont(Fonts::g_systemFont);
	
	HublistManager *hublistManager = HublistManager::getInstance();
	
	hublistManager->addListener(this);
	SettingsManager::getInstance()->addListener(this);

	hublistManager->setServerList(SETTING(HUBLIST_SERVERS));
	hublistManager->getHubLists(hubLists);
	updateDropDown();

	const int sortColumn = SETTING(PUBLIC_HUBS_FRAME_SORT);
	ctrlHubs.setSortFromSettings(sortColumn, getColumnSortType(abs(sortColumn)-1), COLUMN_LAST);

	hubsMenu.CreatePopupMenu();
	hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT));
	hubsMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(ADD_TO_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_COPY_HUB, CTSTRING(COPY_HUB));
	hubsMenu.SetMenuDefaultItem(IDC_CONNECT);
	
	onListSelChanged();

	bHandled = FALSE;
	return TRUE;
}

LRESULT PublicHubsFrame::onCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HWND hWnd = (HWND)lParam;
	HDC hDC = (HDC)wParam;
	if (uMsg == WM_CTLCOLORLISTBOX || hWnd == ctrlPubLists.m_hWnd || hWnd == ctrlFilter.m_hWnd || hWnd == ctrlFilterSel.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT PublicHubsFrame::onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLISTVIEW *l = (NMLISTVIEW *)pnmh;
	if (l->iSubItem == ctrlHubs.getSortColumn())
	{
		if (!ctrlHubs.isAscending())
			ctrlHubs.setSort(-1, ctrlHubs.getSortType());
		else
			ctrlHubs.setSortDirection(false);
	}
	else
	{
		// BAH, sorting on bytes will break of course...oh well...later...
		ctrlHubs.setSort(l->iSubItem, getColumnSortType(l->iSubItem));
	}
	return 0;
}

LRESULT PublicHubsFrame::onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (!checkNick()) return 0;
		
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem != -1)
		openHub(item->iItem);
		
	return 0;
}

LRESULT PublicHubsFrame::onEnter(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	if (!checkNick()) return 0;
		
	int item = ctrlHubs.GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
		openHub(item);
		
	return 0;
}

LRESULT PublicHubsFrame::onClickedRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
{
	int index = ctrlPubLists.GetCurSel();
	if (index < 0) return 0;
	dcassert(index < (int) hubLists.size());	
	HublistManager::getInstance()->refresh(hubLists[index].id);
	return 0;
}

LRESULT PublicHubsFrame::onClickedConfigure(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
{
	PublicHubListDlg dlg(hubLists);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		HublistManager *hublistManager = HublistManager::getInstance();
		hublistManager->setServerList(SETTING(HUBLIST_SERVERS));
		hublistManager->getHubLists(hubLists);
		updateDropDown();
	}
	return 0;
}

LRESULT PublicHubsFrame::onClickedConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!checkNick()) return 0;
		
	if (ctrlHubs.GetSelectedCount() >= 100) // maximum hubs per one connection
	{
		if (MessageBox(CTSTRING(PUBLIC_HUBS_WARNING), _T(" "), MB_ICONWARNING | MB_YESNO) == IDNO)
			return 0;
	}
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
		openHub(i);
		
	return 0;
}

LRESULT PublicHubsFrame::onFilterFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	bHandled = true;
	ctrlFilter.SetFocus();
	return 0;
}

LRESULT PublicHubsFrame::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!checkNick()) return 0;
		
	auto fm = FavoriteManager::getInstance();
	bool save = false;
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		FavoriteHubEntry e;
		e.setName(ctrlHubs.ExGetItemText(i, COLUMN_NAME));
		e.setDescription(ctrlHubs.ExGetItemText(i, COLUMN_DESCRIPTION));
		e.setServer(Util::formatDchubUrl(ctrlHubs.ExGetItemText(i, COLUMN_SERVER)));
		//  if (!client->getPassword().empty()) // ToDo: Use SETTINGS Nick and Password
		//  {
		//      e.setNick(client->getMyNick());
		//      e.setPassword(client->getPassword());
		//  }
		if (fm->addFavoriteHub(e, false)) save = true;
	}
	if (save) fm->saveFavorites();
	return 0;
}

LRESULT PublicHubsFrame::onRemoveFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto fm = FavoriteManager::getInstance();
	bool save = false;
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		if (fm->removeFavoriteHub(getPubServer(i), false)) save = true;
	}
	if (save) fm->saveFavorites();
	return 0;
}

LRESULT PublicHubsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		closed = true;
		HublistManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		WinUtil::setButtonPressed(ID_FILE_CONNECT, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlHubs, SettingsManager::PUBLIC_HUBS_FRAME_ORDER,
		                         SettingsManager::PUBLIC_HUBS_FRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		SET_SETTING(PUBLIC_HUBS_FRAME_SORT, ctrlHubs.getSortForSettings());
		bHandled = FALSE;
		return 0;
	}
}

void PublicHubsFrame::onListSelChanged()
{
	int index = ctrlPubLists.GetCurSel();
	if (index < 0) return;
	dcassert(index < (int) hubLists.size());	
	HublistManager::HubListInfo &hl = hubLists[index];
	HublistManager::getInstance()->refreshAndGetHubList(hl, hl.id);
	showStatus(hl);
	updateList(hl.list);
}

LRESULT PublicHubsFrame::onListSelChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled)
{
	onListSelChanged();
	return 0;
}

void PublicHubsFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);

	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[3];
		ctrlStatus.GetClientRect(sr);
		int tmp = (sr.Width()) > 316 ? 216 : ((sr.Width() > 116) ? sr.Width() - 100 : 16);

		w[0] = sr.right - tmp;
		w[1] = w[0] + (tmp - 16) / 2;
		w[2] = w[0] + (tmp);

		ctrlStatus.SetParts(3, w);
	}

	int const comboH = 140;

	// listview
	CRect rc = rect;
	rc.top += 2;
	rc.bottom -= 56;
	ctrlHubs.MoveWindow(rc);

	// filter box
	rc = rect;
	rc.top = rc.bottom - 52;
	rc.bottom = rc.top + 46;
	rc.right -= 100;
	rc.right -= ((rc.right - rc.left) / 2) + 1;
	ctrlFilterDesc.MoveWindow(rc);

	// filter edit
	rc.top += 16;
	rc.bottom -= 8;
	rc.left += 8;
	rc.right -= ((rc.right - rc.left - 4) / 3);
	ctrlFilter.MoveWindow(rc);

	// filter sel
	rc.bottom += comboH;
	rc.right += ((rc.right - rc.left - 12) / 2);
	rc.left += ((rc.right - rc.left + 8) / 3) * 2;
	ctrlFilterSel.MoveWindow(rc);

	// lists box
	rc = rect;
	rc.top = rc.bottom - 52;
	rc.bottom = rc.top + 46;
	rc.right -= 100;
	rc.left += ((rc.right - rc.left) / 2) + 1;
	ctrlLists.MoveWindow(rc);

	// lists dropdown
	rc.top += 16;
	rc.bottom -= 8 - comboH;
	rc.right -= 8 + 100;
	rc.left += 8;
	ctrlPubLists.MoveWindow(rc);

	// configure button
	rc.left = rc.right + 4;
	rc.bottom -= comboH;
	rc.right += 100;
	ctrlConfigure.MoveWindow(rc);

	// refresh button
	rc = rect;
	rc.bottom -= 2 + 8 + 4;
	rc.top = rc.bottom - 22;
	rc.left = rc.right - 96;
	rc.right -= 2;
	ctrlRefresh.MoveWindow(rc);
}

void PublicHubsFrame::openHub(int ind)
{
	RecentHubEntry r;
	r.setName(ctrlHubs.ExGetItemText(ind, COLUMN_NAME));
	r.setDescription(ctrlHubs.ExGetItemText(ind, COLUMN_DESCRIPTION));
	r.setUsers(ctrlHubs.ExGetItemText(ind, COLUMN_USERS));
	r.setShared(ctrlHubs.ExGetItemText(ind, COLUMN_SHARED));
	const string server = Util::formatDchubUrl(ctrlHubs.ExGetItemText(ind, COLUMN_SERVER));
	r.setServer(server);
	FavoriteManager::getInstance()->addRecent(r);
	HubFrame::openHubWindow(server);
}

bool PublicHubsFrame::checkNick()
{
	if (SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		return false;
	}
	return true;
}

void PublicHubsFrame::updateList(const HubEntry::List &hubs)
{
	ctrlHubs.DeleteAllItems();
	users = 0;
	visibleHubs = 0;

	ctrlHubs.SetRedraw(FALSE);

	double size = -1;
	FilterModes mode = NONE;
	
	int sel = ctrlFilterSel.GetCurSel();
	
	bool doSizeCompare = parseFilter(mode, size);
	
	auto cnt = ctrlHubs.GetItemCount();
	for (auto j = hubs.cbegin(); j != hubs.cend(); ++j)
	{
		const auto &i = *j;
		if (matchFilter(i, sel, doSizeCompare, mode, size))
		{
			TStringList l;
			l.resize(COLUMN_LAST);
			l[COLUMN_NAME] = Text::toT(i.getName());
			l[COLUMN_DESCRIPTION] = Text::toT(i.getDescription());
			l[COLUMN_USERS] = Util::toStringT(i.getUsers());
			l[COLUMN_SERVER] = Text::toT(i.getServer());
			l[COLUMN_COUNTRY] = Text::toT(i.getCountry());
			l[COLUMN_SHARED] = Util::formatBytesT(i.getShared());
			l[COLUMN_MINSHARE] = Util::formatBytesT(i.getMinShare());
			l[COLUMN_MINSLOTS] = Util::toStringT(i.getMinSlots());
			l[COLUMN_MAXHUBS] = Util::toStringT(i.getMaxHubs());
			l[COLUMN_MAXUSERS] = Util::toStringT(i.getMaxUsers());
			l[COLUMN_RELIABILITY] = Util::toStringT(i.getReliability());
			l[COLUMN_RATING] = Text::toT(i.getRating());
			
			const string& country = i.getCountry();
			int index = getCountryByName(country);
			ctrlHubs.insert(cnt++, l, index + 1);
			visibleHubs++;
			users += i.getUsers();
		}
	}
	
	ctrlHubs.SetRedraw(TRUE);
	ctrlHubs.resort();
	
	updateStatus();
}

void PublicHubsFrame::showStatus(const HublistManager::HubListInfo &info)
{
	tstring status;
	const string &url = info.lastRedirUrl.empty()? info.url : info.lastRedirUrl;
	switch (info.state)
	{
		case HublistManager::STATE_DOWNLOADING:
			status = TSTRING(DOWNLOADING_HUB_LIST) + _T(" (") + Text::toT(url) + _T(")");
			break;
		case HublistManager::STATE_DOWNLOADED:
			status = TSTRING(HUB_LIST_DOWNLOADED) + _T(" (") + Text::toT(url) + _T(")");
			break;
		case HublistManager::STATE_DOWNLOAD_FAILED:
			status = TSTRING(DOWNLOAD_FAILED) + _T(" ") + Text::toT(info.error);
			break;
		case HublistManager::STATE_FROM_CACHE:
		{
			status = TSTRING(HUB_LIST_LOADED_FROM_CACHE) + _T(" Download Date: ") +
				Text::toT(Util::formatDateTime("%x", info.lastModified));
			break;
		}
		case HublistManager::STATE_PARSE_FAILED:
			status = TSTRING(HUBLIST_DOWNLOAD_CORRUPTED) + _T(" (") + Text::toT(url) + _T(")");
			/* HUBLIST_CACHE_CORRUPTED */
	}
	ctrlStatus.SetText(0, status.c_str());
}

LRESULT PublicHubsFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam != WPARAM_PROCESS_REDIRECT && wParam != WPARAM_UPDATE_STATE) return 0;
	uint64_t *val = reinterpret_cast<uint64_t*>(lParam);
	int index = -1;
	for (size_t i = 0; i < hubLists.size(); i++)
		if (hubLists[i].id == *val)
		{
			index = i;
			break;
		}
	if (wParam == WPARAM_PROCESS_REDIRECT)
	{
		HublistManager::getInstance()->processRedirect(*val);
	}
	else
	if (index >= 0)
	{
		HublistManager::getInstance()->getHubList(hubLists[index], *val);
		showStatus(hubLists[index]);
		if (hubLists[index].state == HublistManager::STATE_DOWNLOADED ||
		    hubLists[index].state == HublistManager::STATE_FROM_CACHE)
				updateList(hubLists[index].list);
	}
	delete val;
	return 0;
}

void PublicHubsFrame::updateStatus()
{
	ctrlStatus.SetText(1, (TSTRING(HUBS) + _T(": ") + Util::toStringT(visibleHubs)).c_str());
	ctrlStatus.SetText(2, (TSTRING(USERS) + _T(": ") + Util::toStringT(users)).c_str());
}

LRESULT PublicHubsFrame::onFilterChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	tstring tmp;
	WinUtil::getWindowText(ctrlFilter, tmp);
	Text::makeLower(tmp);
	filter = Text::fromT(tmp);
	if (wParam == VK_RETURN)
	{
		int index = ctrlPubLists.GetCurSel();
		if (index < 0) return 0;
		updateList(hubLists[index].list);
	}
	else
	{
		bHandled = FALSE;
	}
	return 0;
}

LRESULT PublicHubsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlHubs /*&& ctrlHubs.GetSelectedCount() == 1*/)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		CRect rc;
		ctrlHubs.GetHeader().GetWindowRect(&rc);
		if (PtInRect(&rc, pt))
		{
			return 0;
		}
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlHubs, pt);
		}
		int status = ctrlHubs.GetSelectedCount() > 0 ? MFS_ENABLED : MFS_DISABLED;
		hubsMenu.EnableMenuItem(IDC_CONNECT, status);
		hubsMenu.EnableMenuItem(IDC_ADD, status);
		hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, status);
		if (ctrlHubs.GetSelectedCount() > 1)
		{
			hubsMenu.EnableMenuItem(IDC_COPY_HUB, MFS_DISABLED);
		}
		else
		{
			int i = -1;
			while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				if (FavoriteManager::getInstance()->isFavoriteHub(getPubServer(i)))
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_DISABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_ENABLED);
				}
				else
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_ENABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_DISABLED);
				}
			}
			hubsMenu.EnableMenuItem(IDC_COPY_HUB, status);
		}
		hubsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT PublicHubsFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = frameIcon;
	opt->isHub = false;
	return TRUE;
}

LRESULT PublicHubsFrame::onCopyHub(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetSelectedCount() == 1)
	{
		TCHAR buf[256];
		int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		ctrlHubs.GetItemText(i, COLUMN_SERVER, buf, 256);
		WinUtil::setClipboard(buf);
	}
	return 0;
}

void PublicHubsFrame::updateDropDown()
{
	ctrlPubLists.ResetContent();
	int selIndex = -1;
	for (size_t i = 0; i < hubLists.size(); i++)
	{		
		ctrlPubLists.AddString(Text::toT(hubLists[i].url).c_str());
		if (selectedHubList == hubLists[i].id) selIndex = i;
	}
	if (selIndex == -1 && !hubLists.empty())
	{
		selectedHubList = hubLists[0].id;
		selIndex = 0;
	}
	ctrlPubLists.SetCurSel(selIndex);
}

bool PublicHubsFrame::parseFilter(FilterModes &mode, double &size)
{
	string::size_type start = string::npos;
	string::size_type end = string::npos;
	int64_t multiplier = 1;

	if (filter.empty()) return false;
	if (filter.compare(0, 2, ">=") == 0)
	{
		mode = GREATER_EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, "<=") == 0)
	{
		mode = LESS_EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, "==") == 0)
	{
		mode = EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, "!=") == 0)
	{
		mode = NOT_EQUAL;
		start = 2;
	}
	else if (filter[0] == '<')
	{
		mode = LESS;
		start = 1;
	}
	else if (filter[0] == '>')
	{
		mode = GREATER;
		start = 1;
	}
	else if (filter[0] == '=')
	{
		mode = EQUAL;
		start = 1;
	}

	if (start == string::npos) return false;
	if (filter.length() <= start) return false;

	if ((end = filter.find("tib")) != tstring::npos)
		multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
	else if ((end = filter.find("gib")) != tstring::npos)
		multiplier = 1024 * 1024 * 1024;
	else if ((end = filter.find("mib")) != tstring::npos)
		multiplier = 1024 * 1024;
	else if ((end = filter.find("kib")) != tstring::npos)
		multiplier = 1024;
	else if ((end = filter.find("tb")) != tstring::npos)
		multiplier = 1000LL * 1000LL * 1000LL * 1000LL;
	else if ((end = filter.find("gb")) != tstring::npos)
		multiplier = 1000 * 1000 * 1000;
	else if ((end = filter.find("mb")) != tstring::npos)
		multiplier = 1000 * 1000;
	else if ((end = filter.find("kb")) != tstring::npos)
		multiplier = 1000;

	if (end == string::npos)
	{
		end = filter.length();
	}

	string tmpSize = filter.substr(start, end - start);
	size = Util::toDouble(tmpSize) * multiplier;

	return true;
}

bool PublicHubsFrame::matchFilter(const HubEntry &entry, int sel, bool doSizeCompare, const FilterModes &mode, const double &size)
{
	if (filter.empty()) return true;

	double entrySize = 0;
	string entryString;

	switch (sel)
	{
		case COLUMN_NAME:
			entryString = entry.getName();
			doSizeCompare = false;
			break;
		case COLUMN_DESCRIPTION:
			entryString = entry.getDescription();
			doSizeCompare = false;
			break;
		case COLUMN_USERS:
			entrySize = entry.getUsers();
			break;
		case COLUMN_SERVER:
			entryString = entry.getServer();
			doSizeCompare = false;
			break;
		case COLUMN_COUNTRY:
			entryString = entry.getCountry();
			doSizeCompare = false;
			break;
		case COLUMN_SHARED:
			entrySize = (double)entry.getShared();
			break;
		case COLUMN_MINSHARE:
			entrySize = (double)entry.getMinShare();
			break;
		case COLUMN_MINSLOTS:
			entrySize = entry.getMinSlots();
			break;
		case COLUMN_MAXHUBS:
			entrySize = entry.getMaxHubs();
			break;
		case COLUMN_MAXUSERS:
			entrySize = entry.getMaxUsers();
			break;
		case COLUMN_RELIABILITY:
			entrySize = entry.getReliability();
			break;
		case COLUMN_RATING:
			entryString = entry.getRating();
			doSizeCompare = false;
			break;
		default:
			break;
	}

	bool insert = false;
	if (doSizeCompare)
	{
		switch (mode)
		{
			case EQUAL:
				insert = (size == entrySize);
				break;
			case GREATER_EQUAL:
				insert = (size <= entrySize);
				break;
			case LESS_EQUAL:
				insert = (size >= entrySize);
				break;
			case GREATER:
				insert = (size < entrySize);
				break;
			case LESS:
				insert = (size > entrySize);
				break;
			case NOT_EQUAL:
				insert = (size != entrySize);
				break;
		}
	}
	else
	{
		string tmp;
		if (sel >= COLUMN_LAST)
		{
			if (Text::toLower(entry.getName(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getDescription(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getServer(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getCountry(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getRating(), tmp).find(filter) != string::npos)
			{
				insert = true;
			}
		}
		if (!insert && Text::toLower(entryString, tmp).find(filter) != string::npos)
			insert = true;
	}

	return insert;
}

void PublicHubsFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlHubs.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

void PublicHubsFrame::on(HublistManagerListener::StateChanged, uint64_t id) noexcept
{
	uint64_t *ptr = new uint64_t(id);
	PostMessage(WM_SPEAKER, WPARAM_UPDATE_STATE, reinterpret_cast<LPARAM>(ptr));
}

void PublicHubsFrame::on(HublistManagerListener::Redirected, uint64_t id) noexcept
{
	uint64_t *ptr = new uint64_t(id);
	PostMessage(WM_SPEAKER, WPARAM_PROCESS_REDIRECT, reinterpret_cast<LPARAM>(ptr));
}

LRESULT PublicHubsFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled */)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			if (isOnline(getPubServer((int)cd->nmcd.dwItemSpec)))
				cd->clrTextBk = RGB(142,233,164);
			return CDRF_DODEFAULT;
		}
	}
	return CDRF_DODEFAULT;
}
