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
#include "HubFrameTasks.h"
#include "SearchFrm.h"
#include "PrivateFrame.h"
#include "MainFrm.h"
#include "Fonts.h"
#include "../client/QueueManager.h"
#include "../client/ConnectivityManager.h"
#include "../client/UserManager.h"
#include "../client/dht/DHT.h"
#include "FavHubProperties.h"
#include "LineDlg.h"

static const unsigned TIMER_VAL = 1000;
static const int INFO_UPDATE_INTERVAL = 60;
static const int STATUS_PART_PADDING = 12;

HubFrame::FrameMap HubFrame::frames;

template<>
string UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::getNick(const OnlineUserPtr& user)
{
	return user->getIdentity().getNick();
}

void HubFrame::Settings::copySettings(const FavoriteHubEntry& entry)
{
	server = entry.getServer();
	name = entry.getName();
	favoriteId = entry.getID();
	rawCommands = entry.getRawCommands();
	windowPosX = entry.getWindowPosX();
	windowPosY = entry.getWindowSizeY();
	windowType = entry.getWindowType();
	chatUserSplit = entry.getChatUserSplit();
	hideUserList = entry.getHideUserList();
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
	showUsersContainer(WC_BUTTON, this, EDIT_MESSAGE_MAP),
	ctrlChatContainer(nullptr),
	swapPanels(false),
	switchPanelsContainer(WC_BUTTON, this, HUBSTATUS_MESSAGE_MAP),
	isTabMenuShown(false),
	showJoins(false),
	showFavJoins(false),
	uiInitialized(false),
	bytesShared(0),
	hubParamUpdated(false),
	asyncUpdate(0),
	asyncUpdateSaved(0),
	disableChat(false)
{
	frameId = WinUtil::getNewFrameID(WinUtil::FRAME_TYPE_HUB);
	ctrlStatusCache.resize(5);
	showUsersStore = !cs.hideUserList;
	showUsers = false;
	serverUrl = cs.server;
	m_nProportionalPos = cs.chatUserSplit;

	if (serverUrl == dht::NetworkName)
	{
		baseClient = dht::DHT::getClientBaseInstance();
		dht::DHT* d = static_cast<dht::DHT*>(baseClient.get());
		d->addListener(this);
		client = nullptr;
		isDHT = true;
		currentDHTState = d->getState();
		currentDHTNodeCount = d->getNodesCount();
	}
	else
	{
		string url = serverUrl;
		if (!cs.keyPrint.empty()) url += "?kp=" + cs.keyPrint;
		baseClient = ClientManager::getInstance()->getClient(url);
		client = static_cast<Client*>(baseClient.get());
		client->setName(cs.name);
		client->setFavoriteId(cs.favoriteId);
		if (cs.rawCommands) client->setRawCommands(cs.rawCommands);
		if (cs.encoding && client->getType() == ClientBase::TYPE_NMDC) client->setEncoding(cs.encoding);
		client->addListener(this);
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
	if (baseClient && baseClient->getType() != ClientBase::TYPE_DHT)
		ClientManager::getInstance()->putClient(baseClient);
}

LRESULT HubFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	BaseChatFrame::onCreate(m_hWnd, rcDefault);

	m_hAccel = LoadAccelerators(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDR_HUB));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	createChatCtrl();
	if (!ctrlChatContainer && ctrlClient.m_hWnd)
	{
		ctrlChatContainer = new CContainedWindow(WC_EDIT, this, EDIT_MESSAGE_MAP);
		ctrlChatContainer->SubclassWindow(ctrlClient.m_hWnd);
	}

	setHubParam();
	
	// TODO - отложить создание контрола...
	// TODO - может колонки не создвать пока они не нужны?
	bHandled = FALSE;
	if (isDHT)
	{
		SetWindowText(CTSTRING(DHT_TITLE));
		SetIcon(g_iconBitmaps.getIcon(IconBitmaps::DHT, 0), FALSE);
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

void HubFrame::updateSplitterPosition(int chatUserSplit, bool swapFlag)
{
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

void HubFrame::initUI()
{
	dcassert(!uiInitialized);
	if (!ctrlUsers)
		ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CONTROLPARENT);

	if (!m_hWndStatusBar)
		CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	BaseChatFrame::initStatusCtrl(m_hWndStatusBar);

	if (!ctrlSwitchPanels)
	{
		ctrlSwitchPanels.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER | BS_PUSHBUTTON, 0, IDC_HUBS_SWITCHPANELS);
		ctrlSwitchPanels.SetFont(Fonts::g_systemFont);
		ctrlSwitchPanels.SetIcon(g_iconBitmaps.getIcon(IconBitmaps::HUB_SWITCH, 0));
		switchPanelsContainer.SubclassWindow(ctrlSwitchPanels.m_hWnd);
	}

	if (!ctrlShowUsers)
	{
		ctrlShowUsers.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		ctrlShowUsers.SetButtonStyle(BS_AUTOCHECKBOX, false);
		ctrlShowUsers.SetFont(Fonts::g_systemFont);
		ctrlShowUsers.SetCheck(showUsersStore ? BST_CHECKED : BST_UNCHECKED);
		showUsersContainer.SubclassWindow(ctrlShowUsers.m_hWnd);
	}

	if (!ctrlModeIcon)
		ctrlModeIcon.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SS_ICON | BS_CENTER | BS_PUSHBUTTON, 0);

	if (!tooltip)
	{
		tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
		tooltip.SetDelayTime(TTDT_AUTOPOP, 10000);
		tooltip.SetMaxTipWidth(255);
		tooltip.AddTool(ctrlSwitchPanels, ResourceManager::CMD_HELP_USER_LIST_LOCATION);
		tooltip.AddTool(ctrlShowUsers, ResourceManager::CMD_HELP_TOGGLE_USER_LIST);
	}

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
	showJoins = fhe ? fhe->getShowJoins() : BOOLSETTING(SHOW_JOINS);
	fm->releaseFavoriteHubEntryPtr(fhe);

	showFavJoins = BOOLSETTING(FAV_SHOW_JOINS);

	ctrlUsers.initialize(wi);

	showUsers = showUsersStore;
	ctrlUsers.setShowUsers(showUsers);
	updateSplitterPosition(wi.chatUserSplit, wi.swapPanels);
	if (isDHT) ctrlUsers.insertDHTUsers();

	updateModeIcon();
	restoreStatusFromCache();
	
	FavoriteManager::getInstance()->addListener(this);
	UserManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	uiInitialized = true;
}

