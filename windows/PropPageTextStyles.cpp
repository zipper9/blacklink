#include "stdafx.h"
#include "OperaColorsPage.h"
#include "PropertiesDlg.h"
#include "WinUtil.h"
#include "Colors.h"
#include "Fonts.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_AVAILABLE_STYLES, ResourceManager::SETCZDC_STYLES            },
	{ IDC_BACK_COLOR,       ResourceManager::SETCZDC_BACK_COLOR        },
	{ IDC_TEXT_COLOR,       ResourceManager::SETCZDC_TEXT_COLOR        },
	{ IDC_TEXT_STYLE,       ResourceManager::SETCZDC_TEXT_STYLE        },
	{ IDC_DEFAULT_STYLES,   ResourceManager::SETCZDC_DEFAULT_STYLE     },
	{ IDC_BOLD_MSG_AUTHORS, ResourceManager::SETTINGS_BOLD_MSG_AUTHORS },
	{ IDC_CZDC_PREVIEW,     ResourceManager::SETTINGS_TEXT_PREVIEW     },
	{ IDC_RESET_TAB_COLOR,  ResourceManager::SETTINGS_SET_DEFAULT      },
	{ IDC_SELECT_TAB_COLOR, ResourceManager::SETTINGS_SELECT_COLOR     },
	{ IDC_STYLES,           ResourceManager::SETTINGS_TEXT_STYLES      },
	{ IDC_THEME,            ResourceManager::SETTINGS_COLOR_THEMES     },
	{ IDC_EXPORT,           ResourceManager::SETTINGS_THEME_EXPORT     },
	{ IDC_CHATCOLORS,       ResourceManager::SETTINGS_COLORS           },
	{ IDC_BAN_COLOR,        ResourceManager::BAN_COLOR_DLG             },
	{ 0,                    ResourceManager::Strings()                 }
};

