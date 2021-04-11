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
#include "HubFrame.h"
#include "SearchFrm.h"
#include "PrivateFrame.h"
#include "MainFrm.h"
#include "../client/QueueManager.h"
#include "../client/ShareManager.h"
#include "../client/Util.h"
#include "../client/LogManager.h"
#include "../client/AdcCommand.h"
#include "../client/SettingsManager.h"
#include "../client/ConnectionManager.h"
#include "../client/ConnectivityManager.h"
#include "../client/NmdcHub.h"
#include "../client/UploadManager.h"
#include "../client/dht/DHT.h"
#include "FavHubProperties.h"
#include "LineDlg.h"

static const unsigned TIMER_VAL = 1000;
static const int INFO_UPDATE_INTERVAL = 60;
static const int STATUS_PART_PADDING = 12;

HubFrame::FrameMap HubFrame::frames;
CriticalSection HubFrame::csFrames;

HIconWrapper HubFrame::iconSwitchPanels(IDR_SWITCH_PANELS_ICON);
HIconWrapper HubFrame::iconModeActive(IDR_MODE_ACTIVE_ICO);
HIconWrapper HubFrame::iconModePassive(IDR_MODE_PASSIVE_ICO);
HIconWrapper HubFrame::iconModeNone(IDR_MODE_OFFLINE_ICO);

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

const int HubFrame::columnId[] =
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

template<>
string UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::getNick(const OnlineUserPtr& user)
{
	return user->getIdentity().getNick();
}

HubFrame::HubFrame(const string& server,
                   const string& name,
                   const string rawCommands[],
                   int  chatUserSplit,
                   bool hideUserList,
                   bool suppressChatAndPM) :
	timer(m_hWnd)
	, client(nullptr)
	, infoUpdateSeconds(INFO_UPDATE_INTERVAL)
	, hubUpdateCount(0)
#if 0
	, m_is_first_goto_end(false)
#endif
	, waitingForPassword(false)
	, showingPasswordDlg(false)
	, shouldUpdateStats(false)
	, shouldSort(false)
	, showUsersContainer(nullptr)
	, ctrlFilterContainer(nullptr)
	, ctrlChatContainer(nullptr)
	, ctrlFilterSelContainer(nullptr)
	, filterSelPos(COLUMN_NICK)
	, swapPanels(false)
	, switchPanelsContainer(nullptr)
	, isTabMenuShown(false)
	, showJoins(false)
	, showFavJoins(false)
	, updateColumnsInfoProcessed(false)
	, tabMenu(nullptr)
	, bytesShared(0)
	, activateCounter(0)
	, hubParamUpdated(false)
	, m_is_ddos_detect(false)
	, m_count_lock_chat(0)
	, asyncUpdate(0)
	, asyncUpdateSaved(0)
{
	prevCursorX = prevCursorY = INT_MAX;
	csUserMap = std::unique_ptr<RWLock>(RWLock::create());
	ctrlStatusCache.resize(5);
	showUsersStore = !hideUserList;
	showUsers = false;
	serverUrl = server;
	m_nProportionalPos = chatUserSplit;

	if (server == dht::NetworkName)
	{
		dht::DHT* d = dht::DHT::getInstance();
		d->addListener(this);
		baseClient = d;
		client = nullptr;
		isDHT = true;
		currentDHTState = d->getState();
		currentDHTNodeCount = d->getNodesCount();
	}
	else
	{
		client = ClientManager::getInstance()->getClient(serverUrl);
		client->setName(name);
		if (rawCommands) client->setRawCommands(rawCommands);
		client->setSuppressChatAndPM(suppressChatAndPM);
		client->addListener(this);
		baseClient = client;
		isDHT = false;
		currentDHTState = 0;
		currentDHTNodeCount = 0;
	}

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

void HubFrame::doDestroyFrame()
{
	destroyUserMenu();
	destroyTabMenu();
	destroyMessagePanel(true);
}

void HubFrame::destroyTabMenu()
{
	delete tabMenu;
	tabMenu = nullptr;
}

HubFrame::~HubFrame()
{
	removeFrame(Util::emptyString);
	delete ctrlChatContainer;
	if (client)
		ClientManager::getInstance()->putClient(client);
	dcassert(userMap.empty());
}

void HubFrame::createCtrlUsers()
{
	if (!ctrlUsers.m_hWnd)
	{
		ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_STATICEDGE, IDC_USERS);
		ctrlUsers.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
		if (WinUtil::setExplorerTheme(ctrlUsers))
			customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED | CustomDrawHelpers::FLAG_USE_HOT_ITEM;
	}
}

LRESULT HubFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(HubFrame::columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(HubFrame::columnId));
	
	BaseChatFrame::onCreate(m_hWnd, rcDefault);
	
	setHubParam();
	
	// TODO - отложить создание контрола...
	// TODO - может колонки не создвать пока они не нужны?
	bHandled = FALSE;
	if (isDHT)
	{
		SetWindowText(CTSTRING(DHT_TITLE));
		dht::DHT* d = dht::DHT::getInstance();
		const tstring& connState = TSTRING_I(d->isConnected() ? ResourceManager::DHT_CONN_STATE_YES : ResourceManager::DHT_CONN_STATE_NO);
		string ipAddress;
		bool isFirewalled;
		d->getPublicIPInfo(ipAddress, isFirewalled);
		const tstring& portState = TSTRING_I(isFirewalled ? ResourceManager::DHT_PORT_STATE_FIREWALLED : ResourceManager::DHT_PORT_STATE_OPEN);
		uint16_t port = dht::DHT::getPort();
		tstring message = TSTRING_F(DHT_STATUS_MESSAGE, connState % Text::toT(ipAddress) % port % portState);
		addSystemMessage(message, Colors::g_ChatTextSystem);
	}
	else
		updateWindowTitle();
	prevHubName.clear();
	if (client)
	{
		client->connectIfNetworkOk();
#if 0
		const FavoriteHubEntry* fe = FavoriteManager::getFavoriteHubEntry(client->getHubUrl());
		bool isFavActive = fe ? ClientManager::isActive(fe) : false;
		LogManager::message("Connect: " + client->getHubUrl() + string(" Mode: ") +
		                    (client->isActive() ? ("Active" + (isFavActive ? string("(favorites)") : string())) : "Passive") + string(" Support: ") +
			                ConnectivityManager::getInstance()->getPortmapInfo(true));
#endif
	}
	timer.createTimer(TIMER_VAL, 2);
	return 1;
}

void HubFrame::updateColumnsInfo(const FavoriteManager::WindowInfo& wi)
{
	if (!updateColumnsInfoProcessed)
	{
		FavoriteManager::getInstance()->addListener(this);
		UserManager::getInstance()->addListener(this);
		SettingsManager::getInstance()->addListener(this);
		updateColumnsInfoProcessed = true;
		ctrlUsers.insertColumns(wi.headerOrder, wi.headerWidths, wi.headerVisible);
		// ctrlUsers.SetCallbackMask(ctrlUsers.GetCallbackMask() | LVIS_STATEIMAGEMASK);

		setListViewColors(ctrlUsers);
		// ctrlUsers.setSortColumn(-1); // TODO - научится сортировать после активации фрейма а не в начале
		if (wi.headerSort >= 0)
		{
			ctrlUsers.setSortColumn(wi.headerSort);
			ctrlUsers.setAscending(wi.headerSortAsc);
		}
		else
		{
			ctrlUsers.setSortFromSettings(SETTING(HUB_FRAME_SORT));
		}
		ctrlUsers.SetImageList(g_userImage.getIconList(), LVSIL_SMALL);
	}
}

void HubFrame::updateSplitterPosition(int chatUserSplit, bool swapFlag)
{
	dcassert(activateCounter == 1);
	m_nProportionalPos = 0;
	if (chatUserSplit != -1)
	{
		swapPanels = swapFlag;
		m_nProportionalPos = chatUserSplit;
	}
	else
		swapPanels = false;
	if (m_nProportionalPos == 0)
	{
		swapPanels = SETTING(HUB_POSITION) != SettingsManager::POS_RIGHT;
		m_nProportionalPos = swapPanels ? 2500 : 7500;
	}
	setSplitterPanes();
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
}

void HubFrame::createMessagePanel()
{
	bool updateFlag = false;
	dcassert(!ClientManager::isBeforeShutdown());
	if (!isClosedOrShutdown())
	{
		if (!ctrlFilter.m_hWnd && !ClientManager::isStartup())
		{
			++activateCounter;
			createCtrlUsers();
			BaseChatFrame::createMessageCtrl(this, EDIT_MESSAGE_MAP, isSuppressChatAndPM());
			dcassert(!ctrlFilterContainer);
			ctrlFilterContainer = new CContainedWindow(WC_EDIT, this, FILTER_MESSAGE_MAP);
			ctrlFilter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			                  ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
			ctrlFilterContainer->SubclassWindow(ctrlFilter.m_hWnd);
			ctrlFilter.SetFont(Fonts::g_systemFont);
			if (!filter.empty())
				ctrlFilter.SetWindowText(filter.c_str());
			
			ctrlFilterSelContainer = new CContainedWindow(WC_COMBOBOX, this, FILTER_MESSAGE_MAP);
			ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
			                     WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
			ctrlFilterSelContainer->SubclassWindow(ctrlFilterSel.m_hWnd);
			ctrlFilterSel.SetFont(Fonts::g_systemFont);
			
			for (size_t j = 0; j < COLUMN_LAST; ++j)
				ctrlFilterSel.AddString(CTSTRING_I(columnNames[j]));

			ctrlFilterSel.AddString(CTSTRING(ANY));
			ctrlFilterSel.SetCurSel(filterSelPos);
			
			tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
			tooltip.SetDelayTime(TTDT_AUTOPOP, 10000);
			dcassert(tooltip.IsWindow());
			tooltip.SetMaxTipWidth(255);
			
			CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
			BaseChatFrame::createStatusCtrl(m_hWndStatusBar);
			
			switchPanelsContainer = new CContainedWindow(WC_BUTTON, this, HUBSTATUS_MESSAGE_MAP),
			ctrlSwitchPanels.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER | BS_PUSHBUTTON, 0, IDC_HUBS_SWITCHPANELS);
			ctrlSwitchPanels.SetFont(Fonts::g_systemFont);
			ctrlSwitchPanels.SetIcon(iconSwitchPanels);
			switchPanelsContainer->SubclassWindow(ctrlSwitchPanels.m_hWnd);
			tooltip.AddTool(ctrlSwitchPanels, ResourceManager::CMD_SWITCHPANELS);

			ctrlShowUsers.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
			ctrlShowUsers.SetButtonStyle(BS_AUTOCHECKBOX, false);
			ctrlShowUsers.SetFont(Fonts::g_systemFont);
			ctrlShowUsers.SetCheck(showUsersStore ? BST_CHECKED : BST_UNCHECKED);
			
			showUsersContainer = new CContainedWindow(WC_BUTTON, this, EDIT_MESSAGE_MAP);
			showUsersContainer->SubclassWindow(ctrlShowUsers.m_hWnd);
			tooltip.AddTool(ctrlShowUsers, ResourceManager::CMD_USERLIST);
			ctrlModeIcon.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SS_ICON | BS_CENTER | BS_PUSHBUTTON, 0);
			//  ctrlModeIcon.SetIcon(iconModeActive);

			dcassert(baseClient->getHubUrl() == serverUrl);
			auto fm = FavoriteManager::getInstance();
			FavoriteManager::WindowInfo wi;
			if (!fm->getFavoriteHubWindowInfo(serverUrl, wi))
			{
				wi.headerOrder = SETTING(HUB_FRAME_ORDER);
				wi.headerWidths = SETTING(HUB_FRAME_WIDTHS);
				wi.headerVisible = SETTING(HUB_FRAME_VISIBLE);
				wi.headerSort = -1;
				wi.chatUserSplit = -1;
				wi.swapPanels = false;
			}

			const FavoriteHubEntry *fhe = fm->getFavoriteHubEntryPtr(serverUrl);
			bool isAutoConnect = fhe ? fhe->getAutoConnect() : false;
			if (activateCounter == 1)
				showJoins = fhe ? fhe->getShowJoins() : BOOLSETTING(SHOW_JOINS);
			fm->releaseFavoriteHubEntryPtr(fhe);

			showFavJoins = BOOLSETTING(FAV_SHOW_JOINS);
			createFavHubMenu(fhe != nullptr, isAutoConnect);
			updateColumnsInfo(wi);
			ctrlMessage.SetFocus();
			if (activateCounter == 1)
			{
				showUsers = showUsersStore;
				if (showUsers)
					firstLoadAllUsers();
				updateSplitterPosition(wi.chatUserSplit, wi.swapPanels);
			}
			if (isDHT) insertDHTUsers();
			updateFlag = true;
		}
		BaseChatFrame::createMessagePanel();
		setCountMessages(0);
		if (!ctrlChatContainer && ctrlClient.m_hWnd)
		{
			ctrlChatContainer = new CContainedWindow(WC_EDIT, this, EDIT_MESSAGE_MAP);
			ctrlChatContainer->SubclassWindow(ctrlClient.m_hWnd);
		}
		if (updateFlag)
		{
			updateModeIcon();
			UpdateLayout(TRUE); // TODO - сконструировать статус отдельным методом
			restoreStatusFromCache(); // Восстанавливать статус нужно после UpdateLayout
		}
	}
}

