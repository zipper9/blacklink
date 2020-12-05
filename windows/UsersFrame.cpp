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

#include "resource.h"
#include "UsersFrame.h"
#include "MainFrm.h"
#include "LineDlg.h"
#include "HubFrame.h"
#include "ResourceLoader.h"
#include "WinUtil.h"
#include "ExMessageBox.h"
#include "../client/Util.h"
#include <algorithm>

const int UsersFrame::columnId[] =
{
	COLUMN_NICK,
	COLUMN_HUB,
	COLUMN_SEEN,
	COLUMN_DESCRIPTION,
	COLUMN_SPEED_LIMIT,
	COLUMN_PM_HANDLING,
	COLUMN_SLOTS,
	COLUMN_CID
};

static const int columnSizes[] = { 200, 300, 150, 200, 100, 100, 100, 300 };

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::AUTO_GRANT_NICK,
	ResourceManager::LAST_HUB,
	ResourceManager::LAST_SEEN,
	ResourceManager::DESCRIPTION,
	ResourceManager::UPLOAD_SPEED_LIMIT,
	ResourceManager::PM_HANDLING,
	ResourceManager::SLOTS,
	ResourceManager::CID
};

static tstring formatLastSeenTime(time_t t)
{
	if (!t) return Util::emptyStringT;
	return Text::toT(Util::formatDateTime(t));
}

UsersFrame::UsersFrame() : startup(true)
{
	ctrlUsers.setColumns(_countof(columnId), columnId, columnNames, columnSizes);	
	ctrlUsers.setColumnFormat(COLUMN_SPEED_LIMIT, LVCFMT_RIGHT);
	ctrlUsers.setColumnFormat(COLUMN_SLOTS, LVCFMT_RIGHT);
}

LRESULT UsersFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);  //[-] SCALOlaz
	// ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_USERS);
	ctrlUsers.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	ResourceLoader::LoadImageList(IDR_FAV_USERS_STATES, images, 16, 16);
	ctrlUsers.SetImageList(images, LVSIL_SMALL);
	setListViewColors(ctrlUsers);
	WinUtil::setExplorerTheme(ctrlUsers);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));

	ctrlUsers.insertColumns(SettingsManager::USERS_FRAME_ORDER, SettingsManager::USERS_FRAME_WIDTHS, SettingsManager::USERS_FRAME_VISIBLE);
	ctrlUsers.setSortFromSettings(SETTING(USERS_FRAME_SORT));
	
	FavoriteManager::getInstance()->addListener(this);
	UserManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	
	CLockRedraw<> lockRedraw(ctrlUsers);
	{
		FavoriteManager::LockInstanceUsers lockedInstance;
		const auto& favUsers = lockedInstance.getFavoriteUsersL();
		for (auto i = favUsers.cbegin(); i != favUsers.cend(); ++i)
			addUser(i->second);
	}
	
	ctrlIgnored.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | WS_HSCROLL | WS_VSCROLL |
	                   LVS_REPORT | LVS_SHOWSELALWAYS | LVS_ALIGNLEFT | /*LVS_NOCOLUMNHEADER |*/ LVS_NOSORTHEADER | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_IGNORELIST);
	ctrlIgnored.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	ctrlIgnored.SetImageList(images, LVSIL_SMALL);
	setListViewColors(ctrlIgnored);
	
	m_nProportionalPos = 8500;  // SETTING(USERS_FRAME_SPLIT);
	SetSplitterPanes(ctrlUsers.m_hWnd, ctrlIgnored.m_hWnd, false);
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
	
	CRect rc;
	ctrlIgnored.GetClientRect(rc);
	ctrlIgnored.InsertColumn(0, CTSTRING(IGNORED_USERS) /*_T("Dummy")*/, LVCFMT_LEFT, 180 /*rc.Width()*/, 0);
	WinUtil::setExplorerTheme(ctrlIgnored);

	ctrlIgnoreAdd.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_IGNORE_ADD);
	ctrlIgnoreAdd.SetWindowText(CTSTRING(ADD));
	ctrlIgnoreAdd.SetFont(Fonts::g_systemFont);

	ctrlIgnoreName.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_NOHIDESEL | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_IGNORELIST_EDIT);
	ctrlIgnoreName.SetLimitText(64);
	ctrlIgnoreName.SetFont(Fonts::g_font);
	
	ctrlIgnoreRemove.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_IGNORE_REMOVE);
	ctrlIgnoreRemove.SetWindowText(CTSTRING(REMOVE));
	ctrlIgnoreRemove.SetFont(Fonts::g_systemFont);
	::EnableWindow(ctrlIgnoreRemove, FALSE);
	
	ctrlIgnoreClear.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_IGNORE_CLEAR);
	ctrlIgnoreClear.SetWindowText(CTSTRING(IGNORE_CLEAR));
	ctrlIgnoreClear.SetFont(Fonts::g_systemFont);

	insertIgnoreList();
	
	startup = false;
	bHandled = FALSE;
	UpdateLayout(); // Именно в таком порядке! Здесь второму фрейму координаты записываются.
	return TRUE;
}

