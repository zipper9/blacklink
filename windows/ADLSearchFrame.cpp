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

/*
 * Automatic Directory Listing Search
 * Henrik Engström, henrikengstrom at home se
 */

#include "stdafx.h"
#include "ADLSearchFrame.h"
#include "AdlsProperties.h"
#include "Colors.h"
#include "ExMessageBox.h"
#include "HelpTextDlg.h"

int ADLSearchFrame::columnIndexes[] =
{
	COLUMN_ACTIVE_SEARCH_STRING,
	COLUMN_SOURCE_TYPE,
	COLUMN_DEST_DIR,
	COLUMN_MIN_FILE_SIZE,
	COLUMN_MAX_FILE_SIZE
};

int ADLSearchFrame::columnSizes[] =
{
	140,
	110,
	120,
	90,
	90
};

static ResourceManager::Strings columnNames[] =
{
	ResourceManager::ACTIVE_SEARCH_STRING,
	ResourceManager::ADLS_MATCH_TYPE,
	ResourceManager::DESTINATION,
	ResourceManager::MIN_FILE_SIZE,
	ResourceManager::MAX_FILE_SIZE,
};

static const tstring& sizeTypeToDisplayString(ADLSearch::SizeType t)
{
	switch (t)
	{
		default:
		case ADLSearch::SizeBytes:
			return TSTRING(B);
		case ADLSearch::SizeKiloBytes:
			return TSTRING(KB);
		case ADLSearch::SizeMegaBytes:
			return TSTRING(MB);
		case ADLSearch::SizeGigaBytes:
			return TSTRING(GB);
	}
}

LRESULT ADLSearchFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	m_hAccel = LoadAccelerators(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDR_ADLSEARCH));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->AddMessageFilter(this);

	// Create list control
	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_ADLLIST);
	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	setListViewColors(ctrlList);
	WinUtil::setExplorerTheme(ctrlList);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(ADLSEARCH_FRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(ADLSEARCH_FRAME_WIDTHS), COLUMN_LAST);
	for (size_t j = 0; j < COLUMN_LAST; j++)
		ctrlList.InsertColumn(j, CTSTRING_I(columnNames[j]), LVCFMT_LEFT, columnSizes[j], j);
	ctrlList.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	
	// Create buttons
	ctrlAdd.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_ADD);
	ctrlAdd.SetWindowText(CTSTRING(NEW));
	ctrlAdd.SetFont(Fonts::g_systemFont);
	
	ctrlEdit.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_EDIT);
	ctrlEdit.SetWindowText(CTSTRING(PROPERTIES));
	ctrlEdit.SetFont(Fonts::g_systemFont);
	
	ctrlRemove.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_REMOVE);
	ctrlRemove.SetWindowText(CTSTRING(REMOVE));
	ctrlRemove.SetFont(Fonts::g_systemFont);
	
	ctrlMoveUp.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_MOVE_UP);
	ctrlMoveUp.SetWindowText(CTSTRING(MOVE_UP));
	ctrlMoveUp.SetFont(Fonts::g_systemFont);
	
	ctrlMoveDown.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_MOVE_DOWN);
	ctrlMoveDown.SetWindowText(CTSTRING(MOVE_DOWN));
	ctrlMoveDown.SetFont(Fonts::g_systemFont);
	
	ctrlHelp.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_CHECKBOX | BS_PUSHLIKE, 0, IDC_ADLS_HELP);
	ctrlHelp.SetWindowText(CTSTRING(WHATS_THIS));
	ctrlHelp.SetFont(Fonts::g_systemFont);
	
	// Create context menu
	contextMenu.CreatePopupMenu();
	contextMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(NEW), g_iconBitmaps.getBitmap(IconBitmaps::ADD, 0));
	contextMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE), g_iconBitmaps.getBitmap(IconBitmaps::REMOVE, 0));
	contextMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES), g_iconBitmaps.getBitmap(IconBitmaps::PROPERTIES, 0));
	contextMenu.AppendMenu(MF_SEPARATOR);
	contextMenu.AppendMenu(MF_STRING, IDC_MOVE_UP, CTSTRING(MOVE_UP), g_iconBitmaps.getBitmap(IconBitmaps::MOVE_UP, 0));
	contextMenu.AppendMenu(MF_STRING, IDC_MOVE_DOWN, CTSTRING(MOVE_DOWN), g_iconBitmaps.getBitmap(IconBitmaps::MOVE_DOWN, 0));

	SettingsManager::getInstance()->addListener(this);
	load();
	
	bHandled = FALSE;
	return TRUE;
}

