#include "stdafx.h"
#include "FinishedFrameBase.h"
#include "Colors.h"
#include "ShellContextMenu.h"
#include "../client/QueueManager.h"
#include "../client/UploadManager.h"
#include "../client/FormatUtil.h"
#include "ExMessageBox.h"

#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
#include "TextFrame.h"
#endif

static const int columnId[] =
{
	FinishedItem::COLUMN_FILE,
	FinishedItem::COLUMN_TYPE,
	FinishedItem::COLUMN_DONE,
	FinishedItem::COLUMN_PATH,
	FinishedItem::COLUMN_TTH,
	FinishedItem::COLUMN_NICK,
	FinishedItem::COLUMN_HUB,
	FinishedItem::COLUMN_SIZE,
	FinishedItem::COLUMN_NETWORK_TRAFFIC,
	FinishedItem::COLUMN_SPEED,
	FinishedItem::COLUMN_IP
};

static const int columnSizes[] =
{
	180, // COLUMN_FILE
	64,  // COLUMN_TYPE
	120, // COLUMN_DONE
	290, // COLUMN_PATH
	125, // COLUMN_TTH
	100, // COLUMN_NICK
	120, // COLUMN_HUB
	85,  // COLUMN_SIZE
	85,  // COLUMN_NETWORK_TRAFFIC
	90,  // COLUMN_SPEED
	100  // COLUMN_IP
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::FILENAME,
	ResourceManager::TYPE,
	ResourceManager::TIME,
	ResourceManager::PATH,
	ResourceManager::TTH,
	ResourceManager::NICK,
	ResourceManager::HUB,
	ResourceManager::SIZE,
	ResourceManager::NETWORK_TRAFFIC,
	ResourceManager::SPEED,
	ResourceManager::IP
};

HTREEITEM FinishedFrameBase::createRootItem(TreeItemType nodeType)
{
	auto data = new TreeItemData;
	data->type = nodeType;
	data->dateAsInt = 0;
	return ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
		nodeType == RootRecent ? CTSTRING(CURRENT_SESSION_RAM) : CTSTRING(HISTORY_DATABASE),
		3, // nImage
		3, // nSelectedImage
		0, // nState
		0, // nStateMask
		reinterpret_cast<LPARAM>(data), // lParam
		rootItem, // hParent,
		TVI_LAST  // hInsertAfter
		);
}

void FinishedFrameBase::onCreate(HWND hwnd, int id)
{
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(columnId));
	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(columnId));

	ctrlList.Create(hwnd, CWindow::rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, id);
	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));

	ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	WinUtil::setExplorerTheme(ctrlList);
	setListViewColors(ctrlList);

	ctrlList.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlList.setColumnFormat(FinishedItem::COLUMN_SIZE, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(FinishedItem::COLUMN_SPEED, LVCFMT_RIGHT);
	ctrlList.setColumnFormat(FinishedItem::COLUMN_NETWORK_TRAFFIC, LVCFMT_RIGHT);

	ctrlList.insertColumns(columnOrder, columnWidth, columnVisible);
	ctrlList.setSortFromSettings(SettingsManager::instance.getUiSettings()->getInt(columnSort));

	copyMenu.CreatePopupMenu();
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_TYPE, CTSTRING(TYPE));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_PATH, CTSTRING(PATH));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
	copyMenu.AppendMenu(MF_STRING, IDC_NETWORK_TRAFFIC, CTSTRING(NETWORK_TRAFFIC));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_HUB_URL, CTSTRING(HUB_ADDRESS));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_SPEED, CTSTRING(SPEED));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_IP, CTSTRING(COPY_IP));

	directoryMenu.CreatePopupMenu();
	directoryMenu.AppendMenu(MF_STRING, IDC_REMOVE_TREE_ITEM, CTSTRING(REMOVE));

	ctrlTree.Create(hwnd, CWindow::rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WinUtil::getTreeViewStyle(), WS_EX_CLIENTEDGE, IDC_TRANSFER_TREE);
	setTreeViewColors(ctrlTree);
	WinUtil::setExplorerTheme(ctrlTree);

	g_TransferTreeImage.init();
	ctrlTree.SetImageList(g_TransferTreeImage.getIconList(), TVSIL_NORMAL);
			
	rootItem = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
		transferType == e_TransferDownload ? CTSTRING(FINISHED_DOWNLOADS) : CTSTRING(FINISHED_UPLOADS),
		2, // nImage
		2, // nSelectedImage
		0, // nState
		0, // nStateMask
		0, // lParam
		0, // hParent,
		TVI_ROOT  // hInsertAfter
		);

	loading = true;
	ctrlTree.EnableWindow(FALSE);
	ctrlStatus.SetText(0, CTSTRING(LOADING_DATA));

	loader.parent = this;
	loader.hwnd = hwnd;
	loader.start(0, "FinishedFrameLoader");
}

