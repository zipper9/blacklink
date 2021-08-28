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
#include "HubFrame.h"
#include "SearchFrm.h"
#include "PrivateFrame.h"
#include "MainFrm.h"
#include "../client/QueueManager.h"
#include "../client/ConnectivityManager.h"
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

template<>
string UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::getNick(const OnlineUserPtr& user)
{
	return user->getIdentity().getNick();
}

void HubFrame::Settings::copySettings(const FavoriteHubEntry& entry)
{
	server = entry.getServer();
	name = entry.getName();
	rawCommands = entry.getRawCommands();
	windowPosX = entry.getWindowPosX();
	windowPosY = entry.getWindowSizeY();
	windowType = entry.getWindowType();
	chatUserSplit = entry.getChatUserSplit();
	hideUserList = entry.getHideUserList();
	suppressChatAndPM = entry.getSuppressChatAndPM();
}

HubFrame::HubFrame(const Settings& cs) :
	timer(m_hWnd),
	client(nullptr),
	ctrlUsers(this),
	infoUpdateSeconds(INFO_UPDATE_INTERVAL),
	hubUpdateCount(0),
	waitingForPassword(false),
	shouldUpdateStats(false),
	showingPasswordDlg(false),
	showUsersContainer(nullptr),
	ctrlChatContainer(nullptr),
	swapPanels(false),
	switchPanelsContainer(nullptr),
	isTabMenuShown(false),
	showJoins(false),
	showFavJoins(false),
	userListInitialized(false),
	bytesShared(0),
	activateCounter(0),
	hubParamUpdated(false),
	m_is_ddos_detect(false),
	asyncUpdate(0),
	asyncUpdateSaved(0)
{
	ctrlStatusCache.resize(5);
	showUsersStore = !cs.hideUserList;
	showUsers = false;
	serverUrl = cs.server;
	m_nProportionalPos = cs.chatUserSplit;

	if (serverUrl == dht::NetworkName)
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
		client->setName(cs.name);
		if (cs.rawCommands) client->setRawCommands(cs.rawCommands);
		if (cs.encoding && client->getType() == ClientBase::TYPE_NMDC) client->setEncoding(cs.encoding);
		client->setSuppressChatAndPM(cs.suppressChatAndPM);
		client->addListener(this);
		baseClient = client;
		isDHT = false;
		currentDHTState = 0;
		currentDHTNodeCount = 0;
	}
}

void HubFrame::doDestroyFrame()
{
	destroyUserMenu();
	if (tabMenu.m_hMenu) tabMenu.DestroyMenu();
	destroyMessagePanel(true);
}

HubFrame::~HubFrame()
{
	removeFrame(Util::emptyString);
	delete ctrlChatContainer;
	if (client)
		ClientManager::getInstance()->putClient(client);
}

LRESULT HubFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	BaseChatFrame::onCreate(m_hWnd, rcDefault);

	m_hAccel = LoadAccelerators(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDR_HUB));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);
	
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

LRESULT HubFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);

	bHandled = FALSE;
	return 0;
}

