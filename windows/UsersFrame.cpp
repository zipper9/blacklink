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
#include "FavUserDlg.h"
#include "HubFrame.h"
#include "ResourceLoader.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "ExMessageBox.h"
#include "../client/Util.h"
#include "../client/UserManager.h"
#include <algorithm>

const int UsersFrame::columnId[] =
{
	COLUMN_NICK,
	COLUMN_HUB,
	COLUMN_SEEN,
	COLUMN_DESCRIPTION,
	COLUMN_SPEED_LIMIT,
	COLUMN_PM_HANDLING,
	COLUMN_SHARE_GROUP,
	COLUMN_SLOTS,
	COLUMN_CID
};

static const int columnSizes[] = { 200, 300, 150, 200, 100, 100, 100, 100, 300 };

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::AUTO_GRANT_NICK,
	ResourceManager::LAST_HUB,
	ResourceManager::LAST_SEEN,
	ResourceManager::DESCRIPTION,
	ResourceManager::UPLOAD_SPEED_LIMIT,
	ResourceManager::PM_HANDLING,
	ResourceManager::SHARE_GROUP,
	ResourceManager::SLOTS,
	ResourceManager::CID
};

static tstring formatLastSeenTime(time_t t)
{
	if (!t) return Util::emptyStringT;
	return Text::toT(Util::formatDateTime(t));
}

static int getDefaultCommand()
{
	static const int cmd[] = { IDC_GETLIST, IDC_PRIVATE_MESSAGE, IDC_MATCH_QUEUE, IDC_EDIT, IDC_OPEN_USER_LOG };
	int action = SETTING(FAVUSERLIST_DBLCLICK);
	if (action < 0 || action > (int) _countof(cmd)) return 0;
	return cmd[action];
}

UsersFrame::UsersFrame() : startup(true), barHeight(0)
{
	ctrlUsers.setColumns(_countof(columnId), columnId, columnNames, columnSizes);	
	ctrlUsers.setColumnFormat(COLUMN_SPEED_LIMIT, LVCFMT_RIGHT);
	ctrlUsers.setColumnFormat(COLUMN_SLOTS, LVCFMT_RIGHT);
}

LRESULT UsersFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_USERS);
	ctrlUsers.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	ctrlUsers.SetImageList(g_favUserImage.getIconList(), LVSIL_SMALL);
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

	ctrlIgnored.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CONTROLPARENT);
	ctrlShowIgnored.Create(m_hWnd, rcDefault, CTSTRING(SHOW_IGNORED_USERS), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_AUTOCHECKBOX | BS_VCENTER, 0, IDC_SHOW_IGNORED);
	ctrlShowIgnored.SetFont(Fonts::g_systemFont);

	m_nProportionalPos = SETTING(USERS_FRAME_SPLIT);
	SetSplitterPanes(ctrlUsers, ctrlIgnored, false);
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);

	int showIgnored = SETTING(SHOW_IGNORED_USERS);
	if (showIgnored == -1)
		showIgnored = ctrlIgnored.getCount() ? 1 : 0;
	ctrlShowIgnored.SetCheck(showIgnored ? BST_CHECKED : BST_UNCHECKED);
	if (!showIgnored)
		SetSinglePaneMode(SPLIT_PANE_LEFT);
	
	startup = false;
	bHandled = FALSE;
	updateLayout();
	return TRUE;
}

LRESULT UsersFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);

	bHandled = FALSE;
	return 0;
}

