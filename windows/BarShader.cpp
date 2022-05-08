#include "stdafx.h"
#include "BarShader.h"
#include "WinUtil.h"
#include "ColorUtil.h"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <math.h>

CBarShader::CBarShader(uint32_t dwHeight, uint32_t dwWidth, COLORREF crColor /*= 0*/, uint64_t qwFileSize /*= 1*/)
{
	m_iWidth = dwWidth;
	m_iHeight = dwHeight;
	m_qwFileSize = 0;
	m_Spans.SetAt(0, crColor);
	m_Spans.SetAt(qwFileSize, 0);
	m_bIsPreview = false;
	m_used3dlevel = 0;
	SetFileSize(qwFileSize);
}

CBarShader::~CBarShader(void)
{
}

void CBarShader::BuildModifiers()
{
	static const double dDepths[5] = { 5.5, 4.0, 3.0, 2.50, 2.25 };     //aqua bar - smoother gradient jumps...
	double  depth = dDepths[((m_used3dlevel > 5) ? (256 - m_used3dlevel) : m_used3dlevel) - 1];
	uint32_t dwCount = (m_iHeight + 1)/2;
	double piOverDepth = M_PI / depth;
	double base = M_PI_2 - piOverDepth;
	double increment = piOverDepth / static_cast<double>(dwCount - 1);
	
	m_pdblModifiers.resize(dwCount);
	for (uint32_t i = 0; i < dwCount; i++, base += increment)
		m_pdblModifiers[i] = sin(base);
}

void CBarShader::CalcPerPixelandPerByte()
{
	if (m_qwFileSize)
		m_dblPixelsPerByte = static_cast<double>(m_iWidth) / m_qwFileSize;
	else
		m_dblPixelsPerByte = 0.0;
	if (m_iWidth)
		m_dblBytesPerPixel = static_cast<double>(m_qwFileSize) / m_iWidth;
	else
		m_dblBytesPerPixel = 0.0;
}

void CBarShader::SetWidth(uint32_t width)
{
	if (m_iWidth != width)
	{
		m_iWidth = width;
		CalcPerPixelandPerByte();
	}
}

void CBarShader::SetFileSize(uint64_t qwFileSize)
{
	// !SMT!-F
	if ((int64_t)qwFileSize < 0) qwFileSize = 0;
	if (m_qwFileSize != qwFileSize)
	{
		m_qwFileSize = qwFileSize;
		CalcPerPixelandPerByte();
	}
}

void CBarShader::SetHeight(uint32_t height)
{
	if (m_iHeight != height)
	{
		m_iHeight = height;
		BuildModifiers();
	}
}

void CBarShader::FillRange(uint64_t qwStart, uint64_t qwEnd, COLORREF crColor)
{
	if (qwEnd > m_qwFileSize)
		qwEnd = m_qwFileSize;
		
	if (qwStart >= qwEnd)
		return;
	POSITION endprev, endpos = m_Spans.FindFirstKeyAfter(qwEnd + 1ui64);
	
	if ((endprev = endpos) != NULL)
		m_Spans.GetPrev(endpos);
	else
		endpos = m_Spans.GetTailPosition();
		
	COLORREF endcolor = m_Spans.GetValueAt(endpos);
	
	if ((endcolor == crColor) && (m_Spans.GetKeyAt(endpos) <= qwEnd) && (endprev != NULL))
		endpos = endprev;
	else
		endpos = m_Spans.SetAt(qwEnd, endcolor);
		
	for (POSITION pos = m_Spans.FindFirstKeyAfter(qwStart); pos != endpos;)
	{
		POSITION pos1 = pos;
		m_Spans.GetNext(pos);
		m_Spans.RemoveAt(pos1);
	}
	
	m_Spans.GetPrev(endpos);
	
	if ((endpos == NULL) || (m_Spans.GetValueAt(endpos) != crColor))
		m_Spans.SetAt(qwStart, crColor);
}

void CBarShader::Fill(COLORREF crColor)
{
	m_Spans.RemoveAll();
	m_Spans.SetAt(0, crColor);
	m_Spans.SetAt(m_qwFileSize, 0);
}

