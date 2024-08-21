/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#include "stdafx.h"
#include "ShareMiscPage.h"
#include "LineDlg.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "ImageLists.h"
#include "../client/SettingsManager.h"
#include "../client/ShareManager.h"
#include "../client/MediaInfoLib.h"
#include "../client/ConfCore.h"

#ifdef OSVER_WIN_XP
#include "../client/SysVersion.h"
#endif

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const WinUtil::TextItem texts1[] =
{
	{ IDC_SG_CAPTION, ResourceManager::SHARE_GROUP },
	{ IDC_REMOVE, ResourceManager::REMOVE2 },
	{ IDC_ADD, ResourceManager::ADD3 },
	{ IDC_SG_CONTENTS_CAPTION, ResourceManager::SETTINGS_SHARE_GROUP_CONTENTS },
	{ IDC_SAVE, ResourceManager::SETTINGS_SAVE_CHANGES },
	{ 0, ResourceManager::Strings() }
};

static const DialogLayout::Align align1 = { 7, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 8, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align align3 = { 14, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align4 = { 15, DialogLayout::SIDE_RIGHT, U_DU(4) };
static const DialogLayout::Align align5 = { 17, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align6 = { 18, DialogLayout::SIDE_RIGHT, U_DU(4) };

static const DialogLayout::Item layoutItems2[] =
{
	{ IDC_SHARING_OPTIONS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_SHAREHIDDEN, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SHARESYSTEM, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SHAREVIRTUAL, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_SHARING_AUTO_REFRESH, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_REFRESH_SHARE_ON_STARTUP, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_REFRESH_SHARE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_AUTO_REFRESH_TIME, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_CAPTION_MINUTES, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align2 },
	{ IDC_CAPTION_0_TO_DISABLE, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align1 },
	{ IDC_SHARING_HASHING, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_USE_FAST_HASH, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_TTH_IN_STREAM, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_CAPTION_MIN_SIZE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_MIN_FILE_SIZE, 0, UNSPEC, UNSPEC, 0, &align3 },
	{ IDC_SETTINGS_MB, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align4 },
	{ IDC_CAPTION_MAX_HASH_SPEED, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_MAX_HASH_SPEED, 0, UNSPEC, UNSPEC, 0, &align5 },
	{ IDC_SETTINGS_MBS, FLAG_TRANSLATE, AUTO, UNSPEC, 0, &align6 }
};

static const DialogLayout::Item layoutItems3[] =
{
	{ IDC_ENABLE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_PARSE_AUDIO, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_PARSE_VIDEO, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_FORCE_UPDATE, FLAG_TRANSLATE, AUTO, UNSPEC }
};

static const PropPage::Item items[] =
{
	{ IDC_SHAREHIDDEN, Conf::SHARE_HIDDEN, PropPage::T_BOOL },
	{ IDC_SHARESYSTEM, Conf::SHARE_SYSTEM, PropPage::T_BOOL },
	{ IDC_SHAREVIRTUAL, Conf::SHARE_VIRTUAL, PropPage::T_BOOL },
	{ IDC_REFRESH_SHARE_ON_STARTUP, Conf::AUTO_REFRESH_ON_STARTUP, PropPage::T_BOOL },
	{ IDC_AUTO_REFRESH_TIME, Conf::AUTO_REFRESH_TIME, PropPage::T_INT },
	{ IDC_USE_FAST_HASH, Conf::FAST_HASH, PropPage::T_BOOL },
	{ IDC_TTH_IN_STREAM, Conf::SAVE_TTH_IN_NTFS_FILESTREAM, PropPage::T_BOOL },
	{ IDC_MIN_FILE_SIZE, Conf::SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM, PropPage::T_INT},
	{ IDC_MAX_HASH_SPEED, Conf::MAX_HASH_SPEED, PropPage::T_INT },
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
	ADD_TAB(pageShareMediaInfo, ShareMediaInfoPage, SETTINGS_MEDIA_INFO_PAGE);

	ctrlTabs.SetCurSel(0);
	changeTab();

	PropPage::read(pageShareOptions->m_hWnd, items);
	pageShareOptions->fixControls();
	pageShareMediaInfo->readSettings();
	pageShareMediaInfo->fixControls();
	return TRUE;
}

void ShareMiscPage::changeTab()
{
	int pos = ctrlTabs.GetCurSel();
	pageShareGroups->ShowWindow(SW_HIDE);
	pageShareOptions->ShowWindow(SW_HIDE);
	pageShareMediaInfo->ShowWindow(SW_HIDE);

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

		case 1:
			hwnd = pageShareOptions->m_hWnd;
			break;

		default:
			hwnd = pageShareMediaInfo->m_hWnd;
			pageShareMediaInfo->updateStatus();
	}
	::MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	::ShowWindow(hwnd, SW_SHOW);
}

void ShareMiscPage::write()
{
	PropPage::write(pageShareOptions->m_hWnd, items);
	pageShareMediaInfo->writeSettings();
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
	ctrlAction.Attach(GetDlgItem(IDC_REMOVE));

#ifdef OSVER_WIN_XP
	if (SysVersion::isOsVistaPlus())
#endif
		ctrlAction.SetButtonStyle(ctrlAction.GetButtonStyle() | BS_SPLITBUTTON);

	auto sm = ShareManager::getInstance();
	vector<ShareManager::ShareGroupInfo> smGroups;
	sm->getShareGroups(smGroups);
	groups.clear();
	groups.emplace_back(ShareGroupInfo{CID(), TSTRING(SHARE_GROUP_DEFAULT), 0});
	for (const auto& sg : smGroups)
		groups.emplace_back(ShareGroupInfo{sg.id, Text::toT(sg.name), 1});
	sortShareGroups();

	ctrlDirs.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
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

void ShareGroupsPage::updateShareGroup(int index, const tstring& newName)
{
	int count = ctrlDirs.GetItemCount();
	list<string> shareList;
	for (int i = 0; i < count; ++i)
	{
		if (i >= (int) dirs.size()) break;
		if (ctrlDirs.GetCheckState(i)) shareList.push_back(Text::fromT(dirs[i].realPath));
	}
	try
	{
		ShareManager::getInstance()->updateShareGroup(groups[index].id,
			newName.empty() ? Text::fromT(groups[index].name) : Text::fromT(newName), shareList);
	}
	catch (ShareException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
		return;
	}
	if (!newName.empty())
	{
		CID cid = groups[index].id;
		groups[index].name = newName;
		sortShareGroups();
		insertShareGroups(cid);
	}
	changed = false;
	GetDlgItem(IDC_SAVE).EnableWindow(FALSE);
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

LRESULT ShareGroupsPage::onAction(WORD wNotifyCode, WORD wID, HWND hwnd, BOOL&)
{
	if (hwnd == nullptr && wNotifyCode == 0 && action != wID)
	{
		action = wID;
		ctrlAction.SetWindowText(action == IDC_RENAME ? CTSTRING(RENAME) : CTSTRING(REMOVE));
	}
	int index = ctrlGroup.GetCurSel();
	if (index <= 0 || index >= (int) groups.size()) return 0;
	if (action == IDC_REMOVE)
	{
		if (MessageBox(CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
		ShareManager::getInstance()->removeShareGroup(groups[index].id);
		groups.erase(groups.begin() + index);
		sortShareGroups();
		insertShareGroups(CID());
	}
	else
	{
		LineDlg dlg;
		dlg.title = TSTRING(RENAME_SHARE_GROUP_TITLE);
		dlg.description = TSTRING(ENTER_SHARE_GROUP_NAME);
		dlg.line = groups[index].name;
		dlg.allowEmpty = false;
		dlg.icon = IconBitmaps::FINISHED_UPLOADS;
		if (dlg.DoModal(m_hWnd) != IDOK || groups[index].name == dlg.line) return 0;
		updateShareGroup(index, dlg.line);
	}
	return 0;
}

LRESULT ShareGroupsPage::onSaveChanges(WORD, WORD, HWND, BOOL&)
{
	if (!changed) return 0;
	int index = ctrlGroup.GetCurSel();
	if (index < 0 || index >= (int) groups.size()) return 0;
	updateShareGroup(index, Util::emptyStringT);
	return 0;
}

LRESULT ShareGroupsPage::onSplitAction(int, LPNMHDR pnmh, BOOL&)
{
	NMBCDROPDOWN* nm = reinterpret_cast<NMBCDROPDOWN*>(pnmh);
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
	menu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME));

	POINT pt = { 0, nm->rcButton.bottom };
	CButton(pnmh->hwndFrom).ClientToScreen(&pt);
	menu.TrackPopupMenu(0, pt.x, pt.y, m_hWnd);
	return 0;
}

LRESULT ShareOptionsPage::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	DialogLayout::layout(m_hWnd, layoutItems2, _countof(layoutItems2));

	CUpDownCtrl spin1(GetDlgItem(IDC_MIN_FILE_SIZE_SPIN));
	spin1.SetRange32(1, 1000);
	spin1.SetBuddy(GetDlgItem(IDC_MIN_FILE_SIZE));

	CUpDownCtrl spin2(GetDlgItem(IDC_MAX_HASH_SPEED_SPIN));
	spin2.SetRange32(0, 999);
	spin2.SetBuddy(GetDlgItem(IDC_MAX_HASH_SPEED));

	return TRUE;
}

void ShareOptionsPage::fixControls()
{
	const BOOL state = IsDlgButtonChecked(IDC_TTH_IN_STREAM) != 0;
	GetDlgItem(IDC_CAPTION_MIN_SIZE).EnableWindow(state);
	GetDlgItem(IDC_MIN_FILE_SIZE).EnableWindow(state);
	GetDlgItem(IDC_SETTINGS_MB).EnableWindow(state);
}

LRESULT ShareMediaInfoPage::onInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
	DialogLayout::layout(m_hWnd, layoutItems3, _countof(layoutItems3));

	CStatic placeholder(GetDlgItem(IDC_STATUS));
	RECT rc;
	placeholder.GetWindowRect(&rc);
	placeholder.DestroyWindow();
	ScreenToClient(&rc);
	ctrlStatus.Create(m_hWnd, rc, nullptr, WS_CHILD | WS_VISIBLE, 0, IDC_STATUS);

	ctrlEnable.Attach(GetDlgItem(IDC_ENABLE));
	ctrlParseAudio.Attach(GetDlgItem(IDC_PARSE_AUDIO));
	ctrlParseVideo.Attach(GetDlgItem(IDC_PARSE_VIDEO));
	ctrlForceUpdate.Attach(GetDlgItem(IDC_FORCE_UPDATE));

	return 0;
}

LRESULT ShareMediaInfoPage::onEnable(WORD, WORD, HWND, BOOL&)
{
	if (ctrlEnable.GetCheck() == BST_CHECKED && !MediaInfoLib::instance.isOpen())
	{
		MediaInfoLib::instance.init();
		showStatus();
	}
	fixControls();
	return 0;
}

void ShareMediaInfoPage::readSettings()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	unsigned options = ss->getInt(Conf::MEDIA_INFO_OPTIONS);
	bool forceUpdate = ss->getBool(Conf::MEDIA_INFO_FORCE_UPDATE);
	ss->unlockRead();
	ctrlEnable.SetCheck((options & Conf::MEDIA_INFO_OPTION_ENABLE) ? BST_CHECKED : BST_UNCHECKED);
	ctrlParseAudio.SetCheck((options & Conf::MEDIA_INFO_OPTION_SCAN_AUDIO) ? BST_CHECKED : BST_UNCHECKED);
	ctrlParseVideo.SetCheck((options & Conf::MEDIA_INFO_OPTION_SCAN_VIDEO) ? BST_CHECKED : BST_UNCHECKED);
	ctrlForceUpdate.SetCheck(forceUpdate ? BST_CHECKED : BST_UNCHECKED);
}

void ShareMediaInfoPage::writeSettings()
{
	int options = 0;
	if (ctrlEnable.GetCheck() == BST_CHECKED) options |= Conf::MEDIA_INFO_OPTION_ENABLE;
	if (ctrlParseAudio.GetCheck() == BST_CHECKED) options |= Conf::MEDIA_INFO_OPTION_SCAN_AUDIO;
	if (ctrlParseVideo.GetCheck() == BST_CHECKED) options |= Conf::MEDIA_INFO_OPTION_SCAN_VIDEO;
	bool forceUpdate = ctrlForceUpdate.GetCheck() == BST_CHECKED;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	ss->setInt(Conf::MEDIA_INFO_OPTIONS, options);
	ss->setBool(Conf::MEDIA_INFO_FORCE_UPDATE, forceUpdate);
	ss->unlockWrite();
}

void ShareMediaInfoPage::fixControls()
{
	const BOOL state = ctrlEnable.GetCheck() == BST_CHECKED;
	ctrlParseAudio.EnableWindow(state);
	ctrlParseVideo.EnableWindow(state);
	ctrlForceUpdate.EnableWindow(state);
}

void ShareMediaInfoPage::disableControls()
{
	ctrlEnable.SetCheck(BST_UNCHECKED);
	ctrlEnable.EnableWindow(FALSE);
	ctrlParseAudio.EnableWindow(FALSE);
	ctrlParseVideo.EnableWindow(FALSE);
	ctrlForceUpdate.EnableWindow(FALSE);
}

void ShareMediaInfoPage::showStatus()
{
	int icon = -1;
	tstring text;
	if (MediaInfoLib::instance.isOpen())
	{
		icon = IconBitmaps::STATUS_SUCCESS;
		text = TSTRING_F(MEDIA_INFO_STATUS_SUCCESS, MediaInfoLib::instance.getLibraryVersion());
	}
	else if (!File::isExist(MediaInfoLib::getLibraryPath()))
	{
		icon = IconBitmaps::WARNING;
		text = TSTRING(MEDIA_INFO_STATUS_NOT_FOUND);
		disableControls();
	}
	else if (MediaInfoLib::instance.isError())
	{
		icon = IconBitmaps::STATUS_FAILURE;
		text = TSTRING(MEDIA_INFO_STATUS_ERROR);
		disableControls();
	}
	else
		return;
	ctrlStatus.setImage(icon);
	ctrlStatus.setText(text);
	ctrlStatus.Invalidate();
}

void ShareMediaInfoPage::updateStatus()
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	unsigned options = ss->getInt(Conf::MEDIA_INFO_OPTIONS);
	ss->unlockRead();
	if (options & Conf::MEDIA_INFO_OPTION_ENABLE) MediaInfoLib::instance.init();
	showStatus();
}