void UsersFrame::GetSystemSettings(bool bUpdate)
{
	splitBase::GetSystemSettings(bUpdate);
	m_cxyMin = ctrlIgnored.getMinWidth();
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
		usersMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES), g_iconBitmaps.getBitmap(IconBitmaps::PROPERTIES, 0));
		
		if (ctrlUsers.GetSelectedCount() == 1)
		{
			int index = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
			const auto ii = ctrlUsers.getItemData(index);
			const UserPtr& user = ii->getUser();
			usersMenu.AppendMenu(MF_SEPARATOR);
			tstring nick = Text::toT(user->getLastNick());
			reinitUserMenu(user, ii->getHubHint());
			if (!copyMenu)
			{
				copyMenu.CreatePopupMenu();
				for (int i = 0; i < _countof(columnId); ++i)
					copyMenu.AppendMenu(MF_STRING, IDC_COPY + columnId[i],
						i == 0 ? CTSTRING(NICK) : CTSTRING_I(columnNames[i]));
			}
			int copyIndex = 5;
			if (!nick.empty())
			{
				usersMenu.InsertSeparatorFirst(nick);
				copyIndex++;
			}
			appendAndActivateUserItems(usersMenu);
			MENUITEMINFO mii = { sizeof(mii) };
			mii.fMask = MIIM_STRING | MIIM_SUBMENU;
			mii.fType = MFT_STRING;
			mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(COPY));
			mii.hSubMenu = copyMenu;
			usersMenu.InsertMenuItem(copyIndex, TRUE, &mii);
			usersMenu.SetBitmap(copyIndex, TRUE, g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
			mii.fMask = MIIM_FTYPE;
			mii.fType = MFT_SEPARATOR;
			usersMenu.InsertMenuItem(copyIndex + 1, TRUE, &mii);
			if (FavoriteManager::getInstance()->getUserUrl(user).empty())
				usersMenu.EnableMenuItem(IDC_CONNECT, MFS_GRAYED);
		}
		else
		{
			usersMenu.AppendMenu(MF_STRING, IDC_REMOVE_FROM_FAVORITES, CTSTRING(REMOVE_FROM_FAVORITES), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE_USER, 0));
		}
		int defaultCommand = getDefaultCommand();
		if (defaultCommand)
			usersMenu.SetMenuDefaultItem(defaultCommand);
		usersMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		WinUtil::unlinkStaticMenus(usersMenu);
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT UsersFrame::onCopy(WORD, WORD wID, HWND, BOOL&)
{
	int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
	if (i == -1) return 0;
	int columnId = wID - IDC_COPY;
	const ItemInfo* ii = ctrlUsers.getItemData(i);
	tstring data = ii->getText(columnId);
	if (!data.empty())
		WinUtil::setClipboard(ii->getText(columnId));
	return 0;
}

LRESULT UsersFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::FAVORITE_USERS, 0);
	opt->isHub = false;
	return TRUE;
}

void UsersFrame::updateLayout()
{
	WINDOWPLACEMENT wp = { sizeof(wp) };
	GetWindowPlacement(&wp);
	int splitBarHeight = wp.showCmd == SW_MAXIMIZE && BOOLSETTING(SHOW_TRANSFERVIEW) ? GetSystemMetrics(SM_CYSIZEFRAME) : 0;
	if (!barHeight)
	{
		int xdu, ydu;
		WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
		checkBoxSize.cx = WinUtil::dialogUnitsToPixelsX(12, xdu) + WinUtil::getTextWidth(TSTRING(SHOW_IGNORED_USERS), ctrlShowIgnored);
		checkBoxSize.cy = WinUtil::dialogUnitsToPixelsY(8, ydu);
		checkBoxXOffset = WinUtil::dialogUnitsToPixelsX(2, xdu);
		checkBoxYOffset = std::max(WinUtil::dialogUnitsToPixelsY(2, ydu), GetSystemMetrics(SM_CYSIZEFRAME));
		barHeight = checkBoxSize.cy + 2*checkBoxYOffset;
	}

	RECT rect, rect2;
	GetClientRect(&rect);
	rect2 = rect;
	rect2.bottom = rect.bottom - (barHeight - splitBarHeight);
	
	CRect rc = rect2;
	SetSplitterRect(rc);

	rc.left = checkBoxXOffset;
	rc.top = rect2.bottom + checkBoxYOffset;
	rc.right = rc.left + checkBoxSize.cx;
	rc.bottom = rc.top + checkBoxSize.cy;
	ctrlShowIgnored.MoveWindow(rc);
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
		FavUserDlg dlg;
		if (count == 1)
		{
			int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
			const ItemInfo* ii = ctrlUsers.getItemData(i);
			dlg.title = TSTRING_F(SET_PROPERTIES_FOR_USER, ii->getText(COLUMN_NICK));
			dlg.description = ii->getText(COLUMN_DESCRIPTION);
			dlg.flags = ii->flags;
			dlg.speedLimit = ii->speedLimit;
			dlg.shareGroup = ii->shareGroup;
		}
		else
		{
			dlg.title = TSTRING_F(SET_PROPERTIES_FOR_USERS, count);
		}
		if (dlg.DoModal(m_hWnd) != IDOK) return 0;

		auto fm = FavoriteManager::getInstance();
		string description = Text::fromT(dlg.description);
		int i = -1;
		while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			ItemInfo* ii = ctrlUsers.getItemData(i);
			fm->setUserAttributes(ii->getUser(), FavoriteUser::Flags(dlg.flags), dlg.speedLimit, dlg.shareGroup, description);
			updateUser(ii->getUser());
			ctrlUsers.SetCheckState(i, (dlg.flags & FavoriteUser::FLAG_GRANT_SLOT) ? TRUE : FALSE);
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

	bHandled = FALSE;
	if (item->iItem != -1)
	{
		const ItemInfo* ii = ctrlUsers.getItemData(item->iItem);
		if (ii)
		{
			bHandled = TRUE;
			int command = getDefaultCommand();
			if (command)
			{
				reinitUserMenu(ii->getUser(), ii->getHubHint());
				PostMessage(WM_COMMAND, command, 0);
			}
		}
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
	auto ii = new ItemInfo(user);
	int i = ctrlUsers.insertItem(ii, 0);
	bool b = user.isSet(FavoriteUser::FLAG_GRANT_SLOT);
	ctrlUsers.SetCheckState(i, b);
	updateUser(i, ii, user);
}

void UsersFrame::updateUser(const UserPtr& user)
{
	const int count = ctrlUsers.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		ItemInfo *ii = ctrlUsers.getItemData(i);
		if (ii->getUser() == user)
		{
			FavoriteUser currentFavUser;
			if (FavoriteManager::getInstance()->getFavoriteUser(user, currentFavUser))
				updateUser(i, ii, currentFavUser);
		}
	}
}

