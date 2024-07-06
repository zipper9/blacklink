#include "stdafx.h"
#include "UserListColorsTab.h"
#include "WinUtil.h"
#include "Colors.h"
#include "ImageLists.h"
#include "DialogLayout.h"
#include "BrowseFile.h"

extern SettingsManager *g_settings;

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 5,  DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_STYLES,     FLAG_TRANSLATE, UNSPEC, UNSPEC,            },
	{ IDC_CAPTION_PREVIEW,    FLAG_TRANSLATE, UNSPEC, UNSPEC,            },
	{ IDC_CHANGE_COLOR,       FLAG_TRANSLATE, UNSPEC, UNSPEC,            },
	{ IDC_SET_DEFAULT,        FLAG_TRANSLATE, UNSPEC, UNSPEC,            },
	{ IDC_HUB_POSITION_TEXT,  FLAG_TRANSLATE, AUTO,   UNSPEC,            },
	{ IDC_HUB_POSITION_COMBO, 0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_USERLIST,           FLAG_TRANSLATE, AUTO,   UNSPEC             }
};

static const SettingsManager::IntSetting settings[] =
{
	SettingsManager::NORMAL_COLOR,
	SettingsManager::FAVORITE_COLOR,
	SettingsManager::FAV_BANNED_COLOR,
	SettingsManager::RESERVED_SLOT_COLOR,
	SettingsManager::IGNORED_COLOR,
	SettingsManager::FIREBALL_COLOR,
	SettingsManager::SERVER_COLOR,
	SettingsManager::OP_COLOR,
	SettingsManager::PASSIVE_COLOR,
	SettingsManager::CHECKED_COLOR,
	SettingsManager::CHECKED_FAIL_COLOR
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
	ResourceManager::SETTINGS_COLOR_CHECK_FAILED
};

LRESULT UserListColorsTab::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	static const ResourceManager::Strings userNames[] =
	{
		ResourceManager::USER_DESC_OPERATOR,
		ResourceManager::USER_DESC_SERVER,
		ResourceManager::USER_DESC_FAST,
		ResourceManager::USER_DESC_NORMAL,
		ResourceManager::USER_DESC_SLOW
	};

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	origImagePath = imagePath = Text::toT(g_settings->get(SettingsManager::USERLIST_IMAGE));

	ctrlList.Attach(GetDlgItem(IDC_USERLIST_COLORS));
	ctrlPreview.Attach(GetDlgItem(IDC_PREVIEW));
	
	for (int i = 0; i < numberOfColors; i++)
		ctrlList.AddString(CTSTRING_I(strings[i]));
	ctrlList.SetCurSel(0);

	ctrlHubPosition.Attach(GetDlgItem(IDC_HUB_POSITION_COMBO));
	ctrlHubPosition.AddString(CTSTRING(POSITION_LEFT)); // 0
	ctrlHubPosition.AddString(CTSTRING(POSITION_RIGHT)); // 1
	ctrlHubPosition.SetCurSel(SETTING(HUB_POSITION));
	
	ctrlCustomImage.Attach(GetDlgItem(IDC_USERLIST));
	ctrlImagePath.Attach(GetDlgItem(IDC_USERLIST_IMAGE));
	ctrlImagePath.SetWindowText(imagePath.c_str());
	ctrlCustomImage.SetCheck(imagePath.empty() ? BST_UNCHECKED : BST_CHECKED);
	BOOL state = !imagePath.empty();
	ctrlImagePath.EnableWindow(state);
	GetDlgItem(IDC_IMAGEBROWSE).EnableWindow(state);

	ctrlUserList.Attach(GetDlgItem(IDC_STATIC_USERLIST));
	loadImage(!imagePath.empty(), true);

	refreshPreview();
	
	tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	tooltip.SetDelayTime(TTDT_AUTOPOP, 10000);
	tooltip.SetMaxTipWidth(255);
	RECT rc = { 0, 0, 0, 16 };
	unsigned idTool = 0;
	for (int i = 0; i < 2; i++)
	{
		const tstring& connType = i == 1 ? TSTRING(USER_DESC_PASSIVE) : TSTRING(USER_DESC_ACTIVE);
		for (int j = 0; j < 2; j++)
			for (int k = 0; k < 5; k++)
			{
				tstring text = TSTRING_I(userNames[k]);
				text += _T('\n');
				text += connType;
				if (j)
				{
					text += _T('\n');
					text += TSTRING(USER_DESC_AWAY);
				}
				rc.left = rc.right;
				rc.right += 16;
				CToolInfo ti(TTF_SUBCLASS, ctrlUserList, ++idTool, &rc, const_cast<TCHAR*>(text.c_str()));
				tooltip.AddTool(&ti);
			}
	}	
	tooltip.Activate(TRUE);

	return TRUE;
}

