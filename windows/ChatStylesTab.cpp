#include "stdafx.h"
#include "ChatStylesTab.h"
#include "Colors.h"
#include "Fonts.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "wtl_flylinkdc.h"

extern SettingsManager *g_settings;

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 2, DialogLayout::SIDE_TOP,    U_PX(-1) };
static const DialogLayout::Align align2 = { 2, DialogLayout::SIDE_BOTTOM, U_PX(-1) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_FONT,     FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_FONT,             0,              UNSPEC, UNSPEC                                        },
	{ IDC_CHOOSE_FONT,      FLAG_TRANSLATE, UNSPEC, UNSPEC, 0, nullptr, nullptr, &align1, &align2 },
	{ IDC_SET_DEFAULT_FONT, FLAG_TRANSLATE, UNSPEC, UNSPEC, 0, nullptr, nullptr, &align1, &align2 },
	{ IDC_CAPTION_STYLES,   FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_CAPTION_PREVIEW,  FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_TEXT_COLOR,       FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_BACK_COLOR,       FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_TEXT_STYLE,       FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_SET_DEFAULT,      FLAG_TRANSLATE, UNSPEC, UNSPEC                                        },
	{ IDC_BOLD_MSG_AUTHORS, FLAG_TRANSLATE, AUTO,   UNSPEC                                        }
};

