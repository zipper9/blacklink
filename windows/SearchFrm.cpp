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

#include "stdafx.h"

#include "Resource.h"
#include "SearchFrm.h"
#include "MainFrm.h"
#include "BarShader.h"
#include "ImageLists.h"

#include "../client/Client.h"
#include "../client/QueueManager.h"
#include "../client/ClientManager.h"
#include "../client/ShareManager.h"
#include "../client/DownloadManager.h"
#include "../client/DatabaseManager.h"
#include "../client/StringTokenizer.h"
#include "../client/FileTypes.h"
#include "../client/PortTest.h"
#include "../client/dht/DHT.h"

std::list<tstring> SearchFrame::g_lastSearches;
HIconWrapper SearchFrame::iconPurge(IDR_PURGE);
HIconWrapper SearchFrame::iconSearch(IDR_SEARCH);
HIconWrapper SearchFrame::iconUdpOk(IDR_ICON_SUCCESS_ICON);
HIconWrapper SearchFrame::iconUdpFail(IDR_ICON_FAIL_ICON);
HIconWrapper SearchFrame::iconUdpWait(IDR_ICON_WARN_ICON);

static const unsigned SEARCH_RESULTS_WAIT_TIME = 10000;

extern bool g_DisableTestPort;

const int SearchFrame::columnId[] =
{
	COLUMN_FILENAME,
	COLUMN_LOCAL_PATH,
	COLUMN_HITS,
	COLUMN_NICK,
	COLUMN_TYPE,
	COLUMN_SIZE,
	COLUMN_PATH,
	COLUMN_SLOTS,
	COLUMN_HUB,
	COLUMN_EXACT_SIZE,
	COLUMN_LOCATION,
	COLUMN_IP,
	COLUMN_TTH,
	COLUMN_P2P_GUARD,
#ifdef FLYLINKDC_USE_TORRENT
	COLUMN_TORRENT_COMMENT,
	COLUMN_TORRENT_DATE,
	COLUMN_TORRENT_URL,
	COLUMN_TORRENT_TRACKER,
	COLUMN_TORRENT_PAGE
#endif
};

static const int columnSizes[] =
{
	210, // COLUMN_FILENAME
	140, // COLUMN_LOCAL_PATH
	40,  // COLUMN_HITS
	100, // COLUMN_NICK
	64,  // COLUMN_TYPE
	85,  // COLUMN_SIZE
	100, // COLUMN_PATH
	50,  // COLUMN_SLOTS
	130, // COLUMN_HUB
	100, // COLUMN_EXACT_SIZE
	120, // COLUMN_LOCATION
	100, // COLUMN_IP
	125, // COLUMN_TTH
	140, // COLUMN_P2P_GUARD
#ifdef FLYLINKDC_USE_TORRENT
	30,
	40,
	100,
	40,
	20
#endif
};

static const ResourceManager::Strings columnNames[] = 
{
	ResourceManager::FILE,
	ResourceManager::LOCAL_PATH,
	ResourceManager::HIT_COUNT,
	ResourceManager::USER,
	ResourceManager::TYPE,
	ResourceManager::SIZE,
	ResourceManager::PATH,
	ResourceManager::SLOTS,
	ResourceManager::HUB,
	ResourceManager::EXACT_SIZE,
	ResourceManager::LOCATION_BARE,
	ResourceManager::IP,
	ResourceManager::TTH_ROOT,
	ResourceManager::P2P_GUARD,
#ifdef FLYLINKDC_USE_TORRENT
	ResourceManager::TORRENT_COMMENT,
	ResourceManager::TORRENT_DATE,
	ResourceManager::TORRENT_URL,
	ResourceManager::TORRENT_TRACKER,
	ResourceManager::TORRENT_PAGE
#endif
};

static const int hubsColumnIds[] = { 0, 1 };
static const int hubsColumnSizes[] = { 146, 80 };

static const ResourceManager::Strings hubsColumnNames[] = 
{
	ResourceManager::HUB,
	ResourceManager::TIME_TO_WAIT
};

SearchFrame::FrameMap SearchFrame::g_search_frames;

inline bool isTorrent(const SearchFrame::SearchInfo* si)
{
#ifdef FLYLINKDC_USE_TORRENT
	return si->m_is_torrent;
#else
	return false;
#endif
}

SearchFrame::SearchFrame() :
	TimerHelper(m_hWnd),
	searchBoxContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	searchContainer(WC_EDIT, this, SEARCH_MESSAGE_MAP),
	sizeContainer(WC_EDIT, this, SEARCH_MESSAGE_MAP),
	modeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	sizeModeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	fileTypeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	//m_treeContainer(WC_TREEVIEW, this, SEARH_TREE_MESSAGE_MAP),
	
	showUIContainer(WC_COMBOBOX, this, SHOWUI_MESSAGE_MAP),
	//slotsContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	//collapsedContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	//storeIPContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	storeIP(false),
#endif
	//storeSettingsContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	//purgeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	//doSearchContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	//doSearchPassiveContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
	resultsContainer(WC_LISTVIEW, this, SEARCH_MESSAGE_MAP),
	hubsContainer(WC_LISTVIEW, this, SEARCH_MESSAGE_MAP),
	ctrlFilterContainer(WC_EDIT, this, SEARCH_FILTER_MESSAGE_MAP),
	ctrlFilterSelContainer(WC_COMBOBOX, this, SEARCH_FILTER_MESSAGE_MAP),
	initialSize(0), initialMode(SIZE_ATLEAST), initialType(FILE_TYPE_ANY),
	showUI(true), onlyFree(false), isHash(false), droppedResults(0), resultsCount(0),
	expandSR(false),
	storeSettings(false), isExactSize(false), exactSize(0), /*searches(0),*/
	autoSwitchToTTH(false),
	running(false),
	searchEndTime(0),
	searchStartTime(0),
	waitingResults(false),
	needUpdateResultCount(false),
	updateList(false),
	hasWaitTime(false),
#ifdef FLYLINKDC_USE_TREE_SEARCH
	treeExpanded(false),
	treeItemRoot(nullptr),
	treeItemCurrent(nullptr),
	treeItemOld(nullptr),
	useTree(true),
#endif
	shouldSort(false),
	startingSearch(false),
#ifdef FLYLINKDC_USE_TORRENT
	disableTorrentRSS(false),
	subTreeExpanded(false),
#endif
	portStatus(PortTest::STATE_UNKNOWN),
	hTheme(nullptr),
	useDHT(false)
{
	ctrlResults.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlResults.setColumnFormat(COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlResults.setColumnFormat(COLUMN_EXACT_SIZE, LVCFMT_RIGHT);
	ctrlResults.setColumnFormat(COLUMN_TYPE, LVCFMT_RIGHT);
	ctrlResults.setColumnFormat(COLUMN_SLOTS, LVCFMT_RIGHT);
	ctrlHubs.setColumns(2, hubsColumnIds, hubsColumnNames, hubsColumnSizes);
	ctrlHubs.enableHeaderMenu = false;
	ctrlHubs.setSortColumn(0);
}

SearchFrame::~SearchFrame()
{
	dcassert(everything.empty());
	dcassert(closed);
	images.Destroy();
	searchTypesImageList.Destroy();
}

inline static bool isTTHChar(const TCHAR c)
{
	return (c >= _T('2') && c <= _T('7')) || (c >= _T('A') && c <= _T('Z'));
}

static bool isTTH(const tstring& tth)
{
	if (tth.size() != 39)
		return false;
	for (size_t i = 0; i < 39; i++)
		if (!isTTHChar(tth[i]))
			return false;
	return true;
}

void SearchFrame::loadSearchHistory()
{
	DBRegistryMap values;
	DatabaseManager::getInstance()->loadRegistry(values, e_SearchHistory);
	g_lastSearches.clear();
	unsigned key = 0;
	while (true)
	{
		auto i = values.find(Util::toString(++key));
		if (i == values.end()) break;
		g_lastSearches.push_back(Text::toT(i->second.sval));
	}
}

void SearchFrame::saveSearchHistory()
{
	DBRegistryMap values;	
	auto dm = DatabaseManager::getInstance();
	unsigned key = 0;
	for (auto i = g_lastSearches.cbegin(); i != g_lastSearches.cend(); ++i)
		values.insert(DBRegistryMap::value_type(Util::toString(++key), DBRegistryValue(Text::fromT(*i))));
	dm->clearRegistry(e_SearchHistory, 0);
	dm->saveRegistry(values, e_SearchHistory, true);
}

void SearchFrame::openWindow(const tstring& str /* = Util::emptyString */, LONGLONG size /* = 0 */, SizeModes mode /* = SIZE_ATLEAST */, int type /* = FILE_TYPE_ANY */)
{
	SearchFrame* pChild = new SearchFrame();
#ifdef FLYLINKDC_USE_TORRENT
	pChild->disableTorrentRSS = !str.empty();
#endif
	pChild->setInitial(str, size, mode, type);
	pChild->CreateEx(WinUtil::g_mdiClient);
	g_search_frames.insert(FramePair(pChild->m_hWnd, pChild));
}

void SearchFrame::closeAll()
{
	dcdrun(const auto frameCount = g_search_frames.size());
	for (auto i = g_search_frames.cbegin(); i != g_search_frames.cend(); ++i)
	{
		::PostMessage(i->first, WM_CLOSE, 0, 0);
	}
	dcassert(frameCount == g_search_frames.size());
}

LRESULT SearchFrame::onFiletypeChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (BOOLSETTING(SAVE_SEARCH_SETTINGS))
	{
		SET_SETTING(SAVED_SEARCH_TYPE, ctrlFiletype.GetCurSel());
		SET_SETTING(SAVED_SEARCH_SIZEMODE, ctrlSizeMode.GetCurSel());
		SET_SETTING(SAVED_SEARCH_MODE, ctrlMode.GetCurSel());
		tstring st;
		WinUtil::getWindowText(ctrlSize, st);
		SET_SETTING(SAVED_SEARCH_SIZE, Text::fromT(st));
	}
	onSizeMode();
	return 0;
}

void SearchFrame::onSizeMode()
{
	BOOL isNormal = ctrlMode.GetCurSel() != 0;
	::EnableWindow(GetDlgItem(IDC_SEARCH_SIZE), isNormal);
	::EnableWindow(GetDlgItem(IDC_SEARCH_SIZEMODE), isNormal);
}

#ifndef HDS_NOSIZING
#define HDS_NOSIZING 0x0800
#endif

LRESULT SearchFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(tooltip.IsWindow());
	
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlSearchBox.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_TABSTOP, 0);
	loadSearchHistory();
	initSearchHistoryBox();
	searchBoxContainer.SubclassWindow(ctrlSearchBox.m_hWnd);
	ctrlSearchBox.SetExtendedUI();

	ctrlDoSearch.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON |
	                    BS_DEFPUSHBUTTON | WS_TABSTOP, 0, IDC_SEARCH);
	ctrlDoSearch.SetIcon(iconSearch);
	//doSearchContainer.SubclassWindow(ctrlDoSearch.m_hWnd);
	tooltip.AddTool(ctrlDoSearch, ResourceManager::SEARCH);

	ctrlPurge.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON |
	                 BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_PURGE);
	ctrlPurge.SetIcon(iconPurge);
	//purgeContainer.SubclassWindow(ctrlPurge.m_hWnd);
	tooltip.AddTool(ctrlPurge, ResourceManager::CLEAR_SEARCH_HISTORY);

	ctrlMode.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SEARCH_MODE);
	modeContainer.SubclassWindow(ctrlMode.m_hWnd);
	
	ctrlSize.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SEARCH_SIZE);
	sizeContainer.SubclassWindow(ctrlSize.m_hWnd);
	
	ctrlSizeMode.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SEARCH_SIZEMODE);
	sizeModeContainer.SubclassWindow(ctrlSizeMode.m_hWnd);
	
	ctrlFiletype.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_OWNERDRAWFIXED | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_FILETYPES);
	ResourceLoader::LoadImageList(IDR_SEARCH_TYPES, searchTypesImageList, 16, 16);
	fileTypeContainer.SubclassWindow(ctrlFiletype.m_hWnd);
	                    
	ctrlSlots.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, NULL, IDC_FREESLOTS);
	ctrlSlots.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	ctrlSlots.SetFont(Fonts::g_systemFont, FALSE);
	ctrlSlots.SetWindowText(CTSTRING(ONLY_FREE_SLOTS));
	//slotsContainer.SubclassWindow(ctrlSlots.m_hWnd);

	ctrlCollapsed.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, NULL, IDC_OPTION_CHECKBOX);
	ctrlCollapsed.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	ctrlCollapsed.SetFont(Fonts::g_systemFont, FALSE);
	ctrlCollapsed.SetWindowText(CTSTRING(EXPANDED_RESULTS));
	//collapsedContainer.SubclassWindow(ctrlCollapsed.m_hWnd);

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	ctrlStoreIP.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, NULL, IDC_OPTION_CHECKBOX);
	ctrlStoreIP.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	storeIP = BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER);
	ctrlStoreIP.SetCheck(storeIP);
	ctrlStoreIP.SetFont(Fonts::g_systemFont, FALSE);
	ctrlStoreIP.SetWindowText(CTSTRING(STORE_SEARCH_IP));
	//storeIPContainer.SubclassWindow(ctrlStoreIP.m_hWnd);
#endif

	ctrlStoreSettings.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, NULL, IDC_OPTION_CHECKBOX);
	ctrlStoreSettings.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	if (BOOLSETTING(SAVE_SEARCH_SETTINGS))
		ctrlStoreSettings.SetCheck(BST_CHECKED);
	ctrlStoreSettings.SetFont(Fonts::g_systemFont, FALSE);
	ctrlStoreSettings.SetWindowText(CTSTRING(SAVE_SEARCH_SETTINGS_TEXT));
	//storeSettingsContainer.SubclassWindow(ctrlStoreSettings.m_hWnd);	
	
	tooltip.AddTool(ctrlStoreSettings, ResourceManager::SAVE_SEARCH_SETTINGS_TOOLTIP);
	if (BOOLSETTING(ONLY_FREE_SLOTS))
	{
		ctrlSlots.SetCheck(BST_CHECKED);
		onlyFree = true;
	}
	
#ifdef FLYLINKDC_USE_TREE_SEARCH
	ctrlUseGroupTreeSettings.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, NULL, IDC_USE_TREE);
	ctrlUseGroupTreeSettings.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	if (BOOLSETTING(USE_SEARCH_GROUP_TREE_SETTINGS))
		ctrlUseGroupTreeSettings.SetCheck(BST_CHECKED);
	ctrlUseGroupTreeSettings.SetFont(Fonts::g_systemFont, FALSE);
	ctrlUseGroupTreeSettings.SetWindowText(CTSTRING(USE_SEARCH_GROUP_TREE_SETTINGS_TEXT));
#endif

#ifdef FLYLINKDC_USE_TORRENT
	ctrlUseTorrentSearch.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, NULL, IDC_OPTION_CHECKBOX);
	ctrlUseTorrentSearch.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	if (BOOLSETTING(USE_TORRENT_SEARCH))
		ctrlUseTorrentSearch.SetCheck(BST_CHECKED);
	ctrlUseTorrentSearch.SetFont(Fonts::g_systemFont, FALSE);
	ctrlUseTorrentSearch.SetWindowText(CTSTRING(USE_TORRENT_SEARCH_TEXT));
	
	ctrlUseTorrentRSS.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, NULL, IDC_OPTION_CHECKBOX);
	ctrlUseTorrentRSS.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	if (BOOLSETTING(USE_TORRENT_RSS))
		ctrlUseTorrentRSS.SetCheck(BST_CHECKED);
	ctrlUseTorrentRSS.SetFont(Fonts::g_systemFont, FALSE);
	ctrlUseTorrentRSS.SetWindowText(CTSTRING(USE_TORRENT_RSS_TEXT));
