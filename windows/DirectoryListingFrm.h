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

#ifndef DIRECTORY_LISTING_FRM_H
#define DIRECTORY_LISTING_FRM_H

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "UCHandler.h"
#include "ImageLists.h"
#include "TimerHelper.h"
#include "CustomDrawHelpers.h"
#include "BaseHandlers.h"

#include "../client/DirectoryListing.h"
#include "../client/StringSearch.h"
#include "../client/ADLSearch.h"
#include "../client/ShareManager.h"
#include "../client/SettingsManager.h"

class ThreadedDirectoryListing;

#define STATUS_MESSAGE_MAP 9
#define CONTROL_MESSAGE_MAP 10

class DirectoryListingFrame : public MDITabChildWindowImpl<DirectoryListingFrame>,
	public CSplitterImpl<DirectoryListingFrame>,
	public UCHandler<DirectoryListingFrame>, private SettingsManagerListener,
	public InternetSearchBaseHandler,
	private TimerHelper,
	public CMessageFilter
{
		static const int DEFAULT_PRIO = QueueItem::HIGHEST + 1;
		static const int MAX_FAV_DIRS = 100;

	public:
		static void openWindow(const tstring& aFile, const tstring& aDir, const HintedUser& aUser, int64_t aSpeed, bool isDCLST = false);
		static void openWindow(const HintedUser& aUser, const string& txt, int64_t aSpeed);
		static void closeAll();
		static void closeAllOffline();

		typedef MDITabChildWindowImpl<DirectoryListingFrame> baseClass;
		typedef UCHandler<DirectoryListingFrame> ucBase;

		enum
		{
			COLUMN_FILENAME    = 0,
			COLUMN_TYPE        = 1,
			COLUMN_EXACT_SIZE  = 2,
			COLUMN_SIZE        = 3,
			COLUMN_TTH         = 4,
			COLUMN_PATH        = 5,
			COLUMN_HIT         = 6,
			COLUMN_TS          = 7,
			COLUMN_BITRATE     = 9,
			COLUMN_MEDIA_XY    = 10,
			COLUMN_MEDIA_VIDEO = 11,
			COLUMN_MEDIA_AUDIO = 12,
			COLUMN_DURATION    = 13,
			COLUMN_LAST
		};

		enum
		{
			FINISHED,
			ABORTED,
			SPLICE_TREE,
			PROGRESS = 0x1000
		};

		enum
		{
			STATUS_TEXT,
			STATUS_SPEED,
			STATUS_TOTAL_FILES,
			STATUS_TOTAL_FOLDERS,
			STATUS_TOTAL_SIZE,
			STATUS_SELECTED_FILES,
			STATUS_SELECTED_SIZE,
			STATUS_FILE_LIST_DIFF,
			STATUS_MATCH_QUEUE,
			STATUS_FIND,
			STATUS_PREV,
			STATUS_NEXT,
			STATUS_DUMMY,
			STATUS_LAST
		};
		FileImage::TypeDirectoryImages getDirectoryIcon(const DirectoryListing::Directory* dir) const;

		DirectoryListingFrame(const HintedUser& user, DirectoryListing *dl);
		~DirectoryListingFrame();

		static CFrameWndClassInfo& GetWndClassInfo();

		BEGIN_MSG_MAP(DirectoryListingFrame)
		NOTIFY_HANDLER(IDC_FILES, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_FILES, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(IDC_FILES, NM_CUSTOMDRAW, onCustomDrawList)
		NOTIFY_HANDLER(IDC_FILES, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_FILES, NM_DBLCLK, onDoubleClickFiles)
		NOTIFY_HANDLER(IDC_FILES, LVN_ITEMCHANGED, onListItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onSelChangedDirectories)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_CUSTOMDRAW, onCustomDrawTree)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBackground)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenFile)
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER, onOpenFolder)
		COMMAND_ID_HANDLER(IDC_DOWNLOADDIRTO, onDownloadDirTo)
		COMMAND_ID_HANDLER(IDC_DOWNLOADDIRTO_USER, onDownloadDirCustom)
		COMMAND_ID_HANDLER(IDC_DOWNLOADDIRTO_IP, onDownloadDirCustom)
		COMMAND_ID_HANDLER(IDC_DOWNLOADTO, onDownloadTo)
