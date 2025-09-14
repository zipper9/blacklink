#pragma once

#ifdef IRAINMAN_INCLUDE_GDI_OLE

#include <gdiplus.h>

#include "../client/compiler.h"
#include "../client/typedefs.h"
#include "../client/GlobalState.h"
#include <set>

class CGDIImage
{
		friend class CGDIImageOle;
		typedef bool (__cdecl *ONFRAMECHANGED)(CGDIImage *pImage, LPARAM lParam);
		
		Gdiplus::Image *m_pImage;
		Gdiplus::PropertyItem* m_pItem;
		DWORD m_dwWidth;
		DWORD m_dwHeight;
		DWORD m_dwFramesCount;
		DWORD m_dwCurrentFrame;
		HANDLE m_hTimer;
		volatile LONG m_lRef;
		
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
		
		mutable CRITICAL_SECTION m_csCallback;
		typedef std::set<CALLBACK_STRUCT> tCALLBACK;
		tCALLBACK m_Callbacks;
		HWND m_hCallbackWnd;
		UINT m_dwCallbackMsg;

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
		bool isInitialized() const { return m_pImage != nullptr; }
		
		void Draw(HDC hDC, int xDst, int yDst, int wSrc, int hSrc, int xSrc, int ySrc, HDC hBackDC, int xBk, int yBk, int wBk, int hBk);
		void DrawFrame();

		DWORD GetFrameDelay(DWORD dwFrame);
		bool SelectActiveFrame(DWORD dwFrame);
		DWORD GetFrameCount();
		DWORD GetWidth() const { return m_dwWidth; }
		DWORD GetHeight() const { return m_dwHeight; }
		void RegisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam);
		void UnregisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam);
		
		HDC CreateBackDC(HDC displayDC, COLORREF clrBack, int width, int height);
		void DeleteBackDC(HDC hBackDC);
		
		LONG AddRef() { return InterlockedIncrement(&m_lRef); }
		LONG Release();

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

#endif // IRAINMAN_INCLUDE_GDI_OLE
