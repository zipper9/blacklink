#include "stdafx.h"

#include "Resource.h"
#include "../client/SimpleXML.h"

#include "WinUtil.h"
#include "OperaColorsPage.h"
#include "PropertiesDlg.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_AVAILABLE_STYLES, ResourceManager::SETCZDC_STYLES },
	{ IDC_BACK_COLOR, ResourceManager::SETCZDC_BACK_COLOR },
	{ IDC_TEXT_COLOR, ResourceManager::SETCZDC_TEXT_COLOR },
	{ IDC_TEXT_STYLE, ResourceManager::SETCZDC_TEXT_STYLE },
	{ IDC_DEFAULT_STYLES, ResourceManager::SETCZDC_DEFAULT_STYLE },
	{ IDC_BLACK_AND_WHITE, ResourceManager::SETCZDC_BLACK_WHITE },
	{ IDC_BOLD_MSG_AUTHORS, ResourceManager::SETTINGS_BOLD_MSG_AUTHORS },
	{ IDC_CZDC_PREVIEW, ResourceManager::PREVIEW_MENU },
	{ IDC_SELTEXT, ResourceManager::SETTINGS_SELECT_TEXT_FACE },
	{ IDC_RESET_TAB_COLOR, ResourceManager::SETTINGS_RESET },
	{ IDC_SELECT_TAB_COLOR, ResourceManager::SETTINGS_SELECT_COLOR },
	{ IDC_STYLES, ResourceManager::SETTINGS_TEXT_STYLES },
	{ IDC_IMPORT, ResourceManager::SETTINGS_THEME_IMPORT },
	{ IDC_EXPORT, ResourceManager::SETTINGS_THEME_EXPORT },
	{ IDC_CHATCOLORS, ResourceManager::SETTINGS_COLORS },
	{ IDC_BAN_COLOR, ResourceManager::BAN_COLOR_DLG },
	{ IDC_DUPE_COLOR, ResourceManager::DUPE_COLOR_DLG },	
	{ IDC_MODCOLORS, ResourceManager::MOD_COLOR_DLG },	
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_BOLD_MSG_AUTHORS, SettingsManager::BOLD_MSG_AUTHOR, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

PropPageTextStyles::ColorSettings PropPageTextStyles::colors[] =
{
	{ResourceManager::SETTINGS_SELECT_WINDOW_COLOR, SettingsManager::BACKGROUND_COLOR, 0},
	{ResourceManager::SETCZDC_ERROR_COLOR,  SettingsManager::ERROR_COLOR, 0},
	{ResourceManager::PROGRESS_BACK,    SettingsManager::PROGRESS_BACK_COLOR, 0},
	{ResourceManager::PROGRESS_COMPRESS,    SettingsManager::PROGRESS_COMPRESS_COLOR, 0},
	{ResourceManager::PROGRESS_SEGMENT, SettingsManager::PROGRESS_SEGMENT_COLOR, 0},
	{ResourceManager::PROGRESS_DOWNLOADED,  SettingsManager::COLOR_DOWNLOADED, 0},
	{ResourceManager::PROGRESS_RUNNING, SettingsManager::COLOR_RUNNING, 0},
	{ResourceManager::PROGRESS_RUNNING_COMPLETED, SettingsManager::COLOR_RUNNING_COMPLETED, 0},
	{ResourceManager::SETTINGS_DUPE_COLOR,    SettingsManager::DUPE_COLOR, 0},
	{ResourceManager::BAN_COLOR_DLG,    SettingsManager::BAN_COLOR, 0},	
#ifdef SCALOLAZ_USE_COLOR_HUB_IN_FAV
	{ResourceManager::HUB_IN_FAV_BK_COLOR,   SettingsManager::HUB_IN_FAV_BK_COLOR, 0},
	{ResourceManager::HUB_IN_FAV_CONNECT_BK_COLOR,   SettingsManager::HUB_IN_FAV_CONNECT_BK_COLOR, 0},
#endif
};

