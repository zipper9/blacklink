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

#include "../client/ResourceManager.h"
#include "../client/SettingsManager.h"
#include "../client/ConnectionManager.h"
#include "../client/DownloadManager.h"
#include "../client/UploadManager.h"
#include "../client/QueueManager.h"
#include "../client/QueueItem.h"
#include "../client/ThrottleManager.h"
#include "../client/DatabaseManager.h"

#include "WinUtil.h"
#include "TransferView.h"
#include "MainFrm.h"
#include "QueueFrame.h"
#include "WaitingUsersFrame.h"
#include "ResourceLoader.h"
#include "ExMessageBox.h"

static const unsigned TIMER_VAL = 1000;

#ifdef _DEBUG
std::atomic<long> TransferView::ItemInfo::g_count_transfer_item;
#endif

const int TransferView::columnId[] =
{
	COLUMN_USER,
	COLUMN_STATUS,
	COLUMN_TIMELEFT,
	COLUMN_SPEED,
	COLUMN_FILE,
	COLUMN_SIZE,
	COLUMN_PATH,
	COLUMN_HUB,
	COLUMN_IP,
	COLUMN_LOCATION,
	COLUMN_P2P_GUARD,
	COLUMN_CIPHER,
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	COLUMN_RATIO,
#endif
	COLUMN_SHARE,
	COLUMN_SLOTS
};

static const int columnSizes[] =
{
	150, // COLUMN_USER
	280, // COLUMN_STATUS
	75,  // COLUMN_TIMELEFT
	90,  // COLUMN_SPEED
	175, // COLUMN_FILE
	85,  // COLUMN_SIZE
	200, // COLUMN_PATH
	150, // COLUMN_HUB
	100, // COLUMN_IP
	120, // COLUMN_LOCATION
	140, // COLUMN_P2P_GUARD
	100, // COLUMN_CIPHER
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	50,  // COLUMN_RATIO
#endif
	85,  // COLUMN_SHARE
	75   // COLUMN_SLOTS
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::USER,
	ResourceManager::STATUS,
	ResourceManager::TIME_LEFT,
	ResourceManager::SPEED,
	ResourceManager::FILENAME,
	ResourceManager::SIZE,
	ResourceManager::PATH,
	ResourceManager::HUB_SEGMENTS,
	ResourceManager::IP,
	ResourceManager::LOCATION_BARE,
	ResourceManager::P2P_GUARD,
	ResourceManager::CIPHER,
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	ResourceManager::RATIO,
#endif
	ResourceManager::SHARED,
	ResourceManager::SLOTS
};

template<>
string UserInfoBaseHandlerTraitsUser<UserPtr>::getNick(const UserPtr& user)
{
	return user->getLastNick();
}

TransferView::TransferView() : timer(m_hWnd), shouldSort(false)
{
	ctrlTransfers.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlTransfers.setColumnFormat(COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlTransfers.setColumnFormat(COLUMN_TIMELEFT, LVCFMT_RIGHT);
	ctrlTransfers.setColumnFormat(COLUMN_SPEED, LVCFMT_RIGHT);
}

TransferView::~TransferView()
{
	OperaColors::ClearCache();
}

LRESULT TransferView::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	initProgressBars(false);

	ctrlTransfers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_STATICEDGE, IDC_TRANSFERS);
	ctrlTransfers.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	if (WinUtil::setExplorerTheme(ctrlTransfers))
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED | CustomDrawHelpers::FLAG_USE_HOT_ITEM;
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));
	
	ctrlTransfers.insertColumns(SettingsManager::TRANSFER_FRAME_ORDER, SettingsManager::TRANSFER_FRAME_WIDTHS, SettingsManager::TRANSFER_FRAME_VISIBLE);
	ctrlTransfers.setSortFromSettings(SETTING(TRANSFER_FRAME_SORT));
	
	setListViewColors(ctrlTransfers);
	
	ctrlTransfers.SetImageList(g_transferArrowsImage.getIconList(), LVSIL_SMALL);
	
	copyMenu.CreatePopupMenu();
	for (size_t i = 0; i < COLUMN_LAST; ++i)
	{
		copyMenu.AppendMenu(MF_STRING, IDC_COPY + columnId[i], CTSTRING_I(columnNames[i]));
	}
	copyMenu.AppendMenu(MF_SEPARATOR);
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(COPY_TTH));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
	
	usercmdsMenu.CreatePopupMenu();
	
	segmentedMenu.CreatePopupMenu();
	segmentedMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
	segmentedMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSE), g_iconBitmaps.getBitmap(IconBitmaps::PAUSE, 0));
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	appendPreviewItems(segmentedMenu);
	segmentedMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY_USER_INFO));
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	segmentedMenu.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(OPEN_DOWNLOAD_QUEUE), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD_QUEUE, 0));
	segmentedMenu.AppendMenu(MF_SEPARATOR);
#ifdef FLYLINKDC_USE_DROP_SLOW
	segmentedMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
#endif
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	segmentedMenu.AppendMenu(MF_STRING, IDC_CONNECT_ALL, CTSTRING(CONNECT_ALL));
	segmentedMenu.AppendMenu(MF_STRING, IDC_DISCONNECT_ALL, CTSTRING(DISCONNECT_ALL));
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	segmentedMenu.AppendMenu(MF_STRING, IDC_EXPAND_ALL, CTSTRING(EXPAND_ALL));
	segmentedMenu.AppendMenu(MF_STRING, IDC_COLLAPSE_ALL, CTSTRING(COLLAPSE_ALL));
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	segmentedMenu.AppendMenu(MF_STRING, IDC_REMOVEALL, CTSTRING(REMOVE_ALL));
	
	ConnectionManager::getInstance()->addListener(this);
	DownloadManager::getInstance()->addListener(this);
	UploadManager::getInstance()->addListener(this);
	QueueManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	timer.createTimer(TIMER_VAL, 4);
	return 0;
}

void TransferView::prepareClose()
{
	timer.destroyTimer();
	tasks.setDisabled(true);
	tasks.clear();
	
	ctrlTransfers.saveHeaderOrder(SettingsManager::TRANSFER_FRAME_ORDER, SettingsManager::TRANSFER_FRAME_WIDTHS, SettingsManager::TRANSFER_FRAME_VISIBLE);
	SET_SETTING(TRANSFER_FRAME_SORT, ctrlTransfers.getSortForSettings());
	
	SettingsManager::getInstance()->removeListener(this);
	QueueManager::getInstance()->removeListener(this);
	UploadManager::getInstance()->removeListener(this);
	DownloadManager::getInstance()->removeListener(this);
	ConnectionManager::getInstance()->removeListener(this);
}

void TransferView::UpdateLayout()
{
	RECT rc;
	GetClientRect(&rc);
	ctrlTransfers.MoveWindow(&rc);
}

LRESULT TransferView::onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	UpdateLayout();
	return 0;
}

LRESULT TransferView::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlTransfers && ctrlTransfers.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlTransfers, pt);
		}
		
		if (ctrlTransfers.GetSelectedCount() > 0)
		{
			int defaultItem = 0;
			switch (SETTING(TRANSFERLIST_DBLCLICK))
			{
				case 0:
					defaultItem = IDC_PRIVATE_MESSAGE;
					break;
				case 1:
					defaultItem = IDC_GETLIST;
					break;
				case 2:
					defaultItem = IDC_MATCH_QUEUE;
					break;
				case 3:
					defaultItem = IDC_GRANTSLOT;
					break;
				case 4:
					defaultItem = IDC_ADD_TO_FAVORITES;
					break;
				case 5:
					defaultItem = IDC_FORCE;
					break;
				case 6:
					defaultItem = IDC_BROWSELIST;
					break;
				case 7:
					defaultItem = IDC_QUEUE;
			}

			bool hasTTH = false;
			int i = -1;
			while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const ItemInfo* ii = ctrlTransfers.getItemData(i);
				if (ii && !ii->tth.isZero())
				{
					hasTTH = true;
					break;
				}
			}

			bool showUserMenu = false;
			ItemInfo* ii = ctrlTransfers.getItemData(ctrlTransfers.GetNextItem(-1, LVNI_SELECTED));

			// Something wrong happened
			if (!ii)
			{
				bHandled = FALSE;
				return FALSE;
			}
			
			const bool main = !ii->parent && ctrlTransfers.findChildren(ii->getGroupCond()).size() > 1;
			
			clearPreviewMenu();
			OMenu transferMenu;
			transferMenu.CreatePopupMenu();
			clearUserMenu();
				
			transferMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
			transferMenu.AppendMenu(MF_STRING, IDC_FORCE, CTSTRING(FORCE_ATTEMPT));
			transferMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSE), g_iconBitmaps.getBitmap(IconBitmaps::PAUSE, 0));
			transferMenu.AppendMenu(MF_SEPARATOR);
			appendPreviewItems(transferMenu);
			transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)usercmdsMenu, CTSTRING(USER_COMMANDS));
			transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
			transferMenu.AppendMenu(MF_SEPARATOR);
				
			if (ii->download)
			{
				transferMenu.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(OPEN_DOWNLOAD_QUEUE), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD_QUEUE, 0));
				transferMenu.AppendMenu(MF_SEPARATOR);
			}
