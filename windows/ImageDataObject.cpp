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

#include "stdafx.h"
#include "ImageDataObject.h"
#include "../client/StrUtil.h"
#include "../client/LogManager.h"

#ifndef REO_OWNERDRAWSELECT
#define REO_OWNERDRAWSELECT	0x00000040L
#endif

bool CImageDataObject::InsertObject(HWND hWnd, IRichEditOle* pRichEditOle, IOleClientSite* pOleClientSite, IStorage* pStorage, IOleObject* pOleObject)
{
	if (!pOleObject)
		return false;

	HRESULT sc = OleSetContainedObject(pOleObject, TRUE);
	// all items are "contained" -- this makes our reference to this object
	// weak -- which is needed for links to embedding silent update.
	if (sc != S_OK)
	{
		string error = "CImageDataObject::InsertBitmap, OLE OleSetContainedObject error = " + Util::toHexString(sc);
		LogManager::message(error, false);
		return false;
	}

	// Now Add the object to the RichEdit
	REOBJECT reobject = { 0 };
	reobject.cbStruct = sizeof(REOBJECT);

	CLSID clsid;
	sc = pOleObject->GetUserClassID(&clsid);

	if (sc != S_OK)
	{
		string error = "CImageDataObject::InsertBitmap, OLE GetUserClassID error = " + Util::toHexString(sc);
		LogManager::message(error, false);
		dcassert(0);
		return false;
	}

	reobject.clsid = clsid;
	reobject.cp = REO_CP_SELECTION;
	reobject.dvaspect = DVASPECT_CONTENT;
	reobject.dwFlags = REO_BELOWBASELINE | REO_OWNERDRAWSELECT;
	reobject.poleobj = pOleObject;
	reobject.polesite = pOleClientSite;
	reobject.pstg = pStorage;

	// Insert the bitmap at the current location in the richedit control
	sc = pRichEditOle->InsertObject(&reobject);
	if (sc != S_OK)
	{
		string error = "CImageDataObject::InsertBitmap, OLE InsertObject error = " + Util::toHexString(sc);
		LogManager::message(error, false);
		dcassert(0);
		dcassert(::IsWindow(hWnd));
		::SendMessage(hWnd, EM_SCROLLCARET, 0, 0);
		return false;
	}
	dcassert(::IsWindow(hWnd));
	::SendMessage(hWnd, EM_SCROLLCARET, 0, 0);
	return true;
}

void CImageDataObject::SetBitmap(HBITMAP hBitmap)
{
	dcassert(hBitmap);
	
	STGMEDIUM stgm = {0};
	stgm.tymed = TYMED_GDI;
	stgm.hBitmap = hBitmap;
	stgm.pUnkForRelease = NULL;
	
	FORMATETC fm = {0};
	fm.cfFormat = CF_BITMAP;
	fm.ptd = NULL;
	fm.dwAspect = DVASPECT_CONTENT;
	fm.lindex = -1;
	fm.tymed = TYMED_GDI;
	
	SetData(&fm, &stgm, TRUE);
}

IOleObject *CImageDataObject::GetOleObject(IOleClientSite *pOleClientSite, IStorage *pStorage)
{
	dcassert(m_stgmed.hBitmap);
	
	IOleObject *pOleObject = nullptr;
	
	const SCODE sc = ::OleCreateStaticFromData(this, IID_IOleObject, OLERENDER_FORMAT,
	                                           &m_format, pOleClientSite, pStorage, (void **) & pOleObject);
	                                           
	if (sc != S_OK)
	{
		LogManager::message("CImageDataObject::GetOleObject, OleCreateStaticFromData error = " + Util::toString(sc) + " GetLastError() = " + Util::toString(GetLastError()), false);
		dcassert(0);
		return nullptr;
	}
	return pOleObject;
}

/*
void CRichEditCtrlEx::OnInsertObject()
{
// CHECKME: 29.11.2004
IRichEditOle *pRichEdit = NULL;
pRichEdit = GetIRichEditOle();

if (pRichEdit)
{
IOleClientSite *pOleClientSite = NULL;
pRichEdit->GetClientSite(&pOleClientSite);

if (pOleClientSite)
{
HRESULT hr = 0;

COleInsertDialog dlg;
if (dlg.DoModal() == IDOK)
{
LPLOCKBYTES lpLockBytes = NULL;
hr = CreateILockBytesOnHGlobal(NULL, TRUE, &lpLockBytes);
ASSERT(lpLockBytes != NULL);

IStorage *pStorage = NULL;
hr = StgCreateDocfileOnILockBytes(lpLockBytes, STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &pStorage);

if (pStorage)
{
IOleObject *pOleObject;
switch(dlg.GetSelectionType())
{
case COleInsertDialog::createNewItem:
OleCreate(dlg.GetClassID(), IID_IOleObject, OLERENDER_DRAW, NULL, pOleClientSite, pStorage, (void **)&pOleObject);
break;

case COleInsertDialog::insertFromFile:
{
CA2W oleStr(dlg.GetPathName());
OleCreateFromFile(dlg.GetClassID(), oleStr, IID_IOleObject, OLERENDER_DRAW, NULL, pOleClientSite, pStorage, (void **)&pOleObject);
}
break;

case COleInsertDialog::linkToFile:
{
CA2W oleStr(dlg.GetPathName());
OleCreateLinkToFile(oleStr, IID_IOleObject, OLERENDER_DRAW, NULL, pOleClientSite, pStorage, (void **)&pOleObject);
}
break;
}

if (pOleObject != NULL)
{
hr = OleSetContainedObject(pOleObject, TRUE);
ASSERT(hr == S_OK);

CLSID clsid;
pOleObject->GetUserClassID(&clsid);

REOBJECT reobject;
ZeroMemory(&reobject, sizeof(REOBJECT));
reobject.cbStruct = sizeof(REOBJECT);
reobject.cp = REO_CP_SELECTION;
reobject.clsid = clsid;
reobject.poleobj = pOleObject;
reobject.pstg = pStorage;
reobject.polesite = pOleClientSite;
reobject.dvaspect = DVASPECT_CONTENT;
reobject.dwFlags = REO_RESIZABLE;

hr = pRichEdit->InsertObject(&reobject);
ASSERT(hr == S_OK);

pOleObject->Release();
}

pStorage->Release();
}

lpLockBytes->Release(); // ???
}

pOleClientSite->Release();
}

pRichEdit->Release();
}
}
*/