void FinishedFrameBase::insertData()
{
	rootRecent = createRootItem(RootRecent);
	rootDatabase = createRootItem(RootDatabase);

	for (size_t index = 0; index < summary.size(); ++index)
	{
		const TransferHistorySummary& s = summary[index];
		string caption = Util::formatDateTime("%x", s.date, true) + " (" + Util::toString(s.count) + ")";
		if (s.actual)
			caption += " (" + Util::formatBytes(s.actual) + ")";

		TreeItemData* data = new TreeItemData;
		data->type = HistoryDate;
		data->dateAsInt = s.dateAsInt;

		ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			Text::toT(caption).c_str(),
			1, // nImage
			1, // nSelectedImage
			0, // nState
			0, // nStateMask
			reinterpret_cast<LPARAM>(data), // lParam
			rootDatabase, // hParent
			TVI_LAST  // hInsertAfter
			);
	}
	ctrlTree.Expand(rootItem);
	ctrlTree.Expand(rootDatabase);
	ctrlTree.SelectItem(rootItem);
}

void FinishedFrameBase::addStatusLine(const tstring& text)
{
	ctrlStatus.SetText(0, (Text::toT(Util::getShortTimeString()) + _T(' ') + text).c_str());
}

void FinishedFrameBase::updateStatus()
{
	if (totalCountLast != totalCount)
	{
		totalCountLast = totalCount;
		ctrlStatus.SetText(1, TPLURAL_F(PLURAL_ITEMS, totalCount).c_str());
		ctrlStatus.SetText(2, Util::formatBytesT(totalBytes).c_str());
		ctrlStatus.SetText(3, Util::formatBytesT(totalActual).c_str());
		ctrlStatus.SetText(4, (Util::formatBytesT(totalCount > 0 ? totalSpeed / totalCount : 0) + _T('/') + TSTRING(S)).c_str());
		//setCountMessages(totalCount); -- not used
	}
}

void FinishedFrameBase::updateList(const FinishedItemList& fl)
{
	CLockRedraw<true> lockRedraw(ctrlList);
	for (auto i = fl.cbegin(); i != fl.cend(); ++i)
	{
		addFinishedEntry(*i, false);
	}
	updateStatus();
}

void FinishedFrameBase::addFinishedEntry(const FinishedItemPtr& entry, bool ensureVisible)
{
	const auto ii = new FinishedItemInfo(entry);
	totalBytes += entry->getSize();
	totalActual += entry->getActual();
	totalSpeed += entry->getAvgSpeed();
	totalCount++;
	const int loc = ctrlList.insertItem(ii, I_IMAGECALLBACK); // ii->getImageIndex() // fix I_IMAGECALLBACK https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=47103
	if (ensureVisible)
	{
		ctrlList.EnsureVisible(loc, FALSE);
	}
}

