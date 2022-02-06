#include "stdafx.h"
#include "UserListWindow.h"
#include "HubFrameTasks.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "../client/UploadManager.h"
#include "../client/QueueManager.h"
#include "../client/dht/DHT.h"

static const int BUTTON_SIZE = 26;

static const int columnSizes[] =
{
	100,    // COLUMN_NICK
	75,     // COLUMN_SHARED
	150,    // COLUMN_EXACT_SHARED
	150,    // COLUMN_DESCRIPTION
	150,    // COLUMN_APPLICATION
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	75,     // COLUMN_CONNECTION
#endif
	50,     // COLUMN_EMAIL
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	50,     // COLUMN_VERSION
	40,     // COLUMN_MODE
#endif
	40,     // COLUMN_HUBS
	40,     // COLUMN_SLOTS
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	40,     // COLUMN_UPLOAD_SPEED
#endif
	100,    // COLUMN_IP
	100,    // COLUMN_GEO_LOCATION
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	50,     // COLUMN_UPLOAD
	50,     // COLUMN_DOWNLOAD
	10,     // COLUMN_MESSAGES
#endif
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
#ifdef FLYLINKDC_USE_DNS
	100,    // COLUMN_DNS
#endif
#endif
	300,    // COLUMN_CID
	200,    // COLUMN_TAG
	40,     // COLUMN_P2P_GUARD
#ifdef FLYLINKDC_USE_EXT_JSON
	20,     // FLY_HUB_GENDER
	50,     // COLUMN_FLY_HUB_COUNT_FILES
	100,    // COLUMN_FLY_HUB_LAST_SHARE_DATE
	100,    // COLUMN_FLY_HUB_RAM
	100,    // COLUMN_FLY_HUB_SQLITE_DB_SIZE
	100,    // COLUMN_FLY_HUB_QUEUE
	100,    // COLUMN_FLY_HUB_TIMES
	100     // COLUMN_FLY_HUB_SUPPORT_INFO
#endif
};

const int UserListWindow::columnId[] =
{
	COLUMN_NICK,
	COLUMN_SHARED,
	COLUMN_EXACT_SHARED,
	COLUMN_DESCRIPTION,
	COLUMN_APPLICATION,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	COLUMN_CONNECTION,
#endif
	COLUMN_EMAIL,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	COLUMN_VERSION,
	COLUMN_MODE,
#endif
	COLUMN_HUBS,
	COLUMN_SLOTS,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	COLUMN_UPLOAD_SPEED,
#endif
	COLUMN_IP,
	COLUMN_GEO_LOCATION,
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	COLUMN_UPLOAD,
	COLUMN_DOWNLOAD,
	COLUMN_MESSAGES,
#endif
#ifdef FLYLINKDC_USE_DNS
	COLUMN_DNS,
#endif
	COLUMN_CID,
	COLUMN_TAG,
	COLUMN_P2P_GUARD,
#ifdef FLYLINKDC_USE_EXT_JSON
	COLUMN_FLY_HUB_GENDER,
	COLUMN_FLY_HUB_COUNT_FILES,
	COLUMN_FLY_HUB_LAST_SHARE_DATE,
	COLUMN_FLY_HUB_RAM,
	COLUMN_FLY_HUB_SQLITE_DB_SIZE,
	COLUMN_FLY_HUB_QUEUE,
	COLUMN_FLY_HUB_TIMES,
	COLUMN_FLY_HUB_SUPPORT_INFO
#endif
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::NICK,            // COLUMN_NICK
	ResourceManager::SHARED,          // COLUMN_SHARED
	ResourceManager::EXACT_SHARED,    // COLUMN_EXACT_SHARED
	ResourceManager::DESCRIPTION,     // COLUMN_DESCRIPTION
	ResourceManager::APPLICATION,     // COLUMN_APPLICATION
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	ResourceManager::CONNECTION,      // COLUMN_CONNECTION
#endif
	ResourceManager::EMAIL,           // COLUMN_EMAIL
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	ResourceManager::VERSION,         // COLUMN_VERSION
	ResourceManager::MODE,            // COLUMN_MODE
#endif
	ResourceManager::HUBS,            // COLUMN_HUBS
	ResourceManager::SLOTS,           // COLUMN_SLOTS
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	ResourceManager::AVERAGE_UPLOAD,  // COLUMN_UPLOAD_SPEED
#endif
	ResourceManager::IP,              // COLUMN_IP
	ResourceManager::LOCATION_BARE,   // COLUMN_GEO_LOCATION
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	ResourceManager::UPLOADED,        // COLUMN_UPLOAD
	ResourceManager::DOWNLOADED,      // COLUMN_DOWNLOAD
	ResourceManager::MESSAGES_COUNT,  // COLUMN_MESSAGES
#endif
#ifdef FLYLINKDC_USE_DNS
	ResourceManager::DNS_BARE,        // COLUMN_DNS // !SMT!-IP
#endif
	ResourceManager::CID,             // COLUMN_CID
	ResourceManager::TAG,             // COLUMN_TAG
	ResourceManager::P2P_GUARD,       // COLUMN_P2P_GUARD
#ifdef FLYLINKDC_USE_EXT_JSON
	ResourceManager::FLY_HUB_GENDER,  // COLUMN_FLY_HUB_GENDER
	ResourceManager::FLY_HUB_COUNT_FILES,     // COLUMN_FLY_HUB_COUNT_FILES
	ResourceManager::FLY_HUB_LAST_SHARE_DATE, // COLUMN_FLY_HUB_LAST_SHARE_DATE
	ResourceManager::FLY_HUB_RAM,             // COLUMN_FLY_HUB_RAM
	ResourceManager::FLY_HUB_SQLITE_DB_SIZE,  // COLUMN_FLY_HUB_SQLITE_DB_SIZE
	ResourceManager::FLY_HUB_QUEUE,           // COLUMN_FLY_HUB_QUEUE
	ResourceManager::FLY_HUB_TIMES,           // COLUMN_FLY_HUB_TIMES
	ResourceManager::FLY_HUB_SUPPORT_INFO     // COLUMN_FLY_HUB_SUPPORT_INFO
#endif
};

