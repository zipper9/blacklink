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

#include "FlatTabCtrl.h"
#include "TypedTreeListViewCtrl.h"
#include "UCHandler.h"
#include "UserInfoBaseHandler.h"
#include "TimerHelper.h"
#include "CustomDrawHelpers.h"
#include "SearchHistory.h"

#include "../client/UserInfoBase.h"
#include "../client/SearchManager.h"
#include "../client/ClientManagerListener.h"
#include "../client/QueueManager.h"
#include "../client/SearchResult.h"
#include "../client/ShareManager.h"

#ifdef OSVER_WIN_XP
#include "ImageButton.h"
#endif

#define FLYLINKDC_USE_TREE_SEARCH

#define SEARCH_MESSAGE_MAP 6
#define SHOWUI_MESSAGE_MAP 7
#define SEARCH_FILTER_MESSAGE_MAP 11

class HIconWrapper;
class SearchFrame : public MDITabChildWindowImpl<SearchFrame>,
	private SearchManagerListener,
	private ClientManagerListener,
	private FavoriteManagerListener,
	public UCHandler<SearchFrame>, public UserInfoBaseHandler<SearchFrame, UserInfoGuiTraits::NO_COPY>,
	private SettingsManagerListener,
	private TimerHelper,
	public CMessageFilter
{
		class SearchInfo;
		typedef TypedTreeListViewCtrl<SearchInfo, IDC_RESULTS, TTHValue> SearchInfoList;

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
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_SEARCH, onSearch)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_PATH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy)
		COMMAND_ID_HANDLER(IDC_FREESLOTS, onFreeSlots)
		COMMAND_ID_HANDLER(IDC_OPTION_CHECKBOX, onChangeOption)
		COMMAND_ID_HANDLER(IDC_USE_TREE, onToggleTree)
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
#ifdef FLYLINKDC_USE_TREE_SEARCH
		NOTIFY_HANDLER(IDC_TRANSFER_TREE, TVN_SELCHANGED, onSelChangedTree);