LRESULT PropPageTextStyles::onSelectColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int color;
	SettingsManager::IntSetting key;
	switch (wID)
	{
		case IDC_DUPE_COLOR:
			color = SETTING(DUPE_COLOR);
			key = SettingsManager::DUPE_COLOR;
			break;
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
	memset(&font, 0, sizeof(font));
	preview.disable_chat_cache();
	
	PropPage::translate(*this, texts);
	PropPage::read(*this, items);
	
	mainColorChanged = false;
	tempfile = Text::toT(Util::getThemesPath() + "temp.dctmp");
	
	lsbList.Attach(GetDlgItem(IDC_TEXT_STYLES));
	lsbList.ResetContent();
	preview.Attach(GetDlgItem(IDC_PREVIEW));
	
	Fonts::decodeFont(Text::toT(SETTING(TEXT_FONT)), font);
	
	fg = SETTING(TEXT_COLOR);
	bg = SETTING(BACKGROUND_COLOR);
	
	textStyles[TS_GENERAL].Init(
	    this, CSTRING(GENERAL_TEXT), CSTRING(GENERAL_TEXT),
	    SettingsManager::TEXT_GENERAL_BACK_COLOR, SettingsManager::TEXT_GENERAL_FORE_COLOR,
	    SettingsManager::TEXT_GENERAL_BOLD, SettingsManager::TEXT_GENERAL_ITALIC);
	    
	textStyles[TS_MYNICK].Init(
	    this, CSTRING(MY_NICK), CSTRING(MY_NICK),
	    SettingsManager::TEXT_MYNICK_BACK_COLOR, SettingsManager::TEXT_MYNICK_FORE_COLOR,
	    SettingsManager::TEXT_MYNICK_BOLD, SettingsManager::TEXT_MYNICK_ITALIC);
	    
	textStyles[TS_MYMSG].Init(
	    this, CSTRING(MY_MESSAGE), CSTRING(MY_MESSAGE),
	    SettingsManager::TEXT_MYOWN_BACK_COLOR, SettingsManager::TEXT_MYOWN_FORE_COLOR,
	    SettingsManager::TEXT_MYOWN_BOLD, SettingsManager::TEXT_MYOWN_ITALIC);
	    
	textStyles[TS_PRIVATE].Init(
	    this, CSTRING(PRIVATE_MESSAGE), CSTRING(PRIVATE_MESSAGE),
	    SettingsManager::TEXT_PRIVATE_BACK_COLOR, SettingsManager::TEXT_PRIVATE_FORE_COLOR,
	    SettingsManager::TEXT_PRIVATE_BOLD, SettingsManager::TEXT_PRIVATE_ITALIC);
	    
	textStyles[TS_SYSTEM].Init(
	    this, CSTRING(SYSTEM_MESSAGE), CSTRING(SYSTEM_MESSAGE),
	    SettingsManager::TEXT_SYSTEM_BACK_COLOR, SettingsManager::TEXT_SYSTEM_FORE_COLOR,
	    SettingsManager::TEXT_SYSTEM_BOLD, SettingsManager::TEXT_SYSTEM_ITALIC);
	    
	textStyles[TS_SERVER].Init(
	    this, CSTRING(SERVER_MESSAGE), CSTRING(SERVER_MESSAGE),
	    SettingsManager::TEXT_SERVER_BACK_COLOR, SettingsManager::TEXT_SERVER_FORE_COLOR,
	    SettingsManager::TEXT_SERVER_BOLD, SettingsManager::TEXT_SERVER_ITALIC);
	    
	textStyles[TS_TIMESTAMP].Init(
	    this, CSTRING(TIMESTAMP), CSTRING(TEXT_STYLE_TIME_SAMPLE),
	    SettingsManager::TEXT_TIMESTAMP_BACK_COLOR, SettingsManager::TEXT_TIMESTAMP_FORE_COLOR,
	    SettingsManager::TEXT_TIMESTAMP_BOLD, SettingsManager::TEXT_TIMESTAMP_ITALIC);
	    
	textStyles[TS_URL].Init(
	    this, CSTRING(TEXT_STYLE_URL), CSTRING(TEXT_STYLE_URL_SAMPLE),
	    SettingsManager::TEXT_URL_BACK_COLOR, SettingsManager::TEXT_URL_FORE_COLOR,
	    SettingsManager::TEXT_URL_BOLD, SettingsManager::TEXT_URL_ITALIC);
	    
	textStyles[TS_FAVORITE].Init(
	    this, CSTRING(FAV_USER), CSTRING(FAV_USER),
	    SettingsManager::TEXT_FAV_BACK_COLOR, SettingsManager::TEXT_FAV_FORE_COLOR,
	    SettingsManager::TEXT_FAV_BOLD, SettingsManager::TEXT_FAV_ITALIC);
	    
	textStyles[TS_FAV_ENEMY].Init(
	    this, CSTRING(FAV_ENEMY_USER), CSTRING(FAV_ENEMY_USER),
	    SettingsManager::TEXT_ENEMY_BACK_COLOR, SettingsManager::TEXT_ENEMY_FORE_COLOR,
	    SettingsManager::TEXT_ENEMY_BOLD, SettingsManager::TEXT_ENEMY_ITALIC);
	    
	textStyles[TS_OP].Init(
	    this, CSTRING(OPERATOR), CSTRING(OPERATOR),
	    SettingsManager::TEXT_OP_BACK_COLOR, SettingsManager::TEXT_OP_FORE_COLOR,
	    SettingsManager::TEXT_OP_BOLD, SettingsManager::TEXT_OP_ITALIC);
	    
	for (int i = 0; i < TS_LAST; i++)
	{
		textStyles[i].LoadSettings();
		_tcscpy(textStyles[i].szFaceName, font.lfFaceName);
		textStyles[i].bCharSet = font.lfCharSet;
		textStyles[i].yHeight = font.lfHeight;
		lsbList.AddString(Text::toT(textStyles[i].text).c_str());
	}
	lsbList.SetCurSel(0);
	
	ctrlTabList.Attach(GetDlgItem(IDC_TABCOLOR_LIST));
	cmdResetTab.Attach(GetDlgItem(IDC_RESET_TAB_COLOR));
	cmdSetTabColor.Attach(GetDlgItem(IDC_SELECT_TAB_COLOR));
	ctrlTabExample.Attach(GetDlgItem(IDC_SAMPLE_TAB_COLOR));
	
	ctrlTabList.ResetContent();
	for (int i = 0; i < _countof(colors); i++)
	{
		ctrlTabList.AddString(Text::toT(ResourceManager::getString(colors[i].name)).c_str());
		onResetColor(i);
	}
	
	setForeColor(ctrlTabExample, GetSysColor(COLOR_3DFACE));
	
	ctrlTabList.SetCurSel(0);
	BOOL bTmp;
	onTabListChange(0, 0, 0, bTmp);
	
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO2));
	getThemeList();
	ctrlTheme.Detach();
	
	RefreshPreview();
	return TRUE;
}

