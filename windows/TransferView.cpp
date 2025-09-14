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
#include "../client/SettingsManager.h"
#include "../client/ConnectionManager.h"
#include "../client/DownloadManager.h"
#include "../client/UploadManager.h"
#include "../client/QueueManager.h"
#include "../client/ThrottleManager.h"
#include "../client/DatabaseManager.h"
#include "../client/FavoriteManager.h"
#include "../client/UserManager.h"
#include "../client/PathUtil.h"
#include "../client/FormatUtil.h"
#include "../client/Util.h"
#include "../client/ConfCore.h"

#include "TransferView.h"
#include "UserTypeColors.h"
#include "MainFrm.h"
#include "QueueFrame.h"
#include "WaitingUsersFrame.h"
#include "ExMessageBox.h"
#include "NotifUtil.h"
#include "MenuHelper.h"

static const unsigned TIMER_VAL = 200;

#ifdef _DEBUG
std::atomic<long> TransferView::ItemInfo::g_count_transfer_item;
#endif

const int TransferView::columnId[] =
{
	COLUMN_USER,
	COLUMN_STATUS,
	COLUMN_TIME_LEFT,
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
	COLUMN_SLOTS,
	COLUMN_DEBUG_INFO
};

static const int columnSizes[] =
{
	150, // COLUMN_USER
	280, // COLUMN_STATUS
	75,  // COLUMN_TIME_LEFT
	90,  // COLUMN_SPEED
	175, // COLUMN_FILE
	85,  // COLUMN_SIZE
	200, // COLUMN_PATH
	150, // COLUMN_HUB
	100, // COLUMN_IP
	120, // COLUMN_LOCATION
	140, // COLUMN_P2P_GUARD
	90,  // COLUMN_CIPHER
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	50,  // COLUMN_RATIO
#endif
	85,  // COLUMN_SHARE
	75,  // COLUMN_SLOTS
	80   // COLUMN_DEBUG_INFO
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
	ResourceManager::SLOTS,
	ResourceManager::DEBUG_INFO
};

template<>
string UserInfoBaseHandlerTraitsUser<UserPtr>::getNick(const UserPtr& user)
{
	return user->getLastNick();
}

TransferView::TransferView() : timer(m_hWnd), shouldSort(false), onlyActiveUploads(false)
{
	ctrlTransfers.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlTransfers.setColumnFormat(COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlTransfers.setColumnFormat(COLUMN_TIME_LEFT, LVCFMT_RIGHT);
	ctrlTransfers.setColumnFormat(COLUMN_SPEED, LVCFMT_RIGHT);
	topUploadSpeed = topDownloadSpeed = 0;
}

TransferView::~TransferView()
{
	OperaColors::clearCache();
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

	const auto* ss = SettingsManager::instance.getUiSettings();
	ctrlTransfers.insertColumns(Conf::TRANSFER_FRAME_ORDER, Conf::TRANSFER_FRAME_WIDTHS, Conf::TRANSFER_FRAME_VISIBLE);
	ctrlTransfers.setSortFromSettings(ss->getInt(Conf::TRANSFER_FRAME_SORT));

	setListViewColors(ctrlTransfers);

	ctrlTransfers.SetImageList(g_transferArrowsImage.getIconList(), LVSIL_SMALL);
	onlyActiveUploads = ss->getBool(Conf::TRANSFERS_ONLY_ACTIVE_UPLOADS);

	ConnectionManager::getInstance()->addListener(this);
	DownloadManager::getInstance()->addListener(this);
	UploadManager::getInstance()->addListener(this);
	FavoriteManager::getInstance()->addListener(this);
	UserManager::getInstance()->addListener(this);
	SettingsManager::instance.addListener(this);
	timer.createTimer(TIMER_VAL, 4);
	return 0;
}

void TransferView::createMenus()
{
	if (!copyMenu)
	{
		copyMenu.CreatePopupMenu();
		for (size_t i = 0; i < COLUMN_LAST; ++i)
			copyMenu.AppendMenu(MF_STRING, IDC_COPY + columnId[i], CTSTRING_I(columnNames[i]));
		copyMenu.AppendMenu(MF_SEPARATOR);
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(COPY_TTH));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
		MenuHelper::addStaticMenu(copyMenu);
	}
	if (!segmentedMenu)
	{
		segmentedMenu.CreatePopupMenu();
		segmentedMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
		segmentedMenu.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(OPEN_DOWNLOAD_QUEUE), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD_QUEUE, 0));
		appendPreviewItems(segmentedMenu);
		segmentedMenu.AppendMenu(MF_POPUP, copyMenu, CTSTRING(COPY_USER_INFO), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
		segmentedMenu.AppendMenu(MF_SEPARATOR);
		segmentedMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSE), g_iconBitmaps.getBitmap(IconBitmaps::PAUSE, 0));
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
		segmentedMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
#endif
		segmentedMenu.AppendMenu(MF_SEPARATOR);
		segmentedMenu.AppendMenu(MF_STRING, IDC_CONNECT_ALL, CTSTRING(CONNECT_ALL));
		segmentedMenu.AppendMenu(MF_STRING, IDC_DISCONNECT_ALL, CTSTRING(DISCONNECT_ALL));
		segmentedMenu.AppendMenu(MF_SEPARATOR);
		segmentedMenu.AppendMenu(MF_STRING, IDC_EXPAND_ALL, CTSTRING(EXPAND_ALL));
		segmentedMenu.AppendMenu(MF_STRING, IDC_COLLAPSE_ALL, CTSTRING(COLLAPSE_ALL));
#if 0
		segmentedMenu.AppendMenu(MF_SEPARATOR);
		segmentedMenu.AppendMenu(MF_STRING, IDC_REMOVEALL, CTSTRING(REMOVE_ALL));
#endif
	}
}

void TransferView::destroyMenus()
{
	if (copyMenu)
	{
		MenuHelper::removeStaticMenu(copyMenu);
		copyMenu.DestroyMenu();
	}
	if (segmentedMenu)
	{
		MenuHelper::unlinkStaticMenus(segmentedMenu);
		segmentedMenu.DestroyMenu();
	}
}

LRESULT TransferView::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	timer.destroyTimer();
	ctrlTransfers.deleteAll();
	destroyMenus();
	return 0;
}

