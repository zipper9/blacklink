#include "stdafx.h"
#include "GdiUtil.h"

#if defined _M_X64 || defined _M_IX86
#include <immintrin.h>
#endif
#ifdef _M_IX86
#include <intrin.h> // __cpuid
#endif
#if defined _M_ARM || defined _M_ARM64
#include <arm_neon.h>
#endif

void WinUtil::drawAlphaBitmap(HDC hdc, HBITMAP bitmap, int destX, int destY, int srcX, int srcY, int width, int height)
{
	BLENDFUNCTION bf;
	bf.BlendOp = AC_SRC_OVER;
	bf.BlendFlags = 0;
	bf.SourceConstantAlpha = 255;
	bf.AlphaFormat = AC_SRC_ALPHA;
	HDC bitmapDC = CreateCompatibleDC(hdc);
	HGDIOBJ oldBitmap = SelectObject(bitmapDC, bitmap);
	AlphaBlend(hdc, destX, destY, width, height, bitmapDC, srcX, srcY, width, height, bf);
	SelectObject(bitmapDC, oldBitmap);
	DeleteDC(bitmapDC);
}

void WinUtil::drawAlphaBitmap(HDC hdc, HBITMAP bitmap, int x, int y, int width, int height)
{
	drawAlphaBitmap(hdc, bitmap, x, y, 0, 0, width, height);
}

void WinUtil::drawMonoBitmap(HDC hdc, HBITMAP bitmap, int x, int y, int width, int height, COLORREF color)
{
	static const DWORD ROP_DSno = 0x00BB0226;
	static const DWORD ROP_DSa = 0x008800C6;
	static const DWORD ROP_DSo = 0x00EE0086;
	static const DWORD ROP_DSna = 0x00220326;

	HDC hdcMask, hdcColor = nullptr;
	HBITMAP tempBmp = nullptr;
	HGDIOBJ oldBmp1, oldBmp2 = nullptr;

	bool isWhite = (color & 0xFFFFFF) == 0xFFFFFF;
	bool isBlack = (color & 0xFFFFFF) == 0;

	hdcMask = CreateCompatibleDC(hdc);
	oldBmp1 = SelectObject(hdcMask, bitmap);
	if (!isWhite && !isBlack)
	{
		hdcColor = CreateCompatibleDC(hdc);
		tempBmp = CreateCompatibleBitmap(hdc, width, height);
		oldBmp2 = SelectObject(hdcColor, tempBmp);

		HBRUSH brush = CreateSolidBrush(color);
		RECT rc = { 0, 0, width, height };
		FillRect(hdcColor, &rc, brush);
		DeleteObject(brush);
	}

	COLORREF oldBkColor = SetBkColor(hdc, RGB(255, 255, 255));
	COLORREF oldTextColor = SetTextColor(hdc, RGB(0, 0, 0));

	if (isWhite)
	{
		BitBlt(hdc, x, y, width, height, hdcMask, 0, 0, ROP_DSno);
	}
	else if (isBlack)
	{
		BitBlt(hdc, x, y, width, height, hdcMask, 0, 0, ROP_DSa);
	}
	else
	{
		BitBlt(hdcColor, 0, 0, width, height, hdcMask, 0, 0, ROP_DSna);
		BitBlt(hdc, x, y, width, height, hdcMask, 0, 0, ROP_DSa);
		BitBlt(hdc, x, y, width, height, hdcColor, 0, 0, ROP_DSo);
	}

	if (hdcColor)
	{
		SelectObject(hdcColor, oldBmp2);
		DeleteObject(hdcColor);
		DeleteObject(tempBmp);
	}

	SelectObject(hdcMask, oldBmp1);
	DeleteObject(hdcMask);

	SetBkColor(hdc, oldBkColor);
	SetTextColor(hdc, oldTextColor);
}

HBITMAP WinUtil::createFrameControlBitmap(HDC hdc, int width, int height, int type, int flags)
{
	HDC hDestDC = CreateCompatibleDC(hdc);
	HBITMAP bitmap = CreateBitmap(width, height, 1, 1, nullptr);
	HGDIOBJ oldBitmap = SelectObject(hDestDC, bitmap);
	RECT rc = { 0, 0, width, height };
	DrawFrameControl(hDestDC, &rc, type, flags);
	SelectObject(hDestDC, oldBitmap);
	DeleteDC(hDestDC);
	return bitmap;
}

void WinUtil::drawFrame(HDC hdc, const RECT& rc, int width, int height, HBRUSH brush)
{
	RECT rc2 = { rc.left, rc.top, rc.right, rc.top + height };
	FillRect(hdc, &rc2, brush);
	RECT rc3 = { rc.left, rc.bottom - height, rc.right, rc.bottom };
	FillRect(hdc, &rc3, brush);
	RECT rc4 = { rc.left, rc.top + height, rc.left + width, rc.bottom - height };
	FillRect(hdc, &rc4, brush);
	RECT rc5 = { rc.right - width, rc.top + height, rc.right, rc.bottom - height };
	FillRect(hdc, &rc5, brush);
}