void HubFrame::initUserList(const FavoriteManager::WindowInfo& wi)
{
	if (userListInitialized) return;
	FavoriteManager::getInstance()->addListener(this);
	UserManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	userListInitialized = true;
	ctrlUsers.initialize(wi);
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
		if (!ClientManager::isStartup())
		{
			++activateCounter;
			if (!ctrlUsers)
				ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CONTROLPARENT);
			BaseChatFrame::createMessageCtrl(this, EDIT_MESSAGE_MAP, isSuppressChatAndPM());

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
			if (activateCounter == 1)
				showJoins = fhe ? fhe->getShowJoins() : BOOLSETTING(SHOW_JOINS);
			fm->releaseFavoriteHubEntryPtr(fhe);

			showFavJoins = BOOLSETTING(FAV_SHOW_JOINS);
			initUserList(wi);
			ctrlMessage.SetFocus();
			if (activateCounter == 1)
			{
				showUsers = showUsersStore;
				ctrlUsers.setShowUsers(showUsers);
				updateSplitterPosition(wi.chatUserSplit, wi.swapPanels);
			}
			if (isDHT) ctrlUsers.insertDHTUsers();
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

	BaseChatFrame::destroyStatusbar();
	BaseChatFrame::destroyMessagePanel();
	BaseChatFrame::destroyMessageCtrl(l_is_shutdown);
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

HubFrame* HubFrame::openHubWindow(const Settings& cs)
{
	LOCK(csFrames);
	HubFrame* frm;
	const auto i = frames.find(cs.server);
	if (i == frames.end())
	{
		frm = new HubFrame(cs);
		CRect rc = frm->rcDefault;
		rc.left   = cs.windowPosX;
		rc.top    = cs.windowPosY;
		rc.right  = rc.left + cs.windowSizeX;
		rc.bottom = rc.top + cs.windowSizeY;
		if (rc.left < 0 || rc.top < 0 || rc.right - rc.left < 10 || rc.bottom - rc.top < 10)
		{
			CRect rcmdiClient;
			::GetWindowRect(WinUtil::g_mdiClient, &rcmdiClient);
			rc = rcmdiClient; // frm->rcDefault;
		}
		frm->Create(WinUtil::g_mdiClient, rc);
		//if (cs.windowType)
		//	frm->ShowWindow(cs.windowType);
		frames.insert(make_pair(cs.server, frm));
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

HubFrame* HubFrame::openHubWindow(const string& server)
{
	Settings cs;
	cs.server = server;
	return openHubWindow(cs);
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
		const string desc = ConnectivityManager::getInstance()->getInformation();
		string ipAddress;
		if (isDHT)
		{
			bool isFirewalled;
			dht::DHT::getInstance()->getPublicIPInfo(ipAddress, isFirewalled);
		}
		else if (client)
		{
			Ip4Address ip4;
			Ip6Address ip6;
			client->getLocalIp(ip4, ip6);
			if (Util::isValidIp4(ip4))
				ipAddress = Util::printIpAddress(ip4);
			if (Util::isValidIp6(ip6))
			{
				if (!ipAddress.empty()) ipAddress += ", ";
				ipAddress += Util::printIpAddress(ip6);
			}
		}
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
		clearInputBox();
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
	if (client->getType() == ClientBase::TYPE_NMDC) entry.setEncoding(client->getEncoding());
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
	}
	else if (autoConnectType != DONT_CHANGE)
	{
		fm->setFavoriteHubAutoConnect(serverUrl, autoConnect);
		addStatus(autoConnect ? TSTRING(AUTO_CONNECT_ADDED) : TSTRING(AUTO_CONNECT_REMOVED));
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
		addStatus(TSTRING(FAVORITE_HUB_REMOVED));
	else
		addStatus(TSTRING(FAVORITE_HUB_DOES_NOT_EXIST));
}

void HubFrame::createTabMenu()
{
	bool isFav = false;
	bool isAutoConnect = false;
	auto fm = FavoriteManager::getInstance();
	const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(serverUrl);
	if (fhe)
	{
		isFav = true;
		isAutoConnect = fhe->getAutoConnect();
	}
	fm->releaseFavoriteHubEntryPtr(fhe);

	if (tabMenu)
		tabMenu.ClearMenu();
	else
		tabMenu.CreatePopupMenu();
	if (BOOLSETTING(LOG_MAIN_CHAT))
	{
		tabMenu.AppendMenu(MF_STRING, IDC_OPEN_HUB_LOG, CTSTRING(OPEN_HUB_LOG));
		tabMenu.AppendMenu(MF_SEPARATOR);
	}
	if (isFav)
	{
		tabMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS));
		if (isAutoConnect)
			tabMenu.AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_OFF));
		else
			tabMenu.AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_ON));
		tabMenu.AppendMenu(MF_STRING, IDC_EDIT_HUB_PROP, CTSTRING(PROPERTIES));
	}
	else
	{
		tabMenu.AppendMenu(MF_STRING, IDC_ADD_AS_FAVORITE, CTSTRING(ADD_TO_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::FAVORITE, 0));
	}
	tabMenu.AppendMenu(MF_STRING, IDC_RECONNECT, CTSTRING(MENU_RECONNECT), g_iconBitmaps.getBitmap(IconBitmaps::RECONNECT, 0));
	if (isConnected())
		tabMenu.AppendMenu(MF_STRING, ID_DISCONNECT, CTSTRING(DISCONNECT));
	tabMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::g_copyHubMenu, CTSTRING(COPY));
	tabMenu.AppendMenu(MF_SEPARATOR);
	tabMenu.AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED), g_iconBitmaps.getBitmap(IconBitmaps::RESTORE_CONN, 0));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_DISCONNECTED, CTSTRING(MENU_CLOSE_DISCONNECTED));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
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
				Util::getIpInfo(id.getIP4(), ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
				sCopy += Util::getDescription(ipInfo);
				break;
			}
			case IDC_COPY_IP:
				sCopy += Util::printIpAddress(id.getConnectIP());
				break;
			case IDC_COPY_NICK_IP:
			{
				// TODO translate
				sCopy += "User Info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n";
				Ip4Address ip4 = id.getIP4();
				if (Util::isValidIp4(ip4))
				{
					IpAddress ip;
					ip.type = AF_INET;
					ip.data.v4 = ip4;
					sCopy += "\tIPv4: " + Identity::formatIpString(ip) + "\r\n";
				}
				Ip6Address ip6 = id.getIP6();
				if (Util::isValidIp6(ip6))
				{
					IpAddress ip;
					ip.type = AF_INET6;
					ip.data.v6 = ip6;
					sCopy += "\tIPv6: " + Identity::formatIpString(ip) + "\r\n";
				}
				break;
			}
			case IDC_COPY_ALL:
			{
				// TODO: Use Identity::getReport ?
				bool isNMDC = (u->getFlags() & User::NMDC) != 0;
				sCopy += "User info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
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
				         "\t" + STRING(SLOTS) + ": " + Util::toString(id.getSlots()) + "\r\n";
				Ip4Address ip4 = id.getIP4();
				if (Util::isValidIp4(ip4))
				{
					IpAddress ip;
					ip.type = AF_INET;
					ip.data.v4 = ip4;
					sCopy += "\tIPv4: " + Identity::formatIpString(ip) + "\r\n";
				}
				Ip6Address ip6 = id.getIP6();
				if (Util::isValidIp6(ip6))
				{
					IpAddress ip;
					ip.type = AF_INET6;
					ip.data.v6 = ip6;
					sCopy += "\tIPv6: " + Identity::formatIpString(ip) + "\r\n";
				}
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
	ctrlUsers.clearUserList();
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
	ctrlUsers.clearUserList();
}

