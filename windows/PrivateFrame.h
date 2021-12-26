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

#ifndef PRIVATE_FRAME_H
#define PRIVATE_FRAME_H

#include "../client/ClientManagerListener.h"
#include "../client/ConnectionManagerListener.h"
#include "BaseChatFrame.h"
#include "FlatTabCtrl.h"
#include "UCHandler.h"
#include "UserInfoBaseHandler.h"

#define PM_MESSAGE_MAP 9

class PrivateFrame : public MDITabChildWindowImpl<PrivateFrame>,
	private ClientManagerListener, private ConnectionManagerListener,
	public UCHandler<PrivateFrame>,
	public CMessageFilter,
	public UserInfoBaseHandler<PrivateFrame, UserInfoGuiTraits::NO_SEND_PM | UserInfoGuiTraits::USER_LOG>,
	private SettingsManagerListener,
	private BaseChatFrame
{
	public:
		static bool gotMessage(const Identity& from, const Identity& to, const Identity& replyTo, const tstring& message, unsigned maxEmoticons, const string& hubHint, bool myMessage, bool thirdPerson, bool notOpenNewWindow = false);
		static void openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick = Util::emptyString, const tstring& aMessage = Util::emptyStringT);
		static bool isOpen(const UserPtr& u)
		{
			return frames.find(u) != frames.end();
		}
		static bool closeUser(const UserPtr& u);
		static void closeAll();
		static void closeAllOffline();
		
		enum
		{
			PM_USER_UPDATED,
			PM_CHANNEL_CONNECTED,
			PM_CHANNEL_DISCONNECTED
		};
		
		DECLARE_FRAME_WND_CLASS_EX(_T("PrivateFrame"), IDR_PRIVATE, 0, COLOR_3DFACE);
		
		typedef MDITabChildWindowImpl<PrivateFrame> baseClass;
		typedef UCHandler<PrivateFrame> ucBase;
		typedef UserInfoBaseHandler < PrivateFrame, UserInfoGuiTraits::NO_SEND_PM | UserInfoGuiTraits::USER_LOG > uiBase;

		BEGIN_MSG_MAP(PrivateFrame)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		CHAIN_MSG_MAP(BaseChatFrame)
		COMMAND_ID_HANDLER(IDC_SEND_MESSAGE, onSendMessage)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_OFFLINE_PM, onCloseAllOffline)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_PM, onCloseAll)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_OPEN_USER_LOG, onOpenUserLog)
		COMMAND_ID_HANDLER(IDC_SELECT_HUB, onShowHubMenu);
		COMMAND_ID_HANDLER(IDC_CCPM, onCCPM);
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uiBase)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(PM_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_KEYDOWN, onKeyDown)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, onLButton)
		END_MSG_MAP()

		virtual BOOL PreTranslateMessage(MSG* pMsg) override;
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			openFrameLog();
			return 0;
		}
		LRESULT onShowHubMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCCPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled); // !Decker!

		virtual void onBeforeActiveTab(HWND aWnd) override;
		virtual void onAfterActiveTab(HWND aWnd) override;

		void addLine(const Identity& from, const bool myMessage, const bool thirdPerson, const tstring& line, unsigned maxEmoticons, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
		void readFrameLog();
		void openFrameLog() const
		{
			WinUtil::openLog(SETTING(LOG_FILE_PRIVATE_CHAT), getFrameLogParams(), TSTRING(NO_LOG_FOR_USER));
		}
		
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAll();
			return 0;
		}
		
		LRESULT onCloseAllOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAllOffline();
			return 0;
		}
		
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /* bHandled */)
		{
			if (wParam == PM_USER_UPDATED)
				updateHubList();
			else
				updateCCPM(wParam == PM_CHANNEL_CONNECTED);
			return 0;
		}

		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (ctrlMessage)
				ctrlMessage.SetFocus();
			if (ctrlClient.IsWindow())
				ctrlClient.goToEnd(false);
			return 0;
		}

		void addStatus(const tstring& aLine, const bool bInChat = true, const bool bHistory = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem)
		{
			if (!created)
			{
				Create(WinUtil::g_mdiClient);
			}
			BaseChatFrame::addStatus(aLine, bInChat, bHistory, cf);
		}

		bool sendMessage(const tstring& msg, bool thirdPerson = false) override;

		const UserPtr& getUser() const
		{
			return replyTo.user;
		}
		const string& getHub() const
		{
			return replyTo.hint;
		}
		void selectHub(const string& url);

	private:
		PrivateFrame(const HintedUser& replyTo, const string& myNick);
		~PrivateFrame();
		virtual void doDestroyFrame();

		bool created; // TODO: fix me please.
		typedef boost::unordered_map<UserPtr, PrivateFrame*, User::Hash> FrameMap;
		static FrameMap frames;

		static HIconWrapper frameIconOn, frameIconOff;

		HintedUser replyTo;
		tstring replyToRealName;
		tstring lastHubName;
		vector<pair<string, tstring>> hubList;

		CContainedWindow ctrlChatContainer;

		bool isOffline;
		uint64_t awayMsgSendTime;

		void updateHubList();
		void updateCCPM(bool connected);

		// ClientManagerListener
		void on(ClientManagerListener::UserUpdated, const OnlineUserPtr& ou) noexcept override
		{
			updateUser(ou->getUser());
		}
		void on(ClientManagerListener::UserConnected, const UserPtr& user) noexcept override
		{
			updateUser(user);
		}
		void on(ClientManagerListener::UserDisconnected, const UserPtr& user) noexcept override
		{
			updateUser(user);
		}
		void on(SettingsManagerListener::Repaint) override;
		void updateUser(const UserPtr& user)
		{
			if (user == replyTo.user)
				PostMessage(WM_SPEAKER, PM_USER_UPDATED);
		}

		// ConnectionManagerListener
		void on(ConnectionManagerListener::PMChannelConnected, const CID& cid) noexcept override
		{
			if (cid == replyTo.user->getCID())
				PostMessage(WM_SPEAKER, PM_CHANNEL_CONNECTED);
		}
		void on(ConnectionManagerListener::PMChannelDisconnected, const CID& cid) noexcept override
		{
			if (cid == replyTo.user->getCID())
				PostMessage(WM_SPEAKER, PM_CHANNEL_DISCONNECTED);
		}

		void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText);
		void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText);
		StringMap getFrameLogParams() const;

	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_destroy);
};

#endif // !defined(PRIVATE_FRAME_H)
