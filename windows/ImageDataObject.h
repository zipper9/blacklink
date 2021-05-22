// ImageDataObject.h: Impementation for IDataObject Interface to be used
//					     in inserting bitmap to the RichEdit Control.
//
// Author : Hani Atassi  (atassi@arabteam2000.com)
//
// How to use : Just call the static member InsertBitmap with
//				the appropriate parrameters.
//
// Known bugs :
//
//
//////////////////////////////////////////////////////////////////////
#ifndef DCPLUSPLUS_IMAGEDATAOBJECT_H
#define DCPLUSPLUS_IMAGEDATAOBJECT_H

#include "../client/w.h"

class CImageDataObject : IDataObject
{
	public:
		static bool InsertObject(HWND hWnd, IRichEditOle* pRichEditOle, IOleClientSite* pOleClientSite, IStorage* pStorage, IOleObject* pOleObject);
		
	private:
		ULONG   m_ulRefCnt;
		BOOL    m_bRelease;
		
		// The data being bassed to the richedit
		STGMEDIUM m_stgmed;
		FORMATETC m_format;
		
	public:
		CImageDataObject() : m_ulRefCnt(0), m_bRelease(FALSE)
		{
			memset(&m_stgmed, 0, sizeof(m_stgmed));
			memset(&m_format, 0, sizeof(m_format));
		}
		~CImageDataObject()
		{
			if (m_bRelease)
				::ReleaseStgMedium(&m_stgmed);
		}
		
		// Methods of the IUnknown interface
		
		STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject)
		{
			if (iid == IID_IUnknown || iid == IID_IDataObject)
			{
				*ppvObject = this;
				AddRef();
				return S_OK;
			}
			else
				return E_NOINTERFACE;
		}
		STDMETHOD_(ULONG, AddRef)(void)
		{
			m_ulRefCnt++;
			return m_ulRefCnt;
		}
		STDMETHOD_(ULONG, Release)(void)
		{
			if (--m_ulRefCnt == 0)
			{
				delete this;
				return 0;
			}
			
			return m_ulRefCnt;
		}
		
		// Methods of the IDataObject Interface
		
		STDMETHOD(GetData)(FORMATETC* /*pformatetcIn*/, STGMEDIUM *pmedium)
		{
		
			pmedium->tymed = TYMED_GDI;
			pmedium->hBitmap = m_stgmed.hBitmap;
			pmedium->pUnkForRelease = nullptr;
			
			return S_OK;
		}
		STDMETHOD(GetDataHere)(FORMATETC* /*pformatetc*/, STGMEDIUM*  /*pmedium*/)
		{
			return E_NOTIMPL;
		}
		STDMETHOD(QueryGetData)(FORMATETC* /*pformatetc*/)
		{
			return E_NOTIMPL;
		}
		STDMETHOD(GetCanonicalFormatEtc)(FORMATETC* /*pformatectIn*/, FORMATETC* /*pformatetcOut*/)
		{
			return E_NOTIMPL;
		}
		STDMETHOD(SetData)(FORMATETC* pformatetc, STGMEDIUM*  pmedium, BOOL  fRelease)
		{
			m_format = *pformatetc;
			m_stgmed = *pmedium;
			m_bRelease = fRelease;
			return S_OK;
		}
		STDMETHOD(EnumFormatEtc)(DWORD /*dwDirection*/, IEnumFORMATETC** /*ppenumFormatEtc*/)
		{
			return E_NOTIMPL;
		}
		STDMETHOD(DAdvise)(FORMATETC* /*pformatetc*/, DWORD /*advf*/, IAdviseSink* /*pAdvSink*/,
		                   DWORD* /*pdwConnection*/)
		{
			return E_NOTIMPL;
		}
		STDMETHOD(DUnadvise)(DWORD /*dwConnection*/)
		{
			return E_NOTIMPL;
		}
		STDMETHOD(EnumDAdvise)(IEnumSTATDATA ** /*ppenumAdvise*/)
		{
			return E_NOTIMPL;
		}
		
		// Some Other helper functions
		
		void SetBitmap(HBITMAP hBitmap);
		IOleObject *GetOleObject(IOleClientSite *pOleClientSite, IStorage *pStorage);
		
};

#endif // !defined(DCPLUSPLUS_IMAGEDATAOBJECT_H)