#endif

	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_NOSORTHEADER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_HUB);
	ctrlHubs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	auto ctrlHubsHeader = ctrlHubs.GetHeader();
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (CompatibilityManager::isOsVistaPlus())
#endif
		ctrlHubsHeader.SetWindowLong(GWL_STYLE, ctrlHubsHeader.GetWindowLong(GWL_STYLE) | HDS_NOSIZING);
	hubsContainer.SubclassWindow(ctrlHubs.m_hWnd);

#ifdef FLYLINKDC_USE_TREE_SEARCH
	ctrlSearchFilterTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WinUtil::getTreeViewStyle(), WS_EX_CLIENTEDGE, IDC_TRANSFER_TREE);
	ctrlSearchFilterTree.SetBkColor(Colors::g_bgColor);
	ctrlSearchFilterTree.SetTextColor(Colors::g_textColor);
	WinUtil::setExplorerTheme(ctrlSearchFilterTree);
	ctrlSearchFilterTree.SetImageList(searchTypesImageList, TVSIL_NORMAL);
	
	//m_treeContainer.SubclassWindow(ctrlSearchFilterTree);
	useTree = SETTING(USE_SEARCH_GROUP_TREE_SETTINGS) != 0;
#endif

	const bool useSystemIcons = BOOLSETTING(USE_SYSTEM_ICONS);
	ctrlResults.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                   WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | WS_TABSTOP,
	                   WS_EX_CLIENTEDGE, IDC_RESULTS);
	ctrlResults.ownsItemData = false;
	ctrlResults.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	resultsContainer.SubclassWindow(ctrlResults.m_hWnd);
	
	if (useSystemIcons)
	{
		ctrlResults.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	}
	else
	{
		ResourceLoader::LoadImageList(IDR_FOLDERS, images, 16, 16);
		ctrlResults.SetImageList(images, LVSIL_SMALL);
	}
	
	ctrlFilter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
	                  
	ctrlFilterContainer.SubclassWindow(ctrlFilter.m_hWnd);
	ctrlFilter.SetFont(Fonts::g_systemFont);
	
	ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
	                     WS_VSCROLL | CBS_DROPDOWNLIST | WS_TABSTOP, WS_EX_CLIENTEDGE);
	                     
	ctrlFilterSelContainer.SubclassWindow(ctrlFilterSel.m_hWnd);
	ctrlFilterSel.SetFont(Fonts::g_systemFont);
	
	ctrlShowUI.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP);
	ctrlShowUI.SetButtonStyle(BS_AUTOCHECKBOX | WS_TABSTOP, false);
	ctrlShowUI.SetCheck(BST_CHECKED);
	ctrlShowUI.SetFont(Fonts::g_systemFont);
	showUIContainer.SubclassWindow(ctrlShowUI.m_hWnd);
	tooltip.AddTool(ctrlShowUI, ResourceManager::SEARCH_SHOWHIDEPANEL);

	searchLabel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	searchLabel.SetFont(Fonts::g_systemFont, FALSE);
	searchLabel.SetWindowText(CTSTRING(SEARCH_FOR));
	
	sizeLabel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	sizeLabel.SetFont(Fonts::g_systemFont, FALSE);
	sizeLabel.SetWindowText(CTSTRING(SIZE));
	
	typeLabel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	typeLabel.SetFont(Fonts::g_systemFont, FALSE);
	typeLabel.SetWindowText(CTSTRING(FILE_TYPE));
	
	srLabel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	srLabel.SetFont(Fonts::g_systemFont, FALSE);
	srLabel.SetWindowText(CTSTRING(SEARCH_IN_RESULTS));
	
	optionLabel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	optionLabel.SetFont(Fonts::g_systemFont, FALSE);
	optionLabel.SetWindowText(CTSTRING(SEARCH_OPTIONS));

	hubsLabel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	hubsLabel.SetFont(Fonts::g_systemFont, FALSE);
	hubsLabel.SetWindowText(CTSTRING(HUBS));
	
	ctrlSearchBox.SetFont(Fonts::g_systemFont, FALSE);
	ctrlSize.SetFont(Fonts::g_systemFont, FALSE);
	ctrlMode.SetFont(Fonts::g_systemFont, FALSE);
	ctrlSizeMode.SetFont(Fonts::g_systemFont, FALSE);
	ctrlFiletype.SetFont(Fonts::g_systemFont, FALSE);
	
	ctrlMode.AddString(CTSTRING(ANY));
	ctrlMode.AddString(CTSTRING(AT_LEAST));
	ctrlMode.AddString(CTSTRING(AT_MOST));
	ctrlMode.AddString(CTSTRING(EXACT_SIZE));
	
	ctrlSizeMode.AddString(CTSTRING(B));
	ctrlSizeMode.AddString(CTSTRING(KB));
	ctrlSizeMode.AddString(CTSTRING(MB));
	ctrlSizeMode.AddString(CTSTRING(GB));
	
	ctrlFiletype.AddString(CTSTRING(ANY));
	ctrlFiletype.AddString(CTSTRING(AUDIO));
	ctrlFiletype.AddString(CTSTRING(COMPRESSED));
	ctrlFiletype.AddString(CTSTRING(DOCUMENT));
	ctrlFiletype.AddString(CTSTRING(EXECUTABLE));
	ctrlFiletype.AddString(CTSTRING(PICTURE));
	ctrlFiletype.AddString(CTSTRING(VIDEO_AND_SUBTITLES));
	ctrlFiletype.AddString(CTSTRING(DIRECTORY));
	ctrlFiletype.AddString(CTSTRING(TTH));
	ctrlFiletype.AddString(CTSTRING(CD_DVD_IMAGES));
	ctrlFiletype.AddString(CTSTRING(COMICS));
	ctrlFiletype.AddString(CTSTRING(BOOK));
	if (BOOLSETTING(SAVE_SEARCH_SETTINGS))
	{
		ctrlFiletype.SetCurSel(SETTING(SAVED_SEARCH_TYPE));
		ctrlSizeMode.SetCurSel(SETTING(SAVED_SEARCH_SIZEMODE));
		ctrlMode.SetCurSel(SETTING(SAVED_SEARCH_MODE));
		SetDlgItemText(IDC_SEARCH_SIZE, Text::toT(SETTING(SAVED_SEARCH_SIZE)).c_str());
	}
	else
	{
		ctrlFiletype.SetCurSel(0);
		if (initialSize == 0)
			ctrlSizeMode.SetCurSel(2);
		else
			ctrlSizeMode.SetCurSel(0);
		ctrlMode.SetCurSel(1);
	}
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));
	
	ctrlResults.insertColumns(SettingsManager::SEARCH_FRAME_ORDER, SettingsManager::SEARCH_FRAME_WIDTHS, SettingsManager::SEARCH_FRAME_VISIBLE);
	ctrlResults.setSortFromSettings(SETTING(SEARCH_FRAME_SORT), COLUMN_HITS, false);
	
	setListViewColors(ctrlResults);
	ctrlResults.SetFont(Fonts::g_systemFont, FALSE); // use Util::font instead to obey Appearace settings
	ctrlResults.setColumnOwnerDraw(COLUMN_LOCATION);
	ctrlResults.setColumnOwnerDraw(COLUMN_P2P_GUARD);

	hTheme = OpenThemeData(m_hWnd, L"EXPLORER::LISTVIEW");
	if (hTheme)
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED;
	customDrawState.flags |= CustomDrawHelpers::FLAG_GET_COLFMT;
	
	ctrlHubs.insertColumns(Util::emptyString, Util::emptyString, Util::emptyString);
	setListViewColors(ctrlHubs);
	ctrlHubs.SetFont(Fonts::g_systemFont, FALSE); // use Util::font instead to obey Appearace settings
	WinUtil::setExplorerTheme(ctrlHubs);

	copyMenu.CreatePopupMenu();
	targetDirMenu.CreatePopupMenu();
	targetMenu.CreatePopupMenu();
	priorityMenu.CreatePopupMenu();
	tabMenu.CreatePopupMenu();
	
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_SEARCH_FRAME, CTSTRING(MENU_CLOSE_ALL_SEARCHFRAME));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
	
#ifdef FLYLINKDC_USE_TORRENT
	copyMenuTorrent.CreatePopupMenu();
	copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
	copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
	copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
	//copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_FULL_MAGNET_LINK, CTSTRING(COPY_FULL_MAGNET_LINK));
	copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_TORRENT_DATE, CTSTRING(TORRENT_DATE));
	copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_TORRENT_COMMENT, CTSTRING(TORRENT_COMMENT));
	copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_TORRENT_URL, CTSTRING(TORRENT_URL));
	copyMenuTorrent.AppendMenu(MF_STRING, IDC_COPY_TORRENT_PAGE, CTSTRING(TORRENT_PAGE));
#endif
	
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_PATH, CTSTRING(PATH));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_HUB_URL, CTSTRING(HUB_ADDRESS));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_FULL_MAGNET_LINK, CTSTRING(COPY_FULL_MAGNET_LINK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
	
	WinUtil::appendPrioItems(priorityMenu, IDC_PRIORITY_PAUSED);

	ctrlUDPMode.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SS_ICON | BS_CENTER | BS_PUSHBUTTON, 0);
	ctrlUDPTestResult.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlUDPTestResult.SetFont(Fonts::g_systemFont, FALSE);
	
	showPortStatus();
	
	UpdateLayout();
	for (int j = 0; j < _countof(columnNames); j++)
		ctrlFilterSel.AddString(CTSTRING_I(columnNames[j]));

	ctrlFilterSel.SetCurSel(0);
	ctrlStatus.SetText(1, 0, SBT_OWNERDRAW);
	tooltip.SetMaxTipWidth(200);
	tooltip.Activate(TRUE);
	onSizeMode();   //Get Mode, and turn ON or OFF controlls Size
	SearchManager::getInstance()->addListener(this);
	initHubs();
#ifdef FLYLINKDC_USE_TREE_SEARCH
	treeItemRoot = nullptr;
#ifdef FLYLINKDC_USE_TORRENT
	m_RootTorrentRSSTreeItem = nullptr;
	m_24HTopTorrentTreeItem = nullptr;
#endif
	treeItemCurrent = nullptr;
	treeItemOld = nullptr;
#endif
	clearFound();
	if (!initialString.empty())
	{
		g_lastSearches.push_front(initialString);
		ctrlSearchBox.InsertString(0, initialString.c_str());
		ctrlSearchBox.SetCurSel(0);
		ctrlMode.SetCurSel(initialMode);
		ctrlSize.SetWindowText(Util::toStringT(initialSize).c_str());
		ctrlFiletype.SetCurSel(initialType);
		
		onEnter();
	}
	else
	{
		SetWindowText(CTSTRING(SEARCH));
		running = false;
	}
	SettingsManager::getInstance()->addListener(this);
	createTimer(1000);
	
#ifdef FLYLINKDC_USE_TORRENT
	if (!disableTorrentRSS && BOOLSETTING(USE_TORRENT_RSS))
	{
		m_torrentRSSThread.start_torrent_top(m_hWnd);
	}
#endif
	
	bHandled = FALSE;
	return 1;
}

LRESULT SearchFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);

	if (hTheme)
	{
		CloseThemeData(hTheme);
		hTheme = nullptr;
	}
	bHandled = FALSE;
	return 0;
}

LRESULT SearchFrame::onMeasure(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == IDC_FILETYPES)
	{
		auto mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
		mis->itemHeight = 16;
		return TRUE;
	}
	HWND hwnd = 0; // ???
	return OMenu::onMeasureItem(hwnd, uMsg, wParam, lParam, bHandled);
}

LRESULT SearchFrame::onDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	bHandled = FALSE;
	
	if (wParam == IDC_FILETYPES)
	{
		CustomDrawHelpers::drawComboBox(ctrlFiletype, dis, searchTypesImageList);
		return TRUE;
	}
	if (dis->CtlID == ATL_IDW_STATUS_BAR && dis->itemID == 1)
	{
		const auto delta = searchEndTime - searchStartTime;
		if (searchStartTime > 0 && delta)
		{
			bHandled = TRUE;
			const RECT rc = dis->rcItem;
			int borders[3];
			
			ctrlStatus.GetBorders(borders);
			
			const uint64_t now = GET_TICK();
			const uint64_t length = min((uint64_t)(rc.right - rc.left), (rc.right - rc.left) * (now - searchStartTime) / delta);
			
			OperaColors::FloodFill(dis->hDC, rc.left, rc.top, rc.left + (LONG)length, rc.bottom, RGB(128, 128, 128), RGB(160, 160, 160));
			
			SetBkMode(dis->hDC, TRANSPARENT);
			const int textHeight = WinUtil::getTextHeight(dis->hDC);
			const LONG top = rc.top + (rc.bottom - rc.top - textHeight) / 2;
			
			SetTextColor(dis->hDC, RGB(255, 255, 255));
			RECT rc2 = rc;
			rc2.right = rc.left + (LONG)length;
			ExtTextOut(dis->hDC, rc.left + borders[2], top, ETO_CLIPPED, &rc2, statusLine.c_str(), statusLine.size(), NULL);
			
			SetTextColor(dis->hDC, Colors::g_textColor);
			rc2 = rc;
			rc2.left = rc.left + (LONG)length;
			ExtTextOut(dis->hDC, rc.left + borders[2], top, ETO_CLIPPED, &rc2, statusLine.c_str(), statusLine.size(), NULL);
		}
	}
	else if (dis->CtlType == ODT_MENU)
	{
		HWND hwnd = 0; // ???
		bHandled = TRUE;
		return OMenu::onDrawItem(hwnd, uMsg, wParam, lParam, bHandled);
	}
	
	return FALSE;
}

void SearchFrame::initSearchHistoryBox()
{
	ctrlSearchBox.ResetContent();
	for (auto i = g_lastSearches.cbegin(); i != g_lastSearches.cend(); ++i)
	{
		ctrlSearchBox.AddString(i->c_str());
	}
}

#ifdef FLYLINKDC_USE_TORRENT
int SearchFrame::TorrentTopSender::run()
{
#if 0
	try
	{
		CFlyServerConfig::torrentGetTop(m_wnd, PREPARE_RESULT_TOP_TORRENT);
	}
	catch (const std::bad_alloc&)
	{
		ShareManager::tryFixBadAlloc();
	}
#endif
	return 0;
}

int SearchFrame::TorrentSearchSender::run()
{
#if 0
	try
	{
		dcassert(!search.empty());
		if (!search.empty())
		{
			CFlyServerConfig::torrentSearch(m_wnd, PREPARE_RESULT_TORRENT, search);
		}
	}
	catch (const std::bad_alloc&)
	{
		ShareManager::tryFixBadAlloc();
	}
#endif
	return 0;
}
#endif

