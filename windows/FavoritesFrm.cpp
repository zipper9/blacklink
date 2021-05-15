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

#include "FavoritesFrm.h"
#include "HubFrame.h"
#include "FavHubProperties.h"
#include "FavHubGroupsDlg.h"
#include "ExMessageBox.h"
#include "../client/ShareManager.h"

HIconWrapper FavoriteHubsFrame::stateIconOn(IDR_ONLINE_ICO);
HIconWrapper FavoriteHubsFrame::stateIconOff(IDR_OFFLINE_ICO);

int FavoriteHubsFrame::columnIndexes[] =
{
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_NICK,
	COLUMN_PASSWORD,
	COLUMN_SERVER,
	COLUMN_USERDESCRIPTION,
	COLUMN_EMAIL,
	COLUMN_SHARE_GROUP,
	COLUMN_CONNECTION_STATUS,
	COLUMN_LAST_CONNECTED
};

int FavoriteHubsFrame::columnSizes[] =
{
	240,
	260,
	125,
	80,
	230,
	125,
	110,
	100,
	275,
	140
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::AUTO_CONNECT,
	ResourceManager::DESCRIPTION,
	ResourceManager::NICK,
	ResourceManager::PASSWORD,
	ResourceManager::SERVER,
	ResourceManager::USER_DESCRIPTION,
	ResourceManager::EMAIL,
	ResourceManager::SHARE_GROUP,
	ResourceManager::STATUS,
	ResourceManager::LAST_SUCCESFULLY_CONNECTED
};

FavoriteHubsFrame::FavoriteHubsFrame() : TimerHelper(m_hWnd), noSave(true), xdu(0), ydu(0)
{
}

LRESULT FavoriteHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	accel.LoadAccelerators(MAKEINTRESOURCE(IDR_FAVORITES));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_HUBLIST);
	ctrlHubs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	setListViewColors(ctrlHubs);
	ctrlHubs.EnableGroupView(TRUE);
	WinUtil::setExplorerTheme(ctrlHubs);
	
	LVGROUPMETRICS metrics = {0};
	metrics.cbSize = sizeof(metrics);
	metrics.mask = LVGMF_TEXTCOLOR;
	metrics.crHeader = SETTING(TEXT_GENERAL_FORE_COLOR);
	ctrlHubs.SetGroupMetrics(&metrics);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(FAVORITES_FRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(FAVORITES_FRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = LVCFMT_LEFT;
		ctrlHubs.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	ctrlHubs.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	//ctrlHubs.setVisible(SETTING(FAVORITESFRAME_VISIBLE)); // !SMT!-UI
	ctrlHubs.setSortFromSettings(SETTING(FAVORITES_FRAME_SORT), ExListViewCtrl::SORT_STRING_NOCASE, COLUMN_LAST);
	
	ctrlConnect.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                   BS_PUSHBUTTON, 0, IDC_CONNECT);
	ctrlConnect.SetWindowText(CTSTRING(CONNECT));
	ctrlConnect.SetFont(Fonts::g_systemFont);
	ctrlNew.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	               BS_PUSHBUTTON, 0, IDC_NEWFAV);
	ctrlNew.SetWindowText(CTSTRING(NEW));
	ctrlNew.SetFont(Fonts::g_systemFont);
	
	ctrlProps.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 BS_PUSHBUTTON, 0, IDC_EDIT);
	ctrlProps.SetWindowText(CTSTRING(PROPERTIES));
	ctrlProps.SetFont(Fonts::g_systemFont);
	
	ctrlRemove.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  BS_PUSHBUTTON, 0, IDC_REMOVE);
	ctrlRemove.SetWindowText(CTSTRING(REMOVE));
	ctrlRemove.SetFont(Fonts::g_systemFont);
	
	ctrlUp.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	              BS_PUSHBUTTON, 0, IDC_MOVE_UP);
	ctrlUp.SetWindowText(CTSTRING(MOVE_UP));
	ctrlUp.SetFont(Fonts::g_systemFont);
	
	ctrlDown.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                BS_PUSHBUTTON, 0, IDC_MOVE_DOWN);
	ctrlDown.SetWindowText(CTSTRING(MOVE_DOWN));
	ctrlDown.SetFont(Fonts::g_systemFont);
	
	ctrlManageGroups.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                        BS_PUSHBUTTON, 0, IDC_MANAGE_GROUPS);
	ctrlManageGroups.SetWindowText(CTSTRING(MANAGE_GROUPS));
	ctrlManageGroups.SetFont(Fonts::g_systemFont);
	
	onlineStatusImg.Create(16, 16, ILC_COLOR32 | ILC_MASK,  0, 2);
	onlineStatusImg.AddIcon(stateIconOn);
	onlineStatusImg.AddIcon(stateIconOff);
	ctrlHubs.SetImageList(onlineStatusImg, LVSIL_SMALL);
	ClientManager::getOnlineClients(onlineHubs);
	
	FavoriteManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);
	
	createTimer(15000);
	
	fillList();
	
	hubsMenu.CreatePopupMenu();
	hubsMenu.AppendMenu(MF_STRING, IDC_OPEN_HUB_LOG, CTSTRING(OPEN_HUB_LOG), g_iconBitmaps.getBitmap(IconBitmaps::LOGS, 0));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT), g_iconBitmaps.getBitmap(IconBitmaps::QUICK_CONNECT, 0));
	hubsMenu.AppendMenu(MF_STRING, IDC_NEWFAV, CTSTRING(NEW));
	hubsMenu.AppendMenu(MF_STRING, IDC_MOVE_UP, CTSTRING(MOVE_UP));
	hubsMenu.AppendMenu(MF_STRING, IDC_MOVE_DOWN, CTSTRING(MOVE_DOWN));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES));
	hubsMenu.SetMenuDefaultItem(IDC_CONNECT);
	
	noSave = false;
	
	bHandled = FALSE;
	return TRUE;
}