static const PropPage::Item items[] =
{
	{ IDC_BOLD_MSG_AUTHORS, SettingsManager::BOLD_MSG_AUTHOR, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

PropPageTextStyles::ColorSettings PropPageTextStyles::colors[] =
{
	{ ResourceManager::SETTINGS_SELECT_WINDOW_COLOR, SettingsManager::BACKGROUND_COLOR,            0 },
	{ ResourceManager::SETCZDC_ERROR_COLOR,          SettingsManager::ERROR_COLOR,                 0 },
	{ ResourceManager::PROGRESS_BACK,                SettingsManager::PROGRESS_BACK_COLOR,         0 },
	{ ResourceManager::PROGRESS_SEGMENT,             SettingsManager::PROGRESS_SEGMENT_COLOR,      0 },
	{ ResourceManager::PROGRESS_DOWNLOADED,          SettingsManager::COLOR_DOWNLOADED,            0 },
	{ ResourceManager::PROGRESS_RUNNING,             SettingsManager::COLOR_RUNNING,               0 },
	{ ResourceManager::PROGRESS_RUNNING_COMPLETED,   SettingsManager::COLOR_RUNNING_COMPLETED,     0 },
	{ ResourceManager::BAN_COLOR_DLG,                SettingsManager::BAN_COLOR,                   0 },
#ifdef SCALOLAZ_USE_COLOR_HUB_IN_FAV
	{ ResourceManager::HUB_IN_FAV_BK_COLOR,          SettingsManager::HUB_IN_FAV_BK_COLOR,         0 },
	{ ResourceManager::HUB_IN_FAV_CONNECT_BK_COLOR,  SettingsManager::HUB_IN_FAV_CONNECT_BK_COLOR, 0 },
#endif
};

PropPageTextStyles::PropPageTextStyles() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES))
{
	fg = 0;
	bg = 0;
	SetTitle(m_title.c_str());
	m_psp.dwFlags |= PSP_RTLREADING;
	firstLoad = true;
	defaultThemeSelected = false;
	hMainFont = nullptr;
	hBrush = nullptr;

	textStyles[TS_GENERAL].init(this, ResourceManager::GENERAL_TEXT,
	    SettingsManager::TEXT_GENERAL_BACK_COLOR, SettingsManager::TEXT_GENERAL_FORE_COLOR,
	    SettingsManager::TEXT_GENERAL_BOLD, SettingsManager::TEXT_GENERAL_ITALIC);
	    
	textStyles[TS_MYNICK].init(this, ResourceManager::MY_NICK,
	    SettingsManager::TEXT_MYNICK_BACK_COLOR, SettingsManager::TEXT_MYNICK_FORE_COLOR,
	    SettingsManager::TEXT_MYNICK_BOLD, SettingsManager::TEXT_MYNICK_ITALIC);
	    
	textStyles[TS_MYMSG].init(this, ResourceManager::MY_MESSAGE,
	    SettingsManager::TEXT_MYOWN_BACK_COLOR, SettingsManager::TEXT_MYOWN_FORE_COLOR,
	    SettingsManager::TEXT_MYOWN_BOLD, SettingsManager::TEXT_MYOWN_ITALIC);
	    
	textStyles[TS_PRIVATE].init(this, ResourceManager::PRIVATE_MESSAGE,
	    SettingsManager::TEXT_PRIVATE_BACK_COLOR, SettingsManager::TEXT_PRIVATE_FORE_COLOR,
	    SettingsManager::TEXT_PRIVATE_BOLD, SettingsManager::TEXT_PRIVATE_ITALIC);
	    
	textStyles[TS_SYSTEM].init(this, ResourceManager::SYSTEM_MESSAGE,
	    SettingsManager::TEXT_SYSTEM_BACK_COLOR, SettingsManager::TEXT_SYSTEM_FORE_COLOR,
	    SettingsManager::TEXT_SYSTEM_BOLD, SettingsManager::TEXT_SYSTEM_ITALIC);
	    
	textStyles[TS_SERVER].init(this, ResourceManager::SERVER_MESSAGE,
	    SettingsManager::TEXT_SERVER_BACK_COLOR, SettingsManager::TEXT_SERVER_FORE_COLOR,
	    SettingsManager::TEXT_SERVER_BOLD, SettingsManager::TEXT_SERVER_ITALIC);
	    
	textStyles[TS_TIMESTAMP].init(this, ResourceManager::TIMESTAMP,
	    SettingsManager::TEXT_TIMESTAMP_BACK_COLOR, SettingsManager::TEXT_TIMESTAMP_FORE_COLOR,
	    SettingsManager::TEXT_TIMESTAMP_BOLD, SettingsManager::TEXT_TIMESTAMP_ITALIC);
	    
	textStyles[TS_URL].init(this, ResourceManager::TEXT_STYLE_URL,
	    SettingsManager::TEXT_URL_BACK_COLOR, SettingsManager::TEXT_URL_FORE_COLOR,
	    SettingsManager::TEXT_URL_BOLD, SettingsManager::TEXT_URL_ITALIC);
	    
	textStyles[TS_FAVORITE].init(this, ResourceManager::FAV_USER,
	    SettingsManager::TEXT_FAV_BACK_COLOR, SettingsManager::TEXT_FAV_FORE_COLOR,
	    SettingsManager::TEXT_FAV_BOLD, SettingsManager::TEXT_FAV_ITALIC);
	    
	textStyles[TS_FAV_ENEMY].init(this, ResourceManager::FAV_ENEMY_USER,
	    SettingsManager::TEXT_ENEMY_BACK_COLOR, SettingsManager::TEXT_ENEMY_FORE_COLOR,
	    SettingsManager::TEXT_ENEMY_BOLD, SettingsManager::TEXT_ENEMY_ITALIC);
	    
	textStyles[TS_OP].init(this, ResourceManager::OPERATOR,
	    SettingsManager::TEXT_OP_BACK_COLOR, SettingsManager::TEXT_OP_FORE_COLOR,
	    SettingsManager::TEXT_OP_BOLD, SettingsManager::TEXT_OP_ITALIC);

	preview.disableChatCache();
}

PropPageTextStyles::~PropPageTextStyles()
{
	if (hMainFont) DeleteObject(hMainFont);
	if (hBrush) DeleteObject(hBrush);
}