void SearchFrame::onEnter()
{
	BOOL tmp_Handled;
	onEditChange(0, 0, NULL, tmp_Handled); // if in searchbox TTH - select filetypeTTH
	
	CFlyBusyBool busy(startingSearch);
	searchClients.clear();
	
	ctrlResults.deleteAll();
	clearFound();
	
	tstring s;
	WinUtil::getWindowText(ctrlSearch, s);
	
	// Add new searches to the last-search dropdown list
	if (!BOOLSETTING(FORGET_SEARCH_REQUEST))
	{
		g_lastSearches.remove(s);
		const int i = max(SETTING(SEARCH_HISTORY) - 1, 0);
		
		while (g_lastSearches.size() > TStringList::size_type(i))
		{
			g_lastSearches.pop_back();
		}
		g_lastSearches.push_front(s);
		initSearchHistoryBox();
		saveSearchHistory();
	}
	MainFrame::getMainFrame()->updateQuickSearches();
#ifdef FLYLINKDC_USE_TORRENT
	if (BOOLSETTING(USE_TORRENT_SEARCH) && searchParam.fileType != FILE_TYPE_TTH && !isTTH(s) && !s.empty())
	{
		m_torrentSearchThread.start_torrent_search(m_hWnd, s);
	}
#endif
	// Change Default Settings If Changed
	if (onlyFree != BOOLSETTING(ONLY_FREE_SLOTS))
	{
		SET_SETTING(ONLY_FREE_SLOTS, onlyFree);
	}

	bool searchingOnDHT = false;
	searchParam.fileType = ctrlFiletype.GetCurSel();
	const int hubCount = ctrlHubs.GetItemCount();
	if (hubCount <= 1)
	{
		SetWindowText(CTSTRING(SEARCH));
		MessageBox(CTSTRING(SEARCH_NO_HUBS), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
		return;
	}

	for (int i = 0; i < hubCount; i++)
	{
		if (ctrlHubs.GetCheckState(i))
		{
			const string& url = ctrlHubs.getItemData(i)->url;
			if (url.empty())
				continue;
			if (url == dht::NetworkName)
			{
				if (searchParam.fileType != FILE_TYPE_TTH)
				{
					ctrlHubs.SetCheckState(i, FALSE);
					continue;
				}
				searchingOnDHT = true;
			}
			searchClients.emplace_back(SearchClientItem{ url, 0 });
		}
	}

	if (searchClients.empty())
	{
		SetWindowText(CTSTRING(SEARCH));
		return;
	}

	tstring sizeStr;
	WinUtil::getWindowText(ctrlSize, sizeStr);
	
	double size = Util::toDouble(Text::fromT(sizeStr));
	unsigned scale = 1u << (ctrlSizeMode.GetCurSel() * 10);
	size *= scale;
	searchParam.size = size;
	
	search = StringTokenizer<string>(Text::fromT(s), ' ').getTokens();
	
	string filter;
	string filterExclude;
	{
		LOCK(csSearch);
		//strip out terms beginning with -
		for (auto si = search.cbegin(); si != search.cend();)
		{
			if (si->empty())
			{
				si = search.erase(si);
				continue;
			}
			if (searchParam.fileType == FILE_TYPE_TTH)
			{
				if (!isTTH(Text::toT(*si)))
				{
					searchParam.fileType = FILE_TYPE_ANY;
					ctrlFiletype.SetCurSel(0);
				}
			}
			if ((*si)[0] != '-')
			{
				if (!filter.empty()) filter += ' ';
				filter += *si;
			}
			else
			if (si->length() > 1)
			{
				if (!filterExclude.empty()) filterExclude += ' ';
				filterExclude += si->substr(1);
			}
			++si;
		}
		searchParam.generateToken(false);
	}
	
	s = Text::toT(filter);
	if (s.empty())
	{
		SetWindowText(CTSTRING(SEARCH));
		return;
	}
		
	searchTarget = std::move(s);
	
	if (searchParam.size == 0)
		searchParam.sizeMode = SIZE_DONTCARE;
	else
		searchParam.sizeMode = SizeModes(ctrlMode.GetCurSel());
		
	ctrlStatus.SetText(3, _T(""));
	ctrlStatus.SetText(4, _T(""));
	
	isExactSize = searchParam.sizeMode == SIZE_EXACT;
	exactSize = searchParam.size;
	
	if (BOOLSETTING(CLEAR_SEARCH))
	{
		ctrlSearch.SetWindowText(_T(""));
	}
	
	droppedResults = 0;
	resultsCount = 0;
	running = true;
	isHash = searchParam.fileType == FILE_TYPE_TTH;
	
	SetWindowText((TSTRING(SEARCH) + _T(" - ") + searchTarget).c_str());
	
	// stop old search
	ClientManager::cancelSearch(this);
	searchParam.extList.clear();
	// Get ADC searchtype extensions if any is selected
	try
	{
		if (searchParam.fileType == FILE_TYPE_ANY)
		{
			// Custom searchtype
			// disabled with current GUI extList = SettingsManager::getInstance()->getExtensions(Text::fromT(fileType->getText()));
		}
		else if ((searchParam.fileType > FILE_TYPE_ANY && searchParam.fileType < FILE_TYPE_DIRECTORY) /* TODO - || m_ftype == FILE_TYPE_CD_DVD*/)
		{
			// Predefined searchtype
			searchParam.extList = SettingsManager::getExtensions(string(1, '0' + searchParam.fileType));
		}
	}
	catch (const SearchTypeException&)
	{
		searchParam.fileType = FILE_TYPE_ANY;
	}
	
	{
		LOCK(csSearch);
		
		searchStartTime = GET_TICK();
		// more 10 seconds for transfering results
		searchParam.filter = filter;
		searchParam.filterExclude = filterExclude;
		searchParam.normalizeWhitespace();
		searchParam.owner = this;
		if (portStatus == PortTest::STATE_FAILURE || BOOLSETTING(SEARCH_PASSIVE)) // || (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5);
			searchParam.searchMode = SearchParamBase::MODE_PASSIVE;
		else
			searchParam.searchMode = SearchParamBase::MODE_DEFAULT;
		unsigned dhtSearchTime = searchingOnDHT ? dht::SEARCHFILE_LIFETIME : 0;
		searchEndTime = searchStartTime + ClientManager::multiSearch(searchParam, searchClients) + max(dhtSearchTime, SEARCH_RESULTS_WAIT_TIME);
		updateWaitingTime();
		waitingResults = true;
	}
}

void SearchFrame::removeSelected()
{
	int i = -1;
	LOCK(csSearch);
	while ((i = ctrlResults.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		ctrlResults.removeGroupedItem(ctrlResults.getItemData(i));
		// TODO - delete from set
	}
}

void SearchFrame::on(SearchManagerListener::SR, const SearchResult& sr) noexcept
{
	if (isClosedOrShutdown())
		return;
	// Check that this is really a relevant search result...
	{
		LOCK(csSearch);
		
		if (search.empty())
			return;
			
		needUpdateResultCount = true;
		if (sr.getToken() && searchParam.token != sr.getToken())
		{
			droppedResults++;
			return;
		}
		
		string ext;
#if 0
		bool isExecutable = false;
#endif
		if (sr.getType() == SearchResult::TYPE_FILE)
		{
			ext = "x" + Util::getFileExt(sr.getFileName());
#if 0
			isExecutable = sr.getSize() && (getFileTypesFromFileName(ext) & 1<<FILE_TYPE_EXECUTABLE) != 0;
#endif
		}
		if (isHash)
		{
			if (sr.getType() != SearchResult::TYPE_FILE || TTHValue(search[0]) != sr.getTTH())
			{
				droppedResults++;
				return;
			}
		}
		else
		{
#if 0
			if (m_search_param.fileType != FILE_TYPE_EXECUTABLE && m_search_param.fileType != Search::TYPE_ANY && m_search_param.fileType != FILE_TYPE_DIRECTORY)
			{
				if (isExecutable)
				{
					const int l_virus_level = 3;
					CFlyServerJSON::addAntivirusCounter(*aResult, 0, l_virus_level);
					sr.m_virus_level = l_virus_level;
					LogManager::virus_message("Search: ignore virus result  (Level 3): TTH = " + sr.getTTH().toBase32() +
					                          " File: " + sr.getFileName() + +" Size:" + Util::toString(sr.getSize()) +
					                          " Hub: " + sr.getHubUrl() + " Nick: " + sr.getUser()->getLastNick() + " IP = " + sr.getIPAsString());
					// http://dchublist.ru/forum/viewtopic.php?p=22426#p22426
					droppedResults++;
					return;
				}
			}
#endif			
#if 0
			if (isExecutable || ShareManager::checkType(ext, FILE_TYPE_COMPRESSED))
			{
				// Level 1
				size_t l_count = check_antivirus_level(make_pair(sr.getTTH(), sr.getUser()->getLastNick() + " Hub:" + sr.getHubName() + " ( " + sr.getHubUrl() + " ) "), *aResult, 1);
				if (l_count > CFlyServerConfig::g_unique_files_for_virus_detect && isExecutable) // 100$ Virus - block IP ?
				{
					const int l_virus_level = 1;
					// TODO CFlyServerConfig::addBlockIP(aResult.getIPAsString());
					if (registerVirusLevel(*aResult, l_virus_level))
					{
						CFlyServerJSON::addAntivirusCounter(*aResult, l_count, l_virus_level);
					}
				}
				// TODO - Включить блокировку поисковой выдачи return;
				// Level 2
				l_count = check_antivirus_level(make_pair(sr.getTTH(), "."), *aResult, 2);
				if (l_count)
				{
					const int l_virus_level = 2;
					// TODO - Включить блокировку поисковой выдачи return;
				}
			}
#endif
			if (sr.getType() != SearchResult::TYPE_DIRECTORY)
			{
				static const int localFilter[] =
				{
					FILE_TYPE_AUDIO,
					FILE_TYPE_COMPRESSED,
					FILE_TYPE_DOCUMENT,
					FILE_TYPE_EXECUTABLE,
					FILE_TYPE_IMAGE,
					FILE_TYPE_VIDEO,
					FILE_TYPE_CD_DVD,
					FILE_TYPE_COMICS,
					FILE_TYPE_EBOOK,
				};
				for (auto k = 0; k < _countof(localFilter); ++k)
				{
					if (searchParam.fileType == localFilter[k])
					{
						if (!(getFileTypesFromFileName(ext) & 1<<localFilter[k]))
						{
							droppedResults++;
							return;
						}
						break;
					}
				}
			}
			// match all here
			tstring fileName = Text::toT(sr.getFile());
			Text::makeLower(fileName);
			for (auto j = search.cbegin(); j != search.cend(); ++j)
			{
				if (j->empty()) continue;
				tstring search = Text::toT(*j);
				Text::makeLower(search);
				if (search[0] != '-')
				{
					if (fileName.find(search) == tstring::npos)
					{
						droppedResults++;
						return;
					}
				}
				else if (search.length() > 1)
				{
					if (fileName.find(search.substr(1)) != tstring::npos)
					{
						droppedResults++;
						return;
					}
				}
			}
		}
	}
	// Reject results without free slots or size
	if ((onlyFree && sr.freeSlots < 1) || (isExactSize && sr.getSize() != exactSize))
	{
		droppedResults++;
		return;
	}
	if (isClosedOrShutdown())
		return;
	auto searchInfo = new SearchInfo(sr);
	{
		LOCK(csEverything);
		everything.insert(searchInfo);
	}
	if (!safe_post_message(*this, ADD_RESULT, searchInfo))
	{
		LOCK(csEverything);
		everything.erase(searchInfo);
		dcassert(0);
	}
}

LRESULT SearchFrame::onChangeOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	expandSR = ctrlCollapsed.GetCheck() == BST_CHECKED;
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	storeIP = ctrlStoreIP.GetCheck() == BST_CHECKED;
#endif
	storeSettings = ctrlStoreSettings.GetCheck() == BST_CHECKED;
	SET_SETTING(SAVE_SEARCH_SETTINGS, storeSettings);
#ifdef FLYLINKDC_USE_TORRENT
	SET_SETTING(USE_TORRENT_SEARCH, ctrlUseTorrentSearch.GetCheck() == BST_CHECKED);
	SET_SETTING(USE_TORRENT_RSS, ctrlUseTorrentRSS.GetCheck() == BST_CHECKED);
#endif
	return 0;
}


LRESULT SearchFrame::onToggleTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
#ifdef FLYLINKDC_USE_TREE_SEARCH
	useTree = ctrlUseGroupTreeSettings.GetCheck() == BST_CHECKED;
	SET_SETTING(USE_SEARCH_GROUP_TREE_SETTINGS, useTree);
	UpdateLayout();
	if (!useTree)
	{
		if (treeItemRoot)
		{
			treeItemOld = treeItemCurrent;
			ctrlSearchFilterTree.SelectItem(treeItemRoot);
		}
	}
	else
	{
		if (treeItemOld)
		{
			if (ctrlSearchFilterTree.SelectItem(treeItemOld))
			{
				treeItemCurrent = treeItemOld;
			}
			else
			{
				dcassert(0);
			}
		}
	}
#endif
	return 0;
}

LRESULT SearchFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	if (!isClosedOrShutdown())
	{
		bool newDHTStatus = dht::DHT::getInstance()->isConnected();
		if (newDHTStatus != useDHT)
		{
			useDHT = newDHTStatus;
			auto hubInfo = new HubInfo(dht::NetworkName, TSTRING(DHT), false);
			if (useDHT)
				onHubAdded(hubInfo);
			else
				onHubRemoved(hubInfo);
		}
		showPortStatus();
		if (!MainFrame::isAppMinimized(m_hWnd))
		{
			if (needUpdateResultCount)
				updateResultCount();
			if (shouldSort)
			{
				shouldSort = updateList = false;
				ctrlResults.resort();
			}
			if (updateList)
			{
				ctrlResults.Invalidate();
				updateList = false;
			}
		}
		if (waitingResults)
		{
			uint64_t tick = GET_TICK();
			updateStatusLine(tick);
			waitingResults = tick < searchEndTime + 5000;
		}
		if (hasWaitTime)
			updateWaitingTime();
	}
	return 0;
}

int SearchFrame::SearchInfo::compareItems(const SearchInfo* a, const SearchInfo* b, int col)
{
	switch (col)
	{
		case COLUMN_TYPE:
			if (a->sr.getType() == b->sr.getType())
				return stricmp(a->getText(COLUMN_TYPE), b->getText(COLUMN_TYPE));
			else
				return (a->sr.getType() == SearchResult::TYPE_DIRECTORY) ? -1 : 1;
		case COLUMN_HITS:
#ifdef FLYLINKDC_USE_TORRENT
			if (a->m_is_torrent)
				return compare(a->sr.peer << 16 | a->sr.seed, b->sr.peer << 16 | b->sr.seed);
#endif
			return compare(a->hits, b->hits);
		case COLUMN_SLOTS:
			if (a->sr.freeSlots == b->sr.freeSlots)
				return compare(a->sr.slots, b->sr.slots);
			else
				return compare(a->sr.freeSlots, b->sr.freeSlots);
		case COLUMN_SIZE:
		case COLUMN_EXACT_SIZE:
			if (a->sr.getType() == b->sr.getType())
				return compare(a->sr.getSize(), b->sr.getSize());
			else
				return (a->sr.getType() == SearchResult::TYPE_DIRECTORY) ? -1 : 1;
		case COLUMN_IP:
			return compare(a->sr.getIP(), b->sr.getIP());
		default:
			return Util::defaultSort(a->getText(col), b->getText(col));
	}
}

void SearchFrame::SearchInfo::calcImageIndex()
{
#ifdef FLYLINKDC_USE_TORRENT
	if (m_is_torrent)
	{
		iconIndex = FileImage::DIR_TORRENT;
		return;
	}
#endif
	if (iconIndex < 0)
	{
		if (sr.getType() == SearchResult::TYPE_FILE)
			iconIndex = g_fileImage.getIconIndex(sr.getFile());
		else
			iconIndex = FileImage::DIR_ICON;
	}
}

