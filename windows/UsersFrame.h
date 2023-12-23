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

#ifndef USERS_FRAME_H
#define USERS_FRAME_H

#include "FlatTabCtrl.h"
#include "StaticFrame.h"
#include "TypedListViewCtrl.h"
#include "UserInfoBaseHandler.h"
#include "IgnoredUsersWindow.h"
#include "../client/UserInfoBase.h"
#include "../client/FavoriteManagerListener.h"
#include "../client/UserManagerListener.h"
#include "../client/OnlineUser.h"
#include "../client/FavoriteUser.h"

static const int USERS_FRAME_TRAITS = UserInfoGuiTraits::FAVORITES_VIEW | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::NO_COPY;

class UsersFrame : public MDITabChildWindowImpl<UsersFrame>,
	public StaticFrame<UsersFrame, ResourceManager::FAVORITE_USERS, IDC_FAVUSERS>,
	public CSplitterImpl<UsersFrame>,
	public CMessageFilter,
	private FavoriteManagerListener,
	private UserManagerListener,
	public UserInfoBaseHandler<UsersFrame, USERS_FRAME_TRAITS>,
	private SettingsManagerListener
{
	public:	
		UsersFrame();

		UsersFrame(const UsersFrame&) = delete;
		UsersFrame& operator= (const UsersFrame&) = delete;
		
		static CFrameWndClassInfo& GetWndClassInfo();
		
		typedef MDITabChildWindowImpl<UsersFrame> baseClass;
		typedef CSplitterImpl<UsersFrame> splitBase;
		typedef UserInfoBaseHandler<UsersFrame, USERS_FRAME_TRAITS> uibBase;
		
		BEGIN_MSG_MAP(UsersFrame)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETDISPINFO, ctrlUsers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_USERS, LVN_COLUMNCLICK, ctrlUsers.onColumnClick)
		NOTIFY_HANDLER(IDC_USERS, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_USERS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_USERS, NM_DBLCLK, onDoubleClick)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_REMOVE_FROM_FAVORITES, onRemove)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy)
		COMMAND_HANDLER(IDC_SHOW_IGNORED, BN_CLICKED, onToggleIgnored)
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(uibBase)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onToggleIgnored(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			if (wParam != SIZE_MINIMIZED) updateLayout();
			return 0;
		}

		LRESULT onIgnorePrivate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		void updateLayout();
		void showUser(const CID& cid);

		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);

		LRESULT onSetFocus(UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlUsers.SetFocus();
			return 0;
		}

		void GetSystemSettings(bool bUpdate);
		virtual BOOL PreTranslateMessage(MSG* pMsg) override;

		// UserInfoBaseHandler
		void getSelectedUsers(vector<UserPtr>& v) const;
		void openUserLog();

	private:
		enum
		{
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_SEEN,
			COLUMN_DESCRIPTION,
			COLUMN_SPEED_LIMIT,
			COLUMN_PM_HANDLING,
			COLUMN_SLOTS,
			COLUMN_CID,
			COLUMN_SHARE_GROUP,
			COLUMN_LAST
		};
		enum
		{
			USER_UPDATED
		};
		
		class ItemInfo : public UserInfoBase
		{
			public:
				ItemInfo(const FavoriteUser& u) : user(u.user), hubHint(u.url)
				{
					update(u);
				}
				
				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					return columns[col];
				}
				
				static int compareItems(const ItemInfo* a, const ItemInfo* b, int col);
				
				int getImageIndex() const { return 2; }
				static int getStateImageIndex() { return 0; }

				void update(const FavoriteUser& u);
				bool online() const { return isOnline; }

				const UserPtr& getUser() const { return user; }
				const string& getHubHint() const { return hubHint; }

				tstring columns[COLUMN_LAST];
				int speedLimit;
				CID shareGroup;
				uint32_t flags;

			private:
				UserPtr user;
				const string hubHint;
				time_t lastSeen;
				bool isOnline;
		};
		
		// FavoriteManagerListener
		void on(UserAdded, const FavoriteUser& user) noexcept override;
		void on(UserRemoved, const FavoriteUser& user) noexcept override;
		void on(UserStatusChanged, const UserPtr& user) noexcept override;

		// UserManagerListener
		void on(IgnoreListChanged) noexcept override;
		void on(IgnoreListCleared) noexcept override;
		
		void on(SettingsManagerListener::Repaint) override;
		
		void addUser(const FavoriteUser& user);
		void updateUser(const UserPtr& user);
		void updateUser(const int i, ItemInfo* ui, const FavoriteUser& favUser);
		void removeUser(const FavoriteUser& user);

	private:
		typedef TypedListViewCtrl<ItemInfo> UserInfoList;
		UserInfoList ctrlUsers;
		IgnoredUsersWindow ctrlIgnored;
		bool startup;
		OMenu copyMenu;
		int barHeight;
		SIZE checkBoxSize;
		int checkBoxXOffset;
		int checkBoxYOffset;
		CButton ctrlShowIgnored;

		static const int columnId[COLUMN_LAST];
};

#endif // !defined(USERS_FRAME_H)