void ChatStylesTab::TextStyleSettings::init(
    ChatStylesTab *parent, ResourceManager::Strings name,
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

void ChatStylesTab::TextStyleSettings::loadSettings()
{
	dwEffects = 0;
	crBackColor = g_settings->get(bgSetting);
	crTextColor = g_settings->get(fgSetting);
	if (g_settings->get(boldSetting)) dwEffects |= CFE_BOLD;
	if (g_settings->get(italicSetting)) dwEffects |= CFE_ITALIC;
}

void ChatStylesTab::TextStyleSettings::loadDefaultSettings()
{
	dwEffects = 0;
	crBackColor = g_settings->getDefault(bgSetting);
	crTextColor = g_settings->getDefault(fgSetting);
	if (g_settings->getDefault(boldSetting)) dwEffects |= CFE_BOLD;
	if (g_settings->getDefault(italicSetting)) dwEffects |= CFE_ITALIC;
}

void ChatStylesTab::TextStyleSettings::loadSettings(const SettingsStore& ss)
{
	dwEffects = 0;
	int val;
	if (ss.getIntValue(bgSetting, val)) crBackColor = val;
	if (ss.getIntValue(fgSetting, val)) crTextColor = val;
	if (ss.getIntValue(boldSetting, val) && val) dwEffects |= CFE_BOLD;
	if (ss.getIntValue(italicSetting, val) && val) dwEffects |= CFE_ITALIC;
}

void ChatStylesTab::TextStyleSettings::saveSettings() const
{
	g_settings->set(bgSetting, (int) crBackColor);
	g_settings->set(fgSetting, (int) crTextColor);
	BOOL bold = (dwEffects & CFE_BOLD) == CFE_BOLD;
	g_settings->set(boldSetting, bold);
	BOOL italic = (dwEffects & CFE_ITALIC) == CFE_ITALIC;
	g_settings->set(italicSetting, italic);
}

void ChatStylesTab::TextStyleSettings::saveSettings(SettingsStore& ss) const
{
	ss.setIntValue(bgSetting, crBackColor);
	ss.setIntValue(fgSetting, crTextColor);
	BOOL bold = (dwEffects & CFE_BOLD) == CFE_BOLD;
	ss.setIntValue(boldSetting, bold);
	BOOL italic = (dwEffects & CFE_ITALIC) == CFE_ITALIC;
	ss.setIntValue(italicSetting, italic);
}

bool ChatStylesTab::TextStyleSettings::editBackgroundColor()
{
	CColorDialog d(crBackColor, CC_FULLOPEN, *parent);
	if (d.DoModal() == IDOK && crBackColor != d.GetColor())
	{
		crBackColor = d.GetColor();
		return true;
	}
	return false;
}

bool ChatStylesTab::TextStyleSettings::editForegroundColor()
{
	CColorDialog d(crTextColor, CC_FULLOPEN, *parent);
	if (d.DoModal() == IDOK && crTextColor != d.GetColor())
	{
		crTextColor = d.GetColor();
		return true;
	}
	return false;
}

bool ChatStylesTab::TextStyleSettings::editTextStyle()
{
	FontStyleDlg dlg;
	dlg.bold = (dwEffects & CFE_BOLD) != 0;
	dlg.italic = (dwEffects & CFE_ITALIC) != 0;
	if (dlg.DoModal(parent->m_hWnd) != IDOK) return false;
	if (dlg.bold)
		dwEffects |= CFE_BOLD;
	else
		dwEffects &= ~CFE_BOLD;
	if (dlg.italic)
		dwEffects |= CFE_ITALIC;
	else
		dwEffects &= ~CFE_ITALIC;
	return true;
}

ChatStylesTab::ChatStylesTab()
{
	callback = nullptr;
	hBrush = nullptr;
	hMainFont = nullptr;
	bg = 0;

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

	textStyles[TS_BANNED].init(this, ResourceManager::FAV_ENEMY_USER,
	    SettingsManager::TEXT_ENEMY_BACK_COLOR, SettingsManager::TEXT_ENEMY_FORE_COLOR,
	    SettingsManager::TEXT_ENEMY_BOLD, SettingsManager::TEXT_ENEMY_ITALIC);

	textStyles[TS_OP].init(this, ResourceManager::OPERATOR,
	    SettingsManager::TEXT_OP_BACK_COLOR, SettingsManager::TEXT_OP_FORE_COLOR,
	    SettingsManager::TEXT_OP_BOLD, SettingsManager::TEXT_OP_ITALIC);

	textStyles[TS_OTHER].init(this, ResourceManager::OTHER_USER,
	    SettingsManager::TEXT_NORMAL_BACK_COLOR, SettingsManager::TEXT_NORMAL_FORE_COLOR,
	    SettingsManager::TEXT_NORMAL_BOLD, SettingsManager::TEXT_NORMAL_ITALIC);

	preview.disableChatCache();
}

void ChatStylesTab::updateFont()
{
	for (int i = 0; i < TS_LAST; i++)
	{
		_tcscpy(textStyles[i].szFaceName, mainFont.lfFaceName);
		textStyles[i].bCharSet = mainFont.lfCharSet;
		textStyles[i].yHeight = mainFont.lfHeight;
		if (mainFont.lfWeight == FW_BOLD)
			textStyles[i].dwEffects |= CFE_BOLD;
		if (mainFont.lfItalic)
			textStyles[i].dwEffects |= CFE_ITALIC;
	}
}

void ChatStylesTab::cleanup()
{
	if (hMainFont)
	{
		DeleteObject(hMainFont);
		hMainFont = nullptr;
	}
	if (hBrush)
	{
		DeleteObject(hBrush);
		hBrush = nullptr;
	}
}

LRESULT ChatStylesTab::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	lsbList.Attach(GetDlgItem(IDC_TEXT_STYLES));
	lsbList.ResetContent();

	for (int i = 0; i < TS_LAST; i++)
		lsbList.AddString(CTSTRING_I(textStyles[i].name));
	lsbList.SetCurSel(0);

	preview.Attach(GetDlgItem(IDC_PREVIEW));
	ctrlBoldMsgAuthors.Attach(GetDlgItem(IDC_BOLD_MSG_AUTHORS));
	updateTheme();

	return TRUE;
}

LRESULT ChatStylesTab::onSetFont(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	LOGFONT font = mainFont;
	CFontDialog d(&font, CF_SCREENFONTS | CF_FORCEFONTEXIST, nullptr, m_hWnd);
	if (d.DoModal() != IDOK) return 0;
	mainFont = font;
	applyFont();
	updateFont();
	refreshPreview();
	return 0;
}

LRESULT ChatStylesTab::onSetDefaultFont(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	Fonts::decodeFont(Text::toT(g_settings->getDefault(SettingsManager::TEXT_FONT)), mainFont);
	applyFont();
	updateFont();
	refreshPreview();
	return 0;
}

LRESULT ChatStylesTab::onSetBackgroundColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	if (textStyles[index].editBackgroundColor() && callback)
		callback->settingChanged(textStyles[index].bgSetting);
	refreshPreview();
	return TRUE;
}

LRESULT ChatStylesTab::onSetTextColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	if (textStyles[index].editForegroundColor() && callback)
		callback->settingChanged(textStyles[index].fgSetting);
	refreshPreview();
	return TRUE;
}

