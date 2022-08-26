#ifndef FONTS_H_
#define FONTS_H_

#include "../client/w.h"
#include "../client/typedefs.h"

struct Fonts
{
	static void init();
	static void uninit();
	static void decodeFont(const tstring& setting, LOGFONT &dest);
	static tstring encodeFont(const LOGFONT& font);

	static int g_fontHeight;
	static int g_fontHeightPixl;
	static HFONT g_font;
	static HFONT g_boldFont;
	static HFONT g_systemFont;
};

#endif // FONTS_H_
