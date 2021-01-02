/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "ShareMiscPage.h"
#include "../client/SettingsManager.h"
#include "../client/ShareManager.h"
#include "LineDlg.h"
#include "WinUtil.h"

static const WinUtil::TextItem texts1[] =
{
	{ IDC_SG_CAPTION, ResourceManager::SHARE_GROUP },
	{ IDC_REMOVE, ResourceManager::REMOVE2 },
	{ IDC_ADD, ResourceManager::ADD3 },
	{ IDC_SG_CONTENTS_CAPTION, ResourceManager::SETTINGS_SHARE_GROUP_CONTENTS },
	{ IDC_SAVE, ResourceManager::SETTINGS_SAVE_CHANGES },
	{ 0, ResourceManager::Strings() }
};

static const WinUtil::TextItem texts2[] =
{
	{ IDC_SHARING_OPTIONS, ResourceManager::SETTINGS_SHARING_OPTIONS },
	{ IDC_SHAREHIDDEN, ResourceManager::SETTINGS_SHARE_HIDDEN },
	{ IDC_SHARESYSTEM, ResourceManager::SETTINGS_SHARE_SYSTEM },
	{ IDC_SHAREVIRTUAL, ResourceManager::SETTINGS_SHARE_VIRTUAL },
	{ IDC_SHARING_AUTO_REFRESH, ResourceManager::SETTINGS_AUTOMATIC_REFRESH },
	{ IDC_REFRESH_SHARE_ON_STARTUP, ResourceManager::SETTINGS_REFRESH_ON_STARTUP },
	{ IDC_CAPTION_REFRESH_SHARE, ResourceManager::SETTINGS_REFRESH_EVERY },
	{ IDC_CAPTION_MINUTES, ResourceManager::SETTINGS_MINUTES },
	{ IDC_CAPTION_0_TO_DISABLE, ResourceManager::SETTINGS_ZERO_TO_DISABLE },
	{ IDC_SHARING_HASHING, ResourceManager::SETTINGS_HASHING_OPTIONS },
	{ IDC_USE_FAST_HASH, ResourceManager::SETTINGS_USE_FAST_HASH },
	{ IDC_TTH_IN_STREAM, ResourceManager::SETTINGS_SAVE_TTH_IN_NTFS_FILESTREAM },
	{ IDC_CAPTION_MIN_SIZE, ResourceManager::SETTINGS_MIN_SIZE_SAVE_TREES },
	{ IDC_SETTINGS_MB, ResourceManager::MB },
	{ IDC_CAPTION_MAX_HASH_SPEED, ResourceManager::SETTINGS_MAX_HASH_SPEED },
	{ IDC_SETTINGS_MBS, ResourceManager::MBPS },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_SHAREHIDDEN, SettingsManager::SHARE_HIDDEN, PropPage::T_BOOL },
	{ IDC_SHARESYSTEM, SettingsManager::SHARE_SYSTEM, PropPage::T_BOOL },
	{ IDC_SHAREVIRTUAL, SettingsManager::SHARE_VIRTUAL, PropPage::T_BOOL },
	{ IDC_REFRESH_SHARE_ON_STARTUP, SettingsManager::AUTO_REFRESH_ON_STARTUP, PropPage::T_BOOL },
	{ IDC_AUTO_REFRESH_TIME, SettingsManager::AUTO_REFRESH_TIME, PropPage::T_INT },
	{ IDC_USE_FAST_HASH, SettingsManager::FAST_HASH, PropPage::T_BOOL },
	{ IDC_TTH_IN_STREAM, SettingsManager::SAVE_TTH_IN_NTFS_FILESTREAM, PropPage::T_BOOL },
	{ IDC_MIN_FILE_SIZE, SettingsManager::SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM, PropPage::T_INT},
	{ IDC_MAX_HASH_SPEED, SettingsManager::MAX_HASH_SPEED, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

#define ADD_TAB(name, type, text) \
	tcItem.pszText = const_cast<TCHAR*>(CTSTRING(text)); \
	name.reset(new type); \
	tcItem.lParam = reinterpret_cast<LPARAM>(name.get()); \
	name->Create(m_hWnd, type::IDD); \
	ctrlTabs.InsertItem(n++, &tcItem);

LRESULT ShareMiscPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlTabs.Attach(GetDlgItem(IDC_TABS));
	TCITEM tcItem;
	tcItem.mask = TCIF_TEXT | TCIF_PARAM;
	tcItem.iImage = -1;

	int n = 0;
	ADD_TAB(pageShareGroups, ShareGroupsPage, SETTINGS_SHARE_GROUPS);
	ADD_TAB(pageShareOptions, ShareOptionsPage, SETTINGS_OPTIONS);

	ctrlTabs.SetCurSel(0);
	changeTab();

	PropPage::read(pageShareOptions->m_hWnd, items);
	pageShareOptions->fixControls();
	return TRUE;
}

