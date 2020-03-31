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
#include "QueueFrame.h"
#include "SearchFrm.h"
#include "PrivateFrame.h"
#include "LineDlg.h"
#include "../client/ShareManager.h"
#include "../client/ClientManager.h"
#include "../client/DownloadManager.h"
#include "../client/CFlylinkDBManager.h"
#include "BarShader.h"
#include "MainFrm.h"
#include "ExMessageBox.h"

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/hex.hpp"
#endif

static const unsigned TIMER_VAL = 1000;

HIconWrapper QueueFrame::frameIcon(IDR_QUEUE);

int QueueFrame::columnIndexes[] =
{
	COLUMN_TARGET,
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
	COLUMN_SPEED
};

int QueueFrame::columnSizes[] = { 200, 20, 300, 70, 75, 100, 120, 75, 200, 200, 200, 75, 200, 100, 125, 50 };

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::FILENAME,
	ResourceManager::TYPE,
	ResourceManager::STATUS,
	ResourceManager::SEGMENTS,
	ResourceManager::SIZE,
	ResourceManager::DOWNLOADED_PARTS,
	ResourceManager::DOWNLOADED,
	ResourceManager::PRIORITY,
	ResourceManager::USERS,
	ResourceManager::PATH,
	ResourceManager::LOCAL_PATH,
	ResourceManager::EXACT_SIZE,
	ResourceManager::ERRORS,
	ResourceManager::ADDED,
	ResourceManager::TTH_ROOT,
	ResourceManager::SPEED
};

QueueFrame::QueueFrame() :
	timer(m_hWnd),
	menuItems(0), queueSize(0), queueItems(0), queueChanged(false),
	usingDirMenu(false), readdItems(0), fileLists(nullptr), showTree(true),
	showTreeContainer(WC_BUTTON, this, SHOWTREE_MESSAGE_MAP),
	lastCount(0), lastTotal(0), updateStatus(false)
{
	memset(statusSizes, 0, sizeof(statusSizes));
}

QueueFrame::~QueueFrame()
{
	// Clear up dynamicly allocated menu objects
	browseMenu.ClearMenu();
	removeMenu.ClearMenu();
	removeAllMenu.ClearMenu();
	pmMenu.ClearMenu();
	readdMenu.ClearMenu();
}

static tstring getLastNickHubT(const UserPtr& user)
{
	string nick = user->getLastNick();
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	auto hubID = user->getHubID();
	if (hubID)
	{
		const string hubName = CFlylinkDBManager::getInstance()->get_hub_name(hubID);
		if (!hubName.empty())
		{
			nick += " (";
			nick += hubName;
			nick += ')';
		}
	}
#endif
	return Text::toT(nick);
}

LRESULT QueueFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	showTree = BOOLSETTING(QUEUE_FRAME_SHOW_TREE);
	
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlQueue.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_QUEUE);
	setListViewExtStyle(ctrlQueue, BOOLSETTING(SHOW_GRIDLINES), false);
	
	ctrlDirs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	                TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP,
	                WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	                
	WinUtil::SetWindowThemeExplorer(ctrlDirs.m_hWnd);
	
	ctrlDirs.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlQueue.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	
	m_nProportionalPos = SETTING(QUEUE_FRAME_SPLIT);
	SetSplitterPanes(ctrlDirs.m_hWnd, ctrlQueue.m_hWnd);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(QUEUE_FRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(QUEUE_FRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (uint8_t j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_SIZE || j == COLUMN_DOWNLOADED || j == COLUMN_EXACT_SIZE || j == COLUMN_SEGMENTS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlQueue.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlQueue.setColumnOrderArray(COLUMN_LAST, columnIndexes);
	ctrlQueue.setVisible(SETTING(QUEUE_FRAME_VISIBLE));
	
	ctrlQueue.setSortFromSettings(SETTING(QUEUE_FRAME_SORT));
	
	setListViewColors(ctrlQueue);
	ctrlQueue.setFlickerFree(Colors::g_bgBrush);
	
	ctrlDirs.SetBkColor(Colors::g_bgColor);
	ctrlDirs.SetTextColor(Colors::g_textColor);
	
	ctrlShowTree.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlShowTree.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlShowTree.SetCheck(showTree);
	ctrlShowTree.SetFont(Fonts::g_systemFont);
	showTreeContainer.SubclassWindow(ctrlShowTree.m_hWnd);
	
	browseMenu.CreatePopupMenu();
	removeMenu.CreatePopupMenu();
	removeAllMenu.CreatePopupMenu();
	pmMenu.CreatePopupMenu();
	readdMenu.CreatePopupMenu();
	
	removeMenu.AppendMenu(MF_STRING, IDC_REMOVE_SOURCE, CTSTRING(ALL));
	removeMenu.AppendMenu(MF_SEPARATOR);
	
	readdMenu.AppendMenu(MF_STRING, IDC_READD, CTSTRING(ALL));
	readdMenu.AppendMenu(MF_SEPARATOR);
	
	addQueueList();
	QueueManager::getInstance()->addListener(this);
	DownloadManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	
	memset(statusSizes, 0, sizeof(statusSizes));
	statusSizes[0] = 16;
	ctrlStatus.SetParts(6, statusSizes);
	updateStatus = true;
	timer.createTimer(TIMER_VAL);
	bHandled = FALSE;
	return 1;
}

void QueueFrame::QueueItemInfo::removeTarget(bool batchMode)
{
	QueueManager::getInstance()->removeTarget(getTarget(), batchMode);
}

int QueueFrame::QueueItemInfo::compareItems(const QueueItemInfo* a, const QueueItemInfo* b, int col)
{
	switch (col)
	{
		case COLUMN_SIZE:
		case COLUMN_EXACT_SIZE:
			return compare(a->getSize(), b->getSize());
		case COLUMN_PRIORITY:
			return compare((int)a->getPriority(), (int)b->getPriority());
		case COLUMN_DOWNLOADED:
			return compare(a->getDownloadedBytes(), b->getDownloadedBytes());
		case COLUMN_ADDED:
			return compare(a->getAdded(), b->getAdded());
		default:
			return lstrcmpi(a->getText(col).c_str(), b->getText(col).c_str());
	}
}

const tstring QueueFrame::QueueItemInfo::getText(int col) const
{
	switch (col)
	{
		case COLUMN_TARGET:
			return Text::toT(Util::getFileName(getTarget()));
		case COLUMN_TYPE:
			return Text::toT(Util::getFileExtWithoutDot(getTarget()));
		case COLUMN_STATUS:
		{
			if (isFinished())
			{
				return TSTRING(DOWNLOAD_FINISHED_IDLE);
			}
#ifdef FLYLINKDC_USE_TORRENT
			if (!isTorrent())
#endif
			{
				const size_t onlineSources = qi->getLastOnlineCount();
				const size_t totalSources = qi->getSourcesCount();
				if (isWaiting())
				{
					if (onlineSources)
					{
						if (totalSources == 1)
							return TSTRING(WAITING_USER_ONLINE);
						return TSTRING_F(WAITING_USERS_ONLINE_FMT, onlineSources % totalSources);
					}
					else
					{
						switch (totalSources)
						{
							case 0:
								return TSTRING(NO_USERS_TO_DOWNLOAD_FROM);
							case 1:
								return TSTRING(USER_OFFLINE);
							case 2:
								return TSTRING(BOTH_USERS_OFFLINE);
							case 3:
								return TSTRING(ALL_3_USERS_OFFLINE);
							case 4:
								return TSTRING(ALL_4_USERS_OFFLINE);
							default:
								return TSTRING_F(ALL_USERS_OFFLINE_FMT, totalSources);
						}
					}
				}
				else
				{
					if (totalSources == 1)
						return TSTRING(USER_ONLINE);
					return TSTRING_F(USERS_ONLINE_FMT, onlineSources % totalSources);
				}
			}
		}
		case COLUMN_SEGMENTS:
		{
#ifdef FLYLINKDC_USE_TORRENT
			if (isTorrent())
				return _T("TODO - Segmets");
#endif
			const QueueItemPtr qi = getQueueItem();
			return Util::toStringT(qi->getDownloadsSegmentCount()) + _T('/') + Util::toStringT(qi->getMaxSegments());
		}
		case COLUMN_SIZE:
			return getSize() == -1 ? TSTRING(UNKNOWN) : Util::formatBytesT(getSize());
		case COLUMN_DOWNLOADED:
		{
			auto size = getSize();
			if (size > 0)
			{
				auto downloadedSize = getDownloadedBytes();
				return Util::formatBytesT(downloadedSize) + _T(" (") + Util::toStringT((double) downloadedSize * 100.0 / (double) size) + _T("%)");
			}
			return Util::emptyStringT;
		}
		case COLUMN_PRIORITY:
		{
			tstring priority;
			switch (getPriority())
			{
				case QueueItem::PAUSED:
					priority = TSTRING(PAUSED);
					break;
				case QueueItem::LOWEST:
					priority = TSTRING(LOWEST);
					break;
				case QueueItem::LOWER:
					priority = TSTRING(LOWER);
					break;
				case QueueItem::LOW:
					priority = TSTRING(LOW);
					break;
				case QueueItem::NORMAL:
					priority = TSTRING(NORMAL);
					break;
				case QueueItem::HIGH:
					priority = TSTRING(HIGH);
					break;
				case QueueItem::HIGHER:
					priority = TSTRING(HIGHER);
					break;
				case QueueItem::HIGHEST:
					priority = TSTRING(HIGHEST);
					break;
				default:
					dcassert(0);
					break;
			}
			if (getAutoPriority())
			{
				priority += _T(" (") + TSTRING(AUTO) + _T(')');
			}
			return priority;
		}
		case COLUMN_USERS:
		{
			tstring tmp;
#ifdef FLYLINKDC_USE_TORRENT
			if (isTorrent()) return tmp;
#endif
			RLock(*QueueItem::g_cs);
			const auto& sources = qi->getSourcesL();
			for (auto j = sources.cbegin(); j != sources.cend(); ++j)
			{
				if (!tmp.empty())
					tmp += _T(", ");
				tmp += getLastNickHubT(j->first);
			}
			return tmp.empty() ? TSTRING(NO_USERS) : tmp;
		}
		case COLUMN_PATH:
		{
			return Text::toT(Util::getFilePath(getTarget()));
		}
		case COLUMN_LOCAL_PATH:
		{
#ifdef FLYLINKDC_USE_TORRENT
			if (isTorrent())
				return Text::toT(m_save_path);
#endif
			if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_GET_IP))
			{
				string path;
				if (ShareManager::getInstance()->getFilePath(qi->getTTH(), path))
					return Text::toT(path);
			}
			return tstring();
		}
		case COLUMN_EXACT_SIZE:
		{
			return getSize() == -1 ? TSTRING(UNKNOWN) : Util::formatExactSize(getSize());
		}
		case COLUMN_SPEED:
		{
#ifdef FLYLINKDC_USE_TORRENT
			if (isTorrent())
				return _T("TODO Speed"); // TODO
#endif
			return Util::formatBytesT(qi->getAverageSpeed()) + _T('/') + TSTRING(S);
		}
		case COLUMN_ERRORS:
		{
			tstring tmp;
#ifdef FLYLINKDC_USE_TORRENT
			if (!isTorrent())
#endif
			{
				RLock(*QueueItem::g_cs);
				const auto& badSources = qi->getBadSourcesL();
				for (auto j = badSources.cbegin(); j != badSources.cend(); ++j)
				{
					if (!j->second.isSet(QueueItem::Source::FLAG_REMOVED))
					{
						if (!tmp.empty())
							tmp += _T(", ");
						tmp += getLastNickHubT(j->first);
						
						tmp += _T(" (");
						if (j->second.isSet(QueueItem::Source::FLAG_FILE_NOT_AVAILABLE))
						{
							tmp += TSTRING(FILE_NOT_AVAILABLE);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_PASSIVE))
						{
							tmp += TSTRING(PASSIVE_USER);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_BAD_TREE))
						{
							tmp += TSTRING(INVALID_TREE);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_SLOW_SOURCE))
						{
							tmp += TSTRING(SLOW_USER);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_NO_TTHF))
						{
							tmp += TSTRING(SOURCE_TOO_OLD);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_NO_NEED_PARTS))
						{
							tmp += TSTRING(NO_NEEDED_PART);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_UNTRUSTED))
						{
							tmp += TSTRING(CERTIFICATE_NOT_TRUSTED);
						}
						tmp += _T(')');
					}
				}
			}
			return tmp.empty() ? TSTRING(NO_ERRORS) : tmp;
		}
		case COLUMN_ADDED:
		{
			return Text::toT(Util::formatDigitalClock(getAdded()));
		}
		case COLUMN_TTH:
		{
#ifdef FLYLINKDC_USE_TORRENT
			if (isTorrent())
				return Text::toT(libtorrent::aux::to_hex(sha1));
#endif
			return qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_GET_IP) ? Util::emptyStringT : Text::toT(getTTH().toBase32());
		}
		default:
			return Util::emptyStringT;
	}
}