void HubFrame::doDisconnected()
{
	dcassert(!ClientManager::isBeforeShutdown());
	ctrlUsers.clearUserList();
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
		if (ctrlUsers.updateUser(ou, (uint32_t) -1, true))
		{
			const Identity& id = ou->getIdentity();
			if (!client || client->isUserListLoaded())
			{
				dcassert(!id.getNick().empty());
				const bool isFavorite = (ou->getUser()->getFlags() & (User::FAVORITE | User::BANNED)) == User::FAVORITE;
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
		dcdebug("updateUserJoin: user=%s, hub=%s (not connected)\n", ou->getIdentity().getNick().c_str(), ou->getClient().getAddress().c_str());
		dcassert(0);
	}
}

void HubFrame::addTask(int type, Task* task)
{
	bool firstItem;
	uint64_t tick = GET_TICK();
	uint64_t prevTick = tick;
	if (tasks.add(type, task, firstItem, prevTick) && prevTick + TIMER_VAL < tick)
		PostMessage(WM_SPEAKER);
}

void HubFrame::processTasks()
{
	TaskQueue::List t;
	tasks.get(t);
	if (t.empty()) return;

	bool redrawChat = false;
	//if (ctrlUsers.m_hWnd) -- FIXME
	//	ctrlUsers.SetRedraw(FALSE);

	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			switch (i->first)
			{
				case UPDATE_USER:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					shouldUpdateStats |= ctrlUsers.updateUser(u.ou, (uint32_t) -1, isDHT || isConnected());
				}
				break;
				case UPDATE_USERS:
				{
					const OnlineUsersTask& u = static_cast<OnlineUsersTask&>(*i->second);
					for (const OnlineUserPtr& ou : u.userList)
						shouldUpdateStats |= ctrlUsers.updateUser(ou, (uint32_t) -1, isDHT || isConnected());
				}
				break;
				case LOAD_IP_INFO:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					if (ctrlUsers.loadIPInfo(u.ou))
						++asyncUpdate;
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
					ctrlUsers.removeUser(u.ou);
					shouldUpdateStats = true;
				}
				break;
				case REMOVE_USERS:
				{
					const OnlineUsersTask& u = static_cast<const OnlineUsersTask&>(*i->second);
					for (const OnlineUserPtr& ou : u.userList)
					{
						onUserParts(ou);
						ctrlUsers.removeUser(ou);
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
							if (!redrawChat)
							{
								ctrlClient.SetRedraw(FALSE);
								redrawChat = true;
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
							shouldUpdateStats |= ctrlUsers.updateUser(msg->from, 1<<COLUMN_MESSAGES, isDHT || isConnected());
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
						isPrivateFrameOk = PrivateFrame::gotMessage(from, to, replyTo, text, 0, getHubHint(), myPM, pm->thirdPerson);
					}
					if (!isPrivateFrameOk)
					{
						BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + Text::toT(id.getNick()) + _T(": ") + text, 0, Colors::g_ChatTextPrivate);
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
		ctrlClient.Invalidate();
	}
	//if (ctrlUsers.m_hWnd)
	//	ctrlUsers.SetRedraw(TRUE);
}

void HubFrame::onUserParts(const OnlineUserPtr& ou)
{
	const UserPtr& user = ou->getUser();
	const Identity& id = ou->getIdentity();

	if (!id.isBotOrHub())
	{
		const bool isFavorite = (user->getFlags() & (User::FAVORITE | User::BANNED)) == User::FAVORITE;
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
		const int h = getInputBoxHeight();
		int panelHeight = msgPanel->initialized ? h + 6 : 0;

		CRect rc = rect;
		rc.bottom -= panelHeight;
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
		
		if (msgPanel->initialized)
		{
			int buttonPanelWidth = MessagePanel::getPanelWidth();
			rc = rect;
			rc.left += 2;
			rc.bottom -= 4;
			rc.top = rc.bottom - h;
			rc.right -= buttonPanelWidth;

			const CRect rcMessage = rc;
			if (ctrlMessage)
				ctrlMessage.MoveWindow(rcMessage);
		
			rc.left = rc.right;
			rc.right += buttonPanelWidth;
			rc.bottom -= 1;

			rc.top += 1;
			msgPanel->updatePanel(rc);
		}
		if (tooltip)
			tooltip.Activate(TRUE);
	}
	if (showUsers && ctrlUsers)
		ctrlUsers.updateLayout();
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
	if (!userListInitialized)
		return;
	auto fm = FavoriteManager::getInstance();
	FavoriteManager::WindowInfo wi;
	ctrlUsers.getUserList().saveHeaderOrder(wi.headerOrder, wi.headerWidths, wi.headerVisible);
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
		wi.headerSort = ctrlUsers.getUserList().getSortColumn();
		wi.headerSortAsc = ctrlUsers.getUserList().isAscending();
		fm->setFavoriteHubWindowInfo(serverUrl, wi);
	}
	else
	{
		SET_SETTING(HUB_FRAME_ORDER, wi.headerOrder);
		SET_SETTING(HUB_FRAME_WIDTHS, wi.headerWidths);
		SET_SETTING(HUB_FRAME_VISIBLE, wi.headerVisible);
		SET_SETTING(HUB_FRAME_SORT, ctrlUsers.getUserList().getSortForSettings());
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
		if (userListInitialized)
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
						ctrlUsers.ensureVisible(ui);
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
	isTabMenuShown = true;
	createTabMenu();
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
		tabMenu.InsertSeparatorFirst(Text::toT(name));
	}
	else
		tabMenu.InsertSeparatorFirst(TSTRING(DHT_TITLE));
	if (client)
		appendUcMenu(tabMenu, UserCommand::CONTEXT_HUB, client->getHubUrl());
	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | WinUtil::g_tabCtrl->getContextMenuAlign(), pt.x, pt.y, m_hWnd);
	cleanUcMenu(tabMenu);
	tabMenu.RemoveFirstItem();
	return TRUE;
}