#ifdef FLYLINKDC_USE_DROP_SLOW
			transferMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
#endif
			transferMenu.AppendMenu(MF_STRING, IDC_ADD_P2P_GUARD, CTSTRING(CLOSE_CONNECTION_AND_BLOCK_IP), g_iconBitmaps.getBitmap(IconBitmaps::WALL, 0));
			transferMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(CLOSE_CONNECTION), g_iconBitmaps.getBitmap(IconBitmaps::DISCONNECT, 0));
			transferMenu.AppendMenu(MF_SEPARATOR);
			if (!main && (i = ctrlTransfers.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				const ItemInfo* ii = ctrlTransfers.getItemData(i);
				showUserMenu = true;				
				reinitUserMenu(ii->hintedUser.user, ii->hintedUser.hint);
				if (getSelectedUser())
					appendUcMenu(usercmdsMenu, UserCommand::CONTEXT_USER, ClientManager::getHubs(getSelectedUser()->getCID(), getSelectedHint()));
			}

			appendAndActivateUserItems(transferMenu);
				
#ifdef FLYLINKDC_USE_DROP_SLOW
			segmentedMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
			transferMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
#endif
			if (ii->download)
			{
#ifdef FLYLINKDC_USE_DROP_SLOW
				transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_ENABLED);
#endif
				transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_ENABLED);
				if (!ii->target.empty())
				{
					const string target = Text::fromT(ii->target);
#ifdef FLYLINKDC_USE_DROP_SLOW
					bool slowDisconnect;
					{
						QueueManager::LockFileQueueShared fileQueue;
						const auto& queue = fileQueue.getQueueL();
						const auto qi = queue.find(target);
						if (qi != queue.cend())
							slowDisconnect = qi->second->isAutoDrop();
						else
							slowDisconnect = false;
					}
					if (slowDisconnect)
					{
						segmentedMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_CHECKED);
						transferMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_CHECKED);
					}
#endif
					setupPreviewMenu(target);
				}
			}
			else
			{
#ifdef FLYLINKDC_USE_DROP_SLOW
				transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_DISABLED);
#endif
				transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_DISABLED);
			}

#ifdef IRAINMAN_ENABLE_WHOIS
			if (ii->transferIp.type)
			{
				selectedIP = Util::printIpAddressT(ii->transferIp);  // set tstring for 'openlink function'
				WinUtil::appendWhoisMenu(transferMenu, selectedIP, true);
			}
#endif

			activatePreviewItems(transferMenu);

			int flag = hasTTH ? MFS_ENABLED : MFS_DISABLED;
			copyMenu.EnableMenuItem(IDC_COPY_TTH, flag);
			copyMenu.EnableMenuItem(IDC_COPY_LINK, flag);
			copyMenu.EnableMenuItem(IDC_COPY_WMLINK, flag);

			if (!main)
			{
				OMenu* defItemMenu = &transferMenu;
				if (defaultItem)
				{
					if (defaultItem == IDC_GRANTSLOT)
						defItemMenu = &grantMenu;
					else if (defaultItem == IDC_ADD_TO_FAVORITES)
						defItemMenu = &favUserMenu;
				}
				transferMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, hasTTH ? MFS_ENABLED : MFS_DISABLED);
				defItemMenu->SetMenuDefaultItem(defaultItem);
				transferMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}
			else
			{
				if (defaultItem == IDC_QUEUE) segmentedMenu.SetMenuDefaultItem(defaultItem);
				segmentedMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, hasTTH ? MFS_ENABLED : MFS_DISABLED);
				segmentedMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}
			
			if (showUserMenu) usercmdsMenu.ClearMenu();
			WinUtil::unlinkStaticMenus(transferMenu);
			return TRUE;
		}
	}
	bHandled = FALSE;
	return FALSE;
}

#ifdef IRAINMAN_ENABLE_WHOIS
LRESULT TransferView::onWhoisIP(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!selectedIP.empty())
		WinUtil::processWhoisMenu(wID, selectedIP);
	return 0;
}
#endif // IRAINMAN_ENABLE_WHOIS

void TransferView::openDownloadQueue(const ItemInfo* ii)
{
	QueueFrame::openWindow();
	if (ii)
	{
		string target = Text::fromT(ii->target);
		bool isList = ii->type == Transfer::TYPE_FULL_LIST || ii->type == Transfer::TYPE_PARTIAL_LIST;
		if (!target.empty() && QueueFrame::g_frame)
			QueueFrame::g_frame->showQueueItem(target, isList);
	}
}

LRESULT TransferView::onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_QUEUE:
		{
			int i = ctrlTransfers.GetNextItem(-1, LVNI_SELECTED);
			const ItemInfo* ii = i == -1 ? nullptr : ctrlTransfers.getItemData(i);
			openDownloadQueue(ii);
			break;
		}
		case IDC_UPLOAD_QUEUE:
			WaitingUsersFrame::openWindow();
			break;
		default:
			dcassert(0);
			break;
	}
	return 0;
}

void TransferView::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
		
	StringMap ucParams = ucLineParams;
	
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* itemI = ctrlTransfers.getItemData(i);
		if (itemI->hintedUser.user && itemI->hintedUser.user->isOnline())  // [!] IRainman fix.
		{
			StringMap tmp = ucParams;
			ucParams["fileFN"] = Text::fromT(itemI->target);
			
			// compatibility with 0.674 and earlier
			ucParams["file"] = ucParams["fileFN"];
			
			ClientManager::userCommand(itemI->hintedUser, uc, tmp, true);
		}
	}
}

LRESULT TransferView::onForce(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		ctrlTransfers.SetItemText(i, COLUMN_STATUS, CTSTRING(CONNECTING_FORCED));
		
		if (ii->parent == nullptr && ii->hits != -1)
		{
			const vector<ItemInfo*>& children = ctrlTransfers.findChildren(ii->getGroupCond());
			for (auto j = children.cbegin(); j != children.cend(); ++j)
			{
				ItemInfo* jj = *j;
				
				int h = ctrlTransfers.findItem(jj);
				if (h != -1)
					ctrlTransfers.SetItemText(h, COLUMN_STATUS, CTSTRING(CONNECTING_FORCED));
					
				jj->transferFailed = false;
				ConnectionManager::getInstance()->force(jj->hintedUser);
			}
		}
		else
		{
			ii->transferFailed = false;
			ConnectionManager::getInstance()->force(ii->hintedUser);
		}
	}
	return 0;
}

void TransferView::ItemInfo::removeAll()
{
	// Не удаляются отдачи через контекстное меню
	if (hits <= 1)
		QueueManager::getInstance()->removeSource(hintedUser, QueueItem::Source::FLAG_REMOVED);
	else
		QueueManager::getInstance()->removeTarget(Text::fromT(target), false);
}

static inline int getProportionalWidth(int width, int64_t pos, int64_t size)
{
	if (pos >= size || !size) return width;
	return static_cast<int>(width*pos/size);
}

