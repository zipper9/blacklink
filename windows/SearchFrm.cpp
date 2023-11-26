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

#include "SearchFrm.h"
#include "MainFrm.h"
#include "WinUtil.h"
#include "Colors.h"
#include "Fonts.h"
#include "BarShader.h"
#include "ImageLists.h"
#include "ExMessageBox.h"
#include "BrowseFile.h"
#include "LockRedraw.h"

#include "../client/Client.h"
#include "../client/QueueManager.h"
#include "../client/ClientManager.h"
#include "../client/ShareManager.h"
#include "../client/DownloadManager.h"
#include "../client/StringTokenizer.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include "../client/FormatUtil.h"
#include "../client/FileTypes.h"
#include "../client/PortTest.h"
#include "../client/SysVersion.h"
#include "../client/dht/DHT.h"

#define USE_DOWNLOAD_DIR

SearchHistory SearchFrame::lastSearches;
SearchFrame::FrameMap SearchFrame::activeFrames;
CriticalSection SearchFrame::framesLock;

static const unsigned SEARCH_RESULTS_WAIT_TIME = 10000;

extern bool g_DisableTestPort;

using DialogLayout::FLAG_HWND;
using DialogLayout::FLAG_PLACEHOLDER;
using DialogLayout::AUTO;
using DialogLayout::UNSPEC;
using DialogLayout::INDEX_RELATIVE;

static const DialogLayout::Align hal1 = { 0, DialogLayout::SIDE_LEFT, U_DU(4) };
static const DialogLayout::Align hal2 = { 0, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align val1 = { 0, DialogLayout::SIDE_TOP, U_DU(2) };
static const DialogLayout::Align hal3 = { 1 | INDEX_RELATIVE, DialogLayout::SIDE_LEFT, U_DU(1) };
static const DialogLayout::Align hal4 = { 0, DialogLayout::SIDE_LEFT, U_DU(8) };
static const DialogLayout::Align val2 = { 1, DialogLayout::SIDE_BOTTOM, U_PX(2) };
static const DialogLayout::Align val3 = { 4, DialogLayout::SIDE_BOTTOM, U_DU(6) };
static const DialogLayout::Align val4 = { 5, DialogLayout::SIDE_BOTTOM, U_PX(2) };
static const DialogLayout::Align hal5 = { 1 | INDEX_RELATIVE, DialogLayout::SIDE_LEFT, U_DU(2) };
static const DialogLayout::Align val5 = { 8, DialogLayout::SIDE_BOTTOM, U_DU(6) };
static const DialogLayout::Align val6 = { 1 | INDEX_RELATIVE, DialogLayout::SIDE_BOTTOM, U_PX(2) };
static const DialogLayout::Align val7 = { 10, DialogLayout::SIDE_BOTTOM, U_DU(6) };
static const DialogLayout::Align val8 = { 11, DialogLayout::SIDE_BOTTOM, U_DU(2) };
static const DialogLayout::Align val9 = { 1, DialogLayout::SIDE_BOTTOM, U_PX(1) };
static const DialogLayout::Align val10 = { 1 | INDEX_RELATIVE, DialogLayout::SIDE_BOTTOM, U_DU(2) };
static const DialogLayout::Align val11 = { 16, DialogLayout::SIDE_BOTTOM, U_DU(6) };
static const DialogLayout::Align val12 = { 0, DialogLayout::SIDE_BOTTOM, U_PX(0) };

static const DialogLayout::Item layoutItems[] =
{
	{ 0, FLAG_HWND, UNSPEC, AUTO, 0, &hal1, &hal2, &val1 }, // 1. "Search for"
	{ 0, FLAG_HWND, U_PX(30), U_PX(25), 0, nullptr, &hal2, &val9 }, // 2. Clear button
	{ 0, FLAG_HWND, U_PX(30), U_PX(25), 0, nullptr, &hal3, &val9 }, // 3. Search button
	{ 0, FLAG_HWND, UNSPEC, U_DU(12), 0, &hal1, &hal3, &val2 }, // 4. Search box
	{ 0, FLAG_HWND, UNSPEC, AUTO, 0, &hal1, &hal2, &val3 }, // 5. "Size"
	{ 0, FLAG_HWND, U_DU(30), U_DU(12), 0, nullptr, &hal2, &val4 }, // 6. Size units
	{ 0, FLAG_HWND, U_DU(40), U_DU(12), 0, nullptr, &hal5, &val4}, // 7. Size box
	{ 0, FLAG_HWND, UNSPEC, AUTO, 0, &hal1, &hal5, &val4 }, // 8. Size type combobox
	{ 0, FLAG_HWND, UNSPEC, AUTO, 0, &hal1, &hal2, &val5 }, // 9. "File type"
	{ 0, FLAG_HWND, UNSPEC, AUTO, 0, &hal1, &hal2, &val6 }, // 10. File type combobox
	{ 0, FLAG_HWND, UNSPEC, AUTO, 0, &hal1, &hal2, &val7 }, // 11. "Search options"
	{ 0, FLAG_HWND, AUTO, AUTO, 0, &hal4, nullptr, &val8 }, // 12. ctrlSlots
	{ 0, FLAG_HWND, AUTO, AUTO, 0, &hal4, nullptr, &val10 }, // 13. ctrlCollapsed
#ifdef BL_FEATURE_IP_DATABASE
	{ 0, FLAG_HWND, AUTO, AUTO, 0, &hal4, nullptr, &val10 }, // 14. ctrlStoreIP
#else
	{ 0, FLAG_PLACEHOLDER },
#endif
	{ 0, FLAG_HWND, AUTO, AUTO, 0, &hal4, nullptr, &val10 }, // 15. ctrlStoreSettings
	{ 0, FLAG_HWND, AUTO, AUTO, 0, &hal4, nullptr, &val10 }, // 16. ctrlUseGroupTreeSettings
	{ 0, FLAG_HWND, UNSPEC, AUTO, 0, &hal1, &hal2, &val11 }, // 17. "Hubs"
	{ 0, FLAG_HWND, UNSPEC, UNSPEC, 0, &hal1, &hal2, &val6, &val12 } // 18. Hubs ListView
};

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
	COLUMN_P2P_GUARD
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
	140  // COLUMN_P2P_GUARD
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
	ResourceManager::P2P_GUARD
};

static const ResourceManager::Strings hubsColumnNames[] = 
{
	ResourceManager::HUB,
	ResourceManager::TIME_TO_WAIT
};

SearchFrame::SearchFrame() :
	id(WinUtil::getNewFrameID(WinUtil::FRAME_TYPE_SEARCH)),
	TimerHelper(m_hWnd),
	showUIContainer(WC_COMBOBOX, this, SHOWUI_MESSAGE_MAP),
#ifdef BL_FEATURE_IP_DATABASE
	storeIP(false),
