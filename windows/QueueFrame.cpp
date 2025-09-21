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
#include "DirectoryListingFrm.h"
#include "LineDlg.h"
#include "Fonts.h"
#include "ExMessageBox.h"
#include "BrowseFile.h"
#include "MenuHelper.h"
#include "ConfUI.h"
#include "../client/ClientManager.h"
#include "../client/DownloadManager.h"
#include "../client/PathUtil.h"
#include "../client/FormatUtil.h"
#include "../client/TimeUtil.h"
#include "../client/Util.h"
#include "../client/SysVersion.h"

#ifdef DEBUG_QUEUE_FRAME
int QueueFrame::DirItem::itemsCreated;
int QueueFrame::DirItem::itemsRemoved;
int QueueFrame::QueueItemInfo::itemsCreated;
int QueueFrame::QueueItemInfo::itemsRemoved;
#endif

const TTHValue QueueFrame::QueueItemInfo::emptyTTH;

static const unsigned TIMER_VAL = 1000;

const int QueueFrame::columnId[] =
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
	COLUMN_EXACT_SIZE,
	COLUMN_ERRORS,
	COLUMN_ADDED,
	COLUMN_TTH,
	COLUMN_SPEED
};

static const int columnSizes[] =
{
	210, // COLUMN_TARGET
	70,  // COLUMN_TYPE
	145, // COLUMN_STATUS
	70,  // SEGMENTS
	85,  // COLUMN_SIZE
	300, // COLUMN_PROGRESS
	120, // COLUMN_DOWNLOADED
	95,  // COLUMN_PRIORITY
	180, // COLUMN_USERS
	240, // COLUMN_PATH
	100, // COLUMN_EXACT_SIZE
	120, // COLUMN_ERRORS
	120, // COLUMN_ADDED
	125, // COLUMN_TTH
	75   // COLUMN_SPEED
};

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
	ResourceManager::EXACT_SIZE,
	ResourceManager::ERRORS,
	ResourceManager::ADDED,
	ResourceManager::TTH_ROOT,
	ResourceManager::SPEED
};

QueueFrame::QueueFrame() :
	timer(m_hWnd),
	usingDirMenu(false), fileLists(nullptr), showTree(true),
	showTreeContainer(WC_BUTTON, this, SHOWTREE_MESSAGE_MAP),
	clearingTree(0), currentDir(nullptr), treeInserted(false),
	lastTotalCount(0), lastTotalSize(-1), updateStatus(false)
{
	root = new DirItem;
	ctrlQueue.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlQueue.setColumnFormat(COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlQueue.setColumnFormat(COLUMN_DOWNLOADED, LVCFMT_RIGHT);
	ctrlQueue.setColumnFormat(COLUMN_EXACT_SIZE, LVCFMT_RIGHT);
	ctrlQueue.setColumnFormat(COLUMN_SEGMENTS, LVCFMT_RIGHT);

	StatusBarCtrl::PaneInfo pi;
	pi.minWidth = pi.maxWidth = 0;
	pi.weight = 0;
	pi.align = StatusBarCtrl::ALIGN_LEFT;
	pi.flags = StatusBarCtrl::PANE_FLAG_NO_DECOR;
	ctrlStatus.addPane(pi);

	pi.minWidth = 0;
	pi.maxWidth = INT_MAX;
	pi.weight = 1;
	pi.flags = 0;
	ctrlStatus.addPane(pi);

	pi.flags = StatusBarCtrl::PANE_FLAG_HIDE_EMPTY | StatusBarCtrl::PANE_FLAG_NO_SHRINK;
	pi.weight = 0;
	for (int i = 2; i < STATUS_LAST; ++i)
		ctrlStatus.addPane(pi);
}

QueueFrame::~QueueFrame()
{
	destroyMenus();
	deleteTree(fileLists);
	deleteTree(root);
#ifdef DEBUG_QUEUE_FRAME
	LogManager::message("QueueFrame::DirItem: created=" + Util::toString(QueueFrame::DirItem::itemsCreated) + " removed=" + Util::toString(QueueFrame::DirItem::itemsRemoved), false);
	LogManager::message("QueueFrame::QueueItemInfo: created=" + Util::toString(QueueFrame::QueueItemInfo::itemsCreated) + " removed=" + Util::toString(QueueFrame::QueueItemInfo::itemsRemoved), false);
#endif
}

static tstring getSourceName(const UserPtr& user)
{
	tstring res;
#ifdef BL_FEATURE_IP_DATABASE
	string nick, hubUrl;
	if (user->getLastNickAndHub(nick, hubUrl))
	{
		nick += " - ";
		nick += WinUtil::getHubDisplayName(hubUrl);
		res = Text::toT(nick);
	}
#endif
	if (res.empty()) res = Text::toT(user->getLastNick());
	return res;
}

static void checkSourceFlags(tstring& nick, Flags::MaskType flags)
{
	if (flags & QueueItem::Source::FLAG_PARTIAL) nick.insert(0, _T("[P] "));
}

static const tstring& getErrorText(Flags::MaskType flags)
{
	if (flags & QueueItem::Source::FLAG_FILE_NOT_AVAILABLE)
		return TSTRING(FILE_NOT_AVAILABLE);
	if (flags & QueueItem::Source::FLAG_NO_NEED_PARTS)
		return TSTRING(NO_NEEDED_PART);
	if (flags & QueueItem::Source::FLAG_BAD_TREE)
		return TSTRING(INVALID_TREE);
	if (flags & QueueItem::Source::FLAG_NO_TTHF)
		return TSTRING(SOURCE_TOO_OLD);
	if (flags & QueueItem::Source::FLAG_PASSIVE)
		return TSTRING(PASSIVE_USER);
	if (flags & QueueItem::Source::FLAG_SLOW_SOURCE)
		return TSTRING(SLOW_USER);
	if (flags & QueueItem::Source::FLAG_UNTRUSTED)
		return TSTRING(CERTIFICATE_NOT_TRUSTED);
	return Util::emptyStringT;
}

void QueueFrame::sortSources(vector<QueueFrame::SourceInfo>& v)
{
	std::sort(v.begin(), v.end(), [](const SourceInfo& a, const SourceInfo &b)
	{
		Flags::MaskType aPartial = a.flags & QueueItem::Source::FLAG_PARTIAL;
		Flags::MaskType bPartial = b.flags & QueueItem::Source::FLAG_PARTIAL;
		if (aPartial != bPartial) return aPartial < bPartial;
		return a.nick < b.nick;
	});
}

static int getCheckBoxSize(HWND hWnd)
{
	tstring str;
	WinUtil::getWindowText(hWnd, str);
	HDC hdc = GetDC(hWnd);
	if (!hdc) return 0;
	CustomDrawHelpers::CustomDrawCheckBoxState cb;
	cb.init(hWnd, hdc);
	int result = cb.checkBoxSize.cx;
	if (!str.empty()) result += WinUtil::getTextWidth(str, hdc) + cb.checkBoxGap;
	ReleaseDC(hWnd, hdc);
	return result;
}

LRESULT QueueFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	showTree = ss->getBool(Conf::QUEUE_FRAME_SHOW_TREE);
	showProgressBars = ss->getBool(Conf::SHOW_PROGRESS_BARS);

	ctrlStatus.setAutoGripper(true);
	ctrlStatus.Create(m_hWnd, 0, nullptr, WS_CHILD | WS_CLIPCHILDREN);
	ctrlStatus.setFont(Fonts::g_systemFont, false);

	ctrlQueue.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_QUEUE);
	ctrlQueue.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlQueue);

	ctrlDirs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WinUtil::getTreeViewStyle(),
	                WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	WinUtil::setTreeViewTheme(ctrlDirs, Colors::isDarkTheme);

	ctrlDirs.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlQueue.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);

	addSplitter(-1, FLAG_HORIZONTAL | FLAG_PROPORTIONAL | FLAG_INTERACTIVE, ss->getInt(Conf::QUEUE_FRAME_SPLIT));
	if (Colors::isAppThemed && SysVersion::isOsVistaPlus())
		setSplitterColor(0, COLOR_TYPE_SYSCOLOR, COLOR_WINDOW);
	setPaneWnd(0, ctrlDirs.m_hWnd);
	setPaneWnd(1, ctrlQueue.m_hWnd);

	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));

	ctrlQueue.insertColumns(Conf::QUEUE_FRAME_ORDER, Conf::QUEUE_FRAME_WIDTHS, Conf::QUEUE_FRAME_VISIBLE);
	ctrlQueue.setSortFromSettings(ss->getInt(Conf::QUEUE_FRAME_SORT));

	setListViewColors(ctrlQueue);
	setTreeViewColors(ctrlDirs);

	ctrlShowTree.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX);
	ctrlShowTree.SetCheck(showTree ? BST_CHECKED : BST_UNCHECKED);
	ctrlShowTree.SetFont(Fonts::g_systemFont);
	showTreeContainer.SubclassWindow(ctrlShowTree.m_hWnd);

	int checkBoxSize = getCheckBoxSize(ctrlShowTree);
	StatusBarCtrl::PaneInfo statusPane;
	ctrlStatus.getPaneInfo(STATUS_CHECKBOX, statusPane);
	statusPane.minWidth = statusPane.maxWidth = checkBoxSize + 10;
	ctrlStatus.setPaneInfo(STATUS_CHECKBOX, statusPane);

	addQueueList();
	QueueManager::getInstance()->addListener(this);
	DownloadManager::getInstance()->addListener(this);
	SettingsManager::instance.addListener(this);

	updateQueueStatus();

	timer.createTimer(TIMER_VAL);
	bHandled = FALSE;
	return 1;
}

void QueueFrame::clearMenu(OMenu& menu, int count)
{
	for (int i = menu.GetMenuItemCount()-1; i >= count; --i)
		menu.DeleteMenu(i, MF_BYPOSITION);
}

void QueueFrame::clearCheck(HMENU hMenu)
{
	CMenuHandle menu(hMenu);
	for (int i = menu.GetMenuItemCount()-1; i >= 0; --i)
		menu.CheckMenuItem(i, MF_BYPOSITION | MF_UNCHECKED);
}

void QueueFrame::initPriorityMenu()
{
	if (!priorityMenu)
	{
		priorityMenu.SetOwnerDraw(OMenu::OD_NEVER);
		priorityMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(priorityMenu);
		MenuHelper::appendPrioItems(priorityMenu, IDC_PRIORITY_PAUSED);
		priorityMenu.AppendMenu(MF_SEPARATOR);
		priorityMenu.AppendMenu(MF_STRING, IDC_AUTOPRIORITY, CTSTRING(AUTO));
	}
	else
		clearCheck(priorityMenu);
}

