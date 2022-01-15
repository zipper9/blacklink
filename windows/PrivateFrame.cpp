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
#include "../client/dht/DHT.h"
#include <boost/algorithm/string.hpp>

static const size_t MAX_PM_FRAMES = 100;
static const unsigned AWAY_MSG_COOLDOWN_TIME = 300000; // 5 minutes

static const unsigned LOCAL_TYPING_SEND_PERIOD = 120000; // When local isTyping flag is set, send TP1 every 2 minutes
static const unsigned LOCAL_TYPING_TIMEOUT     = 30000;  // Reset local isTyping flag after 30 seconds of inactivity
static const unsigned REMOTE_TYPING_TIMEOUT    = 150000; // Reset remote isTyping flag if nothing is received for 2.5 minutes

static const int STATUS_PART_PADDING = 12;
static const uint32_t FLAG_COUNTRY = 0x10000;

static const int iconSize = 16;
static const int flagIconWidth = 25;
static const int iconTextMargin = 2;

HIconWrapper PrivateFrame::frameIconOn(IDR_PRIVATE);
HIconWrapper PrivateFrame::frameIconOff(IDR_PRIVATE_OFF);

PrivateFrame::FrameMap PrivateFrame::frames;

PrivateFrame::PrivateFrame(const HintedUser& replyTo, const string& myNick) : replyTo(replyTo),
	replyToRealName(Text::toT(replyTo.user->getLastNick())),
	created(false), isOffline(false), isMultipleHubs(false), awayMsgSendTime(0),
	currentLocation(0),
	autoStartCCPM(true), sendCPMI(false), newMessageSent(false), newMessageReceived(false),
	remoteChatClosed(false), sendTimeTyping(0), typingTimeout{0, 0}, lastSentTime(0),
	ctrlChatContainer(WC_EDIT, this, PM_MESSAGE_MAP), timer(m_hWnd),
	statusSizes{0, 220, 60}
{
	ctrlStatusCache.resize(STATUS_LAST);
	ctrlStatusOwnerDraw = 1<<STATUS_LOCATION;
	ctrlClient.setHubParam(replyTo.hint, myNick);
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
	timer.destroyTimer();
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

			p->sendMessage(awayMsg);
			p->awayMsgSendTime = tick + AWAY_MSG_COOLDOWN_TIME;
		}
	}
	return true;
}

void PrivateFrame::openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick, const string& msg)
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
			LogManager::message("Lock > 100 open private message windows! Hub+nick: " + replyTo.hint + " " + myNick + " Message: " + msg);
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
		p->sendMessage(msg);
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
		if (!sendMessage(Text::fromT(fullMessageText)))
			resetInputMessageText = false;
		awayMsgSendTime = GET_TICK() + AWAY_MSG_COOLDOWN_TIME;
	}
	else
	{
		setStatusText(STATUS_TEXT, CTSTRING(USER_WENT_OFFLINE));
		resetInputMessageText = false;
	}
}

bool PrivateFrame::processFrameCommand(const Commands::ParsedCommand& pc, Commands::Result& res)
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
		case Commands::COMMAND_OPEN_LOG:
			if (pc.args.size() < 2)
			{
				res.what = Commands::RESULT_NO_TEXT;
				openFrameLog();
				return true;
			}
			break;
		case Commands::COMMAND_GRANT_EXTRA_SLOT:
			// TODO: add time parameter
			UploadManager::getInstance()->reserveSlot(replyTo, 600);
			res.what = Commands::RESULT_LOCAL_TEXT;
			res.text = STRING(SLOT_GRANTED);
			return true;
		case Commands::COMMAND_GET_LIST:
			UserInfoSimple(replyTo.user, replyTo.hint).getList();
			res.what = Commands::RESULT_NO_TEXT;
			return true;
		case Commands::COMMAND_ADD_FAVORITE:
			if (FavoriteManager::getInstance()->addFavoriteUser(getUser()))
			{
				res.what = Commands::RESULT_LOCAL_TEXT;
				res.text = STRING(FAVORITE_USER_ADDED);
			}
			else
			{
				res.what = Commands::RESULT_ERROR_MESSAGE;
				res.text = STRING(FAVORITE_USER_ALREADY_EXISTS);
			}
			return true;
		case Commands::COMMAND_REMOVE_FAVORITE:
			if (FavoriteManager::getInstance()->removeFavoriteUser(getUser()))
			{
				res.what = Commands::RESULT_LOCAL_TEXT;
				res.text = STRING(FAVORITE_USER_REMOVED);
			}
			else
			{
				res.what = Commands::RESULT_ERROR_MESSAGE;
				res.text = STRING(FAVORITE_USER_DOES_NOT_EXIST);
			}
			return true;
		case Commands::COMMAND_CCPM:
		{
			BOOL unused;
			onCCPM(0, 0, 0, unused);
			res.what = Commands::RESULT_NO_TEXT;
			return true;
		}
	}
	return BaseChatFrame::processFrameCommand(pc, res);
}

