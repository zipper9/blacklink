#ifndef AUTO_IMAGE_LIST_H_
#define AUTO_IMAGE_LIST_H_

#include "../client/w.h"
#include <CommCtrl.h>
#include <boost/unordered/unordered_map.hpp>

class AutoImageList
{
	public:
		AutoImageList() : hImageList(nullptr) {}
		~AutoImageList() { destroy(); }

		AutoImageList(const AutoImageList&) = delete;
		AutoImageList& operator= (const AutoImageList&) = delete;

		bool create(int cx, int cy, int flags = ILC_COLOR32, int initialSize = 10, int grow = 10);
		void destroy();
		void clear();
		int getBitmapIndex(HBITMAP hBitmap) const;
		int addBitmap(HBITMAP hBitmap);
		HIMAGELIST getHandle() const { return hImageList; }

	private:
		HIMAGELIST hImageList;
		boost::unordered_map<HBITMAP, int> m;
};

#endif // AUTO_IMAGE_LIST_H_