void ShareMiscPage::changeTab()
{
	int pos = ctrlTabs.GetCurSel();
	pageShareGroups->ShowWindow(SW_HIDE);
	pageShareOptions->ShowWindow(SW_HIDE);

	CRect rc;
	ctrlTabs.GetClientRect(&rc);
	ctrlTabs.AdjustRect(FALSE, &rc);
	ctrlTabs.MapWindowPoints(m_hWnd, &rc);

	HWND hwnd;
	switch (pos)
	{
		case 0:
			hwnd = pageShareGroups->m_hWnd;
			break;

		default:
			hwnd = pageShareOptions->m_hWnd;
	}
	::MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	::ShowWindow(hwnd, SW_SHOW);
}

void ShareMiscPage::write()
{
	PropPage::write(pageShareOptions->m_hWnd, items);
}

void ShareMiscPage::onShow()
{
	pageShareGroups->checkShareListVersion(ShareManager::getInstance()->getShareListVersion());
}

LRESULT ShareGroupsPage::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	WinUtil::translate(*this, texts1);
	
	ctrlGroup.Attach(GetDlgItem(IDC_SHARE_GROUPS));
	ctrlDirs.Attach(GetDlgItem(IDC_LIST1));

	auto sm = ShareManager::getInstance();
	vector<ShareManager::ShareGroupInfo> smGroups;
	sm->getShareGroups(smGroups);
	groups.clear();
	groups.emplace_back(ShareGroupInfo{CID(), TSTRING(SHARE_GROUP_DEFAULT), 0});
	for (const auto& sg : smGroups)
		groups.emplace_back(ShareGroupInfo{sg.id, Text::toT(sg.name), 1});
	sortShareGroups();

	ctrlDirs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	SET_LIST_COLOR_IN_SETTING(ctrlDirs);
	WinUtil::setExplorerTheme(ctrlDirs);
	CRect rc;
	ctrlDirs.GetClientRect(rc);	
	ctrlDirs.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	ctrlDirs.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);

	insertDirectories();
	shareListVersion = sm->getShareListVersion();

	insertShareGroups(CID());
	return TRUE;
}

void ShareGroupsPage::sortShareGroups()
{
	sort(groups.begin(), groups.end(),
		[](const ShareGroupInfo& a, const ShareGroupInfo& b)
		{
			if (a.def != b.def) return a.def < b.def;
			return stricmp(a.name, b.name) < 0;
		});
}

void ShareGroupsPage::insertShareGroups(const CID& selId)
{
	ctrlGroup.ResetContent();
	int selIndex = -1;
	for (int i = 0; i < (int) groups.size(); ++i)
	{
		ctrlGroup.AddString(groups[i].name.c_str());
		if (selIndex < 0 && groups[i].id == selId) selIndex = i;
	}
	ctrlGroup.SetCurSel(selIndex);
	showShareGroupDirectories(selIndex);
	GetDlgItem(IDC_REMOVE).EnableWindow(selIndex != 0);
	GetDlgItem(IDC_SAVE).EnableWindow(FALSE);
	changed = false;
}

void ShareGroupsPage::insertDirectories()
{
	vector<ShareManager::SharedDirInfo> smDirs;
	ShareManager::getInstance()->getDirectories(smDirs);
	dirs.clear();
	for (const auto& dir : smDirs)
		dirs.emplace_back(DirInfo{Text::toT(dir.realPath), Text::toT(dir.virtualPath)});

	LVITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.pszText = LPSTR_TEXTCALLBACK;
	lvi.iSubItem = 0;

	ctrlDirs.DeleteAllItems();
	for (int i = 0; i < (int) dirs.size(); ++i)
	{
		lvi.iItem = i;
		ctrlDirs.InsertItem(&lvi);
	}
}

void ShareGroupsPage::checkShareListVersion(int64_t version)
{
	if (shareListVersion != version)
	{
		insertDirectories();
		showShareGroupDirectories(ctrlGroup.GetCurSel());
		shareListVersion = version;
	}
}