void PropPageTextStyles::write()
{
	PropPage::write(*this, items);
	
	tstring f = WinUtil::encodeFont(font);
	g_settings->set(SettingsManager::TEXT_FONT, Text::fromT(f));
	
	g_settings->set(SettingsManager::TEXT_COLOR, (int)fg);
	g_settings->set(SettingsManager::BACKGROUND_COLOR, (int)bg);
	
	for (int i = 1; i < _countof(colors); i++)
	{
		g_settings->set(SettingsManager::IntSetting(colors[i].setting), (int)colors[i].value);
	}
	
	for (int i = 0; i < TS_LAST; i++)
	{
		textStyles[i].SaveSettings();
	}
	File::deleteFileT(tempfile);
	Colors::init();
}

void PropPageTextStyles::cancel()
{
	if (mainColorChanged)
	{
		SettingsManager::importDcTheme(tempfile);
		SendMessage(WM_DESTROY, 0, 0);
		PropertiesDlg::g_needUpdate = true;
		SendMessage(WM_INITDIALOG, 0, 0);
		mainColorChanged = false;
	}
	write();
}

LRESULT PropPageTextStyles::onEditBackColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	textStyles[index].EditBackColor();
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onEditForeColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	textStyles[index].EditForeColor();
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onEditTextStyle(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	textStyles[index].EditTextStyle();
	
	_tcscpy(font.lfFaceName, textStyles[index].szFaceName);
	font.lfCharSet = textStyles[index].bCharSet;
	font.lfHeight = textStyles[index].yHeight;
	
	if (index == TS_GENERAL)
	{
		if (textStyles[index].dwEffects & CFE_ITALIC)
			font.lfItalic = TRUE;
		if (textStyles[index].dwEffects & CFE_BOLD)
			font.lfWeight = FW_BOLD;
	}
	{
		CLockRedraw<true> lockRedraw(preview);
		for (int i = 0; i < TS_LAST; i++)
		{
			_tcscpy(textStyles[index].szFaceName, font.lfFaceName);
			textStyles[i].bCharSet = font.lfCharSet;
			textStyles[i].yHeight = font.lfHeight;
			const ChatCtrl::CFlyChatCache message(ClientManager::getFlylinkDCIdentity(), false, true, _T("12:34 "), Text::toT(textStyles[i].preview), textStyles[i], true);
			preview.AppendText(message, 0, false);
		}
	}
	
	RefreshPreview();
	return TRUE;
}

