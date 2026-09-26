#include "stdafx.h"
#include "Fonts.h"
#include "WinUtil.h"
#include "ConfUI.h"
#include "../client/Text.h"
#include "../client/StringTokenizer.h"
#include "../client/SettingsManager.h"

static tstring currentFont;

int Fonts::g_fontHeight = 0;
int Fonts::g_sysFontHeight = 0;

HFONT Fonts::g_font = nullptr;
HFONT Fonts::g_boldFont = nullptr;
HFONT Fonts::g_systemFont = nullptr;
HFONT Fonts::g_dialogFont = nullptr;

void Fonts::init()
{
	HDC hDC = CreateIC(_T("DISPLAY"), nullptr, nullptr, nullptr);
	int dpi = GetDeviceCaps(hDC, LOGPIXELSY);
	DeleteDC(hDC);

	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	_tcscpy(lf.lfFaceName, _T("MS Shell Dlg 2"));
	lf.lfHeight = -MulDiv(8, dpi, 72);
	lf.lfWeight = FW_NORMAL;
	g_dialogFont = CreateFontIndirect(&lf);
	if (!g_dialogFont)
		g_dialogFont = (HFONT) GetStockObject(DEFAULT_GUI_FONT);

	NONCLIENTMETRICS ncm = { offsetof(NONCLIENTMETRICS, lfMessageFont) + sizeof(NONCLIENTMETRICS::lfMessageFont) };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		memcpy(&lf, &ncm.lfMessageFont, sizeof(LOGFONT));
	else
		GetObject((HFONT) GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);

	g_systemFont = CreateFontIndirect(&lf);
	g_sysFontHeight = WinUtil::getTextHeight(WinUtil::g_mainWnd, g_systemFont);

	lf.lfWeight = FW_BOLD;
	g_boldFont = CreateFontIndirect(&lf);

	auto ss = SettingsManager::instance.getUiSettings();
	currentFont = Text::toT(ss->getString(Conf::TEXT_FONT));
	decodeFont(currentFont, lf);

	g_font = ::CreateFontIndirect(&lf);
	g_fontHeight = WinUtil::getTextHeight(WinUtil::g_mainWnd, g_font);
}

void Fonts::uninit()
{
	::DeleteObject(g_font);
	g_font = nullptr;
	::DeleteObject(g_boldFont);
	g_boldFont = nullptr;
	::DeleteObject(g_systemFont);
	g_systemFont = nullptr;
	::DeleteObject(g_dialogFont);
	g_dialogFont = nullptr;
}

void Fonts::decodeFont(const tstring& setting, LOGFONT &dest)
{
	const StringTokenizer<tstring, TStringList> st(setting, _T(','));
	const auto& sl = st.getTokens();

	NONCLIENTMETRICS ncm = { offsetof(NONCLIENTMETRICS, lfMessageFont) + sizeof(NONCLIENTMETRICS::lfMessageFont) };
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

bool Fonts::changeFont()
{
	auto ss = SettingsManager::instance.getUiSettings();
	tstring newFont = Text::toT(ss->getString(Conf::TEXT_FONT));
	if (newFont == currentFont)
		return false;
	currentFont = std::move(newFont);
	LOGFONT lf;
	decodeFont(currentFont, lf);
	DeleteObject(g_font);
	g_font = ::CreateFontIndirect(&lf);
	g_fontHeight = WinUtil::getTextHeight(WinUtil::g_mainWnd, g_font);
	return true;
}
