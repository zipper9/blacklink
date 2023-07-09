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
#include "../client/ClientManager.h"
#include "../client/QueueManager.h"
#include "WaitingUsersFrame.h"
#include "MainFrm.h"
#include "BarShader.h"
#include "ImageLists.h"
#include "Colors.h"

static const unsigned TIMER_VAL = 1000;
static const int STATUS_PART_PADDING = 12;

static const int columnSizes[] =
{
	200, // COLUMN_FILE
	60,  // COLUMN_TYPE
	300, // COLUMN_PATH
	80,  // COLUMN_NICK
	130, // COLUMN_HUB
	100, // COLUMN_TRANSFERRED
	85,  // COLUMN_SIZE
	120, // COLUMN_ADDED
	85,  // COLUMN_WAITING
	120, // COLUMN_LOCATION
	100, // COLUMN_IP
	40,  // COLUMN_SLOTS
	85   // COLUMN_SHARE
};

const int WaitingUsersFrame::columnId[] =
{
	UploadQueueItem::COLUMN_FILE,
	UploadQueueItem::COLUMN_TYPE,
	UploadQueueItem::COLUMN_PATH,
	UploadQueueItem::COLUMN_NICK,
	UploadQueueItem::COLUMN_HUB,
	UploadQueueItem::COLUMN_TRANSFERRED,
	UploadQueueItem::COLUMN_SIZE,
	UploadQueueItem::COLUMN_ADDED,
	UploadQueueItem::COLUMN_WAITING,
	UploadQueueItem::COLUMN_LOCATION,
	UploadQueueItem::COLUMN_IP,
	UploadQueueItem::COLUMN_SLOTS,
	UploadQueueItem::COLUMN_SHARE
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::FILENAME,
	ResourceManager::TYPE,
	ResourceManager::PATH,
	ResourceManager::NICK,
	ResourceManager::HUB,
	ResourceManager::TRANSFERRED,
	ResourceManager::SIZE,
	ResourceManager::ADDED,
	ResourceManager::WAITING_TIME,
	ResourceManager::LOCATION_BARE,
	ResourceManager::IP,
	ResourceManager::SLOTS,
	ResourceManager::SHARED
};

WaitingUsersFrame::WaitingUsersFrame() :
	timer(m_hWnd),
	shouldUpdateStatus(false), shouldSort(false),
	treeRoot(nullptr)
{
	++UploadManager::g_count_WaitingUsersFrame;
	memset(statusSizes, 0, sizeof(statusSizes));
	ctrlList.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlList.setColumnFormat(UploadQueueItem::COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(UploadQueueItem::COLUMN_WAITING, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(UploadQueueItem::COLUMN_SLOTS, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(UploadQueueItem::COLUMN_SHARE, LVCFMT_RIGHT);
}

WaitingUsersFrame::~WaitingUsersFrame()
{
	--UploadManager::g_count_WaitingUsersFrame;
}

LRESULT WaitingUsersFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	initProgressBar(false);

	// status bar
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_UPLOAD_QUEUE);
	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	if (WinUtil::setExplorerTheme(ctrlList))
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED | CustomDrawHelpers::FLAG_USE_HOT_ITEM;

	ctrlQueued.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WinUtil::getTreeViewStyle(),
	                  WS_EX_CLIENTEDGE, IDC_USERS);
	WinUtil::setExplorerTheme(ctrlQueued);
	                  
	ctrlQueued.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	
	m_nProportionalPos = SETTING(UPLOAD_QUEUE_FRAME_SPLIT);
	SetSplitterPanes(ctrlQueued.m_hWnd, ctrlList.m_hWnd);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));

	ctrlList.insertColumns(SettingsManager::UPLOAD_QUEUE_FRAME_ORDER, SettingsManager::UPLOAD_QUEUE_FRAME_WIDTHS, SettingsManager::UPLOAD_QUEUE_FRAME_VISIBLE);
	ctrlList.setSortFromSettings(SETTING(UPLOAD_QUEUE_FRAME_SORT));
	
	// colors
	setListViewColors(ctrlList);
	setTreeViewColors(ctrlQueued);

	memset(statusSizes, 0, sizeof(statusSizes));
	statusSizes[0] = 16;
	ctrlStatus.SetParts(4, statusSizes);
	UpdateLayout();
	
	UploadManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);

	loadAll();
	timer.createTimer(TIMER_VAL);
	bHandled = FALSE;
	return TRUE;
}

