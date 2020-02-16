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
	private TimerHelper
// BUG-MENU , public UserInfoBaseHandler < DirectoryListingFrame, UserInfoGuiTraits::NO_FILE_LIST | UserInfoGuiTraits::NO_COPY >
	, public InternetSearchBaseHandler<DirectoryListingFrame>
	, public PreviewBaseHandler<DirectoryListingFrame>
{
		static const int DEFAULT_PRIO = QueueItem::HIGHEST + 1;
	
	public:
		static void openWindow(const tstring& aFile, const tstring& aDir, const HintedUser& aUser, int64_t aSpeed, bool isDCLST = false);
		static void openWindow(const HintedUser& aUser, const string& txt, int64_t aSpeed);
		static void closeAll();
#ifdef TEST_PARTIAL_FILE_LIST
		static void runTest();
#endif
		
		typedef MDITabChildWindowImpl<DirectoryListingFrame> baseClass;
		typedef UCHandler<DirectoryListingFrame> ucBase;
		typedef InternetSearchBaseHandler<DirectoryListingFrame> isBase;
		// BUG-MENU  typedef UserInfoBaseHandler < DirectoryListingFrame, UserInfoGuiTraits::NO_FILE_LIST | UserInfoGuiTraits::NO_COPY > uiBase;
		typedef PreviewBaseHandler<DirectoryListingFrame> prevBase;
		
		enum
		{
			COLUMN_FILENAME,
			COLUMN_TYPE,
			COLUMN_EXACTSIZE,
			COLUMN_SIZE,
			COLUMN_TTH,
			COLUMN_PATH,
			COLUMN_HIT,
			COLUMN_TS,
			COLUMN_FLY_SERVER_RATING, // TODO: Remove
			COLUMN_BITRATE,
			COLUMN_MEDIA_XY,
			COLUMN_MEDIA_VIDEO,
			COLUMN_MEDIA_AUDIO,
			COLUMN_DURATION,
			
			COLUMN_LAST
		};
		
		enum
		{
			FINISHED,
			ABORTED,
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
		FileImage::TypeDirectoryImages GetTypeDirectory(const DirectoryListing::Directory* dir) const;
		
		DirectoryListingFrame(const HintedUser& aUser, DirectoryListing *dl);
		~DirectoryListingFrame();
		
		DECLARE_FRAME_WND_CLASS(_T("DirectoryListingFrame"), IDR_FILE_LIST)
		
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
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
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
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_DIR_LIST, onCloseAll)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_GENERATE_DCLST, onGenerateDCLST)
		COMMAND_ID_HANDLER(IDC_GENERATE_DCLST_FILE, onGenerateDCLST)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET , IDC_DOWNLOAD_TARGET + targets.size() + LastDir::get().size(), onDownloadTarget)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET_DIR, IDC_DOWNLOAD_TARGET_DIR + LastDir::get().size(), onDownloadTargetDir)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_WITH_PRIO, IDC_DOWNLOAD_WITH_PRIO + DEFAULT_PRIO, onDownloadWithPrio)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOADDIR_WITH_PRIO, IDC_DOWNLOADDIR_WITH_PRIO + DEFAULT_PRIO, onDownloadDirWithPrio)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_FAVORITE_DIRS, IDC_DOWNLOAD_FAVORITE_DIRS + FavoriteManager::getFavoriteDirsCount(), onDownloadFavoriteDirs)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS, IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS + FavoriteManager::getFavoriteDirsCount(), onDownloadWholeFavoriteDirs)
		CHAIN_COMMANDS(isBase)
		CHAIN_COMMANDS(prevBase)
		// BUG-MENU CHAIN_COMMANDS(uiBase)
		CHAIN_COMMANDS(ucBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(CSplitterImpl<DirectoryListingFrame>)
		ALT_MSG_MAP(STATUS_MESSAGE_MAP)
		COMMAND_ID_HANDLER(IDC_FIND, onFind)
		COMMAND_ID_HANDLER(IDC_NEXT, onNext)
		COMMAND_ID_HANDLER(IDC_PREV, onPrev)
		COMMAND_ID_HANDLER(IDC_MATCH_QUEUE, onMatchQueue)
		COMMAND_ID_HANDLER(IDC_FILELIST_DIFF, onListDiff)
		ALT_MSG_MAP(CONTROL_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_XBUTTONUP, onXButtonUp)
		END_MSG_MAP()
		
		LRESULT onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // !SMT!-UI
		LRESULT onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+] NightOrion
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadCustom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDirWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDirTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDirCustom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onSearchFileInInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			const auto ii = ctrlList.getSelectedItem();
			if (ii && ii->type == ItemInfo::FILE)
			{
				searchFileInInternet(wID, ii->file->getName());
			}
			return 0;
		}
		LRESULT onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAddToFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMarkAsDownloaded(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGoToDirectory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTarget(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTargetDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClickFiles(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSelChangedDirectories(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onXButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onDownloadFavoriteDirs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWholeFavoriteDirs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onCustomDrawList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !fulDC!
		LRESULT onCustomDrawTree(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !fulDC!
		LRESULT onGenerateDCLST(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		// FIXME: tstring -> string
		void downloadList(const tstring& aTarget, bool view = false,  QueueItem::Priority prio = QueueItem::DEFAULT);
		void downloadList(bool view = false,  QueueItem::Priority prio = QueueItem::DEFAULT)
		{
			downloadList(Util::emptyStringT, view, prio);
		}
		void downloadList(const FavoriteManager::FavDirList& spl, int newId)
		{
			dcassert(newId < (int)spl.size());
			downloadList(Text::toT(spl[newId].dir));
		}
		
		void refreshTree(DirectoryListing::Directory* tree, HTREEITEM treeItem);
		void UpdateLayout(BOOL bResizeBars = TRUE);
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

		LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 1;
		}
		
		LRESULT onFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPrev(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onMatchQueue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
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
		
		DirectoryListingFrame(const DirectoryListingFrame &) = delete;
		DirectoryListingFrame& operator= (const DirectoryListingFrame &) = delete;
	
	private:
		friend class ThreadedDirectoryListing;

		static void openWindow(DirectoryListing *dl, const HintedUser& aUser, int64_t speed, bool searchResults);
		
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
		void updateSearchButtons();
		void updateTree(DirectoryListing::Directory* tree, HTREEITEM treeItem);
		void appendTargetMenu(OMenu& menu, int idc);
		void appendCustomTargetItems(OMenu& menu, int idc);
		tstring getRootItemText() const;
		void updateRootItemText();
		
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
					const DirectoryListing::File* file;
					const DirectoryListing::Directory* dir;
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
				
				static int compareItems(const ItemInfo* a, const ItemInfo* b, int col)
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					if (a->type == DIRECTORY)
					{
						if (b->type == DIRECTORY)
						{
							switch (col)
							{
								case COLUMN_FILENAME:
									return Util::defaultSort(a->columns[COLUMN_FILENAME], b->columns[COLUMN_FILENAME], true);
								case COLUMN_EXACTSIZE:
									return compare(a->dir->getTotalSize(), b->dir->getTotalSize());
								case COLUMN_SIZE:
									return compare(a->dir->getTotalSize(), b->dir->getTotalSize());
								case COLUMN_HIT:
									return compare(a->dir->getTotalHits(), b->dir->getTotalHits());
								case COLUMN_TS:
									return compare(a->dir->getMaxTS(), b->dir->getMaxTS());
								default:
									return Util::defaultSort(a->columns[col], b->columns[col], false);
							}
						}
						else
						{
							return -1;
						}
					}
					else if (b->type == DIRECTORY)
					{
						return 1;
					}
					else
					{
						switch (col)
						{
							case COLUMN_FILENAME:
								return Util::defaultSort(a->columns[COLUMN_FILENAME], b->columns[COLUMN_FILENAME], true);
							case COLUMN_TYPE:
							{
								int result = Util::defaultSort(a->columns[COLUMN_TYPE], b->columns[COLUMN_TYPE], true);
								if (result) return result;
								return Util::defaultSort(a->columns[COLUMN_FILENAME], b->columns[COLUMN_FILENAME], true);
							}
							case COLUMN_EXACTSIZE:
								return compare(a->file->getSize(), b->file->getSize());
							case COLUMN_SIZE:
								return compare(a->file->getSize(), b->file->getSize());
							case COLUMN_HIT:
								return compare(a->file->getHit(), b->file->getHit());
							case COLUMN_TS:
								return compare(a->file->getTS(), b->file->getTS());
							case COLUMN_BITRATE:
							{
								const DirectoryListing::MediaInfo *aMedia = a->file->getMedia();
								const DirectoryListing::MediaInfo *bMedia = b->file->getMedia();
								if (aMedia && bMedia)
									return compare(aMedia->bitrate, bMedia->bitrate);
								if (aMedia) return 1;
								if (bMedia) return -1;
								return 0;
							}
							case COLUMN_MEDIA_XY:
							{
								const DirectoryListing::MediaInfo *aMedia = a->file->getMedia();
								const DirectoryListing::MediaInfo *bMedia = b->file->getMedia();
								if (aMedia && bMedia)
									return compare(aMedia->getSize(), bMedia->getSize());
								if (aMedia) return 1;
								if (bMedia) return -1;
								return 0;
							}
							default:
								return Util::defaultSort(a->columns[col], b->columns[col], false);
						}
					}
				}

				void updateIconIndex();
				int getImageIndex() const { return iconIndex; }
				static uint8_t getStateImageIndex() { return 0; }
				
			private:
				tstring columns[COLUMN_LAST];
				int iconIndex;
		};
		OMenu targetMenu;
		OMenu targetDirMenu;
		OMenu directoryMenu;
		OMenu priorityMenu;
		OMenu priorityDirMenu;
		OMenu copyMenu;
		
		CContainedWindow statusContainer;
		CContainedWindow treeContainer;
		CContainedWindow listContainer;
		
		StringList targets;
		string downloadDirNick;
		string downloadDirIP;
		
		deque<string> history;
		size_t historyIndex;
		
		CTreeViewCtrl ctrlTree;
		TypedListViewCtrl<ItemInfo, IDC_FILES> ctrlList;
		bool ctrlListFocused;
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
		COLORREF colorInQueue;

		static HIconWrapper frameIcon;

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
		
		int statusSizes[STATUS_LAST];

		std::unique_ptr<DirectoryListing> dl;
		std::atomic_bool abortFlag;
		DirectoryListing::SearchContext search;

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
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];
		
		typedef std::map< HWND, DirectoryListingFrame* > FrameMap;
		static FrameMap activeFrames;
		
		void on(SettingsManagerListener::Repaint) override;
};

class ThreadedDirectoryListing : public Thread, private DirectoryListing::ProgressNotif
{
	public:
		enum
		{
			MODE_LOAD_FILE,
			MODE_DIFF_FILE,
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