LRESULT UsersFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlUsers && ctrlUsers.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlUsers, pt);
		}
		
		clearUserMenu();
		
		OMenu usersMenu;
		usersMenu.CreatePopupMenu();
		usersMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES));
		
		if (ctrlUsers.GetSelectedCount() == 1)
		{
			int index = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
			const auto ii = ctrlUsers.getItemData(index);
			const UserPtr& user = ii->getUser();
			usersMenu.AppendMenu(MF_SEPARATOR);
			tstring nick = Text::toT(user->getLastNick());
			reinitUserMenu(user, Util::emptyString); // TODO: add hub hint.
			if (!nick.empty())
				usersMenu.InsertSeparatorFirst(nick);
			appendAndActivateUserItems(usersMenu);
		}
		else
		{
			usersMenu.AppendMenu(MF_STRING, IDC_REMOVE_FROM_FAVORITES, CTSTRING(REMOVE_FROM_FAVORITES));
		}
		usersMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT UsersFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::FAVORITE_USERS, 0);
	opt->isHub = false;
	return TRUE;
}

static int getButtonWidth(HWND hwnd, HDC dc)
{
	tstring text;
	WinUtil::getWindowText(hwnd, text);
	return std::max<int>(WinUtil::getTextWidth(text, dc) + 10, 80);
}

void UsersFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;

	const int barHigh = 28;
	RECT rect, rect2;
	GetClientRect(&rect);
	rect2 = rect;
	rect2.bottom = rect.bottom - barHigh;
	
	// position bars and offset their dimensions
	UpdateBarsPosition(rect2, bResizeBars);
	
	CRect rc_l, rc_r;
	ctrlUsers.GetClientRect(rc_l);
	rc_l.bottom = rect2.bottom;
	ctrlUsers.MoveWindow(rc_l);
	
	ctrlIgnored.GetClientRect(rc_r);
	rc_r.bottom = rect2.bottom;
	ctrlIgnored.MoveWindow(rc_r);

	HDC dc = GetDC();
	CRect rcControl = rect;
	rcControl.top = rect.bottom - 24;
	rcControl.right -= 4;
	rcControl.left = rcControl.right - getButtonWidth(ctrlIgnoreClear, dc);
	ctrlIgnoreClear.MoveWindow(rcControl);
	
	rcControl.right = rcControl.left - 12;
	rcControl.left = rcControl.right - getButtonWidth(ctrlIgnoreRemove, dc);
	ctrlIgnoreRemove.MoveWindow(rcControl);
	
	rcControl.right = rcControl.left - 4;
	rcControl.left = rcControl.right - getButtonWidth(ctrlIgnoreAdd, dc);
	ctrlIgnoreAdd.MoveWindow(rcControl);
	
	rcControl.right = rcControl.left - 4;
	rcControl.left = rcControl.right - 150;
	ctrlIgnoreName.MoveWindow(rcControl);
	ReleaseDC(dc);

	CRect rc = rect2;
	SetSplitterRect(rc);
}

LRESULT UsersFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlUsers.getSelectedCount())
	{
		if (BOOLSETTING(CONFIRM_USER_REMOVAL))
		{
			UINT checkState = BST_UNCHECKED;
			if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2, checkState) != IDYES)
				return 0;
			if (checkState == BST_CHECKED)
				SET_SETTING(CONFIRM_USER_REMOVAL, FALSE);
		}
		int i = -1;
		while ((i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED)) != -1)
		{
			const ItemInfo* ii = ctrlUsers.getItemData(i);
			if (ii)
				FavoriteManager::getInstance()->removeFavoriteUser(ii->getUser());
		}
	}
	return 0;
}