void HubFrame::destroyMessagePanel(bool p_is_destroy)
{
	const bool l_is_shutdown = p_is_destroy || ClientManager::isBeforeShutdown();
	if (tooltip)
		tooltip.DestroyWindow();
		
	if (ctrlModeIcon)
		ctrlModeIcon.DestroyWindow();
	if (ctrlShowUsers)
		ctrlShowUsers.DestroyWindow();
	safe_delete(showUsersContainer);

	if (ctrlSwitchPanels)
		ctrlSwitchPanels.DestroyWindow();
	safe_delete(switchPanelsContainer);

	if (ctrlFilter)
		ctrlFilter.DestroyWindow();
	safe_delete(ctrlFilterContainer);
		
	if (ctrlFilterSel)
		ctrlFilterSel.DestroyWindow();
	safe_delete(ctrlFilterSelContainer);
		
	BaseChatFrame::destroyStatusbar();
	BaseChatFrame::destroyMessagePanel(l_is_shutdown);
	BaseChatFrame::destroyMessageCtrl(l_is_shutdown);
}

OMenu* HubFrame::createTabMenu()
{
	if (!tabMenu)
	{
		tabMenu = new OMenu;
		tabMenu->CreatePopupMenu();
	}
	return tabMenu;
}

void HubFrame::onBeforeActiveTab(HWND aWnd)
{
	dcassert(m_hWnd);
	if (!ClientManager::isStartup())
	{
		LOCK(csFrames);
		for (auto i = frames.cbegin(); i != frames.cend(); ++i)
		{
			HubFrame* frame = i->second;
			if (!frame->isClosedOrShutdown())
				frame->destroyMessagePanel(false);
		}
	}
}

void HubFrame::onAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isBeforeShutdown())
	{
		dcassert(m_hWnd);
		createMessagePanel();
	}
}
void HubFrame::onInvalidateAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (!ClientManager::isStartup())
		{
			if (ctrlStatus)
			{
				//ctrlStatus->RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
				// TODO подобрать более легкую команду. без этой пропадаю иконки в статусе.
				ctrlShowUsers.Invalidate();
				ctrlSwitchPanels.Invalidate();
				ctrlModeIcon.Invalidate();
			}
		}
	}
}

HubFrame* HubFrame::openHubWindow(const string& server,
                                  const string& name,
                                  const string rawCommands[],
                                  int  windowPosX,
                                  int  windowPosY,
                                  int  windowSizeX,
                                  int  windowSizeY,
                                  int  windowType,
                                  int  chatUserSplit,
                                  bool hideUserList,
                                  bool suppressChatAndPM)
{
	LOCK(csFrames);
	HubFrame* frm;
	const auto i = frames.find(server);
	if (i == frames.end())
	{
		frm = new HubFrame(server, name, rawCommands, chatUserSplit, hideUserList, suppressChatAndPM);
		CRect rc = frm->rcDefault;
		rc.left   = windowPosX;
		rc.top    = windowPosY;
		rc.right  = rc.left + windowSizeX;
		rc.bottom = rc.top + windowSizeY;
		if (rc.left < 0 || rc.top < 0 || rc.right - rc.left < 10 || rc.bottom - rc.top < 10)
		{
			CRect rcmdiClient;
			::GetWindowRect(WinUtil::g_mdiClient, &rcmdiClient);
			rc = rcmdiClient; // frm->rcDefault;
		}
		frm->CreateEx(WinUtil::g_mdiClient, rc);
		if (windowType)
			frm->ShowWindow(windowType);
		frames.insert(make_pair(server, frm));
	}
	else
	{
		frm = i->second;
		if (::IsIconic(frm->m_hWnd))
		{
			::ShowWindow(frm->m_hWnd, SW_RESTORE);
		}
		if (frm->m_hWndMDIClient)
		{
			frm->MDIActivate(frm->m_hWnd);
		}
	}
	return frm;
}

HubFrame* HubFrame::findHubWindow(const string& server)
{
	LOCK(csFrames);
	auto i = frames.find(server);
	return i == frames.cend() ? nullptr : i->second;
}

void HubFrame::processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText)
{
	if (waitingForPassword)
	{
		addStatus(TSTRING(DONT_REMOVE_SLASH_PASSWORD));
		addPasswordCommand();
	}
	else
	{
		sendMessage(fullMessageText);
	}
}

StringMap HubFrame::getFrameLogParams() const
{
	StringMap params;
	
	params["hubNI"] = baseClient->getHubName();
	params["hubURL"] = baseClient->getHubUrl();
	params["myNI"] = client ? client->getMyNick() : SETTING(NICK);
	
	return params;
}

void HubFrame::readFrameLog()
{
	ctrlClient.goToEnd(true);
}

void HubFrame::sendMessage(const tstring& msg, bool thirdperson)
{
	if (isDHT)
	{
		MessageBox(CTSTRING(DHT_NO_CHAT), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
		return;
	}
	dcassert(client);
	if (client)
		client->hubMessage(Text::fromT(msg), thirdperson);
}

void HubFrame::processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText)
{
	if (stricmp(cmd.c_str(), _T("join")) == 0)
	{
		if (!param.empty())
		{
			redirect = Util::formatDchubUrl(Text::fromT(param));
			if (BOOLSETTING(JOIN_OPEN_NEW_WINDOW))
				openHubWindow(redirect);
			else
				followRedirect();
		}
		else
		{
			addStatus(TSTRING(SPECIFY_SERVER));
		}
	}
	else if (stricmp(cmd.c_str(), _T("password")) == 0)
	{
		if (waitingForPassword)
		{
			if (client) client->password(Text::fromT(param), true);
			waitingForPassword = false;
		}
	}
	else if (stricmp(cmd.c_str(), _T("showjoins")) == 0)
	{
		showJoins = !showJoins;
		if (showJoins)
		{
			addStatus(TSTRING(JOIN_SHOWING_ON));
		}
		else
		{
			addStatus(TSTRING(JOIN_SHOWING_OFF));
		}
	}
	else if (stricmp(cmd.c_str(), _T("favshowjoins")) == 0)
	{
		showFavJoins = !showFavJoins;
		if (showFavJoins)
		{
			addStatus(TSTRING(FAV_JOIN_SHOWING_ON));
		}
		else
		{
			addStatus(TSTRING(FAV_JOIN_SHOWING_OFF));
		}
	}
	else if (stricmp(cmd.c_str(), _T("close")) == 0) // TODO
	{
		PostMessage(WM_CLOSE);
	}
	else if (stricmp(cmd.c_str(), _T("userlist")) == 0)
	{
		if (ctrlShowUsers)
		{
			showUsers = !showUsers;
			ctrlShowUsers.SetCheck(showUsers ? BST_CHECKED : BST_UNCHECKED);
		}
	}
	else if (stricmp(cmd.c_str(), _T("connection")) == 0 || stricmp(cmd.c_str(), _T("con")) == 0)
	{
		const string desc = ConnectivityManager::getInstance()->getPortmapInfo(true);
		string ipAddress;
		if (isDHT)
		{
			bool isFirewalled;
			dht::DHT::getInstance()->getPublicIPInfo(ipAddress, isFirewalled);
		}
		else if (client)
			ipAddress = client->getLocalIp();
		else
			ipAddress = "?";
		tstring conn = _T("\r\n") + TSTRING(IP) + _T(": ") + Text::toT(ipAddress) + _T("\r\n") + Text::toT(desc);
		
		if (param == _T("pub"))
			sendMessage(conn);
		else
			addStatus(conn);
	}
	else if (stricmp(cmd.c_str(), _T("favorite")) == 0 || stricmp(cmd.c_str(), _T("fav")) == 0)
	{
		AutoConnectType autoConnect;
		if (param == _T("a"))
			autoConnect = SET;
		else if (param == _T("-a"))
			autoConnect = UNSET;
		else
			autoConnect = DONT_CHANGE;
		addAsFavorite(autoConnect);
	}
	else if ((stricmp(cmd.c_str(), _T("removefavorite")) == 0) || (stricmp(cmd.c_str(), _T("removefav")) == 0) || (stricmp(cmd.c_str(), _T("remfav")) == 0))
	{
		removeFavoriteHub();
	}
	else if (stricmp(cmd.c_str(), _T("getlist")) == 0 || stricmp(cmd.c_str(), _T("gl")) == 0)
	{
		if (!param.empty())
		{
			UserInfo* ui = findUserByNick(param);
			if (ui)
			{
				ui->getList();
			}
		}
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		if (param.empty())
		{
			openFrameLog();
		}
		else if (stricmp(param.c_str(), _T("status")) == 0)
		{
			WinUtil::openLog(SETTING(LOG_FILE_STATUS), getFrameLogParams(), TSTRING(NO_LOG_FOR_STATUS));
		}
	}
	else if (stricmp(cmd.c_str(), _T("pm")) == 0)
	{
		if (!client) return;
		tstring::size_type j = param.find(_T(' '));
		if (j != tstring::npos)
		{
			tstring nick = param.substr(0, j);
			const OnlineUserPtr ou = client->findUser(Text::fromT(nick));
			
			if (ou)
			{
				if (param.size() > j + 1)
					PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick(), param.substr(j + 1));
				else
					PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick());
			}
		}
		else if (!param.empty())
		{
			const OnlineUserPtr ou = client->findUser(Text::fromT(param));
			if (ou)
			{
				PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick());
			}
		}
	}
	else if (stricmp(cmd.c_str(), _T("switch")) == 0)
	{
		if (showUsers)
			switchPanels();
	}
	else if (stricmp(cmd.c_str(), _T("nick")) == 0 || stricmp(cmd.c_str(), _T("n")) == 0)
	{
		tstring sayMessage;
		if (!lastUserName.empty())
			sayMessage = lastUserName + getChatRefferingToNick() + _T(' ');
			
		sayMessage += param;
		sendMessage(sayMessage);
		clearMessageWindow();
	}
	else
	{
		if (BOOLSETTING(SEND_UNKNOWN_COMMANDS))
		{
			sendMessage(fullMessageText);
		}
		else
		{
			addStatus(TSTRING(UNKNOWN_COMMAND) + _T(' ') + cmd);
		}
	}
}

void HubFrame::addAsFavorite(AutoConnectType autoConnectType)
{
	if (!client) return;
	bool autoConnect = autoConnectType != UNSET;
	auto fm = FavoriteManager::getInstance();
	FavoriteHubEntry entry;
	entry.setServer(serverUrl);
	entry.setName(client->getHubName());
	entry.setDescription(client->getHubDescription());
	entry.setAutoConnect(autoConnect);
	string user, password;
	client->getStoredLoginParams(user, password);
	if (!password.empty())
	{
		entry.setNick(user);
		entry.setPassword(password);
	}
	if (fm->addFavoriteHub(entry))
	{
		addStatus(TSTRING(FAVORITE_HUB_ADDED));
		createFavHubMenu(true, autoConnect);
	}
	else if (autoConnectType != DONT_CHANGE)
	{
		fm->setFavoriteHubAutoConnect(serverUrl, autoConnect);
		addStatus(autoConnect ? TSTRING(AUTO_CONNECT_ADDED) : TSTRING(AUTO_CONNECT_REMOVED));
		createFavHubMenu(true, autoConnect);
	}
	else
	{
		addStatus(TSTRING(FAVORITE_HUB_ALREADY_EXISTS));
	}
}

void HubFrame::removeFavoriteHub()
{
	if (!client) return;
	if (FavoriteManager::getInstance()->removeFavoriteHub(client->getHubUrl()))
	{
		addStatus(TSTRING(FAVORITE_HUB_REMOVED));
		createFavHubMenu(false, false);
	}
	else
	{
		addStatus(TSTRING(FAVORITE_HUB_DOES_NOT_EXIST));
	}
}

void HubFrame::createFavHubMenu(bool isFav, bool isAutoConnect)
{
	OMenu* tabMenu = createTabMenu();
	createTabMenu()->ClearMenu();
	if (BOOLSETTING(LOG_MAIN_CHAT))
	{
		tabMenu->AppendMenu(MF_STRING, IDC_OPEN_HUB_LOG, CTSTRING(OPEN_HUB_LOG));
		tabMenu->AppendMenu(MF_SEPARATOR);
	}
	if (isFav)
	{
		tabMenu->AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS));
		if (isAutoConnect)
			tabMenu->AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_OFF));
		else
			tabMenu->AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_ON));
		tabMenu->AppendMenu(MF_STRING, IDC_EDIT_HUB_PROP, CTSTRING(PROPERTIES));
	}
	else
	{
		tabMenu->AppendMenu(MF_STRING, IDC_ADD_AS_FAVORITE, CTSTRING(ADD_TO_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::FAVORITES, 0));
	}
	tabMenu->AppendMenu(MF_STRING, IDC_RECONNECT, CTSTRING(MENU_RECONNECT), g_iconBitmaps.getBitmap(IconBitmaps::RECONNECT, 0));
	tabMenu->AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::g_copyHubMenu, CTSTRING(COPY));
	tabMenu->AppendMenu(MF_STRING, ID_DISCONNECT, CTSTRING(DISCONNECT));
	tabMenu->AppendMenu(MF_SEPARATOR);
	tabMenu->AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED), g_iconBitmaps.getBitmap(IconBitmaps::RESTORE_CONN, 0));
	tabMenu->AppendMenu(MF_STRING, IDC_CLOSE_DISCONNECTED, CTSTRING(MENU_CLOSE_DISCONNECTED));
	tabMenu->AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
}