void PropPageTextStyles::loadSettings()
{
	tstring fontString = Text::toT(SETTING(TEXT_FONT));
	memset(&mainFont, 0, sizeof(mainFont));
	Fonts::decodeFont(fontString, mainFont);

	fg = SETTING(TEXT_COLOR);
	bg = SETTING(BACKGROUND_COLOR);

	if (firstLoad)
	{
		savedFont = std::move(fontString);
		savedColors[0] = fg;
		savedColors[1] = bg;
	}

	for (int i = 0; i < TS_LAST; i++)
	{
		textStyles[i].loadSettings();
		_tcscpy(textStyles[i].szFaceName, mainFont.lfFaceName);
		textStyles[i].bCharSet = mainFont.lfCharSet;
		textStyles[i].yHeight = mainFont.lfHeight;
		if (firstLoad)
		{
			textStyles[i].savedBackColor = textStyles[i].crBackColor;
			textStyles[i].savedTextColor = textStyles[i].crTextColor;
			textStyles[i].savedEffects = textStyles[i].dwEffects;
		}
	}

	for (int i = 0; i < _countof(colors); i++)
	{
		colors[i].value = SettingsManager::get((SettingsManager::IntSetting) colors[i].setting);
		if (firstLoad) colors[i].savedValue = colors[i].value;
	}

	firstLoad = false;
}

void PropPageTextStyles::saveSettings() const
{
	g_settings->set(SettingsManager::TEXT_FONT, Text::fromT(WinUtil::encodeFont(mainFont)));
	g_settings->set(SettingsManager::TEXT_COLOR, (int) fg);
	g_settings->set(SettingsManager::BACKGROUND_COLOR, (int) bg);
	
	for (int i = 1; i < _countof(colors); i++)
		g_settings->set(SettingsManager::IntSetting(colors[i].setting), (int) colors[i].value);
	
	for (int i = 0; i < TS_LAST; i++)
		textStyles[i].saveSettings();
}

void PropPageTextStyles::restoreSettings()
{
	g_settings->set(SettingsManager::TEXT_FONT, Text::fromT(savedFont));
	g_settings->set(SettingsManager::TEXT_COLOR, (int) savedColors[0]);
	g_settings->set(SettingsManager::BACKGROUND_COLOR, (int) savedColors[1]);
	
	for (int i = 1; i < _countof(colors); i++)
		g_settings->set(SettingsManager::IntSetting(colors[i].setting), (int) colors[i].savedValue);
	
	for (int i = 0; i < TS_LAST; i++)
		textStyles[i].restoreSettings();
}

LRESULT PropPageTextStyles::onSelectColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int color;
	SettingsManager::IntSetting key;
	switch (wID)
	{
		case IDC_BAN_COLOR:
			color = SETTING(BAN_COLOR);
			key = SettingsManager::BAN_COLOR;
			break;
		default:
			color = SETTING(ERROR_COLOR);
			key = SettingsManager::ERROR_COLOR;
	}
	CColorDialog dlg(color, CC_FULLOPEN);
	if (dlg.DoModal() == IDOK)
		SettingsManager::set(key, (int)dlg.GetColor());
	return 0;
}

LRESULT PropPageTextStyles::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	loadSettings();
	
	WinUtil::translate(*this, texts);
	PropPage::read(*this, items);
	
	lsbList.Attach(GetDlgItem(IDC_TEXT_STYLES));
	lsbList.ResetContent();
	
	for (int i = 0; i < TS_LAST; i++)
		lsbList.AddString(CTSTRING_I(textStyles[i].name));
	lsbList.SetCurSel(0);

	cmdResetTab.Attach(GetDlgItem(IDC_RESET_TAB_COLOR));
	cmdSetTabColor.Attach(GetDlgItem(IDC_SELECT_TAB_COLOR));
	ctrlTabExample.Attach(GetDlgItem(IDC_SAMPLE_TAB_COLOR));
	
	ctrlTabList.Attach(GetDlgItem(IDC_TABCOLOR_LIST));
	ctrlTabList.ResetContent();
	for (int i = 0; i < _countof(colors); i++)
		ctrlTabList.AddString(Text::toT(ResourceManager::getString(colors[i].name)).c_str());
	
	ctrlTabList.SetCurSel(0);
	BOOL unused;
	onTabListChange(0, 0, 0, unused);
	
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO2));
	getThemeList();

	preview.Attach(GetDlgItem(IDC_PREVIEW));
	refreshPreview();

	return TRUE;
}

