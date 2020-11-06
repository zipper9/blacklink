#include "stdafx.h"
#include "FinishedFrameBase.h"
#include "ShellContextMenu.h"
#include "../client/QueueManager.h"
#include "../client/UploadManager.h"

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

void FinishedFrameBase::SubtreeInfo::createChild(HTREEITEM rootItem, CTreeViewCtrl& tree, TreeItemType nodeType)
{
	auto data = new TreeItemData;
	data->type = nodeType;
	data->dateAsInt = 0;
	root = tree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
		nodeType == Current ? CTSTRING(CURRENT_SESSION_RAM) : CTSTRING(HISTORY_DATABASE),
		3, // g_ISPImage.m_flagImageCount + 14, // nImage
		3, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
		0, // nState
		0, // nStateMask
		reinterpret_cast<LPARAM>(data), // lParam
		rootItem, // aParent,
		TVI_LAST  // hInsertAfter
		);

	data = new TreeItemData;
	data->type = nodeType == Current ? Current : HistoryRootDC;
	data->dateAsInt = 0;
	dc = tree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
		CTSTRING(HISTORY_DCPP),
		1, // g_ISPImage.m_flagImageCount + 14, // nImage
		1, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
		0, // nState
		0, // nStateMask
		reinterpret_cast<LPARAM>(data),
		root, // aParent,
		TVI_LAST  // hInsertAfter
		);

#ifdef FLYLINKDC_USE_TORRENT
	if (nodeType != Current)
	{
		data = new TreeItemData;
		data->type = HistoryRootTorrent;
		data->dateAsInt = 0;
		torrent = tree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			CTSTRING(HISTORY_TORRENTS),
			0, // g_ISPImage.m_flagImageCount + 14, // nImage
			0, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
			0, // nState
			0, // nStateMask
			reinterpret_cast<LPARAM>(data),
			root, // aParent,
			TVI_LAST  // hInsertAfter
			);
	}
#endif
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
	ctrlList.setSortFromSettings(SettingsManager::get(columnSort));
			
	ctxMenu.CreatePopupMenu();
			
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
			
	ctxMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
	ctxMenu.AppendMenu(MF_SEPARATOR);
	//ctxMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));

	if (transferType == e_TransferDownload)
		ctxMenu.AppendMenu(MF_STRING, IDC_REDOWNLOAD_FILE, CTSTRING(DOWNLOAD));
	ctxMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
			
	ctxMenu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
	ctxMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT, CTSTRING(GRANT_EXTRA_SLOT));
	ctxMenu.AppendMenu(MF_STRING, IDC_GETLIST, CTSTRING(GET_FILE_LIST));
	ctxMenu.AppendMenu(MF_SEPARATOR);
	ctxMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
	ctxMenu.AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL));
	ctxMenu.SetMenuDefaultItem(IDC_OPEN_FILE);
			
	directoryMenu.CreatePopupMenu();
	directoryMenu.AppendMenu(MF_STRING, IDC_REMOVE_TREE_ITEM, CTSTRING(REMOVE));

	ctrlTree.Create(hwnd, CWindow::rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WinUtil::getTreeViewStyle(), WS_EX_CLIENTEDGE, IDC_TRANSFER_TREE);
	ctrlTree.SetBkColor(Colors::g_bgColor);
	ctrlTree.SetTextColor(Colors::g_textColor);
	WinUtil::setExplorerTheme(ctrlTree);

	g_TransferTreeImage.init();
	ctrlTree.SetImageList(g_TransferTreeImage.getIconList(), TVSIL_NORMAL);
			
	rootItem = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
		transferType == e_TransferDownload ? CTSTRING(DOWNLOAD) : CTSTRING(UPLOAD),
		2, // g_ISPImage.m_flagImageCount + 14, // nImage
		2, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
		0, // nState
		0, // nStateMask
		0, // lParam
		0, // aParent,
		TVI_ROOT  // hInsertAfter
		);
	currentItem.createChild(rootItem, ctrlTree, Current);
	historyItem.createChild(rootItem, ctrlTree, History);

	vector<TransferHistorySummary> summary;
	DatabaseManager::getInstance()->loadTransferHistorySummary(transferType, summary);

	for (size_t index = 0; index < summary.size(); ++index)
	{
		const TransferHistorySummary& s = summary[index];
		string caption = Util::formatDateTime("%x", s.date, true) + " (" + Util::toString(s.count) + ")";
		if (s.actual)
			caption += " (" + Util::formatBytes(s.actual) + ")";

		TreeItemData* data = new TreeItemData;
		data->type = HistoryDateDC;
		data->dateAsInt = s.dateAsInt;

		ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			Text::toT(caption).c_str(),
			1, // g_ISPImage.m_flagImageCount + 14, // nImage
			1, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
			0, // nState
			0, // nStateMask
			reinterpret_cast<LPARAM>(data), // lParam
			historyItem.dc, // aParent,
			TVI_LAST  // hInsertAfter
			);
	}
