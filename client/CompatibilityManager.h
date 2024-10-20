/*
 * Copyright (C) 2011-2013 Alexey Solomin, a.rainman on gmail pt com
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

#ifndef COMPATIBILITY_MANGER_H
#define COMPATIBILITY_MANGER_H

#ifdef _WIN32

#include "typedefs.h"
#include "w.h"

#ifndef SORT_DIGITSASNUMBERS
#define SORT_DIGITSASNUMBERS 0x00000008
#endif

class CompatibilityManager
{
	public:
		// Call this function as soon as possible (immediately after the start of the program).
		static void init();

		static string getDefaultPath();
		static LONG getComCtlVersion() { return comCtlVersion; }

		static FINDEX_INFO_LEVELS findFileLevel;
		static DWORD findFileFlags;

#if defined(OSVER_WIN_XP) || defined(OSVER_WIN_VISTA)
		static DWORD compareFlags;
#else
		static constexpr DWORD compareFlags = SORT_DIGITSASNUMBERS;
#endif

	private:
		static LONG comCtlVersion;

	public:
		static bool setThreadName(HANDLE h, const char* name);
};

#endif // _WIN32

#endif // COMPATIBILITY_MANGER_H