void PropPageTextStyles::write()
{
	PropPage::write(*this, items);
	
	saveSettings();
	
	if (!tempfile.empty()) File::deleteFile(tempfile);
	Colors::init();
}

void PropPageTextStyles::cancel()
{
	if (!tempfile.empty()) File::deleteFile(tempfile);
}

LRESULT PropPageTextStyles::onEditBackColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	textStyles[index].editBackgroundColor();
	refreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onEditForeColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	textStyles[index].editForegroundColor();
	refreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onEditTextStyle(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	if (!textStyles[index].editTextStyle()) return TRUE;
	
	_tcscpy(mainFont.lfFaceName, textStyles[index].szFaceName);
	mainFont.lfCharSet = textStyles[index].bCharSet;
	mainFont.lfHeight = textStyles[index].yHeight;
	
	if (index == TS_GENERAL)
	{
		if (textStyles[index].dwEffects & CFE_ITALIC)
			mainFont.lfItalic = TRUE;
		if (textStyles[index].dwEffects & CFE_BOLD)
			mainFont.lfWeight = FW_BOLD;
	}
	{
		CLockRedraw<true> lockRedraw(preview);
		for (int i = 0; i < TS_LAST; i++)
		{
			_tcscpy(textStyles[i].szFaceName, mainFont.lfFaceName);
			textStyles[i].bCharSet = mainFont.lfCharSet;
			textStyles[i].yHeight = mainFont.lfHeight;
		}
	}
	
	refreshPreview();
	return TRUE;
}

void PropPageTextStyles::refreshPreview()
{
	HFONT newFont = CreateFontIndirect(&mainFont);
	preview.SetFont(newFont);
	if (hMainFont) DeleteObject(hMainFont);
	hMainFont = newFont;

	preview.SetBackgroundColor(bg);
	
	CHARFORMAT2 old = Colors::g_TextStyleMyNick;
	CHARFORMAT2 old2 = Colors::g_TextStyleTimestamp;
	preview.SetTextStyleMyNick(textStyles[TS_MYNICK]);
	Colors::g_TextStyleTimestamp = textStyles[TS_TIMESTAMP];
	preview.SetWindowText(_T(""));
	
	{
		CLockRedraw<false> lockRedraw(preview);
		for (int i = 0; i < TS_LAST; i++)
		{
			const ChatCtrl::Message message(nullptr, false, true, _T("12:34 "), CTSTRING_I(textStyles[i].name), textStyles[i], false);
			preview.appendText(message, 0);
		}
	}
	preview.InvalidateRect(NULL);
	preview.SetTextStyleMyNick(old);
	Colors::g_TextStyleTimestamp = old2;
}

LRESULT PropPageTextStyles::onDefaultStyles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Fonts::decodeFont(Text::toT(g_settings->getDefault(SettingsManager::TEXT_FONT)), mainFont);
	bg = g_settings->getDefault(SettingsManager::BACKGROUND_COLOR);
	fg = g_settings->getDefault(SettingsManager::TEXT_COLOR);
	
	for (int i = 0; i < TS_LAST; i++)
	{
		textStyles[i].loadDefaultSettings();
		_tcscpy(textStyles[i].szFaceName, mainFont.lfFaceName);
		textStyles[i].bCharSet = mainFont.lfCharSet;
		textStyles[i].yHeight = mainFont.lfHeight;
	}
	
	refreshPreview();
	return TRUE;
}

void PropPageTextStyles::TextStyleSettings::init(
    PropPageTextStyles *parent, ResourceManager::Strings name,
    SettingsManager::IntSetting bgSetting, SettingsManager::IntSetting fgSetting,
    SettingsManager::IntSetting boldSetting, SettingsManager::IntSetting italicSetting)
{
	memset(this, 0, sizeof(CHARFORMAT2));
	cbSize = sizeof(CHARFORMAT2);
	dwMask = CFM_COLOR | CFM_BACKCOLOR | CFM_BOLD | CFM_ITALIC;
	dwReserved = 0;
	
	this->parent = parent;
	this->name = name;
	this->bgSetting = bgSetting;
	this->fgSetting = fgSetting;
	this->boldSetting = boldSetting;
	this->italicSetting = italicSetting;
}