void UsersFrame::updateUser(const int i, ItemInfo* ii, const FavoriteUser& favUser)
{
	const auto flags = favUser.user->getFlags();
	ii->columns[COLUMN_SEEN] = (flags & User::ONLINE) ? TSTRING(ONLINE) : formatLastSeenTime(favUser.lastSeen);
		
	int imageIndex;
	if (flags & User::ONLINE)
		imageIndex = 0;
	else
		imageIndex = 2;

	if (favUser.uploadLimit == FavoriteUser::UL_BAN || favUser.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
		imageIndex += 3;
		
	ii->update(favUser);
		
	ctrlUsers.SetItem(i, 0, LVIF_IMAGE, NULL, imageIndex, 0, 0, NULL);

	ctrlUsers.updateItem(i);
	setCountMessages(ctrlUsers.GetItemCount());
}

void UsersFrame::removeUser(const FavoriteUser& user)
{
	const int count = ctrlUsers.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		ItemInfo *ii = ctrlUsers.getItemData(i);
		if (ii->getUser() == user.user)
		{
			ctrlUsers.DeleteItem(i);
			delete ii;
			setCountMessages(ctrlUsers.GetItemCount());
			return;
		}
	}
}

void UsersFrame::showUser(const CID& cid)
{
	const int count = ctrlUsers.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		ItemInfo *ii = ctrlUsers.getItemData(i);
		if (ii->getUser()->getCID() == cid)
		{
			ctrlUsers.SelectItem(i);
			ctrlUsers.EnsureVisible(i, FALSE);
			return;
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
		setButtonPressed(IDC_FAVUSERS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlUsers.saveHeaderOrder(SettingsManager::USERS_FRAME_ORDER, SettingsManager::USERS_FRAME_WIDTHS, SettingsManager::USERS_FRAME_VISIBLE);
		SET_SETTING(USERS_FRAME_SORT, ctrlUsers.getSortForSettings());
		ctrlUsers.deleteAll();
		
		SET_SETTING(USERS_FRAME_SPLIT, m_nProportionalPos);
		bHandled = FALSE;
		return 0;
	}
}

void UsersFrame::ItemInfo::update(const FavoriteUser& u)
{
	bool isOnline = user->isOnline();
	lastSeen = u.lastSeen;
	speedLimit = u.uploadLimit;
	shareGroup = u.shareGroup;
	flags = u.getFlags();
	columns[COLUMN_NICK] = Text::toT(u.nick);
	columns[COLUMN_HUB] = Text::toT(WinUtil::getHubDisplayName(u.url));
	columns[COLUMN_SEEN] = isOnline ? TSTRING(ONLINE) : formatLastSeenTime(lastSeen);
	columns[COLUMN_DESCRIPTION] = Text::toT(u.description);
	columns[COLUMN_SLOTS] = Util::toStringT(u.user->getSlots());
	columns[COLUMN_CID] = Text::toT(u.user->getCID().toBase32());
	if (flags & FavoriteUser::FLAG_IGNORE_PRIVATE)
		columns[COLUMN_PM_HANDLING] = TSTRING(IGNORE_PRIVATE);
	else if (flags & FavoriteUser::FLAG_FREE_PM_ACCESS)
		columns[COLUMN_PM_HANDLING] = TSTRING(FREE_PM_ACCESS);
	else
		columns[COLUMN_PM_HANDLING].clear();
	columns[COLUMN_SPEED_LIMIT] = Text::toT(UserInfo::getSpeedLimitText(speedLimit));
	if (flags & FavoriteUser::FLAG_HIDE_SHARE)
		columns[COLUMN_SHARE_GROUP] = TSTRING(SHARE_GROUP_NOTHING);
	else
	if (!u.shareGroup.isZero())
	{
		string name;
		ShareManager::getInstance()->getShareGroupName(u.shareGroup, name);
		columns[COLUMN_SHARE_GROUP] = Text::toT(name);
	}
	else
		columns[COLUMN_SHARE_GROUP].clear();
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

void UsersFrame::getSelectedUsers(vector<UserPtr>& v) const
{
	int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
	while (i >= 0)
	{
		ItemInfo* ii = ctrlUsers.getItemData(i);
		v.push_back(ii->getUser());
		i = ctrlUsers.GetNextItem(i, LVNI_SELECTED);
	}
}

void UsersFrame::openUserLog()
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
}

void UsersFrame::on(UserAdded, const FavoriteUser& user) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	addUser(user);
}