void HubFrame::toggleAutoConnect()
{
	auto fm = FavoriteManager::getInstance();
	const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(serverUrl);
	if (!fhe) return;
	bool autoConnect = !fhe->getAutoConnect();
	fm->releaseFavoriteHubEntryPtr(fhe);

	if (!fm->setFavoriteHubAutoConnect(serverUrl, autoConnect)) return;
	addStatus(autoConnect ? TSTRING(AUTO_CONNECT_ADDED) : TSTRING(AUTO_CONNECT_REMOVED));
	createFavHubMenu(true, autoConnect);
}

LRESULT HubFrame::onEditHubProp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!client) return 0;
	FavoriteHubEntry entry;
	auto fm = FavoriteManager::getInstance();
	if (fm->getFavoriteHub(client->getHubUrl(), entry))
	{
		FavHubProperties dlg(&entry);
		if (dlg.DoModal(*this) == IDOK)
			fm->setFavoriteHub(entry);
	}
	return 0;
}

LRESULT HubFrame::onCopyHubInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string sCopy;
	
	switch (wID)
	{
		case IDC_COPY_HUBNAME:
			sCopy = baseClient->getHubName();
			break;
		case IDC_COPY_HUBADDRESS:
			sCopy = baseClient->getHubUrl();
			break;
	}
	
	if (!sCopy.empty())
	{
		WinUtil::setClipboard(sCopy);
	}
	return 0;
}

LRESULT HubFrame::onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// !SMT!-S
	const auto su = getSelectedUser();
	if (su)
	{
		const auto& id = su->getIdentity();
		const auto u = su->getUser();
		string sCopy;
		switch (wID)
		{
			case IDC_COPY_NICK:
				sCopy += id.getNick();
				break;
			case IDC_COPY_EXACT_SHARE:
				sCopy += Identity::formatShareBytes(id.getBytesShared());
				break;
			case IDC_COPY_DESCRIPTION:
				sCopy += id.getDescription();
				break;
			case IDC_COPY_APPLICATION:
				sCopy += id.getApplication();
				break;
			case IDC_COPY_TAG:
				sCopy += id.getTag();
				break;
			case IDC_COPY_CID:
				sCopy += id.getCID();
				break;
			case IDC_COPY_EMAIL_ADDRESS:
				sCopy += id.getEmail();
				break;
			case IDC_COPY_GEO_LOCATION:
			{
				IPInfo ipInfo;
				Util::getIpInfo(id.getIp(), ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
				sCopy += Util::getDescription(ipInfo);
				break;
			}
			case IDC_COPY_IP:
				sCopy += id.getIpAsString();
				break;
			case IDC_COPY_NICK_IP:
			{
				// TODO translate
				sCopy += "User Info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
				         "\tIP: " + Identity::formatIpString(id.getIpAsString());
				break;
			}
			case IDC_COPY_ALL:
			{
				// TODO: Use Identity::getReport ?
				IPInfo ipInfo;
				Util::getIpInfo(id.getIp(), ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
				sCopy += Util::getDescription(ipInfo);
				bool isNMDC = (u->getFlags() & User::NMDC) != 0;
				sCopy += "User info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
				         "\tLocation: " + Util::getDescription(ipInfo) + "\r\n" +
				         "\tNicks: " + Util::toString(ClientManager::getNicks(u->getCID(), Util::emptyString)) + "\r\n" +
				         "\tTag: " + id.getTag() + "\r\n" +
				         "\t" + STRING(HUBS) + ": " + Util::toString(ClientManager::getHubs(u->getCID(), Util::emptyString)) + "\r\n" +
				         "\t" + STRING(SHARED) + ": " + Identity::formatShareBytes(id.getBytesShared()) + (isNMDC ? Util::emptyString : "(" + STRING(SHARED_FILES) +
				                 ": " + Util::toString(id.getSharedFiles()) + ")") + "\r\n" +
				         "\t" + STRING(DESCRIPTION) + ": " + id.getDescription() + "\r\n" +
				         "\t" + STRING(APPLICATION) + ": " + id.getApplication() + "\r\n";
				const auto con = Identity::formatSpeedLimit(id.getDownloadSpeed());
				if (!con.empty())
				{
					sCopy += "\t";
					sCopy += isNMDC ? STRING(CONNECTION) : "Download speed";
					sCopy += ": " + con + "\r\n";
				}
				const auto lim = Identity::formatSpeedLimit(u->getLimit());
				if (!lim.empty())
				{
					sCopy += "\tUpload limit: " + lim + "\r\n";
				}
				sCopy += "\tE-Mail: " + id.getEmail() + "\r\n" +
				         "\tClient Type: " + Util::toString(id.getClientType()) + "\r\n" +
				         "\tMode: " + (id.isTcpActive() ? 'A' : 'P') + "\r\n" +
				         "\t" + STRING(SLOTS) + ": " + Util::toString(id.getSlots()) + "\r\n" +
				         "\tIP: " + Identity::formatIpString(id.getIpAsString()) + "\r\n";
				const auto su = id.getSupports();
				if (!su.empty())
				{
					sCopy += "\tKnown supports: " + su;
				}
				break;
			}
			default:
				// TODO sCopy += ui->getText(wID - IDC_COPY);
				//break;
				dcdebug("HUBFRAME DON'T GO HERE\n");
				return 0;
		}
		if (!sCopy.empty())
		{
			WinUtil::setClipboard(sCopy);
		}
	}
	return 0;
}

LRESULT HubFrame::onDoubleClickUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
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
					lastUserName = Text::toT(ui->getNick());
					appendNickToChat(lastUserName);
					break;
				}
				case 2:
					ui->pm(getHubHint());
					break;
				case 3:
					ui->matchQueue();
					break;
				case 4:
					ui->grantSlotPeriod(getHubHint(), 600);
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

bool HubFrame::updateUser(const OnlineUserPtr& ou, uint32_t columnMask)
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
	if (isNewUser && showUsers && (isDHT || isConnected()))
	{
		if (!client || client->isUserListLoaded())
			shouldSort = true;
		insertUser(ui);
		return true;
	}
	if (ui == nullptr) // Hidden user or hub
	{
		return false;
	}
	else // User found, update info
	{
		if (ui->isHidden())
		{
			if (showUsers)
			{
				ctrlUsers.deleteItem(ui);
			}
			{
				WRITE_LOCK(*csUserMap);
				userMap.erase(ou);
			}
			delete ui;
			return true;
		}
		else
		{
			if (showUsers)
			{
				if (ctrlUsers.m_hWnd)
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
			}
		}
	}
	return false;
}

void HubFrame::removeUser(const OnlineUserPtr& ou)
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

void HubFrame::addStatus(const tstring& line, const bool inChat /*= true*/, const bool history /*= true*/, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextSystem*/)
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (!isSuppressChatAndPM())
		BaseChatFrame::addStatus(line, inChat, history, cf);
	if (BOOLSETTING(LOG_STATUS_MESSAGES))
	{
		StringMap params;
		if (client)
		{
			client->getHubIdentity().getParams(params, "hub", false);
			client->getMyIdentity().getParams(params, "my", true);
		}
		if (baseClient) params["hubURL"] = baseClient->getHubUrl();
		params["message"] = Text::fromT(line);
		LOG(STATUS, params);
	}
}

void HubFrame::doConnected()
{
	m_count_lock_chat = 0;
	clearUserList();
	if (!ClientManager::isBeforeShutdown())
	{
		auto fm = FavoriteManager::getInstance();
		RecentHubEntry* r = fm->getRecentHubEntry(serverUrl);
		if (r)
		{
			r->setLastSeen(Util::formatDateTime(time(nullptr)));
			fm->updateRecent(r);
		}

		addStatus(TSTRING(CONNECTED), true, true, Colors::g_ChatTextServer);
		setDisconnected(false);
		
		setHubParam();
		
		if (client)
			setStatusText(1, Text::toT(client->getCipherName()));
		if (ctrlStatus)
			UpdateLayout(FALSE);

		SHOW_POPUP(POPUP_ON_HUB_CONNECTED, Text::toT(baseClient->getHubUrl()), TSTRING(CONNECTED));
		PLAY_SOUND(SOUND_HUBCON);
		updateModeIcon();
		shouldUpdateStats = true;
	}
}

void HubFrame::clearTaskAndUserList()
{
	tasks.clear();
	clearUserList();
}

void HubFrame::doDisconnected()
{
	dcassert(!ClientManager::isBeforeShutdown());
	clearUserList();
	if (!ClientManager::isBeforeShutdown())
	{
		setDisconnected(true);
		PLAY_SOUND(SOUND_HUBDISCON);
		SHOW_POPUP(POPUP_ON_HUB_DISCONNECTED, Text::toT(baseClient->getHubUrl()), TSTRING(DISCONNECTED));
		updateModeIcon();
		shouldUpdateStats = true;
	}
}

void HubFrame::updateUserJoin(const OnlineUserPtr& ou)
{
	if (isDHT || isConnected())
	{
		if (updateUser(ou, (uint32_t) -1))
		{
			const Identity& id = ou->getIdentity();
			if (!client || client->isUserListLoaded())
			{
				dcassert(!id.getNick().empty());
				const bool isFavorite = FavoriteManager::getInstance()->isFavUserAndNotBanned(ou->getUser());
				tstring userNick = Text::toT(id.getNick());
				if (isFavorite)
				{
					PLAY_SOUND(SOUND_FAVUSER);
					SHOW_POPUP(POPUP_ON_FAVORITE_CONNECTED, userNick + _T(" - ") + Text::toT(baseClient->getHubName()), TSTRING(FAVUSER_ONLINE));
				}
				if (!id.isBotOrHub())
				{
					if (showJoins || (showFavJoins && isFavorite))
						addSystemMessage(TSTRING(JOINS) + _T(' ') + userNick, Colors::g_ChatTextSystem);
				}
				shouldUpdateStats = true;
			}
			else
			{
				shouldUpdateStats = false;
			}
			// Automatically open "op chat"
			if (client && client->isInOperatorList(id.getNick()) && !PrivateFrame::isOpen(ou->getUser()))
			{
				PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick());
			}
		}
		else
		{
			if (client && client->getBytesShared() != bytesShared)
				shouldUpdateStats = true;
		}
	}
	else
	{
		dcassert(0);
	}
}

void HubFrame::addTask(Tasks s, Task* task)
{
	bool firstItem;
	uint64_t tick = GET_TICK();
	uint64_t prevTick = tick;
	if (tasks.add(s, task, firstItem, prevTick) && prevTick + TIMER_VAL < tick)
		PostMessage(WM_SPEAKER);
}