void PropPageTextStyles::RefreshPreview()
{
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
			const ChatCtrl::CFlyChatCache message(ClientManager::getFlylinkDCIdentity(), false, true, _T("12:34 "), Text::toT(textStyles[i].preview).c_str(), textStyles[i], false);
			preview.AppendText(message, 0, false);
		}
	}
	preview.InvalidateRect(NULL);
	preview.SetTextStyleMyNick(old);
	Colors::g_TextStyleTimestamp = old2;
}

LRESULT PropPageTextStyles::onDefaultStyles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bg = RGB(255, 255, 255);
	fg = RGB(67, 98, 154);
	::GetObject((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(font), &font);
	textStyles[TS_GENERAL].crBackColor = RGB(255, 255, 255);
	textStyles[TS_GENERAL].crTextColor = RGB(67, 98, 154);
	textStyles[TS_GENERAL].dwEffects = 0;
	
	textStyles[TS_MYNICK].crBackColor = RGB(255, 255, 255);
	textStyles[TS_MYNICK].crTextColor = RGB(67, 98, 154);
	textStyles[TS_MYNICK].dwEffects = CFE_BOLD;
	
	textStyles[TS_MYMSG].crBackColor = RGB(255, 255, 255);
	textStyles[TS_MYMSG].crTextColor = RGB(67, 98, 154);
	textStyles[TS_MYMSG].dwEffects = 0;
	
	textStyles[TS_PRIVATE].crBackColor = RGB(255, 255, 255);
	textStyles[TS_PRIVATE].crTextColor = RGB(67, 98, 154);
	textStyles[TS_PRIVATE].dwEffects = CFE_ITALIC;
	
	textStyles[TS_SYSTEM].crBackColor = RGB(255, 255, 255);
	textStyles[TS_SYSTEM].crTextColor = RGB(0, 128, 64);
	textStyles[TS_SYSTEM].dwEffects = CFE_BOLD;
	
	textStyles[TS_SERVER].crBackColor = RGB(255, 255, 255);
	textStyles[TS_SERVER].crTextColor = RGB(0, 128, 64);
	textStyles[TS_SERVER].dwEffects = CFE_BOLD;
	
	textStyles[TS_TIMESTAMP].crBackColor = RGB(255, 255, 255);
	textStyles[TS_TIMESTAMP].crTextColor = RGB(67, 98, 154);
	textStyles[TS_TIMESTAMP].dwEffects = 0;
	
	textStyles[TS_URL].crBackColor = RGB(255, 255, 255);
	textStyles[TS_URL].crTextColor = RGB(0, 0, 255);
	textStyles[TS_URL].dwEffects = 0;
	
	textStyles[TS_FAVORITE].crBackColor = RGB(255, 255, 255);
	textStyles[TS_FAVORITE].crTextColor = RGB(67, 98, 154);
	textStyles[TS_FAVORITE].dwEffects = CFE_BOLD;
	
	textStyles[TS_FAV_ENEMY].crBackColor = RGB(255, 255, 255);
	textStyles[TS_FAV_ENEMY].crTextColor = RGB(255, 165, 121);
	textStyles[TS_FAV_ENEMY].dwEffects = CFE_BOLD;
	
	textStyles[TS_OP].crBackColor = RGB(255, 255, 255);
	textStyles[TS_OP].crTextColor = RGB(0, 128, 64);
	textStyles[TS_OP].dwEffects = CFE_BOLD;
	
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onBlackAndWhite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bg = RGB(255, 255, 255);
	fg = RGB(0, 0, 0);
	textStyles[TS_GENERAL].crBackColor = RGB(255, 255, 255);
	textStyles[TS_GENERAL].crTextColor = RGB(37, 60, 121);
	textStyles[TS_GENERAL].dwEffects = 0;
	
	textStyles[TS_MYNICK].crBackColor = RGB(255, 255, 255);
	textStyles[TS_MYNICK].crTextColor = RGB(37, 60, 121);
	textStyles[TS_MYNICK].dwEffects = 0;
	
	textStyles[TS_MYMSG].crBackColor = RGB(255, 255, 255);
	textStyles[TS_MYMSG].crTextColor = RGB(37, 60, 121);
	textStyles[TS_MYMSG].dwEffects = 0;
	
	textStyles[TS_PRIVATE].crBackColor = RGB(255, 255, 255);
	textStyles[TS_PRIVATE].crTextColor = RGB(37, 60, 121);
	textStyles[TS_PRIVATE].dwEffects = 0;
	
	textStyles[TS_SYSTEM].crBackColor = RGB(255, 255, 255);
	textStyles[TS_SYSTEM].crTextColor = RGB(37, 60, 121);
	textStyles[TS_SYSTEM].dwEffects = 0;
	
	textStyles[TS_SERVER].crBackColor = RGB(255, 255, 255);
	textStyles[TS_SERVER].crTextColor = RGB(37, 60, 121);
	textStyles[TS_SERVER].dwEffects = 0;
	
	textStyles[TS_TIMESTAMP].crBackColor = RGB(255, 255, 255);
	textStyles[TS_TIMESTAMP].crTextColor = RGB(37, 60, 121);
	textStyles[TS_TIMESTAMP].dwEffects = 0;
	
	textStyles[TS_URL].crBackColor = RGB(255, 255, 255);
	textStyles[TS_URL].crTextColor = RGB(37, 60, 121);
	textStyles[TS_URL].dwEffects = 0;
	
	textStyles[TS_FAVORITE].crBackColor = RGB(255, 255, 255);
	textStyles[TS_FAVORITE].crTextColor = RGB(37, 60, 121);
	textStyles[TS_FAVORITE].dwEffects = 0;
	
	textStyles[TS_FAV_ENEMY].crBackColor = RGB(255, 255, 255);
	textStyles[TS_FAV_ENEMY].crTextColor = RGB(37, 60, 121);
	textStyles[TS_FAV_ENEMY].dwEffects = 0;
	
	textStyles[TS_OP].crBackColor = RGB(255, 255, 255);
	textStyles[TS_OP].crTextColor = RGB(37, 60, 121);
	textStyles[TS_OP].dwEffects = 0;
	
	RefreshPreview();
	return TRUE;
}

