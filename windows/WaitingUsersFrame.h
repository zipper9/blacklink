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


#ifndef WAITING_QUEUE_FRAME_H
#define WAITING_QUEUE_FRAME_H

#include "FlatTabCtrl.h"
#include "StaticFrame.h"
#include "TypedListViewCtrl.h"
#include "UserInfoBaseHandler.h"
#include "TimerHelper.h"
#include "CustomDrawHelpers.h"
#include "../client/UserInfoBase.h"
#include "../client/UploadManager.h"
#include "../client/TaskQueue.h"

class WaitingUsersFrame : public MDITabChildWindowImpl<WaitingUsersFrame>,
	public StaticFrame<WaitingUsersFrame, ResourceManager::WAITING_USERS, IDC_UPLOAD_QUEUE>,
	private UploadManagerListener,
	public CSplitterImpl<WaitingUsersFrame>,
	public UserInfoBaseHandler<WaitingUsersFrame, UserInfoGuiTraits::NO_COPY>,
	private SettingsManagerListener
{
		typedef UserInfoBaseHandler<WaitingUsersFrame, UserInfoGuiTraits::NO_COPY> uiBase;

		class UploadQueueItem;
		typedef TypedListViewCtrl<UploadQueueItem> CtrlList;

	public:
		static CFrameWndClassInfo& GetWndClassInfo();
		
		WaitingUsersFrame();
		~WaitingUsersFrame();

		WaitingUsersFrame(const WaitingUsersFrame&) = delete;
		WaitingUsersFrame& operator= (const WaitingUsersFrame&) = delete;	

		enum Tasks
		{
			ADD_FILE,
			REMOVE_USER
		};
		
		typedef MDITabChildWindowImpl<WaitingUsersFrame> baseClass;
		typedef CSplitterImpl<WaitingUsersFrame> splitBase;
		
		BEGIN_MSG_MAP(WaitingUsersFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_REMOVEALL, onRemove)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, NM_CUSTOMDRAW, onCustomDraw)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_USERS, TVN_SELCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_USERS, TVN_DELETEITEM, onTreeItemDeleted)
		NOTIFY_HANDLER(IDC_USERS, TVN_KEYDOWN, onKeyDownDirs)
		
		CHAIN_COMMANDS(uiBase)
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onTreeItemDeleted(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		
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

		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		// Update control layouts
		void UpdateLayout(BOOL bResizeBars = TRUE);

		void getSelectedUsers(vector<UserPtr>& v) const;

	private:
		static const int columnId[];
		
		class UploadQueueItem : public UserInfoBase
		{
			public:
				UploadQueueItem(const HintedUser& hintedUser, const UploadQueueFilePtr& file) :
					file(file), hintedUser(hintedUser), iconIndex(-1), ip{0} {}
				void update();
				const UserPtr& getUser() const override { return hintedUser.user; }
				const string& getHubHint() const override { return hintedUser.hint; }
				const UploadQueueFilePtr& getFile() { return file; }
				int getImageIndex() const { return iconIndex < 0 ? 0 : iconIndex; }
				void setImageIndex(int index) { iconIndex = index; }
				static int getStateImageIndex() { return 0; }
				static int getCompareFlags() { return 0; }
				static int compareItems(const UploadQueueItem* a, const UploadQueueItem* b, int col, int flags);

				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					return text[col];
				}
				void setText(int col, const tstring& val)
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					text[col] = val;
				}
		
				enum
				{
					COLUMN_FILE,
					COLUMN_TYPE,
					COLUMN_PATH,
					COLUMN_NICK,
					COLUMN_HUB,
					COLUMN_TRANSFERRED,
					COLUMN_SIZE,
					COLUMN_ADDED,
					COLUMN_WAITING,
					COLUMN_LOCATION,
					COLUMN_IP,
					COLUMN_SLOTS,
					COLUMN_SHARE,
					COLUMN_LAST
				};
	
				IpAddress ip;
				IPInfo ipInfo;

			private:
				int iconIndex;
				UploadQueueFilePtr file;
				HintedUser hintedUser;
				tstring text[COLUMN_LAST];
		};

		struct UserItem
		{
			HintedUser hintedUser;
			UserItem(const HintedUser& hintedUser) : hintedUser(hintedUser) {}
		};

		UserPtr getCurrentUser()
		{
			HTREEITEM selectedItem = ctrlQueued.GetSelectedItem();
			if (!selectedItem || selectedItem == treeRoot) return UserPtr();
			auto userData = ctrlQueued.GetItemData(selectedItem);
			if (!userData) return UserPtr();
			return reinterpret_cast<UserItem*>(userData)->hintedUser.user;
		}

		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_DELETE)
			{
				if (ctrlList.getSelectedCount())
				{
					removeSelected();
				}
			}
			return 0;
		}
		
		LRESULT onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMTVKEYDOWN* kd = (NMTVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_DELETE)
			{
				removeSelectedUser();
			}
			return 0;
		}
		
		void removeSelected();
		
		void removeSelectedUser()
		{
			const UserPtr User = getCurrentUser();
			if (User)
			{
				UploadManager::LockInstanceQueue lockedInstance;
				lockedInstance->clearUserFilesL(User);
			}
			shouldUpdateStatus = true;
		}
		
		void loadFiles(const WaitingUser& wu);
		void loadAll();
		
		struct UserListItem
		{
			UserPtr user;
			HTREEITEM treeItem;
			
			UserListItem() {}
			UserListItem(const UserPtr& user, HTREEITEM treeItem) :
				user(user), treeItem(treeItem) {}
		};

		std::list<UserListItem> userList;

		CtrlList ctrlList;
		CustomDrawHelpers::CustomDrawState customDrawState;
		ProgressBar progressBar;
		bool showProgressBars;
		OMenu copyMenu;

	private:
		CTreeViewCtrl ctrlQueued;
		HTREEITEM treeRoot;

		CStatusBarCtrl ctrlStatus;
		int statusSizes[4];

		void addFile(const HintedUser& hintedUser, const UploadQueueFilePtr& uqi, bool addUser);
		void removeUser(const UserPtr& user);

		void onTimerInternal();
		void updateStatus();
		void updateListItems();
		void initProgressBar(bool check);

		bool shouldUpdateStatus;
		bool shouldSort;

		TaskQueue tasks;
		TimerHelper timer;
		
		struct UploadQueueTask : public Task
		{
			UploadQueueTask(const HintedUser& hintedUser, const UploadQueueFilePtr& item) : hintedUser(hintedUser), item(item) {}

			const HintedUser hintedUser;
			const UploadQueueFilePtr item;
		};
		
		struct UserTask : public Task
		{
			UserTask(const UserPtr& user) : user(user) {}
			const UserPtr user;
		};
		
		void processTasks();
		void addTask(Tasks s, Task* task);

		// UploadManagerListener
		void on(UploadManagerListener::QueueAdd, const HintedUser& hintedUser, const UploadQueueFilePtr& uqi) noexcept override
		{
			addTask(ADD_FILE, new UploadQueueTask(hintedUser, uqi));
		}
		void on(UploadManagerListener::QueueRemove, const UserPtr& user) noexcept override
		{
			addTask(REMOVE_USER, new UserTask(user));
		}

		// SettingsManagerListener
		void on(SettingsManagerListener::ApplySettings) override;
};

#endif
