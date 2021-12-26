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

#include "PrivateFrame.h"
#include "WinUtil.h"
#include "MainFrm.h"
#include "../client/ClientManager.h"
#include "../client/UploadManager.h"
#include "../client/ParamExpander.h"
#include "../client/ConnectionManager.h"
#include <boost/algorithm/string.hpp>

static const size_t MAX_PM_FRAMES = 100;
static const unsigned AWAY_MSG_COOLDOWN_TIME = 300000; // 5 minutes

HIconWrapper PrivateFrame::frameIconOn(IDR_PRIVATE);
HIconWrapper PrivateFrame::frameIconOff(IDR_PRIVATE_OFF);

PrivateFrame::FrameMap PrivateFrame::frames;

PrivateFrame::PrivateFrame(const HintedUser& replyTo, const string& myNick) : replyTo(replyTo),
	replyToRealName(Text::toT(replyTo.user->getLastNick())),
	created(false), isOffline(false), awayMsgSendTime(0),
	ctrlChatContainer(WC_EDIT, this, PM_MESSAGE_MAP)
{
	ctrlStatusCache.resize(1);
	ctrlClient.setHubParam(replyTo.hint, myNick);
}

PrivateFrame::~PrivateFrame()
{
}

void PrivateFrame::doDestroyFrame()
{
	destroyUserMenu();
	destroyMessagePanel(true);
}

StringMap PrivateFrame::getFrameLogParams() const
{
	StringMap params;
	params["hubNI"] = ClientManager::getInstance()->getHubName(replyTo.hint);
	params["hubURL"] = replyTo.hint;
	params["userCID"] = replyTo.user->getCID().toBase32();
	params["userNI"] = Text::fromT(replyToRealName);
	params["myCID"] = ClientManager::getMyCID().toBase32();
	return params;
}

LRESULT PrivateFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	BaseChatFrame::onCreate(m_hWnd, rcDefault);

	m_hAccel = LoadAccelerators(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_PRIVATE));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	PostMessage(WM_SPEAKER, PM_USER_UPDATED);
	created = true;
	ClientManager::getInstance()->addListener(this);
	ConnectionManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	UserManager::getInstance()->setPMOpen(replyTo.user, true);

	bHandled = FALSE;
	return 1;
}

LRESULT PrivateFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);

	bHandled = FALSE;
	return 0;
}

static void removeLineBreaks(string& text)
{
	std::replace(text.begin(), text.end(), '\r', ' ');
	std::replace(text.begin(), text.end(), '\n', ' ');
	boost::replace_all(text, "  ", " ");
}

bool PrivateFrame::gotMessage(const Identity& from, const Identity& to, const Identity& replyTo,
                              const tstring& message, unsigned maxEmoticons, const string& hubHint, bool myMessage,
                              bool thirdPerson, bool notOpenNewWindow /*= false*/)
{
	const auto& id = myMessage ? to : replyTo;
	const auto& myId = myMessage ? replyTo : to;
	
	PrivateFrame* p;
	const auto i = frames.find(id.getUser());
	if (i == frames.end())
	{
		if (notOpenNewWindow || frames.size() > MAX_PM_FRAMES)
		{
			string oneLineMessage = Text::fromT(message);
			removeLineBreaks(oneLineMessage);
			const string key = id.getUser()->getLastNick() + " + " + hubHint;
			LogManager::message("Lock > 100 open private message windows! Hub: " + key + " Message: " + oneLineMessage);
			return false;
		}
		// TODO - Add antispam!
		p = new PrivateFrame(HintedUser(id.getUser(), hubHint), myId.getNick());
		frames.insert(make_pair(id.getUser(), p));
		p->addLine(from, myMessage, thirdPerson, message, maxEmoticons);
		if (BOOLSETTING(POPUP_PM_PREVIEW))
			SHOW_POPUP_EXT(POPUP_ON_NEW_PM, Text::toT(id.getNick()), message, 250, TSTRING(PRIVATE_MESSAGE));
		PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP_OPEN);
	}
	else
	{
		if (!myMessage)
		{
			if (BOOLSETTING(POPUP_PM_PREVIEW))
				SHOW_POPUP_EXT(POPUP_ON_PM, Text::toT(id.getNick()), message, 250, TSTRING(PRIVATE_MESSAGE));
			PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP);
		}
		// Add block spam???
		p = i->second;
		p->addLine(from, myMessage, thirdPerson, message, maxEmoticons);
	}
	if (!myMessage && Util::getAway() && !replyTo.isBotOrHub())
	{
		uint64_t tick = GET_TICK();
		if (tick > p->awayMsgSendTime)
		{
			string awayMsg;
			auto fm = FavoriteManager::getInstance();
			const FavoriteHubEntry *fhe = fm->getFavoriteHubEntryPtr(Util::toString(ClientManager::getHubs(id.getUser()->getCID(), hubHint)));
			if (fhe)
			{
				awayMsg = fhe->getAwayMsg();
				fm->releaseFavoriteHubEntryPtr(fhe);
			}

			StringMap params;
			from.getParams(params, "user", false);
			awayMsg = Util::getAwayMessage(awayMsg, params);

			p->sendMessage(Text::toT(awayMsg));
			p->awayMsgSendTime = tick + AWAY_MSG_COOLDOWN_TIME;
		}
	}
	return true;
}