LRESULT HubFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	CRect rc;
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	isTabMenuShown = false;
	if (!ctrlUsers.m_hWnd)
		return FALSE;

	if (ctrlUsers.showHeaderMenu(pt))
		return TRUE;

	if (reinterpret_cast<HWND>(wParam) == ctrlUsers.m_hWnd && showUsers)
	{
		OMenu* userMenu = createUserMenu();
		userMenu->ClearMenu();
		clearUserMenu();

		int ctx = 0;
		bool isMultiple;
		UserInfo* ui = ctrlUsers.getSelectedUserInfo(&isMultiple);
		if (ui)
		{
			ctx |= UserCommand::CONTEXT_USER;
			if (!isMultiple)
			{
				const OnlineUserPtr& ou = ui->getOnlineUser();
				if (ou->getUser()->isMe())
					ctx |= UserCommand::CONTEXT_FLAG_ME;
				reinitUserMenu(ou, getHubHint());
			}
		}

		appendHubAndUsersItems(*userMenu, false);
		appendUcMenu(*userMenu, ctx, baseClient->getHubUrl());
		WinUtil::appendSeparator(*userMenu);
		userMenu->AppendMenu(MF_STRING, IDC_REFRESH, CTSTRING(REFRESH_USER_LIST));

		if (ui)
		{
			if (pt.x == -1 && pt.y == -1)
				WinUtil::getContextMenuPos(ctrlUsers.getUserList(), pt);
			userMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}

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
			{
				int ctx = UserCommand::CONTEXT_USER;
				if (ui->getUser()->isMe()) ctx |= UserCommand::CONTEXT_FLAG_ME;
				appendUcMenu(*userMenu, ctx, client->getHubUrl());
			}
			userMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		WinUtil::unlinkStaticMenus(*userMenu);
		cleanUcMenu(*userMenu);
		userMenu->ClearMenu();
		return TRUE;
	}

	if (msgPanel && msgPanel->onContextMenu(pt, wParam))
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
		if (uc.isRaw()) client->escapeParams(ucParams);
		client->sendUserCmd(uc, ucParams);
	}
	else if (getSelectedUser())
	{
		const UserInfo* u = ctrlUsers.findUser(getSelectedUser());
		if (u && u->getUser()->isOnline())
		{
			StringMap tmp = ucParams;
			u->getIdentity().getParams(tmp, "user", true);
			if (uc.isRaw()) client->escapeParams(tmp);
			client->sendUserCmd(uc, tmp);
		}
	}
	else
	{
		const auto& listView = ctrlUsers.getUserList();
		int sel = -1;
		while ((sel = listView.GetNextItem(sel, LVNI_SELECTED)) != -1)
		{
			const UserInfo* u = listView.getItemData(sel);
			if (u->getUser()->isOnline())
			{
				StringMap tmp = ucParams;
				u->getIdentity().getParams(tmp, "user", true);
				if (uc.isRaw()) client->escapeParams(tmp);
				client->sendUserCmd(uc, tmp);
			}
		}
	}
}