enum Mask
{
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	IS_AUTOBAN          = 0x0003,
	IS_AUTOBAN_ON       = 0x0001,
#endif
	IS_FAVORITE         = 0x0003 << 2,
	IS_FAVORITE_ON      = 0x0001 << 2,
	IS_BAN              = 0x0003 << 4,
	IS_BAN_ON           = 0x0001 << 4,
	IS_RESERVED_SLOT    = 0x0003 << 6,
	IS_RESERVED_SLOT_ON = 0x0001 << 6,
	IS_IGNORED_USER     = 0x0003 << 8,
	IS_IGNORED_USER_ON  = 0x0001 << 8
};

UserListWindow::UserListWindow(HubFrameCallbacks* hubFrame) :
	ctrlFilterContainer(WC_EDIT, this, FILTER_MESSAGE_MAP),
	ctrlFilterSelContainer(WC_COMBOBOX, this, FILTER_MESSAGE_MAP),
	hubFrame(hubFrame)
{
	showUsers = false; // can't be set to true until ctrlUsers is created
	shouldUpdateStats = shouldSort = false;
	isOp = false;

	filterSelPos = COLUMN_NICK;
	csUserMap = std::unique_ptr<RWLock>(RWLock::create());

	ctrlUsers.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlUsers.setColumnOwnerDraw(COLUMN_GEO_LOCATION);
	ctrlUsers.setColumnOwnerDraw(COLUMN_IP);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	ctrlUsers.setColumnOwnerDraw(COLUMN_UPLOAD);
	ctrlUsers.setColumnOwnerDraw(COLUMN_DOWNLOAD);
	ctrlUsers.setColumnOwnerDraw(COLUMN_MESSAGES);
#endif
	ctrlUsers.setColumnOwnerDraw(COLUMN_P2P_GUARD);
	ctrlUsers.setColumnFormat(COLUMN_SHARED, LVCFMT_RIGHT);
	ctrlUsers.setColumnFormat(COLUMN_EXACT_SHARED, LVCFMT_RIGHT);
	ctrlUsers.setColumnFormat(COLUMN_SLOTS, LVCFMT_RIGHT);
}

UserListWindow::~UserListWindow()
{
	dcassert(userMap.empty());
}

void UserListWindow::initialize(const FavoriteManager::WindowInfo& wi)
{
	ctrlUsers.insertColumns(wi.headerOrder, wi.headerWidths, wi.headerVisible);
	if (wi.headerSort >= 0)
	{
		ctrlUsers.setSortColumn(wi.headerSort);
		ctrlUsers.setAscending(wi.headerSortAsc);
	}
	else
	{
		ctrlUsers.setSortFromSettings(SETTING(HUB_FRAME_SORT));
	}
}

#if 0
void UserListWindow::destroyFilter()
{
	if (ctrlFilter)
		ctrlFilter.DestroyWindow();
	safe_delete(ctrlFilterContainer);
		
	if (ctrlFilterSel)
		ctrlFilterSel.DestroyWindow();
	safe_delete(ctrlFilterSelContainer);
}
#endif