void PrivateFrame::openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick, const tstring& msg)
{
	if (myNick.empty())
	{
		if (ou)
		{
			myNick = ou->getClientBase()->getMyNick();
		}
		else if (!replyTo.hint.empty())
		{
			ClientManager::LockInstanceClients l;
			const auto& clients = l.getData();
			auto i = clients.find(replyTo.hint);
			if (i != clients.end())
				myNick = i->second->getMyNick();
		}
		if (myNick.empty())
			myNick = SETTING(NICK);
	}

	PrivateFrame* p = nullptr;
	const auto i = frames.find(replyTo.user);
	if (i == frames.end())
	{
		if (frames.size() > MAX_PM_FRAMES)
		{
			LogManager::message("Lock > 100 open private message windows! Hub+nick: " + replyTo.hint + " " + myNick + " Message: " + Text::fromT(msg));
			return;
		}

		if (ou)
			replyTo.user->setLastNick(ou->getIdentity().getNick());
		p = new PrivateFrame(replyTo, myNick);
		frames.insert(make_pair(replyTo.user, p));
		p->Create(WinUtil::g_mdiClient);
	}
	else
	{
		p = i->second;
		if (!replyTo.hint.empty())
			p->selectHub(replyTo.hint);
		if (::IsIconic(p->m_hWnd))
			::ShowWindow(p->m_hWnd, SW_RESTORE);
			
		p->MDIActivate(p->m_hWnd);
	}
	if (!msg.empty())
	{
		p->sendMessage(msg);
	}
}

LRESULT PrivateFrame::onKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!processHotKey(wParam))
		bHandled = FALSE;
	return 0;
}

LRESULT PrivateFrame::onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	HWND focus = GetFocus();
	bHandled = FALSE;
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
		
		string::size_type start = x.find_last_of(_T(" <\t\r\n,;"), c);
		
		if (start == string::npos)
			start = 0;
		else
			start++;
			
		string::size_type end = x.find_first_of(_T(" >\t\r\n,;"), start + 1);
		
		if (end == string::npos)
			end = x.length();
		else if (end == start + 1)
			return 0;
			
		bHandled = TRUE;
		appendNickToChat(x.substr(start, end - start));
	}
	return 0;
}

void PrivateFrame::processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText)
{
	if (replyTo.user->isOnline())
	{
		if (!sendMessage(fullMessageText))
			resetInputMessageText = false;
		awayMsgSendTime = GET_TICK() + AWAY_MSG_COOLDOWN_TIME;
	}
	else
	{
		setStatusText(0, CTSTRING(USER_WENT_OFFLINE));
		resetInputMessageText = false;
	}
}

void PrivateFrame::processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText)
{
	if (stricmp(cmd.c_str(), _T("grant")) == 0)
	{
		UploadManager::getInstance()->reserveSlot(replyTo, 600);
		addStatus(TSTRING(SLOT_GRANTED));
	}
	else if (stricmp(cmd.c_str(), _T("close")) == 0) // TODO
	{
		PostMessage(WM_CLOSE);
	}
	else if (stricmp(cmd.c_str(), _T("favorite")) == 0 || stricmp(cmd.c_str(), _T("fav")) == 0)
	{
		FavoriteManager::getInstance()->addFavoriteUser(getUser());
		addStatus(TSTRING(FAVORITE_USER_ADDED));
	}
	else if (stricmp(cmd.c_str(), _T("getlist")) == 0 || stricmp(cmd.c_str(), _T("gl")) == 0)
	{
		BOOL unused;
		clearUserMenu();
		reinitUserMenu(replyTo.user, replyTo.hint);
		onGetList(0, 0, 0, unused);
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		openFrameLog();
	}
	else if (stricmp(cmd.c_str(), _T("ccpm")) == 0)
	{
		BOOL unused;
		onCCPM(0, 0, 0, unused);
	}
}