#endif
	ctrlFilterContainer(WC_EDIT, this, SEARCH_FILTER_MESSAGE_MAP),
	ctrlFilterSelContainer(WC_COMBOBOX, this, SEARCH_FILTER_MESSAGE_MAP),
	colorText(0), colorBackground(0),
	initialSize(0), initialMode(SIZE_ATLEAST), initialType(FILE_TYPE_ANY),
	showUI(true), onlyFree(false), droppedResults(0), resultsCount(0),
	expandSR(false),
	storeSettings(false),
	autoSwitchToTTH(false),
	running(false),
	searchEndTime(0),
	searchStartTime(0),
	waitingResults(false),
	needUpdateResultCount(false),
	updateList(false),
	hasWaitTime(false),
	treeExpanded(false),
	treeItemRoot(nullptr),
	treeItemCurrent(nullptr),
	treeItemOld(nullptr),
	useTree(true),
	shouldSort(false),
	startingSearch(false),
	portStatus(PortTest::STATE_UNKNOWN),
	hTheme(nullptr),
	useDHT(false)
{
	colors.get();
	ctrlResults.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlResults.setColumnFormat(COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlResults.setColumnFormat(COLUMN_EXACT_SIZE, LVCFMT_RIGHT);
	ctrlResults.setColumnFormat(COLUMN_TYPE, LVCFMT_RIGHT);
	ctrlResults.setColumnFormat(COLUMN_SLOTS, LVCFMT_RIGHT);
	static const int hubsColumnIds[] = { 0, 1 };
	int hubsColumnSizes[] = { 154 - GetSystemMetrics(SM_CXVSCROLL), 82 };
	ctrlHubs.setColumns(2, hubsColumnIds, hubsColumnNames, hubsColumnSizes);
	ctrlHubs.enableHeaderMenu = false;
	ctrlHubs.setSortColumn(0);
	hashDb = DatabaseManager::getInstance()->getDefaultHashDatabaseConnection();
}

SearchFrame::~SearchFrame()
{
	dcassert(everything.empty());
	dcassert(closed);
	searchParam.removeToken();
	images.Destroy();
	searchTypesImageList.Destroy();
}

bool SearchFrame::isValidFile(const SearchResult& sr)
{
	return sr.getType() == SearchResult::TYPE_FILE && sr.getSize() > 0 && !sr.getTTH().isZero();
}

void SearchFrame::openWindow(const tstring& str /* = Util::emptyString */, LONGLONG size /* = 0 */, SizeModes mode /* = SIZE_ATLEAST */, int type /* = FILE_TYPE_ANY */)
{
	SearchFrame* frame = new SearchFrame();
	frame->setInitial(str, size, mode, type);
	frame->Create(WinUtil::g_mdiClient);
	framesLock.lock();
	activeFrames.insert(make_pair(frame->id, frame));
	framesLock.unlock();
}

void SearchFrame::closeAll()
{
	LOCK(framesLock);
	for (auto& frame : activeFrames)
		::PostMessage(frame.second->m_hWnd, WM_CLOSE, 0, 0);
}

SearchFrame* SearchFrame::getFramePtr(uint64_t id)
{
	framesLock.lock();
	auto i = activeFrames.find(id);
	if (i == activeFrames.end())
	{
		framesLock.unlock();
		return nullptr;
	}
	return i->second;
}

void SearchFrame::releaseFramePtr(SearchFrame* frame)
{
	if (frame) framesLock.unlock();
}

void SearchFrame::broadcastSearchResult(const SearchResult& sr)
{
	framesLock.lock();
	for (auto& frame : activeFrames)
		frame.second->addSearchResult(sr);
	framesLock.unlock();
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
	ctrlSize.EnableWindow(isNormal);
	ctrlSizeMode.EnableWindow(isNormal);
}

LRESULT SearchFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	colorBackground = Colors::g_tabBackground;
	colorText = Colors::g_tabText;

	tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(tooltip.IsWindow());

	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.ModifyStyleEx(0, WS_EX_COMPOSITED);

	ctrlSearchBox.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_TABSTOP, 0, IDC_SEARCH_STRING);
	lastSearches.load(e_SearchHistory);
	initSearchHistoryBox();
	ctrlSearchBox.SetExtendedUI();

	ctrlDoSearch.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON |
	                    BS_DEFPUSHBUTTON | WS_TABSTOP, 0, IDC_SEARCH);
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus())
		ctrlDoSearchSubclass.SubclassWindow(ctrlDoSearch);
#endif
	ctrlDoSearch.SetIcon(g_iconBitmaps.getIcon(IconBitmaps::SEARCH, 0));
	WinUtil::addTool(tooltip, ctrlDoSearch, ResourceManager::SEARCH);

	ctrlPurge.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON |
	                 BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_PURGE);
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus())
		ctrlPurgeSubclass.SubclassWindow(ctrlPurge);
#endif
	ctrlPurge.SetIcon(g_iconBitmaps.getIcon(IconBitmaps::CLEAR, 0));
	WinUtil::addTool(tooltip, ctrlPurge, ResourceManager::CLEAR_SEARCH_HISTORY);

	ctrlMode.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SEARCH_MODE);

	ctrlSize.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SEARCH_SIZE);

	ctrlSizeMode.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SEARCH_SIZEMODE);
	
	ctrlFiletype.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_OWNERDRAWFIXED | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_FILETYPES);
	ResourceLoader::LoadImageList(IDR_SEARCH_TYPES, searchTypesImageList, 16, 16);

	ctrlSlots.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_AUTOCHECKBOX, NULL, IDC_FREESLOTS);
	ctrlSlots.SetFont(Fonts::g_systemFont, FALSE);
	ctrlSlots.SetWindowText(CTSTRING(ONLY_FREE_SLOTS));

	ctrlCollapsed.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_AUTOCHECKBOX, NULL, IDC_OPTION_CHECKBOX);
	ctrlCollapsed.SetFont(Fonts::g_systemFont, FALSE);
	ctrlCollapsed.SetWindowText(CTSTRING(EXPANDED_RESULTS));

#ifdef BL_FEATURE_IP_DATABASE
	ctrlStoreIP.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_AUTOCHECKBOX, NULL, IDC_OPTION_CHECKBOX);
	storeIP = BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER);
	ctrlStoreIP.SetCheck(storeIP);
	ctrlStoreIP.SetFont(Fonts::g_systemFont, FALSE);
	ctrlStoreIP.SetWindowText(CTSTRING(STORE_SEARCH_IP));
#endif

	ctrlStoreSettings.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_AUTOCHECKBOX, NULL, IDC_OPTION_CHECKBOX);
	if (BOOLSETTING(SAVE_SEARCH_SETTINGS))
		ctrlStoreSettings.SetCheck(BST_CHECKED);
	ctrlStoreSettings.SetFont(Fonts::g_systemFont, FALSE);
	ctrlStoreSettings.SetWindowText(CTSTRING(SAVE_SEARCH_SETTINGS_TEXT));

	WinUtil::addTool(tooltip, ctrlStoreSettings, ResourceManager::SAVE_SEARCH_SETTINGS_TOOLTIP);
	if (BOOLSETTING(ONLY_FREE_SLOTS))
	{
		ctrlSlots.SetCheck(BST_CHECKED);
		onlyFree = true;
	}

	ctrlUseGroupTreeSettings.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_AUTOCHECKBOX, NULL, IDC_USE_TREE);
	if (BOOLSETTING(USE_SEARCH_GROUP_TREE_SETTINGS))
		ctrlUseGroupTreeSettings.SetCheck(BST_CHECKED);
	ctrlUseGroupTreeSettings.SetFont(Fonts::g_systemFont, FALSE);
	ctrlUseGroupTreeSettings.SetWindowText(CTSTRING(USE_SEARCH_GROUP_TREE_SETTINGS_TEXT));

	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_NOSORTHEADER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_HUB);
	ctrlHubs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	auto ctrlHubsHeader = ctrlHubs.GetHeader();