void HubFrame::processTasks()
{
	TaskQueue::List t;
	tasks.get(t);
	if (t.empty()) return;

	bool redrawChat = false;
	if (ctrlUsers.m_hWnd)
		ctrlUsers.SetRedraw(FALSE);

	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			switch (i->first)
			{
				case UPDATE_USER:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					shouldUpdateStats |= updateUser(u.ou, (uint32_t) -1);
				}
				break;
				case UPDATE_USERS:
				{
					const OnlineUsersTask& u = static_cast<OnlineUsersTask&>(*i->second);
					for (const OnlineUserPtr& ou : u.userList)
						shouldUpdateStats |= updateUser(ou, (uint32_t) -1);
				}
				break;
				case LOAD_IP_INFO:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					READ_LOCK(*csUserMap);
					auto j = userMap.find(u.ou);
					if (j != userMap.end())
					{
						UserInfo* ui = j->second;
						ui->loadLocation();
						ui->loadP2PGuard();
						// FIXME: ui->loadIPInfo();
						++asyncUpdate;
					}
				}
				break;
				case UPDATE_USER_JOIN:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					updateUserJoin(u.ou);
				}
				break;
				case REMOVE_USER:
				{
					const OnlineUserTask& u = static_cast<const OnlineUserTask&>(*i->second);
					onUserParts(u.ou);
					removeUser(u.ou);
					shouldUpdateStats = true;
				}
				break;
				case REMOVE_USERS:
				{
					const OnlineUsersTask& u = static_cast<const OnlineUsersTask&>(*i->second);
					for (const OnlineUserPtr& ou : u.userList)
					{
						onUserParts(ou);
						removeUser(ou);
					}
					shouldUpdateStats = true;
				}
				break;
				case ADD_CHAT_LINE:
				{
					dcassert(!ClientManager::isBeforeShutdown());
					if (!ClientManager::isBeforeShutdown())
					{
						if (ctrlClient.IsWindow())
						{
							if (++m_count_lock_chat > 1)
							{
								ctrlClient.SetRedraw(FALSE);
								redrawChat = true;
							}
							else
							{
								ctrlClient.EnableScrollBar(SB_VERT, ESB_ENABLE_BOTH);
								ctrlClient.ShowScrollBar(SB_VERT, TRUE);
							}
						}
						MessageTask& task = static_cast<MessageTask&>(*i->second);
						const ChatMessage* msg = task.getMessage();
						if (msg->from && !ClientManager::isBeforeShutdown())
						{
							const Identity& from = msg->from->getIdentity();
							const bool myMessage = msg->from->getUser()->isMe();
							addLine(from, myMessage, msg->thirdPerson, Text::toT(msg->format()), 0, Colors::g_ChatTextGeneral);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
							auto& user = msg->from->getUser();
							user->incMessageCount();
							//client->incMessagesCount();
							shouldUpdateStats |= updateUser(msg->from, 1<<COLUMN_MESSAGES);
#endif
							// msg->from->getUser()->flushRatio();
						}
						else
						{
							BaseChatFrame::addLine(Text::toT(msg->text), 0, Colors::g_ChatTextPrivate);
						}
					}
				}
				break;
				case CONNECTED:
				{
					doConnected();
				}
				break;
				case DISCONNECTED:
				{
					doDisconnected();
				}
				break;
				case ADD_STATUS_LINE:
				{
					dcassert(!ClientManager::isBeforeShutdown());
					if (!ClientManager::isBeforeShutdown())
					{
						const StatusTask& status = static_cast<StatusTask&>(*i->second);
						addStatus(Text::toT(status.str), status.isInChat, true, Colors::g_ChatTextServer);
					}
				}
				break;
				case GET_PASSWORD:
				{
					//dcassert(ctrlMessage);
					if (isConnected())
					{
						if (!BOOLSETTING(PROMPT_HUB_PASSWORD))
						{
							addPasswordCommand();
							waitingForPassword = true;
						}
						else if (!showingPasswordDlg)
						{
							if (client->getSuppressChatAndPM())
							{
								client->disconnect(false);
								client->fly_fire1(ClientListener::NickError(), ClientListener::BadPassword);
							}
							else
							{
								showingPasswordDlg = true;
								LineDlg linePwd;
								linePwd.title = getHubTitle();
								linePwd.description = Text::toT(STRING_F(ENTER_PASSWORD_FOR_NICK, client->getMyNick()));
								linePwd.password = linePwd.checkBox = true;
								if (linePwd.DoModal(m_hWnd) == IDOK)
								{
									const string pwd = Text::fromT(linePwd.line);
									client->password(pwd, true);
									waitingForPassword = false;
									if (linePwd.checked)
										FavoriteManager::getInstance()->setFavoriteHubPassword(serverUrl, pwd, true);
								}
								else
								{
									client->disconnect(false);
									client->fly_fire1(ClientListener::NickError(), ClientListener::BadPassword);
								}
								showingPasswordDlg = false;
							}
						}
					}
				}
				break;
				case PRIVATE_MESSAGE:
				{
					dcassert(!ClientManager::isBeforeShutdown());
					MessageTask& task = static_cast<MessageTask&>(*i->second);
					const ChatMessage* pm = task.getMessage();
					const Identity& from = pm->from->getIdentity();
					const bool myPM = pm->replyTo->getUser()->isMe();
					const Identity& replyTo = pm->replyTo->getIdentity();
					const Identity& to = pm->to->getIdentity();
					const tstring text = Text::toT(pm->format());
					const auto& id = myPM ? to : replyTo;
					const bool isOpen = PrivateFrame::isOpen(id.getUser());
					bool isPrivateFrameOk = false;
					if ((BOOLSETTING(POPUP_PMS_HUB) && replyTo.isHub() ||
					     BOOLSETTING(POPUP_PMS_BOT) && replyTo.isBot() ||
					     BOOLSETTING(POPUP_PMS_OTHER)) || isOpen)
					{
						isPrivateFrameOk = PrivateFrame::gotMessage(from, to, replyTo, text, 1, getHubHint(), myPM, pm->thirdPerson);
					}
					if (!isPrivateFrameOk)
					{
						BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + Text::toT(id.getNick()) + _T(": ") + text, 1, Colors::g_ChatTextPrivate);
					}
					if (!replyTo.isHub() && !replyTo.isBot())
					{
						const HWND hMainWnd = MainFrame::getMainFrame()->m_hWnd;
						::PostMessage(hMainWnd, WM_SPEAKER, MainFrame::SET_PM_TRAY_ICON, NULL);
					}
				}
				break;
				case CHEATING_USER:
				{
					const StatusTask& task = static_cast<StatusTask&>(*i->second);
					CHARFORMAT2 cf;
					memset(&cf, 0, sizeof(CHARFORMAT2));
					cf.cbSize = sizeof(cf);
					cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD;
					cf.crBackColor = SETTING(BACKGROUND_COLOR);
					cf.crTextColor = SETTING(ERROR_COLOR);
					const tstring msg = Text::toT(task.str);
					if (msg.length() < 256)
						SHOW_POPUP(POPUP_ON_CHEATING_USER, msg, TSTRING(CHEATING_USER));
					BaseChatFrame::addLine(msg, 0, cf);
				}
				break;
				case USER_REPORT:
				{
					const StatusTask& task = static_cast<StatusTask&>(*i->second);
					BaseChatFrame::addLine(Text::toT(task.str), 1, Colors::g_ChatTextSystem);
					if (BOOLSETTING(LOG_MAIN_CHAT))
					{
						StringMap params;
						params["message"] = task.str;
						params["hubURL"] = baseClient->getHubUrl();
						LOG(CHAT, params);
					}
				}
				break;
		
				default:
					dcassert(0);
					break;
			};
		}
		delete i->second;
	}

	// TODO: Check if this has any effect at all
	if (redrawChat)
	{
		ctrlClient.SetRedraw(TRUE);
		ctrlClient.InvalidateRect(nullptr, TRUE);
	}
	if (ctrlUsers.m_hWnd)
		ctrlUsers.SetRedraw(TRUE);
}

void HubFrame::onUserParts(const OnlineUserPtr& ou)
{
	const UserPtr& user = ou->getUser();
	const Identity& id = ou->getIdentity();

	if (!id.isBotOrHub())
	{
		const bool isFavorite = FavoriteManager::getInstance()->isFavUserAndNotBanned(user);
		const tstring userNick = Text::toT(id.getNick());
		if (isFavorite)
		{
			PLAY_SOUND(SOUND_FAVUSER_OFFLINE);
			SHOW_POPUP(POPUP_ON_FAVORITE_DISCONNECTED, userNick + _T(" - ") + Text::toT(baseClient->getHubName()), TSTRING(FAVUSER_OFFLINE));
		}

		if (showJoins || (showFavJoins && isFavorite))
			addSystemMessage(TSTRING(PARTS) + _T(' ') + userNick, Colors::g_ChatTextSystem);
	}
}

void HubFrame::setWindowTitle(const string& text)
{
	if (!isClosedOrShutdown())
		SetWindowText(Text::toT(text).c_str());
}

void HubFrame::UpdateLayout(BOOL resizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	if (ClientManager::isStartup())
		return;
	if (tooltip)
		tooltip.Activate(FALSE);
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, resizeBars);
	if (ctrlStatus)
	{
		if (ctrlStatus.IsWindow() && ctrlLastLinesToolTip.IsWindow())
		{
			CRect sr;
			ctrlStatus.GetClientRect(sr);
			const int tmp = sr.Width() > 332 ? 232 : (sr.Width() > 132 ? sr.Width() - 100 : 32);
			const tstring& cipherName = ctrlStatusCache[1];
			int cipherLen = 0;
			if (!cipherName.empty())
				cipherLen = WinUtil::getTextWidth(cipherName, ctrlStatus) + STATUS_PART_PADDING;
			int hubPic = 0;
			const int hubIconSize = 22;
			hubPic += hubIconSize;
			if (showUsers) hubPic += 20;
			int w[6];
			w[0] = sr.right - tmp - 55 - hubPic - cipherLen;
			w[1] = w[0] + cipherLen;
			w[2] = w[1] + (tmp - 30) / 2;
			w[3] = w[1] + (tmp - 64);
			w[4] = w[3] + 100;
			w[5] = w[4] + 18 + hubPic;
			ctrlStatus.SetParts(6, w);
			
			ctrlLastLinesToolTip.SetMaxTipWidth(max(w[0], 400));
			
			// Strange, can't get the correct width of the last field...
			ctrlStatus.GetRect(4, sr);
			
			// Icon hub Mode : Active, Passive, Offline
			if (ctrlModeIcon)
			{
				sr.left = sr.right + 2;
				sr.right = sr.left + hubIconSize;
				ctrlModeIcon.MoveWindow(sr);
			}

			// Icon-button Switch panels. Analog for command /switch
			if (ctrlSwitchPanels)
			{
				if (showUsers)
				{
					sr.left = sr.right; // + 2;
					sr.right = sr.left + 20;
					ctrlSwitchPanels.MoveWindow(sr);
				}
				else
				{
					ctrlSwitchPanels.MoveWindow(0, 0, 0, 0);
				}
			}
			
			// Checkbox Show/Hide userlist
			sr.left = sr.right + 2;
			sr.right = sr.left + 16;
			if (ctrlShowUsers)
				ctrlShowUsers.MoveWindow(sr);
		}   // end  if (ctrlStatus->IsWindow()...
	}
	if (msgPanel)
	{
		int h = 0;
		const bool useMultiChat = isMultiChat(h);
		CRect rc = rect;
		rc.bottom -= h + (Fonts::g_fontHeightPixl + 1) * int(useMultiChat) + 18;
		if (ctrlStatus)
		{
			setSplitterPanes();
			if (!showUsers)
			{
				if (GetSinglePaneMode() == SPLIT_PANE_NONE)
					SetSinglePaneMode(swapPanels ? SPLIT_PANE_RIGHT : SPLIT_PANE_LEFT);
			}
			else
			{
				if (GetSinglePaneMode() != SPLIT_PANE_NONE)
					SetSinglePaneMode(SPLIT_PANE_NONE);
			}
			SetSplitterRect(rc);
		}
		
		int iButtonPanelLength = MessagePanel::GetPanelWidth();
		const int l_panelWidth = iButtonPanelLength;
		if (msgPanel)
		{
			if (!useMultiChat) // Only for Single Line chat
			{
				iButtonPanelLength += showUsers ?  222 : 0;
			}
			else
			{
				if (showUsers && iButtonPanelLength < 242)
				{
					iButtonPanelLength  = 242;
				}
			}
		}
		rc = rect;
		rc.bottom -= 4;
		rc.top = rc.bottom - h - Fonts::g_fontHeightPixl * int(useMultiChat) - 12;
		rc.left += 2;
		rc.right -= iButtonPanelLength + 2;
		
		const CRect ctrlMessageRect = rc;
		if (ctrlMessage)
			ctrlMessage.MoveWindow(rc);
		
		if (useMultiChat && multiChatLines < 2)
		{
			rc.top += h + 6;
		}
		rc.left = rc.right;
		rc.bottom -= 1;
		
		if (msgPanel)
			msgPanel->UpdatePanel(rc);

		rc.right  += l_panelWidth;
		rc.bottom += 1;
		
		if (ctrlFilter && ctrlFilterSel)
		{
			if (showUsers)
			{
				if (useMultiChat)
				{
					rc = ctrlMessageRect;
					rc.bottom = rc.top + 18;
					rc.left = rc.right + 2;
					rc.right += 139;
					ctrlFilter.MoveWindow(rc);
					
					rc.left = rc.right + 2;
					rc.right = rc.left + 101;
					rc.top -= 1;
					ctrlFilterSel.MoveWindow(rc);
				}
				else
				{
					rc.bottom -= 2;
					rc.top = rc.bottom - 18;
					rc.left = rc.right + 5;
					rc.right = rc.left + 116;
					ctrlFilter.MoveWindow(rc);
					
					rc.left = rc.right + 2;
					rc.right = rc.left + 99;
					rc.top -= 1;
					ctrlFilterSel.MoveWindow(rc);
				}
			}
			else
			{
				rc.left = 0;
				rc.right = 0;
				ctrlFilter.MoveWindow(rc);
				ctrlFilterSel.MoveWindow(rc);
			}
		}
		if (tooltip)
			tooltip.Activate(TRUE);
		if (ctrlClient.IsWindow())
		{
			//ctrlClient.EnableScrollBar(SB_VERT, ESB_ENABLE_BOTH);
			ctrlClient.ShowScrollBar(SB_VERT, TRUE);
		}
	}
}

void HubFrame::setSplitterPanes()
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (isSuppressChatAndPM())
	{
		m_nProportionalPos = 0;
		swapPanels = false;
	}
	if (ctrlUsers && ctrlClient.IsWindow())
	{
		if (swapPanels)
			SetSplitterPanes(ctrlUsers.m_hWnd, ctrlClient.m_hWnd, false);
		else
			SetSplitterPanes(ctrlClient.m_hWnd, ctrlUsers.m_hWnd, false);
	}
}

