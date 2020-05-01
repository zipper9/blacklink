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
#include "Resource.h"

#include "../client/ClientManager.h"
#include "../client/QueueManager.h"
#include "WaitingUsersFrame.h"
#include "MainFrm.h"
#include "CustomDrawHelpers.h"

#include "BarShader.h"

static const unsigned TIMER_VAL = 1000;

HIconWrapper WaitingUsersFrame::frameIcon(IDR_UPLOAD_QUEUE);

int WaitingUsersFrame::columnSizes[] = { 250, 20, 100, 75, 75, 75, 75, 100, 100, 100, 100, 150, 75 };

int WaitingUsersFrame::columnIndexes[] =
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
#ifdef FLYLINKDC_USE_DNS
	UploadQueueItem::COLUMN_DNS,
#endif
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
#ifdef FLYLINKDC_USE_DNS
	ResourceManager::DNS_BARE,
#endif
	ResourceManager::SLOTS,
	ResourceManager::SHARED
};

WaitingUsersFrame::WaitingUsersFrame() :
	timer(m_hWnd),
	showTree(true), m_needsUpdateStatus(false), m_needsResort(false),
	showTreeContainer(_T("BUTTON"), this, SHOWTREE_MESSAGE_MAP)
{
	++UploadManager::g_count_WaitingUsersFrame;
	memset(statusSizes, 0, sizeof(statusSizes));
}

WaitingUsersFrame::~WaitingUsersFrame()
{
	--UploadManager::g_count_WaitingUsersFrame;
}

LRESULT WaitingUsersFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	showTree = BOOLSETTING(UPLOAD_QUEUE_FRAME_SHOW_TREE);
	
	// status bar
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_UPLOAD_QUEUE);
	                  
	setListViewExtStyle(ctrlList, BOOLSETTING(SHOW_GRIDLINES), false);
	ctrlQueued.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	                  TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP,
	                  WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	                  
	ctrlQueued.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	
	m_nProportionalPos = SETTING(UPLOAD_QUEUE_FRAME_SPLIT);
	SetSplitterPanes(ctrlQueued.m_hWnd, ctrlList.m_hWnd);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(UPLOAD_QUEUE_FRAME_ORDER), UploadQueueItem::COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(UPLOAD_QUEUE_FRAME_WIDTHS), UploadQueueItem::COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == UploadQueueItem::COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == UploadQueueItem::COLUMN_LAST);
	
	// column names, sizes
	for (uint8_t j = 0; j < UploadQueueItem::COLUMN_LAST; j++)
	{
		const int fmt = (j == UploadQueueItem::COLUMN_TRANSFERRED || j == UploadQueueItem::COLUMN_SIZE) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlList.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlList.setColumnOrderArray(UploadQueueItem::COLUMN_LAST, columnIndexes);
	ctrlList.setVisible(SETTING(UPLOAD_QUEUE_FRAME_VISIBLE));
	
	ctrlList.setSortFromSettings(SETTING(UPLOAD_QUEUE_FRAME_SORT));
	
	// colors
	setListViewColors(ctrlList);
	
	ctrlQueued.SetBkColor(Colors::g_bgColor);
	ctrlQueued.SetTextColor(Colors::g_textColor);
	
	ctrlShowTree.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlShowTree.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlShowTree.SetCheck(showTree);
	ctrlShowTree.SetFont(Fonts::g_systemFont);
	showTreeContainer.SubclassWindow(ctrlShowTree.m_hWnd);
	
	memset(statusSizes, 0, sizeof(statusSizes));
	statusSizes[0] = 16;
	ctrlStatus.SetParts(4, statusSizes);
	UpdateLayout();
	
	UploadManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	// Load all searches
	LoadAll();
	timer.createTimer(TIMER_VAL);
	bHandled = FALSE;
	return TRUE;
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
		WinUtil::setButtonPressed(IDC_UPLOAD_QUEUE, false);
		
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		HTREEITEM userNode = ctrlQueued.GetRootItem();
		
		while (userNode)
		{
			delete reinterpret_cast<UserItem *>(ctrlQueued.GetItemData(userNode));
			userNode = ctrlQueued.GetNextSiblingItem(userNode);
		}
		ctrlList.DeleteAllItems();
		UQFUsers.clear();
		SET_SETTING(UPLOAD_QUEUE_FRAME_SHOW_TREE, ctrlShowTree.GetCheck() == BST_CHECKED);
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
		
		ctrlStatus.GetRect(0, sr);
		ctrlShowTree.MoveWindow(sr);
	}
	if (showTree)
	{
		if (GetSinglePaneMode() != SPLIT_PANE_NONE)
		{
			SetSinglePaneMode(SPLIT_PANE_NONE);
		}
	}
	else
	{
		if (GetSinglePaneMode() != SPLIT_PANE_RIGHT)
		{
			SetSinglePaneMode(SPLIT_PANE_RIGHT);
		}
	}
	CRect rc = rect;
	SetSplitterRect(rc);
}