void HubFrame::createMessagePanel()
{
	if (!isClosedOrShutdown())
	{
		BaseChatFrame::createMessageCtrl(this, EDIT_MESSAGE_MAP);
		BaseChatFrame::createMessagePanel(false, false);
		BaseChatFrame::setChatDisabled(disableChat);
		setCountMessages(0);
	}
}

void HubFrame::destroyMessagePanel(bool isShutdown)
{
	if (ClientManager::isBeforeShutdown())
		isShutdown = true;
	BaseChatFrame::destroyMessagePanel();
	BaseChatFrame::destroyMessageCtrl(isShutdown);
}

void HubFrame::onDeactivate()
{
	bool update = msgPanel != nullptr;
	destroyMessagePanel(false);
	if (update) UpdateLayout();
}

void HubFrame::onActivate()
{
	if (!ClientManager::isBeforeShutdown())
		showActiveFrame();
}

void HubFrame::showActiveFrame()
{
	if (!uiInitialized && !ClientManager::isStartup()) initUI();
	createMessagePanel();
	UpdateLayout();
	ctrlMessage.SetFocus();
}

HubFrame* HubFrame::openHubWindow(const Settings& cs, bool* isNew)
{
	ASSERT_MAIN_THREAD();
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
		if (isNew) *isNew = true;
	}
	else
	{
		frm = i->second;
		if (::IsIconic(frm->m_hWnd))
			::ShowWindow(frm->m_hWnd, SW_RESTORE);
		if (frm->m_hWndMDIClient)
			frm->MDIActivate(frm->m_hWnd);
		if (isNew) *isNew = false;
	}
	return frm;
}

HubFrame* HubFrame::openHubWindow(const string& server, const string& keyPrint)
{
	Settings cs;
	cs.server = server;
	cs.keyPrint = keyPrint;
	return openHubWindow(cs);
}

HubFrame* HubFrame::findHubWindow(const string& server)
{
	ASSERT_MAIN_THREAD();
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
		sendMessage(Text::fromT(fullMessageText));
	}
}

StringMap HubFrame::getFrameLogParams() const
{
	StringMap params;
	
	params["hubNI"] = baseClient->getHubName();
	params["hubURL"] = baseClient->getHubUrl();
	params["myNI"] = baseClient->getMyNick();
	
	return params;
}