void TransferView::prepareClose()
{
	timer.destroyTimer();
	tasks.setDisabled(true);
	tasks.clear();

	auto ss = SettingsManager::instance.getUiSettings();
	ctrlTransfers.saveHeaderOrder(Conf::TRANSFER_FRAME_ORDER, Conf::TRANSFER_FRAME_WIDTHS, Conf::TRANSFER_FRAME_VISIBLE);
	ss->setInt(Conf::TRANSFER_FRAME_SORT, ctrlTransfers.getSortForSettings());

	SettingsManager::instance.removeListener(this);
	UserManager::getInstance()->removeListener(this);
	FavoriteManager::getInstance()->removeListener(this);
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
			createMenus();
			int defaultItem = 0;
			const auto* ss = SettingsManager::instance.getUiSettings(); 
			switch (ss->getInt(Conf::TRANSFERLIST_DBLCLICK))
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

			const bool segmentedDownload = ii->isParent();

			clearPreviewMenu();
			OMenu transferMenu;
			transferMenu.CreatePopupMenu();
			clearUserMenu();

			transferMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
			if (ii->download)
				transferMenu.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(OPEN_DOWNLOAD_QUEUE), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD_QUEUE, 0));
			appendPreviewItems(transferMenu);
			transferMenu.AppendMenu(MF_POPUP, copyMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));

			transferMenu.AppendMenu(MF_SEPARATOR);
			transferMenu.AppendMenu(MF_STRING, IDC_FORCE, CTSTRING(FORCE_ATTEMPT));
			transferMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSE), g_iconBitmaps.getBitmap(IconBitmaps::PAUSE, 0));
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
			transferMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
#endif
			transferMenu.AppendMenu(MF_SEPARATOR);

			if (!segmentedDownload && (i = ctrlTransfers.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				const ItemInfo* ii = ctrlTransfers.getItemData(i);
				showUserMenu = true;
				reinitUserMenu(ii->hintedUser.user, ii->hintedUser.hint);
				appendAndActivateUserItems(transferMenu);
				if (getSelectedUser())
					appendUcMenu(transferMenu, UserCommand::CONTEXT_USER | UserCommand::CONTEXT_FLAG_TRANSFERS,
						ClientManager::getHubs(getSelectedUser()->getCID(), getSelectedHint()));
			}

#ifdef BL_FEATURE_DROP_SLOW_SOURCES
			segmentedMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
			transferMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
#endif
			if (ii->download)
			{
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
				transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_ENABLED);
#endif
				transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_ENABLED);
				if (!ii->target.empty())
				{
					const string target = Text::fromT(ii->target);
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
					bool slowDisconnect;
					{
						QueueManager::LockFileQueueShared fileQueue;
						const auto& queue = fileQueue.getQueueL();
						const auto qi = queue.find(target);
						if (qi != queue.cend())
							slowDisconnect = (qi->second->getExtraFlags() & QueueItem::XFLAG_AUTODROP) != 0;
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
#ifdef BL_FEATURE_DROP_SLOW_SOURCES
				transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_DISABLED);
#endif
				transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_DISABLED);
			}

			if (ii->transferIp.type)
			{
				selectedIP = Util::printIpAddress(ii->transferIp); // save current IP for performWebSearch
				appendWebSearchItems(getServiceSubMenu(), ii->transferIp.type == AF_INET6 ? SearchUrl::IP6 : SearchUrl::IP4, true, ResourceManager::WEB_SEARCH_IP);
			}

			int flag = hasTTH ? MFS_ENABLED : MFS_DISABLED;
			copyMenu.EnableMenuItem(IDC_COPY_TTH, flag);
			copyMenu.EnableMenuItem(IDC_COPY_LINK, flag);
			copyMenu.EnableMenuItem(IDC_COPY_WMLINK, flag);

			if (!segmentedDownload)
			{
				transferMenu.AppendMenu(MF_SEPARATOR);
				transferMenu.AppendMenu(MF_STRING, IDC_TRANSFERS_ONLY_ACTIVE_UPLOADS, CTSTRING(SHOW_ONLY_ACTIVE_UPLOADS));
				if (onlyActiveUploads)
					transferMenu.CheckMenuItem(IDC_TRANSFERS_ONLY_ACTIVE_UPLOADS, MF_BYCOMMAND | MF_CHECKED);
				OMenu* defItemMenu = &transferMenu;
				if (defaultItem)
				{
					if (defaultItem == IDC_GRANTSLOT)
						defItemMenu = &grantMenu;
					else if (defaultItem == IDC_ADD_TO_FAVORITES)
						defItemMenu = &favUserMenu;
				}
				transferMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, hasTTH ? MFS_ENABLED : MFS_DISABLED);
				activatePreviewItems(transferMenu);
				defItemMenu->SetMenuDefaultItem(defaultItem);
				transferMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}
			else
			{
				if (defaultItem == IDC_QUEUE) segmentedMenu.SetMenuDefaultItem(defaultItem);
				segmentedMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, hasTTH ? MFS_ENABLED : MFS_DISABLED);
				activatePreviewItems(segmentedMenu);
				segmentedMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}

			cleanUcMenu(transferMenu);
			MenuHelper::unlinkStaticMenus(transferMenu);
			return TRUE;
		}
	}
	bHandled = FALSE;
	return FALSE;
}

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
		const ItemInfo* ii = ctrlTransfers.getItemData(i);
		if (ii->hintedUser.user && ii->hintedUser.user->isOnline())
		{
			StringMap tmp = ucParams;
			ucParams["fileFN"] = Text::fromT(ii->target);

			// compatibility with 0.674 and earlier
			ucParams["file"] = ucParams["fileFN"];
			ClientManager::userCommand(ii->hintedUser, uc, tmp, true);
		}
	}
}

LRESULT TransferView::onForce(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		if (ii->isParent())
		{
			for (ItemInfo* child : ii->groupInfo->children)
				child->force();
		}
		else
			ii->force();
	}
	return 0;
}

void TransferView::ItemInfo::removeAll()
{
	if (hits <= 1)
		QueueManager::getInstance()->removeSource(hintedUser, QueueItem::Source::FLAG_REMOVED);
}