bool PrivateFrame::sendMessage(const tstring& msg, bool thirdPerson /*= false*/)
{
	int ccpm = msgPanel->getCCPMState();
	if (ccpm == MessagePanel::CCPM_STATE_CONNECTING)
		return false;
	if (ccpm == MessagePanel::CCPM_STATE_CONNECTED)
		return ConnectionManager::getInstance()->sendCCPMMessage(replyTo, Text::fromT(msg), thirdPerson, false);
	ClientManager::privateMessage(replyTo, Text::fromT(msg), thirdPerson, false);
	return true;
}

LRESULT PrivateFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		closed = true;
		ClientManager::getInstance()->removeListener(this);
		ConnectionManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		UserManager::getInstance()->setPMOpen(replyTo.user, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		frames.erase(replyTo.user);
		bHandled = FALSE;
		return 0;
	}
}

void PrivateFrame::readFrameLog()
{
	const auto linesCount = SETTING(PM_LOG_LINES);
	if (linesCount)
	{
		const string path = Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatParams(SETTING(LOG_FILE_PRIVATE_CHAT), getFrameLogParams(), true));
		appendLogToChat(path, linesCount);
	}
}

void PrivateFrame::addLine(const Identity& from, const bool myMessage, const bool thirdPerson, const tstring& line, unsigned maxEmoticons, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	if (!created)
	{
		if (BOOLSETTING(POPUNDER_PM))
			WinUtil::hiddenCreateEx(this);
		else
			Create(WinUtil::g_mdiClient);
	}
	
	string extra;
	BaseChatFrame::addLine(from, myMessage, thirdPerson, line, maxEmoticons, cf, extra);

	if (BOOLSETTING(LOG_PRIVATE_CHAT))
	{
		StringMap params = getFrameLogParams();
		params["message"] = ChatMessage::formatNick(from.getNick(), thirdPerson) + Text::fromT(line);
		if (!extra.empty())
			params["extra"] = " | " + extra;
		LOG(PM, params);
	}

	addStatus(TSTRING(LAST_CHANGE), false, false);
	
	if (BOOLSETTING(BOLD_PM))
		setDirty();
}

LRESULT PrivateFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	OMenu tabMenu;
	tabMenu.CreatePopupMenu();
	clearUserMenu();

	tabMenu.InsertSeparatorFirst(replyToRealName);
	reinitUserMenu(replyTo.user, replyTo.hint);
	appendAndActivateUserItems(tabMenu);
	appendUcMenu(tabMenu, UserCommand::CONTEXT_USER, ClientManager::getHubs(replyTo.user->getCID(), replyTo.hint));
	WinUtil::appendSeparator(tabMenu);
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_PM, CTSTRING(MENU_CLOSE_ALL_OFFLINE_PM));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_PM, CTSTRING(MENU_CLOSE_ALL_PM));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));

	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);

	cleanUcMenu(tabMenu);
	WinUtil::unlinkStaticMenus(tabMenu);
	return TRUE;
}

LRESULT PrivateFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = frameIconOn;
	opt->icons[1] = frameIconOff;
	opt->isHub = true;
	return TRUE;
}

void PrivateFrame::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
	StringMap ucParams = ucLineParams;
	ClientManager::userCommand(replyTo, uc, ucParams, true);
	// TODO тут ucParams не используетс€ позже
}

void PrivateFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	if (ClientManager::isStartup())
		return;
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
		return;
	if (ctrlMessage)
	{
		RECT rect;
		GetClientRect(&rect);
		// position bars and offset their dimensions
		UpdateBarsPosition(rect, bResizeBars);
		if (ctrlStatus && ctrlLastLinesToolTip)
		{
			if (ctrlStatus.IsWindow() && ctrlLastLinesToolTip.IsWindow())
			{
				CRect sr;
				int w[1];
				ctrlStatus.GetClientRect(sr);
				w[0] = sr.right - 16;
				ctrlStatus.SetParts(1, w);
				ctrlLastLinesToolTip.SetMaxTipWidth(max(w[0], 400));
			}
		}
		const int h = getInputBoxHeight();
		int panelHeight = h + 6;

		CRect rc = rect;
		rc.bottom -= panelHeight;
		if (ctrlClient.IsWindow())
			ctrlClient.MoveWindow(rc);

		const int buttonPanelWidth = msgPanel ? msgPanel->getPanelWidth() : 0;

		rc = rect;
		rc.left += 2;
		rc.bottom -= 4;
		rc.top = rc.bottom - h;
		rc.right -= buttonPanelWidth;

		if (ctrlMessage)
			ctrlMessage.MoveWindow(rc);

		rc.left = rc.right;
		rc.right += buttonPanelWidth;
		rc.bottom -= 1;

		if (msgPanel)
		{
			rc.top++;
			msgPanel->updatePanel(rc);
		}
	}
}

void PrivateFrame::updateHubList()
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (!replyTo.user)
		return;
	OnlineUserList lst;
	ClientManager::getOnlineUsers(replyTo.user->getCID(), lst);

#if 0 // FIXME
	bool banIcon = false;
	Flags::MaskType flags;
	int ul;
	if (FavoriteManager::getInstance()->getFavUserParam(replyTo, flags, ul))
		banIcon = FavoriteManager::hasUploadBan(ul) || FavoriteManager::hasIgnorePM(flags);
#endif

	tstring fullUserName;
	hubList.clear();
	if (!lst.empty())
	{
#if 0 // FIXME		
		if (banIcon)
			setCustomIcon(WinUtil::g_banIconOnline);
		else
			unsetIconState();
#endif
		setDisconnected(false);
		bool found = false;
		for (const auto& ou : lst)
		{
			const auto& client = ou->getClientBase();
			tstring hubName = Text::toT(client->getHubName());
			hubList.emplace_back(client->getHubUrl(), hubName);
			if (client->getHubUrl() == replyTo.hint)
			{
				replyToRealName = Text::toT(ou->getIdentity().getNick());
				lastHubName = hubName;
				found = true;
			}
		}
		if (!found)
		{
			const auto& ou = lst.front();
			const auto& client = ou->getClientBase();
			replyToRealName = Text::toT(ou->getIdentity().getNick());
			replyTo.hint = client->getHubUrl();
			lastHubName = Text::toT(client->getHubName());
		}
		if (replyToRealName.empty())
			replyToRealName = Text::toT(replyTo.user->getLastNick());
		fullUserName = replyToRealName;
		fullUserName += _T(" - ");
		fullUserName += lastHubName;
		if (isOffline)
			addStatus(TSTRING(USER_WENT_ONLINE) + _T(" [") + fullUserName + _T("]"));
		ctrlClient.setHubHint(replyTo.hint);
		isOffline = false;
	}
	else
	{
		isOffline = true;

#if 0 // FIXME
		if (banIcon)
			setCustomIcon(WinUtil::g_banIconOffline);
		else
			setIconState();
#endif
		setDisconnected(true);
		fullUserName = replyToRealName;
		fullUserName += _T(" - ");
		tstring hubName = lastHubName.empty() ? Text::toT(replyTo.hint) : lastHubName;
		if (hubName.empty())
			hubName = CTSTRING(OFFLINE);
		fullUserName += hubName;
		addStatus(TSTRING(USER_WENT_OFFLINE) + _T(" [") + fullUserName + _T("]"));
	}
	if (msgPanel)
	{
		CButton& button = msgPanel->getButton(MessagePanel::BUTTON_SELECT_HUB);
		button.EnableWindow(hubList.size() > 1);
	}
	SetWindowText(fullUserName.c_str());
}

void PrivateFrame::selectHub(const string& url)
{
	if (replyTo.hint == url) return;
	bool found = false;
	for (const auto& hub : hubList)
		if (hub.first == url)
		{
			found = true;
			break;
		}
	if (found)
	{
		replyTo.hint = url;
		updateHubList();
	}
}

