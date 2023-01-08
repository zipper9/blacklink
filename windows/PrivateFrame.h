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
#include "TimerHelper.h"

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
		using BaseChatFrame::addSystemMessage;

		static bool gotMessage(const Identity& from, const Identity& to, const Identity& replyTo, const tstring& message, unsigned maxEmoticons, const string& hubHint, bool myMessage, bool thirdPerson, bool notOpenNewWindow = false);
		static void openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick = Util::emptyString, const string& message = Util::emptyString);
		static bool isOpen(const UserPtr& u)
		{
			return frames.find(u) != frames.end();
		}
		static bool closeUser(const UserPtr& u);
		static void closeAll();
		static void closeAllOffline();
		static void prepareNonMaximized();
		static void changeTheme();

		enum
		{
			PM_USER_UPDATED,
			PM_CHANNEL_CONNECTED,
			PM_CHANNEL_DISCONNECTED,
			PM_CPMI_RECEIVED
		};

		static CFrameWndClassInfo& GetWndClassInfo();

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
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DRAWITEM, onDrawItem)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		CHAIN_MSG_MAP(BaseChatFrame)
		COMMAND_ID_HANDLER(IDC_SEND_MESSAGE, onSendMessage)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_OFFLINE_PM, onCloseAllOffline)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_PM, onCloseAll)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
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
		LRESULT onShowHubMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCCPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled); // !Decker!

		virtual void onDeactivate() override;
		virtual void onActivate() override;

		void addLine(const Identity& from, bool myMessage, bool thirdPerson, const tstring& line, unsigned maxEmoticons, int textStyle = Colors::TEXT_STYLE_NORMAL);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
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

		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /* bHandled */);

		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (newMessageReceived)
				sendSeenIndication();
			if (ctrlMessage)
				ctrlMessage.SetFocus();
			if (ctrlClient.IsWindow())
				ctrlClient.goToEnd(false);
			return 0;
		}

		LRESULT onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

		void setLocation(const Identity& id);
		void setStatusText(int index, const tstring& text);
		int getStatusTextWidth(int indx, const tstring& text) const;
		void updateStatusTextWidth();
		void updateStatusParts();

		const UserPtr& getUser() const
		{
			return replyTo.user;
		}
		const string& getHub() const
		{
			return replyTo.hint;
		}
		bool selectHub(const string& url);

		static PrivateFrame* findFrameByID(uint64_t id);

		// UserInfoBaseHandler
		OnlineUserPtr getSelectedOnlineUser() const;
		void getSelectedUsers(vector<UserPtr>& v) const {}
		void openUserLog() { openFrameLog(); }
		void showActiveFrame();

	private:
		enum
		{
			STATUS_TEXT,
			STATUS_CPMI,
			STATUS_LOCATION,
			STATUS_LAST
		};

		PrivateFrame(const HintedUser& replyTo, const string& myNick);

		bool created; // TODO: fix me please.
		typedef boost::unordered_map<UserPtr, PrivateFrame*, User::Hash> FrameMap;
		static FrameMap frames;

		HintedUser replyTo;
		tstring replyToRealName;
		tstring lastHubName;
		vector<pair<string, tstring>> hubList;

		CContainedWindow ctrlChatContainer;
		int statusSizes[STATUS_LAST];

		bool isMultipleHubs;
		bool isOffline;
		uint64_t awayMsgSendTime;
		uint32_t currentLocation;

		int ccpmState;
		bool autoStartCCPM;
		bool sendCPMI;
		bool newMessageSent;
		bool newMessageReceived;
		bool remoteChatClosed;
		bool uiInitialized;
		uint64_t sendTimeTyping;
		uint64_t typingTimeout[2];
		uint64_t lastSentTime;
		TimerHelper timer;

		void initUI();
		void updateHubList();
		void connectCCPM();
		void updateCCPM(bool connected);
		void processCPMI(const CPMINotification& info);
		void setLocalTyping(bool status);
		void setRemoteTyping(bool status);
		void setRemoteChatClosed(bool status);
		void sendSeenIndication();
		void checkTimer();

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
		void on(ConnectionManagerListener::PMChannelConnected, const CID& cid, bool cpmiSupported) noexcept override
		{
			if (cid == replyTo.user->getCID())
				PostMessage(WM_SPEAKER, PM_CHANNEL_CONNECTED, (WPARAM) cpmiSupported);
		}
		void on(ConnectionManagerListener::PMChannelDisconnected, const CID& cid) noexcept override
		{
			if (cid == replyTo.user->getCID())
				PostMessage(WM_SPEAKER, PM_CHANNEL_DISCONNECTED);
		}
		void on(ConnectionManagerListener::CPMIReceived, const CID& cid, const CPMINotification& info) noexcept override
		{
			if (cid == replyTo.user->getCID())
				WinUtil::postSpeakerMsg(m_hWnd, PM_CPMI_RECEIVED, new CPMINotification(info));
		}

		// BaseCharFrame
		void doDestroyFrame() override;
		bool processFrameCommand(const Commands::ParsedCommand& pc, Commands::Result& res) override;
		void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText) override;
		void onTextEdited() override;
		bool sendMessage(const string& msg, bool thirdPerson = false) override;
		void addStatus(const tstring& line, bool inChat = true, bool history = true, int textStyle = Colors::TEXT_STYLE_SYSTEM_MESSAGE) override;
		void readFrameLog() override;

		StringMap getFrameLogParams() const;

	public:
		void createMessagePanel();
		void destroyMessagePanel(bool isShutdown);
};

#endif // !defined(PRIVATE_FRAME_H)
