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
#include "PrivateFrame.h"
#include "SearchFrm.h"
#include "WinUtil.h"
#include "MainFrm.h"
#include "UserInfoSimple.h"
#include "../client/ClientManager.h"
#include "../client/LogManager.h"
#include "../client/UploadManager.h"
#include "../client/ShareManager.h"

HIconWrapper PrivateFrame::frameIconOn(IDR_PRIVATE);
HIconWrapper PrivateFrame::frameIconOff(IDR_PRIVATE_OFF);

PrivateFrame::FrameMap PrivateFrame::g_pm_frames;
std::unordered_map<string, unsigned> PrivateFrame::g_count_pm;

PrivateFrame::PrivateFrame(const HintedUser& replyTo, const string& myNick) : replyTo(replyTo),
	replyToRealName(replyTo.user->getLastNickT()),
	m_created(false), isOffline(false),
	ctrlChatContainer(WC_EDIT, this, PM_MESSAGE_MAP) // !Decker!
{
	ctrlStatusCache.resize(1);
	ctrlClient.setHubParam(replyTo.hint, myNick); // [+] IRainman fix.
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
	params["hubNI"] = Util::toString(ClientManager::getHubNames(replyTo.user->getCID(), getHubHint()));
	params["hubURL"] = Util::toString(ClientManager::getHubs(replyTo.user->getCID(), getHubHint()));
	params["userCID"] = replyTo.user->getCID().toBase32();
	params["userNI"] = Text::fromT(replyToRealName);
	params["myCID"] = ClientManager::getMyCID().toBase32();
	return params;
}

void PrivateFrame::addMesageLogParams(StringMap& params, const Identity& from, const tstring& aLine, bool bThirdPerson, const tstring& extra)
{
	params["message"] = ChatMessage::formatNick(from.getNick(), bThirdPerson) + Text::fromT(aLine);
	if (!extra.empty())
		params["extra"] = Text::fromT(extra);
}

LRESULT PrivateFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	BaseChatFrame::OnCreate(m_hWnd, rcDefault);
	PostMessage(WM_SPEAKER, PM_USER_UPDATED);
	m_created = true;
	ClientManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	bHandled = FALSE;
	return 1;
}

bool PrivateFrame::gotMessage(const Identity& from, const Identity& to, const Identity& replyTo,
                              const tstring& aMessage, unsigned p_max_smiles, const string& p_HubHint, const bool bMyMess,
                              const bool bThirdPerson, const bool notOpenNewWindow /*= false*/)   // !SMT!-S
{
	const auto& id = bMyMess ? to : replyTo;
	const auto& myId = bMyMess ? replyTo : to; // [+] IRainman fix.
	
	const string l_key = id.getUser()->getLastNick() + " + " + p_HubHint;
	string l_message = Text::fromT(aMessage);
#if 0 // ???
	const bool l_is_spam = CFlyServerConfig::isSpam(l_message);
#else
	const bool l_is_spam = false;
#endif
	const auto i = g_pm_frames.find(id.getUser());
	if (i == g_pm_frames.end())
	{
		Text::removeString_rn(l_message);
		if (l_is_spam)
		{
			LogManager::message("Ignore spam: [ " + l_message + " ] [user+hub = " + l_key + "]");
			return true; // “ипа все ок
		}
		if (notOpenNewWindow || g_pm_frames.size() > MAX_PM_FRAMES)
		{
			LogManager::message("Lock > 100 open private message windows! Hub: " + l_key + " Message: " + l_message);
			return false; // !SMT!-S
		}
		auto& l_count_pm = g_count_pm[l_key];
		if (l_count_pm > 10)
		{
			LogManager::message("Lock > 10 private message for Hub: " + l_key + " Message: " + l_message);
			return false;
		}
		++l_count_pm;
		// TODO - Add antispam!
		PrivateFrame* p = new PrivateFrame(HintedUser(id.getUser(), p_HubHint), myId.getNick());
		g_pm_frames.insert(make_pair(id.getUser(), p));
		p->addLine(from, bMyMess, bThirdPerson, aMessage, p_max_smiles);
		// [!] TODO! и видимо в €дро!
		if (!bMyMess && Util::getAway())
		{
			if (/*!(BOOLSETTING(NO_AWAYMSG_TO_BOTS) && */ !replyTo.isBotOrHub()) // [!] IRainman fix.
			{
				// Again, is there better way for this?
				const FavoriteHubEntry *fhe = FavoriteManager::getFavoriteHubEntry(Util::toString(ClientManager::getHubs(id.getUser()->getCID(), p_HubHint)));
				StringMap params;
				from.getParams(params, "user", false);
				
				if (fhe)
				{
					if (!fhe->getAwayMsg().empty())
						p->sendMessage(Text::toT(fhe->getAwayMsg()));
					else
						p->sendMessage(Text::toT(Util::getAwayMessage(params)));
				}
				else
				{
					p->sendMessage(Text::toT(Util::getAwayMessage(params)));
				}
			}
		}
		// [~] TODO! и видимо в €дро!
		SHOW_POPUP_EXT(POPUP_NEW_PM, Text::toT(id.getNick() + " - " + p_HubHint), PM_PREVIEW, aMessage, 250, TSTRING(PRIVATE_MESSAGE));
		PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP_OPEN);
	}
	else
	{
		if (l_is_spam)
		{
			LogManager::message("Detect private spam: [ " + l_message + " ] [user+hub = " + l_key + "]");
		}
		else
		{
			if (!bMyMess)
			{
				SHOW_POPUP_EXT(POPUP_PM, Text::toT(id.getNick() + " - " + p_HubHint), PM_PREVIEW, aMessage, 250, TSTRING(PRIVATE_MESSAGE));
				PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP);
			}
		}
		// Add block spam???
		i->second->addLine(from, bMyMess, bThirdPerson, aMessage, p_max_smiles);
	}
	return true;
}