int SearchFrame::SearchInfo::getImageIndex() const
{
	//dcassert(iconIndex >= 0);
	return iconIndex;
}

const tstring& SearchFrame::SearchInfo::getText(uint8_t col) const
{
	if (col >= COLUMN_LAST)
		return Util::emptyStringT;
	if (colMask & (1<<col))
		return columns[col];
#ifdef FLYLINKDC_USE_TORRENT
	if (m_is_torrent)
	{
		switch (col)
		{
			case COLUMN_TTH:
				columns[COLUMN_TTH] = Text::toT(sr.getTorrentMagnet());
				break;
			case COLUMN_FILENAME:
				columns[COLUMN_FILENAME] = Text::toT(sr.getFile());
				break;
			case COLUMN_HITS:
				columns[COLUMN_HITS] = Text::toT(sr.getPeersString());
				break;
			case COLUMN_SIZE:
				columns[COLUMN_SIZE] = sr.getSize() > 0 ? Util::formatBytesT(sr.getSize()) : Util::emptyStringT;
				break;
			case COLUMN_TORRENT_COMMENT:
				columns[COLUMN_TORRENT_COMMENT] = Text::toT(Util::toString(sr.m_comment));
				break;
			case COLUMN_TORRENT_DATE:
				columns[COLUMN_TORRENT_DATE] = Text::toT(sr.m_date);
				break;
			case COLUMN_TORRENT_URL:
				columns[COLUMN_TORRENT_URL] = Text::toT(sr.torrentUrl);
				break;
			case COLUMN_TORRENT_TRACKER:
				columns[COLUMN_TORRENT_TRACKER] = Text::toT(sr.m_tracker);
				break;
			case COLUMN_TORRENT_PAGE:
				columns[COLUMN_TORRENT_PAGE] = Text::toT(Util::toString(sr.m_torrent_page));
		}
		colMask |= 1<<col;
		return columns[col];
	}
	else
#endif
	{
		switch (col)
		{
			case COLUMN_FILENAME:
				if (sr.getType() == SearchResult::TYPE_FILE)
				{
					if (sr.getFile().rfind(_T('\\')) == tstring::npos)
						columns[COLUMN_FILENAME] = Text::toT(sr.getFile());
					else
						columns[COLUMN_FILENAME] = Text::toT(Util::getFileName(sr.getFile()));
				}
				else
					columns[COLUMN_FILENAME] = Text::toT(sr.getFileName());
				break;
			case COLUMN_HITS:
				columns[COLUMN_HITS] = hits == 0 ? Util::emptyStringT : Util::toStringT(hits + 1) + _T(' ') + TSTRING(USERS);
				return columns[COLUMN_HITS];
			case COLUMN_NICK:
				if (getUser())
				{
					// FIXME
					columns[COLUMN_NICK] = Text::toT(Util::toString(ClientManager::getNicks(getUser()->getCID(), sr.getHubUrl(), false)));
				}
				break;
			case COLUMN_TYPE:
				if (sr.getType() == SearchResult::TYPE_FILE)
					columns[COLUMN_TYPE] = Text::toT(Util::getFileExtWithoutDot(Text::fromT(getText(COLUMN_FILENAME))));
				else
					columns[COLUMN_TYPE] = TSTRING(DIRECTORY);
				break;
			case COLUMN_EXACT_SIZE:
				if (sr.getSize() > 0)
					columns[COLUMN_EXACT_SIZE] = Util::formatExactSizeT(sr.getSize());
				break;
			case COLUMN_SIZE:
				if (sr.getType() == SearchResult::TYPE_FILE)
					columns[COLUMN_SIZE] = Util::formatBytesT(sr.getSize());
				break;
			case COLUMN_PATH:
				if (sr.getType() == SearchResult::TYPE_FILE)
					columns[COLUMN_PATH] = Text::toT(Util::getFilePath(sr.getFile()));
				else
					columns[COLUMN_PATH] = Text::toT(sr.getFile());
				break;
			case COLUMN_LOCAL_PATH:
				if (sr.getType() == SearchResult::TYPE_FILE)
				{
					string s;				
					ShareManager::getInstance()->getFileInfo(sr.getTTH(), s);
					columns[COLUMN_LOCAL_PATH] = Text::toT(s);
				}
				break;
			case COLUMN_SLOTS:
			{
				tstring& result = columns[COLUMN_SLOTS];
				result = sr.freeSlots == SearchResult::SLOTS_UNKNOWN ? _T("?") : Util::toStringT(sr.freeSlots);
				result += _T('/');
				result += Util::toStringT(sr.slots);
				break;
			}
			case COLUMN_HUB:
			{
				const string& s = sr.getHubUrl();
				if (s != dht::NetworkName)
				{
					tstring result;
					if (!s.empty() && s.front() == '[' && s.back() == ']')
					{
						StringTokenizer<string> st(s.substr(1, s.length()-2), ',');
						for (string& token : st.getWritableTokens())
						{
							boost::algorithm::trim(token);
							string hubName = ClientManager::getHubName(token);
							if (!hubName.empty())
							{
								if (!result.empty()) result += _T(", ");
								result += Text::toT(hubName);
								result += _T(" (");
								result += Text::toT(token);
								result += _T(")");
							}
						}
					}
					else
					{
						string hubName = ClientManager::getHubName(s);
						if (!hubName.empty())
						{
							result += Text::toT(hubName);
							result += _T(" (");
							result += Text::toT(s);
							result += _T(")");
						}
					}
					if (result.empty()) result = TSTRING(OFFLINE);
					columns[COLUMN_HUB] = std::move(result);
				}
				else
					columns[COLUMN_HUB] = Text::toT(s);
				break;
			}
			case COLUMN_IP:
				columns[COLUMN_IP] = Text::toT(sr.getIPAsString());
				break;
			case COLUMN_P2P_GUARD:
				columns[COLUMN_P2P_GUARD] = Text::toT(sr.getIpInfo().p2pGuard);
				break;
			case COLUMN_TTH:
				if (sr.getType() == SearchResult::TYPE_FILE)
					columns[COLUMN_TTH] = Text::toT(sr.getTTH().toBase32());
				break;
			case COLUMN_LOCATION:
				return Util::emptyStringT;
		}
	}
	colMask |= 1<<col;
	return columns[col];
}

void SearchFrame::SearchInfo::Download::operator()(SearchInfo* si)
{
#ifdef FLYLINKDC_USE_TORRENT
	if (si->m_is_torrent)
	{
		dcassert(0);
		return;
	}
#endif
	try
	{
		if (prio == QueueItem::DEFAULT && WinUtil::isShift())
			prio = QueueItem::HIGHEST;
			
		if (si->sr.getType() == SearchResult::TYPE_FILE)
		{
			tstring tmp = tgt;
			dcassert(!tmp.empty());
			if (!tmp.empty() && tmp.back() == PATH_SEPARATOR)
				tmp += si->getText(COLUMN_FILENAME);
			const string target = Text::fromT(tmp);
			bool getConnFlag = true;
			QueueManager::getInstance()->add(target, si->sr.getSize(), si->sr.getTTH(), si->sr.getHintedUser(), mask, prio, true, getConnFlag);
			si->sr.flags |= SearchResult::FLAG_QUEUED;
			sf->updateList = true;
			
			const auto children = sf->getUserList().findChildren(si->getGroupCond());
			for (auto i = children.cbegin(); i != children.cend(); ++i)
			{
				SearchInfo* j = *i;
				try
				{
					if (j)
					{
						getConnFlag = true;
						QueueManager::getInstance()->add(target, j->sr.getSize(), j->sr.getTTH(), j->sr.getHintedUser(), mask, prio, true, getConnFlag);
						j->sr.flags |= SearchResult::FLAG_QUEUED;
						sf->updateList = true;
					}
				}
				catch (const Exception& e)
				{
					LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
				}
				
			}
		}
		else
		{
			QueueManager::getInstance()->addDirectory(si->sr.getFile(), si->sr.getHintedUser(), Text::fromT(tgt), prio);
		}
	}
	catch (const Exception& e)
	{
		LogManager::message("SearchInfo::Download Error = " + e.getError());
	}
	
}

void SearchFrame::SearchInfo::DownloadWhole::operator()(const SearchInfo* si)
{
	try
	{
		QueueItem::Priority prio = WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT;
		if (si->sr.getType() == SearchResult::TYPE_FILE)
		{
			QueueManager::getInstance()->addDirectory(Text::fromT(si->getText(COLUMN_PATH)), si->sr.getHintedUser(), Text::fromT(tgt), prio);
		}
		else
		{
			QueueManager::getInstance()->addDirectory(si->sr.getFile(), si->sr.getHintedUser(), Text::fromT(tgt), prio);
		}
	}
	catch (const Exception&)
	{
	}
}

void SearchFrame::SearchInfo::getList()
{
	try
	{
		QueueManager::getInstance()->addList(sr.getHintedUser(), QueueItem::FLAG_CLIENT_VIEW, Text::fromT(getText(COLUMN_PATH)));
	}
	catch (const Exception&)
	{
		dcassert(0);
		// Ignore for now...
	}
}

void SearchFrame::SearchInfo::browseList()
{
	try
	{
		QueueManager::getInstance()->addList(sr.getHintedUser(), QueueItem::FLAG_CLIENT_VIEW | QueueItem::FLAG_PARTIAL_LIST, Text::fromT(getText(COLUMN_PATH)));
	}
	catch (const Exception&)
	{
		dcassert(0);
		// Ignore for now...
	}
}

void SearchFrame::SearchInfo::CheckTTH::operator()(const SearchInfo* si)
{
	if (firstTTH)
	{
		tth = si->getText(COLUMN_TTH);
		hasTTH = true;
		firstTTH = false;
	}
	else if (hasTTH)
	{
		if (tth != si->getText(COLUMN_TTH))
		{
			hasTTH = false;
		}
	}
	CID cid;
	if (si->sr.getUser())
		cid = si->sr.getUser()->getCID();
	if (firstHubs && hubs.empty() && si->sr.getUser())
	{
		hubs = ClientManager::getHubs(cid, si->sr.getHubUrl());
		firstHubs = false;
	}
	else if (!hubs.empty())
	{
		// we will merge hubs of all users to ensure we can use OP commands in all hubs
		const StringList sl = ClientManager::getHubs(cid, Util::emptyString);
		hubs.insert(hubs.end(), sl.begin(), sl.end());
#if 0
		Util::intersect(hubs, ClientManager::getHubs(cid));
#endif
	}
}

bool SearchFrame::getDownloadDirectory(WORD wID, tstring& dir) const
{
	auto i = dlTargets.find(wID);
	if (i == dlTargets.end()) return false;
	const auto& target = i->second;
	if (target.type == DownloadTarget::PATH_BROWSE)
	{
		if (!WinUtil::browseDirectory(dir, m_hWnd))
			return false;
		LastDir::add(dir);
		return true;
	}
	if (target.type == DownloadTarget::PATH_SRC && !target.path.empty())
	{
		dir = target.path;
		LastDir::add(dir);
		return true;
	}
	if (target.type == DownloadTarget::PATH_DEFAULT || !target.path.empty())
	{
		dir = target.path;
		return true;
	}
	return false;
}

LRESULT SearchFrame::onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	QueueItem::Priority p;
	
	if (wID >= IDC_PRIORITY_PAUSED && wID < IDC_PRIORITY_PAUSED + QueueItem::LAST)
		p = (QueueItem::Priority) (wID - IDC_PRIORITY_PAUSED);
	else
		p = QueueItem::DEFAULT;
	
	auto fm = FavoriteManager::getInstance();
	int i = -1;
	while ((i = ctrlResults.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		SearchInfo* si = ctrlResults.getItemData(i);
		tstring dir = Text::toT(fm->getDownloadDirectory(Util::getFileExt(si->sr.getFileName())));
		(SearchInfo::Download(dir, this, p))(si);
	}
	return 0;
}