#endif
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TO_FAV, IDC_DOWNLOAD_TO_FAV + 499, onDownload)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOADDIR_TO_FAV, IDC_DOWNLOADDIR_TO_FAV + 499, onDownloadWhole)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET, IDC_DOWNLOAD_TARGET + 499, onDownload)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED, IDC_PRIORITY_HIGHEST, onDownloadWithPrio)
		
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uicBase)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SEARCH_MESSAGE_MAP)
		ALT_MSG_MAP(SHOWUI_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowUI)
		ALT_MSG_MAP(SEARCH_FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)
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
		LRESULT onDoubleClickResults(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onFilterChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWhole(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#ifdef FLYLINKDC_USE_TREE_SEARCH
		LRESULT onSelChangedTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
#endif
		
		void UpdateLayout(BOOL resizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
		void onSizeMode();
		void removeSelected();
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
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
		
		LRESULT onFreeSlots(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			onlyFree = ctrlSlots.GetCheck() == BST_CHECKED;
			return 0;
		}
		
		LRESULT onChangeOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onToggleTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
		
		LRESULT onShowUI(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			showUI = (wParam == BST_CHECKED);
			UpdateLayout(FALSE);
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

		void getSelectedUsers(vector<UserPtr>& v) const;

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
				typedef SearchInfo* Ptr;
				typedef vector<Ptr> Array;
				
				SearchInfo(const SearchResult &sr) : sr(sr), collapsed(true), parent(nullptr),
					colMask(0), hits(0), iconIndex(-1)
				{
#ifdef BL_FEATURE_IP_DATABASE
					ipUpdated = false;
#endif
				}
				const UserPtr& getUser() const override { return sr.getUser(); }
				bool collapsed;
				SearchInfo* parent;
				size_t hits;
				int iconIndex;

				void getList();
				void browseList();

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
				
				static int compareItems(const SearchInfo* a, const SearchInfo* b, int col);
				
				int getImageIndex() const;
				void calcImageIndex();
				static int getStateImageIndex() { return 0; }

				SearchInfo* createParent()
				{
					return this;
				}
				SearchResult sr;
				mutable tstring columns[COLUMN_LAST];
				mutable uint32_t colMask;
#ifdef BL_FEATURE_IP_DATABASE
				bool ipUpdated;
#endif
				const TTHValue& getGroupCond() const
				{
					return sr.getTTH();
				}
		};

		struct HubInfo
		{
			HubInfo(const string& url, const tstring& name, bool isOp) : url(url), name(name), isOp(isOp) {}
				
			const tstring& getText(int col) const;
			static int compareItems(const HubInfo* a, const HubInfo* b, int col);
			static const int getImageIndex() { return 0; }
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

		tstring initialString;
		int64_t initialSize;
		SizeModes initialMode;
		int initialType;
		
		CStatusBarCtrl ctrlStatus;
		CEdit ctrlSearch;
		CComboBox ctrlSearchBox;
		void initSearchHistoryBox();
		
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

		CFlyToolTipCtrl tooltip;
		
		CContainedWindow searchContainer;
		CContainedWindow searchBoxContainer;
		CContainedWindow sizeContainer;
		CContainedWindow modeContainer;
		CContainedWindow sizeModeContainer;
		CContainedWindow fileTypeContainer;
#ifdef BL_FEATURE_IP_DATABASE
		CButton ctrlStoreIP;
		bool storeIP;
#endif
		CContainedWindow showUIContainer;
		
		CContainedWindow resultsContainer;
		CContainedWindow hubsContainer;
		CContainedWindow ctrlFilterContainer;
		CContainedWindow ctrlFilterSelContainer;
		tstring filter;
		
		CStatic searchLabel, sizeLabel, optionLabel, typeLabel, hubsLabel, srLabel;
		CButton ctrlSlots;
		bool onlyFree;

		CButton ctrlShowUI;
		bool showUI;
		
		CButton ctrlCollapsed;
		bool expandSR;
		
		CButton ctrlStoreSettings;
		bool storeSettings;
#ifdef FLYLINKDC_USE_TREE_SEARCH
		CButton ctrlUseGroupTreeSettings;
		bool useTree;
#endif
		bool autoSwitchToTTH;
		bool shouldSort;
		CImageList images;
		SearchInfoList ctrlResults;
		CustomDrawHelpers::CustomDrawState customDrawState;
		HTHEME hTheme;
		TypedListViewCtrl<HubInfo, IDC_HUB> ctrlHubs;
		
#ifdef FLYLINKDC_USE_TREE_SEARCH
		CTreeViewCtrl ctrlSearchFilterTree;
		HTREEITEM treeItemRoot;
		HTREEITEM treeItemCurrent;
		HTREEITEM treeItemOld;
		std::unordered_map<string, HTREEITEM> extToTreeItem;

		static const int NUMBER_OF_TYPE_NODES = NUMBER_OF_FILE_TYPES;

		struct ExtGroup
		{
			string ext;
			vector<SearchInfo*> data;
		};
		
		std::unordered_map<HTREEITEM, ExtGroup> groupedResults;
		HTREEITEM typeNodes[NUMBER_OF_TYPE_NODES];
		bool treeExpanded;
		bool itemMatchesSelType(const SearchInfo* si) const;
		HTREEITEM getInsertAfter(int type) const;
#else
		vector<SearchInfo*> results;
#endif
		std::unordered_set<SearchInfo*> everything;
		std::unordered_set<UserPtr> allUsers;
		FastCriticalSection csEverything;

		void clearFound();
		
		OMenu targetMenu;
		OMenu targetDirMenu;
		OMenu priorityMenu;
		OMenu copyMenu;
		OMenu tabMenu;

		StringList search;
		StringList targets;
		StringList wholeTargets;
		CEdit ctrlFilter;
		CComboBox ctrlFilterSel;
		
		bool isHash;
		bool running;
		bool isExactSize;
		bool waitingResults;
		bool needUpdateResultCount;
		bool updateList;
		bool hasWaitTime;
		bool startingSearch;
		
		SearchParamToken searchParam;
		vector<SearchClientItem> searchClients;
		bool useDHT;
		int64_t exactSize;
		size_t resultsCount;
		uint64_t searchEndTime;
		uint64_t searchStartTime;
		tstring searchTarget;
		tstring statusLine;

		FastCriticalSection csSearch;

	public:
		static SearchHistory lastSearches;

	private:
		static HIconWrapper iconUdpOk;
		static HIconWrapper iconUdpFail;
		static HIconWrapper iconUdpWait;
		
		CStatic ctrlUDPMode;
		CStatic ctrlUDPTestResult;
		tstring portStatusText;
		int portStatus;
		string currentReflectedAddress;
		
		size_t droppedResults;
		
		StringMap ucLineParams;
		
		static const int columnId[];
		
		typedef std::map<HWND, SearchFrame*> FrameMap;
		typedef pair<HWND, SearchFrame*> FramePair;
		
		static FrameMap g_search_frames;
		
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

		COLORREF colorShared;
		COLORREF colorDownloaded;
		COLORREF colorCanceled;
		COLORREF colorInQueue;
		class HashDatabaseConnection* hashDb;

		void getFileItemColor(int flags, COLORREF& fg, COLORREF& bg) const;
		void showPortStatus();

		void onEnter();
		int makeTargetMenu(const SearchInfo* si, OMenu& menu, int idc, ResourceManager::Strings title);
		static tstring getTargetDirectory(const SearchInfo* si, const tstring& downloadDir);

		void on(SearchManagerListener::SR, const SearchResult& sr) noexcept override;
		
		//void on(SearchManagerListener::Searching, SearchQueueItem* aSearch) noexcept override;
		
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

		void on(SettingsManagerListener::Repaint) override;
		
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
};

#endif // !defined(SEARCH_FRM_H)
