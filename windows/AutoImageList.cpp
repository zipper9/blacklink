#include "stdafx.h"
#include "AutoImageList.h"

bool AutoImageList::create(int cx, int cy, int flags, int initialSize, int grow)
{
	if (hImageList) return true;
	hImageList = ImageList_Create(cx, cy, flags, initialSize, grow);
	return hImageList != nullptr;
}

void AutoImageList::destroy()
{
	if (hImageList)
	{
		ImageList_Destroy(hImageList);
		hImageList = nullptr;
	}
}

void AutoImageList::clear()
{
	if (hImageList) ImageList_Remove(hImageList, -1);
	m.clear();
}

int AutoImageList::getBitmapIndex(HBITMAP hBitmap) const
{
	auto i = m.find(hBitmap);
	return i == m.end() ? -1 : i->second;
}

int AutoImageList::addBitmap(HBITMAP hBitmap)
{
	if (!hImageList) return -1;
	int index = getBitmapIndex(hBitmap);
	if (index != -1) return index;
	index = ImageList_Add(hImageList, hBitmap, nullptr);
	if (index != -1) m.insert(std::make_pair(hBitmap, index));
	return index;
}