void PrivateFrame::openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick, const tstring& msg)
{
	// [+] IRainman fix.
	if (myNick.empty())
	{
		if (ou)
		{
			myNick = ou->getClient().getMyNick();
		}
		else if (!replyTo.hint.empty())
		{
			const auto l_client = ClientManager::findClient(replyTo.hint);
			myNick = l_client ? l_client->getMyNick() : SETTING(NICK);
		}
		else
		{
			myNick = SETTING(NICK);
		}
	}
	// [~] IRainman fix.
	
	PrivateFrame* p = nullptr;
	const auto i = g_pm_frames.find(replyTo);
	if (i == g_pm_frames.end())
	{
		if (g_pm_frames.size() > MAX_PM_FRAMES)
		{
			LogManager::message("Lock > 100 open private message windows! Hub+nick: " + replyTo.hint + " " + myNick + " Message: " + Text::fromT(msg));
			return;
		}
		
		// [+] IRainman fix.
		if (ou)
		{
			replyTo.user->setLastNick(ou->getIdentity().getNick());
		}
		// [~] IRainman fix.
		p = new PrivateFrame(replyTo, myNick);
		g_pm_frames.insert(make_pair(replyTo, p));
		g_count_pm[replyTo.user->getLastNick() + "~" + replyTo.hint]++;
		p->CreateEx(WinUtil::g_mdiClient);
	}
	else
	{
		p = i->second;
		if (::IsIconic(p->m_hWnd))
			::ShowWindow(p->m_hWnd, SW_RESTORE);
			
		p->MDIActivate(p->m_hWnd);
	}
	if (!msg.empty())
	{
		p->sendMessage(msg);
	}
}

LRESULT PrivateFrame::onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!processingServices(uMsg, wParam, lParam, bHandled))
	{
		processingHotKeys(uMsg, wParam, lParam, bHandled);
	}
	return 0;
}

LRESULT PrivateFrame::onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	HWND focus = GetFocus();
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
			
		bHandled = true;
		// Nickname click, let's see if we can find one like it in the name list...
		// ник в чат
		appendNickToChat(x.substr(start, end - start));
	}
	return 0;
}

void PrivateFrame::processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText)
{
	if (replyTo.user->isOnline())
	{
		sendMessage(fullMessageText);
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
		UploadManager::getInstance()->reserveSlot(HintedUser(getUser(), getHubHint()), 600);
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
		BOOL bTmp;
		clearUserMenu();
		reinitUserMenu(replyTo, getHubHint());
		onGetList(0, 0, 0, bTmp);
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		openFrameLog();
	}
}

void PrivateFrame::sendMessage(const tstring& msg, bool thirdperson /*= false*/)
{
	ClientManager::privateMessage(HintedUser(replyTo, getHubHint()), Text::fromT(msg), thirdperson);
}

LRESULT PrivateFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		closed = true;
		ClientManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		g_count_pm[replyTo.user->getLastNick() + "~" + replyTo.hint]--;
		g_pm_frames.erase(replyTo);
		bHandled = FALSE;
		return 0;
	}
}

void PrivateFrame::readFrameLog()
{
	const auto linesCount = SETTING(SHOW_LAST_LINES_LOG);
	if (linesCount)
	{
		const string path = Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatParams(SETTING(LOG_FILE_PRIVATE_CHAT), getFrameLogParams(), false));
		appendLogToChat(path, linesCount);
	}
}