bool PrivateFrame::sendMessage(const string& msg, bool thirdPerson /*= false*/)
{
	int ccpm = msgPanel->getCCPMState();
	if (ccpm == MessagePanel::CCPM_STATE_CONNECTING)
		return false;
	if (ccpm == MessagePanel::CCPM_STATE_CONNECTED)
	{
		bool result = ConnectionManager::getInstance()->sendCCPMMessage(replyTo, msg, thirdPerson, false);
		if (result)
		{
			lastSentTime = GET_TICK();
			newMessageSent = true;
			typingTimeout[0] = sendTimeTyping = 0;
			if (!typingTimeout[1] && !remoteChatClosed)
				setStatusText(STATUS_CPMI, Util::emptyStringT);
			checkTimer();
		}
		return result;
	}
	ClientManager::privateMessage(replyTo, msg, thirdPerson, false);
	return true;
}

LRESULT PrivateFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		if (sendCPMI)
		{
			AdcCommand c(AdcCommand::CMD_PMI);
			c.addParam("QU1");
			ConnectionManager::getInstance()->sendCCPMMessage(replyTo.user->getCID(), c);
		}
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

void PrivateFrame::addLine(const Identity& from, bool myMessage, bool thirdPerson, const tstring& line, unsigned maxEmoticons, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
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

	addStatus(myMessage ? TSTRING(MESSAGE_SENT) : TSTRING(MESSAGE_RECEIVED), false, false);

	if (BOOLSETTING(BOLD_PM))
		setDirty();

	if (!myMessage)
	{
		setLocation(from);
		setRemoteChatClosed(false);
		newMessageReceived = true;
		if (typingTimeout[1])
		{
			typingTimeout[1] = 0;
			checkTimer();
			remoteChatClosed = false;
			setStatusText(STATUS_CPMI, Util::emptyStringT);
		}
	}
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
	// TODO тут ucParams не используется позже
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
			if (msgPanel->showSelectHubButton)
				msgPanel->getButton(MessagePanel::BUTTON_SELECT_HUB).EnableWindow(isMultipleHubs);
		}
	}
	updateStatusParts();
}

void PrivateFrame::setLocation(const Identity& id)
{
	IpAddress ip = id.getConnectIP();
	if (!Util::isValidIp(ip) || id.isIPCached(ip.type)) return;
	IPInfo ipInfo;
	Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_LOCATION | IPInfo::FLAG_COUNTRY);
	uint32_t location = 0;
	tstring text;
	if (ipInfo.locationImage > 0)
	{
		location = ipInfo.locationImage;
		text = Text::toT(ipInfo.location);
	}
	else if (ipInfo.countryCode)
	{
		location = FLAG_COUNTRY| ipInfo.countryCode;
		text = Text::toT(ipInfo.country);
	}
	if (location == currentLocation) return;
	currentLocation = location;
	setStatusText(STATUS_LOCATION, text);
}

void PrivateFrame::setStatusText(int index, const tstring& text)
{
	if (index != STATUS_TEXT && ctrlStatus)
	{
		int w = getStatusTextWidth(index, text);
		if (w > statusSizes[index])
		{
			statusSizes[index] = w;
			if (ctrlStatus) updateStatusParts();
		}
	}
	BaseChatFrame::setStatusText(index, text);
}

int PrivateFrame::getStatusTextWidth(int index, const tstring& text) const
{
	int w = WinUtil::getTextWidth(text, ctrlStatus) + STATUS_PART_PADDING;
	if (index == STATUS_LOCATION) w += flagIconWidth + iconTextMargin;
	return w;
}

void PrivateFrame::updateStatusTextWidth()
{
	for (int i = 1; i < STATUS_LAST; i++)
	{
		int w = getStatusTextWidth(i, ctrlStatusCache[i]);
		if (w > statusSizes[i]) statusSizes[i] = w;
	}
}

void PrivateFrame::updateStatusParts()
{
	if (!(ctrlStatus && ctrlLastLinesToolTip)) return;
	CRect sr;
	ctrlStatus.GetClientRect(sr);
	int sum = 0;
	for (int i = 1; i < STATUS_LAST; i++)
		sum += statusSizes[i];
	int w[STATUS_LAST];
	w[STATUS_TEXT] = std::max(int(sr.right) - sum, 120);
	for (int i = 1; i < STATUS_LAST; i++)
		w[i] = w[i - 1] + statusSizes[i];
	ctrlStatus.SetParts(STATUS_LAST, w);
	ctrlLastLinesToolTip.SetMaxTipWidth(max(w[STATUS_TEXT], 400));
	restoreStatusFromCache();
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
	isMultipleHubs = hubList.size() > 1;
	if (msgPanel)
		msgPanel->getButton(MessagePanel::BUTTON_SELECT_HUB).EnableWindow(isMultipleHubs);
	SetWindowText(fullUserName.c_str());
}

