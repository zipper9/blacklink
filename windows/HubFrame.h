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

#include "../client/DCPlusPlus.h" // [!] IRainman fix whois entery on menu 
#include "../client/User.h"
#include "../client/ClientManager.h"
#include "../client/DirectoryListing.h"
#include "../client/SimpleXML.h"
#include "../client/ConnectionManager.h"

#include "UserInfoBaseHandler.h"
#include "BaseChatFrame.h"
#include "UCHandler.h"
#include "ImageLists.h"

#define EDIT_MESSAGE_MAP 10     // This could be any number, really...
#ifndef FILTER_MESSAGE_MAP
#define FILTER_MESSAGE_MAP 11
#endif
#define HUBSTATUS_MESSAGE_MAP 12 // Status frame

#define FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME

struct CompareItems;

class HubFrame : public MDITabChildWindowImpl<HubFrame>,
	private ClientListener,
	public  CSplitterImpl<HubFrame>,
	private CFlyTimerAdapter,
	private CFlyTaskAdapter,
	public UCHandler<HubFrame>,
	public UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr >,
	private SettingsManagerListener,
	private FavoriteManagerListener,
	public BaseChatFrame // [+] IRainman copy-past fix.
#ifdef RIP_USE_CONNECTION_AUTODETECT
	, private ConnectionManagerListener // [+] FlylinkDC
#endif
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("HubFrame"), IDR_HUB, 0, COLOR_3DFACE);
		
		typedef CSplitterImpl<HubFrame> splitBase;
		typedef MDITabChildWindowImpl<HubFrame> baseClass;
		typedef UCHandler<HubFrame> ucBase;
		typedef UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr > uibBase;
		
		BEGIN_MSG_MAP(HubFrame)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		//MESSAGE_HANDLER(WM_SPEAKER_FIRST_USER_JOIN, OnSpeakerFirstUserJoin)
		// MESSAGE_RANGE_HANDLER(WM_SPEAKER_BEGIN, WM_SPEAKER_END, OnSpeakerRange)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETDISPINFO, ctrlUsers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_USERS, LVN_COLUMNCLICK, ctrlUsers.onColumnClick)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETINFOTIP, ctrlUsers.onInfoTip)
		NOTIFY_HANDLER(IDC_USERS, LVN_KEYDOWN, onKeyDownUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_DBLCLK, onDoubleClickUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_RETURN, onEnterUsers)
		//NOTIFY_HANDLER(IDC_USERS, LVN_ITEMCHANGED, onItemChanged) [-] IRainman opt
		MESSAGE_HANDLER(WM_CLOSE, onClose)
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
		MESSAGE_HANDLER(WM_TIMER, onTimer)
#endif
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
		CHAIN_MSG_MAP(BaseChatFrame)
		COMMAND_ID_HANDLER(ID_FILE_RECONNECT, onFileReconnect)
		COMMAND_ID_HANDLER(ID_DISCONNECT, onDisconnect)
		COMMAND_ID_HANDLER(IDC_REFRESH, onRefresh)
		COMMAND_ID_HANDLER(IDC_FOLLOW, onFollow)
		COMMAND_ID_HANDLER(IDC_SEND_MESSAGE, onSendMessage)
		COMMAND_ID_HANDLER(IDC_ADD_AS_FAVORITE, onAddAsFavorite)
		COMMAND_ID_HANDLER(IDC_REM_AS_FAVORITE, onRemAsFavorite)
		COMMAND_ID_HANDLER(IDC_AUTO_START_FAVORITE, onAutoStartFavorite)
		COMMAND_ID_HANDLER(IDC_EDIT_HUB_PROP, onEditHubProp)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindows)            // [~] SCALOlaz
		COMMAND_ID_HANDLER(IDC_RECONNECT_DISCONNECTED, onCloseWindows)  // [+] SCALOlaz
		COMMAND_ID_HANDLER(IDC_CLOSE_DISCONNECTED, onCloseWindows)      // [+] SCALOlaz
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
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		ALT_MSG_MAP(HUBSTATUS_MESSAGE_MAP)
		//  COMMAND_ID_HANDLER(IDC_HUBS_SWITCHPANELS, onSwitchPanels)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onSwitchPanels)