#ifdef OSVER_WIN_XP
	if (SysVersion::isOsVistaPlus())
#endif
		ctrlHubsHeader.SetWindowLong(GWL_STYLE, ctrlHubsHeader.GetWindowLong(GWL_STYLE) | HDS_NOSIZING);

	ctrlSearchFilterTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WinUtil::getTreeViewStyle(), WS_EX_CLIENTEDGE, IDC_TRANSFER_TREE);
	setTreeViewColors(ctrlSearchFilterTree);
	WinUtil::setExplorerTheme(ctrlSearchFilterTree);
	ctrlSearchFilterTree.SetImageList(searchTypesImageList, TVSIL_NORMAL);

	useTree = SETTING(USE_SEARCH_GROUP_TREE_SETTINGS) != 0;

	const bool useSystemIcons = BOOLSETTING(USE_SYSTEM_ICONS);
	ctrlResults.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                   WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | WS_TABSTOP,
	                   WS_EX_CLIENTEDGE, IDC_RESULTS);
	ctrlResults.ownsItemData = false;
	ctrlResults.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	
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
	
	ctrlShowUI.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_AUTOCHECKBOX);
	ctrlShowUI.SetCheck(BST_CHECKED);
	ctrlShowUI.SetFont(Fonts::g_systemFont);
	showUIContainer.SubclassWindow(ctrlShowUI.m_hWnd);
	WinUtil::addTool(tooltip, ctrlShowUI, ResourceManager::SEARCH_SHOWHIDEPANEL);

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
		ctrlSize.SetWindowText(Text::toT(SETTING(SAVED_SEARCH_SIZE)).c_str());
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
	ctrlResults.SetFont(Fonts::g_systemFont, FALSE);
	ctrlResults.setColumnOwnerDraw(COLUMN_LOCATION);
	ctrlResults.setColumnOwnerDraw(COLUMN_P2P_GUARD);

	hTheme = OpenThemeData(m_hWnd, L"EXPLORER::LISTVIEW");
	if (hTheme)
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED;
	customDrawState.flags |= CustomDrawHelpers::FLAG_GET_COLFMT;
	
	ctrlHubs.insertColumns(Util::emptyString, Util::emptyString, Util::emptyString);
	setListViewColors(ctrlHubs);
	ctrlHubs.SetFont(Fonts::g_systemFont, FALSE);
	WinUtil::setExplorerTheme(ctrlHubs);

	for (int i = 0; i < _countof(layoutItems); ++i)
		layout[i] = layoutItems[i];
	layout[0].id = (UINT_PTR) searchLabel.m_hWnd;
	layout[1].id = (UINT_PTR) ctrlPurge.m_hWnd;
	layout[2].id = (UINT_PTR) ctrlDoSearch.m_hWnd;
	layout[3].id = (UINT_PTR) ctrlSearchBox.m_hWnd;
	layout[4].id = (UINT_PTR) sizeLabel.m_hWnd;
	layout[5].id = (UINT_PTR) ctrlSizeMode.m_hWnd;
	layout[6].id = (UINT_PTR) ctrlSize.m_hWnd;
	layout[7].id = (UINT_PTR) ctrlMode.m_hWnd;
	layout[8].id = (UINT_PTR) typeLabel.m_hWnd;
	layout[9].id = (UINT_PTR) ctrlFiletype.m_hWnd;
	layout[10].id = (UINT_PTR) optionLabel.m_hWnd;
	layout[11].id = (UINT_PTR) ctrlSlots.m_hWnd;
	layout[12].id = (UINT_PTR) ctrlCollapsed.m_hWnd;
#ifdef BL_FEATURE_IP_DATABASE
	layout[13].id = (UINT_PTR) ctrlStoreIP.m_hWnd;
#endif
	layout[14].id = (UINT_PTR) ctrlStoreSettings.m_hWnd;
	layout[15].id = (UINT_PTR) ctrlUseGroupTreeSettings.m_hWnd;
	layout[16].id = (UINT_PTR) hubsLabel.m_hWnd;
	layout[17].id = (UINT_PTR) ctrlHubs.m_hWnd;

	ctrlPortStatus.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0);
	ctrlPortStatus.SetFont(Fonts::g_systemFont, FALSE);
	showPortStatus();

	UpdateLayout();
	for (int j = 0; j < _countof(columnNames); j++)
		ctrlFilterSel.AddString(CTSTRING_I(columnNames[j]));

	ctrlFilterSel.SetCurSel(0);
	ctrlStatus.SetText(STATUS_PROGRESS, nullptr, SBT_OWNERDRAW);
	tooltip.SetMaxTipWidth(200);
	tooltip.Activate(TRUE);
	onSizeMode();   //Get Mode, and turn ON or OFF controlls Size
	FavoriteManager::getInstance()->addListener(this);
	initHubs();
	treeItemRoot = nullptr;
	treeItemCurrent = nullptr;
	treeItemOld = nullptr;
	clearFound();
	if (!initialString.empty())
	{
		ctrlSearchBox.SetWindowText(initialString.c_str());
		ctrlMode.SetCurSel(initialMode);
		ctrlSize.SetWindowText(Util::toStringT(initialSize).c_str());
		ctrlFiletype.SetCurSel(initialType);
		
		onEnter();
	}
	else
	{
		setWindowTitle(TSTRING(SEARCH));
		running = false;
	}
	SettingsManager::getInstance()->addListener(this);
	createTimer(1000);

	bHandled = FALSE;
	return 1;
}

void SearchFrame::createMenus()
{
	if (!copyMenu)
	{
		copyMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(copyMenu);
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_PATH, CTSTRING(PATH));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_HUB_URL, CTSTRING(HUB_ADDRESS));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_FULL_MAGNET_LINK, CTSTRING(COPY_FULL_MAGNET_LINK));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
	}
	if (!targetDirMenu)
	{
		targetDirMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(targetDirMenu);
	}
	if (!targetMenu)
	{
		targetMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(targetMenu);
	}
	if (!priorityMenu)
	{
		priorityMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(priorityMenu);
		MenuHelper::appendPrioItems(priorityMenu, IDC_PRIORITY_PAUSED);
	}
}

void SearchFrame::destroyMenus()
{
	if (copyMenu)
	{
		MenuHelper::removeStaticMenu(copyMenu);
		copyMenu.DestroyMenu();
	}
	if (targetDirMenu)
	{
		MenuHelper::removeStaticMenu(targetDirMenu);
		targetDirMenu.DestroyMenu();
	}
	if (targetMenu)
	{
		MenuHelper::removeStaticMenu(targetMenu);
		targetMenu.DestroyMenu();
	}
	if (priorityMenu)
	{
		MenuHelper::removeStaticMenu(priorityMenu);
		priorityMenu.DestroyMenu();
	}
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
	destroyMenus();
	bHandled = FALSE;
	return 0;
}

LRESULT SearchFrame::onMeasure(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == IDC_FILETYPES)
	{
		CustomDrawHelpers::measureComboBox(reinterpret_cast<MEASUREITEMSTRUCT*>(lParam), Fonts::g_systemFont);
		return TRUE;
	}
	return OMenu::onMeasureItem(m_hWnd, uMsg, wParam, lParam, bHandled);
}