LRESULT TransferView::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	CRect rc;
	NMLVCUSTOMDRAW* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			CustomDrawHelpers::startDraw(customDrawState, cd);
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			const ItemInfo* ii = reinterpret_cast<const ItemInfo*>(cd->nmcd.lItemlParam);
			if (!ii) return CDRF_DODEFAULT;
			CustomDrawHelpers::startItemDraw(customDrawState, cd);
			customDrawState.indent = ii->parent ? 1 : 0;
			return CDRF_NOTIFYSUBITEMDRAW;
		}

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			const ItemInfo* ii = reinterpret_cast<const ItemInfo*>(cd->nmcd.lItemlParam);
			if (!ii) return CDRF_DODEFAULT;
			const int colIndex = ctrlTransfers.findColumn(cd->iSubItem);
			cd->clrText = Colors::g_textColor;
			cd->clrTextBk = Colors::g_bgColor;
			if (colIndex != COLUMN_USER &&
			    colIndex != COLUMN_HUB &&
			    colIndex != COLUMN_STATUS &&
			    colIndex != COLUMN_LOCATION &&
			    ii->status != ItemInfo::STATUS_RUNNING)
			{
				cd->clrText = OperaColors::blendColors(Colors::g_bgColor, Colors::g_textColor, 0.4);
			}
			
			if (cd->iSubItem == 0)
			{
				CustomDrawHelpers::drawFirstSubItem(customDrawState, cd, ii->getText(colIndex));
				return CDRF_SKIPDEFAULT;
			}

			if (colIndex == COLUMN_STATUS)
			{
				CRect rc3;
				tstring status = ii->getText(COLUMN_STATUS);
				CustomDrawHelpers::startSubItemDraw(customDrawState, cd);
				if (ii->status == ItemInfo::STATUS_RUNNING)
				{
					if (!BOOLSETTING(SHOW_PROGRESS_BARS))
					{
						bHandled = FALSE;
						return 0;
					}
					
#if 0
					//this is just severely broken, msdn says GetSubItemRect requires a one based index
					//but it wont work and index 0 gives the rect of the whole item
					if (cd->iSubItem == 0)
					{
						//use LVIR_LABEL to exclude the icon area since we will be painting over that
						//later
						ctrlTransfers.GetItemRect((int)cd->nmcd.dwItemSpec, &rc, LVIR_LABEL);
					}
					else
					{
						ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, &rc);
					}
#endif
					ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, &rc);
					int barIndex = ii->download ? (!ii->parent ? 0 : 1) : 2;
					int iconIndex = -1;

					// Real rc, the original one.
					CRect real_rc = rc;
					// We need to offset the current rc to (0, 0) to paint on the New dc
					rc.MoveToXY(0, 0);

					CDC cdc;
					cdc.CreateCompatibleDC(cd->nmcd.hdc);
					HBITMAP hBmp = CreateCompatibleBitmap(cd->nmcd.hdc, real_rc.Width(), real_rc.Height());
					
					HBITMAP pOldBmp = cdc.SelectBitmap(hBmp);
					HDC dc = cdc.m_hDC;

					if (BOOLSETTING(STEALTHY_STYLE_ICO))
					{
						if (ii->type == Transfer::TYPE_FULL_LIST || ii->type == Transfer::TYPE_PARTIAL_LIST)
						{
							iconIndex = 5;
						}
						else if (ii->status == ItemInfo::STATUS_RUNNING)
						{
							int64_t speedmark;
							if (!BOOLSETTING(THROTTLE_ENABLE))
							{
								const int64_t speedignore = Util::toInt64(SETTING(UPLOAD_SPEED));
								speedmark = BOOLSETTING(STEALTHY_STYLE_ICO_SPEEDIGNORE) ?
									(ii->download ? SETTING(TOP_DL_SPEED) : SETTING(TOP_UL_SPEED)) / 5 : speedignore * 20;
							}
							else
							{
								if (!ii->download)
									speedmark = ThrottleManager::getInstance()->getUploadLimitInKBytes() / 5;
								else
									speedmark = ThrottleManager::getInstance()->getDownloadLimitInKBytes() / 5;
							}
							const int64_t speedkb = ii->speed / 1000;
							if (speedkb >= speedmark*4) iconIndex = 4; else
							if (speedkb >= speedmark*3) iconIndex = 3; else
							if (speedkb >= speedmark*2) iconIndex = 2; else
							if (speedkb >= (speedmark*3)/2) iconIndex = 1; else iconIndex = 0;
						}
					}

					int width = rc.Width();
					if (progressBar[barIndex].get().odcStyle) width -= 2;
					int pos = width > 1 ? getProportionalWidth(width, ii->getPos(), ii->size) : 0;
					progressBar[barIndex].draw(dc, rc, pos, status, iconIndex);

					BitBlt(cd->nmcd.hdc, real_rc.left, real_rc.top, real_rc.Width(), real_rc.Height(), dc, 0, 0, SRCCOPY);
					ATLVERIFY(cdc.SelectBitmap(pOldBmp) == hBmp);
					DeleteObject(hBmp);
					
					if (cd->iSubItem == 0)
					{
						LVITEM lvItem = {0};
						lvItem.iItem = cd->nmcd.dwItemSpec;
						lvItem.iSubItem = 0;
						lvItem.mask = LVIF_IMAGE | LVIF_STATE;
						lvItem.stateMask = LVIS_SELECTED;
						ctrlTransfers.GetItem(&lvItem);
						
						HIMAGELIST imageList = (HIMAGELIST)::SendMessage(ctrlTransfers.m_hWnd, LVM_GETIMAGELIST, LVSIL_SMALL, 0);
						if (imageList)
						{
							//let's find out where to paint it
							//and draw the background to avoid having
							//the selection color as background
							CRect iconRect;
							ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, 0, LVIR_ICON, iconRect);
							ImageList_Draw(imageList, lvItem.iImage, cd->nmcd.hdc, iconRect.left, iconRect.top, ILD_TRANSPARENT);
						}
					}
					return CDRF_SKIPDEFAULT;
				}
				else
				{
					CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, nullptr, -1, status, false);
					return CDRF_SKIPDEFAULT;
				}
			}
			else if (colIndex == COLUMN_P2P_GUARD)
			{
				const tstring text = ii->getText(colIndex);
				if (!text.empty())
				{
					CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, &g_userStateImage.getIconList(), 3, text, false);
					return CDRF_SKIPDEFAULT;
				}
				return CDRF_DODEFAULT;
			}
			else if (colIndex == COLUMN_LOCATION)
			{
				IPInfo& ipInfo = ii->ipInfo;
				if (!(ipInfo.known & (IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION)) && Util::isValidIp(ii->transferIp))
				{
					Util::getIpInfo(ii->transferIp, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
				}
				if (!ipInfo.country.empty() || !ipInfo.location.empty())
				{
					CustomDrawHelpers::drawLocation(customDrawState, cd, ipInfo);
					return CDRF_SKIPDEFAULT;
				}
				return CDRF_DODEFAULT;
			}
			// Fall through
		}
		default:
			return CDRF_DODEFAULT;
	}
}

LRESULT TransferView::onDoubleClickTransfers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	if (item->iItem != -1)
	{
		CRect rect;
		ctrlTransfers.GetItemRect(item->iItem, rect, LVIR_ICON);
		
		// if double click on state icon, ignore...
		if (item->ptAction.x < rect.left)
			return 0;
			
		ItemInfo* i = ctrlTransfers.getItemData(item->iItem);
		const vector<ItemInfo*>& children = ctrlTransfers.findChildren(i->getGroupCond());
		bool isUser = i->parent != nullptr || children.size() <= 1;
		switch (SETTING(TRANSFERLIST_DBLCLICK))
		{
			case 0:
				if (isUser) i->pm(i->hintedUser.hint);
				break;
			case 1:
				if (isUser) i->getList();
				break;
			case 2:
				if (isUser) i->matchQueue();
			case 3:
				if (isUser) i->grantSlotPeriod(i->hintedUser.hint, 600);
				break;
			case 4:
				if (isUser) i->addFav();
				break;
			case 5:
				if (isUser)
				{
					i->statusString = TSTRING(CONNECTING_FORCED);
					ctrlTransfers.updateItem(i);
					ClientManager::getInstance()->connect(i->hintedUser, Util::toString(Util::rand()), false);
				}
				break;
			case 6:
				if (isUser) i->browseList();
				break;
			case 7:
				openDownloadQueue(i);
		}
	}
	return 0;
}

int TransferView::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col)
{
	if (a->status == b->status)
	{
		if (a->download != b->download)
		{
			return a->download ? -1 : 1;
		}
	}
	else
	{
		return (a->status == ItemInfo::STATUS_RUNNING) ? -1 : 1;
	}
	
	switch (col)
	{
		case COLUMN_USER:
			if (a->hits == b->hits)
				return Util::defaultSort(a->getText(COLUMN_USER), b->getText(COLUMN_USER));
			return compare(a->hits, b->hits);						
		case COLUMN_HUB:
			if (a->running == b->running)
				return Util::defaultSort(a->getText(COLUMN_HUB), b->getText(COLUMN_HUB));
			return compare(a->running, b->running);						
		case COLUMN_STATUS:
			return compare(a->getProgressPosition(), b->getProgressPosition());
		case COLUMN_TIMELEFT:
			return compare(a->timeLeft, b->timeLeft);
		case COLUMN_SPEED:
			return compare(a->speed, b->speed);
		case COLUMN_SIZE:
			return compare(a->size, b->size);
#ifdef FLYLINKDC_USE_COLUMN_RATIO
			//case COLUMN_RATIO:
			//  return compare(a->getRatio(), b->getRatio());
#endif
		case COLUMN_SHARE:
			return a->getUser() && b->getUser() ? compare(a->getUser()->getBytesShared(), b->getUser()->getBytesShared()) : 0;
		case COLUMN_SLOTS:
			return compare(Util::toInt(a->getText(col)), Util::toInt(b->getText(col)));
		case COLUMN_IP:
			return compare(a->transferIp, b->transferIp);
		default:
			return Util::defaultSort(a->getText(col), b->getText(col));
	}
}

TransferView::ItemInfo* TransferView::findItem(const UpdateInfo& ui, int& pos) const
{
	ItemInfo* ii = nullptr;
	int count = ctrlTransfers.GetItemCount();
	for (int j = 0; j < count; ++j)
	{
		ii = ctrlTransfers.getItemData(j);
		if (ii)
		{
			if (ui == *ii)
			{
				pos = j;
				return ii;
			}
			if (ui.download == ii->download && !ii->parent)
			{
				const auto& children = ctrlTransfers.findChildren(ii->getGroupCond()); // TODO - ссылка?
				for (auto k = children.cbegin(); k != children.cend(); ++k)
				{
					ItemInfo* jj = *k;
					if (ui == *jj)       // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=139847  https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=62292
					{
						return jj;
					}
				}
			}
		}
		else
		{
			dcassert(ii);
		}
	}
	return nullptr;
}

