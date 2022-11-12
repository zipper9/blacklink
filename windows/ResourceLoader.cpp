#include "stdafx.h"

#include "ResourceLoader.h"
#include "resource.h"

void CImageEx::Destroy() noexcept
{
	CImage::Destroy();
	if (buffer)
	{
		::GlobalUnlock(buffer);
		::GlobalFree(buffer);
		buffer = nullptr;
	}
}

CImageEx& CImageEx::operator= (CImageEx&& src)
{
	CImage::Destroy();
	Attach(src.Detach());
	if (buffer)
	{
		::GlobalUnlock(buffer);
		::GlobalFree(buffer);
		buffer = nullptr;
	}
	buffer = src.buffer;
	src.buffer = nullptr;
	return *this;
}

bool CImageEx::LoadFromResourcePNG(UINT id) noexcept
{
	return LoadFromResource(id, _T("PNG"));
}

bool CImageEx::LoadFromResource(UINT id, LPCTSTR pType, HMODULE hInst) noexcept
{
	HRESULT res = E_FAIL;
	dcassert(buffer == nullptr);
	HRSRC hResource = ::FindResource(hInst, MAKEINTRESOURCE(id), pType);
	//dcassert(hResource);
	if (!hResource)
	{
#if defined(USE_THEME_MANAGER)
		hInst = nullptr; // Try to load the original resource
		hResource = ::FindResource(hInst, MAKEINTRESOURCE(id), pType);
		dcassert(hResource);
		if (!hResource)
#endif
			return false;
	}
	
	DWORD imageSize = ::SizeofResource(hInst, hResource);
	dcassert(imageSize);
	if (!imageSize)
		return false;
		
	const HGLOBAL hGlobal = ::LoadResource(hInst, hResource);
	dcassert(hGlobal);
	if (!hGlobal)
		return false;
		
	const void* pResourceData = ::LockResource(hGlobal);
	dcassert(pResourceData);
	if (!pResourceData)
		return false;
		
	buffer  = ::GlobalAlloc(GMEM_MOVEABLE, static_cast<size_t>(imageSize));
	dcassert(buffer);
	if (buffer)
	{
		void* pBuffer = ::GlobalLock(buffer);
		dcassert(pBuffer);
		if (pBuffer)
		{
			CopyMemory(pBuffer, pResourceData, static_cast<size_t>(imageSize));
			::GlobalUnlock(buffer);
			// http://msdn.microsoft.com/en-us/library/windows/desktop/aa378980%28v=vs.85%29.aspx
			IStream* pStream = nullptr;
			HRESULT hr = ::CreateStreamOnHGlobal(buffer, FALSE, &pStream);
			dcassert(hr == S_OK);
			if (hr == S_OK)
			{
				res = Load(pStream);
				pStream->Release();
			}
		}
		::GlobalFree(buffer);
		buffer = nullptr;
	}
	return res == S_OK;
}

int ResourceLoader::LoadImageList(LPCTSTR fileName, CImageList& imgList, int cx, int cy)
{
	if (cx <= 0 || cy <= 0)
		return 0;
	CImageEx img;
	img.Load(fileName);
	imgList.Create(cx, cy, ILC_COLOR32 | ILC_MASK, img.GetWidth() / cy, 0);
	imgList.Add(img, img.GetPixel(0, 0));
	img.Destroy();
	return imgList.GetImageCount();
}

int ResourceLoader::LoadImageList(UINT id, CImageList& imgList, int cx, int cy)
{
	int imageCount = 0;
	if (cx <= 0 || cy <= 0)
		return imageCount;
	CImageEx img;
	bool imgAdded = false;
	if (img.LoadFromResource(id, _T("PNG")))
	{
		imageCount = img.GetWidth() / cx;
		imgList.Create(cx, cy, ILC_COLOR32 | ILC_MASK, imageCount, 1);
#if defined(USE_THEME_MANAGER)
		if (ThemeManager::isResourceLibLoaded() && imageCount > 0)
		{
			// Only for Not original images -- load
			CImageEx imgOrig;
			if (imgOrig.LoadFromResource(id, _T("PNG"), nullptr))
			{
				const int imageOriginalCount = imgOrig.GetWidth() / cx;
				if (imageOriginalCount > imageCount)
				{
					dcdebug("ImageList %u: %d images found, %d required\n", id, imageCount, imageOriginalCount);
					if (imgOrig.IsDIBSection() && img.IsDIBSection())
					{
						BYTE* dest = static_cast<BYTE*>(imgOrig.GetBits());
						const BYTE* src = static_cast<BYTE*>(img.GetBits());
						int destPitch = imgOrig.GetPitch();
						int srcPitch = img.GetPitch();
						int copySize = abs(srcPitch);
						for (int i = 0; i < cy; i++)
						{
							memcpy(dest, src, copySize);
							dest += destPitch;
							src += srcPitch;
						}
						imgList.Add(imgOrig, nullptr);
						imgAdded = true;
					}
					else
						dcdebug("ImageList %u: invalid format\n", id);
				}
			}
		}
#endif
		if (!imgAdded) imgList.Add(img);
		img.Destroy();
	}
	return imgList.GetImageCount();
}
