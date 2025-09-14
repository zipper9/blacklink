#ifndef MDI_UTIL_H_
#define MDI_UTIL_H_

#include "../client/w.h"

class FlatTabCtrl;

namespace WinUtil
{
	extern HWND mdiClient;
	extern FlatTabCtrl *tabCtrl;

	void activateMDIChild(HWND hWnd);
	HWND getActiveMDIChild(BOOL* maximized = nullptr);
	bool useMDIMaximized();
	void installKeyboardHook();
	void removeKeyboardHook();

	template<class T> static HWND hiddenCreateEx(T& p) noexcept
	{
		const HWND active = (HWND) ::SendMessage(mdiClient, WM_MDIGETACTIVE, 0, 0);
		LockWindowUpdate(mdiClient);
		HWND ret = p.Create(mdiClient);
		if (active && ::IsWindow(active))
			::SendMessage(mdiClient, WM_MDIACTIVATE, (WPARAM)active, 0);
		LockWindowUpdate(NULL);
		return ret;
	}

	template<class T> static HWND hiddenCreateEx(T* p) noexcept
	{
		return hiddenCreateEx(*p);
	}
}

#endif // MDI_UTIL_H_