void HubFrame::updateModeIcon()
{
	if (!ctrlModeIcon) return;
	if (isDHT)
	{
		ResourceManager::Strings stateText;
		HICON hIcon;
		switch (currentDHTState)
		{
			case dht::DHT::STATE_INITIALIZING:
			case dht::DHT::STATE_BOOTSTRAP:
				stateText = ResourceManager::DHT_STATE_INITIALIZING;
				hIcon = iconModePassive;
				break;
			case dht::DHT::STATE_ACTIVE:
				stateText = ResourceManager::DHT_STATE_ACTIVE;
				hIcon = iconModeActive;
				break;
			case dht::DHT::STATE_FAILED:
				stateText = ResourceManager::DHT_STATE_FAILED;
				hIcon = iconModePassive;
				break;
			default:
				stateText = ResourceManager::DHT_STATE_DISABLED;
				hIcon = iconModeNone;
		}
		const tstring& s = TSTRING_I(stateText);
		ctrlModeIcon.SetIcon(hIcon);
		if (tooltip)
			tooltip.AddTool(ctrlModeIcon, CTSTRING_F(DHT_STATE_FMT, s));
		return;
	}
	if (client && client->isReady())
	{
		if (client->isActive())
		{
			ctrlModeIcon.SetIcon(iconModeActive);
			if (tooltip)
				tooltip.AddTool(ctrlModeIcon, ResourceManager::ACTIVE_NOTICE);
		}
		else
		{
			ctrlModeIcon.SetIcon(iconModePassive);
			if (tooltip)
				tooltip.AddTool(ctrlModeIcon, ResourceManager::PASSIVE_NOTICE);
		}
	}
	else
	{
		ctrlModeIcon.SetIcon(iconModeNone);
		if (tooltip)
			tooltip.AddTool(ctrlModeIcon, ResourceManager::UNKNOWN_MODE_NOTICE);
	}
}

void HubFrame::storeColumnsInfo()
{
	if (!updateColumnsInfoProcessed)
		return;
	auto fm = FavoriteManager::getInstance();
	FavoriteManager::WindowInfo wi;
	ctrlUsers.saveHeaderOrder(wi.headerOrder, wi.headerWidths, wi.headerVisible);
	if (fm->isFavoriteHub(serverUrl))
	{
		BOOL maximized;
		HWND hWndMDIMax = MDIGetActive(&maximized);
		WINDOWPLACEMENT wp = {0};
		wp.length = sizeof(wp);
		GetWindowPlacement(&wp);
		CRect rc;
		GetWindowRect(rc);
		CRect rcmdiClient;
		::GetWindowRect(WinUtil::g_mdiClient, &rcmdiClient);
		if (wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWNORMAL)
		{
			wi.windowPosX = rc.left - (rcmdiClient.left + 2);
			wi.windowPosY = rc.top - (rcmdiClient.top + 2);
			wi.windowSizeX = rc.Width();
			wi.windowSizeY = rc.Height();
		}
		else
		{
			wi.windowSizeX = -1;
			wi.windowSizeY = -1;
		}
		if (!(maximized && m_hWnd != hWndMDIMax) && (wp.showCmd == SW_SHOWNORMAL || wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWMAXIMIZED))
			wi.windowType = wp.showCmd;
		else
			wi.windowType = SW_SHOWMAXIMIZED;
		wi.chatUserSplit = m_nProportionalPos;
		wi.swapPanels = swapPanels;
		wi.hideUserList = !showUsersStore;
		wi.headerSort = ctrlUsers.getSortColumn();
		wi.headerSortAsc = ctrlUsers.isAscending();		
		fm->setFavoriteHubWindowInfo(serverUrl, wi);
	}
	else
	{
		SET_SETTING(HUB_FRAME_ORDER, wi.headerOrder);
		SET_SETTING(HUB_FRAME_WIDTHS, wi.headerWidths);
		SET_SETTING(HUB_FRAME_VISIBLE, wi.headerVisible);
		SET_SETTING(HUB_FRAME_SORT, ctrlUsers.getSortForSettings());
	}
}

LRESULT HubFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	timer.destroyTimer();
	tasks.setDisabled(true);

	const string server = serverUrl;
	if (!closed)
	{
		closed = true;
		if (client)
			client->removeListener(this);
		else if (baseClient)
			static_cast<dht::DHT*>(baseClient)->removeListener(this);
		removeFrame(Util::emptyString);
		storeColumnsInfo();
		if (client)
		{
			auto fm = FavoriteManager::getInstance();
			RecentHubEntry* r = fm->getRecentHubEntry(server);
			if (r)
			{
				r->setName(client->getHubName());
				r->setDescription(client->getHubDescription());
				r->setUsers(Util::toString(client->getUserCount()));
				r->setShared(Util::toString(client->getBytesShared()));
				r->setLastSeen(Util::formatDateTime(time(nullptr)));
				if (!ClientManager::isBeforeShutdown())
					r->setOpenTab("-");
				fm->updateRecent(r);
			}
			if (!ClientManager::isBeforeShutdown() && !originalServerUrl.empty())
			{
				r = fm->getRecentHubEntry(originalServerUrl);
				if (r)
				{
					r->setOpenTab("-");
					fm->updateRecent(r);
				}
			}
		}
		if (updateColumnsInfoProcessed)
		{
			SettingsManager::getInstance()->removeListener(this);
			UserManager::getInstance()->removeListener(this);
			FavoriteManager::getInstance()->removeListener(this);
		}
		if (client)
		{
			client->setAutoReconnect(false);
			client->disconnect(true);
		}
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		clearTaskAndUserList();
		bHandled = FALSE;
		return 0;
	}
}

void HubFrame::clearUserList()
{
	//CFlyBusyBool l_busy(m_is_delete_all_items);
	if (ctrlUsers)
	{
		CLockRedraw<> lockRedraw(ctrlUsers); // TODO это нужно или опустить ниже?
		ctrlUsers.DeleteAllItems();
	}
	{
		WRITE_LOCK(*csUserMap);
		for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
			delete i->second;
		userMap.clear();
	}
}

LRESULT HubFrame::onLButton(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND focus = GetFocus();
	bHandled = false;
	if (focus == ctrlClient.m_hWnd)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		tstring x;
		
		int i = ctrlClient.CharFromPos(pt);
		int line = ctrlClient.LineFromChar(i);
		int c = LOWORD(i) - ctrlClient.LineIndex(line);
		int len = ctrlClient.LineLength(i) + 1;
		if (len < 3)
		{
			return 0;
		}
		
		x.resize(len);
		ctrlClient.GetLine(line, &x[0], len);
		
		string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
		
		if (start == string::npos)
			start = 0;
		else
			start++;
			
			
		string::size_type end = x.find_first_of(_T(" >\t"), start + 1);
		
		if (end == string::npos) // get EOL as well
			end = x.length();
		else if (end == start + 1)
			return 0;
			
		// Nickname click, let's see if we can find one like it in the name list...
		const tstring nick = x.substr(start, end - start);
		UserInfo* ui = findUserByNick(nick);
		if (ui)
		{
			bHandled = true;
			if (client && (wParam & MK_CONTROL))
			{
				PrivateFrame::openWindow(ui->getOnlineUser(), HintedUser(ui->getUser(), client->getHubUrl()), client->getMyNick());
			}
			else if (wParam & MK_SHIFT)
			{
				try
				{
					QueueManager::getInstance()->addList(HintedUser(ui->getUser(), getHubHint()), QueueItem::FLAG_CLIENT_VIEW);
				}
				catch (const Exception& e)
				{
					addStatus(Text::toT(e.getError()));
				}
			}
			else
			{
				switch (SETTING(CHAT_DBLCLICK))
				{
					case 0:
					{
						const int l_items_count = ctrlUsers.GetItemCount();
						int pos = -1;
						CLockRedraw<> lockRedraw(ctrlUsers);
						for (int i = 0; i < l_items_count; ++i)
						{
							if (ctrlUsers.getItemData(i) == ui)
								pos = i;
							ctrlUsers.SetItemState(i, (i == pos) ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
						}
						ctrlUsers.EnsureVisible(pos, FALSE);
						break;
					}
					case 1:
					{
						lastUserName = ui->getText(COLUMN_NICK);
						appendNickToChat(lastUserName);
						break;
					}
					case 2:
						ui->pm(getHubHint());
						break;
					case 3:
						ui->getList();
						break;
					case 4:
						ui->matchQueue();
						break;
					case 5:
						ui->grantSlotPeriod(getHubHint(), 600);
						break;
					case 6:
						ui->addFav();
						break;
				}
			}
		}
	}
	return 0;
}

void HubFrame::addLine(const Identity& from, const bool myMessage, const bool thirdPerson, const tstring& line, unsigned maxSmiles, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	tstring extra;
	BaseChatFrame::addLine(from, myMessage, thirdPerson, line, maxSmiles, cf, extra);
	if (!ClientManager::isStartup())
	{
		SHOW_POPUP(POPUP_ON_CHAT_LINE, line, TSTRING(CHAT_MESSAGE));
	}
	if (BOOLSETTING(LOG_MAIN_CHAT))
	{
		StringMap params;
		params["message"] = ChatMessage::formatNick(from.getNick(), thirdPerson) + Text::fromT(line);
		if (!extra.empty())
			params["extra"] = Text::fromT(extra);
		if (client)
		{
			client->getHubIdentity().getParams(params, "hub", false);
			client->getMyIdentity().getParams(params, "my", true);
		}
		if (baseClient) params["hubURL"] = baseClient->getHubUrl();
		LOG(CHAT, params);
	}
	if (!ClientManager::isStartup() && BOOLSETTING(BOLD_HUB))
	{
		if (!client || client->isUserListLoaded())
			setDirty();
	}
}

LRESULT HubFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	CMenu hSysMenu;
	isTabMenuShown = true;
	OMenu* tabMenu = createTabMenu();
	if (client)
	{
		string name = client->getHubName();
		if (name.empty())
			name = client->getHubUrl();
		else
		if (name.length() > 50)
		{
			name.erase(50);
			name += "...";
		}
		tabMenu->InsertSeparatorFirst(Text::toT(name));
	}
	else
		tabMenu->InsertSeparatorFirst(TSTRING(DHT_TITLE));
	if (client)
		appendUcMenu(*tabMenu, UserCommand::CONTEXT_HUB, client->getHubUrl());
	hSysMenu.Attach(*tabMenu);
	hSysMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	cleanUcMenu(*tabMenu);
	tabMenu->RemoveFirstItem();
	hSysMenu.Detach();
	return TRUE;
}

LRESULT HubFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	CRect rc;
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	isTabMenuShown = false;
	if (!ctrlUsers.m_hWnd)
		return FALSE;
	ctrlUsers.GetHeader().GetWindowRect(&rc);
	
	if (showUsers && PtInRect(&rc, pt))
	{
		ctrlUsers.showMenu(pt);
		return TRUE;
	}
	
	if (reinterpret_cast<HWND>(wParam) == ctrlUsers.m_hWnd && showUsers)
	{
		OMenu* userMenu = createUserMenu();
		userMenu->ClearMenu();
		clearUserMenu();
		
		int selCount = ctrlUsers.GetSelectedCount();
		int ctx = UserCommand::CONTEXT_USER;
		if (selCount == 1)
		{
			if (pt.x == -1 && pt.y == -1)
				WinUtil::getContextMenuPos(ctrlUsers, pt);
			int i = -1;
			i = ctrlUsers.GetNextItem(i, LVNI_SELECTED);
			if (i >= 0)
			{
				const OnlineUserPtr& ou = ctrlUsers.getItemData(i)->getOnlineUser();
				if (ou->getUser()->isMe())
					ctx |= UserCommand::CONTEXT_FLAG_ME;
				reinitUserMenu(ou, getHubHint());
			}
		}
		
		appendHubAndUsersItems(*userMenu, false);
		appendUcMenu(*userMenu, ctx, baseClient->getHubUrl());
		WinUtil::appendSeparator(*userMenu);
		userMenu->AppendMenu(MF_STRING, IDC_REFRESH, CTSTRING(REFRESH_USER_LIST));
		
		if (selCount > 0)
			userMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);

		WinUtil::unlinkStaticMenus(*userMenu);
		cleanUcMenu(*userMenu);
		userMenu->ClearMenu();
		return TRUE;
	}
	
	if (reinterpret_cast<HWND>(wParam) == ctrlClient)
	{
		OMenu* userMenu = createUserMenu();
		userMenu->ClearMenu();
		clearUserMenu();
		
		if (pt.x == -1 && pt.y == -1)
		{
			CRect erc;
			ctrlClient.GetRect(&erc);
			pt.x = erc.Width() / 2;
			pt.y = erc.Height() / 2;
			ctrlClient.ClientToScreen(&pt);
		}
		POINT ptCl = pt;
		ctrlClient.ScreenToClient(&ptCl);
		ctrlClient.onRButtonDown(ptCl);
		
		UserInfo *ui = nullptr;
		if (!ChatCtrl::g_sSelectedUserName.empty())
		{
			ui = findUserByNick(ChatCtrl::g_sSelectedUserName);
		}
		reinitUserMenu(ui ? ui->getOnlineUser() : nullptr, getHubHint());
		
		appendHubAndUsersItems(*userMenu, true);
		
		if (!getSelectedUser())
		{
			userMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		else
		{
			if (client)
				appendUcMenu(*userMenu, UserCommand::CONTEXT_USER, client->getHubUrl());
			userMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		WinUtil::unlinkStaticMenus(*userMenu);
		cleanUcMenu(*userMenu);
		userMenu->ClearMenu();
		return TRUE;
	}
	
	if (msgPanel && msgPanel->OnContextMenu(pt, wParam))
		return TRUE;
	
	return FALSE;
}

LRESULT HubFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = g_iconBitmaps.getIcon(IconBitmaps::HUB_ONLINE, 0);
	opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::HUB_OFFLINE, 0);
	opt->isHub = true;
	return TRUE;
}

