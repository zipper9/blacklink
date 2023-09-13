#include "stdafx.h"

#ifdef IRAINMAN_INCLUDE_GDI_OLE
#include "GdiImage.h"
#include "../client/debug.h"

#ifdef DEBUG_GDI_IMAGE
#include <unordered_set>
#include "../client/Locks.h"

#ifdef _DEBUG
#include "../client/StrUtil.h"
#endif

static FastCriticalSection csImageSet;
static std::unordered_set<CGDIImage*> imageSet;
size_t g_AnimationCount = 0;
size_t g_AnimationCountMax = 0;

bool CGDIImage::checkImage(CGDIImage* image)
{
	if (isShutdown()) return false;
	bool res;
	{
		LOCK(csImageSet);
		res = imageSet.find(image) != imageSet.end();
	}
	if (!res)
	{
		dcdebug("CGDIImage: invalid image %p\n", image);
		dcassert(0);
	}
	return res;
}

size_t CGDIImage::getImageCount()
{
	LOCK(csImageSet);
	return imageSet.size();
}

#ifdef _DEBUG
tstring CGDIImage::getLoadedList()
{
	tstring res;
	LOCK(csImageSet);
	for (const CGDIImage* image : imageSet)
	{
		if (!res.empty()) res += _T('\n');
		res += _T("0x") + Util::toHexStringT(image) + _T(": ");
		res += image->loadedFileName;
		res += _T(" refs=") + Util::toStringT(image->m_lRef);
		res += _T(" timer=") + Util::toHexStringT(image->m_hTimer);
		EnterCriticalSection(&image->m_csCallback);
		size_t callbacks = image->m_Callbacks.size();
		LeaveCriticalSection(&image->m_csCallback);
		res += _T(" callbacks=") + Util::toStringT(callbacks);
	}
	return res;
}
#endif

static void removeImage(CGDIImage* image)
{
	LOCK(csImageSet);
	imageSet.erase(image);
}

static void updateStats(size_t callbacks)
{
	g_AnimationCount = callbacks;
	if (g_AnimationCount > g_AnimationCountMax)
		g_AnimationCountMax = g_AnimationCount;
}
#endif

CGDIImage::CGDIImage(const WCHAR* fileName):
	m_dwFramesCount(0), m_pImage(nullptr), m_pItem(nullptr), m_hTimer(nullptr), m_lRef(1),
	m_hCallbackWnd(nullptr), m_dwCallbackMsg(0),
	m_dwWidth(0),
	m_dwHeight(0),
	m_dwCurrentFrame(0)
{
	dcassert(!isShutdown());
	InitializeCriticalSection(&m_csCallback);
	m_pImage = new Gdiplus::Image(fileName);
	if (m_pImage->GetLastStatus() != Gdiplus::Ok)
	{
		delete m_pImage;
		m_pImage = nullptr;
		return;
	}
#ifdef _DEBUG
	loadedFileName = fileName;
#endif
	m_dwWidth = m_pImage->GetWidth();
	m_dwHeight = m_pImage->GetHeight();
	
	if (m_pImage->GetType() == Gdiplus::ImageTypeBitmap)
	{
		Gdiplus::Bitmap* bitmap = static_cast<Gdiplus::Bitmap*>(m_pImage);
		if (bitmap->GetFlags() & Gdiplus::ImageFlagsHasRealDPI)
		{
			HDC hdc = GetDC(nullptr);
			if (hdc)
			{
				bitmap->SetResolution(GetDeviceCaps(hdc, LOGPIXELSX), GetDeviceCaps(hdc, LOGPIXELSY));
				ReleaseDC(nullptr, hdc);
			}
		}
		applyMask(bitmap);
	}
	if (UINT TotalBuffer = m_pImage->GetPropertyItemSize(PropertyTagFrameDelay))
	{
		m_pItem = (Gdiplus::PropertyItem*)new char[TotalBuffer]; //-V121
		memset(m_pItem, 0, TotalBuffer); //-V106
		m_dwCurrentFrame = 0;
		m_dwWidth  = 0;
		m_dwHeight = 0;
		if (!m_pImage->GetPropertyItem(PropertyTagFrameDelay, TotalBuffer, m_pItem))
		{
			if (SelectActiveFrame(m_dwCurrentFrame))
				if (const DWORD dwFrameCount = GetFrameCount())
				{
					// [!] brain-ripper
					// GDI+ seemed to have bug,
					// and for some GIF's returned zero delay for all frames.
					// Make such a workaround: take 50ms delay on every frame
					bool bHaveDelay = false;
					for (DWORD i = 0; i < dwFrameCount; i++)
					{
						if (GetFrameDelay(i) > 0)
						{
							bHaveDelay = true;
							break;
						}
					}
					if (!bHaveDelay)
					{
						for (DWORD i = 0; i < dwFrameCount; i++)
						{
							((UINT*)m_pItem[0].value)[i] = 5; //-V108
						}
					}
				}
			// [!] brain-ripper
			// Strange bug - m_pImage->GetWidth() and m_pImage->GetHeight()
			// sometimes returns zero, even object successfuly loaded.
			// Cache this values here to return correct values later
			m_dwWidth = m_pImage->GetWidth();
			m_dwHeight = m_pImage->GetHeight();
		}
		else
		{
			freePropItem();
		}
	}
}