LRESULT UsersFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int count = ctrlUsers.GetSelectedCount();
	if (count)
	{
		LineDlg dlg;
		dlg.description = TSTRING(DESCRIPTION);
		if (count == 1)
		{
			int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
			const ItemInfo* ii = ctrlUsers.getItemData(i);
			dlg.title = TSTRING_F(SET_DESCRIPTION_FOR_USER, ii->getText(COLUMN_NICK));
			dlg.line = ii->getText(COLUMN_DESCRIPTION);
		}
		else
		{
			dlg.title = TSTRING_F(SET_DESCRIPTION_FOR_USERS, count);
		}
		if (dlg.DoModal(m_hWnd) != IDOK) return 0;

		string description = Text::fromT(dlg.line);
		int i = -1;
		while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			ItemInfo* ii = ctrlUsers.getItemData(i);
			FavoriteManager::getInstance()->setUserDescription(ii->getUser(), description);
			ii->columns[COLUMN_DESCRIPTION] = dlg.line;
			ctrlUsers.updateItem(i, COLUMN_DESCRIPTION);
		}
	}
	return 0;
}

LRESULT UsersFrame::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* l = (NMITEMACTIVATE*)pnmh;
	if (!startup && l->iItem != -1 && ((l->uNewState & LVIS_STATEIMAGEMASK) != (l->uOldState & LVIS_STATEIMAGEMASK)))
	{
		FavoriteManager::getInstance()->setAutoGrantSlot(ctrlUsers.getItemData(l->iItem)->getUser(), ctrlUsers.GetCheckState(l->iItem) != FALSE);
	}
	return 0;
}

LRESULT UsersFrame::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem != -1)
	{
		static const int cmd[] = { IDC_GETLIST, IDC_PRIVATE_MESSAGE, IDC_MATCH_QUEUE, IDC_EDIT, IDC_OPEN_USER_LOG };
		PostMessage(WM_COMMAND, cmd[SETTING(FAVUSERLIST_DBLCLICK)], 0);
	}
	else
	{
		bHandled = FALSE;
	}
	
	return 0;
}

LRESULT UsersFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE_FROM_FAVORITES, 0);
			break;
		case VK_RETURN:
			PostMessage(WM_COMMAND, IDC_EDIT, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

#if 0
LRESULT UsersFrame::onConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const int count = ctrlUsers.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		const ItemInfo *ii = ctrlUsers.getItemData(i);
		const string& url = FavoriteManager::getUserUrl(ii->getUser());
		if (!url.empty())
		{
			HubFrame::openHubWindow(url);
		}
	}
	return 0;
}
#endif

void UsersFrame::addUser(const FavoriteUser& user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		auto ii = new ItemInfo(user);
		int i = ctrlUsers.insertItem(ii, 0);
		bool b = user.isSet(FavoriteUser::FLAG_GRANT_SLOT);
		ctrlUsers.SetCheckState(i, b);
		updateUser(i, ii, user);
	}
}

void UsersFrame::updateUser(const UserPtr& user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		const int count = ctrlUsers.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			ItemInfo *ii = ctrlUsers.getItemData(i);
			if (ii->getUser() == user)
			{
				FavoriteUser currentFavUser;
				if (FavoriteManager::getFavoriteUser(user, currentFavUser))
					updateUser(i, ii, currentFavUser);
			}
		}
	}
}

void UsersFrame::updateUser(const int i, ItemInfo* ii, const FavoriteUser& favUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		const auto flags = favUser.user->getFlags();
		ii->columns[COLUMN_SEEN] = (flags & User::ONLINE) ? TSTRING(ONLINE) : formatLastSeenTime(favUser.lastSeen);
		
		int imageIndex;
		if (flags & User::ONLINE)
		{
			imageIndex = (flags & User::AWAY) ? 1 : 0;
		} else imageIndex = 2;
		
		if (favUser.uploadLimit == FavoriteUser::UL_BAN || favUser.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
			imageIndex += 3;
		
		ii->update(favUser);
		
		ctrlUsers.SetItem(i, 0, LVIF_IMAGE, NULL, imageIndex, 0, 0, NULL);
		
		ctrlUsers.updateItem(i);
		setCountMessages(ctrlUsers.GetItemCount());
	}
}

void UsersFrame::removeUser(const FavoriteUser& aUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		const int count = ctrlUsers.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			ItemInfo *ii = ctrlUsers.getItemData(i);
			if (ii->getUser() == aUser.user)
			{
				ctrlUsers.DeleteItem(i);
				delete ii;
				setCountMessages(ctrlUsers.GetItemCount());
				return;
			}
		}
	}
}