LRESULT ChatStylesTab::onSetStyle(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	auto savedEffects = textStyles[index].dwEffects;
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

	if (callback)
	{
		if ((savedEffects ^ textStyles[index].dwEffects) & CFE_BOLD)
			callback->settingChanged(textStyles[index].boldSetting);
		if ((savedEffects ^ textStyles[index].dwEffects) & CFE_ITALIC)
			callback->settingChanged(textStyles[index].italicSetting);
	}

	refreshPreview();
	return TRUE;
}

LRESULT ChatStylesTab::onSetDefault(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int index = lsbList.GetCurSel();
	COLORREF crBackColor = textStyles[index].crBackColor;
	COLORREF crTextColor = textStyles[index].crTextColor;
	auto savedEffects = textStyles[index].dwEffects;
	textStyles[index].loadDefaultSettings();
	if (callback)
	{
		if (crBackColor != textStyles[index].crBackColor)
			callback->settingChanged(textStyles[index].bgSetting);
		if (crTextColor != textStyles[index].crTextColor)
			callback->settingChanged(textStyles[index].fgSetting);
		if ((savedEffects ^ textStyles[index].dwEffects) & CFE_BOLD)
			callback->settingChanged(textStyles[index].boldSetting);
		if ((savedEffects ^ textStyles[index].dwEffects) & CFE_ITALIC)
			callback->settingChanged(textStyles[index].italicSetting);
	}

	refreshPreview();
	return TRUE;
}

void ChatStylesTab::refreshPreview()
{
	HFONT newFont = CreateFontIndirect(&mainFont);
	preview.SetFont(newFont);
	if (hMainFont) DeleteObject(hMainFont);
	hMainFont = newFont;

	preview.SetBackgroundColor(bg);
	preview.SetWindowText(_T(""));
	static const tstring timestamp = _T("12:34 ");

	{
		CLockRedraw<false> lockRedraw(preview);
		LONG startPos, endPos;
		for (int i = 0; i < TS_LAST; i++)
		{
			if (i == TS_TIMESTAMP) continue;
			preview.insertAndFormat(timestamp, textStyles[TS_TIMESTAMP], startPos, endPos);
			if (i == TS_URL)
			{
				CHARFORMAT2 cf = textStyles[TS_URL];
				cf.dwMask |= CFM_UNDERLINE;
				cf.dwEffects |= CFE_UNDERLINE;
				preview.insertAndFormat(TSTRING(SAMPLE_URL) + _T("\r\n"), cf, startPos, endPos);
			}
			else if (i >= TS_FAVORITE)
			{
				CHARFORMAT2 cf = textStyles[i];
				if (boldMsgAuthor)
				{
					cf.dwMask |= CFM_BOLD;
					cf.dwEffects |= CFE_BOLD;
				}
				preview.insertAndFormat(_T("<"), textStyles[TS_GENERAL], startPos, endPos);
				preview.insertAndFormat(TSTRING_I(textStyles[i].name), cf, startPos, endPos);
				preview.insertAndFormat(_T("> "), textStyles[TS_GENERAL], startPos, endPos);
				preview.insertAndFormat(TSTRING(SAMPLE_MESSAGE) + _T("\r\n"), textStyles[TS_GENERAL], startPos, endPos);
			}
			else
				preview.insertAndFormat(TSTRING_I(textStyles[i].name) + _T("\r\n"), textStyles[i], startPos, endPos);
		}
	}
	preview.InvalidateRect(nullptr);
}

LRESULT ChatStylesTab::onBoldMsgAuthor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	boldMsgAuthor = ctrlBoldMsgAuthors.GetCheck() == BST_CHECKED;
	refreshPreview();
	if (callback) callback->settingChanged(SettingsManager::BOLD_MSG_AUTHOR);
	return 0;
}

void ChatStylesTab::loadSettings()
{
	currentFont = Text::toT(SETTING(TEXT_FONT));
	memset(&mainFont, 0, sizeof(mainFont));
	Fonts::decodeFont(currentFont, mainFont);

	for (int i = 0; i < TS_LAST; i++)
	{
		textStyles[i].loadSettings();
		_tcscpy(textStyles[i].szFaceName, mainFont.lfFaceName);
		textStyles[i].bCharSet = mainFont.lfCharSet;
		textStyles[i].yHeight = mainFont.lfHeight;
	}

	boldMsgAuthor = g_settings->getBool(SettingsManager::BOLD_MSG_AUTHOR);
}