void TransferView::ItemInfo::force()
{
	if (isParent() || !download) return;
	transferFailed = false;
#if 0
	statusString = TSTRING(CONNECTING_FORCED);
	ctrlTransfers.updateItem(this);
#endif
	ConnectionManager::getInstance()->force(hintedUser);
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
			customDrawState.indent = ii->hasParent() ? 1 : 0;
			return CDRF_NOTIFYSUBITEMDRAW;
		}

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			const ItemInfo* ii = reinterpret_cast<const ItemInfo*>(cd->nmcd.lItemlParam);
			if (!ii) return CDRF_DODEFAULT;
			const int colIndex = ctrlTransfers.findColumn(cd->iSubItem);
			cd->clrTextBk = Colors::g_bgColor;
			if (colIndex == COLUMN_USER)
			{
				if (ii->isParent() || !ii->getUser())
					cd->clrText = Colors::g_textColor;
				else
				{
					using namespace UserTypeColors;
					unsigned short flags = IS_FAVORITE | IS_BAN | IS_RESERVED_SLOT;
					cd->clrText = getColor(flags, ii->getUser());
				}
			}
			else if (colIndex != COLUMN_HUB && colIndex != COLUMN_STATUS && colIndex != COLUMN_LOCATION && ii->status != ItemInfo::STATUS_RUNNING)
				cd->clrText = OperaColors::blendColors(Colors::g_bgColor, Colors::g_textColor, 0.4);
			else
				cd->clrText = Colors::g_textColor;

			if (cd->iSubItem == 0)
			{
				CustomDrawHelpers::drawFirstSubItem(customDrawState, cd, ii->getText(colIndex));
				return CDRF_SKIPDEFAULT;
			}

			if (colIndex == COLUMN_STATUS)
			{
				tstring status = ii->getText(COLUMN_STATUS);
				CustomDrawHelpers::startSubItemDraw(customDrawState, cd);
				if (ii->status == ItemInfo::STATUS_RUNNING)
				{
					if (!showProgressBars)
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
					int barIndex = ii->download ? (ii->hasParent() ? 1 : 0) : 2;
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

					if (showSpeedIcon)
					{
						if (ii->type == Transfer::TYPE_FULL_LIST || ii->type == Transfer::TYPE_PARTIAL_LIST)
						{
							iconIndex = 5;
						}
						else
						{
							auto tm = ThrottleManager::getInstance();
							int64_t speedmark;
							if (tm->getUploadLimitInBytes() == 0 && tm->getDownloadLimitInBytes() == 0)
							{
								speedmark = ii->download ? topDownloadSpeed : topUploadSpeed;
							}
							else
							{
								if (!ii->download)
									speedmark = tm->getUploadLimitInKBytes() / 5;
								else
									speedmark = tm->getDownloadLimitInKBytes() / 5;
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
		bool isUser = !i->isParent();
		const auto* ss = SettingsManager::instance.getUiSettings(); 
		switch (ss->getInt(Conf::TRANSFERLIST_DBLCLICK))
		{
			case 0:
				if (isUser) i->pm();
				break;
			case 1:
				if (isUser) i->getList();
				break;
			case 2:
				if (isUser) i->matchQueue();
				break;
			case 3:
				if (isUser) i->grantSlotPeriod(600);
				break;
			case 4:
				if (isUser) i->addFav();
				break;
			case 5:
				if (isUser) i->force();
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

TransferView::ItemInfo* TransferView::addToken(const UpdateInfo& ui)
{
	if (ui.token.empty() || !ConnectionManager::tokenManager.isToken(ui.token))
	{
#ifdef _DEBUG
		LogManager::message("TransferView: invalid token " + ui.token);
#endif
		return nullptr;
	}
	if (onlyActiveUploads && !ui.download && ui.status != ItemInfo::STATUS_RUNNING)
		return nullptr;
	ItemInfo* ii = new ItemInfo(ui);
	ii->update(ui);
	if (ii->download)
	{
		int pos = ctrlTransfers.insertGroupedItem(ii, false, false);
		ItemInfo* parent = ii->groupInfo->parent;
		if (parent)
		{
			pos = ctrlTransfers.findItem(parent);
			dcassert(pos != -1);
			updateDownload(parent, pos, ui.updateMask & UpdateInfo::MASK_FILE);
		}
		else
			updateDownload(ii, pos, ui.updateMask);
	}
	else
		ctrlTransfers.insertItem(ii, IMAGE_UPLOAD);
	return ii;
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

	bool update = false;
	CLockRedraw<> lockRedraw(ctrlTransfers);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		switch (i->first)
		{
			case ADD_TOKEN:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
				int index;
				ItemInfo* foundItem = findItemByToken(ui.token, index);
				if (foundItem)
				{
#ifdef _DEBUG
					LogManager::message("TransferView: trying to add duplicate token " + ui.token);
#endif
					break;
				}
				addToken(ui);
				break;
			}

			case REMOVE_TOKEN:
			{
				const auto &st = static_cast<StringTask&>(*i->second);
				int index;
				ItemInfo* ii = findItemByToken(st.str, index);
				if (!ii)
				{
#ifdef _DEBUG
					LogManager::message("TransferView: token " + st.str + " not found");
#endif
					break;
				}
				ctrlTransfers.removeGroupedItem(ii);
				break;
			}

			case UPDATE_TOKEN:
			{
				auto &ui = static_cast<UpdateInfo&>(*i->second);
				int index;
				ItemInfo* ii = findItemByToken(ui.token, index);
				if (ii)
				{
					if (ui.download)
					{
						ItemInfo* parent = ii->groupInfo ? ii->groupInfo->parent : nullptr;
						bool changeParent = (ui.updateMask & UpdateInfo::MASK_FILE) && ui.target != ii->target;
						if (changeParent)
						{
							ItemInfo* newParent = findItemByTarget(ui.target);
							if (newParent != parent)
								ctrlTransfers.removeGroupedItem(ii, false);
							else
								changeParent = false;
						}
						if (changeParent)
						{
							ii->target = ui.target;
							ii->qi = ui.qi;
							ui.updateMask ^= UpdateInfo::MASK_FILE;
							ctrlTransfers.insertGroupedItem(ii, false, false);
							ii->update(ui);
							parent = ii->groupInfo->parent;
							if (parent) updateDownload(parent, -1, UpdateInfo::MASK_FILE);
						}
						else
						{
							bool shouldRemoveParent = false;
							if (!parent && (ui.updateMask & UpdateInfo::MASK_FILE))
								ctrlTransfers.changeGroupCondNonVisual(ii, ui.target);
							ii->update(ui);
							if (index != -1)
							{
								if (parent)
								{
									updateDownload(parent, -1, ui.updateMask & UpdateInfo::MASK_FILE);
									shouldRemoveParent = parent->status == ItemInfo::STATUS_WAITING && parent->qi && parent->qi->isFinished();
									if (!shouldRemoveParent)
										updateItem(index, ui.updateMask);
								}
								else
									updateDownload(ii, index, ui.updateMask);
							}
							else if (parent)
							{
								updateDownload(parent, -1, ui.updateMask & UpdateInfo::MASK_FILE);
								shouldRemoveParent = parent->status == ItemInfo::STATUS_WAITING && parent->qi && parent->qi->isFinished();
							}
							if (shouldRemoveParent)
							{
								ctrlTransfers.removeGroupedItem(ii, false);
								ctrlTransfers.insertItem(ii, IMAGE_DOWNLOAD);
							}
						}
					}
					else
					{
						ii->update(ui);
						dcassert(index != -1);
						updateItem(index, ui.updateMask);
					}
				}
				else
				{
					ii = addToken(ui);
				}
				break;
			}

			case POPUP_NOTIF:
			{
				const PopupTask* popup = static_cast<const PopupTask*>(i->second);
				if (!NotifUtil::isPopupEnabled(popup->setting)) break;
				tstring text;
				if (!popup->file.empty())
				{
					text += TSTRING(FILE);
					text += _T(": ");
					text += Text::toT(popup->file);
				}
				if (!popup->user.empty())
				{
					if (!text.empty()) text += _T('\n');
					text += TSTRING(USER);
					text += _T(": ");
					text += Text::toT(popup->user);
				}
				if (!popup->miscText.empty())
				{
					if (!text.empty()) text += _T('\n');
					text += popup->miscText;
				}
				NotifUtil::showPopup(popup->setting, text, TSTRING_I(popup->title), popup->flags);
				break;
			}

			case REPAINT:
				update = true;
				break;

			default:
				dcassert(0);
				break;
		}
		delete i->second;
	}
	if (shouldSort && !MainFrame::isAppMinimized())
	{
		shouldSort = false;
		update = false;
		ctrlTransfers.resort();
	}
	if (update)
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
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

// TODO: use std::move for strings
void TransferView::ItemInfo::update(const UpdateInfo& ui)
{
	if (ui.type != Transfer::TYPE_LAST)
		type = ui.type;

	if (ui.updateMask & UpdateInfo::MASK_STATUS)
		status = ui.status;
	if (ui.updateMask & UpdateInfo::MASK_ERROR_TEXT)
		errorStatusString = ui.errorStatusString;
	if (ui.updateMask & UpdateInfo::MASK_TOKEN)
		token = ui.token;
	if (ui.updateMask & UpdateInfo::MASK_STATUS_STRING)
	{
		// No slots etc from transfermanager better than disconnected from connectionmanager
		if (!transferFailed)
			statusString = ui.statusString;
		transferFailed = ui.transferFailed;
	}
	if (ui.updateMask & UpdateInfo::MASK_SIZE)
		size = ui.size;
	if (ui.updateMask & UpdateInfo::MASK_POS)
		pos = ui.pos;
	if (ui.updateMask & UpdateInfo::MASK_SPEED)
		speed = ui.speed;
	if (ui.updateMask & UpdateInfo::MASK_FILE)
		target = ui.target;
	if (ui.updateMask & UpdateInfo::MASK_TIMELEFT)
		timeLeft = ui.timeLeft;
	if (ui.updateMask & UpdateInfo::MASK_IP)
	{
		dcassert(ui.ip.type);
		if (!transferIp.type)
		{
			transferIp = ui.ip;
			if (!(ipInfo.known & IPInfo::FLAG_P2P_GUARD))
				Util::getIpInfo(transferIp, ipInfo, IPInfo::FLAG_P2P_GUARD);
#ifdef FLYLINKDC_USE_COLUMN_RATIO
			ratioText.clear();
			User::loadIPStatFromDB(ui.hintedUser.user);
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
		}
	}
	if (ui.updateMask & UpdateInfo::MASK_CIPHER)
		cipher = ui.cipher;
	if (ui.updateMask & UpdateInfo::MASK_SEGMENTS)
		running = ui.running;
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
		ItemInfo* parent = groupInfo ? groupInfo->parent : nullptr;
		if (parent) parent->tth = tth;
	}
	if ((ui.updateMask & UpdateInfo::MASK_QUEUE_ITEM) && qi.get() != ui.qi.get())
		qi = ui.qi;
}

void TransferView::updateItem(int ii, uint32_t updateMask)
{
	if (updateMask &
		(UpdateInfo::MASK_STATUS | UpdateInfo::MASK_STATUS_STRING |
		UpdateInfo::MASK_ERROR_TEXT | UpdateInfo::MASK_TOKEN | UpdateInfo::MASK_POS))
	{
		ctrlTransfers.updateItem(ii, COLUMN_STATUS);
	}
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	if (updateMask & UpdateInfo::MASK_POS)
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
		ctrlTransfers.updateItem(ii, COLUMN_TIME_LEFT);
	}
	if (updateMask & UpdateInfo::MASK_IP)
	{
		ctrlTransfers.updateItem(ii, COLUMN_IP);
		//ctrlTransfers.updateItem(ii, COLUMN_LOCATION);
	}
	if (updateMask & UpdateInfo::MASK_SEGMENTS)
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

TransferView::UpdateInfo* TransferView::createUpdateInfoForNewItem(const HintedUser& hintedUser, bool isDownload, const string& token)
{
	QueueItemPtr qi;
	if (isDownload)
	{
		qi = QueueManager::getQueuedItem(hintedUser.user);
		if (!qi)
		{
#ifdef _DEBUG
			LogManager::message("TransferView: no queued item for user " + hintedUser.user->getLastNick());
#endif
			return nullptr;
		}
	}
	UpdateInfo* ui = new UpdateInfo(hintedUser, isDownload);
	if (!hintedUser.hint.empty()) ui->updateMask |= UpdateInfo::MASK_USER;
	ui->setToken(token);
	if (isDownload)
	{
		QueueItem::MaskType flags = qi->getFlags();
		Transfer::Type type;
		if (flags & QueueItem::FLAG_USER_LIST)
			type = Transfer::TYPE_FULL_LIST;
		else if (flags & QueueItem::FLAG_DCLST_LIST)
			type = Transfer::TYPE_FULL_LIST;
		else if (flags & QueueItem::FLAG_PARTIAL_LIST)
			type = Transfer::TYPE_PARTIAL_LIST;
		else
			type = Transfer::TYPE_FILE;
		ui->setType(type);
		ui->setQueueItem(qi);
		ui->setTarget(qi->getTarget());
		//ui->setSize(qi->getSize());
	}

	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setStatusString(TSTRING(CONNECTING));
	return ui;
}

void TransferView::on(ConnectionManagerListener::Added, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	auto task = createUpdateInfoForNewItem(hintedUser, isDownload, token);
	if (task) addTask(ADD_TOKEN, task);
}

void TransferView::on(ConnectionManagerListener::ConnectionStatusChanged, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	auto task = createUpdateInfoForNewItem(hintedUser, isDownload, token);
	if (task) addTask(UPDATE_TOKEN, task);
}

void TransferView::on(ConnectionManagerListener::UserUpdated, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	auto task = createUpdateInfoForNewItem(hintedUser, isDownload, token);
	if (task) addTask(UPDATE_TOKEN, task);
}

void TransferView::on(ConnectionManagerListener::Removed, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept
{
	addTask(REMOVE_TOKEN, new StringTask(token));
}

void TransferView::on(ConnectionManagerListener::FailedDownload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept
{
#ifdef _DEBUG
	LogManager::message("ConnectionManagerListener::FailedDownload user = " + hintedUser.user->getLastNick() + " Reason = " + reason);
#endif
	UpdateInfo* ui = new UpdateInfo(hintedUser, true);
	ui->setToken(token);
#ifdef BL_FEATURE_IPFILTER
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
		ui->setErrorText(Text::toT(status + " [" + reason + "]"));
	}
#endif
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setSpeed(0);
	addTask(UPDATE_TOKEN, ui);
}

void TransferView::on(ConnectionManagerListener::FailedUpload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept
{
	if (onlyActiveUploads)
	{
		addTask(REMOVE_TOKEN, new StringTask(token));
		return;
	}
	UpdateInfo* ui = new UpdateInfo(hintedUser, false);
	ui->setToken(token);
	ui->setStatusString(Text::toT(reason));
	ui->setSpeed(0);
	ui->setStatus(ItemInfo::STATUS_WAITING);
	addTask(UPDATE_TOKEN, ui);
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
		nicks = Text::toT(ClientManager::getNick(hintedUser));
		hubs = Text::toT(WinUtil::getHubDisplayName(hintedUser.hint));
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
	switch (col)
	{
		case COLUMN_USER:
			return hits == -1 ? nicks : TPLURAL_F(PLURAL_USERS, hits);
		case COLUMN_HUB:
			return hits == -1 ? hubs : TPLURAL_F(PLURAL_SEGMENTS, running);
		case COLUMN_STATUS:
		{
			if (status != STATUS_RUNNING && !errorStatusString.empty())
				return errorStatusString;
			return statusString;
		}
		case COLUMN_TIME_LEFT:
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
			return cipher;
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
		case COLUMN_DEBUG_INFO:
		{
			if (token.empty()) return Util::emptyStringT;
			string info = "T: " + token;
			auto cm = ConnectionManager::getInstance();
			ConnectionQueueItemPtr cqi = download ? cm->getDownloadCQI(token) : cm->getUploadCQI(token);
			if (cqi)
			{
				info += ", S: " + Util::toString((int) cqi->getState());
				int errors = cqi->getErrors();
				if (errors) info += ", E: " + Util::toString(errors);
			}
			return Text::toT(info);
		}
		default:
			return Util::emptyStringT;
	}
}

int TransferView::ItemInfo::compareTargets(const ItemInfo* a, const ItemInfo* b)
{
	int res = compare(a->target, b->target);
	if (res) return res;
	if (a->download != b->download) return a->download ? -1 : 1;
	res = compare(a->nicks, b->nicks);
	if (res) return res;
	return compare(a->hubs, b->hubs);
}

int TransferView::ItemInfo::compareUsers(const ItemInfo* a, const ItemInfo* b)
{
	if (a->hits != -1 && b->hits != -1) return compareTargets(a, b);
	int res = compare(a->hits, b->hits);
	if (a->hits != -1 || b->hits != -1) return -res;
	res = compare(a->nicks, b->nicks);
	if (res) return res;
	if (a->download != b->download) return a->download ? -1 : 1;
	return compare(a->hubs, b->hubs);
}

int TransferView::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, int col, int /*flags*/)
{
	int res;
	switch (col)
	{
		case COLUMN_USER:
			break;
		case COLUMN_HUB:
			if (a->hits != -1 && b->hits != -1)
			{
				res = compare(a->running, b->running);
				if (res) return res;
				return compareTargets(a, b);
			}
			res = compare(a->hits, b->hits);
			if (a->hits == -1 || b->hits == -1) return -res;
			if (res) return res;
			res = compare(a->hubs, b->hubs);
			if (res) return res;
			if (a->download != b->download) return a->download ? -1 : 1;
			return compare(a->nicks, b->nicks);
		case COLUMN_STATUS:
			if (a->download != b->download) return a->download ? -1 : 1;
			res = compare(a->status, b->status);
			if (res) return -res;
			if (a->status == ItemInfo::STATUS_RUNNING)
				res = compare(a->getProgressPosition(), b->getProgressPosition());
			else
				res = Util::defaultSort(a->getText(COLUMN_STATUS), b->getText(COLUMN_STATUS));
			if (res) return res;
			if (a->hits != -1 && b->hits != -1) return compareTargets(a, b);
			res = compare(a->hits, b->hits);
			if (res) return res;
			res = compare(a->nicks, b->nicks);
			if (res) return res;
			return compare(a->hubs, b->hubs);
		case COLUMN_TIME_LEFT:
			res = compare(a->timeLeft, b->timeLeft);
			if (res) return res;
			break;
		case COLUMN_SPEED:
			res = compare(a->speed, b->speed);
			if (res) return res;
			break;
		case COLUMN_SIZE:
			res = compare(a->size, b->size);
			if (res) return res;
			break;
#ifdef FLYLINKDC_USE_COLUMN_RATIO
		//case COLUMN_RATIO:
		//  return compare(a->getRatio(), b->getRatio());
#endif
		case COLUMN_SHARE:
		{
			int64_t ashare = a->getUser() ? a->getUser()->getBytesShared() : 0;
			int64_t bshare = b->getUser() ? b->getUser()->getBytesShared() : 0;
			res = compare(ashare, bshare);
			if (res) return res;
			break;
		}
		case COLUMN_SLOTS:
		{
			int aslots = a->getUser() ? a->getUser()->getSlots() : -1;
			int bslots = b->getUser() ? b->getUser()->getSlots() : -1;
			res = compare(aslots, bslots);
			if (res) return res;
			break;
		}
		case COLUMN_IP:
			res = compare(a->transferIp, b->transferIp);
			if (res) return res;
			break;
		case COLUMN_DEBUG_INFO:
			res = compare(a->token, b->token);
			if (res) return res;
			break;
		default:
			res = Util::defaultSort(a->getText(col), b->getText(col));
			if (res) return res;
			break;
	}
	return compareUsers(a, b);
}

void TransferView::starting(UpdateInfo* ui, const Transfer* t)
{
	ui->setTarget(t->getPath());
	ui->setType(t->getType());
	ui->setCipher(Text::toT(t->getUserConnection()->getCipherName()));
	const string& token = t->getConnectionQueueToken();
	ui->setToken(token);
	ui->setIP(t->getIP());
}

void TransferView::on(DownloadManagerListener::Requesting, const DownloadPtr& download) noexcept
{
	UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true);
	starting(ui, download.get());
	ui->setPos(download->getPos());
	ui->setSize(download->getSize());
	ui->setStatus(ItemInfo::STATUS_REQUESTING);
	ui->setTarget(download->getPath());
	ui->setQueueItem(download->getQueueItem());
	if (download->getType() == Download::TYPE_FILE)
		ui->setTTH(download->getTTH());
	ui->setStatusString(TSTRING(REQUESTING) + _T(' ') + getFile(download->getType(), Text::toT(Util::getFileName(download->getPath()))) + _T("..."));
	const string& token = download->getConnectionQueueToken();
	ui->setToken(token);
	addTask(UPDATE_TOKEN, ui);
}

void TransferView::on(DownloadManagerListener::Complete, const DownloadPtr& download) noexcept
{
	onTransferComplete(download.get(), true, Util::getFileName(download->getPath()), false);
}

void TransferView::on(DownloadManagerListener::Failed, const DownloadPtr& download, const string& reason) noexcept
{
	UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true, true);
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setSize(download->getSize());
	ui->setTarget(download->getPath());
	ui->setType(download->getType());
	ui->setToken(download->getConnectionQueueToken());

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
	ui->setSpeed(0);

	if (download->getPos() != download->getSize())
	{
		PopupTask* popup = new PopupTask;
		popup->setting = Conf::POPUP_ON_DOWNLOAD_FAILED;
		popup->title = ResourceManager::DOWNLOAD_FAILED_POPUP;
		popup->flags = NIIF_WARNING;
		popup->user = ClientManager::getNick(ui->hintedUser);
		popup->file = Util::getFileName(download->getPath());
		popup->miscText = std::move(tmpReason);
		addTask(POPUP_NOTIF, popup);
	}
	addTask(UPDATE_TOKEN, ui);
}

static const tstring& getReasonText(int error)
{
	switch (error)
	{
		case QueueItem::ERROR_NO_NEEDED_PART:
			return TSTRING(NO_NEEDED_PART);
		case QueueItem::ERROR_FILE_SLOTS_TAKEN:
			return TSTRING(ALL_FILE_SLOTS_TAKEN);
		case QueueItem::ERROR_DOWNLOAD_SLOTS_TAKEN:
			return TSTRING(ALL_DOWNLOAD_SLOTS_TAKEN);
		case QueueItem::ERROR_NO_FREE_BLOCK:
			return TSTRING(NO_FREE_BLOCK);
	}
	return Util::emptyStringT;
}

void TransferView::on(DownloadManagerListener::Status, const UserConnection* conn, const Download::ErrorInfo& status) noexcept
{
	const tstring& reasonText = getReasonText(status.error);
	if (reasonText.empty()) return;
	UpdateInfo* ui = new UpdateInfo(conn->getHintedUser(), true);
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setStatusString(reasonText);
	ui->setSize(status.size);
	ui->setTarget(status.target);
	ui->setType(status.type);
	const string& token = conn->getConnectionQueueToken();
	ui->setToken(token);
	addTask(UPDATE_TOKEN, ui);
}

void TransferView::on(UploadManagerListener::Starting, const UploadPtr& upload) noexcept
{
	UpdateInfo* ui = new UpdateInfo(upload->getHintedUser(), false);
	starting(ui, upload.get());
	ui->setPos(upload->getAdjustedPos());
	ui->setSize(upload->getType() == Transfer::TYPE_TREE ? upload->getSize() : upload->getFileSize());
	ui->setTarget(upload->getPath());
	ui->setStatus(ItemInfo::STATUS_RUNNING);
	ui->setErrorText(Util::emptyStringT);
	ui->setRunning(1);
	if (upload->getType() == Transfer::TYPE_FILE)
		ui->setTTH(upload->getTTH());

	if (!upload->isSet(Upload::FLAG_RESUMED))
		ui->setStatusString(TSTRING(UPLOAD_STARTING));

	const string& token = upload->getConnectionQueueToken();
	ui->setToken(token);

	addTask(UPDATE_TOKEN, ui);
}

void TransferView::on(DownloadManagerListener::Tick, const DownloadArray& dl) noexcept
{
	for (auto j = dl.cbegin(); j != dl.cend(); ++j)
	{
		UpdateInfo* ui = new UpdateInfo(j->hintedUser, true);
		ui->setStatus(ItemInfo::STATUS_RUNNING);
		ui->setPos(j->pos);
		ui->setSpeed(j->speed);
		ui->setSize(j->size);
		ui->setTimeLeft(j->secondsLeft);
		ui->setType(Transfer::Type(j->type)); // TODO
		ui->setTarget(j->path);
		ui->setToken(j->token);
		ui->setQueueItem(j->qi);
		ui->statusString = ItemInfo::formatStatusString(j->transferFlags, j->startTime, j->pos, j->size);
		ui->updateMask |= UpdateInfo::MASK_STATUS_STRING;
		addTask(UPDATE_TOKEN, ui);
	}
}

void TransferView::on(UploadManagerListener::Tick, const UploadArray& ul) noexcept
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
		ui->setPos(j->pos);
		ui->setSize(j->size);
		ui->setTimeLeft(j->secondsLeft);
		ui->setSpeed(j->speed);
		ui->setType(Transfer::Type(j->type)); // TODO
		ui->setTarget(j->path);
		ui->setToken(j->token);
		ui->statusString = ItemInfo::formatStatusString(j->transferFlags, j->startTime, j->pos, j->size);
		ui->updateMask |= UpdateInfo::MASK_STATUS_STRING;
		addTask(UPDATE_TOKEN, ui);
	}
}

void TransferView::onTransferComplete(const Transfer* t, bool download, const string& fileName, bool failed)
{
	if (t->getType() == Transfer::TYPE_TREE)
		return;

	UpdateInfo* ui = new UpdateInfo(t->getHintedUser(), download);

	ui->setTarget(t->getPath());
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setPos(0);
	ui->setSize(t->getFileSize());
	ui->setTimeLeft(0);
	ui->setRunning(0);
	if (!download && failed)
		ui->setStatusString(TSTRING(UNABLE_TO_SEND_FILE));
	else
		ui->setStatusString(download ? TSTRING(DOWNLOAD_FINISHED_IDLE) : TSTRING(UPLOAD_FINISHED_IDLE));
	const string& token = t->getConnectionQueueToken();
	ui->setToken(token);
	if (!download && !failed)
	{
		PopupTask* popup = new PopupTask;
		popup->setting = Conf::POPUP_ON_UPLOAD_FINISHED;
		popup->title = ResourceManager::UPLOAD_FINISHED_IDLE;
		popup->flags = NIIF_INFO;
		popup->user = ClientManager::getNick(t->getHintedUser());
		popup->file = fileName;
		addTask(POPUP_NOTIF, popup);
	}

	addTask(UPDATE_TOKEN, ui);
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

LRESULT TransferView::onPerformWebSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	performWebSearch(wID, selectedIP);
	return 0;
}

void TransferView::collapseAll()
{
	const auto& parents = ctrlTransfers.getParents();
	for (auto i = parents.cbegin(); i != parents.cend(); ++i)
	{
		ItemInfo* parent = i->second->parent;
		if (parent && (parent->stateFlags & ItemInfoList::STATE_FLAG_EXPANDED))
			ctrlTransfers.collapse(parent, ctrlTransfers.findItem(parent));
	}
}

void TransferView::expandAll()
{
	const auto& parents = ctrlTransfers.getParents();
	for (auto i = parents.cbegin(); i != parents.cend(); ++i)
	{
		ItemInfo* parent = i->second->parent;
		if (parent && !(parent->stateFlags & ItemInfoList::STATE_FLAG_EXPANDED))
			ctrlTransfers.expand(parent, ctrlTransfers.findItem(parent));
	}
}

LRESULT TransferView::onDisconnectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		if (ii->isParent())
		{
			for (ItemInfo* child : ii->groupInfo->children)
				child->disconnect();
		}
		else
			ii->disconnect();
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
				qi->second->toggleExtraFlag(QueueItem::XFLAG_AUTODROP);
		}
	}
	return 0;
}