LRESULT SearchFrame::onDownload(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir;
	if (!getDownloadDirectory(wID, dir)) return 0;
	if (!dir.empty())
		ctrlResults.forEachSelectedT(SearchInfo::Download(dir, this, QueueItem::DEFAULT));
	else
	{
		auto fm = FavoriteManager::getInstance();
		int i = -1;
		while ((i = ctrlResults.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			SearchInfo* si = ctrlResults.getItemData(i);
#ifdef FLYLINKDC_USE_TORRENT
			if (si->m_is_torrent)
			{
				DownloadManager::getInstance()->add_torrent_file(_T(""), Text::toT(si->sr.getTorrentMagnet()));
			}
			else
#endif
			{
				const string t = fm->getDownloadDirectory(Util::getFileExt(si->sr.getFileName()));
				(SearchInfo::Download(Text::toT(t), this, QueueItem::DEFAULT))(si);
			}
		}
	}
	return 0;
}

LRESULT SearchFrame::onDownloadWhole(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir;
	if (getDownloadDirectory(wID, dir) && !dir.empty())
		ctrlResults.forEachSelectedT(SearchInfo::DownloadWhole(dir));
	return 0;
}

LRESULT SearchFrame::onDoubleClickResults(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const LPNMITEMACTIVATE item = (LPNMITEMACTIVATE)pnmh;
	
	if (item->iItem != -1)
	{
		CRect rect;
		ctrlResults.GetItemRect(item->iItem, rect, LVIR_ICON);
		
		// if double click on state icon, ignore...
		if (item->ptAction.x < rect.left)
			return 0;
			
		auto fm = FavoriteManager::getInstance();
		int i = -1;
		while ((i = ctrlResults.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			SearchInfo* si = ctrlResults.getItemData(i);
			if (si)
			{
#ifdef FLYLINKDC_USE_TORRENT
				if (si->m_is_torrent)
				{
					DownloadManager::getInstance()->add_torrent_file(_T(""), Text::toT(si->sr.getTorrentMagnet()));
				}
				else
#endif
				{
					const string t = fm->getDownloadDirectory(Util::getFileExt(si->sr.getFileName()));
					(SearchInfo::Download(Text::toT(t), this, QueueItem::DEFAULT))(si);
				}
			}
		}
		//ctrlResults.forEachSelectedT(SearchInfo::Download(Text::toT(SETTING(DOWNLOAD_DIRECTORY)), this, QueueItem::DEFAULT));
	}
	return 0;
}

LRESULT SearchFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & bHandled)
{
	destroyTimer();
	closing = true;
	CWaitCursor waitCursor;
	if (!closed)
	{
		closed = true;
		SettingsManager::getInstance()->removeListener(this);
		ClientManager::getInstance()->removeListener(this);
		SearchManager::getInstance()->removeListener(this);
		g_search_frames.erase(m_hWnd);
		ctrlResults.deleteAll();
		ctrlHubs.deleteAll();
		clearFound();
		ctrlResults.saveHeaderOrder(SettingsManager::SEARCH_FRAME_ORDER, SettingsManager::SEARCH_FRAME_WIDTHS, SettingsManager::SEARCH_FRAME_VISIBLE);
		SET_SETTING(SEARCH_FRAME_SORT, ctrlResults.getSortForSettings());
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		bHandled = FALSE;
		return 0;
	}
}

void SearchFrame::UpdateLayout(BOOL resizeBars)
{
	if (isClosedOrShutdown())
		return;
	tooltip.Activate(FALSE);
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, resizeBars);

	static const int width = 256;
	static const int labelH = 16;
	static const int comboH = 140;
	static const int lMargin = 4;
	static const int rMargin = 4;
	static const int smallButtonWidth = 30;
	static const int largeButtonWidth = 88;
	static const int buttonHeight = 24;
	static const int labelOffset = 4;
	static const int vertLabelOffset = 3;
	static const int vertSpacing = 10;
	static const int checkboxOffset = 6;
	static const int checkboxHeight = 17;
	static const int bottomMargin = 5;

	const int textHeight = WinUtil::getTextHeight(m_hWnd, Fonts::g_systemFont);
	const int controlHeight = textHeight + 9;
	const int searchInResultsHeight = 2*bottomMargin + controlHeight;
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[5];
		ctrlStatus.GetClientRect(sr);
		int tmp = (sr.Width()) > 420 ? 376 : ((sr.Width() > 116) ? sr.Width() - 100 : 16);
		
		w[0] = 42;
		w[1] = sr.right - tmp;
		w[2] = w[1] + (tmp - 16) / 3;
		w[3] = w[2] + (tmp - 16) / 3;
		w[4] = w[3] + (tmp - 16) / 3;
		
		ctrlStatus.SetParts(5, w);
		
		// Layout showUI button in statusbar part #0
		ctrlStatus.GetRect(0, sr);
		sr.left += 4;
		sr.right += 4;
		ctrlShowUI.MoveWindow(sr);
	}
	if (showUI)
	{
		CRect rc = rect;
#ifdef FLYLINKDC_USE_TREE_SEARCH
		const int treeWidth = useTree ? 200 : 0;
#else
		const int treeWidth = 0;
#endif
		rc.left += width + treeWidth;
		rc.bottom -= searchInResultsHeight;
		ctrlResults.MoveWindow(rc);
		
#ifdef FLYLINKDC_USE_TREE_SEARCH
		if (useTree)
		{
			CRect rcTree = rc;
			rcTree.left -= treeWidth;
			rcTree.right = rcTree.left + treeWidth - 5;
			ctrlSearchFilterTree.MoveWindow(rcTree);
			ctrlSearchFilterTree.ShowWindow(SW_SHOW);
		}
		else
			ctrlSearchFilterTree.ShowWindow(SW_HIDE);
#endif
		// "Search for"
		rc.left = lMargin + labelOffset;
		rc.right = width - rMargin - 2*(lMargin + smallButtonWidth);
		rc.top += 8;
		rc.bottom = rc.top + labelH;
		searchLabel.MoveWindow(rc);

		// Search box
		rc.left = lMargin;
		rc.top = rc.bottom;
		rc.bottom = rc.top + comboH;
		ctrlSearchBox.MoveWindow(rc);

		// "Search"
		rc.left = rc.right + lMargin;
		rc.right = rc.left + smallButtonWidth;
		rc.bottom = rc.top + buttonHeight;
		ctrlDoSearch.MoveWindow(rc);

		// "Clear search history"
		rc.left = rc.right + lMargin;
		rc.right = rc.left + smallButtonWidth;
		ctrlPurge.MoveWindow(rc);

		// "Size"
		rc.left = lMargin + labelOffset;
		rc.top = rc.bottom + vertSpacing;
		rc.bottom = rc.top + labelH;
		rc.right = width - rMargin;
		sizeLabel.MoveWindow(rc);

		// Size mode
		int w2 = width - lMargin - rMargin;
		rc.left = lMargin;
		rc.right = w2 / 2;
		rc.top = rc.bottom;
		rc.bottom = rc.top + comboH;
		ctrlMode.MoveWindow(rc);
		
		// Size input field
		rc.left = rc.right + lMargin;
		rc.right += w2 / 4;
		rc.bottom = rc.top + controlHeight;
		ctrlSize.MoveWindow(rc);
		
		// Units combo
		rc.left = rc.right + lMargin;
		rc.right = width - rMargin;
		rc.bottom = rc.top + comboH;
		ctrlSizeMode.MoveWindow(rc);

		// "File type"
		rc.left = lMargin + labelOffset;
		rc.top += controlHeight + vertSpacing;
		rc.bottom = rc.top + labelH;
		rc.right = width - rMargin;
		typeLabel.MoveWindow(rc);
		
		// File type combo
		rc.left = lMargin;
		rc.top = rc.bottom;
		rc.bottom = rc.top + comboH + 21;
		ctrlFiletype.MoveWindow(rc);
		
		// "Search options"
		rc.top += controlHeight + vertSpacing;
		rc.bottom = rc.top + labelH;
		rc.left = lMargin + labelOffset;
		optionLabel.MoveWindow(rc);
		
		// Checkboxes
		rc.top = rc.bottom + 2;
		rc.bottom = rc.top + checkboxHeight;
		rc.left = lMargin + checkboxOffset;
		ctrlSlots.MoveWindow(rc);
		
		rc.top += checkboxHeight;
		rc.bottom += checkboxHeight;
		ctrlCollapsed.MoveWindow(rc);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		rc.top += checkboxHeight;
		rc.bottom += checkboxHeight;
		ctrlStoreIP.MoveWindow(rc);
#endif
		rc.top += checkboxHeight;
		rc.bottom += checkboxHeight;
		ctrlStoreSettings.MoveWindow(rc);
		
#ifdef FLYLINKDC_USE_TREE_SEARCH
		rc.top += checkboxHeight;
		rc.bottom += checkboxHeight;
		ctrlUseGroupTreeSettings.MoveWindow(rc);
#endif
		
#ifdef FLYLINKDC_USE_TORRENT
		rc.top += checkboxHeight;
		rc.bottom += checkboxHeight;
		ctrlUseTorrentSearch.MoveWindow(rc);
		
		rc.top += checkboxHeight;
		rc.bottom += checkboxHeight;
		ctrlUseTorrentRSS.MoveWindow(rc);
#endif
		
		// "Hubs"
		rc.left = lMargin + labelOffset;
		rc.top = rc.bottom + vertSpacing;
		rc.bottom = rc.top + labelH;
		hubsLabel.MoveWindow(rc);

		// Hubs listview
		rc.left = lMargin;
		rc.top = rc.bottom;
		rc.bottom = rect.bottom - bottomMargin;
		if (rc.bottom < rc.top + (labelH * 3) / 2)
			rc.bottom = rc.top + (labelH * 3) / 2;
		ctrlHubs.MoveWindow(rc);
	}
	else
	{
		CRect rc = rect;
		rc.bottom -= searchInResultsHeight;
		ctrlResults.MoveWindow(rc);
		
		rc.SetRect(0, 0, 0, 0);
		ctrlSearchBox.MoveWindow(rc);
		ctrlMode.MoveWindow(rc);
		ctrlPurge.MoveWindow(rc);
		ctrlSize.MoveWindow(rc);
		ctrlSizeMode.MoveWindow(rc);
		ctrlFiletype.MoveWindow(rc);
		
		ctrlCollapsed.MoveWindow(rc);
		ctrlSlots.MoveWindow(rc);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		ctrlStoreIP.MoveWindow(rc);
#endif
		ctrlStoreSettings.MoveWindow(rc);
#ifdef FLYLINKDC_USE_TREE_SEARCH
		ctrlUseGroupTreeSettings.MoveWindow(rc);
		ctrlSearchFilterTree.ShowWindow(SW_HIDE);
#endif
#ifdef FLYLINKDC_USE_TORRENT
		ctrlUseTorrentSearch.MoveWindow(rc);
		ctrlUseTorrentRSS.MoveWindow(rc);
#endif
		
		ctrlHubs.MoveWindow(rc);
		//  srLabel.MoveWindow(rc);
		//  ctrlFilterSel.MoveWindow(rc);
		//  ctrlFilter.MoveWindow(rc);
		ctrlDoSearch.MoveWindow(rc);
		typeLabel.MoveWindow(rc);
		hubsLabel.MoveWindow(rc);
		sizeLabel.MoveWindow(rc);
		searchLabel.MoveWindow(rc);
		optionLabel.MoveWindow(rc);
	}
	
	// "Search in results"
	CRect rc;
	rc.left = rect.left + lMargin;
	if (showUI)
		rc.left += width;
	rc.top = rect.bottom - searchInResultsHeight + bottomMargin + vertLabelOffset;
	rc.bottom = rc.top + labelH;
	rc.right = rc.left + WinUtil::getTextWidth(CTSTRING(SEARCH_IN_RESULTS), srLabel) + 10;
	srLabel.MoveWindow(rc);
	
	// Input field
	rc.top = rect.bottom - searchInResultsHeight + bottomMargin;
	rc.bottom = rc.top + controlHeight;
	rc.left = rc.right;
	rc.right = rc.left + 150;
	ctrlFilter.MoveWindow(rc);

	// Combo box
	rc.left = rc.right + lMargin;
	rc.right = rc.left + 120;
	ctrlFilterSel.MoveWindow(rc);
	
	// Icon
	rc.left = rc.right + 5;
	rc.right = rc.left + 16;
	rc.top += 3;
	rc.bottom = rc.top + 16;
	ctrlUDPMode.MoveWindow(rc);
	
	// Port status text
	rc.left += 19;
	rc.right = rect.right - rMargin;
	rc.top++;
	ctrlUDPTestResult.MoveWindow(rc);
	
	const POINT pt = {10, 10};
	const HWND hWnd = ctrlSearchBox.ChildWindowFromPoint(pt);
	if (hWnd != NULL && !ctrlSearch.IsWindow() && hWnd != ctrlSearchBox.m_hWnd)
	{
		ctrlSearch.Attach(hWnd);
		searchContainer.SubclassWindow(ctrlSearch.m_hWnd);
	}
	tooltip.Activate(TRUE);
}

void SearchFrame::runUserCommand(UserCommand & uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
		
	StringMap ucParams = ucLineParams;
	
	std::set<CID> users;
	
	int sel = -1;
	while ((sel = ctrlResults.GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		const SearchResult& sr = ctrlResults.getItemData(sel)->sr;
		
		if (!sr.getUser())
			continue;
		if (!sr.getUser()->isOnline())
			continue;
			
		if (uc.getType() == UserCommand::TYPE_RAW_ONCE)
		{
			if (users.find(sr.getUser()->getCID()) != users.end())
				continue;
			users.insert(sr.getUser()->getCID());
		}
		
		ucParams["fileFN"] = sr.getFile();
		ucParams["fileSI"] = Util::toString(sr.getSize());
		ucParams["fileSIshort"] = Util::formatBytes(sr.getSize());
		if (sr.getType() == SearchResult::TYPE_FILE)
		{
			ucParams["fileTR"] = sr.getTTH().toBase32();
		}
		
		// compatibility with 0.674 and earlier
		ucParams["file"] = ucParams["fileFN"];
		ucParams["filesize"] = ucParams["fileSI"];
		ucParams["filesizeshort"] = ucParams["fileSIshort"];
		ucParams["tth"] = ucParams["fileTR"];
		
		StringMap tmp = ucParams;
		ClientManager::userCommand(sr.getHintedUser(), uc, tmp, true);
	}
}

LRESULT SearchFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (hWnd == searchLabel.m_hWnd || hWnd == sizeLabel.m_hWnd || hWnd == optionLabel.m_hWnd || hWnd == typeLabel.m_hWnd
	        || hWnd == hubsLabel.m_hWnd || hWnd == ctrlSlots.m_hWnd ||
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	        hWnd == ctrlStoreIP.m_hWnd ||
#endif
	        hWnd == ctrlStoreSettings.m_hWnd ||
#ifdef FLYLINKDC_USE_TREE_SEARCH
	        hWnd == ctrlUseGroupTreeSettings.m_hWnd ||
#endif
#ifdef FLYLINKDC_USE_TORRENT
	        hWnd == ctrlUseTorrentSearch.m_hWnd ||
	        hWnd == ctrlUseTorrentRSS.m_hWnd ||
#endif
	        hWnd == ctrlCollapsed.m_hWnd || hWnd == srLabel.m_hWnd ||
			hWnd == ctrlUDPTestResult.m_hWnd || hWnd == ctrlUDPMode.m_hWnd)
	{
		::SetBkColor(hDC, ::GetSysColor(COLOR_3DFACE));
		::SetTextColor(hDC, ::GetSysColor(COLOR_BTNTEXT));
		return (LRESULT)::GetSysColorBrush(COLOR_3DFACE);
	}
	return Colors::setColor(hDC);
}

BOOL SearchFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (::TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	return IsDialogMessage(pMsg);
}

LRESULT SearchFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	
	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	cleanUcMenu(tabMenu);
	return TRUE;
}

LRESULT SearchFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::SEARCH, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT SearchFrame::onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlResults.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const SearchResult sr = ctrlResults.getItemData(i)->sr;
		if (sr.getType() == SearchResult::TYPE_FILE)
		{
			WinUtil::searchHash(sr.getTTH());
		}
	}
	return 0;
}

bool SearchFrame::isSkipSearchResult(SearchInfo* si)
{
	//dcassert(!closed);
	if (closed || startingSearch)
	{
		removeSearchInfo(si);
		return true;
	}
	return false;
}

#ifdef FLYLINKDC_USE_TREE_SEARCH
LRESULT SearchFrame::onSelChangedTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	if (closed) return 0;
	NMTREEVIEW* p = (NMTREEVIEW*)pnmh;
	treeItemCurrent = p->itemNew.hItem;
	updateSearchList();
	return 0;
}

bool SearchFrame::itemMatchesSelType(const SearchInfo* si) const
{
	if (treeItemCurrent == treeItemRoot || treeItemCurrent == nullptr)
		return true;
	bool result = false;
	if (!isTorrent(si) && si->sr.getType() == SearchResult::TYPE_FILE)
	{
		const auto fileExt = Text::toLower(Util::getFileExtWithoutDot(si->sr.getFileName()));
		auto it = groupedResults.find(treeItemCurrent);
		if (it == groupedResults.cend()) return false;
		const auto& data = it->second;
		return data.ext == fileExt;
	}
#ifdef FLYLINKDC_USE_TORRENT
	else if (si->m_is_torrent && si->sr.getType() == SearchResult::TYPE_TORRENT_MAGNET)
	{
		if (treeItemCurrent == typeNodes[FILE_TYPE_TORRENT_MAGNET])
			result = true;
		else
			result = false;
	}
#endif
	return result;
}

HTREEITEM SearchFrame::getInsertAfter(int type) const
{
	for (--type; type >= 0; --type)
		if (typeNodes[type]) return typeNodes[type];
	return TVI_FIRST;
}

static int getPrimaryFileType(const string& fileName, bool includeExtended) noexcept
{
	dcassert(!fileName.empty());
	if (fileName.empty())
		return FILE_TYPE_ANY;
	if (fileName.back() == PATH_SEPARATOR)
		return FILE_TYPE_DIRECTORY;

	unsigned mask = getFileTypesFromFileName(fileName);
	if (!mask) return FILE_TYPE_ANY;
	
	static const uint8_t fileTypesToCheck[] =
	{
		FILE_TYPE_COMICS, FILE_TYPE_EBOOK, FILE_TYPE_VIDEO, FILE_TYPE_AUDIO,
		FILE_TYPE_COMPRESSED, FILE_TYPE_DOCUMENT, FILE_TYPE_EXECUTABLE, FILE_TYPE_IMAGE,
		FILE_TYPE_CD_DVD
	};
	for (int i = includeExtended ? 0 : 2; i < _countof(fileTypesToCheck); i++)
	{
		int type = fileTypesToCheck[i];
		if (mask & 1<<type) return type;
	}
	return FILE_TYPE_ANY;
}
#endif // FLYLINKDC_USE_TREE_SEARCH

