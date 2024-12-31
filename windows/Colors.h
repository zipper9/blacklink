#ifndef COLORS_H_
#define COLORS_H_

#include "../client/w.h"
#include "../client/typedefs.h"
#include <atlbase.h>
#include <atlctrls.h>

struct Colors
{
	static void init();
	static void uninit();

	static bool getColorFromString(const tstring& colorText, COLORREF& color);

	enum
	{
		TEXT_STYLE_NORMAL,
		TEXT_STYLE_TIMESTAMP,
		TEXT_STYLE_MY_MESSAGE,
		TEXT_STYLE_SERVER_MESSAGE,
		TEXT_STYLE_SYSTEM_MESSAGE,
		TEXT_STYLE_MY_NICK,
		TEXT_STYLE_FAV_USER,
		TEXT_STYLE_BANNED_USER,
		TEXT_STYLE_OP,
		TEXT_STYLE_CHEATING_USER,
		TEXT_STYLE_OTHER_USER,
		TEXT_STYLE_URL,
		TEXT_STYLE_PRIVATE_CHAT,
		TEXT_STYLE_LOG,
		MAX_TEXT_STYLES
	};

	static CHARFORMAT2 charFormat[MAX_TEXT_STYLES];

	static const CHARFORMAT2& getCharFormat(int textStyle);

	static COLORREF g_textColor;
	static COLORREF g_bgColor;
	static COLORREF g_tabBackground;
	static COLORREF g_tabText;
	static bool isAppThemed;
	static bool isDarkTheme;

	static HBRUSH g_bgBrush;
	static LRESULT setColor(HDC hdc)
	{
		::SetBkColor(hdc, g_bgColor);
		::SetTextColor(hdc, g_textColor);
		return reinterpret_cast<LRESULT>(g_bgBrush);
	}

	static HBRUSH g_tabBackgroundBrush;
};

static inline void setListViewColors(CListViewCtrl& ctrlList)
{
	ctrlList.SetBkColor(Colors::g_bgColor);
	ctrlList.SetTextBkColor(Colors::g_bgColor);
	ctrlList.SetTextColor(Colors::g_textColor);
}

static inline void setTreeViewColors(CTreeViewCtrl& ctrlTree)
{
	ctrlTree.SetBkColor(Colors::g_bgColor);
	ctrlTree.SetTextColor(Colors::g_textColor);
}

#endif // COLORS_H_
