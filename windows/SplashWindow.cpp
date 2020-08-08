#include "stdafx.h"
#include "SplashWindow.h"
#include "../client/ResourceManager.h"
#include "resource.h"
#include <zlib.h>

static const int FRAMES = 40;
static const int FRAME_TIME = 40;

#ifdef _M_X64
#include <immintrin.h>
#else
#include <xmmintrin.h>
#endif

#ifdef _M_X64
static void applyFilter(const uint8_t* src, uint8_t* dest, int width, int height, const uint8_t* coeff)
{
	__m128i mask = _mm_set_epi32(0, 0, 0, 0xFFFFFF);
	__m128i zero = _mm_setzero_si128();
	__m128i coeff1 = _mm_loadl_epi64((const __m128i*) coeff);
	coeff1 = _mm_unpacklo_epi8(coeff1, zero);
	__m128i coeff2 = _mm_loadu_si32(coeff + 6);
	coeff2 = _mm_unpacklo_epi8(coeff2, zero);
	const uint8_t* sp = src;
	uint8_t* dp = dest + width + 1;
	for (int i = height - 2; i; --i)
	{
		for (int j = width - 2; j; --j)
		{
			__m128i v1 = _mm_and_si128(_mm_loadu_si32(sp), mask);
			v1 = _mm_unpacklo_epi8(v1, zero);
			__m128i v2 = _mm_loadu_si32(sp + width);
			v2 = _mm_slli_si128(_mm_unpacklo_epi8(v2, zero), 6);
			v1 = _mm_or_si128(v1, v2);
			__m128i m1 = _mm_mullo_epi16(v1, coeff1);
			__m128i v3 = _mm_and_si128(_mm_loadu_si32(sp + 2*width), mask);
			  v3 = _mm_unpacklo_epi8(v3, zero);
			__m128i m2 = _mm_mullo_epi16(v3, coeff2);
			v1 = _mm_add_epi16(m1, m2);
			v2 = _mm_srli_si128(v1, 6);
			v1 = _mm_add_epi16(v1, v2);
			v2 = _mm_srli_si128(v1, 2);
			v3 = _mm_srli_si128(v1, 4);
			v1 = _mm_add_epi16(v1, v2);
			v1 = _mm_add_epi16(v1, v3);
			v1 = _mm_srli_epi16(v1, 5);
			v1 = _mm_packus_epi16(v1, v1);
			_mm_storeu_si32(dp, v1);
			sp++;
			dp++;
		}
		sp += 2;
		dp += 2;
	}
	unsigned offset = (height - 1) * width;
	for (int i = 0; i < width; ++i)
	{
		dest[i] = src[i];
		dest[offset + i] = src[offset + i];
	}
	sp = src + width;
	dp = dest + width;
	for (int i = 0; i < height - 2; ++i)
	{
		dp[0] = sp[0];
		dp[width-1] = sp[width-1];
		sp += width;
		dp += width;
	}
}

static void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	__m128i zero = _mm_setzero_si128();
	__m128i c = _mm_unpacklo_epi8(_mm_set1_epi32(color), zero);
	__m128i d = _mm_set1_epi16(255);
	__m128i m = _mm_set1_epi16((short)(unsigned short) 0x8081);
	while (count)
	{
		__m128i x = _mm_loadu_si32(data);
		x = _mm_unpacklo_epi8(x, zero);
		__m128i w = _mm_set1_epi16(*mask);
		__m128i v1 = _mm_mullo_epi16(c, w);
		__m128i v2 = _mm_mullo_epi16(x, _mm_sub_epi16(d, w));
		v1 = _mm_add_epi16(v1, v2);
		__m128i v3 = _mm_srli_epi16(_mm_mulhi_epu16(v1, m), 7);
		__m128i res = _mm_packus_epi16(v3, v3);
		_mm_storeu_si32(data, res);
		data++;
		mask++;
		count--;
	}
}

