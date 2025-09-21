#include "stdafx.h"
#include "RecentsFrm.h"
#include "Fonts.h"
#include "HubFrame.h"
#include "LineDlg.h"
#include "../client/FormatUtil.h"
#include "../client/Util.h"

const int RecentHubsFrame::columnId[] =
{
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_USERS,
	COLUMN_SHARED,
	COLUMN_SERVER,
	COLUMN_LAST_SEEN,
	COLUMN_OPEN_TAB
};

static const int columnSizes[] =
{
	200, // COLUMN_NAME
	290, // COLUMN_DESCRIPTION
	50,  // COLUMN_USERS
	50,  // COLUMN_SHARED
	100, // COLUMN_SERVER
	130, // COLUMN_LAST_SEEN
	50   // COLUMN_OPEN_TAB
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::HUB_NAME,
	ResourceManager::DESCRIPTION,
	ResourceManager::USERS,
	ResourceManager::SHARED,
	ResourceManager::HUB_ADDRESS,
	ResourceManager::LAST_SEEN,
	ResourceManager::OPEN
};

tstring RecentHubsFrame::ItemInfo::getText(uint8_t col) const
{
	switch (col)
	{
		case COLUMN_NAME:
			return Text::toT(entry->getName());
		case COLUMN_DESCRIPTION:
			return Text::toT(entry->getDescription());
		case COLUMN_USERS:
			return Text::toT(entry->getUsers());
		case COLUMN_SHARED:
			return Util::formatBytesT(Util::toInt64(entry->getShared()));
		case COLUMN_SERVER:
			return Text::toT(entry->getServer());
		case COLUMN_LAST_SEEN:
			return Text::toT(entry->getLastSeen());
		case COLUMN_OPEN_TAB:
			return Text::toT(entry->getOpenTab());
	}
	return Util::emptyStringT;
}

int RecentHubsFrame::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, int col, int /*flags*/)
{
	int result = 0;
	switch (col)
	{
		case COLUMN_NAME:
			result = Util::defaultSort(Text::toT(a->entry->getName()), Text::toT(b->entry->getName()));
			break;
		case COLUMN_DESCRIPTION:
			result = Util::defaultSort(Text::toT(a->entry->getDescription()), Text::toT(b->entry->getDescription()));
			break;
		case COLUMN_USERS:
			result = compare(Util::toInt(a->entry->getUsers()), Util::toInt(b->entry->getUsers()));
			break;
		case COLUMN_SHARED:
			result = compare(Util::toInt64(a->entry->getShared()), Util::toInt64(b->entry->getShared()));
			break;
		case COLUMN_SERVER:
			return Util::defaultSort(Text::toT(a->entry->getServer()), Text::toT(b->entry->getServer()));
		case COLUMN_LAST_SEEN:
			result = compare(a->entry->getLastSeen(), b->entry->getLastSeen());
			break;
		case COLUMN_OPEN_TAB:
			result = compare(a->entry->getOpenTab(), b->entry->getOpenTab());
	}
	if (result) return result;
	return Util::defaultSort(Text::toT(a->entry->getServer()), Text::toT(b->entry->getServer()));
}

RecentHubsFrame::RecentHubsFrame()
{
	xdu = ydu = 0;
	ctrlHubs.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
}

LRESULT RecentHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	m_hAccel = LoadAccelerators(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_RECENTS));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
	                WS_TABSTOP | WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_RECENTS);

	ctrlHubs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlHubs);
	setListViewColors(ctrlHubs);

	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);

	ctrlHubs.insertColumns(Conf::RECENTS_FRAME_ORDER, Conf::RECENTS_FRAME_WIDTHS, Conf::RECENTS_FRAME_VISIBLE);
	const auto* ss = SettingsManager::instance.getUiSettings();
	ctrlHubs.setSortFromSettings(ss->getInt(Conf::RECENTS_FRAME_SORT));

	ctrlConnect.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_CONNECT);
	ctrlConnect.SetWindowText(CTSTRING(CONNECT));
	ctrlConnect.SetFont(Fonts::g_systemFont);

	ctrlRemove.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_REMOVE);
	ctrlRemove.SetWindowText(CTSTRING(REMOVE));
	ctrlRemove.SetFont(Fonts::g_systemFont);

	ctrlRemoveAll.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_REMOVE_ALL);
	ctrlRemoveAll.SetWindowText(CTSTRING(REMOVE_ALL));
	ctrlRemoveAll.SetFont(Fonts::g_systemFont);

	auto fm = FavoriteManager::getInstance();
	fm->addListener(this);
	SettingsManager::instance.addListener(this);
	updateList(fm->getRecentHubs());

	hubsMenu.CreatePopupMenu();
	hubsMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES), g_iconBitmaps.getBitmap(IconBitmaps::PROPERTIES, 0));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT), g_iconBitmaps.getBitmap(IconBitmaps::QUICK_CONNECT, 0));
	hubsMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(ADD_TO_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::ADD_HUB, 0));
	hubsMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE_HUB, 0));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE, 0));
	hubsMenu.AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL));
	hubsMenu.SetMenuDefaultItem(IDC_CONNECT);

	bHandled = FALSE;
	return TRUE;
}

