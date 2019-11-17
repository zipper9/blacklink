#ifndef USER_INFO_BASE_HANDLER_H
#define USER_INFO_BASE_HANDLER_H

#include "OMenu.h"
#include "resource.h"

class UserInfo;
class UserInfoBase;
struct FavUserTraits;

struct UserInfoGuiTraits // technical class, please do not use it directly!
{
	public:
		static void init(); // only for WinUtil!
		static void uninit(); // only for WinUtil!
		static bool isUserInfoMenus(const HMENU& handle) // only for WinUtil!
		{
			return
			    handle == userSummaryMenu.m_hMenu ||
			    handle == copyUserMenu.m_hMenu ||
			    handle == grantMenu.m_hMenu ||
			    handle == speedMenu.m_hMenu ||
			    handle == privateMenu.m_hMenu
			    ;
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
		static int getSpeedLimitByCtrlId(WORD wID, int lim);
		
		static bool ENABLE(int HANDLERS, Options FLAG) // TODO constexpr
		{
			return (HANDLERS & FLAG) != 0;
		}
		static bool DISABLE(int HANDLERS, Options FLAG) // TODO constexpr
		{
			return (HANDLERS & FLAG) == 0;
		}
		static string g_hubHint;
		
		static OMenu copyUserMenu; // [+] IRainman fix.
		static OMenu grantMenu;
		static OMenu speedMenu; // !SMT!-S
		static OMenu privateMenu; // !SMT!-PSW
		
		static OMenu userSummaryMenu; // !SMT!-UI
		
		friend class UserInfoSimple;

		static const UINT IDC_SPEED_VALUE = 30000; // must be high enough so it won't clash with normal IDCs
};

template<class T>
struct UserInfoBaseHandlerTraitsUser // technical class, please do not use it directly!
{
	protected:
		static T g_user;
};

template<class T, const int options = UserInfoGuiTraits::DEFAULT, class T2 = UserPtr>
class UserInfoBaseHandler : UserInfoBaseHandlerTraitsUser<T2>, public UserInfoGuiTraits
{
		/*
		1) If you want to get the features USER_LOG and (or) NICK_TO_CHAT you need to create methods, respectively onOpenUserLog and (or) onAddNickToChat in its class.
		2) clearUserMenu()
		3) reinitUserMenu(user, hint)
		4) appendAndActivateUserItems(yourMenu)
		5) Before you destroy the menu in your class you will definitely need to call WinUtil::unlinkStaticMenus(yourMenu)
		*/
	public:
		UserInfoBaseHandler() : m_selectedHint(UserInfoGuiTraits::g_hubHint), m_selectedUser(UserInfoBaseHandlerTraitsUser::g_user)
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
		COMMAND_ID_HANDLER(IDC_IGNORE_BY_NAME, onIgnoreOrUnignoreUserByName) // [!] IRainman moved from HubFrame and clean.
		COMMAND_RANGE_HANDLER(IDC_GRANTSLOT, IDC_UNGRANTSLOT, onGrantSlot)
		COMMAND_RANGE_HANDLER(IDC_PM_NORMAL, IDC_PM_FREE, onPrivateAcces)
		COMMAND_RANGE_HANDLER(IDC_SPEED_NORMAL, IDC_SPEED_MANUAL, onSetUserLimit)
		COMMAND_RANGE_HANDLER(IDC_SPEED_VALUE, IDC_SPEED_VALUE + 256, onSetUserLimit)
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
		
		virtual LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(0);
			LogManager::message("Not implemented [virtual LRESULT onOpenUserLog]");
			return 0;
		}
		
