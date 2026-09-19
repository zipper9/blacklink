#pragma once

#include <gdiplus.h>

#include "../client/compiler.h"
#include "../client/typedefs.h"
#include "../client/GlobalState.h"
#include <set>

class CGDIImage
{
		friend class CGDIImageOle;
		typedef bool (__cdecl *ONFRAMECHANGED)(CGDIImage *pImage, LPARAM lParam);

		Gdiplus::Image* image;
		Gdiplus::PropertyItem* propItem;
		int width;
		int height;
		unsigned numFrames;
		unsigned currentFrame;
		HANDLE hTimer;
		volatile LONG ref;

		static void CALLBACK OnTimer(PVOID lpParameter, BOOLEAN TimerOrWaitFired);
		static void destroyTimer(CGDIImage *pGDIImage, HANDLE p_CompletionEvent);

		struct CALLBACK_STRUCT
		{
			CALLBACK_STRUCT(ONFRAMECHANGED _pOnFrameChangedProc, LPARAM _lParam):
				pOnFrameChangedProc(_pOnFrameChangedProc),
				lParam(_lParam)
			{}

			ONFRAMECHANGED pOnFrameChangedProc;
			LPARAM lParam;

			bool operator<(const CALLBACK_STRUCT &cb) const
			{
				return (pOnFrameChangedProc < cb.pOnFrameChangedProc || (pOnFrameChangedProc == cb.pOnFrameChangedProc && lParam < cb.lParam));
			}
		};

		typedef std::set<CALLBACK_STRUCT> CallbackSet;
		CallbackSet callbacks;
		mutable CRITICAL_SECTION csCallback;
		HWND hWndCallback;
		UINT callbackMsg;

		CGDIImage(const WCHAR* fileName);
		~CGDIImage();

		void freePropItem();
		void applyMask(Gdiplus::Bitmap* bitmap);

	public:
		static bool isShutdown()
		{
			return GlobalState::isShuttingDown();
		}
		static CGDIImage *createInstance(const WCHAR* fileName);
		bool isInitialized() const { return image != nullptr; }

		void draw(HDC hDC, int xDst, int yDst, int wSrc, int hSrc, int xSrc, int ySrc);
		void drawFrame();

		unsigned getFrameDelay(int frameNum) const;
		bool selectActiveFrame(int frameNum);
		int getFrameCount();
		int getWidth() const { return width; }
		int getHeight() const { return height; }
		void registerCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam);
		void unregisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam);

		LONG addRef() { return InterlockedIncrement(&ref); }
		LONG release();

		void setCallback(HWND hwnd, UINT message);

#ifdef _DEBUG
		std::wstring loadedFileName;
#endif
#ifdef DEBUG_GDI_IMAGE
		static bool checkImage(CGDIImage* image);
		static size_t getImageCount();
		static void stopTimers();
#ifdef _DEBUG
		static tstring getLoadedList();
#endif
#endif
};