void HubFrame::runUserCommand(UserCommand& uc)
{
	if (!client)
		return;

	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
		
	StringMap ucParams = ucLineParams;
	
	client->getMyIdentity().getParams(ucParams, "my", true);
	client->getHubIdentity().getParams(ucParams, "hub", false);
	
	if (isTabMenuShown)
	{
		client->escapeParams(ucParams);
		client->sendUserCmd(uc, ucParams);
	}
	else
	{
		if (getSelectedUser())
		{
			const UserInfo* u = findUser(getSelectedUser());
			if (u && u->getUser()->isOnline())
			{
				StringMap tmp = ucParams;
				u->getIdentity().getParams(tmp, "user", true);
				client->escapeParams(tmp);
				client->sendUserCmd(uc, tmp);
			}
		}
		else
		{
			int sel = -1;
			while ((sel = ctrlUsers.GetNextItem(sel, LVNI_SELECTED)) != -1)
			{
				const UserInfo *u = ctrlUsers.getItemData(sel);
				if (u->getUser()->isOnline())
				{
					StringMap tmp = ucParams;
					u->getIdentity().getParams(tmp, "user", true);
					client->escapeParams(tmp);
					client->sendUserCmd(uc, tmp);
				}
			}
		}
	}
}

void HubFrame::onTab()
{
	if (ctrlMessage && ctrlMessage.GetWindowTextLength() == 0)
	{
		handleTab(WinUtil::isShift());
		return;
	}
	const HWND focus = GetFocus();
	if (ctrlMessage && focus == ctrlMessage.m_hWnd && !WinUtil::isShift())
	{
		tstring text;
		WinUtil::getWindowText(ctrlMessage, text);
		
		string::size_type textStart = text.find_last_of(_T(" \n\t"));
		
		if (m_complete.empty())
		{
			if (textStart != string::npos)
			{
				m_complete = text.substr(textStart + 1);
			}
			else
			{
				m_complete = text;
			}
			if (m_complete.empty())
			{
				// Still empty, no text entered...
				ctrlUsers.SetFocus();
				return;
			}
			const int y = ctrlUsers.GetItemCount();
			
			for (int x = 0; x < y; ++x)
				ctrlUsers.SetItemState(x, 0, LVNI_FOCUSED | LVNI_SELECTED);
		}
		
		if (textStart == string::npos)
			textStart = 0;
		else
			textStart++;
			
		int start = ctrlUsers.GetNextItem(-1, LVNI_FOCUSED) + 1;
		int i = start;
		const int j = ctrlUsers.GetItemCount();
		
		bool firstPass = i < j;
		if (!firstPass)
			i = 0;
		while (firstPass || i < start)
		{
			const UserInfo* ui = ctrlUsers.getItemData(i);
			const tstring nick = ui->getText(COLUMN_NICK);
			bool found = strnicmp(nick, m_complete, m_complete.length()) == 0;
			tstring::size_type x = 0;
			if (!found)
			{
				// Check if there's one or more [ISP] tags to ignore...
				tstring::size_type y = 0;
				while (nick[y] == _T('['))
				{
					x = nick.find(_T(']'), y);
					if (x != string::npos)
					{
						if (strnicmp(nick.c_str() + x + 1, m_complete.c_str(), m_complete.length()) == 0)
						{
							found = true;
							break;
						}
					}
					else
					{
						break;
					}
					y = x + 1; // assuming that nick[y] == '\0' is legal
				}
			}
			if (found)
			{
				if ((start - 1) != -1)
					ctrlUsers.SetItemState(start - 1, 0, LVNI_SELECTED | LVNI_FOCUSED);
				ctrlUsers.SetItemState(i, LVNI_FOCUSED | LVNI_SELECTED, LVNI_FOCUSED | LVNI_SELECTED);
				ctrlUsers.EnsureVisible(i, FALSE);
				if (ctrlMessage)
				{
					ctrlMessage.SetSel(static_cast<int>(textStart), ctrlMessage.GetWindowTextLength(), TRUE);
					ctrlMessage.ReplaceSel(nick.c_str());
				}
				return;
			}
			i++;
			if (i == j)
			{
				firstPass = false;
				i = 0;
			}
		}
	}
}

LRESULT HubFrame::onCloseWindows(WORD, WORD wID, HWND, BOOL&)
{
	switch (wID)
	{
		case IDC_CLOSE_DISCONNECTED:
			closeDisconnected();
			break;
		case IDC_RECONNECT_DISCONNECTED:
			reconnectDisconnected();
			break;
		case IDC_CLOSE_WINDOW:
			PostMessage(WM_CLOSE);
			break;
	}
	return 0;
}

LRESULT HubFrame::onReconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!client) return 0;
	auto fm = FavoriteManager::getInstance();
	const FavoriteHubEntry *fhe = fm->getFavoriteHubEntryPtr(serverUrl);
	bool hasFavNick = fhe && !fhe->getNick().empty();
	showJoins = fhe ? fhe->getShowJoins() : BOOLSETTING(SHOW_JOINS);
	fm->releaseFavoriteHubEntryPtr(fhe);
	showFavJoins = BOOLSETTING(FAV_SHOW_JOINS);

	if (!hasFavNick && SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);// TODO Добавить адрес хаба в сообщение
		return 0;
	}
	client->reconnect();
	return 0;
}

LRESULT HubFrame::onDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (client)
		client->disconnect(true);
	return 0;
}

LRESULT HubFrame::onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!m_complete.empty() && wParam != VK_TAB && uMsg == WM_KEYDOWN)
	{
		m_complete.clear();
	}
	if (!processControlKey(uMsg, wParam, lParam, bHandled))
	{
		switch (wParam)
		{
			case VK_TAB:
				onTab();
				break;
			default:
				processHotKey(uMsg, wParam, lParam, bHandled);
		}
	}
	return 0;
}

size_t HubFrame::insertUsers()
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

void HubFrame::firstLoadAllUsers()
{
	CWaitCursor waitCursor;
	CLockRedraw<> lockRedraw(ctrlUsers);
	insertUsers();
	shouldSort = true;
}

LRESULT HubFrame::onHubFrmCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	return BaseChatFrame::onCtlColor(uMsg, wParam, lParam, bHandled);
}

LRESULT HubFrame::onShowUsers(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	if (activateCounter >= 1)
	{
		if (wParam == BST_CHECKED)
		{
			setShowUsers(true);
			firstLoadAllUsers();
		}
		else
		{
			setShowUsers(false);
			shouldSort = false;
			CLockRedraw<> lockRedraw(ctrlUsers);
			ctrlUsers.DeleteAllItems();
		}
		UpdateLayout(FALSE);
		shouldUpdateStats = true;
	}
	return 0;
}

void HubFrame::switchPanels()
{
	swapPanels = !swapPanels;
	m_nProportionalPos = 10000 - m_nProportionalPos;
	UpdateLayout();
}

void HubFrame::removeFrame(const string& redirectUrl)
{
	LOCK(csFrames);
	frames.erase(serverUrl);
	if (!redirectUrl.empty())
	{
		frames.insert(make_pair(redirectUrl, this));
		serverUrl = redirectUrl;
	}
}

void HubFrame::followRedirect()
{
	if (client && !redirect.empty())
	{
		if (ClientManager::isConnected(redirect))
		{
			addStatus(TSTRING(REDIRECT_ALREADY_CONNECTED), true, false, Colors::g_ChatTextServer);
			LogManager::message("HubFrame::onFollow " + getHubHint() + " -> " + redirect + " ALREADY CONNECTED", false);
			return;
		}
		if (originalServerUrl.empty())
			originalServerUrl = serverUrl;
		removeFrame(redirect);
		// the client is dead, long live the client!
		client->setAutoReconnect(false);
		ClientManager::getInstance()->putClient(client);
		baseClient = client = nullptr;
		clearTaskAndUserList();
		client = ClientManager::getInstance()->getClient(redirect);
		baseClient = client;
		RecentHubEntry r;
		r.setRedirect(true);
		r.setServer(redirect);
		FavoriteManager::getInstance()->addRecent(r);
		client->addListener(this);
		client->connect();
	}
}

LRESULT HubFrame::onFollow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	followRedirect();
	return 0;
}

LRESULT HubFrame::onEnterUsers(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	int item = ctrlUsers.GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
	{
		try
		{
			QueueManager::getInstance()->addList(HintedUser((ctrlUsers.getItemData(item))->getUser(), getHubHint()), QueueItem::FLAG_CLIENT_VIEW);
		}
		catch (const Exception& e)
		{
			addStatus(Text::toT(e.getError()));
		}
	}
	return 0;
}

void HubFrame::resortUsers()
{
	LOCK(csFrames);
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		if (!i->second->isClosedOrShutdown())
		{
			i->second->resortForFavsFirst(true);
		}
	}
}

void HubFrame::closeDisconnected()
{
	LOCK(csFrames);
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		if (!i->second->isClosedOrShutdown())
		{
			Client* client = i->second->client;
			if (client && !client->isConnected())
				i->second->PostMessage(WM_CLOSE);
		}
	}
}

void HubFrame::reconnectDisconnected()
{
	LOCK(csFrames);
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		if (!i->second->isClosedOrShutdown())
		{
			Client* client = i->second->client;
			if (client && !client->isConnected())
				client->reconnect();
		}
	}
}

void HubFrame::closeAll(size_t threshold)
{
	if (threshold == 0)
	{
		// Ускорим закрытие всех хабов
		ClientManager::getInstance()->prepareClose();
		// ClientManager::getInstance()->prepareClose(); // Отпишемся от подписок клиента
		// SearchManager::getInstance()->prepareClose(); // Отпишемся от подписок поиска
	}
	{
		LOCK(csFrames);
		for (auto i = frames.cbegin(); i != frames.cend(); ++i)
		{
			if (!i->second->isClosedOrShutdown())
			{
				Client* client = i->second->client;
				if (threshold == 0 || !client || client->getUserCount() <= threshold)
					i->second->PostMessage(WM_CLOSE);
			}
		}
	}
}

void HubFrame::updateAllTitles()
{
	LOCK(csFrames);
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
		i->second->hubUpdateCount++;
}

void HubFrame::on(FavoriteManagerListener::UserAdded, const FavoriteUser& user) noexcept
{
	if (isClosedOrShutdown())
		return;
	++asyncUpdate;
	user.user->setFlag(User::ATTRIBS_CHANGED);
	resortForFavsFirst();
}

void HubFrame::on(FavoriteManagerListener::UserRemoved, const FavoriteUser& user) noexcept
{
	if (isClosedOrShutdown())
		return;
	++asyncUpdate;
	user.user->setFlag(User::ATTRIBS_CHANGED);
	resortForFavsFirst();
}

void HubFrame::on(FavoriteManagerListener::UserStatusChanged, const UserPtr& user) noexcept
{
	++asyncUpdate;
	user->setFlag(User::ATTRIBS_CHANGED);
}

void HubFrame::resortForFavsFirst(bool justDoIt /* = false */)
{
	if (justDoIt || BOOLSETTING(SORT_FAVUSERS_FIRST))
	{
		shouldSort = true;
	}
}

void HubFrame::on(UserManagerListener::IgnoreListChanged, const string& userName) noexcept
{
	WRITE_LOCK(*csUserMap);
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
	{
		UserInfo* ui = i->second;
		if (ui->getUser()->getLastNick() == userName)
			ui->flags |= IS_IGNORED_USER; // flag IS_IGNORED_USER_ON will be updated
	}
}

void HubFrame::on(UserManagerListener::IgnoreListCleared) noexcept
{
	WRITE_LOCK(*csUserMap);
	for (auto i = userMap.cbegin(); i != userMap.cend(); ++i)
	{
		UserInfo* ui = i->second;
		ui->flags &= ~IS_IGNORED_USER; // flag IS_IGNORED_USER_ON is cleared and won't be updated
	}
}

void HubFrame::on(UserManagerListener::ReservedSlotChanged, const UserPtr& user) noexcept
{
	user->setFlag(User::ATTRIBS_CHANGED);
	++asyncUpdate;
}

void HubFrame::on(Connecting, const Client*) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	addStatus(Text::toT(STRING(CONNECTING_TO) + ' ' + baseClient->getHubUrl() + " ..."));
	++hubUpdateCount;
}

void HubFrame::on(ClientListener::Connected, const Client* c) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	addTask(CONNECTED, nullptr);
}

void HubFrame::on(ClientListener::DDoSSearchDetect, const string&) noexcept
{
#if 0 // FIXME
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (!m_is_ddos_detect)
	{
		setCustomIcon(*WinUtil::g_HubDDoSIcon.get());
		m_is_ddos_detect = true;
	}
#endif
}

void HubFrame::on(ClientListener::UserUpdated, const OnlineUserPtr& user) noexcept
{
	if (isClosedOrShutdown())
		return;
	addTask(UPDATE_USER_JOIN, new OnlineUserTask(user));
}

void HubFrame::on(ClientListener::StatusMessage, const Client*, const string& line, int statusFlags) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	addTask(ADD_STATUS_LINE, new StatusTask(Text::toDOS(line), !BOOLSETTING(FILTER_MESSAGES) || !(statusFlags & ClientListener::FLAG_IS_SPAM)));
}