LRESULT FavoriteHubsFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);
	bHandled = FALSE;
	return 0;
}

FavoriteHubsFrame::StateKeeper::StateKeeper(ExListViewCtrl& hubs, bool ensureVisible) : hubs(hubs), ensureVisible(ensureVisible)
{
	hubs.SetRedraw(FALSE);
	
	// in grouped mode, the indexes of each item are completely random, so use entry pointers instead
	int i = -1;
	while ((i = hubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		selected.push_back(static_cast<int>(hubs.GetItemData(i)));
	}
	
	SCROLLINFO si = { 0 };
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS;
	hubs.GetScrollInfo(SB_VERT, &si);
	
	scroll = si.nPos;
}

FavoriteHubsFrame::StateKeeper::~StateKeeper()
{
	// restore visual updating now, otherwise it doesn't always scroll
	hubs.SetRedraw(TRUE);
	
	for (int i : selected)
	{
		const auto cnt = hubs.GetItemCount();
		for (int j = 0; j < cnt; ++j)
		{
			if (static_cast<int>(hubs.GetItemData(j)) == i)
			{
				hubs.SetItemState(j, LVIS_SELECTED, LVIS_SELECTED);
				if (ensureVisible)
					hubs.EnsureVisible(j, FALSE);
				break;
			}
		}
	}
	
	ListView_Scroll(hubs.m_hWnd, 0, scroll);
}

const std::vector<int>& FavoriteHubsFrame::StateKeeper::getSelection() const
{
	return selected;
}

void FavoriteHubsFrame::openSelected()
{
	if (!checkNick())
		return;
		
	HubFrame::Settings cs;
	FavoriteHubEntry entry;
	auto fm = FavoriteManager::getInstance();
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		if (fm->getFavoriteHub(static_cast<int>(ctrlHubs.GetItemData(i)), entry))
		{
			RecentHubEntry r;
			r.setName(entry.getName());
			r.setDescription(entry.getDescription());
			r.setUsers("*");
			r.setShared("*");
			r.setOpenTab("+");
			r.setServer(entry.getServer());
			fm->addRecent(r);
			cs.copySettings(entry);
			HubFrame::openHubWindow(cs);
		}
	}
	return;
}

