#ifndef BACKING_STORE_H_
#define BACKING_STORE_H_

#include "../client/w.h"

class BackingStore
{
	public:
		static void globalInit();
		static BackingStore* getBackingStore();
		void release();
		HDC getCompatibleDC(HDC hdc, int width, int height);

	private:
		int refs;
		HDC hMemDC;
		HBITMAP hMemBitmap;
		HGDIOBJ hOldBitmap;
		int width;
		int height;
		int planes;
		int bitsPerPlane;

		BackingStore();
		~BackingStore() { destroy(); }
		void destroy();
};

#endif // BACKING_STORE_H_
