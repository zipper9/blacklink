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

#ifndef HUB_FRAME_H
#define HUB_FRAME_H

#include "../client/User.h"
#include "../client/Client.h"
#include "../client/ClientManager.h"
#include "../client/UserManagerListener.h"
#include "../client/TaskQueue.h"

#include "BaseChatFrame.h"
#include "UserListWindow.h"
#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "UserInfoBaseHandler.h"
#include "TimerHelper.h"
#include "UCHandler.h"

#define EDIT_MESSAGE_MAP 10     // This could be any number, really...
#define HUBSTATUS_MESSAGE_MAP 12 // Status frame

struct CompareItems;

class HubFrame : public MDITabChildWindowImpl<HubFrame>,
	private ClientListener,
	public CSplitterImpl<HubFrame>,
	public CMessageFilter,
	public UCHandler<HubFrame>,
	public UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr >,
	private SettingsManagerListener,
	private FavoriteManagerListener,
	private UserManagerListener,
	public BaseChatFrame,
	public UserListWindow::HubFrameCallbacks
{
	public:
		using BaseChatFrame::addSystemMessage;

		struct Settings
		{
			string server;
			string name;
			string keyPrint;
			const string* rawCommands = nullptr;
			int encoding = 0;
			int favoriteId = 0;
			int windowPosX = 0;
			int windowPosY = 0;
			int windowSizeX = 0;
			int windowSizeY = 0;
			int windowType = SW_MAXIMIZE;
			int chatUserSplit = 5000;
			bool hideUserList = false;

			void copySettings(const FavoriteHubEntry& entry);
		};

		DECLARE_FRAME_WND_CLASS_EX(_T("HubFrame"), IDR_HUB, 0, COLOR_3DFACE);
		
		typedef CSplitterImpl<HubFrame> splitBase;
		typedef MDITabChildWindowImpl<HubFrame> baseClass;
		typedef UCHandler<HubFrame> ucBase;
		typedef UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr > uibBase;
		
		BEGIN_MSG_MAP(HubFrame)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onHubFrmCtlColor)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, onSizeMove)
		MESSAGE_HANDLER(WMU_CHAT_LINK_CLICKED, onChatLinkClicked)

		CHAIN_MSG_MAP(BaseChatFrame)
		COMMAND_ID_HANDLER(IDC_RECONNECT, onReconnect)
		COMMAND_ID_HANDLER(ID_DISCONNECT, onDisconnect)
		COMMAND_ID_HANDLER(IDC_REFRESH, onRefresh)
		COMMAND_ID_HANDLER(IDC_FOLLOW, onFollow)
		COMMAND_ID_HANDLER(IDC_SEND_MESSAGE, onSendMessage)
		COMMAND_ID_HANDLER(IDC_ADD_AS_FAVORITE, onAddAsFavorite)
		COMMAND_ID_HANDLER(IDC_REM_AS_FAVORITE, onRemAsFavorite)
		COMMAND_ID_HANDLER(IDC_AUTO_START_FAVORITE, onToggleAutoConnect)
		COMMAND_ID_HANDLER(IDC_EDIT_HUB_PROP, onEditHubProp)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_RECONNECT_DISCONNECTED, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_CLOSE_DISCONNECTED, onCloseWindows)
		COMMAND_ID_HANDLER(IDC_SELECT_USER, onSelectUser)
		COMMAND_ID_HANDLER(IDC_AUTOSCROLL_CHAT, onAutoScrollChat)
		COMMAND_ID_HANDLER(IDC_BAN_IP, onBanIP)
		COMMAND_ID_HANDLER(IDC_UNBAN_IP, onUnBanIP)
		COMMAND_ID_HANDLER(IDC_OPEN_HUB_LOG, onOpenHubLog)
		COMMAND_ID_HANDLER(IDC_OPEN_USER_LOG, onOpenUserLog)
		COMMAND_ID_HANDLER(IDC_COPY_HUBNAME, onCopyHubInfo)
		COMMAND_ID_HANDLER(IDC_COPY_HUBADDRESS, onCopyHubInfo)
		COMMAND_ID_HANDLER(IDC_COPY_HUB_IP, onCopyHubInfo)
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uibBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(splitBase)
		ALT_MSG_MAP(EDIT_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_KEYDOWN, onKeyDown)
		MESSAGE_HANDLER(BM_SETCHECK, onShowUsers)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, onLButton)
		ALT_MSG_MAP(FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		ALT_MSG_MAP(HUBSTATUS_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onSwitchPanels)
		END_MSG_MAP()
		
		virtual BOOL PreTranslateMessage(MSG* pMsg) override;
		LRESULT onHubFrmCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onCopyHubInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onShowUsers(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onFollow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onReconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSelectUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAutoScrollChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onUnBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSizeMove(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onChatLinkClicked(UINT, WPARAM, LPARAM, BOOL&);

		LRESULT onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (!timer.checkTimerID(wParam))
			{
				bHandled = FALSE;
				return 0;
			}
			if (!isClosedOrShutdown())
				onTimerInternal();
			return 0;
		}

		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			processTasks();
			return 0;
		}

		LRESULT onSwitchPanels(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			switchPanels();
			return 0;
		}

		virtual void onBeforeActiveTab(HWND aWnd) override;
		virtual void onAfterActiveTab(HWND aWnd) override;
		virtual void onInvalidateAfterActiveTab(HWND aWnd) override;

		void UpdateLayout(BOOL resizeBars = TRUE) override;
		void addLine(const Identity& from, bool myMessage, bool thirdPerson, const tstring& line, unsigned maxSmiles, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void runUserCommand(UserCommand& uc);
		void followRedirect();
		void switchPanels();
		void selectCID(const CID& cid);

		static HubFrame* openHubWindow(const Settings& cs, bool* isNew = nullptr);
		static HubFrame* openHubWindow(const string& server, const string& keyPrint = Util::emptyString);
		static HubFrame* findHubWindow(const string& server);
		static void resortUsers();
		static void closeDisconnected();
		static void reconnectDisconnected();
		static void closeAll(size_t threshold = 0);
		static void updateAllTitles();

		static HubFrame* findFrameByID(uint64_t id);

		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (ctrlMessage)
				ctrlMessage.SetFocus();
			return 0;
		}
		
		LRESULT onAddAsFavorite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			addAsFavorite();
			return 0;
		}
		
		LRESULT onRemAsFavorite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			removeFavoriteHub();
			return 0;
		}
		LRESULT onToggleAutoConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			toggleAutoConnect();
			return 0;
		}
		LRESULT onEditHubProp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCloseWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(client);
			if (isConnected())
			{
				ctrlUsers.clearUserList();
				client->refreshUserList(false);
			}
			return 0;
		}

	private:
		enum AutoConnectType
		{
			DONT_CHANGE = 0,
			UNSET = -1,
			SET = 1,
		};

		enum
		{
			STATUS_TEXT,
			STATUS_CIPHER_SUITE,
			STATUS_USERS,
			STATUS_SHARED,
			STATUS_SIZE_PER_USER,
			STATUS_HUB_ICON,
			STATUS_LAST
		};

		HubFrame(const Settings& cs);
		~HubFrame();

		typedef boost::unordered_map<string, HubFrame*> FrameMap;
		static FrameMap frames;
		void removeFrame(const string& redirectUrl);

		UserListWindow ctrlUsers;
		int hubUpdateCount;
		string prevHubName;
		string prevTooltip;

		TaskQueue tasks;
		TimerHelper timer;

		void onTimerHubUpdated();
		int infoUpdateSeconds;
		string redirect;
		tstring autoComplete;
		bool waitingForPassword;
		bool showingPasswordDlg;
		
		ClientBasePtr baseClient;
		Client* client;
		string serverUrl;
		string originalServerUrl;

		bool isDHT;
		int currentDHTState;
		size_t currentDHTNodeCount;
		
		void setHubParam() { hubParamUpdated = true; }
		bool isConnected() const { return client && client->isConnected(); }

		tstring lastUserName;
		
		bool shouldUpdateStats;
		bool showUsers;
		bool showUsersStore;
		
		void setShowUsers(bool value)
		{
			showUsers = value;
			showUsersStore = value;
			ctrlUsers.setShowUsers(value);
		}
		unsigned asyncUpdate;
		unsigned asyncUpdateSaved;
		bool disableChat;
		
		void onTimerInternal();
		void processTasks();
		
		void onUserParts(const OnlineUserPtr& ou);
		
		UserInfo* findUserByNick(const tstring& nick);
		
		void addAsFavorite(AutoConnectType autoConnectType = DONT_CHANGE);
		void removeFavoriteHub();
		
		void toggleAutoConnect();
		void appendHubAndUsersItems(OMenu& menu, const bool isChat);
		void updateStats();
		
		// FavoriteManagerListener
		void on(FavoriteManagerListener::UserAdded, const FavoriteUser& user) noexcept override;
		void on(FavoriteManagerListener::UserRemoved, const FavoriteUser& user) noexcept override;
		void on(FavoriteManagerListener::UserStatusChanged, const UserPtr& user) noexcept override;
		void resortForFavsFirst(bool justDoIt = false);
		
		// UserManagerListener
		void on(UserManagerListener::IgnoreListChanged) noexcept override;
		void on(UserManagerListener::IgnoreListCleared) noexcept override;
		void on(UserManagerListener::ReservedSlotChanged, const UserPtr& user) noexcept override;

		// SettingsManagerListener
		void on(SettingsManagerListener::Repaint) override;
		
		// ClientListener
		void on(ClientListener::Connecting, const Client*) noexcept override;
		void on(ClientListener::Connected, const Client*) noexcept override;
		void on(ClientListener::UserUpdated, const OnlineUserPtr&) noexcept override;
		void on(ClientListener::UserListUpdated, const ClientBase*, const OnlineUserList&) noexcept override;
		void on(ClientListener::UserRemoved, const ClientBase*, const OnlineUserPtr&) noexcept override;
		void on(ClientListener::UserListRemoved, const ClientBase*, const OnlineUserList&) noexcept override;
		void on(ClientListener::Redirect, const Client*, const string&) noexcept override;
		void on(ClientListener::ClientFailed, const Client*, const string&) noexcept override;
		void on(ClientListener::GetPassword, const Client*) noexcept override;
		void on(ClientListener::HubUpdated, const Client*) noexcept override;
		void on(ClientListener::Message, const Client*, std::unique_ptr<ChatMessage>&) noexcept override;
		void on(ClientListener::NickError, ClientListener::NickErrorCode nickError) noexcept override;
		void on(ClientListener::HubFull, const Client*) noexcept override;
		void on(ClientListener::CheatMessage, const string&) noexcept override;
		void on(ClientListener::UserReport, const ClientBase*, const string&) noexcept override;
		void on(ClientListener::HubInfoMessage, ClientListener::HubInfoCode code, const Client* client, const string& line) noexcept override;
		void on(ClientListener::StatusMessage, const Client*, const string& line, int statusFlags) noexcept override;
		void on(SettingsLoaded, const Client*) noexcept override;

		// UserInfoBaseHandler
		OnlineUserPtr getSelectedOnlineUser() const override { return getSelectedUser(); }

		// UserListWindow::HubFrameCallbacks
		void showErrorMessage(const tstring& text) override;
		void setCurrentNick(const tstring& nick) override;
		void appendNickToChat(const tstring& nick) override;
		void addTask(int type, Task* task) override;

		struct StatusTask : public Task
		{
			explicit StatusTask(const string& msg, bool isInChat, bool isSystem) : str(msg), isInChat(isInChat), isSystem(isSystem) { }
			const string str;
			const bool isInChat;
			const bool isSystem;
		};
		void updateUserJoin(const OnlineUserPtr& ou);
		void doDisconnected();
		void doConnected();
		void clearTaskAndUserList();

		bool handleAutoComplete() override;
		void clearAutoComplete() override;

	public:
		static void addDupUsersToSummaryMenu(const ClientManager::UserParams& param, vector<UserInfoGuiTraits::DetailsItem>& detailsItems, UINT& idc);

		StringMap getFrameLogParams() const;
		void openFrameLog() const
		{
			WinUtil::openLog(SETTING(LOG_FILE_MAIN_CHAT), getFrameLogParams(), TSTRING(NO_LOG_FOR_HUB));
		}
		UserListWindow::CtrlUsers& getUserList() { return ctrlUsers.getUserList(); }

	protected:
		// BaseChatFrame
		void doDestroyFrame() override;
		bool processFrameCommand(const Commands::ParsedCommand& pc, Commands::Result& res) override;
		void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText) override;
		bool sendMessage(const string& msg, bool thirdPerson = false) override;
		void addStatus(const tstring& line, bool inChat = true, bool history = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem) override;
		void readFrameLog() override;

	private:
		bool hubParamUpdated;
		int64_t bytesShared;
		CContainedWindow* ctrlChatContainer;
		bool showJoins;
		bool showFavJoins;

		void updateWindowTitle();
		void setWindowTitle(const string& text);
		tstring getHubTitle() const;
		
		bool userListInitialized;
		unsigned activateCounter;
		
		void updateSplitterPosition(int chatUserSplit, bool swapPanels);
		void initUserList(const FavoriteManager::WindowInfo& wi);
		void storeColumnsInfo();
		bool swapPanels;
		CButton ctrlSwitchPanels;
		CContainedWindow* switchPanelsContainer;
		static HIconWrapper iconSwitchPanels;
		CFlyToolTipCtrl tooltip;
		CButton ctrlShowUsers;
		CContainedWindow* showUsersContainer;
		
		OMenu tabMenu;
		bool  isTabMenuShown;
		
		CStatic ctrlModeIcon;
		static HIconWrapper iconModeActive;
		static HIconWrapper iconModePassive;
		static HIconWrapper iconModeNone;
		void updateModeIcon();

		void setSplitterPanes();
		void updateDisabledChatSettings();
		void addPasswordCommand();
		void createTabMenu();

	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_destroy);
};

#endif // !defined(HUB_FRAME_H)
