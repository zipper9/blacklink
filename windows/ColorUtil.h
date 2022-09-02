#ifndef COLOR_UTIL_H_
#define COLOR_UTIL_H_

#include "../client/w.h"

typedef uint32_t HLSCOLOR;

#define HLS(h, l, s) ((uint32_t) (h) | (uint32_t) (l) << 8 | (uint32_t) (s) << 16)
#define HLS_H(hls) ((hls) & 0xFF)
#define HLS_L(hls) (((hls) >> 8) & 0xFF)
#define HLS_S(hls) (((hls) >> 16) & 0xFF)

HLSCOLOR RGB2HLS(COLORREF rgb) noexcept;
COLORREF HLS2RGB(HLSCOLOR hls) noexcept;

COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S) noexcept;
HLSCOLOR HLS_TRANSFORM2(HLSCOLOR hls, int percent_L, int percent_S) noexcept;

namespace ColorUtil
{

	inline COLORREF textFromBackground(COLORREF bg) noexcept
	{
		unsigned r = GetRValue(bg);
		unsigned g = GetGValue(bg);
		unsigned b = GetBValue(bg);
		double y = 0.299 * r + 0.587 * g + 0.114 * b;
		return y > 0.5 * 255 ? RGB(0, 0, 0) : RGB(255, 255, 255);
	}

	inline COLORREF lighter(COLORREF color) noexcept
	{
		return HLS_TRANSFORM(color, 35, -20);
	}

};

#endif // COLOR_UTIL_H_