LRESULT UserListWindow::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(UserListWindow::columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(UserListWindow::columnId));

	ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP |
		LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_STATICEDGE, IDC_USERS);
	ctrlUsers.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	if (WinUtil::setExplorerTheme(ctrlUsers))
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED | CustomDrawHelpers::FLAG_USE_HOT_ITEM;
	setListViewColors(ctrlUsers);
	ctrlUsers.SetImageList(g_userImage.getIconList(), LVSIL_SMALL);

	dcassert(!ctrlFilterContainer);
	ctrlFilter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
	ctrlFilter.SetCueBannerText(CTSTRING(FILTER_HINT));
	ctrlFilterContainer.SubclassWindow(ctrlFilter.m_hWnd);
	ctrlFilter.SetFont(Fonts::g_systemFont);
	if (!filter.empty())
		ctrlFilter.SetWindowText(filter.c_str());

	ctrlClearFilter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_ICON | BS_CENTER, 0, IDC_CLEAR);
#ifdef OSVER_WIN_XP
	if (!CompatibilityManager::isOsVistaPlus())
		clearFilterSubclass.SubclassWindow(ctrlClearFilter);
#endif
	ctrlClearFilter.SetIcon(g_iconBitmaps.getIcon(IconBitmaps::CLEAR, 0));

	ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlFilterSelContainer.SubclassWindow(ctrlFilterSel.m_hWnd);
	ctrlFilterSel.SetFont(Fonts::g_systemFont);

	for (size_t j = 0; j < COLUMN_LAST; ++j)
		ctrlFilterSel.AddString(CTSTRING_I(columnNames[j]));

	ctrlFilterSel.AddString(CTSTRING(ANY));
	ctrlFilterSel.SetCurSel(filterSelPos);

	return 0;
}

LRESULT UserListWindow::onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	updateLayout();
	return 0;
}

LRESULT UserListWindow::onDoubleClickUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	if (item && item->iItem != -1)
	{
		if (UserInfo* ui = ctrlUsers.getItemData(item->iItem))
			switch (SETTING(USERLIST_DBLCLICK))
			{
				case 0:
					ui->getList();
					break;
				case 1:
				{
					if (hubFrame)
					{
						tstring nick = Text::toT(ui->getNick());
						hubFrame->setCurrentNick(nick);
						hubFrame->appendNickToChat(nick);
					}
					break;
				}
				case 2:
					ui->pm(hubHint);
					break;
				case 3:
					ui->matchQueue();
					break;
				case 4:
					ui->grantSlotPeriod(hubHint, 600);
					break;
				case 5:
					ui->addFav();
					break;
				case 6:
					ui->browseList();
					break;
			}
	}
	return 0;
}

void UserListWindow::insertUserInternal(UserInfo* ui, int pos)
{
	int result = -1;
	if (pos != -1)
		result = ctrlUsers.insertItem(pos, ui, I_IMAGECALLBACK);
	else
		result = ctrlUsers.insertItem(ui, I_IMAGECALLBACK);
	ui->getIdentityRW().getChanges();
	dcassert(result != -1);
}

void UserListWindow::insertUser(UserInfo* ui)
{
#ifdef IRAINMAN_USE_HIDDEN_USERS
	dcassert(!ui->isHidden());
#endif
	//single update
	//avoid refreshing the whole list and just update the current item
	//instead
	if (filter.empty())
	{
		dcassert(ctrlUsers.findItem(ui) == -1);
		insertUserInternal(ui, -1);
	}
	else
	{
		int64_t size = -1;
		FilterModes mode = NONE;
		const int sel = getFilterSelPos();
		bool doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);
		
		if (matchFilter(*ui, sel, doSizeCompare, mode, size))
		{
			dcassert(ctrlUsers.findItem(ui) == -1);
			insertUserInternal(ui, -1);
		}
		else
		{
			//deleteItem checks to see that the item exists in the list
			//unnecessary to do it twice.
			ctrlUsers.deleteItem(ui);
		}
	}
}

void UserListWindow::updateUserList()
{
	CLockRedraw<> lockRedraw(ctrlUsers);
	ctrlUsers.DeleteAllItems();
	if (filter.empty())
	{
		insertUsers();
	}
	else
	{
		int64_t size = -1;
		FilterModes mode = NONE;
		dcassert(ctrlFilterSel);
		const int sel = getFilterSelPos();
		const bool doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);
		READ_LOCK(*csUserMap);
		int pos = 0;
		for (auto i = userMap.cbegin(); i != userMap.cend(); ++i, ++pos)
		{
			UserInfo* ui = i->second;
#ifdef IRAINMAN_USE_HIDDEN_USERS
			dcassert(!ui->isHidden());
#endif
			if (matchFilter(*ui, sel, doSizeCompare, mode, size))
				insertUserInternal(ui, pos);
		}
	}
	shouldSort = false;
	ctrlUsers.resort();
	shouldUpdateStats = true;
}

