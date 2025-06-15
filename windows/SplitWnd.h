#ifndef SPLIT_WND_H_
#define SPLIT_WND_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlcrack.h>

#include <stdint.h>
#include <vector>

class SplitWndBase
{
	public:
		SplitWndBase();
		~SplitWndBase();

		class Callback
		{
		public:
			static const int SIZE_UNDEFINED      = -1;
			static const int SIZE_USE_MINMAX_MSG = -2;

			virtual void setPaneRect(int pane, const RECT& rc) = 0;
			virtual void splitterMoved(int splitter) = 0;
			virtual int getMinPaneSize(int pane) const { return SIZE_UNDEFINED; }
		};

		void addSplitter(int pane, uint16_t flags, int position, int thickness = -1);
		void setPaneWnd(int pane, HWND hWnd);
		void setSplitterColor(int index, int colorType, COLORREF color);
		void setSplitterPos(int index, int pos, bool proportional);
		void setMargins(const MARGINS& margins);
		void setCallback(Callback* p) { callback = p; }
		void setFullDragMode(int mode);
		void setSinglePaneMode(int pane);
		int getSplitterPos(int index, bool proportional) const;
		int getSinglePaneMode() const { return singlePane; }
		void updateClientRect(HWND hWnd);
		int getOptions() const { return options; }
		void setOptions(int opt) { options = opt; }

		enum
		{
			FLAG_HORIZONTAL    = 0x01,
			FLAG_PROPORTIONAL  = 0x02,
			FLAG_ALIGN_OTHER   = 0x04,
			FLAG_INTERACTIVE   = 0x08,
			FLAG_SET_THICKNESS = 0x10
		};

		enum
		{
			COLOR_TYPE_TRANSPARENT,
			COLOR_TYPE_RGB,
			COLOR_TYPE_SYSCOLOR
		};

		enum
		{
			FULL_DRAG_DEFAULT,
			FULL_DRAG_ENABLED,
			FULL_DRAG_DISABLED
		};

		enum
		{
			OPT_PAINT_MARGINS = 1
		};

	protected:
		void getPaneRect(int pane, RECT& rc) const;
		void updateLayout();
		void cleanup();
		void draw(HDC hdc);
		bool handleButtonPress(HWND hWnd, POINT pt);
		void handleButtonRelease(HWND hWnd);
		void handleMouseMove(HWND hWnd, POINT pt);
		void resetCapture(HWND hWnd);
		void redraw(HWND hWnd, int index);

	private:
		enum
		{
			FLAG_WIDTH_CHANGED  = 1,
			FLAG_HEIGHT_CHANGED = 2,
			FLAG_WND_HIDDEN     = 4
		};

		enum
		{
			STATE_DRAGGING               = 1,
			STATE_POSITION_CHANGED       = 2,
			STATE_FORCE_UPDATE           = 4,
			STATE_APPLY_PROPORTIONAL_POS = 8
		};

		struct PaneInfo
		{
			RECT rc;
			HWND hWnd;
			uint16_t flags;
		};

		struct SplitterInfo
		{
			uint16_t flags;
			uint16_t state;
			int prev;
			int position;
			int propPos;
			int thickness;
			PaneInfo panes[2];
			int colorType;
			COLORREF color;
		};

		std::vector<SplitterInfo> sp;
		RECT rcClient;
		MARGINS margins;
		int hotIndex;
		POINT prevPos;
		Callback* callback;
		HBRUSH halftoneBrush;
		int fullDragMode;
		int ghostBarPos;
		int singlePane;
		int options;

		void drawFrame(HDC hdc) const;
		void drawGhostBar(HWND hWnd);
		void getRootRect(RECT& rc) const;
		void updatePosition(int index, int& newPos, RECT newRect[]) const;
		bool setRect(PaneInfo& info, const RECT& rc);
		bool setRects(SplitterInfo& info, const RECT newRect[]);
		bool changePosition(int index, int newPos);
		void updateProportionalPos(SplitterInfo& info);
		int getProportionalPos(const SplitterInfo& info, const RECT& rc) const;
		void paneRectUpdated(int index, int which);
		void getSplitterBarRect(int index, RECT& rc) const;
		void getSplitterBarRect(int index, int pos, RECT& rc) const;
		int findSplitterBar(POINT pt) const;
		void updateHotIndex(POINT pt);
		int getMinPaneSize(int pane) const;
		static int getDefaultThickness(uint16_t flags);
};

template<typename T>
class SplitWndImpl : public SplitWndBase
{
	public:
		void updateLayout()
		{
			T* pT = static_cast<T*>(this);
			updateClientRect(pT->m_hWnd);
			SplitWndBase::updateLayout();
		}

		BEGIN_MSG_MAP(SplitWndImpl)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, onLButtonUp)
		MESSAGE_HANDLER(WM_CAPTURECHANGED, onCaptureChanged)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 0;
		}

		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &bHandled)
		{
			cleanup();
			return 0;
		}

		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 1;
		}

		LRESULT onPaint(UINT, WPARAM, LPARAM, BOOL&)
		{
			T* pT = static_cast<T*>(this);
			updateClientRect(pT->m_hWnd);
			PAINTSTRUCT ps;
			HDC hdc = pT->BeginPaint(&ps);
			draw(hdc);
			pT->EndPaint(&ps);
			return 0;
		}

		LRESULT onSize(UINT, WPARAM, LPARAM, BOOL&)
		{
			T* pT = static_cast<T*>(this);
			updateClientRect(pT->m_hWnd);
			updateLayout();
			return 0;
		}

		LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			T* pT = static_cast<T*>(this);
			POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			handleMouseMove(pT->m_hWnd, pt);
			return 0;
		}

		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			T* pT = static_cast<T*>(this);
			POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			if (!handleButtonPress(pT->m_hWnd, pt))
				bHandled = FALSE;
			return 0;
		}

		LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			T* pT = static_cast<T*>(this);
			handleButtonRelease(pT->m_hWnd);
			return 0;
		}

		LRESULT onCaptureChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			T* pT = static_cast<T*>(this);
			resetCapture(pT->m_hWnd);
			return 0;
		}
};

#endif // SPLIT_WND_H_