void QueueFrame::createMenus()
{
	if (!browseMenu)
	{
		browseMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(browseMenu);
	}
	else
		browseMenu.ClearMenu();
	if (!removeMenu)
	{
		removeMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(removeMenu);
		removeMenu.AppendMenu(MF_STRING, IDC_REMOVE_SOURCE, CTSTRING(ALL));
		removeMenu.AppendMenu(MF_SEPARATOR);
	}
	else
		clearMenu(removeMenu, 2);
	if (!removeAllMenu)
	{
		removeAllMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(removeAllMenu);
	}
	else
		removeAllMenu.ClearMenu();
	if (!pmMenu)
	{
		pmMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(pmMenu);
	}
	else
		pmMenu.ClearMenu();
	if (!readdMenu)
	{
		readdMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(readdMenu);
		readdMenu.AppendMenu(MF_STRING, IDC_READD, CTSTRING(ALL));
		readdMenu.AppendMenu(MF_SEPARATOR);
	}
	else
		clearMenu(readdMenu, 2);
	if (!segmentsMenu)
	{
		segmentsMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(segmentsMenu);
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID;
		mii.fType = MFT_RADIOCHECK;
		static const uint8_t segCounts[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 50, 100, 150, 200 };
		for (int i = 0; i < _countof(segCounts); i++)
		{
			int count = segCounts[i];
			tstring text = TPLURAL_F(PLURAL_SEGMENTS, count);
			mii.wID = IDC_SEGMENTONE + count - 1;
			mii.dwTypeData = const_cast<TCHAR*>(text.c_str());
			segmentsMenu.InsertMenuItem(i, TRUE, &mii);
		}
	}
	else
		clearCheck(segmentsMenu);
	if (!copyMenu)
	{
		copyMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(copyMenu);
		for (int i = 0; i < _countof(columnId); ++i)
			if (columnId[i] != COLUMN_PROGRESS)
				copyMenu.AppendMenu(MF_STRING, IDC_COPY + columnId[i], CTSTRING_I(columnNames[i]));
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
	}
	initPriorityMenu();
}

void QueueFrame::destroyMenus()
{
	if (browseMenu)
	{
		MenuHelper::removeStaticMenu(browseMenu);
		browseMenu.DestroyMenu();
	}
	if (removeMenu)
	{
		MenuHelper::removeStaticMenu(removeMenu);
		removeMenu.DestroyMenu();
	}
	if (removeAllMenu)
	{
		MenuHelper::removeStaticMenu(removeAllMenu);
		removeAllMenu.DestroyMenu();
	}
	if (pmMenu)
	{
		MenuHelper::removeStaticMenu(pmMenu);
		pmMenu.DestroyMenu();
	}
	if (readdMenu)
	{
		MenuHelper::removeStaticMenu(readdMenu);
		readdMenu.DestroyMenu();
	}
	if (segmentsMenu)
	{
		MenuHelper::removeStaticMenu(segmentsMenu);
		segmentsMenu.DestroyMenu();
	}
	if (priorityMenu)
	{
		MenuHelper::removeStaticMenu(priorityMenu);
		priorityMenu.DestroyMenu();
	}
	if (copyMenu)
	{
		MenuHelper::removeStaticMenu(copyMenu);
		copyMenu.DestroyMenu();
	}
}

void QueueFrame::QueueItemInfo::updateIconIndex()
{
	if (iconIndex < 0)
	{
		if (qi)
			iconIndex = g_fileImage.getIconIndex(qi->getTarget());
		else
			iconIndex = FileImage::DIR_ICON;
	}
}

int QueueFrame::QueueItemInfo::compareItems(const QueueItemInfo* a, const QueueItemInfo* b, int col, int /*flags*/)
{
	if (a->type != b->type)
		return (int) a->type - (int) b->type;
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
			return compare(a->qi ? a->qi->getAdded() : 0, b->qi ? b->qi->getAdded() : 0);
		case COLUMN_TTH:
			return compare(a->getTTH().toBase32(), b->getTTH().toBase32());
		case COLUMN_SPEED:
			return compare(a->qi ? a->qi->getAverageSpeed() : 0, b->qi ? b->qi->getAverageSpeed() : 0);
		default:
			return Util::defaultSort(a->getText(col), b->getText(col));
	}
}

tstring QueueFrame::QueueItemInfo::getText(int col) const
{
	switch (col)
	{
		case COLUMN_TARGET:
			if (dir)
				return Text::toT(dir->name);
			if (qi)
			{
				if (qi->getFlags() & QueueItem::FLAG_PARTIAL_LIST)
				{
					qi->lockAttributes();
					string tempTarget = qi->getTempTargetL();
					qi->unlockAttributes();
					tstring str = Text::toT(tempTarget);
					Util::toNativePathSeparators(str);
					if (str.empty() || str[0] != _T(PATH_SEPARATOR))
						str.insert(0, 1, _T(PATH_SEPARATOR));
					return str;
				}
				return Text::toT(Util::getFileName(qi->getTarget()));
			}
			break;
		case COLUMN_TYPE:
			if (dir)
				return TSTRING(DIRECTORY);
			if (qi)
			{
				auto flags = qi->getFlags();
				if (flags & QueueItem::FLAG_USER_GET_IP)
					return TSTRING(CONNECTION_CHECK);
				if (flags & QueueItem::FLAG_USER_LIST)
					return (flags & QueueItem::FLAG_PARTIAL_LIST) ? TSTRING(PARTIAL_FILE_LIST) : TSTRING(FILE_LIST);
				return Text::toT(Util::getFileExtWithoutDot(qi->getTarget()));
			}
			break;
		case COLUMN_STATUS:
		{
			if (qi)
			{
				if (qi->isFinished()) return TSTRING(DOWNLOAD_FINISHED_IDLE);
				if (qi->isWaiting())
				{
					if (onlineSourcesCount)
					{
						if (sourcesCount == 1)
							return TSTRING(WAITING_USER_ONLINE);
						return TSTRING_F(WAITING_USERS_ONLINE_FMT, onlineSourcesCount % sourcesCount);
					}
					else
					{
						switch (sourcesCount)
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
								return TSTRING_F(ALL_USERS_OFFLINE_FMT, sourcesCount);
						}
					}
				}
				else
				{
					if (onlineSourcesCount == 1)
						return TSTRING(USER_ONLINE);
					return TSTRING_F(USERS_ONLINE_FMT, onlineSourcesCount % sourcesCount);
				}
			}
			break;
		}
		case COLUMN_SEGMENTS:
		{
			if (!qi) break;
			qi->lockAttributes();
			auto maxSegments = qi->getMaxSegmentsL();
			qi->unlockAttributes();
			return Util::toStringT(qi->getDownloadsSegmentCount()) + _T('/') + Util::toStringT(maxSegments);
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
			if (!qi) break;

			qi->lockAttributes();
			auto prio = qi->getPriorityL();
			auto extraFlags = qi->getExtraFlagsL();
			qi->unlockAttributes();

			tstring priority;
			switch (prio)
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
			if (extraFlags & QueueItem::XFLAG_AUTO_PRIORITY)
				priority += _T(" (") + TSTRING(AUTO) + _T(')');
			return priority;
		}
		case COLUMN_USERS:
		{
			if (!qi) break;
			tstring tmp;
			{
				QueueRLock(*QueueItem::g_cs);
				const auto& sources = qi->getSourcesL();
				for (auto j = sources.cbegin(); j != sources.cend(); ++j)
				{
					if (!tmp.empty())
						tmp += _T(", ");
					tstring nick = getSourceName(j->first);
					checkSourceFlags(nick, j->second.getFlags());
					tmp += nick;
				}
			}
			return tmp.empty() ? TSTRING(NO_USERS) : tmp;
		}
		case COLUMN_PATH:
		{
			if (dir)
				return Text::toT(getFullPath(dir));
			if (qi)
			{
				if (qi->getFlags() & QueueItem::FLAG_PARTIAL_LIST)
				{
					string target = qi->getTarget();
					qi->lockAttributes();
					const string dirName = qi->getTempTargetL();
					qi->unlockAttributes();
					if (dirName.length() + 1 < target.length())
					{
						size_t pos = target.length() - (dirName.length() + 1);
						if (target[pos] == ':') target.erase(pos);
					}
					return Text::toT(Util::getFilePath(target));
				}
				return Text::toT(Util::getFilePath(qi->getTarget()));
			}
			break;
		}
		case COLUMN_EXACT_SIZE:
		{
			return getSize() == -1 ? TSTRING(UNKNOWN) : Util::formatExactSizeT(getSize());
		}
		case COLUMN_SPEED:
		{
			if (!qi) break;
			return Util::formatBytesT(qi->getAverageSpeed()) + _T('/') + TSTRING(S);
		}
		case COLUMN_ERRORS:
		{
			if (!qi) break;
			tstring tmp;
			{
				QueueRLock(*QueueItem::g_cs);
				const auto& badSources = qi->getBadSourcesL();
				for (auto j = badSources.cbegin(); j != badSources.cend(); ++j)
				{
					if (!j->second.isAnySet(QueueItem::Source::FLAG_REMOVED))
					{
						if (!tmp.empty())
							tmp += _T(", ");
						tstring nick = getSourceName(j->first);
						checkSourceFlags(nick, j->second.getFlags());
						tmp += nick;
						const tstring& error = getErrorText(j->second.getFlags());
						if (!error.empty())
						{
							tmp += _T(" (");
							tmp += error;
							tmp += _T(')');
						}
					}
				}
			}
			return tmp.empty() ? TSTRING(NO_ERRORS) : tmp;
		}
		case COLUMN_ADDED:
		{
			if (!qi) break;
			return Text::toT(Util::formatDateTime(qi->getAdded()));
		}
		case COLUMN_TTH:
		{
			if (qi && !(qi->getFlags() & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)))
				return Text::toT(qi->getTTH().toBase32());
			break;
		}
	}
	return Util::emptyStringT;
}

