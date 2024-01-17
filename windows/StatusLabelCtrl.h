#ifndef STATUS_LABEL_CTRL_H_
#define STATUS_LABEL_CTRL_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "../client/typedefs.h"

class BackingStore;

class StatusLabelCtrl: public CWindowImpl<StatusLabelCtrl>
{
	public:
		StatusLabelCtrl() {}
		~StatusLabelCtrl() { cleanup(); }

		StatusLabelCtrl(const StatusLabelCtrl&) = delete;
		StatusLabelCtrl& operator= (const StatusLabelCtrl&) = delete;

		BEGIN_MSG_MAP(StatusLabelCtrl)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SETTEXT, onSetText)
		MESSAGE_HANDLER(WM_SETFONT, onSetFont)
		MESSAGE_HANDLER(WM_GETFONT, onGetFont)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return 0; }
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { cleanup(); return 0; }
		LRESULT onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSetFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onGetFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) { return TRUE; }
		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&);

		void setImage(int index, int size = 0);
		SIZE getIdealSize(HDC hdc);
		void setText(const tstring& s);
		const tstring& getText() const { return text; }

	private:
		tstring text;
		HFONT font = nullptr;
		HBITMAP hBitmap = nullptr;
		int bitmapWidth = 0;
		int bitmapHeight = 0;
		int textWidth = -1;
		int textHeight = -1;
		int iconSpace = 0;
		BackingStore* backingStore = nullptr;

		void updateTextSize(HDC hdc);
		void draw(HDC hdc, const RECT& rc);
		void cleanup();
};

#endif // STATUS_LABEL_CTRL_H_