LRESULT WaitingUsersFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (copyMenu)
	{
		MenuHelper::removeStaticMenu(copyMenu);
		copyMenu.DestroyMenu();
	}
	bHandled = FALSE;
	return 0;
}

LRESULT WaitingUsersFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	timer.destroyTimer();
	tasks.setDisabled(true);

	if (!closed)
	{
		closed = true;
		UploadManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		setButtonPressed(IDC_UPLOAD_QUEUE, false);
		
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlQueued.DeleteAllItems();
		ctrlList.deleteAll();
		userList.clear();
		ctrlList.saveHeaderOrder(SettingsManager::UPLOAD_QUEUE_FRAME_ORDER, SettingsManager::UPLOAD_QUEUE_FRAME_WIDTHS,
		                         SettingsManager::UPLOAD_QUEUE_FRAME_VISIBLE);
		                           
		SET_SETTING(UPLOAD_QUEUE_FRAME_SORT, ctrlList.getSortForSettings());
		SET_SETTING(UPLOAD_QUEUE_FRAME_SPLIT, m_nProportionalPos);
		
		tasks.clear();
		bHandled = FALSE;
		return 0;
	}
}

void WaitingUsersFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[4];
		ctrlStatus.GetClientRect(sr);
		w[3] = sr.right - 16;
#define setw(x) w[x] = max(w[x+1] - statusSizes[x], 0)
		setw(2);
		setw(1);
#undef setw
		w[0] = 36;
		
		ctrlStatus.SetParts(4, w);
	}
	CRect rc = rect;
	SetSplitterRect(rc);
}

LRESULT WaitingUsersFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (getSelectedUser())
	{
		const UserPtr User = getCurrentUser();
		if (User)
		{
			UploadManager::LockInstanceQueue lockedInstance;
			lockedInstance->clearUserFilesL(User);
		}
	}
	else
	{
		if (ctrlList.getSelectedCount())
		{
			int j = -1;
			UserList removeUsers;
			while ((j = ctrlList.GetNextItem(j, LVNI_SELECTED)) != -1)
			{
				// Ok let's cheat here, if you try to remove more users here is not working :(
				removeUsers.push_back(ctrlList.getItemData(j)->getUser());
			}
			UploadManager::LockInstanceQueue lockedInstance;
			for (auto i = removeUsers.cbegin(); i != removeUsers.cend(); ++i)
			{
				lockedInstance->clearUserFilesL(*i);
			}
		}
	}
	shouldUpdateStatus = true;
	return 0;
}