static void getAttributes(TStringList& l, const FavoriteHubEntry* entry)
{
	l.push_back(Text::toT(entry->getName()));
	l.push_back(Text::toT(entry->getDescription()));
	l.push_back(Text::toT(entry->getNick(false)));
	l.push_back(tstring(entry->getPassword().size(), '*'));
	l.push_back(Text::toT(entry->getServer()));
	l.push_back(Text::toT(entry->getUserDescription()));
	l.push_back(Text::toT(entry->getEmail()));
	if (entry->getHideShare())
		l.push_back(TSTRING(SHARE_GROUP_NOTHING));
	else
	{
		const CID& sg = entry->getShareGroup();
		if (!sg.isZero())
		{
			string name;
			ShareManager::getInstance()->getShareGroupName(sg, name);
			l.push_back(Text::toT(name));
		}
		else
			l.push_back(Util::emptyStringT);
	}
}

void FavoriteHubsFrame::addEntryL(const FavoriteHubEntry* entry, int pos, int groupIndex, time_t now)
{
	TStringList l;
	getAttributes(l, entry);
	const ConnectionStatus& cs = entry->getConnectionStatus();
	bool online = isOnline(entry->getServer());
	bool wasConnecting = cs.lastAttempt >= Util::getStartTime();
	l.push_back(online ? TSTRING(ONLINE) : printConnectionStatus(cs, now));
	l.push_back(printLastConnected(cs));
	
	const int i = ctrlHubs.insert(pos, l, 0, static_cast<LPARAM>(entry->getID()));
	ctrlHubs.SetCheckState(i, entry->getAutoConnect());
	
	LVITEM lvItem = { 0 };
	lvItem.mask = LVIF_GROUPID | LVIF_IMAGE;
	lvItem.iItem = i;
	if (online)
		lvItem.iImage = 0;
	else
		lvItem.iImage = wasConnecting ? 1 : -1;
	lvItem.iGroupId = groupIndex;
	ctrlHubs.SetItem(&lvItem);
}

int FavoriteHubsFrame::findItem(int id) const
{
	int count = ctrlHubs.GetItemCount();
	for (int i = 0; i < count; ++i)
		if (static_cast<int>(ctrlHubs.GetItemData(i)) == id)
			return i;
	return -1;
}

LRESULT FavoriteHubsFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == HUB_CONNECTED)
	{
		std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
		onlineHubs.insert(*hub);
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(*hub);
		if (fhe)
		{
			int id = fhe->getID();
			ConnectionStatus cs = fhe->getConnectionStatus();
			fm->releaseFavoriteHubEntryPtr(fhe);
			int index = findItem(id);
			if (index != -1)
			{
				ctrlHubs.SetItem(index, 0, LVIF_IMAGE, nullptr, 0, 0, 0, 0);
				ctrlHubs.SetItemText(index, COLUMN_CONNECTION_STATUS, CTSTRING(ONLINE));
				tstring lastConn = printLastConnected(cs);
				ctrlHubs.SetItemText(index, COLUMN_LAST_CONNECTED, lastConn.c_str());
				ctrlHubs.Update(index);
			}
		}
		return 0;
	}
	
	if (wParam == HUB_DISCONNECTED)
	{
		std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
		onlineHubs.erase(*hub);
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(*hub);
		if (fhe)
		{
			int id = fhe->getID();
			ConnectionStatus cs = fhe->getConnectionStatus();
			fm->releaseFavoriteHubEntryPtr(fhe);
			int index = findItem(id);
			if (index != -1)
			{
				ctrlHubs.SetItem(index, 0, LVIF_IMAGE, nullptr, 1, 0, 0, 0);
				tstring status = printConnectionStatus(cs, GET_TIME());
				ctrlHubs.SetItemText(index, COLUMN_CONNECTION_STATUS, status.c_str());
				ctrlHubs.Update(index);
			}
		}
		return 0;
	}
	
	return 0;
}

