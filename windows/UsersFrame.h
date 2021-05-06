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
#include "ExListViewCtrl.h"
#include "UserInfoBaseHandler.h"
#include "../client/UserInfoBase.h"

#include "../client/FavoriteManagerListener.h"
#include "../client/UserManager.h"
#include "../client/File.h"
#include "../client/OnlineUser.h"

class UsersFrame : public MDITabChildWindowImpl<UsersFrame>,
	public StaticFrame<UsersFrame, ResourceManager::FAVORITE_USERS, IDC_FAVUSERS>,
	public CSplitterImpl<UsersFrame>,
	private FavoriteManagerListener,
	private UserManagerListener,
	public UserInfoBaseHandler<UsersFrame, UserInfoGuiTraits::INLINE_CONTACT_LIST | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::NO_COPY>,
	private SettingsManagerListener
{
	public:	
		UsersFrame();
		~UsersFrame()
		{
			images.Destroy();
		}

		UsersFrame(const UsersFrame&) = delete;
		UsersFrame& operator= (const UsersFrame&) = delete;
		
		static CFrameWndClassInfo& GetWndClassInfo();
		
		typedef MDITabChildWindowImpl<UsersFrame> baseClass;
		typedef CSplitterImpl<UsersFrame> splitBase;
		typedef UserInfoBaseHandler<UsersFrame, UserInfoGuiTraits::INLINE_CONTACT_LIST | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::NO_COPY> uibBase;
		
		BEGIN_MSG_MAP(UsersFrame)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETDISPINFO, ctrlUsers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_USERS, LVN_COLUMNCLICK, ctrlUsers.onColumnClick)
		NOTIFY_HANDLER(IDC_USERS, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_USERS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_USERS, NM_DBLCLK, onDoubleClick)
		NOTIFY_HANDLER(IDC_IGNORELIST, NM_CUSTOMDRAW, ctrlIgnored.onCustomDraw)
		NOTIFY_HANDLER(IDC_IGNORELIST, LVN_ITEMCHANGED, onIgnoredItemChanged)
		COMMAND_ID_HANDLER(IDC_IGNORE_ADD, onIgnoreAdd)
		COMMAND_ID_HANDLER(IDC_IGNORE_REMOVE, onIgnoreRemove)
		COMMAND_ID_HANDLER(IDC_IGNORE_CLEAR, onIgnoreClear)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_REMOVE_FROM_FAVORITES, onRemove)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy)
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(uibBase)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onIgnorePrivate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		
		LRESULT onSetFocus(UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlUsers.SetFocus();
			return 0;
		}
		LRESULT onIgnoredItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onIgnoreAdd(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreRemove(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreClear(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);

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
		};
		
		// FavoriteManagerListener
		void on(UserAdded, const FavoriteUser& user) noexcept override;
		void on(UserRemoved, const FavoriteUser& user) noexcept override;
		void on(UserStatusChanged, const UserPtr& user) noexcept override;

		// UserManagerListener
		void on(IgnoreListChanged, const string& userName) noexcept override;
		void on(IgnoreListCleared) noexcept override;
		
		void on(SettingsManagerListener::Repaint) override;
		
		void addUser(const FavoriteUser& user);
		void updateUser(const UserPtr& user);
		void updateUser(const int i, ItemInfo* ui, const FavoriteUser& favUser);
		void removeUser(const FavoriteUser& user);

		void insertIgnoreList();
		void updateIgnoreListButtons();
		
	private:
		typedef TypedListViewCtrl<ItemInfo, IDC_USERS> UserInfoList;
		UserInfoList ctrlUsers;
		CImageList images;
		ExListViewCtrl ctrlIgnored;
		CEdit ctrlIgnoreName;
		CButton ctrlIgnoreAdd;
		CButton ctrlIgnoreRemove;
		CButton ctrlIgnoreClear;
		tstring selectedIgnore;
		bool startup;
		OMenu copyMenu;

		static const int columnId[COLUMN_LAST];
};

#endif // !defined(USERS_FRAME_H)