LRESULT TransferView::onOnlyActiveUploads(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	onlyActiveUploads = !onlyActiveUploads;
	auto ss = SettingsManager::instance.getUiSettings();
	ss->setBool(Conf::TRANSFERS_ONLY_ACTIVE_UPLOADS, onlyActiveUploads);
	if (onlyActiveUploads)
	{
		int count = ctrlTransfers.GetItemCount();
		for (int i = count - 1; i >= 0; i--)
		{
			const ItemInfo* ii = ctrlTransfers.getItemData(i);
			if (!ii->download && ii->status != ItemInfo::STATUS_RUNNING)
			{
				ctrlTransfers.DeleteItem(i);
				delete ii;
			}
		}
	}
	return 0;
}

void TransferView::on(SettingsManagerListener::ApplySettings)
{
	initProgressBars(true);
	if (ctrlTransfers.isRedraw())
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void TransferView::updateDownload(ItemInfo* ii, int index, int updateMask)
{
	dcassert(ii->download);
	if (!ii->qi)
	{
		dcassert(0);
		return;
	}
	if (index < 0)
	{
		index = ctrlTransfers.findItem(ii);
		if (index < 0)
		{
			dcassert(0);
			return;
		}
	}
	const QueueItemPtr& qi = ii->qi;
	int transferFlags;
	const int16_t segs = qi->getTransferFlags(transferFlags);
	if (ii->running != segs)
	{
		ii->running = segs;
		updateMask |= UpdateInfo::MASK_SEGMENTS;
	}
	int64_t size = qi->getSize();
	if (size != ii->size)
	{
		ii->size = size;
		updateMask |= UpdateInfo::MASK_SIZE;
	}
	int64_t pos = qi->getDownloadedBytes();
	if (pos != ii->pos)
	{
		ii->pos = pos;
		updateMask |= UpdateInfo::MASK_POS;
	}
	if (segs > 0)
	{
		if (ii->isParent())
			transferFlags &= ~(TRANSFER_FLAG_PARTIAL | TRANSFER_FLAG_SECURE | TRANSFER_FLAG_TRUSTED);
		ii->status = ItemInfo::STATUS_RUNNING;
		qi->updateDownloadedBytesAndSpeed();
		int64_t totalSpeed = qi->getAverageSpeed();
		if (totalSpeed > 0)
		{
			if (totalSpeed != ii->speed)
			{
				ii->speed = totalSpeed;
				updateMask |= UpdateInfo::MASK_SPEED;
			}
			int64_t timeLeft = (size - pos) / totalSpeed;
			if (timeLeft != ii->timeLeft)
			{
				ii->timeLeft = timeLeft;
				updateMask |= UpdateInfo::MASK_TIMELEFT;
			}
		}
		if (qi->getTimeFileBegin() == 0)
		{
			// file is starting
			qi->setTimeFileBegin(GET_TICK());
			ii->statusString = TSTRING(DOWNLOAD_STARTING);
			updateMask |= UpdateInfo::MASK_STATUS_STRING;
			PLAY_SOUND(SOUND_BEGINFILE);
			SHOW_POPUP(POPUP_ON_DOWNLOAD_STARTED, TSTRING(FILE) + _T(": ") + Text::toT(Util::getFileName(qi->getTarget())), TSTRING(DOWNLOAD_STARTING));
		}
		else
		{
			ii->statusString = ItemInfo::formatStatusString(transferFlags, qi->getTimeFileBegin(), pos, size);
			ii->errorStatusString.clear();
		}
	}
	else
	{
		//qi->setTimeFileBegin(0); // FIXME
		if (ii->status != ItemInfo::STATUS_WAITING)
		{
			qi->updateDownloadedBytesAndSpeed();
			ii->status = ItemInfo::STATUS_WAITING;
		}
	}
	if (updateMask)
		updateItem(index, updateMask);
}

#if 0
void TransferView::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const DownloadPtr& download) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (qi->isUserList()) return;

		const string& token = download->getConnectionQueueToken();
		dcassert(!token.empty());
		// update file item
		UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true);
		ui->setToken(token);
		ui->setTarget(qi->getTarget());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setStatusString(TSTRING(DOWNLOAD_FINISHED_IDLE));

		PopupTask* popup = new PopupTask;
		popup->setting = Conf::POPUP_ON_DOWNLOAD_FINISHED;
		popup->title = ResourceManager::DOWNLOAD_FINISHED_POPUP;
		popup->flags = NIIF_INFO;
		popup->file = Util::getFileName(qi->getTarget());

		addTask(TRANSFER_UPDATE_PARENT, ui);
		addTask(POPUP_NOTIF, popup);
	}
}
#endif