LRESULT WaitingUsersFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	RECT rc;
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	ctrlList.GetHeader().GetWindowRect(&rc);
	if (PtInRect(&rc, pt))
	{
		ctrlList.showMenu(pt);
		return TRUE;
	}
	
	// Create context menu
	OMenu contextMenu;
	contextMenu.CreatePopupMenu();
	clearUserMenu();
	
	if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() == 1)
	{
		if (pt.x == -1 && pt.y == -1)
			WinUtil::getContextMenuPos(ctrlList, pt);

		int j = ctrlList.GetNextItem(-1, LVNI_SELECTED);
		if (j == -1) return FALSE;
		UploadQueueItem* ui = ctrlList.getItemData(j);

		if (!copyMenu)
		{
			copyMenu.CreatePopupMenu();
			MenuHelper::addStaticMenu(copyMenu);
			for (int i = 0; i < _countof(columnId); ++i)
				copyMenu.AppendMenu(MF_STRING, IDC_COPY + columnId[i], CTSTRING_I(columnNames[i]));
		}

		reinitUserMenu(ui->getUser(), ui->getHint());
		appendAndActivateUserItems(contextMenu);
		int copyIndex = 3;
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_STRING | MIIM_SUBMENU;
		mii.fType = MFT_STRING;
		mii.dwTypeData = const_cast<TCHAR*>(CTSTRING(COPY));
		mii.hSubMenu = copyMenu;
		contextMenu.InsertMenuItem(copyIndex, TRUE, &mii);
		contextMenu.SetBitmap(copyIndex, TRUE, g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
		mii.fMask = MIIM_FTYPE;
		mii.fType = MFT_SEPARATOR;
		contextMenu.InsertMenuItem(copyIndex + 1, TRUE, &mii);
		contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		MenuHelper::unlinkStaticMenus(contextMenu);
		return TRUE;
	}
	else if (reinterpret_cast<HWND>(wParam) == ctrlQueued && ctrlQueued.GetSelectedItem() != NULL)
	{
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlQueued, pt);
		}
		else
		{
			UINT a = 0;
			ctrlQueued.ScreenToClient(&pt);
			HTREEITEM ht = ctrlQueued.HitTest(pt, &a);
			if (ht != NULL && ht != ctrlQueued.GetSelectedItem())
				ctrlQueued.SelectItem(ht);
				
			ctrlQueued.ClientToScreen(&pt);
		}
		
		HTREEITEM selectedItem = ctrlQueued.GetSelectedItem();
		if (!selectedItem || selectedItem == treeRoot) return FALSE;
		UserItem* ui = reinterpret_cast<UserItem*>(ctrlQueued.GetItemData(selectedItem));
		if (!ui) return FALSE;

		reinitUserMenu(ui->hintedUser.user, ui->hintedUser.hint);
		appendAndActivateUserItems(contextMenu);
		contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		MenuHelper::unlinkStaticMenus(contextMenu);
		return TRUE;
	}
	return FALSE;
}

LRESULT WaitingUsersFrame::onCopy(WORD, WORD wID, HWND, BOOL&)
{
	int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	if (i == -1) return 0;
	int columnId = wID - IDC_COPY;
	const UploadQueueItem* ui = ctrlList.getItemData(i);
	const tstring& data = ui->getText(columnId);
	if (!data.empty())
		WinUtil::setClipboard(data);
	return 0;
}

LRESULT WaitingUsersFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::UPLOAD_QUEUE, 0);
	opt->isHub = false;
	return TRUE;
}

void WaitingUsersFrame::loadFiles(const WaitingUser& wu)
{
	for (const UploadQueueFilePtr& uqi : wu.getWaitingFiles())
		addFile(wu.getHintedUser(), uqi, false);
}

void WaitingUsersFrame::loadAll()
{
	CLockRedraw<> lockRedraw(ctrlList);
	CLockRedraw<true> lockRedrawQueued(ctrlQueued);
	
	userList.clear();
	ctrlList.deleteAllNoLock();

	ctrlQueued.DeleteAllItems();
	treeRoot = ctrlQueued.InsertItem(TVIF_TEXT | TVIF_PARAM, CTSTRING(ALL_USERS), 0, 0, 0, 0, NULL, NULL, TVI_LAST);

	// Load queue
	{
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		for (const WaitingUser& wu : users)
		{
			tstring text = Text::toT(wu.getUser()->getLastNick()) + _T(" - ") + WinUtil::getHubNames(wu.getHintedUser()).first;
			HTREEITEM treeItem = ctrlQueued.InsertItem(TVIF_PARAM | TVIF_TEXT,
				text.c_str(), 0, 0, 0, 0,
				reinterpret_cast<LPARAM>(new UserItem(wu.getHintedUser())), treeRoot, TVI_LAST);
			userList.emplace_back(wu.getHintedUser(), treeItem);
		}
	}
	shouldSort = true;
	shouldUpdateStatus = true;
}

