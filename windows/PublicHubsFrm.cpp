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
#include "FavoritesFrm.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "CountryList.h"
#include "../client/FormatUtil.h"
#include "../client/SettingsUtil.h"
#include "../client/ConfCore.h"

const int PublicHubsFrame::columnId[] =
{
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_USERS,
	COLUMN_SERVER,
	COLUMN_COUNTRY,
	COLUMN_SECURE,
	COLUMN_SHARED,
	COLUMN_MINSHARE,
	COLUMN_MINSLOTS,
	COLUMN_MAXHUBS,
	COLUMN_MAXUSERS,
	COLUMN_RELIABILITY,
	COLUMN_RATING,
	COLUMN_WEBSITE,
	COLUMN_EMAIL,
	COLUMN_ENCODING,
	COLUMN_SECURE_URL,
	COLUMN_SOFTWARE,
	COLUMN_NETWORK
};

static const int columnSizes[] =
{
	200, // COLUMN_NAME
	290, // COLUMN_DESCRIPTION
	60,  // COLUMN_USERS
	190, // COLUMN_SERVER
	140, // COLUMN_COUNTRY
	60,  // COLUMN_SECURE
	100, // COLUMN_SHARED
	100, // COLUMN_MINSHARE
	80,  // COLUMN_MINSLOTS
	80,  // COLUMN_MAXHUBS
	80,  // COLUMN_MAXUSERS
	80,  // COLUMN_RELIABILITY
	60,  // COLUMN_RATING
	180, // COLUMN_WEBSITE
	130, // COLIMN_EMAIL
	80,  // COLUMN_ENCODING
	190, // COLUMN_SECURE_URL
	130, // COLUMN_SOFTWARE
	140  // COLUMN_NETWORK
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::HUB_NAME,
	ResourceManager::DESCRIPTION,
	ResourceManager::USERS,
	ResourceManager::HUB_ADDRESS,
	ResourceManager::COUNTRY,
	ResourceManager::SECURE,
	ResourceManager::SHARED,
	ResourceManager::MIN_SHARE,
	ResourceManager::MIN_SLOTS,
	ResourceManager::MAX_HUBS,
	ResourceManager::MAX_USERS,
	ResourceManager::RELIABILITY,
	ResourceManager::RATING,
	ResourceManager::WEBSITE,
	ResourceManager::EMAIL,
	ResourceManager::FAVORITE_HUB_CHARACTER_SET,
	ResourceManager::SECURE_URL,
	ResourceManager::HUB_SOFTWARE,
	ResourceManager::HUB_NETWORK
};

PublicHubsFrame::PublicHubsFrame() : users(0), visibleHubs(0)
{
	ctrlHubs.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlHubs.setColumnFormat(COLUMN_USERS, LVCFMT_RIGHT);
	ctrlHubs.setColumnFormat(COLUMN_SHARED, LVCFMT_RIGHT);
	ctrlHubs.setColumnFormat(COLUMN_MINSHARE, LVCFMT_RIGHT);
	ctrlHubs.setColumnFormat(COLUMN_MINSLOTS, LVCFMT_RIGHT);
	ctrlHubs.setColumnFormat(COLUMN_MAXHUBS, LVCFMT_RIGHT);
	ctrlHubs.setColumnFormat(COLUMN_MAXUSERS, LVCFMT_RIGHT);
	xdu = ydu = 0;
}