void TransferView::onSpeakerAddItem(const UpdateInfo& ui)
{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
	LogManager::message("TRANSFER_ADD_ITEM: " + ui.dumpInfo(ui.hintedUser.user), false);
#endif
	int pos = -1;
	ItemInfo* foundItem = findItem(ui, pos);
	if (foundItem)
	{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
		LogManager::message("[!] TRANSFER_ADD_ITEM skip duplicate: " + ui.dumpInfo(ui.hintedUser.user), false);
#endif
		return;
	}
	dcassert(!ui.token.empty());
	if (ui.token.empty() || !ConnectionManager::tokenManager.isToken(ui.token))
	{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
		LogManager::message("[!] TRANSFER_ADD_ITEM skip missing token: " + ui.dumpInfo(ui.hintedUser.user), false);
#endif
		return;
	}
	ItemInfo* ii = new ItemInfo(ui);
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
	if (ui.token.empty())
		LogManager::message("[!] TRANSFER_ADD_ITEM empty token: " + ui.dumpInfo(ui.hintedUser.user), false);
#endif
	ii->update(ui);
	if (ii->download)
		ctrlTransfers.insertGroupedItem(ii, false, false, true);
	else
		ctrlTransfers.insertItem(ii, IMAGE_UPLOAD);
}

void TransferView::addTask(Tasks s, Task* task)
{
	bool firstItem;
	uint64_t tick = GET_TICK();
	uint64_t prevTick = tick;
	if (tasks.add(s, task, firstItem, prevTick) && prevTick + TIMER_VAL < tick)
		PostMessage(WM_SPEAKER);
}

void TransferView::processTasks()
{
	TaskQueue::List t;
	tasks.get(t);
	if (t.empty()) return;
		
	CLockRedraw<> lockRedraw(ctrlTransfers);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		switch (i->first)
		{
			case TRANSFER_ADD_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
				onSpeakerAddItem(ui);
			}
			break;

			case TRANSFER_REMOVE_TOKEN_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
				unsigned removedCount = 0;
				LogManager::message("TRANSFER_REMOVE_TOKEN_ITEM: token=" + ui.token, false);
#endif
				for (int j = 0; j < ctrlTransfers.GetItemCount(); ++j)
				{
					ItemInfo* ii = ctrlTransfers.getItemData(j);

					bool found = ii->token == ui.token;
					if (!found)
					{
						if (
						    //!ii->token.empty() && !ui.token.empty() && ConnectionManager::g_tokens_manager.isToken(ui.token) == false ||
						    ConnectionManager::tokenManager.getTokenCount() == 0)
						{
							found = true;
							//dcassert(0);
#ifdef _DEBUG
							LogManager::message("TRANSFER_REMOVE_TOKEN_ITEM [!] Force ui.token = " + ui.token);
#endif
						}
					}
					if (found)
					{
						ctrlTransfers.removeGroupedItem(ii);
						j = 0;
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
						removedCount++;
#endif
					}
				}
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
				LogManager::message("TRANSFER_REMOVE_TOKEN_ITEM: removedCount=" + Util::toString(removedCount), false);
#endif
			}
			break;

			case TRANSFER_REMOVE_DOWNLOAD_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
				LogManager::message("TRANSFER_REMOVE_DOWNLOAD_ITEM: " + ui.dumpInfo(ui.hintedUser.user), false);
#endif
				dcassert(!ui.target.empty());
				if (!ui.target.empty())
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					unsigned removedCount = 0;
#endif
					for (int j = 0; j < ctrlTransfers.GetItemCount(); ++j)
					{
						ItemInfo* ii = ctrlTransfers.getItemData(j);
						if (ui.download && ii->target == ui.target)
						{
							ctrlTransfers.removeGroupedItem(ii);
							j = 0;
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
							removedCount++;
#endif
						}
					}
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					LogManager::message("TRANSFER_REMOVE_DOWNLOAD_ITEM: removedCount=" + Util::toString(removedCount), false);
#endif
				}
			}
			break;

			case TRANSFER_REMOVE_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
				int pos = -1;
				ItemInfo* ii = findItem(ui, pos);
				if (ii)
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					LogManager::message("TRANSFER_REMOVE_ITEM found: " + ui.dumpInfo(ii->getUser()), false);
#endif
					if (ui.download)
					{
						ctrlTransfers.removeGroupedItem(ii);
					}
					else
					{
						dcassert(pos != -1);
						ctrlTransfers.DeleteItem(pos);
						delete ii;
					}
				}
				else
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					LogManager::message("[!] TRANSFER_REMOVE_ITEM not found: " + ui.dumpInfo(ui.hintedUser.user), false);
#endif
				}
			}
			break;

			case TRANSFER_UPDATE_ITEM:
			{
				auto &ui = static_cast<UpdateInfo&>(*i->second);
				int pos = -1;
				ItemInfo* ii = findItem(ui, pos);
				if (ii)
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					LogManager::message("TRANSFER_UPDATE_ITEM found: " + ui.dumpInfo(ii->getUser()), false);
#endif
					if (ui.download)
					{
						const ItemInfo* parent = ii->parent ? ii->parent : ii;
						{
							if (!ui.token.empty())
							{
								if (!ConnectionManager::tokenManager.isToken(ui.token))
								{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
									LogManager::message("[!] TRANSFER_UPDATE_ITEM bad token: token=" + ui.token, false);
#endif
									//UpdateInfo* l_remove_ui = new UpdateInfo(HintedUser(), true); // Костыль
									//l_remove_ui->setToken(ui.token);
									//addTask(TRANSFER_REMOVE_TOKEN_ITEM, l_remove_ui);
									break;
								}
							}
						}
#if 1
						if (ui.type == Transfer::TYPE_FILE || ui.type == Transfer::TYPE_TREE)
						{
							/* parent item must be updated with correct info about whole file */
							if (ui.status == ItemInfo::STATUS_RUNNING && parent->status == ItemInfo::STATUS_RUNNING && parent->hits == -1)
							{
								// - пропадает из прогресса - не врубать
								// TODO - ui.updateMask &= ~UpdateInfo::MASK_TOKEN;
								ui.updateMask &= ~UpdateInfo::MASK_SIZE;
								ui.updateMask &= ~UpdateInfo::MASK_POS;
								ui.updateMask &= ~UpdateInfo::MASK_ACTUAL;
								ui.updateMask &= ~UpdateInfo::MASK_STATUS_STRING;
								ui.updateMask &= ~UpdateInfo::MASK_ERROR_STATUS_STRING;
								ui.updateMask &= ~UpdateInfo::MASK_TIMELEFT;
							}
							else
							{
								//dcassert(0);
							}
						}
#endif						
						/* if target has changed, regroup the item */
						bool changeParent = false;
						changeParent = (ui.updateMask & UpdateInfo::MASK_FILE) && ui.target != ii->target;
						if (changeParent)
						{
							ctrlTransfers.removeGroupedItem(ii, false);
						}
						
						ii->update(ui);
						
						if (changeParent)
						{
							ctrlTransfers.insertGroupedItem(ii, false, false, true);
							parent = ii->parent ? ii->parent : ii;
						}
						else if (ii == parent || !parent->collapsed)
						{
							const auto l_pos = ctrlTransfers.findItem(ii);
							//dcassert(l_pos == pos);
							updateItem(l_pos, ui.updateMask);
						}
						break;
					}
					ii->update(ui);
					dcassert(pos != -1);
					updateItem(pos, ui.updateMask);
				}
				else
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					LogManager::message("[!] TRANSFER_UPDATE_ITEM not found: " + ui.dumpInfo(ui.hintedUser.user), false);
#endif
					if (!ui.target.empty())
						onSpeakerAddItem(ui);
				}
			}
			break;

			case TRANSFER_UPDATE_PARENT:
			{
				auto& ui = static_cast<UpdateInfo&>(*i->second);
				const ItemInfoList::ParentPair* pp = ctrlTransfers.findParentPair(ui.target);
				
				if (!pp)
					break;

				if (ui.hintedUser.user)
				{
					int pos = -1;
					ItemInfo* ii = findItem(ui, pos);
					if (ii)
					{
						ii->status = ui.status;
						ii->statusString = ui.statusString;
						ii->errorStatusString = ui.errorStatusString;
						if (!pp->parent->collapsed)
						{
							updateItem(ctrlTransfers.findItem(ii), ui.updateMask);
						}
					}
				}
				
				pp->parent->update(ui);
				updateItem(ctrlTransfers.findItem(pp->parent), ui.updateMask);
			}
			break;

			case TRANSFER_UPDATE_TOKEN_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
				LogManager::message("TRANSFER_UPDATE_TOKEN_ITEM: token=" + ui.token, false);
#endif
				int count = ctrlTransfers.GetItemCount();
				for (int j = 0; j < count; ++j)
				{
					ItemInfo* ii = ctrlTransfers.getItemData(j);
					if (ii->token == ui.token)
					{
						ii->update(ui);
						updateItem(j, ui.updateMask);
						break;
					}
				}
				break;
			}

			default:
				dcassert(0);
				break;
		};
		delete i->second;  // [1] https://www.box.net/shared/307aa981b9cef05fc096
	}
	if (shouldSort && !MainFrame::isAppMinimized())
	{
		shouldSort = false;
		ctrlTransfers.resort();
	}
}

LRESULT TransferView::onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlTransfers.getItemData(i);
		if (ii && !ii->tth.isZero())
			WinUtil::searchHash(ii->tth);
	}
	return 0;
}