void HubFrame::readFrameLog()
{
	ctrlClient.goToEnd(true);
}

bool HubFrame::sendMessage(const string& msg, bool thirdPerson)
{
	if (isDHT)
	{
		MessageBox(CTSTRING(DHT_NO_CHAT), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
		return false;
	}
	if (!client) return false;
	client->hubMessage(msg, thirdPerson);
	return true;
}

bool HubFrame::processFrameCommand(const Commands::ParsedCommand& pc, Commands::Result& res)
{
	if (!Commands::checkArguments(pc, res.text))
	{
		res.what = Commands::RESULT_ERROR_MESSAGE;
		return true;
	}
	switch (pc.command)
	{
		case Commands::COMMAND_CLOSE:
			res.what = Commands::RESULT_NO_TEXT;
			PostMessage(WM_CLOSE);
			return true;
		case Commands::COMMAND_JOIN:
		{
			res.what = Commands::RESULT_NO_TEXT;
			string url = Util::formatDchubUrl(pc.args[1]);
			if (BOOLSETTING(JOIN_OPEN_NEW_WINDOW))
			{
				openHubWindow(url);
			}
			else
			{
				redirect = std::move(url);
				followRedirect();
			}
			return true;
		}
		case Commands::COMMAND_PASSWORD:
			res.what = Commands::RESULT_NO_TEXT;
			if (waitingForPassword)
			{
				if (client) client->password(pc.args[1], true);
				waitingForPassword = false;
			}
			return true;
		case Commands::COMMAND_SHOW_JOINS:
			res.what = Commands::RESULT_NO_TEXT;
			showJoins = !showJoins;
			addStatus(showJoins ? TSTRING(JOIN_SHOWING_ON) : TSTRING(JOIN_SHOWING_OFF));
			return true;
		case Commands::COMMAND_FAV_SHOW_JOINS:
			res.what = Commands::RESULT_NO_TEXT;
			showFavJoins = !showFavJoins;
			addStatus(showFavJoins ? TSTRING(FAV_JOIN_SHOWING_ON) : TSTRING(FAV_JOIN_SHOWING_OFF));
			return true;
		case Commands::COMMAND_TOGGLE_USER_LIST:
			res.what = Commands::RESULT_NO_TEXT;
			if (ctrlShowUsers)
			{
				showUsers = !showUsers;
				ctrlShowUsers.SetCheck(showUsers ? BST_CHECKED : BST_UNCHECKED);
			}
			return true;
		case Commands::COMMAND_USER_LIST_LOCATION:
			res.what = Commands::RESULT_NO_TEXT;
			if (showUsers) switchPanels();
			return true;
		case Commands::COMMAND_INFO_CONNECTION:
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
			res.text = STRING(IP) + ": " + ipAddress + '\n' + desc;
			res.what = Commands::isPublic(pc.args) ? Commands::RESULT_TEXT : Commands::RESULT_LOCAL_TEXT;
			return true;
		}
		case Commands::COMMAND_ADD_FAVORITE:
		{
			res.what = Commands::RESULT_NO_TEXT;
			AutoConnectType autoConnect;
			string param;
			if (pc.args.size() >= 2)
			{
				param = pc.args[1];
				Text::asciiMakeLower(param);
			}
			if (param == "a")
				autoConnect = SET;
			else if (param == "-a")
				autoConnect = UNSET;
			else
				autoConnect = DONT_CHANGE;
			addAsFavorite(autoConnect);
			return true;
		}
		case Commands::COMMAND_REMOVE_FAVORITE:
			res.what = Commands::RESULT_NO_TEXT;
			removeFavoriteHub();
			return true;
		case Commands::COMMAND_LAST_NICK:
			if (!lastUserName.empty())
				res.text = Text::fromT(lastUserName + getChatRefferingToNick() + _T(' '));
			if (pc.args.size() >= 2)
				res.text += pc.args[1];
			res.what = Commands::RESULT_TEXT;
			return true;
		case Commands::COMMAND_OPEN_LOG:
			res.what = Commands::RESULT_NO_TEXT;
			if (pc.args.size() < 2)
			{
				openFrameLog();
				return true;
			}
			if (stricmp(pc.args[1].c_str(), "status") == 0)
			{
				WinUtil::openLog(SETTING(LOG_FILE_STATUS), getFrameLogParams(), TSTRING(NO_LOG_FOR_STATUS));
				return true;
			}
			break;
		case Commands::COMMAND_GET_LIST:
		{
			if (pc.args.size() < 2)
			{
				res.what = Commands::RESULT_ERROR_MESSAGE;
				res.text = STRING(COMMAND_ARG_REQUIRED);
				return true;
			}
			res.what = Commands::RESULT_NO_TEXT;
			UserInfo* ui = findUserByNick(Text::toT(pc.args[1]));
			if (ui) ui->getList();
			return true;
		}
		case Commands::COMMAND_PRIVATE_MESSAGE:
		{
			res.what = Commands::RESULT_NO_TEXT;
			if (!client) return true;
			const OnlineUserPtr ou = client->findUser(pc.args[1]);
			if (ou)
			{
				string message;
				if (pc.args.size() >= 3) message = pc.args[2];
				PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick(), message);
			}
			return true;
		}
	}
	return BaseChatFrame::processFrameCommand(pc, res);
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
	entry.setKeyPrint(client->getKeyPrint());
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

	if (tabMenu)
		tabMenu.ClearMenu();
	else
		tabMenu.CreatePopupMenu();
	if (!isDHT)
	{
		auto fm = FavoriteManager::getInstance();
		const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(serverUrl);
		if (fhe)
		{
			isFav = true;
			isAutoConnect = fhe->getAutoConnect();
		}
		fm->releaseFavoriteHubEntryPtr(fhe);
		if (BOOLSETTING(LOG_MAIN_CHAT))
		{
			tabMenu.AppendMenu(MF_STRING, IDC_OPEN_HUB_LOG, CTSTRING(OPEN_HUB_LOG), g_iconBitmaps.getBitmap(IconBitmaps::LOGS, 0));
			tabMenu.AppendMenu(MF_SEPARATOR);
		}
		if (isFav)
		{
			tabMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE_HUB, 0));
			if (isAutoConnect)
				tabMenu.AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_OFF));
			else
				tabMenu.AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_ON));
			tabMenu.AppendMenu(MF_STRING, IDC_EDIT_HUB_PROP, CTSTRING(PROPERTIES), g_iconBitmaps.getBitmap(IconBitmaps::PROPERTIES, 0));
		}
		else
		{
			tabMenu.AppendMenu(MF_STRING, IDC_ADD_AS_FAVORITE, CTSTRING(ADD_TO_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::ADD_HUB, 0));
		}
		tabMenu.AppendMenu(MF_STRING, IDC_RECONNECT, CTSTRING(MENU_RECONNECT), g_iconBitmaps.getBitmap(IconBitmaps::RECONNECT, 0));
		if (isConnected())
			tabMenu.AppendMenu(MF_STRING, ID_DISCONNECT, CTSTRING(DISCONNECT), g_iconBitmaps.getBitmap(IconBitmaps::DISCONNECT, 0));
		WinUtil::g_copyHubMenu.EnableMenuItem(IDC_COPY_IP, client ? MFS_ENABLED : MFS_GRAYED);
		tabMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::g_copyHubMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
		tabMenu.AppendMenu(MF_SEPARATOR);
	}
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
		case IDC_COPY_HUB_IP:
			if (client) sCopy = client->getIpAsString();
			break;
	}
	
	if (!sCopy.empty())
	{
		WinUtil::setClipboard(sCopy);
	}
	return 0;
}

