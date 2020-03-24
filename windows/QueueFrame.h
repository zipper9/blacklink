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

class QueueFrame : public MDITabChildWindowImpl<QueueFrame>,
	public StaticFrame<QueueFrame, ResourceManager::DOWNLOAD_QUEUE, IDC_QUEUE>,
	private QueueManagerListener,
	private DownloadManagerListener,
	public CSplitterImpl<QueueFrame>,
	public PreviewBaseHandler<QueueFrame, false>,
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
		typedef PreviewBaseHandler<QueueFrame, false> prevBase;
		
		BEGIN_MSG_MAP(QueueFrame)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETDISPINFO, ctrlQueue.onGetDispInfo)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_COLUMNCLICK, ctrlQueue.onColumnClick)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETINFOTIP, ctrlQueue.onInfoTip)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_ITEMCHANGED, onItemChangedQueue)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		NOTIFY_HANDLER(IDC_QUEUE, NM_DBLCLK, onSearchDblClick)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
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
		LRESULT onItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onAutoPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onRemoveOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		
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
		
		LRESULT onSearchDblClick(int idCtrl, LPNMHDR /*pnmh*/, BOOL& bHandled)
		{
			return onSearchAlternates(BN_CLICKED, (WORD)idCtrl, m_hWnd, bHandled); // !SMT!-UI
		}
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void removeDir(HTREEITEM ht);
		void setPriority(HTREEITEM ht, const QueueItem::Priority& p);
		void setAutoPriority(HTREEITEM ht, const bool& ap);
		void changePriority(bool inc);
		
		LRESULT onItemChangedQueue(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			const NMLISTVIEW* lv = (NMLISTVIEW*)pnmh;
			if ((lv->uNewState & LVIS_SELECTED) != (lv->uOldState & LVIS_SELECTED))
			{
				m_update_status++;
			}
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
			COLUMN_LOCAL_PATH,
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
			REMOVE_ITEM_ARRAY,
			UPDATE_ITEM,
			UPDATE_STATUS
		};
		
		vector<QueueItem::RunningSegment> runningChunks;
		vector<Segment> doneChunks;

		StringList m_tmp_target_to_delete; // [+] NightOrion bugfix deleting folder from queue
		
		vector<std::pair<std::string, UserPtr> > m_remove_source_array;

		void removeSources();
		
		class QueueItemInfo
		{
			public:
				explicit QueueItemInfo(const QueueItemPtr& qi) : m_qi(qi)
				{
				}
#ifdef FLYLINKDC_USE_TORRENT
				explicit QueueItemInfo(const libtorrent::sha1_hash& sha1, const std::string& savePath) : sha1(sha1), m_save_path(savePath)
				{
				}
#endif
				QueueItemInfo& operator= (const QueueItemInfo&) = delete;
				const tstring getText(int col) const;
				static int compareItems(const QueueItemInfo* a, const QueueItemInfo* b, int col);
				void removeTarget(bool p_is_batch_remove);
				
				void removeBatch()
				{
					removeTarget(true);
				}
				int getImageIndex() const
				{
					return g_fileImage.getIconIndex(getTarget());
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
#ifdef FLYLINKDC_USE_TORRENT
				bool isTorrent() const
				{
					return m_qi == nullptr;
				}
#endif
				const QueueItemPtr& getQueueItem() const
				{
					return m_qi;
				}
				string getPath() const
				{
					return Util::getFilePath(getTarget());
				}
				bool isSet(Flags::MaskType aFlag) const
				{
					if (m_qi)
						return m_qi->isSet(aFlag);
					return false; // TODO
				}
				bool isAnySet(Flags::MaskType aFlag) const
				{
					if (m_qi)
						return m_qi->isAnySet(aFlag);
					return false; // TODO
				}
				string getTarget() const
				{
					if (m_qi)
						return m_qi->getTarget();
#ifdef FLYLINKDC_USE_TORRENT
					return m_save_path; // TODO
#else
					return string();
#endif
				}
				int64_t getSize() const
				{
					if (m_qi)
						return m_qi->getSize();
					return 0; // TODO
				}
				int64_t getDownloadedBytes() const
				{
					if (m_qi)
						return m_qi->getDownloadedBytes();
					return  0; // TODO
				}
				time_t getAdded() const
				{
					if (m_qi)
						return m_qi->getAdded();
					return GET_TIME(); // TODO
				}
				const TTHValue getTTH() const
				{
					if (m_qi)
						return m_qi->getTTH();
					return TTHValue(); // TODO
				}
				QueueItem::Priority getPriority() const
				{
					if (m_qi)
						return m_qi->getPriority();
					return QueueItem::Priority(); // TODO
				}
				bool isWaiting() const
				{
					if (m_qi)
						return m_qi->isWaiting();
					return false; // TODO
				}
				bool isFinished() const
				{
					if (m_qi)
						return m_qi->isFinished();
					return false; // TODO
				}
				bool getAutoPriority() const
				{
					if (m_qi)
						return m_qi->getAutoPriority();
					return false; // TODO
				}
				
			private:
				const QueueItemPtr m_qi;
#ifdef FLYLINKDC_USE_TORRENT
				const libtorrent::sha1_hash sha1;
				string m_save_path;
#endif
		};
		
		struct QueueItemInfoTask : public Task
		{
			explicit QueueItemInfoTask(QueueItemInfo* ii) : ii(ii) { }
			QueueItemInfo* ii;
		};
		
		struct UpdateTask : public Task
		{
				explicit UpdateTask(const string& target) : target(target) { }
#ifdef FLYLINKDC_USE_TORRENT
				libtorrent::sha1_hash sha1;
				explicit UpdateTask(const string& target, const libtorrent::sha1_hash& sha1) : target(target), sha1(sha1) { }
#endif
				const string& getTarget() const { return target; }


			private:
				const string target;
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
		bool m_dirty;
		unsigned m_last_count;
		int64_t  m_last_total;
		
		int menuItems;
		int readdItems;
		
		HTREEITEM m_fileLists;
		
		typedef pair<string, QueueItemInfo*> DirectoryMapPair;
		typedef std::unordered_multimap<string, QueueItemInfo*, noCaseStringHash, noCaseStringEq> QueueDirectoryMap;
		typedef QueueDirectoryMap::iterator QueueDirectoryIter;
		typedef QueueDirectoryMap::const_iterator QueueDirectoryIterC;
		typedef pair<QueueDirectoryIterC, QueueDirectoryIterC> QueueDirectoryPairC;
		QueueDirectoryMap m_directories;
		string curDir;
		
		TypedListViewCtrl<QueueItemInfo, IDC_QUEUE> ctrlQueue;
		CTreeViewCtrl ctrlDirs;
		
		CStatusBarCtrl ctrlStatus;
		int statusSizes[6]; // TODO: fix my size.
		
		int64_t m_queueSize;
		int m_queueItems;
		int m_update_status;
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];

		TaskQueue tasks;
		TimerHelper timer;
		
		void addQueueList();
		void addQueueItem(QueueItemInfo* qi, bool noSort);
		
		HTREEITEM addDirectory(const string& dir, bool isFileList = false, HTREEITEM startAt = NULL);
		void removeDirectory(const string& dir, bool isFileList = false);
		void removeDirectories(HTREEITEM ht);
		
		void updateQueue();
		void updateQueueStatus();
		
		bool isCurDir(const string& aDir) const
		{
			return stricmp(curDir, aDir) == 0;
		}
		
		void moveSelected();
		void moveSelectedDir();
		void moveDir(HTREEITEM ht, const string& target);
		const QueueItemInfo* getSelectedQueueItem() const
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(ctrlQueue.GetNextItem(-1, LVNI_SELECTED));
			return ii;
		}
		
		void moveNode(HTREEITEM item, HTREEITEM parent);
		
		// temporary vector for moving directories
		typedef pair<QueueItemInfo*, string> TempMovePair;
		vector<TempMovePair> m_move_temp_array;
		void moveTempArray();
		
		void clearTree(HTREEITEM item);
		
		QueueItemInfo* getItemInfo(const string& target, const string& p_path) const;
		
		bool confirmDelete();
		void removeSelected();
		void removeSelectedDir();
		void removeAllDir();
		
		void renameSelected();
		void renameSelectedDir();
		
		string getSelectedDir() const;
		string getDir(HTREEITEM ht) const;
		
		void removeItem(const string& p_target);

		void onTimerInternal();
		void processTasks();
		void addTask(Tasks s, Task* task);

		void on(QueueManagerListener::Added, const QueueItemPtr& aQI) noexcept override;
		void on(QueueManagerListener::AddedArray, const std::vector<QueueItemPtr>& p_qi_array) noexcept override;
		void on(QueueManagerListener::Moved, const QueueItemPtr& aQI, const string& oldTarget) noexcept override;
		void on(QueueManagerListener::Removed, const QueueItemPtr& aQI) noexcept override;
		void on(QueueManagerListener::RemovedArray, const std::vector<string>& p_qi_array) noexcept override;
		void on(QueueManagerListener::TargetsUpdated, const StringList& p_targets) noexcept override;
		void on(QueueManagerListener::StatusUpdated, const QueueItemPtr& aQI) noexcept override;
		void on(QueueManagerListener::StatusUpdatedList, const QueueItemList& p_list) noexcept override; // [+] IRainman opt.
		void on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept override; // [+] IRainman opt.
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
		void on(DownloadManagerListener::RemoveTorrent, const libtorrent::sha1_hash& p_sha1) noexcept override;
		void on(DownloadManagerListener::CompleteTorrentFile, const std::string& p_file_name) noexcept override;
		void on(DownloadManagerListener::AddedTorrent, const libtorrent::sha1_hash& p_sha1, const std::string& p_save_path) noexcept override;
#endif
};

#endif // !defined(QUEUE_FRAME_H)
