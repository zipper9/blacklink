#ifndef USER_INFO_BASE_HANDLER_H
#define USER_INFO_BASE_HANDLER_H

#include "OMenu.h"
#include "resource.h"
#include "ImageLists.h"
#include "../client/CID.h"
#include "../client/LogManager.h"

class UserInfo;
class UserInfoBase;
struct FavUserTraits;

struct UserInfoGuiTraits // internal class, please do not use it directly!
{
		friend class UserInfoSimple;

	public:
		struct DetailsItem
		{
			enum
			{
				TYPE_USER,
				TYPE_FAV_INFO,
				TYPE_TAG,
				TYPE_QUEUED
			};

			int type;
			UINT id;
			UINT flags;
			tstring text;
			CID cid;
			string hubUrl;
		};

		static void init(); // only for WinUtil!
		static void uninit(); // only for WinUtil!
		static bool isUserInfoMenu(HMENU handle) // only for WinUtil!
		{
			return handle == userSummaryMenu.m_hMenu ||
			       handle == copyUserMenu.m_hMenu ||
			       handle == grantMenu.m_hMenu ||
			       handle == speedMenu.m_hMenu ||
			       handle == privateMenu.m_hMenu ||
			       handle == favUserMenu.m_hMenu;
		}

		enum Options
		{
			DEFAULT = 0,
			NO_SEND_PM = 1,
			NO_CONNECT_FAV_HUB = 2,
			NO_COPY = 4, // Please disable if your want custom copy menu, for user items please use copyUserMenu.
			NO_FILE_LIST = 8,
			NICK_TO_CHAT = 16,
			USER_LOG = 32,
			INLINE_CONTACT_LIST = 64,
		};

	protected:
		static int getCtrlIdBySpeedLimit(int limit);
		static bool getSpeedLimitByCtrlId(WORD wID, int& lim, const tstring& nick);
		static void updateSpeedMenuText(int customSpeed);
		static void processDetailsMenu(WORD id);
		static void copyUserInfo(WORD idc, const Identity& id);
		static void addSummaryMenu(const OnlineUserPtr& ou);

		static bool ENABLE(int HANDLERS, Options FLAG)
		{
			return (HANDLERS & FLAG) != 0;
		}
		static bool DISABLE(int HANDLERS, Options FLAG)
		{
			return (HANDLERS & FLAG) == 0;
		}
		static string g_hubHint;
		
		static OMenu copyUserMenu;
		static OMenu grantMenu;
		static OMenu speedMenu;
		static OMenu privateMenu;
		static OMenu favUserMenu;
		static int speedMenuCustomVal;
		static int displayedSpeed[2];

		static OMenu userSummaryMenu;

		static vector<DetailsItem> detailsItems;
		static UINT detailsItemMaxId;

		static const UINT IDC_SPEED_VALUE = 30000; // must be high enough so it won't clash with normal IDCs
};

template<class T>
struct UserInfoBaseHandlerTraitsUser // technical class, please do not use it directly!
{
	protected:
		static T g_user;
		static string getNick(const T& user);
};

template<class T, const int options = UserInfoGuiTraits::DEFAULT, class T2 = UserPtr>
class UserInfoBaseHandler : UserInfoBaseHandlerTraitsUser<T2>, public UserInfoGuiTraits
{
		/*
		1) If USER_LOG is set, openUserLog must be implemented; if NICK_TO_CHAT is set, addNickToChat must be implemented
		2) clearUserMenu()
		3) reinitUserMenu(user, hint)
		4) appendAndActivateUserItems(yourMenu)
		5) Before you destroy the menu in your class you will definitely need to call WinUtil::unlinkStaticMenus(yourMenu)
		*/
	public:
		UserInfoBaseHandler() : selectedHint(UserInfoGuiTraits::g_hubHint), selectedUser(UserInfoBaseHandlerTraitsUser::g_user)
#ifdef _DEBUG
			, _debugIsClean(true)
#endif
		{
		}
		