void HubFrame::addStatus(const tstring& line, const bool inChat /*= true*/, const bool history /*= true*/, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextSystem*/)
{
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

		addStatus(TSTRING(CONNECTED));
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
		dcdebug("updateUserJoin: user=%s, hub=%s (not connected)\n", ou->getIdentity().getNick().c_str(), ou->getClientBase()->getHubUrl().c_str());
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
							addLine(from, myMessage, msg->thirdPerson, Text::toT(msg->format()), UINT_MAX, Colors::g_ChatTextGeneral);
#ifdef BL_FEATURE_IP_DATABASE
							auto& user = msg->from->getUser();
							user->incMessageCount();
							//client->incMessagesCount();
							shouldUpdateStats |= ctrlUsers.updateUser(msg->from, 1<<COLUMN_MESSAGES, isDHT || isConnected());
#endif
							// msg->from->getUser()->flushRatio();
						}
						else
						{
							BaseChatFrame::addLine(Text::toT(msg->text), UINT_MAX, Colors::g_ChatTextPrivate);
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
						addStatus(Text::toT(status.str), status.isInChat, true, status.isSystem ? Colors::g_ChatTextSystem : Colors::g_ChatTextServer);
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
								client->fire(ClientListener::NickError(), ClientListener::BadPassword);
							}
							else
							{
								showingPasswordDlg = true;
								string nick = client->getMyNick();
								LineDlg linePwd;
								linePwd.title = getHubTitle();
								linePwd.description = Text::toT(STRING_F(ENTER_PASSWORD_FOR_NICK, nick));
								linePwd.password = linePwd.checkBox = true;
								linePwd.icon = IconBitmaps::KEY;
								if (linePwd.DoModal(m_hWnd) == IDOK)
								{
									const string pwd = Text::fromT(linePwd.line);
									client->password(pwd, true);
									waitingForPassword = false;
									if (linePwd.checked)
										FavoriteManager::getInstance()->setFavoriteHubPassword(serverUrl, nick, pwd, true);
								}
								else
								{
									client->disconnect(false);
									client->fire(ClientListener::NickError(), ClientListener::BadPassword);
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
						isPrivateFrameOk = PrivateFrame::gotMessage(from, to, replyTo, text, UINT_MAX, client->getHubUrl(), myPM, pm->thirdPerson);
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
					BaseChatFrame::addLine(Text::toT(task.str), 0, Colors::g_ChatTextSystem);
					if (BOOLSETTING(LOG_MAIN_CHAT))
					{
						StringMap params;
						params["message"] = task.str;
						params["hubURL"] = baseClient->getHubUrl();
						LOG(CHAT, params);
					}
				}
				break;

				case SETTINGS_LOADED:
					updateDisabledChatSettings();
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
		int w[STATUS_LAST];
		w[STATUS_TEXT] = sr.right - tmp - 55 - hubPic - cipherLen;
		w[STATUS_CIPHER_SUITE] = w[STATUS_TEXT] + cipherLen;
		w[STATUS_USERS] = w[STATUS_CIPHER_SUITE] + (tmp - 30) / 2;
		w[STATUS_SHARED] = w[STATUS_CIPHER_SUITE] + (tmp - 64);
		w[STATUS_SIZE_PER_USER] = w[STATUS_SHARED] + 100;
		w[STATUS_HUB_ICON] = w[STATUS_SIZE_PER_USER] + 18 + hubPic;
		ctrlStatus.SetParts(STATUS_LAST, w);

		ctrlLastLinesToolTip.SetMaxTipWidth(max(w[STATUS_TEXT], 400));

		// Strange, can't get the correct width of the last field...
		ctrlStatus.GetRect(4, sr);

		// Hub mode icon: active, passive, offline
		if (ctrlModeIcon)
		{
			sr.left = sr.right + 2;
			sr.right = sr.left + hubIconSize;
			ctrlModeIcon.MoveWindow(sr);
		}

		// Switch panels button, same as /switch command
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
	}
	if (msgPanel)
	{
		const int h = getInputBoxHeight();
		int panelHeight = msgPanel->initialized ? h + 6 : 0;

		CRect rc = rect;
		rc.bottom -= panelHeight;

		setSplitterPanes();
		SetSplitterRect(rc);

		if (msgPanel->initialized)
		{
			int buttonPanelWidth = msgPanel->getPanelWidth();
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
	else
	{
		setSplitterPanes();
		SetSplitterRect(&rect);
	}
	if (showUsers && ctrlUsers)
		ctrlUsers.updateLayout();
}

void HubFrame::setSplitterPanes()
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (ctrlUsers && ctrlClient)
	{
		if (swapPanels && !disableChat)
			SetSplitterPanes(ctrlUsers.m_hWnd, ctrlClient.m_hWnd, false);
		else
			SetSplitterPanes(ctrlClient.m_hWnd, ctrlUsers.m_hWnd, false);
	}
	int newMode = SPLIT_PANE_NONE;
	if (disableChat)
		newMode = SPLIT_PANE_RIGHT;
	else if (!showUsers)
		newMode = swapPanels ? SPLIT_PANE_RIGHT : SPLIT_PANE_LEFT;
	if (GetSinglePaneMode() != newMode)
		SetSinglePaneMode(newMode);
}

