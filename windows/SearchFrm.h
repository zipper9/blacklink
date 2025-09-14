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

#ifndef SEARCH_FRM_H
#define SEARCH_FRM_H

#include "MDITabChildWindow.h"
#include "TypedTreeListViewCtrl.h"
#include "UCHandler.h"
#include "UserInfoBaseHandler.h"
#include "TimerHelper.h"
#include "CustomDrawHelpers.h"
#include "SearchHistory.h"
#include "FileStatusColors.h"
#include "DialogLayout.h"
#include "StatusBarCtrl.h"
#include "StatusLabelCtrl.h"
#include "SearchBoxCtrl.h"
#include "ControlList.h"

#include "../client/ClientManagerListener.h"
#include "../client/FavoriteManagerListener.h"
#include "../client/UserInfoBase.h"
#include "../client/SearchManager.h"
#include "../client/QueueManager.h"
#include "../client/SearchResult.h"
#include "../client/ShareManager.h"
#include "../client/FavoriteUser.h"

#ifdef OSVER_WIN_XP
#include "ImageButton.h"
#endif

#define CHECKBOX_MESSAGE_MAP 7

class SearchFrame : public MDITabChildWindowImpl<SearchFrame>,
	private ClientManagerListener,
	private FavoriteManagerListener,
	public UCHandler<SearchFrame>, public UserInfoBaseHandler<SearchFrame, UserInfoGuiTraits::NO_COPY>,
	private SettingsManagerListener,
	private TimerHelper,
	public CMessageFilter,
	private StatusBarCtrl::Callback
{
		class SearchInfo;
		typedef TypedTreeListViewCtrl<SearchInfo, TTHValue, false> SearchInfoList;

	public:
		static void openWindow(const tstring& str = Util::emptyStringT, LONGLONG size = 0, SizeModes mode = SIZE_ATLEAST, int type = FILE_TYPE_ANY);
		static void closeAll();
		
		static CFrameWndClassInfo& GetWndClassInfo();
		
		typedef MDITabChildWindowImpl<SearchFrame> baseClass;
		typedef UCHandler<SearchFrame> ucBase;
		typedef UserInfoBaseHandler<SearchFrame, UserInfoGuiTraits::NO_COPY> uicBase;

		BEGIN_MSG_MAP(SearchFrame)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETDISPINFO, ctrlResults.onGetDispInfo)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_COLUMNCLICK, ctrlResults.onColumnClick)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETINFOTIP, ctrlResults.onInfoTip)
		NOTIFY_HANDLER(IDC_HUB, LVN_GETDISPINFO, ctrlHubs.onGetDispInfo)
		NOTIFY_HANDLER(IDC_RESULTS, NM_DBLCLK, onDoubleClickResults)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_HUB, LVN_ITEMCHANGED, onItemChangedHub)
		NOTIFY_HANDLER(IDC_RESULTS, NM_CUSTOMDRAW, onCustomDraw)
		NOTIFY_HANDLER(IDC_OPTION_CHECKBOX, NM_CUSTOMDRAW, onCheckBoxCustomDraw)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DRAWITEM, onDrawItem)
		MESSAGE_HANDLER(WM_MEASUREITEM, onMeasure)
		MESSAGE_HANDLER(DM_GETDEFID, onGetDefID)
		MESSAGE_HANDLER(WM_NEXTDLGCTL, onNextDlgCtl)
		MESSAGE_HANDLER(WMU_RETURN, onFilterReturn)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_SEARCH, onSearch)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_PATH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy)
		COMMAND_ID_HANDLER(IDC_OPTION_CHECKBOX, onChangeOption)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetList)
		COMMAND_ID_HANDLER(IDC_BROWSELIST, onBrowseList)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchByTTH)
		COMMAND_ID_HANDLER(IDC_COPY_HUB_URL, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FULL_MAGNET_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
		COMMAND_ID_HANDLER(IDC_PURGE, onPurge)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_SEARCH_FRAME, onCloseAll)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_HANDLER(IDC_SEARCH_STRING, CBN_EDITCHANGE, onEditChange)
		COMMAND_HANDLER(IDC_SEARCH_STRING, CBN_SELCHANGE, onEditSelChange)
		COMMAND_ID_HANDLER(IDC_FILETYPES, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_SIZEMODE, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_SIZE, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_MODE, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenFileOrFolder)
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER, onOpenFileOrFolder)
		NOTIFY_HANDLER(IDC_TRANSFER_TREE, TVN_SELCHANGED, onSelChangedTree);
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TO_FAV, IDC_DOWNLOAD_TO_FAV + 499, onDownload)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOADDIR_TO_FAV, IDC_DOWNLOADDIR_TO_FAV + 499, onDownloadWhole)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET, IDC_DOWNLOAD_TARGET + 499, onDownload)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED, IDC_PRIORITY_HIGHEST, onDownloadWithPrio)
		COMMAND_RANGE_HANDLER(IDC_LOCATE_FILE_IN_QUEUE, IDC_LOCATE_FILE_IN_QUEUE + 9, onLocateInQueue)
		COMMAND_HANDLER(IDC_FILTER_SEL, CBN_SELCHANGE, onFilterSelChange)
		COMMAND_CODE_HANDLER(EN_CHANGE, onFilterChanged)
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uicBase)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(CHECKBOX_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowOptions)
		END_MSG_MAP()

		SearchFrame();
		~SearchFrame();

		SearchFrame(const SearchFrame&) = delete;
		SearchFrame& operator= (const SearchFrame&) = delete;

		virtual BOOL PreTranslateMessage(MSG* pMsg) override;
		LRESULT onFiletypeChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onDrawItem(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onMeasure(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCtlColor(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCheckBoxCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onDoubleClickResults(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onFilterSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onFilterChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWhole(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLocateInQueue(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenFileOrFolder(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onShowOptions(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onNextDlgCtl(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onFilterReturn(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSelChangedTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

		void UpdateLayout(BOOL resizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
		void onSizeMode();
		void removeSelected();
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			ctrlResults.forEachSelected(&SearchInfo::view);
			return 0;
		}
#endif
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			removeSelected();
			return 0;
		}

		LRESULT onChangeOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/);

		LRESULT onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			onEnter();
			return 0;
		}

		LRESULT onGetDefID(UINT, WPARAM, LPARAM, BOOL&)
		{
			return MAKELONG(IDC_SEARCH, DC_HASDEFID);
		}

		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
			
			if (kd->wVKey == VK_DELETE)
			{
				removeSelected();
			}
			return 0;
		}

		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (::IsWindow(ctrlSearch))
				ctrlSearch.SetFocus();
			return 0;
		}

		void setInitial(const tstring& str, LONGLONG size, SizeModes mode, int type)
		{
			initialString = str;
			initialSize = size;
			initialMode = mode;
			initialType = type;
			running = true;
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

		void addSearchResult(const SearchResult& sr);
		void getSelectedUsers(vector<UserPtr>& v) const;

		static SearchFrame* getFramePtr(uint64_t id);
		static void releaseFramePtr(SearchFrame* frame);
		static void broadcastSearchResult(const SearchResult& sr);

	private:
		bool getDownloadDirectory(WORD wID, tstring& dir) const;

	private:
		enum
		{
			COLUMN_FILENAME        = 0,
			COLUMN_LOCAL_PATH      = 1,
			COLUMN_HITS            = 2,
			COLUMN_NICK            = 3,
			COLUMN_TYPE            = 4,
			COLUMN_SIZE            = 5,
			COLUMN_PATH            = 6,
			COLUMN_SLOTS           = 13,
			COLUMN_HUB             = 14,
			COLUMN_EXACT_SIZE      = 15,
			COLUMN_LOCATION        = 16,
			COLUMN_IP              = 17,
			COLUMN_TTH             = 18,
			COLUMN_P2P_GUARD       = 19,
			COLUMN_LAST
		};

		enum FilterModes
		{
			NONE,
			EQUAL,
			GREATER_EQUAL,
			LESS_EQUAL,
			GREATER,
			LESS,
			NOT_EQUAL
		};

		class SearchInfo : public UserInfoBase
		{
			public:
				SearchInfo(const SearchResult &sr) : sr(sr), stateFlags(0),
					colMask(0), hits(0), iconIndex(-1)
				{
#ifdef BL_FEATURE_IP_DATABASE
					ipUpdated = false;
#endif
				}

				const UserPtr& getUser() const override { return sr.getUser(); }
				const string& getHubHint() const override { return sr.getHubUrl(); }

				int iconIndex;

				int hits;
				uint8_t stateFlags;
				SearchInfoList::GroupInfoPtr groupInfo;

				void getList();
				void browseList();
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
				void view();
#endif

				struct Download
				{
					Download(const tstring& target, SearchFrame* sf, QueueItem::Priority prio, QueueItem::MaskType mask = 0) : tgt(target), sf(sf), prio(prio), mask(mask) { }
					void operator()(SearchInfo* si);
					const tstring tgt;
					SearchFrame* sf;
					QueueItem::Priority prio;
					QueueItem::MaskType mask;
				};
				struct DownloadWhole
				{
					DownloadWhole(const tstring& target) : tgt(target) { }
					void operator()(const SearchInfo* si);
					const tstring tgt;
				};
				struct CheckTTH
				{
					CheckTTH() : op(true), firstHubs(true), hasTTH(false), firstTTH(true) { }
					void operator()(const SearchInfo* si);
					bool firstHubs;
					StringList hubs;
					bool op;
					bool hasTTH;
					bool firstTTH;
					tstring tth;
				};
				
				const tstring& getText(uint8_t col) const;

				static int compareItems(const SearchInfo* a, const SearchInfo* b, int col, int flags);
				static int getCompareFlags() { return 0; }

				int getImageIndex() const;
				void calcImageIndex();
				static int getStateImageIndex() { return 0; }
				uint8_t getStateFlags() const { return stateFlags; }
				void setStateFlags(uint8_t flags) { stateFlags = flags; }

				SearchInfo* createParent() { return nullptr; }
				SearchInfo* getParent() { return groupInfo ? groupInfo->parent : nullptr; }
				bool hasParent() const { return groupInfo && groupInfo->parent && groupInfo->parent != this; }

				SearchResult sr;
				mutable tstring columns[COLUMN_LAST];
				mutable uint32_t colMask;
#ifdef BL_FEATURE_IP_DATABASE
				bool ipUpdated;
#endif
				const TTHValue& getGroupCond() const { return sr.getTTH(); }
		};

		struct HubInfo
		{
			HubInfo(const string& url, const tstring& name, bool isOp) : url(url), name(name), isOp(isOp) {}

			const tstring& getText(int col) const;
			static int compareItems(const HubInfo* a, const HubInfo* b, int col, int flags);
			static int getCompareFlags() { return 0; }
			static int getImageIndex() { return 0; }
			static uint8_t getStateImageIndex() { return 0; }
			int getType() const;

			const string url;
			const tstring name;
			tstring waitTime;
			const bool isOp;
		};
		
		// WM_SPEAKER
		enum Speakers
		{
			ADD_RESULT,
			HUB_ADDED,
			HUB_CHANGED,
			HUB_REMOVED
		};

		enum
		{
			STATUS_CHECKBOX,
			STATUS_PROGRESS,
			STATUS_TIME,
			STATUS_COUNT,
			STATUS_DROPPED,
			STATUS_LAST
		};

		const uint64_t id;
		tstring initialString;
		int64_t initialSize;
		SizeModes initialMode;
		int initialType;

		StatusBarCtrl ctrlStatus;
		CEdit ctrlSearch;
		CComboBox ctrlSearchBox;
		void initSearchHistoryBox();

		DialogLayout::Item layout[18];
		CEdit ctrlSize;
		CComboBox ctrlMode;
		CComboBox ctrlSizeMode;
		CComboBox ctrlFiletype;
		CImageList searchTypesImageList;

		CButton ctrlPurge;
		CButton ctrlDoSearch;
#ifdef OSVER_WIN_XP
		ImageButton ctrlPurgeSubclass;
		ImageButton ctrlDoSearchSubclass;
#endif

		WinUtil::ControlList controls;
		CToolTipCtrl tooltip;

		CContainedWindow showOptionsContainer;
		tstring filter;

		COLORREF colorBackground;
		COLORREF colorText;
#ifdef BL_FEATURE_IP_DATABASE
		bool storeIP;
#endif
		bool onlyFree;
		bool expandSR;
		bool storeSettings;
		bool useTree;

		CButton ctrlShowOptions;
		bool showOptions;

		bool autoSwitchToTTH;
		bool shouldSort;
		bool boldSearch;
		CImageList images;
		SearchInfoList ctrlResults;
		CustomDrawHelpers::CustomDrawState customDrawState;
		CustomDrawHelpers::CustomDrawCheckBoxState customDrawCheckBox;
		HTHEME hTheme;
		TypedListViewCtrl<HubInfo> ctrlHubs;

		CTreeViewCtrl ctrlSearchFilterTree;
		HTREEITEM treeItemRoot;
		HTREEITEM treeItemCurrent;
		HTREEITEM treeItemOld;
		boost::unordered_map<string, HTREEITEM> extToTreeItem;

		static const int NUMBER_OF_TYPE_NODES = NUMBER_OF_FILE_TYPES;

		struct ExtGroup
		{
			#if 0
			string ext;
			#endif
			vector<SearchInfo*> data;
		};

		boost::unordered_map<HTREEITEM, ExtGroup> groupedResults;
		HTREEITEM typeNodes[NUMBER_OF_TYPE_NODES];
		bool treeExpanded;
		HTREEITEM getInsertAfter(int type) const;

		boost::unordered_set<SearchInfo*> everything;
		boost::unordered_set<UserPtr> allUsers;
		FastCriticalSection csEverything;

		void clearFound();

		OMenu targetMenu;
		OMenu targetDirMenu;
		OMenu priorityMenu;
		OMenu copyMenu;
		CMenu tabMenu;

		StringList targets;
		SearchBoxCtrl ctrlFilter;
		CComboBox ctrlFilterSel;

		bool running;
		bool waitingResults;
		bool needUpdateResultCount;
		bool updateList;
		bool hasWaitTime;
		bool startingSearch;

		SearchParam searchParam;
		vector<SearchClientItem> searchClients;
		bool useDHT;
		size_t resultsCount;
		uint64_t searchEndTime;
		uint64_t searchStartTime;
		tstring searchTarget;
		tstring statusLine;

		FastCriticalSection csSearch;

	public:
		static SearchHistory lastSearches;

	private:
		StatusLabelCtrl ctrlPortStatus;
		int portStatus;
		string currentReflectedAddress;

		size_t droppedResults;

		StringMap ucLineParams;

		static const int columnId[];

		typedef std::map<uint64_t, SearchFrame*> FrameMap;

		static FrameMap activeFrames;
		static CriticalSection framesLock;

		struct DownloadTarget
		{
			enum DefinedTypes
			{
				PATH_DEFAULT,
				PATH_FAVORITE,
				PATH_SRC,
				PATH_LAST,
				PATH_BROWSE
			};
			
			DownloadTarget(const tstring& path, DefinedTypes type): path(path), type(type) {}
			DownloadTarget(): type(PATH_DEFAULT) {}
			
			tstring path;
			DefinedTypes type;
		};
		std::map<int, DownloadTarget> dlTargets;

		FileStatusColors colors;
		class HashDatabaseConnection* hashDb;

		void getFileItemColor(int flags, COLORREF& fg, COLORREF& bg) const;
		void initCheckBoxCustomDraw();
		void showPortStatus();
		void toggleTree();

		void onEnter();
		int makeTargetMenu(const SearchInfo* si, OMenu& menu, int idc, ResourceManager::Strings title);
		static tstring getTargetDirectory(const SearchInfo* si, const tstring& downloadDir);
		static bool isValidFile(const SearchResult& sr);

		// ClientManagerListener
		void on(ClientConnected, const Client* c) noexcept override
		{
			speak(HUB_ADDED, c);
		}
		void on(ClientUpdated, const Client* c) noexcept override
		{
			speak(HUB_CHANGED, c);
		}
		void on(ClientDisconnected, const Client* c) noexcept override
		{
			if (!ClientManager::isShutdown())
			{
				speak(HUB_REMOVED, c);
			}
		}
		void on(UserConnected, const UserPtr& user) noexcept override { onUserUpdated(user); }
		void on(UserDisconnected, const UserPtr& user) noexcept override { onUserUpdated(user); }

		// FavoriteManagerListener
		void on(UserAdded, const FavoriteUser& user) noexcept override { onUserUpdated(user.user); }
		void on(UserRemoved, const FavoriteUser& user) noexcept override { onUserUpdated(user.user); }
		void on(UserStatusChanged, const UserPtr& user) noexcept override { onUserUpdated(user); }

		void on(SettingsManagerListener::ApplySettings) override;

		void createMenus();
		void destroyMenus();
		void initHubs();
		void updateWaitingTime();
		void onHubAdded(HubInfo* info);
		void onHubChanged(HubInfo* info);
		void onHubRemoved(HubInfo* info);
		void onUserUpdated(const UserPtr& user) noexcept;
		bool matchFilter(const SearchInfo* si, int sel, bool doSizeCompare = false, FilterModes mode = NONE, int64_t size = 0);
		bool parseFilter(FilterModes& mode, int64_t& size);
		void removeSearchInfo(SearchInfo* si);
		void insertStoredResults(HTREEITEM treeItem, int filter, bool doSizeCompare, FilterModes mode, int64_t size);
		void updateSearchList(SearchInfo* si = nullptr);
		void updateResultCount();
		void updateStatusLine(uint64_t tick);

		void addSearchResult(SearchInfo* si);
		bool isSkipSearchResult(SearchInfo* si);

		LRESULT onItemChangedHub(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

		void speak(Speakers s, const Client* c);

		// StatusBarCtrl::Callback
		void statusPaneClicked(int pane, int button, POINT pt) override {}
		void drawStatusPane(int pane, HDC hdc, const RECT& rc) override;
		bool isStatusPaneEmpty(int pane) const override { return false; }
};

#endif // !defined(SEARCH_FRM_H)