CGDIImage::~CGDIImage()
{
	_ASSERTE(m_Callbacks.empty());
	if (m_hTimer)
	{
		destroyTimer(this, INVALID_HANDLE_VALUE);
		// INVALID_HANDLE_VALUE:  wait for any running timer callback functions to complete 
	}
	delete m_pImage;
	freePropItem();
	DeleteCriticalSection(&m_csCallback);
}

void CGDIImage::applyMask(Gdiplus::Bitmap* bitmap)
{
	Gdiplus::BitmapData bitmapData;
	if (bitmap->LockBits(nullptr, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite,
		PixelFormat32bppARGB, &bitmapData) != Gdiplus::Ok) return;
	uint8_t* ptr = static_cast<uint8_t*>(bitmapData.Scan0);
	uint32_t* data = reinterpret_cast<uint32_t*>(ptr);
	const uint32_t maskColor = data[0] & 0xFFFFFF;
	for (unsigned y = 0; y < bitmapData.Height; ++y)
	{
		data = reinterpret_cast<uint32_t*>(ptr);
		for (unsigned x = 0; x < bitmapData.Width; ++x)
		{
			if ((data[x] & 0xFFFFFF) == maskColor)
				data[x] &= 0xFFFFFF;
		}
		ptr += bitmapData.Stride;
	}
	bitmap->UnlockBits(&bitmapData);
}

void CGDIImage::Draw(HDC hDC, int xDst, int yDst, int wSrc, int hSrc, int xSrc, int ySrc, HDC hBackDC, int xBk, int yBk, int wBk, int hBk)
{
	if (hBackDC)
	{
		BitBlt(hBackDC, xBk, yBk, wBk, hBk, NULL, 0, 0, PATCOPY);
		Gdiplus::Graphics Graph(hBackDC);
		Graph.DrawImage(m_pImage, xBk, yBk, xSrc, ySrc, wSrc, hSrc, Gdiplus::UnitPixel);
		BitBlt(hDC, xDst, yDst, wBk, hBk, hBackDC, 0, 0, SRCCOPY);
	}
	else
	{
		Gdiplus::Graphics Graph(hDC);
		Graph.DrawImage(m_pImage, xDst, yDst, xSrc, ySrc, wSrc, hSrc, Gdiplus::UnitPixel);
	}
}

DWORD CGDIImage::GetFrameDelay(DWORD dwFrame)
{
	if (m_pItem)
		return ((UINT*)m_pItem[0].value)[dwFrame] * 10;
	else
		return 5;
}

