/*
 * Copyright (C) 2010 Crise, crise<at>mail.berlios.de
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

#ifndef EX_MESSAGE_BOX
#define EX_MESSAGE_BOX

class ExMessageBox
{
	private:
		struct MessageBoxValues
		{
			HHOOK hHook;
			HWND  hWnd;
			WNDPROC lpMsgBoxProc;
			UINT typeFlags;
			LPCTSTR lpName;
			void* userdata;
		} static __declspec(thread) mbv;
		
		static LRESULT CALLBACK CbtHookProc(int nCode, WPARAM wParam, LPARAM lParam);
		
	public:
	
		// Get the default WindowProc for the message box (for CallWindowProc)
		static WNDPROC GetMessageBoxProc()
		{
			return mbv.lpMsgBoxProc;
		}
		
		// Userdata manipulation
		static void SetUserData(void* data)
		{
			mbv.userdata = data;
		}
		static void* GetUserData()
		{
			return mbv.userdata;
		}
		
		// Get the MessageBox flags
		static int GetTypeFlags()
		{
			return mbv.typeFlags;
		}
		
		// Show the message box
		static int Show(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType, WNDPROC wndProc);
};

int MessageBoxWithCheck(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, LPCTSTR lpQuestion, UINT uType, UINT& bCheck);

#endif // EX_MESSAGE_BOX