void HubFrame::updateDisabledChatSettings()
{
	if (!client) return;
	bool flag = client->getSuppressChatAndPM();
	if (flag == disableChat) return;
	disableChat = flag;
	if (disableChat)
	{
		showUsers = true;
		swapPanels = false;
		ctrlShowUsers.SetCheck(BST_CHECKED);
		ctrlShowUsers.EnableWindow(FALSE);
	}
	else
		ctrlShowUsers.EnableWindow(TRUE);
	setChatDisabled(disableChat);
	if (msgPanel)
		UpdateLayout();
	else
		setSplitterPanes();
}

void HubFrame::updateModeIcon()
{
	if (!ctrlModeIcon) return;
	ResourceManager::Strings stateText;
	int icon;
	if (isDHT)
	{
		switch (currentDHTState)
		{
			case dht::DHT::STATE_INITIALIZING:
			case dht::DHT::STATE_BOOTSTRAP:
				stateText = ResourceManager::DHT_STATE_INITIALIZING;
				icon = IconBitmaps::HUB_MODE_PASSIVE;
				break;
			case dht::DHT::STATE_ACTIVE:
				stateText = ResourceManager::DHT_STATE_ACTIVE;
				icon = IconBitmaps::HUB_MODE_ACTIVE;
				break;
			case dht::DHT::STATE_FAILED:
				stateText = ResourceManager::DHT_STATE_FAILED;
				icon = IconBitmaps::HUB_MODE_PASSIVE;
				break;
			default:
				stateText = ResourceManager::DHT_STATE_DISABLED;
				icon = IconBitmaps::HUB_MODE_OFFLINE;
		}
		const tstring& s = TSTRING_I(stateText);
		ctrlModeIcon.SetIcon(g_iconBitmaps.getIcon(icon, 0));
		if (tooltip)
			tooltip.AddTool(ctrlModeIcon, CTSTRING_F(DHT_STATE_FMT, s));
		return;
	}
	if (client && client->isReady())
	{
		if (client->isActive())
		{
			stateText = ResourceManager::ACTIVE_NOTICE;
			icon = IconBitmaps::HUB_MODE_ACTIVE;
		}
		else
		{
			stateText = ResourceManager::PASSIVE_NOTICE;
			icon = IconBitmaps::HUB_MODE_PASSIVE;
		}
	}
	else
	{
		stateText = ResourceManager::UNKNOWN_MODE_NOTICE;
		icon = IconBitmaps::HUB_MODE_OFFLINE;
	}
	ctrlModeIcon.SetIcon(g_iconBitmaps.getIcon(icon, 0));
	if (tooltip)
		tooltip.AddTool(ctrlModeIcon, stateText);
}

