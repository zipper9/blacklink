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

	static CHARFORMAT2 g_TextStyleTimestamp;
	static CHARFORMAT2 g_ChatTextGeneral;
	static CHARFORMAT2 g_ChatTextOldHistory;
	static CHARFORMAT2 g_TextStyleMyNick;
	static CHARFORMAT2 g_ChatTextMyOwn;
	static CHARFORMAT2 g_ChatTextServer;
	static CHARFORMAT2 g_ChatTextSystem;
	static CHARFORMAT2 g_TextStyleFavUsers;
	static CHARFORMAT2 g_TextStyleFavUsersBan;
	static CHARFORMAT2 g_TextStyleOPs;
	static CHARFORMAT2 g_TextStyleOtherUsers;
	static CHARFORMAT2 g_TextStyleURL;
	static CHARFORMAT2 g_ChatTextPrivate;
	static CHARFORMAT2 g_ChatTextLog;

	static COLORREF g_textColor;
	static COLORREF g_bgColor;
	static COLORREF g_tabBackground;
	static COLORREF g_tabText;

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

#endif // COLORS_H_