bool CGDIImage::SelectActiveFrame(DWORD dwFrame)
{
	dcassert(!isShutdown());
	if (!isShutdown())
	{
		static const GUID g_Guid = Gdiplus::FrameDimensionTime;
		if (m_pImage) // crash https://drdump.com/DumpGroup.aspx?DumpGroupID=230505&Login=guest
			m_pImage->SelectActiveFrame(&g_Guid, dwFrame); // [1] https://www.box.net/shared/x4tgntvw818gzd274nek
		return true;
		/* [!]TODO
		if(m_pImage)
		return m_pImage->SelectActiveFrame(&g_Guid, dwFrame) == Ok;
		else
		return false;
		*/
	}
	else
	{
		return false;
	}
}

DWORD CGDIImage::GetFrameCount()
{
	dcassert(!isShutdown());
	if (m_dwFramesCount == 0)
	{
		//First of all we should get the number of frame dimensions
		//Images considered by GDI+ as:
		//frames[animation_frame_index][how_many_animation];
		if (const UINT count = m_pImage->GetFrameDimensionsCount())
		{
			//Now we should get the identifiers for the frame dimensions
			std::vector<GUID> l_pDimensionIDs(count); //-V121
			m_pImage->GetFrameDimensionsList(&l_pDimensionIDs[0], count);
			//For gif image , we only care about animation set#0
			m_dwFramesCount = m_pImage->GetFrameCount(&l_pDimensionIDs[0]);
		}
	}
	return m_dwFramesCount;
}

void CGDIImage::DrawFrame()
{
	dcassert(!isShutdown());
	
	EnterCriticalSection(&m_csCallback);
	static int g_count = 0;
	for (auto i = m_Callbacks.cbegin(); i != m_Callbacks.cend(); ++i)
	{
		if (!i->pOnFrameChangedProc(this, i->lParam))
		{
			i = m_Callbacks.erase(i);
			if (i == m_Callbacks.end())
				break;
		}
	}
	LeaveCriticalSection(&m_csCallback);
}

void CGDIImage::destroyTimer(CGDIImage *pGDIImage, HANDLE completionEvent)
{
	EnterCriticalSection(&pGDIImage->m_csCallback);
	if (pGDIImage->m_hTimer)
	{
		if (!DeleteTimerQueueTimer(NULL, pGDIImage->m_hTimer, completionEvent))
		{
			auto result = GetLastError();
			if (result != ERROR_IO_PENDING)
				dcassert(0);
		}
		pGDIImage->m_hTimer = NULL;
	}
	LeaveCriticalSection(&pGDIImage->m_csCallback);
}

void CALLBACK CGDIImage::OnTimer(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	CGDIImage *pGDIImage = (CGDIImage *)lpParameter;
	if (pGDIImage)
	{
		if (isShutdown())
		{
			destroyTimer(pGDIImage, NULL);
			pGDIImage->Release();
			return;
		}
#ifdef DEBUG_GDI_IMAGE
		if (checkImage(pGDIImage))
		{
#endif
			if (pGDIImage->SelectActiveFrame(pGDIImage->m_dwCurrentFrame)) //Change Active frame
			{
				DWORD dwDelay = pGDIImage->GetFrameDelay(pGDIImage->m_dwCurrentFrame);
				if (dwDelay == 0)
					dwDelay++;
				destroyTimer(pGDIImage, NULL);
				if (pGDIImage->m_hCallbackWnd)
				{
					// We should call DrawFrame in context of window thread
					pGDIImage->AddRef();
					SendMessage(pGDIImage->m_hCallbackWnd, pGDIImage->m_dwCallbackMsg, 0, reinterpret_cast<LPARAM>(pGDIImage));
				}
				else
				{
					pGDIImage->DrawFrame();
				}
#ifdef DEBUG_GDI_IMAGE
				if (checkImage(pGDIImage))
				{
#endif
					// Move to the next frame
					if (pGDIImage->m_dwFramesCount)
					{
						pGDIImage->m_dwCurrentFrame++;
						pGDIImage->m_dwCurrentFrame %= pGDIImage->m_dwFramesCount;
					}
					EnterCriticalSection(&pGDIImage->m_csCallback);
					if (!pGDIImage->m_Callbacks.empty() && !isShutdown() && !pGDIImage->m_hTimer)
					{
						pGDIImage->AddRef();
						if (!CreateTimerQueueTimer(&pGDIImage->m_hTimer, NULL, OnTimer, pGDIImage, dwDelay, 0, WT_EXECUTEDEFAULT))
							pGDIImage->Release();
					}
					LeaveCriticalSection(&pGDIImage->m_csCallback);
#ifdef DEBUG_GDI_IMAGE
				}
#endif
			}
#ifdef DEBUG_GDI_IMAGE
		}
#endif
		pGDIImage->Release();
	}
}