void HubFrame::storeColumnsInfo()
{
	if (!uiInitialized)
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
			static_cast<dht::DHT*>(baseClient.get())->removeListener(this);
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
		if (uiInitialized)
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
		if (end == start + 1 || end <= start)
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
					QueueManager::getInstance()->addList(HintedUser(ui->getUser(), baseClient->getHubUrl()), QueueItem::FLAG_CLIENT_VIEW);
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
						if (client && !ui->isMe()) ui->pm(client->getHubUrl());
						break;
					case 3:
						if (!ui->isMe()) ui->getList();
						break;
					case 4:
						if (!ui->isMe()) ui->matchQueue();
						break;
					case 5:
						if (!ui->isMe()) ui->grantSlotPeriod(baseClient->getHubUrl(), 600);
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

void HubFrame::addLine(const Identity& from, bool myMessage, bool thirdPerson, const tstring& line, unsigned maxSmiles, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	string extra;
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
			params["extra"] = extra;
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
				reinitUserMenu(ou, baseClient->getHubUrl());
			}
			else
			{
				ctx |= UserCommand::CONTEXT_FLAG_MULTIPLE;
				reinitUserMenu(nullptr, baseClient->getHubUrl());
			}
		}

		appendHubAndUsersItems(*userMenu, false);
		appendUcMenu(*userMenu, ctx, baseClient->getHubUrl());
		if (baseClient->getType() == ClientBase::TYPE_NMDC)
		{
			WinUtil::appendSeparator(*userMenu);
			userMenu->AppendMenu(MF_STRING, IDC_REFRESH, CTSTRING(REFRESH_USER_LIST));
		}

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
		reinitUserMenu(ui ? ui->getOnlineUser() : nullptr, baseClient->getHubUrl());
		
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
	if (isDHT)
		opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::DHT, 0);
	else
	{
		opt->icons[0] = g_iconBitmaps.getIcon(IconBitmaps::HUB_ONLINE, 0);
		opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::HUB_OFFLINE, 0);
	}
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
	if (uiInitialized)
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
	ASSERT_MAIN_THREAD();
	frames.erase(serverUrl);
	if (!redirectUrl.empty())
	{
		frames.insert(make_pair(redirectUrl, this));
		serverUrl = redirectUrl;
	}
}