LRESULT PublicHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	m_hAccel = LoadAccelerators(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDR_INTERNET_HUBS));	
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.ModifyStyleEx(0, WS_EX_COMPOSITED);

	int w[3] = { 0, 0, 0 };
	ctrlStatus.SetParts(3, w);

	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
	                WS_EX_CLIENTEDGE, IDC_HUBLIST);
	ctrlHubs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	if (WinUtil::setExplorerTheme(ctrlHubs))
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED | CustomDrawHelpers::FLAG_USE_HOT_ITEM;

	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));

	auto ss = SettingsManager::instance.getUiSettings();
	ctrlHubs.insertColumns(Conf::PUBLIC_HUBS_FRAME_ORDER, Conf::PUBLIC_HUBS_FRAME_WIDTHS, Conf::PUBLIC_HUBS_FRAME_VISIBLE);
	ctrlHubs.setSortFromSettings(ss->getInt(Conf::PUBLIC_HUBS_FRAME_SORT), COLUMN_USERS, false);
	setListViewColors(ctrlHubs);
	ctrlHubs.SetImageList(g_otherImage.getIconList(), LVSIL_SMALL);

	ctrlPubLists.Create(m_hWnd, rcDefault, NULL,
	                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | WS_HSCROLL |
	                    WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE, IDC_PUB_LIST_DROPDOWN);
	ctrlPubLists.SetFont(Fonts::g_systemFont, FALSE);

	ctrlConfigure.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_PUB_LIST_CONFIG);
	ctrlConfigure.SetWindowText(CTSTRING(CONFIGURE));
	ctrlConfigure.SetFont(Fonts::g_systemFont);

	ctrlRefresh.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_REFRESH);
	ctrlRefresh.SetWindowText(CTSTRING(REFRESH));
	ctrlRefresh.SetFont(Fonts::g_systemFont);

	ctrlFilter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP, 0, IDC_FILTER_BOX);
	ctrlFilter.setHint(TSTRING(FILTER_HUBS_HINT));
	ctrlFilter.SetFont(Fonts::g_systemFont);
	ctrlFilter.setBitmap(g_iconBitmaps.getBitmap(IconBitmaps::FILTER, 0));
	ctrlFilter.setCloseBitmap(g_iconBitmaps.getBitmap(IconBitmaps::CLEAR_SEARCH, 0));
	ctrlFilter.setNotifMask(SearchBoxCtrl::NOTIF_RETURN | SearchBoxCtrl::NOTIF_TAB);

	ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
	                     WS_VSCROLL | CBS_DROPDOWNLIST | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_FILTER_SEL);
	ctrlFilterSel.SetFont(Fonts::g_systemFont, FALSE);

	// populate the filter list with the column names
	WinUtil::fillComboBoxStrings(ctrlFilterSel, columnNames, COLUMN_LAST);

	ctrlFilterSel.AddString(CTSTRING(ANY));
	ctrlFilterSel.SetCurSel(COLUMN_LAST);

	ctrlHubs.SetFocus();

	HublistManager *hublistManager = HublistManager::getInstance();

	hublistManager->addListener(this);
	SettingsManager::instance.addListener(this);
	ClientManager::getInstance()->addListener(this);
	FavoriteManager::getInstance()->addListener(this);

	hublistManager->setServerList(Util::getConfString(Conf::HUBLIST_SERVERS));
	hublistManager->getHubLists(hubLists);
	updateDropDown();

	copyMenu.CreatePopupMenu();
	for (int i = 0; i < _countof(columnNames); i++)
		copyMenu.AppendMenu(MF_STRING, IDC_COPY + i, CTSTRING_I(columnNames[i]));

	onListSelChanged();

	bHandled = FALSE;
	return TRUE;
}