		virtual LRESULT onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(0);
			LogManager::message("Not implemented [virtual LRESULT onAddNickToChat]");
			return 0;
		}
		
		virtual LRESULT onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(0);
			LogManager::message("Not implemented [virtual LRESULT onCopyUserInfo]");
			/*
			// !SMT!-S
			const auto su = getSelectedUser();
			if (su)
			{
			    __if_exists(T2::getIdentity)
			    {
			        const auto id = su->getIdentity();
			        const auto su = su->getUser();
			    }
			    __if_exists(T2::getLastNick)
			    {
			        const auto& u = su;
			    }
			
			    string sCopy;
			    switch (wID)
			    {
			        case IDC_COPY_NICK:
			            __if_exists(T2::getIdentity)
			            {
			                iiii
			                sCopy += id.getNick();
			            }
			            __if_exists(T2::getLastNick)
			            {
			                iiii
			                sCopy += su->getLastNick();
			            }
			            break;
			        case IDC_COPY_IP:
			            __if_exists(T2::getIp)
			            {
			                iiii
			                sCopy += su->getIp();
			            }
			            __if_exists(T2::getIP)
			            {
			                iiii
			                sCopy += su->getIP();
			            }
			            break;
			        case IDC_COPY_NICK_IP:
			        {
			            // TODO translate
			            sCopy += "User Info:\r\n";
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n";
			            }
			            __if_exists(T2::getLastNick)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + su->getLastNick() + "\r\n";
			            }
			            sCopy += "\tIP: ";
			            __if_exists(T2::getIp)
			            {
			                sCopy += Identity::formatIpString(su->getIp());
			            }
			            __if_exists(T2::getIP)
			            {
			                sCopy += Identity::formatIpString(su->getIP());
			            }
			            break;
			        }
			        case IDC_COPY_ALL:
			        {
			            // TODO translate
			            sCopy += "User info:\r\n";
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n";
			            }
			            __if_exists(T2::getLastNick)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + su->getLastNick() + "\r\n";
			            }
			            sCopy += "\tNicks: " + Util::toString(ClientManager::getNicks(u->getCID(), Util::emptyString)) + "\r\n" +
			                     "\t" + STRING(HUBS) + ": " + Util::toString(ClientManager::getHubs(u->getCID(), Util::emptyString)) + "\r\n" +
			                     "\t" + STRING(SHARED) + ": " + Identity::formatShareBytes(u->getBytesShared());
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += (u->isNMDC() ? Util::emptyString : "(" + STRING(SHARED_FILES) + ": " + Util::toString(id.getSharedFiles()) + ")");
			            }
			            sCopy += "\r\n";
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\t" + STRING(DESCRIPTION) + ": " + id.getDescription() + "\r\n" +
			                         "\t" + STRING(APPLICATION) + ": " + id.getApplication() + "\r\n";
			                const auto con = Identity::formatSpeedLimit(id.getDownloadSpeed());
			                if (!con.empty())
			                {
			                    sCopy += "\t";
			                    sCopy += (u->isNMDC() ? STRING(CONNECTION) : "Download speed");
			                    sCopy += ": " + con + "\r\n";
			                }
			            }
			            const auto lim = Identity::formatSpeedLimit(u->getLimit());
			            if (!lim.empty())
			            {
			                sCopy += "\tUpload limit: " + lim + "\r\n";
			            }
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\tE-Mail: " + id.getEmail() + "\r\n" +
			                         "\tClient Type: " + Util::toString(id.getClientType()) + "\r\n" +
			                         "\tMode: " + (id.isTcpActive(&id.getClient()) ? 'A' : 'P') + "\r\n";
			            }
			            sCopy += "\t" + STRING(SLOTS) + ": " + Util::toString(su->getSlots()) + "\r\n";
			            __if_exists(T2::getIp)
			            {
			                sCopy += "\tIP: " + Identity::formatIpString(id.getIp()) + "\r\n";
			            }
			            __if_exists(T2::getIP)
			            {
			                sCopy += "\tIP: " + Identity::formatIpString(id.getIP()) + "\r\n";
			            }
			            __if_exists(T2::getIdentity)
			            {
			                const auto su = id.getSupports();
			                if (!su.empty())
			                {
			                    sCopy += "\tKnown supports: " + su;
			                }
			            }
			            break;
			        }
			        default:
			            __if_exists(T2::getIdentity)
			            {
			                switch (wID)
			                {
			                    case IDC_COPY_EXACT_SHARE:
			                        sCopy += Identity::formatShareBytes(id.getBytesShared());
			                        break;
			                    case IDC_COPY_DESCRIPTION:
			                        sCopy += id.getDescription();
			                        break;
			                    case IDC_COPY_APPLICATION:
			                        sCopy += id.getApplication();
			                        break;
			                    case IDC_COPY_TAG:
			                        sCopy += id.getTag();
			                        break;
			                    case IDC_COPY_CID:
			                        sCopy += id.getCID();
			                        break;
			                    case IDC_COPY_EMAIL_ADDRESS:
			                        sCopy += id.getEmail();
			                        break;
			                    default:
			                        dcdebug("THISFRAME DON'T GO HERE\n");
			                        return 0;
			                }
			                break;
			            }
			            dcdebug("THISFRAME DON'T GO HERE\n");
			            return 0;
			    }
			    if (!sCopy.empty())
			    {
			        WinUtil::setClipboard(sCopy);
			    }
			}
			*/
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
			doAction(&UserInfoBase::doReport, m_selectedHint);
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
			MENUINFO menuInfo = {0};
			menuInfo.cbSize = sizeof(MENUINFO);
			menuInfo.fMask = MIM_MENUDATA;
			speedMenu.GetMenuInfo(&menuInfo);
			const int iLimit = menuInfo.dwMenuData;
			
			const int lim = getSpeedLimitByCtrlId(wID, iLimit);
			
			doAction(&UserInfoBase::setUploadLimit, lim);
			return 0;
		}
		LRESULT onPrivateAcces(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).pm)(m_selectedHint);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					// !SMT!-S
					if (((T*)this)->getUserList().getSelectedCount() > 1)
					{
						const tstring l_message = UserInfoSimple::getBroadcastPrivateMessage();
						if (!l_message.empty()) // [+] SCALOlaz: support for abolition and prohibition to send a blank line
						{
							((T*)this)->getUserList().forEachSelectedParam(&UserInfoBase::pm_msg, m_selectedHint, l_message);
						}
					}
					else
					{
						using std::placeholders::_1;
						((T*)this)->getUserList().forEachSelectedT(std::bind(&UserInfoBase::pm, _1, m_selectedHint));
					}
					// !SMT!-S end
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
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 600);
					break;
				case IDC_GRANTSLOT_HOUR:
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 3600);
					break;
				case IDC_GRANTSLOT_DAY:
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 24 * 3600);
					break;
				case IDC_GRANTSLOT_WEEK:
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 7 * 24 * 3600);
					break;
				case IDC_GRANTSLOT_PERIOD:
				{
					const uint64_t slotTime = UserInfoSimple::inputSlotTime();
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, slotTime);
				}
				break;
				case IDC_UNGRANTSLOT:
					doAction(&UserInfoBase::ungrantSlot, m_selectedHint);
					break;
			}
			return 0;
		}
		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::removeAll);
			return 0;
		}
		
		// [+] IRainman fix.
		const T2& getSelectedUser() const
		{
			return m_selectedUser;
		}
		const string& getSelectedHint() const
		{
			return m_selectedHint;
		}
		
		void clearUserMenu()
		{
			m_selectedHint.clear();
			m_selectedUser = nullptr;
			userSummaryMenu.ClearMenu(); // !SMT!-UI
			
			for (int j = 0; j < speedMenu.GetMenuItemCount(); ++j) // !SMT!-S
			{
				speedMenu.CheckMenuItem(j, MF_BYPOSITION | MF_UNCHECKED);
			}
			
			// !SMT!-S
			privateMenu.CheckMenuItem(IDC_PM_NORMAL, MF_BYCOMMAND | MF_UNCHECKED);
			privateMenu.CheckMenuItem(IDC_PM_IGNORED, MF_BYCOMMAND | MF_UNCHECKED);
			privateMenu.CheckMenuItem(IDC_PM_FREE, MF_BYCOMMAND | MF_UNCHECKED);
			
			dcdrun(_debugIsClean = true;)
		}
		
		void reinitUserMenu(const T2& user, const string& hint)
		{
			dcassert(_debugIsClean);
			
			m_selectedHint = hint;
			m_selectedUser = user;
		}
		// [~] IRainman fix.
		void appendAndActivateUserItems(CMenu& p_menu, bool p_is_only_first_user_info = true)
		{
			dcassert(_debugIsClean);
			dcdrun(_debugIsClean = false;)
			
			if (m_selectedUser)
			{
				appendAndActivateUserItemsForSingleUser(p_menu, m_selectedHint);
			}
			else
			{
				__if_exists(T::getUserList) // ??
				{
					doAction(&UserInfoBase::createSummaryInfo, m_selectedHint, p_is_only_first_user_info);
					FavUserTraits traits; // empty
					
					if (ENABLE(options, NICK_TO_CHAT))
					{
						p_menu.AppendMenu(MF_STRING, IDC_ADD_NICK_TO_CHAT, CTSTRING(ADD_NICK_TO_CHAT));
					}
					appendSendAutoMessageItems(p_menu);
					appendFileListItems(p_menu);
					
					appendContactListMenu(p_menu, traits);
					appendFavPrivateMenu(p_menu);
					appendIgnoreByNameItem(p_menu, traits);
					appendGrantItems(p_menu);
					appendSpeedLimitMenu(p_menu);
					appendSeparator(p_menu);
					
					appendRemoveAllItems(p_menu);
					appendSeparator(p_menu);
					
					//appenUserSummaryMenu(p_menu);
				}
			}
		}
		
	private:	
		void appendAndActivateUserItemsForSingleUser(CMenu& p_menu, const string& p_selectedHint) // [+] IRainman fix.
		{
			dcassert(m_selectedUser);
			
			UserInfoSimple ui(m_selectedUser, p_selectedHint);
			FavUserTraits traits;
			traits.init(ui);
			ui.createSummaryInfo(p_selectedHint);
			activateFavPrivateMenuForSingleUser(p_menu, traits);
			activateSpeedLimitMenuForSingleUser(p_menu, traits);
			if (ENABLE(options, NICK_TO_CHAT))
			{
				p_menu.AppendMenu(MF_STRING, IDC_ADD_NICK_TO_CHAT, CTSTRING(ADD_NICK_TO_CHAT));
			}
			if (ENABLE(options, USER_LOG))
			{
				p_menu.AppendMenu(MF_STRING | (!BOOLSETTING(LOG_PRIVATE_CHAT) ? MF_DISABLED : 0), IDC_OPEN_USER_LOG, CTSTRING(OPEN_USER_LOG));
				p_menu.AppendMenu(MF_SEPARATOR);
			}
			appendSendAutoMessageItems(p_menu);
			appendCopyMenuForSingleUser(p_menu);
			appendFileListItems(p_menu);
			appendContactListMenu(p_menu, traits);
			appendFavPrivateMenu(p_menu);
			appendIgnoreByNameItem(p_menu, traits);
			appendGrantItems(p_menu);
			appendSpeedLimitMenu(p_menu);
			appendSeparator(p_menu);
			appendRemoveAllItems(p_menu);
			appendSeparator(p_menu);
			appendUserSummaryMenu(p_menu);
		}
		
		void appendSeparator(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_SEPARATOR);
		}
		
		void appendUserSummaryMenu(CMenu& p_menu)
		{
			//const bool notEmpty = userSummaryMenu.GetMenuItemCount() > 1;
			
			//p_menu.AppendMenu(MF_POPUP, notEmpty ? (UINT_PTR)(HMENU)userSummaryMenu : 0, CTSTRING(USER_SUMMARY));
			//p_menu.EnableMenuItem(p_menu.GetMenuItemCount() - 1, notEmpty ? MFS_ENABLED : MFS_DISABLED);
			
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)userSummaryMenu, CTSTRING(USER_SUMMARY));
		}
		
		void appendFavPrivateMenu(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)privateMenu, CTSTRING(ACCES_TO_PERSONAL_MESSAGES));
		}
		
		void activateFavPrivateMenuForSingleUser(CMenu& p_menu, const FavUserTraits& traits)
		{
			dcassert(m_selectedUser);
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
		
		void appendSpeedLimitMenu(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)speedMenu, CTSTRING(UPLOAD_SPEED_LIMIT));
		}
		
		void activateSpeedLimitMenuForSingleUser(CMenu& p_menu, const FavUserTraits& traits) // !SMT!-S
		{
			dcassert(m_selectedUser);
			const int id = getCtrlIdBySpeedLimit(traits.uploadLimit);
			speedMenu.CheckMenuItem(id, MF_BYCOMMAND | MF_CHECKED);
			MENUINFO menuInfo = {0};
			menuInfo.cbSize = sizeof(MENUINFO);
			menuInfo.fMask = MIM_MENUDATA;
			menuInfo.dwMenuData = traits.uploadLimit;
			p_menu.SetMenuInfo(&menuInfo);
		}
		
		void appendIgnoreByNameItem(CMenu& p_menu, const FavUserTraits& traits)
		{
			p_menu.AppendMenu(MF_STRING, IDC_IGNORE_BY_NAME, traits.isIgnoredByName ? CTSTRING(UNIGNORE_USER_BY_NAME) : CTSTRING(IGNORE_USER_BY_NAME));
		}
		
		template <typename T>
		void internal_appendContactListItems(T& p_menu, const FavUserTraits& traits)
		{
#ifndef IRAINMAN_ALLOW_ALL_CLIENT_FEATURES_ON_NMDC
			if (traits.adcOnly) // TODO
			{
			}
#endif
			if (traits.isEmpty || !traits.isFav)
			{
				p_menu.AppendMenu(MF_STRING, IDC_ADD_TO_FAVORITES, CTSTRING(ADD_TO_FAVORITES));
			}
			if (traits.isEmpty || traits.isFav)
			{
				p_menu.AppendMenu(MF_STRING, IDC_REMOVE_FROM_FAVORITES, CTSTRING(REMOVE_FROM_FAVORITES));
			}
			
			if (DISABLE(options, NO_CONNECT_FAV_HUB))
			{
				if (traits.isFav)
				{
					p_menu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT_FAVUSER_HUB));
				}
			}
		}
		
		void appendContactListMenu(CMenu& p_menu, const FavUserTraits& traits)
		{
			if (ENABLE(options, INLINE_CONTACT_LIST))
			{
				internal_appendContactListItems(p_menu, traits);
			}
			else
			{
				CMenuHandle favMenu;
				favMenu.CreatePopupMenu();
				internal_appendContactListItems(favMenu, traits);
				p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)favMenu, CTSTRING(CONTACT_LIST_MENU));
			}
		}
	public:
		void appendCopyMenuForSingleUser(CMenu& p_menu)
		{
			dcassert(m_selectedUser);
			
			//[?] if (DISABLE(options, NO_COPY))
			{
				p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyUserMenu, CTSTRING(COPY));
				appendSeparator(p_menu);
			}
		}
	private:
		void appendSendAutoMessageItems(CMenu& p_menu/*, const int count*/)
		{
			if (DISABLE(options, NO_SEND_PM))
			{
				if (m_selectedUser)
				{
					p_menu.AppendMenu(MF_STRING, IDC_PRIVATE_MESSAGE, CTSTRING(SEND_PRIVATE_MESSAGE));
					p_menu.AppendMenu(MF_SEPARATOR);
				}
			}
		}
		
		
		void appendFileListItems(CMenu& p_menu)
		{
			if (DISABLE(options, NO_FILE_LIST))
			{
				p_menu.AppendMenu(MF_STRING, IDC_GETLIST, CTSTRING(GET_FILE_LIST));
				p_menu.AppendMenu(MF_STRING, IDC_BROWSELIST, CTSTRING(BROWSE_FILE_LIST));
				p_menu.AppendMenu(MF_STRING, IDC_MATCH_QUEUE, CTSTRING(MATCH_QUEUE));
				appendSeparator(p_menu);
			}
		}
		
		void appendRemoveAllItems(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_STRING, IDC_REMOVEALL, CTSTRING(REMOVE_FROM_ALL));
		}
		
		void appendGrantItems(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)grantMenu, CTSTRING(GRANT_SLOTS_MENU));
		}
		
		string m_selectedHint;
		T2 m_selectedUser;
		
	protected:
	
		void doAction(void (UserInfoBase::*func)(const int data), const int data)
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(data);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					using std::placeholders::_1;
					((T*)this)->getUserList().forEachSelectedT(std::bind(func, _1, data));
				}
			}
		}
		
		void doAction(void (UserInfoBase::*func)(const string &hubHint, const tstring& data), const string &hubHint, const tstring& data) // [+] IRainman.
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(hubHint, data);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelectedParam(func, hubHint, data);
				}
			}
		}
		void doAction(void (UserInfoBase::*func)(const string &hubHint, const uint64_t data), const string &hubHint, const uint64_t data)
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(hubHint, data);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelectedParam(func, hubHint, data);
				}
			}
		}
		
		// !SMT!-S
		void doAction(void (UserInfoBase::*func)(const string &hubHint), const string &hubHint, bool p_is_only_first_user_info = true)
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(hubHint);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					using std::placeholders::_1;
					if (p_is_only_first_user_info)
					{
						((T*)this)->getUserList().forFirstSelectedT(std::bind(func, _1, hubHint));
					}
					else
					{
						((T*)this)->getUserList().forEachSelectedT(std::bind(func, _1, hubHint));
					}
				}
			}
		}
		
		// !SMT!-S
		void doAction(void (UserInfoBase::*func)())
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)();
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelected(func);
				}
			}
		}
	private:
		dcdrun(bool _debugIsClean;)
};

#endif /* USER_INFO_BASE_HANDLER_H */