#ifdef FLYLINKDC_USE_TORRENT
	summary.clear();
	DatabaseManager::getInstance()->loadTorrentTransferHistorySummary(transferType, summary);
	for (size_t index = 0; index < summary.size(); ++index)
	{
		const TransferHistorySummary& s = summary[index];
		string caption = Util::formatDateTime("%x", s.date, true) + " (" + Util::toString(s.count) + ")";
		if (s.fileSize)
			caption += " (" + Util::formatBytes(s.fileSize) + ")";

		TreeItemData* data = new TreeItemData;
		data->type = HistoryDateTorrent;
		data->dateAsInt = s.dateAsInt;

		ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			Text::toT(caption).c_str(),
			0, // g_ISPImage.m_flagImageCount + 14, // nImage
			0, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
			0, // nState
			0, // nStateMask
			reinterpret_cast<LPARAM>(data), // lParam
			historyItem.torrent, // aParent,
			TVI_LAST  // hInsertAfter
			);
	}
#endif
	ctrlTree.Expand(rootItem);
	ctrlTree.Expand(historyItem.root);
	ctrlTree.Expand(historyItem.dc);
#ifdef FLYLINKDC_USE_TORRENT
	ctrlTree.Expand(historyItem.torrent);
#endif
	ctrlTree.SelectItem(currentItem.dc);
}

void FinishedFrameBase::addStatusLine(const tstring& aLine)
{
	ctrlStatus.SetText(0, (Text::toT(Util::getShortTimeString()) + _T(' ') + aLine).c_str());
}