LRESULT UsersFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		closed = true;
		FavoriteManager::getInstance()->removeListener(this);
		UserManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		//WinUtil::UnlinkStaticMenus(usersMenu); // !SMT!-S
		WinUtil::setButtonPressed(IDC_FAVUSERS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlUsers.saveHeaderOrder(SettingsManager::USERS_FRAME_ORDER, SettingsManager::USERS_FRAME_WIDTHS, SettingsManager::USERS_FRAME_VISIBLE);
		SET_SETTING(USERS_FRAME_SORT, ctrlUsers.getSortForSettings());
		ctrlUsers.deleteAll();
		
		//if (m_nProportionalPos < 8000 || m_nProportionalPos > 9000)
		//  m_nProportionalPos = 8500;
		//SET_SETTING(USERS_FRAME_SPLIT, m_nProportionalPos); // Пока НЕ сохраняем положение сплиттера. Неясно какие габариты правильные
		bHandled = FALSE;
		return 0;
	}
}

void UsersFrame::ItemInfo::update(const FavoriteUser& u)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		bool isOnline = user->isOnline();
		lastSeen = u.lastSeen;
		speedLimit = u.uploadLimit;
		columns[COLUMN_NICK] = Text::toT(u.nick);
		columns[COLUMN_HUB] = isOnline ? WinUtil::getHubNames(u.user, u.url).first : Text::toT(u.url);
		columns[COLUMN_SEEN] = isOnline ? TSTRING(ONLINE) : formatLastSeenTime(lastSeen);
		columns[COLUMN_DESCRIPTION] = Text::toT(u.description);
		
		if (u.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
			columns[COLUMN_PM_HANDLING] = TSTRING(IGNORE_PRIVATE);
		else if (u.isSet(FavoriteUser::FLAG_FREE_PM_ACCESS))
			columns[COLUMN_PM_HANDLING] = TSTRING(FREE_PM_ACCESS);
		else
			columns[COLUMN_PM_HANDLING].clear();
			
		columns[COLUMN_SPEED_LIMIT] = Text::toT(FavoriteUser::getSpeedLimitText(u.uploadLimit));
		columns[COLUMN_SLOTS] = Util::toStringT(u.user->getSlots());
		columns[COLUMN_CID] = Text::toT(u.user->getCID().toBase32());
	}
}

int UsersFrame::ItemInfo::compareItems(const UsersFrame::ItemInfo* a, const UsersFrame::ItemInfo* b, int col)
{
	dcassert(col >= 0 && col < COLUMN_LAST);
	switch (col)
	{
		case COLUMN_SEEN:
			return compare(a->lastSeen, b->lastSeen);
		case COLUMN_SLOTS:
			return compare(a->user->getSlots(), b->user->getSlots());
		case COLUMN_SPEED_LIMIT:
			if (a->speedLimit == b->speedLimit) return 0;
			if (a->speedLimit == FavoriteUser::UL_BAN || b->speedLimit == FavoriteUser::UL_BAN)
				return a->speedLimit == FavoriteUser::UL_BAN ? -1 : 1;
			if (a->speedLimit == FavoriteUser::UL_SU || b->speedLimit == FavoriteUser::UL_SU)
				return a->speedLimit == FavoriteUser::UL_SU ? 1 : -1;
			if (a->speedLimit == 0 || b->speedLimit == 0)
				return a->speedLimit == 0 ? 1 : -1;
			return a->speedLimit < b->speedLimit ? -1 : 1;
		default:
			return Util::defaultSort(a->columns[col], b->columns[col], true);
	}
}

LRESULT UsersFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == USER_UPDATED)
	{
		UserPtr* user = (UserPtr*)lParam;
		updateUser(*user);
		delete user;
	}
	return 0;
}

LRESULT UsersFrame::onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlUsers.GetSelectedCount() == 1)
	{
		int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
		ItemInfo* ii = ctrlUsers.getItemData(i);
		dcassert(i != -1);
		
		const auto& user = ii->getUser();
		StringMap params;
		params["hubNI"] = Util::toString(ClientManager::getHubNames(user->getCID(), Util::emptyString));
		params["hubURL"] = Util::toString(ClientManager::getHubs(user->getCID(), Util::emptyString));
		params["userCID"] = user->getCID().toBase32();
		params["userNI"] = user->getLastNick();
		params["myCID"] = ClientManager::getMyCID().toBase32();
		
		WinUtil::openLog(SETTING(LOG_FILE_PRIVATE_CHAT), params, TSTRING(NO_LOG_FOR_USER));
	}
	return 0;
}

void UsersFrame::on(UserAdded, const FavoriteUser& aUser) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	addUser(aUser);
}

void UsersFrame::on(UserRemoved, const FavoriteUser& aUser) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		removeUser(aUser);
	}
}

void UsersFrame::on(UserStatusChanged, const UserPtr& aUser) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		safe_post_message(*this, USER_UPDATED, new UserPtr(aUser));
	}
}

void UsersFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlUsers.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

LRESULT UsersFrame::onIgnorePrivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo *ii = ctrlUsers.getItemData(i);
		switch (wID)
		{
			case IDC_PM_IGNORED:
				ii->columns[COLUMN_PM_HANDLING] = TSTRING(IGNORE_PRIVATE);
				FavoriteManager::getInstance()->setIgnorePM(ii->getUser());
				break;
			case IDC_PM_FREE:
				ii->columns[COLUMN_PM_HANDLING] = TSTRING(FREE_PM_ACCESS);
				FavoriteManager::getInstance()->setFreePM(ii->getUser());
				break;
			default:
				ii->columns[COLUMN_PM_HANDLING].clear();
				FavoriteManager::getInstance()->setNormalPM(ii->getUser());
		};
		
		updateUser(ii->getUser());
		ctrlUsers.updateItem(i);
	}
	return 0;
}

LRESULT UsersFrame::onSetUserLimit(WORD /* wNotifyCode */, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const int lim = getSpeedLimitByCtrlId(wID, speedMenuCustomVal);
	int i = -1;
	while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo *ii = ctrlUsers.getItemData(i);
		FavoriteManager::getInstance()->setUploadLimit(ii->getUser(), lim);
		ii->columns[COLUMN_SPEED_LIMIT] = Text::toT(FavoriteUser::getSpeedLimitText(lim));
		updateUser(ii->getUser());
		ctrlUsers.updateItem(i);
	}
	return 0;
}

LRESULT UsersFrame::onIgnoredItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	updateIgnoreListButtons();
	return 0;
}

void UsersFrame::insertIgnoreList()
{
	StringSet ignoreSet;
	UserManager::getInstance()->getIgnoreList(ignoreSet);
	vector<tstring> sortedList;
	sortedList.resize(ignoreSet.size());
	size_t index = 0;
	for (auto i = ignoreSet.cbegin(); i != ignoreSet.cend(); ++i)
		sortedList[index++] = Text::toT(*i);
	std::sort(sortedList.begin(), sortedList.end(),
		[](tstring& s1, tstring& s2) { return Util::defaultSort(s1, s2, true) < 0; });
	int selectedItem = -1;
	for (size_t i = 0; i < sortedList.size(); i++)
	{
		ctrlIgnored.insert(i, sortedList[i]);
		if (sortedList[i] == selectedIgnore)
			selectedItem = i;
	}
	if (selectedItem != -1)
		ctrlIgnored.SelectItem(selectedItem);
	updateIgnoreListButtons();
}

void UsersFrame::updateIgnoreListButtons()
{
	ctrlIgnoreRemove.EnableWindow(ctrlIgnored.GetNextItem(-1, LVNI_SELECTED) != -1);
	ctrlIgnoreClear.EnableWindow(ctrlIgnored.GetItemCount() != 0);
}

LRESULT UsersFrame::onIgnoreAdd(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	tstring name;
	WinUtil::getWindowText(ctrlIgnoreName, name);
	if (name.empty()) return 0;
	ctrlIgnoreName.SetWindowText(_T(""));
	tstring prevIgnore = std::move(selectedIgnore);
	selectedIgnore = name;
	if (!UserManager::getInstance()->addToIgnoreList(Text::fromT(name)))
	{
		selectedIgnore = std::move(prevIgnore);
		MessageBox(CTSTRING(ALREADY_IGNORED), getAppNameVerT().c_str(), MB_OK);
		return 0;
	}
	return 0;
}

LRESULT UsersFrame::onIgnoreRemove(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	vector<string> userNames;
	int i = -1;
	while ((i = ctrlIgnored.GetNextItem(i, LVNI_SELECTED)) != -1)
		userNames.push_back(ctrlIgnored.ExGetItemText(i, 0));
	if (!userNames.empty())
		UserManager::getInstance()->removeFromIgnoreList(userNames);
	return 0;
}

LRESULT UsersFrame::onIgnoreClear(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	if (MessageBox(CTSTRING(CLEAR_LIST_OF_IGNORED_USERS), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		UserManager::getInstance()->clearIgnoreList();
	}	
	return 0;
}

void UsersFrame::on(IgnoreListChanged, const string&) noexcept
{
	ctrlIgnored.DeleteAllItems();
	insertIgnoreList();
}

void UsersFrame::on(IgnoreListCleared) noexcept
{
	ctrlIgnored.DeleteAllItems();
	insertIgnoreList();
}

CFrameWndClassInfo& UsersFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("UsersFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::FAVORITE_USERS, 0);

	return wc;
}
