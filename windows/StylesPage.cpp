#include "stdafx.h"
#include "StylesPage.h"
#include "WinUtil.h"
#include "Colors.h"
#include "DialogLayout.h"
#include "ThemeUtil.h"
#include "ColorUtil.h"
#include "BrowseFile.h"
#include "../client/File.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 2, DialogLayout::SIDE_TOP,    U_PX(-1) };
static const DialogLayout::Align align2 = { 2, DialogLayout::SIDE_BOTTOM, U_PX(-1) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_THEME,       FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_THEME_COMBO, 0,              UNSPEC, UNSPEC                                        },
	{ IDC_EXPORT,      FLAG_TRANSLATE, UNSPEC, UNSPEC, 0, nullptr, nullptr, &align1, &align2 }
};

StylesPage::StylesPage() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES))
{
	SetTitle(m_title.c_str());
	m_psp.dwFlags |= PSP_RTLREADING;
	themeModified = false;
	tabChat.setCallback(this);
	tabUserList.setCallback(this);
	tabProgress.setCallback(this);
	tabOther.setCallback(this);
}

#define ADD_TAB(name, type, text) \
	tcItem.pszText = const_cast<TCHAR*>(CTSTRING(text)); \
	tcItem.lParam = reinterpret_cast<LPARAM>(&name); \
	name.Create(m_hWnd, type::IDD); \
	ctrlTabs.InsertItem(n++, &tcItem);

LRESULT StylesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	ctrlTabs.Attach(GetDlgItem(IDC_TABS));
	TCITEM tcItem;
	tcItem.mask = TCIF_TEXT | TCIF_PARAM;
	tcItem.iImage = -1;

	tabChat.loadSettings();
	tabUserList.loadSettings();
	tabProgress.loadSettings();
	tabOther.loadSettings();

	COLORREF bg = g_settings->get(SettingsManager::BACKGROUND_COLOR);
	tabChat.setBackgroundColor(bg);
	tabUserList.setBackgroundColor(bg);

	currentTheme = Text::toT(g_settings->get(SettingsManager::COLOR_THEME));
	themeModified = g_settings->getBool(SettingsManager::COLOR_THEME_MODIFIED);

	int n = 0;
	ADD_TAB(tabChat, ChatStylesTab, STYLES_CHAT);
	ADD_TAB(tabUserList, UserListColorsTab, STYLES_USER_LIST);
	ADD_TAB(tabProgress, ProgressBarTab, STYLES_PROGRESS_BAR);
	ADD_TAB(tabOther, OtherColorsTab, STYLES_OTHER);
	ctrlTabs.SetCurSel(0);
	changeTab();

	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO));
	getThemeList();

	return TRUE;
}

void StylesPage::changeTab()
{
	int pos = ctrlTabs.GetCurSel();
	tabChat.ShowWindow(SW_HIDE);
	tabUserList.ShowWindow(SW_HIDE);
	tabProgress.ShowWindow(SW_HIDE);
	tabOther.ShowWindow(SW_HIDE);

	CRect rc;
	ctrlTabs.GetClientRect(&rc);
	ctrlTabs.AdjustRect(FALSE, &rc);
	ctrlTabs.MapWindowPoints(m_hWnd, &rc);

	CWindow* wnd;
	switch (pos)
	{
		case 0: wnd = &tabChat; break;
		case 1: wnd = &tabUserList; break;
		case 2: wnd = &tabProgress; break;
		case 3: wnd = &tabOther; break;
		default: return;
	}
	wnd->MoveWindow(&rc);
	wnd->ShowWindow(SW_SHOW);
}

void StylesPage::write()
{
	tabChat.saveSettings();
	tabUserList.saveSettings();
	tabProgress.saveSettings();
	tabOther.saveSettings();

	g_settings->set(SettingsManager::COLOR_THEME, Text::fromT(currentTheme));
	if (themeModified)
		g_settings->set(SettingsManager::COLOR_THEME_MODIFIED, true);
	else
		g_settings->unset(SettingsManager::COLOR_THEME_MODIFIED);
	Colors::init();
}