LRESULT SearchFrame::onDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if (wParam == IDC_FILETYPES)
	{
		CustomDrawHelpers::drawComboBox(ctrlFiletype, dis, searchTypesImageList);
		return TRUE;
	}
	if (dis->CtlType == ODT_MENU)
	{
		return OMenu::onDrawItem(m_hWnd, uMsg, wParam, lParam, bHandled);
	}
	if (dis->CtlID == ATL_IDW_STATUS_BAR && dis->itemID == STATUS_PROGRESS)
	{
		const auto delta = searchEndTime - searchStartTime;
		if (searchStartTime > 0 && delta)
		{
			const RECT rc = dis->rcItem;
			int borders[3];
			ctrlStatus.GetBorders(borders);

			const uint64_t now = GET_TICK();
			const int width = rc.right - rc.left;
			const int pos = (int) min<int64_t>(width, width * (now - searchStartTime) / delta);

			OperaColors::drawBar(dis->hDC, rc.left, rc.top, rc.left + pos, rc.bottom, RGB(128, 128, 128), RGB(160, 160, 160));

			SetBkMode(dis->hDC, TRANSPARENT);
			const int textHeight = WinUtil::getTextHeight(dis->hDC);
			const int top = rc.top + (rc.bottom - rc.top - textHeight) / 2;
			
			SetTextColor(dis->hDC, RGB(255, 255, 255));
			RECT rc2 = rc;
			rc2.right = rc.left + pos;
			ExtTextOut(dis->hDC, rc.left + borders[2], top, ETO_CLIPPED, &rc2, statusLine.c_str(), statusLine.size(), nullptr);
			
			SetTextColor(dis->hDC, Colors::g_textColor);
			rc2 = rc;
			rc2.left = rc.left + pos;
			ExtTextOut(dis->hDC, rc.left + borders[2], top, ETO_CLIPPED, &rc2, statusLine.c_str(), statusLine.size(), nullptr);
		}
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

void SearchFrame::initSearchHistoryBox()
{
	ctrlSearchBox.ResetContent();
	const auto& data = lastSearches.getData();
	for (const tstring& s : data)
		ctrlSearchBox.AddString(s.c_str());
}

void SearchFrame::onEnter()
{
	BOOL tmp_Handled;
	onEditChange(0, 0, NULL, tmp_Handled); // if in searchbox TTH - select filetypeTTH
	
	BusyCounter<bool> busy(startingSearch);
	searchClients.clear();
	
	ctrlResults.deleteAll();
	clearFound();
	
	tstring s;
	WinUtil::getWindowText(ctrlSearch, s);
	
	// Add new searches to the last-search dropdown list
	if (!BOOLSETTING(FORGET_SEARCH_REQUEST))
	{
		lastSearches.addItem(s, SETTING(SEARCH_HISTORY));
		initSearchHistoryBox();
		lastSearches.save(e_SearchHistory);
	}
	MainFrame::getMainFrame()->updateQuickSearches();
	// Change Default Settings If Changed
	if (onlyFree != BOOLSETTING(ONLY_FREE_SLOTS))
	{
		SET_SETTING(ONLY_FREE_SLOTS, onlyFree);
	}

	bool searchingOnDHT = false;
	int fileType = ctrlFiletype.GetCurSel();
	const int hubCount = ctrlHubs.GetItemCount();
	if (hubCount <= 1)
	{
		setWindowTitle(TSTRING(SEARCH));
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
				if (fileType != FILE_TYPE_TTH)
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
		setWindowTitle(TSTRING(SEARCH));
		return;
	}

	tstring sizeStr;
	WinUtil::getWindowText(ctrlSize, sizeStr);
	
	double size = Util::toDouble(Text::fromT(sizeStr));
	unsigned scale = 1u << (ctrlSizeMode.GetCurSel() * 10);
	size *= scale;
	searchParam.size = size;
	searchParam.owner = id;

	running = false;
	int prevFileType = fileType;
	{
		LOCK(csSearch);
		searchParam.setFilter(Text::fromT(s), fileType);
		searchParam.removeToken();
		searchParam.generateToken(false);
		s = Text::toT(searchParam.filter);
		searchParam.prepareFilter();
		fileType = searchParam.fileType;
	}

	if (prevFileType == FILE_TYPE_TTH && fileType == FILE_TYPE_ANY)
		ctrlFiletype.SetCurSel(0);

	if (s.empty())
	{
		setWindowTitle(TSTRING(SEARCH));
		return;
	}

	searchTarget = std::move(s);
	
	if (searchParam.size == 0)
		searchParam.sizeMode = SIZE_DONTCARE;
	else
		searchParam.sizeMode = SizeModes(ctrlMode.GetCurSel());
		
	ctrlStatus.SetText(STATUS_COUNT, _T(""));
	ctrlStatus.SetText(STATUS_DROPPED, _T(""));
	
	if (BOOLSETTING(CLEAR_SEARCH))
	{
		ctrlSearch.SetWindowText(_T(""));
	}
	
	droppedResults = 0;
	resultsCount = 0;
	running = true;
	
	setWindowTitle(TSTRING(SEARCH) + _T(" - ") + searchTarget);
	
	// stop old search
	ClientManager::cancelSearch(id);
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
		if (portStatus == PortTest::STATE_FAILURE || BOOLSETTING(SEARCH_PASSIVE))
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

void SearchFrame::addSearchResult(const SearchResult& sr)
{
	if (isClosedOrShutdown())
		return;
	// Check that this is really a relevant search result...
	{
		LOCK(csSearch);
		if (searchParam.filter.empty())
			return;

		if (sr.getToken() && searchParam.token != sr.getToken())
			return;

		needUpdateResultCount = true;
		if (!searchParam.matchSearchResult(sr, onlyFree))
		{
			droppedResults++;
			return;
		}
	}
	auto searchInfo = new SearchInfo(sr);
	{
		LOCK(csEverything);
		everything.insert(searchInfo);
		allUsers.insert(sr.getUser());
	}
	if (!WinUtil::postSpeakerMsg(*this, ADD_RESULT, searchInfo))
	{
		LOCK(csEverything);
		everything.erase(searchInfo);
		dcassert(0);
	}
}

LRESULT SearchFrame::onChangeOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	expandSR = ctrlCollapsed.GetCheck() == BST_CHECKED;
#ifdef BL_FEATURE_IP_DATABASE
	storeIP = ctrlStoreIP.GetCheck() == BST_CHECKED;
#endif
	storeSettings = ctrlStoreSettings.GetCheck() == BST_CHECKED;
	SET_SETTING(SAVE_SEARCH_SETTINGS, storeSettings);
	return 0;
}


LRESULT SearchFrame::onToggleTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
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
		case COLUMN_TTH:
			return compare(a->getText(COLUMN_TTH), b->getText(COLUMN_TTH));
		default:
			return Util::defaultSort(a->getText(col), b->getText(col));
	}
}