void CBarShader::Draw(HDC hdc, int iLeft, int iTop, int P3DDepth)
{
	m_used3dlevel = (byte)P3DDepth;
	COLORREF crLastColor = (COLORREF)~0, crPrevBkColor = GetBkColor(hdc);
	POSITION pos = m_Spans.GetHeadPosition();
	RECT rectSpan;
	rectSpan.top = iTop;
	rectSpan.bottom = iTop + m_iHeight;
	rectSpan.right = iLeft;
	// flylinkdc-r5xx_7.7.502.16176_20131211_210642.mini.dmp: 0xC0000091: Floating-point overflow.
	// https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=48383
	int64_t iBytesInOnePixel = static_cast<int64_t>(m_dblBytesPerPixel + 0.5);
	uint64_t qwStart = 0;
	COLORREF crColor = m_Spans.GetNextValue(pos);
	
	iLeft += m_iWidth;
	while ((pos != NULL) && (rectSpan.right < iLeft))
	{
		uint64_t qwSpan = m_Spans.GetKeyAt(pos) - qwStart;
		int iPixels = static_cast<int>(qwSpan * m_dblPixelsPerByte + 0.5);
		
		if (iPixels > 0)
		{
			rectSpan.left = rectSpan.right;
			rectSpan.right += iPixels;
			FillRect(hdc, &rectSpan, crLastColor = crColor);
			
			qwStart += static_cast<uint64_t>(iPixels * m_dblBytesPerPixel + 0.5);
		}
		else
		{
			double dRed = 0, dGreen = 0, dBlue = 0;
			uint32_t dwRed, dwGreen, dwBlue;
			uint64_t qwLast = qwStart, qwEnd = qwStart + iBytesInOnePixel;
			
			do
			{
				double  dblWeight = (min(m_Spans.GetKeyAt(pos), qwEnd) - qwLast) * m_dblPixelsPerByte;
				dRed   += GetRValue(crColor) * dblWeight;
				dGreen += GetGValue(crColor) * dblWeight;
				dBlue  += GetBValue(crColor) * dblWeight;
				if ((qwLast = m_Spans.GetKeyAt(pos)) >= qwEnd)
					break;
				crColor = m_Spans.GetValueAt(pos);
				m_Spans.GetNext(pos);
			}
			while (pos != NULL);
			rectSpan.left = rectSpan.right;
			rectSpan.right++;
			
			//  Saturation
			dwRed = static_cast<uint32_t>(dRed);
			if (dwRed > 255)
				dwRed = 255;
			dwGreen = static_cast<uint32_t>(dGreen);
			if (dwGreen > 255)
				dwGreen = 255;
			dwBlue = static_cast<uint32_t>(dBlue);
			if (dwBlue > 255)
				dwBlue = 255;
				
			FillRect(hdc, &rectSpan, crLastColor = RGB(dwRed, dwGreen, dwBlue));
			qwStart += iBytesInOnePixel;
		}
		while ((pos != NULL) && (m_Spans.GetKeyAt(pos) <= qwStart))
			crColor = m_Spans.GetNextValue(pos);
	}
	if ((rectSpan.right < iLeft) && (crLastColor != ~0))
	{
		rectSpan.left = rectSpan.right;
		rectSpan.right = iLeft;
		FillRect(hdc, &rectSpan, crLastColor);
	}
	SetBkColor(hdc, crPrevBkColor);
}

static void FillSolidRect(HDC hdc, const RECT* rc, COLORREF clr)
{
	if (clr != CLR_INVALID)
	{
		COLORREF clrOld = SetBkColor(hdc, clr);
		ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rc, nullptr, 0, nullptr);
		SetBkColor(hdc, clrOld);
	}
}

void CBarShader::FillRect(HDC hdc, LPCRECT rectSpan, COLORREF crColor)
{
	if (!crColor)
		FillSolidRect(hdc, rectSpan, crColor);
	else
	{
		if (m_pdblModifiers.empty())
			BuildModifiers();
			
		double  dblRed = GetRValue(crColor), dblGreen = GetGValue(crColor), dblBlue = GetBValue(crColor);
		double  dAdd;
		
		if (m_used3dlevel > 5)      //Cax2 aqua bar
		{
			const double dMod = 1.0 - .025 * (256 - m_used3dlevel);      //variable central darkness - from 97.5% to 87.5% of the original colour...
			dAdd = 255;
			
			dblRed = dMod * dblRed - dAdd;
			dblGreen = dMod * dblGreen - dAdd;
			dblBlue = dMod * dblBlue - dAdd;
		}
		else
			dAdd = 0;
			
		RECT rect;
		int  top = rectSpan->top, bottom = rectSpan->bottom;
		uint32_t count = (m_iHeight + 1) / 2;
		dcassert(m_pdblModifiers.size() <= (size_t) count);
		
		rect.right = rectSpan->right;
		rect.left = rectSpan->left;
		
		for (uint32_t i = 0; i < count; ++i)
		{
			double pdCurr = m_pdblModifiers[i];
			crColor = RGB(static_cast<int>(dAdd + dblRed * pdCurr),
			              static_cast<int>(dAdd + dblGreen * pdCurr),
			              static_cast<int>(dAdd + dblBlue * pdCurr));
			rect.top = top++;
			rect.bottom = top;
			FillSolidRect(hdc, &rect, crColor);
			
			rect.bottom = bottom--;
			rect.top = bottom;
			//  Fast way to fill, background color is already set inside previous FillSolidRect
			FillSolidRect(hdc, &rect, crColor);
		}
	}
}