bool QueueFrame::QueueItemInfo::updateCachedInfo()
{
	dcassert(qi);
	uint32_t sourcesVersion = qi->getSourcesVersion();
	if (sourcesVersion == version) return false;
	QueueRLock(*QueueItem::g_cs);
	sourcesCount = (uint16_t) std::min(qi->getSourcesL().size(), (size_t) UINT16_MAX);
	onlineSourcesCount = (uint16_t) std::min(qi->getOnlineSourceCountL(), (size_t) UINT16_MAX);
	version = sourcesVersion;
	return true;
}

void QueueFrame::on(QueueManagerListener::Added, const QueueItemPtr& qi) noexcept
{
	addTask(ADD_ITEM, new QueueItemTask(qi));
}

void QueueFrame::on(QueueManagerListener::AddedArray, const vector<QueueItemPtr>& data) noexcept
{
	if (!GlobalState::isShuttingDown())
		addTask(ADD_ITEM_ARRAY, new QueueItemArrayTask(data));
}

void QueueFrame::insertListItem(const QueueItemPtr& qi, const string& dirname, bool sort)
{
	size_t subdirLen = 0;
	if (!showTree || isInsideCurrentDir(dirname, subdirLen))
	{
		if (!subdirLen)
		{
			QueueItemInfo* ii = new QueueItemInfo(qi);
			if (sort)
				ctrlQueue.insertItem(ii, I_IMAGECALLBACK);
			else
				ctrlQueue.insertItem(ctrlQueue.GetItemCount(), ii, I_IMAGECALLBACK);
		}
		else
		{
			string subdir = dirname.substr(currentDirPath.length(), subdirLen);
			int count = ctrlQueue.GetItemCount();
			for (int i = 0; i < count; ++i)
			{
				QueueItemInfo* ii = ctrlQueue.getItemData(i);
				const DirItem* dirItem = ii->getDirItem();
				if (dirItem && !stricmp(dirItem->name, subdir))
				{
					ctrlQueue.updateItem(i, COLUMN_SIZE);
					break;
				}
			}
		}
	}
}

static string::size_type findCommonSubdir(const string& a, const string& b)
{
	string::size_type pos = 0;
	string::size_type result = 0;
	while (pos < a.length())
	{
		string::size_type next = a.find(PATH_SEPARATOR, pos);
		if (next == string::npos || next >= b.length() || b[next] != PATH_SEPARATOR) break;
		if (strnicmp(a.c_str() + pos, b.c_str() + pos, next - pos)) break;
		result = next;
		pos = next + 1;
	}
	return result;
}

void QueueFrame::addQueueItem(const QueueItemPtr& qi, bool sort, bool updateTree)
{
	dcassert(!closed);
	string target = qi->getTarget();
	if (qi->getFlags() & QueueItem::FLAG_PARTIAL_LIST)
	{
		qi->lockAttributes();
		const string dirName = qi->getTempTargetL();
		qi->unlockAttributes();
		dcassert(dirName.length() + 1 < target.length());
		if (dirName.length() + 1 < target.length())
		{
			size_t pos = target.length() - (dirName.length() + 1);
			dcassert(target[pos] == ':' && target.substr(pos + 1) == dirName);
			if (target[pos] == ':') target.erase(pos);
		}
	}
	const string dirname = Util::getFilePath(target);
	if (dirname.length() == target.length())
	{
		dcassert(0);
		return;
	}

	updateStatus = true;
	const string filename = target.substr(dirname.length());
	if (qi->isUserList())
	{
		if (!fileLists)
		{
			fileLists = new DirItem;
			fileLists->name = dirname;
			if (updateTree)
				insertFileListsItem();
		}
		addItem(fileLists, dirname, dirname.length(), filename, qi, false);
		insertListItem(qi, dirname, sort);
		return;
	}

#if defined(DEBUG_QUEUE_FRAME) && defined(DEBUG_QUEUE_FRAME_TREE)
	LogManager::message("Adding subdir: " + dirname, false);
#endif

	size_t minLen = std::numeric_limits<size_t>::max();
	vector<DirItem*> toSplit;
	bool inserted = false;
	for (DirItem* item : root->directories)
	{
		size_t subdirLen = findCommonSubdir(dirname, item->name);
		if (subdirLen)
		{
			if (subdirLen >= item->name.length()-1)
			{
				addItem(item, dirname, subdirLen + 1, filename, qi, updateTree);
				inserted = true;
				break;
			}
			toSplit.push_back(item);
			if (subdirLen < minLen) minLen = subdirLen;
		}
	}
	if (!inserted)
	{
		if (!toSplit.empty())
		{
			DirItem* newParent = toSplit[0];
			for (DirItem* item : toSplit)
			{
				splitDir(item, minLen, newParent, updateTree);
				if (item != newParent)
				{
					removeFromParent(item);
					ctrlDirs.DeleteItem(item->ht);
				}
				else
					addItem(item, dirname, minLen + 1, filename, qi, updateTree);
			}
		}
		else
			addItem(root, dirname, dirname.length(), filename, qi, updateTree);
	}
	insertListItem(qi, dirname, sort);
}

void QueueFrame::splitDir(DirItem* dir, string::size_type pos, DirItem* newParent, bool updateTree)
{
	dcassert(pos && pos < dir->name.length()-1 && dir->name[pos] == PATH_SEPARATOR);	
#if defined(DEBUG_QUEUE_FRAME) && defined(DEBUG_QUEUE_FRAME_TREE)
	LogManager::message("Split " + dir->name + " at " + Util::toString(pos), false);
#endif

	if (dir->ht)
	{
		ctrlDirs.DeleteItem(dir->ht);
		dir->ht = nullptr;
	}
	
	list<DirItem*> oldDirs = std::move(dir->directories);
	list<QueueItemPtr> oldFiles = std::move(dir->files);
	int64_t totalSize = dir->totalSize;
	size_t totalFileCount = dir->totalFileCount;
	dir->directories.clear();
	dir->files.clear();

	DirItem* first = nullptr;
	if (dir == newParent) first = dir;

	DirItem* last = newParent;
	string::size_type start = pos + 1;
	while (start < dir->name.length())
	{
		string::size_type end = dir->name.find(PATH_SEPARATOR, start);
		if (end == string::npos) break;
		DirItem* newDir = new DirItem;
		if (!first) first = newDir;
		newDir->name = dir->name.substr(start, end - start);
		newDir->parent = last;
		newDir->totalSize = totalSize;
		newDir->totalFileCount = totalFileCount;
		last->directories.push_back(newDir);
		last = newDir;
		start = end + 1;
	}

	last->directories = std::move(oldDirs);
	for (DirItem* dirItem : last->directories)
		dirItem->parent = last;
	last->files = std::move(oldFiles);

	if (dir != newParent)
	{
		removeFromParent(dir);
		deleteDirItem(dir);		
		newParent->totalSize += totalSize;
		newParent->totalFileCount += totalFileCount;
	}
	else
		dir->name.erase(pos + 1);

	if (updateTree)
		insertSubtree(first, first->parent->ht);
}

void QueueFrame::addItem(DirItem* dir, const string& path, string::size_type pathStart, const string& filename, const QueueItemPtr& qi, bool updateTree)
{
	DirItem* last = dir;
	while (pathStart < path.length())
	{
		string::size_type pos = path.find(PATH_SEPARATOR, pathStart);
		if (pos == string::npos) break;
		string name = path.substr(pathStart, pos - pathStart);
		list<DirItem*>& dirs = last->directories;
		bool found = false;
		for (auto i = dirs.begin(); i != dirs.end(); ++i)
		{
			DirItem* item = *i;
			if (item->name.length() == name.length() && !stricmp(item->name, name))
			{
				last = item;
				found = true;
				break;
			}
		}
		if (!found)
		{
			DirItem* newDir = new DirItem;
			newDir->name = std::move(name);
			newDir->parent = last;
			last->directories.push_back(newDir);
			last = newDir;
			if (updateTree)
				insertTreeItem(last, last->parent->ht, TVI_SORT);
		}
		pathStart = pos + 1;
	}
	if (last == root)
	{
		DirItem* newDir = new DirItem;
		newDir->name = path;
		newDir->parent = root;
		root->directories.push_back(newDir);
		last = newDir;
		if (updateTree)
			insertTreeItem(last, last->parent->ht, TVI_SORT);
	}
	last->files.push_back(qi);
	int64_t size = qi->getSize();
	if (size < 0) size = 0;
	while (last)
	{
		last->totalSize += size;
		last->totalFileCount++;
		last = last->parent;
	}
}

void QueueFrame::insertTreeItem(DirItem* dir, HTREEITEM htParent, HTREEITEM htAfter)
{
	tstring text = Text::toT(dir->name);
	Util::removePathSeparator(text);
	TVINSERTSTRUCT tvi = {0};
	tvi.hInsertAfter = htAfter;
	tvi.item.mask = TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_TEXT;
	tvi.item.iImage = tvi.item.iSelectedImage = FileImage::DIR_ICON;
	tvi.hParent = htParent;
	tvi.item.pszText = const_cast<TCHAR*>(text.c_str());
	tvi.item.lParam = reinterpret_cast<LPARAM>(dir);
	dir->ht = ctrlDirs.InsertItem(&tvi);
}

void QueueFrame::insertFileListsItem()
{
	TVINSERTSTRUCT tvi = {0};
	tvi.hInsertAfter = TVI_FIRST;
	tvi.item.mask = TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_TEXT;
	tvi.item.iImage = tvi.item.iSelectedImage = FileImage::DIR_ICON;
	tvi.hParent = TVI_ROOT;
	tvi.item.pszText = const_cast<TCHAR*>(CTSTRING(FILE_LISTS));
	tvi.item.lParam = reinterpret_cast<LPARAM>(fileLists);
	fileLists->ht = ctrlDirs.InsertItem(&tvi);
}

string QueueFrame::getFullPath(const DirItem* dir)
{
	string result;
	while (dir && !dir->name.empty())
	{
		if (dir->name.back() != PATH_SEPARATOR) result.insert(0, PATH_SEPARATOR_STR);
		result.insert(0, dir->name);
		dir = dir->parent;
	}
	return result;
}

void QueueFrame::removeFromParent(DirItem* dir)
{
	if (!dir->parent) return;
	auto& dirs = dir->parent->directories;
	for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
		if (*i == dir)
		{
			dirs.erase(i);
			break;
		}
}

