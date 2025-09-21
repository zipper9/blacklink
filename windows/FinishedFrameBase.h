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

#ifndef FINISHED_FRAME_BASE_H
#define FINISHED_FRAME_BASE_H

#include "MDITabChildWindow.h"
#include "StaticFrame.h"
#include "TypedListViewCtrl.h"
#include "WinUtil.h"
#include "ImageLists.h"
#include "SplitWnd.h"
#include "StatusBarCtrl.h"
#include "../client/FinishedManager.h"
#include "../client/SettingsManager.h"
#include "../client/DatabaseManager.h"
#include "../client/SysVersion.h"

#define FINISHED_TREE_MESSAGE_MAP 11
#define FINISHED_LIST_MESSAGE_MAP 12

class FinishedFrameBase;

class FinishedFrameLoader : public Thread
{
		friend class FinishedFrameBase;

	protected:
		virtual int run();

	private:
		FinishedFrameBase* parent = nullptr;
		HWND hwnd = nullptr;
};

class FinishedFrameBase
{
		friend class FinishedFrameLoader;
		const eTypeTransfer transferType;

	public:
		FinishedFrameBase(eTypeTransfer transferType) :
			transferType(transferType),
			currentTreeItemSelected(false),
			loading(false),
			abortFlag(false),
			totalBytes(0),
			totalActual(0),
			totalSpeed(0),
			totalCount(0),
			totalCountLast(0),
			type(transferType == e_TransferUpload ? FinishedManager::e_Upload : FinishedManager::e_Download)
		{
		}

		FinishedFrameBase(const FinishedFrameBase&) = delete;
		FinishedFrameBase& operator= (const FinishedFrameBase&) = delete;

		virtual ~FinishedFrameBase() {}

	protected:
		enum TreeItemType
		{
			RootRecent,
			RootDatabase,
			HistoryDate
		};

		enum
		{
			SPEAK_ADD_ITEM,
			SPEAK_REMOVE_ITEM,
			SPEAK_UPDATE_STATUS,
			SPEAK_REMOVE_DROPPED_ITEMS,
			SPEAK_FINISHED
		};

		enum
		{
			STATUS_TEXT,
			STATUS_COUNT,
			STATUS_BYTES,
			STATUS_ACTUAL,
			STATUS_SPEED,
			STATUS_LAST
		};

