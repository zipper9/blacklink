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

#ifndef QUEUE_FRAME_H
#define QUEUE_FRAME_H

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "ImageLists.h"
#include "../client/QueueManagerListener.h"
#include "../client/DownloadManagerListener.h"
#include "HIconWrapper.h"
#include "TimerHelper.h"

#define SHOWTREE_MESSAGE_MAP 12

#define WMU_SHOW_QUEUE_ITEM (WM_USER + 120)

class QueueFrame : public MDITabChildWindowImpl<QueueFrame>,
	public StaticFrame<QueueFrame, ResourceManager::DOWNLOAD_QUEUE, IDC_QUEUE>,
	private QueueManagerListener,
	private DownloadManagerListener,
	public CSplitterImpl<QueueFrame>,
	public PreviewBaseHandler<QueueFrame>,
	private SettingsManagerListener
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("QueueFrame"), IDR_QUEUE, 0, COLOR_3DFACE);
		
		QueueFrame();
		~QueueFrame();

		QueueFrame(const QueueFrame&) = delete;
		QueueFrame& operator= (const QueueFrame&) = delete;
		
		typedef MDITabChildWindowImpl<QueueFrame> baseClass;
		typedef CSplitterImpl<QueueFrame> splitBase;
		typedef PreviewBaseHandler<QueueFrame> prevBase;
		
		BEGIN_MSG_MAP(QueueFrame)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETDISPINFO, ctrlQueue.onGetDispInfo)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_COLUMNCLICK, ctrlQueue.onColumnClick)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETINFOTIP, ctrlQueue.onInfoTip)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_ITEMCHANGED, onItemChangedQueue)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onTreeItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		NOTIFY_HANDLER(IDC_QUEUE, NM_DBLCLK, onDoubleClick)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WMU_SHOW_QUEUE_ITEM, onShowQueueItem)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopyMagnet)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVE_ALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_RECHECK, onRecheck);
		COMMAND_ID_HANDLER(IDC_REMOVE_OFFLINE, onRemoveOffline)
		COMMAND_ID_HANDLER(IDC_MOVE, onMove)
		COMMAND_ID_HANDLER(IDC_RENAME, onRename)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED, IDC_PRIORITY_HIGHEST, onPriority)
		COMMAND_RANGE_HANDLER(IDC_SEGMENTONE, IDC_SEGMENTTWO_HUNDRED, onSegments)
		COMMAND_RANGE_HANDLER(IDC_BROWSELIST, IDC_BROWSELIST + menuItems, onBrowseList)
		COMMAND_RANGE_HANDLER(IDC_REMOVE_SOURCE, IDC_REMOVE_SOURCE + menuItems, onRemoveSource)
		COMMAND_RANGE_HANDLER(IDC_REMOVE_SOURCES, IDC_REMOVE_SOURCES + 1 + menuItems, onRemoveSources)
		COMMAND_RANGE_HANDLER(IDC_PM, IDC_PM + menuItems, onPM)
		COMMAND_RANGE_HANDLER(IDC_READD, IDC_READD + 1 + readdItems, onReadd)
		COMMAND_ID_HANDLER(IDC_AUTOPRIORITY, onAutoPriority)
		NOTIFY_HANDLER(IDC_QUEUE, NM_CUSTOMDRAW, onCustomDraw)
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_COMMANDS(prevBase)
		ALT_MSG_MAP(SHOWTREE_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowTree)
		END_MSG_MAP()
		
		LRESULT onPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSegments(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveSource(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveSources(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onReadd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRecheck(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTreeItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onAutoPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onRemoveOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onShowQueueItem(UINT, WPARAM, LPARAM, BOOL&);
		
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
		
		LRESULT onDoubleClick(int idCtrl, LPNMHDR /*pnmh*/, BOOL& bHandled);
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void changePriority(bool inc);
		void showQueueItem(string& target, bool isList);
		
		LRESULT onItemChangedQueue(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			const NMLISTVIEW* lv = (NMLISTVIEW*)pnmh;
			if ((lv->uNewState ^ lv->uOldState) & LVIS_SELECTED)
				updateStatus = true;
			return 0;
		}
		
		LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
		{
			if (ctrlQueue.IsWindow())
			{
				ctrlQueue.SetFocus();
			}
			return 0;
		}
		
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			usingDirMenu ? removeSelectedDir() : removeSelected();
			return 0;
		}

		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			removeAllDir();
			return 0;
		}
		
		LRESULT onMove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			usingDirMenu ? moveSelectedDir() : moveSelected();
			return 0;
		}
		
		LRESULT onRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			usingDirMenu ? renameSelectedDir() : renameSelected();
			return 0;
		}
		
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
		LRESULT onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

		void onTab();
		
		LRESULT onShowTree(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			showTree = (wParam == BST_CHECKED);
			UpdateLayout(FALSE);
			return 0;
		}
		
	private:
		enum
		{
			COLUMN_FIRST,
			COLUMN_TARGET = COLUMN_FIRST,
			COLUMN_TYPE,
			COLUMN_STATUS,
			COLUMN_SEGMENTS,
			COLUMN_SIZE,
			COLUMN_PROGRESS,
			COLUMN_DOWNLOADED,
			COLUMN_PRIORITY,
			COLUMN_USERS,
			COLUMN_PATH,
			COLUMN_LOCAL_PATH, // FIXME: remove this column
			COLUMN_EXACT_SIZE,
			COLUMN_ERRORS,
			COLUMN_ADDED,
			COLUMN_TTH,
			COLUMN_SPEED,
			COLUMN_LAST
		};

		enum Tasks
		{
			ADD_ITEM,
			REMOVE_ITEM,
			REMOVE_ITEM_OLD_PATH,
			UPDATE_ITEM,
			UPDATE_STATUS
		};
		
		vector<QueueItem::RunningSegment> runningChunks;
		vector<Segment> doneChunks;

		vector<std::pair<std::string, UserPtr> > sourcesToRemove;

		void removeSources();
		
		class DirItem
		{
			public:
				string name;
				std::list<DirItem*> directories;
				std::list<QueueItemPtr> files;
				DirItem* parent;
				int64_t totalSize;
				size_t totalFileCount;
				HTREEITEM ht;

#ifdef DEBUG_QUEUE_FRAME
				static int itemsCreated;
				static int itemsRemoved;
				~DirItem()
				{
					++itemsRemoved;
				}
#endif

				DirItem() : parent(nullptr), totalSize(0), totalFileCount(0), ht(nullptr)
				{
#ifdef DEBUG_QUEUE_FRAME
					++itemsCreated;
#endif
				}
		};

		class QueueItemInfo
		{
			public:
				enum ItemType
				{
					DIRECTORY,
					FILE
				} const type;		
	
#ifdef DEBUG_QUEUE_FRAME
				static int itemsCreated;
				static int itemsRemoved;
				~QueueItemInfo()
				{
					++itemsRemoved;
				}
#endif
				
				explicit QueueItemInfo(const QueueItemPtr& qi) : qi(qi), dir(nullptr), type(FILE)
				{
#ifdef DEBUG_QUEUE_FRAME
					++itemsCreated;
#endif
				}

				explicit QueueItemInfo(const DirItem* dir) : dir(dir), type(DIRECTORY)
				{
#ifdef DEBUG_QUEUE_FRAME
					++itemsCreated;
#endif
				}

				const tstring getText(int col) const;
				static int compareItems(const QueueItemInfo* a, const QueueItemInfo* b, int col);
				
				void updateIconIndex();
				int getImageIndex() const { return iconIndex; }
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
				const QueueItemPtr& getQueueItem() const
				{
					return qi;
				}
				const DirItem* getDirItem() const
				{
					return dir;
				}

				int64_t getSize() const
				{
					if (dir)
						return dir->totalSize;
					if (qi)
						return qi->getSize();
					return 0;
				}
				int64_t getDownloadedBytes() const
				{
					if (qi)
						return qi->getDownloadedBytes();
					return 0;
				}
				QueueItem::Priority getPriority() const
				{
					if (qi)
						return qi->getPriority();
					return QueueItem::Priority();
				}
				bool isWaiting() const
				{
					if (qi)
						return qi->isWaiting();
					return false;
				}
				bool isFinished() const
				{
					if (qi)
						return qi->isFinished();
					return false;
				}
				
				QueueItemInfo& operator= (const QueueItemInfo&) = delete;

			private:
				const QueueItemPtr qi;
				const DirItem* const dir;
				int iconIndex = -1;
		};
		
		struct QueueItemTask : public Task
		{
			explicit QueueItemTask(const QueueItemPtr& qi) : qi(qi) { }
			QueueItemPtr qi;
		};
		
		struct TargetTask : public Task
		{
				explicit TargetTask(const string& target) : target(target) { }
				const string target;
		};

		struct RemoveQueueItemTask : public Task
		{
			RemoveQueueItemTask(const QueueItemPtr& qi, const string& path) : qi(qi), path(path) {}
			QueueItemPtr qi;
			const string path;
		};

		static HIconWrapper frameIcon;

		OMenu browseMenu;
		OMenu removeMenu;
		OMenu removeAllMenu;
		OMenu pmMenu;
		OMenu readdMenu;
		
		CButton ctrlShowTree;
		CContainedWindow showTreeContainer;
		bool showTree;
		bool usingDirMenu;
		size_t lastTotalCount;
		int64_t lastTotalSize;
		
		int menuItems;
		int readdItems;
		
		DirItem* root;
		DirItem* fileLists;
		int clearingTree;
		const DirItem* currentDir;
		string currentDirPath;
		bool treeInserted;
		
		TypedListViewCtrl<QueueItemInfo, IDC_QUEUE> ctrlQueue;
		CTreeViewCtrl ctrlDirs;
		
		CStatusBarCtrl ctrlStatus;
		int statusSizes[6]; // TODO: fix my size.
		bool updateStatus;
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];

		TaskQueue tasks;
		TimerHelper timer;
		
		void addQueueList();
		void addQueueItem(const QueueItemPtr& qi, bool sort, bool updateTree);
		void addItem(DirItem* dir, const string& path, string::size_type pathStart, const string& filename, const QueueItemPtr& qi, bool updateTree);
		void splitDir(DirItem* dir, string::size_type pos, DirItem* newParent, bool updateTree);
		static void removeFromParent(DirItem* dir);
		static string getFullPath(const DirItem* dir);
		static void walkDirItem(const DirItem* dir, std::function<void(const QueueItemPtr&)> func);
		void deleteDirItem(DirItem* dir);
		static void deleteTree(DirItem* dir);
		bool findItem(const QueueItemPtr& qi, const string& path, DirItem* &dir, list<QueueItemPtr>::iterator &file) const;
		bool removeItem(const QueueItemPtr& qi, const string* oldPath);
		bool isCurrentDir(const string& target) const;
		void insertListItem(const QueueItemPtr& qi, const string& dirname, bool sort);

		void insertTreeItem(DirItem* dir, HTREEITEM htParent, HTREEITEM htAfter);
		void insertFileListsItem();
		void insertSubtree(DirItem* dir, HTREEITEM htParent);
		void insertTrees();
		void removeEmptyDir(DirItem* dir);
		
		void updateQueue();
		void updateQueueStatus();
		
		void moveDir(const DirItem* dir, const string& newPath);
		void moveSelected();
		void moveSelectedDir();
		
		const QueueItemInfo* getSelectedQueueItem() const
		{
			int pos = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
			if (pos < 0) return nullptr;
			return ctrlQueue.getItemData(pos);
		}
		
		typedef pair<QueueItemPtr, string> TempMovePair;
		vector<TempMovePair> itemsToMove;
		void moveTempArray();
		
		bool confirmDelete();
		void removeSelected();
		void removeSelectedDir();
		void removeAllDir();
		
		void renameSelected();
		void renameSelectedDir();
		
		void onTimerInternal();
		void processTasks();
		void addTask(Tasks s, Task* task);

		void on(QueueManagerListener::Added, const QueueItemPtr& qi) noexcept override;