void WaitingUsersFrame::removeUser(const UserPtr& user)
{
	HTREEITEM userNode = nullptr;
	for (auto i = userList.cbegin(); i != userList.cend(); ++i)
	{
		if (i->user == user)
		{
			userNode = i->treeItem;
			userList.erase(i);
			shouldUpdateStatus = true;
			break;
		}
	}
	
	if (userNode) ctrlQueued.DeleteItem(userNode);
}

LRESULT WaitingUsersFrame::onItemChanged(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	HTREEITEM userNode = ctrlQueued.GetSelectedItem();
	if (!userNode) return 0;

	CLockRedraw<> lockRedraw(ctrlList);

	if (userNode == treeRoot)
	{
		ctrlList.deleteAll();
		shouldSort = false;
		shouldUpdateStatus = true;
		return 0;
	}
	
	UserItem* ui = reinterpret_cast<UserItem*>(ctrlQueued.GetItemData(userNode));
	dcassert(ui);
	{
		ctrlList.deleteAll();
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		for (const WaitingUser& wu : users)
			if (wu.getHintedUser().equals(ui->hintedUser))
			{
				loadFiles(wu);
				shouldSort = shouldUpdateStatus = true;
				break;
			}
	}
	return 0;
}

LRESULT WaitingUsersFrame::onTreeItemDeleted(int, LPNMHDR pnmh, BOOL&)
{
	const NMTREEVIEW* p = (const NMTREEVIEW *) pnmh;
	delete reinterpret_cast<const UserItem*>(p->itemOld.lParam);
	return 0;
}

void WaitingUsersFrame::addFile(const HintedUser& hintedUser, const UploadQueueFilePtr& uqi, bool addUser)
{
	dcassert(uqi != nullptr);
	
	if (addUser)
	{
		for (auto i = userList.cbegin(); i != userList.cend(); ++i)
		{
			if (i->user == hintedUser.user)
			{
				addUser = false;
				break;
			}
		}
	}
	if (addUser)
	{
		tstring text;
		OnlineUserPtr ou = ClientManager::findOnlineUser(hintedUser.user->getCID(), hintedUser.hint, true);
		if (ou)
			text = Text::toT(ou->getIdentity().getNick() + " - " + ou->getClientBase()->getHubName());
		else
			text = Text::toT(hintedUser.user->getLastNick() + " - " + hintedUser.hint);
		HTREEITEM treeItem = ctrlQueued.InsertItem(TVIF_PARAM | TVIF_TEXT, text.c_str(), 0, 0, 0, 0, reinterpret_cast<LPARAM>(new UserItem(hintedUser)), treeRoot, TVI_LAST);
		userList.emplace_back(hintedUser.user, treeItem);
	}
	HTREEITEM selNode = ctrlQueued.GetSelectedItem();
	if (!selNode)
		return;
	UserItem* treeItem = reinterpret_cast<UserItem*>(ctrlQueued.GetItemData(selNode));
	if (!treeItem || treeItem->hintedUser.user != hintedUser.user)
		return;
	UploadQueueItem* ui = new UploadQueueItem(hintedUser, uqi);
	ui->update();
	int imageIndex = g_fileImage.getIconIndex(uqi->getFile());
	ui->setImageIndex(imageIndex);
	ctrlList.insertItem(ctrlList.GetItemCount(), ui, I_IMAGECALLBACK);
}