void SearchFrame::addSearchResult(SearchInfo* si)
{
	if (isSkipSearchResult(si))
		return;
	const SearchResult& sr = si->sr;
	SearchInfoList::ParentPair* pp = nullptr;
	if (!isTorrent(si))
	{
		const auto user = sr.getUser();
		if (sr.getIP())
			user->setIP(sr.getIP());
		// Check previous search results for dupes
		if (!si->getText(COLUMN_TTH).empty())
		{
			pp = ctrlResults.findParentPair(sr.getTTH());
			if (pp)
			{
				if (pp->parent && // fix https://drdump.com/DumpGroup.aspx?DumpGroupID=1052556
				    user->getCID() == pp->parent->getUser()->getCID() && sr.getFile() == pp->parent->sr.getFile())
				{
					removeSearchInfo(si);
					return;
				}
				for (auto k = pp->children.cbegin(); k != pp->children.cend(); ++k)
				{
					if (user->getCID() == (*k)->getUser()->getCID())
					{
						if (sr.getFile() == (*k)->sr.getFile())
						{
							removeSearchInfo(si);
							return;
						}
					}
				}
			}
		}
		else
		{
			for (auto s = ctrlResults.getParents().cbegin(); s != ctrlResults.getParents().cend(); ++s)
			{
				const SearchInfo* si2 = s->second.parent;
				const auto& sr2 = si2->sr;
				if (user->getCID() == sr2.getUser()->getCID())
				{
					if (sr.getFile() == sr2.getFile())
					{
						removeSearchInfo(si);
						return;
					}
				}
			}
		}
	}
	if (running || isTorrent(si))
	{
		resultsCount++;
#ifdef FLYLINKDC_USE_TREE_SEARCH
#ifdef FLYLINKDC_USE_TORRENT
		if (si->m_is_top_torrent)
		{
			if (!m_RootTorrentRSSTreeItem)
			{
				m_RootTorrentRSSTreeItem = ctrlSearchFilterTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
				                                                           _T("Torrent RSS"),
				                                                           0, // nImage
				                                                           0, // nSelectedImage
				                                                           0, // nState
				                                                           0, // nStateMask
				                                                           0, // lParam
				                                                           0, // aParent,
				                                                           TVI_LAST // hInsertAfter
				                                                           );
			}
			if (sr.m_group_name.empty())
			{
				m_24HTopTorrentTreeItem = add_category("...in 24 hours", "Hits", si, sr, SearchResult::TYPE_TORRENT_MAGNET, m_RootTorrentRSSTreeItem, true, true);
			}
			else
			{
				add_category(sr.m_group_name, "Categories", si, sr, SearchResult::TYPE_TORRENT_MAGNET, m_RootTorrentRSSTreeItem, true, true);
			}
			for (auto const &c : m_category_map)
			{
				auto& res = groupedResults[c.second];
				res.data.push_back(si);
				res.ext = ".torrent-magnet-top";
			}
			groupedResults[m_RootTorrentRSSTreeItem].data.push_back(si);
			groupedResults[m_RootTorrentRSSTreeItem].ext = ".torrent-magnet-top";
		}
		else
#endif
		{
			if (!treeItemRoot)
			{
				treeItemRoot = ctrlSearchFilterTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
				                                               _T("Search"),
				                                               0, // nImage
				                                               0, // nSelectedImage
				                                               0, // nState
				                                               0, // nStateMask
				                                               0, // lParam
				                                               0, // aParent,
				                                               TVI_ROOT // hInsertAfter
				                                               );
			}
			if (sr.getType() == SearchResult::TYPE_FILE || sr.getType() == SearchResult::TYPE_DIRECTORY)
			{
				const string& file = sr.getFileName();
				int fileType = sr.getType() == SearchResult::TYPE_DIRECTORY ? FILE_TYPE_DIRECTORY : getPrimaryFileType(file, true);
				auto& typeNode = typeNodes[fileType];
				if (!typeNode)
				{
					typeNode = ctrlSearchFilterTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
					                                           fileType == FILE_TYPE_ANY ? CTSTRING(OTHER) : CTSTRING_I(SearchManager::getTypeStr(fileType)),
					                                           fileType, // nImage
					                                           fileType, // nSelectedImage
					                                           0, // nState
					                                           0, // nStateMask
					                                           fileType, // lParam
					                                           treeItemRoot, // aParent,
					                                           getInsertAfter(fileType));
					if (!treeExpanded)
					{
						ctrlSearchFilterTree.Expand(treeItemRoot);
						treeExpanded = true;
					}
				}
				string fileExt;
				if (fileType != FILE_TYPE_DIRECTORY)
				{
					HTREEITEM fileExtNode;
					string fileExt = Text::toLower(Util::getFileExtWithoutDot(file));
					const auto extItem = extToTreeItem.find(fileExt);
					if (extItem == extToTreeItem.end())
					{
						fileExtNode = ctrlSearchFilterTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
							fileExt.empty() ? CTSTRING(SEARCH_NO_EXTENSION) : Text::toT(fileExt).c_str(),
							0, // nImage
							0, // nSelectedImage
							0, // nState
							0, // nStateMask
							0, // lParam
							typeNode, // aParent,
							TVI_SORT);
						extToTreeItem.insert(make_pair(fileExt, fileExtNode));
					}
					else
					{
						fileExtNode = extItem->second;
					}
					auto& res1 = groupedResults[fileExtNode];
					res1.data.push_back(si);
					res1.ext = fileExt;
				}
				auto& res2 = groupedResults[typeNode];
				res2.data.push_back(si);
				res2.ext = std::move(fileExt);
			}
#ifdef FLYLINKDC_USE_TORRENT
			else if (sr.getType() == SearchResult::TYPE_TORRENT_MAGNET)
			{
				const auto l_file_type = FILE_TYPE_TORRENT_MAGNET;
				auto& l_torrent_node = typeNodes[FILE_TYPE_TORRENT_MAGNET];
				if (l_torrent_node == nullptr)
				{
					l_torrent_node = ctrlSearchFilterTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
					                                                 _T("Torrents"),
					                                                 l_file_type, // nImage
					                                                 l_file_type, // nSelectedImage
					                                                 0, // nState
					                                                 0, // nStateMask
					                                                 l_file_type, // lParam
					                                                 treeItemRoot, // aParent,
					                                                 getInsertAfter(FILE_TYPE_TORRENT_MAGNET));
					if (!treeExpanded)
					{
						ctrlSearchFilterTree.Expand(treeItemRoot);
						treeExpanded = true;
					}
				}
				{
					const auto l_file_name = sr.getFileName();
					if (l_file_name.find(" PC ") != string::npos)
					{
						add_category("PC", "Soft", si, sr, l_file_type, l_torrent_node);
						add_category("DVD-ISO", "Soft", si, sr, l_file_type, l_torrent_node);
					}
					add_category(sr.m_tracker, "Tracker", si, sr, l_file_type, l_torrent_node, true);
					
					for (int j = 2020; j > 1900; --j)
					{
						add_category(Util::toString(j), "Year", si, sr, l_file_type, l_torrent_node);
					}
					{
						const char* l_doc_array[] = {
							"PDF",
							"RTF",
							"DJVU",
							"FB2"
						};
						for (const auto r : l_doc_array)
						{
							add_category(r, "Doc", si, sr, l_file_type, l_torrent_node);
						}
					}
					{
						const char* l_audio_rip_array[] = {
							"MP3",
							"FLAC"
						};
						for (const auto r : l_audio_rip_array)
						{
							add_category(r, "Audio", si, sr, l_file_type, l_torrent_node);
						}
					}
					
					{
						const char* l_type_rip_array[] = {
							"DVDRip",
							"DVD-Audio",
							"DVD-Video",
							"DVD-5",
							"DVD-9",
							"HDRip",
							"BDRip",
							"XviD",
							"BDRemux",
							"HDVideo",
							"HDTV",
							"CAMRip",
							"TS",
							"SATRip",
							"WEB-DLRip",
							"VHSRip",
							"iTunes",
							"Amedia",
							"КПК"
						};
						for (const auto r : l_type_rip_array)
						{
							add_category(r, "Video", si, sr, l_file_type, l_torrent_node);
						}
					}
					{
						const char* l_type_team_array[] = { "MediaClub",
						                                    "Files-x",
						                                    "GeneralFilm",
						                                    "MegaPeer",
						                                    "Neofilm",
						                                    "SeadLine Studio",
						                                    "Scarabey",
						                                    "den904",
						                                    "qqss44",
						                                    "HQ-ViDEO",
						                                    "SMALL-RiP",
						                                    "HQClub",
						                                    "HELLYWOOD",
						                                    "FreeHD",
						                                    "HDReactor",
						                                    "R.G."
						                                  };
						for (const auto t : l_type_team_array)
						{
							add_category(t, "Team", si, sr, l_file_type, l_torrent_node);
						}
					}
					{
						const char* l_type_resolution_array[] = {
							"1080p",
							"720p"
							"x264",
						};
						for (const auto r : l_type_resolution_array)
						{
							add_category(r, "Resolution", si, sr, l_file_type, l_torrent_node);
						}
					}
				}
				for (auto const &c : m_category_map)
				{
					auto& res = groupedResults[c.second];
					res.data.push_back(si);
					res.ext = ".torrent-magnet";
				}
				groupedResults[l_torrent_node].data.push_back(si);
				groupedResults[l_torrent_node].ext = ".torrent-magnet";
			}
#endif
		}
#endif
		{
			CLockRedraw<> lockRedraw(ctrlResults);
			if (isTorrent(si))
			{
#ifdef FLYLINKDC_USE_TREE_SEARCH
				if (itemMatchesSelType(si))
#endif
				{
					ctrlResults.insertItem(si, I_IMAGECALLBACK);
				}
			}
			else
			{
#ifndef FLYLINKDC_USE_TREE_SEARCH
				results.push_back(si);
#endif
				if (!si->getText(COLUMN_TTH).empty())
				{
#ifdef FLYLINKDC_USE_TREE_SEARCH
					if (itemMatchesSelType(si))
#endif
					{
						ctrlResults.insertGroupedItem(si, expandSR, false, true);
					}
				}
				else
				{
#ifdef FLYLINKDC_USE_TREE_SEARCH
					if (treeItemCurrent == treeItemRoot || treeItemCurrent == nullptr)
#endif
					{
						ctrlResults.insertItem(si, I_IMAGECALLBACK);
					}
				}
			}
		}
		if (!filter.empty())
		{
			updateSearchList(si);
		}
		if (BOOLSETTING(BOLD_SEARCH))
		{
			setDirty();
		}
		shouldSort = true;
	}
}

#ifdef FLYLINKDC_USE_TORRENT
HTREEITEM SearchFrame::add_category(const std::string p_search, const std::string p_group, SearchInfo* p_si,
                                    const SearchResult& p_sr, int p_type_node, HTREEITEM p_parent_node,
                                    bool p_force_add /* = false */, bool p_expand /*= false */)
{
	HTREEITEM l_item = nullptr;
	const string l_year = p_search;
	const auto l_file_name = p_sr.getFileName();
	if (l_file_name.find(l_year) != string::npos || p_force_add == true)
	{
		const auto l_sub_item = m_tree_sub_torrent_map.find(l_year);
		if (l_sub_item == m_tree_sub_torrent_map.end())
		{
			auto& l_group_node = m_category_map[p_group];
			if (!l_group_node)
			{
				l_group_node = ctrlSearchFilterTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
				                                               Text::toT(p_group).c_str(),
				                                               FILE_TYPE_TORRENT_MAGNET + 2, // nImage
				                                               FILE_TYPE_TORRENT_MAGNET + 2, // nSelectedImage
				                                               0, // nState
				                                               0, // nStateMask
				                                               p_type_node, // lParam
				                                               p_parent_node, // aParent,
				                                               0  // hInsertAfter
				                                              );
			}
			else
			{
				if (p_expand)
				{
					ctrlSearchFilterTree.Expand(l_group_node);
				}
			}
			l_item = ctrlSearchFilterTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			                                         Text::toT(l_year).c_str(),
			                                         FILE_TYPE_TORRENT_MAGNET + 1, // nImage
			                                         FILE_TYPE_TORRENT_MAGNET + 1, // nSelectedImage
			                                         0, // nState
			                                         0, // nStateMask
			                                         0, // lParam
			                                         l_group_node, // aParent,
			                                         0  // hInsertAfter
			                                        );
			m_tree_sub_torrent_map.insert(std::make_pair(l_year, l_item));
			if (!subTreeExpanded)
			{
				ctrlSearchFilterTree.Expand(p_parent_node);
				ctrlSearchFilterTree.Expand(l_group_node);
				subTreeExpanded = true;
			}
		}
		else
		{
			l_item = l_sub_item->second;
		}
		auto& res = groupedResults[l_item];
		res.data.push_back(p_si);
		res.ext = ".torrent-magnet";
	}
	return l_item;
}
#endif

LRESULT SearchFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	dcassert(!isClosedOrShutdown());
	switch (wParam)
	{
		case ADD_RESULT:
		{
			SearchInfo* si = reinterpret_cast<SearchInfo*>(lParam);
			if (isClosedOrShutdown())
			{
				delete si;
				return 0;
			}
			addSearchResult(si);
		}
		break;
#ifdef FLYLINKDC_USE_TORRENT
		case PREPARE_RESULT_TOP_TORRENT:
		{
			dcassert(BOOLSETTING(USE_TORRENT_RSS));
			SearchResult* sr = reinterpret_cast<SearchResult*>(lParam);
			if (isClosedOrShutdown())
			{
				delete sr;
				return 0;
			}
			auto ptr = new SearchInfo(*sr);
			delete sr;
			{
				LOCK(csEverything);
				everything.insert(ptr);
			}
			ptr->m_is_torrent = true;
			ptr->m_is_top_torrent = true;
			addSearchResult(ptr);
		}
		break;
		case PREPARE_RESULT_TORRENT:
		{
			dcassert(BOOLSETTING(USE_TORRENT_SEARCH));			
			SearchResult* sr = reinterpret_cast<SearchResult*>(lParam);
			if (isClosedOrShutdown())
			{
				delete sr;
				return 0;
			}
			auto ptr = new SearchInfo(*sr);
			delete sr;
			{
				LOCK(csEverything);
				everything.insert(ptr);
			}
			//ptr->m_torrent_page = z;
			ptr->m_is_torrent = true;
			addSearchResult(ptr);
		}
		break;
#endif
		case HUB_ADDED:
			onHubAdded((HubInfo*) lParam);
			break;
		case HUB_CHANGED:
			onHubChanged((HubInfo*) lParam);
			break;
		case HUB_REMOVED:
			onHubRemoved((HubInfo*) lParam);
			break;
	}
	
	return 0;
}

