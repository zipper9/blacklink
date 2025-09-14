#include "stdafx.h"
#include "UserListWindow.h"
#include "HubFrameTasks.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "LockRedraw.h"
#include "UserTypeColors.h"
#include "../client/UserManager.h"
#include "../client/UploadManager.h"
#include "../client/QueueManager.h"
#include "../client/GlobalState.h"
#include "../client/Util.h"
#include "../client/dht/DHT.h"

using namespace UserTypeColors;

static const int columnSizes[] =
{
	100,    // COLUMN_NICK
	75,     // COLUMN_SHARED
	150,    // COLUMN_EXACT_SHARED
	150,    // COLUMN_DESCRIPTION
	150,    // COLUMN_APPLICATION
	50,     // COLUMN_EMAIL
	40,     // COLUMN_HUBS
	40,     // COLUMN_SLOTS
	100,    // COLUMN_IP
	100,    // COLUMN_GEO_LOCATION
#ifdef BL_FEATURE_IP_DATABASE
	50,     // COLUMN_UPLOAD
	50,     // COLUMN_DOWNLOAD
	10,     // COLUMN_MESSAGES
#endif
	300,    // COLUMN_CID
	200,    // COLUMN_TAG
	40,     // COLUMN_P2P_GUARD
#ifdef BL_FEATURE_NMDC_EXT_JSON
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
	COLUMN_EMAIL,
	COLUMN_HUBS,
	COLUMN_SLOTS,
	COLUMN_IP,
	COLUMN_GEO_LOCATION,
#ifdef BL_FEATURE_IP_DATABASE
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
#ifdef BL_FEATURE_NMDC_EXT_JSON
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
	ResourceManager::EMAIL,           // COLUMN_EMAIL
	ResourceManager::HUBS,            // COLUMN_HUBS
	ResourceManager::SLOTS,           // COLUMN_SLOTS
	ResourceManager::IP,              // COLUMN_IP
	ResourceManager::LOCATION_BARE,   // COLUMN_GEO_LOCATION
#ifdef BL_FEATURE_IP_DATABASE
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
#ifdef BL_FEATURE_NMDC_EXT_JSON
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

UserListWindow::UserListWindow(HubFrameCallbacks* hubFrame) : hubFrame(hubFrame)
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	showHidden = ss->getBool(Conf::SHOW_HIDDEN_USERS);
	showUsers = false; // can't be set to true until ctrlUsers is created
	shouldSort = false;
	isOp = false;

	xdu = ydu = 0;

	filterSelPos = COLUMN_NICK;

	ctrlUsers.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlUsers.setColumnOwnerDraw(COLUMN_GEO_LOCATION);
	ctrlUsers.setColumnOwnerDraw(COLUMN_IP);
#ifdef BL_FEATURE_IP_DATABASE
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
		const auto* ss = SettingsManager::instance.getUiSettings();
		ctrlUsers.setSortFromSettings(ss->getInt(Conf::HUB_FRAME_SORT));
	}
}

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

	ctrlSearchBox.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP, 0, IDC_EDIT);
	ctrlSearchBox.setHint(TSTRING(FILTER_HINT));
	ctrlSearchBox.SetFont(Fonts::g_systemFont);
	ctrlSearchBox.setBitmap(g_iconBitmaps.getBitmap(IconBitmaps::FILTER, 0));
	ctrlSearchBox.setCloseBitmap(g_iconBitmaps.getBitmap(IconBitmaps::CLEAR_SEARCH, 0));
	ctrlSearchBox.setNotifMask(SearchBoxCtrl::NOTIF_RETURN | SearchBoxCtrl::NOTIF_TAB);

	ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlFilterSel.SetFont(Fonts::g_systemFont);

	WinUtil::fillComboBoxStrings(ctrlFilterSel, columnNames, COLUMN_LAST);
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
		{
			const auto* ss = SettingsManager::instance.getUiSettings();
			switch (ss->getInt(Conf::USERLIST_DBLCLICK))
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
					ui->pm();
					break;
				case 3:
					ui->matchQueue();
					break;
				case 4:
					ui->grantSlotPeriod(600);
					break;
				case 5:
					ui->addFav();
					break;
				case 6:
					ui->browseList();
					break;
			}
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