void QueueFrame::removeEmptyDir(DirItem* dir)
{
	if (dir == fileLists)
	{
		if (dir->directories.empty() && dir->files.empty())
		{
			if (dir->ht) ctrlDirs.DeleteItem(dir->ht);
			fileLists = nullptr;
			deleteDirItem(dir);
		}
	}
	else
	{
		while (dir->directories.empty() && dir->files.empty())
		{
			DirItem* parent = dir->parent;
			if (!parent) break;
			removeFromParent(dir);
			if (dir->ht) ctrlDirs.DeleteItem(dir->ht);
#ifdef DEBUG_QUEUE_FRAME
			LogManager::message("Remove empty dir: " + getFullPath(dir), false);
#endif
			deleteDirItem(dir);
			dir = parent;
		}
	}
}

void QueueFrame::deleteDirItem(DirItem* dir)
{
	if (dir == currentDir)
	{
		currentDir = nullptr;
		currentDirPath.clear();
	}
	if (dir->parent == currentDir)
	{
		int count = ctrlQueue.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getDirItem() == dir)
			{
				ctrlQueue.DeleteItem(i);
				delete ii;
				break;
			}
		}
	}
	delete dir;
}

void QueueFrame::insertSubtree(DirItem* dir, HTREEITEM htParent)
{
	// FIXME: Use TVI_LAST and sort items later
	insertTreeItem(dir, htParent, TVI_SORT);
	for (DirItem* item : dir->directories)
		insertSubtree(item, dir->ht);
}

void QueueFrame::deleteTree(DirItem* dir)
{
	if (!dir) return;
	for (DirItem* item : dir->directories)
		deleteTree(item);
	delete dir;
}

void QueueFrame::insertTrees()
{
	if (fileLists)
		insertFileListsItem();
	for (DirItem* item : root->directories)
		insertSubtree(item, TVI_ROOT);
	treeInserted = true;
}

bool QueueFrame::findItem(const QueueItemPtr& qi, QueueItem::MaskType flags, const string& path, DirItem* &dir, bool remove)
{
	string::size_type last = path.rfind(PATH_SEPARATOR);
	if (last == string::npos)
	{
		dcassert(0);
		return false;
	}
	// Must match QueueItem::isUserList
	if (flags & (QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		for (auto i = fileLists->files.begin(); i != fileLists->files.end(); ++i)
		{
			const string& target = (*i)->getTarget();
			if (target.length() == path.length() && !stricmp(target, path))
			{
				dir = fileLists;
				if (remove) dir->files.erase(i);
				return true;
			}
		}
		return false;
	}
	DirItem* item = nullptr;
	for (DirItem* firstItem : root->directories)
	{
		size_t len = findCommonSubdir(firstItem->name, path);
		if (len == firstItem->name.length()-1)
		{
			item = firstItem;
			break;
		}
	}
	if (!item) return false;
	string::size_type start = item->name.length();
	while (start < last)
	{
		string::size_type end = path.find(PATH_SEPARATOR, start);
		if (end == string::npos) return false;
		size_t len = end - start;
		DirItem* nextItem = nullptr;
		for (DirItem* tmp : item->directories)
			if (tmp->name.length() == len && !strnicmp(tmp->name.c_str(), path.c_str() + start, len))
			{
				nextItem = tmp;
				break;
			}
		if (!nextItem) return false;
		item = nextItem;
		start = end + 1;
	}
	if (last == path.length()-1) return false;
	dir = item;
	if (qi)
	{
		for (auto i = item->files.begin(); i != item->files.end(); ++i)
			if ((*i) == qi)
			{
				if (remove) dir->files.erase(i);
				return true;
			}
	}
	else
	{
		for (auto i = item->files.begin(); i != item->files.end(); ++i)
		{
			const string& target = (*i)->getTarget();
			if (target.length() == path.length() && !stricmp(target, path))
			{
				if (remove) dir->files.erase(i);
				return true;
			}
		}
	}
	return false;
}

bool QueueFrame::removeItem(const QueueItemPtr& qi)
{
	DirItem* dir;
	if (!findItem(QueueItemPtr(), qi->getFlags(), qi->getTarget(), dir, true)) return false;
	updateStatus = true;
	int64_t size = qi->getSize();
	if (size < 0) size = 0;
	DirItem* item = dir;
	const DirItem* foundSubdir = nullptr;
	while (item)
	{
		item->totalSize -= size;
		dcassert(item->totalSize >= 0);
		item->totalFileCount--;
		DirItem* prev = item;
		item = item->parent;
		if (item == currentDir) foundSubdir = prev;
	}
	if (dir == currentDir || !showTree)
	{
		int count = ctrlQueue.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getQueueItem() == qi)
			{
				ctrlQueue.DeleteItem(i);
				delete ii;
				break;
			}
		}
	}
	else if (foundSubdir && foundSubdir->totalSize)
	{
		int count = ctrlQueue.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getDirItem() == foundSubdir)
			{
				ctrlQueue.updateItem(i, COLUMN_SIZE);
				break;
			}
		}
	}
	removeEmptyDir(dir);
	return true;
}

bool QueueFrame::updateItemSize(const QueueItemPtr& qi, int64_t diff)
{
	DirItem* dir;
	if (!findItem(QueueItemPtr(), qi->getFlags(), qi->getTarget(), dir, false)) return false;
	updateStatus = true;
	DirItem* item = dir;
	const DirItem* foundSubdir = nullptr;
	while (item)
	{
		item->totalSize += diff;
		dcassert(item->totalSize >= 0);
		DirItem* prev = item;
		item = item->parent;
		if (item == currentDir) foundSubdir = prev;
	}
	if (dir == currentDir)
	{
		int count = ctrlQueue.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getQueueItem() == qi)
			{
				ctrlQueue.updateItem(i, COLUMN_SIZE);
				break;
			}
		}
	}
	else if (foundSubdir)
	{
		int count = ctrlQueue.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getDirItem() == foundSubdir)
			{
				ctrlQueue.updateItem(i, COLUMN_SIZE);
				break;
			}
		}
	}
	return true;
}

void QueueFrame::walkDirItem(const QueueFrame::DirItem* dir, std::function<void(const QueueItemPtr&)> func)
{
	for (const QueueItemPtr& qi : dir->files)
		func(qi);
	for (const DirItem* dirItem : dir->directories)
		walkDirItem(dirItem, func);
}

bool QueueFrame::isCurrentDir(const string& target) const
{
	if (currentDirPath.empty()) return false;
	string::size_type pos = target.rfind(PATH_SEPARATOR);
	return pos == currentDirPath.length()-1 && !strnicmp(currentDirPath.c_str(), target.c_str(), pos + 1);
}

bool QueueFrame::isInsideCurrentDir(const string& target, size_t& subdirLen) const
{
	if (currentDirPath.empty() || target.length() < currentDirPath.length()) return false;
	if (strnicmp(currentDirPath.c_str(), target.c_str(), currentDirPath.length())) return false;
	
	string::size_type pos = target.find(PATH_SEPARATOR, currentDirPath.length());
	if (pos != string::npos)
		subdirLen = pos - currentDirPath.length();
	else
		subdirLen = 0;
	return true;
}

void QueueFrame::addQueueList()
{
	CLockRedraw<> lockRedraw(ctrlQueue);
	CLockRedraw<true> lockRedrawDir(ctrlDirs);
	{
		QueueManager::LockFileQueueShared fileQueue;
		const auto& li = fileQueue.getQueueL();
		for (auto j = li.cbegin(); j != li.cend(); ++j)
			addQueueItem(j->second, false, false);
	}
	if (showTree)
		insertTrees();
	ctrlQueue.resort();
}

void QueueFrame::on(QueueManagerListener::Removed, const QueueItemPtr& qi) noexcept
{
	if (!GlobalState::isShuttingDown())
		addTask(REMOVE_ITEM, new QueueItemTask(qi));
}

void QueueFrame::on(QueueManagerListener::RemovedArray, const vector<QueueItemPtr>& data) noexcept
{
	if (!GlobalState::isShuttingDown())
		addTask(REMOVE_ITEM_ARRAY, new QueueItemArrayTask(data));
}

void QueueFrame::on(QueueManagerListener::Moved, const QueueItemPtr& qs, const QueueItemPtr& qt) noexcept
{
	addTask(REMOVE_ITEM, new QueueItemTask(qs));
	addTask(ADD_ITEM, new QueueItemTask(qt));
}

void QueueFrame::on(QueueManagerListener::FileSizeUpdated, const QueueItemPtr& qi, int64_t diff) noexcept
{
	addTask(UPDATE_FILE_SIZE, new UpdateFileSizeTask(qi, diff));
}

void QueueFrame::on(QueueManagerListener::StatusUpdated, const QueueItemPtr& qi) noexcept
{
	dcassert(!GlobalState::isShuttingDown());
	if (!GlobalState::isShuttingDown())
		addTask(UPDATE_ITEM, new TargetTask(qi->getTarget()));
}

void QueueFrame::on(QueueManagerListener::StatusUpdatedList, const QueueItemList& itemList) noexcept
{
	dcassert(!GlobalState::isShuttingDown());
	if (!GlobalState::isShuttingDown())
	{
		for (auto i = itemList.cbegin(); i != itemList.cend(); ++i)
			on(QueueManagerListener::StatusUpdated(), *i);
	}
}

