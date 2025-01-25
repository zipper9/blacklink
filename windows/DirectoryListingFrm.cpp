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
#include "DirectoryListingFrm.h"
#include "PrivateFrame.h"
#include "QueueFrame.h"
#include "DclstGenDlg.h"
#include "SearchDlg.h"
#include "FindDuplicatesDlg.h"
#include "DuplicateFilesDlg.h"
#include "Fonts.h"
#include "BrowseFile.h"
#include "LineDlg.h"
#include "ConfUI.h"
#include "../client/QueueManager.h"
#include "../client/ClientManager.h"
#include "../client/ShareManager.h"
#include "../client/DatabaseManager.h"
#include "../client/FavoriteManager.h"
#include "../client/Client.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include "../client/FormatUtil.h"
#include "../client/SettingsUtil.h"
#include "../client/Util.h"
#include "../client/ConfCore.h"
#include <boost/algorithm/string/trim.hpp>

static const size_t MAX_NAVIGATION_HISTORY = 25;
static const size_t MAX_TYPED_HISTORY = 64;

static const int STATUS_PART_PADDING = 12;

static const char* THREAD_NAME = "DirectoryListingLoader";

DirectoryListingFrame::FrameMap DirectoryListingFrame::activeFrames;

const int DirectoryListingFrame::columnId[] =
{
	COLUMN_FILENAME,
	COLUMN_TYPE,
	COLUMN_SIZE,
	COLUMN_TTH,
	COLUMN_PATH,
	COLUMN_UPLOAD_COUNT,
	COLUMN_TS,
	COLUMN_EXACT_SIZE,
	COLUMN_BITRATE,
	COLUMN_MEDIA_XY,
	COLUMN_MEDIA_VIDEO,
	COLUMN_MEDIA_AUDIO,
	COLUMN_DURATION,
	COLUMN_FILES
};

static const int columnSizes[] =
{
	200, // COLUMN_FILENAME
	60,  // COLUMN_TYPE
	85,  // COLUMN_SIZE
	200, // COLUMN_TTH
	300, // COLUMN_PATH
	50,  // COLUMN_UPLOAD_COUNT
	120, // COLUMN_TS
	100, // COLUMN_EXACT_SIZE
	80,  // COLUMN_BITRATE
	100, // COLUMN_MEDIA_XY
	100, // COLUMN_MEDIA_VIDEO
	100, // COLUMN_MEDIA_AUDIO
	80,  // COLUMN_DURATION
	80   // COLUMN_FILES
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::FILE,
	ResourceManager::TYPE,
	ResourceManager::SIZE,
	ResourceManager::TTH_ROOT,
	ResourceManager::PATH,
	ResourceManager::UPLOAD_COUNT,
	ResourceManager::ADDED,
	ResourceManager::EXACT_SIZE,
	ResourceManager::BITRATE,
	ResourceManager::MEDIA_X_Y,
	ResourceManager::MEDIA_VIDEO,
	ResourceManager::MEDIA_AUDIO,
	ResourceManager::DURATION,
	ResourceManager::FILES
};

enum
{
	MENU_NONE,
	MENU_LIST,
	MENU_TREE
};

static SearchOptions searchOptions;
static FindDuplicatesDlg::Options findDupsOptions;

void DirectoryListingFrame::openWindow(const tstring& file, const tstring& dir, const HintedUser& user, int64_t speed, bool isDcLst /*= false*/)
{
	HWND hwnd;
	DirectoryListingFrame* frame = new DirectoryListingFrame(user, nullptr, false);
	frame->setSpeed(speed);
	frame->setDclstFlag(isDcLst);
	frame->setFileName(Text::fromT(file));
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::POPUNDER_FILELIST))
		hwnd = WinUtil::hiddenCreateEx(frame);
	else
		hwnd = frame->Create(WinUtil::g_mdiClient);
	if (hwnd)
	{
		frame->loadFile(file, dir);
		activeFrames.insert(DirectoryListingFrame::FrameMap::value_type(frame->m_hWnd, frame));
	}
	else
		delete frame;
}

void DirectoryListingFrame::openWindow(const HintedUser& user, const string& txt, int64_t speed)
{
	DirectoryListingFrame* frame = findFrame(user, true);
	if (frame)
	{
		frame->setSpeed(speed);
		frame->loadXML(txt);
		return;
	}
	frame = new DirectoryListingFrame(user, nullptr, true);
	frame->setSpeed(speed);
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::POPUNDER_FILELIST))
		WinUtil::hiddenCreateEx(frame);
	else
		frame->Create(WinUtil::g_mdiClient);
	frame->loadXML(txt);
	activeFrames.insert(DirectoryListingFrame::FrameMap::value_type(frame->m_hWnd, frame));
}

DirectoryListingFrame* DirectoryListingFrame::openWindow(DirectoryListing* dl, const HintedUser& user, int64_t speed, bool searchResults)
{
	DirectoryListingFrame* frame = new DirectoryListingFrame(user, dl, false);
	frame->searchResultsFlag = searchResults;
	frame->setSpeed(speed);
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::POPUNDER_FILELIST))
		WinUtil::hiddenCreateEx(frame);
	else
		frame->Create(WinUtil::g_mdiClient);
	frame->updateWindowTitle();
	frame->refreshTree(frame->dl->getRoot(), frame->treeRoot, false);
	frame->loading = false;
	frame->initStatus();
	frame->enableControls();
	frame->ctrlStatus.SetText(STATUS_TEXT, _T(""));
	activeFrames.insert(DirectoryListingFrame::FrameMap::value_type(frame->m_hWnd, frame));
	return frame;
}

DirectoryListingFrame* DirectoryListingFrame::findFrame(const UserPtr& user, bool browsing)
{
	for (const auto& af : activeFrames)
	{
		DirectoryListingFrame* frame = af.second;
		if (frame->loading || frame->dclstFlag || frame->searchResultsFlag) continue;
		if (frame->dl->getUser() == user && frame->isBrowsing() == browsing)
			return frame;
	}
	return nullptr;
}

DirectoryListingFrame::DirectoryListingFrame(const HintedUser &user, DirectoryListing *dl, bool browsing) :
	TimerHelper(m_hWnd),
	treeContainer(WC_TREEVIEW, this, CONTROL_MESSAGE_MAP),
	listContainer(WC_LISTVIEW, this, CONTROL_MESSAGE_MAP),
	navHistoryIndex(0),
	typedHistoryState(0),
	setWindowTitleTick(0),
	browsing(browsing),
	ownList(user.user->isMe()),
	treeRoot(nullptr), selectedDir(nullptr), treeViewFocused(false), hTheme(nullptr),
	dclstFlag(false), searchResultsFlag(false), filteredListFlag(false),
	updating(false), loading(true), refreshing(false), listItemChanged(false), offline(false), showingDupFiles(false),
	abortFlag(false),
	id(WinUtil::getNewFrameID(WinUtil::FRAME_TYPE_DIRECTORY_LISTING)),
	originalId(0),
	changingPath(0),
	updatingLayout(0),
	activeMenu(MENU_NONE)
{
	if (!dl) dl = new DirectoryListing(abortFlag);

	int scanOptions = 0;
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::FILELIST_SHOW_SHARED))
		scanOptions |= DirectoryListing::SCAN_OPTION_SHARED;
	if (ss->getBool(Conf::FILELIST_SHOW_DOWNLOADED))
		scanOptions |= DirectoryListing::SCAN_OPTION_DOWNLOADED;
	if (ss->getBool(Conf::FILELIST_SHOW_CANCELED))
		scanOptions |= DirectoryListing::SCAN_OPTION_CANCELED;
	if (ownList && ss->getBool(Conf::FILELIST_SHOW_MY_UPLOADS))
	{
		auto cs = SettingsManager::instance.getCoreSettings();
		cs->lockRead();
		if (cs->getBool(Conf::ENABLE_UPLOAD_COUNTER))
			scanOptions |= DirectoryListing::SCAN_OPTION_SHOW_MY_UPLOADS;
		cs->unlockRead();
	}

	dl->setScanOptions(scanOptions);
	this->dl.reset(dl);
	dl->setHintedUser(user);

	colors.get();

	ctrlList.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlList.setColumnFormat(COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(COLUMN_EXACT_SIZE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(COLUMN_TYPE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(COLUMN_UPLOAD_COUNT, LVCFMT_RIGHT);
}

DirectoryListingFrame* DirectoryListingFrame::findFrameByID(uint64_t id)
{
	for (const auto& i : activeFrames)
	{
		DirectoryListingFrame* frame = i.second;
		if (frame->id == id) return frame;
	}
	return nullptr;
}

void DirectoryListingFrame::updateWindowTitle()
{
	setWindowTitleTick = 0;
	if (dclstFlag)
	{
		setWindowTitle(Text::toT(Util::getFileName(getFileName())));
		return;
	}
	const UserPtr& user = dl->getUser();
	if (!user) return;
	if (user->getFlags() & User::FAKE)
	{
		setWindowTitle(Text::toT(user->getLastNick()));
		return;
	}
	const HintedUser &hintedUser = dl->getHintedUser();
	bool userOffline = false;
	tstring text = Text::toT(hintedUser.user->getLastNick());
	if (!hintedUser.user->isMe())
	{
		text += _T(" - ");
		StringList hubNames;
		if (!hintedUser.hint.empty())
			hubNames = ClientManager::getHubNames(hintedUser.user->getCID(), hintedUser.hint, true);
		if (hubNames.empty())
			hubNames = ClientManager::getHubNames(hintedUser.user->getCID(), Util::emptyString, false);
		if (hubNames.empty())
		{
			userOffline = true;
			text += lastHubName.empty() ? TSTRING(OFFLINE) : lastHubName;
		}
		else
		{
			lastHubName = Text::toT(hubNames[0]);
			text += lastHubName;
		}
	}
	setWindowTitle(text);
	if (offline != userOffline)
	{
		offline = userOffline;
		setDisconnected(offline);
	}
}

StringMap DirectoryListingFrame::getFrameLogParams() const
{
	StringMap params;
	const HintedUser& hintedUser = dl->getHintedUser();
	if (hintedUser.user && !(hintedUser.user->getFlags() & User::FAKE))
	{
		if (!hintedUser.hint.empty())
		{
			params["hubNI"] = ClientManager::getInstance()->getOnlineHubName(hintedUser.hint);
			params["hubURL"] = hintedUser.hint;
		}
		params["userCID"] = hintedUser.user->getCID().toBase32();
		params["userNI"] = hintedUser.getNick();
		params["myCID"] = ClientManager::getMyCID().toBase32();
	}
	return params;
}

void DirectoryListingFrame::loadFile(const tstring& name, const tstring& dir)
{
	loadStartTime = GET_TICK();
	ctrlStatus.SetText(STATUS_TEXT, CTSTRING(LOADING_FILE_LIST));
	//don't worry about cleanup, the object will delete itself once the thread has finished it's job
	ThreadedDirectoryListing* tdl = new ThreadedDirectoryListing(this, ThreadedDirectoryListing::MODE_LOAD_FILE);
	tdl->setFile(Text::fromT(name));
	tdl->setDir(dir);
	loading = true;
	try
	{
		tdl->start(0, THREAD_NAME);
	}
	catch (const ThreadException& e)
	{
		delete tdl;
		loading = false;
		LogManager::message("DirectoryListingFrame::loadFile error: " + e.getError());
	}
}

void DirectoryListingFrame::loadXML(const string& txt)
{
	loadStartTime = GET_TICK();
	ctrlStatus.SetText(STATUS_TEXT, CTSTRING(LOADING_FILE_LIST));
	//don't worry about cleanup, the object will delete itself once the thread has finished it's job
	ThreadedDirectoryListing* tdl = new ThreadedDirectoryListing(this, ThreadedDirectoryListing::MODE_LOAD_PARTIAL_LIST);
	tdl->setText(txt);
	loading = true;
	try
	{
		tdl->start(0, THREAD_NAME);
	}
	catch (const ThreadException& e)
	{
		delete tdl;
		loading = false;
		LogManager::message("DirectoryListingFrame::loadXML error: " + e.getError());
	}
}

LRESULT DirectoryListingFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	m_hAccel = LoadAccelerators(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDR_FILELIST));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	if (dclstFlag || searchResultsFlag)
	{
		HICON icon = g_iconBitmaps.getIcon(dclstFlag ? IconBitmaps::DCLST : IconBitmaps::FILELIST_SEARCH, 0);
		SetIcon(icon, FALSE);
		SetIcon(icon, TRUE);
	}

	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | DS_CONTROL | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.SetFont(Fonts::g_systemFont);

	ctrlTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | WinUtil::getTreeViewStyle(), WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	WinUtil::setTreeViewTheme(ctrlTree, Colors::isDarkTheme);
	treeContainer.SubclassWindow(ctrlTree);

	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_FILES);
	listContainer.SubclassWindow(ctrlList);
	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));

	hTheme = OpenThemeData(m_hWnd, L"EXPLORER::LISTVIEW");
	if (hTheme)
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED;
	customDrawState.flags |= CustomDrawHelpers::FLAG_GET_COLFMT;

	setListViewColors(ctrlList);
	setTreeViewColors(ctrlTree);

	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));

	const auto* ss = SettingsManager::instance.getUiSettings();
	ctrlList.insertColumns(Conf::DIRLIST_FRAME_ORDER, Conf::DIRLIST_FRAME_WIDTHS, Conf::DIRLIST_FRAME_VISIBLE);
	ctrlList.setSortFromSettings(ss->getInt(Conf::DIRLIST_FRAME_SORT));

	ctrlTree.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);

	navWnd.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, WS_EX_COMPOSITED);
	navWnd.navBar.setCallback(this);

	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
	SetSplitterPanes(ctrlTree.m_hWnd, nullptr);
	m_nProportionalPos = ss->getInt(Conf::DIRLIST_FRAME_SPLIT);
	int icon = dclstFlag ? FileImage::DIR_DCLST : FileImage::DIR_ICON;
	nick = dclstFlag ? Util::getFileName(getFileName()) : (dl->getUser() ? dl->getUser()->getLastNick() : Util::emptyString);
	tstring rootText = getRootItemText();
	treeRoot = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
	                               rootText.c_str(), icon, icon, 0, 0,
	                               NULL, NULL, TVI_LAST);
	dcassert(treeRoot != NULL);

	memset(statusSizes, 0, sizeof(statusSizes));
	ctrlStatus.SetParts(STATUS_LAST, statusSizes);

	navWnd.initToolbars(this);
	disableControls();

	SettingsManager::instance.addListener(this);
	closed = false;
	bHandled = FALSE;

	return 1;
}