void PrivateFrame::addLine(const Identity& from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	if (!m_created)
	{
		if (BOOLSETTING(POPUNDER_PM))
			WinUtil::hiddenCreateEx(this);
		else
			CreateEx(WinUtil::g_mdiClient);
	}
	
	tstring extra;
	BaseChatFrame::addLine(from, bMyMess, bThirdPerson, aLine, p_max_smiles, cf, extra);
	
	if (BOOLSETTING(LOG_PRIVATE_CHAT))
	{
		StringMap params = getFrameLogParams();
		addMesageLogParams(params, from, aLine, bThirdPerson, extra);
		LOG(PM, params);
	}
	
	addStatus(TSTRING(LAST_CHANGE), false, false);
	
	if (BOOLSETTING(BOLD_PM))
		setDirty();
}

LRESULT PrivateFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	
	OMenu tabMenu;
	tabMenu.CreatePopupMenu();
	clearUserMenu();
	
	tabMenu.InsertSeparatorFirst(replyToRealName);
	reinitUserMenu(replyTo, getHubHint()); // [!] IRainman fix.
	appendAndActivateUserItems(tabMenu);
	appendUcMenu(tabMenu, UserCommand::CONTEXT_USER, ClientManager::getHubs(replyTo.user->getCID(), getHubHint()));
	if (!(tabMenu.GetMenuState(tabMenu.GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
	{
		tabMenu.AppendMenu(MF_SEPARATOR);
	}
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_PM, CTSTRING(MENU_CLOSE_ALL_OFFLINE_PM));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_PM, CTSTRING(MENU_CLOSE_ALL_PM));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
	
	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	
	cleanUcMenu(tabMenu);
	WinUtil::unlinkStaticMenus(tabMenu); // TODO - fix copy-paste
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
	ClientManager::userCommand(HintedUser(replyTo, getHubHint()), uc, ucParams, true);
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
		if (ctrlStatus && m_ctrlLastLinesToolTip)
		{
			if (ctrlStatus.IsWindow() && m_ctrlLastLinesToolTip.IsWindow())
			{
				CRect sr;
				int w[1];
				ctrlStatus.GetClientRect(sr);				
				w[0] = sr.right - 16;				
				ctrlStatus.SetParts(1, w);				
				m_ctrlLastLinesToolTip.SetMaxTipWidth(max(w[0], 400));
			}
		}
		int h = 0, chat_columns = 0;
		const bool bUseMultiChat = isMultiChat(h, chat_columns);
		CRect rc = rect;
		//rc.bottom -= h + 15;
		rc.bottom -= h + (Fonts::g_fontHeightPixl + 1) * int(bUseMultiChat) + 18;
		
		if (ctrlClient.IsWindow())
			ctrlClient.MoveWindow(rc);
		
		const int iButtonPanelLength = MessagePanel::GetPanelWidth();
		
		rc = rect;
		rc.bottom -= 4;
		rc.top = rc.bottom - h - Fonts::g_fontHeightPixl * int(bUseMultiChat) - 12;
		rc.left += 2;
		rc.right -= iButtonPanelLength + 2;
		if (ctrlMessage)
			ctrlMessage.MoveWindow(rc);
		
		if (bUseMultiChat)
			rc.top += h + 6;

		rc.left = rc.right;
		rc.bottom -= 1;
		
		if (msgPanel)
			msgPanel->UpdatePanel(rc);
	}
}

void PrivateFrame::updateTitle()
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (!replyTo.user)
		return;
	pair<tstring, bool> hubs = WinUtil::getHubNames(replyTo, getHubHint());
	
#if 0 // FIXME
	bool banIcon = false;
	Flags::MaskType flags;
	int ul;
	if (FavoriteManager::getFavUserParam(replyTo, flags, ul))
		banIcon = FavoriteManager::hasUploadBan(ul) || FavoriteManager::hasIgnorePM(flags);
#endif
	
	if (hubs.second)
	{
#if 0 // FIXME		
		if (banIcon)
			setCustomIcon(WinUtil::g_banIconOnline);
		else
			unsetIconState();
#endif			
		setDisconnected(false);
		// [!] IRainman fix: when the user first came to the network
		// with the opening of the window private message - update the name,
		// if when you open the window it was already known the real name - use it.
		if (replyToRealName.empty())
		{
			replyToRealName = replyTo.user->getLastNickT();
		}
		if (isOffline)
			addStatus(TSTRING(USER_WENT_ONLINE) + _T(" [") + replyToRealName + _T(" - ") + hubs.first + _T("]"));
		isOffline = false;
	}
	else
	{
		isOffline = true;
//[-]PPA        ctrlClient.setClient(NULL);

#if 0 // FIXME
		if (banIcon)
			setCustomIcon(WinUtil::g_banIconOffline);
		else
			setIconState();
#endif
		setDisconnected(true);
		addStatus(TSTRING(USER_WENT_OFFLINE) + _T(" [") + replyToRealName + _T(" - ") + Text::toT(getHubHint()) + _T("]"));
		// [-] IRainman fix
		//isOffline = true;
		//ctrlClient.setClient(NULL);
		// [~] IRainman fix
	}
	SetWindowText((replyToRealName + _T(" - ") + (hubs.second ? hubs.first : Text::toT(getHubHint()))).c_str());
}

