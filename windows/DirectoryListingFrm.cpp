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

#include "../client/QueueManager.h"
#include "../client/ClientManager.h"
#include "../client/util_flylinkdc.h"
#include "../client/ShareManager.h"
#include "DirectoryListingFrm.h"
#include "PrivateFrame.h"
#include "DclstGenDlg.h"
#include "MainFrm.h"
#include "SearchDlg.h"

#define SCALOLAZ_DIRLIST_ADDFAVUSER

static const int BUTTON_SPACE = 8;
static const int STATUS_PART_PADDING = 12;

HIconWrapper DirectoryListingFrame::frameIcon(IDR_FILE_LIST);
HIconWrapper DirectoryListingFrame::frameIconOffline(IDR_FILE_LIST_OFFLINE);

DirectoryListingFrame::UserList DirectoryListingFrame::userList;
CriticalSection DirectoryListingFrame::lockUserList;

DirectoryListingFrame::FrameMap DirectoryListingFrame::activeFrames;

const int DirectoryListingFrame::columnId[] =
{
	COLUMN_FILENAME,
	COLUMN_TYPE,
	COLUMN_SIZE,
	COLUMN_TTH,
	COLUMN_PATH,
	COLUMN_HIT,
	COLUMN_TS,
	COLUMN_EXACT_SIZE,
	COLUMN_BITRATE,
	COLUMN_MEDIA_XY,
	COLUMN_MEDIA_VIDEO,
	COLUMN_MEDIA_AUDIO,
	COLUMN_DURATION
};

static const int columnSizes[] =
{
	200, // COLUMN_FILENAME
	60,  // COLUMN_TYPE
	85,  // COLUMN_SIZE
	200, // COLUMN_TTH
	300, // COLUMN_PATH
	50,  // COLUMN_HIT
	120, // COLUMN_TS
	100, // COLUMN_EXACT_SIZE
	80,  // COLUMN_BITRATE
	100, // COLUMN_MEDIA_XY
	100, // COLUMN_MEDIA_VIDEO
	100, // COLUMN_MEDIA_AUDIO
	80   // COLUMN_DURATION
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::FILE,
	ResourceManager::TYPE,
	ResourceManager::SIZE,
	ResourceManager::TTH_ROOT,
	ResourceManager::PATH,
	ResourceManager::DOWNLOADED,
	ResourceManager::ADDED,
	ResourceManager::EXACT_SIZE,
	ResourceManager::BITRATE,
	ResourceManager::MEDIA_X_Y,
	ResourceManager::MEDIA_VIDEO,
	ResourceManager::MEDIA_AUDIO,
	ResourceManager::DURATION
};

static SearchOptions g_searchOptions;

DirectoryListingFrame::~DirectoryListingFrame()
{
	removeFromUserList();
}

void DirectoryListingFrame::addToUserList(const UserPtr& user, bool isBrowsing)
{
	if (!user || user->getCID().isZero()) return;
	CFlyLock(lockUserList);
	for (auto i = userList.cbegin(); i != userList.cend(); ++i)
	{
		const UserFrame& uf = *i;
		if (uf.user == user && uf.isBrowsing == isBrowsing) return;
	}
	UserFrame uf;
	uf.user = user;
	uf.isBrowsing = isBrowsing;
	uf.frame = this;
	userList.push_back(uf);
}

void DirectoryListingFrame::removeFromUserList()
{
	CFlyLock(lockUserList);
	for (auto i = userList.cbegin(); i != userList.cend(); ++i)
		if (i->frame == this)
		{
			userList.erase(i);
			break;
		}
}

void DirectoryListingFrame::openWindow(const tstring& aFile, const tstring& aDir, const HintedUser& aUser, int64_t speed, bool isDCLST /*= false*/)
{
#if 0 // Always open new window!
	bool l_is_users_map_exists;
	{
		CFlyLock(g_csUsersMap);
		auto i = g_usersMap.end();
		if (!isDCLST && aHintedUser.user && !aHintedUser.user->getCID().isZero())
			i = g_usersMap.find(aHintedUser);

		l_is_users_map_exists = i != g_usersMap.end();
		if (l_is_users_map_exists)
		{
			if (!BOOLSETTING(POPUNDER_FILELIST))
			{
				i->second->speed = speed;
				i->second->MDIActivate(i->second->m_hWnd);
			}
		}
	}
	if (!l_is_users_map_exists)
#endif
	{
		HWND aHWND = NULL;
		DirectoryListingFrame* frame = new DirectoryListingFrame(aUser, nullptr);
		frame->setSpeed(speed);
		frame->setDclstFlag(isDCLST);
		frame->setFileName(Text::fromT(aFile));
		if (BOOLSETTING(POPUNDER_FILELIST))
			aHWND = WinUtil::hiddenCreateEx(frame);
		else
			aHWND = frame->CreateEx(WinUtil::g_mdiClient);
		if (aHWND)
		{
			frame->loadFile(aFile, aDir);
			activeFrames.insert(DirectoryListingFrame::FrameMap::value_type(frame->m_hWnd, frame));
		}
		else
		{
			delete frame;
		}
	}
}

void DirectoryListingFrame::openWindow(const HintedUser& aUser, const string& txt, int64_t speed)
{
	{
		CFlyLock(lockUserList);
		for (auto i = userList.begin(); i != userList.end(); ++i)
		{
			UserFrame& frame = *i;
			if (frame.user == aUser.user && frame.isBrowsing)
			{
				frame.frame->setSpeed(speed);
				frame.frame->loadXML(txt);
				return;
			}
		}
	}
	DirectoryListingFrame* frame = new DirectoryListingFrame(aUser, nullptr);
	frame->setSpeed(speed);
	frame->addToUserList(aUser, true);
	if (BOOLSETTING(POPUNDER_FILELIST))
		WinUtil::hiddenCreateEx(frame);
	else
		frame->CreateEx(WinUtil::g_mdiClient);
	frame->loadXML(txt);
	activeFrames.insert(DirectoryListingFrame::FrameMap::value_type(frame->m_hWnd, frame));
}

void DirectoryListingFrame::openWindow(DirectoryListing *dl, const HintedUser& aUser, int64_t speed, bool searchResults)
{
	DirectoryListingFrame* frame = new DirectoryListingFrame(aUser, dl);
	frame->searchResultsFlag = searchResults;
	frame->setSpeed(speed);
	if (BOOLSETTING(POPUNDER_FILELIST))
		WinUtil::hiddenCreateEx(frame);
	else
		frame->CreateEx(WinUtil::g_mdiClient);
	frame->setWindowTitle();
	frame->refreshTree(frame->dl->getRoot(), frame->treeRoot);
	frame->loading = false;
	frame->initStatus();
	frame->enableControls();
	frame->ctrlStatus.SetText(STATUS_TEXT, _T(""));
	activeFrames.insert(DirectoryListingFrame::FrameMap::value_type(frame->m_hWnd, frame));
}