void WinUtil::drawEdge(HDC hdc, const RECT& rc, int mode, HBRUSH brush1, HBRUSH brush2)
{
	dcassert(mode == 0 || mode == 1);
	RECT rc2;
	rc2.left = rc.left;
	rc2.top = rc.top;
	rc2.right = rc.right - mode;
	rc2.bottom = rc2.top + 1;
	FillRect(hdc, &rc2, brush1);
	rc2.top = rc2.bottom;
	rc2.right = rc2.left + 1;
	rc2.bottom = rc.bottom - mode;
	FillRect(hdc, &rc2, brush1);
	rc2.right = rc.right;
	rc2.bottom = rc.bottom;
	rc2.left = rc.left + 1 - mode;
	rc2.top = rc2.bottom - 1;
	FillRect(hdc, &rc2, brush2);
	rc2.bottom = rc2.top;
	rc2.left = rc2.right - 1;
	rc2.top = rc.top + 1 - mode;
	FillRect(hdc, &rc2, brush2);
}

int WinUtil::getDisplayDpi()
{
	HDC hDC = CreateIC(_T("DISPLAY"), nullptr, nullptr, nullptr);
	if (!hDC) return 0;
	int dpi = GetDeviceCaps(hDC, LOGPIXELSX);
	DeleteDC(hDC);
	return dpi;
}

void WinUtil::blend32Slow(const uint8_t* a, const uint8_t* b, uint8_t* c, unsigned pixelCount, int alpha)
{
	uint16_t alpha1 = alpha;
	uint16_t alpha2 = 256 - alpha;
	while (pixelCount)
	{
		c[0] = (a[0] * alpha1 + b[0] * alpha2) >> 8;
		c[1] = (a[1] * alpha1 + b[1] * alpha2) >> 8;
		c[2] = (a[2] * alpha1 + b[2] * alpha2) >> 8;
		c[3] = 0xFF;
		a += 4;
		b += 4;
		c += 4;
		pixelCount--;
	}
}

#if defined _M_X64  || defined _M_IX86

void WinUtil::blend32(const uint8_t* a, const uint8_t* b, uint8_t* c, unsigned pixelCount, int alpha)
{
	__m128i zero = _mm_setzero_si128();
	__m128i coeff1 = _mm_set1_epi16((short) alpha);
	__m128i coeff2 = _mm_set1_epi16((short) (256 - alpha));
	unsigned count = pixelCount >> 1;
	while (count)
	{
		__m128i x = _mm_loadl_epi64((const __m128i*) a);
		x = _mm_unpacklo_epi8(x, zero);
		__m128i y = _mm_loadl_epi64((const __m128i*) b);
		y = _mm_unpacklo_epi8(y, zero);
		__m128i v1 = _mm_mullo_epi16(x, coeff1);
		__m128i v2 = _mm_mullo_epi16(y, coeff2);
		v1 = _mm_add_epi16(v1, v2);
		__m128i v3 = _mm_srli_epi16(v1, 8);
		__m128i res = _mm_packus_epi16(v3, v3);
		_mm_storel_epi64((__m128i*) c, res);
		a += 8;
		b += 8;
		c += 8;
		count--;
	}
	if (pixelCount & 1)
		blend32Slow(a, b, c, 1, alpha);
}

#elif defined _M_ARM || defined _M_ARM64

void WinUtil::blend32(const uint8_t* a, const uint8_t* b, uint8_t* c, unsigned pixelCount, int alpha)
{
	if (alpha == 0 || alpha == 256)
	{
		memcpy(c, alpha == 0 ? b : a, pixelCount * 4);
		return;
	}
	uint8x8_t coeff1 = vdup_n_u8((uint8_t) alpha);
	uint8x8_t coeff2 = vdup_n_u8((uint8_t) (256 - alpha));
	unsigned count = pixelCount >> 1;
	while (count)
	{
		uint32x2_t v1 = vld1_u32((const uint32_t *) a);
		uint32x2_t v2 = vld1_u32((const uint32_t *) b);
		uint16x8_t v3 = vaddq_u16(
			vmull_u8(vreinterpret_u8_u32(v1), coeff1),
			vmull_u8(vreinterpret_u8_u32(v2), coeff2));
		uint8x8_t v4 = vshrn_n_u16(v3, 8);
		vst1_u32((uint32_t *) c, vreinterpret_u32_u8(v4));
		a += 8;
		b += 8;
		c += 8;
		count--;
	}
	if (pixelCount & 1)
		blend32Slow(a, b, c, 1, alpha);
}

#else

void WinUtil::blend32(const uint8_t* a, const uint8_t* b, uint8_t* c, unsigned pixelCount, int alpha)
{
	blend32Slow(a, b, c, pixelCount, alpha);
}

#endif

bool WinUtil::hasFastBlend()
{
#if defined _M_X64 || defined _M_ARM || defined _M_ARM64
	return true;
#elif defined _M_IX86
	int info[4];
	__cpuid(info, 1);
	return (info[3] & 1<<26) != 0; // SSE2
#else
	return false;
#endif
}
