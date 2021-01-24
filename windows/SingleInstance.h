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

#ifndef SINGLE_INSTANCE_H
#define SINGLE_INSTANCE_H

#include "../client/w.h"

#define WMU_WHERE_ARE_YOU_MSG _T("WMU_WHERE_ARE_YOU-{C8052503-235C-486A-A7A2-1D614A9A4242}")
const UINT WMU_WHERE_ARE_YOU = ::RegisterWindowMessage(WMU_WHERE_ARE_YOU_MSG);

class SingleInstance
{
		DWORD  lastError;
		HANDLE hMutex;
		
	public:
		SingleInstance(const TCHAR* strMutexName)
		{
			// strMutexName must be unique
			hMutex = CreateMutex(NULL, FALSE, strMutexName);
			lastError = GetLastError();
		}
		
		~SingleInstance()
		{
			if (hMutex) CloseHandle(hMutex);
		}
		
		BOOL IsAnotherInstanceRunning() const
		{
			return lastError == ERROR_ALREADY_EXISTS;
		}
};

#endif // !defined(SINGLE_INSTANCE_H)