DirectoryListingFrame::DirectoryListingFrame(const HintedUser &user, DirectoryListing *dl) :
	TimerHelper(m_hWnd),
	statusContainer(STATUSCLASSNAME, this, STATUS_MESSAGE_MAP),
	treeContainer(WC_TREEVIEW, this, CONTROL_MESSAGE_MAP),
	listContainer(WC_LISTVIEW, this, CONTROL_MESSAGE_MAP),
	historyIndex(0),
	setWindowTitleTick(0),
	treeRoot(nullptr), selectedDir(nullptr), hTheme(nullptr),
	dclstFlag(false), searchResultsFlag(false), filteredListFlag(false),
	updating(false), loading(true), refreshing(false), listItemChanged(false), offline(false),
	abortFlag(false)
{
	if (!dl) dl = new DirectoryListing(abortFlag);
	this->dl.reset(dl);
	dl->setHintedUser(user);

	colorShared = RGB(30,213,75);
	colorSharedLighter = HLS_TRANSFORM(colorShared, 35, -20);
	colorDownloaded = RGB(79,172,176);
	colorDownloadedLighter = HLS_TRANSFORM(colorDownloaded, 35, -20);
	colorCanceled = RGB(196,114,196);
	colorCanceledLighter = HLS_TRANSFORM(colorCanceled, 35, -20);
	colorFound = RGB(255,255,0);
	colorFoundLighter = HLS_TRANSFORM(colorFound, 20, -20);
	colorInQueue = RGB(186,0,42);

	ctrlList.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlList.setColumnFormat(COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(COLUMN_EXACT_SIZE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(COLUMN_TYPE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(COLUMN_HIT, LVCFMT_RIGHT);
}

void DirectoryListingFrame::setWindowTitle()
{
	setWindowTitleTick = 0;
	if (dclstFlag)
	{
		SetWindowText(Text::toT(Util::getFileName(getFileName())).c_str());
		return;
	}
	if (dl->getUser() && !dl->getUser()->getCID().isZero())
	{
		const HintedUser &user = dl->getHintedUser();
		if (user.user)
		{
			bool userOffline = false;
			tstring text = Text::toT(user.user->getLastNick());
			if (!user.user->isMe())
			{
				text += _T(" - ");
				StringList hubNames;
				if (!user.hint.empty())
					hubNames = ClientManager::getHubNames(user.user->getCID(), user.hint, true);
				if (hubNames.empty())
					hubNames = ClientManager::getHubNames(user.user->getCID(), Util::emptyString, false);
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
			SetWindowText(text.c_str());
			if (offline != userOffline)
			{
				offline = userOffline;
				setDisconnected(offline);
			}
		}
	}
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
		tdl->start(0);
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
		tdl->start(0);
	}
	catch (const ThreadException& e)
	{
		delete tdl;
		loading = false;
		LogManager::message("DirectoryListingFrame::loadXML error: " + e.getError());
	}
}

int DirectoryListingFrame::getButtonWidth(ResourceManager::Strings id) const
{
	int width = WinUtil::getTextWidth(TSTRING_I(id), ctrlStatus) + 10;
	if (width < 50) width = 50;
	return width + BUTTON_SPACE;
}

LRESULT DirectoryListingFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.SetFont(Fonts::g_systemFont);
	statusContainer.SubclassWindow(ctrlStatus.m_hWnd);

	ctrlTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WinUtil::getTreeViewStyle(), WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	WinUtil::setExplorerTheme(ctrlTree);
	treeContainer.SubclassWindow(ctrlTree);

	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_FILES);
	listContainer.SubclassWindow(ctrlList);
	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));

	hTheme = OpenThemeData(m_hWnd, L"EXPLORER::LISTVIEW");
	if (hTheme)
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED;
	customDrawState.flags |= CustomDrawHelpers::FLAG_GET_COLFMT;

	setListViewColors(ctrlList);
	ctrlTree.SetBkColor(Colors::g_bgColor);
	ctrlTree.SetTextColor(Colors::g_textColor);

	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));

	ctrlList.insertColumns(SettingsManager::DIRLIST_FRAME_ORDER, SettingsManager::DIRLIST_FRAME_WIDTHS, SettingsManager::DIRLIST_FRAME_VISIBLE);

	ctrlList.setSortFromSettings(SETTING(DIRLIST_FRAME_SORT));

	ctrlTree.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);

	ctrlFind.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED |
	                BS_PUSHBUTTON, 0, IDC_FIND);
	ctrlFind.SetWindowText(CTSTRING(FIND));
	ctrlFind.SetFont(Fonts::g_systemFont);

	ctrlFindPrev.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED |
	                    BS_PUSHBUTTON, 0, IDC_PREV);
	ctrlFindPrev.SetWindowText(CTSTRING(PREV));
	ctrlFindPrev.SetFont(Fonts::g_systemFont);

	ctrlFindNext.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED |
	                    BS_PUSHBUTTON, 0, IDC_NEXT);
	ctrlFindNext.SetWindowText(CTSTRING(NEXT));
	ctrlFindNext.SetFont(Fonts::g_systemFont);

	ctrlMatchQueue.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED |
	                      BS_PUSHBUTTON, 0, IDC_MATCH_QUEUE);
	ctrlMatchQueue.SetWindowText(CTSTRING(MATCH_QUEUE));
	ctrlMatchQueue.SetFont(Fonts::g_systemFont);

	ctrlListDiff.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED |
	                    BS_PUSHBUTTON, 0, IDC_FILELIST_DIFF);
	ctrlListDiff.SetWindowText(CTSTRING(FILE_LIST_DIFF));
	ctrlListDiff.SetFont(Fonts::g_systemFont);

	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
	SetSplitterPanes(ctrlTree.m_hWnd, ctrlList.m_hWnd);
	m_nProportionalPos = SETTING(DIRLIST_FRAME_SPLIT);
	int icon = dclstFlag ? FileImage::DIR_DSLCT : FileImage::DIR_ICON;
	tstring rootText = getRootItemText();
	treeRoot = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
	                               rootText.c_str(), icon, icon, 0, 0,
	                               NULL, NULL, TVI_LAST);
	dcassert(treeRoot != NULL);

	memset(statusSizes, 0, sizeof(statusSizes));
	statusSizes[STATUS_FILE_LIST_DIFF] = getButtonWidth(ResourceManager::FILE_LIST_DIFF);
	statusSizes[STATUS_MATCH_QUEUE] = getButtonWidth(ResourceManager::MATCH_QUEUE);
	statusSizes[STATUS_FIND] = getButtonWidth(ResourceManager::FIND);
	statusSizes[STATUS_PREV] = getButtonWidth(ResourceManager::PREV);
	statusSizes[STATUS_NEXT] = getButtonWidth(ResourceManager::NEXT);

	ctrlStatus.SetParts(STATUS_LAST, statusSizes);

	targetMenu.CreatePopupMenu();
	targetDirMenu.CreatePopupMenu();
	priorityMenu.CreatePopupMenu();
	priorityDirMenu.CreatePopupMenu();
	copyMenu.CreatePopupMenu();
	if (dl->getUser())
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));

	WinUtil::appendPrioItems(priorityMenu, IDC_DOWNLOAD_WITH_PRIO);
	WinUtil::appendPrioItems(priorityDirMenu, IDC_DOWNLOADDIR_WITH_PRIO);

	ctrlTree.EnableWindow(FALSE);
	SettingsManager::getInstance()->addListener(this);
	closed = false;
	bHandled = FALSE;
	return 1;
}

LRESULT DirectoryListingFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (hTheme)
	{
		CloseThemeData(hTheme);
		hTheme = nullptr;
	}
	bHandled = FALSE;
	return 0;
}

tstring DirectoryListingFrame::getRootItemText() const
{
	const string nick = dclstFlag ? Util::getFileName(getFileName()) : (dl->getUser() ? dl->getUser()->getLastNick() : Util::emptyString);
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

void DirectoryListingFrame::enableControls()
{
	ctrlTree.EnableWindow(TRUE);
	ctrlList.EnableWindow(TRUE);
	ctrlFind.EnableWindow(TRUE);
	ctrlListDiff.EnableWindow(TRUE);
	ctrlMatchQueue.EnableWindow(!dl->isOwnList());
	createTimer(1000);
}

void DirectoryListingFrame::updateTree(DirectoryListing::Directory* aTree, HTREEITEM aParent)
{
	if (aTree)
	{
		for (auto i = aTree->directories.cbegin(); i != aTree->directories.cend(); ++i)
		{
			if (!loading && !isClosedOrShutdown())
				throw AbortException(STRING(ABORT_EM));

			DirectoryListing::Directory *dir = *i;
			const tstring name = Text::toT(dir->getName());
			const auto typeDirectory = GetTypeDirectory(dir);

			HTREEITEM ht = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM, name.c_str(), typeDirectory, typeDirectory, 0, 0, (LPARAM) dir, aParent, TVI_LAST);
			dir->setUserData(static_cast<void*>(ht));
			if (dir->getAdls())
				ctrlTree.SetItemState(ht, TVIS_BOLD, TVIS_BOLD);

			updateTree(dir, ht);
		}
	}
}