LRESULT WaitingUsersFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (getSelectedUser())
	{
		const UserPtr User = getCurrentdUser();
		if (User)
		{
			UploadManager::LockInstanceQueue lockedInstance; // [+] IRainman opt.
			lockedInstance->clearUserFilesL(User);
		}
	}
	else
	{
		if (ctrlList.getSelectedCount())
		{
			int j = -1;
			UserList RemoveUsers;
			while ((j = ctrlList.GetNextItem(j, LVNI_SELECTED)) != -1)
			{
				// Ok let's cheat here, if you try to remove more users here is not working :(
				RemoveUsers.push_back((ctrlList.getItemData(j))->getUser());
			}
			UploadManager::LockInstanceQueue lockedInstance; // [+] IRainman opt.
			for (auto i = RemoveUsers.cbegin(); i != RemoveUsers.cend(); ++i)
			{
				lockedInstance->clearUserFilesL(*i);
			}
		}
	}
	m_needsUpdateStatus = true; // [!] IRainman opt.
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
	// !SMT!-UI
	OMenu contextMenu;
	contextMenu.CreatePopupMenu();
	clearUserMenu(); // [+] IRainman fix.
	
	if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() > 0)
	{
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlList, pt);
		}
		
		appendAndActivateUserItems(contextMenu);
		
		contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
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
		
		// !SMT!-UI
		reinitUserMenu(getCurrentdUser(), Util::emptyString); // [+] IRainman fix.
		appendAndActivateUserItems(contextMenu);
		
		contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		WinUtil::unlinkStaticMenus(contextMenu); // TODO - fix copy-paste
		return TRUE;
	}
	return FALSE;
}

LRESULT WaitingUsersFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = frameIcon;
	opt->isHub = false;
	return TRUE;
}

void WaitingUsersFrame::LoadAll()
{
	CLockRedraw<> lockRedraw(ctrlList);
	CLockRedraw<true> lockRedrawQueued(ctrlQueued);
	
	HTREEITEM userNode = ctrlQueued.GetRootItem();
	while (userNode)
	{
		delete reinterpret_cast<UserItem *>(ctrlQueued.GetItemData(userNode));
		userNode = ctrlQueued.GetNextSiblingItem(userNode);
	}
	// TODO - delete
	ctrlList.DeleteAllItems();
	ctrlQueued.DeleteAllItems();
	UQFUsers.clear();
	
	// Load queue
	{
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		UQFUsers.reserve(users.size());
		for (auto uit = users.cbegin(); uit != users.cend(); ++uit)
		{
			const UserPtr user = uit->getUser();
			UQFUsers.push_back(user);			
			tstring text = Text::toT(user->getLastNick() + " - " + uit->m_hintedUser.hint);
			ctrlQueued.InsertItem(TVIF_PARAM | TVIF_TEXT, text.c_str(),
			                      0, 0, 0, 0, reinterpret_cast<LPARAM>(new UserItem(user)), TVI_ROOT, TVI_LAST);
			for (auto i = uit->m_waiting_files.cbegin(); i != uit->m_waiting_files.cend(); ++i)
			{
				AddFile(*i);
			}
		}
	}
	m_needsResort = true; // [!] IRainman opt.
	m_needsUpdateStatus = true; // [!] IRainman opt.
}

void WaitingUsersFrame::RemoveUser(const UserPtr& aUser)
{
	HTREEITEM userNode = ctrlQueued.GetRootItem();
	
	for (auto i = UQFUsers.cbegin(); i != UQFUsers.cend(); ++i)
	{
		if (*i == aUser) // [1] https://www.box.net/shared/95ab392bc53d0452debc
		{
			UQFUsers.erase(i);
			break;
		}
	}
	
	while (userNode)
	{
		UserItem *ui = reinterpret_cast<UserItem *>(ctrlQueued.GetItemData(userNode));
		if (aUser == ui->m_user)
		{
			delete ui;
			ctrlQueued.DeleteItem(userNode);
			return;
		}
		userNode = ctrlQueued.GetNextSiblingItem(userNode);
	}
	m_needsUpdateStatus = true; // [!] IRainman opt.
}

