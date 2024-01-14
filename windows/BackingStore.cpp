#include "stdafx.h"
#include "BackingStore.h"

static bool tlsInitialized;
static DWORD tlsIndex = TLS_OUT_OF_INDEXES;

void BackingStore::globalInit()
{
	if (tlsInitialized) return;
	tlsIndex = TlsAlloc();
	tlsInitialized = true;
}

BackingStore::BackingStore()
{
	refs = 1;
	hMemDC = nullptr;
	hMemBitmap = nullptr;
	hOldBitmap = nullptr;
	width = height = 0;
	planes = bitsPerPlane = 0;
}

void BackingStore::destroy()
{
	if (hMemDC)
	{
		if (hOldBitmap)
		{
			SelectObject(hMemDC, hOldBitmap);
			hOldBitmap = nullptr;
		}
		DeleteDC(hMemDC);
		hMemDC = nullptr;
	}
	if (hMemBitmap)
	{
		DeleteObject(hMemBitmap);
		hMemBitmap = nullptr;
	}
	width = height = 0;
	planes = bitsPerPlane = 0;
}

HDC BackingStore::getCompatibleDC(HDC hdc, int w, int h)
{
	int newBits = GetDeviceCaps(hdc, BITSPIXEL);
	int newPlanes = GetDeviceCaps(hdc, PLANES);
	if (newBits == bitsPerPlane && newPlanes == planes && w <= width && h <= height) return hMemDC;
	destroy();
	hMemDC = CreateCompatibleDC(hdc);
	if (!hMemDC) return nullptr;
	hMemBitmap = CreateCompatibleBitmap(hdc, w, h);
	if (!hMemBitmap)
	{
		destroy();
		return nullptr;
	}
	hOldBitmap = SelectObject(hMemDC, hMemBitmap);
	width = w;
	height = h;
	planes = newPlanes;
	bitsPerPlane = newBits;
	return hMemDC;
}

BackingStore* BackingStore::getBackingStore()
{
	BackingStore* instance = (BackingStore*) TlsGetValue(tlsIndex);
	if (instance)
	{
		++instance->refs;
		return instance;
	}
	instance = new BackingStore();
	TlsSetValue(tlsIndex, instance);
	return instance;
}

void BackingStore::release()
{
	dcassert(refs > 0);
	if (--refs == 0)
	{
		delete this;
		if (tlsIndex != TLS_OUT_OF_INDEXES) TlsSetValue(tlsIndex, nullptr);
	}
}
