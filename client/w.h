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

#ifndef DCPLUSPLUS_DCPP_W_H_
#define DCPLUSPLUS_DCPP_W_H_

#include "compiler.h"

#include "w_flylinkdc.h"

#ifndef STRICT
#define STRICT 1
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef NOSERVICE
#define NOSERVICE
#endif

#ifndef NOMCX
#define NOMCX
#endif

#ifndef NOIME
#define NOIME
#endif

#include <windows.h>
#include <tchar.h>

// http://msdn.microsoft.com/en-us/library/windows/desktop/ms644930(v=vs.85).aspx
// WM_USER through 0x7FFF
// Integer messages for use by private window classes.
// WM_APP through 0xBFFF
// Messages available for use by applications.

#define WM_SPEAKER (WM_APP + 500)

#endif // DCPLUSPLUS_DCPP_W_H_
