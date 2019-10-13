/*
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

#include "stdafx.h"
#include "HIconWrapper.h"

HIconWrapper::~HIconWrapper()
{
	if (icon && (fuLoad & LR_SHARED) == 0) //
		// "It is only necessary to call DestroyIcon for icons and cursors created with the following functions:
		// CreateIconFromResourceEx (if called without the LR_SHARED flag), CreateIconIndirect, and CopyIcon.
		// Do not use this function to destroy a shared icon.
		//A shared icon is valid as long as the module from which it was loaded remains in memory.
		// The following functions obtain a shared icon.
	{
		dcdrun(const BOOL res =) DestroyIcon(icon);
		// Ругаемся на кастомных иконках dcassert(l_res);
	}
}

HICON HIconWrapper::load(WORD id, int cx, int cy, UINT fuLoad)
{
	dcassert(id);
	fuLoad |= LR_SHARED;
	HICON icon = NULL;
	const auto themeHandle = ThemeManager::getResourceLibInstance();
	if (themeHandle)
	{
		icon = (HICON) LoadImage(ThemeManager::getResourceLibInstance(), MAKEINTRESOURCE(id), IMAGE_ICON, cx, cy, fuLoad);
		if (!icon)
		{
			dcdebug("!!!!!!!![Error - 1] (HICON)::LoadImage: ID = %d, fuLoad = %x\n", id, fuLoad);
		}
	}
	if (!icon)
	{
		static const HMODULE g_current = GetModuleHandle(nullptr);
		if (themeHandle != g_current)
		{
			icon = (HICON) LoadImage(g_current, MAKEINTRESOURCE(id), IMAGE_ICON, cx, cy, fuLoad);
			dcdebug("!!!!!!!![step - 2] (HICON)::LoadImage: ID = %d, icon = %p, fuLoad = %x\n", id, icon, fuLoad);
		}
	}
	dcassert(icon);
	return icon;
}