LRESULT UserListColorsTab::onChangeColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = ctrlList.GetCurSel();
	if (index == -1) return 0;
	CColorDialog d(colors[index], CC_FULLOPEN, m_hWnd);
	if (d.DoModal() == IDOK)
	{
		if (colors[index] != d.GetColor())
		{
			colors[index] = d.GetColor();
			if (callback) callback->settingChanged(settings[index]);
			refreshPreview();
		}
	}
	return 0;
}

LRESULT UserListColorsTab::onSetDefault(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = ctrlList.GetCurSel();
	if (index == -1) return 0;
	uint32_t color = static_cast<uint32_t>(g_settings->getDefault(settings[index]));
	if (colors[index] != color)
	{
		colors[index] = color;
		if (callback) callback->settingChanged(settings[index]);
		refreshPreview();
	}
	return 0;
}

void UserListColorsTab::refreshPreview()
{
	CHARFORMAT2 cf;
	ctrlPreview.SetWindowText(_T(""));
	ctrlPreview.SetBackgroundColor(bg);
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

void UserListColorsTab::loadSettings()
{
	for (int i = 0; i < numberOfColors; i++)
		colors[i] = static_cast<uint32_t>(g_settings->get(settings[i]));
	bg = g_settings->get(SettingsManager::BACKGROUND_COLOR);
}

void UserListColorsTab::saveSettings() const
{
	const tstring& path = ctrlCustomImage.GetCheck() == BST_CHECKED ? imagePath : Util::emptyStringT;
	g_settings->set(SettingsManager::USERLIST_IMAGE, Text::fromT(path));
	for (int i = 0; i < numberOfColors; i++)
		g_settings->set(settings[i], static_cast<int>(colors[i]));
	
	if (path != origImagePath) g_userImage.reinit();
	g_settings->set(SettingsManager::HUB_POSITION, ctrlHubPosition.GetCurSel());
}

void UserListColorsTab::getValues(SettingsStore& ss) const
{
	for (int i = 0; i < numberOfColors; i++)
		ss.setIntValue(settings[i], static_cast<int>(colors[i]));
}

void UserListColorsTab::setValues(const SettingsStore& ss)
{
	int val;
	for (int i = 0; i < numberOfColors; i++)
		if (ss.getIntValue(settings[i], val)) colors[i] = val;
	if (ss.getIntValue(SettingsManager::BACKGROUND_COLOR, val)) bg = val;
}

void UserListColorsTab::updateTheme()
{
	refreshPreview();
}

void UserListColorsTab::setBackgroundColor(COLORREF clr)
{
	if (bg != clr)
	{
		bg = clr;
		if (ctrlPreview.m_hWnd)
			refreshPreview();
	}
}

LRESULT UserListColorsTab::onImageBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring x;
	WinUtil::getWindowText(ctrlImagePath, x);
	if (WinUtil::browseFile(x, m_hWnd, false) != IDOK) return 0;
	CImageEx newImg;
	if (!loadImage(newImg, x, true)) return 0;
	imagePath = std::move(x);
	ctrlImagePath.SetWindowText(imagePath.c_str());
	ctrlUserList.SetBitmap((HBITMAP) newImg);
	imgUsers = std::move(newImg);
	return 0;
}

LRESULT UserListColorsTab::onCustomImage(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	bool state = ctrlCustomImage.GetCheck() == BST_CHECKED;
	ctrlImagePath.EnableWindow(state);
	GetDlgItem(IDC_IMAGEBROWSE).EnableWindow(state);
	loadImage(state, false);
	return 0;
}

bool UserListColorsTab::loadImage(CImageEx& newImg, const tstring& path, bool showError)
{
	if (FAILED(newImg.Load(path.c_str())))
	{
		if (showError) MessageBox(CTSTRING(CUSTOM_USERLIST_ERROR_LOADING), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR);
		return false;
	}
	const int width = 20 * 16;
	const int height = 16;
	if (newImg.GetWidth() != width || newImg.GetHeight() != height)
	{
		if (showError) MessageBox(CTSTRING_F(CUSTOM_USERLIST_ERROR_SIZE, width % height), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}

void UserListColorsTab::loadImage(bool useCustom, bool init)
{
	CImageEx img;
	bool isCustom = useCustom && !imagePath.empty() && loadImage(img, imagePath, false);
	if (!isCustom && !customImageLoaded && !init) return;
	if (!isCustom) img.LoadFromResourcePNG(IDR_USERS);
	ctrlUserList.SetBitmap((HBITMAP) img);
	imgUsers = std::move(img);
	customImageLoaded = isCustom;
}