#if 0
		void on(QueueManagerListener::AddedArray, const std::vector<QueueItemPtr>& qiAarray) noexcept override;
#endif
		void on(QueueManagerListener::Moved, const QueueItemPtr& qi, const string& oldTarget) noexcept override;
		void on(QueueManagerListener::Removed, const QueueItemPtr& qi) noexcept override;
#if 0
		void on(QueueManagerListener::RemovedArray, const std::vector<string>& qiArray) noexcept override;
#endif
		void on(QueueManagerListener::TargetsUpdated, const StringList& targets) noexcept override;
		void on(QueueManagerListener::StatusUpdated, const QueueItemPtr& qi) noexcept override;
		void on(QueueManagerListener::StatusUpdatedList, const QueueItemList& itemList) noexcept override;
		void on(QueueManagerListener::Tick, const QueueItemList& list) noexcept override;
		void on(SettingsManagerListener::Repaint) override;
		
		void onRechecked(const string& target, const string& message);
		
		void on(QueueManagerListener::RecheckStarted, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckNoFile, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckFileTooSmall, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckDownloadsRunning, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckNoTree, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckAlreadyFinished, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckDone, const string& target) noexcept override;
		
#ifdef FLYLINKDC_USE_TORRENT
		void on(DownloadManagerListener::TorrentEvent, const DownloadArray&) noexcept override;
		void on(DownloadManagerListener::RemoveTorrent, const libtorrent::sha1_hash& sha1) noexcept override;
		void on(DownloadManagerListener::CompleteTorrentFile, const std::string& fileName) noexcept override;
		void on(DownloadManagerListener::AddedTorrent, const libtorrent::sha1_hash& sha1, const std::string& savePath) noexcept override;
#endif
};

#endif // !defined(QUEUE_FRAME_H)