LRESULT FavoriteHubsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlHubs)
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
		hubsMenu.EnableMenuItem(IDC_OPEN_HUB_LOG, status);
		hubsMenu.EnableMenuItem(IDC_CONNECT, status);
		hubsMenu.EnableMenuItem(IDC_EDIT, status);
		hubsMenu.EnableMenuItem(IDC_MOVE_UP, status);
		hubsMenu.EnableMenuItem(IDC_MOVE_DOWN, status);
		hubsMenu.EnableMenuItem(IDC_REMOVE, status);
		
		tstring x;
		if (ctrlHubs.GetSelectedCount() == 1)
		{
			int index = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
			auto fm = FavoriteManager::getInstance();
			const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(static_cast<int>(ctrlHubs.GetItemData(index)));
			if (fhe)
			{
				x = Text::toT(fhe->getName());
				fm->releaseFavoriteHubEntryPtr(fhe);
			}
		}
		
		if (!x.empty())
			hubsMenu.InsertSeparatorFirst(x);
			
		hubsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		if (!x.empty())
			hubsMenu.RemoveFirstItem();
			
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT FavoriteHubsFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::FAVORITES, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT FavoriteHubsFrame::onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_NEWFAV, 0);
	}
	else
	{
		PostMessage(WM_COMMAND, IDC_CONNECT, 0);
	}
	
	return 0;
}

LRESULT FavoriteHubsFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_NEWFAV, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		case VK_RETURN:
			PostMessage(WM_COMMAND, IDC_CONNECT, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int count = ctrlHubs.GetSelectedCount();
	if (count)
	{
		if (BOOLSETTING(CONFIRM_HUB_REMOVAL))
		{
			UINT checkState = BST_UNCHECKED;
			if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
				return 0;
			if (checkState == BST_CHECKED) SET_SETTING(CONFIRM_HUB_REMOVAL, FALSE);
		}
		
		std::vector<int> hubIds;
		hubIds.reserve(count);
		int i = -1;
		while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
			hubIds.push_back(static_cast<int>(ctrlHubs.GetItemData(i)));

		auto fm = FavoriteManager::getInstance();
		bool save = false;
		for (int id : hubIds)
			if (fm->removeFavoriteHub(id, false))
				save = true;
		if (save)
			fm->saveFavorites();
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
	if (i != -1)
	{
		auto fm = FavoriteManager::getInstance();
		FavoriteHubEntry entry;
		if (!fm->getFavoriteHub(static_cast<int>(ctrlHubs.GetItemData(i)), entry))
		{
			dcassert(0);
			return 0;
		}
		FavHubProperties dlg(&entry);
		if (dlg.DoModal(*this) == IDOK)
			fm->setFavoriteHub(entry);
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto fm = FavoriteManager::getInstance();
	FavoriteHubEntry entry;
	FavHubProperties dlg(&entry);
	if (dlg.DoModal(*this) == IDOK)
		fm->addFavoriteHub(entry, true);
	return 0;
}

bool FavoriteHubsFrame::checkNick()
{
	if (SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		return false;
	}
	return true;
}

LRESULT FavoriteHubsFrame::onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	handleMove(true);
	return 0;
}

LRESULT FavoriteHubsFrame::onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	handleMove(false);
	return 0;
}

void FavoriteHubsFrame::handleMove(bool up)
{
	auto fm = FavoriteManager::getInstance();
	StateKeeper keeper(ctrlHubs);
	const std::vector<int>& selected = keeper.getSelection();
	TStringList groups(getSortedGroups());

	{
		FavoriteManager::LockInstanceHubs lock(fm, true);
		FavoriteHubEntryList& fh = lock.getFavoriteHubs();
		if (fh.size() <= 1) return;
	
		if (!up)
			reverse(fh.begin(), fh.end());
		FavoriteHubEntryList moved;
		for (size_t i = 0; i < fh.size(); ++i)
		{
			FavoriteHubEntry* fhe = fh[i];
			if (find(selected.begin(), selected.end(), fhe->getID()) == selected.end())
				continue;
			if (find(moved.begin(), moved.end(), fhe) != moved.end())
				continue;
			const string& group = fhe->getGroup();
			for (size_t j = i; ;)
			{
				if (j == 0)
				{
					// couldn't move within the same group; change group.
					if (!up)
						reverse(groups.begin(), groups.end());
					auto ig = find(groups.begin(), groups.end(), Text::toT(group));
					if (ig != groups.begin())
					{
						fhe->setGroup(Text::fromT(*(--ig)));
						fh.erase(fh.begin() + i);
						fh.push_back(fhe);
						moved.push_back(fhe);
					}
					break;
				}
				if (fh[--j]->getGroup() == group)
				{
					std::swap(fh[i], fh[j]);
					break;
				}
			}
		}
		if (!up)
			reverse(fh.begin(), fh.end());		
	}
	fm->saveFavorites();	
	fillList(groups);
}