LRESULT FinishedFrameBase::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string data;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const auto item = ctrlList.getItemData(i);
		tstring sCopy;
		switch (wID)
		{
			case IDC_COPY_NICK:
				sCopy = item->getText(FinishedItem::COLUMN_NICK);
				break;
			case IDC_COPY_FILENAME:
				sCopy = item->getText(FinishedItem::COLUMN_FILE);
				break;
			case IDC_COPY_TYPE:
				sCopy = item->getText(FinishedItem::COLUMN_TYPE);
				break;
			case IDC_COPY_PATH:
				sCopy = item->getText(FinishedItem::COLUMN_PATH);
				break;
			case IDC_COPY_SIZE:
				sCopy = item->getText(FinishedItem::COLUMN_SIZE);
				break;
			case IDC_NETWORK_TRAFFIC:
				sCopy = item->getText(FinishedItem::COLUMN_NETWORK_TRAFFIC);
				break;
			case IDC_COPY_HUB_URL:
				sCopy = item->getText(FinishedItem::COLUMN_HUB);
				break;
			case IDC_COPY_TTH:
				sCopy = item->getText(FinishedItem::COLUMN_TTH);
				break;
			case IDC_COPY_SPEED:
				sCopy = item->getText(FinishedItem::COLUMN_SPEED);
				break;
			case IDC_COPY_IP:
				sCopy = item->getText(FinishedItem::COLUMN_IP);
				break;
			default:
				dcassert(0);
				return 0;
		}
		if (!sCopy.empty())
		{
			if (data.empty())
			{
				data = Text::fromT(sCopy);
			}
			else
			{
				data += "\r\n";
				data += Text::fromT(sCopy);
			}
		}
	}
	if (!data.empty())
		WinUtil::setClipboard(data);
	return 0;
}

LRESULT FinishedFrameBase::onSelChangedTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	const NMTREEVIEW* p = (const NMTREEVIEW *) pnmh;
	currentTreeItemSelected = false;
	totalBytes = 0;
	totalActual = 0;
	totalSpeed = 0;
	totalCount = 0;
	if (p->itemNew.state & TVIS_SELECTED)
	{
		CWaitCursor waitCursor;
		ctrlList.deleteAll();
		if (p->itemNew.lParam)
		{
			const TreeItemData* data = reinterpret_cast<const TreeItemData*>(p->itemNew.lParam);
			if (data->type == RootRecent)
			{
				currentTreeItemSelected = true;
				auto fm = FinishedManager::getInstance();
				updateList(fm->lockList(type));
				fm->unlockList(type);
			}
			else
			{
				vector<FinishedItemPtr> items;
				if (data->type == HistoryDate)
				{
					auto conn = DatabaseManager::getInstance()->getDefaultConnection();
					if (conn) conn->loadTransferHistory(transferType, data->dateAsInt, items);
				}
				{
					CLockRedraw<true> lockRedraw(ctrlList);
					for (auto i = items.begin(); i != items.end(); ++i)
					{
						FinishedItemPtr& item = *i;
						const string& nick = item->getNick();
						const string& hub = item->getHub();
						if (!nick.empty() && !hub.empty() && !Util::isAdcHub(hub))
							item->setCID(ClientManager::makeCid(nick, hub));
						addFinishedEntry(item, false);
					}
				}
			}
			ctrlList.resort();
		}
		updateStatus();
	}
	return 0;
}

LRESULT FinishedFrameBase::onTreeItemDeleted(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	const NMTREEVIEW* p = (const NMTREEVIEW *) pnmh;
	delete reinterpret_cast<const TreeItemData*>(p->itemOld.lParam);
	return 0;
}

LRESULT FinishedFrameBase::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMITEMACTIVATE* item = (const NMITEMACTIVATE*) pnmh;
			
	if (item->iItem != -1)
	{
		const auto ii = ctrlList.getItemData(item->iItem);
		WinUtil::openFile(Text::toT(ii->entry->getTarget()));
	}
	return 0;
}
		