void TransferView::on(DownloadManagerListener::RemoveToken, const string& token) noexcept
{
	addTask(REMOVE_TOKEN, new StringTask(token));
}

void TransferView::on(ConnectionManagerListener::RemoveToken, const string& token) noexcept
{
	addTask(REMOVE_TOKEN, new StringTask(token));
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

void TransferView::pauseSelectedTransfer()
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
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::CONFIRM_DELETE))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return 0;
		if (checkState == BST_CHECKED)
			ss->setBool(Conf::CONFIRM_DELETE, false);
	}
	ctrlTransfers.forEachSelected(&ItemInfo::removeAll);
	return 0;
}

void TransferView::onTimerInternal()
{
	shouldSort = true;
	processTasks();
	if (ctrlTransfers.isColumnVisible(COLUMN_DEBUG_INFO))
	{
		int count = ctrlTransfers.GetItemCount();
		for (int i = 0; i < count; i++)
		{
			const ItemInfo* ii = ctrlTransfers.getItemData(i);
			if (!ii->groupInfo || ii->groupInfo->parent != ii)
				ctrlTransfers.updateItem(i, COLUMN_DEBUG_INFO);
		}
	}
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
	const auto* ss = SettingsManager::instance.getUiSettings();
	showProgressBars = ss->getBool(Conf::SHOW_PROGRESS_BARS);
	showSpeedIcon = ss->getBool(Conf::STEALTHY_STYLE_ICO);
	if (ss->getBool(Conf::STEALTHY_STYLE_ICO_SPEEDIGNORE))
	{
		topDownloadSpeed = ss->getInt(Conf::TOP_DL_SPEED) / 5;
		topUploadSpeed = ss->getInt(Conf::TOP_UL_SPEED) / 5;
	}
	else
	{
		auto cs = SettingsManager::instance.getCoreSettings();
		cs->lockRead();
		topDownloadSpeed = topUploadSpeed = Util::toInt64(cs->getString(Conf::UPLOAD_SPEED)) * 20;
		cs->unlockRead();
	}

	ProgressBar::Settings settings;
	bool setColors = ss->getBool(Conf::PROGRESS_OVERRIDE_COLORS);
	settings.clrBackground = setColors ? ss->getInt(Conf::DOWNLOAD_BAR_COLOR) : GetSysColor(COLOR_HIGHLIGHT);
	settings.clrText = ss->getInt(Conf::PROGRESS_TEXT_COLOR_DOWN);
	settings.clrEmptyBackground = ss->getInt(Conf::PROGRESS_BACK_COLOR);
	settings.odcStyle = ss->getBool(Conf::PROGRESSBAR_ODC_STYLE);
	settings.odcBumped = ss->getBool(Conf::PROGRESSBAR_ODC_BUMPED);
	settings.depth = ss->getInt(Conf::PROGRESS_3DDEPTH);
	settings.setTextColor = ss->getBool(Conf::PROGRESS_OVERRIDE_COLORS2);
	if (!check || progressBar[0].get() != settings) progressBar[0].set(settings);
	progressBar[0].setWindowBackground(Colors::g_bgColor);

	if (setColors) settings.clrBackground = ss->getInt(Conf::PROGRESS_SEGMENT_COLOR);
	if (!check || progressBar[1].get() != settings) progressBar[1].set(settings);
	progressBar[1].setWindowBackground(Colors::g_bgColor);

	if (setColors) settings.clrBackground = ss->getInt(Conf::UPLOAD_BAR_COLOR);
	settings.clrText = ss->getInt(Conf::PROGRESS_TEXT_COLOR_UP);
	if (!check || progressBar[2].get() != settings) progressBar[2].set(settings);
	progressBar[2].setWindowBackground(Colors::g_bgColor);
}