		BEGIN_MSG_MAP(UserInfoBaseHandler)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetList)
		COMMAND_ID_HANDLER(IDC_BROWSELIST, onBrowseList)
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		COMMAND_ID_HANDLER(IDC_CHECKLIST, onCheckList)
#endif
		COMMAND_ID_HANDLER(IDC_GET_USER_RESPONSES, onGetUserResponses)
		COMMAND_ID_HANDLER(IDC_MATCH_QUEUE, onMatchQueue)
		COMMAND_ID_HANDLER(IDC_PRIVATE_MESSAGE, onPrivateMessage)
		COMMAND_ID_HANDLER(IDC_ADD_NICK_TO_CHAT, onAddNickToChat)
		COMMAND_ID_HANDLER(IDC_OPEN_USER_LOG, onOpenUserLog)
		COMMAND_ID_HANDLER(IDC_ADD_TO_FAVORITES, onAddToFavorites)
		COMMAND_ID_HANDLER(IDC_REMOVE_FROM_FAVORITES, onRemoveFromFavorites)
		COMMAND_ID_HANDLER(IDC_IGNORE_BY_NAME, onIgnoreOrUnignoreUserByName)
		COMMAND_RANGE_HANDLER(IDC_GRANTSLOT, IDC_UNGRANTSLOT, onGrantSlot)
		COMMAND_RANGE_HANDLER(IDC_PM_NORMAL, IDC_PM_FREE, onPrivateAccess)
		COMMAND_RANGE_HANDLER(IDC_SPEED_NORMAL, IDC_SPEED_MANUAL, onSetUserLimit)
		COMMAND_RANGE_HANDLER(IDC_SPEED_VALUE, IDC_SPEED_VALUE + 256, onSetUserLimit)
		COMMAND_RANGE_HANDLER(IDC_USER_INFO, UserInfoGuiTraits::detailsItemMaxId, onDetailsMenu)
		COMMAND_ID_HANDLER(IDC_REMOVEALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_REPORT, onReport)
		COMMAND_ID_HANDLER(IDC_CONNECT, onConnectFav)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_EXACT_SHARE, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_APPLICATION, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_TAG, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_CID, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_DESCRIPTION, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_EMAIL_ADDRESS, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_GEO_LOCATION, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_NICK_IP, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_IP, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_ALL, onCopyUserInfo)
		END_MSG_MAP()

		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			static_cast<T*>(this)->openUserLog();
			return 0;
		}

		LRESULT onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			static_cast<T*>(this)->addNickToChat();
			return 0;
		}

		LRESULT onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			OnlineUserPtr ou = static_cast<T*>(this)->getSelectedOnlineUser();
			if (ou)
			{
				const Identity& id = ou->getIdentity();
				copyUserInfo(wID, id);
				return 0;
			}
			if (wID == IDC_COPY_NICK)
			{
				const auto& su = getSelectedUser();
				if (su)
					WinUtil::setClipboard(Text::toT(UserInfoBaseHandlerTraitsUser<T2>::getNick(su)));
			}
			return 0;
		}

		LRESULT onMatchQueue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::matchQueue);
			return 0;
		}
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::getList);
			return 0;
		}
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::browseList);
			return 0;
		}
		LRESULT onReport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::doReport, selectedHint);
			return 0;
		}
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		LRESULT onCheckList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::checkList);
			return 0;
		}
