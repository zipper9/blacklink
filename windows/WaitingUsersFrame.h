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
#include "TypedListViewCtrl.h"
#include "UserInfoBaseHandler.h"
#include "TimerHelper.h"
#include "../client/UserInfoBase.h"
#include "../client/UploadManager.h"
#include "../client/TaskQueue.h"

#define SHOWTREE_MESSAGE_MAP 12

class WaitingUsersFrame : public MDITabChildWindowImpl<WaitingUsersFrame>,
	public StaticFrame<WaitingUsersFrame, ResourceManager::WAITING_USERS, IDC_UPLOAD_QUEUE>,
	private UploadManagerListener,
	public CSplitterImpl<WaitingUsersFrame>,
	public UserInfoBaseHandler<WaitingUsersFrame>,
	private SettingsManagerListener
{
		typedef UserInfoBaseHandler<WaitingUsersFrame> uiBase;
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("WaitingUsersFrame"), IDR_UPLOAD_QUEUE, 0, COLOR_3DFACE);
		
		WaitingUsersFrame();
		~WaitingUsersFrame();

		WaitingUsersFrame(const WaitingUsersFrame&) = delete;
		WaitingUsersFrame& operator= (const WaitingUsersFrame&) = delete;	

		enum Tasks
		{
			ADD_ITEM,
			REMOVE,
			REMOVE_WAITING_ITEM,
			UPDATE_ITEMS
		};
		
		typedef MDITabChildWindowImpl<WaitingUsersFrame> baseClass;
		typedef CSplitterImpl<WaitingUsersFrame> splitBase;
		
		// Inline message map
		BEGIN_MSG_MAP(WaitingUsersFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_HANDLER(IDC_REMOVE, BN_CLICKED, onRemove)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, NM_CUSTOMDRAW, onCustomDraw)
//      NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_ITEMCHANGED, onItemChangedQueue)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		
		CHAIN_COMMANDS(uiBase)
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SHOWTREE_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowTree)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
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
				processTasks();
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
		
		LRESULT onShowTree(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			showTree = (wParam == BST_CHECKED);
			UpdateLayout(FALSE);
			LoadAll();
			return 0;
		}
		
		// Update control layouts
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
	private:
		static int columnSizes[];
		static int columnIndexes[];
		static HIconWrapper frameIcon;
		
		struct UserItem
		{
			UserPtr m_user;
			UserItem(const UserPtr& p_user) : m_user(p_user) { }
		};
		
		UserPtr getCurrentdUser()
		{
			HTREEITEM selectedItem = GetParentItem();
			return selectedItem ? reinterpret_cast<UserItem *>(ctrlQueued.GetItemData(selectedItem))->m_user : nullptr;
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
			const UserPtr User = getCurrentdUser();
			if (User)
			{
				UploadManager::LockInstanceQueue lockedInstance;
				lockedInstance->clearUserFilesL(User);
			}
			m_needsUpdateStatus = true;
		}
		
		// Communication with manager
		void LoadAll();
		
		HTREEITEM GetParentItem();
		
		// Contained controls
		CButton ctrlShowTree;
		CContainedWindow showTreeContainer;
		
		bool showTree;
		
		UserList UQFUsers;
		
		typedef TypedListViewCtrl<UploadQueueItem, IDC_UPLOAD_QUEUE> CtrlList;
		CtrlList ctrlList;

	public:
		CtrlList& getUserList() { return ctrlList; }

	private:
		CTreeViewCtrl ctrlQueued;
		
		CStatusBarCtrl ctrlStatus;
		int statusSizes[4]; // TODO: fix my size.
		
		void AddFile(const UploadQueueItemPtr& aUQI);
		void RemoveFile(const UploadQueueItemPtr& aUQI);
		void RemoveUser(const UserPtr& aUser);
		
		void updateStatus();
		
		bool m_needsUpdateStatus;
		bool m_needsResort;

		TaskQueue tasks;
		TimerHelper timer;
		
		struct UploadQueueTask : public Task
		{
			UploadQueueTask(const UploadQueueItemPtr& item) : m_item(item)
			{
			}
			~UploadQueueTask()
			{
			}
			UploadQueueItemPtr getItem() const
			{
				return m_item;
			}
			UploadQueueItemPtr m_item;
		};
		
		struct UserTask : public Task
		{
			UserTask(const UserPtr& user) : m_user(user)
			{
			}
			const UserPtr& getUser() const
			{
				return m_user;
			}
			UserPtr m_user;
		};
		
		void processTasks();
		void addTask(Tasks s, Task* task);

		// UploadManagerListener
		void on(UploadManagerListener::QueueAdd, const UploadQueueItemPtr& uqi) noexcept override
		{
			addTask(ADD_ITEM, new UploadQueueTask(uqi));
		}
		void on(UploadManagerListener::QueueRemove, const UserPtr& user) noexcept override
		{
			addTask(REMOVE, new UserTask(user));
		}
		void on(UploadManagerListener::QueueItemRemove, const UploadQueueItemPtr& uqi) noexcept override
		{
			addTask(REMOVE_WAITING_ITEM, new UploadQueueTask(uqi));
		}
		void on(UploadManagerListener::QueueUpdate) noexcept override;
		// SettingsManagerListener
		void on(SettingsManagerListener::Repaint) override;
		
};

#endif