static inline bool useEffect() { return true; }
#elif defined(_M_IX86)
static void applyFilter(const uint8_t* src, uint8_t* dest, int width, int height, const uint8_t* coeff)
{
	__m64 zero = _mm_setzero_si64();
	__m64 coeff1 = *(__m64*) coeff;
	coeff1 = _m_punpcklbw(coeff1, zero);
	__m64 coeff2 = *(__m64*) (coeff + 3);
	coeff2 = _m_punpcklbw(coeff2, zero);
	__m64 coeff3 = *(__m64*) (coeff + 6);
	coeff3 = _m_punpcklbw(coeff3, zero);
	const uint8_t *sp = src;
	uint8_t* dp = dest + width + 1;
	for (int i = height - 2; i; --i)
	{
		for (int j = width - 2; j; --j)
		{
			__m64 v1 = *(const __m64*) sp;
			v1 = _m_punpcklbw(v1, zero);
			__m64 v2 = *(const __m64*) (sp + width);
			v2 = _m_punpcklbw(v2, zero);
			__m64 m1 = _m_pmullw(v1, coeff1);
			__m64 v3 = *(const __m64*) (sp + 2*width);
			v3 = _m_punpcklbw(v3, zero);
			__m64 m2 = _m_pmullw(v2, coeff2);
			__m64 m3 = _m_pmullw(v3, coeff3);
			v1 = _m_paddw(m1, m2);
			v1 = _m_paddw(v1, m3);
			__m64 z1 = _m_psrlqi(v1, 16);
			__m64 z2 = _m_psrlqi(v1, 32);
			v1 = _m_paddw(v1, z1);
			v1 = _m_paddw(v1, z2);
			v1 = _m_psrlwi(v1, 5);
			v2 = _m_packuswb(v1, v1);
			*(__m64*) dp = v2;
			sp++;
			dp++;
		}
		sp += 2;
		dp += 2;
	}
	unsigned offset = (height - 1) * width;
	for (int i = 0; i < width; ++i)
	{
		dest[i] = src[i];
		dest[offset + i] = src[offset + i];
	}
	sp = src + width;
	dp = dest + width;
	for (int i = 0; i < height - 2; ++i)
	{
		dp[0] = sp[0];
		dp[width-1] = sp[width-1];
		sp += width;
		dp += width;
	}
	_m_empty();
}

static void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	__m64 c = _m_punpcklbw(_m_from_int(color), _mm_setzero_si64());
	__m64 d = _mm_set1_pi16(255);
	__m64 m = _mm_set1_pi16((short)(unsigned short) 0x8081);
	while (count)
	{
		__m64 x = _m_from_int(*data);
		x = _m_punpcklbw(x, _mm_setzero_si64());
		__m64 w = _mm_set1_pi16(*mask);
		__m64 v1 = _m_pmullw(c, w);
		__m64 v2 = _m_pmullw(x, _m_psubw(d, w));
		v1 = _m_paddw(v1, v2);
		__m64 v3 = _m_psrlwi(_m_pmulhuw(v1, m), 7);
		__m64 res = _m_packuswb(v3, v3);
		*data = _m_to_int(res);
		data++;
		mask++;
		count--;
	}
	_m_empty();
}

static inline bool useEffect()
{
	int info[4];
	__cpuid(info, 1);
	return (info[3] & 1<<26) != 0;
}
#else
static void applyFilter(const uint8_t* src, uint8_t* dest, int width, int height, const uint8_t* coeff)
{
}

static void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
}

static inline bool useEffect() { return false; }
#endif

static uint32_t interpolateColor(uint32_t c1, uint32_t c2, double frac)
{
	uint32_t r1 = (c1 >> 16) & 0xFF;
	uint32_t g1 = (c1 >> 8) & 0xFF;
	uint32_t b1 = c1 & 0xFF;
	uint32_t r2 = (c2 >> 16) & 0xFF;
	uint32_t g2 = (c2 >> 8) & 0xFF;
	uint32_t b2 = c2 & 0xFF;
	uint32_t r = (uint32_t) (r1*frac + r2*(1-frac));
	uint32_t g = (uint32_t) (g1*frac + g2*(1-frac));
	uint32_t b = (uint32_t) (b1*frac + b2*(1-frac));
	return r << 16 | g << 8 | b;
}

SplashWindow::SplashWindow()
{
	memset(dibSect, 0, sizeof(dibSect));
	memset(dibBuf, 0, sizeof(dibBuf));
	memset(maskBuf, 0, sizeof(maskBuf));
	frameIndex = filterIndex = maskIndex = 0;
	memDC = nullptr;
	font = nullptr;
	useEffect = ::useEffect();
}

SplashWindow::~SplashWindow()
{
	cleanup();
}

LRESULT SplashWindow::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	versionText = TSTRING_F(ABOUT_VERSION, VERSION_STR);

	BITMAPINFOHEADER bmi = {};
	bmi.biWidth = WIDTH;
	bmi.biHeight = HEIGHT;
	bmi.biBitCount = 32;
	bmi.biCompression = BI_RGB;
	bmi.biPlanes = 1;
	bmi.biSize = sizeof(bmi);
	unsigned maskSize = bmi.biWidth * bmi.biHeight;
	bmi.biSizeImage = maskSize*4;
	int count = useEffect ? _countof(maskBuf) : 1;
	for (int i = 0; i < count; ++i)
		maskBuf[i] = new uint8_t[maskSize + 8];
	count = useEffect ? _countof(dibSect) : 1;
	for (int i = 0; i < count; ++i)
	{
		dibSect[i] = CreateDIBSection(nullptr, (BITMAPINFO*) &bmi, DIB_RGB_COLORS, &dibBuf[i], nullptr, 0);
		memset(dibBuf[i], 0, bmi.biSizeImage);
	}

	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	inflateInit(&zs);

	HRSRC res = FindResource(nullptr, MAKEINTRESOURCE(IDR_LOGO), _T("RAW"));
	dcassert(res);

	unsigned resSize = SizeofResource(nullptr, res);
	HGLOBAL hGlobal = LoadResource(nullptr, res);
	dcassert(hGlobal);
	const void* resData = LockResource(hGlobal);
	
	uint8_t* in = maskBuf[useEffect ? 2 : 0];
	zs.avail_in = resSize;
	zs.next_in = (Bytef*) resData;
	zs.avail_out = maskSize;
	zs.next_out = (Bytef*) in;
	inflate(&zs, 0);

	uint32_t* out = static_cast<uint32_t*>(dibBuf[0]);
	for (unsigned i = 0; i < maskSize; ++i)
	{
		uint32_t val = 255 - in[i];
		val |= val << 8 | val << 16;
		out[i] = val;
	}

	frameIndex = filterIndex = maskIndex = 0;
	if (useEffect) SetTimer(2, 5000);
	return 0;
}