#ifdef FLYLINKDC_USE_TORRENT
void QueueFrame::on(DownloadManagerListener::RemoveTorrent, const libtorrent::sha1_hash& sha1) noexcept
{
#ifdef _DEBUG // ????
	addTask(REMOVE_ITEM, new StringTask(DownloadManager::getInstance()->get_torrent_name(sha1)));
#endif
}

void QueueFrame::on(DownloadManagerListener::CompleteTorrentFile, const std::string& fileName) noexcept
{
}

void QueueFrame::on(DownloadManagerListener::AddedTorrent, const libtorrent::sha1_hash& sha1, const std::string& savePath) noexcept
{
#ifdef _DEBUG
	addTask(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(sha1, savePath)));
#endif
}

void QueueFrame::on(DownloadManagerListener::TorrentEvent, const DownloadArray& torrentEvents) noexcept
{
#ifdef _DEBUG
	for (auto j = torrentEvents.cbegin(); j != torrentEvents.cend(); ++j)
		addTask(UPDATE_ITEM, new UpdateTask(j->path, j->sha1));
#endif
}
#endif

void QueueFrame::on(QueueManagerListener::AddedArray, const std::vector<QueueItemPtr>& qiArray) noexcept
{
	for (auto i = qiArray.cbegin(); i != qiArray.cend(); ++i)
		addTask(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(*i))); // FIXME FIXME FIXME
}

void QueueFrame::on(QueueManagerListener::Added, const QueueItemPtr& qi) noexcept
{
	addTask(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(qi))); // FIXME FIXME FIXME
}

void QueueFrame::addQueueItem(QueueItemInfo* ii, bool noSort)
{
	dcassert(!closed);
	if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		dcassert(ii->getSize() >= 0);
		queueSize += ii->getSize();
	}
	queueItems++;
	queueChanged = true;
	
	const string dir = Util::getFilePath(ii->getTarget());
	
	const auto i = directories.find(dir);
	const bool updateDir = (i == directories.end());
	directories.insert(make_pair(dir, ii)); // TODO directories.insert(i,DirectoryMapPair(dir, ii));
	if (updateDir)
	{
		addDirectory(dir, ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST));
	}
	if (!showTree || isCurDir(dir))
	{
		if (noSort)
		{
			ctrlQueue.insertItem(ctrlQueue.GetItemCount(), ii, I_IMAGECALLBACK);
		}
		else
		{
			ctrlQueue.insertItem(ii, I_IMAGECALLBACK);
		}
	}
}

QueueFrame::QueueItemInfo* QueueFrame::getItemInfo(const string& target, const string& path) const
{
	auto i = directories.equal_range(path);
	for (auto j = i.first; j != i.second; ++j)
	{
		if (j->second->getTarget() == target)
			return j->second;
	}
	return nullptr;
}

QueueFrame::QueueItemInfo* QueueFrame::removeItemInfo(const string& target, const string& path)
{
	auto i = directories.equal_range(path);
	for (auto j = i.first; j != i.second; ++j)
	{
		if (j->second->getTarget() == target)
		{
			QueueItemInfo* result = j->second;
			directories.erase(j);
			return result;
		}
	}
	return nullptr;
}

QueueFrame::QueueItemInfo* QueueFrame::removeItemInfo(const QueueItemPtr& qi, const string& path)
{
	auto i = directories.equal_range(path);
	for (auto j = i.first; j != i.second; ++j)
	{
		if (j->second->getQueueItem() == qi)
		{
			QueueItemInfo* result = j->second;
			directories.erase(j);
			return result;
		}
	}
	return nullptr;
}

void QueueFrame::addQueueList()
{
	CLockRedraw<> lockRedraw(ctrlQueue);
	CLockRedraw<true> lockRedrawDir(ctrlDirs);
	{
		QueueManager::LockFileQueueShared fileQueue;
		const auto& li = fileQueue.getQueueL();
		for (auto j = li.cbegin(); j != li.cend(); ++j)
		{
			const QueueItemPtr& aQI = j->second;
			QueueItemInfo* ii = new QueueItemInfo(aQI);
			addQueueItem(ii, true);
		}
	}
	ctrlQueue.resort();
}