bool HubFrame::handleAutoComplete()
{
	tstring text;
	WinUtil::getWindowText(ctrlMessage, text);
		
	string::size_type textStart = text.find_last_of(_T(" \n\t"));
		
	auto& listView = ctrlUsers.getUserList();
	if (autoComplete.empty())
	{
		if (textStart != string::npos)
			autoComplete = text.substr(textStart + 1);
		else
			autoComplete = text;
		if (autoComplete.empty())
		{
			// Still empty, no text entered...
			ctrlUsers.SetFocus();
			return true;
		}
		const int itemCount = listView.GetItemCount();
		for (int i = 0; i < itemCount; ++i)
			listView.SetItemState(i, 0, LVNI_FOCUSED | LVNI_SELECTED);
	}
	
	if (textStart == string::npos)
		textStart = 0;
	else
		textStart++;

	int start = listView.GetNextItem(-1, LVNI_FOCUSED) + 1;
	int i = start;
	const int itemCount = listView.GetItemCount();
		
	bool firstPass = i < itemCount;
	if (!firstPass) i = 0;
	while (firstPass || i < start)
	{
		const UserInfo* ui = listView.getItemData(i);
		const tstring nick = ui->getText(COLUMN_NICK);
		bool found = strnicmp(nick, autoComplete, autoComplete.length()) == 0;
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
					if (strnicmp(nick.c_str() + x + 1, autoComplete.c_str(), autoComplete.length()) == 0)
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
			if (start != 0)
				listView.SetItemState(start - 1, 0, LVNI_SELECTED | LVNI_FOCUSED);
			listView.SetItemState(i, LVNI_FOCUSED | LVNI_SELECTED, LVNI_FOCUSED | LVNI_SELECTED);
			listView.EnsureVisible(i, FALSE);
			if (ctrlMessage)
			{
				ctrlMessage.SetSel(static_cast<int>(textStart), ctrlMessage.GetWindowTextLength(), TRUE);
				ctrlMessage.ReplaceSel(nick.c_str());
			}
			return true;
		}
		i++;
		if (i == itemCount)
		{
			firstPass = false;
			i = 0;
		}
	}
	return false;
}