void PrivateFrame::updateCCPM(bool connected)
{
	if (connected)
	{
		const tstring& text = TSTRING(CCPM_CONNECTED);
		addSystemMessage(text, Colors::g_ChatTextSystem);
		addStatus(text, false);
		if (msgPanel)
		{
			if (!msgPanel->showCCPMButton)
			{
				msgPanel->showCCPMButton = true;
				UpdateLayout();
			}
			msgPanel->setCCPMState(MessagePanel::CCPM_STATE_CONNECTED);
		}
	}
	else
	{
		const tstring& text = TSTRING(CCPM_DISCONNECTED);
		addSystemMessage(text, Colors::g_ChatTextSystem);
		addStatus(text, false);
		if (msgPanel)
			msgPanel->setCCPMState(MessagePanel::CCPM_STATE_DISCONNECTED);
	}
}

LRESULT PrivateFrame::onCCPM(WORD, WORD, HWND, BOOL&)
{
	auto cm = ConnectionManager::getInstance();
	int state = cm->getCCPMState(replyTo.user->getCID());
	if (state == ConnectionManager::CCPM_STATE_CONNECTING)
	{
		if (msgPanel)
			msgPanel->setCCPMState(MessagePanel::CCPM_STATE_CONNECTING);
		// TODO: show MessageBox
		return 0;
	}
	if (state == ConnectionManager::CCPM_STATE_DISCONNECTED)
	{
		if (!cm->connectCCPM(replyTo))
		{
			addSystemMessage(TSTRING(CCPM_FAILURE), Colors::g_ChatTextSystem);
			return 0;
		}
		if (msgPanel)
			msgPanel->setCCPMState(MessagePanel::CCPM_STATE_CONNECTING);
		addStatus(TSTRING(CCPM_IN_PROGRESS), false);
	}
	else
	{
		cm->disconnectCCPM(replyTo.user->getCID());
	}
	return 0;
}

LRESULT PrivateFrame::onShowHubMenu(WORD, WORD, HWND hWndCtl, BOOL&)
{
	if (hubList.empty()) return 0;
	CMenu menu;
	menu.CreatePopupMenu();
	CMenuItemInfo mi;
	int n = 0;
	vector<pair<string, tstring>> menuHubList;
	for (const auto& hub : hubList)
	{
		menuHubList.push_back(hub);
		mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE;
		mi.fType = MFT_STRING | MFT_RADIOCHECK;
		mi.dwTypeData = const_cast<TCHAR*>(menuHubList.back().second.c_str());
		mi.fState = MFS_ENABLED;
		if (replyTo.hint == hub.first) mi.fState |= MFS_CHECKED;
		mi.wID = IDC_PM_HUB + n;
		menu.InsertMenuItem(n, TRUE, &mi);
		if (++n == 100) break;
	}
	CPoint pt;
	CRect rc;
	CButton button(hWndCtl);
	button.GetClientRect(&rc);
	pt.x = rc.Width() / 2;
	pt.y = rc.Height() / 2;
	button.ClientToScreen(&pt);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, m_hWnd);
	cmd -= IDC_PM_HUB;
	if (cmd >= 0 && cmd < (int) menuHubList.size())
		selectHub(menuHubList[cmd].first);
	return 0;
}

LRESULT PrivateFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{

	bHandled = FALSE;
	POINT cpt;
	GetCursorPos(&cpt);
	
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	
	
	if (msgPanel && msgPanel->onContextMenu(pt, wParam))
		return TRUE;
		
	if (reinterpret_cast<HWND>(wParam) == ctrlClient && ctrlClient.IsWindow())
	{
		if (pt.x == -1 && pt.y == -1)
		{
			CRect erc;
			ctrlClient.GetRect(&erc);
			pt.x = erc.Width() / 2;
			pt.y = erc.Height() / 2;
			//  ctrlClient.ClientToScreen(&pt);
		}
		//  POINT ptCl = pt;
		ctrlClient.ScreenToClient(&pt);
		ctrlClient.onRButtonDown(pt);
		
		//  ctrlClient.OnRButtonDown(p, replyTo);
		const int i = ctrlClient.CharFromPos(pt);
		const int line = ctrlClient.LineFromChar(i);
		const int c = LOWORD(i) - ctrlClient.LineIndex(line);
		const int len = ctrlClient.LineLength(i) + 1;
		if (len < 2)
			return 0;
			
		tstring x;
		x.resize(len);
		ctrlClient.GetLine(line, &x[0], len);
		
		string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
		if (start == string::npos)
		{
			start = 0;
		}
		if (x.substr(start, (replyToRealName.length() + 2)) == (_T('<') + replyToRealName + _T('>')))
		{
			if (!replyTo.user->isOnline())
			{
				return S_OK;
			}
			OMenu* userMenu = createUserMenu();
			userMenu->ClearMenu();
			clearUserMenu();
			
			reinitUserMenu(replyTo.user, replyTo.hint);
			
			appendUcMenu(*userMenu, UserCommand::CONTEXT_USER, ClientManager::getHubs(replyTo.user->getCID(), replyTo.hint));
			WinUtil::appendSeparator(*userMenu);
			userMenu->InsertSeparatorFirst(replyToRealName);
			appendAndActivateUserItems(*userMenu);
			
			userMenu->AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
			userMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpt.x, cpt.y, m_hWnd);
			
			WinUtil::unlinkStaticMenus(*userMenu);
			cleanUcMenu(*userMenu);
			bHandled = TRUE;
		}
		else
		{
			OMenu textMenu;
			textMenu.CreatePopupMenu();
			appendChatCtrlItems(textMenu, false);
			textMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpt.x, cpt.y, m_hWnd);
			bHandled = TRUE;
		}
	}
	return S_OK;
}

