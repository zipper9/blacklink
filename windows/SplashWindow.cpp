#include "stdafx.h"
#include "SplashWindow.h"
#include "../client/ResourceManager.h"
#include "../client/version.h"
#include "resource.h"
#include <zlib.h>

static const int FRAMES = 40;
static const int FRAME_TIME = 40;
static const int EFFECT_DELAY_TIME = 5000;

static const uint8_t f1[10] = { 9, 8, 9, 0, 5, 0, 9, 8, 9 };
static const uint8_t f2[10] = { 9, 8, 9, 0, 0, 0, 2, 5, 2 };
static const uint8_t f3[10] = { 7, 1, 5, 4, 0, 4, 5, 1, 7 };

static const int FILTER_COUNT = 3;
static const uint8_t* filters[FILTER_COUNT] = { f1, f2, f3 };

static const uint32_t SHADE_COLOR = 0xFFFFB4FF;
static const uint32_t LOGO_COLOR  = 0xFF000000;

static void blendOpaqueC(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	unsigned c1 = (color>>16) & 0xFF;
	unsigned c2 = (color>>8) & 0xFF;
	unsigned c3 = color & 0xFF;
	while (count)
	{
		uint32_t x = *data;
		uint8_t m = *mask++;
		unsigned d1 = (c1*m + (255-m)*((x>>16) & 0xFF))/255;
		unsigned d2 = (c2*m + (255-m)*((x>>8) & 0xFF))/255;
		unsigned d3 = (c3*m + (255-m)*(x & 0xFF))/255;
		*data++ = d1<<16 | d2<<8 | d3;
		count--;
	}
}

#ifdef _M_X64
#include <immintrin.h>
#elif defined(_M_IX86)
#include <xmmintrin.h>
#elif defined _M_ARM || defined _M_ARM64
#include <arm_neon.h>
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

