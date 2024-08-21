#include "stdafx.h"
#include "Colors.h"
#include "BarShader.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/Text.h"

HBRUSH Colors::g_bgBrush = nullptr;
HBRUSH Colors::g_tabBackgroundBrush = nullptr;

COLORREF Colors::g_textColor = 0;
COLORREF Colors::g_bgColor = 0;
COLORREF Colors::g_tabBackground = 0;
COLORREF Colors::g_tabText = 0;

bool Colors::isAppThemed = false;

CHARFORMAT2 Colors::charFormat[Colors::MAX_TEXT_STYLES];

struct NamedColor
{
	const TCHAR* name;
	COLORREF color;
};

static const NamedColor namedColors[] =
{
	{ _T("red"),       RGB(0xff, 0x00, 0x00) },
	{ _T("cyan"),      RGB(0x00, 0xff, 0xff) },
	{ _T("blue"),      RGB(0x00, 0x00, 0xff) },
	{ _T("darkblue"),  RGB(0x00, 0x00, 0xa0) },
	{ _T("lightblue"), RGB(0xad, 0xd8, 0xe6) },
	{ _T("purple"),    RGB(0x80, 0x00, 0x80) },
	{ _T("yellow"),    RGB(0xff, 0xff, 0x00) },
	{ _T("lime"),      RGB(0x00, 0xff, 0x00) },
	{ _T("fuchsia"),   RGB(0xff, 0x00, 0xff) },
	{ _T("white"),     RGB(0xff, 0xff, 0xff) },
	{ _T("silver"),    RGB(0xc0, 0xc0, 0xc0) },
	{ _T("grey"),      RGB(0x80, 0x80, 0x80) },
	{ _T("black"),     RGB(0x00, 0x00, 0x00) },
	{ _T("orange"),    RGB(0xff, 0xa5, 0x00) },
	{ _T("brown"),     RGB(0xa5, 0x2a, 0x2a) },
	{ _T("maroon"),    RGB(0x80, 0x00, 0x00) },
	{ _T("green"),     RGB(0x00, 0x80, 0x00) },
	{ _T("olive"),     RGB(0x80, 0x80, 0x00) }
};