void DirectoryListingFrame::refreshTree(DirectoryListing::Directory* dir, HTREEITEM treeItem, const string& selPath)
{
	if (!loading && !isClosedOrShutdown())
		throw AbortException(STRING(ABORT_EM));

	ctrlStatus.SetText(STATUS_TEXT, CTSTRING(PREPARING_FILE_LIST));
	CLockRedraw<> lockRedraw(ctrlTree);
	refreshing = true;
	HTREEITEM next = nullptr;
	while ((next = ctrlTree.GetChildItem(treeItem)) != nullptr)
		ctrlTree.DeleteItem(next);
	updateTree(dir, treeItem);
	const auto typeDirectory = GetTypeDirectory(dir);
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
		setWindowTitle();
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
			addHistory(dl->getPath(d));
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
			if (!(search.getWhatFound() == DirectoryListing::SearchContext::FOUND_FILE &&
				search.getFile() == info->file) && search.setFound(info->file))
				dumpFoundPath();
		}
	}
	listItemChanged = true;
	return 0;
}

void DirectoryListingFrame::addHistory(const string& name)
{
	history.erase(history.begin() + historyIndex, history.end());
	while (history.size() > 25)
		history.pop_front();
	history.push_back(name);
	historyIndex = history.size();
}

// Метод определяет иконку для папки в зависимости от ее содержимого
FileImage::TypeDirectoryImages DirectoryListingFrame::GetTypeDirectory(const DirectoryListing::Directory* dir) const
{
	if (!dir->getComplete())
	{
		// Папка пустая
		return FileImage::DIR_MASKED;
	}

	// Проверяем все подпапки в папке
	for (auto i = dir->directories.cbegin(); i != dir->directories.cend(); ++i)
	{
		const string& nameSubDirectory = (*i)->getName();

		// Папка содержащая хотя бы одну папку BDMV, является Blu-ray папкой
		if (FileImage::isBdFolder(nameSubDirectory))
		{
			return FileImage::DIR_BD; // Папка Blu-ray
		}

		// Папка содержащая подпапки VIDEO_TS или AUDIO_TS, является DVD папкой
		if (FileImage::isDvdFolder(nameSubDirectory))
		{
			return FileImage::DIR_DVD; // Папка DVD
		}
	}

	// Проверяем все файлы в папке
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const string& nameFile = (*i)->getName();
		if (FileImage::isDvdFile(nameFile))
		{
			return FileImage::DIR_DVD; // Папка DVD
		}
	}

	return FileImage::DIR_ICON;
}

void DirectoryListingFrame::changeDir(const DirectoryListing::Directory* dir)
{
	showDirContents(dir, nullptr, nullptr);
	if (dir->isSet(DirectoryListing::FLAG_FOUND) &&
		!(search.getWhatFound() == DirectoryListing::SearchContext::FOUND_DIR &&
		search.getDirectory() == dir))
	{
		if (search.setFound(dir))
			dumpFoundPath();
	}
	if (!dir->getComplete())
	{
		if (dl->getUser()->isOnline())
		{
			try
			{
				QueueManager::getInstance()->addList(dl->getHintedUser(), QueueItem::FLAG_PARTIAL_LIST, dl->getPath(dir));
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

void DirectoryListingFrame::up()
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t == NULL)
		return;
	t = ctrlTree.GetParentItem(t);
	if (t == NULL)
		return;
	ctrlTree.SelectItem(t);
}

void DirectoryListingFrame::back()
{
	if (history.size() > 1 && historyIndex > 1)
	{
		size_t n = min(historyIndex, history.size()) - 1;
		deque<string> tmp = history;
		selectItem(Text::toT(history[n - 1]));
		historyIndex = n;
		history = tmp;
	}
}

void DirectoryListingFrame::forward()
{
	if (history.size() > 1 && historyIndex < history.size())
	{
		size_t n = min(historyIndex, history.size() - 1);
		deque<string> tmp = history;
		selectItem(Text::toT(history[n]));
		historyIndex = n + 1;
		history = tmp;
	}
}

LRESULT DirectoryListingFrame::onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(i);
		string realPath;
		if (ShareManager::getInstance()->getFilePath(ii->file->getTTH(), realPath))
			openFileFromList(Text::toT(realPath));
	}
	return 0;
}

