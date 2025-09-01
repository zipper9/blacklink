#ifndef GDI_UTIL_H_
#define GDI_UTIL_H_

#include "../client/w.h"
#include <stdint.h>

namespace WinUtil
{

	void drawBitmap(HDC hdc, HBITMAP bitmap, int x, int y, int width, int height);
	void drawBitmap(HDC hdc, HBITMAP bitmap, int destX, int destY, int srcX, int srcY, int width, int height);
	void drawAlphaBitmap(HDC hdc, HBITMAP bitmap, int x, int y, int width, int height);
	void drawAlphaBitmap(HDC hdc, HBITMAP bitmap, int destX, int destY, int srcX, int srcY, int width, int height);
	void drawMonoBitmap(HDC hdc, HBITMAP bitmap, int x, int y, int width, int height, COLORREF color);
	HBITMAP createFrameControlBitmap(HDC hdc, int width, int height, int type, int flags);
	void drawFrame(HDC hdc, const RECT& rc, int width, int height, HBRUSH brush);
	void drawEdge(HDC hdc, const RECT& rc, int mode, HBRUSH brush1, HBRUSH brush2);
	int getDisplayDpi();
	void blend32Slow(const uint8_t* a, const uint8_t* b, uint8_t* c, unsigned pixelCount, int alpha);
	void blend32(const uint8_t* a, const uint8_t* b, uint8_t* c, unsigned pixelCount, int alpha);
	bool hasFastBlend();

}

#endif // GDI_UTIL_H_