void Colors::init()
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	isAppThemed = IsAppThemed();
	g_textColor = ss->getInt(Conf::TEXT_COLOR);
	g_bgColor = ss->getInt(Conf::BACKGROUND_COLOR);
	g_tabBackground = ss->getInt(Conf::TABS_ACTIVE_BACKGROUND_COLOR);
	g_tabText = ss->getInt(Conf::TABS_ACTIVE_TEXT_COLOR);

	if (g_bgBrush) DeleteObject(g_bgBrush);
	g_bgBrush = CreateSolidBrush(g_bgColor);

	if (g_tabBackgroundBrush) DeleteObject(g_tabBackgroundBrush);
	g_tabBackgroundBrush = CreateSolidBrush(isAppThemed ? g_tabBackground : GetSysColor(COLOR_BTNFACE));

	CHARFORMAT2 cf;
	memset(&cf, 0, sizeof(CHARFORMAT2));
	cf.cbSize = sizeof(cf);
	cf.dwReserved = 0;
	cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD | CFM_ITALIC;
	cf.dwEffects = 0;
	cf.crBackColor = g_bgColor;
	cf.crTextColor = g_textColor;

	charFormat[TEXT_STYLE_TIMESTAMP] = cf;
	charFormat[TEXT_STYLE_TIMESTAMP].crBackColor = ss->getInt(Conf::TEXT_TIMESTAMP_BACK_COLOR);
	charFormat[TEXT_STYLE_TIMESTAMP].crTextColor = ss->getInt(Conf::TEXT_TIMESTAMP_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_TIMESTAMP_BOLD))
		charFormat[TEXT_STYLE_TIMESTAMP].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_TIMESTAMP_ITALIC))
		charFormat[TEXT_STYLE_TIMESTAMP].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_NORMAL] = cf;
	charFormat[TEXT_STYLE_NORMAL].crBackColor = ss->getInt(Conf::TEXT_GENERAL_BACK_COLOR);
	charFormat[TEXT_STYLE_NORMAL].crTextColor = ss->getInt(Conf::TEXT_GENERAL_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_GENERAL_BOLD))
		charFormat[TEXT_STYLE_NORMAL].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_GENERAL_ITALIC))
		charFormat[TEXT_STYLE_NORMAL].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_MY_NICK] = cf;
	charFormat[TEXT_STYLE_MY_NICK].crBackColor = ss->getInt(Conf::TEXT_MYNICK_BACK_COLOR);
	charFormat[TEXT_STYLE_MY_NICK].crTextColor = ss->getInt(Conf::TEXT_MYNICK_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_MYNICK_BOLD))
		charFormat[TEXT_STYLE_MY_NICK].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_MYNICK_ITALIC))
		charFormat[TEXT_STYLE_MY_NICK].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_MY_MESSAGE] = cf;
	charFormat[TEXT_STYLE_MY_MESSAGE].crBackColor = ss->getInt(Conf::TEXT_MYOWN_BACK_COLOR);
	charFormat[TEXT_STYLE_MY_MESSAGE].crTextColor = ss->getInt(Conf::TEXT_MYOWN_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_MYOWN_BOLD))
		charFormat[TEXT_STYLE_MY_MESSAGE].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_MYOWN_ITALIC))
		charFormat[TEXT_STYLE_MY_MESSAGE].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_PRIVATE_CHAT] = cf;
	charFormat[TEXT_STYLE_PRIVATE_CHAT].crBackColor = ss->getInt(Conf::TEXT_PRIVATE_BACK_COLOR);
	charFormat[TEXT_STYLE_PRIVATE_CHAT].crTextColor = ss->getInt(Conf::TEXT_PRIVATE_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_PRIVATE_BOLD))
		charFormat[TEXT_STYLE_PRIVATE_CHAT].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_PRIVATE_ITALIC))
		charFormat[TEXT_STYLE_PRIVATE_CHAT].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_SYSTEM_MESSAGE] = cf;
	charFormat[TEXT_STYLE_SYSTEM_MESSAGE].crBackColor = ss->getInt(Conf::TEXT_SYSTEM_BACK_COLOR);
	charFormat[TEXT_STYLE_SYSTEM_MESSAGE].crTextColor = ss->getInt(Conf::TEXT_SYSTEM_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_SYSTEM_BOLD))
		charFormat[TEXT_STYLE_SYSTEM_MESSAGE].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_SYSTEM_ITALIC))
		charFormat[TEXT_STYLE_SYSTEM_MESSAGE].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_SERVER_MESSAGE] = cf;
	charFormat[TEXT_STYLE_SERVER_MESSAGE].crBackColor = ss->getInt(Conf::TEXT_SERVER_BACK_COLOR);
	charFormat[TEXT_STYLE_SERVER_MESSAGE].crTextColor = ss->getInt(Conf::TEXT_SERVER_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_SERVER_BOLD))
		charFormat[TEXT_STYLE_SERVER_MESSAGE].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_SERVER_ITALIC))
		charFormat[TEXT_STYLE_SERVER_MESSAGE].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_LOG] = charFormat[TEXT_STYLE_NORMAL];
	charFormat[TEXT_STYLE_LOG].crTextColor = OperaColors::blendColors(ss->getInt(Conf::TEXT_GENERAL_BACK_COLOR), ss->getInt(Conf::TEXT_GENERAL_FORE_COLOR), 0.4);

	charFormat[TEXT_STYLE_FAV_USER] = cf;
	charFormat[TEXT_STYLE_FAV_USER].crBackColor = ss->getInt(Conf::TEXT_FAV_BACK_COLOR);
	charFormat[TEXT_STYLE_FAV_USER].crTextColor = ss->getInt(Conf::TEXT_FAV_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_FAV_BOLD))
		charFormat[TEXT_STYLE_FAV_USER].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_FAV_ITALIC))
		charFormat[TEXT_STYLE_FAV_USER].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_BANNED_USER] = cf;
	charFormat[TEXT_STYLE_BANNED_USER].crBackColor = ss->getInt(Conf::TEXT_ENEMY_BACK_COLOR);
	charFormat[TEXT_STYLE_BANNED_USER].crTextColor = ss->getInt(Conf::TEXT_ENEMY_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_ENEMY_BOLD))
		charFormat[TEXT_STYLE_BANNED_USER].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_ENEMY_ITALIC))
		charFormat[TEXT_STYLE_BANNED_USER].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_OP] = cf;
	charFormat[TEXT_STYLE_OP].crBackColor = ss->getInt(Conf::TEXT_OP_BACK_COLOR);
	charFormat[TEXT_STYLE_OP].crTextColor = ss->getInt(Conf::TEXT_OP_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_OP_BOLD))
		charFormat[TEXT_STYLE_OP].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_OP_ITALIC))
		charFormat[TEXT_STYLE_OP].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_OTHER_USER] = cf;
	charFormat[TEXT_STYLE_OTHER_USER].crBackColor = ss->getInt(Conf::TEXT_NORMAL_BACK_COLOR);
	charFormat[TEXT_STYLE_OTHER_USER].crTextColor = ss->getInt(Conf::TEXT_NORMAL_FORE_COLOR);
	if (ss->getBool(Conf::TEXT_NORMAL_BOLD))
		charFormat[TEXT_STYLE_OTHER_USER].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_NORMAL_ITALIC))
		charFormat[TEXT_STYLE_OTHER_USER].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_URL] = cf;
	charFormat[TEXT_STYLE_URL].dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_BACKCOLOR | CFM_LINK | CFM_UNDERLINE;
	charFormat[TEXT_STYLE_URL].crBackColor = ss->getInt(Conf::TEXT_URL_BACK_COLOR);
	charFormat[TEXT_STYLE_URL].crTextColor = ss->getInt(Conf::TEXT_URL_FORE_COLOR);
	charFormat[TEXT_STYLE_URL].dwEffects = CFE_LINK | CFE_UNDERLINE;
	if (ss->getBool(Conf::TEXT_URL_BOLD))
		charFormat[TEXT_STYLE_URL].dwEffects |= CFE_BOLD;
	if (ss->getBool(Conf::TEXT_URL_ITALIC))
		charFormat[TEXT_STYLE_URL].dwEffects |= CFE_ITALIC;

	charFormat[TEXT_STYLE_CHEATING_USER] = cf;
	charFormat[TEXT_STYLE_CHEATING_USER].crBackColor = ss->getInt(Conf::BACKGROUND_COLOR);
	charFormat[TEXT_STYLE_CHEATING_USER].crTextColor = ss->getInt(Conf::ERROR_COLOR);
	charFormat[TEXT_STYLE_CHEATING_USER].dwEffects |= CFE_BOLD;
}