LRESULT PublicHubsFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);
	bHandled = FALSE;
	return 0;
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
	PublicHubsListDlg dlg(hubLists);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		HublistManager *hublistManager = HublistManager::getInstance();
		hublistManager->setServerList(Util::getConfString(Conf::HUBLIST_SERVERS));
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
		const HubInfo* data = ctrlHubs.getItemData(i);
		string server = getPubServer(data);
		FavoriteHubEntry e;
		e.setName(Text::fromT(data->getText(COLUMN_NAME)));
		e.setDescription(Text::fromT(data->getText(COLUMN_DESCRIPTION)));
		e.setServer(server);
		e.setKeyPrint(data->getKeyPrint());
		if (!Util::isAdcHub(server))
			e.setEncoding(Text::charsetFromString(Text::fromT(data->getText(COLUMN_ENCODING))));
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
		ctrlHubs.deleteAll();
		closed = true;
		FavoriteManager::getInstance()->removeListener(this);
		ClientManager::getInstance()->removeListener(this);
		HublistManager::getInstance()->removeListener(this);
		SettingsManager::instance.removeListener(this);
		setButtonPressed(IDC_PUBLIC_HUBS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlHubs.saveHeaderOrder(Conf::PUBLIC_HUBS_FRAME_ORDER, Conf::PUBLIC_HUBS_FRAME_WIDTHS, Conf::PUBLIC_HUBS_FRAME_VISIBLE);
		auto ss = SettingsManager::instance.getUiSettings();
		ss->setInt(Conf::PUBLIC_HUBS_FRAME_SORT, ctrlHubs.getSortForSettings());
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

	const int buttonOffset = 1;

	if (!xdu)
	{
		WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
		comboHeight = WinUtil::getComboBoxHeight(ctrlPubLists, nullptr);
		buttonWidth = WinUtil::dialogUnitsToPixelsX(54, xdu);
		buttonHeight = comboHeight + 2*buttonOffset;
		filterWidth = WinUtil::dialogUnitsToPixelsX(110, xdu);
		minComboWidth = WinUtil::dialogUnitsToPixelsX(90, xdu);
		smallHorizSpace = WinUtil::dialogUnitsToPixelsX(2, xdu);
		horizSpace = WinUtil::dialogUnitsToPixelsX(8, xdu);
		horizOffset = WinUtil::dialogUnitsToPixelsX(2, xdu);
		vertOffset = WinUtil::dialogUnitsToPixelsY(3, ydu);
	}

	int panelHeight = comboHeight + 2*vertOffset;
	const int fixedWidth = minComboWidth + filterWidth + horizSpace + 3*smallHorizSpace+ 2*buttonWidth;
	const int minWidth = fixedWidth + minComboWidth + 2*horizOffset;

	if (rect.right - rect.left < minWidth)
		panelHeight = 0;

	// listview
	CRect rc = rect;
	rc.bottom -= panelHeight;
	ctrlHubs.MoveWindow(rc);

	if (panelHeight)
	{
		const int comboExpandedHeight = WinUtil::dialogUnitsToPixelsY(120, ydu);

		// lists dropdown
		rc.top = rect.bottom - panelHeight + vertOffset;
		rc.left = rect.left + horizOffset;
		rc.bottom = rc.top + comboExpandedHeight;
		rc.right = rect.right - (horizOffset + fixedWidth);
		ctrlPubLists.SetWindowPos(nullptr, &rc, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

		// configure button
		rc.left = rc.right + smallHorizSpace;
		rc.right = rc.left + buttonWidth;
		rc.top -= buttonOffset;
		rc.bottom = rc.top + buttonHeight;
		ctrlConfigure.SetWindowPos(nullptr, &rc, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

		// refresh button
		rc.left = rc.right + smallHorizSpace;
		rc.right = rc.left + buttonWidth;
		ctrlRefresh.SetWindowPos(nullptr, &rc, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

		// filter edit
		rc.top += buttonOffset;
		rc.bottom = rc.top + comboHeight;
		rc.left = rc.right + horizSpace;
		rc.right = rc.left + filterWidth;
		ctrlFilter.SetWindowPos(nullptr, &rc, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
		ctrlFilter.Invalidate(); // TODO: check SetWindowPos flags to invalidate it

		// filter sel
		rc.left = rc.right + smallHorizSpace;
		rc.right = rc.left + minComboWidth;
		rc.bottom = rc.top + comboExpandedHeight;
		ctrlFilterSel.SetWindowPos(nullptr, &rc, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}
	else
	{
		ctrlPubLists.ShowWindow(SW_HIDE);
		ctrlConfigure.ShowWindow(SW_HIDE);
		ctrlRefresh.ShowWindow(SW_HIDE);
		ctrlFilter.ShowWindow(SW_HIDE);
		ctrlFilterSel.ShowWindow(SW_HIDE);
	}
}

void PublicHubsFrame::openHub(int ind)
{
	const HubInfo* data = ctrlHubs.getItemData(ind);
	RecentHubEntry r;
	r.setName(Text::fromT(data->getText(COLUMN_NAME)));
	r.setDescription(Text::fromT(data->getText(COLUMN_DESCRIPTION)));
	string server = getPubServer(data);
	r.setServer(server);
	r.setOpenTab("+");
	FavoriteManager::getInstance()->addRecent(r);
	string encoding = Text::fromT(data->getText(COLUMN_ENCODING));
	HubFrame::Settings cs;
	cs.server = server;
	cs.keyPrint = data->getKeyPrint();
	cs.encoding = Text::charsetFromString(encoding);
	HubFrame::openHubWindow(cs);
}

bool PublicHubsFrame::checkNick()
{
	if (ClientManager::isNickEmpty())
	{
		MessageBox(CTSTRING(ENTER_NICK), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		return false;
	}
	return true;
}

void PublicHubsFrame::updateList(const HubEntry::List &hubs)
{
	StringSet onlineHubs;
	ClientManager::getOnlineClients(onlineHubs);

	users = 0;
	visibleHubs = 0;

	ctrlHubs.SetRedraw(FALSE);
	ctrlHubs.deleteAllNoLock();

	double size = -1;
	FilterModes mode = NONE;

	int sel = ctrlFilterSel.GetCurSel();
	if (sel >= 0 && sel < COLUMN_LAST) sel = columnId[sel];
	bool doSizeCompare = parseFilter(mode, size);
	auto fm = FavoriteManager::getInstance();

	for (auto j = hubs.cbegin(); j != hubs.cend(); ++j)
	{
		const auto &i = *j;
		if (matchFilter(i, sel, doSizeCompare, mode, size))
		{
			HubInfo* data = new HubInfo;
			data->update(i);
			int flags = 0;
			if (onlineHubs.find(data->getHubUrl()) != onlineHubs.end())
				flags |= HubInfo::FLAG_ONLINE_NORMAL;
			if (fm->isFavoriteHub(data->getHubUrl()))
				flags |= HubInfo::FLAG_FAVORITE_NORMAL;
			const string& secureUrl = data->getSecureHubUrl();
			if (!secureUrl.empty())
			{
				if (onlineHubs.find(secureUrl) != onlineHubs.end())
					flags |= HubInfo::FLAG_ONLINE_SECURE;
				if (fm->isFavoriteHub(secureUrl))
					flags |= HubInfo::FLAG_FAVORITE_SECURE;
			}
			data->setFlags(flags);
			ctrlHubs.insertItem(data, I_IMAGECALLBACK);
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
			status = TSTRING_F(HUBLIST_DOWNLOADING, Text::toT(url));
			break;
		case HublistManager::STATE_DOWNLOADED:
			status = TSTRING_F(HUBLIST_DOWNLOADED, Text::toT(url));
			break;
		case HublistManager::STATE_DOWNLOAD_FAILED:
			status = TSTRING_F(HUBLIST_DOWNLOAD_FAILED, Text::toT(info.error));
			break;
		case HublistManager::STATE_FROM_CACHE:
		{
			tstring date = Text::toT(Util::formatDateTime("%x", info.lastModified));
			status = TSTRING_F(HUBLIST_LOADED_FROM_CACHE, date);
			break;
		}
		case HublistManager::STATE_PARSE_FAILED:
			status = TSTRING(HUBLIST_DOWNLOAD_CORRUPTED);
			/* HUBLIST_CACHE_CORRUPTED */
	}
	ctrlStatus.SetText(0, status.c_str());
}

LRESULT PublicHubsFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	switch (wParam)
	{
		case WPARAM_UPDATE_STATE:
		{
			uint64_t *val = reinterpret_cast<uint64_t*>(lParam);
			int index = -1;
			for (size_t i = 0; i < hubLists.size(); i++)
				if (hubLists[i].id == *val)
				{
					index = i;
					break;
				}
			if (index >= 0)
			{
				HublistManager::getInstance()->getHubList(hubLists[index], *val);
				showStatus(hubLists[index]);
				if (hubLists[index].state == HublistManager::STATE_DOWNLOADED ||
					hubLists[index].state == HublistManager::STATE_FROM_CACHE)
						updateList(hubLists[index].list);
			}
			delete val;
			break;
		}
		case WPARAM_HUB_CONNECTED:
		{
			std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
			bool secureUrl;
			HubInfo* data = findHub(*hub, secureUrl, nullptr);
			if (data)
			{
				int oldFlags = data->getFlags();
				data->setFlag(secureUrl ? HubInfo::FLAG_ONLINE_SECURE : HubInfo::FLAG_ONLINE_NORMAL);
				if (data->getFlags() != oldFlags)
					redraw();
			}
			break;
		}
		case WPARAM_HUB_DISCONNECTED:
		{
			std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
			bool secureUrl;
			HubInfo* data = findHub(*hub, secureUrl, nullptr);
			if (data)
			{
				int oldFlags = data->getFlags();
				data->clearFlag(secureUrl ? HubInfo::FLAG_ONLINE_SECURE : HubInfo::FLAG_ONLINE_NORMAL);
				if (data->getFlags() != oldFlags)
					redraw();
			}
			break;
		}
		case WPARAM_FAVORITE_ADDED:
		{
			std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
			int pos;
			bool secureUrl;
			HubInfo* data = findHub(*hub, secureUrl, &pos);
			if (data)
			{
				int oldFlags = data->getFlags();
				data->setFlag(secureUrl ? HubInfo::FLAG_FAVORITE_SECURE : HubInfo::FLAG_FAVORITE_NORMAL);
				if (data->getFlags() != oldFlags)
				{
					ctrlHubs.updateImage(pos, 0);
					redraw();
				}
			}
			break;
		}
		case WPARAM_FAVORITE_REMOVED:
		{
			std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
			int pos;
			bool secureUrl;
			HubInfo* data = findHub(*hub, secureUrl, &pos);
			if (data)
			{
				int oldFlags = data->getFlags();
				data->clearFlag(secureUrl ? HubInfo::FLAG_FAVORITE_SECURE : HubInfo::FLAG_FAVORITE_NORMAL);
				if (data->getFlags() != oldFlags)
				{
					ctrlHubs.updateImage(pos, 0);
					redraw();
				}
			}
			break;
		}
	}
	return 0;
}

void PublicHubsFrame::redraw()
{
	RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void PublicHubsFrame::updateStatus()
{
	ctrlStatus.SetText(1, (TSTRING(HUBS) + _T(": ") + Util::toStringT(visibleHubs)).c_str());
	ctrlStatus.SetText(2, (TSTRING(USERS) + _T(": ") + Util::toStringT(users)).c_str());
}

LRESULT PublicHubsFrame::onNextDlgCtl(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	MSG msg = {};
	msg.hwnd = ctrlFilter.m_hWnd;
	msg.message = WM_KEYDOWN;
	msg.wParam = VK_TAB;
	IsDialogMessage(&msg);
	return 0;
}

LRESULT PublicHubsFrame::onFilterChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = ctrlPubLists.GetCurSel();
	if (index < 0) return 0;

	if (!SettingsManager::instance.getUiSettings()->getBool(Conf::FILTER_ENTER))
	{
		tstring text = ctrlFilter.getText();
		Text::makeLower(text);
		filter = Text::fromT(text);
		updateList(hubLists[index].list);
	}
	return 0;
}

LRESULT PublicHubsFrame::onFilterReturn(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	int index = ctrlPubLists.GetCurSel();
	if (index < 0) return 0;

	if (SettingsManager::instance.getUiSettings()->getBool(Conf::FILTER_ENTER))
	{
		tstring text = ctrlFilter.getText();
		Text::makeLower(text);
		filter = Text::fromT(text);
		updateList(hubLists[index].list);
	}
	return 0;
}

LRESULT PublicHubsFrame::onFilterSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
{
	int index = ctrlPubLists.GetCurSel();
	if (index < 0) return 0;

	tstring text = ctrlFilter.getText();
	Text::makeLower(text);
	filter = Text::fromT(text);
	updateList(hubLists[index].list);
	return 0;
}

LRESULT PublicHubsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlHubs)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		CRect rc;
		ctrlHubs.GetHeader().GetWindowRect(&rc);
		if (PtInRect(&rc, pt))
			return 0;
		if (pt.x == -1 && pt.y == -1)
			WinUtil::getContextMenuPos(ctrlHubs, pt);

		OMenu hubsMenu;
		hubsMenu.CreatePopupMenu();
		int selCount = ctrlHubs.GetSelectedCount();
		int copyMenuIndex = -1;
		if (selCount > 0)
		{
			if (selCount == 1)
			{
				int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
				if (i == -1) return 0;
				const HubInfo* data = ctrlHubs.getItemData(i);
				hubsMenu.InsertSeparatorFirst(data->getText(COLUMN_NAME));
				if (data->getFlags() & HubInfo::FLAGS_ONLINE)
					hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(OPEN_HUB_WINDOW), g_iconBitmaps.getBitmap(IconBitmaps::GOTO_HUB, 0));
				else
					hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT), g_iconBitmaps.getBitmap(IconBitmaps::QUICK_CONNECT, 0));
				if (FavoriteManager::getInstance()->isFavoriteHub(getPubServer(data)))
				{
					hubsMenu.AppendMenu(MF_STRING, IDC_FAVORITES, CTSTRING(OPEN_FAV_HUBS_WINDOW), g_iconBitmaps.getBitmap(IconBitmaps::FAVORITES, 0));
					hubsMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE_HUB, 0));
				}
				else
					hubsMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(ADD_TO_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::ADD_HUB, 0));
			}
			else
			{
				hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT), g_iconBitmaps.getBitmap(IconBitmaps::QUICK_CONNECT, 0));
				hubsMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(ADD_TO_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::ADD_HUB, 0));
				hubsMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE_HUB, 0));
			}
			copyMenuIndex = hubsMenu.GetMenuItemCount();
			hubsMenu.AppendMenu(MF_POPUP, copyMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
			hubsMenu.AppendMenu(MF_SEPARATOR);
		}
		hubsMenu.AppendMenu(MF_STRING, IDC_REFRESH, CTSTRING(REFRESH));
		hubsMenu.SetMenuDefaultItem(IDC_CONNECT);

		hubsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		if (copyMenuIndex != -1)
			hubsMenu.RemoveMenu(copyMenuIndex, MF_BYPOSITION);
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT PublicHubsFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::INTERNET_HUBS, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT PublicHubsFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int column = wID - IDC_COPY;
	if (column < COLUMN_FIRST || column >= COLUMN_LAST) return 0;
	column = columnId[column];
	tstring result;
	int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
	while (i != -1)
	{
		const HubInfo* data = ctrlHubs.getItemData(i);
		const tstring& text = data->getText(column);
		if (!text.empty())
		{
			if (!result.empty()) result += _T("\r\n");
			result += text;
		}
		i = ctrlHubs.GetNextItem(i, LVNI_SELECTED);
	}
	if (!result.empty()) WinUtil::setClipboard(result);	
	return 0;
}