HubFrame* HubFrame::findFrameByID(uint64_t id)
{
	for (const auto& i : frames)
	{
		HubFrame* frame = i.second;
		if (frame->frameId == id) return frame;
	}
	return nullptr;
}

void HubFrame::followRedirect()
{
	if (client && !redirect.empty())
	{
		if (ClientManager::isConnected(redirect))
		{
			addStatus(TSTRING(REDIRECT_ALREADY_CONNECTED), true, false, Colors::g_ChatTextServer);
			LogManager::message("HubFrame::onFollow " + baseClient->getHubUrl() + " -> " + redirect + " ALREADY CONNECTED", false);
			return;
		}
		if (originalServerUrl.empty())
			originalServerUrl = serverUrl;
		removeFrame(redirect);
		// the client is dead, long live the client!
		client->setAutoReconnect(false);
		ClientManager::getInstance()->putClient(baseClient);
		baseClient.reset();
		client = nullptr;
		clearTaskAndUserList();
		baseClient = ClientManager::getInstance()->getClient(redirect);
		client = static_cast<Client*>(baseClient.get());
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
	ASSERT_MAIN_THREAD();
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
	ASSERT_MAIN_THREAD();
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
	ASSERT_MAIN_THREAD();
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
		ClientManager::getInstance()->prepareClose();
	}
	{
		ASSERT_MAIN_THREAD();
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

void HubFrame::prepareNonMaximized()
{
	ASSERT_MAIN_THREAD();
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		HubFrame* frame = i->second;
		if (!frame->uiInitialized)
		{
			frame->initUI();
			frame->ctrlClient.restoreChatCache();
		}
	}
}

void HubFrame::updateAllTitles()
{
	ASSERT_MAIN_THREAD();
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
		i->second->hubUpdateCount++;
}

void HubFrame::on(FavoriteManagerListener::UserAdded, const FavoriteUser& user) noexcept
{
	if (isClosedOrShutdown())
		return;
	++asyncUpdate;
	resortForFavsFirst();
}

void HubFrame::on(FavoriteManagerListener::UserRemoved, const FavoriteUser& user) noexcept
{
	if (isClosedOrShutdown())
		return;
	++asyncUpdate;
}

void HubFrame::on(FavoriteManagerListener::UserStatusChanged, const UserPtr& user) noexcept
{
	++asyncUpdate;
}

void HubFrame::resortForFavsFirst(bool justDoIt /* = false */)
{
	if (justDoIt || BOOLSETTING(SORT_FAVUSERS_FIRST))
		ctrlUsers.setSortFlag();
}

void HubFrame::on(UserManagerListener::IgnoreListChanged) noexcept
{
	ctrlUsers.onIgnoreListChanged();
	++asyncUpdate;
}

void HubFrame::on(UserManagerListener::IgnoreListCleared) noexcept
{
	ctrlUsers.onIgnoreListCleared();
	++asyncUpdate;
}

void HubFrame::on(UserManagerListener::ReservedSlotChanged, const UserPtr& user) noexcept
{
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
	string convertedLine = line;
	Util::convertToDos(convertedLine);
	addTask(ADD_STATUS_LINE, new StatusTask(convertedLine, !BOOLSETTING(FILTER_MESSAGES) || !(statusFlags & ClientListener::FLAG_IS_SPAM), false));
}

void HubFrame::on(ClientListener::SettingsLoaded, const Client*) noexcept
{
	addTask(SETTINGS_LOADED, new Task);
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
		addTask(ADD_STATUS_LINE, new StatusTask(STRING(REDIRECT_ALREADY_CONNECTED), true, true));
		doubleRedir = true;
	}
	
	redirect = redirAddr;
	if (BOOLSETTING(AUTO_FOLLOW) || doubleRedir)
		PostMessage(WM_COMMAND, IDC_FOLLOW, 0);
	else
		addTask(ADD_STATUS_LINE, new StatusTask(STRING_F(PRESS_FOLLOW_FMT, redirAddr), true, true));
}