void WaitingUsersFrame::updateStatus()
{
	if (ctrlStatus)
	{
		const int cnt = ctrlList.GetItemCount();
		const int users = ctrlQueued.GetCount() - 1;

		tstring tmp[2];
		tmp[0] = TPLURAL_F(PLURAL_USERS, users);
		tmp[1] = TPLURAL_F(PLURAL_ITEMS, cnt);
		bool u = false;

		for (int i = 1; i < 3; i++)
		{
			const int w = WinUtil::getTextWidth(tmp[i - 1], ctrlStatus) + STATUS_PART_PADDING;
			if (statusSizes[i] < w)
			{
				statusSizes[i] = w;
				u = true;
			}
			ctrlStatus.SetText(i + 1, tmp[i - 1].c_str());
		}

		if (u) UpdateLayout(TRUE);
		setCountMessages(ctrlList.GetItemCount());
	}
}

void WaitingUsersFrame::removeSelected()
{
	int j = -1;
	UserList RemoveUsers;
	while ((j = ctrlList.GetNextItem(j, LVNI_SELECTED)) != -1)
	{
		// Ok let's cheat here, if you try to remove more users here is not working :(
		RemoveUsers.push_back(ctrlList.getItemData(j)->getUser());
	}
	{
		UploadManager::LockInstanceQueue lockedInstance;
		for (auto i = RemoveUsers.cbegin(); i != RemoveUsers.cend(); ++i)
		{
			lockedInstance->clearUserFilesL(*i);
		}
	}
	shouldUpdateStatus = true;
}

void WaitingUsersFrame::addTask(Tasks s, Task* task)
{
	bool firstItem;
	uint64_t tick = GET_TICK();
	uint64_t prevTick = tick;
	if (tasks.add(s, task, firstItem, prevTick) && prevTick + TIMER_VAL < tick)
		PostMessage(WM_SPEAKER);
}

void WaitingUsersFrame::processTasks()
{
	TaskQueue::List t;
	tasks.get(t);
	if (t.empty()) return;
		
	CLockRedraw<> lockCtrlList(ctrlList);
	CLockRedraw<> lockCtrlQueued(ctrlQueued);
	for (auto j = t.cbegin(); j != t.cend(); ++j)
	{
		switch (j->first)
		{
			case REMOVE_USER:
				removeUser(static_cast<UserTask&>(*j->second).user);
				break;

			case ADD_FILE:
			{
				auto uqt = static_cast<UploadQueueTask*>(j->second);
				addFile(uqt->hintedUser, uqt->item, true);
				shouldSort = true;
				break;
			}

			case UPDATE_ITEMS:
			{
				const int itemCount = ctrlList.GetItemCount();
				if (itemCount)
				{
					const int topIndex = ctrlList.GetTopIndex();
					const int countPerPage = ctrlList.GetCountPerPage();
					int64_t itime = GET_TIME();
					for (int i = topIndex; i < countPerPage; i++)
					{
						auto ii = ctrlList.getItemData(i);
						if (ii)
						{
							const UploadQueueFilePtr& file = ii->getFile();
							ii->setText(UploadQueueItem::COLUMN_TRANSFERRED, Util::formatBytesT(file->getPos()) + _T(" (") + Util::toStringT((double) file->getPos() * 100.0 / (double) file->getSize()) + _T("%)"));
							ii->setText(UploadQueueItem::COLUMN_WAITING, Util::formatSecondsT(itime - file->getTime()));
							ctrlList.updateItem(i, UploadQueueItem::COLUMN_TRANSFERRED);
							ctrlList.updateItem(i, UploadQueueItem::COLUMN_WAITING);
						}
					}
				}
				if (shouldSort)
				{
					ctrlList.resort();
					shouldSort = false;
				}
				if (shouldUpdateStatus)
				{
					if (BOOLSETTING(BOLD_WAITING_USERS))
						setDirty();
					updateStatus();
					shouldUpdateStatus = false;
				}
			}
			break;

			default:
				dcassert(0);
		}
		if (j->first != UPDATE_ITEMS)
		{
			shouldUpdateStatus = true;
		}
		delete j->second;
	}
}

void WaitingUsersFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		initProgressBar(true);
		if (ctrlList.isRedraw())
		{
			setTreeViewColors(ctrlQueued);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

void WaitingUsersFrame::on(UploadManagerListener::QueueUpdate) noexcept
{
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown())
		addTask(UPDATE_ITEMS, nullptr);
}

LRESULT WaitingUsersFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	CRect rc;
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	UploadQueueItem *ii = (UploadQueueItem*)cd->nmcd.lItemlParam; // ??
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			CustomDrawHelpers::startDraw(customDrawState, cd);
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
			CustomDrawHelpers::startItemDraw(customDrawState, cd);
			return CDRF_NOTIFYSUBITEMDRAW;
			
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			int column = ctrlList.findColumn(cd->iSubItem);
			if (column == UploadQueueItem::COLUMN_TRANSFERRED && BOOLSETTING(SHOW_PROGRESS_BARS))
			{
				const tstring& text = ii->getText(UploadQueueItem::COLUMN_TRANSFERRED);
				ctrlList.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				// Real rc, the original one.
				CRect real_rc = rc;
				// We need to offset the current rc to (0, 0) to paint on the New dc
				rc.MoveToXY(0, 0);

				CDC cdc;
				cdc.CreateCompatibleDC(cd->nmcd.hdc);
				HBITMAP hBmp = CreateCompatibleBitmap(cd->nmcd.hdc, real_rc.Width(), real_rc.Height());
				HBITMAP pOldBmp = cdc.SelectBitmap(hBmp);
				HDC dc = cdc.m_hDC;

				const UploadQueueFilePtr& file = ii->getFile();
				auto size = file->getSize();
				auto pos = file->getPos();
				int width;
				if (pos >= size || !size)
					width = rc.right;
				else
					width = static_cast<int>(rc.right*pos/size);

				progressBar.draw(dc, rc, width, text, -1);

				BitBlt(cd->nmcd.hdc, real_rc.left, real_rc.top, real_rc.Width(), real_rc.Height(), dc, 0, 0, SRCCOPY);
				ATLVERIFY(cdc.SelectBitmap(pOldBmp) == hBmp);
				DeleteObject(hBmp);
				return CDRF_SKIPDEFAULT;
			}
			if (column == UploadQueueItem::COLUMN_LOCATION)
			{
				const tstring& text = ii->getText(UploadQueueItem::COLUMN_LOCATION);
				if (!text.empty())
				{
					CustomDrawHelpers::drawLocation(customDrawState, cd, ii->ipInfo);
					return CDRF_SKIPDEFAULT;
				}
			}
		}
	}
	return CDRF_DODEFAULT;
}

void WaitingUsersFrame::initProgressBar(bool check)
{
	ProgressBar::Settings settings;
	settings.clrBackground = BOOLSETTING(PROGRESS_OVERRIDE_COLORS) ? SETTING(UPLOAD_BAR_COLOR) : GetSysColor(COLOR_HIGHLIGHT);
	settings.clrText = SETTING(PROGRESS_TEXT_COLOR_UP);
	settings.clrEmptyBackground = SETTING(PROGRESS_BACK_COLOR);
	settings.odcStyle = false;
	settings.odcBumped = false;
	settings.depth = SETTING(PROGRESS_3DDEPTH);
	settings.setTextColor = BOOLSETTING(PROGRESS_OVERRIDE_COLORS2);
	if (!check || progressBar.get() != settings) progressBar.set(settings);
	progressBar.setWindowBackground(Colors::g_bgColor);
}