HTREEITEM QueueFrame::addDirectory(const string& dir, bool isFileList /* = false */, HTREEITEM startAt /* = NULL */)
{
	TVINSERTSTRUCT tvi = {0};
	tvi.hInsertAfter = TVI_SORT;
	tvi.item.mask = TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_TEXT;
	tvi.item.iImage = tvi.item.iSelectedImage = FileImage::DIR_ICON;
	
	if (isFileList)
	{
		// We assume we haven't added it yet, and that all filelists go to the same
		// directory...
		dcassert(fileLists == nullptr);
		tvi.hParent = NULL;
		tvi.item.pszText = const_cast<TCHAR*>(CTSTRING(FILE_LISTS));
		tvi.item.lParam = reinterpret_cast<LPARAM>(new string(dir));
		fileLists = ctrlDirs.InsertItem(&tvi);
		return fileLists;
	}
	
	// More complicated, we have to find the last available tree item and then see...
	string::size_type i = 0;
	string::size_type j;
	
	HTREEITEM next = NULL;
	HTREEITEM parent = NULL;
	
	if (startAt == NULL)
	{
		// First find the correct drive letter or netpath
		dcassert(dir.size() >= 3);
		dcassert((dir[1] == ':' && dir[2] == '\\') || (dir[0] == '\\' && dir[1] == '\\'));
		
		next = ctrlDirs.GetRootItem();
		
		while (next != nullptr)
		{
			if (next != fileLists)
			{
				const string* stmp = reinterpret_cast<string*>(ctrlDirs.GetItemData(next));
				if (stmp)
				{
					if (strnicmp(*stmp, dir, 3) == 0)
						break;
				}
			}
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		
		if (next == NULL)
		{
			// First addition, set commonStart to the dir minus the last part...
			i = dir.rfind('\\', dir.length() - 2);
			if (i != string::npos)
			{
				tstring name = Text::toT(dir.substr(0, i));
				tvi.hParent = NULL;
				tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
				tvi.item.lParam = reinterpret_cast<LPARAM>(new string(dir.substr(0, i + 1)));
				next = ctrlDirs.InsertItem(&tvi);
			}
			else
			{
				dcassert((dir.length() == 3 && dir[1] == ':' && dir[2] == '\\') || (dir.length() == 2 && dir[0] == '\\' && dir[1] == '\\'));
				tstring name = Text::toT(dir);
				tvi.hParent = NULL;
				tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
				tvi.item.lParam = reinterpret_cast<LPARAM>(new string(dir));
				next = ctrlDirs.InsertItem(&tvi);
			}
		}
		
		// Ok, next now points to x:\... find how much is common
		
		const string* rootStr = reinterpret_cast<string*>(ctrlDirs.GetItemData(next));
		
		i = 0;
		if (rootStr)
			for (;;)
			{
				j = dir.find('\\', i);
				if (j == string::npos)
					break;
				if (strnicmp(dir.c_str() + i, rootStr->c_str() + i, j - i + 1) != 0)
					break;
				i = j + 1;
			}
			
		if (rootStr && i < rootStr->length())
		{
			HTREEITEM oldRoot = next;
			
			// Create a new root
			tstring name = Text::toT(rootStr->substr(0, i - 1));
			tvi.hParent = NULL;
			tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
			tvi.item.lParam = reinterpret_cast<LPARAM>(new string(rootStr->substr(0, i)));
			HTREEITEM newRoot = ctrlDirs.InsertItem(&tvi);
			
			parent = addDirectory(*rootStr, false, newRoot);
			
			next = ctrlDirs.GetChildItem(oldRoot);
			while (next)
			{
				moveNode(next, parent);
				next = ctrlDirs.GetChildItem(oldRoot);
			}
			ctrlDirs.DeleteItem(oldRoot);
			parent = newRoot;
		}
		else
		{
			// Use this root as parent
			parent = next;
			next = ctrlDirs.GetChildItem(parent);
		}
	}
	else
	{
		parent = startAt;
		next = ctrlDirs.GetChildItem(parent);
		i = getDir(parent).length();
		dcassert(strnicmp(getDir(parent), dir, getDir(parent).length()) == 0);
	}
	
	while (i < dir.length())
	{
		while (next != nullptr)
		{
			if (next != fileLists)
			{
				const string& n = getDir(next);
				if (!n.empty() && strnicmp(n.c_str() + i, dir.c_str() + i, n.length() - i) == 0) // i = 56 https://www.box.net/shared/3571b0c47c1a8360aec0  n = {npos=4294967295 } https://www.box.net/shared/487b71099375c9313d2a
				{
					// Found a part, we assume it's the best one we can find...
					i = n.length();
					
					parent = next;
					next = ctrlDirs.GetChildItem(next);
					break;
				}
			}
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		
		if (next == NULL)
		{
			// We didn't find it, add...
			j = dir.find('\\', i);
			// dcassert(j != string::npos);
			tstring name;
			if (j != string::npos)
			{
				name = Text::toT(dir.substr(i, j - i));
			}
			else
			{
				name = Text::toT(dir.substr(i));
			}
			tvi.hParent = parent;
			tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
			tvi.item.lParam = reinterpret_cast<LPARAM>(new string(dir.substr(0, j + 1)));
			
			parent = ctrlDirs.InsertItem(&tvi);
			
			i = j + 1;
		}
	}
	
	return parent;
}

HTREEITEM QueueFrame::findDirItem(const string& dir) const
{
	HTREEITEM next = ctrlDirs.GetRootItem();
	HTREEITEM parent = nullptr;
	string::size_type i = 0;

	while (i < dir.length())
	{
		while (next != nullptr)
		{
			if (next != fileLists)
			{
				const string& n = getDir(next);
				if (strnicmp(n.c_str() + i, dir.c_str() + i, n.length() - i) == 0)
				{
					// Match!
					parent = next;
					next = ctrlDirs.GetChildItem(next);
					i = n.length();
					break;
				}
			}
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		if (next == nullptr)
			break;
	}

	return parent;
}

void QueueFrame::removeDirectory(const string& dir, bool isFileList /* = false */)
{
	if (isFileList)
	{
		dcassert(fileLists != nullptr);
		ctrlDirs.DeleteItem(fileLists);
		fileLists = nullptr;
		return;
	}
	
	HTREEITEM parent = findDirItem(dir);
	if (!parent) return;
	
	HTREEITEM next = parent;
	dcassert(!closed);
	while (ctrlDirs.GetChildItem(next) == NULL && directories.find(getDir(next)) == directories.end())
	{
		parent = ctrlDirs.GetParentItem(next);
		ctrlDirs.DeleteItem(next);
		if (parent == NULL)
			break;
		next = parent;
	}
}

void QueueFrame::removeDirectories(HTREEITEM ht)
{
	HTREEITEM next = ctrlDirs.GetChildItem(ht);
	while (next != NULL)
	{
		removeDirectories(next);
		next = ctrlDirs.GetNextSiblingItem(ht);
	}
	ctrlDirs.DeleteItem(ht);
}

void QueueFrame::on(QueueManagerListener::RemovedArray, const std::vector<string>& qiArray) noexcept
{
	if (!ClientManager::isBeforeShutdown())
		addTask(REMOVE_ITEM_ARRAY, new StringArrayTask(qiArray));
}

void QueueFrame::on(QueueManagerListener::Removed, const QueueItemPtr& qi) noexcept
{
	if (!ClientManager::isBeforeShutdown())
		addTask(REMOVE_ITEM, new StringTask(qi->getTarget()));
}

void QueueFrame::on(QueueManagerListener::Moved, const QueueItemPtr& qi, const string& oldTarget) noexcept
{
	addTask(REMOVE_ITEM_PTR, new RemoveQueueItemTask(qi, Util::getFilePath(oldTarget)));
	addTask(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(qi)));
}

void QueueFrame::on(QueueManagerListener::Tick, const QueueItemList& itemList) noexcept
{
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown() && !itemList.empty())
		on(QueueManagerListener::StatusUpdatedList(), itemList);
}

void QueueFrame::on(QueueManagerListener::TargetsUpdated, const StringList& targets) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		for (auto i = targets.cbegin(); i != targets.cend(); ++i)
			addTask(UPDATE_ITEM, new UpdateTask(*i));
	}
}

void QueueFrame::on(QueueManagerListener::StatusUpdated, const QueueItemPtr& qi) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
		addTask(UPDATE_ITEM, new UpdateTask(qi->getTarget()));
}

void QueueFrame::on(QueueManagerListener::StatusUpdatedList, const QueueItemList& itemList) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		for (auto i = itemList.cbegin(); i != itemList.cend(); ++i)
			on(QueueManagerListener::StatusUpdated(), *i);
	}
}

void QueueFrame::removeItem(const string& target)
{
	const auto path = Util::getFilePath(target);
	const QueueItemInfo* ii = removeItemInfo(target, path);
	if (ii)
	{
		removeItem(ii, path);	
		delete ii;
	}
}

void QueueFrame::removeItem(const QueueItemPtr& qi, const string& path)
{
	const QueueItemInfo* ii = removeItemInfo(qi, path);
	if (ii)
	{
		removeItem(ii, path);	
		delete ii;
	}
}