void HubFrame::on(ClientListener::UserListUpdated, const ClientBase*, const OnlineUserList& userList) noexcept
{
	if (isClosedOrShutdown())
		return;
	addTask(UPDATE_USERS, new OnlineUsersTask(userList));
}

void HubFrame::on(ClientListener::UserRemoved, const ClientBase*, const OnlineUserPtr& user) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	addTask(REMOVE_USER, new OnlineUserTask(user));
}

void HubFrame::on(ClientListener::UserListRemoved, const ClientBase*, const OnlineUserList& userList) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	addTask(REMOVE_USERS, new OnlineUsersTask(userList));
}

void HubFrame::on(Redirect, const Client*, const string& line) noexcept
{
	string redirAddr = Util::formatDchubUrl(line);
	bool doubleRedir = false;
	if (ClientManager::isConnected(redirAddr))
	{
		addTask(ADD_STATUS_LINE, new StatusTask(STRING(REDIRECT_ALREADY_CONNECTED), true));
		doubleRedir = true;
	}
	
	redirect = redirAddr;
	if (BOOLSETTING(AUTO_FOLLOW) || doubleRedir)
		PostMessage(WM_COMMAND, IDC_FOLLOW, 0);
	else
		addTask(ADD_STATUS_LINE, new StatusTask(STRING_F(PRESS_FOLLOW_FMT, redirAddr), true));
}

void HubFrame::on(ClientListener::ClientFailed, const Client* c, const string& line) noexcept
{
	if (!isClosedOrShutdown())
	{
		addTask(ADD_STATUS_LINE, new StatusTask(line, true));
	}
	addTask(DISCONNECTED, nullptr);
}

void HubFrame::on(ClientListener::GetPassword, const Client*) noexcept
{
	addTask(GET_PASSWORD, nullptr);
}

void HubFrame::onTimerHubUpdated()
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (hubParamUpdated)
	{
		if (isDHT)
			ctrlClient.setHubParam(dht::NetworkName, SETTING(NICK));
		else if (client)
			ctrlClient.setHubParam(client->getHubUrl(), client->getMyNick());
		hubParamUpdated = false;
	}
	if (client && hubUpdateCount)
	{
		hubUpdateCount = 0;
		updateWindowTitle();
	}
}

tstring HubFrame::getHubTitle() const
{
	string hubName = baseClient->getHubName();
	const string& hubUrl = baseClient->getHubUrl();
	if (hubUrl != hubName)
	{
		hubName += " (";
		hubName += hubUrl;
		hubName += ')';
	}
	return Text::toT(hubName);
}

void HubFrame::updateWindowTitle()
{
	if (!client) return;
	string fullHubName = client->getHubName();
	string tooltip = fullHubName;
	const string& url = client->getHubUrl();
	if (BOOLSETTING(HUB_URL_IN_TITLE) && !url.empty() && url != fullHubName)
		fullHubName += " (" + url + ')';

	string description = client->getHubDescription();
	if (!description.empty())
	{
		tooltip += "\r\n";
		tooltip += description;
	}

	if (!url.empty())
	{
		tooltip += "\r\n";
		tooltip += url;
	}

	string ip = client->getIpAsString();
	if (!ip.empty())
	{
		tooltip += "\r\n";
		tooltip += ip;
	}

	if (fullHubName != prevHubName)
	{
		setWindowTitle(fullHubName);
		if (BOOLSETTING(BOLD_HUB) && !prevHubName.empty())
			setDirty();
		prevHubName = std::move(fullHubName);
	}

	if (tooltip != prevTooltip)
	{
		prevTooltip = std::move(tooltip);
		setTooltipText(Text::toT(prevTooltip));
	}
}

void HubFrame::on(ClientListener::HubUpdated, const Client*) noexcept
{
	hubUpdateCount++;
}

void HubFrame::on(ClientListener::Message, const Client*,  std::unique_ptr<ChatMessage>& message) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	ChatMessage* messagePtr = message.release();
#ifdef _DEBUG
	if (messagePtr->text.find("&#124") != string::npos)
	{
		dcassert(0);
	}
#endif
	if (messagePtr->isPrivate())
		addTask(PRIVATE_MESSAGE, new MessageTask(messagePtr));
	else
		addTask(ADD_CHAT_LINE, new MessageTask(messagePtr));
}

void HubFrame::on(ClientListener::HubFull, const Client*) noexcept
{
	if (isClosedOrShutdown())
		return;
	addTask(ADD_STATUS_LINE, new StatusTask(STRING(HUB_FULL), true));
}

void HubFrame::on(ClientListener::NickError, ClientListener::NickErrorCode nickError) noexcept
{
	if (!client || isClosedOrShutdown())
		return;
	StatusTask* status = nullptr;
	string nick = client->getMyNick();
	if (BOOLSETTING(AUTO_CHANGE_NICK) && nickError != ClientListener::BadPassword)
	{
		auto fm = FavoriteManager::getInstance();
		auto fhe = fm->getFavoriteHubEntryPtr(client->getHubUrl());
		bool noPassword = !fhe || fhe->getPassword().empty();
		fm->releaseFavoriteHubEntryPtr(fhe);

		if (noPassword)
		{
			const string& tempNick = client->getRandomTempNick();
			if (!tempNick.empty())
			{
				nick = tempNick;
				Client::removeRandomSuffix(nick);
			}
			bool suffixAppended;
			if (client->convertNick(nick, suffixAppended))
			{
				string oldNick = client->getMyNick();
				if (!suffixAppended) Client::appendRandomSuffix(nick);
				client->setMyNick(nick, true);
				setHubParam();				
				client->setAutoReconnect(true);
				client->setReconnDelay(30);
				if (nickError == ClientListener::Rejected)
					status = new StatusTask(STRING_F(NICK_ERROR_REJECTED_AUTO, oldNick % nick), true);
				else
					status = new StatusTask(STRING_F(NICK_ERROR_TAKEN_AUTO, oldNick % nick), true);
			}
		}
	}
	if (!status)
		switch (nickError)
		{
			case ClientListener::Rejected:
				status = new StatusTask(STRING_F(NICK_ERROR_REJECTED, client->getMyNick()), true);
				break;
			case ClientListener::Taken:
				status = new StatusTask(STRING_F(NICK_ERROR_TAKEN, client->getMyNick()), true);
				break;
			default:
				status = new StatusTask(STRING_F(NICK_ERROR_BAD_PASSWORD, client->getMyNick()), true);
		}
	addTask(ADD_STATUS_LINE, status);
}

void HubFrame::on(ClientListener::CheatMessage, const string& line) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	addTask(CHEATING_USER, new StatusTask(line, true));
}

void HubFrame::on(ClientListener::UserReport, const ClientBase*, const string& report) noexcept
{
	if (isClosedOrShutdown())
		return;
	addTask(USER_REPORT, new StatusTask(report, true));
}

void HubFrame::on(ClientListener::HubInfoMessage, ClientListener::HubInfoCode code, const Client* client, const string& line) noexcept
{
	switch (code)
	{
		case ClientListener::LoggedIn:
			addTask(ADD_STATUS_LINE, new StatusTask(STRING_F(YOU_ARE_OP_MSG, baseClient->getHubUrl()), true));
			break;

		case ClientListener::HubTopic:
			addTask(ADD_STATUS_LINE, new StatusTask(STRING(HUB_TOPIC) + " " + line, true));
			break;
	}
}

LRESULT HubFrame::onFilterChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (uMsg == WM_CHAR && wParam == VK_TAB)
	{
		handleTab(WinUtil::isShift());
		return 0;
	}
	
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

LRESULT HubFrame::onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
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

bool HubFrame::parseFilter(FilterModes& mode, int64_t& size)
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

void HubFrame::insertUserInternal(UserInfo* ui, int pos)
{
	int result = -1;
	if (pos != -1)
		result = ctrlUsers.insertItem(pos, ui, I_IMAGECALLBACK);
	else
		result = ctrlUsers.insertItem(ui, I_IMAGECALLBACK);
	ui->getIdentityRW().getChanges();
	dcassert(result != -1);
}

void HubFrame::insertUser(UserInfo* ui)
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

void HubFrame::updateUserList()
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

void HubFrame::handleTab(bool reverse)
{
	HWND focus = GetFocus();
	
	if (reverse)
	{
		if (ctrlFilterSel && focus == ctrlFilterSel.m_hWnd)
			ctrlFilter.SetFocus();
		else if (ctrlMessage && ctrlFilter && focus == ctrlFilter.m_hWnd)
			ctrlMessage.SetFocus();
		else if (ctrlMessage && focus == ctrlMessage.m_hWnd)
			ctrlUsers.SetFocus();
		else if (ctrlUsers && focus == ctrlUsers.m_hWnd)
			ctrlClient.SetFocus();
		else if (ctrlFilterSel && focus == ctrlClient.m_hWnd)
			ctrlFilterSel.SetFocus();
	}
	else
	{
		if (focus == ctrlClient.m_hWnd)
		{
			ctrlUsers.SetFocus();
		}
		else if (ctrlUsers && ctrlMessage && focus == ctrlUsers.m_hWnd)
		{
			ctrlMessage.SetFocus();
		}
		else if (ctrlMessage && focus == ctrlMessage.m_hWnd)
		{
			if (ctrlFilter)
				ctrlFilter.SetFocus();
		}
		else if (ctrlFilter && ctrlFilterSel && focus == ctrlFilter.m_hWnd)
		{
			ctrlFilterSel.SetFocus();
		}
		else if (ctrlFilterSel && focus == ctrlFilterSel.m_hWnd)
		{
			ctrlClient.SetFocus();
		}
	}
}

bool HubFrame::matchFilter(UserInfo& ui, int sel, bool doSizeCompare, FilterModes mode, int64_t size)
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

void HubFrame::appendHubAndUsersItems(OMenu& menu, const bool isChat)
{
	if (getSelectedUser())
	{
		const bool isMe = getSelectedUser()->getUser()->isMe();
		menu.InsertSeparatorFirst(Text::toT(getSelectedUser()->getIdentity().getNick()));
		// some commands starts in UserInfoBaseHandler, that requires user visible
		// in ListView. for now, just disable menu item to workaronud problem
		if (!isMe)
		{
			appendAndActivateUserItems(menu, false);	
			if (isChat)
				menu.AppendMenu(MF_STRING, IDC_SELECT_USER, CTSTRING(SELECT_USER_LIST));
		}
		else
			appendCopyMenuForSingleUser(menu);
	}
	else
	{
		if (!isChat)
		{
			const int count = ctrlUsers.GetSelectedCount();
			menu.InsertSeparatorFirst(Util::toStringT(count) + _T(' ') + TSTRING(HUB_USERS));
			if (count < 50) // https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1712
			{
				appendAndActivateUserItems(menu, false);
			}
		}
	}
	
	if (isChat)
	{
		appendChatCtrlItems(menu, client);
	}
	
	if (isChat)
	{
		switch (SETTING(CHAT_DBLCLICK))
		{
			case 0:
				menu.SetMenuDefaultItem(IDC_SELECT_USER);
				break;
			case 1:
				menu.SetMenuDefaultItem(IDC_ADD_NICK_TO_CHAT);
				break;
			case 2:
				menu.SetMenuDefaultItem(IDC_PRIVATE_MESSAGE);
				break;
			case 3:
				menu.SetMenuDefaultItem(IDC_GETLIST);
				break;
			case 4:
				menu.SetMenuDefaultItem(IDC_MATCH_QUEUE);
				break;
			case 6:
				menu.SetMenuDefaultItem(IDC_ADD_TO_FAVORITES);
				break;
			case 7:
				menu.SetMenuDefaultItem(IDC_BROWSELIST);
		}
	}
	else
	{
		switch (SETTING(USERLIST_DBLCLICK))
		{
			case 0:
				menu.SetMenuDefaultItem(IDC_GETLIST);
				break;
			case 1:
				menu.SetMenuDefaultItem(IDC_ADD_NICK_TO_CHAT);
				break;
			case 2:
				menu.SetMenuDefaultItem(IDC_PRIVATE_MESSAGE);
				break;
			case 3:
				menu.SetMenuDefaultItem(IDC_MATCH_QUEUE);
				break;
			case 5:
				menu.SetMenuDefaultItem(IDC_ADD_TO_FAVORITES);
				break;
			case 6:
				menu.SetMenuDefaultItem(IDC_BROWSELIST);
		}
	}
}

LRESULT HubFrame::onSelectUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (getSelectedUser() && ctrlUsers)
	{
		const int pos = ctrlUsers.findItem(Text::toT(getSelectedUser()->getIdentity().getNick()));
		if (pos != -1)
		{
			CLockRedraw<> lockRedraw(ctrlUsers);
			const auto l_count_per_page = ctrlUsers.GetCountPerPage();
			const int items = ctrlUsers.GetItemCount();
			for (int i = 0; i < items; ++i)
				ctrlUsers.SetItemState(i, i == pos ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
			ctrlUsers.EnsureVisible(pos, FALSE);
			const auto l_last_pos = pos + l_count_per_page / 2;
			if (!ctrlUsers.EnsureVisible(l_last_pos, FALSE))
				ctrlUsers.EnsureVisible(pos, FALSE);
		}
	}
	return 0;
}