void QueueFrame::addTask(Tasks s, Task* task)
{
	bool firstItem;
	uint64_t tick = GET_TICK();
	uint64_t prevTick = tick;
	if (tasks.add(s, task, firstItem, prevTick) && prevTick + TIMER_VAL < tick)
		PostMessage(WM_SPEAKER); // FAILED?
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
				const auto& it = static_cast<QueueItemTask&>(*ti->second);
#ifdef DEBUG_QUEUE_FRAME
				LogManager::message("Add item " + it.qi->getTTH().toBase32() + " to " + it.qi->getTarget(), false);
#endif
				addQueueItem(it.qi, true, showTree);
			}
			break;
			case ADD_ITEM_ARRAY:
			{
				const auto& task = static_cast<QueueItemArrayTask&>(*ti->second);
				int prevCount = ctrlQueue.GetItemCount();
				CLockRedraw<> lockRedraw(ctrlQueue);
				CLockRedraw<true> lockRedrawDir(ctrlDirs);
				for (const QueueItemPtr& qi : task.data)
				{
#ifdef DEBUG_QUEUE_FRAME
					LogManager::message("Add item " + qi->getTTH().toBase32() + " to " + qi->getTarget(), false);
#endif
					addQueueItem(qi, false, showTree);
				}
				if (ctrlQueue.GetItemCount() != prevCount)
					ctrlQueue.resort();
			}
			break;
			case REMOVE_ITEM:
			{
				const auto& task = static_cast<QueueItemTask&>(*ti->second);
#ifdef DEBUG_QUEUE_FRAME
				LogManager::message("Remove item " + task.qi->getTTH().toBase32() + " at " + task.qi->getTarget(), false);
#endif
				removeItem(task.qi);
			}
			break;
			case REMOVE_ITEM_ARRAY:
			{
				const auto& task = static_cast<QueueItemArrayTask&>(*ti->second);
				CLockRedraw<> lockRedraw(ctrlQueue);
				CLockRedraw<true> lockRedrawDir(ctrlDirs);
				for (const QueueItemPtr& qi : task.data)
				{
#ifdef DEBUG_QUEUE_FRAME
					LogManager::message("Remove item " + qi->getTTH().toBase32() + " at " + qi->getTarget(), false);
#endif
					removeItem(qi);
				}
			}
			break;
			case UPDATE_ITEM:
			{
				const auto& task = static_cast<TargetTask&>(*ti->second);
				const string& target = task.target;
				if (!showTree || isCurrentDir(target))
				{
					int count = ctrlQueue.GetItemCount();
					for (int pos = 0; pos < count; ++pos)
					{
						const QueueItemPtr& qi = ctrlQueue.getItemData(pos)->getQueueItem();
						if (qi && qi->getTarget() == target)
						{
							ctrlQueue.updateItem(pos, COLUMN_SEGMENTS);
							ctrlQueue.updateItem(pos, COLUMN_PROGRESS);
							ctrlQueue.updateItem(pos, COLUMN_PRIORITY);
							ctrlQueue.updateItem(pos, COLUMN_USERS);
							ctrlQueue.updateItem(pos, COLUMN_ERRORS);
							ctrlQueue.updateItem(pos, COLUMN_STATUS);
							ctrlQueue.updateItem(pos, COLUMN_DOWNLOADED);
							ctrlQueue.updateItem(pos, COLUMN_SPEED);
							break;
						}
					}
				}
			}
			break;
			case UPDATE_STATUS:
			{
				auto& status = static_cast<StringTask&>(*ti->second);
				ctrlStatus.setPaneText(STATUS_TEXT, Text::toT(status.str));
			}
			break;
			case UPDATE_FILE_SIZE:
			{
				const auto& task = static_cast<UpdateFileSizeTask&>(*ti->second);
				updateItemSize(task.qi, task.diff);
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
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::CONFIRM_DELETE))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return false;
		if (checkState == BST_CHECKED)
			ss->setBool(Conf::CONFIRM_DELETE, false);
	}
	return true;
}

void QueueFrame::removeSelected()
{
	if (confirmDelete())
	{
		vector<QueueItemPtr> targets;
		auto func = [&targets](const QueueItemPtr& qi) { targets.push_back(qi); };
		CWaitCursor waitCursor;

		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getDirItem())
				walkDirItem(ii->getDirItem(), func);
			else
			{
				const QueueItemPtr& qi = ii->getQueueItem();
				if (qi) targets.push_back(qi);
			}
		}
		
		auto qm = QueueManager::getInstance();
		if (targets.size() > 1) qm->startBatch();
		for (const QueueItemPtr& qi : targets)
			qm->removeTarget(qi->getTarget());
		if (targets.size() > 1) qm->endBatch();
	}
}

void QueueFrame::removeAllDir()
{
	if (ctrlDirs.GetSelectedItem() && confirmDelete())
	{
		QueueManager::getInstance()->removeAll();
		
		clearingTree++;
		ctrlDirs.DeleteAllItems();
		clearingTree--;
		ctrlQueue.deleteAllNoLock();

		for (DirItem* dir : root->directories)
			deleteTree(dir);
		root->directories.clear();
		if (fileLists)
		{
			deleteTree(fileLists);
			fileLists = nullptr;
		}
	}
}

void QueueFrame::removeSelectedDir()
{
	HTREEITEM ht = ctrlDirs.GetSelectedItem();
	if (ht && confirmDelete())
	{
		vector<QueueItemPtr> targets;
		auto func = [&targets](const QueueItemPtr& qi) { targets.push_back(qi); };
		CWaitCursor waitCursor;
		walkDirItem(reinterpret_cast<DirItem*>(ctrlDirs.GetItemData(ht)), func);

		auto qm = QueueManager::getInstance();
		if (targets.size() > 1) qm->startBatch();
		for (const QueueItemPtr& qi : targets)
			qm->removeTarget(qi->getTarget());
		if (targets.size() > 1) qm->endBatch();
	}
}

void QueueFrame::moveDir(const DirItem* dir, const string& newPath)
{
	string oldPath = getFullPath(dir);
	auto func = [this, &oldPath, &newPath](const QueueItemPtr& qi)
	{
		string target = qi->getTarget();
		dcassert(oldPath.length() < target.length() && target[oldPath.length()-1] == PATH_SEPARATOR);
		target.erase(0, oldPath.length());
		target.insert(0, newPath);
		itemsToMove.emplace_back(qi, target);
	};
	walkDirItem(dir, func);
}

void QueueFrame::moveSelected()
{
	tstring name;
	if (showTree && !currentDirPath.empty())
		name = Text::toT(currentDirPath);
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		const string toDir = Text::fromT(name);
		if (toDir.empty()) return;
		
		int j = -1;
		while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(j);
			const DirItem* dir = ii->getDirItem();
			if (dir)
			{
				string newPath = getFullPath(dir);
				Util::removePathSeparator(newPath);
				string::size_type pos = newPath.rfind(PATH_SEPARATOR);
				dcassert(pos != string::npos);
				if (pos != string::npos)
				{
					newPath.erase(0, pos + 1);
					newPath.insert(0, toDir);
					newPath += PATH_SEPARATOR;
					moveDir(dir, newPath);
				}
			}
			else
			{
				const QueueItemPtr& qi = ii->getQueueItem();
				if (qi)
					itemsToMove.emplace_back(qi, toDir + Util::getFileName(qi->getTarget()));
			}
		}
		moveTempArray();
	}
}

void QueueFrame::moveTempArray()
{
	auto qm = QueueManager::getInstance();
	for (auto& item : itemsToMove)
	{
#ifdef DEBUG_QUEUE_FRAME
		LogManager::message("Move " + item.first->getTarget() + " to " + item.second, false);
#endif
		qm->move(item.first->getTarget(), item.second);
	}
	itemsToMove.clear();
}

void QueueFrame::moveSelectedDir()
{
	HTREEITEM ht = ctrlDirs.GetSelectedItem();
	if (!ht) return;
		
	tstring name;
	if (currentDirPath.empty())
		name = Text::toT(currentDirPath);
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		const string toDir = Text::fromT(name);
		if (toDir.empty()) return;

		const DirItem* dirItem = reinterpret_cast<DirItem*>(ctrlDirs.GetItemData(ht));
		if (!dirItem || dirItem == fileLists) return;
		moveDir(dirItem, toDir);
		moveTempArray();
	}
}

void QueueFrame::renameSelected()
{
	// Single file, get the full filename and move...
	const QueueItemInfo* ii = getSelectedQueueItem();
	const QueueItemPtr& qi = ii->getQueueItem();
	if (!qi) return;
	const tstring target = Text::toT(qi->getTarget());
	const tstring filename = Util::getFileName(target);
	
	LineDlg dlg;
	dlg.title = TSTRING(RENAME);
	dlg.description = TSTRING(FILENAME);
	dlg.line = filename;
	dlg.allowEmpty = false;
	if (dlg.DoModal(m_hWnd) != IDOK) return;

	string newFilename = Text::fromT(dlg.line);
	if (!Util::isValidFileName(newFilename, false))
	{
		MessageBox(CTSTRING(INVALID_FILENAME), getAppNameVerT().c_str(), MB_OK | MB_ICONWARNING);
		return;
	}
	string path = Text::fromT(target.substr(0, target.length() - filename.length()));
	QueueManager::getInstance()->move(qi->getTarget(), path + newFilename);
}

void QueueFrame::renameSelectedDir()
{
	HTREEITEM ht = ctrlDirs.GetSelectedItem();
	if (!ht || currentDirPath.empty()) return;
		
	const string lname = Util::getLastDir(currentDirPath);
	LineDlg dlg;
	dlg.description = TSTRING(DIRECTORY);
	dlg.title = TSTRING(RENAME);
	dlg.line = Text::toT(lname);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		string newName = Text::fromT(dlg.line);
		if (newName.empty()) return;
		const DirItem* dir = reinterpret_cast<DirItem*>(ctrlDirs.GetItemData(ht));
		if (!dir || dir == fileLists) return;

		string newPath = currentDirPath;
		Util::removePathSeparator(newPath);
		string::size_type pos = newPath.rfind(PATH_SEPARATOR);
		dcassert(pos != string::npos);
		if (pos == string::npos) return;
		newPath.erase(pos + 1);
		newPath += newName;
		newPath += PATH_SEPARATOR;
		moveDir(dir, newPath);
		moveTempArray();
	}
}

LRESULT QueueFrame::onDoubleClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	const NMITEMACTIVATE* pActivate = reinterpret_cast<const NMITEMACTIVATE*>(pnmh);
	if (pActivate->iItem != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(pActivate->iItem);
		if (ii && ii->getDirItem())
		{
			HTREEITEM ht = ii->getDirItem()->ht;
			dcassert(ht);
			ctrlDirs.SelectItem(ht);
			return 0;
		}
	}
	return onSearchAlternates(BN_CLICKED, (WORD)idCtrl, m_hWnd, bHandled);
}