void HubFrame::clearAutoComplete()
{
	autoComplete.clear();
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

LRESULT HubFrame::onKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!autoComplete.empty() && wParam != VK_TAB)
		autoComplete.clear();

	if (!processHotKey(wParam))
		bHandled = FALSE;
	return 0;
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
			setShowUsers(true);
		else
			setShowUsers(false);
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
		ctrlUsers.setSortFlag();
}

void HubFrame::on(UserManagerListener::IgnoreListChanged, const string& userName) noexcept
{
	ctrlUsers.onIgnoreListChanged(userName);
}

void HubFrame::on(UserManagerListener::IgnoreListCleared) noexcept
{
	ctrlUsers.onIgnoreListCleared();
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
		{
			ctrlClient.setHubParam(dht::NetworkName, SETTING(NICK));
			ctrlUsers.setHubHint(dht::NetworkName);
		}
		else if (client)
		{
			ctrlClient.setHubParam(client->getHubUrl(), client->getMyNick());
			ctrlUsers.setHubHint(client->getHubUrl());
		}
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

static int chatActionToId(int action)
{
	switch (action)
	{
		case 0:
			return IDC_SELECT_USER;
		case 1:
			return IDC_ADD_NICK_TO_CHAT;
		case 2:
			return IDC_PRIVATE_MESSAGE;
		case 3:
			return IDC_GETLIST;
		case 4:
			return IDC_MATCH_QUEUE;
		case 6:
			return IDC_ADD_TO_FAVORITES;
		case 7:
			return IDC_BROWSELIST;
	}
	return 0;
}

static int userListActionToId(int action)
{
	switch (action)
	{
		case 0:
			return IDC_GETLIST;
		case 1:
			return IDC_ADD_NICK_TO_CHAT;
		case 2:
			return IDC_PRIVATE_MESSAGE;
		case 3:
			return IDC_MATCH_QUEUE;
		case 5:
			return IDC_ADD_TO_FAVORITES;
		case 6:
			return IDC_BROWSELIST;
	}
	return 0;
}

void HubFrame::appendHubAndUsersItems(OMenu& menu, const bool isChat)
{
	if (getSelectedUser())
	{
		const bool isMe = getSelectedUser()->getUser()->isMe();
		menu.InsertSeparatorFirst(Text::toT(getSelectedUser()->getIdentity().getNick()));
		if (!isMe)
			appendAndActivateUserItems(menu, false);	
		else
			appendCopyMenuForSingleUser(menu);
		if (isChat && showUsers)
			menu.AppendMenu(MF_STRING, IDC_SELECT_USER, CTSTRING(SELECT_USER_LIST));
	}
	else if (!isChat)
	{
		int count = ctrlUsers.getUserList().GetSelectedCount();
		menu.InsertSeparatorFirst(Util::toStringT(count) + _T(' ') + TSTRING(HUB_USERS));
		if (count < 50) // Limit maximum number of selected users
			appendAndActivateUserItems(menu, false);
	}
	
	if (isChat)
	{
		appendChatCtrlItems(menu, client && client->isOp());
	}
	
	if (isChat)
	{
		int idc = chatActionToId(SETTING(CHAT_DBLCLICK));
		if (idc) menu.SetMenuDefaultItem(idc);
	}
	else
	{
		int idc = userListActionToId(SETTING(USERLIST_DBLCLICK));
		if (idc) menu.SetMenuDefaultItem(idc);
	}
}

LRESULT HubFrame::onSelectUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (getSelectedUser() && ctrlUsers)
	{
		tstring nick = Text::toT(getSelectedUser()->getIdentity().getNick());
		if (ctrlUsers.selectNick(nick))
			ctrlUsers.getUserList().SetFocus();
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
			const auto& listView = ctrlUsers.getUserList();
			int i = -1;
			while ((i = listView.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				if (!lastUserName.empty())
					lastUserName += _T(", ");

				lastUserName += Text::toT(listView.getItemData(i)->getNick());
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
		ui = ctrlUsers.findUser(getSelectedUser());
		if (!ui) return 0;
	}

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

void HubFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	auto& listView = ctrlUsers.getUserList();
	if (listView)
	{
		listView.SetImageList(g_userImage.getIconList(), LVSIL_SMALL);
		if (listView.isRedraw())
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

void HubFrame::addDupeUsersToSummaryMenu(const ClientManager::UserParams& param)
{
	vector<std::pair<tstring, UINT>> menuStrings;
	{
		LOCK(csFrames);
		for (auto f = frames.cbegin(); f != frames.cend(); ++f)
		{
			const auto& frame = f->second;
			if (!frame->isClosedOrShutdown())
				frame->ctrlUsers.getDupUsers(param, frame->getHubTitle(), menuStrings);
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
			if (ou) return ctrlUsers.findUser(ou);
		}
		return nullptr;
	}

	if (!client) return nullptr;
	const OnlineUserPtr ou = client->findUser(Text::fromT(nick));
	if (ou) return ctrlUsers.findUser(ou);
	return nullptr;
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
					ctrlUsers.clearUserList();
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
		}
		bool redraw = false;
		if (asyncUpdate != asyncUpdateSaved)
		{
			asyncUpdateSaved = asyncUpdate;
			redraw = true;
		}
		if (!MainFrame::isAppMinimized() && ctrlUsers.checkSortFlag())
			redraw = false;
		if (redraw)
			ctrlUsers.getUserList().RedrawWindow();
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
		auto& listView = ctrlUsers.getUserList();
		const size_t itemCount = listView.m_hWnd ? listView.GetItemCount() : 0;
		const size_t shownUsers = listView.m_hWnd ? itemCount : allUsers;
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

void HubFrame::showErrorMessage(const tstring& text)
{
	addStatus(text);
}

void HubFrame::setCurrentNick(const tstring& nick)
{
	lastUserName = nick;
}

void HubFrame::appendNickToChat(const tstring& nick)
{
	BaseChatFrame::appendNickToChat(nick);
}

BOOL HubFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::g_tabCtrl->isActive(m_hWnd)) return FALSE;
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	if (WinUtil::isCtrl()) return FALSE;
	return IsDialogMessage(pMsg);
}