void FinishedFrameBase::updateStatus()
{
	if (totalCountLast != totalCount)
	{
		totalCountLast = totalCount;
		ctrlStatus.SetText(1, (Util::toStringW(totalCount) + _T(' ') + TSTRING(ITEMS)).c_str());
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
			if (data->type == Current)
			{
				currentTreeItemSelected = true;
				auto fm = FinishedManager::getInstance();
				updateList(fm->lockList(type));
				fm->unlockList(type);
			}
			else
			{
				vector<FinishedItemPtr> items;
				if (data->type == HistoryDateDC)
				{
					DatabaseManager::getInstance()->loadTransferHistory(transferType, data->dateAsInt, items);
				}
#ifdef FLYLINKDC_USE_TORRENT
				else if (data->type == HistoryDateTorrent)
				{
					DatabaseManager::getInstance()->loadTorrentTransferHistory(transferType, data->dateAsInt, items);
				}
#endif
				{
					CLockRedraw<true> lockRedraw(ctrlList);
					for (auto i = items.cbegin(); i != items.cend(); ++i)
						addFinishedEntry(*i, false);
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
	}
	return updated;
}

LRESULT FinishedFrameBase::onContextMenu(HWND hwnd, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				
		if (pt.x == -1 && pt.y == -1)
			WinUtil::getContextMenuPos(ctrlList, pt);
				
		bool shellMenuShown = false;
		if (BOOLSETTING(SHOW_SHELL_MENU) && ctrlList.GetSelectedCount() == 1)
		{
			int index = ctrlList.GetNextItem(-1, LVNI_SELECTED);
			const auto itemData = ctrlList.getItemData(index);
			if (itemData && itemData->entry)
			{
				const string path = itemData->entry->getTarget();
				if (File::isExist(path))
				{
					CShellContextMenu shellMenu;
					shellMenu.SetPath(Text::toT(path));
							
					CMenu* pShellMenu = shellMenu.GetMenu();
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
					pShellMenu->AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));
#endif
					pShellMenu->AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
					pShellMenu->AppendMenu(MF_SEPARATOR);
					pShellMenu->AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
					pShellMenu->AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL));
					pShellMenu->AppendMenu(MF_SEPARATOR);

					UINT idCommand = shellMenu.ShowContextMenu(hwnd, pt);
					if (idCommand != 0)
						::PostMessage(hwnd, WM_COMMAND, idCommand, 0);

					shellMenuShown = true;
				}
			}
		}
				
		if (!shellMenuShown)
				ctxMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hwnd);
				
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
	switch (wID)
	{
		case IDC_REMOVE:
		{
			vector<int64_t> idDC;
#ifdef FLYLINKDC_USE_TORRENT
			vector<int64_t> idTorrent;
#endif
			int i = -1, p = -1;
			auto fm = FinishedManager::getInstance();
			while ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				const auto ii = ctrlList.getItemData(i);
				if (!currentTreeItemSelected)
				{
#ifdef FLYLINKDC_USE_TORRENT
					if (ii->entry->isTorrent)
						idTorrent.push_back(ii->entry->getID());
					else
#endif
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
			ctrlList.SelectItem((p < ctrlList.GetItemCount() - 1) ? p : ctrlList.GetItemCount() - 1);
#ifdef FLYLINKDC_USE_TORRENT
			DatabaseManager::getInstance()->deleteTorrentTransferHistory(idTorrent);
#endif
			DatabaseManager::getInstance()->deleteTransferHistory(idDC);
			updateStatus();
			break;
		}
		case IDC_REMOVE_ALL:
		{
			CWaitCursor waitCursor;
			if (!currentTreeItemSelected)
			{
				const int count = ctrlList.GetItemCount();
				vector<int64_t> idDC;
#ifdef FLYLINKDC_USE_TORRENT
				vector<int64_t> idTorrent;
#endif
				idDC.reserve(count);
				for (int i = 0; i < count; ++i)
				{
					const auto ii = ctrlList.getItemData(i);
#ifdef FLYLINKDC_USE_TORRENT
					if (ii->entry->isTorrent)
						idTorrent.push_back(ii->entry->getID());
					else
#endif
						idDC.push_back(ii->entry->getID());
				}
#ifdef FLYLINKDC_USE_TORRENT
				DatabaseManager::getInstance()->deleteTorrentTransferHistory(idTorrent);
#endif
				DatabaseManager::getInstance()->deleteTransferHistory(idDC);
			}
			else
			{
				FinishedManager::getInstance()->removeAll(type);
			}
			ctrlList.deleteAll();
			totalBytes = 0;
			totalActual = 0;
			totalSpeed = 0;
			totalCount = 0;
			updateStatus();
			break;
		}
	}
	return 0;
}

#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
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
					qm->add(ii->entry->getTarget(), ii->entry->getSize(), ii->entry->getTTH(), user, 0, QueueItem::DEFAULT, false, getConnFlag);
				}
				catch (const Exception& e)
				{
					//fix https://drdump.com/Problem.aspx?ProblemID=226879
					LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
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
					QueueManager::getInstance()->addList(HintedUser(u, ii->entry->getHub()), QueueItem::FLAG_CLIENT_VIEW);
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