bool UserListWindow::updateUser(const OnlineUserPtr& ou, uint32_t columnMask, bool isConnected)
{
	UserInfo* ui = nullptr;
	bool isNewUser = false;
	if (!ou->isHidden() && !ou->isHub())
	{
		WRITE_LOCK(*csUserMap);
		auto item = userMap.insert(make_pair(ou, ui));
		if (item.second)
		{
			ui = new UserInfo(ou);
			dcassert(item.first->second == nullptr);
			item.first->second = ui;
			isNewUser = true;
		}
		else
		{
			ui = item.first->second;
		}
	}
	if (isNewUser && showUsers && isConnected)
	{
		shouldSort = true;
		insertUser(ui);
		return true;
	}
	if (ui == nullptr) // Hidden user or hub
		return false;

	// User found, update info
	if (ui->isHidden())
	{
		if (showUsers)
			ctrlUsers.deleteItem(ui);
		{
			WRITE_LOCK(*csUserMap);
			userMap.erase(ou);
		}
		delete ui;
		return true;
	}

	if (showUsers)
	{
		auto changes = ui->getIdentityRW().getChanges();
		//LogManager::message("User " + ui->getNick() + ": changes=0x" + Util::toHexString(changes), false);
		if (changes & 1<<COLUMN_IP)
			ui->stateLocation = ui->stateP2PGuard = UserInfo::STATE_INITIAL;
		if (changes & 1<<ctrlUsers.getSortColumn())
			shouldSort = true;
		const int pos = ctrlUsers.findItem(ui);
		if (pos != -1)
		{
			if (columnMask == (uint32_t) -1)
			{
				ctrlUsers.updateItem(pos);
				// Force icon to redraw
				LVITEM item;
				memset(&item, 0, sizeof(item));
				item.iItem = pos;
				item.mask = LVIF_IMAGE;
				item.iImage = I_IMAGECALLBACK;
				ctrlUsers.SetItem(&item);
			}
			else
			{
				columnMask &= changes;
				for (int columnIndex = 0; columnIndex < COLUMN_LAST; ++columnIndex)
					if (columnMask & 1<<columnIndex)
						ctrlUsers.updateItem(pos, columnIndex);
			}
		}
		else
		{
			// Item not found
		}
	}
	return false;
}

void UserListWindow::removeUser(const OnlineUserPtr& ou)
{
	WRITE_LOCK(*csUserMap);
	const auto it = userMap.find(ou);
	if (it != userMap.end())
	{
		auto ui = it->second;
		userMap.erase(it);
		if (showUsers)
			ctrlUsers.deleteItem(ui);
		delete ui;
	}
}

size_t UserListWindow::insertUsers()
{
	READ_LOCK(*csUserMap);
	int pos = ctrlUsers.GetItemCount();
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i, ++pos)
	{
		UserInfo* ui = i->second;
#ifdef IRAINMAN_USE_HIDDEN_USERS
		dcassert(!ui->isHidden());
#endif
		insertUserInternal(ui, pos);
	}
	return userMap.size();
}

void UserListWindow::setShowUsers(bool flag)
{
	if (showUsers == flag || (flag && !ctrlUsers)) return;
	showUsers = flag;
	if (flag)
	{
		CWaitCursor waitCursor;
		CLockRedraw<> lockRedraw(ctrlUsers);
		insertUsers();
		ctrlUsers.resort();
		shouldSort = false;
	}
	else
		removeListViewItems();
}

UserInfo* UserListWindow::findUser(const OnlineUserPtr& user)
{
	READ_LOCK(*csUserMap);
	auto i = userMap.find(user);
	return i == userMap.end() ? nullptr : i->second;
}

void UserListWindow::insertDHTUsers()
{
	clearUserList();
	dht::DHT* d = dht::DHT::getInstance();
	vector<CID> cidList;
	{
		dht::DHT::LockInstanceNodes lock(d);
		const auto nodes = lock.getNodes();
		if (nodes)
		{
			for (const auto& node : *nodes)
				if (node->getType() < 4 && node->getUser()->hasNick())
					cidList.push_back(node->getUser()->getCID());
		}
	}
	for (const CID& cid : cidList)
	{
		OnlineUserPtr ou = ClientManager::findDHTNode(cid);
		if (ou)
			updateUser(ou, (uint32_t) -1, true);
	}
}

void UserListWindow::clearUserList()
{
	removeListViewItems();
	{
		WRITE_LOCK(*csUserMap);
		for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
			delete i->second;
		userMap.clear();
	}
}

void UserListWindow::removeListViewItems()
{
	if (ctrlUsers)
	{
		CLockRedraw<> lockRedraw(ctrlUsers); // ???
		ctrlUsers.DeleteAllItems();
	}
	shouldSort = false;
}