LRESULT ADLSearchFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	dcassert(pLoop);
	pLoop->RemoveMessageFilter(this);
	bHandled = FALSE;
	if (dlgHelp)
	{
		dlgHelp->DestroyWindow();
		delete dlgHelp;
		dlgHelp = nullptr;
	}
	return 0;
}

LRESULT ADLSearchFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HWND hWnd = (HWND) lParam;
	HDC hDC = (HDC) wParam;
	if (hWnd == ctrlList.m_hWnd)
		return Colors::setColor(hDC);
	bHandled = FALSE;
	return FALSE;
}

LRESULT ADLSearchFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!closed)
	{
		closed = true;
		ADLSearchManager::getInstance()->save();
		SettingsManager::getInstance()->removeListener(this);
		setButtonPressed(IDC_FILE_ADL_SEARCH, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlList, SettingsManager::ADLSEARCH_FRAME_ORDER,
		                         SettingsManager::ADLSEARCH_FRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		                         
		bHandled = FALSE;
		return 0;
	}
}

void ADLSearchFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	WINDOWPLACEMENT wp = { sizeof(wp) };
	GetWindowPlacement(&wp);

	RECT rect;
	GetClientRect(&rect);
	// Position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);

	int splitBarHeight = wp.showCmd == SW_MAXIMIZE && BOOLSETTING(SHOW_TRANSFERVIEW) ? GetSystemMetrics(SM_CYSIZEFRAME) : 0;
	if (!xdu)
	{
		WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
		buttonWidth = WinUtil::dialogUnitsToPixelsX(54, xdu);
		buttonHeight = WinUtil::dialogUnitsToPixelsY(12, ydu);
		vertMargin = std::max(WinUtil::dialogUnitsToPixelsY(3, ydu), GetSystemMetrics(SM_CYSIZEFRAME));
		horizMargin = WinUtil::dialogUnitsToPixelsX(2, xdu);
		buttonSpace = WinUtil::dialogUnitsToPixelsX(8, xdu);
	}

	CRect rc = rect;
	rc.bottom -= buttonHeight + 2*vertMargin - splitBarHeight;
	ctrlList.MoveWindow(rc);

	//const long bwidth = 90;
	//const long bspace = 10;
	rc.top = rc.bottom + vertMargin;
	rc.bottom = rc.top + buttonHeight;
	rc.left = horizMargin;
	rc.right = rc.left + buttonWidth;
	ctrlAdd.MoveWindow(rc);

	rc.OffsetRect(buttonWidth + horizMargin, 0);
	ctrlEdit.MoveWindow(rc);

	rc.OffsetRect(buttonWidth + horizMargin, 0);
	ctrlRemove.MoveWindow(rc);

	rc.OffsetRect(buttonSpace + buttonWidth + horizMargin, 0);
	ctrlMoveUp.MoveWindow(rc);

	rc.OffsetRect(buttonWidth + horizMargin, 0);
	ctrlMoveDown.MoveWindow(rc);

	rc.OffsetRect(buttonSpace + buttonWidth + horizMargin, 0);
	ctrlHelp.MoveWindow(rc);
}

// Keyboard shortcuts
LRESULT ADLSearchFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		case VK_RETURN:
			PostMessage(WM_COMMAND, IDC_EDIT, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT ADLSearchFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlList)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlList, pt);
		}

		int status = ctrlList.GetNextItem(-1, LVNI_SELECTED) != -1 ? MFS_ENABLED : MFS_GRAYED;
		contextMenu.EnableMenuItem(IDC_EDIT, status);
		contextMenu.EnableMenuItem(IDC_REMOVE, status);
		contextMenu.EnableMenuItem(IDC_MOVE_UP, status);
		contextMenu.EnableMenuItem(IDC_MOVE_DOWN, status);
		contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT ADLSearchFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::ADL_SEARCH, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT ADLSearchFrame::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ADLSearch s;
	ADLSProperties dlg(&s);
	if (dlg.DoModal(*this) == IDOK)
	{
		TStringList sl;
		getItemText(sl, s);
		int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
		if (i < 0)
			ctrlList.insert(sl); // Add to end
		else
			ctrlList.insert(i, sl); // Add before selection
		auto am = ADLSearchManager::getInstance();
		{
			ADLSearchManager::LockInstance lock(am, true);
			auto& collection = lock.getCollection();
			if (i < 0)
			{
				collection.emplace_back(std::move(s));
				i = collection.size() - 1;
			}
			else
				collection.emplace(collection.begin() + i, std::move(s));
			am->setDirtyL();
		}
		setCheckState++;
		ctrlList.SetCheckState(i, s.isActive);
		setCheckState--;
		ctrlList.EnsureVisible(i, FALSE);
	}
	
	return 0;
}