void PropPageTextStyles::TextStyleSettings::loadSettings()
{
	dwEffects = 0;
	crBackColor = g_settings->get(bgSetting);
	crTextColor = g_settings->get(fgSetting);
	if (g_settings->get(boldSetting)) dwEffects |= CFE_BOLD;
	if (g_settings->get(italicSetting)) dwEffects |= CFE_ITALIC;
}

void PropPageTextStyles::TextStyleSettings::loadDefaultSettings()
{
	dwEffects = 0;
	crBackColor = g_settings->getDefault(bgSetting);
	crTextColor = g_settings->getDefault(fgSetting);
	if (g_settings->getDefault(boldSetting)) dwEffects |= CFE_BOLD;
	if (g_settings->getDefault(italicSetting)) dwEffects |= CFE_ITALIC;
}

void PropPageTextStyles::TextStyleSettings::saveSettings() const
{
	g_settings->set(bgSetting, (int) crBackColor);
	g_settings->set(fgSetting, (int) crTextColor);
	BOOL bold = (dwEffects & CFE_BOLD) == CFE_BOLD;
	g_settings->set(boldSetting, bold);
	BOOL italic = (dwEffects & CFE_ITALIC) == CFE_ITALIC;
	g_settings->set(italicSetting, italic);
}

void PropPageTextStyles::TextStyleSettings::restoreSettings() const
{
	g_settings->set(bgSetting, (int) savedBackColor);
	g_settings->set(fgSetting, (int) savedTextColor);
	BOOL bold = (savedEffects & CFE_BOLD) == CFE_BOLD;
	g_settings->set(boldSetting, bold);
	BOOL italic = (savedEffects & CFE_ITALIC) == CFE_ITALIC;
	g_settings->set(italicSetting, italic);
}

void PropPageTextStyles::TextStyleSettings::editBackgroundColor()
{
	CColorDialog d(crBackColor, CC_FULLOPEN, *parent);
	if (d.DoModal() == IDOK)
		crBackColor = d.GetColor();
}

void PropPageTextStyles::TextStyleSettings::editForegroundColor()
{
	CColorDialog d(crTextColor, CC_FULLOPEN, *parent);
	if (d.DoModal() == IDOK)
		crTextColor = d.GetColor();
}

bool PropPageTextStyles::TextStyleSettings::editTextStyle()
{
	LOGFONT font = {0};
	
	_tcscpy(font.lfFaceName, szFaceName);
	font.lfCharSet = bCharSet;
	font.lfHeight = yHeight;
	
	if (dwEffects & CFE_BOLD)
		font.lfWeight = FW_BOLD;
	else
		font.lfWeight = FW_REGULAR;
		
	if (dwEffects & CFE_ITALIC)
		font.lfItalic = TRUE;
	else
		font.lfItalic = FALSE;
		
	CFontDialog d(&font, CF_SCREENFONTS | CF_FORCEFONTEXIST, NULL, *parent);
	if (d.DoModal() != IDOK) return false;

	_tcscpy(szFaceName, font.lfFaceName);
	bCharSet = font.lfCharSet;
	yHeight = font.lfHeight;
		
	if (font.lfWeight == FW_BOLD)
		dwEffects |= CFE_BOLD;
	else
		dwEffects &= ~CFE_BOLD;
			
	if (font.lfItalic)
		dwEffects |= CFE_ITALIC;
	else
		dwEffects &= ~CFE_ITALIC;
	return true;
}

LRESULT PropPageTextStyles::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	lsbList.Detach();
	preview.Detach();
	ctrlTabList.Detach();
	cmdResetTab.Detach();
	cmdSetTabColor.Detach();
	ctrlTabExample.Detach();
	ctrlTheme.Detach();	
	return 0;
}