static void blendOpaque(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
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

static void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	unsigned c0 = color>>24;
	if (c0 == 255)
	{
		blendOpaque(data, mask, color, count);
		return;
	}
	__m128i zero = _mm_setzero_si128();
	__m128i c = _mm_unpacklo_epi8(_mm_set1_epi32(color), zero);
	__m128i d = _mm_set1_epi16(255);
	__m128i q = _mm_set1_epi16(c0);
	__m128i m = _mm_set1_epi16((short)(unsigned short)0x8081);
	while (count)
	{
		__m128i x = _mm_loadu_si32(data);
		x = _mm_unpacklo_epi8(x, zero);
		__m128i w = _mm_set1_epi16(*mask);
		__m128i v1 = _mm_mullo_epi16(q, w);
		__m128i v2 = _mm_srli_epi16(_mm_mulhi_epu16(v1, m), 7);
		__m128i v3 = _mm_mullo_epi16(c, v2);
		__m128i v4 = _mm_mullo_epi16(x, _mm_sub_epi16(d, v2));
		v3 = _mm_add_epi16(v3, v4);
		__m128i v5 = _mm_srli_epi16(_mm_mulhi_epu16(v3, m), 7);
		__m128i res = _mm_packus_epi16(v5, v5);
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

static void blendOpaque(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
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

static void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	unsigned c0 = color>>24;
	if (c0 == 255)
	{
		blendOpaque(data, mask, color, count);
		return;
	}
	__m64 c = _m_punpcklbw(_m_from_int(color), _mm_setzero_si64());
	__m64 d = _mm_set1_pi16(255);
	__m64 q = _mm_set1_pi16(c0);
	__m64 m = _mm_set1_pi16((short)(unsigned short) 0x8081);
	while (count)
	{
		__m64 x = _m_from_int(*data);
		x = _m_punpcklbw(x, _mm_setzero_si64());
		__m64 w = _mm_set1_pi16(*mask);
		__m64 v1 = _m_pmullw(q, w);
		__m64 v2 = _m_psrlwi(_m_pmulhuw(v1, m), 7);
		__m64 v3 = _m_pmullw(c, v2);
		__m64 v4 = _m_pmullw(x, _m_psubw(d, v2));
		v3 = _m_paddw(v3, v4);
		__m64 v5 = _m_psrlwi(_m_pmulhuw(v3, m), 7);
		__m64 res = _m_packuswb(v5, v5);
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
#elif defined _M_ARM64
static void applyFilter(const uint8_t* src, uint8_t* dest, int width, int height, const uint8_t* coeff)
{
	static const uint8_t mask1[] = { 0, 1, 2, 0xFF, 1, 2, 3, 0xFF };
	static const uint8_t mask2[] = { 0, 1, 2, 0xFF, 0, 1, 2, 0xFF };
	uint8x8_t vind = vld1_u8(mask2);
	uint8x8_t vcoeff1 = vld1_u8(coeff);
	uint8x8_t vcoeff2 = vld1_u8(coeff + 3);
	uint8x8_t vcoeff3 = vld1_u8(coeff + 6);
	vcoeff1 = vtbl1_u8(vcoeff1, vind);
	vcoeff2 = vtbl1_u8(vcoeff2, vind);
	vcoeff3 = vtbl1_u8(vcoeff3, vind);
	vind = vld1_u8(mask1);
	const uint8_t *sp = src;
	uint8_t* dp = dest + width + 1;
	for (int i = height - 2; i; --i)
	{
		for (int j = width - 2; j > 0; j -= 2)
		{
			uint8x8_t v1 = vld1_u8(sp);
			v1 = vtbl1_u8(v1, vind);
			uint8x8_t v2 = vld1_u8(sp + width);
			uint16x8_t m1 = vmull_u8(v1, vcoeff1);
			v2 = vtbl1_u8(v2, vind);
			uint8x8_t v3 = vld1_u8(sp + 2*width);
			uint16x8_t m2 = vmull_u8(v2, vcoeff2);
			v3 = vtbl1_u8(v3, vind);
			uint16x8_t m3 = vmull_u8(v3, vcoeff3);
			m1 = vaddq_u16(m1, m2);
			m1 = vaddq_u16(m1, m3);
			uint16x8_t v4 = vpaddq_u16(m1, m1);
			v4 = vpaddq_u16(v4, v4);
			v4 = vshrq_n_u16(v4, 5);
			uint8x8_t v5 = vqmovn_u16(v4);
			vst1_lane_u16((uint16_t*) dp, vreinterpret_u16_u8(v5), 0);
			sp += 2;
			dp += 2;
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

static void blendOpaque(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	static const uint8_t index[] = { 0, 0, 0, 0xFF, 1, 1, 1, 0xFF };
	uint8x8_t vcolor = vreinterpret_u8_u32(vdup_n_u32(color));
	uint8x8_t v255 = vdup_n_u8(255);
	uint8x8_t vind = vld1_u8(index);
	while (count >= 2)
	{
		uint16x4_t v1 = vld1_u16((const uint16_t*) mask);
		uint8x8_t v2 = vtbl1_u8(vreinterpret_u8_u16(v1), vind);
		uint8x8_t v4 = vsub_u8(v255, v2);
		uint16x8_t v3 = vmull_u8(v2, vcolor);
		uint8x8_t v5 = vreinterpret_u8_u32(vld1_u32(data));
		v3 = vmlal_u8(v3, v4, v5);
		uint32x4_t vhi = vmull_high_n_u16(v3, 0x8081);
		uint32x4_t vlo = vmull_n_u16(vget_low_u16(v3), 0x8081);
		v3 = vuzp2q_u16(vreinterpretq_u16_u32(vlo), vreinterpretq_u16_u32(vhi));
		uint8x8_t v7 = vshrn_n_u16(v3, 7);
		vst1_u32(data, vreinterpret_u32_u8(v7));
		data += 2;
		mask += 2;
		count -= 2;
	}
}

static void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	if ((color >> 24) == 0xFF)
	{
		blendOpaque(data, mask, color, count);
		return;
	}

	static const uint8_t index[] = { 0, 0, 0, 0xFF, 2, 2, 2, 0xFF };
	uint8x8_t va = vdup_n_u8(color >> 24);
	uint8x8_t vcolor = vreinterpret_u8_u32(vdup_n_u32(color));
	uint8x8_t v255 = vdup_n_u8(255);
	uint8x8_t vind = vld1_u8(index);
	while (count >= 2)
	{
		uint16x4_t v1 = vld1_u16((const uint16_t*) mask);
		uint16x8_t v2 = vmull_u8(vreinterpret_u8_u16(v1), va);
		uint32x4_t vhi = vmull_high_n_u16(v2, 0x8081);
		uint32x4_t vlo = vmull_n_u16(vget_low_u16(v2), 0x8081);
		uint16x8_t v3 = vuzp2q_u16(vreinterpretq_u16_u32(vlo), vreinterpretq_u16_u32(vhi));
		v1 = vshr_n_u16(vget_low_u16(v3), 7);
		uint8x8_t v4 = vtbl1_u8(vreinterpret_u8_u16(v1), vind);
		uint8x8_t v5 = vsub_u8(v255, v4);
		v3 = vmull_u8(v4, vcolor);
		uint8x8_t v6 = vreinterpret_u8_u32(vld1_u32(data));
		v3 = vmlal_u8(v3, v5, v6);
		vhi = vmull_high_n_u16(v3, 0x8081);
		vlo = vmull_n_u16(vget_low_u16(v3), 0x8081);
		v2 = vuzp2q_u16(vreinterpretq_u16_u32(vlo), vreinterpretq_u16_u32(vhi));
		uint8x8_t v7 = vshrn_n_u16(v2, 7);
		vst1_u32(data, vreinterpret_u32_u8(v7));
		data += 2;
		mask += 2;
		count -= 2;
	}
}

static inline bool useEffect() { return true; }
#elif defined _M_ARM
void blendOpaque(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	static const uint8_t colorMask[] = { 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0 };
	uint8x8_t vcolor = vreinterpret_u8_u32(vdup_n_u32(color));
	uint8x8_t vand = vld1_u8(colorMask);
	uint8x8_t v255 = vdup_n_u8(255);
	uint16x4_t vdiv = vdup_n_u16(0x8081);
	uint16x4_t vzr = vdup_n_u16(0);
	while (count)
	{
		uint8x8_t v1 = vand_u8(vld1_dup_u8(mask), vand);
		uint8x8_t v2 = vsub_u8(v255, v1);
		uint16x8_t v3 = vmull_u8(v1, vcolor);
		uint8x8_t v4 = vreinterpret_u8_u32(vld1_dup_u32(data));
		v3 = vmlal_u8(v3, v4, v2);
		uint32x4_t v5 = vmull_u16(vget_low_u16(v3), vdiv);
		uint16x4_t v6 = vshrn_n_u32(v5, 16);
		v2 = vshrn_n_u16(vcombine_u16(v6, vzr), 7);
		vst1_lane_u32(data, vreinterpret_u32_u8(v2), 0);
		data++;
		mask++;
		count--;
	}
}

void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
	unsigned c0 = color >> 24;
	if (c0 == 0xFF)
	{
		blendOpaque(data, mask, color, count);
		return;
	}

	uint8x8_t vcolor = vreinterpret_u8_u32(vdup_n_u32(color));
	uint8x8_t va = vcreate_u8(c0 | c0<<8 | c0<<16);
	uint8x8_t v255 = vdup_n_u8(255);
	uint16x4_t vdiv = vdup_n_u16(0x8081);
	uint16x4_t vzr = vdup_n_u16(0);
	while (count)
	{
		uint8x8_t v1 = vld1_dup_u8(mask);
		uint16x8_t v2 = vmull_u8(v1, va);
		uint32x4_t v3 = vmull_u16(vget_low_u16(v2), vdiv);
		uint16x4_t v4 = vshrn_n_u32(v3, 16);
		v1 = vshrn_n_u16(vcombine_u16(v4, vzr), 7);
		uint8x8_t v5 = vsub_u8(v255, v1);
		v2 = vmull_u8(v1, vcolor);
		uint8x8_t v6 = vreinterpret_u8_u32(vld1_dup_u32(data));
		v2 = vmlal_u8(v2, v5, v6);
		v3 = vmull_u16(vget_low_u16(v2), vdiv);
		v4 = vshrn_n_u32(v3, 16);
		v1 = vshrn_n_u16(vcombine_u16(v4, vzr), 7);
		vst1_lane_u32(data, vreinterpret_u32_u8(v1), 0);
		data++;
		mask++;
		count--;
	}
}

void applyFilter(const uint8_t* src, uint8_t* dest, int width, int height, const uint8_t* coeff)
{
	static const uint8_t mask1[] = { 0, 1, 2, 0xFF, 1, 2, 3, 0xFF };
	static const uint8_t mask2[] = { 0, 1, 2, 0xFF, 0, 1, 2, 0xFF };
	uint8x8_t vind = vld1_u8(mask2);
	uint8x8_t vcoeff1 = vld1_u8(coeff);
	uint8x8_t vcoeff2 = vld1_u8(coeff + 3);
	uint8x8_t vcoeff3 = vld1_u8(coeff + 6);
	vcoeff1 = vtbl1_u8(vcoeff1, vind);
	vcoeff2 = vtbl1_u8(vcoeff2, vind);
	vcoeff3 = vtbl1_u8(vcoeff3, vind);
	vind = vld1_u8(mask1);
	const uint8_t *sp = src;
	uint8_t* dp = dest + width + 1;
	for (int i = height - 2; i; --i)
	{
		for (int j = width - 2; j > 0; j -= 2)
		{
			uint8x8_t v1 = vld1_u8(sp);
			v1 = vtbl1_u8(v1, vind);
			uint8x8_t v2 = vld1_u8(sp + width);
			uint16x8_t m1 = vmull_u8(v1, vcoeff1);
			v2 = vtbl1_u8(v2, vind);
			uint8x8_t v3 = vld1_u8(sp + 2*width);
			uint16x8_t m2 = vmull_u8(v2, vcoeff2);
			v3 = vtbl1_u8(v3, vind);
			uint16x8_t m3 = vmull_u8(v3, vcoeff3);
			m1 = vaddq_u16(m1, m2);
			m1 = vaddq_u16(m1, m3);
			uint16x4_t v4 = vpadd_u16(vget_low_u16(m1), vget_high_u16(m1));
			v4 = vpadd_u16(v4, v4);
			v4 = vshr_n_u16(v4, 5);
			uint8x8_t v5 = vqmovn_u16(vcombine_u16(v4, v4));
			vst1_lane_u16((uint16_t*) dp, vreinterpret_u16_u8(v5), 0);
			sp += 2;
			dp += 2;
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

static inline bool useEffect() { return true; }
#else
static void applyFilter(const uint8_t* src, uint8_t* dest, int width, int height, const uint8_t* coeff)
{
}

static void blend(uint32_t* data, const uint8_t* mask, uint32_t color, unsigned count)
{
}

static inline bool useEffect() { return false; }
#endif

SplashWindow::SplashWindow()
{
	memset(dibSect, 0, sizeof(dibSect));
	memset(dibBuf, 0, sizeof(dibBuf));
	memset(maskBuf, 0, sizeof(maskBuf));
	frameIndex = filterIndex = maskIndex = 0;
	memDC = nullptr;
	font = nullptr;
	useEffect = ::useEffect();
	hasBorder = false;
	useDialogBackground = false;
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

	zs.avail_in = resSize;
	zs.next_in = (Bytef*) resData;
	zs.avail_out = maskSize;
	zs.next_out = (Bytef*) maskBuf[useEffect ? 2 : 0];
	inflate(&zs, 0);

	if (!IsAppThemed())
		useDialogBackground = false;

	frameIndex = filterIndex = maskIndex = 0;
	if (useEffect) SetTimer(2, EFFECT_DELAY_TIME);
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
	if (!memDC)
	{
		memDC = CreateCompatibleDC(hdc);
		SelectObject(memDC, dibSect[0]);
		uint32_t* destBuf = static_cast<uint32_t*>(dibBuf[0]);
		if (!(useDialogBackground && SUCCEEDED(DrawThemeParentBackground(m_hWnd, memDC, nullptr))))
			memset(destBuf, 0xFF, WIDTH * HEIGHT * 4);
		if (useEffect)
			blend(destBuf, maskBuf[2], LOGO_COLOR, WIDTH * HEIGHT);
		else
			blendOpaqueC(destBuf, maskBuf[0], LOGO_COLOR, WIDTH * HEIGHT);
		if (hasBorder) drawBorder(destBuf);
	}
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
			SetTimer(2, EFFECT_DELAY_TIME);
			filterIndex = (filterIndex + 1) % FILTER_COUNT;
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

void SplashWindow::drawNextFrame()
{
	bool whiteBackground = true;
	if (useDialogBackground)
	{
		SelectObject(memDC, dibSect[1]);
		if (SUCCEEDED(DrawThemeParentBackground(m_hWnd, memDC, nullptr))) whiteBackground = false;
	}
	uint32_t* destBuf = static_cast<uint32_t*>(dibBuf[1]);
	uint8_t* destMask = maskBuf[maskIndex ^ 1];
	const uint8_t* srcMask = frameIndex == 1 ? maskBuf[2] : maskBuf[maskIndex];
	const uint8_t* filter = filters[filterIndex];
	applyFilter(srcMask, destMask, WIDTH, HEIGHT, filter);
	maskIndex ^= 1;
	uint32_t shadeColor;
	if (frameIndex <= 12)
	{
		unsigned alpha = (unsigned) (255 * ((double) frameIndex / 12));
		shadeColor = (SHADE_COLOR & 0xFFFFFF) | alpha<<24;
	}
	else if (frameIndex >= 24)
	{
		unsigned alpha = (unsigned) (255 * (1.0 - ((double) (frameIndex - 24)) / (FRAMES - 24)));
		shadeColor = (SHADE_COLOR & 0xFFFFFF) | alpha<<24;
	}
	else
		shadeColor = SHADE_COLOR;
	if (whiteBackground) memset(destBuf, 0xFF, WIDTH * HEIGHT * 4);
	blend(destBuf, destMask, shadeColor, WIDTH * HEIGHT);
	blend(destBuf, maskBuf[2], LOGO_COLOR, WIDTH * HEIGHT);
	if (hasBorder) drawBorder(destBuf);
}

void SplashWindow::drawBorder(uint32_t* buf)
{
	static const uint32_t BORDER_COLOR = 0;
	for (int i = 0; i < WIDTH; i++)
		buf[i] = BORDER_COLOR;
	uint32_t* p = buf + (HEIGHT-1)*WIDTH;
	for (int i = 0; i < WIDTH; i++)
		p[i] = BORDER_COLOR;
	p = buf + WIDTH;
	for (int i = 0; i < HEIGHT-2; i++, p += WIDTH)
		*p = BORDER_COLOR;
	p = buf + 2*WIDTH - 1;
	for (int i = 0; i < HEIGHT-2; i++, p += WIDTH)
		*p = BORDER_COLOR;
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
	if (memDC)
	{
		DeleteDC(memDC);
		memDC = nullptr;
	}
}

void SplashWindow::setProgressText(const tstring& text)
{
	csProgressText.lock();
	progressText = text;
	csProgressText.unlock();
}