LRESULT StylesPage::onSelectTheme(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int sel = ctrlTheme.GetCurSel();
	if (sel < 0 || sel >= (int) themes.size()) return 0;
	if (themes[sel].temp) return 0;
	if (themeModified && MessageBox(CTSTRING(ASK_SAVE_THEME), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		if (!saveTheme())
		{
			themes.clear();
			ctrlTheme.ResetContent();
			getThemeList();
		}
		return 0;
	}
	SettingsStore ss;
	const tstring& themeFile = themes[sel].path;
	if (!themeFile.empty())
	{
		string error;
		try
		{
			Util::importDcTheme(themeFile, ss);
		}
		catch (const FileException& e)
		{
			error = STRING(COULD_NOT_OPEN_TARGET_FILE) + e.getError();
		}
		catch (const SimpleXMLException& e)
		{
			error = STRING(COULD_NOT_PARSE_XML_DATA) + e.getError();
		}
		if (!error.empty())
		{
			MessageBox(Text::toT(error).c_str(), getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
			return 0;
		}
		currentTheme = themes[sel].name;
	}
	else
	{
		Util::getDefaultTheme(ss);
		currentTheme.clear();
	}
	tabChat.setValues(ss);
	tabChat.updateTheme();
	tabUserList.setValues(ss);
	tabUserList.updateTheme();
	tabProgress.setValues(ss);
	tabProgress.updateTheme();
	tabOther.setValues(ss);
	tabOther.updateTheme();
	themeModified = false;
	themes.clear();
	ctrlTheme.ResetContent();
	getThemeList();
	return 0;
}

static const WinUtil::FileMaskItem types[] =
{
	{ ResourceManager::FILEMASK_THEME,        _T("*.dctheme") },
	{ ResourceManager::FILEMASK_XML_SETTINGS, _T("*.xml")     },
	{ ResourceManager::FILEMASK_ALL,          _T("*.*")       },
	{ ResourceManager::Strings(),             nullptr         }
};

static const tstring defExt = _T("dctheme");

bool StylesPage::saveTheme()
{
	tstring path;
	if (!currentTheme.empty())
	{
		path = currentTheme;
		tstring copy = TSTRING_F(THEME_COPY, Util::emptyStringT);
		if (path.find(copy) == tstring::npos)
			path = TSTRING_F(THEME_COPY, path);
	}
	tstring themesPath = Text::toT(Util::getThemesPath());
	File::ensureDirectory(themesPath);
	if (WinUtil::browseFile(path, m_hWnd, true, themesPath, WinUtil::getFileMaskString(types).c_str(), defExt.c_str()))
	{
		SettingsStore ss;
		tabChat.getValues(ss);
		tabUserList.getValues(ss);
		tabProgress.getValues(ss);
		tabOther.getValues(ss);
		try
		{
			Util::exportDcTheme(path, &ss);
			tstring fileName = Util::getFileName(path);
			if (stricmp(Util::getFileExtWithoutDot(fileName), defExt) == 0)
				fileName.erase(fileName.length() - (defExt.length() + 1));
			themeModified = false;
			currentTheme = std::move(fileName);
			themes.clear();
			ctrlTheme.ResetContent();
			getThemeList();
			return true;
		}
		catch (Exception& e)
		{
			string error = STRING(COULD_NOT_OPEN_TARGET_FILE) + e.getError();
			MessageBox(Text::toT(error).c_str(), getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
		}
	}
	return false;
}

LRESULT StylesPage::onExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	saveTheme();
	return 0;
}

void StylesPage::getThemeList()
{
	if (themes.empty())
	{
		const string ext = ".dctheme";
		const string fileFindPath = Util::getThemesPath() + "*" + ext;
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			string name = i->getFileName();
			if (name.empty() || i->isDirectory() || i->getSize() == 0)
				continue;
			const tstring wName = Text::toT(name.substr(0, name.length() - ext.length()));
			const tstring path = Text::toT(Util::getThemesPath() + name);
			themes.push_back({ wName, path });
		}

		int sel = -1;
		sort(themes.begin(), themes.end(),
			[](const ThemeInfo& l, const ThemeInfo& r) { return stricmp(l.name, r.name) < 0; });
		themes.insert(themes.begin(), { TSTRING(THEME_DEFAULT_NAME), Util::emptyStringT });
		if (currentTheme.empty()) sel = 0;

		int index = 0;
		bool addModified = false;
		for (const ThemeInfo& theme : themes)
		{
			if (sel < 0 && theme.name == currentTheme) sel = index;
			ctrlTheme.InsertString(index++, theme.name.c_str());
			if (themeModified && index == sel + 1)
				addModified = true;
		}

		if (addModified)
		{
			tstring text = themes[sel].name + TSTRING(THEME_MODIFIED);
			ctrlTheme.InsertString(++sel, text.c_str());
			themes.insert(themes.begin() + sel, { text, Util::emptyStringT, true });
		}

		if (sel < 0)
		{
			if (!currentTheme.empty())
			{
				sel = ctrlTheme.GetCount();
				tstring text = currentTheme;
				if (themeModified) text += TSTRING(THEME_MODIFIED);
				themes.push_back({ text, Util::emptyStringT, true });
				ctrlTheme.InsertString(sel, text.c_str());
			}
			else
				sel = 0;
		}
		ctrlTheme.EnableWindow(ctrlTheme.GetCount() > 1);
		ctrlTheme.SetCurSel(sel);
	}
}

void StylesPage::setThemeModified()
{
	int sel = ctrlTheme.GetCurSel();
	if (sel >= 0)
	{
		bool removeOld = sel != 0 && themes[sel].temp;
		tstring text = themes[sel].name + TSTRING(THEME_MODIFIED);
		themes.insert(themes.begin() + sel + 1, { text, Util::emptyStringT, true });
		ctrlTheme.InsertString(sel + 1, text.c_str());
		if (removeOld)
		{
			themes.erase(themes.begin() + sel);
			ctrlTheme.DeleteString(sel);
		}
		else
			++sel;
		ctrlTheme.SetCurSel(sel);
		ctrlTheme.EnableWindow(ctrlTheme.GetCount() > 1);
	}
	themeModified = true;
}

void StylesPage::settingChanged(int id)
{
	if (!themeModified && Util::isThemeAttribute(id))
		setThemeModified();
}

void StylesPage::intSettingChanged(int id, int value)
{
	if (id == SettingsManager::BACKGROUND_COLOR)
	{
		tabChat.setBackgroundColor(value, true);
		tabUserList.setBackgroundColor(value);
		tabProgress.setBackgroundColor(value);
	}
	else if (id == SettingsManager::PROGRESS_BACK_COLOR)
		tabProgress.setEmptyBarBackground(value);
	else if (id == SettingsManager::DOWNLOAD_BAR_COLOR)
	{
		// Derive PROGRESS_SEGMENT_COLOR from DOWNLOAD_BAR_COLOR
		COLORREF clr = HLS_TRANSFORM(value, 0, -35);
		tabOther.setColor(4, clr);
	}
}