TStringList FavoriteHubsFrame::getSortedGroups() const
{
	std::set<tstring, noCaseStringLess> sortedGroups;
	{
		FavoriteManager::LockInstanceHubs lock(FavoriteManager::getInstance(), false);
		const FavHubGroups& favHubGroups = lock.getFavHubGroups();
		for (auto i = favHubGroups.cbegin(), iend = favHubGroups.cend(); i != iend; ++i)
			sortedGroups.insert(Text::toT(i->first));
	}
	
	TStringList groups(sortedGroups.begin(), sortedGroups.end());
	groups.insert(groups.begin(), Util::emptyStringT); // default group (otherwise, hubs without group don't show up)
	return groups;
}

static int getGroupIndex(const string& group, const TStringList& groups)
{
	int index = 0;
	if (!group.empty())
	{
		TStringList::const_iterator groupI = find(groups.begin() + 1, groups.end(), Text::toT(group));
		if (groupI != groups.end())
			index = groupI - groups.begin();
	}			
	return index;
}

void FavoriteHubsFrame::fillList(const TStringList& groups)
{
	bool oldNoSave = noSave;
	noSave = true;
	
	ctrlHubs.DeleteAllItems();
	
	for (size_t i = 0; i < groups.size(); ++i)
	{
		// insert groups
		LVGROUP lg = {0};
		lg.cbSize = sizeof(lg);
		lg.iGroupId = static_cast<int>(i);
		lg.state = LVGS_NORMAL |
#ifdef OSVER_WIN_XP
		           (CompatibilityManager::isOsVistaPlus() ? LVGS_COLLAPSIBLE : 0)
#else
		           LVGS_COLLAPSIBLE
#endif
		           ;
		lg.mask = LVGF_GROUPID | LVGF_HEADER | LVGF_STATE | LVGF_ALIGN;
		lg.uAlign = LVGA_HEADER_LEFT;
		
		// Header-title must be unicode (Convert if necessary)
		lg.pszHeader = T2W(const_cast<TCHAR*>(groups[i].c_str()));
		lg.cchHeader = static_cast<int>(groups[i].length());
		ctrlHubs.InsertGroup(i, &lg);
	}
	
	{
		time_t now = GET_TIME();
		FavoriteManager::LockInstanceHubs lock(FavoriteManager::getInstance(), false);
		const auto& fl = lock.getFavoriteHubs();
		auto cnt = ctrlHubs.GetItemCount();
		for (auto i = fl.cbegin(); i != fl.cend(); ++i)
		{
			const string& group = (*i)->getGroup();
			addEntryL(*i, cnt++, getGroupIndex(group, groups), now);
		}
	}
	
	noSave = oldNoSave;
}

void FavoriteHubsFrame::fillList()
{
	TStringList groups(getSortedGroups());
	fillList(groups);
}