void QueueFrame::removeItem(const QueueItemInfo* ii, const string& path)
{
	if (!showTree || isCurDir(path))
	{
		dcassert(ctrlQueue.findItem(ii) != -1);
		ctrlQueue.deleteItem(ii);
	}
	
	if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		queueSize -= ii->getSize();
		dcassert(queueSize >= 0);
	}
	queueItems--;
	dcassert(queueItems >= 0);
	
	if (directories.find(path) == directories.end())
	{
		removeDirectory(path, ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST));
		if (isCurDir(path))
			curDir.clear();
	}
	queueChanged = true;
	
	if (!queueItems)
		updateStatus = true;
}

void QueueFrame::addTask(Tasks s, Task* task)
{
	bool firstItem;
	uint64_t tick = GET_TICK();
	uint64_t prevTick = tick;
	if (tasks.add(s, task, firstItem, prevTick) && prevTick + TIMER_VAL < tick)
		PostMessage(WM_SPEAKER);
}

void QueueFrame::processTasks()
{
	TaskQueue::List t;
	tasks.get(t);
	if (t.empty()) return;
	
	dcassert(!closed);
	for (auto ti = t.cbegin(); ti != t.cend(); ++ti)
	{
		switch (ti->first)
		{
			case ADD_ITEM:
			{
				const auto& iit = static_cast<QueueItemInfoTask&>(*ti->second);
				dcassert(ctrlQueue.findItem(iit.ii) == -1);
				addQueueItem(iit.ii, false);
				queueChanged = true;
			}
			break;
			case REMOVE_ITEM_ARRAY:
			{
				const auto& targetArray = static_cast<StringArrayTask&>(*ti->second);
				CLockRedraw<> lockRedraw(ctrlQueue);
				for (auto i = targetArray.strArray.cbegin(); i != targetArray.strArray.cend(); ++i)
					removeItem(*i);
			}
			break;
			case REMOVE_ITEM:
			{
				const auto& task = static_cast<StringTask&>(*ti->second);
				removeItem(task.str);
			}
			break;
			case REMOVE_ITEM_PTR:
			{
				const auto& task = static_cast<RemoveQueueItemTask&>(*ti->second);
				removeItem(task.qi, task.path);
			}
			break;
			case UPDATE_ITEM:
			{
				auto &ui = static_cast<UpdateTask&>(*ti->second);
				const string path = Util::getFilePath(ui.getTarget());
				const bool isCurrent = isCurDir(path);
				if (showTree && !isCurrent)
				{
					// TODO addQueueItem(ui.ii, false);
					//queueChanged = true;
				}
				if (!showTree || isCurrent)
				{
					const QueueItemInfo* ii = getItemInfo(ui.getTarget(), path);
					if (!ii)
					{
						//dcassert(0);
#ifdef _DEBUG
						//LogManager::message("Error find ui.getTarget() +" + ui.getTarget());
#endif
						break;
					}
					
					if (!showTree || isCurrent)
					{
						const int pos = ctrlQueue.findItem(ii);
						if (pos != -1)
						{
							const int l_top_index = ctrlQueue.GetTopIndex();
							if (pos >= l_top_index && pos <= l_top_index + ctrlQueue.GetCountPerPage())
							{
								ctrlQueue.updateItem(pos, COLUMN_SEGMENTS);
								ctrlQueue.updateItem(pos, COLUMN_PROGRESS);
								ctrlQueue.updateItem(pos, COLUMN_PRIORITY);
								ctrlQueue.updateItem(pos, COLUMN_USERS);
								ctrlQueue.updateItem(pos, COLUMN_ERRORS);
								ctrlQueue.updateItem(pos, COLUMN_STATUS);
								ctrlQueue.updateItem(pos, COLUMN_DOWNLOADED);
								ctrlQueue.updateItem(pos, COLUMN_SPEED);
							}
						}
					}
				}
			}
			break;
			case UPDATE_STATUS:
			{
				auto& status = static_cast<StringTask&>(*ti->second);
				ctrlStatus.SetText(1, Text::toT(status.str).c_str());
			}
			break;
			default:
				dcassert(0);
				break;
		}
		delete ti->second;
	}
}

bool QueueFrame::confirmDelete()
{
	if (BOOLSETTING(CONFIRM_DELETE))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return false;
		if (checkState == BST_CHECKED) SET_SETTING(CONFIRM_DELETE, FALSE);
	}
	return true;
}

void QueueFrame::removeSelected()
{
	if (confirmDelete())
	{
		CWaitCursor waitCursor;
		ctrlQueue.forEachSelected(&QueueItemInfo::removeBatch);
#if 0
		QueueManager::FileQueue::removeArray();
		QueueManager::getInstance()->fire_remove_batch();
#endif
	}
}

void QueueFrame::removeAllDir()
{
	if (ctrlDirs.GetSelectedItem() && confirmDelete())
	{
		QueueManager::getInstance()->removeAll();
		ctrlDirs.DeleteAllItems();
		ctrlQueue.DeleteAllItems();
	}
}

void QueueFrame::removeSelectedDir()
{
	if (ctrlDirs.GetSelectedItem() && confirmDelete())
	{
		CWaitCursor waitCursor;
		CLockRedraw<> lockRedraw(ctrlQueue);
		targetsToDelete.clear();
		removeDir(ctrlDirs.GetSelectedItem());
		auto qm = QueueManager::getInstance();
		for (auto i = targetsToDelete.cbegin(); i != targetsToDelete.cend(); ++i)
			qm->removeTarget(*i, true);
		targetsToDelete.clear();
#if 0
		QueueManager::FileQueue::removeArray();
		QueueManager::getInstance()->fire_remove_batch();
#endif
	}
}

void QueueFrame::moveSelected()
{
	tstring name;
	if (showTree)
	{
		name = Text::toT(curDir);
	}
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		vector<const QueueItemInfo*> movingItems;
		int j = -1;
		while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(j);
			movingItems.push_back(ii);
		}
		const string toDir = Text::fromT(name);
		for (auto i = movingItems.cbegin(); i != movingItems.end(); ++i)
		{
			const QueueItemInfo* ii = *i;
			const string& target = ii->getTarget();
			QueueManager::getInstance()->move(target, toDir + Util::getFileName(target));
		}
	}
}

void QueueFrame::moveTempArray()
{
	auto qm = QueueManager::getInstance();
	for (auto i = itemsToMove.cbegin(); i != itemsToMove.cend(); ++i)
		qm->move((*i).first->getTarget(), (*i).second + Util::getFileName((*i).first->getTarget()));
	itemsToMove.clear();
}

void QueueFrame::moveSelectedDir()
{
	if (ctrlDirs.GetSelectedItem() == NULL)
		return;
		
	dcassert(!curDir.empty());
	tstring name = Text::toT(curDir);
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		itemsToMove.clear();
		moveDir(ctrlDirs.GetSelectedItem(), Text::fromT(name) + Util::getLastDir(getDir(ctrlDirs.GetSelectedItem())) + PATH_SEPARATOR);
		moveTempArray();
	}
}

void QueueFrame::moveDir(HTREEITEM ht, const string& target)
{
	dcassert(!closed);
	HTREEITEM next = ctrlDirs.GetChildItem(ht);
	while (next != NULL)
	{
		// must add path separator since getLastDir only give us the name
		moveDir(next, target + Util::getLastDir(getDir(next)) + PATH_SEPARATOR);
		next = ctrlDirs.GetNextSiblingItem(next);
	}
	
	const string* s = reinterpret_cast<string*>(ctrlDirs.GetItemData(ht));
	
	if (s)
	{
		const auto p = directories.equal_range(*s);
		for (auto i = p.first; i != p.second; ++i)
			itemsToMove.push_back(TempMovePair(i->second, target));
	}
	else
	{
		dcassert(0);
	}
}

void QueueFrame::renameSelected()
{
	// Single file, get the full filename and move...
	const QueueItemInfo* ii = getSelectedQueueItem();
	const tstring target = Text::toT(ii->getTarget());
	const tstring filename = Util::getFileName(target);
	
	LineDlg dlg;
	dlg.title = TSTRING(RENAME);
	dlg.description = TSTRING(FILENAME);
	dlg.line = filename;
	if (dlg.DoModal(m_hWnd) == IDOK)
		QueueManager::getInstance()->move(ii->getTarget(), Text::fromT(target.substr(0, target.length() - filename.length()) + dlg.line));
}

void QueueFrame::renameSelectedDir()
{
	if (ctrlDirs.GetSelectedItem() == NULL)
		return;
		
	dcassert(!curDir.empty());
	const string lname = Util::getLastDir(getDir(ctrlDirs.GetSelectedItem()));
	LineDlg dlg;
	dlg.description = TSTRING(DIRECTORY);
	dlg.title = TSTRING(RENAME);
	dlg.line = Text::toT(lname);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		const string name = curDir.substr(0, curDir.length() - (lname.length() + 2)) + PATH_SEPARATOR + Text::fromT(dlg.line) + PATH_SEPARATOR;
		itemsToMove.clear();
		moveDir(ctrlDirs.GetSelectedItem(), name);
		moveTempArray();
	}
}