void TransferView::ItemInfo::update(const UpdateInfo& ui)
{
	if (ui.type != Transfer::TYPE_LAST)
		type = ui.type;
		
	if (ui.updateMask & UpdateInfo::MASK_STATUS)
	{
		status = ui.status;
	}
	if (ui.updateMask & UpdateInfo::MASK_ERROR_STATUS_STRING)
	{
		errorStatusString = ui.errorStatusString;
	}
	if (ui.updateMask & UpdateInfo::MASK_TOKEN)
	{
		token = ui.token;
	}
	if (ui.updateMask & UpdateInfo::MASK_STATUS_STRING)
	{
		// No slots etc from transfermanager better than disconnected from connectionmanager
		if (!transferFailed)
			statusString = ui.statusString;
		transferFailed = ui.transferFailed;
	}
	if (ui.updateMask & UpdateInfo::MASK_SIZE)
	{
		size = ui.size;
	}
	if (ui.updateMask & UpdateInfo::MASK_POS)
	{
		pos = ui.pos;
	}
	if (ui.updateMask & UpdateInfo::MASK_ACTUAL)
	{
		actual = ui.actual;
	}
	if (ui.updateMask & UpdateInfo::MASK_SPEED)
	{
		speed = ui.speed;
	}
	if (ui.updateMask & UpdateInfo::MASK_FILE)
	{
		target = ui.target;
	}
	if (ui.updateMask & UpdateInfo::MASK_TIMELEFT)
	{
		timeLeft = ui.timeLeft;
	}
	if (ui.updateMask & UpdateInfo::MASK_IP)
	{
		dcassert(!ui.m_ip.empty());
		if (!transferIp.type)
		{
			IpAddress ip;
			if (Util::parseIpAddress(ip, Text::fromT(ui.m_ip)))
			{
				transferIp = ip;
				if (!(ipInfo.known & IPInfo::FLAG_P2P_GUARD))
					Util::getIpInfo(transferIp, ipInfo, IPInfo::FLAG_P2P_GUARD);
			}
#ifdef FLYLINKDC_USE_COLUMN_RATIO
			ratioText.clear();
			ui.hintedUser.user->loadIPStat();
			uint64_t bytes[2];
			ui.hintedUser.user->getBytesTransfered(bytes);
			if (bytes[0] + bytes[1])
			{
				ratioText = Util::toStringT(bytes[0] ? ((double) bytes[1] / (double) bytes[0]) : 0);
				ratioText += _T(" (");
				ratioText += Util::formatBytesT(bytes[1]);
				ratioText += _T('/');
				ratioText += Util::formatBytesT(bytes[0]);
				ratioText += _T(")");
			}
#endif
#ifdef FLYLINKDC_USE_DNS
			columns[COLUMN_DNS] = ui.dns; // !SMT!-IP
#endif
		}
	}
	if (ui.updateMask & UpdateInfo::MASK_CIPHER)
	{
		cipher = ui.cipher;
	}
	if (ui.updateMask & UpdateInfo::MASK_SEGMENT)
	{
		running = ui.running;
	}
	if (ui.updateMask & UpdateInfo::MASK_USER)
	{
		if (!hintedUser.equals(ui.hintedUser))
		{
			hintedUser = ui.hintedUser;
			updateNicks();
		}
	}
	if (ui.updateMask & UpdateInfo::MASK_TTH)
	{
		tth = ui.tth;
		if (parent) parent->tth = tth;
	}
}

void TransferView::updateItem(int ii, uint32_t updateMask)
{
	if (updateMask & UpdateInfo::MASK_STATUS ||
	    updateMask & UpdateInfo::MASK_STATUS_STRING ||
	    updateMask & UpdateInfo::MASK_ERROR_STATUS_STRING ||
	    updateMask & UpdateInfo::MASK_TOKEN ||
	    updateMask & UpdateInfo::MASK_POS ||
	    updateMask & UpdateInfo::MASK_ACTUAL)
	{
		ctrlTransfers.updateItem(ii, COLUMN_STATUS);
	}
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	if (updateMask & UpdateInfo::MASK_POS || updateMask & UpdateInfo::MASK_ACTUAL)
	{
		ctrlTransfers.updateItem(ii, COLUMN_RATIO);
	}
#endif
	if (updateMask & UpdateInfo::MASK_SIZE)
	{
		ctrlTransfers.updateItem(ii, COLUMN_SIZE);
	}
	if (updateMask & UpdateInfo::MASK_SPEED)
	{
		ctrlTransfers.updateItem(ii, COLUMN_SPEED);
	}
	if (updateMask & UpdateInfo::MASK_FILE)
	{
		ctrlTransfers.updateItem(ii, COLUMN_PATH);
		ctrlTransfers.updateItem(ii, COLUMN_FILE);
	}
	if (updateMask & UpdateInfo::MASK_TIMELEFT)
	{
		ctrlTransfers.updateItem(ii, COLUMN_TIMELEFT);
	}
	if (updateMask & UpdateInfo::MASK_IP)
	{
		ctrlTransfers.updateItem(ii, COLUMN_IP);
		//ctrlTransfers.updateItem(ii, COLUMN_LOCATION);
	}
	if (updateMask & UpdateInfo::MASK_SEGMENT)
	{
		ctrlTransfers.updateItem(ii, COLUMN_HUB);
	}
	if (updateMask & UpdateInfo::MASK_CIPHER)
	{
		ctrlTransfers.updateItem(ii, COLUMN_CIPHER);
	}
	if (updateMask & UpdateInfo::MASK_USER)
	{
		ctrlTransfers.updateItem(ii, COLUMN_USER);
	}
}

TransferView::UpdateInfo* TransferView::createUpdateInfoForAddedEvent(const HintedUser& hintedUser, bool isDownload, const string& token)
{
	UpdateInfo* ui = new UpdateInfo(hintedUser, isDownload);
	dcassert(!token.empty());
	ui->setToken(token);
	if (ui->download)
	{
		string target;
		int64_t size = 0;
		int flags = 0;
		if (QueueManager::getQueueInfo(hintedUser.user, target, size, flags)) // deadlock
		{
			Transfer::Type type = Transfer::TYPE_FILE;
			if (flags & QueueItem::FLAG_USER_LIST)
				type = Transfer::TYPE_FULL_LIST;
			else if (flags & QueueItem::FLAG_DCLST_LIST)
				type = Transfer::TYPE_FULL_LIST;
			else if (flags & QueueItem::FLAG_PARTIAL_LIST)
				type = Transfer::TYPE_PARTIAL_LIST;
				
			ui->setType(type);
			ui->setTarget(target);
			ui->setSize(size);
		}
		else
		{
#ifdef _DEBUG
			LogManager::message("Skip TransferView::createUpdateInfoForAddedEvent - hintedUser.user not found: " + hintedUser.user->getLastNick());
#endif
			delete ui;
			return nullptr;
		}
	}
	
	ui->setStatus(ItemInfo::STATUS_WAITING);
	const string status = STRING(CONNECTING);
	ui->setStatusString(Text::toT(status));
	return ui;
}

void TransferView::on(ConnectionManagerListener::Added, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!token.empty());
	auto task = createUpdateInfoForAddedEvent(hintedUser, isDownload, token);
	if (task) addTask(TRANSFER_ADD_ITEM, task);
}

void TransferView::on(ConnectionManagerListener::ConnectionStatusChanged, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!token.empty());
	auto task = createUpdateInfoForAddedEvent(hintedUser, isDownload, token);
	if (task) addTask(TRANSFER_UPDATE_ITEM, task);
}

void TransferView::on(ConnectionManagerListener::UserUpdated, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!token.empty());
	auto task = createUpdateInfoForAddedEvent(hintedUser, isDownload, token);
	if (task) addTask(TRANSFER_UPDATE_ITEM, task);
}

void TransferView::on(ConnectionManagerListener::Removed, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!token.empty());
	auto item = new UpdateInfo(hintedUser, isDownload);
	item->setToken(token);
	addTask(TRANSFER_REMOVE_ITEM, item);
}

void TransferView::on(ConnectionManagerListener::FailedDownload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
#ifdef _DEBUG
		LogManager::message("ConnectionManagerListener::FailedDownload user = " + hintedUser.user->getLastNick() + " Reason = " + reason);
#endif
		UpdateInfo* ui = new UpdateInfo(hintedUser, true);
		dcassert(!token.empty());
		ui->setToken(token);
#ifdef FLYLINKDC_USE_IPFILTER
		const auto userFlags = ui->hintedUser.user->getFlags();
		if (userFlags & (User::PG_IPTRUST_BLOCK | User::PG_IPGUARD_BLOCK | User::PG_P2PGUARD_BLOCK))
		{
			string status = STRING(CONNECTION_BLOCKED);
			if (userFlags & User::PG_IPTRUST_BLOCK)
				status += " [IPTrust.ini]";
			if (userFlags & User::PG_IPGUARD_BLOCK)
				status += " [IPGuard.ini]";
			if (userFlags & User::PG_P2PGUARD_BLOCK)
				status += " [P2PGuard.ini]";
			ui->setErrorStatusString(Text::toT(status + " [" + reason + "]"));
		}
		else
#endif
		{
			ui->setErrorStatusString(Text::toT(reason));
		}
		
		ui->setStatus(ItemInfo::STATUS_WAITING);
		addTask(TRANSFER_UPDATE_ITEM, ui);
	}
}