LRESULT ADLSearchFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	if (i < 0) return 0;

	ADLSearch s;
	auto am = ADLSearchManager::getInstance();
	{
		ADLSearchManager::LockInstance lock(am, false);
		const auto& collection = lock.getCollection();
		if (i >= (int) collection.size()) return 0;
		s = collection[i];
	}

	ADLSProperties dlg(&s);
	if (dlg.DoModal(*this) == IDOK)
	{
		showItem(i, s);
		ADLSearchManager::LockInstance lock(am, true);
		auto& collection = lock.getCollection();
		if (i < (int) collection.size()) collection[i] = s;
		am->setDirtyL();
	}

	return 0;
}

LRESULT ADLSearchFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	vector<int> index;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) >= 0) index.push_back(i);
	if (index.empty()) return 0;

	if (BOOLSETTING(CONFIRM_ADLS_REMOVAL))
	{
		UINT checkState = BST_UNCHECKED;
		if (MessageBoxWithCheck(m_hWnd, CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) != IDYES)
			return 0;
		if (checkState == BST_CHECKED) SET_SETTING(CONFIRM_ADLS_REMOVAL, FALSE);
	}

	std::sort(index.begin(), index.end());

	auto am = ADLSearchManager::getInstance();
	ADLSearchManager::LockInstance lock(am, true);
	auto& collection = lock.getCollection();
	for (auto i = index.rbegin(); i != index.rend(); ++i)
	{
		int pos = *i;
		collection.erase(collection.begin() + pos);
		ctrlList.DeleteItem(pos);
	}
	am->setDirtyL();
	return 0;
}

LRESULT ADLSearchFrame::onHelp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	CButton wnd(hWndCtl);
	if (wnd.GetCheck() == BST_CHECKED)
	{
		wnd.SetCheck(BST_UNCHECKED);
		if (dlgHelp)
		{
			dlgHelp->DestroyWindow();
			delete dlgHelp;
			dlgHelp = nullptr;
		}
	}
	else
	{
		wnd.SetCheck(BST_CHECKED);
		if (!dlgHelp)
		{
			dlgHelp = new HelpTextDlg;
			CRect rc;
			wnd.GetWindowRect(&rc);
			dlgHelp->setNotifWnd(m_hWnd);
			dlgHelp->Create(m_hWnd);
			CRect rcDlg;
			dlgHelp->GetWindowRect(&rcDlg);
			rc.bottom = rc.top;
			rc.top = rc.bottom - rcDlg.Height();
			rc.right = rc.left + rcDlg.Width();
			HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::HELP, 0);
			dlgHelp->SetIcon(dialogIcon, TRUE);
			dlgHelp->SetIcon(dialogIcon, FALSE);
			dlgHelp->SetWindowPos(nullptr, &rc, SWP_SHOWWINDOW | SWP_NOSIZE);
		}
	}
	return 0;
}

LRESULT ADLSearchFrame::onHelpDialogClosed(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	if (dlgHelp)
	{
		dlgHelp->DestroyWindow();
		delete dlgHelp;
		dlgHelp = nullptr;
	}
	CButton(GetDlgItem(IDC_ADLS_HELP)).SetCheck(BST_UNCHECKED);
	return 0;
}

LRESULT ADLSearchFrame::onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	vector<int> index;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) >= 0) index.push_back(i);
	if (index.empty()) return 0;
	std::sort(index.begin(), index.end());

	auto am = ADLSearchManager::getInstance();
	ADLSearchManager::LockInstance lock(am, true);
	auto& collection = lock.getCollection();
	bool changed = false;
	int topPos = 0;
	for (auto i = index.begin(); i != index.end(); ++i)
	{
		int pos = *i;
		if (pos > topPos)
		{
			*i = --pos;
			std::swap(collection[pos], collection[pos + 1]);
			changed = true;
		}
		else
			topPos = pos + 1;
	}

	if (changed)
	{
		am->setDirtyL();
		update(collection, index);
	}

	return 0;
}

LRESULT ADLSearchFrame::onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	vector<int> index;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) >= 0) index.push_back(i);
	if (index.empty()) return 0;
	std::sort(index.begin(), index.end());

	auto am = ADLSearchManager::getInstance();
	ADLSearchManager::LockInstance lock(am, true);
	auto& collection = lock.getCollection();
	bool changed = false;
	int bottomPos = collection.size() - 1;
	for (auto i = index.rbegin(); i != index.rend(); ++i)
	{
		int pos = *i;
		if (pos < bottomPos)
		{
			*i = ++pos;
			std::swap(collection[pos - 1], collection[pos]);
			changed = true;
		}
		else
			bottomPos = pos - 1;
	}

	if (changed)
	{
		am->setDirtyL();
		update(collection, index);
	}

	return 0;
}