LRESULT QueueFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	OMenu priorityMenu;
	priorityMenu.CreatePopupMenu();
	WinUtil::appendPrioItems(priorityMenu, IDC_PRIORITY_PAUSED);
	priorityMenu.AppendMenu(MF_STRING, IDC_AUTOPRIORITY, CTSTRING(AUTO));
	
	if (reinterpret_cast<HWND>(wParam) == ctrlQueue && ctrlQueue.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlQueue, pt);
		}
		
		OMenu segmentsMenu;
		segmentsMenu.CreatePopupMenu();
		static const uint8_t segCounts[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 50, 100, 150, 200 };
		for (int i = 0; i < _countof(segCounts); i++)
		{
			tstring text = Util::toStringT(segCounts[i]);
			text += _T(' ');
			text += segCounts[i] == 1 ? TSTRING(SEGMENT) : TSTRING(SEGMENTS);
			segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTONE + segCounts[i] - 1, text.c_str());
		}
		
		if (ctrlQueue.GetSelectedCount() > 0)
		{
			usingDirMenu = false;
			CMenuItemInfo mi;
			
			while (browseMenu.GetMenuItemCount() > 0)
			{
				browseMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (removeMenu.GetMenuItemCount() > 2)
			{
				removeMenu.RemoveMenu(2, MF_BYPOSITION);
			}
			while (removeAllMenu.GetMenuItemCount() > 0)
			{
				removeAllMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (pmMenu.GetMenuItemCount() > 0)
			{
				pmMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (readdMenu.GetMenuItemCount() > 2)
			{
				readdMenu.RemoveMenu(2, MF_BYPOSITION);
			}
			clearPreviewMenu();
			
			if (ctrlQueue.GetSelectedCount() == 1)
			{
				OMenu copyMenu;
				copyMenu.CreatePopupMenu();
				copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
				for (int i = 0; i < COLUMN_LAST; ++i)
					copyMenu.AppendMenu(MF_STRING, IDC_COPY + i, CTSTRING_I(columnNames[i]));
					
				OMenu singleMenu;
				singleMenu.CreatePopupMenu();
				singleMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
				appendPreviewItems(singleMenu);
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)browseMenu, CTSTRING(GET_FILE_LIST));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)pmMenu, CTSTRING(SEND_PRIVATE_MESSAGE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)readdMenu, CTSTRING(READD_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
				singleMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)removeMenu, CTSTRING(REMOVE_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)removeAllMenu, CTSTRING(REMOVE_FROM_ALL));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
				singleMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES);
				
				const QueueItemInfo* ii = getSelectedQueueItem();
				if (ii
#ifdef FLYLINKDC_USE_TORRENT
					&& !ii->isTorrent()
#endif
				)
				{
					{
						const QueueItemPtr qi = ii->getQueueItem();
						segmentsMenu.CheckMenuItem(IDC_SEGMENTONE - 1 + qi->getMaxSegments(), MF_CHECKED);
					}
					if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
					{
						setupPreviewMenu(ii->getTarget());
						activatePreviewItems(singleMenu);
					}
					else
					{
						singleMenu.EnableMenuItem((UINT_PTR)(HMENU)segmentsMenu, MFS_DISABLED);
					}
				}
				menuItems = 0;
				int pmItems = 0;
				if (ii
#ifdef FLYLINKDC_USE_TORRENT
					&& !ii->isTorrent()
#endif
				)
				{
					const QueueItemPtr qi = ii->getQueueItem();
					if (qi)
					{
						RLock(*QueueItem::g_cs);
						const auto& sources = ii->getQueueItem()->getSourcesL(); // Делать копию нельзя
						// ниже сохраняем адрес итератора
						for (auto i = sources.cbegin(); i != sources.cend(); ++i)
						{
							const auto user = i->first;
							tstring nick = getLastNickHubT(user);
								
							// tstring nick = WinUtil::escapeMenu(WinUtil::getNicks(user, Util::emptyString));
							// add hub hint to menu
							//const auto& hubs = ClientManager::getHubNames(user->getCID(), Util::emptyString);
							//if (!hubs.empty())
							//  nick += _T(" (") + Text::toT(hubs[0]) + _T(")");
								
							mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
							mi.fType = MFT_STRING;
							mi.dwTypeData = const_cast<TCHAR*>(nick.c_str());
							mi.dwItemData = (ULONG_PTR) &i->first;
							mi.wID = IDC_BROWSELIST + menuItems;
							browseMenu.InsertMenuItem(menuItems, TRUE, &mi);
							mi.wID = IDC_REMOVE_SOURCE + 1 + menuItems; // "All" is before sources
							removeMenu.InsertMenuItem(menuItems + 2, TRUE, &mi); // "All" and separator come first
							mi.wID = IDC_REMOVE_SOURCES + menuItems;
							removeAllMenu.InsertMenuItem(menuItems, TRUE, &mi);
							if (user->isOnline())
							{
								mi.wID = IDC_PM + menuItems;
								pmMenu.InsertMenuItem(menuItems, TRUE, &mi);
								pmItems++;
							}
							menuItems++;
						}
						readdItems = 0;
						const auto& badSources = ii->getQueueItem()->getBadSourcesL(); // Делать копию нельзя
						// ниже сохраняем адрес итератора
						for (auto i = badSources.cbegin(); i != badSources.cend(); ++i)
						{
							const auto& user = i->first;
							tstring nick = WinUtil::getNicks(user, Util::emptyString);
							if (i->second.isSet(QueueItem::Source::FLAG_FILE_NOT_AVAILABLE))
							{
								nick += _T(" (") + TSTRING(FILE_NOT_AVAILABLE) + _T(")");
							}
							else if (i->second.isSet(QueueItem::Source::FLAG_PASSIVE))
							{
								nick += _T(" (") + TSTRING(PASSIVE_USER) + _T(")");
							}
							else if (i->second.isSet(QueueItem::Source::FLAG_BAD_TREE))
							{
								nick += _T(" (") + TSTRING(INVALID_TREE) + _T(")");
							}
							else if (i->second.isSet(QueueItem::Source::FLAG_NO_NEED_PARTS))
							{
								nick += _T(" (") + TSTRING(NO_NEEDED_PART) + _T(")");
							}
							else if (i->second.isSet(QueueItem::Source::FLAG_NO_TTHF))
							{
								nick += _T(" (") + TSTRING(SOURCE_TOO_OLD) + _T(")");
							}
							else if (i->second.isSet(QueueItem::Source::FLAG_SLOW_SOURCE))
							{
								nick += _T(" (") + TSTRING(SLOW_USER) + _T(")");
							}
							else if (i->second.isSet(QueueItem::Source::FLAG_UNTRUSTED))
							{
								nick += _T(" (") + TSTRING(CERTIFICATE_NOT_TRUSTED) + _T(")");
							}
							// add hub hint to menu
							const auto& hubs = ClientManager::getHubNames(user->getCID(), Util::emptyString);
							if (!hubs.empty())
								nick += _T(" (") + Text::toT(hubs[0]) + _T(")");
									
							mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
							mi.fType = MFT_STRING;
							mi.dwTypeData = const_cast<TCHAR*>(nick.c_str());
							mi.dwItemData = (ULONG_PTR) &(*i);
							mi.wID = IDC_READD + 1 + readdItems;  // "All" is before sources
							readdMenu.InsertMenuItem(readdItems + 2, TRUE, &mi);  // "All" and separator come first
							readdItems++;
						}
					}
				}
					
				if (menuItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)browseMenu, MFS_DISABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeMenu, MFS_DISABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeAllMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)browseMenu, MFS_ENABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeMenu, MFS_ENABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeAllMenu, MFS_ENABLED);
				}
				
				if (pmItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)pmMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)pmMenu, MFS_ENABLED);
				}
				
				if (readdItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)readdMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)readdMenu, MFS_ENABLED);
				}
				
				priorityMenu.CheckMenuItem(ii->getPriority(), MF_BYPOSITION | MF_CHECKED);
				if (ii->getAutoPriority())
					priorityMenu.CheckMenuItem(QueueItem::LAST, MF_BYPOSITION | MF_CHECKED);
					
				singleMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);			
			}
			else
			{
				OMenu multiMenu;
				multiMenu.CreatePopupMenu();
				multiMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
				multiMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				multiMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
				multiMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				multiMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
				multiMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES);
				multiMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}
			
			return TRUE;
		}
	}
	else if (reinterpret_cast<HWND>(wParam) == ctrlDirs && ctrlDirs.GetSelectedItem() != NULL)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlDirs, pt);
		}
		else
		{
			// Strange, windows doesn't change the selection on right-click... (!)
			UINT a = 0;
			ctrlDirs.ScreenToClient(&pt);
			HTREEITEM ht = ctrlDirs.HitTest(pt, &a);
			if (ht != NULL && ht != ctrlDirs.GetSelectedItem())
				ctrlDirs.SelectItem(ht);
			ctrlDirs.ClientToScreen(&pt);
		}
		usingDirMenu = true;
		
		OMenu dirMenu;
		dirMenu.CreatePopupMenu();
		dirMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
		dirMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
		dirMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME));
		dirMenu.AppendMenu(MF_SEPARATOR);
		OMenu deleteAllMenu;
		deleteAllMenu.CreatePopupMenu();
		deleteAllMenu.AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL_QUEUE));
		dirMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU) deleteAllMenu, CTSTRING(REMOVE_ALL));
		dirMenu.AppendMenu(MF_SEPARATOR);
		dirMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
		dirMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT QueueFrame::onRecheck(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	StringList tmp;
	int j = -1;
	while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(j);
		tmp.push_back(ii->getTarget());
	}
	for (auto i = begin(tmp); i != end(tmp); ++i)
	{
		QueueManager::getInstance()->recheck(*i);
	}
	return 0;
}