void SearchFrame::SearchInfo::calcImageIndex()
{
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
			columns[COLUMN_HITS] = hits < 0 ? Util::emptyStringT : Util::toStringT(hits + 1) + _T(' ') + TSTRING(USERS);
			return columns[COLUMN_HITS];
		case COLUMN_NICK:
			if (getUser())
				columns[COLUMN_NICK] = Text::toT(ClientManager::getNick(getUser(), sr.getHubUrl()));
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
						string hubName = ClientManager::getOnlineHubName(token);
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
					string hubName = ClientManager::getOnlineHubName(s);
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
	colMask |= 1<<col;
	return columns[col];
}

void SearchFrame::SearchInfo::Download::operator()(SearchInfo* si)
{
	try
	{
		if (prio == QueueItem::DEFAULT && WinUtil::isShift())
			prio = QueueItem::HIGHEST;
			
		SearchInfo* parent = si->getParent();
		if (parent) si = parent;
		if (si->sr.getType() == SearchResult::TYPE_FILE)
		{
			tstring tmp = tgt;
			dcassert(!tmp.empty());
			if (!tmp.empty() && tmp.back() == PATH_SEPARATOR)
				tmp += si->getText(COLUMN_FILENAME);
			const string target = Text::fromT(tmp);
			bool getConnFlag = true;
			QueueManager::QueueItemParams params;
			params.size = si->sr.getSize();
			params.root = &si->sr.getTTH();
			params.priority = prio;
			QueueManager::getInstance()->add(target, params, si->sr.getHintedUser(), mask, 0, getConnFlag);
			si->sr.flags |= SearchResult::FLAG_QUEUED;
			sf->updateList = true;

			if (si->groupInfo)
			{
				const auto& children = si->groupInfo->children;
				for (auto i = children.cbegin(); i != children.cend(); ++i)
				{
					SearchInfo* j = *i;
					try
					{
						if (j)
						{
							getConnFlag = true;
							params.size = j->sr.getSize();
							params.root = &j->sr.getTTH();
							QueueManager::getInstance()->add(target, params, j->sr.getHintedUser(), mask, true, getConnFlag);
							j->sr.flags |= SearchResult::FLAG_QUEUED;
							sf->updateList = true;
						}
					}
					catch (const Exception& e)
					{
						LogManager::message(e.getError());
					}
				}
			}
		}
		else
		{
			tstring targetDir = getTargetDirectory(si, tgt);
			QueueManager::getInstance()->addDirectory(si->sr.getFile(), si->sr.getHintedUser(), Text::fromT(targetDir), prio, QueueManager::DIR_FLAG_DOWNLOAD_DIRECTORY);
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
		tstring targetDir = getTargetDirectory(si, tgt);
		if (si->sr.getType() == SearchResult::TYPE_FILE)
		{
			QueueManager::getInstance()->addDirectory(Text::fromT(si->getText(COLUMN_PATH)), si->sr.getHintedUser(), Text::fromT(targetDir), prio, QueueManager::DIR_FLAG_DOWNLOAD_DIRECTORY);
		}
		else
		{
			QueueManager::getInstance()->addDirectory(si->sr.getFile(), si->sr.getHintedUser(), Text::fromT(targetDir), prio, QueueManager::DIR_FLAG_DOWNLOAD_DIRECTORY);
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
		QueueManager::getInstance()->addList(sr.getHintedUser(), 0, QueueItem::XFLAG_CLIENT_VIEW, Text::fromT(getText(COLUMN_PATH)));
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
		QueueManager::getInstance()->addList(sr.getHintedUser(), QueueItem::FLAG_PARTIAL_LIST, QueueItem::XFLAG_CLIENT_VIEW, Text::fromT(getText(COLUMN_PATH)));
	}
	catch (const Exception&)
	{
		dcassert(0);
		// Ignore for now...
	}
}

#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
void SearchFrame::SearchInfo::view()
{
	try
	{
		if (sr.getType() == SearchResult::TYPE_FILE)
		{
			bool getConnFlag = true;
			QueueManager::QueueItemParams params;
			params.size = sr.getSize();
			params.root = &sr.getTTH();
			QueueManager::getInstance()->add(Util::getTempPath() + sr.getFileName(), params, sr.getHintedUser(),
				0, QueueItem::XFLAG_CLIENT_VIEW | QueueItem::XFLAG_TEXT_VIEW, getConnFlag);
			sr.flags |= SearchResult::FLAG_QUEUED;
		}
	}
	catch (const Exception&)
	{
	}
}
#endif

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
		tstring dir = Text::toT(fm->getDownloadDirectory(Util::getFileExt(si->sr.getFileName()), si->getUser()));
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
			const string t = fm->getDownloadDirectory(Util::getFileExt(si->sr.getFileName()), si->getUser());
			(SearchInfo::Download(Text::toT(t), this, QueueItem::DEFAULT))(si);
		}
	}
	return 0;
}