LRESULT FavoriteHubsFrame::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMITEMACTIVATE* l = (NMITEMACTIVATE*)pnmh;
	if (l->iItem != -1)
	{
		BOOL enabled = ctrlHubs.GetItemState(l->iItem, LVIS_SELECTED);
		::EnableWindow(GetDlgItem(IDC_CONNECT), enabled);
		::EnableWindow(GetDlgItem(IDC_REMOVE), enabled);
		::EnableWindow(GetDlgItem(IDC_EDIT), enabled);
		::EnableWindow(GetDlgItem(IDC_MOVE_UP), enabled);
		::EnableWindow(GetDlgItem(IDC_MOVE_DOWN), enabled);
		if (!noSave && ((l->uNewState & LVIS_STATEIMAGEMASK) != (l->uOldState & LVIS_STATEIMAGEMASK)))
		{
			auto fm = FavoriteManager::getInstance();
			int id = static_cast<int>(ctrlHubs.GetItemData(l->iItem));
			fm->setFavoriteHubAutoConnect(id, ctrlHubs.GetCheckState(l->iItem) != FALSE);
		}
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	destroyTimer();
	if (!closed)
	{
		closed = true;
		ClientManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		FavoriteManager::getInstance()->removeListener(this);
		setButtonPressed(IDC_FAVORITES, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlHubs, SettingsManager::FAVORITES_FRAME_ORDER,
		                         SettingsManager::FAVORITES_FRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		                         
		SET_SETTING(FAVORITES_FRAME_SORT, ctrlHubs.getSortForSettings());
		bHandled = FALSE;
		return 0;
	}
}

void FavoriteHubsFrame::UpdateLayout(BOOL resizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, resizeBars);

	if (!xdu)
	{
		WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
		buttonWidth = WinUtil::dialogUnitsToPixelsX(54, xdu);
		buttonHeight = WinUtil::dialogUnitsToPixelsY(12, ydu);
		vertMarginBottom = WinUtil::dialogUnitsToPixelsY(3, ydu);
		int splitBarHeight = GetSystemMetrics(SM_CYSIZEFRAME);
		if (vertMarginBottom < splitBarHeight) vertMarginBottom = splitBarHeight;
		vertMarginTop = vertMarginBottom;
		vertMarginBottom -= splitBarHeight;
	}

	CRect rc = rect;
	rc.bottom -= buttonHeight + vertMarginTop + vertMarginBottom;
	ctrlHubs.MoveWindow(rc);

	static const int bspace = 10;

	rc.top = rc.bottom + vertMarginTop;
	rc.bottom = rc.top + buttonHeight;

	rc.left = 2;
	rc.right = rc.left + buttonWidth;
	ctrlNew.MoveWindow(rc);

	rc.OffsetRect(buttonWidth + 2, 0);
	ctrlProps.MoveWindow(rc);

	rc.OffsetRect(buttonWidth + 2, 0);
	ctrlRemove.MoveWindow(rc);

	rc.OffsetRect(bspace + buttonWidth + 2, 0);
	ctrlUp.MoveWindow(rc);

	rc.OffsetRect(buttonWidth + 2, 0);
	ctrlDown.MoveWindow(rc);

	rc.OffsetRect(bspace + buttonWidth + 2, 0);
	ctrlConnect.MoveWindow(rc);

	rc.OffsetRect(bspace + buttonWidth + 2, 0);
	rc.right += 16;
	ctrlManageGroups.MoveWindow(rc);
}

LRESULT FavoriteHubsFrame::onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetSelectedCount() == 1)
	{
		int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(static_cast<int>(ctrlHubs.GetItemData(i)));
		if (fhe)
		{
			StringMap params;
			params["hubNI"] = fhe->getName();
			params["hubURL"] = fhe->getServer();
			params["myNI"] = fhe->getNick();
			fm->releaseFavoriteHubEntryPtr(fhe);
			WinUtil::openLog(SETTING(LOG_FILE_MAIN_CHAT), params, TSTRING(NO_LOG_FOR_HUB));
		}
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onManageGroups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	FavHubGroupsDlg dlg;
	dlg.DoModal();
	
	StateKeeper keeper(ctrlHubs);
	fillList();
	
	return 0;
}

void FavoriteHubsFrame::on(SettingsManagerListener::Repaint)
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

LRESULT FavoriteHubsFrame::onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	// On sort, disable move functionality.
	//::EnableWindow(GetDlgItem(IDC_MOVE_UP), FALSE);
	//::EnableWindow(GetDlgItem(IDC_MOVE_DOWN), FALSE);
	NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
	if (l->iSubItem == ctrlHubs.getSortColumn())
	{
		if (!ctrlHubs.isAscending())
		{
			ctrlHubs.setSort(-1, ctrlHubs.getSortType());
		}
		else
		{
			ctrlHubs.setSortDirection(false);
		}
	}
	else
	{
		ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_STRING_NOCASE);
	}
	return 0;
}