LRESULT QueueFrame::onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		WinUtil::searchHash(ctrlQueue.getItemData(i)->getTTH());
	return 0;
}

LRESULT QueueFrame::onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		WinUtil::copyMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize());
	}
	return 0;
}

LRESULT QueueFrame::onBrowseList(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		browseMenu.GetMenuItemInfo(wID, FALSE, &mi);
		OMenuItem* omi = (OMenuItem*)mi.dwItemData;
		if (omi)
		{
			const UserPtr* s   = (UserPtr*)omi->m_data;
			try
			{
				const auto& hubs = ClientManager::getHubNames((*s)->getCID(), Util::emptyString);
				QueueManager::getInstance()->addList(HintedUser(*s, !hubs.empty() ? hubs[0] : Util::emptyString), QueueItem::FLAG_CLIENT_VIEW);
			}
			catch (const Exception&)
			{
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onReadd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
#ifdef FLYLINKDC_USE_TORRENT
		if (ii->isTorrent())
			return 0;
#endif
			
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		
		readdMenu.GetMenuItemInfo(wID, FALSE, &mi);
		if (wID == IDC_READD)
		{
			try
			{
				const auto& item = ii->getQueueItem();
				QueueManager::getInstance()->readdAll(item);
			}
			catch (const QueueException& e)
			{
				ctrlStatus.SetText(1, Text::toT(e.getError()).c_str());
			}
		}
		else
		{
			OMenuItem* omi = (OMenuItem*)mi.dwItemData;
			if (omi)
			{
				const UserPtr s = *(UserPtr*)omi->m_data; // TODO - https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=62702
				// Мертвый юзер
				try
				{
					QueueManager::getInstance()->readd(ii->getTarget(), s);
				}
				catch (const QueueException& e)
				{
					ctrlStatus.SetText(1, Text::toT(e.getError()).c_str());
				}
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onRemoveSource(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		sourcesToRemove.clear();
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
#ifdef FLYLINKDC_USE_TORRENT
		if (ii->isTorrent())
			return 0;
#endif

		if (wID == IDC_REMOVE_SOURCE)
		{
			RLock(*QueueItem::g_cs);
			const auto& sources = ii->getQueueItem()->getSourcesL();
			for (auto si = sources.cbegin(); si != sources.cend(); ++si)
			{
				sourcesToRemove.push_back(std::make_pair(ii->getTarget(), si->first));
			}
		}
		else
		{
			CMenuItemInfo mi;
			mi.fMask = MIIM_DATA;
			removeMenu.GetMenuItemInfo(wID, FALSE, &mi);
			OMenuItem* omi = (OMenuItem*)mi.dwItemData;
			if (omi)
			{
				UserPtr* s = (UserPtr*)omi->m_data; // https://drdump.com/Problem.aspx?ProblemID=241344
				sourcesToRemove.push_back(std::make_pair(ii->getTarget(), *s));
			}
		}
		removeSources();
	}
	return 0;
}

LRESULT QueueFrame::onRemoveSources(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CMenuItemInfo mi;
	mi.fMask = MIIM_DATA;
	removeAllMenu.GetMenuItemInfo(wID, FALSE, &mi);
	OMenuItem* omi = (OMenuItem*)mi.dwItemData;
	if (omi)
	{
		if (UserPtr* s = (UserPtr*)omi->m_data)
		{
			QueueManager::getInstance()->removeSource(*s, QueueItem::Source::FLAG_REMOVED);
		}
	}
	return 0;
}

LRESULT QueueFrame::onPM(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		
		pmMenu.GetMenuItemInfo(wID, FALSE, &mi);
		OMenuItem* omi = (OMenuItem*)mi.dwItemData;
		if (omi)
		{
			if (UserPtr* s = (UserPtr*)omi->m_data)
			{
				// [!] IRainman: Open the window of PM with an empty address if the user NMDC,
				// as soon as it appears on the hub of the network, the window immediately PM knows about it, and update the Old.
				// If the user ADC, as soon as he appears on any of the ADC hubs at once a personal window to know.
				const auto hubs = ClientManager::getHubs((*s)->getCID(), Util::emptyString);
				PrivateFrame::openWindow(nullptr, HintedUser(*s, !hubs.empty() ? hubs[0] : Util::emptyString));
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onAutoPriority(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{

	if (usingDirMenu)
	{
		setAutoPriority(ctrlDirs.GetSelectedItem(), true);
	}
	else
	{
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			QueueManager::getInstance()->setAutoPriority(ctrlQueue.getItemData(i)->getTarget(), !ctrlQueue.getItemData(i)->getAutoPriority());
		}
	}
	return 0;
}

LRESULT QueueFrame::onSegments(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItemInfo* ii = ctrlQueue.getItemData(i);
#ifdef FLYLINKDC_USE_TORRENT
		if (!ii->isTorrent())
#endif
		{
			{
				const QueueItemPtr& qi = ii->getQueueItem();
				qi->setMaxSegments(max((uint8_t)1, (uint8_t)(wID - IDC_SEGMENTONE + 1))); // !BUGMASTER!-S
			}
			ctrlQueue.updateItem(ctrlQueue.findItem(ii), COLUMN_SEGMENTS);
		}
	}
	
	return 0;
}

LRESULT QueueFrame::onPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	QueueItem::Priority p;
	
	if (wID >= IDC_PRIORITY_PAUSED && wID < IDC_PRIORITY_PAUSED + QueueItem::LAST)
		p = (QueueItem::Priority) (wID - IDC_PRIORITY_PAUSED);
	else
		p = QueueItem::DEFAULT;
	
	if (usingDirMenu)
	{
		setPriority(ctrlDirs.GetSelectedItem(), p);
	}
	else
	{
		auto qm = QueueManager::getInstance();
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			const string& target = ctrlQueue.getItemData(i)->getTarget();
			qm->setPriority(target, p, true);
		}
	}
	
	return 0;
}

void QueueFrame::removeDir(HTREEITEM ht)
{
	dcassert(!closed);
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child != NULL)
	{
		removeDir(child);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	const auto dp = directories.equal_range(name);
	for (auto i = dp.first; i != dp.second; ++i)
		targetsToDelete.push_back(i->second->getTarget());
}

/*
 * @param inc True = increase, False = decrease
 */
void QueueFrame::changePriority(bool inc)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItem::Priority p = ctrlQueue.getItemData(i)->getPriority();
		int newPriority = p + (inc ? 1 : -1);
		if (newPriority < QueueItem::PAUSED || newPriority > QueueItem::HIGHEST)
		{
			// Trying to go higher than HIGHEST or lower than PAUSED
			// so do nothing
			continue;
		}
		
		p = (QueueItem::Priority) newPriority;

		const string& target = ctrlQueue.getItemData(i)->getTarget();
		QueueManager::getInstance()->setPriority(target, p, true);
	}
}

void QueueFrame::setPriority(HTREEITEM ht, const QueueItem::Priority& p)
{
	dcassert(!closed);
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child)
	{
		setPriority(child, p);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	const auto dp = directories.equal_range(name);	
	auto qm = QueueManager::getInstance();
	for (auto i = dp.first; i != dp.second; ++i)
		qm->setPriority(i->second->getTarget(), p, true);
}

void QueueFrame::setAutoPriority(HTREEITEM ht, const bool& ap)
{
	dcassert(!closed);
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child)
	{
		setAutoPriority(child, ap);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	const auto dp = directories.equal_range(name);
	for (auto i = dp.first; i != dp.second; ++i)
	{
		QueueManager::getInstance()->setAutoPriority(i->second->getTarget(), ap);
	}
}

void QueueFrame::updateQueueStatus()
{
	if (!closed && ctrlStatus.IsWindow())
	{
		int64_t total = 0;
		unsigned cnt = ctrlQueue.GetSelectedCount();
		if (cnt == 0)
		{
			cnt = ctrlQueue.GetItemCount();
			
			if (showTree)
			{
				if (lastCount != cnt)
				{
					int i = -1;
					while (!closed && (i = ctrlQueue.GetNextItem(i, LVNI_ALL)) != -1)
					{
						const QueueItemInfo* ii = ctrlQueue.getItemData(i);
						if (ii)
						{
							const int64_t size = ii->getSize(); // https://drdump.com/Problem.aspx?ProblemID=131118 + https://drdump.com/Problem.aspx?ProblemID=143789
							total += size;
						}
					}
					lastCount = cnt;
					lastTotal = total;
				}
				else
				{
					total = lastTotal;
				}
			}
			else
			{
				total = queueSize;
			}
		}
		else
		{
			if (lastCount != cnt)
			{
				int i = -1;
				while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
				{
					const QueueItemInfo* ii = ctrlQueue.getItemData(i);
					total += ii->getSize();
				}
				lastCount = cnt;
				lastTotal = total;
			}
			else
			{
				total = lastTotal;
			}
		}
		
		tstring tmp1 = TSTRING(ITEMS) + _T(": ") + Util::toStringT(cnt);
		tstring tmp2 = TSTRING(SIZE) + _T(": ") + Util::formatBytesT(total);
		bool u = false;
		
		int w = WinUtil::getTextWidth(tmp1, ctrlStatus.m_hWnd);
		if (statusSizes[1] < w)
		{
			statusSizes[1] = w;
			u = true;
		}
		ctrlStatus.SetText(2, tmp1.c_str());
		w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
		if (statusSizes[2] < w)
		{
			statusSizes[2] = w;
			u = true;
		}
		ctrlStatus.SetText(3, tmp2.c_str());
		
		if (queueChanged)
		{
			tmp1 = TSTRING(FILES) + _T(": ") + Util::toStringT(queueItems);
			tmp2 = TSTRING(SIZE) + _T(": ") + Util::formatBytesT(queueSize);
			
			w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
			if (statusSizes[3] < w)
			{
				statusSizes[3] = w;
				u = true;
			}
			ctrlStatus.SetText(4, tmp1.c_str());
			
			w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
			if (statusSizes[4] < w)
			{
				statusSizes[4] = w;
				u = true;
			}
			ctrlStatus.SetText(5, tmp2.c_str());
			
			queueChanged = false;
		}
		
		if (u)
		{
			UpdateLayout(TRUE);
		}
	}
}

void QueueFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	{
		RECT rect;
		GetClientRect(&rect);
		// position bars and offset their dimensions
		UpdateBarsPosition(rect, bResizeBars);
		
		if (ctrlStatus.IsWindow())
		{
			CRect sr;
			int w[6];
			ctrlStatus.GetClientRect(sr);
			w[5] = sr.right - 16;
#define setw(x) w[x] = max(w[x+1] - statusSizes[x], 0)
			setw(4);
			setw(3);
			setw(2);
			setw(1);
			
			w[0] = 36;
			
			ctrlStatus.SetParts(6, w);
			
			ctrlStatus.GetRect(0, sr);
			if (ctrlShowTree.IsWindow())
				ctrlShowTree.MoveWindow(sr);
		}
		
		if (showTree)
		{
			if (GetSinglePaneMode() != SPLIT_PANE_NONE)
			{
				SetSinglePaneMode(SPLIT_PANE_NONE);
				updateQueue();
			}
		}
		else
		{
			if (GetSinglePaneMode() != SPLIT_PANE_RIGHT)
			{
				SetSinglePaneMode(SPLIT_PANE_RIGHT);
				updateQueue();
			}
		}
		
		CRect rc = rect;
		SetSplitterRect(rc);
	}
}

LRESULT QueueFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	timer.destroyTimer();
	tasks.setDisabled(true);

	if (!closed)
	{
		closed = true;
		SettingsManager::getInstance()->removeListener(this);
		DownloadManager::getInstance()->removeListener(this);
		QueueManager::getInstance()->removeListener(this);
		
		WinUtil::setButtonPressed(IDC_QUEUE, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		SET_SETTING(QUEUE_FRAME_SHOW_TREE, ctrlShowTree.GetCheck() == BST_CHECKED);
		ctrlDirs.DeleteAllItems();
		ctrlQueue.DeleteAllItems();
		// try fix https://www.crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=101839
		// https://www.crash-server.com/Problem.aspx?ProblemID=43187
		// https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=30936
		for (auto i = directories.cbegin(); i != directories.cend(); ++i)
		{
			delete i->second;
		}
		directories.clear();
		
		ctrlQueue.saveHeaderOrder(SettingsManager::QUEUE_FRAME_ORDER, SettingsManager::QUEUE_FRAME_WIDTHS, SettingsManager::QUEUE_FRAME_VISIBLE);
		SET_SETTING(QUEUE_FRAME_SORT, ctrlQueue.getSortForSettings());
		SET_SETTING(QUEUE_FRAME_SPLIT, m_nProportionalPos);
		tasks.clear();
		bHandled = FALSE;
		return 0;
	}
}

LRESULT QueueFrame::onTreeItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
#if 0
	const NMTREEVIEW* tv = reinterpret_cast<const NMTREEVIEW*>(pnmh);
	DWORD_PTR data = ctrlDirs.GetItemData(tv->itemNew.hItem);
	tstring text;
	if (data) Text::toT(*reinterpret_cast<const string*>(data), text);
	ctrlStatus.SetText(1, text.c_str());
#endif
	updateQueue();
	return 0;
}