int SearchFrame::makeTargetMenu(const SearchInfo* si, OMenu& menu, int idc, ResourceManager::Strings title, ResourceManager::Strings prevFoldersTitle)
{
	menu.ClearMenu();
	dlTargets[idc + 0] = DownloadTarget(Util::emptyStringT, DownloadTarget::PATH_DEFAULT); // for 'Download' without options
	menu.InsertSeparatorFirst(TSTRING_I(title));
	
	int n = 1;
	{
		FavoriteManager::LockInstanceDirs lockedInstance;
		const auto& spl = lockedInstance.getFavoriteDirs();
		if (!spl.empty())
		{
			for (auto i = spl.cbegin(); i != spl.cend(); ++i)
			{
				tstring target = Text::toT(i->name);
				dlTargets[idc + n] = DownloadTarget(Text::toT(i->dir), DownloadTarget::PATH_FAVORITE);
				WinUtil::escapeMenu(target);
				menu.AppendMenu(MF_STRING, idc + n, target.c_str());
				n++;
			}
			menu.AppendMenu(MF_SEPARATOR);
		}
	}
	
	// !SMT!-S: Append special folder, like in source share
	if (si && !isTorrent(si))
	{
		tstring target = si->getText(COLUMN_PATH);
		if (target.length() > 2)
		{
			size_t start = target.substr(0, target.length() - 1).find_last_of(_T('\\'));
			if (start == tstring::npos) start = 0; else start++;
			target = Text::toT(SETTING(DOWNLOAD_DIRECTORY)) + target.substr(start);
			dlTargets[idc + n] = DownloadTarget(target, DownloadTarget::PATH_SRC);
			Util::removePathSeparator(target);
			WinUtil::escapeMenu(target);
			menu.AppendMenu(MF_STRING, idc + n, target.c_str());
			n++;
		}
	}
	
	dlTargets[idc + n] = DownloadTarget(Util::emptyStringT, DownloadTarget::PATH_BROWSE);
	menu.AppendMenu(MF_STRING, idc + n, CTSTRING(BROWSE));
	n++;
	
	//Append last favorite download dirs
	if (!LastDir::get().empty())
	{
		if (prevFoldersTitle)
			menu.InsertSeparatorLast(TSTRING_I(prevFoldersTitle));
		else
			menu.AppendMenu(MF_SEPARATOR);
		for (auto i = LastDir::get().cbegin(); i != LastDir::get().cend(); ++i)
		{
			tstring target = *i;
			dlTargets[idc + n] = DownloadTarget(target, DownloadTarget::PATH_LAST);
			Util::removePathSeparator(target);
			WinUtil::escapeMenu(target);
			menu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TO_FAV + n, target.c_str());
			n++;
		}
	}
	return n;
}

LRESULT SearchFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlResults && ctrlResults.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlResults, pt);
		}
		const auto selCount = ctrlResults.GetSelectedCount();
#ifdef FLYLINKDC_USE_TORRENT
		if (selCount == 1)
		{
			int i = ctrlResults.GetNextItem(-1, LVNI_SELECTED);
			dcassert(i != -1);
			const SearchInfo* si = si = ctrlResults.getItemData(i);
			if (si->m_is_torrent)
			{
				OMenu resultsMenu;
				resultsMenu.CreatePopupMenu();
				resultsMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TO_FAV, CTSTRING(DOWNLOAD));
				resultsMenu.AppendMenu(MF_POPUP, (HMENU)copyMenuTorrent, CTSTRING(COPY));
				resultsMenu.AppendMenu(MF_SEPARATOR);
				dlTargets.clear();
				makeTargetMenu(si, targetMenu, IDC_DOWNLOAD_TO_FAV, ResourceManager::DOWNLOAD_TO, ResourceManager::PREVIOUS_FOLDERS);
				resultsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
				return TRUE;
			}
		}
#endif
		if (selCount)
		{
			clearUserMenu();
			OMenu resultsMenu;
			
			resultsMenu.CreatePopupMenu();
			
			resultsMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TO_FAV, CTSTRING(DOWNLOAD), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD_QUEUE, 0));
			resultsMenu.AppendMenu(MF_POPUP, (HMENU)targetMenu, CTSTRING(DOWNLOAD_TO));
			resultsMenu.AppendMenu(MF_POPUP, (HMENU)priorityMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
#ifdef USE_DOWNLOAD_DIR
			resultsMenu.AppendMenu(MF_STRING, IDC_DOWNLOADDIR_TO_FAV, CTSTRING(DOWNLOAD_WHOLE_DIR));
			resultsMenu.AppendMenu(MF_POPUP, (HMENU)targetDirMenu, CTSTRING(DOWNLOAD_WHOLE_DIR_TO));
#endif
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
			resultsMenu.AppendMenu(MF_SEPARATOR);
			resultsMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));
#endif
			resultsMenu.AppendMenu(MF_SEPARATOR);
			resultsMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
			
			resultsMenu.AppendMenu(MF_POPUP, (HMENU)copyMenu, CTSTRING(COPY));
			resultsMenu.AppendMenu(MF_SEPARATOR);
			appendAndActivateUserItems(resultsMenu, true);
			resultsMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
			resultsMenu.SetMenuDefaultItem(IDC_DOWNLOAD_TO_FAV);
			
			const SearchInfo* si = nullptr;
			const SearchResult* sr = nullptr;
			if (ctrlResults.GetSelectedCount() == 1)
			{
				int i = ctrlResults.GetNextItem(-1, LVNI_SELECTED);
				dcassert(i != -1);
				si = ctrlResults.getItemData(i);
				sr = &si->sr;
			}

			// Add target menu
			dlTargets.clear();
			int n = makeTargetMenu(si, targetMenu, IDC_DOWNLOAD_TO_FAV, ResourceManager::DOWNLOAD_TO, ResourceManager::PREVIOUS_FOLDERS);
			
			const SearchInfo::CheckTTH SIcheck = ctrlResults.forEachSelectedT(SearchInfo::CheckTTH());
			
			if (SIcheck.hasTTH)
			{
				targets.clear();
				QueueManager::getTargets(TTHValue(Text::fromT(SIcheck.tth)), targets);
				
				if (!targets.empty())
				{
					targetMenu.InsertSeparatorLast(TSTRING(ADD_AS_SOURCE));
					for (auto i = targets.cbegin(); i != targets.cend(); ++i)
					{
						tstring target = Text::toT(*i);
						dlTargets[IDC_DOWNLOAD_TARGET + n] = DownloadTarget(target, DownloadTarget::PATH_DEFAULT);
						WinUtil::escapeMenu(target);
						targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET + n, target.c_str());
						n++;
					}
				}
			}
			
#ifdef USE_DOWNLOAD_DIR
			// second sub-menu
			makeTargetMenu(si, targetDirMenu, IDC_DOWNLOADDIR_TO_FAV, ResourceManager::DOWNLOAD_WHOLE_DIR_TO, ResourceManager::Strings());
#endif

			if (sr && sr->getType() == SearchResult::TYPE_FILE)
			{
				copyMenu.RenameItem(IDC_COPY_FILENAME, TSTRING(FILENAME));
			}
			else if (sr && sr->getType() == SearchResult::TYPE_DIRECTORY)
			{
				copyMenu.RenameItem(IDC_COPY_FILENAME, TSTRING(FOLDERNAME));
			}
			appendUcMenu(resultsMenu, UserCommand::CONTEXT_SEARCH, SIcheck.hubs);
			
			copyMenu.InsertSeparatorFirst(TSTRING(USERINFO));
			tstring caption;
			if (sr && !sr->getFileName().empty())
			{
				caption = Text::toT(sr->getFileName());
				Util::removePathSeparator(caption);
				Text::limitStringLength(caption);
			} else caption = TSTRING(FILES);
			resultsMenu.InsertSeparatorFirst(caption);
			resultsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			resultsMenu.RemoveFirstItem();
			copyMenu.RemoveFirstItem();
			
			//cleanMenu(resultsMenu);
			return TRUE;
		}
	}
	bHandled = FALSE;
	return FALSE;
}

void SearchFrame::initHubs()
{
	useDHT = dht::DHT::getInstance()->isConnected();
	CLockRedraw<> lockRedraw(ctrlHubs);
		
	ctrlHubs.insertItem(new HubInfo(Util::emptyString, TSTRING(ONLY_WHERE_OP), false), 0);
	ctrlHubs.SetCheckState(0, false);
	ClientManager::getInstance()->addListener(this);
	ClientManager::HubInfoArray hubInfo;
	ClientManager::getConnectedHubInfo(hubInfo);
	if (useDHT)
		onHubAdded(new HubInfo(dht::NetworkName, TSTRING(DHT), false));
	for (const auto& hub : hubInfo)
		onHubAdded(new HubInfo(hub.url, Text::toT(hub.name), hub.isOp));
}

void SearchFrame::updateWaitingTime()
{
	unsigned elapsed = unsigned(GET_TICK() - searchStartTime);
	hasWaitTime = false;
	int count = ctrlHubs.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		HubInfo* info = ctrlHubs.getItemData(i);	
		bool updated = false;
		for (const auto& item : searchClients)
		{
			if (info->url == item.url)
			{
				if (elapsed < item.waitTime)
				{
					info->waitTime = Util::formatSecondsT((item.waitTime-elapsed)/1000, true);
					updated = true;
					hasWaitTime = true;
				}
				break;
			}
		}
		if (!updated) info->waitTime.clear();
		ctrlHubs.updateItem(i, 1);
	}
	ctrlHubs.RedrawWindow();
}

void SearchFrame::onHubAdded(HubInfo* info)
{
	int nItem = ctrlHubs.insertItem(info, 0);
	if (nItem >= 0)
		ctrlHubs.SetCheckState(nItem, (ctrlHubs.GetCheckState(0) ? info->isOp : true));
}

void SearchFrame::onHubChanged(HubInfo* info)
{
	int nItem = 0;
	const int n = ctrlHubs.GetItemCount();
	for (; nItem < n; nItem++)
	{
		if (ctrlHubs.getItemData(nItem)->url == info->url)
			break;
	}
	if (nItem == n)
	{
		delete info;
		return;
	}
			
	delete ctrlHubs.getItemData(nItem);
	ctrlHubs.SetItemData(nItem, (DWORD_PTR)info);
	ctrlHubs.updateItem(nItem);
	
	if (ctrlHubs.GetCheckState(0))
		ctrlHubs.SetCheckState(nItem, info->isOp);
}

void SearchFrame::onHubRemoved(HubInfo* info)
{
	if (isClosedOrShutdown())
		return;
	int nItem = 0;
	const int n = ctrlHubs.GetItemCount();
	for (; nItem < n; nItem++)
	{
		if (ctrlHubs.getItemData(nItem)->url == info->url)
			break;
	}
		
	delete info;
		
	if (nItem == n)
		return;
			
	delete ctrlHubs.getItemData(nItem);
	ctrlHubs.DeleteItem(nItem);
}

LRESULT SearchFrame::onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlResults.forEachSelected(&SearchInfo::getList);
	return 0;
}

LRESULT SearchFrame::onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlResults.forEachSelected(&SearchInfo::browseList);
	return 0;
}

LRESULT SearchFrame::onItemChangedHub(int /* idCtrl */, LPNMHDR pnmh, BOOL& /* bHandled */)
{
	const NMLISTVIEW* lv = (NMLISTVIEW*)pnmh;
	if (lv->iItem == 0 && (lv->uNewState ^ lv->uOldState) & LVIS_STATEIMAGEMASK)
	{
		if (((lv->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1)
		{
			for (int iItem = 0; (iItem = ctrlHubs.GetNextItem(iItem, LVNI_ALL)) != -1;)
			{
				const HubInfo* client = ctrlHubs.getItemData(iItem);
				if (!client->isOp)
					ctrlHubs.SetCheckState(iItem, false);
			}
		}
	}
	return 0;
}

LRESULT SearchFrame::onPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (MessageBox(CTSTRING(CONFIRM_CLEAR_SEARCH), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
	tooltip.Activate(FALSE);
	ctrlSearchBox.ResetContent();
	g_lastSearches.clear();
	DatabaseManager::getInstance()->clearRegistry(e_SearchHistory, 0);
	MainFrame::getMainFrame()->updateQuickSearches(true);
	tooltip.Activate(TRUE);
	return 0;
}

LRESULT SearchFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string data;
	int i = -1;
	while ((i = ctrlResults.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const SearchInfo* si = ctrlResults.getItemData(i);
		const SearchResult &sr = si->sr;
		string sCopy;
		switch (wID)
		{
			case IDC_COPY_NICK:
				sCopy = sr.getUser()->getLastNick();
				break;
			case IDC_COPY_FILENAME:
				if (sr.getType() == SearchResult::TYPE_FILE || sr.getType() == SearchResult::TYPE_TORRENT_MAGNET)
					sCopy = Util::getFileName(sr.getFile());
				else
					sCopy = Util::getLastDir(sr.getFile());
				break;
			case IDC_COPY_PATH:
				sCopy = Util::getFilePath(sr.getFile());
				break;
			case IDC_COPY_SIZE:
				sCopy = Util::formatBytes(sr.getSize());
				break;
			case IDC_COPY_HUB_URL:
				sCopy = Util::formatDchubUrl(sr.getHubUrl());
				break;
			case IDC_COPY_LINK:
			case IDC_COPY_FULL_MAGNET_LINK:
				if (sr.getType() == SearchResult::TYPE_FILE)
					sCopy = Util::getMagnet(sr.getTTH(), sr.getFileName(), sr.getSize());
				if (wID == IDC_COPY_FULL_MAGNET_LINK && !sr.getHubUrl().empty())
					sCopy += "&xs=" + Util::formatDchubUrl(sr.getHubUrl());
				break;
			case IDC_COPY_WMLINK:
				if (sr.getType() == SearchResult::TYPE_FILE)
					sCopy = Util::getWebMagnet(sr.getTTH(), sr.getFileName(), sr.getSize());
				break;
			case IDC_COPY_TTH:
				if (sr.getType() == SearchResult::TYPE_FILE)
					sCopy = sr.getTTH().toBase32();
#ifdef FLYLINKDC_USE_TORRENT
				else if (sr.getType() == SearchResult::TYPE_TORRENT_MAGNET)
					sCopy = sr.getTorrentMagnet();
#endif
				break;
#ifdef FLYLINKDC_USE_TORRENT
			case IDC_COPY_TORRENT_DATE:
				sCopy = sr.m_date;
				break;
			case IDC_COPY_TORRENT_COMMENT:
				sCopy = Util::toString(sr.m_comment);
				break;
			case IDC_COPY_TORRENT_URL:
				sCopy = sr.torrentUrl;
				break;
			case IDC_COPY_TORRENT_PAGE:
				sCopy = Util::toString(sr.m_torrent_page);
				break;
#endif
			default:
				dcdebug("SEARCHFRAME DON'T GO HERE\n");
				return 0;
		}
		if (!sCopy.empty())
		{
			if (data.empty())
				data = sCopy;
			else
				data = data + "\r\n" + sCopy;
		}
	}
	if (!data.empty())
	{
		WinUtil::setClipboard(data);
	}
	return 0;
}

static inline void getFileItemColor(int flags, COLORREF& fg, COLORREF& bg)
{
	static const COLORREF colorShared = RGB(114,219,139);
	static const COLORREF colorDownloaded = RGB(145,194,196);
	static const COLORREF colorCanceled = RGB(210,168,211);
	static const COLORREF colorInQueue = RGB(186,0,42);
	fg = RGB(0,0,0);
	bg = RGB(255,255,255);
	if (flags & SearchResult::FLAG_SHARED)
		bg = colorShared; else
	if (flags & SearchResult::FLAG_DOWNLOADED)
		bg = colorDownloaded; else
	if (flags & SearchResult::FLAG_DOWNLOAD_CANCELED)
		bg = colorCanceled;
	if (flags & SearchResult::FLAG_QUEUED)
		fg = colorInQueue;
}

LRESULT SearchFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			CustomDrawHelpers::startDraw(customDrawState, cd);
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			cd->clrText = Colors::g_textColor;
			cd->clrTextBk = Colors::g_bgColor;
			SearchInfo* si = reinterpret_cast<SearchInfo*>(cd->nmcd.lItemlParam);
			if (si)
			{
				si->calcImageIndex();
				if (!isTorrent(si))
				{
					si->sr.checkTTH();
					getFileItemColor(si->sr.flags, cd->clrText, cd->clrTextBk);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
					if (!si->ipUpdated && storeIP && si->getUser())
					{
						Ip4Address ip = si->sr.getIP();
						if (ip)
						{
							si->ipUpdated = true;
							si->getUser()->setIP(ip);
						}
					}
#endif
				}
				customDrawState.indent = si->parent ? 1 : 0;
			}
			CustomDrawHelpers::startItemDraw(customDrawState, cd);
			if (hTheme) CustomDrawHelpers::drawBackground(hTheme, customDrawState, cd);
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		}

		case CDDS_ITEMPOSTPAINT:
			CustomDrawHelpers::drawFocusRect(customDrawState, cd);
			return CDRF_SKIPDEFAULT;

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			SearchInfo* si = reinterpret_cast<SearchInfo*>(cd->nmcd.lItemlParam);
			if (!si)
				return CDRF_DODEFAULT;
			int column = ctrlResults.findColumn(cd->iSubItem);
#ifdef FLYLINKDC_USE_TORRENT
			if (!si->m_is_torrent)
#endif
			{
				if (column == COLUMN_P2P_GUARD)
				{
					si->sr.loadP2PGuard();
					si->colMask &= ~(1<<COLUMN_P2P_GUARD);
					const string& p2pGuard = si->sr.getIpInfo().p2pGuard;
					tstring text = Text::toT(p2pGuard);
					CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, &g_userStateImage.getIconList(), text.empty() ? -1 : 3, text, false);
					return CDRF_SKIPDEFAULT;
				}
				else if (column == COLUMN_LOCATION)
				{
					si->sr.loadLocation();
					si->colMask &= ~(1<<COLUMN_LOCATION);
					const auto& ipInfo = si->sr.getIpInfo();
					CustomDrawHelpers::drawLocation(customDrawState, cd, ipInfo);
					return CDRF_SKIPDEFAULT;
				}
			}
#ifdef FLYLINKDC_USE_TORRENT
			else
			{
				if (column == COLUMN_TTH)
				{
					CRect rc;
					ctrlResults.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
					const tstring l_value = si->getText(column);
					if (!l_value.empty())
					{
						LONG top = rc.top + (rc.Height() - 15) / 2;
						if ((top - rc.top) < 2)
							top = rc.top + 1;
						const POINT ps = { rc.left, top };
						if (si->sr.m_tracker_index >= 0)
						{
							g_trackerImage.Draw(cd->nmcd.hdc, si->sr.m_tracker_index, ps);
						}
						::ExtTextOut(cd->nmcd.hdc, rc.left + 6 + 17, rc.top + 2, ETO_CLIPPED, rc, l_value.c_str(), l_value.length(), NULL);
					}
					return CDRF_SKIPDEFAULT;
				}
			}
#endif	
			if (cd->iSubItem == 0)
				CustomDrawHelpers::drawFirstSubItem(customDrawState, cd, si->getText(column));
			else
				CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, nullptr, -1, si->getText(column), false);
			return CDRF_SKIPDEFAULT;
		}
		default:
			return CDRF_DODEFAULT;
	}
