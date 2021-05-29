#ifndef STATIC_FRAME_H_
#define STATIC_FRAME_H_

#include "MainFrm.h"

template < class T, int title, int ID = -1 >
class StaticFrame
{
	public:
		StaticFrame()
		{
		}
		virtual ~StaticFrame()
		{
			g_frame = nullptr;
		}
		
		static T* g_frame;

		static void toggleWindow()
		{
			if (g_frame == nullptr)
			{
				g_frame = new T();
				g_frame->Create(WinUtil::g_mdiClient, g_frame->rcDefault, CTSTRING_I(ResourceManager::Strings(title)));
				setButtonPressed(ID, true);
			}
			else
			{
				// match the behavior of MainFrame::onSelected()
				HWND hWnd = g_frame->m_hWnd;
				if (isMDIChildActive(hWnd))
				{
					::PostMessage(hWnd, WM_CLOSE, NULL, NULL);
				}
				else if (g_frame->MDIGetActive() != hWnd)
				{
					MainFrame::getMainFrame()->MDIActivate(hWnd);
					setButtonPressed(ID, true);
				}
				else if (BOOLSETTING(TOGGLE_ACTIVE_WINDOW))
				{
					::SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
					g_frame->MDINext(hWnd);
					hWnd = g_frame->MDIGetActive();
					setButtonPressed(ID, true);
				}
				if (::IsIconic(hWnd))
					::ShowWindow(hWnd, SW_RESTORE);
			}
		}

		static void openWindow()
		{
			if (g_frame == nullptr)
			{
				g_frame = new T();
				g_frame->Create(WinUtil::g_mdiClient, g_frame->rcDefault, CTSTRING_I(ResourceManager::Strings(title)));
				setButtonPressed(ID, true);
			}
			else
			{
				HWND hWnd = g_frame->m_hWnd;
				if (isMDIChildActive(hWnd))
					return;
				MainFrame::getMainFrame()->MDIActivate(hWnd);
				setButtonPressed(ID, true);
				if (::IsIconic(hWnd))
					::ShowWindow(hWnd, SW_RESTORE);
			}
		}

		static bool isMDIChildActive(HWND hWnd)
		{
			HWND wnd = MainFrame::getMainFrame()->MDIGetActive();
			dcassert(wnd != NULL);
			return (hWnd == wnd);
		}

		static void closeWindow()
		{
			if (g_frame)
			{
				::PostMessage(g_frame->m_hWnd, WM_CLOSE, NULL, NULL);
			}
		}

	protected:
		static void setButtonPressed(int nID, bool bPressed /* = true */)
		{
			if (nID == -1) return;
			auto& toolbar = MainFrame::getMainFrame()->getToolBar();
			if (toolbar.IsWindow()) toolbar.CheckButton(nID, bPressed);
		}
};

template<class T, int title, int ID>
T* StaticFrame<T, title, ID>::g_frame = nullptr;

#endif // STATIC_FRAME_H_