LRESULT PrivateFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{

	bHandled = FALSE;
	POINT cpt;
	GetCursorPos(&cpt);
	
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	
	
	if (msgPanel && msgPanel->OnContextMenu(pt, wParam))
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
		ctrlClient.OnRButtonDown(pt);
		
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
			OMenu* l_user_menu = createUserMenu();
			l_user_menu->ClearMenu();
			clearUserMenu(); // !SMT!-S
			
			reinitUserMenu(replyTo, getHubHint()); // [!] IRainman fix.
			
			appendUcMenu(*l_user_menu, UserCommand::CONTEXT_USER, ClientManager::getHubs(replyTo.user->getCID(), getHubHint()));
			if (!(l_user_menu->GetMenuState(l_user_menu->GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
			{
				l_user_menu->AppendMenu(MF_SEPARATOR);
			}
			l_user_menu->InsertSeparatorFirst(replyToRealName);
			appendAndActivateUserItems(*l_user_menu);
			
			l_user_menu->AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
			l_user_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpt.x, cpt.y, m_hWnd);
			
			WinUtil::unlinkStaticMenus(*l_user_menu); // TODO - fix copy-paste
			cleanUcMenu(*l_user_menu);
			bHandled = TRUE;
		}
		else
		{
			OMenu textMenu;
			textMenu.CreatePopupMenu();
			appendChatCtrlItems(textMenu);
			textMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpt.x, cpt.y, m_hWnd);
			bHandled = TRUE;
		}
	}
	return S_OK;
}

void PrivateFrame::closeAll()
{
	dcdrun(const auto l_size_g_frames = g_pm_frames.size());
	for (auto i = g_pm_frames.cbegin(); i != g_pm_frames.cend(); ++i)
	{
		i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == g_pm_frames.size());
}

void PrivateFrame::closeAllOffline()
{
	dcdrun(const auto l_size_g_frames = g_pm_frames.size());
	for (auto i = g_pm_frames.cbegin(); i != g_pm_frames.cend(); ++i)
	{
		if (!i->first->isOnline())
			i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == g_pm_frames.size());
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

// !SMT!-S
bool PrivateFrame::closeUser(const UserPtr& u)
{
	const auto i = g_pm_frames.find(u);
	if (i == g_pm_frames.end())
	{
		return false;
	}
	i->second->PostMessage(WM_CLOSE, 0, 0);
	return true;
}

void PrivateFrame::onBeforeActiveTab(HWND aWnd)
{
	dcdrun(const auto l_size_g_frames = g_pm_frames.size());
	for (auto i = g_pm_frames.cbegin(); i != g_pm_frames.cend(); ++i)
	{
		i->second->destroyMessagePanel(false);
	}
	dcassert(l_size_g_frames == g_pm_frames.size());
}

void PrivateFrame::onAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isBeforeShutdown())
	{
		createMessagePanel();
		if (ctrlStatus) UpdateLayout();
	}
}

void PrivateFrame::onInvalidateAfterActiveTab(HWND aWnd)
{
}

void PrivateFrame::createMessagePanel()
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!isClosedOrShutdown())
	{
		if (!ctrlStatus.m_hWnd && ClientManager::isStartup() == false)
		{
			BaseChatFrame::createMessageCtrl(this, PM_MESSAGE_MAP, false); // TODO - проверить hub
			if (!ctrlChatContainer.IsWindow())
				ctrlChatContainer.SubclassWindow(ctrlClient.m_hWnd);
			CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
			BaseChatFrame::createStatusCtrl(m_hWndStatusBar);
			restoreStatusFromCache(); // ¬осстанавливать статус нужно после UpdateLayout
			ctrlMessage.SetFocus();
		}
		BaseChatFrame::createMessagePanel();
		setCountMessages(0);
	}
}

void PrivateFrame::destroyMessagePanel(bool p_is_destroy)
{
	const bool l_is_shutdown = p_is_destroy || ClientManager::isBeforeShutdown();
	BaseChatFrame::destroyStatusbar(l_is_shutdown);
	BaseChatFrame::destroyMessagePanel(l_is_shutdown);
	BaseChatFrame::destroyMessageCtrl(l_is_shutdown);
}