LRESULT DirectoryListingFrame::onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	if ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlList.getItemData(i);
		string realPath;
		if (ShareManager::getInstance()->getFilePath(ii->file->getTTH(), realPath))
			WinUtil::openFolder(Text::toT(realPath));
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDoubleClickFiles(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	const HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t != NULL && item->iItem != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(item->iItem);
		if (ii->type == ItemInfo::FILE)
		{
			const tstring& path = ii->columns[COLUMN_PATH];
			if (!path.empty())
			{
				openFileFromList(path);
			}
			else
			{
				try
				{
					QueueItem::Priority prio = WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT;
					bool getConnFlag = true;
					if (Util::isDclstFile(ii->file->getName()))
						dl->download(ii->file, Text::fromT(ii->getText(COLUMN_FILENAME)), true, prio, true, getConnFlag);
					else
						dl->download(ii->file, Text::fromT(ii->getText(COLUMN_FILENAME)), false, prio, false, getConnFlag);

				}
				catch (const Exception& e)
				{
					ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
				}
			}
		}
		else
		{
			HTREEITEM ht = ctrlTree.GetChildItem(t);
			while (ht != NULL)
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
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadDirWithPrio(WORD, WORD wID, HWND, BOOL&)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;
	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;

	int prio = wID - IDC_DOWNLOADDIR_WITH_PRIO;
	if (!(prio >= QueueItem::PAUSED && prio < QueueItem::LAST))
		prio = QueueItem::DEFAULT;
	bool getConnFlag = true;

	try
	{
		dl->download(dir, SETTING(DOWNLOAD_DIRECTORY), (QueueItem::Priority) prio, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadDirTo(WORD, WORD, HWND, BOOL&)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;
	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;

	tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
	if (WinUtil::browseDirectory(target, m_hWnd))
	{
		LastDir::add(target);
		bool getConnFlag = true;
		try
		{
			dl->download(dir, Text::fromT(target), WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
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

	bool getConnFlag = true;
	try
	{
		dl->download(dir, useDir, WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	return 0;
}

void DirectoryListingFrame::downloadList(const tstring& aTarget, bool view /* = false */, QueueItem::Priority prio /* = QueueItem::Priority::DEFAULT */)
{
	int i = -1;
	bool getConnFlag = true;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(i);

		const tstring& target = aTarget.empty() ? Text::toT(FavoriteManager::getDownloadDirectory(Text::fromT(Util::getFileExt(ii->getText(COLUMN_FILENAME))))) : aTarget;

		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				if (view)
				{
					File::deleteFile(target + Text::toT(Util::validateFileName(ii->file->getName())));
				}
				dl->download(ii->file, Text::fromT(target + ii->getText(COLUMN_FILENAME)), view,
					(WinUtil::isShift() || view) ? QueueItem::HIGHEST : prio, false, getConnFlag);
			}
			else if (!view)
			{
				dl->download(ii->dir, Text::fromT(target), WinUtil::isShift() ? QueueItem::HIGHEST : prio, getConnFlag);
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
}

LRESULT DirectoryListingFrame::onDownloadWithPrio(WORD, WORD wID, HWND, BOOL&)
{
	int prio = wID - IDC_DOWNLOAD_WITH_PRIO;
	if (!(prio >= QueueItem::PAUSED && prio < QueueItem::LAST))
		prio = QueueItem::DEFAULT;

	downloadList(false, (QueueItem::Priority) prio);
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadTo(WORD, WORD, HWND, BOOL&)
{
	if (ctrlList.GetSelectedCount() == 1)
	{
		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		bool getConnFlag = true;
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY)) + ii->getText(COLUMN_FILENAME);
				if (WinUtil::browseFile(target, m_hWnd))
				{
					LastDir::add(Util::getFilePath(target));
					dl->download(ii->file, Text::fromT(target), false,
						WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, false, getConnFlag);
				}
			}
			else
			{
				tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
				if (WinUtil::browseDirectory(target, m_hWnd))
				{
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
	}
	else
	{
		tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
		if (WinUtil::browseDirectory(target, m_hWnd))
		{
			LastDir::add(target);
			downloadList(target);
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
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				string filename = Text::fromT(ii->getText(COLUMN_FILENAME));
				dl->download(ii->file, useDir + filename, false,
					WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, false, getConnFlag);
			} else
			{
				dl->download(ii->dir, useDir,
					WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	else
	{
		downloadList(Text::toT(useDir));
	}
	return 0;
}

#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
LRESULT DirectoryListingFrame::onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	downloadList(Text::toT(Util::getTempPath()), true);
	return 0;
}
#endif

LRESULT DirectoryListingFrame::onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ItemInfo* ii = ctrlList.getSelectedItem();
	if (ii && ii->type == ItemInfo::FILE)
	{
		WinUtil::searchHash(ii->file->getTTH());
	}
	return 0;
}

LRESULT DirectoryListingFrame::onAddToFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (const UserPtr& pUser = dl->getUser())
	{
		FavoriteManager::getInstance()->addFavoriteUser(pUser);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onMarkAsDownloaded(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
#if 0 // FIXME: removed
	int i = -1;

	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* pItemInfo = ctrlList.getItemData(i);
		if (pItemInfo->type == ItemInfo::FILE)
		{
			DirectoryListing::File *file = const_cast<DirectoryListing::File*>(pItemInfo->file);
			CFlylinkDBManager::getInstance()->push_download_tth(file->getTTH());
			file->setFlag(DirectoryListing::FLAG_DOWNLOAD);
			pItemInfo->UpdatePathColumn(file);
			ctrlList.updateItem(pItemInfo);
		}
		else
		{
			// Recursively add directory content?
		}
	}
#endif
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

LRESULT DirectoryListingFrame::onMatchQueue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int count = QueueManager::getInstance()->matchListing(*dl);
	tstring str = TSTRING_F(MATCHED_FILES_FMT, count);
	ctrlStatus.SetText(STATUS_TEXT, str.c_str());
	return 0;
}

LRESULT DirectoryListingFrame::onListDiff(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
	if (WinUtil::browseFile(file, m_hWnd, false, Text::toT(Util::getListPath()), g_file_list_type))
	{
		ctrlStatus.SetText(STATUS_TEXT, CTSTRING(LOADING_FILE_LIST));
		ctrlTree.SelectItem(NULL); // refreshTree won't select item without this
		ctrlTree.EnableWindow(FALSE);
		ctrlList.EnableWindow(FALSE);
		ctrlFind.EnableWindow(FALSE);
		ctrlFindNext.EnableWindow(FALSE);
		ctrlFindPrev.EnableWindow(FALSE);
		ctrlListDiff.EnableWindow(FALSE);
		ctrlMatchQueue.EnableWindow(FALSE);
		destroyTimer();
		loadStartTime = GET_TICK();
		ThreadedDirectoryListing* tdl = new ThreadedDirectoryListing(this, ThreadedDirectoryListing::MODE_SUBTRACT_FILE);
		tdl->setFile(Text::fromT(file));
		loading = true;
		try
		{
			tdl->start(0);
		}
		catch (const ThreadException& e)
		{
			delete tdl;
			loading = false;
			enableControls();
			LogManager::message("DirectoryListingFrame::onListDiff error: " + e.getError());
		}
	}
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
		const DirectoryListing::Directory* pd = ii->file->getParent();
		while (pd != NULL && pd != dl->getRoot())
		{
			fullPath = Text::toT(pd->getName()) + _T("\\") + fullPath;
			pd = pd->getParent();
		}
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

void DirectoryListingFrame::appendTargetMenu(OMenu& menu, const int idc)
{
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& dirs = lockedInstance.getFavoriteDirsL();
	if (!dirs.empty())
	{
		int n = 0;
		for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
		{
			tstring tmp = Text::toT(i->name);
			WinUtil::escapeMenu(tmp);
			menu.AppendMenu(MF_STRING, idc + n, tmp.c_str());
			n++;
		}
		menu.AppendMenu(MF_SEPARATOR);
	}
}

void DirectoryListingFrame::appendCustomTargetItems(OMenu& menu, int idc)
{
	User* user = dl->getUser().get();
	auto ip = user->getIP();
	string downloadDir = SETTING(DOWNLOAD_DIRECTORY);
	string nick = user->getLastNick();
	if (!nick.empty())
	{
		downloadDirNick = downloadDir + nick;
		tstring tmp = Text::toT(downloadDirNick);
		WinUtil::escapeMenu(tmp);
		menu.AppendMenu(MF_STRING, idc, tmp.c_str());
		Util::appendPathSeparator(downloadDirNick);
	}
	if (!ip.is_unspecified())
	{
		downloadDirIP = downloadDir + ip.to_string();
		tstring tmp = Text::toT(downloadDirIP);
		WinUtil::escapeMenu(tmp);
		menu.AppendMenu(MF_STRING, idc + 1, tmp.c_str());
		Util::appendPathSeparator(downloadDirIP);
	}
}

LRESULT DirectoryListingFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bool ownList = dl->isOwnList();
	if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlList, pt);
		}

		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));

		for (int i = targetMenu.GetMenuItemCount()-1; i >= 0; i--)
			targetMenu.DeleteMenu(i, MF_BYPOSITION);

		OMenu fileMenu;
		fileMenu.CreatePopupMenu();

		if (!ownList)
			fileMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_WITH_PRIO + DEFAULT_PRIO, CTSTRING(DOWNLOAD));
		fileMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
		fileMenu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
		if (!ownList)
		{
			fileMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)targetMenu, CTSTRING(DOWNLOAD_TO));
			fileMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
		}
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		fileMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));
#endif
		fileMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
#if 0
		fileMenu.AppendMenu(MF_STRING, IDC_MARK_AS_DOWNLOADED, CTSTRING(MARK_AS_DOWNLOADED));
#endif
		fileMenu.AppendMenu(MF_SEPARATOR);

		fileMenu.AppendMenu(MF_STRING, IDC_GENERATE_DCLST_FILE, CTSTRING(DCLS_GENERATE_LIST));
		fileMenu.AppendMenu(MF_SEPARATOR);
		fileMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
#ifdef SCALOLAZ_DIRLIST_ADDFAVUSER
		if (!ownList)
			fileMenu.AppendMenu(MF_STRING, IDC_ADD_TO_FAVORITES, CTSTRING(ADD_TO_FAVORITES));
#endif
		fileMenu.AppendMenu(MF_SEPARATOR);
		int searchIndex = fileMenu.GetMenuItemCount();
		appendInternetSearchItems(fileMenu);

		if (ctrlList.GetSelectedCount() == 1 && ii->type == ItemInfo::FILE)
		{
			fileMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MF_BYCOMMAND | MFS_ENABLED);
			fileMenu.EnableMenuItem(searchIndex, MF_BYPOSITION | MFS_ENABLED);
			fileMenu.EnableMenuItem(IDC_GENERATE_DCLST_FILE, MF_BYCOMMAND | MFS_DISABLED);

			//Append Favorite download dirs.
			if (!ownList)
			{
				appendTargetMenu(targetMenu, IDC_DOWNLOAD_FAVORITE_DIRS);
				targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOADTO, CTSTRING(BROWSE));
				appendCustomTargetItems(targetMenu, IDC_DOWNLOADTO_USER);

				targets.clear();
				QueueManager::getTargets(ii->file->getTTH(), targets);

				int n = 0;
				if (!targets.empty())
				{
					targetMenu.AppendMenu(MF_SEPARATOR);
					for (auto i = targets.cbegin(); i != targets.cend(); ++i)
					{
						tstring tmp = Text::toT(*i);
						WinUtil::escapeMenu(tmp);
						targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET + (++n), tmp.c_str());
					}
				}
				LastDir::appendItem(targetMenu, n);
			}

			bool existingFile = true;
			if (!ownList)
			{
				string unused;
				existingFile = ShareManager::getInstance()->getFilePath(ii->file->getTTH(), unused);
			}
			if (existingFile)
			{
				fileMenu.SetMenuDefaultItem(IDC_OPEN_FILE);
				fileMenu.EnableMenuItem(IDC_OPEN_FILE, MF_BYCOMMAND | MFS_ENABLED);
				fileMenu.EnableMenuItem(IDC_OPEN_FOLDER, MF_BYCOMMAND | MFS_ENABLED);
			}
			else
			{
				fileMenu.SetMenuDefaultItem(IDC_DOWNLOAD_WITH_PRIO + DEFAULT_PRIO);
				fileMenu.EnableMenuItem(IDC_OPEN_FILE, MF_BYCOMMAND | MFS_DISABLED);
				fileMenu.EnableMenuItem(IDC_OPEN_FOLDER, MF_BYCOMMAND | MFS_DISABLED);
			}
			if (ii->file->getAdls())
				fileMenu.AppendMenu(MF_STRING, IDC_GO_TO_DIRECTORY, CTSTRING(GO_TO_DIRECTORY));
			copyMenu.RenameItem(IDC_COPY_FILENAME, TSTRING(FILENAME));

			//fileMenu.EnableMenuItem((UINT_PTR)(HMENU)copyMenu, MF_BYCOMMAND | MFS_ENABLED);
			appendUcMenu(fileMenu, UserCommand::CONTEXT_FILELIST, ClientManager::getHubs(dl->getUser()->getCID(), dl->getHintedUser().hint));
			fileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			cleanUcMenu(fileMenu);
		}
		else   // if(ctrlList.GetSelectedCount() == 1 && ii->type == ItemInfo::FILE) {
		{
			int count = ctrlList.GetSelectedCount();
			fileMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MF_BYCOMMAND | MFS_DISABLED);
			fileMenu.EnableMenuItem(searchIndex, MF_BYPOSITION | MFS_DISABLED);
			//fileMenu.EnableMenuItem((UINT_PTR)(HMENU)copyMenu, MF_BYCOMMAND | MFS_DISABLED); // !SMT!-UI
			fileMenu.EnableMenuItem(IDC_GENERATE_DCLST_FILE, MF_BYCOMMAND | (count == 1 ? MFS_ENABLED : MFS_DISABLED));    // [+] SSA
			//Append Favorite download dirs
			if (!ownList)
			{
				appendTargetMenu(targetMenu, IDC_DOWNLOAD_FAVORITE_DIRS);
				targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOADTO, CTSTRING(BROWSE));
				appendCustomTargetItems(targetMenu, IDC_DOWNLOADTO_USER);
			}

			int n = 0;
			LastDir::appendItem(targetMenu, n);
			if (ii->type == ItemInfo::DIRECTORY &&
			    ii->dir->getAdls() && ii->dir->getParent() != dl->getRoot())
			{
				fileMenu.AppendMenu(MF_STRING, IDC_GO_TO_DIRECTORY, CTSTRING(GO_TO_DIRECTORY));
			}
			copyMenu.RenameItem(IDC_COPY_FILENAME, TSTRING(FOLDERNAME));

			appendUcMenu(fileMenu, UserCommand::CONTEXT_FILELIST, ClientManager::getHubs(dl->getUser()->getCID(), dl->getHintedUser().hint));
			fileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			cleanUcMenu(fileMenu);
		}
		WinUtil::unlinkStaticMenus(fileMenu); // TODO - fix copy-paste

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

		OMenu directoryMenu;
		directoryMenu.CreatePopupMenu();

		for (int i = targetDirMenu.GetMenuItemCount()-1; i >= 0; i--)
			targetDirMenu.DeleteMenu(i, MF_BYPOSITION);

		if (!ownList)
		{
#ifdef SCALOLAZ_DIRLIST_ADDFAVUSER
			directoryMenu.AppendMenu(MF_STRING, IDC_ADD_TO_FAVORITES, CTSTRING(ADD_TO_FAVORITES));
			directoryMenu.AppendMenu(MF_SEPARATOR);
#endif
			directoryMenu.AppendMenu(MF_STRING, IDC_DOWNLOADDIR_WITH_PRIO + DEFAULT_PRIO, CTSTRING(DOWNLOAD));
			directoryMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)targetDirMenu, CTSTRING(DOWNLOAD_TO));
			directoryMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityDirMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
			directoryMenu.AppendMenu(MF_SEPARATOR);

			appendTargetMenu(targetDirMenu, IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS);
			targetDirMenu.AppendMenu(MF_STRING, IDC_DOWNLOADDIRTO, CTSTRING(BROWSE));
			appendCustomTargetItems(targetDirMenu, IDC_DOWNLOADDIRTO_USER);

			tstring tmp;
			int n = 0;
			if (!LastDir::get().empty())
			{
				targetDirMenu.AppendMenu(MF_SEPARATOR);
				for (auto i = LastDir::get().cbegin(); i != LastDir::get().cend(); ++i)
				{
					targetDirMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET_DIR + (++n), WinUtil::escapeMenu(*i, tmp).c_str());
				}
			}
		}

		directoryMenu.AppendMenu(MF_STRING, IDC_GENERATE_DCLST, CTSTRING(DCLS_GENERATE_LIST));
		directoryMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);

		return TRUE;
	}

	bHandled = FALSE;
	return FALSE;
}