LRESULT RecentHubsFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);
	bHandled = FALSE;
	return 0;
}

void RecentHubsFrame::openHubWindow(RecentHubEntry* entry)
{
	entry->setOpenTab("+");
	FavoriteManager::getInstance()->addRecent(*entry);
	HubFrame::openHubWindow(entry->getServer());
}

LRESULT RecentHubsFrame::onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	if (item->iItem != -1)
	{
		const ItemInfo* ii = ctrlHubs.getItemData(item->iItem);
		openHubWindow(ii->entry);
	}
	return 0;
}

#if 0
LRESULT RecentHubsFrame::onEnter(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	int item = ctrlHubs.GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
	{
		const ItemInfo* ii = ctrlHubs.getItemData(item);
		openHubWindow(ii->entry);
	}
	return 0;
}
#endif

LRESULT RecentHubsFrame::onClickedConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlHubs.getItemData(i);
		openHubWindow(ii->entry);
	}
	return 0;
}

LRESULT RecentHubsFrame::onAddFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetSelectedCount() == 1)
	{
		int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		if (i != -1)
		{
			const ItemInfo* ii = ctrlHubs.getItemData(i);
			FavoriteHubEntry entry;
			entry.setName(ii->entry->getName());
			entry.setDescription(ii->entry->getDescription());
			entry.setServer(ii->entry->getServer());
			FavoriteManager::getInstance()->addFavoriteHub(entry);
		}
	}
	return 0;
}

LRESULT RecentHubsFrame::onRemoveFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool save = false;
	auto fm = FavoriteManager::getInstance();
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlHubs.getItemData(i);
		if (fm->removeFavoriteHub(ii->entry->getServer(), false))
			save = true;
	}
	if (save)
		fm->saveFavorites();
	return 0;
}

void RecentHubsFrame::removeSelectedItems()
{
	auto fm = FavoriteManager::getInstance();
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlHubs.getItemData(i);
		fm->removeRecent(ii->entry);
	}
}

LRESULT RecentHubsFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	removeSelectedItems();
	return 0;
}

LRESULT RecentHubsFrame::onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetItemCount() == 0) return 0;
	if (MessageBox(CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		ctrlHubs.DeleteAllItems();
		FavoriteManager::getInstance()->clearRecents();
	}
	return 0;
}

LRESULT RecentHubsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		closed = true;
		FavoriteManager::getInstance()->removeListener(this);
		SettingsManager::instance.removeListener(this);
		setButtonPressed(IDC_RECENTS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlHubs.saveHeaderOrder(Conf::RECENTS_FRAME_ORDER, Conf::RECENTS_FRAME_WIDTHS, Conf::RECENTS_FRAME_VISIBLE);
		auto ss = SettingsManager::instance.getUiSettings();
		ss->setInt(Conf::RECENTS_FRAME_SORT, ctrlHubs.getSortForSettings());
		bHandled = FALSE;
		return 0;
	}
}

void RecentHubsFrame::UpdateLayout(BOOL)
{
	WINDOWPLACEMENT wp = { sizeof(wp) };
	GetWindowPlacement(&wp);

	int splitBarHeight = wp.showCmd == SW_MAXIMIZE &&
		SettingsManager::instance.getUiSettings()->getBool(Conf::SHOW_TRANSFERVIEW) ?
		GetSystemMetrics(SM_CYSIZEFRAME) : 0;
	if (!xdu)
	{
		WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
		buttonWidth = WinUtil::dialogUnitsToPixelsX(54, xdu);
		buttonHeight = WinUtil::dialogUnitsToPixelsY(12, ydu);
		vertMargin = std::max(WinUtil::dialogUnitsToPixelsY(3, ydu), GetSystemMetrics(SM_CYSIZEFRAME));
		horizMargin = WinUtil::dialogUnitsToPixelsX(2, xdu);
		buttonSpace = WinUtil::dialogUnitsToPixelsX(8, xdu);
	}

	RECT rc;
	GetClientRect(&rc);
	rc.bottom -= buttonHeight + 2*vertMargin - splitBarHeight;
	ctrlHubs.MoveWindow(&rc);

	rc.top = rc.bottom + vertMargin;
	rc.bottom = rc.top + buttonHeight;
	rc.left = horizMargin;
	rc.right = rc.left + buttonWidth;
	ctrlConnect.MoveWindow(&rc);

	OffsetRect(&rc, buttonSpace + buttonWidth + horizMargin, 0);
	ctrlRemove.MoveWindow(&rc);

	OffsetRect(&rc, buttonWidth + horizMargin, 0);
	ctrlRemoveAll.MoveWindow(&rc);
}

