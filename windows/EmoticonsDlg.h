#ifndef EMOTICONS_DLG_H_
#define EMOTICONS_DLG_H_

#ifdef BL_UI_FEATURE_EMOTICONS

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "../client/typedefs.h"

class CGDIImage;

class CAnimatedButton: public CWindowImpl<CAnimatedButton, CButton>
{
		CGDIImage *image;
		bool initialized;
		int xBk;
		int yBk;
		int xSrc;
		int ySrc;
		int wSrc;
		int hSrc;
		int width, height;

		HDC hBackDC;
		HTHEME hTheme;

	public:
		BEGIN_MSG_MAP(CAnimatedButton)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_ERASEBKGND, onErase)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_THEMECHANGED, onThemeChanged)
		END_MSG_MAP()
		
		explicit CAnimatedButton(CGDIImage *pImage);
		~CAnimatedButton();
		
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onErase(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	private:
		static bool __cdecl OnFrameChanged(CGDIImage *pImage, LPARAM lParam);
		void drawBackground(HDC hdc);
		void drawImage(HDC hdc);
		void draw(HDC hdc);
};

class EmoticonsDlg : public CDialogImpl<EmoticonsDlg>
{
	public:
		enum { IDD = IDD_EMOTICONS_DLG };
		
		BEGIN_MSG_MAP(EmoticonsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_RBUTTONUP, onClose)
		MESSAGE_HANDLER(WM_LBUTTONUP, onClose)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		COMMAND_CODE_HANDLER(BN_CLICKED, onIconClick)
		NOTIFY_CODE_HANDLER(BCN_HOTITEMCHANGE, onHotItemChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onIconClick(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/);
		LRESULT onHotItemChange(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			EndDialog(0);
			return 0;
		}
		~EmoticonsDlg();
		
		tstring result;
		CRect pos;
		bool isError = false;
		HWND hWndNotif = nullptr;

	private:
		CToolTipCtrl tooltip;
		
		static WNDPROC g_MFCWndProc;
		static LRESULT CALLBACK NewWndProc(HWND, UINT, WPARAM, LPARAM);
		static EmoticonsDlg* g_pDialog;
		
		std::vector<CAnimatedButton*> buttonList;
		void clearButtons();
};

#endif // BL_UI_FEATURE_EMOTICONS

#endif // EMOTICONS_DLG_H_