#endif
		LRESULT onGetUserResponses(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::getUserResponses);
			return 0;
		}
		
		LRESULT onAddToFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::addFav);
			return 0;
		}

		LRESULT onRemoveFromFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::delFav);
			return 0;
		}

		LRESULT onSetUserLimit(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			tstring nick;
			const auto& su = getSelectedUser();
			if (su) nick = Text::toT(UserInfoBaseHandlerTraitsUser<T2>::getNick(su));
			int lim = speedMenuCustomVal;
			if (getSpeedLimitByCtrlId(wID, lim, nick)) doAction(&UserInfoBase::setUploadLimit, lim);
			return 0;
		}

		LRESULT onPrivateAccess(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			switch (wID)
			{
				case IDC_PM_IGNORED:
					doAction(&UserInfoBase::setIgnorePM);
					break;
				case IDC_PM_FREE:
					doAction(&UserInfoBase::setFreePM);
					break;
				case IDC_PM_NORMAL:
					doAction(&UserInfoBase::setNormalPM);
					break;
			};
			return 0;
		}

		LRESULT onIgnoreOrUnignoreUserByName(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::ignoreOrUnignoreUserByName);
			return 0;
		}
		
		LRESULT onPrivateMessage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (selectedUser)
			{
				(UserInfoSimple(selectedUser, selectedHint).pm)(selectedHint);
			}
			else
			{
				vector<T2> selected;
				static_cast<T*>(this)->getSelectedUsers(selected);
				if (selected.size() > 1)
				{
					const tstring message = UserInfoSimple::getBroadcastPrivateMessage();
					if (!message.empty())
						for (const auto& u : selected)
							UserInfoSimple(u, selectedHint).pmText(selectedHint, message);
				}
				else if (!selected.empty())
				{
					UserInfoSimple(selected[0], selectedHint).pm(selectedHint);
				}
			}
			return 0;
		}
		
		LRESULT onConnectFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::connectFav);
			return 0;
		}

		LRESULT onGrantSlot(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			switch (wID)
			{
				case IDC_GRANTSLOT:
					doAction(&UserInfoBase::grantSlotPeriod, selectedHint, 600);
					break;
				case IDC_GRANTSLOT_HOUR:
					doAction(&UserInfoBase::grantSlotPeriod, selectedHint, 3600);
					break;
				case IDC_GRANTSLOT_DAY:
					doAction(&UserInfoBase::grantSlotPeriod, selectedHint, 24 * 3600);
					break;
				case IDC_GRANTSLOT_WEEK:
					doAction(&UserInfoBase::grantSlotPeriod, selectedHint, 7 * 24 * 3600);
					break;
				case IDC_GRANTSLOT_PERIOD:
				{
					const uint64_t slotTime = UserInfoSimple::inputSlotTime();
					doAction(&UserInfoBase::grantSlotPeriod, selectedHint, slotTime);
				}
				break;
				case IDC_UNGRANTSLOT:
					doAction(&UserInfoBase::ungrantSlot, selectedHint);
					break;
			}
			return 0;
		}

		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::removeAll);
			return 0;
		}

		LRESULT onDetailsMenu(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			processDetailsMenu(wID);
			return 0;
		}

		OnlineUserPtr getSelectedOnlineUser() const
		{
			return OnlineUserPtr();
		}

		void openUserLog()
		{
			dcassert(0);
			LogManager::message("openUserLog is not implemented");
		}

		void addNickToChat()
		{
			dcassert(0);
			LogManager::message("addNickToChat is not implemented");
		}

		const T2& getSelectedUser() const
		{
			return selectedUser;
		}

		const string& getSelectedHint() const
		{
			return selectedHint;
		}

		void clearUserMenu()
		{
			selectedHint.clear();
			selectedUser = nullptr;
			userSummaryMenu.ClearMenu();
			
			for (int j = 0; j < speedMenu.GetMenuItemCount(); ++j)
			{
				speedMenu.CheckMenuItem(j, MF_BYPOSITION | MF_UNCHECKED);
			}
			
			privateMenu.CheckMenuItem(IDC_PM_NORMAL, MF_BYCOMMAND | MF_UNCHECKED);
			privateMenu.CheckMenuItem(IDC_PM_IGNORED, MF_BYCOMMAND | MF_UNCHECKED);
			privateMenu.CheckMenuItem(IDC_PM_FREE, MF_BYCOMMAND | MF_UNCHECKED);
			
			dcdrun(_debugIsClean = true;)
		}
		
		void reinitUserMenu(const T2& user, const string& hint)
		{
			selectedHint = hint;
			selectedUser = user;
		}

		void appendAndActivateUserItems(OMenu& menu, bool useOnlyFirstItem = true)
		{
			dcassert(_debugIsClean);
			dcdrun(_debugIsClean = false;)
			
			if (selectedUser)
			{
				appendAndActivateUserItemsForSingleUser(menu, selectedHint);
			}
			else
			{
				vector<T2> selected;
				static_cast<T*>(this)->getSelectedUsers(selected);
				if (!selected.empty())
				{
					FavUserTraits traits; // empty
					if (ENABLE(options, NICK_TO_CHAT))
						menu.AppendMenu(MF_STRING, IDC_ADD_NICK_TO_CHAT, CTSTRING(ADD_NICK_TO_CHAT));
					appendSendAutoMessageItems(menu, selected.size());
					appendFileListItems(menu, traits);
					appendContactListMenu(menu, traits);
					appendFavPrivateMenu(menu);
					appendIgnoreByNameItem(menu, traits);
					appendGrantItems(menu);
					appendSpeedLimitMenu(menu, 0);
					appendSeparator(menu);					
					appendRemoveAllItems(menu);
					appendSeparator(menu);
				}
			}
		}
		
	private:	
		void appendAndActivateUserItemsForSingleUser(OMenu& menu, const string& hint)
		{
			dcassert(selectedUser);
			
			UserInfoSimple ui(selectedUser, hint);
			FavUserTraits traits;
			traits.init(ui);
			OnlineUserPtr ou = static_cast<T*>(this)->getSelectedOnlineUser();
			if (ou) addSummaryMenu(ou);
			if (ENABLE(options, NICK_TO_CHAT))
			{
				menu.AppendMenu(MF_STRING, IDC_ADD_NICK_TO_CHAT, CTSTRING(ADD_NICK_TO_CHAT));
			}
			if (ENABLE(options, USER_LOG))
			{
				menu.AppendMenu(MF_STRING | (!BOOLSETTING(LOG_PRIVATE_CHAT) ? MF_DISABLED : 0), IDC_OPEN_USER_LOG, CTSTRING(OPEN_USER_LOG), g_iconBitmaps.getBitmap(IconBitmaps::LOGS, 0));
			}
			appendSendAutoMessageItems(menu, 1);
			appendCopyMenuForSingleUser(menu);
			appendFileListItems(menu, traits);
			appendContactListMenu(menu, traits);
			appendFavPrivateMenu(menu);
			appendIgnoreByNameItem(menu, traits);
			appendGrantItems(menu);
			appendSpeedLimitMenu(menu, traits.uploadLimit);
			appendSeparator(menu);
			appendRemoveAllItems(menu);
			appendUserSummaryMenu(menu);
			activateFavPrivateMenuForSingleUser(menu, traits);
			activateSpeedLimitMenuForSingleUser(menu, traits);
		}
		
		static void appendSeparator(OMenu& menu)
		{
			menu.AppendMenu(MF_SEPARATOR);
		}
		
		static void appendUserSummaryMenu(OMenu& menu)
		{
			if (userSummaryMenu.GetMenuItemCount() > 1)
				menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)userSummaryMenu, CTSTRING(USER_SUMMARY));
		}
		
		static void appendFavPrivateMenu(OMenu& menu)
		{
			menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)privateMenu, CTSTRING(PM_HANDLING), g_iconBitmaps.getBitmap(IconBitmaps::MESSAGES, 0));
		}
		
		void activateFavPrivateMenuForSingleUser(OMenu& menu, const FavUserTraits& traits)
		{
			dcassert(selectedUser);
			if (traits.isIgnoredPm)
			{
				privateMenu.CheckMenuItem(IDC_PM_IGNORED, MF_BYCOMMAND | MF_CHECKED);
			}
			else if (traits.isFreePm)
			{
				privateMenu.CheckMenuItem(IDC_PM_FREE, MF_BYCOMMAND | MF_CHECKED);
			}
			else
			{
				privateMenu.CheckMenuItem(IDC_PM_NORMAL, MF_BYCOMMAND | MF_CHECKED);
			}
		}
		
		static void appendSpeedLimitMenu(OMenu& menu, int customSpeedVal)
		{
			updateSpeedMenuText(customSpeedVal);
			menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)speedMenu, CTSTRING(UPLOAD_SPEED_LIMIT), g_iconBitmaps.getBitmap(IconBitmaps::LIMIT, 0));
		}
		
		void activateSpeedLimitMenuForSingleUser(OMenu& menu, const FavUserTraits& traits)
		{
			dcassert(selectedUser);
			const int id = getCtrlIdBySpeedLimit(traits.uploadLimit);
			speedMenu.CheckMenuItem(id, MF_BYCOMMAND | MF_CHECKED);
			speedMenuCustomVal = traits.uploadLimit;
		}
		
		static void appendIgnoreByNameItem(OMenu& menu, const FavUserTraits& traits)
		{
			bool isIgnored = traits.isIgnoredByName || traits.isIgnoredByWildcard;
			if (isIgnored)
				menu.AppendMenu(MF_STRING, IDC_IGNORE_BY_NAME, CTSTRING(UNIGNORE_USER_BY_NAME), g_iconBitmaps.getBitmap(IconBitmaps::CHAT_ALLOW, 0));
			else
				menu.AppendMenu(MF_STRING, IDC_IGNORE_BY_NAME, CTSTRING(IGNORE_USER_BY_NAME), g_iconBitmaps.getBitmap(IconBitmaps::CHAT_PROHIBIT, 0));
			if (traits.isIgnoredByWildcard)
				menu.EnableMenuItem(IDC_IGNORE_BY_NAME, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
		}
		
		static void internal_appendContactListItems(OMenu& menu, const FavUserTraits& traits)
		{
			if (traits.isEmpty || !traits.isFav)
				menu.AppendMenu(MF_STRING, IDC_ADD_TO_FAVORITES, CTSTRING(ADD_TO_FAVORITES), g_iconBitmaps.getBitmap(IconBitmaps::ADD_USER, 0));
			if (traits.isEmpty || traits.isFav)
				menu.AppendMenu(MF_STRING, IDC_REMOVE_FROM_FAVORITES, CTSTRING(REMOVE_FROM_FAVORITES), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE_USER, 0));
			if (DISABLE(options, NO_CONNECT_FAV_HUB) && traits.isFav)
				menu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT_FAVUSER_HUB), g_iconBitmaps.getBitmap(IconBitmaps::QUICK_CONNECT, 0));
		}
		
		static void appendContactListMenu(OMenu& menu, const FavUserTraits& traits)
		{
			if (ENABLE(options, INLINE_CONTACT_LIST))
			{
				internal_appendContactListItems(menu, traits);
			}
			else
			{
				favUserMenu.ClearMenu();
				internal_appendContactListItems(favUserMenu, traits);
				menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)favUserMenu, CTSTRING(CONTACT_LIST_MENU), g_iconBitmaps.getBitmap(IconBitmaps::CONTACT_LIST, 0));
			}
		}

	public:
		void appendCopyMenuForSingleUser(OMenu& menu)
		{
			dcassert(selectedUser);
			if (DISABLE(options, NO_COPY))
			{
				int flags = static_cast<T*>(this)->getSelectedOnlineUser() ? MF_ENABLED : MF_GRAYED;
				int count = copyUserMenu.GetMenuItemCount();
				for (int i = 1; i < count; i++)
					copyUserMenu.EnableMenuItem(i, MF_BYPOSITION | flags);
				menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyUserMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
				appendSeparator(menu);
			}
		}

	private:
		static void appendSendAutoMessageItems(OMenu& menu, int count)
		{
			if (DISABLE(options, NO_SEND_PM))
			{
				if (count > 0 && count < 50)
				{
					menu.AppendMenu(MF_STRING, IDC_PRIVATE_MESSAGE, CTSTRING(SEND_PRIVATE_MESSAGE), g_iconBitmaps.getBitmap(IconBitmaps::PM, 0));
					menu.AppendMenu(MF_SEPARATOR);
				}
			}
		}		
		
		static void appendFileListItems(OMenu& menu, const FavUserTraits& traits)
		{
			if (DISABLE(options, NO_FILE_LIST) && traits.isOnline)
			{
				menu.AppendMenu(MF_STRING, IDC_GETLIST, CTSTRING(GET_FILE_LIST), g_iconBitmaps.getBitmap(IconBitmaps::FILELIST, 0));
				menu.AppendMenu(MF_STRING, IDC_BROWSELIST, CTSTRING(BROWSE_FILE_LIST));
				menu.AppendMenu(MF_STRING, IDC_MATCH_QUEUE, CTSTRING(MATCH_QUEUE));
				appendSeparator(menu);
			}
		}
		
		static void appendRemoveAllItems(OMenu& menu)
		{
			menu.AppendMenu(MF_STRING, IDC_REMOVEALL, CTSTRING(REMOVE_FROM_ALL));
		}
		
		static void appendGrantItems(OMenu& menu)
		{
			menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)grantMenu, CTSTRING(GRANT_SLOTS_MENU), g_iconBitmaps.getBitmap(IconBitmaps::UPLOAD_QUEUE, 0));
		}
		
		string selectedHint;
		T2 selectedUser;
		
	protected:	
		void doAction(void (UserInfoBase::*func)(const int data), const int data)
		{
			if (selectedUser)
			{
				(UserInfoSimple(selectedUser, selectedHint).*func)(data);
			}
			else
			{
				vector<T2> selected;
				static_cast<T*>(this)->getSelectedUsers(selected);
				for (auto& u : selected)
					(UserInfoSimple(u, selectedHint).*func)(data);
			}
		}
		
		void doAction(void (UserInfoBase::*func)(const string &hubHint, const tstring& data), const string &hubHint, const tstring& data)
		{
			if (selectedUser)
			{
				(UserInfoSimple(selectedUser, selectedHint).*func)(hubHint, data);
			}
			else
			{
				vector<T2> selected;
				static_cast<T*>(this)->getSelectedUsers(selected);
				for (auto& u : selected)
					(UserInfoSimple(u, selectedHint).*func)(hubHint, data);
			}
		}
		
		void doAction(void (UserInfoBase::*func)(const string &hubHint, const uint64_t data), const string &hubHint, const uint64_t data)
		{
			if (selectedUser)
			{
				(UserInfoSimple(selectedUser, selectedHint).*func)(hubHint, data);
			}
			else
			{
				vector<T2> selected;
				static_cast<T*>(this)->getSelectedUsers(selected);
				for (auto& u : selected)
					(UserInfoSimple(u, selectedHint).*func)(hubHint, data);
			}
		}
		
		void doAction(void (UserInfoBase::*func)(const string &hubHint), const string &hubHint, bool useOnlyFirstItem = true)
		{
			if (selectedUser)
			{
				(UserInfoSimple(selectedUser, selectedHint).*func)(hubHint);
			}
			else
			{
				vector<T2> selected;
				static_cast<T*>(this)->getSelectedUsers(selected);
				if (!selected.empty())
				{
					if (useOnlyFirstItem)
						(UserInfoSimple(selected[0], selectedHint).*func)(hubHint);
					else
						for (auto& u : selected)
							(UserInfoSimple(u, selectedHint).*func)(hubHint);
				}
			}
		}

		void doAction(void (UserInfoBase::*func)())
		{
			if (selectedUser)
			{
				(UserInfoSimple(selectedUser, selectedHint).*func)();
			}
			else
			{
				vector<T2> selected;
				static_cast<T*>(this)->getSelectedUsers(selected);
				for (auto& u : selected)
					(UserInfoSimple(u, selectedHint).*func)();
			}
		}
	private:
		dcdrun(bool _debugIsClean;)
};

struct FavUserTraits
{
	FavUserTraits() :
		isEmpty(true),
		isFav(false),
		isAutoGrantSlot(false),
		uploadLimit(0),
		isIgnoredPm(false), isFreePm(false),
		isIgnoredByName(false),
		isIgnoredByWildcard(false),
		isOnline(true)
	{
	}
	void init(const UserInfoBase& ui);
	
	int uploadLimit;
	
	bool isAutoGrantSlot;
	bool isFav;
	bool isEmpty;
	bool isIgnoredPm;
	bool isFreePm;
	bool isIgnoredByName;
	bool isIgnoredByWildcard;
	bool isOnline;
};

#endif /* USER_INFO_BASE_HANDLER_H */
