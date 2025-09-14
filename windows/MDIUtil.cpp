#include "stdafx.h"
#include "MDIUtil.h"
#include "FlatTabCtrl.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"

static HHOOK hook = nullptr;
HWND WinUtil::mdiClient = nullptr;
FlatTabCtrl* WinUtil::tabCtrl = nullptr;

void WinUtil::activateMDIChild(HWND hWnd)
{
	SendMessage(mdiClient, WM_SETREDRAW, FALSE, 0);
	SendMessage(mdiClient, WM_MDIACTIVATE, (WPARAM) hWnd, 0);
	SendMessage(mdiClient, WM_SETREDRAW, TRUE, 0);
	RedrawWindow(mdiClient, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

HWND WinUtil::getActiveMDIChild(BOOL* maximized)
{
	dcassert(::IsWindow(mdiClient));
	return (HWND) SendMessage(mdiClient, WM_MDIGETACTIVE, 0, (LPARAM) maximized);
}

bool WinUtil::useMDIMaximized()
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	return ss->getBool(Conf::MDI_MAXIMIZED);
}

static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
		if (WinUtil::tabCtrl && wParam == VK_CONTROL && LOWORD(lParam) == 1)
		{
			if (lParam & 0x80000000)
				WinUtil::tabCtrl->endSwitch();
			else
				WinUtil::tabCtrl->startSwitch();
		}
	}
	return CallNextHookEx(hook, code, wParam, lParam);
}

void WinUtil::installKeyboardHook()
{
	if (!hook)
		hook = SetWindowsHookEx(WH_KEYBOARD, keyboardProc, nullptr, GetCurrentThreadId());
}

void WinUtil::removeKeyboardHook()
{
	if (hook)
	{
		UnhookWindowsHookEx(hook);
		hook = nullptr;
	}
}