bool PrivateFrame::selectHub(const string& url)
{
	if (replyTo.hint == url) return true;
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
	return found;
}

void PrivateFrame::connectCCPM()
{
	if (replyTo.hint == dht::NetworkName)
	{
		OnlineUserList ouList;
		ClientManager::getInstance()->findOnlineUsers(replyTo.user->getCID(), ouList, ClientBase::TYPE_ADC);
		for (OnlineUserPtr& ou : ouList)
			if (selectHub(ou->getClientBase()->getHubUrl())) break;
	}
	if (!ConnectionManager::getInstance()->connectCCPM(replyTo))
	{
		addSystemMessage(TSTRING(CCPM_FAILURE), Colors::g_ChatTextSystem);
		return;
	}
	if (msgPanel)
		msgPanel->setCCPMState(MessagePanel::CCPM_STATE_CONNECTING);
	addStatus(TSTRING(CCPM_IN_PROGRESS), false);
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

void PrivateFrame::setLocalTyping(bool status)
{
	bool sendStatus = false;
	if (status)
	{
		uint64_t now = GET_TICK();
		if (!typingTimeout[0])
		{
			sendStatus = true;
			sendTimeTyping = now + LOCAL_TYPING_SEND_PERIOD;
		}
		typingTimeout[0] = now + LOCAL_TYPING_TIMEOUT;
	}
	else if (typingTimeout[0])
	{
		sendStatus = true;
		typingTimeout[0] = 0;
		sendTimeTyping = 0;
	}
	if (sendStatus)
	{
		AdcCommand c(AdcCommand::CMD_PMI);
		c.addParam(typingTimeout[0] ? "TP1" : "TP0");
		ConnectionManager::getInstance()->sendCCPMMessage(replyTo.user->getCID(), c);
	}
	checkTimer();
}

void PrivateFrame::setRemoteTyping(bool status)
{
	if (status)
	{
		remoteChatClosed = false;
		if (!typingTimeout[1])
			setStatusText(STATUS_CPMI, TSTRING_F(CPMI_TYPING, replyToRealName));
		typingTimeout[1] = GET_TICK() + REMOTE_TYPING_TIMEOUT;
	}
	else if (typingTimeout[1])
	{
		typingTimeout[1] = 0;
		if (!remoteChatClosed)
			setStatusText(STATUS_CPMI, Util::emptyStringT);
	}
	checkTimer();
}

void PrivateFrame::setRemoteChatClosed(bool status)
{
	if (remoteChatClosed == status) return;
	remoteChatClosed = status;
	if (remoteChatClosed)
		setStatusText(STATUS_CPMI, TSTRING_F(CPMI_CLOSED, replyToRealName));
	else if (!typingTimeout[1])
		setStatusText(STATUS_CPMI, Util::emptyStringT);
}

void PrivateFrame::processCPMI(const CPMINotification& info)
{
	if (info.isTyping != -1)
		setRemoteTyping(info.isTyping == 1);
	if (newMessageSent && info.seenTime >= lastSentTime)
	{
		newMessageSent = false;
		if (!typingTimeout[1])
		{
			remoteChatClosed = false;
			setStatusText(STATUS_CPMI, TSTRING_F(CPMI_SEEN, replyToRealName));
		}
	}
	setRemoteChatClosed(info.isClosed);
}

void PrivateFrame::sendSeenIndication()
{
	newMessageReceived = false;
	if (sendCPMI)
	{
		AdcCommand c(AdcCommand::CMD_PMI);
		c.addParam("SN1");
		ConnectionManager::getInstance()->sendCCPMMessage(replyTo.user->getCID(), c);
	}
}

void PrivateFrame::checkTimer()
{
	if (typingTimeout[0] || typingTimeout[1])
		timer.createTimer(500, 3);
	else
		timer.destroyTimer();
}

void PrivateFrame::onTextEdited()
{
	if (sendCPMI && ctrlMessage.GetWindowTextLength())
		setLocalTyping(true);
}

LRESULT PrivateFrame::onCCPM(WORD, WORD, HWND, BOOL&)
{
	ConnectionManager::PMConnState s;
	ConnectionManager::getInstance()->getCCPMState(replyTo.user->getCID(), s);
	if (s.state == ConnectionManager::CCPM_STATE_CONNECTING)
	{
		if (msgPanel)
			msgPanel->setCCPMState(MessagePanel::CCPM_STATE_CONNECTING);
		// TODO: show MessageBox
		return 0;
	}
	if (s.state == ConnectionManager::CCPM_STATE_DISCONNECTED)
		connectCCPM();
	else
		ConnectionManager::getInstance()->disconnectCCPM(replyTo.user->getCID());
	return 0;
}

LRESULT PrivateFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!timer.checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	uint64_t now = GET_TICK();
	if (typingTimeout[0])
	{
		if (now > typingTimeout[0] || !WinUtil::g_tabCtrl->isActive(m_hWnd))
			setLocalTyping(false);
		else if (now > sendTimeTyping)
		{
			sendTimeTyping = now + LOCAL_TYPING_SEND_PERIOD;
			AdcCommand c(AdcCommand::CMD_PMI);
			c.addParam("TP1");
			ConnectionManager::getInstance()->sendCCPMMessage(replyTo.user->getCID(), c);
		}
	}
	if (typingTimeout[1] && now > typingTimeout[1])
		setRemoteTyping(false);
	return 0;
}

