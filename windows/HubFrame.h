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
#include "../client/TaskQueue.h"

#include "TypedListViewCtrl.h"
#include "UserInfoBaseHandler.h"
#include "BaseChatFrame.h"
#include "TimerHelper.h"
#include "UCHandler.h"
#include "CustomDrawHelpers.h"

#define EDIT_MESSAGE_MAP 10     // This could be any number, really...
#ifndef FILTER_MESSAGE_MAP
#define FILTER_MESSAGE_MAP 11
#endif
#define HUBSTATUS_MESSAGE_MAP 12 // Status frame

struct CompareItems;

class HubFrame : public MDITabChildWindowImpl<HubFrame>,
	private ClientListener,
	public  CSplitterImpl<HubFrame>,
	public UCHandler<HubFrame>,
	public UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr >,
	private SettingsManagerListener,
	private FavoriteManagerListener,
	private UserManagerListener,
	public BaseChatFrame
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("HubFrame"), IDR_HUB, 0, COLOR_3DFACE);
		
		typedef CSplitterImpl<HubFrame> splitBase;
		typedef MDITabChildWindowImpl<HubFrame> baseClass;
		typedef UCHandler<HubFrame> ucBase;
		typedef UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr > uibBase;
		
		BEGIN_MSG_MAP(HubFrame)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETDISPINFO, ctrlUsers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_USERS, LVN_COLUMNCLICK, ctrlUsers.onColumnClick)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETINFOTIP, ctrlUsers.onInfoTip)
		NOTIFY_HANDLER(IDC_USERS, LVN_KEYDOWN, onKeyDownUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_DBLCLK, onDoubleClickUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_RETURN, onEnterUsers)
		//NOTIFY_HANDLER(IDC_USERS, LVN_ITEMCHANGED, onItemChanged) [-] IRainman opt
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onHubFrmCtlColor)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onStyleChange)
		MESSAGE_HANDLER(WM_CAPTURECHANGED, onStyleChanged)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, onSizeMove)
		MESSAGE_HANDLER(WMU_CHAT_LINK_CLICKED, onChatLinkClicked)

		CHAIN_MSG_MAP(BaseChatFrame)
		COMMAND_ID_HANDLER(ID_FILE_RECONNECT, onFileReconnect)
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
		NOTIFY_HANDLER(IDC_USERS, NM_CUSTOMDRAW, onCustomDraw)
		COMMAND_ID_HANDLER(IDC_COPY_HUBNAME, onCopyHubInfo)
		COMMAND_ID_HANDLER(IDC_COPY_HUBADDRESS, onCopyHubInfo)
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uibBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(splitBase)
		ALT_MSG_MAP(EDIT_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_KEYDOWN, onChar)
		MESSAGE_HANDLER(WM_KEYUP, onChar)
		MESSAGE_HANDLER(BM_SETCHECK, onShowUsers)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, onLButton)
		//MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		ALT_MSG_MAP(FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_CHAR, onFilterChar)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)
		ALT_MSG_MAP(HUBSTATUS_MESSAGE_MAP)
		//  COMMAND_ID_HANDLER(IDC_HUBS_SWITCHPANELS, onSwitchPanels)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onSwitchPanels)
		END_MSG_MAP()
		
		LRESULT onHubFrmCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyHubInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDoubleClickUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onShowUsers(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onFollow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onEnterUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onFilterChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onFileReconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSelectUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAutoScrollChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onUnBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onStyleChange(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onStyleChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSizeMove(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
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
		
		void switchPanels();
		LRESULT onSwitchPanels(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			switchPanels();
			return 0;
		}

		virtual void onBeforeActiveTab(HWND aWnd) override;
		virtual void onAfterActiveTab(HWND aWnd) override;
		virtual void onInvalidateAfterActiveTab(HWND aWnd) override;
		
		void UpdateLayout(BOOL resizeBars = TRUE);
		//void addLine(const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void addLine(const Identity& ou, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void addStatus(const tstring& aLine, const bool bInChat = true, const bool bHistory = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem);
		void onTab();
		void handleTab(bool reverse);
		void runUserCommand(::UserCommand& uc);
		void followRedirect();
		
		static HubFrame* openHubWindow(const string& server,
		                               const string& name = Util::emptyString,
									   const string rawCommands[] = nullptr,
		                               int  windowPosX = 0,
		                               int  windowPosY = 0,
		                               int  windowSizeX = 0,
		                               int  windowSizeY = 0,
		                               int  windowType = SW_MAXIMIZE,
		                               int  chatUserSplit = 5000,
		                               bool hideUserList = false,
		                               bool suppressChatAndPM = false);
		static void resortUsers();
		static void closeDisconnected();
		static void reconnectDisconnected();
		static void closeAll(size_t threshold = 0);
		static void updateAllTitles();
		
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
				clearUserList();
				client->refreshUserList(false);
			}
			return 0;
		}
		
		LRESULT onKeyDownUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			const NMLVKEYDOWN* l = (NMLVKEYDOWN*)pnmh;
			if (l->wVKey == VK_TAB)
			{
				onTab();
			}
			return 0;
		}
		
		typedef TypedListViewCtrl<UserInfo, IDC_USERS> CtrlUsers;
		CtrlUsers& getUserList() { return ctrlUsers; }
		