#ifdef DEBUG_TRANSFERS
		COMMAND_ID_HANDLER(IDC_DOWNLOAD_BY_PATH, onDownloadByPath)
#endif
		COMMAND_ID_HANDLER(IDC_DOWNLOADTO_USER, onDownloadCustom)
		COMMAND_ID_HANDLER(IDC_DOWNLOADTO_IP, onDownloadCustom)
		COMMAND_ID_HANDLER(IDC_GO_TO_DIRECTORY, onGoToDirectory)
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchByTTH)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy)
		COMMAND_ID_HANDLER(IDC_ADD_TO_FAVORITES, onAddToFavorites)
		COMMAND_ID_HANDLER(IDC_MARK_AS_DOWNLOADED, onMarkAsDownloaded)
		COMMAND_ID_HANDLER(IDC_PRIVATE_MESSAGE, onPM)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_EXACT_SIZE, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_PATH, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_FOLDER_NAME, onCopyFolder)
		COMMAND_ID_HANDLER(IDC_COPY_FOLDER_PATH, onCopyFolder)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_DIR_LIST, onCloseAll)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_OFFLINE_DIR_LIST, onCloseOffline)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_GENERATE_DCLST, onGenerateDcLst)
		COMMAND_ID_HANDLER(IDC_GENERATE_DCLST_FILE, onGenerateDcLst)
		COMMAND_ID_HANDLER(IDC_SHOW_DUPLICATES, onShowDuplicates)
		COMMAND_ID_HANDLER(IDC_GOTO_ORIGINAL, onGoToOriginal)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET + 1, IDC_DOWNLOAD_TARGET + LastDir::get().size(), onDownloadToLastDir)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET_TREE + 1, IDC_DOWNLOAD_TARGET_TREE + LastDir::get().size(), onDownloadToLastDirTree)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_WITH_PRIO, IDC_DOWNLOAD_WITH_PRIO + DEFAULT_PRIO, onDownloadWithPrio)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_WITH_PRIO_TREE, IDC_DOWNLOAD_WITH_PRIO_TREE + DEFAULT_PRIO, onDownloadWithPrioTree)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TO_FAV, IDC_DOWNLOAD_TO_FAV + MAX_FAV_DIRS - 1, onDownloadToFavDir)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOADDIR_TO_FAV, IDC_DOWNLOADDIR_TO_FAV + MAX_FAV_DIRS - 1, onDownloadToFavDirTree)
		COMMAND_RANGE_HANDLER(IDC_LOCATE_FILE_IN_QUEUE, IDC_LOCATE_FILE_IN_QUEUE + 9, onLocateInQueue)
		COMMAND_RANGE_HANDLER(IDC_COPY_URL, IDC_COPY_URL + 99, onCopyUrl)
		COMMAND_RANGE_HANDLER(IDC_COPY_URL_TREE, IDC_COPY_URL_TREE + 99, onCopyUrlTree)
		CHAIN_COMMANDS(InternetSearchBaseHandler)
		CHAIN_COMMANDS(ucBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(CSplitterImpl<DirectoryListingFrame>)
		CHAIN_MSG_MAP_ALT(DirectoryListingFrame, STATUS_MESSAGE_MAP)
		ALT_MSG_MAP(STATUS_MESSAGE_MAP)
		COMMAND_ID_HANDLER(IDC_FIND, onFind)
		COMMAND_ID_HANDLER(IDC_NEXT, onNext)
		COMMAND_ID_HANDLER(IDC_PREV, onPrev)
		COMMAND_ID_HANDLER(IDC_MATCH_QUEUE, onMatchQueueOrFindDups)
		COMMAND_ID_HANDLER(IDC_FILELIST_DIFF, onListDiff)
		ALT_MSG_MAP(CONTROL_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_XBUTTONUP, onXButtonUp)
		END_MSG_MAP()

		LRESULT onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // !SMT!-UI
		LRESULT onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+] NightOrion
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef DEBUG_TRANSFERS
		LRESULT onDownloadByPath(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onDownloadCustom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWithPrioTree(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDirTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDirCustom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadToFavDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadToFavDirTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadToLastDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadToLastDirTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onSearchFileOnInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			const auto ii = ctrlList.getSelectedItem();
			if (ii && ii->type == ItemInfo::FILE)
			{
				searchFileOnInternet(wID, ii->file->getName());
			}
			return 0;
		}
		LRESULT onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyUrl(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyUrlTree(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyFolder(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAddToFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMarkAsDownloaded(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGoToDirectory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClickFiles(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSelChangedDirectories(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onXButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onCustomDrawList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !fulDC!
		LRESULT onCustomDrawTree(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !fulDC!
		LRESULT onGenerateDcLst(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onShowDuplicates(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGoToOriginal(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLocateInQueue(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

		void downloadSelected(const tstring& target, bool view = false,  QueueItem::Priority prio = QueueItem::DEFAULT);
		void downloadSelected(bool view = false, QueueItem::Priority prio = QueueItem::DEFAULT)
		{
			downloadSelected(Util::emptyStringT, view, prio);
		}

		void refreshTree(DirectoryListing::Directory* tree, HTREEITEM treeItem, bool insertParent, const string& selPath = Util::emptyString);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		int getButtonWidth(ResourceManager::Strings id) const;
		void runUserCommand(UserCommand& uc);
		void loadFile(const tstring& name, const tstring& dir);
		void loadXML(const string& txt);

		void setFileName(const string& name) { fileName = name; }
		const string& getFileName() const { return fileName; }

		void setDclstFlag(bool dclstFlag) { this->dclstFlag = dclstFlag; }
		void setSpeed(int64_t speed) { this->speed = speed; }

		HTREEITEM findItem(HTREEITEM ht, const tstring& name);
		void selectItem(const tstring& name);

		LRESULT onListItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

		LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlList.SetFocus();
			return 0;
		}

		void setWindowTitle();

		LRESULT onEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 1;
		}

		LRESULT onFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPrev(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		LRESULT onMatchQueueOrFindDups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onListDiff(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

		LRESULT onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMTVKEYDOWN* kd = (NMTVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_TAB)
			{
				onTab();
			}
			return 0;
		}

		void onTab()
		{
			HWND focus = ::GetFocus();
			if (focus == ctrlTree.m_hWnd)
				ctrlList.SetFocus();
			else if (focus == ctrlList.m_hWnd)
				ctrlTree.SetFocus();
		}

		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}

		LRESULT onCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAll();
			return 0;
		}

		LRESULT onCloseOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAllOffline();
			return 0;
		}

		virtual BOOL PreTranslateMessage(MSG* pMsg) override;

		DirectoryListingFrame(const DirectoryListingFrame &) = delete;
		DirectoryListingFrame& operator= (const DirectoryListingFrame &) = delete;

	private:
		enum
		{
			SEARCH_CURRENT,
			SEARCH_NEXT,
			SEARCH_PREV,
			SEARCH_LAST
		};

		friend class ThreadedDirectoryListing;

		static DirectoryListingFrame* openWindow(DirectoryListing *dl, const HintedUser& user, int64_t speed, bool searchResults);
		static DirectoryListingFrame* findFrameByID(uint64_t id);

		void addToUserList(const UserPtr& user, bool isBrowsing);
		void removeFromUserList();
		void getFileItemColor(const Flags::MaskType flags, COLORREF &fg, COLORREF &bg);
		void getDirItemColor(const Flags::MaskType flags, COLORREF &fg, COLORREF &bg);
		void changeDir(const DirectoryListing::Directory* dir);
		void showDirContents(const DirectoryListing::Directory *dir, const DirectoryListing::Directory *selSubdir, const DirectoryListing::File *selFile);
		void selectFile(const DirectoryListing::File *file);
		void updateStatus();
		void initStatus();
		void enableControls();
		void addHistory(const string& name);
		void up();
		void back();
		void forward();
		void openFileFromList(const tstring& file);
		void showFound();
		void dumpFoundPath(); // DEBUG
		void clearSearch();
		void changeFoundItem();
		void updateSearchButtons();
		void updateTree(DirectoryListing::Directory* tree, HTREEITEM treeItem);
		void appendFavTargets(OMenu& menu, int idc);
		void appendCustomTargetItems(OMenu& menu, int idc);
		tstring getRootItemText() const;
		void updateRootItemText();
		bool addFavMenu(OMenu& menu);
		string getFileUrl(const string& hubUrl, const string& path) const;
		void appendCopyUrlItems(CMenu& menu, int idc, ResourceManager::Strings text);

		class ItemInfo
		{
				friend class DirectoryListingFrame;
			public:
				enum ItemType
				{
					FILE,
					DIRECTORY
				} type;

				union
				{
					DirectoryListing::File* file;
					DirectoryListing::Directory* dir;
				};

				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					return columns[col];
				}

				struct TotalSize
				{
					TotalSize() : total(0) { }
					void operator()(ItemInfo* a)
					{
						total += a->type == DIRECTORY ? a->dir->getTotalSize() : a->file->getSize();
					}
					int64_t total;
				};

				ItemInfo(DirectoryListing::File* f,  const DirectoryListing* dl);
				ItemInfo(DirectoryListing::Directory* d);

				static int compareItems(const ItemInfo* a, const ItemInfo* b, int col);
				void updateIconIndex();
				int getImageIndex() const { return iconIndex; }
				static uint8_t getStateImageIndex() { return 0; }

			private:
				tstring columns[COLUMN_LAST];
				int iconIndex;
		};

		const uint64_t id;
		uint64_t originalId;
		string nick;

		OMenu targetMenu;
		OMenu targetDirMenu;
		OMenu priorityMenu;
		OMenu priorityDirMenu;

		CContainedWindow statusContainer;
		CContainedWindow treeContainer;
		CContainedWindow listContainer;

		StringList targets;
		string downloadDirNick;
		string downloadDirIP;
		vector<string> contextMenuHubUrl;

		deque<string> history;
		size_t historyIndex;

		CTreeViewCtrl ctrlTree;
		TypedListViewCtrl<ItemInfo, IDC_FILES> ctrlList;
		CustomDrawHelpers::CustomDrawState customDrawState;
		HTHEME hTheme;
		CStatusBarCtrl ctrlStatus;
		HTREEITEM treeRoot;
		HTREEITEM selectedDir;

		CButton ctrlFind, ctrlFindNext, ctrlFindPrev;
		CButton ctrlListDiff;
		CButton ctrlMatchQueue;

		COLORREF colorShared, colorSharedLighter;
		COLORREF colorDownloaded, colorDownloadedLighter;
		COLORREF colorCanceled, colorCanceledLighter;
		COLORREF colorFound, colorFoundLighter;
		COLORREF colorContrastText;
		COLORREF colorInQueue;

		uint64_t loadStartTime;
		string fileName;
		int64_t speed;      /**< Speed at which this file list was downloaded */

		bool dclstFlag;
		bool searchResultsFlag;
		bool filteredListFlag;
		bool updating;
		bool loading;
		bool refreshing;
		bool listItemChanged;

		bool offline;
		tstring lastHubName;
		int setWindowTitleTick;

		int statusSizes[STATUS_LAST];

		std::unique_ptr<DirectoryListing> dl;
		std::atomic_bool abortFlag;
		DirectoryListing::SearchContext search[SEARCH_LAST];
		vector<const DirectoryListing::Directory*> pathCache;
		DirectoryListing::TTHToFileMap dupFiles;
		bool showingDupFiles;

		StringMap ucLineParams;

		struct UserFrame
		{
			UserPtr user;
			bool isBrowsing;
			DirectoryListingFrame* frame;
		};

		typedef std::list<UserFrame> UserList;

		static UserList userList;
		static CriticalSection lockUserList;

		static const int columnId[];

		typedef std::map< HWND, DirectoryListingFrame* > FrameMap;
		static FrameMap activeFrames;

		void on(SettingsManagerListener::Repaint) override;
		void redraw()
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
};

class ThreadedDirectoryListing : public Thread, private DirectoryListing::ProgressNotif
{
	public:
		enum
		{
			MODE_LOAD_FILE,
			MODE_SUBTRACT_FILE,
			MODE_LOAD_PARTIAL_LIST
		};

		ThreadedDirectoryListing(DirectoryListingFrame* pWindow, int mode) : window(pWindow), mode(mode) {}
		void setFile(const string &file) { this->filePath = file; }
		void setText(const string &text) { this->text = text; }
		void setDir(const tstring &dir) { this->directory = dir; }

	protected:
		DirectoryListingFrame* window;
		string filePath;
		string text;
		tstring directory;
		int mode;

	private:
		virtual int run() override;
		virtual void notify(int progress) override;
};

#endif // !defined(DIRECTORY_LISTING_FRM_H)