void TransferView::on(ConnectionManagerListener::FailedUpload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept
{
	UpdateInfo* ui = new UpdateInfo(hintedUser, false);
	ui->setToken(token);
	ui->setStatusString(Text::toT(reason));
	addTask(TRANSFER_UPDATE_TOKEN_ITEM, ui);
}

void TransferView::on(ConnectionManagerListener::ListenerStarted) noexcept
{
	::PostMessage(*MainFrame::getMainFrame(), WMU_LISTENER_INIT, 0, 0);
}

void TransferView::on(ConnectionManagerListener::ListenerFailed, const char* type, int af, int errorCode, const string& errorText) noexcept
{
	MainFrame::ListenerError* error = new MainFrame::ListenerError;
	error->type = type;
	error->af = af;
	error->errorCode = errorCode;
	error->errorText = errorText;
	::PostMessage(*MainFrame::getMainFrame(), WMU_LISTENER_INIT, 1, reinterpret_cast<LPARAM>(error));
}

static tstring getFile(Transfer::Type type, const tstring& fileName)
{
	switch (type)
	{
		case Transfer::TYPE_FULL_LIST:
			return TSTRING(FILE_LIST);
		case Transfer::TYPE_PARTIAL_LIST:
			return TSTRING(PARTIAL_FILE_LIST);
		default:
			return fileName;
	}
}

void TransferView::ItemInfo::updateNicks()
{
	if (hintedUser.user)
	{
		nicks = WinUtil::getNicks(hintedUser);
		hubs = WinUtil::getHubNames(hintedUser).first;
	}
}

int64_t TransferView::ItemInfo::getPos() const
{
	if (pos < 0) return 0;
	if (pos > size) return size;
	return pos;
}

const tstring TransferView::ItemInfo::getText(uint8_t col) const
{
	if (ClientManager::isBeforeShutdown())
		return Util::emptyStringT;
	switch (col)
	{
		case COLUMN_USER:
			return hits == -1 ? nicks : (Util::toStringT(hits) + _T(' ') + TSTRING(USERS));
		case COLUMN_HUB:
			return hits == -1 ? hubs : (Util::toStringT(running) + _T(' ') + TSTRING(NUMBER_OF_SEGMENTS));
		case COLUMN_STATUS:
		{
			if (status != STATUS_RUNNING && !errorStatusString.empty())
				return statusString + _T(" [") + errorStatusString  + _T("]");
			return statusString;
		}
		case COLUMN_TIMELEFT:
			//dcassert(timeLeft >= 0);
			return (status == STATUS_RUNNING && timeLeft > 0) ? Util::formatSecondsT(timeLeft) : Util::emptyStringT;
		case COLUMN_SPEED:
			return status == STATUS_RUNNING ? (Util::formatBytesT(speed) + _T('/') + TSTRING(S)) : Util::emptyStringT;
		case COLUMN_FILE:
			return getFile(type, Util::getFileName(target));
		case COLUMN_SIZE:
			return size >= 0 ? Util::formatBytesT(size) : Util::emptyStringT;
		case COLUMN_PATH:
			return Util::getFilePath(target);
		case COLUMN_IP:
			return Util::printIpAddressT(transferIp);
#ifdef FLYLINKDC_USE_COLUMN_RATIO
		case COLUMN_RATIO:
			return ratioText;
#endif
		case COLUMN_CIPHER:
			if (token.empty()) return cipher;
			return cipher + _T(" [Token: ") + Text::toT(token) + _T("]");
		case COLUMN_SHARE:
			return hintedUser.user ? Util::formatBytesT(hintedUser.user->getBytesShared()) : Util::emptyStringT;
		case COLUMN_SLOTS:
			return hintedUser.user ? Util::toStringT(hintedUser.user->getSlots()) : Util::emptyStringT;
		case COLUMN_P2P_GUARD:
			return ipInfo.p2pGuard.empty() ? Util::emptyStringT : Text::toT(ipInfo.p2pGuard);
		case COLUMN_LOCATION:
		{
			const string& description = Util::getDescription(ipInfo);
			return description.empty() ? Util::emptyStringT : Text::toT(description);
		}
		default:
			return Util::emptyStringT;
	}
}

void TransferView::starting(UpdateInfo* ui, const Transfer* t)
{
	ui->setTarget(t->getPath());
	ui->setType(t->getType());
	ui->setCipher(Text::toT(t->getCipherName()));
	const string& token = t->getConnectionQueueToken();
	dcassert(!token.empty());
	ui->setToken(token);
	ui->setIP(t->getIP());
}

void TransferView::on(DownloadManagerListener::Requesting, const DownloadPtr& download) noexcept
{
	UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true);
	starting(ui, download.get());
	ui->setPos(download->getPos());
	ui->setActual(download->getActual());
	ui->setSize(download->getSize());
	ui->setStatus(ItemInfo::STATUS_RUNNING);
	ui->setTarget(download->getPath());
	if (download->getType() == Download::TYPE_FILE)
		ui->setTTH(download->getTTH());
	ui->updateMask &= ~UpdateInfo::MASK_STATUS; // hack to avoid changing item status
	ui->setStatusString(TSTRING(REQUESTING) + _T(' ') + getFile(download->getType(), Text::toT(Util::getFileName(download->getPath()))) + _T("..."));
	const string& token = download->getConnectionQueueToken();
	dcassert(!token.empty());
	ui->setToken(token);
	addTask(TRANSFER_UPDATE_ITEM, ui);
}

#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
void TransferView::on(DownloadManagerListener::Starting, const DownloadPtr& download) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true); // [!] IRainman fix.
		
		ui->setStatus(ItemInfo::STATUS_RUNNING);
		ui->setStatusString(TSTRING(DOWNLOAD_STARTING));
		ui->setTarget(download->getPath());
		ui->setType(download->getType());
		// [-] ui->setIP(download->getUserConnection()->getRemoteIp()); // !SMT!-IP [-] IRainman opt.
		const auto token = download->getConnectionQueueToken();
		dcassert(!token.empty());
		ui->setToken(token);
		
		addTask(TRANSFER_UPDATE_ITEM, ui);
	}
}
#endif // FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE

void TransferView::on(DownloadManagerListener::Failed, const DownloadPtr& download, const string& reason) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true, true);
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setSize(download->getSize());
		ui->setTarget(download->getPath());
		ui->setType(download->getType());
		// [-] ui->setIP(download->getUserConnection()->getRemoteIp()); // !SMT!-IP [-] IRainman opt.
		const auto token = download->getConnectionQueueToken();
		dcassert(!token.empty());
		ui->setToken(token); // fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1671
		
		tstring tmpReason = Text::toT(reason);
		if (download->isSet(Download::FLAG_SLOWUSER))
		{
			tmpReason += _T(": ") + TSTRING(SLOW_USER);
		}
		else if (download->getOverlapped() && !download->isSet(Download::FLAG_OVERLAP))
		{
			tmpReason += _T(": ") + TSTRING(OVERLAPPED_SLOW_SEGMENT);
		}
		
		ui->setStatusString(tmpReason);
		
		SHOW_POPUPF(POPUP_ON_DOWNLOAD_FAILED,
		            TSTRING(FILE) + _T(": ") + Util::getFileName(ui->target) + _T('\n') +
		            TSTRING(USER) + _T(": ") + WinUtil::getNicks(ui->hintedUser) + _T('\n') +
		            TSTRING(REASON) + _T(": ") + tmpReason, TSTRING(DOWNLOAD_FAILED) + _T(' '), NIIF_WARNING);
		addTask(TRANSFER_UPDATE_ITEM, ui);
	}
}

static tstring getReasonText(int error)
{
	switch (error)
	{
		case QueueManager::ERROR_NO_NEEDED_PART:
			return TSTRING(NO_NEEDED_PART);
		case QueueManager::ERROR_FILE_SLOTS_TAKEN:
			return TSTRING(ALL_FILE_SLOTS_TAKEN);
		case QueueManager::ERROR_DOWNLOAD_SLOTS_TAKEN:
			return TSTRING(ALL_DOWNLOAD_SLOTS_TAKEN);
		case QueueManager::ERROR_NO_FREE_BLOCK:
			return TSTRING(NO_FREE_BLOCK);
	}
	return Util::emptyStringT;
}

void TransferView::on(DownloadManagerListener::Status, const UserConnection* conn, const Download::ErrorInfo& status) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		tstring reasonText = getReasonText(status.error);
		if (reasonText.empty()) return;
		UpdateInfo* ui = new UpdateInfo(conn->getHintedUser(), true);
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setStatusString(reasonText);
		ui->setSize(status.size);
		ui->setTarget(status.target);
		ui->setType(status.type);
		const string& token = conn->getConnectionQueueToken();
		dcassert(!token.empty());
		ui->setToken(token);
		addTask(TRANSFER_UPDATE_ITEM, ui);
	}
}