private:
		enum FilterModes
		{
			NONE,
			EQUAL,
			GREATER_EQUAL,
			LESS_EQUAL,
			GREATER,
			LESS,
			NOT_EQUAL
		};

		enum AutoConnectType
		{
			DONT_CHANGE = 0,
			UNSET = -1,
			SET = 1,
		};
		
		HubFrame(const string& server,
		         const string& name,
		         const string rawCommands[],
		         int  chatUserSplit,
		         bool hideUserList,
		         bool suppressChatAndPM);
		~HubFrame();
		
		virtual void doDestroyFrame();
		typedef boost::unordered_map<string, HubFrame*> FrameMap;
		static CriticalSection csFrames;
		static FrameMap frames;
		void removeFrame(const string& redirectUrl);
		
		int hubUpdateCount;
		string prevHubName;
		string prevTooltip;

		TaskQueue tasks;
		TimerHelper timer;

		void onTimerHubUpdated();
		int infoUpdateSeconds;
		string redirect;
		tstring m_complete;
		bool waitingForPassword;
		bool showingPasswordDlg;
		
		Client* client;
		string serverUrl;
		
		void setHubParam() { hubParamUpdated = true; }
		bool isConnected() const { return client && client->isConnected(); }
		
		CtrlUsers ctrlUsers;
		CustomDrawHelpers::CustomDrawState customDrawState;
		void createCtrlUsers();
		
		tstring lastUserName;
		
		bool showUsers;
		bool showUsersStore;
		
		void setShowUsers(bool value)
		{
			showUsers = value;
			showUsersStore = value;
		}
		bool isSuppressChatAndPM() const
		{
			return client && client->getSuppressChatAndPM();
		}
		void firstLoadAllUsers();
		size_t insertUsers();
		
		std::unique_ptr<RWLock> csUserMap;
		UserInfo::OnlineUserMap userMap;
		bool shouldUpdateStats;
		bool shouldSort;
		unsigned m_count_lock_chat;
		unsigned asyncUpdate;
		unsigned asyncUpdateSaved;
		
		static const int columnId[COLUMN_LAST];
		
		void onTimerInternal();
		void processTasks();
		void addTask(Tasks s, Task* task);
		
		bool updateUser(const OnlineUserPtr& ou, uint32_t columnMask); // returns true if this is a new user
		void removeUser(const OnlineUserPtr& ou);
		
		void insertUser(UserInfo* ui);
		void insertUserInternal(UserInfo* ui, int pos);
		void updateUserList();
		bool parseFilter(FilterModes& mode, int64_t& size);
		bool matchFilter(UserInfo& ui, int sel, bool doSizeCompare = false, FilterModes mode = NONE, int64_t size = 0);
		UserInfo* findUser(const tstring& nick);
		UserInfo* findUser(const OnlineUserPtr& user);
		
		void addAsFavorite(AutoConnectType autoConnectType = DONT_CHANGE);
		void removeFavoriteHub();
		
		void createFavHubMenu(bool isFav, bool isAutoConnect);
		void toggleAutoConnect();
		void clearUserList();
		void appendHubAndUsersItems(OMenu& menu, const bool isChat);
		void updateStats();
		
		// FavoriteManagerListener
		void on(FavoriteManagerListener::UserAdded, const FavoriteUser& user) noexcept override;
		void on(FavoriteManagerListener::UserRemoved, const FavoriteUser& user) noexcept override;
		void on(FavoriteManagerListener::UserStatusChanged, const UserPtr& user) noexcept override;
		void resortForFavsFirst(bool justDoIt = false);
		
		// UserManagerListener
		void on(UserManagerListener::IgnoreListChanged, const string& userName) noexcept override;
		void on(UserManagerListener::IgnoreListCleared) noexcept override;
		void on(UserManagerListener::ReservedSlotChanged, const UserPtr& user) noexcept override;

		// SettingsManagerListener
		void on(SettingsManagerListener::Repaint) override;
		
		// ClientListener
		void on(ClientListener::Connecting, const Client*) noexcept override;
		void on(ClientListener::Connected, const Client*) noexcept override;
		void on(ClientListener::UserUpdated, const OnlineUserPtr&) noexcept override;
		void on(ClientListener::UserListUpdated, const Client*, const OnlineUserList&) noexcept override;
		void on(ClientListener::UserRemoved, const Client*, const OnlineUserPtr&) noexcept override;
		void on(ClientListener::Redirect, const Client*, const string&) noexcept override;
		void on(ClientListener::ClientFailed, const Client*, const string&) noexcept override;
		void on(ClientListener::GetPassword, const Client*) noexcept override;
		void on(ClientListener::HubUpdated, const Client*) noexcept override;
		void on(ClientListener::Message, const Client*, std::unique_ptr<ChatMessage>&) noexcept override;
		//void on(PrivateMessage, const Client*, const string &strFromUserName, const UserPtr&, const UserPtr&, const UserPtr&, const string&, bool = true) noexcept override; // !SMT!-S [-] IRainman fix.
		void on(ClientListener::NickError, ClientListener::NickErrorCode nickError) noexcept override;
		void on(ClientListener::HubFull, const Client*) noexcept override;
		void on(ClientListener::CheatMessage, const string&) noexcept override;
		void on(ClientListener::UserReport, const Client*, const string&) noexcept override;
		void on(ClientListener::HubInfoMessage, ClientListener::HubInfoCode code, const Client* client, const string& line) noexcept override;
		void on(ClientListener::StatusMessage, const Client*, const string& line, int statusFlags) noexcept override;
		void on(ClientListener::DDoSSearchDetect, const string&) noexcept override;
		
		struct StatusTask : public Task
		{
			explicit StatusTask(const string& msg, bool isInChat) : str(msg), isInChat(isInChat) { }
			const string str;
			const bool isInChat;
		};
		void updateUserJoin(const OnlineUserPtr& ou);
		void doDisconnected();
		void doConnected();
		void clearTaskAndUserList();
		void getUserColor(bool isOp, const UserPtr& user, COLORREF& fg, COLORREF& bg, unsigned short& flags, const OnlineUserPtr& onlineUser);

	public:
		static void addDupeUsersToSummaryMenu(const ClientManager::UserParams& param);
		void sendMessage(const tstring& msg, bool thirdperson = false)
		{
			dcassert(client);
			if (client)
				client->hubMessage(Text::fromT(msg), thirdperson);
		}
		void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText);
		void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText);
		
		StringMap getFrameLogParams() const;
		void readFrameLog();
		void openFrameLog() const
		{
			WinUtil::openLog(SETTING(LOG_FILE_MAIN_CHAT), getFrameLogParams(), TSTRING(NO_LOG_FOR_HUB));
		}

	private:
		CEdit ctrlFilter;
		CComboBox ctrlFilterSel;
		int filterSelPos;
		int getFilterSelPos() const
		{
			return ctrlFilterSel.m_hWnd ? ctrlFilterSel.GetCurSel() : filterSelPos;
		}
		tstring filter;
		tstring filterLower;
		bool hubParamUpdated;
		int64_t bytesShared;
		CContainedWindow* ctrlFilterContainer;
		CContainedWindow* ctrlChatContainer;
		CContainedWindow* ctrlFilterSelContainer;
		bool showJoins;
		bool showFavJoins;

		void updateWindowTitle();
		void setWindowTitle(const string& text);
		
		bool updateColumnsInfoProcessed;
		bool m_is_ddos_detect;
		unsigned activateCounter;
		
		void updateSplitterPosition(int chatUserSplit, bool swapPanels);
		void updateColumnsInfo(const FavoriteManager::WindowInfo& wi);
		void storeColumnsInfo();
		bool swapPanels;
		CButton ctrlSwitchPanels;
		CContainedWindow* switchPanelsContainer;
		static HIconWrapper g_hSwitchPanelsIco;
		CFlyToolTipCtrl tooltip;
		CButton ctrlShowUsers;
		CContainedWindow* showUsersContainer;
		
		OMenu* tabMenu;
		bool   isTabMenuShown;
		
#ifdef SCALOLAZ_HUB_MODE
		CStatic ctrlShowMode;
		static HIconWrapper g_hModeActiveIco;
		static HIconWrapper g_hModePassiveIco;
		static HIconWrapper g_hModeNoneIco;
		void updateHubMode();
#endif
		void setSplitterPanes();
		void addPasswordCommand();
		OMenu* createTabMenu();
		void destroyTabMenu();

	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_destroy);
};

#endif // !defined(HUB_FRAME_H)