		class FinishedItemInfo
		{
			public:
				FinishedItemInfo(const FinishedItemPtr& fi) : entry(fi)
				{
					for (size_t i = FinishedItem::COLUMN_FIRST; i < FinishedItem::COLUMN_LAST; ++i)
						columns[i] = fi->getText(i);
				}
				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < FinishedItem::COLUMN_LAST);
					return columns[col];
				}
				static int compareItems(const FinishedItemInfo* a, const FinishedItemInfo* b, int col, int /*flags*/)
				{
					return FinishedItem::compareItems(a->entry.get(), b->entry.get(), col);
				}
				static int getCompareFlags()
				{
					return 0;
				}
				int getImageIndex() const
				{
					return g_fileImage.getIconIndex(entry->getTarget());
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}

			public:
				FinishedItemPtr entry;

			private:
				tstring columns[FinishedItem::COLUMN_LAST];
		};

		struct TreeItemData
		{
			TreeItemType type;
			unsigned dateAsInt;
		};

		bool currentTreeItemSelected;

		vector<TransferHistorySummary> summary;
		FinishedFrameLoader loader;
		bool loading;
		std::atomic_bool abortFlag;

		StatusBarCtrl ctrlStatus;
		CMenu copyMenu;
		OMenu directoryMenu;

		TypedListViewCtrl<FinishedItemInfo> ctrlList;

		CTreeViewCtrl ctrlTree;
		HTREEITEM rootItem;
		HTREEITEM rootRecent;
		HTREEITEM rootDatabase;

		int64_t totalBytes;
		int64_t totalActual;
		int64_t totalSpeed;
		int64_t totalCount;
		int64_t totalCountLast;

		const FinishedManager::eType type;
		int boldFinished;
		int columnWidth;
		int columnOrder;
		int columnVisible;
		int columnSort;
		int splitterPos;

		HTREEITEM createRootItem(TreeItemType nodeType);
		void insertData();
		void addStatusLine(const tstring& line);
		void updateStatus();
		void updateList(const FinishedItemList& fl);
		void addFinishedEntry(const FinishedItemPtr& entry, bool ensureVisible);
		void removeDroppedItems(int64_t maxTempId);
		void appendMenuItems(OMenu& menu, bool fileExists, const CID& userCid, int& copyMenuPos);
		void removeTreeItem();

		void onCreate(HWND hwnd, int id);
		bool onSpeaker(WPARAM wParam, LPARAM lParam);
		LRESULT onContextMenu(HWND hwnd, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSelChangedTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onTreeItemDeleted(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onReDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGrant(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */);
};

template<class T, int title, int id, int icon>
class FinishedFrame : public MDITabChildWindowImpl<T>,
	public StaticFrame<T, title, id>,
	public SplitWndImpl<FinishedFrame<T, title, id, icon>>,
	public FinishedFrameBase,
	protected FinishedManagerListener,
	private SettingsManagerListener
{
	public:
		typedef MDITabChildWindowImpl<T> baseClass;

		FinishedFrame(eTypeTransfer transferType) :
			FinishedFrameBase(transferType),
			treeContainer(WC_TREEVIEW, this, FINISHED_TREE_MESSAGE_MAP),
			listContainer(WC_LISTVIEW, this, FINISHED_LIST_MESSAGE_MAP)
		{
			StatusBarCtrl::PaneInfo pi;
			pi.minWidth = 0;
			pi.maxWidth = INT_MAX;
			pi.weight = 1;
			pi.align = StatusBarCtrl::ALIGN_LEFT;
			pi.flags = 0;
			ctrlStatus.addPane(pi);

			pi.flags = StatusBarCtrl::PANE_FLAG_HIDE_EMPTY | StatusBarCtrl::PANE_FLAG_NO_SHRINK;
			pi.weight = 0;
			for (int i = 1; i < STATUS_LAST; ++i)
				ctrlStatus.addPane(pi);
		}

	protected:
		BEGIN_MSG_MAP(T)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVE_TREE_ITEM, onRemoveTreeItem)
		COMMAND_ID_HANDLER(IDC_REMOVE_ALL, onRemoveAll)
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenFile)
		COMMAND_ID_HANDLER(IDC_REDOWNLOAD_FILE, onReDownload)
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER, onOpenFolder)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetList)
		COMMAND_ID_HANDLER(IDC_GRANTSLOT, onGrant)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TYPE, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_PATH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy)
		COMMAND_ID_HANDLER(IDC_NETWORK_TRAFFIC, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_HUB_URL, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_SPEED, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_IP, onCopy)
		NOTIFY_HANDLER(id, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(id, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(id, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(id, NM_DBLCLK, onDoubleClick)
		NOTIFY_HANDLER(IDC_TRANSFER_TREE, TVN_SELCHANGED, onSelChangedTree)
		NOTIFY_HANDLER(IDC_TRANSFER_TREE, TVN_DELETEITEM, onTreeItemDeleted)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(SplitWndImpl<FinishedFrame>)
		ALT_MSG_MAP(FINISHED_TREE_MESSAGE_MAP)
		ALT_MSG_MAP(FINISHED_LIST_MESSAGE_MAP)

		END_MSG_MAP()

		static const int columnId[FinishedItem::COLUMN_LAST];

		CContainedWindow listContainer;
		CContainedWindow treeContainer;

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			FinishedFrameBase::onCreate(this->m_hWnd, id);
			treeContainer.SubclassWindow(ctrlTree);

			auto ss = SettingsManager::instance.getUiSettings();
			this->addSplitter(-1, SplitWndBase::FLAG_HORIZONTAL | SplitWndBase::FLAG_PROPORTIONAL | SplitWndBase::FLAG_INTERACTIVE, ss->getInt(splitterPos));
			if (Colors::isAppThemed && SysVersion::isOsVistaPlus())
				this->setSplitterColor(0, SplitWndBase::COLOR_TYPE_SYSCOLOR, COLOR_WINDOW);
			this->setPaneWnd(0, ctrlTree.m_hWnd);
			this->setPaneWnd(1, ctrlList.m_hWnd);

			SettingsManager::instance.addListener(this);
			FinishedManager::getInstance()->addListener(this);

			UpdateLayout();
			bHandled = FALSE;
			return TRUE;
		}

		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (!this->closed)
			{
				this->closed = true;
				FinishedManager::getInstance()->removeListener(this);
				SettingsManager::instance.removeListener(this);
				if (loading)
				{
					abortFlag.store(true);
					loader.join();
				}

				this->setButtonPressed(id, false);
				this->PostMessage(WM_CLOSE);
				return 0;
			}
			else
			{
				ctrlList.saveHeaderOrder(columnOrder, columnWidth, columnVisible);
				auto ss = SettingsManager::instance.getUiSettings();
				ss->setInt(columnSort, ctrlList.getSortForSettings());
				ss->setInt(splitterPos, this->getSplitterPos(0, true));
				ctrlList.deleteAll();

				bHandled = FALSE;
				return 0;
			}
		}

		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			this->PostMessage(WM_CLOSE);
			return 0;
		}

		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			bool updated = FinishedFrameBase::onSpeaker(wParam, lParam);
			if (updated && SettingsManager::instance.getUiSettings()->getBool(boldFinished))
				this->setDirty();
			return 0;
		}

		LRESULT onRemoveTreeItem(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			HTREEITEM treeItem = ctrlTree.GetSelectedItem();
			if (!treeItem) return 0;
			DWORD_PTR itemData = ctrlTree.GetItemData(treeItem);
			if (!itemData) return 0;
			const TreeItemData* data = reinterpret_cast<const TreeItemData*>(itemData);
			if (data->type == HistoryDate)
				this->SendMessage(WM_COMMAND, IDC_REMOVE_ALL);
			return 0;
		}

		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			return FinishedFrameBase::onContextMenu(this->m_hWnd, wParam, lParam, bHandled);
		}

		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
		{
			FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
			opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(icon, 0);
			opt->isHub = false;
			return TRUE;
		}

		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = reinterpret_cast<NMLVKEYDOWN*>(pnmh);
			if (kd->wVKey == VK_DELETE)
				this->PostMessage(WM_COMMAND, IDC_REMOVE);
			return 0;
		}

		void UpdateLayout(BOOL = TRUE)
		{
			RECT rect;
			this->GetClientRect(&rect);
			RECT prevRect = rect;

			if (ctrlStatus)
			{
				HDC hdc = this->GetDC();
				int statusHeight = ctrlStatus.getPrefHeight(hdc);
				rect.bottom -= statusHeight;
				RECT rcStatus = rect;
				rcStatus.top = rect.bottom;
				rcStatus.bottom = rcStatus.top + statusHeight;
				ctrlStatus.SetWindowPos(nullptr, &rcStatus, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
				ctrlStatus.updateLayout(hdc);
				this->ReleaseDC(hdc);
			}

			MARGINS margins;
			WinUtil::getMargins(margins, prevRect, rect);
			this->setMargins(margins);
			this->updateLayout();
		}

	protected:
		void on(UpdateStatus) noexcept override
		{
			this->SendMessage(WM_SPEAKER, SPEAK_UPDATE_STATUS, 0);
		}

		void on(SettingsManagerListener::ApplySettings) override
		{
			if (ctrlList.isRedraw())
			{
				setTreeViewColors(ctrlTree);
				this->RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
			}
		}

		void on(FinishedManagerListener::DroppedItems, int64_t maxTempId) noexcept override
		{
			WinUtil::postSpeakerMsg(this->m_hWnd, SPEAK_REMOVE_DROPPED_ITEMS, new int64_t(maxTempId));
		}
};

#endif // !defined(FINISHED_FRAME_BASE_H)