void QueueFrame::onTab()
{
	if (showTree)
	{
		HWND focus = ::GetFocus();
		if (focus == ctrlDirs.m_hWnd)
		{
			ctrlQueue.SetFocus();
		}
		else if (focus == ctrlQueue.m_hWnd)
		{
			ctrlDirs.SetFocus();
		}
	}
}

void QueueFrame::updateQueue()
{
	dcassert(!closed);
	CWaitCursor waitCursor;
	ctrlQueue.DeleteAllItems();
	QueueDirectoryPairC i;
	if (showTree)
	{
		i = directories.equal_range(getSelectedDir());
	}
	else
	{
		i.first = directories.begin();
		i.second = directories.end();
	}
	
	CLockRedraw<> lockRedraw(ctrlQueue);
	int count = ctrlQueue.GetItemCount();
	for (auto j = i.first; j != i.second; ++j)
	{
		QueueItemInfo* ii = j->second;
		ctrlQueue.insertItem(count++, ii, I_IMAGECALLBACK);
	}
	ctrlQueue.resort();
	curDir = getSelectedDir();
}

// Put it here to avoid a copy for each recursion...
void QueueFrame::moveNode(HTREEITEM item, HTREEITEM parent)
{
	static TCHAR g_tmpBuf[1024];
	g_tmpBuf[0] = 0;
	TVINSERTSTRUCT tvis = {0};
	tvis.itemex.hItem = item;
	tvis.itemex.mask = TVIF_CHILDREN | TVIF_HANDLE | TVIF_IMAGE | TVIF_INTEGRAL | TVIF_PARAM |
	                   TVIF_SELECTEDIMAGE | TVIF_STATE | TVIF_TEXT;
	tvis.itemex.pszText = g_tmpBuf;
	tvis.itemex.cchTextMax = _countof(g_tmpBuf);
	ctrlDirs.GetItem((TVITEM*)&tvis.itemex);
	tvis.hInsertAfter = TVI_SORT;
	tvis.hParent = parent;
	tvis.item.mask &= ~TVIF_HANDLE;
	ctrlDirs.SetItemData(item, 0); // string stored in Item Data is now owned by a new item ht
	HTREEITEM ht = ctrlDirs.InsertItem(&tvis);
	HTREEITEM next = ctrlDirs.GetChildItem(item);
	while (next)
	{
		moveNode(next, ht);
		next = ctrlDirs.GetChildItem(item);
	}
	ctrlDirs.DeleteItem(item);
}