TransferView::ItemInfo* TransferView::findItemByToken(const string& token, int& index)
{
	dcassert(!token.empty());
	int count = ctrlTransfers.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		if (ii->token == token)
		{
			index = i;
			return ii;
		}
	}
	index = -1;
	for (int i = 0; i < count; i++)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		if (ii->groupInfo && !(ii->stateFlags & ItemInfoList::STATE_FLAG_EXPANDED))
		{
			const auto& children = ii->groupInfo->children;
			for (ItemInfo* ii : children)
				if (ii->token == token) return ii;
		}
	}
	return nullptr;
}

TransferView::ItemInfo* TransferView::findItemByTarget(const tstring& target)
{
	dcassert(!target.empty());
	int count = ctrlTransfers.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		if (!ii->hasParent() && ii->target == target)
			return ii;
	}
	return nullptr;
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
	stateFlags = 0;
	transferFailed = false;
	status = STATUS_WAITING;
	pos = 0;
	size = 0;
	speed = 0;
	timeLeft = 0;
	hits = -1;
	running = 0;
	type = Transfer::TYPE_FILE;
	transferIp.type = 0;
	updateNicks();
}

TransferView::ItemInfo* TransferView::ItemInfo::createParent()
{
	dcassert(download);
	ItemInfo* ii = new ItemInfo;
	ii->hits = 0;
	ii->statusString = TSTRING(CONNECTING);
	ii->target = target;
	ii->tth = tth;
	ii->qi = qi;
	return ii;
}

tstring TransferView::ItemInfo::formatStatusString(int transferFlags, uint64_t startTime, int64_t pos, int64_t size)
{
	int64_t elapsed = startTime ? (GET_TICK() - startTime) / 1000 : 0;
	tstring statusString;
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
	return statusString;
}