LRESULT HubFrame::onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (isConnected())
	{
		if (getSelectedUser())
		{
			lastUserName = Text::toT(getSelectedUser()->getIdentity().getNick());
		}
		else
		{
			lastUserName.clear();
			int i = -1;
			while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				if (!lastUserName.empty())
					lastUserName += _T(", ");
					
				lastUserName += Text::toT(ctrlUsers.getItemData(i)->getNick());
			}
		}
		appendNickToChat(lastUserName);
	}
	return 0;
}

LRESULT HubFrame::onAutoScrollChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlClient.invertAutoScroll();
	return 0;
}

LRESULT HubFrame::onBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		const tstring s = _T("!banip ") + ChatCtrl::g_sSelectedIP;
		sendMessage(s);
	}
	return 0;
}

LRESULT HubFrame::onUnBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		const tstring s = _T("!unban ") + ChatCtrl::g_sSelectedIP;
		sendMessage(s);
	}
	return 0;
}

LRESULT HubFrame::onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UserInfo* ui = nullptr;
	if (getSelectedUser())
	{
		ui = findUser(getSelectedUser()); // !SMT!-S
	}
	else
	{
		int i = -1;
		if (ctrlUsers)
			if ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
				ui = ctrlUsers.getItemData(i);
	}
	
	if (!ui)
		return 0;
		
	StringMap params = getFrameLogParams();
	
	params["userNI"] = ui->getNick();
	params["userCID"] = ui->getUser()->getCID().toBase32();
	
	WinUtil::openLog(SETTING(LOG_FILE_PRIVATE_CHAT), params, TSTRING(NO_LOG_FOR_USER));
	
	return 0;
}

LRESULT HubFrame::onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	openFrameLog();
	return 0;
}

LRESULT HubFrame::onMouseMove(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	if (x != prevCursorX || y != prevCursorY)
	{
		if ((wParam & MK_LBUTTON) && ::GetCapture() == m_hWnd)
			UpdateLayout(FALSE);
		prevCursorX = x;
		prevCursorY = y;
	}
	bHandled = FALSE;
	return 0;
}

LRESULT HubFrame::onCaptureChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	UpdateLayout(FALSE);
	prevCursorX = prevCursorY = INT_MAX;
	return 0;
}

void HubFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (ctrlUsers)
	{
		ctrlUsers.SetImageList(g_userImage.getIconList(), LVSIL_SMALL);
		//!!!!ctrlUsers.SetImageList(g_userStateImage.getIconList(), LVSIL_STATE);
		if (ctrlUsers.isRedraw())
		{
			ctrlClient.SetBackgroundColor(Colors::g_bgColor);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		UpdateLayout();
	}
}

LRESULT HubFrame::onSizeMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	if (isClosedOrShutdown())
		return 0;
	if (ctrlClient.IsWindow())
		ctrlClient.goToEnd(false);
	return 0;
}

LRESULT HubFrame::onChatLinkClicked(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
	if (!redirect.empty() && Text::toT(redirect) == ChatCtrl::g_sSelectedURL)
		followRedirect();
	else
		bHandled = FALSE;
	return 0;
}

LRESULT HubFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	if (isClosedOrShutdown())
		return CDRF_DODEFAULT;
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
				const tstring ip = ui->getText(COLUMN_IP);
				if (!ip.empty())
				{
					string ip2 = ui->getIdentity().getIpAsString();
					const bool isPhantomIP = ui->getIdentity().isPhantomIP();
					CustomDrawHelpers::drawIPAddress(customDrawState, cd, isPhantomIP, ip);
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
					if (ui->getIp())
					{
						ui->stateLocation = ui->stateP2PGuard = UserInfo::STATE_IN_PROGRESS;
						// FIXME: use background thread instead of the UI thread
						addTask(LOAD_IP_INFO, new OnlineUserTask(ui->getOnlineUser()));
					}
					else
					{
						ui->stateP2PGuard = UserInfo::STATE_DONE;
						ui->clearLocation();
					}
				}
				const UserPtr& user = ui->getUser();
				if (user->getFlags() & User::ATTRIBS_CHANGED)
				{
					ui->flags = UserInfo::ALL_MASK;
					user->unsetFlag(User::ATTRIBS_CHANGED);
				}
				getUserColor(client && client->isOp(), user, cd->clrText, cd->clrTextBk, ui->flags, ui->getOnlineUser());
				return CDRF_NOTIFYSUBITEMDRAW;
			}
		}
		
		default:
			return CDRF_DODEFAULT;
	}
}

void HubFrame::addDupeUsersToSummaryMenu(const ClientManager::UserParams& param)
{
	vector<std::pair<tstring, UINT> > menuStrings;
	{
		auto fm = FavoriteManager::getInstance();
		LOCK(csFrames);
		for (auto f = frames.cbegin(); f != frames.cend(); ++f)
		{
			const auto& frame = f->second;
			if (frame->isClosedOrShutdown())
				continue;
			READ_LOCK(*frame->csUserMap);
			for (auto i = frame->userMap.cbegin(); i != frame->userMap.cend(); ++i)
			{
				if (frame->isClosedOrShutdown())
					continue;
				const auto& id = i->second->getIdentity();
				const auto currentIp = Util::printIpAddress(id.getUser()->getIP());
				if (/*(param.bytesShared && id.getBytesShared() == param.bytesShared) ||*/
				    (param.nick == id.getNick()) ||
				    (!param.ip.empty() && param.ip == currentIp))
				{
					tstring info = frame->getHubTitle() + _T(" - ") + i->second->getText(COLUMN_NICK);
					const UINT flags = (!param.ip.empty() && param.ip == currentIp) ? MF_CHECKED : 0;
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
					if (!id.getApplication().empty() || !currentIp.empty())
						menuStrings.push_back(make_pair(UserInfoSimple::getTagIP(id.getTag(), currentIp), 0));
					else
						menuStrings.push_back(make_pair(_T(""), 0));
				}
			}
		}
	}
	for (auto i = menuStrings.cbegin(); i != menuStrings.cend(); ++i)
	{
		userSummaryMenu.AppendMenu(MF_SEPARATOR);
		userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED | i->second, (UINT_PTR) 0, i->first.c_str());
		++i;
		if (i != menuStrings.cend() && !i->first.empty())
			userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, (UINT_PTR) 0, i->first.c_str());
	}
}

void HubFrame::addPasswordCommand()
{
	const TCHAR* pass = _T("/password ");
	if (ctrlMessage)
	{
		ctrlMessage.SetWindowText(pass);
		ctrlMessage.SetFocus();
		ctrlMessage.SetSel(10, 10);
	}
	else
	{
		lastMessage = pass;
	}
}

UserInfo* HubFrame::findUser(const OnlineUserPtr& user)
{
	READ_LOCK(*csUserMap);
	auto i = userMap.find(user);
	return i == userMap.end() ? nullptr : i->second;
}

UserInfo* HubFrame::findUserByNick(const tstring& nick)
{
	dcassert(!nick.empty());
	if (nick.empty())
	{
		dcassert(0);
		return nullptr;
	}
	
	if (isDHT)
	{
		CID cid;
		{
			dht::DHT::LockInstanceNodes lock(dht::DHT::getInstance());
			const auto nodes = lock.getNodes();
			if (nodes)
			{
				string nickUtf8 = Text::fromT(nick);
				for (const auto& node : *nodes)
				{
					const Identity& id = node->getIdentity();
					if (id.getNick() == nickUtf8)
					{
						cid = id.getUser()->getCID();
						break;
					}
				}
			}
		}
		if (!cid.isZero())
		{
			const OnlineUserPtr ou = ClientManager::findDHTNode(cid);
			if (ou) return findUser(ou);
		}
		return nullptr;
	}

	if (!client) return nullptr;
	const OnlineUserPtr ou = client->findUser(Text::fromT(nick));
	if (ou) return findUser(ou);
	return nullptr;
}

void HubFrame::insertDHTUsers()
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
			updateUser(ou, (uint32_t) -1);
	}
}

void HubFrame::onTimerInternal()
{
	if (!ClientManager::isStartup() && !isClosedOrShutdown())
	{
		onTimerHubUpdated();
		if (isDHT)
		{
			dht::DHT* d = dht::DHT::getInstance();
			int state = d->getState();
			size_t nodeCount = d->getNodesCount();
			if (currentDHTState != state)
			{
				currentDHTState = state;
				updateModeIcon();
				if (currentDHTState == dht::DHT::STATE_IDLE)
				{
					clearUserList();
					shouldUpdateStats = true;
				}
			}
			if (currentDHTNodeCount != nodeCount)
			{
				currentDHTNodeCount = nodeCount;
				shouldUpdateStats = true;
			}
		}
		if (shouldUpdateStats)
		{
			dcassert(!ClientManager::isBeforeShutdown());
			updateStats();
			shouldUpdateStats = false;
#if 0
			if (!m_is_first_goto_end)
			{
				m_is_first_goto_end = true;
				ctrlClient.goToEnd(true); // Пока не пашет и не появляется скроллер
			}
#endif
		}
		bool redraw = false;
		if (asyncUpdate != asyncUpdateSaved)
		{
			asyncUpdateSaved = asyncUpdate;
			redraw = true;
		}
		if (shouldSort && ctrlUsers && ctrlStatus && !MainFrame::isAppMinimized())
		{
			redraw = false;
			shouldSort = false;
			ctrlUsers.resort();
			//LogManager::message("Resort! Hub = " + baseClient->getHubUrl() + " count = " + Util::toString(ctrlUsers ? itemCount : 0));
		}
		if (redraw)
			ctrlUsers.RedrawWindow();
		processTasks();
	}
	if (!isDHT && --infoUpdateSeconds == 0)
	{
		infoUpdateSeconds = INFO_UPDATE_INTERVAL;
		ClientManager::infoUpdated(client);
	}
}

void HubFrame::updateStats()
{
	if (isDHT || (client && client->isUserListLoaded()))
	{
		size_t allUsers;
		if (isDHT)
		{
			bytesShared = 0;
			allUsers = currentDHTNodeCount;
		}
		else
		{
			bytesShared = client->getBytesShared();
			allUsers = client->getUserCount();
		}
		const size_t itemCount = ctrlUsers.m_hWnd ? ctrlUsers.GetItemCount() : 0;
		const size_t shownUsers = ctrlUsers.m_hWnd ? itemCount : allUsers;
		tstring users = Util::toStringT(shownUsers);
		if (shownUsers != allUsers)
		{
			users += _T('/');
			users += Util::toStringT(allUsers);
		}
		users += _T(' ');
		users += TSTRING(HUB_USERS);
		setStatusText(2, users.c_str());
		if (!isDHT)
		{
			setStatusText(3, Util::formatBytesT(bytesShared));
			setStatusText(4, allUsers ? (Util::formatBytesT(bytesShared / allUsers) + _T('/') + TSTRING(USER)) : Util::emptyStringT);
		}
	}
}

void HubFrame::getUserColor(bool isOp, const UserPtr& user, COLORREF& fg, COLORREF& bg, unsigned short& flags, const OnlineUserPtr& onlineUser)
{
	bool isFav = false;
	bg = Colors::g_bgColor;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	if (SETTING(ENABLE_AUTO_BAN))
	{
		if ((flags & IS_AUTOBAN) == IS_AUTOBAN)
		{
			// BUG? isFav is false here
			if (onlineUser && user->hasAutoBan(&onlineUser->getClient(), isFav) != User::BAN_NONE)
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
	if (isOp && onlineUser)
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
		if (UserManager::getInstance()->isInIgnoreList(onlineUser ? onlineUser->getIdentity().getNick() : user->getLastNick()))
			flags = (flags & ~IS_IGNORED_USER) | IS_IGNORED_USER_ON;
		else
			flags = (flags & ~IS_IGNORED_USER);
	}
	if ((flags & IS_RESERVED_SLOT) == IS_RESERVED_SLOT)
	{
		if (UploadManager::getInstance()->getReservedSlotTick(user))
			flags = (flags & ~IS_RESERVED_SLOT) | IS_RESERVED_SLOT_ON;
		else
			flags = (flags & ~IS_RESERVED_SLOT);
	}
	if ((flags & IS_FAVORITE) == IS_FAVORITE || (flags & IS_BAN) == IS_BAN)
	{
		bool isBanned = false;
		isFav = FavoriteManager::getInstance()->isFavoriteUser(user, isBanned);
		flags &= ~(IS_FAVORITE | IS_BAN);
		if (isFav)
		{
			flags |= IS_FAVORITE_ON;
			if (isBanned)
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
	else if (onlineUser && onlineUser->getIdentity().isOp())
	{
		fg = SETTING(OP_COLOR);
	}
	else if (flags & IS_IGNORED_USER)
	{
		fg = SETTING(IGNORED_COLOR);
	}
	else if (userFlags & User::FIREBALL)
	{
		fg = SETTING(FIREBALL_COLOR);
	}
	else if (userFlags & User::SERVER)
	{
		fg = SETTING(SERVER_COLOR);
	}
	else if (onlineUser && !onlineUser->getIdentity().isTcpActive())
	{
		fg = SETTING(PASSIVE_COLOR);
	}
	else
	{
		fg = SETTING(NORMAL_COLOR);
	}
}