LRESULT DirectoryListingFrame::onXButtonUp(UINT /*uMsg*/, WPARAM wParam, LPARAM /* lParam */, BOOL& /* bHandled */)
{
	if (GET_XBUTTON_WPARAM(wParam) & XBUTTON1)
	{
		back();
		return TRUE;
	}
	else if (GET_XBUTTON_WPARAM(wParam) & XBUTTON2)
	{
		forward();
		return TRUE;
	}

	return FALSE;
}

LRESULT DirectoryListingFrame::onDownloadTarget(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_TARGET - 1;
	dcassert(newId >= 0);

	if (ctrlList.GetSelectedCount() == 1)
	{
		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		bool getConnFlag = true;
		if (ii->type == ItemInfo::FILE)
		{
			if (newId < (int)targets.size())
			{
				try
				{
					dl->download(ii->file, targets[newId], false,
						WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, false, getConnFlag);
				}
				catch (const Exception& e)
				{
					ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
				}
			}
			else
			{
				newId -= (int)targets.size();
				dcassert(newId < (int)LastDir::get().size());
				downloadList(LastDir::get()[newId]);
			}
		}
		else
		{
			dcassert(newId < (int)LastDir::get().size());
			downloadList(LastDir::get()[newId]);
		}
	}
	else if (ctrlList.GetSelectedCount() > 1)
	{
		dcassert(newId < (int)LastDir::get().size());
		downloadList(LastDir::get()[newId]);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadTargetDir(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_TARGET_DIR - 1;
	dcassert(newId >= 0);

	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;

	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;
	bool getConnFlag = true;
	try
	{
		dcassert(newId < (int)LastDir::get().size());
		dl->download(dir, Text::fromT(LastDir::get()[newId]),
			WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadFavoriteDirs(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_FAVORITE_DIRS;
	dcassert(newId >= 0);
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& spl = lockedInstance.getFavoriteDirsL();
	if (ctrlList.GetSelectedCount() == 1)
	{
		const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		bool getConnFlag = true;
		if (ii->type == ItemInfo::FILE)
		{
			if (newId < (int)targets.size())
			{
				try
				{
					dl->download(ii->file, targets[newId], false,
						WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, false, getConnFlag);
				}
				catch (const Exception& e)
				{
					ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
				}
			}
			else
			{
				newId -= (int)targets.size();
				downloadList(spl, newId);
			}
		}
		else
		{
			dcassert(newId < (int)spl.size());
			downloadList(spl, newId);
		}
	}
	else if (ctrlList.GetSelectedCount() > 1)
	{
		downloadList(spl, newId);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadWholeFavoriteDirs(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS;
	dcassert(newId >= 0);

	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (!t) return 0;

	DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(ctrlTree.GetItemData(t));
	if (!dir) return 0;
	bool getConnFlag = true;
	try
	{
		FavoriteManager::LockInstanceDirs lockedInstance;
		const auto& spl = lockedInstance.getFavoriteDirsL();
		dcassert(newId < (int)spl.size());
		dl->download(dir, spl[newId].dir,
			WinUtil::isShift() ? QueueItem::HIGHEST : QueueItem::DEFAULT, getConnFlag);
	}
	catch (const Exception& e)
	{
		ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
	}
	return 0;
}

LRESULT DirectoryListingFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	if (kd->wVKey == VK_BACK)
	{
		up();
	}
	else if (kd->wVKey == VK_TAB)
	{
		onTab();
	}
	else if (kd->wVKey == VK_LEFT && WinUtil::isAlt())
	{
		back();
	}
	else if (kd->wVKey == VK_RIGHT && WinUtil::isAlt())
	{
		forward();
	}
	else if (kd->wVKey == VK_RETURN)
	{
		if (ctrlList.GetSelectedCount() == 1)
		{
			const ItemInfo* ii = ctrlList.getItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
			if (ii->type == ItemInfo::DIRECTORY)
			{
				HTREEITEM ht = ctrlTree.GetChildItem(ctrlTree.GetSelectedItem());
				while (ht != NULL)
				{
					if ((DirectoryListing::Directory*)ctrlTree.GetItemData(ht) == ii->dir)
					{
						ctrlTree.SelectItem(ht);
						break;
					}
					ht = ctrlTree.GetNextSiblingItem(ht);
				}
			}
			else
			{
				downloadList(Text::toT(SETTING(DOWNLOAD_DIRECTORY)));
			}
		}
		else
		{
			downloadList(Text::toT(SETTING(DOWNLOAD_DIRECTORY)));
		}
	}
	return 0;
}

void DirectoryListingFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);

	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[STATUS_LAST] = {0};
		ctrlStatus.GetClientRect(sr);
		w[STATUS_DUMMY - 1] = sr.right - 16;
		for (int i = STATUS_DUMMY - 2; i >= 0; --i)
		{
			w[i] = max(w[i + 1] - statusSizes[i + 1], 0);
		}

		ctrlStatus.SetParts(STATUS_LAST, w);
		ctrlStatus.GetRect(0, sr);

		sr.left = w[STATUS_FILE_LIST_DIFF - 1];
		sr.right = w[STATUS_FILE_LIST_DIFF];
		ctrlListDiff.MoveWindow(sr);

		sr.left = w[STATUS_MATCH_QUEUE - 1];
		sr.right = w[STATUS_MATCH_QUEUE];
		ctrlMatchQueue.MoveWindow(sr);

		sr.left = w[STATUS_FIND - 1];
		sr.right = w[STATUS_FIND];
		ctrlFind.MoveWindow(sr);

		sr.left = w[STATUS_PREV - 1];
		sr.right = w[STATUS_PREV];
		ctrlFindPrev.MoveWindow(sr);

		sr.left = w[STATUS_NEXT - 1];
		sr.right = w[STATUS_NEXT];
		ctrlFindNext.MoveWindow(sr);
	}

	SetSplitterRect(&rect);
}

void DirectoryListingFrame::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;

	StringMap ucParams = ucLineParams;

	boost::unordered_set<UserPtr, User::Hash> nicks;

	int sel = -1;
	while ((sel = ctrlList.GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(sel);
		if (uc.getType() == UserCommand::TYPE_RAW_ONCE)
		{
			if (nicks.find(dl->getUser()) != nicks.end())
				continue;
			nicks.insert(dl->getUser());
		}
		if (!dl->getUser()->isOnline())
			return;
		ucParams["fileTR"] = "NONE";
		if (ii->type == ItemInfo::FILE)
		{
			ucParams["type"] = "File";
			ucParams["fileFN"] = dl->getPath(ii->file) + ii->file->getName();
			ucParams["fileSI"] = Util::toString(ii->file->getSize());
			ucParams["fileSIshort"] = Util::formatBytes(ii->file->getSize());
			ucParams["fileTR"] = ii->file->getTTH().toBase32();
		}
		else
		{
			ucParams["type"] = "Directory";
			ucParams["fileFN"] = dl->getPath(ii->dir) + ii->dir->getName();
			ucParams["fileSI"] = Util::toString(ii->dir->getTotalSize());
			ucParams["fileSIshort"] = Util::formatBytes(ii->dir->getTotalSize());
		}

		// compatibility with 0.674 and earlier
		ucParams["file"] = ucParams["fileFN"];
		ucParams["filesize"] = ucParams["fileSI"];
		ucParams["filesizeshort"] = ucParams["fileSIshort"];
		ucParams["tth"] = ucParams["fileTR"];

		StringMap tmp = ucParams;
		const UserPtr tmpPtr = dl->getUser();
		ClientManager::userCommand(dl->getHintedUser(), uc, tmp, true);
	}
}

void DirectoryListingFrame::closeAll()
{
	for (auto i = activeFrames.cbegin(); i != activeFrames.cend(); ++i)
		i->second->PostMessage(WM_CLOSE, 0, 0);
}

LRESULT DirectoryListingFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string data;
	// !SMT!-UI: copy several rows
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlList.getItemData(i);
		string sCopy;
		switch (wID)
		{
			case IDC_COPY_NICK:
				sCopy = dl->getUser()->getLastNick();
				break;
			case IDC_COPY_FILENAME:
				sCopy = Util::getFileName(ii->type == ItemInfo::FILE ? ii->file->getName() : ii->dir->getName()); // !SMT!-F
				break;
			case IDC_COPY_SIZE:
				sCopy = Util::formatBytes(ii->type == ItemInfo::FILE ? ii->file->getSize() : ii->dir->getTotalSize()); // !SMT!-F
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
		SettingsManager::getInstance()->removeListener(this);
		activeFrames.erase(m_hWnd);
		ctrlList.deleteAll();
		ctrlList.saveHeaderOrder(SettingsManager::DIRLIST_FRAME_ORDER, SettingsManager::DIRLIST_FRAME_WIDTHS, SettingsManager::DIRLIST_FRAME_VISIBLE);
		SET_SETTING(DIRLIST_FRAME_SORT, ctrlList.getSortForSettings());
		SET_SETTING(DIRLIST_FRAME_SPLIT, m_nProportionalPos);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		bHandled = FALSE;
		return 0;
	}
}

LRESULT DirectoryListingFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click

	if (const UserPtr& user = dl->getUser())
	{
		OMenu tabMenu;
		tabMenu.CreatePopupMenu();

		string nick = user->getLastNick();
		tabMenu.InsertSeparatorFirst(Text::toT(nick));

#ifdef SCALOLAZ_DIRLIST_ADDFAVUSER
		tabMenu.AppendMenu(MF_STRING, IDC_ADD_TO_FAVORITES, CTSTRING(ADD_TO_FAVORITES));
#endif
		tabMenu.AppendMenu(MF_SEPARATOR);

		tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_DIR_LIST));
		tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));

		tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);

		WinUtil::unlinkStaticMenus(tabMenu); // TODO - fix copy-paste
	}
	return TRUE;
}