tstring FavoriteHubsFrame::printConnectionStatus(const ConnectionStatus& cs, time_t curTime)
{
	if (cs.status != ConnectionStatus::UNKNOWN && cs.lastAttempt)
	{
		tstring statusText = cs.status == ConnectionStatus::SUCCESS ? TSTRING(CONNECTION_STATUS_OK) : TSTRING(CONNECTION_STATUS_FAIL);
		uint64_t delta = curTime > cs.lastAttempt ? curTime - cs.lastAttempt : 0;
		tstring timeText = Text::toT(Util::formatTime(delta, true));
		return TSTRING_F(CONNECTION_STATUS_FMT, statusText % timeText);
	}
	return Util::emptyStringT;
}

tstring FavoriteHubsFrame::printLastConnected(const ConnectionStatus& cs)
{
	if (cs.lastSuccess)
		return Text::toT(Util::formatDateTime(cs.lastSuccess));
	return Util::emptyStringT;
}

LRESULT FavoriteHubsFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	const time_t now = GET_TIME();
	auto fm = FavoriteManager::getInstance();
	CLockRedraw<> lockRedraw(ctrlHubs);
	const int count = ctrlHubs.GetItemCount();
	for (int pos = 0; pos < count; ++pos)
	{
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(static_cast<int>(ctrlHubs.GetItemData(pos)));
		if (fhe)
		{
			const auto& cs = fhe->getConnectionStatus();
			bool online = isOnline(fhe->getServer());
			tstring status = online ? TSTRING(ONLINE) : printConnectionStatus(cs, now);
			tstring lastConn = printLastConnected(cs);
			fm->releaseFavoriteHubEntryPtr(fhe);
			ctrlHubs.SetItemText(pos, COLUMN_CONNECTION_STATUS, status.c_str());
			ctrlHubs.SetItemText(pos, COLUMN_LAST_CONNECTED, lastConn.c_str());
		}
	}
	if (ctrlHubs.getSortColumn() == COLUMN_CONNECTION_STATUS || ctrlHubs.getSortColumn() == COLUMN_LAST_CONNECTED)
		ctrlHubs.resort();
	return 0;
}

void FavoriteHubsFrame::on(FavoriteAdded, const FavoriteHubEntry*) noexcept
{
	StateKeeper keeper(ctrlHubs);
	fillList();
}

void FavoriteHubsFrame::on(FavoriteRemoved, const FavoriteHubEntry* entry) noexcept
{
	ctrlHubs.DeleteItem(ctrlHubs.find(static_cast<LPARAM>(entry->getID())));
}

void FavoriteHubsFrame::on(FavoriteChanged, const FavoriteHubEntry* entry) noexcept
{
	int index = ctrlHubs.find(static_cast<LPARAM>(entry->getID()));
	if (index < 0) return;
	
	TStringList groups(getSortedGroups());
	int groupIndex = getGroupIndex(entry->getGroup(), groups);
	LVITEM lvItem = { 0 };
	lvItem.mask = LVIF_GROUPID;
	lvItem.iItem = index;
	if (ctrlHubs.GetItem(&lvItem) && lvItem.iGroupId == groupIndex)
	{
		TStringList l;
		getAttributes(l, entry);
		for (size_t i = 0; i < l.size(); ++i)
			ctrlHubs.SetItemText(index, i, l[i].c_str());
		// TODO: resort if needed
		ctrlHubs.SetCheckState(index, entry->getAutoConnect());
		return;
	}
	StateKeeper keeper(ctrlHubs);
	fillList(groups);
}

BOOL FavoriteHubsFrame::PreTranslateMessage(MSG* pMsg)
{
	return accel.TranslateAccelerator(m_hWnd, pMsg);
}

CFrameWndClassInfo& FavoriteHubsFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("FavoriteHubsFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::FAVORITES, 0);

	return wc;
}