void ShareGroupsPage::showShareGroupDirectories(int index)
{
	if (index < 0 || index >= (int) groups.size()) return;
	boost::unordered_set<string> selectedDirs;
	auto sm = ShareManager::getInstance();
	if (!sm->getShareGroupDirectories(groups[index].id, selectedDirs)) return;
	int count = ctrlDirs.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		BOOL checkState = FALSE;
		if (i < (int) dirs.size())
		{
			string dir = Text::fromT(Text::toLower(dirs[i].realPath));
			checkState = selectedDirs.find(dir) != selectedDirs.end();
		}
		ctrlDirs.SetCheckState(i, checkState);
	}
}

LRESULT ShareGroupsPage::onGetDispInfo(int, LPNMHDR pnmh, BOOL&)
{
	NMLVDISPINFO* di = (NMLVDISPINFO*)pnmh;
	if (di && (di->item.mask & LVIF_TEXT))
	{
		int index = di->item.iItem;
		if (index >= 0 && index < (int) dirs.size())
			di->item.pszText = const_cast<TCHAR*>(dirs[index].realPath.c_str());
	}
	return 0;
}

LRESULT ShareGroupsPage::onItemChanged(int, LPNMHDR pnmh, BOOL&)
{
	const NMITEMACTIVATE* l = (NMITEMACTIVATE*)pnmh;
	if (l->iItem != -1 && !changed && (l->uNewState & LVIS_STATEIMAGEMASK) != (l->uOldState & LVIS_STATEIMAGEMASK))
	{
		changed = true;
		GetDlgItem(IDC_SAVE).EnableWindow(TRUE);
	}
	return 0;
}

LRESULT ShareGroupsPage::onSelectGroup(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	int index = ctrlGroup.GetCurSel();
	showShareGroupDirectories(index);
	GetDlgItem(IDC_REMOVE).EnableWindow(index != 0);
	if (changed)
	{
		GetDlgItem(IDC_SAVE).EnableWindow(FALSE);
		changed = false;
	}
	return 0;
}

LRESULT ShareGroupsPage::onAdd(WORD, WORD, HWND, BOOL&)
{
	LineDlg dlg;
	dlg.title = TSTRING(ADD_SHARE_GROUP_TITLE);
	dlg.description = TSTRING(ENTER_SHARE_GROUP_NAME);
	dlg.allowEmpty = false;
	dlg.icon = IconBitmaps::FINISHED_UPLOADS;
	if (dlg.DoModal(m_hWnd) != IDOK) return 0;
	list<string> shareList;
	CID id;
	try
	{
		ShareManager::getInstance()->addShareGroup(Text::fromT(dlg.line), shareList, id);
	}
	catch (ShareException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		return 0;
	}
	groups.emplace_back(ShareGroupInfo{id, dlg.line, 1});
	sortShareGroups();
	insertShareGroups(id);
	return 0;
}

LRESULT ShareGroupsPage::onRemove(WORD, WORD, HWND, BOOL&)
{
	int index = ctrlGroup.GetCurSel();
	if (index <= 0 || index >= (int) groups.size()) return 0;
	if (MessageBox(CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
	ShareManager::getInstance()->removeShareGroup(groups[index].id);
	groups.erase(groups.begin() + index);
	sortShareGroups();
	insertShareGroups(CID());
	return 0;
}

LRESULT ShareGroupsPage::onSaveChanges(WORD, WORD, HWND, BOOL&)
{
	if (!changed) return 0;
	int index = ctrlGroup.GetCurSel();
	if (index < 0 || index >= (int) groups.size()) return 0;
	int count = ctrlDirs.GetItemCount();
	list<string> shareList;
	for (int i = 0; i < count; ++i)
	{
		if (i >= (int) dirs.size()) break;
		if (ctrlDirs.GetCheckState(i)) shareList.push_back(Text::fromT(dirs[i].realPath));
	}
	try
	{
		ShareManager::getInstance()->updateShareGroup(groups[index].id, Text::fromT(groups[index].name), shareList);
	}
	catch (ShareException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		return 0;
	}
	changed = false;
	GetDlgItem(IDC_SAVE).EnableWindow(FALSE);
	return 0;
}

LRESULT ShareOptionsPage::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	WinUtil::translate(*this, texts2);
	
	CUpDownCtrl updown;
	SET_MIN_MAX(IDC_MIN_FILE_SIZE_SPIN, 1, 1000);
	//SET_MIN_MAX(IDC_REFRESH_SPIN, 0, 3000);
	SET_MIN_MAX(IDC_MAX_HASH_SPEED_SPIN, 0, 999);
	
	return TRUE;
}

void ShareOptionsPage::fixControls()
{
	const BOOL state = IsDlgButtonChecked(IDC_TTH_IN_STREAM) != 0;
	::EnableWindow(GetDlgItem(IDC_CAPTION_MIN_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_MIN_FILE_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MB), state);
}
