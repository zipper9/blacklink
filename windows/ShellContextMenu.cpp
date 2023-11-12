/*
*
* Based on a class by R. Engels
* http://www.codeproject.com/shell/shellcontextmenu.asp
*/

#include "stdafx.h"
#include "resource.h"
#include "ShellContextMenu.h"

ShellContextMenu* ShellContextMenu::instance = nullptr;

ShellContextMenu::ShellContextMenu() :
	shellFolder(nullptr),
	pidlParent(nullptr),
	pidlChild(nullptr),
	hMenu(nullptr),
	menuAttached(false),
	contextMenu(nullptr),
	contextMenu2(nullptr),
	contextMenu3(nullptr),
	oldWndProc(nullptr)
{
}

ShellContextMenu::~ShellContextMenu()
{
	cleanup();
	if (hMenu && !menuAttached) DestroyMenu(hMenu);
}

void ShellContextMenu::cleanup()
{
	if (shellFolder)
	{
		shellFolder->Release();
		shellFolder = nullptr;
	}
	if (pidlParent)
	{
		CoTaskMemFree(pidlParent);
		pidlParent = nullptr;
	}
	pidlChild = nullptr;
}

void ShellContextMenu::setPath(const wstring& strPath)
{
	cleanup();
	SFGAOF sfgao;
	HRESULT hr = SHParseDisplayName(const_cast<WCHAR*>(strPath.c_str()), nullptr, &pidlParent, 0, &sfgao);
	if (SUCCEEDED(hr))
	{
		hr = SHBindToParent(pidlParent, IID_IShellFolder, (void**) &shellFolder, &pidlChild);
		if (FAILED(hr)) cleanup();
	}
}

HMENU ShellContextMenu::getMenu()
{
	if (!hMenu) hMenu = CreatePopupMenu();
	return hMenu;
}

void ShellContextMenu::attachMenu(HMENU hNewMenu)
{
	if (hMenu && !menuAttached)
		DestroyMenu(hMenu);
	hMenu = hNewMenu;
	menuAttached = true;
}

HMENU ShellContextMenu::detachMenu()
{
	HMENU res = nullptr;
	if (menuAttached)
	{
		res = hMenu;
		hMenu = nullptr;
		menuAttached = false;
	}
	return res;
}

UINT ShellContextMenu::showContextMenu(HWND hWnd, POINT pt)
{
	int menuType = initContextMenu();
	if (!menuType) return 0;

	getMenu();

	// lets fill the popupmenu
	contextMenu->QueryContextMenu(hMenu, GetMenuItemCount(hMenu), ID_SHELLCONTEXTMENU_MIN, ID_SHELLCONTEXTMENU_MAX, CMF_NORMAL | CMF_EXPLORE);
	
	instance = this;
	// subclass window to handle menu related messages in ShellContextMenu
	if (menuType > 1)  // only subclass if its version 2 or 3
		oldWndProc = (WNDPROC) SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) hookWndProc);
	else
		oldWndProc = nullptr;

	UINT idCommand = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);

	if (oldWndProc) // unsubclass
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) oldWndProc);

	instance = nullptr;
		
	if (idCommand >= ID_SHELLCONTEXTMENU_MIN && idCommand <= ID_SHELLCONTEXTMENU_MAX)
	{
		invokeCommand(idCommand - ID_SHELLCONTEXTMENU_MIN);
		idCommand = 0;
	}

	contextMenu->Release();
	contextMenu = nullptr;
	if (contextMenu2)
	{
		contextMenu2->Release();
		contextMenu2 = nullptr;
	}
	if (contextMenu3)
	{
		contextMenu3->Release();
		contextMenu3 = nullptr;
	}
	return idCommand;
}

int ShellContextMenu::initContextMenu()
{
	if (!shellFolder || !pidlChild) return 0;
	int menuType = 0;

	// first we retrieve the normal IContextMenu interface(every object should have it)
	shellFolder->GetUIObjectOf(nullptr, 1, &pidlChild, IID_IContextMenu, nullptr, (void**) &contextMenu);

	if (contextMenu)
	{
		// since we got an IContextMenu interface we can now obtain the higher version interfaces via that
		if (contextMenu->QueryInterface(IID_IContextMenu3, (void**) &contextMenu3) == S_OK)
			menuType = 3;
		else if (contextMenu->QueryInterface(IID_IContextMenu2, (void**) &contextMenu2) == S_OK)
			menuType = 2;
		else
			menuType = 1;
	}
	return menuType;
}

void ShellContextMenu::invokeCommand(UINT idCommand)
{
	CMINVOKECOMMANDINFO cmi = {0};
	cmi.cbSize = sizeof(CMINVOKECOMMANDINFO);
	cmi.lpVerb = (LPSTR) MAKEINTRESOURCE(idCommand);
	cmi.nShow = SW_SHOWNORMAL;
	contextMenu->InvokeCommand(&cmi);
}

LRESULT CALLBACK ShellContextMenu::hookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_MENUCHAR: // only supported by IContextMenu3
			if (instance->contextMenu3)
			{
				LRESULT lResult = 0;
				instance->contextMenu3->HandleMenuMsg2(message, wParam, lParam, &lResult);
				return lResult;
			}
			break;

		case WM_DRAWITEM:
		case WM_MEASUREITEM:
			if (wParam)
				break; // if wParam != 0 then the message is not menu-related

		case WM_INITMENUPOPUP:
			if (instance->contextMenu2)
				instance->contextMenu2->HandleMenuMsg(message, wParam, lParam);
			else if (instance->contextMenu3)
				instance->contextMenu3->HandleMenuMsg(message, wParam, lParam);
			return (message == WM_INITMENUPOPUP) ? 0 : TRUE; // inform caller that we handled WM_INITPOPUPMENU by ourself

		default:
			break;
	}

	// call original WndProc of window to prevent undefined bevhaviour of window
	return ::CallWindowProc(instance->oldWndProc, hWnd, message, wParam, lParam);
}