void UserListWindow::ensureVisible(const UserInfo* ui)
{
	int count = ctrlUsers.GetItemCount();
	int pos = -1;
	CLockRedraw<> lockRedraw(ctrlUsers);
	for (int i = 0; i < count; ++i)
	{
		if (ctrlUsers.getItemData(i) == ui)
			pos = i;
		ctrlUsers.SetItemState(i, (i == pos) ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
	}
	ctrlUsers.EnsureVisible(pos, FALSE);
}


LRESULT UserListWindow::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	//if (isClosedOrShutdown())
	//	return CDRF_DODEFAULT;
	if (ClientManager::isStartup())
		return CDRF_DODEFAULT;
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			CustomDrawHelpers::startDraw(customDrawState, cd);
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			const UserInfo* ui = reinterpret_cast<const UserInfo*>(cd->nmcd.lItemlParam);
			if (!ui) return CDRF_DODEFAULT;
			const int column = ctrlUsers.findColumn(cd->iSubItem);
			if (column == COLUMN_FLY_HUB_GENDER)
			{
				int icon = ui->getIdentity().getGenderType();
				if (icon)
				{
					tstring text = ui->getIdentity().getGenderTypeAsString(icon);
					CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, &g_genderImage.getIconList(), icon-1, text, false);
					return CDRF_SKIPDEFAULT;
				}
			}
			else if (column == COLUMN_IP)
			{
				const IpAddress ip = ui->getIp();
				if (ip.type)
				{
					const bool isPhantomIP = ui->getIdentity().isIPCached(ip.type);
					CustomDrawHelpers::drawIPAddress(customDrawState, cd, isPhantomIP, Util::printIpAddressT(ip));
					return CDRF_SKIPDEFAULT;
				}
			}
			else if (column == COLUMN_GEO_LOCATION)
			{
				const auto& ipInfo = ui->getIpInfo();
				if (!ipInfo.country.empty() || !ipInfo.location.empty())
				{
					CustomDrawHelpers::drawLocation(customDrawState, cd, ipInfo);
					return CDRF_SKIPDEFAULT;
				}
				return CDRF_DODEFAULT;
			}
			else if (column == COLUMN_P2P_GUARD)
			{
				if (ui->stateP2PGuard == UserInfo::STATE_DONE)
				{
					const string text = ui->getIdentity().getP2PGuard();
					if (!text.empty())
					{
						CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, &g_userStateImage.getIconList(), 3, Text::toT(text), false);
						return CDRF_SKIPDEFAULT;
					}
				}
			}
			return CDRF_DODEFAULT;
		}
		case CDDS_ITEMPREPAINT:
		{
			CustomDrawHelpers::startItemDraw(customDrawState, cd);
			UserInfo* ui = reinterpret_cast<UserInfo*>(cd->nmcd.lItemlParam);
			if (ui)
			{
				if (ui->getUser()->testAndClearFlag(User::LAST_IP_CHANGED))
					ui->stateLocation = ui->stateP2PGuard = UserInfo::STATE_INITIAL;
				if (ui->stateLocation == UserInfo::STATE_INITIAL || ui->stateP2PGuard == UserInfo::STATE_INITIAL)
				{
					IpAddress ip = ui->getIp();
					if (ip.type == AF_INET && hubFrame)
					{
						ui->stateLocation = ui->stateP2PGuard = UserInfo::STATE_IN_PROGRESS;
						// FIXME: use background thread instead of the UI thread
						hubFrame->addTask(LOAD_IP_INFO, new OnlineUserTask(ui->getOnlineUser()));
					}
					else
					{
						ui->stateP2PGuard = UserInfo::STATE_DONE;
						ui->clearLocation();
					}
				}
				ui->flags |= IS_FAVORITE | IS_BAN | IS_RESERVED_SLOT;
				getUserColor(cd->clrText, cd->clrTextBk, ui->flags, ui->getOnlineUser());
				return CDRF_NOTIFYSUBITEMDRAW;
			}
		}
		
		default:
			return CDRF_DODEFAULT;
	}
}

LRESULT UserListWindow::onEnterUsers(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	int item = ctrlUsers.GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
	{
		try
		{
			QueueManager::getInstance()->addList(HintedUser((ctrlUsers.getItemData(item))->getUser(), hubHint), QueueItem::FLAG_CLIENT_VIEW);
		}
		catch (const Exception& e)
		{
			if (hubFrame) hubFrame->showErrorMessage(Text::toT(e.getError()));
		}
	}
	return 0;
}

LRESULT UserListWindow::onFilterChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	dcassert(ctrlFilter);
	if (ctrlFilter)
	{
		if (wParam == VK_RETURN || !BOOLSETTING(FILTER_ENTER))
		{
			WinUtil::getWindowText(ctrlFilter, filter);
			filterLower = Text::toLower(filter);
			updateUserList();
		}
	}
	bHandled = FALSE;
	return 0;
}

LRESULT UserListWindow::onClearFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlFilter.SetWindowText(_T(""));
	if (!filter.empty())
	{
		filter.clear();
		filterLower.clear();
		updateUserList();
	}
	return 0;
}

