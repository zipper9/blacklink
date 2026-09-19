#include "stdafx.h"

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

void CGDIImage::stopTimers()
{
	std::vector<CGDIImage*> imageVec;
	{
		LOCK(csImageSet);
		for (CGDIImage* image : imageSet)
		{
			imageVec.push_back(image);
			image->addRef();
		}
	}
	for (CGDIImage* image : imageVec)
	{
		destroyTimer(image, INVALID_HANDLE_VALUE);
		image->release();
	}
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
		res += _T(" refs=") + Util::toStringT(image->ref);
		res += _T(" timer=") + Util::toHexStringT(image->hTimer);
		EnterCriticalSection(&image->csCallback);
		size_t callbacks = image->callbacks.size();
		LeaveCriticalSection(&image->csCallback);
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
	numFrames(0), image(nullptr), propItem(nullptr), hTimer(nullptr), ref(1),
	hWndCallback(nullptr), callbackMsg(0),
	width(0), height(0), currentFrame(0)
{
	dcassert(!isShutdown());
	InitializeCriticalSection(&csCallback);
	image = new Gdiplus::Image(fileName);
	if (image->GetLastStatus() != Gdiplus::Ok)
	{
		delete image;
		image = nullptr;
		return;
	}
#ifdef _DEBUG
	loadedFileName = fileName;
#endif
	width = image->GetWidth();
	height = image->GetHeight();

	if (image->GetType() == Gdiplus::ImageTypeBitmap)
	{
		Gdiplus::Bitmap* bitmap = static_cast<Gdiplus::Bitmap*>(image);
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
	UINT propBufSize = image->GetPropertyItemSize(PropertyTagFrameDelay);
	if (propBufSize)
	{
		propItem = (Gdiplus::PropertyItem*) new char[propBufSize];
		memset(propItem, 0, propBufSize);
		currentFrame = 0;
		width  = 0;
		height = 0;
		if (!image->GetPropertyItem(PropertyTagFrameDelay, propBufSize, propItem))
		{
			if (selectActiveFrame(currentFrame))
				if (const int frameCount = getFrameCount())
				{
					// [!] brain-ripper
					// GDI+ seemed to have bug,
					// and for some GIF's returned zero delay for all frames.
					// Make such a workaround: take 50ms delay on every frame
					bool bHaveDelay = false;
					for (int i = 0; i < frameCount; i++)
					{
						if (getFrameDelay(i) > 0)
						{
							bHaveDelay = true;
							break;
						}
					}
					if (!bHaveDelay)
					{
						for (int i = 0; i < frameCount; i++)
							((UINT*) propItem[0].value)[i] = 5;
					}
				}
			// [!] brain-ripper
			// Strange bug - image->GetWidth() and image->GetHeight()
			// sometimes returns zero, even object successfuly loaded.
			// Cache this values here to return correct values later
			width = image->GetWidth();
			height = image->GetHeight();
		}
		else
			freePropItem();
	}
}

CGDIImage::~CGDIImage()
{
	_ASSERTE(callbacks.empty());
	if (hTimer)
	{
		destroyTimer(this, INVALID_HANDLE_VALUE);
		// INVALID_HANDLE_VALUE:  wait for any running timer callback functions to complete
	}
	delete image;
	freePropItem();
	DeleteCriticalSection(&csCallback);
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

void CGDIImage::draw(HDC hDC, int xDst, int yDst, int wSrc, int hSrc, int xSrc, int ySrc)
{
	Gdiplus::Graphics Graph(hDC);
	Graph.DrawImage(image, xDst, yDst, xSrc, ySrc, wSrc, hSrc, Gdiplus::UnitPixel);
}

unsigned CGDIImage::getFrameDelay(int frameNum) const
{
	if (propItem)
		return ((UINT*) propItem[0].value)[frameNum] * 10;
	else
		return 5;
}

bool CGDIImage::selectActiveFrame(int frameNum)
{
	dcassert(!isShutdown());
	if (!isShutdown())
	{
		static const GUID g_Guid = Gdiplus::FrameDimensionTime;
		if (image)
			image->SelectActiveFrame(&g_Guid, frameNum);
		return true;
	}
	return false;
}

int CGDIImage::getFrameCount()
{
	dcassert(!isShutdown());
	if (numFrames == 0)
	{
		//First of all we should get the number of frame dimensions
		//Images considered by GDI+ as:
		//frames[animation_frame_index][how_many_animation];
		if (const UINT count = image->GetFrameDimensionsCount())
		{
			//Now we should get the identifiers for the frame dimensions
			std::vector<GUID> l_pDimensionIDs(count); //-V121
			image->GetFrameDimensionsList(&l_pDimensionIDs[0], count);
			//For gif image , we only care about animation set#0
			numFrames = image->GetFrameCount(&l_pDimensionIDs[0]);
		}
	}
	return numFrames;
}

void CGDIImage::drawFrame()
{
	dcassert(!isShutdown());

	EnterCriticalSection(&csCallback);
	for (auto i = callbacks.cbegin(); i != callbacks.cend(); ++i)
	{
		if (!i->pOnFrameChangedProc(this, i->lParam))
		{
			i = callbacks.erase(i);
			if (i == callbacks.end())
				break;
		}
	}
	LeaveCriticalSection(&csCallback);
}

void CGDIImage::destroyTimer(CGDIImage *pGDIImage, HANDLE completionEvent)
{
	EnterCriticalSection(&pGDIImage->csCallback);
	if (pGDIImage->hTimer)
	{
		if (!DeleteTimerQueueTimer(NULL, pGDIImage->hTimer, completionEvent))
		{
			auto result = GetLastError();
			if (result != ERROR_IO_PENDING)
				dcassert(0);
		}
		pGDIImage->hTimer = NULL;
	}
	LeaveCriticalSection(&pGDIImage->csCallback);
}

void CALLBACK CGDIImage::OnTimer(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	CGDIImage *pGDIImage = (CGDIImage *)lpParameter;
	if (pGDIImage)
	{
		if (isShutdown())
		{
			destroyTimer(pGDIImage, NULL);
			pGDIImage->release();
			return;
		}
#ifdef DEBUG_GDI_IMAGE
		if (checkImage(pGDIImage))
		{
#endif
			if (pGDIImage->selectActiveFrame(pGDIImage->currentFrame)) //Change Active frame
			{
				unsigned delay = pGDIImage->getFrameDelay(pGDIImage->currentFrame);
				if (delay == 0)
					delay++;
				destroyTimer(pGDIImage, NULL);
				if (pGDIImage->hWndCallback)
				{
					// We should call DrawFrame in context of window thread
					pGDIImage->addRef();
					SendMessage(pGDIImage->hWndCallback, pGDIImage->callbackMsg, 0, reinterpret_cast<LPARAM>(pGDIImage));
				}
				else
					pGDIImage->drawFrame();
#ifdef DEBUG_GDI_IMAGE
				if (checkImage(pGDIImage))
				{
#endif
					// Move to the next frame
					if (pGDIImage->numFrames)
						pGDIImage->currentFrame = (pGDIImage->currentFrame + 1) % pGDIImage->numFrames;
					EnterCriticalSection(&pGDIImage->csCallback);
					if (!pGDIImage->callbacks.empty() && !isShutdown() && !pGDIImage->hTimer)
					{
						pGDIImage->addRef();
						if (!CreateTimerQueueTimer(&pGDIImage->hTimer, NULL, OnTimer, pGDIImage, delay, 0, WT_EXECUTEDEFAULT))
							pGDIImage->release();
					}
					LeaveCriticalSection(&pGDIImage->csCallback);
#ifdef DEBUG_GDI_IMAGE
				}
#endif
			}
#ifdef DEBUG_GDI_IMAGE
		}
#endif
		pGDIImage->release();
	}
}

void CGDIImage::registerCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam)
{
	dcassert(!isShutdown());

	if (getFrameCount() > 1)
	{
		EnterCriticalSection(&csCallback);
		callbacks.insert(CALLBACK_STRUCT(pOnFrameChangedProc, lParam));
		if (!hTimer)
		{
			addRef();
			if (!CreateTimerQueueTimer(&hTimer, NULL, OnTimer, this, 0, 0, WT_EXECUTEDEFAULT))
				release();
		}
#ifdef DEBUG_GDI_IMAGE
		updateStats(callbacks.size());
#endif
		LeaveCriticalSection(&csCallback);
	}
}

void CGDIImage::unregisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam)
{
	if (isShutdown())
	{
		EnterCriticalSection(&csCallback);
		callbacks.clear();
		LeaveCriticalSection(&csCallback);
	}
	else
	{
		if (getFrameCount() > 1)
		{
			EnterCriticalSection(&csCallback);
			CallbackSet::iterator i = callbacks.find(CALLBACK_STRUCT(pOnFrameChangedProc, lParam));
			if (i != callbacks.end())
			{
				callbacks.erase(i);
#ifdef DEBUG_GDI_IMAGE
				updateStats(callbacks.size());
#endif
			}
			LeaveCriticalSection(&csCallback);
		}
	}
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

LONG CGDIImage::release()
{
	const LONG lRef = InterlockedDecrement(&ref);
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
	hWndCallback = hwnd;
	callbackMsg = message;
}

void CGDIImage::freePropItem()
{
	delete[] (char*) propItem;
	propItem = nullptr;
}
