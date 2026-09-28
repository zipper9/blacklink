#include "stdafx.h"
#include "GDIImageOle.h"
#include "../client/debug.h"
#include <algorithm>

// CGDIImageOle

const IID IID_IGDIImageDeleteNotify = {0xa991582, 0x6f1a, 0x4150, 0xbd, 0xcd, 0x25, 0x3b, 0xb7, 0x8c, 0xf9, 0xf};

static HWND activeMDIWindow = nullptr;

void setActiveMDIWindow(HWND hwnd)
{
	activeMDIWindow = hwnd;
}

CGDIImageOle::CGDIImageOle():
	m_pImage(nullptr),
	m_flags(0),
	m_id(0),
	m_hCallbackWnd(nullptr),
	m_dwUpdateMsg(WM_USER),
	m_bIsDeleting(false),
	m_bRegistered(false),
	m_clrBack(0)
{
}

HRESULT CGDIImageOle::FinalConstruct()
{
	//InitializeCriticalSection(&m_csAdviseSink);
	return S_OK;
}

void CGDIImageOle::FinalRelease()
{
	if (m_pImage)
	{
		if (m_bRegistered)
		{
			m_pImage->unregisterCallback(OnFrameChanged, (LPARAM)this);
			m_bRegistered = false;
		}
		m_pImage->release();
		m_pImage = nullptr;
	}
	//DeleteCriticalSection(&m_csAdviseSink);
}

enum DefinedTestRectValue {TR_NOOVERLAP, TR_INTERSECT, TR_CONTAIN};

static DefinedTestRectValue TestRectInRect(RECT *pRect1, RECT *pRect2)
{
	RECT rcIntersect;
	DefinedTestRectValue ret = TR_NOOVERLAP;

	if (IntersectRect(&rcIntersect, pRect1, pRect2))
	{
		if (EqualRect(&rcIntersect, pRect2))
			ret = TR_CONTAIN;
		else
			ret = TR_INTERSECT;
	}

	return ret;
}

HRESULT CGDIImageOle::FireViewChangeEx(BOOL bEraseBackground)
{
	dcassert(!CGDIImage::isShutdown());
	HRESULT Res = S_OK;
	if (m_bInPlaceActive)
	{
		// Active
		if (m_hWndCD != NULL)
			::InvalidateRect(m_hWndCD, NULL, bEraseBackground); // Window based
		else if (m_bWndLess && m_spInPlaceSite != NULL)
			m_spInPlaceSite->InvalidateRect(NULL, bEraseBackground); // Windowless
	}
	else // Inactive
	{
		/*
		if (CComCompositeControl<CGDIImageOle>::m_hWnd)
		    ::InvalidateRect(CComCompositeControl<CGDIImageOle>::m_hWnd, NULL, FALSE);
		else if (m_hCallbackWnd)
		{
		    ::PostMessage(m_hCallbackWnd, m_dwUpdateMsg, 0, (LPARAM)this);
		}
		else
		{
		    SendOnViewChange(DVASPECT_CONTENT);
		}
		*/
		if (m_spClientSite && !m_bIsDeleting)
		{
			/*
			IOleClientSite *pClientSite = m_spClientSite;
			pClientSite->AddRef();
			::PostMessage(m_hCallbackWnd, m_dwUpdateMsg, 0, (LPARAM)pClientSite);
			*/
			CComPtr<IOleInPlaceSite> spInPlaceSite;
			m_spClientSite->QueryInterface(__uuidof(IOleInPlaceSite), (void **)&spInPlaceSite);

			HWND hwndParent = NULL;

			if (spInPlaceSite &&
			    spInPlaceSite->GetWindow(&hwndParent) == S_OK &&
			    hwndParent &&
			    ::GetParent(hwndParent) == activeMDIWindow) // Чуток быстрее и проще добраться чем MDIGetActive()
			{
				OLEINPLACEFRAMEINFO frameInfo;
				IOleInPlaceFrame *pInPlaceFrame = nullptr;
				IOleInPlaceUIWindow *pInPlaceUIWindow = nullptr;
				RECT rcPos, rcClip;
				frameInfo.cb = sizeof(OLEINPLACEFRAMEINFO);

				if (spInPlaceSite->GetWindowContext(&pInPlaceFrame, &pInPlaceUIWindow, &rcPos, &rcClip, &frameInfo) == S_OK)
				{
					if (pInPlaceFrame) pInPlaceFrame->Release();
					if (pInPlaceUIWindow) pInPlaceUIWindow->Release();

					DefinedTestRectValue TestRect = TestRectInRect(&rcClip, &rcPos);

					if (TestRect == TR_CONTAIN)
					{
						// Object fully visible
						SendOnViewChange(DVASPECT_CONTENT);
					}
					else if (TestRect == TR_INTERSECT)
					{
						// Object partially visible

						// [!] brain-ripper
						// this implementation flickers smiles
						::InvalidateRect(hwndParent, &rcPos, FALSE);

						/*
						// [!] brain-ripper
						// this implementation doesn't respect selection color
						HDC hDC = ::GetDC(hwndParent);
						m_pImage->Draw(hDC,
						    rcPos.left,
						    rcPos.top,
						    min(m_dwW, DWORD(rcPos.right - rcPos.left)),
						    min(m_dwH, DWORD(rcPos.bottom - rcPos.top)), 0, 0, m_hBackDC, 0, 0,
						    min(m_dwW, DWORD(rcPos.right - rcPos.left)),
						    min(m_dwH, DWORD(rcPos.bottom - rcPos.top)));
						int l_res = ::ReleaseDC(hwndParent, hDC);
						dcassert(l_res);
						//*/
					}
					else
					{
						// Object hidden
						if (m_bRegistered)
						{
							// Returning S_FALSE on callback makes
							// pImage to unregister callback function.
							// This mechanism used because it is not allowed Register/Unregister
							// callback functions from inside callback functions
							Res = S_FALSE;
							m_bRegistered = false;
						}
					}
				}
			}
		}
	}
	return Res;
}