const CHARFORMAT2& Colors::getCharFormat(int textStyle)
{
	if (textStyle < 0 || textStyle >= MAX_TEXT_STYLES) textStyle = TEXT_STYLE_NORMAL;
	return charFormat[textStyle];
}

void Colors::uninit()
{
	DeleteObject(g_bgBrush);
	DeleteObject(g_tabBackgroundBrush);
}

static inline int fromHexChar(int ch)
{
	if (ch >= '0' && ch <= '9') return ch-'0';
	if (ch >= 'a' && ch <= 'f') return ch-'a'+10;
	if (ch >= 'A' && ch <= 'F') return ch-'A'+10;
	return -1;
}

bool Colors::getColorFromString(const tstring& colorText, COLORREF& color)
{
	if (colorText.empty()) return false;
	if (colorText[0] == _T('#'))
	{
		if (colorText.length() == 7)
		{
			uint32_t v = 0;
			for (tstring::size_type i = 1; i < colorText.length(); ++i)
			{
				int x = fromHexChar(colorText[i]);
				if (x < 0) return false;
				v = v << 4 | x;
			}
			color = (v >> 16) | ((v << 16) & 0xFF0000) | (v & 0x00FF00);
			return true;
		}
		if (colorText.length() == 4)
		{
			int r = fromHexChar(colorText[1]);
			int g = fromHexChar(colorText[2]);
			int b = fromHexChar(colorText[3]);
			if (r < 0 || g < 0 || b < 0) return false;
			color = r | r << 4 | g << 8 | g << 12 | b << 16 | b << 20;
			return true;
		}
		return false;
	}
	tstring colorTextLower;
	Text::toLower(colorText, colorTextLower);
	// Add constant colors http://www.computerhope.com/htmcolor.htm
	for (const auto& nc : namedColors)
	{
		if (colorTextLower == nc.name)
		{
			color = nc.color;
			return true;
		}
	}
	return false;
}