LRESULT DirectoryListingFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = frameIcon;
	opt->icons[1] = frameIconOffline;
	opt->isHub = false;
	return TRUE;
}

void DirectoryListingFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlList.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

LRESULT DirectoryListingFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	switch (wParam)
	{
		case FINISHED:
			loading = false;
			if (!isClosedOrShutdown())
			{
				initStatus();
				ctrlStatus.SetText(0, (TSTRING(PROCESSED_FILE_LIST) + _T(' ') + Util::toStringT((GET_TICK() - loadStartTime) / 1000) + TSTRING(S)).c_str());
				enableControls();
				//notify the user that we've loaded the list
				setDirty();
			}
			else
			{
				PostMessage(WM_CLOSE, 0, 0);
			}
			break;
		case ABORTED:
		{
			loading = false;
			PostMessage(WM_CLOSE, 0, 0);
			break;
		}
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

void DirectoryListingFrame::getDirItemColor(const Flags::MaskType flags, COLORREF &fg, COLORREF &bg)
{
	fg = RGB(0,0,0);
	bg = RGB(255,255,255);
	if (flags & DirectoryListing::FLAG_FOUND)
		bg = colorFound; else
	if (flags & DirectoryListing::FLAG_HAS_FOUND)
		bg = colorFoundLighter; else
	if (flags & DirectoryListing::FLAG_HAS_SHARED)
	{
		if (flags & (DirectoryListing::FLAG_HAS_DOWNLOADED | DirectoryListing::FLAG_HAS_CANCELED | DirectoryListing::FLAG_HAS_OTHER))
			bg = colorSharedLighter;
		else
			bg = colorShared;
	} else
	if (flags & DirectoryListing::FLAG_HAS_DOWNLOADED)
	{
		if (flags & (DirectoryListing::FLAG_HAS_CANCELED | DirectoryListing::FLAG_HAS_OTHER))
			bg = colorDownloadedLighter;
		else
			bg = colorDownloaded;
	} else
	if (flags & DirectoryListing::FLAG_HAS_CANCELED)
	{
		if (flags & DirectoryListing::FLAG_HAS_OTHER)
			bg = colorCanceledLighter;
		else
			bg = colorCanceled;
	}
	if (flags & DirectoryListing::FLAG_HAS_QUEUED)
		fg = colorInQueue;
}

void DirectoryListingFrame::getFileItemColor(const Flags::MaskType flags, COLORREF &fg, COLORREF &bg)
{
	fg = RGB(0,0,0);
	bg = RGB(255,255,255);
	if (flags & DirectoryListing::FLAG_FOUND)
		bg = colorFound; else
	if (flags & DirectoryListing::FLAG_HAS_FOUND)
		bg = colorFoundLighter; else
	if (flags & DirectoryListing::FLAG_SHARED)
		bg = colorShared; else
	if (flags & DirectoryListing::FLAG_DOWNLOADED)
		bg = colorDownloaded; else
	if (flags & DirectoryListing::FLAG_CANCELED)
		bg = colorCanceled;
	if (flags & DirectoryListing::FLAG_QUEUED)
		fg = colorInQueue;
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
				Flags::MaskType flags = ii->file->getFlags();
				getFileItemColor(flags, plvcd->clrText, plvcd->clrTextBk);
			}
			else if (ii->type == ItemInfo::DIRECTORY)
			{
				Flags::MaskType flags = ii->dir->getFlags();
				getDirItemColor(flags, plvcd->clrText, plvcd->clrTextBk);
			}
			CustomDrawHelpers::startItemDraw(customDrawState, plvcd);
			if (hTheme) CustomDrawHelpers::drawBackground(hTheme, customDrawState, plvcd);
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
				const DirectoryListing::MediaInfo* media = ii->file->getMedia();
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
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
			if ((plvcd->nmcd.uItemState & CDIS_SELECTED) == 0)
			{
				DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(plvcd->nmcd.lItemlParam);
				if (dir)
					getDirItemColor(dir->getFlags(), plvcd->clrText, plvcd->clrTextBk);
			}
	}
	return CDRF_DODEFAULT;
}