void PropPageTextStyles::TextStyleSettings::Init(
    PropPageTextStyles *parent,
    const char *text, const char *preview,
    SettingsManager::IntSetting bgSetting, SettingsManager::IntSetting fgSetting,
    SettingsManager::IntSetting boldSetting, SettingsManager::IntSetting italicSetting)
{
	cbSize = sizeof(CHARFORMAT2);
	dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_BACKCOLOR;
	dwReserved = 0;
	
	this->parent = parent;
	this->text = text;
	this->preview = preview;
	this->bgSetting = bgSetting;
	this->fgSetting = fgSetting;
	this->boldSetting = boldSetting;
	this->italicSetting = italicSetting;
}

void PropPageTextStyles::TextStyleSettings::LoadSettings()
{
	dwEffects = 0;
	crBackColor = g_settings->get(bgSetting);
	crTextColor = g_settings->get(fgSetting);
	if (g_settings->get(boldSetting)) dwEffects |= CFE_BOLD;
	if (g_settings->get(italicSetting)) dwEffects |= CFE_ITALIC;
}

void PropPageTextStyles::TextStyleSettings::SaveSettings()
{
	g_settings->set(bgSetting, (int) crBackColor);
	g_settings->set(fgSetting, (int) crTextColor);
	BOOL bold = (dwEffects & CFE_BOLD) == CFE_BOLD;
	g_settings->set(boldSetting, (int) bold);
	BOOL italic = (dwEffects & CFE_ITALIC) == CFE_ITALIC;
	g_settings->set(italicSetting, (int) italic);
}

void PropPageTextStyles::TextStyleSettings::EditBackColor()
{
	CColorDialog d(crBackColor, 0, *parent);
	if (d.DoModal() == IDOK)
		crBackColor = d.GetColor();
}

void PropPageTextStyles::TextStyleSettings::EditForeColor()
{
	CColorDialog d(crTextColor, 0, *parent);
	if (d.DoModal() == IDOK)
		crTextColor = d.GetColor();
}

void PropPageTextStyles::TextStyleSettings::EditTextStyle()
{
	LOGFONT font = {0};
	Fonts::decodeFont(Text::toT(SETTING(TEXT_FONT)), font);
	
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
	d.m_cf.rgbColors = crTextColor;
	if (d.DoModal() == IDOK)
	{
		_tcscpy(szFaceName, font.lfFaceName);
		bCharSet = font.lfCharSet;
		yHeight = font.lfHeight;
		
		crTextColor = d.m_cf.rgbColors;
		if (font.lfWeight == FW_BOLD)
			dwEffects |= CFE_BOLD;
		else
			dwEffects &= ~CFE_BOLD;
			
		if (font.lfItalic)
			dwEffects |= CFE_ITALIC;
		else
			dwEffects &= ~CFE_ITALIC;
	}
}