bool UserListWindow::shouldShowUser(const UserInfo* ui) const
{
	if (showHidden) return true;
	const Identity& identity = ui->getIdentity();
	return !identity.isHidden() && !identity.isHub();
}

bool UserListWindow::insertUser(UserInfo* ui)
{
	if (!shouldShowUser(ui))
		return false;

	//single update
	//avoid refreshing the whole list and just update the current item
	//instead
	if (filter.empty())
	{
		dcassert(ctrlUsers.findItem(ui) == -1);
		insertUserInternal(ui, -1);
		return true;
	}

	int64_t size = -1;
	FilterModes mode = NONE;
	const int sel = getFilterSelPos();
	bool doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);

	if (matchFilter(*ui, sel, doSizeCompare, mode, size))
	{
		dcassert(ctrlUsers.findItem(ui) == -1);
		insertUserInternal(ui, -1);
		return true;
	}
	//deleteItem checks to see that the item exists in the list
	//unnecessary to do it twice.
	ctrlUsers.deleteItem(ui);
	return false;
}

void UserListWindow::updateUserList()
{
	ASSERT_MAIN_THREAD();
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
		int pos = 0;
		for (auto i = userMap.cbegin(); i != userMap.cend(); ++i, ++pos)
		{
			UserInfo* ui = i->second;
			if (matchFilter(*ui, sel, doSizeCompare, mode, size) && shouldShowUser(ui))
				insertUserInternal(ui, pos);
		}
	}
	shouldSort = false;
	ctrlUsers.resort();
	hubFrame->updateUserCount();
}

bool UserListWindow::updateUser(const OnlineUserPtr& ou, uint32_t columnMask, bool isConnected)
{
	ASSERT_MAIN_THREAD();
	UserInfo* ui = nullptr;
	bool isNewUser = false;
	{
		auto item = userMap.insert(make_pair(ou, ui));
		if (item.second)
		{
			ui = new UserInfo(ou);
			dcassert(item.first->second == nullptr);
			item.first->second = ui;
			isNewUser = true;
		}
		else
			ui = item.first->second;
	}
	if (isNewUser && showUsers && isConnected)
	{
		if (insertUser(ui)) shouldSort = true;
		return true;
	}
	if (!ui) // User not found
		return false;

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
	ASSERT_MAIN_THREAD();
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
	ASSERT_MAIN_THREAD();
	int pos = ctrlUsers.GetItemCount();
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i, ++pos)
	{
		UserInfo* ui = i->second;
		if (shouldShowUser(ui)) insertUserInternal(ui, pos);
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

void UserListWindow::setShowHidden(bool flag)
{
	ASSERT_MAIN_THREAD();
	if (showHidden == flag) return;
	showHidden = flag;
	if (!showUsers || !ctrlUsers) return;
	bool update = false;
	CLockRedraw<> lockRedraw(ctrlUsers);
	if (showHidden)
	{
		int64_t size = -1;
		FilterModes mode = NONE;
		int sel = 0;
		bool doSizeCompare = false;
		if (!filter.empty())
		{
			dcassert(ctrlFilterSel);
			sel = getFilterSelPos();
			doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);
		}
		for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
		{
			const Identity& identity = i->first->getIdentity();
			if (identity.isHidden() || identity.isHub())
			{
				UserInfo* ui = i->second;
				if (filter.empty() || matchFilter(*ui, sel, doSizeCompare, mode, size))
				{
					insertUserInternal(ui, -1);
					update = true;
				}
			}
		}
	}
	else
	{
		for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
		{
			const Identity& identity = i->first->getIdentity();
			if (identity.isHidden() || identity.isHub())
			{
				UserInfo* ui = i->second;
				ctrlUsers.deleteItem(ui);
				update = true;
			}
		}
	}
	if (update)
	{
		shouldSort = false;
		ctrlUsers.resort();
	}
}