void DirectoryListingFrame::createFileMenus()
{
	if (!targetMenu)
	{
		targetMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(targetMenu);
	}
	if (!priorityMenu)
	{
		priorityMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(priorityMenu);
		MenuHelper::appendPrioItems(priorityMenu, IDC_DOWNLOAD_WITH_PRIO);
	}
}

void DirectoryListingFrame::createDirMenus()
{
	if (!targetDirMenu)
	{
		targetDirMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(targetDirMenu);
	}
	if (!priorityDirMenu)
	{
		priorityDirMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(priorityDirMenu);
		MenuHelper::appendPrioItems(priorityDirMenu, IDC_DOWNLOAD_WITH_PRIO_TREE);
	}
}

void DirectoryListingFrame::destroyMenus()
{
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
	if (targetDirMenu)
	{
		MenuHelper::removeStaticMenu(targetDirMenu);
		targetDirMenu.DestroyMenu();
	}
	if (priorityDirMenu)
	{
		MenuHelper::removeStaticMenu(priorityDirMenu);
		priorityDirMenu.DestroyMenu();
	}
}

LRESULT DirectoryListingFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
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

tstring DirectoryListingFrame::getRootItemText() const
{
	tstring result = Text::toT(nick);
	if (searchResultsFlag)
		result += TSTRING(SEARCH_RESULTS_SUFFIX);
	else
	if (filteredListFlag)
		result += TSTRING(FILTERED_LIST_SUFFIX);
	return result;
}

void DirectoryListingFrame::updateRootItemText()
{
	tstring name = getRootItemText();
	TVITEM item = {};
	item.hItem = treeRoot;
	item.mask = TVIF_TEXT;
	item.pszText = const_cast<TCHAR*>(name.c_str());
	ctrlTree.SetItem(&item);
}

void DirectoryListingFrame::startLoading()
{
	ctrlStatus.SetText(STATUS_TEXT, CTSTRING(LOADING_FILE_LIST));
	disableControls();
	loadStartTime = GET_TICK();
	loading = true;
	destroyTimer();
}

void DirectoryListingFrame::disableControls()
{
	ctrlTree.EnableWindow(FALSE);
	ctrlList.EnableWindow(FALSE);
	navWnd.EnableWindow(FALSE);
}

void DirectoryListingFrame::enableControls()
{
	ctrlTree.EnableWindow(TRUE);
	ctrlList.EnableWindow(TRUE);
	navWnd.EnableWindow(TRUE);
	createTimer(1000);
}

void DirectoryListingFrame::updateTree(DirectoryListing::Directory* tree, HTREEITEM treeItem)
{
	if (tree)
	{
		for (auto i = tree->directories.cbegin(); i != tree->directories.cend(); ++i)
		{
			if (!loading && !isClosedOrShutdown())
				throw AbortException(STRING(ABORT_EM));

			DirectoryListing::Directory *dir = *i;
			const tstring name = Text::toT(dir->getName());
			const auto typeDirectory = getDirectoryIcon(dir);

			HTREEITEM ht = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM, name.c_str(), typeDirectory, typeDirectory, 0, 0, (LPARAM) dir, treeItem, TVI_LAST);
			dir->setUserData(static_cast<void*>(ht));
			if (dir->getAdls())
				ctrlTree.SetItemState(ht, TVIS_BOLD, TVIS_BOLD);

			updateTree(dir, ht);
		}
	}
}

void DirectoryListingFrame::refreshTree(DirectoryListing::Directory* dir, HTREEITEM treeItem, bool insertParent, const string& selPath)
{
	if (!loading && !isClosedOrShutdown())
		throw AbortException(STRING(ABORT_EM));

	ctrlStatus.SetText(STATUS_TEXT, CTSTRING(PREPARING_FILE_LIST));
	CLockRedraw<> lockRedraw(ctrlTree);
	refreshing = true;
	HTREEITEM next = nullptr;
	while ((next = ctrlTree.GetChildItem(treeItem)) != nullptr)
		ctrlTree.DeleteItem(next);
	if (insertParent)
	{
		const tstring name = Text::toT(dir->getName());
		const auto typeDirectory = getDirectoryIcon(dir);
		HTREEITEM ht = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM, name.c_str(), typeDirectory, typeDirectory, 0, 0, (LPARAM) dir, treeItem, TVI_LAST);
		dir->setUserData(static_cast<void*>(ht));
		treeItem = ht;
	}
	updateTree(dir, treeItem);
	const auto typeDirectory = getDirectoryIcon(dir);
	dir->setUserData(static_cast<void*>(treeItem));
	ctrlTree.SetItemData(treeItem, DWORD_PTR(dir));
	ctrlTree.SetItemImage(treeItem, typeDirectory, typeDirectory);
	ctrlTree.Expand(treeItem);
	refreshing = false;
	HTREEITEM selItem = treeItem;
	if (!selPath.empty())
	{
		DirectoryListing::Directory* subdir = dl->findDirPath(selPath);
		if (subdir)
			selItem = static_cast<HTREEITEM>(subdir->getUserData());
	}
	ctrlTree.SelectItem(selItem);
}

void DirectoryListingFrame::updateStatus()
{
	if (!isClosedOrShutdown() && !updating && ctrlStatus.IsWindow())
	{
		tstring tmp;
		int cnt = ctrlList.GetSelectedCount();
		int64_t total = 0;
		if (cnt == 0)
		{
			cnt = ctrlList.GetItemCount();
			total = ctrlList.forEachT(ItemInfo::TotalSize()).total;
			tmp = TSTRING(DISPLAYED_ITEMS);
		}
		else
		{
			total = ctrlList.forEachSelectedT(ItemInfo::TotalSize()).total;
			tmp = TSTRING(SELECTED_ITEMS);
		}

		tmp += Util::toStringT(cnt);
		bool u = false;

		int w = WinUtil::getTextWidth(tmp, ctrlStatus) + STATUS_PART_PADDING;
		if (statusSizes[STATUS_SELECTED_FILES] < w)
		{
			statusSizes[STATUS_SELECTED_FILES] = w;
			u = true;
		}
		ctrlStatus.SetText(STATUS_SELECTED_FILES, tmp.c_str());

		tmp = TSTRING(SIZE) + _T(": ") + Util::formatBytesT(total);
		w = WinUtil::getTextWidth(tmp, ctrlStatus) + STATUS_PART_PADDING;
		if (statusSizes[STATUS_SELECTED_SIZE] < w)
		{
			statusSizes[STATUS_SELECTED_SIZE] = w;
			u = true;
		}
		ctrlStatus.SetText(STATUS_SELECTED_SIZE, tmp.c_str());

		if (u)
			UpdateLayout(TRUE);

		listItemChanged = false;
		updateWindowTitle();
	}
}

void DirectoryListingFrame::initStatus()
{
	const DirectoryListing::Directory *root = dl->getRoot();
	size_t files = root->getTotalFileCount();
	string size = Util::formatBytes(root->getTotalSize());

	tstring tmp = TSTRING(TOTAL_FILES) + Util::toStringT(files);
	statusSizes[STATUS_TOTAL_FILES] = WinUtil::getTextWidth(tmp, ctrlStatus) + STATUS_PART_PADDING;
	ctrlStatus.SetText(STATUS_TOTAL_FILES, tmp.c_str());

	tmp = TSTRING(TOTAL_FOLDERS) + Util::toStringT(root->getTotalFolderCount());
	statusSizes[STATUS_TOTAL_FOLDERS] = WinUtil::getTextWidth(tmp, ctrlStatus) + STATUS_PART_PADDING;
	ctrlStatus.SetText(STATUS_TOTAL_FOLDERS, tmp.c_str());

	tmp = TSTRING(TOTAL_SIZE) + Util::formatBytesT(root->getTotalSize());
	statusSizes[STATUS_TOTAL_SIZE] = WinUtil::getTextWidth(tmp, ctrlStatus) + STATUS_PART_PADDING;
	ctrlStatus.SetText(STATUS_TOTAL_SIZE, tmp.c_str());

	tmp = TSTRING(SPEED) + _T(": ") + Util::formatBytesT(speed) + _T('/') + TSTRING(S);
	statusSizes[STATUS_SPEED] = WinUtil::getTextWidth(tmp, ctrlStatus) + STATUS_PART_PADDING;
	ctrlStatus.SetText(STATUS_SPEED, tmp.c_str());

	UpdateLayout(FALSE);
}

LRESULT DirectoryListingFrame::onSelChangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (refreshing)
		return 0;

	NMTREEVIEW* p = reinterpret_cast<NMTREEVIEW*>(pnmh);
	if (p->itemNew.state & TVIS_SELECTED)
	{
		DirectoryListing::Directory* d = reinterpret_cast<DirectoryListing::Directory*>(p->itemNew.lParam);
		if (d)
		{
			selectedDir = p->itemNew.hItem;
			changeDir(d);
			if (!changingPath)
			{
				string path = dl->getPath(d);
				if (path.empty()) path += PATH_SEPARATOR;
				addNavHistory(path);
			}
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onListItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLISTVIEW* p = reinterpret_cast<NMLISTVIEW*>(pnmh);
	if (p->iItem >= 0 && (p->uNewState & LVIS_SELECTED))
	{
		const ItemInfo *info = ctrlList.getItemData(p->iItem);
		if (info->type == ItemInfo::FILE && info->file->isSet(DirectoryListing::FLAG_FOUND))
		{
			auto& s = search[SEARCH_CURRENT];
			if (!(s.getWhatFound() == DirectoryListing::SearchContext::FOUND_FILE && s.getFile() == info->file) && s.setFound(info->file))
				changeFoundItem();
		}
	}
	listItemChanged = true;
	return 0;
}

void DirectoryListingFrame::addNavHistory(const string& name)
{
	auto i = std::find(navHistory.begin(), navHistory.end(), name);
	if (i != navHistory.end()) navHistory.erase(i);
	while (navHistory.size() > MAX_NAVIGATION_HISTORY)
		navHistory.pop_front();
	navHistory.push_back(name);
	navHistoryIndex = navHistory.size() - 1;
	navWnd.enableNavigationButton(IDC_NAVIGATION_FORWARD, false);
	if (navHistory.size() > 1) navWnd.enableNavigationButton(IDC_NAVIGATION_BACK, true);
}

void DirectoryListingFrame::addTypedHistory(const string& name)
{
	auto i = std::find(typedHistory.begin(), typedHistory.end(), name);
	if (i != typedHistory.end()) typedHistory.erase(i);
	while (typedHistory.size() > MAX_TYPED_HISTORY)
		typedHistory.pop_front();
	typedHistory.push_back(name);
	++typedHistoryState;
}

// Choose folder icon (normal, DVD, BluRay)
FileImage::TypeDirectoryImages DirectoryListingFrame::getDirectoryIcon(const DirectoryListing::Directory* dir) const
{
	if (dclstFlag && !dir->getParent())
		return FileImage::DIR_DCLST;

	if (!dir->getComplete())
		return FileImage::DIR_MASKED;

	// Check subfolders
	for (auto i = dir->directories.cbegin(); i != dir->directories.cend(); ++i)
	{
		const string& nameSubDirectory = (*i)->getName();

		// Has BDMV folder
		if (FileImage::isBdFolder(nameSubDirectory))
			return FileImage::DIR_BD;

		// Has VIDEO_TS or AUDIO_TS
		if (FileImage::isDvdFolder(nameSubDirectory))
			return FileImage::DIR_DVD;
	}

	// Check files
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const string& nameFile = (*i)->getName();
		if (FileImage::isDvdFile(nameFile))
			return FileImage::DIR_DVD;
	}

	return FileImage::DIR_ICON;
}

void DirectoryListingFrame::changeDir(const DirectoryListing::Directory* dir)
{
	const string path = dl->getPath(dir);
	navWnd.updateNavBar(dir, path, this);

	showDirContents(dir, nullptr, nullptr);
	auto& s = search[SEARCH_CURRENT];
	if (dir->isSet(DirectoryListing::FLAG_FOUND) &&
		!(s.getWhatFound() == DirectoryListing::SearchContext::FOUND_DIR && s.getDirectory() == dir))
	{
		if (s.setFound(dir)) changeFoundItem();
	}
	if (!dir->getComplete())
	{
		if (dl->getUser()->isOnline())
		{
			try
			{
				QueueItem::MaskType flags = QueueItem::FLAG_PARTIAL_LIST;
				if (WinUtil::isShift()) flags |= QueueItem::FLAG_RECURSIVE_LIST;
				QueueManager::getInstance()->addList(dl->getHintedUser(), flags, 0, path);
				ctrlStatus.SetText(STATUS_TEXT, CTSTRING(DOWNLOADING_LIST));
			}
			catch (const QueueException& e)
			{
				dcassert(0);
				ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
			}
		}
		else
		{
			ctrlStatus.SetText(STATUS_TEXT, CTSTRING(USER_OFFLINE));
		}
	}
}

void DirectoryListingFrame::getParsedPath(const DirectoryListing::Directory* dir, vector<const DirectoryListing::Directory*>& path)
{
	path.clear();
	while (dir)
	{
		path.push_back(dir);
		dir = dir->getParent();
	}
	size_t last = path.size() - 1;
	size_t mid = path.size() / 2;
	for (size_t i = 0; i < mid; i++)
		std::swap(path[i], path[last-i]);
}

void DirectoryListingFrame::showDirContents(const DirectoryListing::Directory *dir, const DirectoryListing::Directory *selSubdir, const DirectoryListing::File *selFile)
{
	CWaitCursor waitCursor;
	ctrlList.SetRedraw(FALSE);
	ItemInfo *selectedItem = nullptr;
	updating = true;
	ctrlList.deleteAllNoLock();
	int count = 0;
	for (auto i = dir->directories.cbegin(); i != dir->directories.cend(); ++i)
	{
		DirectoryListing::Directory *subdir = *i;
		ItemInfo *info = new ItemInfo(subdir);
		ctrlList.insertItem(count++, info, I_IMAGECALLBACK);
		if (subdir == selSubdir) selectedItem = info;
	}
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		DirectoryListing::File *file = *i;
		ItemInfo *info = new ItemInfo(file, dl.get());
		ctrlList.insertItem(count++, info, I_IMAGECALLBACK);
		if (file == selFile) selectedItem = info;
	}
	ctrlList.resort();
	if (selectedItem)
	{
		int index = ctrlList.findItem(selectedItem);
		ctrlList.SelectItem(index);
	}
	updating = false;
	ctrlList.SetRedraw(TRUE);
	updateStatus();
}