LRESULT PropPageTextStyles::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	lsbList.Detach();
	preview.Detach();
	ctrlTabList.Detach();
	cmdResetTab.Detach();
	cmdSetTabColor.Detach();
	ctrlTabExample.Detach();
	
	return 1;
}

LRESULT PropPageTextStyles::onImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO2));
	const tstring file = Text::toT(WinUtil::getDataFromMap(ctrlTheme.GetCurSel(), themeList));
	ctrlTheme.Detach();
	if (!mainColorChanged)
		SettingsManager::exportDcTheme(tempfile);
	SettingsManager::importDcTheme(file);
	SendMessage(WM_DESTROY, 0, 0);
	PropertiesDlg::g_needUpdate = true;
	SendMessage(WM_INITDIALOG, 0, 0);
	mainColorChanged = true;
	//  RefreshPreview();
	return 0;
}

static const TCHAR types[] = _T("DC++ Theme Files\0*.dctheme;\0DC++ Settings Files\0*.xml;\0All Files\0*.*\0\0");// TODO translate
static const TCHAR defExt[] = _T(".dctheme");

LRESULT PropPageTextStyles::onExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
//  if (WinUtil::browseFile(file, m_hWnd, true, x, CTSTRING(DC_THEMES_DIALOG_FILE_TYPES_STRING), defExt) == IDOK)
	if (WinUtil::browseFile(file, m_hWnd, true, Text::toT(Util::getThemesPath()), types, defExt) == IDOK)// [!]IRainman  убрать
	{
		SettingsManager::exportDcTheme(file); // [!] IRainman fix.
	}
	return 0;
}

void PropPageTextStyles::onResetColor(int i)
{
	colors[i].value = SettingsManager::get((SettingsManager::IntSetting)colors[i].setting, true);
	setForeColor(ctrlTabExample, colors[i].value);
}

LRESULT PropPageTextStyles::onTabListChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	COLORREF color = colors[ctrlTabList.GetCurSel()].value;
	setForeColor(ctrlTabExample, color);
	RefreshPreview();
	return S_OK;
}

LRESULT PropPageTextStyles::onClickedResetTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	onResetColor(ctrlTabList.GetCurSel());
	return S_OK;
}

LRESULT PropPageTextStyles::onClientSelectTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CColorDialog d(colors[ctrlTabList.GetCurSel()].value, CC_FULLOPEN, *this); //[~] SCALOlaz 0 before CC_FULLOPEN
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
		RefreshPreview();
	}
	return S_OK;
}

LRESULT PropPageTextStyles::onClickedText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LOGFONT tmp = font;
	CFontDialog d(&tmp, CF_EFFECTS | CF_SCREENFONTS | CF_FORCEFONTEXIST, NULL, *this); // !SMT!-F
	d.m_cf.rgbColors = fg;
	if (d.DoModal() == IDOK)
	{
		font = tmp;
		fg = d.GetColor();
	}
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	HWND hWnd = (HWND)lParam;
	if (hWnd == ctrlTabExample.m_hWnd)
	{
		::SetBkMode((HDC)wParam, TRANSPARENT);
		HANDLE h = GetProp(hWnd, _T("fillcolor"));
		if (h != NULL)
		{
			return (LRESULT)h;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void PropPageTextStyles::getThemeList()
{
	if (themeList.empty())
	{
		const string ext = ".dctheme";
		const string fileFindPath = Util::getThemesPath() + "*" + ext;
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			string name = i->getFileName();
			if (name.empty() || i->isDirectory() || i->getSize() == 0)
			{
				continue;
			}
			const wstring wName = Text::toT(name.substr(0, name.length() - ext.length()));
			themeList.insert(ThemePair(wName, Util::getThemesPath() + name));
		}
		
		int index = 0;
		for (auto i = themeList.cbegin(); i != themeList.cend(); ++i)
		{
			ctrlTheme.InsertString(index++, i->first.c_str());
		}
		
		if (index == 0)
		{
			ctrlTheme.InsertString(0, CTSTRING(THEME_DEFAULT_NAME));    //Only Default Theme
			ctrlTheme.EnableWindow(FALSE);
		}
		ctrlTheme.SetCurSel(0);
	}
}