LRESULT QueueFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlQueue && ctrlQueue.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlQueue, pt);
		}

		int selCount = ctrlQueue.GetSelectedCount();
		if (selCount)
		{
			const QueueItemInfo* ii = getSelectedQueueItem();
			dcassert(ii);
			const QueueItemPtr& qi = ii->getQueueItem();

			usingDirMenu = false;
			createMenus();
			clearPreviewMenu();
			
			if (selCount == 1 && qi)
			{
				bool isFileList = qi->isUserList();

				OMenu singleMenu;
				singleMenu.CreatePopupMenu();
				int indexPreview = -1;
				int indexSegments = -1;
				int indexBrowse = -1;
				if (!isFileList)
				{
					singleMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
					indexPreview = singleMenu.GetMenuItemCount();
					appendPreviewItems(singleMenu);
					indexSegments = indexPreview + 1;
					singleMenu.AppendMenu(MF_POPUP, segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				}
				int indexPriority = singleMenu.GetMenuItemCount();
				singleMenu.AppendMenu(MF_POPUP, priorityMenu, CTSTRING(SET_PRIORITY), g_iconBitmaps.getBitmap(IconBitmaps::PRIORITY, 0));
				if (!isFileList)
				{
					indexBrowse = indexPriority + 1;
					singleMenu.AppendMenu(MF_POPUP, browseMenu, CTSTRING(GET_FILE_LIST), g_iconBitmaps.getBitmap(IconBitmaps::FILELIST, 0));
				}
				int indexPM = singleMenu.GetMenuItemCount();
				singleMenu.AppendMenu(MF_POPUP, pmMenu, CTSTRING(SEND_PRIVATE_MESSAGE), g_iconBitmaps.getBitmap(IconBitmaps::PM, 0));
				singleMenu.AppendMenu(MF_POPUP, readdMenu, CTSTRING(READD_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, copyMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE), g_iconBitmaps.getBitmap(IconBitmaps::MOVE, 0));
				singleMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME), g_iconBitmaps.getBitmap(IconBitmaps::RENAME, 0));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_POPUP, removeMenu, CTSTRING(REMOVE_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, removeAllMenu, CTSTRING(REMOVE_FROM_ALL));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE, 0));
				if (!isFileList)
					singleMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES);
				
				sourcesList.clear();
				pmSourcesList.clear();
				browseSourcesList.clear();
				readdList.clear();

				qi->lockAttributes();
				auto maxSegments = qi->getMaxSegmentsL();
				auto extraFlags = qi->getExtraFlagsL();
				qi->unlockAttributes();

				if (indexSegments != -1)
				{
					segmentsMenu.CheckMenuItem(IDC_SEGMENTONE - 1 + maxSegments, MF_CHECKED);
				}
				if (indexPreview != -1)
				{
					setupPreviewMenu(qi->getTarget());
					bool hasPreview = previewMenu.GetMenuItemCount() > 0 && ii->getDownloadedBytes() > 0;
					singleMenu.EnableMenuItem(indexPreview, MF_BYPOSITION | (hasPreview ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
				}
				{
					QueueRLock(*QueueItem::g_cs);
					const auto& sources = qi->getSourcesL();
					for (auto i = sources.cbegin(); i != sources.cend(); ++i)
					{
						const UserPtr& user = i->first;
						bool hasFileList = DirectoryListingFrame::findFrame(user, false) != nullptr;
						bool isOnline = user->isOnline();
						SourceInfo si{user, i->second.getFlags(), getSourceName(user), hasFileList};
						sourcesList.push_back(si);
						if (hasFileList || isOnline)
							browseSourcesList.push_back(si);
						if (isOnline)
							pmSourcesList.push_back(si);
					}
					const auto& badSources = qi->getBadSourcesL();
					for (auto i = badSources.cbegin(); i != badSources.cend(); ++i)
						readdList.emplace_back(SourceInfo{i->first, i->second.getFlags(), getSourceName(i->first), false});
				}

				sortSources(sourcesList);
				for (size_t i = 0; i < sourcesList.size(); ++i)
				{
					const SourceInfo& si = sourcesList[i];
					tstring nick = si.nick;
					checkSourceFlags(nick, si.flags);
					WinUtil::escapeMenu(nick);
					removeMenu.AppendMenu(MF_STRING, IDC_REMOVE_SOURCE + 1 + i, nick.c_str());
					removeAllMenu.AppendMenu(MF_STRING, IDC_REMOVE_SOURCES + i, nick.c_str());
				}

				sortSources(pmSourcesList);
				for (size_t i = 0; i < pmSourcesList.size(); ++i)
				{
					const SourceInfo& si = pmSourcesList[i];
					tstring nick = si.nick;
					checkSourceFlags(nick, si.flags);
					WinUtil::escapeMenu(nick);
					pmMenu.AppendMenu(MF_STRING, IDC_PM + i, nick.c_str());
				}

				sortSources(browseSourcesList);
				for (size_t i = 0; i < browseSourcesList.size(); ++i)
				{
					const SourceInfo& si = browseSourcesList[i];
					tstring nick = si.nick;
					checkSourceFlags(nick, si.flags);
					WinUtil::escapeMenu(nick);
					browseMenu.AppendMenu(MF_STRING, IDC_BROWSELIST + i, nick.c_str(),
						si.hasFileList ? g_iconBitmaps.getBitmap(IconBitmaps::GOTO_FILELIST, 0) : nullptr);
				}

				sortSources(readdList);
				for (size_t i = 0; i < readdList.size(); ++i)
				{
					const SourceInfo& si = readdList[i];
					tstring nick = si.nick;
					checkSourceFlags(nick, si.flags);
					const tstring& error = getErrorText(si.flags);
					if (!error.empty())
					{
						nick += _T(" (");
						nick += error;
						nick += _T(')');
					}
					WinUtil::escapeMenu(nick);
					readdMenu.AppendMenu(MF_STRING, IDC_READD + 1 + i, nick.c_str());
				}

				if (sourcesList.empty())
				{
					// removeMenu
					singleMenu.EnableMenuItem(indexPM + 7, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					// removeAllMenu
					singleMenu.EnableMenuItem(indexPM + 8, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
				}

				if (browseSourcesList.empty() && indexBrowse != -1)
					singleMenu.EnableMenuItem(indexBrowse, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);

				if (pmSourcesList.empty())
					singleMenu.EnableMenuItem(indexPM, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);

				if (readdList.empty())
					singleMenu.EnableMenuItem(indexPM + 1, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);

				priorityMenu.CheckMenuItem(ii->getPriority(), MF_BYPOSITION | MF_CHECKED);
				if (extraFlags & QueueItem::XFLAG_AUTO_PRIORITY)
					priorityMenu.CheckMenuItem(IDC_AUTOPRIORITY, MF_BYCOMMAND | MF_CHECKED);

				singleMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
				MenuHelper::unlinkStaticMenus(singleMenu);
			}
			else
			{
				OMenu multiMenu;
				multiMenu.CreatePopupMenu();
				multiMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES), g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));
				multiMenu.AppendMenu(MF_POPUP, segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				multiMenu.AppendMenu(MF_POPUP, priorityMenu, CTSTRING(SET_PRIORITY), g_iconBitmaps.getBitmap(IconBitmaps::PRIORITY, 0));
				multiMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE), g_iconBitmaps.getBitmap(IconBitmaps::MOVE, 0));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				multiMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE, 0));
				multiMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES);
				multiMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
				MenuHelper::unlinkStaticMenus(multiMenu);
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

		HTREEITEM ht = ctrlDirs.GetSelectedItem();
		if (!ht) return TRUE;
		const DirItem* dir = reinterpret_cast<DirItem*>(ctrlDirs.GetItemData(ht));

		usingDirMenu = true;
		initPriorityMenu();

		OMenu dirMenu;
		dirMenu.CreatePopupMenu();
		dirMenu.AppendMenu(MF_POPUP, priorityMenu, CTSTRING(SET_PRIORITY), g_iconBitmaps.getBitmap(IconBitmaps::PRIORITY, 0));
		if (dir != fileLists)
		{
			dirMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE), g_iconBitmaps.getBitmap(IconBitmaps::MOVE, 0));
			dirMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME), g_iconBitmaps.getBitmap(IconBitmaps::RENAME, 0));
		}
		dirMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE, 0));

		if (dir != fileLists)
		{
			CMenu copyDirMenu;
			copyDirMenu.CreatePopupMenu();
			copyDirMenu.AppendMenu(MF_STRING, IDC_COPY_FOLDER_NAME, CTSTRING(FOLDERNAME));
			copyDirMenu.AppendMenu(MF_STRING, IDC_COPY_FOLDER_PATH, CTSTRING(FULL_PATH));
			dirMenu.AppendMenu(MF_SEPARATOR);
			dirMenu.AppendMenu(MF_POPUP, copyDirMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
			copyDirMenu.Detach();
		}

		OMenu deleteAllMenu;
		deleteAllMenu.CreatePopupMenu();
		MenuHelper::addStaticMenu(deleteAllMenu);
		deleteAllMenu.AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL_QUEUE), g_iconBitmaps.getBitmap(IconBitmaps::EXCLAMATION, 0));
		dirMenu.AppendMenu(MF_SEPARATOR);
		dirMenu.AppendMenu(MF_POPUP, deleteAllMenu, CTSTRING(REMOVE_ALL));

		dirMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		MenuHelper::unlinkStaticMenus(dirMenu);
		MenuHelper::removeStaticMenu(deleteAllMenu);
		
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT QueueFrame::onRecheck(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	StringList tmp;
	int j = -1;
	auto func = [&tmp](const QueueItemPtr& qi) { tmp.push_back(qi->getTarget()); };
	while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(j);
		const QueueItemPtr& qi = ii->getQueueItem();
		if (ii->getDirItem())
			walkDirItem(ii->getDirItem(), func);
		else if (qi)
			tmp.push_back(qi->getTarget());
	}
	for (const string& target : tmp)
		QueueManager::getInstance()->recheck(target);
	return 0;
}

LRESULT QueueFrame::onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		const QueueItemPtr& qi = ii->getQueueItem();
		if (qi && !qi->getTTH().isZero())
			WinUtil::searchHash(qi->getTTH());
	}
	return 0;
}

LRESULT QueueFrame::onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		const QueueItemPtr& qi = ii->getQueueItem();
		if (qi)
			WinUtil::copyMagnet(qi->getTTH(), Util::getFileName(qi->getTarget()), qi->getSize());
	}
	return 0;
}