void TransferView::on(UploadManagerListener::Starting, const UploadPtr& upload) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(upload->getHintedUser(), false);
		starting(ui, upload.get());
		ui->setPos(upload->getAdjustedPos());
		ui->setActual(upload->getAdjustedActual());
		ui->setSize(upload->getType() == Transfer::TYPE_TREE ? upload->getSize() : upload->getFileSize());
		ui->setTarget(upload->getPath());
		ui->setStatus(ItemInfo::STATUS_RUNNING);
		ui->setRunning(1);
		if (upload->getType() == Transfer::TYPE_FILE)
			ui->setTTH(upload->getTTH());

		if (!upload->isSet(Upload::FLAG_RESUMED))
			ui->setStatusString(TSTRING(UPLOAD_STARTING));

		const string& token = upload->getConnectionQueueToken();
		dcassert(!token.empty());
		ui->setToken(token);

		addTask(TRANSFER_UPDATE_ITEM, ui);
	}
}

void TransferView::on(DownloadManagerListener::Tick, const DownloadArray& dl) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (ConnectionManager::tokenManager.getTokenCount() == 0)
		{
			UpdateInfo* ui = new UpdateInfo(HintedUser(), true);
			addTask(TRANSFER_REMOVE_TOKEN_ITEM, ui);
		}
		
		if (!MainFrame::isAppMinimized())
		{
			for (auto j = dl.cbegin(); j != dl.cend(); ++j)
			{
				UpdateInfo* ui = new UpdateInfo(j->hintedUser, true);
				ui->setStatus(ItemInfo::STATUS_RUNNING);
				ui->setActual(j->actual);
				ui->setPos(j->pos);
				ui->setSpeed(j->speed);
				ui->setSize(j->size);
				ui->setTimeLeft(j->secondsLeft);
				ui->setType(Transfer::Type(j->type)); // TODO
				ui->setTarget(j->path);
				ui->setToken(j->token);
				ui->formatStatusString(j->transferFlags, j->startTime);
				dcassert(!j->token.empty());
				addTask(TRANSFER_UPDATE_ITEM, ui);
			}
		}
	}
}

void TransferView::on(UploadManagerListener::Tick, const UploadArray& ul) noexcept
{
	if (!ClientManager::isBeforeShutdown() && !MainFrame::isAppMinimized())
	{
		for (auto j = ul.cbegin(); j != ul.cend(); ++j)
		{
			if (j->pos == 0)
			{
				dcassert(0);
				continue;
			}
			UpdateInfo* ui = new UpdateInfo(j->hintedUser, false);
			ui->setStatus(ItemInfo::STATUS_RUNNING);
			ui->setActual(j->actual);
			ui->setPos(j->pos);
			ui->setSize(j->size);
			ui->setTimeLeft(j->secondsLeft);
			ui->setSpeed(j->speed);
			ui->setType(Transfer::Type(j->type)); // TODO
			ui->setTarget(j->path);
			ui->setToken(j->token);
			ui->formatStatusString(j->transferFlags, j->startTime);
			dcassert(!j->token.empty());
			addTask(TRANSFER_UPDATE_ITEM, ui);
		}
	}
}

void TransferView::onTransferComplete(const Transfer* t, const bool download, const string& fileName, bool failed)
{
	if (t->getType() == Transfer::TYPE_TREE)
		return;
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(t->getHintedUser(), download);
		
		ui->setTarget(t->getPath());
		//ui->setToken(t->getConnectionToken());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		/*
		    if (t->getType() == Transfer::TYPE_FULL_LIST)
		    {
		        ui->setPos(t->getSize());
		    }
		    else
		    {
		        ui->setPos(0);
		    }
		*/
		ui->setPos(0);
		ui->setActual(t->getFileSize());
		ui->setSize(t->getFileSize());
		ui->setTimeLeft(0);
		ui->setRunning(0);
		if (!download && failed)
			ui->setStatusString(TSTRING(UNABLE_TO_SEND_FILE));
		else
			ui->setStatusString(download ? TSTRING(DOWNLOAD_FINISHED_IDLE) : TSTRING(UPLOAD_FINISHED_IDLE));
		const string& token = t->getConnectionQueueToken();
		dcassert(!token.empty());
		ui->setToken(token);
		if (!download && !failed)
		{
			SHOW_POPUP(POPUP_ON_UPLOAD_FINISHED,
			           TSTRING(FILE) + _T(": ") + Text::toT(fileName) + _T('\n') +
			           TSTRING(USER) + _T(": ") + WinUtil::getNicks(t->getHintedUser()), TSTRING(UPLOAD_FINISHED_IDLE));
		}
		
		addTask(TRANSFER_UPDATE_ITEM, ui);
	}
}

void TransferView::ItemInfo::disconnectAndBlock()
{
	if (transferIp.type == AF_INET)
	{
		auto dm = DatabaseManager::getInstance();
		dm->clearCachedP2PGuardData(transferIp.data.v4);
		auto conn = dm->getDefaultConnection();
		if (conn)
		{
			vector<P2PGuardData> data = { P2PGuardData(Text::fromT(nicks), transferIp.data.v4, transferIp.data.v4) };
			conn->saveP2PGuardData(data, DatabaseManager::PG_DATA_MANUAL, false);
		}
	}
	disconnect();
}

void TransferView::ItemInfo::disconnect()
{
	ConnectionManager::getInstance()->disconnect(hintedUser.user, download);
}

LRESULT TransferView::onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlTransfers.getItemData(i);
		const string target = Text::fromT(ii->target);
		if (ii->download)
		{
			const auto qi = QueueManager::fileQueue.findTarget(target);
			if (qi) runPreview(wID, qi);
		}
		else
			runPreview(wID, target);
	}
	
	return 0;
}

void TransferView::collapseAll()
{
	for (int q = ctrlTransfers.GetItemCount() - 1; q != -1; --q)
	{
		ItemInfo* m = ctrlTransfers.getItemData(q);
		if (m->download)
		{
			if (m->parent)
			{
				ctrlTransfers.deleteItem(m);
			}
			else if (!m->collapsed)
			{
				m->collapsed = true;
				ctrlTransfers.SetItemState(ctrlTransfers.findItem(m), INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			}
		}
	}
}

void TransferView::expandAll()
{
	for (auto i = ctrlTransfers.getParents().cbegin(); i != ctrlTransfers.getParents().cend(); ++i)
	{
		ItemInfo* l = (*i).second.parent;
		if (l->collapsed)
		{
			ctrlTransfers.Expand(l, ctrlTransfers.findItem(l));
		}
	}
}

LRESULT TransferView::onDisconnectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlTransfers.getItemData(i);
		
		const vector<ItemInfo*>& children = ctrlTransfers.findChildren(ii->getGroupCond());
		for (auto j = children.cbegin(); j != children.cend(); ++j)
		{
			ItemInfo* jj = *j;
			jj->disconnect();
			
			int h = ctrlTransfers.findItem(jj);
			if (h != -1)
			{
				ctrlTransfers.SetItemText(h, COLUMN_STATUS, CTSTRING(DISCONNECTED));
			}
		}
		
		ctrlTransfers.SetItemText(i, COLUMN_STATUS, CTSTRING(DISCONNECTED));
	}
	return 0;
}

LRESULT TransferView::onSlowDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlTransfers.getItemData(i);
		if (ii->download)
		{
			const string tmp = Text::fromT(ii->target);
			
			QueueManager::LockFileQueueShared fileQueue;
			const auto& queue = fileQueue.getQueueL();
			const auto qi = queue.find(tmp);
			if (qi != queue.cend())
				qi->second->changeAutoDrop();
		}
	}
	
	return 0;
}

void TransferView::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		initProgressBars(true);
		if (ctrlTransfers.isRedraw())
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

void TransferView::on(QueueManagerListener::Tick, const QueueItemList& qil) noexcept
{
	if (!ClientManager::isBeforeShutdown() && !MainFrame::isAppMinimized())
		on(QueueManagerListener::StatusUpdatedList(), qil);
}

void TransferView::on(QueueManagerListener::StatusUpdatedList, const QueueItemList& qil) noexcept
{
	dcassert(!qil.empty());
	if (!ClientManager::isBeforeShutdown() && !qil.empty())
		for (auto i = qil.cbegin(); i != qil.cend(); ++i)
			on(QueueManagerListener::StatusUpdated(), *i);
}

void TransferView::on(QueueManagerListener::StatusUpdated, const QueueItemPtr& qi) noexcept
{
	if (qi->isUserList())
		return;
	auto ui = new UpdateInfo();
	parseQueueItemUpdateInfo(ui, qi);
	addTask(TRANSFER_UPDATE_PARENT, ui);
}

void TransferView::parseQueueItemUpdateInfo(UpdateInfo* ui, const QueueItemPtr& qi)
{
	ui->setTarget(qi->getTarget());
	ui->setType(Transfer::TYPE_FILE);

	if (qi->isRunning())
	{
		int transferFlags;
		const int64_t totalSpeed = qi->getAverageSpeed();
		const int16_t segs = qi->getTransferFlags(transferFlags);
		ui->setRunning(segs);
		if (segs > 0)
		{
			ui->setStatus(ItemInfo::STATUS_RUNNING);
			ui->setSize(qi->getSize());
			ui->setPos(qi->getDownloadedBytes());
			ui->setActual(qi->getDownloadedBytes());
			ui->setTimeLeft((totalSpeed > 0) ? ((ui->size - ui->pos) / totalSpeed) : 0);
			ui->setSpeed(totalSpeed);
			
			if (qi->getTimeFileBegin() == 0)
			{
				// file is starting
				qi->setTimeFileBegin(GET_TICK());
				ui->setStatusString(TSTRING(DOWNLOAD_STARTING));
				PLAY_SOUND(SOUND_BEGINFILE);
				SHOW_POPUP(POPUP_ON_DOWNLOAD_STARTED, TSTRING(FILE) + _T(": ") + Util::getFileName(ui->target), TSTRING(DOWNLOAD_STARTING));
			}
			else
			{
				ui->formatStatusString(transferFlags, qi->getTimeFileBegin());
				ui->setErrorStatusString(Util::emptyStringT);
			}
		}
	}
	else
	{
		qi->setTimeFileBegin(0);
		ui->setSize(qi->getSize());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setRunning(0);
	}
}

