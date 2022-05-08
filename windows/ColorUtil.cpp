#include "stdafx.h"
#include "ColorUtil.h"
#include <algorithm>

HLSCOLOR RGB2HLS(COLORREF rgb) noexcept
{
	uint8_t r = GetRValue(rgb);
	uint8_t g = GetGValue(rgb);
	uint8_t b = GetBValue(rgb);
	uint8_t minval = std::min(r, std::min(g, b));
	uint8_t maxval = std::max(r, std::max(g, b));
	int mdiff = int(maxval) - int(minval);
	int msum  = int(maxval) + int(minval);

	float luminance = msum / 510.0f;
	float saturation = 0.0f;
	float hue = 0.0f;

	if (maxval != minval)
	{
		float rnorm = (maxval - r) / (float) mdiff;
		float gnorm = (maxval - g) / (float) mdiff;
		float bnorm = (maxval - b) / (float) mdiff;

		saturation = (luminance <= 0.5f) ? float(mdiff) / float(msum ) : float(mdiff) / (510.0f - float(msum));

		if (r == maxval)
			hue = 60.0f * (6.0f + bnorm - gnorm);
		else
		if (g == maxval)
			hue = 60.0f * (2.0f + rnorm - bnorm);
		else
			hue = 60.0f * (4.0f + gnorm - rnorm);
		if (hue > 360.0f)
			hue -= 360.0f;
	}
	return HLS(unsigned((hue * 255) / 360), unsigned(luminance * 255), unsigned(saturation * 255));
}

static unsigned toRGB(float rm1, float rm2, float rh) noexcept
{
	if (rh > 360.0f)
		rh -= 360.0f;
	else
	if (rh < 0.0f)
		rh += 360.0f;

	if (rh < 60.0f)
		rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;
	else
	if (rh < 180.0f)
		rm1 = rm2;
	else
	if (rh < 240.0f)
		rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;

	return unsigned(rm1 * 255);
}

COLORREF HLS2RGB(HLSCOLOR hls) noexcept
{
	unsigned s = HLS_S(hls);
	unsigned l = HLS_L(hls);
	if (!s) return RGB(l, l, l);

	float hue = (HLS_H(hls) * 360) / 255.0f;
	float luminance = l / 255.0f;
	float saturation = s / 255.0f;

	float rm1, rm2;

	if (luminance <= 0.5f)
		rm2 = luminance + luminance * saturation;
	else
		rm2 = luminance + saturation - luminance * saturation;
	rm1 = 2.0f * luminance - rm2;
	unsigned r = toRGB(rm1, rm2, hue + 120.0f);
	unsigned g = toRGB(rm1, rm2, hue);
	unsigned b = toRGB(rm1, rm2, hue - 120.0f);

	return RGB(r, g, b);
}

COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S) noexcept
{
	HLSCOLOR hls = RGB2HLS(rgb);
	hls = HLS_TRANSFORM2(hls, percent_L, percent_S);
	return HLS2RGB(hls);
}

HLSCOLOR HLS_TRANSFORM2(HLSCOLOR hls, int percent_L, int percent_S) noexcept
{
	int h = HLS_H(hls);
	int l = HLS_L(hls);
	int s = HLS_S(hls);

	if (percent_L > 0)
	{
		l += ((255 - l) * percent_L) / 100;
		if (l > 255) l = 255;
	}
	else if (percent_L < 0)
	{
		l = (l * (100 + percent_L)) / 100;
	}
	if (percent_S > 0)
	{
		s += ((255 - s) * percent_S) / 100;
		if (s > 255) s = 255;
	}
	else if (percent_S < 0)
	{
		s = (s * (100 + percent_S)) / 100;
	}
	return HLS(h, l, s);
}
