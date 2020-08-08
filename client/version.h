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
#ifndef FLY_VERSION_H
#define FLY_VERSION_H

#undef FLYLINKDC_SUPPORT_WIN_XP
#undef FLYLINKDC_SUPPORT_WIN_VISTA

#define APPNAME "blacklink"

#define VERSION_MAJOR 0
#define VERSION_MINOR 3

#define VER_STRINGIZE1(a) #a
#define VER_STRINGIZE(a) VER_STRINGIZE1(a)

#define VERSION_MAJOR_STR VER_STRINGIZE(VERSION_MAJOR)
#define VERSION_MINOR_STR VER_STRINGIZE(VERSION_MINOR)

#define VERSION_STR VERSION_MAJOR_STR "." VERSION_MINOR_STR

#define DCVERSIONSTRING "0.785"

#endif // FLY_VERSION_H

/* Update the .rc file as well... */