UserInfo* UserListWindow::findUser(const OnlineUserPtr& user) const
{
	ASSERT_MAIN_THREAD();
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
	ASSERT_MAIN_THREAD();
	removeListViewItems();
	{
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
	if (GlobalState::isStartingUp())
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
					if (ip.type && hubFrame)
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
				cd->clrTextBk = Colors::g_bgColor;
				cd->clrText = getColor(ui->flags, ui->getOnlineUser());
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
			QueueManager::getInstance()->addList(HintedUser((ctrlUsers.getItemData(item))->getUser(), hubHint), 0, QueueItem::XFLAG_CLIENT_VIEW);
		}
		catch (const Exception& e)
		{
			if (hubFrame) hubFrame->showErrorMessage(Text::toT(e.getError()));
		}
	}
	return 0;
}

LRESULT UserListWindow::onNextDlgCtl(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	MSG msg = {};
	msg.hwnd = ctrlSearchBox.m_hWnd;
	msg.message = WM_KEYDOWN;
	msg.wParam = VK_TAB;
	IsDialogMessage(&msg);
	return 0;
}

LRESULT UserListWindow::onFilterChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!SettingsManager::instance.getUiSettings()->getBool(Conf::FILTER_ENTER))
	{
		filter = ctrlSearchBox.getText();
		filterLower = Text::toLower(filter);
		updateUserList();
	}
	return 0;
}

LRESULT UserListWindow::onFilterReturn(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (SettingsManager::instance.getUiSettings()->getBool(Conf::FILTER_ENTER))
	{
		filter = ctrlSearchBox.getText();
		filterLower = Text::toLower(filter);
		updateUserList();
	}
	return 0;
}

LRESULT UserListWindow::onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	filter = ctrlSearchBox.getText();
	filterLower = Text::toLower(filter);
	if (ctrlFilterSel)
		filterSelPos = ctrlFilterSel.GetCurSel();
	updateUserList();
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

void UserListWindow::onIgnoreListChanged()
{
	ASSERT_MAIN_THREAD();
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
	{
		UserInfo* ui = i->second;
		ui->flags |= IS_IGNORED_USER; // flag IS_IGNORED_USER_ON will be updated
	}
}

void UserListWindow::onIgnoreListCleared()
{
	ASSERT_MAIN_THREAD();
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
	if (!ctrlUsers || !ctrlSearchBox) return;

	CRect rect;
	GetClientRect(&rect);

	if (!xdu) WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
	int comboWidth = WinUtil::dialogUnitsToPixelsX(80, xdu);
	int comboHeight = WinUtil::getComboBoxHeight(ctrlFilterSel, nullptr);
	int editHeight = comboHeight;
	int minSearchBoxWidth = WinUtil::dialogUnitsToPixelsX(60, xdu);

	if (minSearchBoxWidth + comboWidth + 6 <= rect.Width())
	{
		HDWP dwp = BeginDeferWindowPos(4);
		CRect rc;
		rc.right = rect.right - 2;
		rc.top = rect.bottom - 4 - editHeight + 1;
		rc.bottom = rc.top + 256;
		rc.left = rc.right - comboWidth;
		ctrlFilterSel.DeferWindowPos(dwp, nullptr, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);

		rc.right = rc.left - 2;
		rc.left = 2;
		rc.bottom = rc.top + editHeight;
		ctrlSearchBox.DeferWindowPos(dwp, nullptr, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);

		rect.bottom = rc.top - 4;
		ctrlUsers.DeferWindowPos(dwp, nullptr, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER);
		EndDeferWindowPos(dwp);
	}
	else
	{
		ctrlSearchBox.ShowWindow(SW_HIDE);
		ctrlFilterSel.ShowWindow(SW_HIDE);
		ctrlUsers.SetWindowPos(nullptr, &rect, SWP_NOZORDER | SWP_NOACTIVATE);
	}
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

void UserListWindow::getSelectedUsers(vector<OnlineUserPtr>& v) const
{
	int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
	while (i >= 0)
	{
		UserInfo* ui = ctrlUsers.getItemData(i);
		v.push_back(ui->getOnlineUser());
		i = ctrlUsers.GetNextItem(i, LVNI_SELECTED);
	}
}

void UserListWindow::selectItem(int pos)
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
}