//	return CDRF_DODEFAULT; /// all case have a return, this return never run.
}

LRESULT SearchFrame::onFilterChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!BOOLSETTING(FILTER_ENTER) || wParam == VK_RETURN)
	{
		WinUtil::getWindowText(ctrlFilter, filter);
		Text::makeLower(filter);
		updateSearchList();
	}
	bHandled = false;
	return 0;
}

bool SearchFrame::parseFilter(FilterModes& mode, int64_t& size)
{
	tstring::size_type start = tstring::npos;
	tstring::size_type end = tstring::npos;
	int64_t multiplier = 1;
	
	if (filter.compare(0, 2, _T(">="), 2) == 0)
	{
		mode = GREATER_EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, _T("<="), 2) == 0)
	{
		mode = LESS_EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, _T("=="), 2) == 0)
	{
		mode = EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, _T("!="), 2) == 0)
	{
		mode = NOT_EQUAL;
		start = 2;
	}
	else if (filter[0] == _T('<'))
	{
		mode = LESS;
		start = 1;
	}
	else if (filter[0] == _T('>'))
	{
		mode = GREATER;
		start = 1;
	}
	else if (filter[0] == _T('='))
	{
		mode = EQUAL;
		start = 1;
	}
	
	if (start == tstring::npos)
		return false;
	if (filter.length() <= start)
		return false;
		
	if ((end = filter.find(_T("tib"))) != tstring::npos)
		multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
	else if ((end = filter.find(_T("gib"))) != tstring::npos)
		multiplier = 1024 * 1024 * 1024;
	else if ((end = filter.find(_T("mib"))) != tstring::npos)
		multiplier = 1024 * 1024;
	else if ((end = filter.find(_T("kib"))) != tstring::npos)
		multiplier = 1024;
	else if ((end = filter.find(_T("tb"))) != tstring::npos)
		multiplier = 1000LL * 1000LL * 1000LL * 1000LL;
	else if ((end = filter.find(_T("gb"))) != tstring::npos)
		multiplier = 1000 * 1000 * 1000;
	else if ((end = filter.find(_T("mb"))) != tstring::npos)
		multiplier = 1000 * 1000;
	else if ((end = filter.find(_T("kb"))) != tstring::npos)
		multiplier = 1000;
	else if ((end = filter.find(_T('b'))) != tstring::npos)
		multiplier = 1;
	
	if (end == tstring::npos)
	{
		end = filter.length();
	}
	
	const tstring tmpSize = filter.substr(start, end - start);
	size = static_cast<int64_t>(Util::toDouble(Text::fromT(tmpSize)) * multiplier);
	
	return true;
}

bool SearchFrame::matchFilter(const SearchInfo* si, int sel, bool doSizeCompare, FilterModes mode, int64_t size)
{
	if (filter.empty()) return true;
	if (doSizeCompare)
	{
		bool insert = true;
		switch (mode)
		{
			case EQUAL:
				insert = (size == si->sr.getSize());
				break;
			case GREATER_EQUAL:
				insert = (size <= si->sr.getSize());
				break;
			case LESS_EQUAL:
				insert = (size >= si->sr.getSize());
				break;
			case GREATER:
				insert = (size < si->sr.getSize());
				break;
			case LESS:
				insert = (size > si->sr.getSize());
				break;
			case NOT_EQUAL:
				insert = (size != si->sr.getSize());
				break;
		}
		return insert;
	}
	tstring s = si->getText(static_cast<uint8_t>(sel));
	Text::makeLower(s);
	return s.find(filter) != tstring::npos;
}

void SearchFrame::clearFound()
{
#ifdef FLYLINKDC_USE_TREE_SEARCH
	extToTreeItem.clear();
#ifdef FLYLINKDC_USE_TORRENT
	m_category_map.clear();
	m_tree_sub_torrent_map.clear();
#endif
	groupedResults.clear();
	for (int i = 0; i < _countof(typeNodes); i++)
		typeNodes[i] = nullptr;
	treeItemCurrent = nullptr;
	treeItemOld = nullptr;
	treeItemRoot = nullptr;
#ifdef FLYLINKDC_USE_TORRENT
	m_RootTorrentRSSTreeItem = nullptr;
	m_24HTopTorrentTreeItem = nullptr;
	subTreeExpanded = false;
#endif
	treeExpanded = false;
	ctrlSearchFilterTree.DeleteAllItems();
#else
	results.clear();
#endif
	{
		LOCK(csEverything);
		for (auto i = everything.begin(); i != everything.end(); ++i)
		{
			delete *i;
		}
		everything.clear();
	}
}

void SearchFrame::removeSearchInfo(SearchInfo* si)
{	
	{
		LOCK(csEverything);
		everything.erase(si);
	}
	delete si;
}

void SearchFrame::insertStoredResults(HTREEITEM treeItem, int filter, bool doSizeCompare, FilterModes mode, int64_t size)
{
#ifdef FLYLINKDC_USE_TREE_SEARCH
	auto it = groupedResults.find(treeItem);
	if (it == groupedResults.cend()) return;
	auto& data = it->second.data;
#else
	auto& data = results;
#endif
	for (SearchInfo* si : data)
		if (matchFilter(si, filter, doSizeCompare, mode, size))
		{
			si->parent = nullptr;
			si->collapsed = true;
			si->hits = 0;
			if (si->sr.getType() == SearchResult::TYPE_FILE)
				ctrlResults.insertGroupedItem(si, expandSR, false, true);
			else
				ctrlResults.insertItem(si, I_IMAGECALLBACK);
		}
}

void SearchFrame::updateSearchList(SearchInfo* si)
{
	dcassert(!closed);
	int64_t size = -1;
	FilterModes mode = NONE;
	int sel = ctrlFilterSel.GetCurSel();
	if (sel != -1) sel = columnId[sel];
	bool doSizeCompare = sel == COLUMN_SIZE && parseFilter(mode, size);
	if (si)
	{
		if (!matchFilter(si, sel, doSizeCompare, mode, size))
			ctrlResults.deleteItem(si);
	}
	else
	{
		CLockRedraw<> lockRedraw(ctrlResults);
		ctrlResults.deleteAll();
#ifdef FLYLINKDC_USE_TREE_SEARCH
		if (treeItemCurrent == nullptr || treeItemCurrent == treeItemRoot)
		{
			for (int i = 0; i < _countof(typeNodes); ++i)
				if (typeNodes[i])
					insertStoredResults(typeNodes[i], sel, doSizeCompare, mode, size);
		}
		else
			insertStoredResults(treeItemCurrent, sel, doSizeCompare, mode, size);
#else
		insertStoredResults(nullptr, sel, doSizeCompare, mode, size);
#endif
		ctrlResults.resort();
		shouldSort = false;
	}
}

LRESULT SearchFrame::onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	WinUtil::getWindowText(ctrlFilter, filter);
	updateSearchList();
	bHandled = FALSE;
	return 0;
}

LRESULT SearchFrame::onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring searchString;
	WinUtil::getWindowText(ctrlSearchBox, searchString);
	if (isTTH(searchString))
	{
		ctrlFiletype.SetCurSel(ctrlFiletype.FindStringExact(0, _T("TTH")));
		autoSwitchToTTH = true;
	}
	else
	{
		if (!searchString.empty())
		{
			if (Util::isMagnetLink(searchString))
			{
				const tstring::size_type xt = searchString.find(L"xt=urn:tree:tiger:");
				if (xt != tstring::npos && xt + 18 + 39 <= searchString.size())
				{
					searchString = searchString.substr(xt + 18, 39);
					if (isTTH(searchString))
					{
						ctrlSearchBox.SetWindowText(searchString.c_str());
						ctrlFiletype.SetCurSel(ctrlFiletype.FindStringExact(0, _T("TTH")));
						autoSwitchToTTH = true;
						return 0;
					}
				}
			}
		}
		if (autoSwitchToTTH)
		{
			//В диалоге поиска очень хорошо сделано, что при вставке хэша автоматически подставляется тип файла TTH. Но если после этого попробовать
			//найти по имени, то приходится убирать TTH вручную --> соответственно требуется при несовпадении с шаблоном TTH ставить обратно "Любой".
			ctrlFiletype.SetCurSel(ctrlFiletype.FindStringExact(0, CTSTRING(ANY)));
			autoSwitchToTTH = false;
		}
	}
	return 0;
}

void SearchFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlResults.isRedraw())
		{
			ctrlHubs.SetBkColor(Colors::g_bgColor);
			ctrlHubs.SetTextBkColor(Colors::g_bgColor);
			ctrlHubs.SetTextColor(Colors::g_textColor);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

void SearchFrame::speak(Speakers s, const Client* c)
{
	if (!isClosedOrShutdown())
	{
		HubInfo * hubInfo = new HubInfo(c->getHubUrl(), Text::toT(c->getHubName()), c->getMyIdentity().isOp());
		safe_post_message(*this, s, hubInfo);
	}
}

void SearchFrame::updateResultCount()
{
	ctrlStatus.SetText(3, (Util::toStringT(resultsCount) + _T(' ') + TSTRING(FILES)).c_str());
	ctrlStatus.SetText(4, (Util::toStringT(droppedResults) + _T(' ') + TSTRING(FILTERED)).c_str());
	needUpdateResultCount = false;
	#if 0 // not used
	setCountMessages(resultsCount);
	#endif
}

void SearchFrame::updateStatusLine(uint64_t tick)
{
	auto delta = searchEndTime - searchStartTime;
	uint64_t percent = delta ? (tick - searchStartTime) * 100 / delta : 0;
	bool searchDone = percent >= 100;
	if (searchDone)
	{
		statusLine = searchTarget + _T(" - ") + TSTRING(DONE);
		ctrlStatus.SetText(2, CTSTRING(DONE));
	}
	else
	{
		statusLine = TSTRING(SEARCHING_FOR) + _T(' ') + searchTarget + _T(" ... ") + Util::toStringT(percent) + _T("%");
		ctrlStatus.SetText(2, Util::formatSecondsT((searchEndTime - tick) / 1000).c_str());
	}
	SetWindowText(statusLine.c_str());
	::InvalidateRect(m_hWndStatusBar, NULL, TRUE);
}

void SearchFrame::showPortStatus()
{
	if (g_DisableTestPort) return;
	string reflectedAddress;
	int port;
	int state = g_portTest.getState(PortTest::PORT_UDP, port, &reflectedAddress);
	if (state == portStatus && currentReflectedAddress == reflectedAddress) return;
	tstring newText;
	switch (state)
	{
		case PortTest::STATE_RUNNING:
			ctrlUDPMode.SetIcon(iconUdpWait);
			newText = CTSTRING(UDP_PORT_TEST_WAIT);
			break;

		case PortTest::STATE_FAILURE:
			ctrlUDPMode.SetIcon(iconUdpFail);
			newText = CTSTRING(UDP_PORT_TEST_FAILED);
			break;

		case PortTest::STATE_SUCCESS:
			ctrlUDPMode.SetIcon(iconUdpOk);
			newText = CTSTRING(UDP_PORT_TEST_OK);
			if (!reflectedAddress.empty())
			{
				newText += _T(" (");
				newText += Text::toT(reflectedAddress);
				newText += _T(")");
			}
			break;

		default:
			return;
	}
	portStatus = state;
	currentReflectedAddress = std::move(reflectedAddress);
	if (newText != portStatusText)
	{
		portStatusText = std::move(newText);
		ctrlUDPTestResult.SetWindowText(portStatusText.c_str());
	}
	ClientManager::infoUpdated(true);
}

const tstring& SearchFrame::HubInfo::getText(int col) const
{
	if (col == 0) return name;
	if (col == 1) return waitTime;
	return Util::emptyStringT;
}

int SearchFrame::HubInfo::compareItems(const HubInfo* a, const HubInfo* b, int col)
{
	int aType = a->getType();
	int bType = b->getType();
	if (aType != bType) return aType - bType;
	return Util::defaultSort(a->name, b->name);
}

int SearchFrame::HubInfo::getType() const
{
	if (url.empty()) return 0;
	return url == dht::NetworkName ? 1 : 2;
}

CFrameWndClassInfo& SearchFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("SearchFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::SEARCH, 0);

	return wc;
}