LRESULT QueueFrame::onGetList(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		size_t index = wID - IDC_BROWSELIST;
		if (index < browseSourcesList.size())
		{
			UserPtr& user = browseSourcesList[index].user;
			if (browseSourcesList[index].hasFileList)
			{
				DirectoryListingFrame* frame = DirectoryListingFrame::findFrame(user, false);
				if (frame)
				{
					WinUtil::activateMDIChild(frame->m_hWnd);
					int itemIndex = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
					if (itemIndex != -1)
					{
						const QueueItemInfo* ii = ctrlQueue.getItemData(itemIndex);
						const QueueItemPtr& qi = ii->getQueueItem();
						if (qi) frame->selectItem(qi->getTTH());
					}
					return 0;
				}
			}
			try
			{
				QueueManager::getInstance()->addList(HintedUser(user, Util::emptyString), 0, QueueItem::XFLAG_CLIENT_VIEW);
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
		const QueueItemPtr& qi = ii->getQueueItem();
		if (qi)
		{
			if (wID == IDC_READD)
			{
				try
				{
					QueueManager::getInstance()->readdAll(qi);
				}
				catch (const QueueException& e)
				{
					ctrlStatus.setPaneText(STATUS_TEXT, Text::toT(e.getError()));
				}
			}
			else
			{
				size_t index = wID - (IDC_READD + 1);
				if (index < readdList.size())
				{
					try
					{
						QueueManager::getInstance()->readd(qi->getTarget(), readdList[index].user);
					}
					catch (const QueueException& e)
					{
						ctrlStatus.setPaneText(STATUS_TEXT, Text::toT(e.getError()));
					}
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
		const QueueItemPtr& qi = ii->getQueueItem();
		if (qi)
		{
			if (wID == IDC_REMOVE_SOURCE)
			{
				QueueRLock(*QueueItem::g_cs);
				const auto& sources = qi->getSourcesL();
				for (auto si = sources.cbegin(); si != sources.cend(); ++si)
					sourcesToRemove.push_back(std::make_pair(qi->getTarget(), si->first));
			}
			else
			{
				size_t index = wID - (IDC_REMOVE_SOURCE + 1);
				if (index < sourcesList.size())
					sourcesToRemove.push_back(std::make_pair(qi->getTarget(), sourcesList[index].user));
			}
		}
		removeSources();
	}
	return 0;
}

LRESULT QueueFrame::onRemoveSources(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	size_t index = wID - IDC_REMOVE_SOURCES;
	if (index < sourcesList.size())
		QueueManager::getInstance()->removeSource(sourcesList[index].user, QueueItem::Source::FLAG_REMOVED);
	return 0;
}

LRESULT QueueFrame::onPM(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		size_t index = wID - IDC_PM;
		if (index < pmSourcesList.size())
		{
			const UserPtr& user = pmSourcesList[index].user;
			const auto hubs = ClientManager::getHubs(user->getCID(), Util::emptyString);
			PrivateFrame::openWindow(nullptr, HintedUser(user, !hubs.empty() ? hubs[0] : Util::emptyString));
		}
	}
	return 0;
}

LRESULT QueueFrame::onAutoPriority(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto qm = QueueManager::getInstance();
	int autoPriority = -1;
	auto func = [qm, &autoPriority](const QueueItemPtr& qi)
	{
		if (autoPriority == -1)
			autoPriority = (qi->getExtraFlags() & QueueItem::XFLAG_AUTO_PRIORITY) ? 0 : 1;
		qm->setAutoPriority(qi->getTarget(), autoPriority != 0);
	};

	if (usingDirMenu)
	{
		HTREEITEM ht = ctrlDirs.GetSelectedItem();
		if (!ht) return 0;
		const DirItem* dirItem = reinterpret_cast<DirItem*>(ctrlDirs.GetItemData(ht));
		walkDirItem(dirItem, func);
	}
	else
	{
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getDirItem())
				walkDirItem(ii->getDirItem(), func);
			else
			{
				const QueueItemPtr& qi = ii->getQueueItem();
				if (qi)
				{
					if (autoPriority == -1)
						autoPriority = (qi->getExtraFlags() & QueueItem::XFLAG_AUTO_PRIORITY) ? 0 : 1;
					qm->setAutoPriority(qi->getTarget(), autoPriority != 0);
				}
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onSegments(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	unsigned segments = max(1, wID - IDC_SEGMENTONE + 1);
	auto func = [segments](const QueueItemPtr& qi)
	{
		qi->lockAttributes();
		qi->setMaxSegmentsL(segments);
		qi->unlockAttributes();
	};
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		if (ii->getDirItem())
			walkDirItem(ii->getDirItem(), func);
		else
		{
			const QueueItemPtr& qi = ii->getQueueItem();
			if (qi)
			{
				qi->lockAttributes();
				qi->setMaxSegmentsL(segments);
				qi->unlockAttributes();
				ctrlQueue.updateItem(ctrlQueue.findItem(ii), COLUMN_SEGMENTS);
			}
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
	
	auto qm = QueueManager::getInstance();
	auto func = [qm, p](const QueueItemPtr& qi) { qm->setPriority(qi->getTarget(), p, true); };
	if (usingDirMenu)
	{
		HTREEITEM ht = ctrlDirs.GetSelectedItem();
		if (!ht) return 0;
		const DirItem* dirItem = reinterpret_cast<DirItem*>(ctrlDirs.GetItemData(ht));
		walkDirItem(dirItem, func);
	}
	else
	{
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(i);
			if (ii->getDirItem())
				walkDirItem(ii->getDirItem(), func);
			else
			{
				const QueueItemPtr& qi = ii->getQueueItem();
				if (qi)
					qm->setPriority(qi->getTarget(), p, true);
			}
		}
	}
	
	return 0;
}

/*
 * @param inc True = increase, False = decrease
 */
void QueueFrame::changePriority(bool inc)
{
	auto qm = QueueManager::getInstance();
	auto func = [qm, inc](const QueueItemPtr& qi)
	{
		qi->lockAttributes();
		QueueItem::Priority p =	qi->getPriorityL();
		qi->unlockAttributes();
		int newPriority = p + (inc ? 1 : -1);
		if (newPriority < QueueItem::PAUSED || newPriority > QueueItem::HIGHEST) return;
		qm->setPriority(qi->getTarget(), (QueueItem::Priority) newPriority, true);
	};
	
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		if (ii->getDirItem())
			walkDirItem(ii->getDirItem(), func);
		else
		{
			const QueueItemPtr& qi = ii->getQueueItem();
			if (qi) func(qi);
		}
	}
}

void QueueFrame::updateQueueStatus()
{
	if (!closed && ctrlStatus.m_hWnd)
	{
		size_t newTotalCount = root->totalFileCount;
		int64_t newTotalSize = root->totalSize;
		if (fileLists)
		{
			newTotalCount += fileLists->totalFileCount;
			//newTotalSize += fileLists->totalSize;
		}

		tstring countStr;
		int64_t total = 0;
		unsigned cnt = ctrlQueue.GetSelectedCount();
		if (cnt == 0)
		{
			cnt = ctrlQueue.GetItemCount();
			if (showTree && currentDir)
				total = currentDir->totalSize;
			else
				total = newTotalSize;
			countStr = TSTRING(DISPLAYED_ITEMS);
		}
		else
		{
			int i = -1;
			while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const QueueItemInfo* ii = ctrlQueue.getItemData(i);
				if (ii)
				{
					int64_t size = ii->getSize();
					if (size > 0) total += size;
				}
			}
			countStr = TSTRING(SELECTED_ITEMS);
		}

		countStr += Util::toStringT(cnt);
		ctrlStatus.setAutoRedraw(false);
		ctrlStatus.setPaneText(STATUS_ITEM_COUNT, countStr);
		ctrlStatus.setPaneText(STATUS_ITEM_SIZE, TSTRING(SIZE) + _T(": ") + Util::formatBytesT(total));

		if (newTotalCount != lastTotalCount || newTotalSize != lastTotalSize)
		{
			lastTotalCount = newTotalCount;
			lastTotalSize = newTotalSize;
			ctrlStatus.setPaneText(STATUS_TOTAL_COUNT, TSTRING(TOTAL_FILES) + Util::toStringT(lastTotalCount));
			ctrlStatus.setPaneText(STATUS_TOTAL_SIZE, TSTRING(TOTAL_SIZE) + Util::formatBytesT(lastTotalSize));
		}

		ctrlStatus.setAutoRedraw(true);
		updateStatus = false;
	}
}

void QueueFrame::UpdateLayout(BOOL)
{
	if (isClosedOrShutdown())
		return;

	RECT rect;
	GetClientRect(&rect);
	RECT prevRect = rect;

	if (ctrlStatus)
	{
		HDC hdc = GetDC();
		int statusHeight = ctrlStatus.getPrefHeight(hdc);
		rect.bottom -= statusHeight;
		RECT rcStatus = rect;
		rcStatus.top = rect.bottom;
		rcStatus.bottom = rcStatus.top + statusHeight;
		ctrlStatus.SetWindowPos(nullptr, &rcStatus, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
		ctrlStatus.updateLayout(hdc);
		ReleaseDC(hdc);

		// Place checkbox in statusbar part #0
		RECT sr;
		ctrlStatus.getPaneRect(0, sr);
		sr.left += 4;
		sr.right -= 4;
		ctrlShowTree.SetWindowPos(nullptr, &sr, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	if (showTree)
	{
		if (getSinglePaneMode() != -1)
		{
			if (!treeInserted) insertTrees();
			setSinglePaneMode(-1);
			updateQueue(true);
		}
	}
	else
	{
		if (getSinglePaneMode() != 1)
		{
			clearingTree++;
			ctrlDirs.DeleteAllItems();
			clearingTree--;
			treeInserted = false;
			setSinglePaneMode(1);
			updateQueue(true);
		}
	}

	MARGINS margins;
	WinUtil::getMargins(margins, prevRect, rect);
	setMargins(margins);
	updateLayout();
}

LRESULT QueueFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	timer.destroyTimer();
	tasks.setDisabled(true);

	if (!closed)
	{
		closed = true;
		SettingsManager::instance.removeListener(this);
		DownloadManager::getInstance()->removeListener(this);
		QueueManager::getInstance()->removeListener(this);

		setButtonPressed(IDC_QUEUE, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		auto ss = SettingsManager::instance.getUiSettings();
		ss->setBool(Conf::QUEUE_FRAME_SHOW_TREE, ctrlShowTree.GetCheck() == BST_CHECKED);
		ctrlQueue.deleteAllNoLock();
		clearingTree++;
		ctrlDirs.DeleteAllItems();
		clearingTree--;

		ctrlQueue.saveHeaderOrder(Conf::QUEUE_FRAME_ORDER, Conf::QUEUE_FRAME_WIDTHS, Conf::QUEUE_FRAME_VISIBLE);
		ss->setInt(Conf::QUEUE_FRAME_SORT, ctrlQueue.getSortForSettings());
		ss->setInt(Conf::QUEUE_FRAME_SPLIT, getSplitterPos(0, true));
		tasks.clear();
		bHandled = FALSE;
		return 0;
	}
}

LRESULT QueueFrame::onTreeItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (!closed && !clearingTree)
	{
		updateQueue(false);
		updateQueueStatus();
	}
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

void QueueFrame::redrawQueue()
{
	ctrlQueue.RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ERASENOW | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void QueueFrame::updateQueue(bool changingState)
{
	dcassert(!closed);
	CWaitCursor waitCursor;
	ctrlQueue.deleteAllNoLock();
	if (changingState) redrawQueue();
	ctrlQueue.SetRedraw(FALSE);
	if (showTree)
	{
		if (!ctrlDirs.GetRootItem())
			insertTrees();
		HTREEITEM ht = ctrlDirs.GetSelectedItem();
		if (!ht)
		{
			ctrlQueue.SetRedraw(TRUE);
			redrawQueue();
			return;
		}
		auto dir = reinterpret_cast<DirItem*>(ctrlDirs.GetItemData(ht));
		int count = 0;
		for (DirItem* item : dir->directories)
		{
			QueueItemInfo* ii = new QueueItemInfo(item);
			ctrlQueue.insertItem(count++, ii, I_IMAGECALLBACK);
		}
		for (const QueueItemPtr& qi : dir->files)
		{
			QueueItemInfo* ii = new QueueItemInfo(qi);
			ctrlQueue.insertItem(count++, ii, I_IMAGECALLBACK);
		}
		currentDir = dir;
		currentDirPath = getFullPath(dir);
#if defined(DEBUG_QUEUE_FRAME) && defined(DEBUG_QUEUE_FRAME_TREE)
		LogManager::message("currentDirPath: " + currentDirPath, true);
#endif
	}
	else
	{
		int count = 0;
		auto func = [this, &count](const QueueItemPtr& qi)
		{
			QueueItemInfo* ii = new QueueItemInfo(qi);
			ctrlQueue.insertItem(count++, ii, I_IMAGECALLBACK);
		};
		if (fileLists) walkDirItem(fileLists, func);
		walkDirItem(root, func);
	}
	ctrlQueue.resort();
	ctrlQueue.SetRedraw(TRUE);
	redrawQueue();
}

LRESULT QueueFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)pnmh;
	
	QueueItemInfo *ii = reinterpret_cast<QueueItemInfo*>(cd->nmcd.lItemlParam);
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
		{
			ii->updateIconIndex();
			return CDRF_NOTIFYSUBITEMDRAW;
		}
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			if (ctrlQueue.findColumn(cd->iSubItem) == COLUMN_PROGRESS)
			{
				if (!showProgressBars)
				{
					bHandled = FALSE;
					return 0;
				}
				const QueueItemPtr& qi = ii->getQueueItem();
				if (!qi || qi->getSize() == -1) return CDRF_DODEFAULT;
				
				const auto* ss = SettingsManager::instance.getUiSettings();
				CRect rc;
				ctrlQueue.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				CBarShader statusBar(rc.Height(), rc.Width(), ss->getInt(Conf::PROGRESS_BACK_COLOR), ii->getSize());
				COLORREF colorRunning = ss->getInt(Conf::COLOR_RUNNING);
				COLORREF colorRunning2 = ss->getInt(Conf::COLOR_RUNNING_COMPLETED);
				COLORREF colorDownloaded = ss->getInt(Conf::COLOR_DOWNLOADED);
				ii->getQueueItem()->getChunksVisualisation(runningChunks, doneChunks);
				for (auto i = runningChunks.cbegin(); i < runningChunks.cend(); ++i)
				{
					const QueueItem::SegmentEx& rs = *i;
					statusBar.FillRange(rs.start, rs.end, colorRunning);
					statusBar.FillRange(rs.start, rs.start + rs.pos, colorRunning2);
				}
				for (auto i = doneChunks.cbegin(); i < doneChunks.cend(); ++i)
				{
					statusBar.FillRange(i->getStart(), i->getEnd(), colorDownloaded);
				}
				CDC cdc;
				cdc.CreateCompatibleDC(cd->nmcd.hdc);
				HBITMAP pOldBmp = cdc.SelectBitmap(CreateCompatibleBitmap(cd->nmcd.hdc,  rc.Width(),  rc.Height()));

				statusBar.Draw(cdc, 0, 0, ss->getInt(Conf::PROGRESS_3DDEPTH));
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
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::DOWNLOAD_QUEUE, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT QueueFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring data;
	int i = -1, columnId = wID - IDC_COPY; // !SMT!-UI: copy several rows
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		const QueueItemPtr& qi = ii->getQueueItem();
		tstring sdata;
		if (wID == IDC_COPY_LINK)
		{
			if (qi)
				sdata = Text::toT(Util::getMagnet(qi->getTTH(), Util::getFileName(qi->getTarget()), qi->getSize()));
		}
		else if (wID == IDC_COPY_WMLINK)
		{
			if (qi)
				sdata = Text::toT(Util::getWebMagnet(qi->getTTH(), Util::getFileName(qi->getTarget()), qi->getSize()));
		}
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

LRESULT QueueFrame::onCopyFolder(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HTREEITEM ht = ctrlDirs.GetSelectedItem();
	if (!ht) return 0;
	const DirItem* di = reinterpret_cast<const DirItem*>(ctrlDirs.GetItemData(ht));
	if (!di || di->name.empty()) return 0;
	string data = di->name;
	if (wID == IDC_COPY_FOLDER_NAME)
	{
		Util::removePathSeparator(data);
		auto pos = data.rfind(PATH_SEPARATOR);
		if (pos != string::npos && pos != data.length()-1) data.erase(0, pos + 1);
	}
	else
	{
		ht = ctrlDirs.GetParentItem(ht);
		while (ht)
		{
			di = reinterpret_cast<const DirItem*>(ctrlDirs.GetItemData(ht));
			string name = di->name;
			Util::appendPathSeparator(name);
			data.insert(0, name);
			ht = ctrlDirs.GetParentItem(ht);
		}
		Util::appendPathSeparator(data);
	}
	WinUtil::setClipboard(data);
	return 0;
}

LRESULT QueueFrame::onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		const QueueItemInfo* ii = getSelectedQueueItem();
		const auto& qi = ii->getQueueItem();
		if (qi)
			runPreview(wID, qi);
	}
	return 0;
}

LRESULT QueueFrame::onRemoveOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	sourcesToRemove.clear();
	auto func = [this](const QueueItemPtr& qi)
	{
		QueueRLock(*QueueItem::g_cs);
		const auto& sources = qi->getSourcesL();
		for (auto i = sources.cbegin(); i != sources.cend(); ++i)
		{
			if (!i->first->isOnline())
				sourcesToRemove.emplace_back(qi->getTarget(), i->first);
		}
	};
	int j = -1;
	while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(j);
		if (ii->getDirItem())
			walkDirItem(ii->getDirItem(), func);
		else
		{
			const QueueItemPtr& qi = ii->getQueueItem();
			if (qi) func(qi);
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

void QueueFrame::on(SettingsManagerListener::ApplySettings)
{
	dcassert(!GlobalState::isShuttingDown());
	if (!GlobalState::isShuttingDown())
	{
		if (ctrlQueue.isRedraw())
		{
			setTreeViewColors(ctrlDirs);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		const auto* ss = SettingsManager::instance.getUiSettings();
		showProgressBars = ss->getBool(Conf::SHOW_PROGRESS_BARS);
	}
}

void QueueFrame::onRechecked(const string& target, const string& message)
{
	if (!GlobalState::isShuttingDown())
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

LRESULT QueueFrame::onShowQueueItem(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
	string* target = reinterpret_cast<string*>(lParam);
	HTREEITEM ht = nullptr;
	if (showTree)
	{
		if (wParam)
		{
			if (fileLists)
				ht = fileLists->ht;
		}
		else
		{
			DirItem* dir;
			if (findItem(QueueItemPtr(), 0, *target, dir, false))
				ht = dir->ht;
		}
	}
	if (ht)
	{
		ctrlDirs.EnsureVisible(ht);
		ctrlDirs.SelectItem(ht);
		int count = ctrlQueue.GetItemCount();
		for (int i = 0; i < count; ++i)
		{
			QueueItemInfo* ii = ctrlQueue.getItemData(i);
			const QueueItemPtr& qi = ii->getQueueItem();
			if (qi && !stricmp(qi->getTarget(), *target))
			{
				ctrlQueue.SelectItem(i);
				break;
			}
		}
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

void QueueFrame::onTimerInternal()
{
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown())
	{
		bool u = false;
		int first = ctrlQueue.GetTopIndex();
		int last = std::min(ctrlQueue.GetItemCount(), first + ctrlQueue.GetCountPerPage() + 1);
		for (int i = first; i < last; ++i)
		{
			auto ii = ctrlQueue.getItemData(i);
			if (!(ii && ii->type == QueueItemInfo::FILE)) continue;
			if (ii->updateCachedInfo())
			{
				ctrlQueue.updateItem(i);
				u = true;
			}
			if (ii->getQueueItem()->isRunning())
			{
				u = true;
				ii->getQueueItem()->updateDownloadedBytesAndSpeed();
				ctrlQueue.updateItem(i, COLUMN_DOWNLOADED);
				if (!(ii->flags & QueueItemInfo::FLAG_RUNNING))
				{
					ii->flags |= QueueItemInfo::FLAG_RUNNING;
					ctrlQueue.updateItem(i, COLUMN_STATUS);
				}
			}
			else if (ii->flags & QueueItemInfo::FLAG_RUNNING)
			{
				u = true;
				ii->flags &= ~QueueItemInfo::FLAG_RUNNING;
				ii->getQueueItem()->updateDownloadedBytesAndSpeed();
				ctrlQueue.updateItem(i, COLUMN_STATUS);
			}
		}
		if (u) ctrlQueue.Invalidate();
		if (updateStatus) updateQueueStatus();
	}
	processTasks();
}

void QueueFrame::QueueListViewCtrl::sortItems()
{
	int count = GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		auto ii = getItemData(i);
		if (ii->type == QueueItemInfo::FILE)
			ii->updateCachedInfo();
	}
	TypedListViewCtrl<QueueItemInfo>::sortItems();
}

CFrameWndClassInfo& QueueFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("QueueFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::DOWNLOAD_QUEUE, 0);

	return wc;
}