bool CGDIImageOle::SetImage(CGDIImage *pImage, COLORREF clrBack, HWND hCallbackWnd, DWORD dwUpdateMsg)
{
	dcassert(!CGDIImage::isShutdown());
	if (m_pImage)
		return false;

	m_pImage = pImage;

	if (!m_pImage)
		return false;

	m_pImage->addRef();

	int width = m_pImage->getWidth();
	int height = m_pImage->getHeight();

	SIZEL szPix;
	szPix.cx = width;
	szPix.cy = height;
	SIZEL szHi;
	AtlPixelToHiMetric(&szPix, &szHi);
	m_sizeExtent.cx = szHi.cx;
	m_sizeExtent.cy = szHi.cy;

	m_hCallbackWnd = hCallbackWnd;
	m_dwUpdateMsg = dwUpdateMsg;
	m_clrBack = clrBack;

	m_pImage->registerCallback(OnFrameChanged, (LPARAM)this);
	m_bRegistered = true;

	return true;
}

HRESULT CGDIImageOle::ReplaceImage(void *newImage)
{
	if (!m_pImage)
		return E_FAIL;

	if (m_bRegistered)
	{
		m_pImage->unregisterCallback(OnFrameChanged, (LPARAM)this);
		m_bRegistered = false;
	}
	m_pImage->release();

	m_pImage = static_cast<CGDIImage*>(newImage);
	m_pImage->addRef();
	m_pImage->registerCallback(OnFrameChanged, (LPARAM)this);
	m_bRegistered = true;

	return S_OK;
}

STDMETHODIMP CGDIImageOle::get_Text(BSTR *text)
{
	if (!text) return E_INVALIDARG;
	*text = SysAllocString(m_text.c_str());
	return S_OK;
}

STDMETHODIMP CGDIImageOle::put_Text(BSTR text)
{
	if (!text) return E_INVALIDARG;
	m_text = text;
	return S_OK;
}

STDMETHODIMP CGDIImageOle::get_BackgroundColor(COLORREF *color)
{
	if (!color) return E_INVALIDARG;
	*color = m_clrBack;
	return S_OK;
}

STDMETHODIMP CGDIImageOle::put_BackgroundColor(COLORREF color)
{
	m_clrBack = color;
	return S_OK;
}

STDMETHODIMP CGDIImageOle::get_Flags(int *flags)
{
	if (!flags) return E_INVALIDARG;
	*flags = m_flags;
	return S_OK;
}

STDMETHODIMP CGDIImageOle::put_Flags(int flags)
{
	m_flags = flags;
	return S_OK;
}

STDMETHODIMP CGDIImageOle::get_EmoticonId(int *id)
{
	if (!id) return E_INVALIDARG;
	*id = m_id;
	return S_OK;
}

STDMETHODIMP CGDIImageOle::put_EmoticonId(int id)
{
	m_id = id;
	return S_OK;
}

HRESULT CGDIImageOle::OnDraw(ATL_DRAWINFO& di)
{
	if (!CGDIImage::isShutdown())
	{
		int width = di.prcBounds->right - di.prcBounds->left;
		int height = di.prcBounds->bottom - di.prcBounds->top;
		HBRUSH hBrush = CreateSolidBrush(m_clrBack);
		FillRect(di.hdcDraw, (const RECT *) di.prcBounds, hBrush);
		DeleteObject(hBrush);
		m_pImage->draw(di.hdcDraw, di.prcBounds->left, di.prcBounds->top, width, height, 0, 0);
		if (!m_bRegistered)
		{
			// Object became visible
			m_pImage->registerCallback(OnFrameChanged, (LPARAM)this);
			m_bRegistered = true;
		}
	}
	return S_OK;
}

bool CGDIImageOle::OnFrameChanged(CGDIImage *pImage, LPARAM lParam)
{
	dcassert(!CGDIImage::isShutdown());
	if (CGDIImage::isShutdown())
		return true;
	CGDIImageOle *pGDIImage = (CGDIImageOle *)lParam;
	return pGDIImage->FireViewChangeEx(FALSE) == S_OK;
}

LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	return S_OK;
}