bool UserListWindow::selectNick(const tstring& nick)
{
	const int pos = ctrlUsers.findItem(nick);
	if (pos != -1)
	{
		selectItem(pos);
		return true;
	}
	return false;
}

bool UserListWindow::selectCID(const CID& cid)
{
	int pos = -1;
	int count = ctrlUsers.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		const auto* data = ctrlUsers.getItemData(i);
		if (data->getUser()->getCID() == cid)
		{
			pos = i;
			break;
		}
	}
	if (pos != -1)
	{
		selectItem(pos);
		return true;
	}
	return false;
}

void UserListWindow::getDupUsers(const ClientManager::UserParams& param, const tstring& hubTitle, const string& hubUrl, UINT& idc, vector<UserInfoGuiTraits::DetailsItem>& items) const
{
	ASSERT_MAIN_THREAD();
	auto fm = FavoriteManager::getInstance();
	bool hasIP6 = !Util::isEmpty(param.ip6);
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
	{
		const auto& id = i->second->getIdentity();
		const Ip4Address currentIp4 = id.getUser()->getIP4();
		const Ip6Address currentIp6 = id.getUser()->getIP6();
		bool nickMatches = param.nick == id.getNick();
		bool ipMatches = (param.ip4 && param.ip4 == currentIp4) || (hasIP6 && param.ip6 == currentIp6);
		if (nickMatches || ipMatches)
		{
			const UserPtr& user = i->second->getUser();
			tstring info = hubTitle + _T(" - ") + i->second->getText(COLUMN_NICK);
			const UINT flags = ipMatches ? MF_CHECKED : 0;
			FavoriteUser favUser;
			string favInfo;
			if (fm->getFavoriteUser(user, favUser))
			{
				favInfo = std::move(favUser.description);
				if (favUser.isSet(FavoriteUser::FLAG_GRANT_SLOT))
				{
					if (!favInfo.empty()) favInfo += ", ";
					favInfo += STRING(AUTO_GRANT);
				}
				if (favUser.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
				{
					if (!favInfo.empty()) favInfo += ", ";
					favInfo += STRING(IGNORE_PRIVATE);
				}
				if (favUser.uploadLimit != FavoriteUser::UL_NONE)
				{
					if (!favInfo.empty()) favInfo += ", ";
					favInfo += UserInfo::getSpeedLimitText(favUser.uploadLimit);
				}
			}
			items.emplace_back(UserInfoGuiTraits::DetailsItem{UserInfoGuiTraits::DetailsItem::TYPE_USER,
				idc++, flags | MF_SEPARATOR, info, user->getCID(), hubUrl});
			if (!favInfo.empty())
				items.emplace_back(UserInfoGuiTraits::DetailsItem{UserInfoGuiTraits::DetailsItem::TYPE_FAV_INFO,
					idc++, 0, Text::toT(STRING_F(FAVUSER_INFO_FMT, favInfo)), user->getCID(), hubUrl});
			tstring tag = UserInfoSimple::getTagIP(id.getTag(), currentIp4, currentIp6);
			if (!tag.empty())
				items.emplace_back(UserInfoGuiTraits::DetailsItem{UserInfoGuiTraits::DetailsItem::TYPE_TAG,
					0, 0, tag, user->getCID(), hubUrl});
		}
	}
}

void UserListWindow::setHubHint(const string& hint)
{
	hubHint = hint;
}

bool UserListWindow::loadIPInfo(const OnlineUserPtr& ou)
{
	ASSERT_MAIN_THREAD();
	auto j = userMap.find(ou); // FIXME
	if (j == userMap.end()) return false;
	UserInfo* ui = j->second;
	ui->loadLocation();
	ui->loadP2PGuard();
	// FIXME: ui->loadIPInfo();
	return true;
}