LRESULT UserListWindow::onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	dcassert(ctrlFilter);
	if (ctrlFilter)
	{
		WinUtil::getWindowText(ctrlFilter, filter);
		filterLower = Text::toLower(filter);
		if (ctrlFilterSel)
			filterSelPos = ctrlFilterSel.GetCurSel();
		updateUserList();
	}
	bHandled = FALSE;
	return 0;
}

bool UserListWindow::parseFilter(FilterModes& mode, int64_t& size)
{
	tstring::size_type start = tstring::npos;
	tstring::size_type end = tstring::npos;
	int64_t multiplier = 1;

	if (filterLower.empty())
		return false;
	if (filterLower.compare(0, 2, _T(">="), 2) == 0)
	{
		mode = GREATER_EQUAL;
		start = 2;
	}
	else if (filterLower.compare(0, 2, _T("<="), 2) == 0)
	{
		mode = LESS_EQUAL;
		start = 2;
	}
	else if (filterLower.compare(0, 2, _T("=="), 2) == 0)
	{
		mode = EQUAL;
		start = 2;
	}
	else if (filterLower.compare(0, 2, _T("!="), 2) == 0)
	{
		mode = NOT_EQUAL;
		start = 2;
	}
	else if (filterLower[0] == _T('<'))
	{
		mode = LESS;
		start = 1;
	}
	else if (filterLower[0] == _T('>'))
	{
		mode = GREATER;
		start = 1;
	}
	else if (filterLower[0] == _T('='))
	{
		mode = EQUAL;
		start = 1;
	}

	if (start == tstring::npos)
		return false;
	if (filterLower.length() <= start)
		return false;

	if ((end = filterLower.find(_T("tib"))) != tstring::npos)
		multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
	else if ((end = filterLower.find(_T("gib"))) != tstring::npos)
		multiplier = 1024 * 1024 * 1024;
	else if ((end = filterLower.find(_T("mib"))) != tstring::npos)
		multiplier = 1024 * 1024;
	else if ((end = filterLower.find(_T("kib"))) != tstring::npos)
		multiplier = 1024;
	else if ((end = filterLower.find(_T("tb"))) != tstring::npos)
		multiplier = 1000LL * 1000LL * 1000LL * 1000LL;
	else if ((end = filterLower.find(_T("gb"))) != tstring::npos)
		multiplier = 1000 * 1000 * 1000;
	else if ((end = filterLower.find(_T("mb"))) != tstring::npos)
		multiplier = 1000 * 1000;
	else if ((end = filterLower.find(_T("kb"))) != tstring::npos)
		multiplier = 1000;

	if (end == tstring::npos)
		end = filterLower.length();

	const tstring tmpSize = filterLower.substr(start, end - start);
	size = static_cast<int64_t>(Util::toDouble(Text::fromT(tmpSize)) * multiplier);

	return true;
}

bool UserListWindow::matchFilter(UserInfo& ui, int sel, bool doSizeCompare, FilterModes mode, int64_t size)
{
	if (filter.empty())
		return true;

	bool insert = false;
	if (doSizeCompare)
	{
		switch (mode)
		{
			case EQUAL:
				insert = (size == ui.getIdentity().getBytesShared());
				break;
			case GREATER_EQUAL:
				insert = (size <=  ui.getIdentity().getBytesShared());
				break;
			case LESS_EQUAL:
				insert = (size >=  ui.getIdentity().getBytesShared());
				break;
			case GREATER:
				insert = (size < ui.getIdentity().getBytesShared());
				break;
			case LESS:
				insert = (size > ui.getIdentity().getBytesShared());
				break;
			case NOT_EQUAL:
				insert = (size != ui.getIdentity().getBytesShared());
				break;
		}
	}
	else
	{
		if (sel >= COLUMN_LAST)
		{
			for (uint8_t i = COLUMN_FIRST; i < COLUMN_LAST; ++i)
			{
				const tstring s = Text::toLower(ui.getText(i));
				if (s.find(filterLower) != tstring::npos)
				{
					insert = true;
					break;
				}
			}
		}
		else
		{
			if (sel == COLUMN_GEO_LOCATION)
			{
				ui.loadLocation();
			}
			else if (sel == COLUMN_P2P_GUARD)
			{
				ui.loadP2PGuard();
			}
			const tstring s = Text::toLower(ui.getText(sel));
			if (s.find(filterLower) != tstring::npos)
				insert = true;
		}
	}
	return insert;
}