void DirectoryListingFrame::selectFile(const DirectoryListing::File *file)
{
	int count = ctrlList.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		const ItemInfo *info = ctrlList.getItemData(i);
		if (info->type == ItemInfo::FILE && info->file == file)
		{
			ctrlList.SelectItem(i);
			break;
		}
	}
}

void DirectoryListingFrame::navigationUp()
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t == NULL)
		return;
	t = ctrlTree.GetParentItem(t);
	if (t == NULL)
		return;
	ctrlTree.SelectItem(t);
}

void DirectoryListingFrame::navigationBack()
{
	if (navHistory.size() > 1 && navHistoryIndex > 0)
	{
		changingPath++;
		tstring path = Text::toT(navHistory[--navHistoryIndex]);
		selectItem(path);
		if (navHistoryIndex == 0) navWnd.enableNavigationButton(IDC_NAVIGATION_BACK, false);
		navWnd.enableNavigationButton(IDC_NAVIGATION_FORWARD, true);
		changingPath--;
	}
}

void DirectoryListingFrame::navigationForward()
{
	if (navHistory.size() > 1 && navHistoryIndex + 1 < navHistory.size())
	{
		changingPath++;
		tstring path = Text::toT(navHistory[++navHistoryIndex]);
		selectItem(path);
		if (navHistoryIndex + 1 == navHistory.size()) navWnd.enableNavigationButton(IDC_NAVIGATION_FORWARD, false);
		navWnd.enableNavigationButton(IDC_NAVIGATION_BACK, true);
		changingPath--;
	}
}

LRESULT DirectoryListingFrame::onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(i);
		if (ii->type == ItemInfo::FILE)
		{
			string realPath;
			if (ShareManager::getInstance()->getFileInfo(ii->file->getTTH(), realPath))
				openFileFromList(Text::toT(realPath));
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	if ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlList.getItemData(i);
		if (ii->type == ItemInfo::FILE)
		{
			string realPath;
			if (ShareManager::getInstance()->getFileInfo(ii->file->getTTH(), realPath))
				WinUtil::openFolder(Text::toT(realPath));
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onMarkAsDownloaded(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto db = DatabaseManager::getInstance();
	auto hashDb = db->getHashDatabaseConnection();
	if (!hashDb) return 0;
	int i = -1;
	DirectoryListing::Directory* parent = nullptr;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlList.getItemData(i);
		if (ii->type == ItemInfo::FILE &&
		    ii->file->getSize() &&
		    !ii->file->isAnySet(DirectoryListing::FLAG_DOWNLOADED | DirectoryListing::FLAG_SHARED) &&
		    !ii->file->getTTH().isZero() &&
		    hashDb->putFileInfo(ii->file->getTTH().data, DatabaseManager::FLAG_DOWNLOADED, ii->file->getSize(), nullptr, false))
		{
			ii->file->setFlag(DirectoryListing::FLAG_DOWNLOADED);
			parent = ii->file->getParent();
		}
	}
	db->putHashDatabaseConnection(hashDb);
	if (parent)
	{
		DirectoryListing::Directory::updateFlags(parent);
		redraw();
	}
	return 0;
}

void DirectoryListingFrame::performDefaultAction(int index)
{
	const HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t || index == -1) return;
	const ItemInfo* ii = ctrlList.getItemData(index);
	if (ii->type == ItemInfo::FILE)
	{
		const tstring& path = ii->columns[COLUMN_PATH];
		if (!path.empty())
		{
			openFileFromList(path);
		}
		else
		{
			DirectoryListing::Directory* parent = nullptr;
			try
			{
				QueueItem::Priority prio = WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT;
				bool getConnFlag = true;
				parent = ii->file->getParent();
				if (Util::isDclstFile(ii->file->getName()))
					dl->download(ii->file, Text::fromT(ii->getText(COLUMN_FILENAME)), true, prio, true, getConnFlag);
				else
					dl->download(ii->file, Text::fromT(ii->getText(COLUMN_FILENAME)), false, prio, false, getConnFlag);
			}
			catch (const Exception& e)
			{
				ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
			}
			if (parent) DirectoryListing::Directory::updateFlags(parent);
			redraw();
		}
	}
	else
	{
		HTREEITEM ht = ctrlTree.GetChildItem(t);
		while (ht)
		{
			if ((DirectoryListing::Directory*)ctrlTree.GetItemData(ht) == ii->dir)
			{
				ctrlTree.SelectItem(ht);
				break;
			}
			ht = ctrlTree.GetNextSiblingItem(ht);
		}
	}
}