void TransferView::UpdateInfo::formatStatusString(int transferFlags, uint64_t startTime)
{
	int64_t elapsed = startTime ? (GET_TICK() - startTime) / 1000 : 0;
	statusString.clear();
	if (transferFlags & TRANSFER_FLAG_PARTIAL)
		statusString += _T("[P]");
	if (transferFlags & TRANSFER_FLAG_SECURE)
		statusString += (transferFlags & TRANSFER_FLAG_TRUSTED)? _T("[SSL+T]") : _T("[SSL+U]");
	if (transferFlags & TRANSFER_FLAG_TTH_CHECK)
		statusString += _T("[T]");
	if (transferFlags & TRANSFER_FLAG_COMPRESSED)
		statusString += _T("[Z]");
	if (transferFlags & TRANSFER_FLAG_CHUNKED)
		statusString += _T("[C]");
	if (!statusString.empty())
		statusString += _T(' ');
	double percent;
	if (pos >= size)
		percent = 100;
	else
		percent = double(pos) * 100 / double(size);
	if (transferFlags & TRANSFER_FLAG_DOWNLOAD)
		statusString += TSTRING_F(DOWNLOADED_BYTES_FMT, Util::formatBytesT(pos) % Util::toStringT(percent) % Util::formatSecondsT(elapsed));
	else
		statusString += TSTRING_F(UPLOADED_BYTES_FMT, Util::formatBytesT(pos) % Util::toStringT(percent) % Util::formatSecondsT(elapsed));
	updateMask |= MASK_STATUS_STRING;
}

void TransferView::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const DownloadPtr& download) noexcept
{

	if (!ClientManager::isBeforeShutdown())
	{
		if (qi->isUserList())
		{
			return;
		}
		
		const string& token = download->getConnectionQueueToken();
		dcassert(!token.empty());
		// update file item
		UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true);
		ui->setToken(token);
		ui->setTarget(qi->getTarget());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setStatusString(TSTRING(DOWNLOAD_FINISHED_IDLE));
		SHOW_POPUP(POPUP_ON_DOWNLOAD_FINISHED, TSTRING(FILE) + _T(": ") + Util::getFileName(ui->target), TSTRING(DOWNLOAD_FINISHED_IDLE));
		addTask(TRANSFER_UPDATE_PARENT, ui);
	}
}

void TransferView::on(DownloadManagerListener::RemoveToken, const string& token) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(HintedUser(), true);
		ui->setToken(token);
		addTask(TRANSFER_REMOVE_TOKEN_ITEM, ui);
	}
}

void TransferView::on(ConnectionManagerListener::RemoveToken, const string& token) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(HintedUser(), true);
		ui->setToken(token);
		addTask(TRANSFER_REMOVE_TOKEN_ITEM, ui);
	}
}

void TransferView::on(QueueManagerListener::Removed, const QueueItemPtr& qi) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UserList users;
		qi->getUsers(users);
		for (auto i = users.cbegin(); i != users.cend(); ++i)
		{
			dcassert(!qi->getTarget().empty());
			if (!qi->getTarget().empty())
			{
				UpdateInfo* ui = new UpdateInfo(HintedUser(*i, Util::emptyString), true);
				ui->setTarget(qi->getTarget());
				addTask(TRANSFER_REMOVE_DOWNLOAD_ITEM, ui);
			}
		}
	}
}

LRESULT TransferView::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring data;
	int i = -1, columnId = wID - IDC_COPY;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlTransfers.getItemData(i);
		if (ii)
		{
			tstring sdata;
			if (wID == IDC_COPY_TTH || wID == IDC_COPY_LINK || wID == IDC_COPY_WMLINK)
			{
				if (ii->tth.isZero()) continue;
				switch (wID)
				{
					case IDC_COPY_TTH:
						sdata = Text::toT(ii->tth.toBase32());
						break;
					case IDC_COPY_LINK:
						sdata = Text::toT(Util::getMagnet(ii->tth, Text::fromT(Util::getFileName(ii->target)), ii->size));
						break;
					default:
						sdata = Text::toT(Util::getWebMagnet(ii->tth, Text::fromT(Util::getFileName(ii->target)), ii->size));
				}
			}
			else if (columnId >= COLUMN_FIRST && columnId < COLUMN_LAST)
			{
				sdata = ii->getText(columnId);
			}
			if (!sdata.empty())
			{
				if (data.empty())
					data = std::move(sdata);
				else
				{
					data += _T("\r\n");
					data += sdata;
				}
			}
		}
	}
	WinUtil::setClipboard(data);
	return 0;
}

void TransferView::pauseSelectedTransfer(void)
{
	const ItemInfo* ii = ctrlTransfers.getItemData(ctrlTransfers.GetNextItem(-1, LVNI_SELECTED));
	if (ii)
	{
		const string target = Text::fromT(ii->target);
		QueueManager::getInstance()->setPriority(target, QueueItem::PAUSED, true);
	}
}

LRESULT TransferView::onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (BOOLSETTING(CONFIRM_DELETE))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return 0;
		if (checkState == BST_CHECKED) SET_SETTING(CONFIRM_DELETE, FALSE);
	}
	ctrlTransfers.forEachSelected(&ItemInfo::removeAll);
	return 0;
}

void TransferView::onTimerInternal()
{
	shouldSort = true;
	processTasks();
}

void TransferView::getSelectedUsers(vector<UserPtr>& v) const
{
	int i = ctrlTransfers.GetNextItem(-1, LVNI_SELECTED);
	while (i >= 0)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		v.push_back(ii->getUser());
		i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED);
	}
}

void TransferView::initProgressBars(bool check)
{
	ProgressBar::Settings settings;
	bool setColors = BOOLSETTING(PROGRESS_OVERRIDE_COLORS);
	settings.clrBackground = setColors ? SETTING(DOWNLOAD_BAR_COLOR) : GetSysColor(COLOR_HIGHLIGHT);
	settings.clrText = SETTING(PROGRESS_TEXT_COLOR_DOWN);
	settings.clrEmptyBackground = SETTING(PROGRESS_BACK_COLOR);
	settings.odcStyle = BOOLSETTING(PROGRESSBAR_ODC_STYLE);
	settings.odcBumped = BOOLSETTING(PROGRESSBAR_ODC_BUMPED);
	settings.depth = SETTING(PROGRESS_3DDEPTH);
	settings.setTextColor = BOOLSETTING(PROGRESS_OVERRIDE_COLORS2);
	if (!check || progressBar[0].get() != settings) progressBar[0].set(settings);
	progressBar[0].setWindowBackground(Colors::g_bgColor);

	if (setColors) settings.clrBackground = SETTING(PROGRESS_SEGMENT_COLOR);
	if (!check || progressBar[1].get() != settings) progressBar[1].set(settings);
	progressBar[1].setWindowBackground(Colors::g_bgColor);

	if (setColors) settings.clrBackground = SETTING(UPLOAD_BAR_COLOR);
	settings.clrText = SETTING(PROGRESS_TEXT_COLOR_UP);
	if (!check || progressBar[2].get() != settings) progressBar[2].set(settings);
	progressBar[2].setWindowBackground(Colors::g_bgColor);
}

TransferView::ItemInfo::ItemInfo() : hintedUser(HintedUser(nullptr, Util::emptyString)), download(true)
{
	init();
}

TransferView::ItemInfo::ItemInfo(const TransferView::UpdateInfo& ui) : hintedUser(ui.hintedUser), download(ui.download)
{
	init();
}

void TransferView::ItemInfo::init()
{
#ifdef _DEBUG
	++g_count_transfer_item;
#endif
	transferFailed = false;
	status = STATUS_WAITING;
	pos = 0;
	size = 0;
	actual = 0;
	speed = 0;
	timeLeft = 0;
	collapsed = true;
	parent = nullptr;
	hits = -1;
	running = 0;
	type = Transfer::TYPE_FILE;
	transferIp.type = 0;
	updateNicks();
}

#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
string TransferView::UpdateInfo::dumpInfo(const UserPtr& user) const
{
	string s;
	if (user)
		s += "user=" + user->getLastNick() + ' ';
	s += "download=" + Util::toString(download);
	s += " token=" + token;
	s += " target=\"" + Text::fromT(target) + '"';
	s += " status=\"" + Text::fromT(statusString) + '"';
	s += " type=" + Util::toString(type);
	if (!errorStatusString.empty())
		s += " error=\"" + Text::fromT(errorStatusString) + '"';
	return s;
}
#endif