LRESULT RecentHubsFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
	if (i != -1)
	{
		const ItemInfo* ii = ctrlHubs.getItemData(i);
		dcassert(ii->entry != nullptr);
		LineDlg dlg;
		dlg.description = TSTRING(DESCRIPTION);
		dlg.title = Text::toT(ii->entry->getName());
		dlg.line = Text::toT(ii->entry->getDescription());
		dlg.icon = IconBitmaps::RECENT_HUBS;
		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			ii->entry->setDescription(Text::fromT(dlg.line));
			ctrlHubs.updateItem(i, COLUMN_DESCRIPTION);
		}
	}
	return 0;
}

void RecentHubsFrame::on(SettingsManagerListener::ApplySettings)
{
	dcassert(!GlobalState::isShuttingDown());
	if (!GlobalState::isShuttingDown())
	{
		if (ctrlHubs.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

LRESULT RecentHubsFrame::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NM_LISTVIEW* lv = (const NM_LISTVIEW*) pnmh;
	BOOL enable = (lv->uNewState & LVIS_FOCUSED) != 0;
	GetDlgItem(IDC_CONNECT).EnableWindow(enable);
	GetDlgItem(IDC_REMOVE).EnableWindow(enable);
	return 0;
}

LRESULT RecentHubsFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::RECENT_HUBS, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT RecentHubsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlHubs && ctrlHubs.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		if (pt.x == -1 && pt.y == -1)
			WinUtil::getContextMenuPos(ctrlHubs, pt);

		int status = ctrlHubs.GetSelectedCount() > 0 ? MFS_ENABLED : MFS_DISABLED;
		hubsMenu.EnableMenuItem(IDC_CONNECT, status);
		hubsMenu.EnableMenuItem(IDC_ADD, status);
		hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, status);
		if (ctrlHubs.GetSelectedCount() > 1)
		{
			hubsMenu.EnableMenuItem(IDC_EDIT, MFS_DISABLED);
		}
		else
		{
			auto fm = FavoriteManager::getInstance();
			int i = -1;
			while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const ItemInfo* ii = ctrlHubs.getItemData(i);
				if (fm->isFavoriteHub(ii->entry->getServer()))
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_DISABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_ENABLED);
				}
				else
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_ENABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_DISABLED);
				}
			}
			hubsMenu.EnableMenuItem(IDC_EDIT, status);
		}
		hubsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}

	return FALSE;
}

void RecentHubsFrame::on(RecentRemoved, const RecentHubEntry* entry) noexcept
{
	int count = ctrlHubs.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		ItemInfo* ii = ctrlHubs.getItemData(i);
		if (ii->entry == entry)
		{
			ctrlHubs.DeleteItem(i);
			delete ii;
			break;
		}
	}
}

void RecentHubsFrame::on(RecentUpdated, const RecentHubEntry* entry) noexcept
{
	int count = ctrlHubs.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		const ItemInfo* ii = ctrlHubs.getItemData(i);
		if (ii->entry == entry)
		{
			ctrlHubs.updateItem(i);
			break;
		}
	}
}

void RecentHubsFrame::updateList(const RecentHubEntry::List& fl)
{
	CLockRedraw<true> lockRedraw(ctrlHubs);
	for (auto i = fl.cbegin(); i != fl.cend(); ++i)
		addEntry(*i);
}

void RecentHubsFrame::addEntry(RecentHubEntry* entry)
{
	ItemInfo* ii = new ItemInfo(entry);
	ctrlHubs.insertItem(ii, I_IMAGECALLBACK);
}

LRESULT RecentHubsFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*)pnmh;
	if (kd->wVKey == VK_DELETE)
		removeSelectedItems();
	else
		bHandled = FALSE;
	return 0;
}

BOOL RecentHubsFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::tabCtrl->isActive(m_hWnd)) return FALSE;
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	if (WinUtil::isCtrl()) return FALSE;
	return IsDialogMessage(pMsg);
}

CFrameWndClassInfo& RecentHubsFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("RecentHubsFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::RECENT_HUBS, 0);

	return wc;
}