LRESULT DirectoryListingFrame::onDoubleClickFiles(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	performDefaultAction(item->iItem);
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadWithPrioTree(WORD, WORD wID, HWND, BOOL&)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;
	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;

	int prio = wID - IDC_DOWNLOAD_WITH_PRIO_TREE;
	if (!(prio >= QueueItem::PAUSED && prio < QueueItem::LAST))
		prio = WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT;

	QueueManager::getInstance()->startBatch();
	bool getConnFlag = true;
	try
	{
		dl->download(dir, Util::getDownloadDir(dl->getUser()), (QueueItem::Priority) prio, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	QueueManager::getInstance()->endBatch();
	auto parent = dir->getParent();
	if (parent) DirectoryListing::Directory::updateFlags(parent);
	redraw();
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadDirTo(WORD, WORD, HWND, BOOL&)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;
	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;

	tstring target = Text::toT(Util::getDownloadDir(dl->getUser()));
	if (WinUtil::browseDirectory(target, m_hWnd))
	{
		LastDir::add(target);
		QueueManager::getInstance()->startBatch();
		bool getConnFlag = true;
		try
		{
			dl->download(dir, Text::fromT(target), WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
		QueueManager::getInstance()->endBatch();
		auto parent = dir->getParent();
		if (parent) DirectoryListing::Directory::updateFlags(parent);
		redraw();
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadDirCustom(WORD, WORD wID, HWND, BOOL&)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;
	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;

	const string& useDir = wID == IDC_DOWNLOADDIRTO_USER? downloadDirNick : downloadDirIP;
	if (useDir.empty()) return 0;

	QueueManager::getInstance()->startBatch();
	bool getConnFlag = true;
	try
	{
		dl->download(dir, useDir, WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	QueueManager::getInstance()->endBatch();
	auto parent = dir->getParent();
	if (parent) DirectoryListing::Directory::updateFlags(parent);
	redraw();
	return 0;
}

void DirectoryListingFrame::downloadSelected(const tstring& target, bool view /* = false */, QueueItem::Priority prio /* = QueueItem::Priority::DEFAULT */)
{
	if (view || WinUtil::isShift()) prio = QueueItem::HIGHEST;
	int i = -1;
	bool getConnFlag = true;
	bool redrawFlag = false;
	auto fm = FavoriteManager::getInstance();
	DirectoryListing::Directory* parent = nullptr;
	int selCount = ctrlList.GetSelectedCount();
	if (selCount > 1) QueueManager::getInstance()->startBatch();
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(i);
		tstring itemTarget;
		if (target.empty())
		{
			string ext = Text::fromT(Util::getFileExt(ii->getText(COLUMN_FILENAME)));
			itemTarget = Text::toT(fm->getDownloadDirectory(ext, dl->getUser()));
		}
		else
			itemTarget = target;
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				tstring path = itemTarget + ii->getText(COLUMN_FILENAME);
				if (view)
					File::deleteFile(path);
				redrawFlag = true;
				parent = ii->file->getParent();
				dl->download(ii->file, Text::fromT(path), view, prio, false, getConnFlag);
			}
			else if (!view)
			{
				redrawFlag = true;
				parent = ii->dir->getParent();
				dl->download(ii->dir, Text::fromT(itemTarget), prio, getConnFlag);
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	if (selCount > 1) QueueManager::getInstance()->endBatch();
	if (redrawFlag)
	{
		if (parent) DirectoryListing::Directory::updateFlags(parent);
		redraw();
	}
}

LRESULT DirectoryListingFrame::onDownloadWithPrio(WORD, WORD wID, HWND, BOOL&)
{
	int prio = wID - IDC_DOWNLOAD_WITH_PRIO;
	if (!(prio >= QueueItem::PAUSED && prio < QueueItem::LAST))
		prio = QueueItem::DEFAULT;

	downloadSelected(false, (QueueItem::Priority) prio);
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadTo(WORD, WORD, HWND, BOOL&)
{
	string downloadDir = Util::getDownloadDir(dl->getUser());
	if (ctrlList.GetSelectedCount() == 1)
	{
		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		bool getConnFlag = true;
		bool redrawFlag = false;
		DirectoryListing::Directory* parent = nullptr;
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				tstring target = Text::toT(downloadDir) + ii->getText(COLUMN_FILENAME);
				if (WinUtil::browseFile(target, m_hWnd))
				{
					redrawFlag = true;
					parent = ii->file->getParent();
					LastDir::add(Util::getFilePath(target));
					dl->download(ii->file, Text::fromT(target), false,
						WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, false, getConnFlag);
				}
			}
			else
			{
				tstring target = Text::toT(downloadDir);
				if (WinUtil::browseDirectory(target, m_hWnd))
				{
					redrawFlag = true;
					parent = ii->dir->getParent();
					LastDir::add(target);
					dl->download(ii->dir, Text::fromT(target),
						WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
				}
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
		if (redrawFlag)
		{
			if (parent) DirectoryListing::Directory::updateFlags(parent);
			redraw();
		}
	}
	else
	{
		tstring target = Text::toT(downloadDir);
		if (WinUtil::browseDirectory(target, m_hWnd))
		{
			LastDir::add(target);
			downloadSelected(target);
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadCustom(WORD, WORD wID, HWND, BOOL&)
{
	const string& useDir = wID == IDC_DOWNLOADTO_USER? downloadDirNick : downloadDirIP;
	if (useDir.empty()) return 0;
	if (ctrlList.GetSelectedCount() == 1)
	{
		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		bool getConnFlag = true;
		DirectoryListing::Directory* parent = nullptr;
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				string filename = Text::fromT(ii->getText(COLUMN_FILENAME));
				parent = ii->file->getParent();
				dl->download(ii->file, useDir + filename, false,
					WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, false, getConnFlag);
			} else
			{
				parent = ii->dir->getParent();
				dl->download(ii->dir, useDir,
					WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
		if (parent) DirectoryListing::Directory::updateFlags(parent);
		redraw();
	}
	else
	{
		downloadSelected(Text::toT(useDir));
	}
	return 0;
}

#ifdef DEBUG_TRANSFERS
LRESULT DirectoryListingFrame::onDownloadByPath(WORD, WORD, HWND, BOOL&)
{
	if (ctrlList.GetSelectedCount() == 1)
	{
		DirectoryListing::Directory* parent = nullptr;
		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		bool getConnFlag = true;
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				DirectoryListing::File* file = ii->file;
				string path = Util::toAdcFile(dl->getPath(file) + file->getName());
				parent = file->getParent();
				dl->download(file, path, false,
					WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, false, getConnFlag);
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
		if (parent) DirectoryListing::Directory::updateFlags(parent);
		redraw();
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadAny(WORD, WORD, HWND, BOOL&)
{
	LineDlg dlg;
	dlg.title = _T("Download file by name");
	dlg.description = _T("Enter full path (ADC style)");
	dlg.allowEmpty = false;
	dlg.icon = IconBitmaps::DOWNLOAD;
	if (dlg.DoModal() != IDOK) return 0;
	string path = Text::fromT(dlg.line);
	try
	{
		bool getConnFlag = true;
		QueueManager::QueueItemParams params;
		params.sourcePath = path;
		auto pos = path.rfind('/');
		if (pos != string::npos) path.erase(0, pos + 1);
		QueueManager::getInstance()->add(path, params, dl->getHintedUser(), 0, 0, getConnFlag);
	}
	catch (const Exception& e)
	{
		LogManager::message(e.getError());
	}
	return 0;
}
#endif

#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
LRESULT DirectoryListingFrame::onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	downloadSelected(Text::toT(Util::getTempPath()), true);
	return 0;
}
#endif

LRESULT DirectoryListingFrame::onPerformWebSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (activeMenu == MENU_TREE)
	{
		const HTREEITEM t = ctrlTree.GetSelectedItem();
		if (!t) return 0;
		const DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
		if (dir) performWebSearch(wID, dir->getName());
		return 0;
	}
	const auto ii = ctrlList.getSelectedItem();
	if (!ii) return 0;
	if (ii->type == ItemInfo::FILE)
		performWebSearch(wID, ii->file->getName());
	else
		performWebSearch(wID, ii->dir->getName());
	return 0;
}

LRESULT DirectoryListingFrame::onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ItemInfo* ii = ctrlList.getSelectedItem();
	if (ii && ii->type == ItemInfo::FILE)
	{
		WinUtil::searchHash(ii->file->getTTH());
	}
	return 0;
}

LRESULT DirectoryListingFrame::onPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const HintedUser& pUser = dl->getHintedUser();
	if (pUser.user)
	{
		PrivateFrame::openWindow(nullptr, pUser);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onMatchQueueOrFindDups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ownList)
	{
		FindDuplicatesDlg dlg(findDupsOptions);
		if (dlg.DoModal(*this) == IDOK)
		{
			int64_t minSize = findDupsOptions.sizeMin;
			if (minSize > 0)
			{
				int sizeUnitShift = 0;
				if (findDupsOptions.sizeUnit > 0 && findDupsOptions.sizeUnit <= 3)
					sizeUnitShift = 10*findDupsOptions.sizeUnit;
				minSize <<= sizeUnitShift;
			}
			else
				minSize = 1;
			dl->findDuplicates(dupFiles, minSize);
			if (dupFiles.empty())
			{
				MessageBox(CTSTRING(DUPLICATE_FILES_NOT_FOUND), CTSTRING(SEARCH), MB_ICONINFORMATION);
				clearSearch();
				showingDupFiles = false;
			}
			else
			{
				showingDupFiles = true;
				goToFirstFound();
			}
			updateSearchButtons();
			redraw();
			showFound();
		}
	}
	else
	{
		int count = QueueManager::getInstance()->matchListing(*dl);
		tstring str = TPLURAL_F(PLURAL_MATCHED_FILES, count);
		ctrlStatus.SetText(STATUS_TEXT, str.c_str());
	}
	return 0;
}

LRESULT DirectoryListingFrame::onLocateInQueue(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = (int) wID - IDC_LOCATE_FILE_IN_QUEUE;
	if (index < 0 || index >= (int) targets.size())
	{
		dcassert(0);
		return 0;
	}
	QueueFrame::openWindow();
	if (QueueFrame::g_frame)
		QueueFrame::g_frame->showQueueItem(targets[index], false);
	return 0;
}

LRESULT DirectoryListingFrame::onListDiff(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string selectedFile;
	if (wID != IDC_FILELIST_DIFF2)
	{
		string pattern = nick + ".????????_????.*.xml.bz2";
		StringList files = File::findFiles(Util::getListPath(), pattern, false);
		if (!files.empty())
		{
			string loadedFile = Util::getFileName(fileName);
			auto i = std::remove(files.begin(), files.end(), loadedFile);
			if (i != files.end()) files.erase(i);
			if (!files.empty())
			{
				size_t titleLen = pattern.length() - 10;
				CMenu menu;
				menu.CreatePopupMenu();
				for (size_t i = 0; i < files.size(); ++i)
					menu.AppendMenu(MF_STRING, i + 1, Text::toT(files[i].substr(0, titleLen)).c_str());
				menu.AppendMenu(MF_SEPARATOR);
				menu.AppendMenu(MF_STRING, IDC_BROWSE, CTSTRING(BROWSE));
				RECT rc;
				int count = navWnd.ctrlButtons.GetButtonCount();
				navWnd.ctrlButtons.GetItemRect(count - 1, &rc);
				navWnd.ctrlButtons.ClientToScreen(&rc);
				int result = menu.TrackPopupMenu(TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTALIGN, rc.right, rc.bottom, m_hWnd);
				if (result != IDC_BROWSE)
				{
					if (result <= 0 || (size_t) result > files.size()) return 0;
					selectedFile = Util::getListPath() + files[result-1];
				}
			}
		}
	}
	if (selectedFile.empty())
	{
		tstring file;
		if (!WinUtil::browseFile(file, m_hWnd, false, Text::toT(Util::getListPath()), WinUtil::getFileMaskString(WinUtil::fileListsMask).c_str()) || file.empty())
			return 0;
		selectedFile = Text::fromT(file);
	}
	ctrlTree.SelectItem(NULL); // refreshTree won't select item without this
	startLoading();
	ThreadedDirectoryListing* tdl = new ThreadedDirectoryListing(this, ThreadedDirectoryListing::MODE_SUBTRACT_FILE);
	tdl->setFile(selectedFile);
	tdl->start(0, THREAD_NAME);
	return 0;
}

LRESULT DirectoryListingFrame::onListCompare(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
	if (!WinUtil::browseFile(file, m_hWnd, false, Text::toT(Util::getListPath()), WinUtil::getFileMaskString(WinUtil::fileListsMask).c_str()) || file.empty())
		return 0;
	startLoading();
	ThreadedDirectoryListing* tdl = new ThreadedDirectoryListing(this, ThreadedDirectoryListing::MODE_COMPARE_FILE);
	tdl->setFile(Text::fromT(file));
	tdl->start(0, THREAD_NAME);
	return 0;
}

LRESULT DirectoryListingFrame::onGoToDirectory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlList.GetSelectedCount() != 1)
		return 0;

	tstring fullPath;
	const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
	if (ii->type == ItemInfo::FILE)
	{
		if (!ii->file->getAdls())
			return 0;
		fullPath = Text::toT(static_cast<const DirectoryListing::AdlFile*>(ii->file)->getFullPath());
	}
	else if (ii->type == ItemInfo::DIRECTORY)
	{
		if (!(ii->dir->getAdls() && ii->dir->getParent() != dl->getRoot()))
			return 0;
		fullPath = Text::toT(static_cast<const DirectoryListing::AdlDirectory*>(ii->dir)->getFullPath());
	}

	selectItem(fullPath);

	return 0;
}

void DirectoryListingFrame::goToFirstFound()
{
	search[SEARCH_CURRENT].goToFirstFound(dl->getRoot());
	search[SEARCH_PREV].clear();
	search[SEARCH_NEXT] = search[SEARCH_CURRENT];
	if (!search[SEARCH_NEXT].next())
		search[SEARCH_NEXT].clear();
}

HTREEITEM DirectoryListingFrame::findItem(HTREEITEM ht, const tstring& name)
{
	string::size_type i = name.find('\\');
	if (i == string::npos)
		return ht;

	for (HTREEITEM child = ctrlTree.GetChildItem(ht); child != NULL; child = ctrlTree.GetNextSiblingItem(child))
	{
		DirectoryListing::Directory* d = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(child));
		if (d && Text::toT(d->getName()) == name.substr(0, i))
			return findItem(child, name.substr(i + 1));
	}
	return NULL;
}

void DirectoryListingFrame::selectItem(const tstring& name)
{
	const HTREEITEM ht = findItem(treeRoot, name);
	if (ht != NULL)
	{
		ctrlTree.EnsureVisible(ht);
		ctrlTree.SelectItem(ht);
	}
}

void DirectoryListingFrame::selectItem(const TTHValue& tth)
{
	if (loading) return;
	const DirectoryListing::File* f = dl->getRoot()->findFileByHash(tth);
	if (!f) return;
	const DirectoryListing::Directory* dir = f->getParent();
	HTREEITEM ht = static_cast<HTREEITEM>(dir->getUserData());
	ctrlTree.EnsureVisible(ht);
	ctrlTree.SelectItem(ht);
	selectFile(f);
}

void DirectoryListingFrame::appendFavTargets(OMenu& menu, const int idc)
{
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& dirs = lockedInstance.getFavoriteDirs();
	if (!dirs.empty())
	{
		int n = 0;
		for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
		{
			tstring tmp = Text::toT(i->name);
			WinUtil::escapeMenu(tmp);
			menu.AppendMenu(MF_STRING, idc + n, tmp.c_str());
			if (++n == MAX_FAV_DIRS) break;
		}
		menu.AppendMenu(MF_SEPARATOR);
	}
}

void DirectoryListingFrame::appendCustomTargetItems(OMenu& menu, int idc)
{
	User* user = dl->getUser().get();
	string downloadDir = Util::getDownloadDir(dl->getUser());
	string nick = user->getLastNick();
	if (!nick.empty())
	{
		downloadDirNick = downloadDir + nick;
		tstring tmp = Text::toT(downloadDirNick);
		WinUtil::escapeMenu(tmp);
		menu.AppendMenu(MF_STRING, idc, tmp.c_str());
		Util::appendPathSeparator(downloadDirNick);
	}
	Ip4Address ip4 = user->getIP4();
	if (ip4)
	{
		downloadDirIP = downloadDir + Util::printIpAddress(ip4);
		tstring tmp = Text::toT(downloadDirIP);
		WinUtil::escapeMenu(tmp);
		menu.AppendMenu(MF_STRING, idc + 1, tmp.c_str());
		Util::appendPathSeparator(downloadDirIP);
	}
	else
	{
		Ip6Address ip6 = user->getIP6();
		if (!Util::isEmpty(ip6))
		{
			downloadDirIP = downloadDir + Util::printIpAddress(ip6);
			tstring tmp = Text::toT(downloadDirIP);
			WinUtil::escapeMenu(tmp);
			menu.AppendMenu(MF_STRING, idc + 1, tmp.c_str());
			Util::appendPathSeparator(downloadDirIP);
		}
	}
}

LRESULT DirectoryListingFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	int selCount;

	contextMenuHubUrl.clear();
	string hubUrl;
	const HintedUser& hintedUser = dl->getHintedUser();
	if (hintedUser.user && !(hintedUser.user->getFlags() & User::FAKE))
		hubUrl = hintedUser.hint;

	if (reinterpret_cast<HWND>(wParam) == ctrlList && (selCount = ctrlList.GetSelectedCount()) > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

		if (pt.x == -1 && pt.y == -1)
			WinUtil::getContextMenuPos(ctrlList, pt);

		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));

		createFileMenus();
		targetMenu.ClearMenu();

		OMenu fileMenu;
		fileMenu.CreatePopupMenu();

		bool existingFile = false;
		bool hasTTH = selCount == 1 && ii->type == ItemInfo::FILE && ii->file->getSize() && !ii->file->getTTH().isZero();
		if (selCount == 1 && ii->type == ItemInfo::FILE)
			existingFile = ownList ? true : ShareManager::getInstance()->isTTHShared(ii->file->getTTH());
		if (existingFile)
		{
			fileMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
			fileMenu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
			fileMenu.AppendMenu(MF_SEPARATOR);
		}
		if (!ownList)
		{
			fileMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_WITH_PRIO + DEFAULT_PRIO, CTSTRING(DOWNLOAD), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD, 0));
#ifdef DEBUG_TRANSFERS
			if (selCount == 1)
				fileMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_BY_PATH, _T("Download by path"));
#endif
		}
		if (showingDupFiles && selCount == 1 && ii->type == ItemInfo::FILE && ii->file->isAnySet(DirectoryListing::FLAG_FOUND))
			fileMenu.AppendMenu(MF_STRING, IDC_SHOW_DUPLICATES, CTSTRING(SHOW_DUPLICATES));
		if (!ownList)
		{
			fileMenu.AppendMenu(MF_POPUP, targetMenu, CTSTRING(DOWNLOAD_TO));
			fileMenu.AppendMenu(MF_POPUP, priorityMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
		}
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
		fileMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT), g_iconBitmaps.getBitmap(IconBitmaps::NOTEPAD, 0));
#endif
		fileMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
		fileMenu.AppendMenu(MF_STRING, IDC_MARK_AS_DOWNLOADED, CTSTRING(MARK_AS_DOWNLOADED));

		int webSearchIndex =
			appendWebSearchItems(fileMenu, SearchUrl::KEYWORD, true, ResourceManager::WEB_SEARCH_KEYWORD) ?
			fileMenu.GetMenuItemCount() - 1 : -1;

		targets.clear();
		if (hasTTH)
		{
			QueueManager::getTargets(ii->file->getTTH(), targets, 10);
			if (!targets.empty())
			{
				if (targets.size() > 1)
				{
					CMenu locateMenu;
					locateMenu.CreatePopupMenu();
					for (size_t i = 0; i < targets.size(); ++i)
					{
						tstring target = Text::toT(targets[i]);
						WinUtil::escapeMenu(target);
						locateMenu.AppendMenu(MF_STRING, IDC_LOCATE_FILE_IN_QUEUE + i, target.c_str());
					}
					fileMenu.AppendMenu(MF_POPUP, locateMenu, CTSTRING(LOCATE_FILE_IN_QUEUE));
					locateMenu.Detach();
				}
				else
					fileMenu.AppendMenu(MF_STRING, IDC_LOCATE_FILE_IN_QUEUE, CTSTRING(LOCATE_FILE_IN_QUEUE));
			}
		}

		fileMenu.AppendMenu(MF_SEPARATOR);

		if (ownList)
		{
			fileMenu.AppendMenu(MF_STRING, IDC_GENERATE_DCLST_FILE, CTSTRING(DCLS_GENERATE_LIST), g_iconBitmaps.getBitmap(IconBitmaps::DCLST, 0));
			fileMenu.AppendMenu(MF_SEPARATOR);
		}

		CMenu copyMenu;
		copyMenu.CreatePopupMenu();
		if (dl->getUser() && !dl->getUser()->getCID().isZero())
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));

		fileMenu.AppendMenu(MF_POPUP, copyMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
		fileMenu.AppendMenu(MF_SEPARATOR);

		activeMenu = MENU_LIST;
		if (selCount == 1 && ii->type == ItemInfo::FILE)
		{
			if (!hasTTH)
				fileMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MF_BYCOMMAND | MFS_DISABLED);
			if (!hasTTH || ii->file->isAnySet(DirectoryListing::FLAG_DOWNLOADED | DirectoryListing::FLAG_SHARED))
				fileMenu.EnableMenuItem(IDC_MARK_AS_DOWNLOADED, MF_BYCOMMAND | MFS_DISABLED);
			fileMenu.EnableMenuItem(IDC_GENERATE_DCLST_FILE, MF_BYCOMMAND | MFS_DISABLED);

			//Append Favorite download dirs.
			if (!ownList)
			{
				appendFavTargets(targetMenu, IDC_DOWNLOAD_TO_FAV);
				targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOADTO, CTSTRING(BROWSE));
				appendCustomTargetItems(targetMenu, IDC_DOWNLOADTO_USER);

				int n = 0;
				LastDir::appendItems(targetMenu, n);
			}

			if (ii->file->getAdls())
				fileMenu.AppendMenu(MF_STRING, IDC_GO_TO_DIRECTORY, CTSTRING(GO_TO_DIRECTORY));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_EXACT_SIZE, CTSTRING(EXACT_SIZE));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_PATH, CTSTRING(LOCAL_PATH));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
			if (ownList)
				appendCopyUrlItems(copyMenu, IDC_COPY_URL, ResourceManager::FILE_URL);
			else if (!hubUrl.empty())
			{
				copyMenu.AppendMenu(MF_STRING, IDC_COPY_URL, CTSTRING(FILE_URL));
				contextMenuHubUrl.push_back(hubUrl);
			}

			if (hintedUser.user)
				appendUcMenu(fileMenu, UserCommand::CONTEXT_FILELIST, ClientManager::getHubs(hintedUser.user->getCID(), hintedUser.hint));
			fileMenu.SetMenuDefaultItem(existingFile ? IDC_OPEN_FILE : IDC_DOWNLOAD_WITH_PRIO + DEFAULT_PRIO);
			copyMenu.Detach();
			fileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			cleanUcMenu(fileMenu);
		}
		else
		{
			bool isDirectory = selCount == 1 && ii->type == ItemInfo::DIRECTORY;
			fileMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MF_BYCOMMAND | MFS_DISABLED);
			if (selCount > 1 && webSearchIndex != -1)
				fileMenu.EnableMenuItem(webSearchIndex, MF_BYPOSITION | MFS_DISABLED);
			if (!isDirectory)
				fileMenu.EnableMenuItem(IDC_GENERATE_DCLST_FILE, MF_BYCOMMAND | MFS_DISABLED);
			if (isDirectory)
				fileMenu.EnableMenuItem(IDC_MARK_AS_DOWNLOADED, MF_BYCOMMAND | MFS_DISABLED);
			//Append Favorite download dirs
			if (!ownList)
			{
				appendFavTargets(targetMenu, IDC_DOWNLOAD_TO_FAV);
				targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOADTO, CTSTRING(BROWSE));
				appendCustomTargetItems(targetMenu, IDC_DOWNLOADTO_USER);
			}

			int n = 0;
			LastDir::appendItems(targetMenu, n);
			if (isDirectory && ii->dir->getAdls() && ii->dir->getParent() != dl->getRoot())
				fileMenu.AppendMenu(MF_STRING, IDC_GO_TO_DIRECTORY, CTSTRING(GO_TO_DIRECTORY));

			copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, isDirectory ? CTSTRING(FOLDERNAME) : CTSTRING(FILENAME));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_EXACT_SIZE, CTSTRING(EXACT_SIZE));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_PATH, CTSTRING(LOCAL_PATH));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
			if (isDirectory)
			{
				if (ownList)
					appendCopyUrlItems(copyMenu, IDC_COPY_URL, ResourceManager::FOLDER_URL);
				else if (!hubUrl.empty())
				{
					copyMenu.AppendMenu(MF_STRING, IDC_COPY_URL, CTSTRING(FOLDER_URL));
					contextMenuHubUrl.push_back(hubUrl);
				}
			}
			if (isDirectory)
				copyMenu.EnableMenuItem(IDC_COPY_TTH, MF_BYCOMMAND | MFS_DISABLED);
			copyMenu.EnableMenuItem(IDC_COPY_LINK, MF_BYCOMMAND | MFS_DISABLED);
			copyMenu.EnableMenuItem(IDC_COPY_WMLINK, MF_BYCOMMAND | MFS_DISABLED);

			if (hintedUser.user)
				appendUcMenu(fileMenu, UserCommand::CONTEXT_FILELIST, ClientManager::getHubs(hintedUser.user->getCID(), hintedUser.hint));
			copyMenu.Detach();
			fileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			cleanUcMenu(fileMenu);
		}
		MenuHelper::unlinkStaticMenus(fileMenu);
		return TRUE;
	}
	if (reinterpret_cast<HWND>(wParam) == ctrlTree && ctrlTree.GetSelectedItem() != NULL)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlTree, pt);
		}
		else
		{
			ctrlTree.ScreenToClient(&pt);
			UINT a = 0;
			HTREEITEM ht = ctrlTree.HitTest(pt, &a);
			if (ht != NULL && ht != ctrlTree.GetSelectedItem())
				ctrlTree.SelectItem(ht);
			ctrlTree.ClientToScreen(&pt);
		}
		// Strange, windows doesn't change the selection on right-click... (!)

		createDirMenus();
		targetDirMenu.ClearMenu();

		CMenu copyDirMenu;
		OMenu directoryMenu;
		directoryMenu.CreatePopupMenu();

		HTREEITEM ht = ctrlTree.GetSelectedItem();
		if (ht != treeRoot)
		{
			copyDirMenu.CreatePopupMenu();
			copyDirMenu.AppendMenu(MF_STRING, IDC_COPY_FOLDER_NAME, CTSTRING(FOLDERNAME));
			const DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(ht));
			if (dir && !dir->getAdls())
			{
				copyDirMenu.AppendMenu(MF_STRING, IDC_COPY_FOLDER_PATH, CTSTRING(FULL_PATH));
				if (ownList)
					appendCopyUrlItems(copyDirMenu, IDC_COPY_URL_TREE, ResourceManager::FOLDER_URL);
				else if (!hubUrl.empty())
				{
					copyDirMenu.AppendMenu(MF_STRING, IDC_COPY_URL_TREE, CTSTRING(FOLDER_URL));
					contextMenuHubUrl.push_back(hubUrl);
				}
			}
		}

		if (!ownList)
		{
			directoryMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_WITH_PRIO_TREE + DEFAULT_PRIO, CTSTRING(DOWNLOAD), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD, 0));
			directoryMenu.AppendMenu(MF_POPUP, targetDirMenu, CTSTRING(DOWNLOAD_TO));
			directoryMenu.AppendMenu(MF_POPUP, priorityDirMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
#ifdef DEBUG_TRANSFERS
			directoryMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_ANY, _T("Download file by name..."));