#endif
		END_MSG_MAP()
		
		LRESULT onHubFrmCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT OnSpeakerRange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
//		LRESULT OnSpeakerFirstUserJoin(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
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
		
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#endif
		
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		void OnSwitchedPanels();
		LRESULT onSwitchPanels(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			OnSwitchedPanels();
			return 0;
		}
#endif
		virtual void onBeforeActiveTab(HWND aWnd) override;
		virtual void onAfterActiveTab(HWND aWnd) override;
		virtual void onInvalidateAfterActiveTab(HWND aWnd) override;
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void addLine(const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void addLine(const Identity& ou, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void addStatus(const tstring& aLine, const bool bInChat = true, const bool bHistory = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem);
		void onTab();
		void handleTab(bool reverse);
		void runUserCommand(::UserCommand& uc);
		
		static HubFrame* openHubWindow(const string& server,
		                               const string& name     = Util::emptyString,
		                               const string& rawOne   = Util::emptyString,
		                               const string& rawTwo   = Util::emptyString,
		                               const string& rawThree = Util::emptyString,
		                               const string& rawFour  = Util::emptyString,
		                               const string& rawFive  = Util::emptyString,
		                               int  windowPosX = 0,
		                               int  windowPosY = 0,
		                               int  windowSizeX = 0,
		                               int  windowSizeY = 0,
		                               int  windowType = SW_MAXIMIZE,
		                               int  chatUserSplit = 5000,
		                               bool userListState = true,
		                               bool suppressChatAndPM = false);
		static void resortUsers();
		static void closeDisconnected();
		static void reconnectDisconnected();
		static void closeAll(size_t thershold = 0);
		
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
		LRESULT onAutoStartFavorite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			autoConnectStart();
			return 0;
		}
		LRESULT onEditHubProp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCloseWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(m_client);
			if (isConnected())
			{
				clearUserList();
				m_client->refreshUserList(false);
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
		
#ifndef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
		static void timer_process_all();
#endif
		
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
		
		HubFrame(const string& server,
		         const string& name,
		         const string& rawOne,
		         const string& rawTwo,
		         const string& rawThree,
		         const string& rawFour,
		         const string& rawFive,
		         int  chatUserSplit,
		         bool userListState,
		         bool suppressChatAndPM);
		~HubFrame();
		
		virtual void doDestroyFrame();
		typedef boost::unordered_map<string, HubFrame*> FrameMap;
		static CriticalSection g_frames_cs;
		static FrameMap g_frames;
		void erase_frame(const string& p_redirect);
		void timer_process_internal();
		
		tstring shortHubName;
		int hubUpdateCount;
		string prevHubName;

		bool m_is_process_disconnected;
		bool m_is_first_goto_end;
		void onTimerHubUpdated();
		int8_t m_upnp_message_tick;
		uint8_t m_second_count;
		void setShortHubName(const tstring& name);
		string m_redirect;
		string m_last_redirect;
		tstring m_complete;
		tstring m_last_hub_message;
		bool m_waitingForPW;
		uint8_t m_password_do_modal;
		
		Client* m_client;
		string m_server;
		
		void setHubParam()
		{
			++m_is_hub_param_update;
		}
		bool isConnected() const
		{
			return m_client && m_client->isConnected();
		}
		CContainedWindow ctrlClientContainer;
		
		CtrlUsers ctrlUsers;
		bool ctrlUsersFocused;
		void createCtrlUsers();
		
		tstring m_lastUserName; // SSA_SAVE_LAST_NICK_MACROS
		
		bool m_showUsers;
		bool m_showUsersStore;
		
		void setShowUsers(bool m_value)
		{
			m_showUsers = m_value;
			m_showUsersStore = m_value;
		}
		bool isSupressChatAndPM() const
		{
			return m_client && m_client->isSupressChatAndPM();
		}
		void firstLoadAllUsers();
		unsigned usermap2ListrView();
		
		std::unique_ptr<webrtc::RWLockWrapper> m_userMapCS;
		//CriticalSection m_userMapCS;
		UserInfo::OnlineUserMap m_userMap;
		bool m_needsUpdateStats;
		bool m_needsResort;
		bool m_is_init_load_list_view;
		int m_count_init_insert_list_view;
		unsigned m_last_count_resort;
		unsigned m_count_speak;
		unsigned m_count_lock_chat;
		
		static int g_columnIndexes[COLUMN_LAST];
		static int g_columnSizes[COLUMN_LAST];
		
		bool updateUser(const OnlineUserPtr& p_ou, const int p_index_column);
		void removeUser(const OnlineUserPtr& p_ou);
		
		void InsertUserList(UserInfo* ui);
		void InsertItemInternal(const UserInfo* ui);
		void updateUserList(); // [!] IRainman opt.
		bool parseFilter(FilterModes& mode, int64_t& size);
		bool matchFilter(UserInfo& ui, int sel, bool doSizeCompare = false, FilterModes mode = NONE, int64_t size = 0);
		UserInfo* findUser(const tstring& p_nick);   // !SMT!-S
		UserInfo* findUser(const OnlineUserPtr& p_user);
		
		FavoriteHubEntry* addAsFavorite(const FavoriteManager::AutoStartType p_autoconnect = FavoriteManager::NOT_CHANGE);// [!] IRainman fav options
		void removeFavoriteHub();
		
		void createFavHubMenu(const FavoriteHubEntry* fhe);
		
		void autoConnectStart();
		
		void clearUserList();
		void clearTaskList();
		
		void appendHubAndUsersItems(OMenu& p_menu, const bool isChat);
		
		// FavoriteManagerListener
		void on(FavoriteManagerListener::UserAdded, const FavoriteUser& /*aUser*/) noexcept override;
		void on(FavoriteManagerListener::UserRemoved, const FavoriteUser& /*aUser*/) noexcept override;
		void resortForFavsFirst(bool justDoIt = false);
		
		// SettingsManagerListener
		void on(SettingsManagerListener::Repaint) override;
		
		// ClientListener
		void on(ClientListener::Connecting, const Client*) noexcept override;
		void on(ClientListener::Connected, const Client*) noexcept override;
		void on(ClientListener::UserDescUpdated, const OnlineUserPtr&) noexcept override;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
		void on(ClientListener::UserShareUpdated, const OnlineUserPtr&) noexcept override;
#endif
		void on(ClientListener::UserUpdatedMyINFO, const OnlineUserPtr&) noexcept override; // !SMT!-fix
		void on(ClientListener::UsersUpdated, const Client*, const OnlineUserList&) noexcept override;
		void on(ClientListener::UserRemoved, const Client*, const OnlineUserPtr&) noexcept override;
		void on(ClientListener::Redirect, const Client*, const string&) noexcept override;
		void on(ClientListener::ClientFailed, const Client*, const string&) noexcept override;
		void on(ClientListener::GetPassword, const Client*) noexcept override;
		void on(ClientListener::HubUpdated, const Client*) noexcept override;
		void on(ClientListener::Message, const Client*, std::unique_ptr<ChatMessage>&) noexcept override;
		//void on(PrivateMessage, const Client*, const string &strFromUserName, const UserPtr&, const UserPtr&, const UserPtr&, const string&, bool = true) noexcept override; // !SMT!-S [-] IRainman fix.
		void on(ClientListener::NickTaken) noexcept override;
		void on(ClientListener::HubFull, const Client*) noexcept override;
		void on(ClientListener::FirstExtJSON, const Client*) noexcept override;
		void on(ClientListener::CheatMessage, const string&) noexcept override;
		void on(ClientListener::UserReport, const Client*, const string&) noexcept override; // [+] IRainman
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
		void on(ClientListener::HubTopic, const Client*, const string&) noexcept override;
#endif
		void on(ClientListener::StatusMessage, const Client*, const string& line, int statusFlags) noexcept override;
#ifdef RIP_USE_CONNECTION_AUTODETECT
		void on(ConnectionManagerListener::OpenTCPPortDetected, const string&) noexcept override;
#endif
		void on(ClientListener::DDoSSearchDetect, const string&) noexcept override;
		
		struct StatusTask : public Task
		{
			explicit StatusTask(const string& p_msg, bool p_isInChat) : m_str(p_msg), m_isInChat(p_isInChat) { }
			const string m_str;
			const bool m_isInChat;
		};
		void updateUserJoin(const OnlineUserPtr& p_ou);
		void speak(Tasks s)
		{
			m_tasks.add(static_cast<uint8_t>(s), nullptr);
		}
#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
		void speak(Tasks s, const OnlineUserPtr& u)
		{
			m_tasks.add(static_cast<uint8_t>(s), new OnlineUserTask(u));
		}
#endif
		// [~] !SMT!-S
		
		void speak(Tasks s, const string& msg, bool inChat = true)
		{
			m_tasks.add(static_cast<uint8_t>(s), new StatusTask(msg, inChat));
		}
#ifndef FLYLINKDC_PRIVATE_MESSAGE_USE_WIN_MESSAGES_Q
		void speak(Tasks s, ChatMessage* p_message_ptr)
		{
			m_tasks.add(static_cast<uint8_t>(s), new MessageTask(p_message_ptr));
			if (++m_count_speak < 2)
			{
				force_speak();
			}
		}
#endif
		void doDisconnected();
		void doConnected();
		void clearTaskAndUserList();
	public:
		static void addDupeUsersToSummaryMenu(ClientManager::UserParams& p_param); // !SMT!-UI
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		bool isFlyAntivirusHub() const
		{
			dcassert(m_client);
			return m_client && m_client->isFlyAntivirusHub();
		}
#endif
		// [+] IRainman: copy-past fix.
		void sendMessage(const tstring& msg, bool thirdperson = false)
		{
			dcassert(m_client);
			if (m_client)
				m_client->hubMessage(Text::fromT(msg), thirdperson);
		}
		void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText);
		void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText);
		
		void addMesageLogParams(StringMap& params, tstring aLine, bool bThirdPerson, tstring extra);
		void addFrameLogParams(StringMap& params);
		
		StringMap getFrameLogParams() const;
		void readFrameLog();
		void openFrameLog() const
		{
			WinUtil::openLog(SETTING(LOG_FILE_MAIN_CHAT), getFrameLogParams(), TSTRING(NO_LOG_FOR_HUB));
		}
	private:
		CEdit ctrlFilter;
		CComboBox ctrlFilterSel;
		int m_FilterSelPos;
		int getFilterSelPos() const
		{
			return ctrlFilterSel.m_hWnd ? ctrlFilterSel.GetCurSel() : m_FilterSelPos;
		}
		tstring filter;
		tstring filterLower;
		string m_window_text;
		uint8_t m_is_window_text_update;
		uint8_t m_is_hub_param_update;
		void setWindowTitle(const string& p_text);
		void updateWindowText();
		CContainedWindow* m_ctrlFilterContainer;
		CContainedWindow* m_ctrlChatContainer;
		CContainedWindow* m_ctrlFilterSelContainer;
		bool m_showJoins;
		bool m_favShowJoins;
		void initShowJoins(const FavoriteHubEntry *p_fhe);
		
		bool m_isUpdateColumnsInfoProcessed;
		bool m_is_ddos_detect;
		size_t m_ActivateCounter;
		
		void updateSplitterPosition(const FavoriteHubEntry *p_fhe);
		void updateColumnsInfo(const FavoriteHubEntry *p_fhe);
		void storeColumsInfo();
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		bool m_isClientUsersSwitch;
		CButton m_ctrlSwitchPanels;
		CContainedWindow* m_switchPanelsContainer;
		static HIconWrapper g_hSwitchPanelsIco;
#endif
		CFlyToolTipCtrl tooltip;
		CButton m_ctrlShowUsers;
		void setShowUsersCheck()
		{
			m_ctrlShowUsers.SetCheck((m_ActivateCounter == 1 ? m_showUsersStore : m_showUsers) ? BST_CHECKED : BST_UNCHECKED);
		}
		CContainedWindow* m_showUsersContainer;
		
		OMenu* m_tabMenu;
		bool   m_isTabMenuShown;
		
#ifdef SCALOLAZ_HUB_MODE
		CStatic ctrlShowMode;
		static HIconWrapper g_hModeActiveIco;
		static HIconWrapper g_hModePassiveIco;
		static HIconWrapper g_hModeNoneIco;
		void HubModeChange();
#endif
		void TuneSplitterPanes();
		void addPasswordCommand();
		OMenu* createTabMenu();
		void destroyTabMenu();
	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_destroy);
		
};

#endif // !defined(HUB_FRAME_H)