void ChatStylesTab::saveSettings() const
{
	for (int i = 0; i < TS_LAST; i++)
		textStyles[i].saveSettings();
	g_settings->set(SettingsManager::TEXT_FONT, Text::fromT(Fonts::encodeFont(mainFont)));
	g_settings->set(SettingsManager::BOLD_MSG_AUTHOR, boldMsgAuthor);
}

void ChatStylesTab::getValues(SettingsStore& ss) const
{
	for (int i = 0; i < TS_LAST; i++)
		textStyles[i].saveSettings(ss);
	ss.setStrValue(SettingsManager::TEXT_FONT, Text::fromT(Fonts::encodeFont(mainFont)));
	ss.setIntValue(SettingsManager::BOLD_MSG_AUTHOR, boldMsgAuthor);
}

void ChatStylesTab::setValues(const SettingsStore& ss)
{
	for (int i = 0; i < TS_LAST; i++)
		textStyles[i].loadSettings(ss);
	string sval;
	if (ss.getStrValue(SettingsManager::TEXT_FONT, sval))
		Fonts::decodeFont(Text::toT(sval), mainFont);
	ss.getBoolValue(SettingsManager::BOLD_MSG_AUTHOR, boldMsgAuthor);
	int val;
	if (ss.getIntValue(SettingsManager::BACKGROUND_COLOR, val)) bg = val;
}

void ChatStylesTab::updateTheme()
{
	ctrlBoldMsgAuthors.SetCheck(boldMsgAuthor ? BST_CHECKED : BST_UNCHECKED);
	currentFont = Fonts::encodeFont(mainFont);
	showFont();
	refreshPreview();
}

void ChatStylesTab::setBackgroundColor(COLORREF clr, bool updateStyles)
{
	if (bg != clr)
	{
		if (updateStyles)
			for (int i = 0; i < TS_LAST; i++)
				if (textStyles[i].crBackColor == bg)
					textStyles[i].crBackColor = clr;
		bg = clr;
		if (preview.m_hWnd)
			refreshPreview();
	}
}

void ChatStylesTab::applyFont()
{
	tstring newFont = Fonts::encodeFont(mainFont);
	if (newFont != currentFont)
	{
		currentFont = std::move(newFont);
		if (callback) callback->settingChanged(SettingsManager::TEXT_FONT);
	}
	showFont();
}

void ChatStylesTab::showFont()
{
	tstring s = mainFont.lfFaceName;
	s += _T(", ");
	int size = mainFont.lfHeight;
	if (size < 0)
	{
		HDC hDC = GetDC();
		size = -MulDiv(size, 72, GetDeviceCaps(hDC, LOGPIXELSY));
		ReleaseDC(hDC);
	}
	s += Util::toStringT(size);
	if (mainFont.lfWeight == FW_BOLD)
	{
		s += _T(", ");
		s += TSTRING(STYLE_BOLD);
	}
	if (mainFont.lfItalic)
	{
		s += _T(", ");
		s += TSTRING(STYLE_ITALIC);
	}
	SetDlgItemText(IDC_FONT, s.c_str());
}

static const DialogLayout::Item fontStyleLayoutItems[] =
{
	{ IDC_CHECK_BOLD,   FLAG_TRANSLATE, AUTO,   UNSPEC },
	{ IDC_CHECK_ITALIC, FLAG_TRANSLATE, AUTO,   UNSPEC },
	{ IDOK,             FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDCANCEL,         FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

LRESULT FontStyleDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, fontStyleLayoutItems, _countof(fontStyleLayoutItems));
	SetWindowText(CTSTRING(TEXT_STYLE));
	CButton(GetDlgItem(IDC_CHECK_BOLD)).SetCheck(bold ? BST_CHECKED : BST_UNCHECKED);
	CButton(GetDlgItem(IDC_CHECK_ITALIC)).SetCheck(italic ? BST_CHECKED : BST_UNCHECKED);
	CenterWindow(GetParent());
	return 0;
}

LRESULT FontStyleDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		bold = IsDlgButtonChecked(IDC_CHECK_BOLD);
		italic = IsDlgButtonChecked(IDC_CHECK_ITALIC);
	}
	EndDialog(wID);
	return 0;
}