void HubFrame::on(ClientListener::ClientFailed, const Client* c, const string& line) noexcept
{
	if (!isClosedOrShutdown())
		addTask(ADD_STATUS_LINE, new StatusTask(line, true, true));
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
	addTask(ADD_STATUS_LINE, new StatusTask(STRING(HUB_FULL), true, false));
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
					status = new StatusTask(STRING_F(NICK_ERROR_REJECTED_AUTO, oldNick % nick), true, false);
				else
					status = new StatusTask(STRING_F(NICK_ERROR_TAKEN_AUTO, oldNick % nick), true, false);
			}
		}
	}
	if (!status)
		switch (nickError)
		{
			case ClientListener::Rejected:
				status = new StatusTask(STRING_F(NICK_ERROR_REJECTED, client->getMyNick()), true, false);
				break;
			case ClientListener::Taken:
				status = new StatusTask(STRING_F(NICK_ERROR_TAKEN, client->getMyNick()), true, false);
				break;
			default:
				status = new StatusTask(STRING_F(NICK_ERROR_BAD_PASSWORD, client->getMyNick()), true, false);
		}
	addTask(ADD_STATUS_LINE, status);
}

void HubFrame::on(ClientListener::CheatMessage, const string& line) noexcept
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	addTask(CHEATING_USER, new StatusTask(line, true, true));
}

void HubFrame::on(ClientListener::UserReport, const ClientBase*, const string& report) noexcept
{
	if (isClosedOrShutdown())
		return;
	addTask(USER_REPORT, new StatusTask(report, true, true));
}

void HubFrame::on(ClientListener::HubInfoMessage, ClientListener::HubInfoCode code, const Client* client, const string& line) noexcept
{
	switch (code)
	{
		case ClientListener::LoggedIn:
			addTask(ADD_STATUS_LINE, new StatusTask(STRING_F(YOU_ARE_OP_MSG, baseClient->getHubUrl()), true, false));
			break;

		case ClientListener::HubTopic:
			addTask(ADD_STATUS_LINE, new StatusTask(STRING(HUB_TOPIC) + " " + line, true, false));
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
		selectCID(getSelectedUser()->getUser()->getCID());
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
		const string s = "!banip " + Text::fromT(ChatCtrl::g_sSelectedIP);
		sendMessage(s);
	}
	return 0;
}

LRESULT HubFrame::onUnBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		const string s = "!unban " + Text::fromT(ChatCtrl::g_sSelectedIP);
		sendMessage(s);
	}
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

void HubFrame::addNickToChat()
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
}

void HubFrame::openUserLog()
{
	UserInfo* ui = nullptr;
	if (getSelectedUser())
	{
		ui = ctrlUsers.findUser(getSelectedUser());
		if (!ui) return;
	}

	StringMap params = getFrameLogParams();
	
	params["userNI"] = ui->getNick();
	params["userCID"] = ui->getUser()->getCID().toBase32();
	
	WinUtil::openLog(SETTING(LOG_FILE_PRIVATE_CHAT), params, TSTRING(NO_LOG_FOR_USER));
}

void HubFrame::addDupUsersToSummaryMenu(const ClientManager::UserParams& param, vector<UserInfoGuiTraits::DetailsItem>& detailsItems, UINT& idc)
{
	detailsItems.clear();
	{
		ASSERT_MAIN_THREAD();
		for (auto f = frames.cbegin(); f != frames.cend(); ++f)
		{
			const auto& frame = f->second;
			if (!frame->isClosedOrShutdown())
				frame->ctrlUsers.getDupUsers(param, frame->getHubTitle(), frame->baseClient->getHubUrl(), idc, detailsItems);
		}
	}
	for (const auto& item : detailsItems)
	{
		unsigned flags = item.flags;
		if (flags & MF_SEPARATOR)
		{
			userSummaryMenu.AppendMenu(MF_SEPARATOR);
			flags &= ~MF_SEPARATOR;
		}
		userSummaryMenu.AppendMenu(MF_STRING | flags, item.id, item.text.c_str());
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

void HubFrame::selectCID(const CID& cid)
{
	if (ctrlUsers.selectCID(cid))
		ctrlUsers.getUserList().SetFocus();
}

BOOL HubFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::g_tabCtrl->isActive(m_hWnd)) return FALSE;
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	if (isFindDialogMessage(pMsg)) return TRUE;
	if (WinUtil::isCtrl()) return FALSE;
	return IsDialogMessage(pMsg);
}

CFrameWndClassInfo& HubFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("HubFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::HUB_ONLINE, 0);

	return wc;
}