LRESULT SearchFrame::onDownloadWhole(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir;
	if (!getDownloadDirectory(wID, dir)) return 0;
	if (!dir.empty())
		ctrlResults.forEachSelectedT(SearchInfo::DownloadWhole(dir));
	else
	{
		int i = -1;
		while ((i = ctrlResults.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			SearchInfo* si = ctrlResults.getItemData(i);
			(SearchInfo::DownloadWhole(Util::emptyStringT))(si);
		}
	}
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

		if (ctrlResults.GetSelectedCount() == 1)
		{
			int i = ctrlResults.GetNextItem(-1, LVNI_SELECTED);
			auto si = ctrlResults.getItemData(i);
			if (si && isValidFile(si->sr))
			{
				string realPath;
				if (ShareManager::getInstance()->getFileInfo(si->sr.getTTH(), realPath))
				{
					WinUtil::openFile(Text::toT(realPath));
					return 0;
				}
			}
		}

		auto fm = FavoriteManager::getInstance();
		int i = -1;
		while ((i = ctrlResults.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			SearchInfo* si = ctrlResults.getItemData(i);
			if (si)
			{
				const string t = fm->getDownloadDirectory(Util::getFileExt(si->sr.getFileName()), si->getUser());
				(SearchInfo::Download(Text::toT(t), this, QueueItem::DEFAULT))(si);
			}
		}
	}
	return 0;
}

LRESULT SearchFrame::onOpenFileOrFolder(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlResults.GetSelectedCount() == 1)
	{
		int i = ctrlResults.GetNextItem(-1, LVNI_SELECTED);
		auto si = ctrlResults.getItemData(i);
		if (si && isValidFile(si->sr))
		{
			string realPath;
			if (ShareManager::getInstance()->getFileInfo(si->sr.getTTH(), realPath))
			{
				if (wID == IDC_OPEN_FILE)
					WinUtil::openFile(Text::toT(realPath));
				else
					WinUtil::openFolder(Text::toT(realPath));
			}
		}
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
		FavoriteManager::getInstance()->removeListener(this);
		framesLock.lock();
		activeFrames.erase(id);
		framesLock.unlock();
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

	int xdu, ydu;
	WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
	const int width = WinUtil::dialogUnitsToPixelsX(145, xdu);
	const int comboBoxHeight = WinUtil::dialogUnitsToPixelsY(120, ydu);
	static const int labelH = 16;
	static const int lMargin = 4;
	static const int rMargin = 4;
	static const int vertLabelOffset = 3;
	static const int bottomMargin = 5;

	const int textHeight = WinUtil::getTextHeight(m_hWnd, Fonts::g_systemFont);
	const int controlHeight = textHeight + 9;
	const int searchInResultsHeight = 2*bottomMargin + controlHeight;
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[STATUS_LAST];
		ctrlStatus.GetClientRect(sr);
		int tmp = (sr.Width()) > 420 ? 376 : ((sr.Width() > 116) ? sr.Width() - 100 : 16);
		
		w[0] = WinUtil::dialogUnitsToPixelsX(28, xdu);
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
		const int treeWidth = useTree ? 200 : 0;
		rc.left += width + treeWidth;
		rc.bottom -= searchInResultsHeight;
		ctrlResults.MoveWindow(rc);

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

		// required for DialogLayout
		ctrlSearchBox.MoveWindow(0, 0, 0, comboBoxHeight);
		ctrlMode.MoveWindow(0, 0, 0, comboBoxHeight);
		ctrlSizeMode.MoveWindow(0, 0, 0, comboBoxHeight);

		DialogLayout::Options opt;
		opt.width = U_PX(width);
		int dialogHeight = rect.bottom - rect.top - bottomMargin;
		opt.height = U_PX(dialogHeight);
		opt.show = true;
		DialogLayout::layout(m_hWnd, layout, _countof(layout), &opt);
	}
	else
	{
		CRect rc = rect;
		rc.bottom -= searchInResultsHeight;
		ctrlResults.MoveWindow(rc);

		ctrlSearchBox.ShowWindow(SW_HIDE);
		ctrlMode.ShowWindow(SW_HIDE);
		ctrlPurge.ShowWindow(SW_HIDE);
		ctrlSize.ShowWindow(SW_HIDE);
		ctrlSizeMode.ShowWindow(SW_HIDE);
		ctrlFiletype.ShowWindow(SW_HIDE);

		ctrlCollapsed.ShowWindow(SW_HIDE);
		ctrlSlots.ShowWindow(SW_HIDE);
#ifdef BL_FEATURE_IP_DATABASE
		ctrlStoreIP.ShowWindow(SW_HIDE);
#endif
		ctrlStoreSettings.ShowWindow(SW_HIDE);
		ctrlUseGroupTreeSettings.ShowWindow(SW_HIDE);
		ctrlSearchFilterTree.ShowWindow(SW_HIDE);

		ctrlHubs.ShowWindow(SW_HIDE);
		ctrlDoSearch.ShowWindow(SW_HIDE);
		typeLabel.ShowWindow(SW_HIDE);
		hubsLabel.ShowWindow(SW_HIDE);
		sizeLabel.ShowWindow(SW_HIDE);
		searchLabel.ShowWindow(SW_HIDE);
		optionLabel.ShowWindow(SW_HIDE);
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

	// Port status
	rc.left = rc.right + WinUtil::dialogUnitsToPixelsX(6, xdu);
	rc.right = std::min(rect.right, rc.left + WinUtil::dialogUnitsToPixelsX(200, xdu));
	ctrlPortStatus.MoveWindow(rc);

	if (!ctrlSearch)
	{
		COMBOBOXINFO inf = { sizeof(inf) };
		ctrlSearchBox.GetComboBoxInfo(&inf);
		ctrlSearch.Attach(inf.hwndItem);
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
			
		if (uc.once())
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
#ifdef BL_FEATURE_IP_DATABASE
	        hWnd == ctrlStoreIP.m_hWnd ||
#endif
	        hWnd == ctrlStoreSettings.m_hWnd ||
	        hWnd == ctrlUseGroupTreeSettings.m_hWnd ||
	        hWnd == ctrlCollapsed.m_hWnd || hWnd == srLabel.m_hWnd ||
			hWnd == ctrlPortStatus.m_hWnd)
	{
		SetTextColor(hDC, colorText);
		SetBkColor(hDC, colorBackground);
		return reinterpret_cast<LRESULT>(Colors::g_tabBackgroundBrush);
	}
	return Colors::setColor(hDC);
}

BOOL SearchFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::g_tabCtrl->isActive(m_hWnd)) return FALSE;
	if (WinUtil::isCtrl()) return FALSE;
	return IsDialogMessage(pMsg);
}

LRESULT SearchFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	
	if (!tabMenu)
	{
		tabMenu.CreatePopupMenu();
		tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_SEARCH_FRAME, CTSTRING(MENU_CLOSE_ALL_SEARCHFRAME));
		tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
	}
	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
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
	if (si->sr.getType() == SearchResult::TYPE_FILE)
	{
		const auto fileExt = Text::toLower(Util::getFileExtWithoutDot(si->sr.getFileName()));
		auto it = groupedResults.find(treeItemCurrent);
		if (it == groupedResults.cend()) return false;
		const auto& data = it->second;
		return data.ext == fileExt;
	}
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