void UserListWindow::getUserColor(COLORREF& fg, COLORREF& bg, unsigned short& flags, const OnlineUserPtr& onlineUser)
{
	const UserPtr& user = onlineUser->getUser();
	auto statusFlags = onlineUser->getIdentity().getStatus();
	bg = Colors::g_bgColor;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	if (SETTING(ENABLE_AUTO_BAN))
	{
		if ((flags & IS_AUTOBAN) == IS_AUTOBAN)
		{
			bool isFav = false;
			if (user->hasAutoBan(&onlineUser->getClient(), isFav) != User::BAN_NONE)
				flags = (flags & ~IS_AUTOBAN) | IS_AUTOBAN_ON;
			else
				flags = (flags & ~IS_AUTOBAN);
			if (isFav)
				flags = (flags & ~IS_FAVORITE) | IS_FAVORITE_ON;
			else
				flags = (flags & ~IS_FAVORITE);
		}
		if (flags & IS_AUTOBAN)
		{
			bg = SETTING(BAN_COLOR);
		}
	}
#endif // IRAINMAN_ENABLE_AUTO_BAN
#ifdef FLYLINKDC_USE_DETECT_CHEATING
	if (isOp)
	{
	
		const auto fc = onlineUser->getIdentity().getFakeCard();
		if (fc & Identity::BAD_CLIENT)
		{
			fg = SETTING(BAD_CLIENT_COLOR);
			return;
		}
		else if (fc & Identity::BAD_LIST)
		{
			fg = SETTING(BAD_FILELIST_COLOR);
			return;
		}
		else if (fc & Identity::CHECKED && BOOLSETTING(SHOW_SHARE_CHECKED_USERS))
		{
			fg = SETTING(FULL_CHECKED_COLOR);
			return;
		}
	}
#endif // FLYLINKDC_USE_DETECT_CHEATING
	dcassert(user);
	const auto userFlags = user->getFlags();
	if ((flags & IS_IGNORED_USER) == IS_IGNORED_USER)
	{
		flags &= ~IS_IGNORED_USER;
		if (UserManager::getInstance()->isInIgnoreList(onlineUser->getIdentity().getNick()))
			flags |= IS_IGNORED_USER_ON;
	}
	if ((flags & IS_RESERVED_SLOT) == IS_RESERVED_SLOT)
	{
		flags &= ~IS_RESERVED_SLOT;
		if (userFlags & User::RESERVED_SLOT)
			flags |= IS_RESERVED_SLOT_ON;
	}
	if ((flags & IS_FAVORITE) == IS_FAVORITE || (flags & IS_BAN) == IS_BAN)
	{
		flags &= ~(IS_FAVORITE | IS_BAN);
		if (userFlags & User::FAVORITE)
		{
			flags |= IS_FAVORITE_ON;
			if (userFlags & User::BANNED)
				flags |= IS_BAN_ON;
		}
	}
	
	if (flags & IS_RESERVED_SLOT)
	{
		fg = SETTING(RESERVED_SLOT_COLOR);
	}
	else if (flags & IS_FAVORITE_ON)
	{
		if (flags & IS_BAN_ON)
			fg = SETTING(TEXT_ENEMY_FORE_COLOR);
		else
			fg = SETTING(FAVORITE_COLOR);
	}
	else if (onlineUser->getIdentity().isOp())
	{
		fg = SETTING(OP_COLOR);
	}
	else if (flags & IS_IGNORED_USER)
	{
		fg = SETTING(IGNORED_COLOR);
	}
	else if (statusFlags & Identity::SF_FIREBALL)
	{
		fg = SETTING(FIREBALL_COLOR);
	}
	else if (statusFlags & Identity::SF_SERVER)
	{
		fg = SETTING(SERVER_COLOR);
	}
	else if (statusFlags & Identity::SF_PASSIVE)
	{
		fg = SETTING(PASSIVE_COLOR);
	}
	else
	{
		fg = SETTING(NORMAL_COLOR);
	}
}

void UserListWindow::onIgnoreListChanged(const string& userName)
{
	WRITE_LOCK(*csUserMap);
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
	{
		UserInfo* ui = i->second;
		if (ui->getUser()->getLastNick() == userName)
			ui->flags |= IS_IGNORED_USER; // flag IS_IGNORED_USER_ON will be updated
	}
}

void UserListWindow::onIgnoreListCleared()
{
	WRITE_LOCK(*csUserMap);
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
	{
		UserInfo* ui = i->second;
		ui->flags &= ~IS_IGNORED_USER; // flag IS_IGNORED_USER_ON is cleared and won't be updated
	}
}

bool UserListWindow::checkSortFlag()
{
	if (!shouldSort || !ctrlUsers) return false;
	shouldSort = false;
	ctrlUsers.resort();
	//LogManager::message("Resort! Hub = " + baseClient->getHubUrl() + " count = " + Util::toString(ctrlUsers ? itemCount : 0));
	return true;
}