#endif

			appendFavTargets(targetDirMenu, IDC_DOWNLOADDIR_TO_FAV);
			targetDirMenu.AppendMenu(MF_STRING, IDC_DOWNLOADDIRTO, CTSTRING(BROWSE));
			appendCustomTargetItems(targetDirMenu, IDC_DOWNLOADDIRTO_USER);

			int n = IDC_DOWNLOAD_TARGET_TREE - IDC_DOWNLOAD_TARGET;
			LastDir::appendItems(targetDirMenu, n);

			appendWebSearchItems(directoryMenu, SearchUrl::KEYWORD, true, ResourceManager::WEB_SEARCH_KEYWORD);
			if (copyDirMenu)
			{
				directoryMenu.AppendMenu(MF_SEPARATOR);
				directoryMenu.AppendMenu(MF_POPUP, copyDirMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
				copyDirMenu.Detach();
			}
		}
		else
		{
			appendWebSearchItems(directoryMenu, SearchUrl::KEYWORD, true, ResourceManager::WEB_SEARCH_KEYWORD);
			directoryMenu.AppendMenu(MF_STRING, IDC_GENERATE_DCLST, CTSTRING(DCLS_GENERATE_LIST), g_iconBitmaps.getBitmap(IconBitmaps::DCLST, 0));
			if (copyDirMenu)
			{
				directoryMenu.AppendMenu(MF_SEPARATOR);
				directoryMenu.AppendMenu(MF_POPUP, copyDirMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
				copyDirMenu.Detach();
			}
		}
		directoryMenu.AppendMenu(MF_SEPARATOR);
		directoryMenu.AppendMenu(MF_STRING, IDC_FILELIST_DIFF2, CTSTRING(FILE_LIST_DIFF2));
		directoryMenu.AppendMenu(MF_STRING, IDC_FILELIST_COMPARE, CTSTRING(FILE_LIST_COMPARE));
		if (originalId && findFrameByID(originalId))
			directoryMenu.AppendMenu(MF_STRING, IDC_GOTO_ORIGINAL, CTSTRING(GOTO_ORIGINAL), g_iconBitmaps.getBitmap(IconBitmaps::GOTO_FILELIST, 0));

		activeMenu = MENU_TREE;
		directoryMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		MenuHelper::unlinkStaticMenus(directoryMenu);
		return TRUE;
	}

	bHandled = FALSE;
	return FALSE;
}

LRESULT DirectoryListingFrame::onXButtonUp(UINT /*uMsg*/, WPARAM wParam, LPARAM /* lParam */, BOOL& /* bHandled */)
{
	if (GET_XBUTTON_WPARAM(wParam) & XBUTTON1)
	{
		navigationBack();
		return TRUE;
	}
	else if (GET_XBUTTON_WPARAM(wParam) & XBUTTON2)
	{
		navigationForward();
		return TRUE;
	}

	return FALSE;
}

LRESULT DirectoryListingFrame::onDownloadToLastDir(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_TARGET - 1;
	dcassert(newId >= 0);
	if (newId >= (int) LastDir::get().size())
	{
		dcassert(0);
		return 0;
	}

	if (ctrlList.GetSelectedCount())
		downloadSelected(LastDir::get()[newId]);
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadToLastDirTree(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_TARGET_TREE - 1;
	dcassert(newId >= 0);
	if (newId >= (int) LastDir::get().size())
	{
		dcassert(0);
		return 0;
	}

	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;

	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;

	QueueManager::getInstance()->startBatch();
	bool getConnFlag = true;
	try
	{
		dl->download(dir, Text::fromT(LastDir::get()[newId]),
			WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	QueueManager::getInstance()->endBatch();
	auto parent = dir->getParent();
	if (parent) DirectoryListing::Directory::updateFlags(parent);
	redraw();
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadToFavDir(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_TO_FAV;
	dcassert(newId >= 0);
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& spl = lockedInstance.getFavoriteDirs();
	if (newId >= (int) spl.size())
	{
		dcassert(0);
		return 0;
	}

	if (ctrlList.GetSelectedCount())
		downloadSelected(Text::toT(spl[newId].dir));
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadToFavDirTree(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOADDIR_TO_FAV;
	dcassert(newId >= 0);

	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;

	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;

	QueueManager::getInstance()->startBatch();
	bool getConnFlag = true;
	try
	{
		FavoriteManager::LockInstanceDirs lockedInstance;
		const auto& spl = lockedInstance.getFavoriteDirs();
		if (newId >= (int)spl.size())
		{
			dcassert(0);
			return 0;
		}
		dl->download(dir, spl[newId].dir,
			WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	QueueManager::getInstance()->endBatch();
	auto parent = dir->getParent();
	if (parent) DirectoryListing::Directory::updateFlags(parent);
	redraw();
	return 0;
}

LRESULT DirectoryListingFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	if (kd->wVKey == VK_BACK)
	{
		navigationUp();
	}
	else if (kd->wVKey == VK_TAB)
	{
		onTab();
	}
	else if (kd->wVKey == VK_LEFT && WinUtil::isAlt())
	{
		navigationBack();
	}
	else if (kd->wVKey == VK_RIGHT && WinUtil::isAlt())
	{
		navigationForward();
	}
	else if (kd->wVKey == VK_RETURN)
	{
		if (ctrlList.GetSelectedCount() == 1)
			performDefaultAction(ctrlList.GetSelectedIndex());
	}
	return 0;
}

void DirectoryListingFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown() || updatingLayout)
		return;

	updatingLayout++;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);

	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		ctrlStatus.GetClientRect(sr);
		int sum = 0;
		for (int i = 1; i < STATUS_LAST; i++)
			sum += statusSizes[i];
		int w[STATUS_LAST];
		w[STATUS_TEXT] = std::max(int(sr.right) - sum, 120);
		for (int i = 1; i < STATUS_LAST; i++)
			w[i] = w[i - 1] + statusSizes[i];
		ctrlStatus.SetParts(STATUS_LAST, w);
	}

	SetSplitterRect(&rect);
	updatingLayout--;
}

void DirectoryListingFrame::runUserCommand(UserCommand& uc)
{
	if (!dl->getUser()->isOnline())
	{
		ctrlStatus.SetText(STATUS_TEXT, CTSTRING(USER_OFFLINE));
		return;
	}

	StringMap ucLineParams;
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;

	int sel = -1;
	while ((sel = ctrlList.GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		StringMap ucParams = ucLineParams;
		const ItemInfo* ii = ctrlList.getItemData(sel);
		if (ii->type == ItemInfo::FILE)
			dl->getFileParams(ii->file, ucParams);
		else
			dl->getDirectoryParams(ii->dir, ucParams);
		ClientManager::userCommand(dl->getHintedUser(), uc, ucParams, true);
		if (uc.once()) break;
	}
}

void DirectoryListingFrame::closeAll()
{
	for (auto i = activeFrames.cbegin(); i != activeFrames.cend(); ++i)
		i->second->PostMessage(WM_CLOSE, 0, 0);
}

void DirectoryListingFrame::closeAllOffline()
{
	for (auto i = activeFrames.cbegin(); i != activeFrames.cend(); ++i)
	{
		auto frame = i->second;
		if (frame->offline) frame->PostMessage(WM_CLOSE, 0, 0);
	}
}

LRESULT DirectoryListingFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDC_COPY_NICK)
	{
		WinUtil::setClipboard(nick);
		return 0;
	}

	string data;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(i);
		string sCopy;
		switch (wID)
		{
			case IDC_COPY_FILENAME:
				sCopy = Text::fromT(ii->columns[COLUMN_FILENAME]);
				break;
			case IDC_COPY_SIZE:
				sCopy = Text::fromT(ii->columns[COLUMN_SIZE]);
				break;
			case IDC_COPY_EXACT_SIZE:
				sCopy = Util::toString(ii->type == ItemInfo::FILE ? ii->file->getSize() : ii->dir->getTotalSize());
				break;
			case IDC_COPY_PATH:
				sCopy = Text::fromT(ii->columns[COLUMN_PATH]);
				break;
			case IDC_COPY_LINK:
				if (ii->type == ItemInfo::FILE)
					sCopy = Util::getMagnet(ii->file->getTTH(), ii->file->getName(), ii->file->getSize());
				break;
			case IDC_COPY_TTH:
				if (ii->type == ItemInfo::FILE)
					sCopy = ii->file->getTTH().toBase32();
				break;
			case IDC_COPY_WMLINK:
				if (ii->type == ItemInfo::FILE)
					sCopy = Util::getWebMagnet(ii->file->getTTH(), ii->file->getName(), ii->file->getSize());
				break;
			default:
				dcdebug("DIRECTORYLISTINGFRAME DON'T GO HERE\n");
				return 0;
		}
		if (!sCopy.empty())
		{
			if (!data.empty())
				data += "\r\n";
			data += sCopy;
		}
	}
	if (!data.empty())
	{
		WinUtil::setClipboard(data);
	}
	return 0;
}

string DirectoryListingFrame::getFileUrl(const string& hubUrl, const string& path) const
{
	Util::ParsedUrl url;
	Util::decodeUrl(hubUrl, url);
	url.path = Util::encodeUriPath(Util::toAdcFile(path));
	int protocol = Util::getHubProtocol(url.protocol);
	if (ownList)
	{
		if (protocol == Util::HUB_PROTOCOL_ADC || protocol == Util::HUB_PROTOCOL_ADCS)
			url.user = ClientManager::getMyCID().toBase32();
		else
			url.user = ClientManager::findMyNick(hubUrl);
	}
	else
	{
		if (protocol == Util::HUB_PROTOCOL_ADC || protocol == Util::HUB_PROTOCOL_ADCS)
		{
			const UserPtr& user = dl->getUser();
			if (user && !user->getCID().isZero() && !(user->getFlags() & User::NMDC))
				url.user = user->getCID().toBase32();
		}
	}
	if (url.user.empty()) url.user = nick;
	return Util::formatUrl(url, true);
}

void DirectoryListingFrame::appendCopyUrlItems(CMenu& menu, int idc, ResourceManager::Strings text)
{
	vector<ClientBasePtr> hubs;
	ClientManager::getConnectedHubs(hubs);
	tstring ts = TSTRING_I(text);
	int n = 0;
	for (ClientBasePtr& clientBase : hubs)
	{
		const Client* client = static_cast<Client*>(clientBase.get());
		tstring menuText = ts;
		menuText += _T(" (");
		menuText += Text::toT(client->getHubName());
		menuText += _T(')');
		menu.AppendMenu(MF_STRING, idc + n, menuText.c_str());
		contextMenuHubUrl.push_back(client->getHubUrl());
		if (++n == 15) break;
	}
}

LRESULT DirectoryListingFrame::onCopyUrl(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	if (i != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(i);
		string path;
		if (ii->type == ItemInfo::FILE)
		{
			const DirectoryListing::File* file = ii->file;
			if (file->getParent()->getAdls())
				path = static_cast<const DirectoryListing::AdlFile*>(file)->getFullPath();
			else
				path = dl->getPath(file);
			path += file->getName();
		}
		else if (ii->type == ItemInfo::DIRECTORY)
			path = dl->getPath(ii->dir);
		int index = wID - IDC_COPY_URL;
		if (!path.empty() && index < (int) contextMenuHubUrl.size())
			WinUtil::setClipboard(getFileUrl(contextMenuHubUrl[index], path));
	}
	return 0;
}