LRESULT PrivateFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /* bHandled */)
{
	switch (wParam)
	{
		case PM_CHANNEL_CONNECTED:
			sendCPMI = BOOLSETTING(USE_CPMI) && wParam;
			updateCCPM(true);
			break;
		case PM_CHANNEL_DISCONNECTED:
			sendCPMI = false;
			updateCCPM(false);
			break;
		case PM_CPMI_RECEIVED:
		{
			auto info = reinterpret_cast<CPMINotification*>(lParam);
			processCPMI(*info);
			delete info;
			break;
		}
		default:
			dcassert(wParam == PM_USER_UPDATED);
			updateHubList();
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

LRESULT PrivateFrame::onDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if (dis->CtlID == ATL_IDW_STATUS_BAR && dis->itemID == STATUS_LOCATION)
	{
		if (currentLocation)
		{
			RECT rc = dis->rcItem;
			POINT pt = { rc.left, (rc.top + rc.bottom - iconSize) / 2 };
			if (currentLocation & FLAG_COUNTRY)
				g_flagImage.drawCountry(dis->hDC, currentLocation & ~FLAG_COUNTRY, pt);
			else
				g_flagImage.drawLocation(dis->hDC, currentLocation, pt);
			pt.x += flagIconWidth;
			const tstring& desc = ctrlStatusCache[STATUS_LOCATION];
			rc.left = pt.x + iconTextMargin;
			int oldMode = GetBkMode(dis->hDC);
			SetBkMode(dis->hDC, TRANSPARENT);
			DrawText(dis->hDC, desc.c_str(), desc.length(), &rc, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
			SetBkMode(dis->hDC, oldMode);
		}
	}
	else
		bHandled = FALSE;
	return 0;
}

void PrivateFrame::closeAll()
{
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		i->second->PostMessage(WM_CLOSE, 0, 0);
	}
}

void PrivateFrame::closeAllOffline()
{
	for (auto i = frames.cbegin(); i != frames.cend(); ++i)
	{
		if (!i->first->isOnline())
			i->second->PostMessage(WM_CLOSE, 0, 0);
	}
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
			updateStatusTextWidth();
			ctrlMessage.SetFocus();
		}
		auto flags = replyTo.user ? replyTo.user->getFlags() : 0;
		bool showSelectHub = (flags & User::NMDC) == 0;
		bool showCCPM = showSelectHub && (flags & (User::MYSELF | User::CCPM)) == User::CCPM;
		BaseChatFrame::createMessagePanel(showSelectHub, showCCPM);
		if (showCCPM && replyTo.user)
		{
			ConnectionManager::PMConnState s;
			ConnectionManager::getInstance()->getCCPMState(replyTo.user->getCID(), s);
			msgPanel->setCCPMState(s.state);
			sendCPMI = false;
			if (s.state == ConnectionManager::CCPM_STATE_CONNECTING)
				addStatus(TSTRING(CCPM_IN_PROGRESS), false);
			else if (s.state == ConnectionManager::CCPM_STATE_CONNECTED)
			{
				sendCPMI = BOOLSETTING(USE_CPMI) && s.cpmiSupported;
				if (s.cpmi.isTyping == 1)
				{
					remoteChatClosed = false;
					setRemoteTyping(true);
				}
				else
					setRemoteChatClosed(s.cpmi.isClosed);
			}
			else if (autoStartCCPM && BOOLSETTING(CCPM_AUTO_START))
			{
				autoStartCCPM = false;
				connectCCPM();
			}
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