static void translateDuration(const string& src, tstring& columnAudio, tstring& columnDuration)
{
	columnDuration.clear();
	columnAudio.clear();
	if (!src.empty())
	{
		const size_t pos = src.find('|', 0);
		if (pos != string::npos && pos)
		{
			if (src.size() > 6 && src[0] >= '1' && src[0] <= '9' && // Проверим факт наличия в начале длительности
				(src[1] == 'm' || src[2] == 'n') || // "1mn XXs"
				(src[1] == 's' || src[2] == ' ') || // "1s XXXms"
				(src[2] == 's' || src[3] == ' ') || // "59s XXXms"
				(src[2] == 'm' || src[3] == 'n') ||   // "59mn XXs"
				(src[1] == 'h') ||  // "1h XXmn"
				(src[2] == 'h')     // "60h XXmn"
				)
			{
				Text::toT(src.substr(0, pos - 1), columnDuration);
				if (pos + 2 < src.length())
				Text::toT(src.substr(pos + 2), columnAudio);
			}
			else
			{
				columnAudio = Text::toT(src); // Если не распарсили - показывает что есть
				//dcassert(0); // fix "1 076 Kbps,23mn 9s | MPEG Audio, 160 Kbps, 2 channels"
			}
		}
	}
}

DirectoryListingFrame::ItemInfo::ItemInfo(DirectoryListing::File* f, const DirectoryListing* dl) :
	type(FILE), file(f), iconIndex(-1)
{
	columns[COLUMN_FILENAME] = Text::toT(f->getName());
	columns[COLUMN_TYPE] = Util::getFileExt(columns[COLUMN_FILENAME]);
	if (!columns[COLUMN_TYPE].empty() && columns[COLUMN_TYPE][0] == '.')
		columns[COLUMN_TYPE].erase(0, 1);
	columns[COLUMN_EXACT_SIZE] = Util::formatExactSize(f->getSize());
	columns[COLUMN_SIZE] =  Util::formatBytesT(f->getSize());
	columns[COLUMN_TTH] = Text::toT(f->getTTH().toBase32());
	if (dl->isOwnList())
	{
		string s;
		ShareManager::getInstance()->getFilePath(f->getTTH(), s);
		columns[COLUMN_PATH] = Text::toT(s);
	}
	else
	{
		columns[COLUMN_PATH] = Text::toT(f->getPath());
		Util::uriSeparatorsToPathSeparators(columns[COLUMN_PATH]);
	}

	if (f->getHit())
		columns[COLUMN_HIT] = Util::toStringT(f->getHit());
	if (f->getTS())
		columns[COLUMN_TS] = Text::toT(Util::formatDateTime(static_cast<time_t>(f->getTS())));
	const DirectoryListing::MediaInfo *media = f->getMedia();
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
		translateDuration(media->audio, columns[COLUMN_MEDIA_AUDIO], columns[COLUMN_DURATION]);
	}
}

DirectoryListingFrame::ItemInfo::ItemInfo(DirectoryListing::Directory* d) : type(DIRECTORY), dir(d), iconIndex(-1)
{
	columns[COLUMN_FILENAME] = Text::toT(d->getName());
	columns[COLUMN_EXACT_SIZE] = Util::formatExactSize(d->getTotalSize());
	columns[COLUMN_SIZE] = Util::formatBytesT(d->getTotalSize());
	auto hits = d->getTotalHits();
	if (hits) columns[COLUMN_HIT] = Util::toStringT(hits);
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

int DirectoryListingFrame::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, int col)
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
				case COLUMN_HIT:
					return compare(a->dir->getTotalHits(), b->dir->getTotalHits());
				case COLUMN_TS:
					return compare(a->dir->getMaxTS(), b->dir->getMaxTS());
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

