#include "stdafx.h"
#include "Colors.h"
#include "BarShader.h"
#include "../client/SettingsManager.h"

HBRUSH Colors::g_bgBrush = nullptr;
COLORREF Colors::g_textColor = 0;
COLORREF Colors::g_bgColor = 0;

CHARFORMAT2 Colors::g_TextStyleTimestamp;
CHARFORMAT2 Colors::g_ChatTextGeneral;
CHARFORMAT2 Colors::g_ChatTextOldHistory;
CHARFORMAT2 Colors::g_TextStyleMyNick;
CHARFORMAT2 Colors::g_ChatTextMyOwn;
CHARFORMAT2 Colors::g_ChatTextServer;
CHARFORMAT2 Colors::g_ChatTextSystem;
CHARFORMAT2 Colors::g_TextStyleFavUsers;
CHARFORMAT2 Colors::g_TextStyleFavUsersBan;
CHARFORMAT2 Colors::g_TextStyleOPs;
CHARFORMAT2 Colors::g_TextStyleOtherUsers;
CHARFORMAT2 Colors::g_TextStyleURL;
CHARFORMAT2 Colors::g_ChatTextPrivate;
CHARFORMAT2 Colors::g_ChatTextLog;

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
	g_textColor = SETTING(TEXT_COLOR);
	g_bgColor = SETTING(BACKGROUND_COLOR);
	
	if (g_bgBrush) DeleteObject(g_bgBrush);
	g_bgBrush = CreateSolidBrush(Colors::g_bgColor);
	
	CHARFORMAT2 cf;
	memset(&cf, 0, sizeof(CHARFORMAT2));
	cf.cbSize = sizeof(cf);
	cf.dwReserved = 0;
	cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD | CFM_ITALIC;
	cf.dwEffects = 0;
	cf.crBackColor = SETTING(BACKGROUND_COLOR);
	cf.crTextColor = SETTING(TEXT_COLOR);
	
	g_TextStyleTimestamp = cf;
	g_TextStyleTimestamp.crBackColor = SETTING(TEXT_TIMESTAMP_BACK_COLOR);
	g_TextStyleTimestamp.crTextColor = SETTING(TEXT_TIMESTAMP_FORE_COLOR);
	if (SETTING(TEXT_TIMESTAMP_BOLD))
		g_TextStyleTimestamp.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_TIMESTAMP_ITALIC))
		g_TextStyleTimestamp.dwEffects |= CFE_ITALIC;
		
	g_ChatTextGeneral = cf;
	g_ChatTextGeneral.crBackColor = SETTING(TEXT_GENERAL_BACK_COLOR);
	g_ChatTextGeneral.crTextColor = SETTING(TEXT_GENERAL_FORE_COLOR);
	if (SETTING(TEXT_GENERAL_BOLD))
		g_ChatTextGeneral.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_GENERAL_ITALIC))
		g_ChatTextGeneral.dwEffects |= CFE_ITALIC;
		
	g_ChatTextOldHistory = cf;
	g_ChatTextOldHistory.crBackColor = SETTING(TEXT_GENERAL_BACK_COLOR);
	g_ChatTextOldHistory.crTextColor = SETTING(TEXT_GENERAL_FORE_COLOR);
	g_ChatTextOldHistory.yHeight = 5;

	g_TextStyleMyNick = cf;
	g_TextStyleMyNick.crBackColor = SETTING(TEXT_MYNICK_BACK_COLOR);
	g_TextStyleMyNick.crTextColor = SETTING(TEXT_MYNICK_FORE_COLOR);
	if (SETTING(TEXT_MYNICK_BOLD))
		g_TextStyleMyNick.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_MYNICK_ITALIC))
		g_TextStyleMyNick.dwEffects |= CFE_ITALIC;
		
	g_ChatTextMyOwn = cf;
	g_ChatTextMyOwn.crBackColor = SETTING(TEXT_MYOWN_BACK_COLOR);
	g_ChatTextMyOwn.crTextColor = SETTING(TEXT_MYOWN_FORE_COLOR);
	if (SETTING(TEXT_MYOWN_BOLD))
		g_ChatTextMyOwn.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_MYOWN_ITALIC))
		g_ChatTextMyOwn.dwEffects |= CFE_ITALIC;
		
	g_ChatTextPrivate = cf;
	g_ChatTextPrivate.crBackColor = SETTING(TEXT_PRIVATE_BACK_COLOR);
	g_ChatTextPrivate.crTextColor = SETTING(TEXT_PRIVATE_FORE_COLOR);
	if (SETTING(TEXT_PRIVATE_BOLD))
		g_ChatTextPrivate.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_PRIVATE_ITALIC))
		g_ChatTextPrivate.dwEffects |= CFE_ITALIC;
		
	g_ChatTextSystem = cf;
	g_ChatTextSystem.crBackColor = SETTING(TEXT_SYSTEM_BACK_COLOR);
	g_ChatTextSystem.crTextColor = SETTING(TEXT_SYSTEM_FORE_COLOR);
	if (SETTING(TEXT_SYSTEM_BOLD))
		g_ChatTextSystem.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_SYSTEM_ITALIC))
		g_ChatTextSystem.dwEffects |= CFE_ITALIC;
		
	g_ChatTextServer = cf;
	g_ChatTextServer.crBackColor = SETTING(TEXT_SERVER_BACK_COLOR);
	g_ChatTextServer.crTextColor = SETTING(TEXT_SERVER_FORE_COLOR);
	if (SETTING(TEXT_SERVER_BOLD))
		g_ChatTextServer.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_SERVER_ITALIC))
		g_ChatTextServer.dwEffects |= CFE_ITALIC;
		
	g_ChatTextLog = g_ChatTextGeneral;
	g_ChatTextLog.crTextColor = OperaColors::blendColors(SETTING(TEXT_GENERAL_BACK_COLOR), SETTING(TEXT_GENERAL_FORE_COLOR), 0.4);
	
	g_TextStyleFavUsers = cf;
	g_TextStyleFavUsers.crBackColor = SETTING(TEXT_FAV_BACK_COLOR);
	g_TextStyleFavUsers.crTextColor = SETTING(TEXT_FAV_FORE_COLOR);
	if (SETTING(TEXT_FAV_BOLD))
		g_TextStyleFavUsers.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_FAV_ITALIC))
		g_TextStyleFavUsers.dwEffects |= CFE_ITALIC;
		
	g_TextStyleFavUsersBan = cf;
	g_TextStyleFavUsersBan.crBackColor = SETTING(TEXT_ENEMY_BACK_COLOR);
	g_TextStyleFavUsersBan.crTextColor = SETTING(TEXT_ENEMY_FORE_COLOR);
	if (SETTING(TEXT_ENEMY_BOLD))
		g_TextStyleFavUsersBan.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_ENEMY_ITALIC))
		g_TextStyleFavUsersBan.dwEffects |= CFE_ITALIC;
		
	g_TextStyleOPs = cf;
	g_TextStyleOPs.crBackColor = SETTING(TEXT_OP_BACK_COLOR);
	g_TextStyleOPs.crTextColor = SETTING(TEXT_OP_FORE_COLOR);
	if (SETTING(TEXT_OP_BOLD))
		g_TextStyleOPs.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_OP_ITALIC))
		g_TextStyleOPs.dwEffects |= CFE_ITALIC;
		
	g_TextStyleOtherUsers = cf;
	g_TextStyleOtherUsers.crBackColor = SETTING(TEXT_NORMAL_BACK_COLOR);
	g_TextStyleOtherUsers.crTextColor = SETTING(TEXT_NORMAL_FORE_COLOR);
	if (SETTING(TEXT_NORMAL_BOLD))
		g_TextStyleOtherUsers.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_NORMAL_ITALIC))
		g_TextStyleOtherUsers.dwEffects |= CFE_ITALIC;

	g_TextStyleURL = cf;
	g_TextStyleURL.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_BACKCOLOR | CFM_LINK | CFM_UNDERLINE;
	g_TextStyleURL.crBackColor = SETTING(TEXT_URL_BACK_COLOR);
	g_TextStyleURL.crTextColor = SETTING(TEXT_URL_FORE_COLOR);
	g_TextStyleURL.dwEffects = CFE_LINK | CFE_UNDERLINE;
	if (SETTING(TEXT_URL_BOLD))
		g_TextStyleURL.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_URL_ITALIC))
		g_TextStyleURL.dwEffects |= CFE_ITALIC;
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