void UserListWindow::updateLayout()
{
	if (!ctrlUsers || !ctrlFilter) return;

	CRect rect;
	GetClientRect(&rect);

	int xdu, ydu;
	WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
	int comboWidth = WinUtil::dialogUnitsToPixelsX(80, xdu);
	int comboHeight = WinUtil::getComboBoxHeight(ctrlFilterSel, nullptr);
	int editHeight = std::max(comboHeight, BUTTON_SIZE);
	
	HDWP dwp = BeginDeferWindowPos(4);
	CRect rc;
	rc.right = rect.right - 2;
	rc.top = rect.bottom - 4 - editHeight + 1;
	rc.bottom = rc.top + 256;
	rc.left = rc.right - comboWidth;
	ctrlFilterSel.DeferWindowPos(dwp, nullptr, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER);

	rc.right = rc.left - 2;
	rc.left = rc.right - editHeight;
	rc.top--;
	rc.bottom = rc.top + editHeight;
	ctrlClearFilter.DeferWindowPos(dwp, nullptr, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER);

	rc.right = rc.left - 2;
	rc.left = 2;
	ctrlFilter.DeferWindowPos(dwp, nullptr, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER);

	rect.bottom = rc.top - 4;
	ctrlUsers.DeferWindowPos(dwp, nullptr, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER);
	EndDeferWindowPos(dwp);
}

bool UserListWindow::showHeaderMenu(POINT pt)
{
	RECT rc;
	ctrlUsers.GetHeader().GetWindowRect(&rc);
	if (PtInRect(&rc, pt))
	{
		ctrlUsers.showMenu(pt);
		return true;
	}
	return false;
}

UserInfo* UserListWindow::getSelectedUserInfo(bool* isMultiple) const
{
	int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
	if (i < 0) return nullptr;
	UserInfo* ui = ctrlUsers.getItemData(i);
	if (isMultiple)
		*isMultiple = ctrlUsers.GetNextItem(i, LVNI_SELECTED) != -1;
	return ui;
}

bool UserListWindow::selectNick(const tstring& nick)
{
	const int pos = ctrlUsers.findItem(nick);
	if (pos != -1)
	{
		CLockRedraw<> lockRedraw(ctrlUsers);
		const auto perPage = ctrlUsers.GetCountPerPage();
		const int items = ctrlUsers.GetItemCount();
		for (int i = 0; i < items; ++i)
			ctrlUsers.SetItemState(i, i == pos ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
		ctrlUsers.EnsureVisible(pos, FALSE);
		const auto lastPos = pos + perPage / 2;
		if (!ctrlUsers.EnsureVisible(lastPos, FALSE))
			ctrlUsers.EnsureVisible(pos, FALSE);
		return true;
	}
	return false;
}

void UserListWindow::getDupUsers(const ClientManager::UserParams& param, const tstring& hubTitle, vector<std::pair<tstring, UINT>>& menuStrings) const
{
	auto fm = FavoriteManager::getInstance();
	bool hasIP6 = !Util::isEmpty(param.ip6);
	READ_LOCK(*csUserMap);
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
	{
		const auto& id = i->second->getIdentity();
		const Ip4Address currentIp4 = id.getUser()->getIP4();
		const Ip6Address currentIp6 = id.getUser()->getIP6();
		bool nickMatches = param.nick == id.getNick();
		bool ipMatches = (param.ip4 && param.ip4 == currentIp4) || (hasIP6 && param.ip6 == currentIp6);
		if (nickMatches || ipMatches)
		{
			tstring info = hubTitle + _T(" - ") + i->second->getText(COLUMN_NICK);
			const UINT flags = ipMatches ? MF_CHECKED : 0;
			FavoriteUser favUser;
			if (fm->getFavoriteUser(i->second->getUser(), favUser))
			{
				string favInfo;
				if (favUser.isSet(FavoriteUser::FLAG_GRANT_SLOT))
					favInfo += ' ' + STRING(AUTO_GRANT);
				if (favUser.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
					favInfo += ' ' + STRING(IGNORE_PRIVATE);
				if (favUser.uploadLimit != FavoriteUser::UL_NONE)
					favInfo += ' ' + UserInfo::getSpeedLimitText(favUser.uploadLimit);
				if (!favUser.description.empty())
					favInfo += " \"" + favUser.description + '\"';
				if (!favInfo.empty())
					info += _T(", FavInfo: ") + Text::toT(favInfo);
			}
			menuStrings.push_back(make_pair(info, flags));
			menuStrings.push_back(make_pair(UserInfoSimple::getTagIP(id.getTag(), currentIp4, currentIp6), 0));
		}
	}
}

void UserListWindow::setHubHint(const string& hint)
{
	hubHint = hint;
}

bool UserListWindow::loadIPInfo(const OnlineUserPtr& ou)
{
	READ_LOCK(*csUserMap);
	auto j = userMap.find(ou); // FIXME
	if (j == userMap.end()) return false;
	UserInfo* ui = j->second;
	ui->loadLocation();
	ui->loadP2PGuard();
	// FIXME: ui->loadIPInfo();
	return true;
}