LRESULT PropPageTextStyles::onImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring themeFile;
	int sel = ctrlTheme.GetCurSel();
	if (sel >= 0 && sel < (int) themes.size()) themeFile = themes[sel].path;
	if (!themeFile.empty())
	{
		if (tempfile.empty()) tempfile = Text::toT(Util::getThemesPath() + "temp.dctmp");
		if (defaultThemeSelected)
		{
			saveSettings();
			SettingsManager::exportDcTheme(tempfile);
		}
		SettingsManager::importDcTheme(themeFile);
		loadSettings();
		restoreSettings();
		defaultThemeSelected = false;
	}
	else
	{
		if (!tempfile.empty())
		{
			SettingsManager::importDcTheme(tempfile);
			loadSettings();
			restoreSettings();
		}
		defaultThemeSelected = true;
	}	
	refreshPreview();
	BOOL unused;
	onTabListChange(0, 0, 0, unused);
	PropertiesDlg::g_needUpdate = true;
	return 0;
}

static const WinUtil::FileMaskItem types[] =
{
	{ ResourceManager::FILEMASK_THEME,        _T("*.dctheme") },
	{ ResourceManager::FILEMASK_XML_SETTINGS, _T("*.xml")     },
	{ ResourceManager::FILEMASK_ALL,          _T("*.*")       },
	{ ResourceManager::Strings(),             nullptr         }
};

static const TCHAR defExt[] = _T("dctheme");

LRESULT PropPageTextStyles::onExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
	if (WinUtil::browseFile(file, m_hWnd, true, Text::toT(Util::getThemesPath()), WinUtil::getFileMaskString(types).c_str(), defExt))
	{
		saveSettings();
		SettingsManager::exportDcTheme(file);
		restoreSettings();
	}
	return 0;
}

LRESULT PropPageTextStyles::onTabListChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	COLORREF color = colors[ctrlTabList.GetCurSel()].value;
	setForeColor(ctrlTabExample, color);
	return 0;
}

LRESULT PropPageTextStyles::onSetDefaultColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = ctrlTabList.GetCurSel();
	colors[index].value = SettingsManager::getDefault((SettingsManager::IntSetting) colors[index].setting);
	setForeColor(ctrlTabExample, colors[index].value);
	return 0;
}

LRESULT PropPageTextStyles::onClientSelectTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CColorDialog d(colors[ctrlTabList.GetCurSel()].value, CC_FULLOPEN, *this);
	if (d.DoModal() == IDOK)
	{
		colors[ctrlTabList.GetCurSel()].value = d.GetColor();
		switch (ctrlTabList.GetCurSel())
		{
			case 0:
				bg = d.GetColor();
				break;
		}
		setForeColor(ctrlTabExample, d.GetColor());
		refreshPreview();
	}
	return 0;
}

LRESULT PropPageTextStyles::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	HWND hWnd = reinterpret_cast<HWND>(lParam);
	if (hWnd == ctrlTabExample.m_hWnd)
	{
		::SetBkMode((HDC)wParam, TRANSPARENT);
		HANDLE h = GetProp(hWnd, _T("fillcolor"));
		if (h) return reinterpret_cast<LRESULT>(h);
		return TRUE;
	}
	return FALSE;
}

void PropPageTextStyles::getThemeList()
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
		
		sort(themes.begin(), themes.end(),
			[](const ThemeInfo& l, const ThemeInfo& r) { return stricmp(l.name, r.name) < 0; });
		themes.insert(themes.begin(), { TSTRING(THEME_DEFAULT_NAME), Util::emptyStringT });

		int index = 0;
		for (const ThemeInfo& theme : themes)
			ctrlTheme.InsertString(index++, theme.name.c_str());
		
		if (index <= 1)
			ctrlTheme.EnableWindow(FALSE);
		ctrlTheme.SetCurSel(0);
		defaultThemeSelected = true;
	}
}

void PropPageTextStyles::setForeColor(CEdit& cs, COLORREF cr)
{
	HBRUSH newBrush = CreateSolidBrush(cr);
	SetProp(cs.m_hWnd, _T("fillcolor"), newBrush);
	if (hBrush) DeleteObject(hBrush);
	hBrush = newBrush;
	cs.Invalidate();
	cs.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASENOW | RDW_UPDATENOW | RDW_FRAME);
}