bool FinishedFrameBase::onSpeaker(WPARAM wParam, LPARAM lParam)
{
	bool updated = false;
	switch (wParam)
	{
		case SPEAK_ADD_ITEM:
		{					
			const FinishedItemPtr* entry = reinterpret_cast<FinishedItemPtr*>(lParam);
			if (currentTreeItemSelected)
			{
				addFinishedEntry(*entry, true);
				if ((*entry)->getID() == 0)
					updated = true;
				updateStatus();
			}
			delete entry;
		}
		break;

		case SPEAK_REMOVE_ITEM:
		{
			if (currentTreeItemSelected)
			{
				const FinishedItemPtr* entry = reinterpret_cast<FinishedItemPtr*>(lParam);
				const int count = ctrlList.GetItemCount();
				for (int i = 0; i < count; ++i)
				{
					auto itemData = ctrlList.getItemData(i);
					if (itemData && itemData->entry == *entry)
					{
						delete itemData;
						ctrlList.DeleteItem(i);
						break;
					}
				}
				delete entry;
				updateStatus();
			}
		}
		break;

		case SPEAK_UPDATE_STATUS:
			updateStatus();
			break;

		case SPEAK_REMOVE_DROPPED_ITEMS:
		{
			int64_t* pval = reinterpret_cast<int64_t*>(lParam);
			removeDroppedItems(*pval);
			delete pval;
		}
		break;

		case SPEAK_FINISHED:
			loading = false;
			loader.join();
			if (!abortFlag)
			{
				ctrlStatus.SetText(0, _T(""));
				insertData();
				ctrlTree.EnableWindow(TRUE);
			}
	}
	return updated;
}

void FinishedFrameBase::appendMenuItems(OMenu& menu, bool fileExists, const CID& userCid, int& copyMenuPos)
{
	if (fileExists)
	{
		menu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
		menu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
		menu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT), g_iconBitmaps.getBitmap(IconBitmaps::NOTEPAD, 0));
#endif
	}
	else if (transferType == e_TransferDownload)
		menu.AppendMenu(MF_STRING, IDC_REDOWNLOAD_FILE, CTSTRING(REDOWNLOAD), g_iconBitmaps.getBitmap(IconBitmaps::DOWNLOAD, 0));
	copyMenuPos = menu.GetMenuItemCount();
	menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY), g_iconBitmaps.getBitmap(IconBitmaps::COPY_TO_CLIPBOARD, 0));
	if (!userCid.isZero())
	{
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, IDC_GRANTSLOT, CTSTRING(GRANT_EXTRA_SLOT));
		menu.AppendMenu(MF_STRING, IDC_GETLIST, CTSTRING(GET_FILE_LIST));
	}
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE, 0));
	menu.AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL));
	if (fileExists) menu.SetMenuDefaultItem(IDC_OPEN_FILE);
}

LRESULT FinishedFrameBase::onContextMenu(HWND hwnd, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				
		if (pt.x == -1 && pt.y == -1)
			WinUtil::getContextMenuPos(ctrlList, pt);
				
		int copyMenuPos;
		tstring filePath;
		CID userCid;
		if (ctrlList.GetSelectedCount() == 1)
		{
			int index = ctrlList.GetNextItem(-1, LVNI_SELECTED);
			const auto itemData = ctrlList.getItemData(index);
			if (itemData && itemData->entry)
			{
				userCid = itemData->entry->getCID();
				filePath = Text::toT(itemData->entry->getTarget());
				if (!File::isExist(filePath))
					filePath.clear();
				if (!ClientManager::findOnlineUser(userCid, Util::emptyString, false))
					userCid.init();
			}
		}
		if (!filePath.empty() && SettingsManager::instance.getUiSettings()->getBool(Conf::SHOW_SHELL_MENU))
		{
			ShellContextMenu shellMenu;
			shellMenu.setPath(filePath);

			OMenu ctxMenu;
			ctxMenu.SetOwnerDraw(OMenu::OD_NEVER);
			ctxMenu.CreatePopupMenu();
			shellMenu.attachMenu(ctxMenu);
			appendMenuItems(ctxMenu, true, userCid, copyMenuPos);
			ctxMenu.AppendMenu(MF_SEPARATOR);
			UINT idCommand = shellMenu.showContextMenu(hwnd, pt);
			ctxMenu.RemoveMenu(copyMenuPos, MF_BYPOSITION);
			if (idCommand != 0)
				::PostMessage(hwnd, WM_COMMAND, idCommand, 0);
		}
		else
		{
			OMenu ctxMenu;
			ctxMenu.CreatePopupMenu();
			appendMenuItems(ctxMenu, !filePath.empty(), userCid, copyMenuPos);
			ctxMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hwnd);
			ctxMenu.RemoveMenu(copyMenuPos, MF_BYPOSITION);
		}
		return TRUE;
	}
	if (reinterpret_cast<HWND>(wParam) == ctrlTree && ctrlTree.GetSelectedItem() != NULL)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

		if (pt.x == -1 && pt.y == -1)
			WinUtil::getContextMenuPos(ctrlTree, pt);
		else
		{
			ctrlTree.ScreenToClient(&pt);
			UINT a = 0;
			HTREEITEM ht = ctrlTree.HitTest(pt, &a);
			if (ht != NULL && ht != ctrlTree.GetSelectedItem())
				ctrlTree.SelectItem(ht);
			ctrlTree.ClientToScreen(&pt);
		}

		directoryMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hwnd);
		return TRUE;
	}
			
	bHandled = FALSE;
	return FALSE;
}
	