LRESULT QueueFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)pnmh;
	
	const QueueItemInfo *ii = (const QueueItemInfo*)cd->nmcd.lItemlParam;
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
		{
#if 0
			if (ii &&
			        !ii->isTorrent() &&
			        ii->getQueueItem() &&  // https://drdump.com/Problem.aspx?ProblemID=259662
			        !ii->getQueueItem()->getBadSourcesL().empty()) // https://www.crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=117848
			{
				cd->clrText = SETTING(ERROR_COLOR);
				return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
			}
			return CDRF_NOTIFYSUBITEMDRAW;
#else
			return CDRF_NOTIFYSUBITEMDRAW;
#endif
		}
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			if (ctrlQueue.findColumn(cd->iSubItem) == COLUMN_PROGRESS)
			{
				if (!BOOLSETTING(SHOW_PROGRESS_BARS))
				{
					bHandled = FALSE;
					return 0;
				}
				if (ii->getSize() == -1) return CDRF_DODEFAULT;
				
				CRect rc;
				ctrlQueue.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				CBarShader statusBar(rc.Height(), rc.Width(), SETTING(PROGRESS_BACK_COLOR), ii->getSize());
#ifdef FLYLINKDC_USE_TORRENT
				if (!ii->isTorrent())
#endif
				{
					COLORREF colorRunning = SETTING(COLOR_RUNNING);
					COLORREF colorRunning2 = SETTING(COLOR_RUNNING_COMPLETED);
					COLORREF colorDownloaded = SETTING(COLOR_DOWNLOADED);
					QueueManager::getChunksVisualisation(ii->getQueueItem(), runningChunks, doneChunks);
					for (auto i = runningChunks.cbegin(); i < runningChunks.cend(); ++i)
					{
						const QueueItem::RunningSegment& rs = *i;
						statusBar.FillRange(rs.start, rs.end, colorRunning);
						statusBar.FillRange(rs.start, rs.start + rs.pos, colorRunning2);
					}
					for (auto i = doneChunks.cbegin(); i < doneChunks.cend(); ++i)
					{
						statusBar.FillRange(i->getStart(), i->getEnd(), colorDownloaded);
					}
				}
				CDC cdc;
				cdc.CreateCompatibleDC(cd->nmcd.hdc);
				HBITMAP pOldBmp = cdc.SelectBitmap(CreateCompatibleBitmap(cd->nmcd.hdc,  rc.Width(),  rc.Height()));
				
				statusBar.Draw(cdc, 0, 0, SETTING(PROGRESS_3DDEPTH));
				BitBlt(cd->nmcd.hdc, rc.left, rc.top, rc.Width(), rc.Height(), cdc.m_hDC, 0, 0, SRCCOPY);
				DeleteObject(cdc.SelectBitmap(pOldBmp));
				
				return CDRF_SKIPDEFAULT;
			}
		}
		default:
			return CDRF_DODEFAULT;
	}
}

LRESULT QueueFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = frameIcon;
	opt->isHub = false;
	return TRUE;
}

LRESULT QueueFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring data;
	int i = -1, columnId = wID - IDC_COPY; // !SMT!-UI: copy several rows
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItemInfo* ii = ctrlQueue.getItemData(i);
		tstring sdata;
		if (wID == IDC_COPY_LINK)
			sdata = Text::toT(Util::getMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize()));
		else if (wID == IDC_COPY_WMLINK)
			sdata = Text::toT(Util::getWebMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize()));
		else
			sdata = ii->getText(columnId);
			
		if (data.empty())
			data = std::move(sdata);
		else
		{
			data += _T("\r\n");
			data += sdata;
		}
	}
	WinUtil::setClipboard(data);
	return 0;
}

LRESULT QueueFrame::onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		const QueueItemInfo* ii = getSelectedQueueItem();
#ifdef FLYLINKDC_USE_TORRENT
		if (ii->isTorrent())
			return 0;
#endif
		const auto& qi = ii->getQueueItem();
		startMediaPreview(wID, qi);
	}
	return 0;
}

LRESULT QueueFrame::onRemoveOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int j = -1;
	sourcesToRemove.clear();
	while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(j);
#ifdef FLYLINKDC_USE_TORRENT
		if (!ii->isTorrent())
#endif
		{
			RLock(*QueueItem::g_cs);
			const auto& sources = ii->getQueueItem()->getSourcesL();
			for (auto i = sources.cbegin(); i != sources.cend(); ++i)  // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=111640
			{
				if (!i->first->isOnline())
				{
					sourcesToRemove.push_back(std::make_pair(ii->getTarget(), i->first));
				}
			}
		}
	}
	removeSources();
	return 0;
}

void QueueFrame::removeSources()
{
	auto qm = QueueManager::getInstance();
	for (auto j = sourcesToRemove.cbegin(); j != sourcesToRemove.cend(); ++j)
		qm->removeSource(j->first, j->second, QueueItem::Source::FLAG_REMOVED);
}

void QueueFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlQueue.isRedraw())
		{
			ctrlQueue.setFlickerFree(Colors::g_bgBrush);
			ctrlDirs.SetBkColor(Colors::g_bgColor);
			ctrlDirs.SetTextColor(Colors::g_textColor);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

void QueueFrame::onRechecked(const string& target, const string& message)
{
	if (!ClientManager::isBeforeShutdown())
	{
		addTask(UPDATE_STATUS, new StringTask(STRING_F(INTEGRITY_CHECK_FMT, message % target)));
	}
}

void QueueFrame::on(QueueManagerListener::RecheckStarted, const string& target) noexcept
{
	onRechecked(target, STRING(STARTED));
}

void QueueFrame::on(QueueManagerListener::RecheckNoFile, const string& target) noexcept
{
	onRechecked(target, STRING(UNFINISHED_FILE_NOT_FOUND));
}

void QueueFrame::on(QueueManagerListener::RecheckFileTooSmall, const string& target) noexcept
{
	onRechecked(target, STRING(UNFINISHED_FILE_TOO_SMALL));
}

void QueueFrame::on(QueueManagerListener::RecheckDownloadsRunning, const string& target) noexcept
{
	onRechecked(target, STRING(DOWNLOADS_RUNNING));
}

void QueueFrame::on(QueueManagerListener::RecheckNoTree, const string& target) noexcept
{
	onRechecked(target, STRING(NO_FULL_TREE));
}

void QueueFrame::on(QueueManagerListener::RecheckAlreadyFinished, const string& target) noexcept
{
	onRechecked(target, STRING(FILE_ALREADY_FINISHED));
}

void QueueFrame::on(QueueManagerListener::RecheckDone, const string& target) noexcept
{
	onRechecked(target, STRING(DONE));
}

LRESULT QueueFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*)pnmh;
	if (kd->wVKey == VK_DELETE)
	{
		if (ctrlQueue.getSelectedCount())
		{
			removeSelected();
		}
	}
	else if (kd->wVKey == VK_ADD)
	{
		// Increase Item priority
		changePriority(true);
	}
	else if (kd->wVKey == VK_SUBTRACT)
	{
		// Decrease item priority
		changePriority(false);
	}
	else if (kd->wVKey == VK_TAB)
	{
		onTab();
	}
	return 0;
}

LRESULT QueueFrame::onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTVKEYDOWN* kd = (NMTVKEYDOWN*)pnmh;
	if (kd->wVKey == VK_DELETE)
	{
		removeSelectedDir();
	}
	else if (kd->wVKey == VK_TAB)
	{
		onTab();
	}
	return 0;
}

LRESULT QueueFrame::onDeleteTreeItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMTREEVIEW* tv = reinterpret_cast<const NMTREEVIEW*>(pnmh);
	delete reinterpret_cast<string*>(tv->itemOld.lParam);
	return 0;
}

LRESULT QueueFrame::onShowQueueItem(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
	string* target  = reinterpret_cast<string*>(lParam);
	HTREEITEM ht = nullptr;
	if (wParam)
	{
		ht = fileLists;
	}
	else
	{
		const auto path = Util::getFilePath(*target);
		const QueueItemInfo* ii = getItemInfo(*target, path);
		if (ii) ht = findDirItem(path);
	}
	if (ht)
	{
		ctrlDirs.EnsureVisible(ht);
		ctrlDirs.SelectItem(ht);
		// TODO: select file
	}
	delete target;
	return 0;
}

void QueueFrame::showQueueItem(string& target, bool isList)
{
	string* s = new string(std::move(target));
	if (!PostMessage(WMU_SHOW_QUEUE_ITEM, isList ? 1 : 0, reinterpret_cast<LPARAM>(s)))
		delete s;
}

string QueueFrame::getSelectedDir() const
{
	HTREEITEM ht = ctrlDirs.GetSelectedItem();
	return ht == NULL ? Util::emptyString : getDir(ctrlDirs.GetSelectedItem());
}

string QueueFrame::getDir(HTREEITEM ht) const
{
	dcassert(ht != NULL);
	if (ht)
	{
		string *pstr = reinterpret_cast<string*>(ctrlDirs.GetItemData(ht));
		if (pstr)
			return *pstr;
		else
		{
			dcassert(0);
			return Util::emptyString;
		}
	}
	else
		return Util::emptyString;
}

void QueueFrame::onTimerInternal()
{
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown() && updateStatus)
	{
		updateQueueStatus();
		setCountMessages(ctrlQueue.GetItemCount());
		updateStatus = false;
	}
	processTasks();
}