LRESULT DirectoryListingFrame::onCopyUrlTree(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HTREEITEM ht = ctrlTree.GetSelectedItem();
	if (!ht) return 0;
	DirectoryListing::Directory* d = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(ht));
	if (!d) return 0;

	string path = dl->getPath(d);
	int index = wID - IDC_COPY_URL_TREE;
	if (!path.empty() && index < (int) contextMenuHubUrl.size())
		WinUtil::setClipboard(getFileUrl(contextMenuHubUrl[index], path));
	return 0;
}

LRESULT DirectoryListingFrame::onCopyFolder(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HTREEITEM ht = ctrlTree.GetSelectedItem();
	if (!ht) return 0;
	DirectoryListing::Directory* d = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(ht));
	if (!d) return 0;

	string path;
	if (wID == IDC_COPY_FOLDER_PATH)
		path = Util::toAdcFile(dl->getPath(d));
	else
		path = d->getName();
	if (!path.empty())
		WinUtil::setClipboard(path);
	return 0;
}

LRESULT DirectoryListingFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	destroyTimer();
	closing = true;
	CWaitCursor waitCursor;
	if (loading)
	{
		//tell the thread to abort and wait until we get a notification
		//that it's done.
		abortFlag.store(true);
		return 0;
	}
	if (!closed)
	{
		closed = true;
		SettingsManager::instance.removeListener(this);
		activeFrames.erase(m_hWnd);
		ctrlList.deleteAll();
		ctrlList.saveHeaderOrder(Conf::DIRLIST_FRAME_ORDER, Conf::DIRLIST_FRAME_WIDTHS, Conf::DIRLIST_FRAME_VISIBLE);
		auto ss = SettingsManager::instance.getUiSettings();
		ss->setInt(Conf::DIRLIST_FRAME_SORT, ctrlList.getSortForSettings());
		ss->setInt(Conf::DIRLIST_FRAME_SPLIT, m_nProportionalPos);
		bHandled = FALSE;
	}
	return 0;
}

LRESULT DirectoryListingFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	OMenu tabMenu;
	tabMenu.CreatePopupMenu();

	clearUserMenu();
	const HintedUser& hintedUser = dl->getHintedUser();
	if (hintedUser.user && !(hintedUser.user->getFlags() & User::FAKE) && !dl->isOwnList())
	{
		string nick = hintedUser.user->getLastNick();
		tabMenu.InsertSeparatorFirst(Text::toT(nick));
		reinitUserMenu(hintedUser.user, hintedUser.hint);
		appendAndActivateUserItems(tabMenu);
		tabMenu.AppendMenu(MF_SEPARATOR);
	}

	if (offline)
		tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_OFFLINE_DIR_LIST));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_DIR_LIST));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));

	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	MenuHelper::unlinkStaticMenus(tabMenu);
	return TRUE;
}

LRESULT DirectoryListingFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	if (!dclstFlag)
	{
		opt->icons[0] = g_iconBitmaps.getIcon(searchResultsFlag ? IconBitmaps::FILELIST_SEARCH : IconBitmaps::FILELIST, 0);
		opt->icons[1] = g_iconBitmaps.getIcon(searchResultsFlag ? IconBitmaps::FILELIST_SEARCH_OFFLINE : IconBitmaps::FILELIST_OFFLINE, 0);
	}
	else
		opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::DCLST, 0);
	opt->isHub = false;
	return TRUE;
}

void DirectoryListingFrame::on(SettingsManagerListener::ApplySettings)
{
	FileStatusColorsEx newColors;
	newColors.get();
	if (ctrlList.isRedraw() || !colors.compare(newColors))
	{
		colors = newColors;
		setTreeViewColors(ctrlTree);
		WinUtil::setTreeViewTheme(ctrlTree, Colors::isDarkTheme);
		redraw();
	}
}

LRESULT DirectoryListingFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	switch (wParam)
	{
		case FINISHED:
			loading = false;
			if (!isClosedOrShutdown())
			{
				enableControls();
				if (lParam == ThreadedDirectoryListing::MODE_COMPARE_FILE)
				{
					ctrlStatus.SetText(0, _T(""));
					bool hasMatches = (dl->getRoot()->getFlags() & DirectoryListing::FLAG_HAS_FOUND) != 0;
					if (hasMatches)
					{
						goToFirstFound();
						updateSearchButtons();
						redraw();
						showFound();
					}
					else
						MessageBox(CTSTRING(NO_MATCHES), CTSTRING(SEARCH), MB_ICONINFORMATION);
				}
				else
				{
					initStatus();
					ctrlStatus.SetText(0, (TSTRING(PROCESSED_FILE_LIST) + _T(' ') + Util::toStringT((GET_TICK() - loadStartTime) / 1000) + TSTRING(S)).c_str());
					//notify the user that we've loaded the list
					setDirty();
				}
			}
			else
			{
				PostMessage(WM_CLOSE, 0, 0);
			}
			break;
		case ABORTED:
		{
			std::unique_ptr<ErrorInfo> error(reinterpret_cast<ErrorInfo*>(lParam));
			loading = false;
			int mode = -1;
			if (error)
			{
				mode = error->mode;
				if (!error->text.empty())
					MessageBox(error->text.c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR);
			}
			if (mode == ThreadedDirectoryListing::MODE_LOAD_FILE)
				PostMessage(WM_CLOSE, 0, 0);
			else
			{
				enableControls();
				ctrlStatus.SetText(0, _T(""));
			}
			break;
		}
		case SPLICE_TREE:
		{
			unique_ptr<DirectoryListing> newListing(reinterpret_cast<DirectoryListing*>(lParam));
			DirectoryListing::SpliceTreeResult sr;
			auto root = dl->getRoot();
			if (root->getFlags() & DirectoryListing::FLAG_HAS_FOUND)
			{
				clearSearch();
				root->clearMatches();
				updateSearchButtons();
			}
			if (dl->spliceTree(*newListing, sr))
			{
				HTREEITEM htParent = sr.parentUserData ? (HTREEITEM) sr.parentUserData : treeRoot;
				loading = true;
				try { refreshTree(sr.firstItem, htParent, sr.insertParent, newListing->getBasePath()); }
				catch (Exception&) { dcassert(0); }
				loading = false;
				ctrlTree.SetItemData(treeRoot, (DWORD_PTR) dl->getRoot());
				if (ctrlTree.GetSelectedItem() == selectedDir)
				{
					auto d = reinterpret_cast<const DirectoryListing::Directory*>(ctrlTree.GetItemData(selectedDir));
					if (d) showDirContents(d, nullptr, nullptr);
				}
				root = dl->getRoot();
				if (!root->getComplete())
				{
					ctrlTree.SetItemImage(treeRoot, FileImage::DIR_MASKED, FileImage::DIR_MASKED);
				}
			}
			break;
		}
		case ADL_SEARCH:
			ctrlStatus.SetText(0, CTSTRING(PERFORMING_ADL_SEARCH));
			break;
		default:
			if (wParam & PROGRESS)
			{
				tstring str = TSTRING(LOADING_FILE_LIST_PROGRESS);
				str += Util::toStringT(wParam & ~PROGRESS);
				str += _T('%');
				ctrlStatus.SetText(0, str.c_str());
				break;
			}
			dcassert(0);
			break;
	}
	return 0;
}

void DirectoryListingFrame::getDirItemColor(DirectoryListing::Directory::MaskType flags, COLORREF &fg, COLORREF &bg)
{
	fg = Colors::g_textColor;
	bg = Colors::g_bgColor;
	if (flags & DirectoryListing::FLAG_FOUND)
	{
		fg = colors.fgNormal[FileStatusColors::FOUND];
		bg = colors.bgNormal[FileStatusColors::FOUND];
	}
	else if (flags & DirectoryListing::FLAG_HAS_FOUND)
	{
		fg = colors.fgLighter[FileStatusColors::FOUND];
		bg = colors.bgLighter[FileStatusColors::FOUND];
	}
	else if (flags & DirectoryListing::FLAG_HAS_SHARED)
	{
		if (flags & (DirectoryListing::FLAG_HAS_DOWNLOADED | DirectoryListing::FLAG_HAS_CANCELED | DirectoryListing::FLAG_HAS_OTHER))
		{
			fg = colors.fgLighter[FileStatusColors::SHARED];
			bg = colors.bgLighter[FileStatusColors::SHARED];
		}
		else
		{
			fg = colors.fgNormal[FileStatusColors::SHARED];
			bg = colors.bgNormal[FileStatusColors::SHARED];
		}
	}
	else if (flags & DirectoryListing::FLAG_HAS_DOWNLOADED)
	{
		if (flags & (DirectoryListing::FLAG_HAS_CANCELED | DirectoryListing::FLAG_HAS_OTHER))
		{
			fg = colors.fgLighter[FileStatusColors::DOWNLOADED];
			bg = colors.bgLighter[FileStatusColors::DOWNLOADED];
		}
		else
		{
			fg = colors.fgNormal[FileStatusColors::DOWNLOADED];
			bg = colors.bgNormal[FileStatusColors::DOWNLOADED];
		}
	}
	else if (flags & DirectoryListing::FLAG_HAS_CANCELED)
	{
		if (flags & DirectoryListing::FLAG_HAS_OTHER)
		{
			fg = colors.fgLighter[FileStatusColors::CANCELED];
			bg = colors.bgLighter[FileStatusColors::CANCELED];
		}
		else
		{
			fg = colors.fgNormal[FileStatusColors::CANCELED];
			bg = colors.bgNormal[FileStatusColors::CANCELED];
		}
	}
	if (flags & DirectoryListing::FLAG_HAS_QUEUED)
		fg = colors.fgInQueue;
}

void DirectoryListingFrame::getFileItemColor(DirectoryListing::File::MaskType flags, COLORREF &fg, COLORREF &bg)
{
	fg = Colors::g_textColor;
	bg = Colors::g_bgColor;
	if (flags & DirectoryListing::FLAG_FOUND)
	{
		fg = colors.fgNormal[FileStatusColors::FOUND];
		bg = colors.bgNormal[FileStatusColors::FOUND];
	}
	else if (flags & DirectoryListing::FLAG_HAS_FOUND)
	{
		fg = colors.fgLighter[FileStatusColors::FOUND];
		bg = colors.bgLighter[FileStatusColors::FOUND];
	}
	else if (flags & DirectoryListing::FLAG_SHARED)
	{
		fg = colors.fgNormal[FileStatusColors::SHARED];
		bg = colors.bgNormal[FileStatusColors::SHARED];
	}
	else if (flags & DirectoryListing::FLAG_DOWNLOADED)
	{
		fg = colors.fgNormal[FileStatusColors::DOWNLOADED];
		bg = colors.bgNormal[FileStatusColors::DOWNLOADED];
	}
	else if (flags & DirectoryListing::FLAG_CANCELED)
	{
		fg = colors.fgNormal[FileStatusColors::CANCELED];
		bg = colors.bgNormal[FileStatusColors::CANCELED];
	}
	if (flags & DirectoryListing::FLAG_QUEUED)
		fg = colors.fgInQueue;
}

LRESULT DirectoryListingFrame::onCustomDrawList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW plvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	switch (plvcd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			CustomDrawHelpers::startDraw(customDrawState, plvcd);
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
		{
			ItemInfo *ii = reinterpret_cast<ItemInfo*>(plvcd->nmcd.lItemlParam);
			ii->updateIconIndex();
			if (ii->type == ItemInfo::FILE)
			{
				auto flags = ii->file->getFlags();
				getFileItemColor(flags, plvcd->clrText, plvcd->clrTextBk);
			}
			else
			{
				auto flags = ii->dir->getFlags();
				getDirItemColor(flags, plvcd->clrText, plvcd->clrTextBk);
			}
			CustomDrawHelpers::startItemDraw(customDrawState, plvcd);
			if (customDrawState.flags & CustomDrawHelpers::FLAG_SELECTED)
				plvcd->clrText = Colors::g_textColor;
			if (hTheme)
				CustomDrawHelpers::drawBackground(hTheme, customDrawState, plvcd);
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		}

		case CDDS_ITEMPOSTPAINT:
			CustomDrawHelpers::drawFocusRect(customDrawState, plvcd);
			return CDRF_SKIPDEFAULT;

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			const ItemInfo *ii = reinterpret_cast<ItemInfo*>(plvcd->nmcd.lItemlParam);
			int column = ctrlList.findColumn(plvcd->iSubItem);
			if (column == COLUMN_MEDIA_XY && ii->type == ItemInfo::FILE)
			{
				const MediaInfoUtil::Info* media = ii->file->getMedia();
				if (media)
				{
					CustomDrawHelpers::drawVideoResIcon(customDrawState, plvcd, ii->columns[COLUMN_MEDIA_XY], media->width, media->height);
					return CDRF_SKIPDEFAULT;
				}
			}
			if (plvcd->iSubItem == 0)
				CustomDrawHelpers::drawFirstSubItem(customDrawState, plvcd, ii->getText(column));
			else
				CustomDrawHelpers::drawTextAndIcon(customDrawState, plvcd, nullptr, -1, ii->getText(column), false);
			return CDRF_SKIPDEFAULT;
		}
	}
	return CDRF_DODEFAULT;
}

LRESULT DirectoryListingFrame::onCustomDrawTree(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW plvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);

	switch (plvcd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			treeViewFocused = GetFocus() == ctrlTree;
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
		{
			DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(plvcd->nmcd.lItemlParam);
			if (dir)
			{
				COLORREF bg, fg;
				getDirItemColor(dir->getFlags(), fg, bg);
				if (!(plvcd->nmcd.uItemState & CDIS_SELECTED))
				{
					plvcd->clrTextBk = bg;
					plvcd->clrText = fg;
				}
				else if (treeViewFocused)
					plvcd->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
				else
					plvcd->clrText =
						GetSysColor(COLOR_WINDOW) == GetSysColor(COLOR_BTNFACE) ?
						GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_BTNTEXT);
			}
		}
	}
	return CDRF_DODEFAULT;
}

static void parseDuration(const string& src, unsigned& msec, tstring& columnAudio, tstring& columnDuration)
{
	unsigned val;
	bool result = false;
	columnDuration.clear();
	columnAudio.clear();
	auto pos = src.find('|');
	if (pos != string::npos)
	{
		result = MediaInfoUtil::parseDuration(src.substr(0, pos), val);
		if (result)
		{
			Text::toT(src.substr(pos + 1), columnAudio);
			boost::trim(columnAudio);
		}
	}
	else
		result = MediaInfoUtil::parseDuration(src, val);
	if (!result)
	{
		Text::toT(src, columnAudio);
		return;
	}
	msec = val;
	Text::toT(Util::formatTime((val + 999) / 1000), columnDuration);
}

