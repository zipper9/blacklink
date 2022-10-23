/*
* Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
* Based on a class by R. Engels
* http://www.codeproject.com/shell/shellcontextmenu.asp
*/

#ifndef DCPLUSPLUS_WIN32_SHELL_CONTEXT_MENU_H
#define DCPLUSPLUS_WIN32_SHELL_CONTEXT_MENU_H

#include "../client/typedefs.h"
#include "../client/w.h"

class ShellContextMenu
{
		static ShellContextMenu* instance;

	public:
		ShellContextMenu();
		~ShellContextMenu();

		void setPath(const wstring& strPath);
		HMENU getMenu();
		void attachMenu(HMENU hMenu);
		HMENU detachMenu();
		UINT showContextMenu(HWND hWnd, POINT pt);

	private:
		bool menuAttached;
		HMENU hMenu;
		IShellFolder* shellFolder;
		LPITEMIDLIST pidlParent;
		LPCITEMIDLIST pidlChild;

		IContextMenu* contextMenu;
		IContextMenu2* contextMenu2;
		IContextMenu3* contextMenu3;
		WNDPROC oldWndProc;

		int initContextMenu();
		void invokeCommand(UINT idCommand);
		void cleanup();
		static LRESULT CALLBACK hookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};


#endif // !defined(DCPLUSPLUS_WIN32_SHELL_CONTEXT_MENU_H)