void PrivateFrame::closeAll()
{
	dcdrun(const auto l_size_g_frames = frames.size());
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == frames.size());
}

void PrivateFrame::closeAllOffline()
{
	dcdrun(const auto l_size_g_frames = frames.size());
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		if (!i->first->isOnline())
			i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == frames.size());
}

void PrivateFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlClient.IsWindow())
		{
			ctrlClient.SetBackgroundColor(Colors::g_bgColor);
		}
		UpdateLayout();
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

bool PrivateFrame::closeUser(const UserPtr& u)
{
	const auto i = frames.find(u);
	if (i == frames.end())
	{
		return false;
	}
	i->second->PostMessage(WM_CLOSE, 0, 0);
	return true;
}

void PrivateFrame::onBeforeActiveTab(HWND aWnd)
{
	dcdrun(const auto l_size_g_frames = frames.size());
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		i->second->destroyMessagePanel(false);
	}
	dcassert(l_size_g_frames == frames.size());
}

void PrivateFrame::onAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isBeforeShutdown())
	{
		createMessagePanel();
		if (ctrlStatus) UpdateLayout();
	}
}

void PrivateFrame::createMessagePanel()
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!isClosedOrShutdown())
	{
		if (!ctrlStatus.m_hWnd && !ClientManager::isStartup())
		{
			BaseChatFrame::createMessageCtrl(this, PM_MESSAGE_MAP, false); // TODO - проверить hub
			if (!ctrlChatContainer.IsWindow())
				ctrlChatContainer.SubclassWindow(ctrlClient.m_hWnd);
			CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
			BaseChatFrame::createStatusCtrl(m_hWndStatusBar);
			restoreStatusFromCache(); // ¬осстанавливать статус нужно после UpdateLayout
			ctrlMessage.SetFocus();
		}
		auto flags = replyTo.user ? replyTo.user->getFlags() : 0;
		bool showSelectHub = (flags & User::NMDC) == 0;
		bool showCCPM = showSelectHub && (flags & (User::MYSELF | User::CCPM)) == User::CCPM;
		BaseChatFrame::createMessagePanel(showSelectHub, showCCPM);
		if (showCCPM && replyTo.user)
		{
			int state = ConnectionManager::getInstance()->getCCPMState(replyTo.user->getCID());
			msgPanel->setCCPMState(state);
			if (state == ConnectionManager::CCPM_STATE_CONNECTING)
				addStatus(TSTRING(CCPM_IN_PROGRESS), false);
		}
		setCountMessages(0);
	}
}

void PrivateFrame::destroyMessagePanel(bool p_is_destroy)
{
	const bool l_is_shutdown = p_is_destroy || ClientManager::isBeforeShutdown();
	BaseChatFrame::destroyStatusbar();
	BaseChatFrame::destroyMessagePanel();
	BaseChatFrame::destroyMessageCtrl(l_is_shutdown);
}

BOOL PrivateFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::g_tabCtrl->isActive(m_hWnd)) return FALSE;
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	if (WinUtil::isCtrl()) return FALSE;
	return IsDialogMessage(pMsg);
}
