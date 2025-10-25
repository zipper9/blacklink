#ifndef TEXT_HOST_IMPL_H_
#define TEXT_HOST_IMPL_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>
#include <textserv.h>

#ifndef TXES_ISDIALOG
#include "TextServ2.h"
#endif

struct ITextDocument;

class TextHostImpl : public ITextHost2
{
public:
	enum
	{
		SB_FLAG_HORZ = 1,
		SB_FLAG_VERT = 2
	};

	TextHostImpl();
	virtual ~TextHostImpl();

	// IUnknown
	virtual HRESULT _stdcall QueryInterface(REFIID riid, void **ppvObject);
	virtual ULONG _stdcall AddRef();
	virtual ULONG _stdcall Release();

	// ITextHost
	virtual HDC TxGetDC();
	virtual int TxReleaseDC(HDC hdc);
	virtual BOOL TxShowScrollBar(int fnBar, BOOL fShow);
	virtual BOOL TxEnableScrollBar(int fuSBFlags, int fuArrowflags);
	virtual BOOL TxSetScrollRange(int fnBar, LONG nMinPos, int nMaxPos, BOOL fRedraw);
	virtual BOOL TxSetScrollPos(int fnBar, int nPos, BOOL fRedraw);
	virtual void TxInvalidateRect(LPCRECT prc, BOOL fMode);
	virtual void TxViewChange(BOOL fUpdate);
	virtual BOOL TxCreateCaret(HBITMAP hbmp, int xWidth, int yHeight);
	virtual BOOL TxShowCaret(BOOL fShow);
	virtual BOOL TxSetCaretPos(int x, int y);
	virtual BOOL TxSetTimer(UINT idTimer, UINT uTimeout);
	virtual void TxKillTimer(UINT idTimer);
	virtual void TxScrollWindowEx(int dx, int dy, LPCRECT lprcScroll, LPCRECT lprcClip, HRGN hrgnUpdate, LPRECT lprcUpdate, UINT fuScroll);
	virtual void TxSetCapture(BOOL fCapture);
	virtual void TxSetFocus();
	virtual void TxSetCursor(HCURSOR hcur, BOOL fText);
	virtual BOOL TxScreenToClient(LPPOINT lppt);
	virtual BOOL TxClientToScreen(LPPOINT lppt);
	virtual HRESULT TxActivate(LONG *plOldState);
	virtual HRESULT TxDeactivate(LONG lNewState);
	virtual HRESULT TxGetClientRect(LPRECT prc);
	virtual HRESULT TxGetViewInset(LPRECT prc);
	virtual HRESULT TxGetCharFormat(const CHARFORMATW **ppCF);
	virtual HRESULT TxGetParaFormat(const PARAFORMAT **ppPF);
	virtual COLORREF TxGetSysColor(int nIndex);
	virtual HRESULT TxGetBackStyle(TXTBACKSTYLE *pstyle);
	virtual HRESULT TxGetMaxLength(DWORD *plength);
	virtual HRESULT TxGetScrollBars(DWORD *pdwScrollBar);
	virtual HRESULT TxGetPasswordChar(TCHAR *pch);
	virtual HRESULT TxGetAcceleratorPos(LONG *pcp);
	virtual HRESULT TxGetExtent(LPSIZEL lpExtent);
	virtual HRESULT OnTxCharFormatChange(const CHARFORMATW *pcf);
	virtual HRESULT OnTxParaFormatChange(const PARAFORMAT *ppf);
	virtual HRESULT TxGetPropertyBits(DWORD dwMask, DWORD *pdwBits);
	virtual HRESULT TxNotify(DWORD iNotify, void *pv);
	virtual HIMC TxImmGetContext();
	virtual void TxImmReleaseContext(HIMC himc);
	virtual HRESULT TxGetSelectionBarWidth(LONG *lSelBarWidth);

	// ITextHost2
	virtual BOOL TxIsDoubleClickPending();
	virtual HRESULT TxGetWindow(HWND *phwnd);
	virtual HRESULT TxSetForegroundWindow();
	virtual HPALETTE TxGetPalette();
	virtual HRESULT TxGetEastAsianFlags(LONG *pFlags);
	virtual HCURSOR TxSetCursor2(HCURSOR hcur, BOOL bText);
	virtual void TxFreeTextServicesNotification();
	virtual HRESULT TxGetEditStyle(DWORD dwItem, DWORD *pdwData);
	virtual HRESULT TxGetWindowStyles(DWORD *pdwStyle, DWORD *pdwExStyle);
	virtual HRESULT TxShowDropCaret(BOOL fShow, HDC hdc, LPCRECT prc);
	virtual HRESULT TxDestroyCaret();
	virtual HRESULT TxGetHorzExtent(LONG *plHorzExtent);

	bool init(HWND hWnd, CREATESTRUCT& cs);
	HWND getWindow() const { return hWnd; }
	void setWindowRect(const RECT& rc);
	SIZEL getExtent() const { return extent; }
	void setExtent(SIZEL value);
	void draw(HDC hDC, RECT& rcPaint);
	bool setCursor(POINT pt);
	ITextServices* getTextServices() { return textServices; }
	ITextDocument* queryTextDocument();
	IRichEditOle* queryRichEditOle();
	HRESULT sendMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *pRes);
	CScrollBar& getVScroll() { return ctrlVScroll; }
	CScrollBar& getHScroll() { return ctrlHScroll; }
	bool isLargeSBRange() const;
	void setTextMargins(const MARGINS& m);
	int getScrollBarFlags() const { return sbFlags; }

private:
	int refs;
	HWND hWnd;
	RECT rcWindow;
	RECT rcClient;
	ITextServices* textServices;
	int flags;
	int sbFlags;
	int sbWidth;
	int sbHeight;
	CScrollBar ctrlHScroll;
	CScrollBar ctrlVScroll;
	unsigned winStyle;
	MARGINS textMargins;
	SIZEL extent;
	CHARFORMAT2W charFormat;
	PARAFORMAT2	paraFormat;
	TCHAR passwordChar;
	LONG selBarWidth;
	SIZE caretSize;
	unsigned maxTextLength;

	static void initCharFormat(CHARFORMAT2* cf, const LOGFONT& lf);
	static void initParaFormat(PARAFORMAT2* pf);
	void updateVScroll();
	void updateHScroll();
};

#endif // TEXT_HOST_IMPL_H_