DirectoryListingFrame::ItemInfo::ItemInfo(DirectoryListing::File* f, const DirectoryListing* dl) :
	type(FILE), file(f), iconIndex(-1), duration(0)
{
	columns[COLUMN_FILENAME] = Text::toT(f->getName());
	columns[COLUMN_TYPE] = Util::getFileExt(columns[COLUMN_FILENAME]);
	if (!columns[COLUMN_TYPE].empty() && columns[COLUMN_TYPE][0] == '.')
		columns[COLUMN_TYPE].erase(0, 1);
	columns[COLUMN_EXACT_SIZE] = Util::formatExactSizeT(f->getSize());
	columns[COLUMN_SIZE] =  Util::formatBytesT(f->getSize());
	columns[COLUMN_TTH] = Text::toT(f->getTTH().toBase32());
	if (dl->isOwnList())
	{
		string s;
		ShareManager::getInstance()->getFileInfo(f->getTTH(), s);
		columns[COLUMN_PATH] = Text::toT(s);
	}
	else
	{
		columns[COLUMN_PATH] = Text::toT(f->getPath());
		Util::uriSeparatorsToPathSeparators(columns[COLUMN_PATH]);
	}

	if (f->getUploadCount())
		columns[COLUMN_UPLOAD_COUNT] = Util::toStringT(f->getUploadCount());
	if (f->getTS())
		columns[COLUMN_TS] = Text::toT(Util::formatDateTime(static_cast<time_t>(f->getTS())));
	const MediaInfoUtil::Info *media = f->getMedia();
	if (media)
	{
		if (media->bitrate)
			columns[COLUMN_BITRATE] = Util::toStringT(media->bitrate);
		if (media->width && media->height)
		{
			TCHAR buf[64];
			_stprintf(buf, _T("%ux%u"), media->width, media->height);
			columns[COLUMN_MEDIA_XY] = buf;
		}
		columns[COLUMN_MEDIA_VIDEO] = Text::toT(media->video);
		parseDuration(media->audio, duration, columns[COLUMN_MEDIA_AUDIO], columns[COLUMN_DURATION]);
	}
}

DirectoryListingFrame::ItemInfo::ItemInfo(DirectoryListing::Directory* d) :
	type(DIRECTORY), dir(d), iconIndex(-1), duration(0)
{
	columns[COLUMN_FILENAME] = Text::toT(d->getName());
	columns[COLUMN_EXACT_SIZE] = Util::formatExactSizeT(d->getTotalSize());
	columns[COLUMN_SIZE] = Util::formatBytesT(d->getTotalSize());
	columns[COLUMN_FILES] = Util::toStringT(d->getTotalFileCount());
	auto totalUploads = d->getTotalUploadCount();
	if (totalUploads) columns[COLUMN_UPLOAD_COUNT] = Util::toStringT(totalUploads);
	auto maxTS = d->getMaxTS();
	if (maxTS) columns[COLUMN_TS] = Text::toT(Util::formatDateTime(static_cast<time_t>(maxTS)));
	auto minBitrate = d->getMinBirate(), maxBitrate = d->getMaxBirate();
	if (minBitrate < maxBitrate)
	{
		TCHAR buf[64];
		_stprintf(buf, _T("%u-%u"), minBitrate, maxBitrate);
		columns[COLUMN_BITRATE] = buf;
	}
	else
	if (minBitrate == maxBitrate)
		columns[COLUMN_BITRATE] = Util::toStringT(minBitrate);
}

int DirectoryListingFrame::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, int col, int /*flags*/)
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
				case COLUMN_EXACT_SIZE:
				case COLUMN_SIZE:
					return compare(a->dir->getTotalSize(), b->dir->getTotalSize());
				case COLUMN_UPLOAD_COUNT:
					return compare(a->dir->getTotalUploadCount(), b->dir->getTotalUploadCount());
				case COLUMN_TS:
					return compare(a->dir->getMaxTS(), b->dir->getMaxTS());
				case COLUMN_FILES:
					return compare(a->dir->getTotalFileCount(), b->dir->getTotalFileCount());
				default:
					return Util::defaultSort(a->columns[col], b->columns[col], false);
			}
		}
		return -1;
	}
	if (b->type == DIRECTORY)
		return 1;
	switch (col)
	{
		case COLUMN_FILENAME:
		case COLUMN_FILES:
			return Util::defaultSort(a->columns[COLUMN_FILENAME], b->columns[COLUMN_FILENAME], true);
		case COLUMN_TYPE:
		{
			int result = Util::defaultSort(a->columns[COLUMN_TYPE], b->columns[COLUMN_TYPE], true);
			if (result) return result;
			return Util::defaultSort(a->columns[COLUMN_FILENAME], b->columns[COLUMN_FILENAME], true);
		}
		case COLUMN_EXACT_SIZE:
		case COLUMN_SIZE:
			return compare(a->file->getSize(), b->file->getSize());
		case COLUMN_UPLOAD_COUNT:
			return compare(a->file->getUploadCount(), b->file->getUploadCount());
		case COLUMN_TS:
			return compare(a->file->getTS(), b->file->getTS());
		case COLUMN_BITRATE:
		{
			const MediaInfoUtil::Info* aMedia = a->file->getMedia();
			const MediaInfoUtil::Info* bMedia = b->file->getMedia();
			if (aMedia && bMedia)
				return compare(aMedia->bitrate, bMedia->bitrate);
			if (aMedia) return 1;
			if (bMedia) return -1;
			return 0;
		}
		case COLUMN_MEDIA_XY:
		{
			const MediaInfoUtil::Info* aMedia = a->file->getMedia();
			const MediaInfoUtil::Info* bMedia = b->file->getMedia();
			if (aMedia && bMedia)
				return compare(aMedia->getSize(), bMedia->getSize());
			if (aMedia) return 1;
			if (bMedia) return -1;
			return 0;
		}
		case COLUMN_DURATION:
			return compare(a->duration, b->duration);
		case COLUMN_TTH:
			return compare(a->columns[COLUMN_TTH], b->columns[COLUMN_TTH]);
	}
	return Util::defaultSort(a->columns[col], b->columns[col], false);
}

void DirectoryListingFrame::ItemInfo::updateIconIndex()
{
	if (iconIndex < 0)
	{
		if (type == DIRECTORY)
			iconIndex = FileImage::DIR_ICON;
		else
			iconIndex = g_fileImage.getIconIndex(file->getName());
	}
}

LRESULT DirectoryListingFrame::onGenerateDcLst(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const DirectoryListing::Directory* dir = nullptr;

	if (wID == IDC_GENERATE_DCLST_FILE)  // Call from files panel
	{
		int i = -1;
		while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			const ItemInfo* ii = ctrlList.getItemData(i);
			if (ii->type == ItemInfo::DIRECTORY)
			{
				dir = ii->dir;
				break;
			}
		}
	}
	else
	{
		HTREEITEM t = ctrlTree.GetSelectedItem();
		if (t != NULL)
			dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	}
	if (dir)
	{
		try
		{
			if (dl->getUser())
			{
				DclstGenDlg dlg(dir, dl->getUser());
				dlg.DoModal(GetParent());
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}

	return 0;
}

LRESULT DirectoryListingFrame::onShowDuplicates(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	if (i == -1) return 0;
	const ItemInfo* ii = ctrlList.getItemData(i);
	if (!(ii->type == ItemInfo::FILE && ii->file->isSet(DirectoryListing::FLAG_FOUND))) return 0;
	auto it = dupFiles.find(ii->file->getTTH());
	if (it == dupFiles.cend()) return 0;
	DuplicateFilesDlg dlg(dl.get(), it->second, ii->file);
	dlg.DoModal(*this);
	if (dlg.goToFile)
	{
		const DirectoryListing::Directory *dir = dlg.goToFile->getParent();
		dcassert(dir);
		HTREEITEM ht = static_cast<HTREEITEM>(dir->getUserData());
		if (ht != selectedDir)
		{
			ctrlTree.EnsureVisible(ht);
			ctrlTree.SelectItem(ht);
			showDirContents(dir, nullptr, dlg.goToFile);
			selectedDir = ht;
		}
		else
			selectFile(dlg.goToFile);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onGoToOriginal(WORD, WORD, HWND, BOOL&)
{
	HTREEITEM ht = ctrlTree.GetSelectedItem();
	if (!ht) return 0;
	DirectoryListing::Directory* d = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(ht));
	if (!d) return 0;

	if (!originalId) return 0;
	DirectoryListingFrame* frame = findFrameByID(originalId);
	if (!frame) return 0;

	string path = dl->getPath(d);
	d = frame->dl->findDirPath(Util::toAdcFile(path));
	if (d) frame->ctrlTree.SelectItem(static_cast<HTREEITEM>(d->getUserData()));
	WinUtil::activateMDIChild(frame->m_hWnd);
	return 0;
}

void DirectoryListingFrame::openFileFromList(const tstring& file)
{
	if (file.empty())
		return;

	if (Util::isDclstFile(Text::fromT(file)))
		DirectoryListingFrame::openWindow(file, Util::emptyStringT, HintedUser(), 0, true);
	else
		WinUtil::openFile(file);
}

OnlineUserPtr DirectoryListingFrame::getSelectedOnlineUser() const
{
	if (offline || dclstFlag || ownList) return OnlineUserPtr();
	const HintedUser& hintedUser = dl->getHintedUser();
	if (!hintedUser.user || (hintedUser.user->getFlags() & User::FAKE)) return OnlineUserPtr();
	return ClientManager::findOnlineUser(hintedUser.user->getCID(), hintedUser.hint, true);
}

void DirectoryListingFrame::openUserLog()
{
	WinUtil::openLog(Util::getConfString(Conf::LOG_FILE_PRIVATE_CHAT), getFrameLogParams(), TSTRING(NO_LOG_FOR_USER));
}

void DirectoryListingFrame::showFound()
{
	const auto& s = search[SEARCH_CURRENT];
	if (s.getWhatFound() == DirectoryListing::SearchContext::FOUND_NOTHING) return;
	const DirectoryListing::Directory *dir = s.getDirectory();
	const DirectoryListing::File *file = s.getFile();
	dcassert(dir);
	HTREEITEM ht = static_cast<HTREEITEM>(dir->getUserData());
	if (ht != selectedDir)
	{
		ctrlTree.EnsureVisible(ht);
		ctrlTree.SelectItem(ht);
		showDirContents(dir, nullptr, file);
		selectedDir = ht;
	} else
	if (file)
		selectFile(file);
	dumpFoundPath();
}

void DirectoryListingFrame::dumpFoundPath()
{
#ifdef _DEBUG
	const auto& s = search[SEARCH_CURRENT];
	const DirectoryListing::Directory *dir = s.getDirectory();
	const DirectoryListing::File *file = s.getFile();
	const vector<int> &v = s.getDirIndex();
	tstring str;
	for (int i=0; i < (int) v.size(); i++)
	{
		if (!str.empty()) str += _T(' ');
		str += Util::toStringT(v[i]);
	}
	if (s.getWhatFound() == DirectoryListing::SearchContext::FOUND_DIR)
	{
		str += _T(" Dir: ");
		str += Text::toT(dir->getName());
	} else
	{
		str += _T(" File: ");
		str += Text::toT(file->getName());
	}
	ctrlStatus.SetText(STATUS_TEXT, str.c_str());
#endif
}

void DirectoryListingFrame::clearSearch()
{
	for (int i = 0; i < SEARCH_LAST; ++i)
		search[i].clear();
}

void DirectoryListingFrame::changeFoundItem()
{
	dumpFoundPath();
	search[SEARCH_NEXT] = search[SEARCH_PREV] = search[SEARCH_CURRENT];
	if (!search[SEARCH_NEXT].next()) search[SEARCH_NEXT].clear();
	if (!search[SEARCH_PREV].prev()) search[SEARCH_PREV].clear();
	updateSearchButtons();
}

void DirectoryListingFrame::updateSearchButtons()
{
	navWnd.enableControlButton(IDC_PREV, search[SEARCH_PREV].getWhatFound() != DirectoryListing::SearchContext::FOUND_NOTHING);
	navWnd.enableControlButton(IDC_NEXT, search[SEARCH_NEXT].getWhatFound() != DirectoryListing::SearchContext::FOUND_NOTHING);
}

LRESULT DirectoryListingFrame::onFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (WinUtil::isShift()) searchOptions.clear();
	searchOptions.enableSharedDays = dl->hasTimestamps();
	SearchDlg dlg(searchOptions);
	if (dlg.DoModal(*this) != IDOK) return 0;

	bool hasMatches = (dl->getRoot()->getFlags() & DirectoryListing::FLAG_HAS_FOUND) != 0;
	showingDupFiles = false;

	if (dlg.clearResults())
	{
		if (hasMatches)
		{
			clearSearch();
			ctrlStatus.SetText(STATUS_TEXT, _T(""));
			dl->getRoot()->clearMatches();
			updateSearchButtons();
			redraw();
		}
		return 0;
	}

	const string &findStr = searchOptions.text;
	DirectoryListing::SearchQuery sq;
	if (!findStr.empty() && searchOptions.fileType != FILE_TYPE_TTH)
	{
		if (searchOptions.regExp)
		{
			int reFlags = 0;
			if (!searchOptions.matchCase) reFlags |= std::regex_constants::icase;
			try
			{
				sq.re.assign(findStr, (std::regex::flag_type) reFlags);
				sq.flags |= DirectoryListing::SearchQuery::FLAG_REGEX;
			}
			catch (std::regex_error&) {}
		} else
		if (searchOptions.matchCase)
		{
			sq.text = findStr;
			sq.flags |= DirectoryListing::SearchQuery::FLAG_STRING | DirectoryListing::SearchQuery::FLAG_MATCH_CASE;
		} else
		{
			Text::utf8ToWide(findStr, sq.wtext);
			Text::makeLower(sq.wtext);
			sq.flags |= DirectoryListing::SearchQuery::FLAG_WSTRING;
		}
	}

	if (searchOptions.fileType != FILE_TYPE_ANY)
	{
		if (searchOptions.fileType == FILE_TYPE_TTH)
			Util::fromBase32(findStr.c_str(), sq.tth.data, TTHValue::BYTES);
		sq.type = searchOptions.fileType;
		sq.flags |= DirectoryListing::SearchQuery::FLAG_TYPE;
	}

	if (searchOptions.sizeMin >= 0 || searchOptions.sizeMax >= 0)
	{
		int sizeUnitShift = 0;
		if (searchOptions.sizeUnit > 0 && searchOptions.sizeUnit <= 3)
			sizeUnitShift = 10*searchOptions.sizeUnit;
		sq.minSize = searchOptions.sizeMin;
		if (sq.minSize >= 0)
			sq.minSize <<= sizeUnitShift;
		else
			sq.minSize = 0;
		sq.maxSize = searchOptions.sizeMax;
		if (sq.maxSize >= 0)
			sq.maxSize <<= sizeUnitShift;
		else
			sq.maxSize = std::numeric_limits<int64_t>::max();
		sq.flags |= DirectoryListing::SearchQuery::FLAG_SIZE;
	}

	if (searchOptions.sharedDays > 0)
	{
		sq.minSharedTime = GET_TIME() - searchOptions.sharedDays*(60*60*24);
		sq.flags |= DirectoryListing::SearchQuery::FLAG_TIME_SHARED;
	}

	if (searchOptions.skipEmpty)
		sq.flags |= DirectoryListing::SearchQuery::FLAG_SKIP_EMPTY;
	if (searchOptions.skipOwned)
		sq.flags |= DirectoryListing::SearchQuery::FLAG_SKIP_OWNED;
	if (searchOptions.skipCanceled)
		sq.flags |= DirectoryListing::SearchQuery::FLAG_SKIP_CANCELED;

	DirectoryListing *dest = searchOptions.newWindow ? new DirectoryListing(abortFlag, true, dl.get()) : nullptr;

	if (!search[SEARCH_CURRENT].match(sq, dl->getRoot(), dest, pathCache))
	{
		delete dest;
		clearSearch();
		updateSearchButtons();
		if (hasMatches) redraw();
		MessageBox(CTSTRING(NO_MATCHES), CTSTRING(SEARCH), MB_ICONINFORMATION);
		return 0;
	}

	search[SEARCH_NEXT] = search[SEARCH_CURRENT];
	if (!search[SEARCH_NEXT].next()) search[SEARCH_NEXT].clear();
	search[SEARCH_PREV].clear();

	updateSearchButtons();
	redraw();
	showFound();

	const size_t* count = search->getCount();
	tstring statusText;
	if (count[0] && count[1])
	{
		tstring found0 = TPLURAL_F(PLURAL_FILES, count[0]);
		tstring found1 = TPLURAL_F(PLURAL_FOLDERS, count[1]);
		statusText = TSTRING_F(FOUND_FILES_AND_FOLDERS, found0 % found1);
	}
	else
	{
		tstring found = count[0] ? TPLURAL_F(PLURAL_FILES, count[0]) : TPLURAL_F(PLURAL_FOLDERS, count[1]);
		statusText = TSTRING_F(FOUND_FILES_OR_FOLDERS, found);
	}
	ctrlStatus.SetText(STATUS_TEXT, statusText.c_str());

	if (dest)
	{
		DirectoryListingFrame* newFrame = openWindow(dest, dl->getHintedUser(), speed, true);
		newFrame->setFileName(fileName);
		newFrame->originalId = id;
	}

	return 0;
}

LRESULT DirectoryListingFrame::onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (search[SEARCH_NEXT].getWhatFound() != DirectoryListing::SearchContext::FOUND_NOTHING)
	{
		search[SEARCH_PREV] = std::move(search[SEARCH_CURRENT]);
		search[SEARCH_CURRENT] = search[SEARCH_NEXT];
		if (!search[SEARCH_NEXT].next()) search[SEARCH_NEXT].clear();
		showFound();
		updateSearchButtons();
	}
	return 0;
}