// OperaColors
#define MIN3(a, b, c) (((a) < (b)) ? ((((a) < (c)) ? (a) : (c))) : ((((b) < (c)) ? (b) : (c))))
#define MAX3(a, b, c) (((a) > (b)) ? ((((a) > (c)) ? (a) : (c))) : ((((b) > (c)) ? (b) : (c))))
#define CENTER(a, b, c) ((((a) < (b)) && ((a) < (c))) ? (((b) < (c)) ? (b) : (c)) : ((((b) < (a)) && ((b) < (c))) ? (((a) < (c)) ? (a) : (c)) : (((a) < (b)) ? (a) : (b))))
#define ABS(a) (((a) < 0) ? (-(a)): (a))

OperaColors::FCIMap OperaColors::cache;

void OperaColors::EnlightenFlood(COLORREF clr, COLORREF& a, COLORREF& b)
{
	const HLSCOLOR hls_a = ::RGB2HLS(clr);
	const HLSCOLOR hls_b = hls_a;
	BYTE buf = HLS_L(hls_a);
	if (buf < 38)
		buf = 0;
	else
		buf -= 38;
	a = ::HLS2RGB(HLS(HLS_H(hls_a), buf, HLS_S(hls_a)));
	buf = HLS_L(hls_b);
	if (buf > 217)
		buf = 255;
	else
		buf += 38;
	b = ::HLS2RGB(HLS(HLS_H(hls_b), buf, HLS_S(hls_b)));
}

void OperaColors::ClearCache()
{
	for (auto i = cache.begin(); i != cache.end(); ++i)
	{
		delete i->second;
	}
	cache.clear();
}

void OperaColors::FloodFill(HDC hdc, int x1, int y1, int x2, int y2, const COLORREF c1, const COLORREF c2, bool light /*= true */)
{
	if (x2 <= x1 || y2 <= y1 || x2 > 10000)
		return;
		
	int w = x2 - x1;
	int h = y2 - y1;
	
	FloodCacheItem::FCIMapper fcim = { c1, c2, light };
	const auto i = cache.find(fcim);
	
	FloodCacheItem* fci = nullptr;
	if (i != cache.end())
	{
		fci = i->second;
		if (fci->h >= h && fci->w >= w)
		{
			// Perfect, this kind of flood already exist in memory, let's paint it stretched
			SetStretchBltMode(hdc, HALFTONE);
			StretchBlt(hdc, x1, y1, w, h, fci->hDC, 0, 0, fci->w, fci->h, SRCCOPY);
			return;
		}
		fci->cleanup();
	}
	else
	{
		fci = new FloodCacheItem();
		cache[fcim] = fci;
	}
	
	fci->hDC = CreateCompatibleDC(hdc); // Leak (?)
	fci->w = w;
	fci->h = h;
	fci->mapper = fcim;
	
	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(BITMAPINFOHEADER));
	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biWidth = w;
	bih.biHeight = -h;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;
	bih.biClrUsed = 32;
	fci->bitmap = CreateDIBitmap(hdc, &bih, 0, NULL, NULL, DIB_RGB_COLORS);
	const auto oldBitmap = SelectObject(fci->hDC, fci->bitmap);
	if (!DeleteObject(oldBitmap))
	{
#ifdef _DEBUG
		const auto errorCode = GetLastError();
		dcdebug("DeleteObject: error = %d", errorCode);
		dcassert(0);
#endif
	}
	
	if (!light)
	{
		for (int x = 0; x < w; ++x)
		{
			HBRUSH hBr = CreateSolidBrush(blendColors(c2, c1, double(x - x1) / (double)(w)));
			const RECT rc = { x, 0, x + 1, h };
			FillRect(fci->hDC, &rc, hBr);
			DeleteObject(hBr);
		}
	}
	else
	{
		const int MAX_SHADE = 44;
		const int SHADE_LEVEL = 90;
		static const int blendVector[MAX_SHADE] =
		{
			0, 8, 16, 20, 10, 4, 0, -2, -4, -6, -10, -12, -14, -16, -14, -12, -10, -8, -6, -4, -2, 0,
			1, 2, 3, 8, 10, 12, 14, 16, 14, 12, 10, 6, 4, 2, 0, -4, -10, -20, -16, -8, 0
		};
		for (int x = 0; x <= w; ++x)
		{
			const COLORREF cr = blendColors(c2, c1, double(x) / double(w));
			for (int y = 0; y < h; ++y)
			{
				const size_t index = (size_t) ((double(y) / h) * (MAX_SHADE - 1));
				SetPixelV(fci->hDC, x, y, brightenColor(cr, (double) blendVector[index] / (double) SHADE_LEVEL));
			}
		}
	}
	BitBlt(hdc, x1, y1, x2, y2, fci->hDC, 0, 0, SRCCOPY);
}