// Clicked 'Active' check box
LRESULT ADLSearchFrame::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (setCheckState) return 0;

	BOOL hasSelected = ctrlList.GetNextItem(-1, LVNI_SELECTED) != -1;
	NMITEMACTIVATE* item = reinterpret_cast<NMITEMACTIVATE*>(pnmh);
	GetDlgItem(IDC_EDIT).EnableWindow(hasSelected);
	GetDlgItem(IDC_REMOVE).EnableWindow(hasSelected);
	GetDlgItem(IDC_MOVE_UP).EnableWindow(hasSelected);
	GetDlgItem(IDC_MOVE_DOWN).EnableWindow(hasSelected);

	if ((item->uChanged & LVIF_STATE) == 0)
		return 0;
	if ((item->uOldState & INDEXTOSTATEIMAGEMASK(0xf)) == 0)
		return 0;
	if ((item->uNewState & INDEXTOSTATEIMAGEMASK(0xf)) == 0)
		return 0;

	if (item->iItem >= 0)
	{
		auto am = ADLSearchManager::getInstance();
		ADLSearchManager::LockInstance lock(am, true);
		ADLSearchManager::SearchCollection& collection = lock.getCollection();
		size_t index = item->iItem;
		if (index < collection.size())
		{
			collection[index].isActive = ctrlList.GetCheckState(item->iItem) != 0;
			am->setDirtyL();
		}
	}
	return 0;
}

LRESULT ADLSearchFrame::onDoubleClickList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = reinterpret_cast<NMITEMACTIVATE*>(pnmh);

	if (item->iItem >= 0)
		PostMessage(WM_COMMAND, IDC_EDIT, 0);
	else if (item->iItem == -1)
		PostMessage(WM_COMMAND, IDC_ADD, 0);

	return 0;
}

void ADLSearchFrame::getItemText(TStringList& sl, const ADLSearch& s) const
{
	sl.clear();
	sl.push_back(Text::toT(s.searchString));
	sl.push_back(ADLSearch::sourceTypeToDisplayString(s.sourceType));
	sl.push_back(Text::toT(s.destDir));

	tstring fs;
	if (s.minFileSize >= 0)
	{
		fs = Util::toStringT(s.minFileSize);
		fs += _T(' ');
		fs += sizeTypeToDisplayString(s.typeFileSize);
	}
	sl.push_back(fs);

	fs.clear();
	if (s.maxFileSize >= 0)
	{
		fs = Util::toStringT(s.maxFileSize);
		fs += _T(' ');
		fs += sizeTypeToDisplayString(s.typeFileSize);
	}
	sl.push_back(fs);
}

void ADLSearchFrame::load()
{
	ctrlList.DeleteAllItems();
	ADLSearchManager::LockInstance lock(ADLSearchManager::getInstance(), false);
	const auto& collection = lock.getCollection();
	TStringList sl;
	for (size_t i = 0; i < collection.size(); ++i)
	{
		getItemText(sl, collection[i]);
		ctrlList.insert(sl);
		setCheckState++;
		ctrlList.SetCheckState(i, collection[i].isActive);
		setCheckState--;
	}
}

void ADLSearchFrame::update(const ADLSearchManager::SearchCollection& collection, const vector<int>& selection)
{
	size_t j = 0;
	setCheckState++;
	for (size_t i = 0; i < collection.size(); i++)
	{
		UINT state = 0;
		if (j < selection.size() && i == selection[j])
		{
			state = LVNI_SELECTED;
			++j;
		}
		showItem(i, collection[i]);
		ctrlList.SetItemState(i, state, LVNI_SELECTED);
	}
	setCheckState--;
}

void ADLSearchFrame::showItem(int index, const ADLSearch& s)
{
	TStringList sl;
	getItemText(sl, s);
	for (size_t i = 0; i < sl.size(); ++i)
		ctrlList.SetItemText(index, i, sl[i].c_str());
	setCheckState++;
	ctrlList.SetCheckState(index, s.isActive);
	setCheckState--;
}

void ADLSearchFrame::on(SettingsManagerListener::Repaint)
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

BOOL ADLSearchFrame::PreTranslateMessage(MSG* pMsg)
{
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (TranslateAccelerator(mainFrame->m_hWnd, mainFrame->m_hAccel, pMsg)) return TRUE;
	if (!WinUtil::g_tabCtrl->isActive(m_hWnd)) return FALSE;
	if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg)) return TRUE;
	if (WinUtil::isCtrl()) return FALSE;
	if (dlgHelp && dlgHelp->IsDialogMessage(pMsg)) return TRUE;
	return IsDialogMessage(pMsg);
}

CFrameWndClassInfo& ADLSearchFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("ADLSearchFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::ADL_SEARCH, 0);

	return wc;
}