LRESULT DirectoryListingFrame::onPrev(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (search[SEARCH_PREV].getWhatFound() != DirectoryListing::SearchContext::FOUND_NOTHING)
	{
		search[SEARCH_NEXT] = std::move(search[SEARCH_CURRENT]);
		search[SEARCH_CURRENT] = search[SEARCH_PREV];
		if (!search[SEARCH_PREV].prev()) search[SEARCH_PREV].clear();
		showFound();
		updateSearchButtons();
	}
	return 0;
}

LRESULT DirectoryListingFrame::onNavigation(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_NAVIGATION_BACK:
			navigationBack();
			break;
		case IDC_NAVIGATION_FORWARD:
			navigationForward();
			break;
		default:
			navigationUp();
	}
	return 0;
}

LRESULT DirectoryListingFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown())
	{
		if (listItemChanged)
			updateStatus();
		else if (++setWindowTitleTick == 10)
			updateWindowTitle();
	}
	return 0;
}

void DirectoryListingFrame::DrawSplitterPane(CDCHandle dc, int nPane)
{
}

void DirectoryListingFrame::UpdatePane(int nPane, const RECT& rcPane)
{
	if (nPane != 1) return;
	int navBarHeight = navWnd.updateNavBarHeight();

	RECT rc;
	rc.left = rcPane.left;
	rc.top = rcPane.top;
	rc.right = rcPane.right;
	rc.bottom = rc.top + navBarHeight;
	navWnd.SetWindowPos(nullptr, &rc, SWP_NOACTIVATE | SWP_NOZORDER);

	rc.top = rc.top + navWnd.getWindowHeight();
	rc.bottom = rcPane.bottom;
	ctrlList.SetWindowPos(nullptr, &rc, SWP_NOACTIVATE | SWP_NOZORDER);
}

const DirectoryListing::Directory* DirectoryListingFrame::getNavBarItem(int index) const
{
	auto d = reinterpret_cast<const DirectoryListing::Directory*>(ctrlTree.GetItemData(selectedDir));
	if (!d) return nullptr;
	vector<const DirectoryListing::Directory*> path;
	getParsedPath(d, path);
	--index;
	return (index >= 0 && index < (int) path.size()) ? path[index] : nullptr;
}

void DirectoryListingFrame::selectItem(int index)
{
	auto d = getNavBarItem(index);
	if (!d) return;
	HTREEITEM ht = static_cast<HTREEITEM>(d->getUserData());
	ctrlTree.SelectItem(ht);
}

void DirectoryListingFrame::getPopupItems(int index, vector<Item>& res) const
{
	if (index == 0)
	{
		res.emplace_back(Item{ Text::toT(getNick()), getIconForPath(Util::emptyString), 0, IF_DEFAULT });
		return;
	}
	auto d = reinterpret_cast<const DirectoryListing::Directory*>(ctrlTree.GetItemData(selectedDir));
	if (!d) return;
	vector<const DirectoryListing::Directory*> path;
	getParsedPath(d, path);
	if (--index >= (int) path.size()) return;
	d = path[index];
	++index;
	auto nextDir = index < (int) path.size() ? path[index] : nullptr;

	HBITMAP icon = g_iconBitmaps.getBitmap(IconBitmaps::FOLDER, 0);
	for (const auto dir : d->directories)
	{
		uint16_t flags = 0;
		if (dir == nextDir) flags |= IF_DEFAULT;
		res.emplace_back(Item{ Text::toT(dir->getName()), icon, (uintptr_t) dir, flags });
	}
}

void DirectoryListingFrame::selectPopupItem(int index, const tstring& text, uintptr_t itemData)
{
	if (!itemData)
	{
		ctrlTree.SelectItem(treeRoot);
		return;
	}
	auto d = reinterpret_cast<const DirectoryListing::Directory*>(itemData);
	HTREEITEM ht = static_cast<HTREEITEM>(d->getUserData());
	ctrlTree.SelectItem(ht);
}

tstring DirectoryListingFrame::getCurrentPath() const
{
	HTREEITEM ht = ctrlTree.GetSelectedItem();
	if (!ht) return tstring();
	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(ht));
	string path = dl->getPath(dir);
	if (path.empty()) path += '\\';
	return Text::toT(path);
}

HBITMAP DirectoryListingFrame::getCurrentPathIcon() const
{
	int icon = IconBitmaps::FOLDER;
	HTREEITEM ht = ctrlTree.GetSelectedItem();
	if (ht)
	{
		DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(ht));
		if (!dir->getParent())
			icon = dclstFlag ? IconBitmaps::DCLST : IconBitmaps::FILELIST;
	}
	return g_iconBitmaps.getBitmap(icon, 0);
}

bool DirectoryListingFrame::setCurrentPath(const tstring& path)
{
	if (path.empty()) return false;
	string s1 = Text::fromT(path);
	string s2 = s1;
	char* c = &s2[0];
	size_t len = s2.length();
	for (size_t i = 0; i < len; ++i)
		if (c[i] == '\\') c[i] = '/';
	const DirectoryListing::Directory* dir = dl->findDirPathNoCase(s2);
	if (!dir) return false;
	const HTREEITEM ht = (HTREEITEM) dir->getUserData();
	if (!ht) return false;
	addTypedHistory(s1);
	ctrlTree.EnsureVisible(ht);
	ctrlTree.SelectItem(ht);
	return true;
}

uint64_t DirectoryListingFrame::getHistoryState() const
{
	return typedHistoryState;
}

void DirectoryListingFrame::getHistoryItems(vector<HistoryItem>& res) const
{
	res.resize(typedHistory.size());
	for (size_t i = 0; i < typedHistory.size(); ++i)
	{
		res[i].text = Text::toT(typedHistory[i]);
		res[i].icon = getIconForPath(typedHistory[i]);
	}
}

tstring DirectoryListingFrame::getHistoryItem(int index) const
{
	if (index >= 0 && index < (int) typedHistory.size())
		return Text::toT(typedHistory[index]);
	return Util::emptyStringT;
}

HBITMAP DirectoryListingFrame::getChevronMenuImage(int index, uintptr_t itemData)
{
	int icon = IconBitmaps::FOLDER;
	if (static_cast<int>(itemData) == DirectoryListingNavWnd::PATH_ITEM_ROOT)
		icon = dclstFlag ? IconBitmaps::DCLST : IconBitmaps::FILELIST;
	return g_iconBitmaps.getBitmap(icon, 0);
}

HBITMAP DirectoryListingFrame::getIconForPath(const string& path) const
{
	int icon = IconBitmaps::FOLDER;
	if (path.empty() || (path.length() == 1 && path[0] == '\\'))
		icon = dclstFlag ? IconBitmaps::DCLST : IconBitmaps::FILELIST;
	return g_iconBitmaps.getBitmap(icon, 0);
}

int ThreadedDirectoryListing::run()
{
	try
	{
		switch (mode)
		{
			case MODE_LOAD_FILE:
			{
				dcassert(!filePath.empty());
				DirectoryListing* dl = window->dl.get();
				const string filename = Util::getFileName(filePath);
				const UserPtr& user = dl->getUser();
				window->updateWindowTitle();
				dl->loadFile(filePath, this, user->isMe());
				window->updateWindowTitle();
				auto adls = ADLSearchManager::getInstance();
				if (!adls->isEmpty())
				{
					window->PostMessage(WM_SPEAKER, DirectoryListingFrame::ADL_SEARCH);
					adls->matchListing(dl, &window->abortFlag);
					if (window->abortFlag) throw AbortException("ADL search aborted");
				}
				if (!dl->isOwnList())
				{
					auto ss = SettingsManager::instance.getCoreSettings();
					ss->lockRead();
					bool matchList = ss->getBool(Conf::AUTO_MATCH_DOWNLOADED_LISTS);
					ss->unlockRead();
					if (matchList) QueueManager::getInstance()->matchListing(*dl);
				}
				if (window->getDclstFlag() && dl->getIncludeSelf())
					dl->addDclstSelf(filePath, window->abortFlag);
				window->refreshTree(dl->getRoot(), window->treeRoot, false, Util::toAdcFile(Text::fromT(directory)));
				break;
			}
			case MODE_SUBTRACT_FILE:
			{
				dcassert(!filePath.empty());
				DirectoryListing newListing(window->abortFlag, false);
				newListing.loadFile(filePath, this, window->ownList);
				if (!newListing.isAborted())
				{
					window->dl->getRoot()->filterList(*newListing.getTTHSet());
					window->filteredListFlag = true;
					window->refreshTree(window->dl->getRoot(), window->treeRoot, false);
					window->updateRootItemText();
				}
				break;
			}
			case MODE_COMPARE_FILE:
			{
				dcassert(!filePath.empty());
				DirectoryListing newListing(window->abortFlag, false);
				newListing.loadFile(filePath, this, false);
				if (!newListing.isAborted())
				{
					window->dl->getRoot()->matchTTHSet(*newListing.getTTHSet());
				}
				break;
			}
			case MODE_LOAD_PARTIAL_LIST:
			{
				unique_ptr<DirectoryListing> newListing(new DirectoryListing(window->abortFlag));
				newListing->setScanOptions(window->dl->getScanOptions());
				newListing->setHintedUser(window->dl->getHintedUser());
				newListing->loadXML(text, this, false);
				WinUtil::postSpeakerMsg(*window, DirectoryListingFrame::SPLICE_TREE, newListing.release());
				break;
			}
			default:
				dcassert(0);
		}
		window->PostMessage(WM_SPEAKER, DirectoryListingFrame::FINISHED, mode);
	}
	catch (const AbortException&)
	{
		auto error = new DirectoryListingFrame::ErrorInfo;
		error->mode = mode;
		window->PostMessage(WM_SPEAKER, DirectoryListingFrame::ABORTED, reinterpret_cast<LPARAM>(error));
	}
	catch (const Exception& e)
	{
		auto error = new DirectoryListingFrame::ErrorInfo;
		error->text = TSTRING(ERROR_PARSING_FILE_LIST);
		error->text += Text::toT(e.getError());
		error->mode = mode;
		window->PostMessage(WM_SPEAKER, DirectoryListingFrame::ABORTED, reinterpret_cast<LPARAM>(error));
	}

	//cleanup the thread object
	delete this;

	return 0;
}

void DirectoryListingFrame::onTab()
{
	HWND focus = ::GetFocus();
	HWND hWnd[] = { ctrlTree.m_hWnd, ctrlList.m_hWnd };
	int index = -1;
	for (int i = 0; i < _countof(hWnd); i++)
		if (focus == hWnd[i])
		{
			index = i;
			break;
		}
	if (index >= 0)
	{
		index += WinUtil::isCtrl() ? -1 : 1;
		index %= _countof(hWnd);
	}
	else
		index = 0;
	::SetFocus(hWnd[index]);
}

void ThreadedDirectoryListing::notify(int progress)
{
	window->PostMessage(WM_SPEAKER, DirectoryListingFrame::PROGRESS | progress);
}

BOOL DirectoryListingFrame::PreTranslateMessage(MSG* pMsg)
{
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	MainFrame* mainFrame = MainFrame::getMainFrame();
	return TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg);
}

CFrameWndClassInfo& DirectoryListingFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("DirectoryListingFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::FILELIST, 0);

	return wc;
}