LRESULT FinishedFrameBase::onRemove(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::CONFIRM_FINISHED_REMOVAL))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(loader.hwnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return 0;
		if (checkState == BST_CHECKED)
			ss->setBool(Conf::CONFIRM_FINISHED_REMOVAL, false);
	}
	vector<int64_t> idDC;
	int i = -1, p = -1;
	auto fm = FinishedManager::getInstance();
	while ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		const auto ii = ctrlList.getItemData(i);
		if (!currentTreeItemSelected)
		{
			idDC.push_back(ii->entry->getID());
		}
		else
		{
			totalSpeed -= ii->entry->getAvgSpeed();
			fm->removeItem(ii->entry, type);
		}
		totalBytes -= ii->entry->getSize();
		totalActual -= ii->entry->getActual();
		totalCount--;
		delete ii;
		ctrlList.DeleteItem(i);
		p = i;
	}
	int count = ctrlList.GetItemCount();
	if (count)
		ctrlList.SelectItem(p < count - 1 ? p : count - 1);
	else
		removeTreeItem();
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	if (conn) conn->deleteTransferHistory(idDC);
	updateStatus();
	return 0;
}

LRESULT FinishedFrameBase::onRemoveAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::CONFIRM_FINISHED_REMOVAL))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(loader.hwnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return 0;
		if (checkState == BST_CHECKED)
			ss->setBool(Conf::CONFIRM_FINISHED_REMOVAL, false);
	}
	CWaitCursor waitCursor;
	if (!currentTreeItemSelected)
	{
		const int count = ctrlList.GetItemCount();
		vector<int64_t> idDC;
		idDC.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			const auto ii = ctrlList.getItemData(i);
			idDC.push_back(ii->entry->getID());
		}
		auto conn = DatabaseManager::getInstance()->getDefaultConnection();
		if (conn) conn->deleteTransferHistory(idDC);
	}
	else
		FinishedManager::getInstance()->removeAll(type);
	ctrlList.deleteAll();
	removeTreeItem();
	totalBytes = 0;
	totalActual = 0;
	totalSpeed = 0;
	totalCount = 0;
	updateStatus();
	return 0;
}

#ifdef BL_UI_FEATURE_VIEW_AS_TEXT
LRESULT FinishedFrameBase::onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i;
	if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		const auto ii = ctrlList.getItemData(i);
		if (ii) TextFrame::openWindow(Text::toT(ii->entry->getTarget()));
	}
	return 0;
}
#endif

LRESULT FinishedFrameBase::onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i;
	if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		const auto ii = ctrlList.getItemData(i);
		if (ii) WinUtil::openFile(Text::toT(ii->entry->getTarget()));
	}
	return 0;
}

