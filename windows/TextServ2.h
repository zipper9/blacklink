#include "TextServ.h"

// An optional extension to ITextHost which provides functionality
// necessary to allow TextServices to embed OLE objects

class ITextHost2 : public ITextHost
{
public:
	//Is a double click in the message queue?
	virtual BOOL TxIsDoubleClickPending() = 0;

	//Get the overall window for this control
	virtual HRESULT TxGetWindow(HWND *phwnd) = 0;

	//Set control window to foreground
	virtual HRESULT TxSetForegroundWindow() = 0;

	//Set control window to foreground
	virtual HPALETTE TxGetPalette() = 0;

	//Get East Asian flags
	virtual HRESULT TxGetEastAsianFlags(LONG *pFlags) = 0;

	//Routes the cursor change to the winhost
	virtual HCURSOR TxSetCursor2(HCURSOR hcur, BOOL bText) = 0;

	//Notification that text services is freed
	virtual void TxFreeTextServicesNotification() = 0;

	//Get Edit Style flags
	virtual HRESULT TxGetEditStyle(DWORD dwItem, DWORD *pdwData) = 0;

	//Get Window Style bits
	virtual HRESULT TxGetWindowStyles(DWORD *pdwStyle, DWORD *pdwExStyle) = 0;

	//Show / hide drop caret (D2D-only)
	virtual HRESULT TxShowDropCaret(BOOL fShow, HDC hdc, LPCRECT prc) = 0;

	//Destroy caret (D2D-only)
	virtual HRESULT TxDestroyCaret() = 0;

	//Get Horizontal scroll extent
	virtual HRESULT TxGetHorzExtent(LONG *plHorzExtent) = 0;
};
