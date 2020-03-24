#include "stdafx.h"
#include "UserListColors.h"
#include "WinUtil.h"
#include "ImageLists.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_STATIC_ULC, ResourceManager::SETTINGS_USER_LIST },
	{ IDC_CHANGE_COLOR, ResourceManager::SETTINGS_CHANGE },
	{ IDC_USERLIST, ResourceManager::USERLIST_ICONS },
	//{ IDC_IMAGEBROWSE, ResourceManager::BROWSE }, // [~] JhaoDa, not necessary any more
	{ IDC_HUB_POSITION_TEXT, ResourceManager::HUB_USERS_POSITION_TEXT },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_USERLIST_IMAGE, SettingsManager::USERLIST_IMAGE, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

static const SettingsManager::IntSetting settings[] =
{
	SettingsManager::NORMAL_COLOR,
	SettingsManager::FAVORITE_COLOR,
	SettingsManager::TEXT_ENEMY_FORE_COLOR, // ???
	SettingsManager::RESERVED_SLOT_COLOR,
	SettingsManager::IGNORED_COLOR,
	SettingsManager::FIREBALL_COLOR,
	SettingsManager::SERVER_COLOR,
	SettingsManager::OP_COLOR,
	SettingsManager::PASSIVE_COLOR,
	SettingsManager::CHECKED_COLOR,
	SettingsManager::BAD_CLIENT_COLOR,
	SettingsManager::BAD_FILELIST_COLOR
};

static const ResourceManager::Strings strings[] =
{
	ResourceManager::SETTINGS_COLOR_NORMAL,
	ResourceManager::SETTINGS_COLOR_FAVORITE,
	ResourceManager::FAV_ENEMY_USER,
	ResourceManager::SETTINGS_COLOR_RESERVED,
	ResourceManager::SETTINGS_COLOR_IGNORED,
	ResourceManager::COLOR_FAST,
	ResourceManager::COLOR_SERVER,
	ResourceManager::COLOR_OP,
	ResourceManager::COLOR_PASSIVE,
	ResourceManager::SETTINGS_COLOR_FULL_CHECKED,
	ResourceManager::SETTINGS_COLOR_BAD_CLIENT,
	ResourceManager::SETTINGS_COLOR_BAD_FILELIST
};

LRESULT UserListColors::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	
	ctrlList.Attach(GetDlgItem(IDC_USERLIST_COLORS));
	ctrlPreview.Attach(GetDlgItem(IDC_PREVIEW));
	ctrlPreview.SetBackgroundColor(Colors::g_bgColor);
	
	for (int i = 0; i < numberOfColors; i++)
	{
		colors[i] = static_cast<uint32_t>(g_settings->get(settings[i]));
		ctrlList.AddString(CTSTRING_I(strings[i]));
	}
	ctrlList.SetCurSel(0);

	ctrlHubPosition.Attach(GetDlgItem(IDC_HUB_POSITION_COMBO));
	ctrlHubPosition.AddString(CTSTRING(TABS_LEFT)); // 0
	ctrlHubPosition.AddString(CTSTRING(TABS_RIGHT)); // 1
	ctrlHubPosition.SetCurSel(SETTING(HUB_POSITION));
	
	// for Custom Themes
	imgUsers.LoadFromResourcePNG(IDR_USERS); // TODO уже загружен
	GetDlgItem(IDC_STATIC_USERLIST).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP) imgUsers));
	SetDlgItemText(IDC_USERS_LINK, CTSTRING(USERLIST_ICONS_LINK));
	linkUsers.init(GetDlgItem(IDC_USERS_LINK), CTSTRING(USERLIST_ICONS_LINK));

	refreshPreview();
	
	return TRUE;
}

LRESULT UserListColors::onChangeColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	int index = ctrlList.GetCurSel();
	if (index == -1) return 0;
	CColorDialog d(colors[index], 0, hWndCtl);
	if (d.DoModal() == IDOK)
	{
		colors[index] = d.GetColor();
		refreshPreview();
	}
	return TRUE;
}

void UserListColors::refreshPreview()
{
	CHARFORMAT2 cf;
	ctrlPreview.SetWindowText(_T(""));
	cf.dwMask = CFM_COLOR;
	cf.dwEffects = 0;
	
	for (int i = 0; i < numberOfColors; i++)
	{
		cf.crTextColor = colors[i];
		ctrlPreview.SetSelectionCharFormat(cf);
		tstring str = TSTRING_I(strings[i]);
		if (i) str.insert(0, _T("\r\n"));
		ctrlPreview.AppendText(str.c_str());
	}
	
	ctrlPreview.InvalidateRect(NULL);
}

void UserListColors::write()
{
	PropPage::write(*this, items);
	for (int i = 0; i < numberOfColors; i++)
		g_settings->set(settings[i], static_cast<int>(colors[i]));
	
	g_userImage.reinit(); // TODO: call reinit only when USERLIST_IMAGE changes
	
	g_settings->set(SettingsManager::HUB_POSITION, ctrlHubPosition.GetCurSel());
	//HubFrame::UpdateLayout(); // Обновляем отображение. Нужно как-то вызвать эту функцию.
}

void UserListColors::browseForPic(int dlgItem)
{
	CEdit edit(GetDlgItem(dlgItem));
	tstring x;
	WinUtil::getWindowText(edit, x);
	if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
		edit.SetWindowText(x.c_str());
}

LRESULT UserListColors::onImageBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	browseForPic(IDC_USERLIST_IMAGE);
	return 0;
}