LRESULT PublicHubsFrame::onOpenFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	FavoriteHubsFrame::openWindow();
	if (FavoriteHubsFrame::g_frame)
	{
		int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		if (i != -1)
			FavoriteHubsFrame::g_frame->showHub(getPubServer(i));
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
	const string* entryString = nullptr;
	string tmp;

	switch (sel)
	{
		case COLUMN_NAME:
			entryString = &entry.getName();
			doSizeCompare = false;
			break;
		case COLUMN_DESCRIPTION:
			entryString = &entry.getDescription();
			doSizeCompare = false;
			break;
		case COLUMN_USERS:
			entrySize = entry.getUsers();
			break;
		case COLUMN_SERVER:
			entryString = &entry.getServer();
			doSizeCompare = false;
			break;
		case COLUMN_COUNTRY:
			entryString = &entry.getCountry();
			doSizeCompare = false;
			break;
		case COLUMN_SHARED:
			entrySize = (double) entry.getShared();
			break;
		case COLUMN_MINSHARE:
			entrySize = (double) entry.getMinShare();
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
			entryString = &entry.getRating();
			doSizeCompare = false;
			break;
		case COLUMN_ENCODING:
			entryString = &entry.getEncoding();
			doSizeCompare = false;
			break;
		case COLUMN_SECURE:
			if (!entry.getSecureUrl().empty()) tmp = STRING(YES);
			entryString = &tmp;
			doSizeCompare = false;
			break;
		case COLUMN_SECURE_URL:
			entryString = &entry.getSecureUrl();
			doSizeCompare = false;
			break;
		case COLUMN_WEBSITE:
			entryString = &entry.getWebsite();
			doSizeCompare = false;
			break;
		case COLUMN_EMAIL:
			entryString = &entry.getEmail();
			doSizeCompare = false;
			break;
		case COLUMN_SOFTWARE:
			entryString = &entry.getSoftware();
			doSizeCompare = false;
			break;
		case COLUMN_NETWORK:
			entryString = &entry.getNetwork();
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
		if (sel >= COLUMN_LAST)
		{
			if (Text::toLower(entry.getName(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getDescription(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getServer(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getSecureUrl(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getCountry(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getRating(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getWebsite(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getEmail(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getSoftware(), tmp).find(filter) != string::npos ||
				Text::toLower(entry.getNetwork(), tmp).find(filter) != string::npos)
			{
				insert = true;
			}
		}
		if (!insert && entryString && Text::toLower(*entryString, tmp).find(filter) != string::npos)
			insert = true;
	}

	return insert;
}

void PublicHubsFrame::on(SettingsManagerListener::ApplySettings)
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
	WinUtil::postSpeakerMsg(m_hWnd, WPARAM_UPDATE_STATE, ptr);
}

void PublicHubsFrame::on(HublistManagerListener::Redirected, uint64_t id) noexcept
{
	uint64_t *ptr = new uint64_t(id);
	WinUtil::postSpeakerMsg(m_hWnd, WPARAM_UPDATE_STATE, ptr);
}

LRESULT PublicHubsFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled */)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			CustomDrawHelpers::startDraw(customDrawState, cd);
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
		{
			const HubInfo* data = reinterpret_cast<HubInfo*>(cd->nmcd.lItemlParam);
			cd->clrText = Colors::g_textColor;
			cd->clrTextBk = Colors::g_bgColor;
			if (data->getFlags() & HubInfo::FLAGS_ONLINE)
				cd->clrTextBk = RGB(142,233,164);
			CustomDrawHelpers::startItemDraw(customDrawState, cd);
			return CDRF_NOTIFYSUBITEMDRAW;
		}
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			const HubInfo* data = reinterpret_cast<HubInfo*>(cd->nmcd.lItemlParam);
			const int column = ctrlHubs.findColumn(cd->iSubItem);
			if (column == COLUMN_COUNTRY)
			{
				const tstring& country = data->getText(COLUMN_COUNTRY);
				if (!country.empty())
				{
					uint16_t code = getCountryCode(data->getCountryIndex());
					CustomDrawHelpers::drawCountry(customDrawState, cd, code, country);
					return CDRF_SKIPDEFAULT;
				}
			}
			CDCHandle(cd->nmcd.hdc).SelectFont((column == 0 &&
				(data->getFlags() & HubInfo::FLAGS_FAVORITE)) ? Fonts::g_boldFont : Fonts::g_systemFont);
			return CDRF_NEWFONT;
		}
	}
	return CDRF_DODEFAULT;
}

const tstring& PublicHubsFrame::HubInfo::getText(int col) const
{
	if (col >= 0 && col < COLUMN_LAST) return text[col];
	return Util::emptyStringT;
}

int PublicHubsFrame::HubInfo::compareItems(const HubInfo* a, const HubInfo* b, int col, int /*flags*/)
{
	switch (col)
	{
		case COLUMN_USERS:
			return compare(a->users, b->users);
		case COLUMN_SHARED:
			return compare(a->shared, b->shared);
		case COLUMN_MINSHARE:
			return compare(a->minShare, b->minShare);
		case COLUMN_MINSLOTS:
			return compare(a->minSlots, b->minSlots);
		case COLUMN_MAXHUBS:
			return compare(a->maxHubs, b->maxHubs);
		case COLUMN_MAXUSERS:
			return compare(a->maxUsers, b->maxUsers);
		case COLUMN_RELIABILITY:
			return compare(a->reliability, b->reliability);
		case COLUMN_RATING:
			return compare(a->rating, b->rating);
	}
	return Util::defaultSort(a->getText(col), b->getText(col));
}

void PublicHubsFrame::HubInfo::update(const HubEntry& hub)
{
	const string& country = hub.getCountry();
	hubUrl = Text::toLower(hub.getServer());
	secureHubUrl = Text::toLower(hub.getSecureUrl());
	text[COLUMN_NAME] = Text::toT(hub.getName());
	text[COLUMN_DESCRIPTION] = Text::toT(hub.getDescription());
	text[COLUMN_USERS] = Util::toStringT(hub.getUsers());
	text[COLUMN_SERVER] = Text::toT(hub.getServer());
	text[COLUMN_COUNTRY] = Text::toT(country);
	text[COLUMN_SHARED] = Util::formatBytesT(hub.getShared());
	text[COLUMN_MINSHARE] = Util::formatBytesT(hub.getMinShare());
	text[COLUMN_MINSLOTS] = Util::toStringT(hub.getMinSlots());
	text[COLUMN_MAXHUBS] = Util::toStringT(hub.getMaxHubs());
	text[COLUMN_MAXUSERS] = Util::toStringT(hub.getMaxUsers());
	text[COLUMN_RELIABILITY] = Util::toStringT(hub.getReliability());
	text[COLUMN_RATING] = Text::toT(hub.getRating());
	text[COLUMN_ENCODING] = Text::toT(hub.getEncoding());
	if (!secureHubUrl.empty()) text[COLUMN_SECURE] = TSTRING(YES);
	text[COLUMN_SECURE_URL] = Text::toT(hub.getSecureUrl());
	text[COLUMN_WEBSITE] = Text::toT(hub.getWebsite());
	text[COLUMN_EMAIL] = Text::toT(hub.getEmail());
	text[COLUMN_SOFTWARE] = Text::toT(hub.getSoftware());
	text[COLUMN_NETWORK] = Text::toT(hub.getNetwork());
	keyPrint = hub.getKeyPrint();
	countryIndex = getCountryByName(country);
	users = hub.getUsers();
	shared = hub.getShared();
	minShare = hub.getMinShare();
	minSlots = hub.getMinSlots();
	maxHubs = hub.getMaxHubs();
	maxUsers = hub.getMaxUsers();
	reliability = hub.getReliability();
	rating = Util::toInt(hub.getRating());
}

PublicHubsFrame::HubInfo* PublicHubsFrame::findHub(const string& url, bool& secureUrl, int* pos) const
{
	secureUrl = false;
	int count = ctrlHubs.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		HubInfo* data = ctrlHubs.getItemData(i);
		if (data->getHubUrl() == url)
		{
			if (pos) *pos = i;
			return data;
		}
		if (!data->getSecureHubUrl().empty() && data->getSecureHubUrl() == url)
		{
			secureUrl = true;
			if (pos) *pos = i;
			return data;
		}
	}
	if (pos) *pos = -1;
	return nullptr;
}

void PublicHubsFrame::on(ClientConnected, const Client* c) noexcept
{
	if (!ClientManager::isBeforeShutdown())
		WinUtil::postSpeakerMsg(m_hWnd, WPARAM_HUB_CONNECTED, new string(c->getHubUrl()));
}

void PublicHubsFrame::on(ClientDisconnected, const Client* c) noexcept
{
	if (!ClientManager::isBeforeShutdown())
		WinUtil::postSpeakerMsg(m_hWnd, WPARAM_HUB_DISCONNECTED, new string(c->getHubUrl()));
}

void PublicHubsFrame::on(FavoriteAdded, const FavoriteHubEntry* fhe) noexcept
{
	if (!ClientManager::isBeforeShutdown())
		WinUtil::postSpeakerMsg(m_hWnd, WPARAM_FAVORITE_ADDED, new string(fhe->getServer()));
}

void PublicHubsFrame::on(FavoriteRemoved, const FavoriteHubEntry* fhe) noexcept
{
	if (!ClientManager::isBeforeShutdown())
		WinUtil::postSpeakerMsg(m_hWnd, WPARAM_FAVORITE_REMOVED, new string(fhe->getServer()));
}

BOOL PublicHubsFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::g_tabCtrl->isActive(m_hWnd)) return FALSE;
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	if (WinUtil::isCtrl()) return FALSE;
	return IsDialogMessage(pMsg);
}

CFrameWndClassInfo& PublicHubsFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("PublicHubsFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::INTERNET_HUBS, 0);

	return wc;
}