LRESULT WaitingUsersFrame::onItemChanged(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	HTREEITEM userNode = ctrlQueued.GetSelectedItem();
	
	while (userNode)
	{
		CLockRedraw<> lockRedraw(ctrlList);
		CLockRedraw<true> lockRedrawQueued(ctrlQueued);
		ctrlList.DeleteAllItems();
		UserItem* ui = reinterpret_cast<UserItem *>(ctrlQueued.GetItemData(userNode));
		if (ui)
		{
			UploadManager::LockInstanceQueue lockedInstance;
			const auto& users = lockedInstance->getUploadQueueL();
			auto it = std::find_if(users.begin(), users.end(), [&](const UserPtr & u)
			{
				return u == ui->m_user;
			});
			if (it != users.end())
			{
				for (auto i = it->m_waiting_files.cbegin(); i != it->m_waiting_files.cend(); ++i)
				{
					AddFile(*i);
				}
				m_needsResort = true; // [!] IRainman opt.
				m_needsUpdateStatus = true; // [!] IRainman opt.
				return 0;
			}
		}
		else
		{
			LoadAll();
		}
		userNode = ctrlQueued.GetNextSiblingItem(userNode);
	}
	return 0;
}

void WaitingUsersFrame::RemoveFile(const UploadQueueItemPtr& aUQI)
{
	ctrlList.deleteItem(aUQI.get());
}

void WaitingUsersFrame::AddFile(const UploadQueueItemPtr& aUQI)
{
	dcassert(aUQI != nullptr);
	HTREEITEM userNode = ctrlQueued.GetRootItem();
	bool add = true;
	
	HTREEITEM selNode = ctrlQueued.GetSelectedItem();
	
	if (userNode)
	{
		for (auto i = UQFUsers.cbegin(); i != UQFUsers.cend(); ++i)
		{
			if (*i == aUQI->getUser())
			{
				add = false;
				break;
			}
		}
	}
	if (add)
	{
		UQFUsers.push_back(aUQI->getUser());
		const HintedUser& hintedUser = aUQI->getHintedUser();
		tstring text = Text::toT(aUQI->getUser()->getLastNick()) + _T(" - ") + WinUtil::getHubNames(hintedUser).first;
		userNode = ctrlQueued.InsertItem(TVIF_PARAM | TVIF_TEXT, text.c_str(),
		                                 0, 0, 0, 0, reinterpret_cast<LPARAM>(new UserItem(hintedUser)), TVI_ROOT, TVI_LAST);
	}
	if (selNode)
	{
		TCHAR selBuf[256];
		selBuf[0] = 0;
		ctrlQueued.GetItemText(selNode, selBuf, 255);
		tstring text = Text::toT(aUQI->getUser()->getLastNick()) + _T(" - ") + WinUtil::getHubNames(aUQI->getHintedUser()).first;
		if (text != selBuf)
			return;
	}
	aUQI->update();
	ctrlList.insertItem(ctrlList.GetItemCount(), aUQI.get(), aUQI->getImageIndex()); // aUQI->getImageIndex() TODO - image callback
}

HTREEITEM WaitingUsersFrame::GetParentItem()
{
	HTREEITEM item = ctrlQueued.GetSelectedItem(), parent = ctrlQueued.GetParentItem(item);
	parent = parent ? parent : item;
	ctrlQueued.SelectItem(parent);
	return parent;
}

void WaitingUsersFrame::updateStatus()
{
	if (ctrlStatus.IsWindow())
	{
		const int cnt = ctrlList.GetItemCount();
		const int users = ctrlQueued.GetCount();
		
		tstring tmp[2];
		if (showTree)
		{
			tmp[0] = TSTRING(USERS) + _T(": ") + Util::toStringT(users);
		}
		
		tmp[1] = TSTRING(ITEMS) + _T(": ") + Util::toStringT(cnt);
		bool u = false;
		
		for (int i = 1; i < 3; i++)
		{
			const int w = WinUtil::getTextWidth(tmp[i - 1], ctrlStatus.m_hWnd);
			
			if (statusSizes[i] < w)
			{
				statusSizes[i] = w + 50;
				u = true;
			}
			ctrlStatus.SetText(i + 1, tmp[i - 1].c_str());
		}
		
		if (u)
		{
			UpdateLayout(TRUE);
		}
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
		UploadManager::LockInstanceQueue lockedInstance; // [+] IRainman opt.
		for (auto i = RemoveUsers.cbegin(); i != RemoveUsers.cend(); ++i)
		{
			lockedInstance->clearUserFilesL(*i);
		}
	}
	m_needsUpdateStatus = true; // [!] IRainman opt.
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
			case REMOVE_WAITING_ITEM:
			{
				RemoveFile(static_cast<UploadQueueTask&>(*j->second).getItem());
			}
			break;
			case REMOVE:
			{
				RemoveUser(static_cast<UserTask&>(*j->second).getUser());
			}
			break;
			case ADD_ITEM:
			{
				AddFile(static_cast<UploadQueueTask&>(*j->second).getItem());
				m_needsResort = true;
			}
			break;
			case UPDATE_ITEMS:
			{
				const int l_item_count = ctrlList.GetItemCount();
				if (l_item_count)
				{
					const int l_top_index = ctrlList.GetTopIndex();
					const int l_count_per_page = ctrlList.GetCountPerPage();
					int64_t itime = GET_TIME();
					for (int i = l_top_index; i < l_count_per_page; i++)
					{
						auto ii = ctrlList.getItemData(i);
						if (ii)
						{
							// https://drdump.com/DumpGroup.aspx?DumpGroupID=491521
							ii->setText(UploadQueueItem::COLUMN_TRANSFERRED, Util::formatBytesT(ii->getPos()) + _T(" (") + Util::toStringT((double)ii->getPos() * 100.0 / (double)ii->getSize()) + _T("%)"));
							ii->setText(UploadQueueItem::COLUMN_WAITING, Util::formatSecondsT(itime - ii->getTime()));
							ctrlList.updateItem(i, UploadQueueItem::COLUMN_TRANSFERRED);
							ctrlList.updateItem(i, UploadQueueItem::COLUMN_WAITING);
						}
					}
				}
				if (m_needsResort)
				{
					ctrlList.resort();
					m_needsResort = false;
				}
				if (m_needsUpdateStatus)
				{
					if (BOOLSETTING(BOLD_WAITING_USERS))
						setDirty();
					updateStatus();
					m_needsUpdateStatus = false;
				}
			}
			break;
			default:
				dcassert(0);
		}
		if (j->first != UPDATE_ITEMS)
		{
			m_needsUpdateStatus = true;
		}
		delete j->second;
	}
}

void WaitingUsersFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlList.isRedraw())
		{
			ctrlQueued.SetBkColor(Colors::g_bgColor);
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
			ctrlListFocused = ctrlList.m_hWnd == GetFocus();
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
			return CDRF_NOTIFYSUBITEMDRAW;
			
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			// Let's draw a box if needed...
			if (BOOLSETTING(SHOW_PROGRESS_BARS) && ctrlList.findColumn(cd->iSubItem) == UploadQueueItem::COLUMN_TRANSFERRED)
			{
				// draw something nice...
				LocalArray<TCHAR, 256> buf;
				ctrlList.GetItemText((int)cd->nmcd.dwItemSpec, cd->iSubItem, buf.data(), 255);
				ctrlList.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				// Real rc, the original one.
				CRect real_rc = rc;
				// We need to offset the current rc to (0, 0) to paint on the New dc
				rc.MoveToXY(0, 0);
				
				// Text rect
				CRect rc2 = rc;
				rc2.left += 6; // indented with 6 pixels
				rc2.right -= 2; // and without messing with the border of the cell
				
				// Set references
				CDC cdc;
				cdc.CreateCompatibleDC(cd->nmcd.hdc);
				HBITMAP hBmp = CreateCompatibleBitmap(cd->nmcd.hdc,  real_rc.Width(),  real_rc.Height());
				HBITMAP pOldBmp = cdc.SelectBitmap(hBmp);
				HDC& dc = cdc.m_hDC;
				
				HFONT oldFont = (HFONT)SelectObject(dc, Fonts::g_font);
				SetBkMode(dc, TRANSPARENT);
				
				CBarShader statusBar(rc.bottom - rc.top, rc.right - rc.left, RGB(150, 0, 0), ii->getSize());
				statusBar.FillRange(0, ii->getPos(), RGB(222, 160, 0));
				statusBar.Draw(cdc, rc.top, rc.left, SETTING(PROGRESS_3DDEPTH));
				
				SetTextColor(dc, SETTING(PROGRESS_TEXT_COLOR_UP));
				::ExtTextOut(dc, rc2.left, rc2.top + (rc2.Height() - WinUtil::getTextHeight(dc) - 1) / 2, ETO_CLIPPED, rc2, buf.data(), _tcslen(buf.data()), NULL);
				
				SelectObject(dc, oldFont);
				
				BitBlt(cd->nmcd.hdc, real_rc.left, real_rc.top, real_rc.Width(), real_rc.Height(), dc, 0, 0, SRCCOPY);
				
				DeleteObject(cdc.SelectBitmap(pOldBmp));
				return CDRF_SKIPDEFAULT;
			}
			
			if (ctrlList.findColumn(cd->iSubItem) == UploadQueueItem::COLUMN_LOCATION)
			{
				const tstring& text = ii->getText(UploadQueueItem::COLUMN_LOCATION);
				if (!text.empty())
				{
					CustomDrawHelpers::drawLocation(ctrlList, ctrlListFocused, cd, ii->ipInfo);
					return CDRF_SKIPDEFAULT;
				}
			}
		}
	}
	return CDRF_DODEFAULT;
}