void UsersFrame::on(UserRemoved, const FavoriteUser& user) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		removeUser(user);
	}
}

void UsersFrame::on(UserStatusChanged, const UserPtr& user) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		WinUtil::postSpeakerMsg(*this, USER_UPDATED, new UserPtr(user));
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
	auto fm = FavoriteManager::getInstance();
	int i = -1;
	while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo *ii = ctrlUsers.getItemData(i);
		FavoriteUser::Flags flag = FavoriteUser::FLAG_NONE;
		switch (wID)
		{
			case IDC_PM_IGNORED:
				ii->columns[COLUMN_PM_HANDLING] = TSTRING(IGNORE_PRIVATE);
				flag = FavoriteUser::FLAG_IGNORE_PRIVATE;
				break;
			case IDC_PM_FREE:
				ii->columns[COLUMN_PM_HANDLING] = TSTRING(FREE_PM_ACCESS);
				flag = FavoriteUser::FLAG_FREE_PM_ACCESS;
				break;
			default:
				ii->columns[COLUMN_PM_HANDLING].clear();
		}
		ii->flags = (ii->flags & ~FavoriteUser::PM_FLAGS_MASK) | flag;
		fm->setFlags(ii->getUser(), flag, FavoriteUser::PM_FLAGS_MASK, false);
		updateUser(ii->getUser());
		ctrlUsers.updateItem(i);
	}
	return 0;
}

LRESULT UsersFrame::onToggleIgnored(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	BOOL state = CButton(hWndCtl).GetCheck() == BST_CHECKED;
	SetSinglePaneMode(state ? SPLIT_PANE_NONE : SPLIT_PANE_LEFT);
	SET_SETTING(SHOW_IGNORED_USERS, state ? 1 : 0);
	return 0;
}

void UsersFrame::on(IgnoreListChanged) noexcept
{
	ctrlIgnored.updateUsers();
}

void UsersFrame::on(IgnoreListCleared) noexcept
{
	ctrlIgnored.updateUsers();
}

BOOL UsersFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::g_tabCtrl->isActive(m_hWnd)) return FALSE;
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	if (WinUtil::isCtrl()) return FALSE;
	return IsDialogMessage(pMsg);
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