void SearchFrame::addSearchResult(SearchInfo* si)
{
	if (isSkipSearchResult(si))
		return;
	const SearchResult& sr = si->sr;
	const auto user = sr.getUser();
	const IpAddress& ip = sr.getIP();
	if (ip.type)
		user->setIP(ip);
	// Check previous search results for dupes
	const auto& parents = ctrlResults.getParents();
	if (!sr.getTTH().isZero())
	{
		auto it = parents.find(sr.getTTH());
		if (it != parents.end())
		{
			const auto& gi = it->second;
			if (gi->parent &&
			    user->getCID() == gi->parent->getUser()->getCID() && sr.getFile() == gi->parent->sr.getFile())
			{
				removeSearchInfo(si);
				return;
			}
			for (const SearchInfo* item : gi->children)
				if (user->getCID() == item->getUser()->getCID() && sr.getFile() == item->sr.getFile())
				{
					removeSearchInfo(si);
					return;
				}
		}
	}
#if 0
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
#endif
	if (running)
	{
		resultsCount++;
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
		{
			CLockRedraw<> lockRedraw(ctrlResults);
			if (!si->getText(COLUMN_TTH).empty())
			{
				if (itemMatchesSelType(si))
					ctrlResults.insertGroupedItem(si, expandSR, true);
			}
			else
			{
				if (treeItemCurrent == treeItemRoot || treeItemCurrent == nullptr)
					ctrlResults.insertItem(si, I_IMAGECALLBACK);
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
			break;
		}
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

tstring SearchFrame::getTargetDirectory(const SearchInfo* si, const tstring& downloadDir)
{
	const tstring& target = si->getText(COLUMN_PATH);
	if (target.length() > 2)
	{
		size_t start = target.rfind(_T('\\'), target.length() - 2);
		if (start == tstring::npos) start = 0; else start++;
		if (!downloadDir.empty())
		{
			dcassert(downloadDir.back() == _T('\\'));
			return downloadDir + target.substr(start);
		}
		string defaultDownloadDir = Util::getDownloadDir(si->getUser());
		return Text::toT(defaultDownloadDir) + target.substr(start);
	}
	return Util::emptyStringT;
}

int SearchFrame::makeTargetMenu(const SearchInfo* si, OMenu& menu, int idc, ResourceManager::Strings title)
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
	
	// Append the result of getTargetDirectory
	if (si && si->sr.getType() == SearchResult::TYPE_FILE && idc != IDC_DOWNLOADDIR_TO_FAV)
	{
		tstring target = getTargetDirectory(si, Util::emptyStringT);
		if (!target.empty())
		{
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
		menu.InsertSeparatorLast(TSTRING(PREVIOUS_FOLDERS));
		for (auto i = LastDir::get().cbegin(); i != LastDir::get().cend(); ++i)
		{
			tstring target = *i;
			dlTargets[idc + n] = DownloadTarget(target, DownloadTarget::PATH_LAST);
			Util::removePathSeparator(target);
			WinUtil::escapeMenu(target);
			menu.AppendMenu(MF_STRING, idc + n, target.c_str());
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
		if (selCount)
		{
			createMenus();
			const SearchInfo* si = nullptr;
			const SearchResult* sr = nullptr;
			if (selCount == 1)
			{
				int i = ctrlResults.GetNextItem(-1, LVNI_SELECTED);
				dcassert(i != -1);
				si = ctrlResults.getItemData(i);
				sr = &si->sr;
			}

			clearUserMenu();
			OMenu resultsMenu;

			resultsMenu.CreatePopupMenu();
			int defaultItem = IDC_DOWNLOAD_TO_FAV;

			if (sr && isValidFile(*sr))
			{
				bool existingFile = ShareManager::getInstance()->isTTHShared(sr->getTTH());
				if (existingFile)
				{
					resultsMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
					resultsMenu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
					resultsMenu.AppendMenu(MF_SEPARATOR);
					defaultItem = IDC_OPEN_FILE;
				}
			}
			resultsMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TO_FAV, CTSTRING(DOWNLOAD), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD, 0));
			resultsMenu.AppendMenu(MF_POPUP, targetMenu, CTSTRING(DOWNLOAD_TO));
			resultsMenu.AppendMenu(MF_POPUP, priorityMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
#ifdef USE_DOWNLOAD_DIR
			if (sr && sr->getType() == SearchResult::TYPE_FILE)
			{
				resultsMenu.AppendMenu(MF_STRING, IDC_DOWNLOADDIR_TO_FAV, CTSTRING(DOWNLOAD_WHOLE_DIR));
				resultsMenu.AppendMenu(MF_POPUP, targetDirMenu, CTSTRING(DOWNLOAD_WHOLE_DIR_TO));
			}
#endif
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
			resultsMenu.AppendMenu(MF_SEPARATOR);
			resultsMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT), g_iconBitmaps.getBitmap(IconBitmaps::NOTEPAD, 0));
#endif
			resultsMenu.AppendMenu(MF_SEPARATOR);
			resultsMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
			
			resultsMenu.AppendMenu(MF_POPUP, copyMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
			resultsMenu.AppendMenu(MF_SEPARATOR);
			appendAndActivateUserItems(resultsMenu, true);
			resultsMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE, 0));
			resultsMenu.SetMenuDefaultItem(defaultItem);

			// Add target menu
			dlTargets.clear();
			int n = makeTargetMenu(si, targetMenu, IDC_DOWNLOAD_TO_FAV, ResourceManager::DOWNLOAD_TO);
			
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
			if (sr)
				makeTargetMenu(si, targetDirMenu, IDC_DOWNLOADDIR_TO_FAV, ResourceManager::DOWNLOAD_WHOLE_DIR_TO);
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

			tstring caption;
			if (sr && !sr->getFileName().empty())
			{
				caption = Text::toT(sr->getFileName());
				Util::removePathSeparator(caption);
				WinUtil::limitStringLength(caption);
			} else caption = TSTRING(FILES);
			resultsMenu.InsertSeparatorFirst(caption);
			resultsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			resultsMenu.RemoveFirstItem();

			cleanUcMenu(resultsMenu);
			MenuHelper::unlinkStaticMenus(resultsMenu);
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
	vector<ClientBasePtr> hubs;
	ClientManager::getConnectedHubs(hubs);
	if (useDHT)
		onHubAdded(new HubInfo(dht::NetworkName, TSTRING(DHT), false));
	for (const ClientBasePtr& clientBase : hubs)
	{
		const Client* client = static_cast<Client*>(clientBase.get());
		onHubAdded(new HubInfo(client->getHubUrl(), Text::toT(client->getHubName()), client->getMyIdentity().isOp()));
	}
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
	bool hasHistory = !lastSearches.empty();
	if (hasHistory && BOOLSETTING(CONFIRM_CLEAR_SEARCH_HISTORY))
	{
		UINT check = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(CONFIRM_CLEAR_SEARCH), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION, check) != IDYES)
			return 0;
		if (check == BST_CHECKED)
			SET_SETTING(CONFIRM_CLEAR_SEARCH_HISTORY, FALSE);
	}
	tooltip.Activate(FALSE);
	ctrlSearchBox.ResetContent();
	if (hasHistory)
		lastSearches.clear(e_SearchHistory);
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
				if (sr.getType() == SearchResult::TYPE_FILE)
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
				break;
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

void SearchFrame::getFileItemColor(int flags, COLORREF& fg, COLORREF& bg) const
{
	fg = Colors::g_textColor;
	bg = Colors::g_bgColor;
	if (flags & SearchResult::FLAG_SHARED)
	{
		fg = colors.fgNormal[FileStatusColors::SHARED];
		bg = colors.bgNormal[FileStatusColors::SHARED];
	}
	else if (flags & SearchResult::FLAG_DOWNLOADED)
	{
		fg = colors.fgNormal[FileStatusColors::DOWNLOADED];
		bg = colors.bgNormal[FileStatusColors::DOWNLOADED];
	}
	else if (flags & SearchResult::FLAG_DOWNLOAD_CANCELED)
	{
		fg = colors.fgNormal[FileStatusColors::CANCELED];
		bg = colors.bgNormal[FileStatusColors::CANCELED];
	}
	if (flags & SearchResult::FLAG_QUEUED)
		fg = colors.fgInQueue;
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
				si->sr.checkTTH(hashDb);
				getFileItemColor(si->sr.flags, cd->clrText, cd->clrTextBk);
#ifdef BL_FEATURE_IP_DATABASE
				if (!si->ipUpdated && storeIP && si->getUser())
				{
					const IpAddress& ip = si->sr.getIP();
					if (ip.type)
					{
						si->ipUpdated = true;
						si->getUser()->setIP(ip);
					}
				}
#endif
				customDrawState.indent = si->hasParent() ? 1 : 0;
			}
			CustomDrawHelpers::startItemDraw(customDrawState, cd);
			if (customDrawState.flags & CustomDrawHelpers::FLAG_SELECTED)
				cd->clrText = Colors::g_textColor;
			if (hTheme)
				CustomDrawHelpers::drawBackground(hTheme, customDrawState, cd);
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
			if (column == COLUMN_NICK)
			{
				const tstring& text = si->getText(COLUMN_NICK);
				const UserPtr& user = si->getUser();
				int icon = user->isOnline() ? 0 : 2;
				auto userFlags = user->getFlags();
#ifdef ENABLE_USER_TEXT_COLORS
				COLORREF savedColor = cd->clrText;
				if (userFlags & User::FAVORITE)
				{
					if (userFlags & User::BANNED)
					{
						icon += 3;
						cd->clrText = SETTING(TEXT_ENEMY_FORE_COLOR);
					}
					else
						cd->clrText = SETTING(FAVORITE_COLOR);
				}
#else
				if (userFlags & User::BANNED) icon += 3;
#endif
				CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, &g_favUserImage.getIconList(), icon, text, false);
#ifdef ENABLE_USER_TEXT_COLORS
				cd->clrText = savedColor;
#endif
				return CDRF_SKIPDEFAULT;
			}
			if (column == COLUMN_P2P_GUARD)
			{
				si->sr.loadP2PGuard();
				si->colMask &= ~(1<<COLUMN_P2P_GUARD);
				const string& p2pGuard = si->sr.getIpInfo().p2pGuard;
				tstring text = Text::toT(p2pGuard);
				CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, &g_userStateImage.getIconList(), text.empty() ? -1 : 3, text, false);
				return CDRF_SKIPDEFAULT;
			}
			if (column == COLUMN_LOCATION)
			{
				si->sr.loadLocation();
				si->colMask &= ~(1<<COLUMN_LOCATION);
				const auto& ipInfo = si->sr.getIpInfo();
				CustomDrawHelpers::drawLocation(customDrawState, cd, ipInfo);
				return CDRF_SKIPDEFAULT;
			}
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
	extToTreeItem.clear();
	groupedResults.clear();
	for (int i = 0; i < _countof(typeNodes); i++)
		typeNodes[i] = nullptr;
	treeItemCurrent = nullptr;
	treeItemOld = nullptr;
	treeItemRoot = nullptr;
	treeExpanded = false;
	ctrlSearchFilterTree.DeleteAllItems();
	{
		LOCK(csEverything);
		for (auto i = everything.begin(); i != everything.end(); ++i)
		{
			delete *i;
		}
		everything.clear();
		allUsers.clear();
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
	auto it = groupedResults.find(treeItem);
	if (it == groupedResults.cend()) return;
	auto& data = it->second.data;
	for (SearchInfo* si : data)
		if (matchFilter(si, filter, doSizeCompare, mode, size))
		{
			si->groupInfo.reset();
			si->hits = -1;
			si->stateFlags = 0;
			if (si->sr.getType() == SearchResult::TYPE_FILE)
				ctrlResults.insertGroupedItem(si, expandSR, true);
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
		if (treeItemCurrent == nullptr || treeItemCurrent == treeItemRoot)
		{
			for (int i = 0; i < _countof(typeNodes); ++i)
				if (typeNodes[i])
					insertStoredResults(typeNodes[i], sel, doSizeCompare, mode, size);
		}
		else
			insertStoredResults(treeItemCurrent, sel, doSizeCompare, mode, size);
		ctrlResults.resort();
		shouldSort = false;
	}
}