LRESULT FinishedFrameBase::onReDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto qm = QueueManager::getInstance();
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const auto ii = ctrlList.getItemData(i);
		if (ii)
		{
			if (!ii->entry->getTTH().isZero() && !File::isExist(ii->entry->getTarget()))
			{
				const UserPtr user = ClientManager::findLegacyUser(ii->entry->getNick(), ii->entry->getHub());
				bool getConnFlag = true;
				try
				{
					QueueManager::QueueItemParams params;
					params.size = ii->entry->getSize();
					params.root = &ii->entry->getTTH();
					params.readdBadSource = false;
					qm->add(ii->entry->getTarget(), params, HintedUser(user, Util::emptyString), 0, 0, getConnFlag);
				}
				catch (const Exception& e)
				{
					LogManager::message(e.getError());
				}
			}
		}
	}
	return 0;
}

LRESULT FinishedFrameBase::onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i;
	if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		const auto ii = ctrlList.getItemData(i);
		if (ii) WinUtil::openFolder(Text::toT(ii->entry->getTarget()));
	}
	return 0;
}

LRESULT FinishedFrameBase::onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i;
	if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		if (const auto ii = ctrlList.getItemData(i))
		{
			const UserPtr u = ClientManager::findUser(ii->entry->getCID());
			if (u && u->isOnline())
			{
				try
				{
					QueueManager::getInstance()->addList(HintedUser(u, ii->entry->getHub()), 0, QueueItem::XFLAG_CLIENT_VIEW);
				}
				catch (const Exception& e)
				{
					addStatusLine(Text::toT(e.getError()));
				}
			}
			else
				addStatusLine(TSTRING(USER_OFFLINE));
		}
		else
			addStatusLine(TSTRING(USER_OFFLINE));
	}
	return 0;
}
		
LRESULT FinishedFrameBase::onGrant(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i;
	if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		if (const auto ii = ctrlList.getItemData(i))
		{
			const UserPtr u = ClientManager::findUser(ii->entry->getCID());
			if (u && u->isOnline())
			{
				UploadManager::getInstance()->reserveSlot(HintedUser(u, ii->entry->getHub()), 600);
			}
			else
				addStatusLine(TSTRING(USER_OFFLINE));
		}
		else
			addStatusLine(TSTRING(USER_OFFLINE));
	}
	return 0;
}

LRESULT FinishedFrameBase::onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
{
	ctrlList.SetFocus();
	return 0;
}

void FinishedFrameBase::removeDroppedItems(int64_t maxTempId)
{
	if (currentTreeItemSelected)
	{
		int count = ctrlList.GetItemCount();
		for (int i = 0; i < count;)
		{
			auto itemData = ctrlList.getItemData(i);
			if (itemData && itemData->entry->getTempID() <= maxTempId)
			{
				delete itemData;
				ctrlList.DeleteItem(i);
				count--;
			}
			else
				++i;
		}
		totalCount = count;
		updateStatus();
	}
}

void FinishedFrameBase::removeTreeItem()
{
	HTREEITEM treeItem = ctrlTree.GetSelectedItem();
	if (!treeItem) return;
	DWORD_PTR itemData = ctrlTree.GetItemData(treeItem);
	if (!itemData) return;
	const TreeItemData* data = reinterpret_cast<const TreeItemData*>(itemData);
	if (data->type == HistoryDate) ctrlTree.DeleteItem(treeItem);
}

int FinishedFrameLoader::run()
{
	auto dm = DatabaseManager::getInstance();
	auto conn = dm->getConnection();
	if (conn)
	{
		conn->setAbortFlag(&parent->abortFlag);
		conn->loadTransferHistorySummary(parent->transferType, parent->summary);
		conn->setAbortFlag(nullptr);
		dm->putConnection(conn);
	}
	PostMessage(hwnd, WM_SPEAKER, FinishedFrameBase::SPEAK_FINISHED, 0);
	return 0;
}