int WaitingUsersFrame::UploadQueueItem::compareItems(const UploadQueueItem* a, const UploadQueueItem* b, uint8_t col)
{
	dcassert(!ClientManager::isBeforeShutdown());
	switch (col)
	{
		case COLUMN_FILE:
		case COLUMN_TYPE:
		case COLUMN_PATH:
		case COLUMN_NICK:
		case COLUMN_HUB:
			return stricmp(a->getText(col), b->getText(col));
		case COLUMN_TRANSFERRED:
			return compare(a->file->getPos(), b->file->getPos());
		case COLUMN_SIZE:
			return compare(a->file->getSize(), b->file->getSize());
		case COLUMN_ADDED:
		case COLUMN_WAITING:
			return compare(a->file->getTime(), b->file->getTime());
		case COLUMN_SLOTS:
			return compare(a->getUser()->getSlots(), b->getUser()->getSlots());
		case COLUMN_SHARE:
			return compare(a->getUser()->getBytesShared(), b->getUser()->getBytesShared());
		case COLUMN_IP:
			return compare(a->ip, b->ip);
	}
	return stricmp(a->getText(col), b->getText(col));
}

void WaitingUsersFrame::UploadQueueItem::update()
{
	const auto& user = getUser();
	string nick;
	Ip4Address ip4;
	Ip6Address ip6;
	int64_t bytesShared;
	int slots;
	user->getInfo(nick, ip4, ip6, bytesShared, slots);

	const string& filename = file->getFile();
	if (!filename.empty())
	{
		if (file->getFlags() & UploadQueueFile::FLAG_PARTIAL_FILE_LIST)
		{
			setText(COLUMN_FILE, TSTRING(PARTIAL_FILE_LIST2));
			setText(COLUMN_PATH, Text::toT(filename));
		}
		else
		{
			setText(COLUMN_FILE, Text::toT(Util::getFileName(filename)));
			if (filename.length() != 43 || memcmp(filename.c_str(), "TTH/", 4))
			{
				setText(COLUMN_TYPE, Text::toT(Util::getFileExtWithoutDot(filename)));
				setText(COLUMN_PATH, Text::toT(Util::getFilePath(filename)));
			}
		}
	}
	else
		setText(COLUMN_FILE, TSTRING(UNKNOWN_FILE));
	setText(COLUMN_NICK, Text::toT(nick));
	setText(COLUMN_HUB, hintedUser.user ? Text::toT(Util::toString(ClientManager::getHubNames(hintedUser.user->getCID(), Util::emptyString))) : Util::emptyStringT);
	setText(COLUMN_TRANSFERRED, Util::formatBytesT(file->getPos()) + _T(" (") + Util::toStringT((double) file->getPos() * 100.0 / (double) file->getSize()) + _T("%)"));
	setText(COLUMN_SIZE, Util::formatBytesT(file->getSize()));
	setText(COLUMN_ADDED, Text::toT(Util::formatDateTime(file->getTime())));
	setText(COLUMN_WAITING, Util::formatSecondsT(GET_TIME() - file->getTime()));
	setText(COLUMN_SHARE, Util::formatBytesT(bytesShared));
	setText(COLUMN_SLOTS, Util::toStringT(slots));
	if (Util::isValidIp4(ip4))
	{
		ip.type = AF_INET;
		ip.data.v4 = ip4;
	}
	else if (Util::isValidIp6(ip6))
	{
		ip.type = AF_INET6;
		ip.data.v6 = ip6;
	}
	if (ip.type)
	{
		tstring ipStr = Util::printIpAddressT(ip);
		if (text[COLUMN_IP] != ipStr)
		{
			text[COLUMN_IP] = std::move(ipStr);
			Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
			setText(COLUMN_LOCATION, Text::toT(Util::getDescription(ipInfo)));
		}
	}
}

void WaitingUsersFrame::getSelectedUsers(vector<UserPtr>& v) const
{
	int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	while (i >= 0)
	{
		UploadQueueItem* ui = ctrlList.getItemData(i);
		v.push_back(ui->getUser());
		i = ctrlList.GetNextItem(i, LVNI_SELECTED);
	}
}

CFrameWndClassInfo& WaitingUsersFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("WaitingUsersFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::UPLOAD_QUEUE, 0);

	return wc;
}
