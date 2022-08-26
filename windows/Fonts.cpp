#include "stdafx.h"
#include "Fonts.h"
#include "WinUtil.h"
#include "StringTokenizer.h"

HFONT Fonts::g_font = nullptr;
int Fonts::g_fontHeight = 0;
int Fonts::g_fontHeightPixl = 0;
HFONT Fonts::g_boldFont = nullptr;
HFONT Fonts::g_systemFont = nullptr;

void Fonts::init()
{
	LOGFONT lf;
	NONCLIENTMETRICS ncm = { sizeof(ncm) };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		memcpy(&lf, &ncm.lfMessageFont, sizeof(LOGFONT));
	else
		GetObject((HFONT) GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);

	g_systemFont = CreateFontIndirect(&lf);
	
	lf.lfWeight = FW_BOLD;
	g_boldFont = CreateFontIndirect(&lf);
	
	decodeFont(Text::toT(SETTING(TEXT_FONT)), lf);
	
	g_font = ::CreateFontIndirect(&lf);
	g_fontHeight = WinUtil::getTextHeight(WinUtil::g_mainWnd, g_font);

	HDC hDC = CreateIC(_T("DISPLAY"), nullptr, nullptr, nullptr);
	g_fontHeightPixl = -MulDiv(lf.lfHeight, GetDeviceCaps(hDC, LOGPIXELSY), 72); // FIXME
	DeleteDC(hDC);
}

void Fonts::uninit()
{
	::DeleteObject(g_font);
	g_font = nullptr;
	::DeleteObject(g_boldFont);
	g_boldFont = nullptr;
	::DeleteObject(g_systemFont);
	g_systemFont = nullptr;
}

void Fonts::decodeFont(const tstring& setting, LOGFONT &dest)
{
	const StringTokenizer<tstring, TStringList> st(setting, _T(','));
	const auto& sl = st.getTokens();
	
	NONCLIENTMETRICS ncm = { sizeof(ncm) };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		memcpy(&dest, &ncm.lfMessageFont, sizeof(LOGFONT));
	else
		GetObject((HFONT) GetStockObject(DEFAULT_GUI_FONT), sizeof(dest), &dest);

	tstring face;
	if (sl.size() == 4)
	{
		face = sl[0];
		dest.lfHeight = Util::toInt(sl[1]);
		dest.lfWeight = Util::toInt(sl[2]);
		dest.lfItalic = (BYTE)Util::toInt(sl[3]);
	}
	
	if (!face.empty() && face.length() < LF_FACESIZE)
		_tcscpy(dest.lfFaceName, face.c_str());
}

tstring Fonts::encodeFont(const LOGFONT& font)
{
	tstring res(font.lfFaceName);
	res += _T(',');
	res += Util::toStringT(font.lfHeight);
	res += _T(',');
	res += Util::toStringT(font.lfWeight);
	res += _T(',');
	res += Util::toStringT(font.lfItalic);
	return res;
}