void SplashWindow::drawText(HDC hdc)
{
	if (!font)
	{
		NONCLIENTMETRICS ncm = { sizeof(ncm) };
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
		font = CreateFontIndirect(&ncm.lfMessageFont);
	}
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(0, 0, 0));
	HGDIOBJ oldFont = SelectObject(hdc, font);
	static const int textOffset = 160;
	RECT rc = { 0, textOffset, WIDTH, HEIGHT };
	DrawText(hdc, versionText.c_str(), versionText.length(), &rc, DT_CENTER|DT_TOP);
	csProgressText.lock();
	if (!progressText.empty())
	{
		rc.top = 0;
		DrawText(hdc, progressText.c_str(), progressText.length(), &rc, DT_CENTER|DT_TOP);
	}
	csProgressText.unlock();
	SelectObject(hdc, oldFont);
}

LRESULT SplashWindow::onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(&ps);
	if (!memDC) memDC = CreateCompatibleDC(hdc);
	SelectObject(memDC, dibSect[frameIndex == 0 ? 0 : 1]);
	if (frameIndex) drawText(memDC);
	BitBlt(hdc, 0, 0, WIDTH, HEIGHT, memDC, 0, 0, SRCCOPY);
	if (!frameIndex) drawText(hdc);

	EndPaint(&ps);
	return 0;
}

LRESULT SplashWindow::onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return TRUE;
}

LRESULT SplashWindow::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	cleanup();
	return 0;
}

LRESULT SplashWindow::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (wParam == 1)
	{
		if (++frameIndex == FRAMES)
		{
			frameIndex = 0;
			KillTimer(1);
			SetTimer(2, 5000);
			filterIndex ^= 1;
		}
		else
			drawNextFrame();
		Invalidate();
	}
	else 
	{
		KillTimer(2);
		SetTimer(1, FRAME_TIME);
	}
	return 0;
}

static const uint8_t filter1[10] = { 9, 8, 9, 0, 5, 0, 9, 8, 9 };
static const uint8_t filter2[10] = { 9, 8, 9, 0, 0, 0, 2, 5, 2 };
static const uint32_t SHADE_COLOR = 0xFFB4FF;

void SplashWindow::drawNextFrame()
{
	uint32_t* destBuf = static_cast<uint32_t*>(dibBuf[1]);
	uint8_t* destMask = maskBuf[maskIndex ^ 1];
	const uint8_t* srcMask = frameIndex == 1 ? maskBuf[2] : maskBuf[maskIndex];
	const uint8_t* filter = filterIndex == 0 ? filter1 : filter2;
	applyFilter(srcMask, destMask, WIDTH, HEIGHT, filter);
	maskIndex ^= 1;
	uint32_t shadeColor;
	if (frameIndex <= 12)
	{
		double frac = 1.0 - (double) frameIndex / 12;
		shadeColor = interpolateColor(0, SHADE_COLOR, frac);
	}
	else if (frameIndex >= 24)
	{
		double frac = ((double) (frameIndex - 24)) / (FRAMES - 24);
		shadeColor = interpolateColor(0xFFFFFF, SHADE_COLOR, frac);
	}
	else
		shadeColor = SHADE_COLOR;
	memset(destBuf, 0xFF, WIDTH * HEIGHT * 4);
	blend(destBuf, destMask, shadeColor, WIDTH * HEIGHT);
	blend(destBuf, maskBuf[2], 0, WIDTH * HEIGHT);
}

void SplashWindow::cleanup()
{
	for (int i = 0; i < 2; ++i)
		if (dibSect[i])
		{
			DeleteObject(dibSect[i]);
			dibSect[i] = nullptr;
			dibBuf[i] = nullptr;
		}
	for (int i = 0; i < 3; ++i)
	{
		delete[] maskBuf[i];
		maskBuf[i] = nullptr;
	}
	if (font)
	{
		DeleteObject(font);
		font = nullptr;
	}
}

void SplashWindow::setProgressText(const tstring& text)
{
	csProgressText.lock();
	progressText = text;
	csProgressText.unlock();
}