void CGDIImage::RegisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam)
{
	dcassert(!isShutdown());
	
	if (GetFrameCount() > 1)
	{
		EnterCriticalSection(&m_csCallback);
		m_Callbacks.insert(CALLBACK_STRUCT(pOnFrameChangedProc, lParam));
		if (!m_hTimer)
		{
			AddRef();
			if (!CreateTimerQueueTimer(&m_hTimer, NULL, OnTimer, this, 0, 0, WT_EXECUTEDEFAULT))
				Release();
		}
#ifdef DEBUG_GDI_IMAGE
		updateStats(m_Callbacks.size());
#endif
		LeaveCriticalSection(&m_csCallback);
	}
}

void CGDIImage::UnregisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam)
{
	if (isShutdown())
	{
		EnterCriticalSection(&m_csCallback);
		m_Callbacks.clear();
		LeaveCriticalSection(&m_csCallback);
	}
	else
	{
		if (GetFrameCount() > 1)
		{
			EnterCriticalSection(&m_csCallback);
			tCALLBACK::iterator i = m_Callbacks.find(CALLBACK_STRUCT(pOnFrameChangedProc, lParam));
			if (i != m_Callbacks.end())
			{
				m_Callbacks.erase(i);
#ifdef DEBUG_GDI_IMAGE
				updateStats(m_Callbacks.size());
#endif
			}
			LeaveCriticalSection(&m_csCallback);
		}
	}
}

HDC CGDIImage::CreateBackDC(HDC displayDC, COLORREF clrBack, int width, int height)
{
	HDC hDC = nullptr;
	if (m_pImage && displayDC)
	{
		hDC = CreateCompatibleDC(displayDC);
		if (hDC)
		{
			::SaveDC(hDC);
			HBITMAP hBitmap = CreateCompatibleBitmap(displayDC, width, height);
			if (hBitmap)
			{
				SelectObject(hDC, hBitmap);
				HBRUSH hBrush = CreateSolidBrush(clrBack);
				if (hBrush) SelectObject(hDC, hBrush);
			}
		}
	}
	return hDC;
}

void CGDIImage::DeleteBackDC(HDC hBackDC)
{
	HBITMAP hBmp = (HBITMAP)GetCurrentObject(hBackDC, OBJ_BITMAP);
	HBRUSH hBrush = (HBRUSH)GetCurrentObject(hBackDC, OBJ_BRUSH);

	RestoreDC(hBackDC, -1);

	DeleteDC(hBackDC);
	DeleteObject(hBmp);
	DeleteObject(hBrush);
}

CGDIImage *CGDIImage::createInstance(const WCHAR* fileName)
{
	CGDIImage* image = new CGDIImage(fileName);
#ifdef DEBUG_GDI_IMAGE
	LOCK(csImageSet);
	imageSet.insert(image);
#endif
	return image;
}

LONG CGDIImage::Release()
{
	const LONG lRef = InterlockedDecrement(&m_lRef);
	dcassert(lRef >= 0);
	if (lRef == 0)
	{
#ifdef DEBUG_GDI_IMAGE
		dcdebug("CGDIImage: deleting %p\n", this);
		removeImage(this);
#endif
		delete this;
	}
	return lRef;
}

void CGDIImage::setCallback(HWND hwnd, UINT message)
{
	m_hCallbackWnd = hwnd;
	m_dwCallbackMsg = message;
}

void CGDIImage::freePropItem()
{
	delete[] (char*) m_pItem;
	m_pItem = nullptr;
}

#endif // IRAINMAN_INCLUDE_GDI_OLE