LRESULT SearchFrame::onFilterChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
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
	if (Util::isTigerHashString(searchString))
	{
		ctrlFiletype.SetCurSel(FILE_TYPE_TTH);
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
					if (Util::isTigerHashString(searchString))
					{
						ctrlSearchBox.SetWindowText(searchString.c_str());
						ctrlFiletype.SetCurSel(FILE_TYPE_TTH);
						autoSwitchToTTH = true;
						return 0;
					}
				}
			}
		}
		if (autoSwitchToTTH)
		{
			ctrlFiletype.SetCurSel(FILE_TYPE_ANY);
			autoSwitchToTTH = false;
		}
	}
	return 0;
}

LRESULT SearchFrame::onEditSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = ctrlSearchBox.GetCurSel();
	tstring text = WinUtil::getComboBoxItemText(ctrlSearchBox, index);
	if (Util::isTigerHashString(text))
	{
		ctrlFiletype.SetCurSel(FILE_TYPE_TTH);
		autoSwitchToTTH = true;
	}
	else if (autoSwitchToTTH)
	{
		ctrlFiletype.SetCurSel(FILE_TYPE_ANY);
		autoSwitchToTTH = false;
	}
	return 0;
}

void SearchFrame::onUserUpdated(const UserPtr& user) noexcept
{
	LOCK(csEverything);
	if (allUsers.find(user) != allUsers.end())
		updateList = true;
}

void SearchFrame::on(SettingsManagerListener::Repaint)
{
	SetClassLongPtr(m_hWnd, GCLP_HBRBACKGROUND, (LONG_PTR) Colors::g_tabBackgroundBrush);
	bool redraw = false;
	if (colorBackground != Colors::g_tabBackground || colorText != Colors::g_tabText)
	{
		colorBackground = Colors::g_tabBackground;
		colorText = Colors::g_tabText;
		redraw = true;
	}

	FileStatusColors newColors;
	newColors.get();
	if (ctrlResults.isRedraw() || !colors.compare(newColors))
	{
		colors = newColors;
		ctrlHubs.SetBkColor(Colors::g_bgColor);
		ctrlHubs.SetTextBkColor(Colors::g_bgColor);
		ctrlHubs.SetTextColor(Colors::g_textColor);
		setTreeViewColors(ctrlSearchFilterTree);
		redraw = true;
	}
	if (redraw)
		RedrawWindow(nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void SearchFrame::getSelectedUsers(vector<UserPtr>& v) const
{
	int i = ctrlResults.GetNextItem(-1, LVNI_SELECTED);
	while (i >= 0)
	{
		SearchInfo* si = ctrlResults.getItemData(i);
		v.push_back(si->getUser());
		i = ctrlResults.GetNextItem(i, LVNI_SELECTED);
	}
}

void SearchFrame::speak(Speakers s, const Client* c)
{
	if (!isClosedOrShutdown())
	{
		HubInfo* hubInfo = new HubInfo(c->getHubUrl(), Text::toT(c->getHubName()), c->getMyIdentity().isOp());
		WinUtil::postSpeakerMsg(*this, s, hubInfo);
	}
}

void SearchFrame::updateResultCount()
{
	ctrlStatus.SetText(STATUS_COUNT, (Util::toStringT(resultsCount) + _T(' ') + TSTRING(FILES)).c_str());
	ctrlStatus.SetText(STATUS_DROPPED, (Util::toStringT(droppedResults) + _T(' ') + TSTRING(FILTERED)).c_str());
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
		ctrlStatus.SetText(STATUS_TIME, CTSTRING(DONE));
	}
	else
	{
		statusLine = TSTRING(SEARCHING_FOR) + _T(' ') + searchTarget + _T(" ... ") + Util::toStringT(percent) + _T("%");
		ctrlStatus.SetText(STATUS_TIME, Util::formatSecondsT((searchEndTime - tick) / 1000).c_str());
	}
	setWindowTitle(statusLine);
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
	int icon;
	switch (state)
	{
		case PortTest::STATE_RUNNING:
			icon = IconBitmaps::WARNING;
			newText = CTSTRING(UDP_PORT_TEST_WAIT);
			break;

		case PortTest::STATE_FAILURE:
			icon = IconBitmaps::STATUS_FAILURE;
			newText = CTSTRING(UDP_PORT_TEST_FAILED);
			break;

		case PortTest::STATE_SUCCESS:
			icon = IconBitmaps::STATUS_SUCCESS;
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
	ctrlPortStatus.setImage(icon, 0);
	portStatus = state;
	currentReflectedAddress = std::move(reflectedAddress);
	if (newText != ctrlPortStatus.getText())
	{
		ctrlPortStatus.setText(newText);
		ctrlPortStatus.Invalidate();
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
			0, 0, NULL, NULL, NULL, Colors::g_tabBackgroundBrush, NULL, _T("SearchFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::SEARCH, 0);

	return wc;
}