LRESULT DirectoryListingFrame::onGenerateDCLST(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
		// Получить выбранный каталог в XML
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

// [+] SSA open file with dclst support
void DirectoryListingFrame::openFileFromList(const tstring& file)
{
	if (file.empty())
		return;

	if (Util::isDclstFile(file))
		DirectoryListingFrame::openWindow(file, Util::emptyStringT, HintedUser(), 0, true);
	else
		WinUtil::openFile(file);

}

void DirectoryListingFrame::showFound()
{
	if (search.getWhatFound() == DirectoryListing::SearchContext::FOUND_NOTHING) return;
	const DirectoryListing::Directory *dir = search.getDirectory();
	const DirectoryListing::File *file = search.getFile();
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
	const DirectoryListing::Directory *dir = search.getDirectory();
	const DirectoryListing::File *file = search.getFile();
	const vector<int> &v = search.getDirIndex();
	tstring str;
	for (int i=0; i < (int) v.size(); i++)
	{
		if (!str.empty()) str += _T(' ');
		str += Util::toStringT(v[i]);
	}
	if (search.getWhatFound() == DirectoryListing::SearchContext::FOUND_DIR)
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

void DirectoryListingFrame::updateSearchButtons()
{
	BOOL enable = search.getWhatFound() == DirectoryListing::SearchContext::FOUND_NOTHING ? FALSE : TRUE;
	ctrlFindPrev.EnableWindow(enable);
	ctrlFindNext.EnableWindow(enable);
}

LRESULT DirectoryListingFrame::onFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	g_searchOptions.enableSharedDays = dl->hasTimestamps();
	SearchDlg dlg(g_searchOptions);
	if (dlg.DoModal() != IDOK) return 0;

	bool hasMatches = search.getWhatFound() != DirectoryListing::SearchContext::FOUND_NOTHING;

	if (dlg.clearResults())
	{
		if (hasMatches)
		{
			search.clear();
			dl->getRoot()->clearMatches();
			updateSearchButtons();
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		return 0;
	}

	const string &findStr = g_searchOptions.text;
	DirectoryListing::SearchQuery sq;
	if (!findStr.empty() && g_searchOptions.fileType != FILE_TYPE_TTH)
	{
		if (g_searchOptions.regExp)
		{
			int reFlags = 0;
			if (!g_searchOptions.matchCase) reFlags |= std::regex_constants::icase;
			try
			{
				sq.re.assign(findStr, (std::regex::flag_type) reFlags);
				sq.flags |= DirectoryListing::SearchQuery::FLAG_REGEX;
			}
			catch (std::regex_error&) {}
		} else
		if (g_searchOptions.matchCase)
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

	if (g_searchOptions.fileType != FILE_TYPE_ANY)
	{
		if (g_searchOptions.fileType == FILE_TYPE_TTH)
			Encoder::fromBase32(findStr.c_str(), sq.tth.data, TTHValue::BYTES);
		sq.type = g_searchOptions.fileType;
		sq.flags |= DirectoryListing::SearchQuery::FLAG_TYPE;
	}

	if (g_searchOptions.sizeMin >= 0 || g_searchOptions.sizeMax >= 0)
	{
		int sizeUnitShift = 0;
		if (g_searchOptions.sizeUnit > 0 && g_searchOptions.sizeUnit <= 3)
			sizeUnitShift = 10*g_searchOptions.sizeUnit;
		sq.minSize = g_searchOptions.sizeMin;
		if (sq.minSize >= 0)
			sq.minSize <<= sizeUnitShift;
		else
			sq.minSize = 0;
		sq.maxSize = g_searchOptions.sizeMax;
		if (sq.maxSize >= 0)
			sq.maxSize <<= sizeUnitShift;
		else
			sq.maxSize = std::numeric_limits<int64_t>::max();
		sq.flags |= DirectoryListing::SearchQuery::FLAG_SIZE;
	}

	if (g_searchOptions.sharedDays > 0)
	{
		sq.minSharedTime = GET_TIME();
		sq.minSharedTime -= g_searchOptions.sharedDays*(60*60*24);
		sq.flags |= DirectoryListing::SearchQuery::FLAG_TIME_SHARED;
	}

	if (g_searchOptions.onlyNewFiles)
		sq.flags |= DirectoryListing::SearchQuery::FLAG_ONLY_NEW_FILES;

	DirectoryListing *dest = g_searchOptions.newWindow ? new DirectoryListing(abortFlag) : nullptr;

	if (!search.match(sq, dl->getRoot(), dest))
	{
		delete dest;
		updateSearchButtons();
		if (hasMatches) RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		MessageBox(CTSTRING(NO_MATCHES), CTSTRING(SEARCH));
		return 0;
	}

	updateSearchButtons();
	RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	showFound();

	if (dest)
		openWindow(dest, dl->getHintedUser(), speed, true);

	return 0;
}

LRESULT DirectoryListingFrame::onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (search.next()) showFound();
	return 0;
}

LRESULT DirectoryListingFrame::onPrev(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (search.prev()) showFound();
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
			setWindowTitle();
	}
	return 0;
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
				const string filename = Util::getFileName(filePath);
				bool isList = (_strnicmp(filename.c_str(), "files", 5)
					|| _strnicmp(filename.c_str() + filename.length() - 8, ".xml.bz2", 8));
				const UserPtr& user = window->dl->getUser();
				bool isOwnList = user->isMe();
				window->setWindowTitle();
				window->dl->loadFile(filePath, this, isOwnList);
				window->addToUserList(user, false);
				window->setWindowTitle();
				ADLSearchManager::getInstance()->matchListing(*window->dl);
				if (isList)
					window->dl->checkDupes();
				window->refreshTree(window->dl->getRoot(), window->treeRoot, Util::toAdcFile(Text::fromT(directory)));
				break;
			}
			case MODE_SUBTRACT_FILE:
			{
				dcassert(!filePath.empty());
				DirectoryListing newListing(window->abortFlag, false);
				newListing.loadFile(filePath, this, window->dl->isOwnList());
				if (!newListing.isAborted())
				{
					window->dl->getRoot()->filterList(*newListing.getTTHSet());
					window->filteredListFlag = true;
					window->refreshTree(window->dl->getRoot(), window->treeRoot);
					window->updateRootItemText();
				}
				break;
			}
			case MODE_LOAD_PARTIAL_LIST:
			{
				DirectoryListing newListing(window->abortFlag);
				newListing.setHintedUser(window->dl->getHintedUser());
				newListing.loadXML(text, this, false);
				DirectoryListing::Directory *subdir = window->dl->findDirPath(newListing.getBasePath());
				if (subdir)
				{
					HTREEITEM ht;
					if (subdir == window->dl->getRoot())
						ht = window->treeRoot;
					else
						ht = static_cast<HTREEITEM>(subdir->getUserData());
					DirectoryListing::Directory *newRoot = newListing.getRoot();
					if (window->dl->spliceTree(subdir, newListing))
					{
						window->refreshTree(newRoot, ht);
						newRoot->setUserData(static_cast<void*>(ht));
						window->showDirContents(newRoot, nullptr, nullptr);
					}
				}
				break;
			}
			default:
				dcassert(0);
		}
		window->PostMessage(WM_SPEAKER, DirectoryListingFrame::FINISHED);
	}
	catch (const AbortException&)
	{
		window->PostMessage(WM_SPEAKER, DirectoryListingFrame::ABORTED);
	}
	catch (const Exception& e)
	{
		tstring error = TSTRING(ERROR_PARSING_FILE_LIST);
		error += Text::toT(e.getError());
		::MessageBox(NULL, error.c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR);
		window->PostMessage(WM_SPEAKER, DirectoryListingFrame::ABORTED);
	}

	//cleanup the thread object
	delete this;

	return 0;
}

void ThreadedDirectoryListing::notify(int progress)
{
	window->PostMessage(WM_SPEAKER, DirectoryListingFrame::PROGRESS | progress);
}
