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

#ifndef _H_ICON_WRAPPER_H_
#define _H_ICON_WRAPPER_H_

#include "ThemeManager.h"

class HIconWrapper
{
	public:
		HIconWrapper(): icon(NULL), fuLoad(0) {}
		
		explicit HIconWrapper(WORD id, int cx = 16, int cy = 16, UINT fuLoad = LR_DEFAULTCOLOR) :
			fuLoad(fuLoad)
		{
			icon = load(id, cx, cy, fuLoad);
			dcassert(icon);
		}
		explicit HIconWrapper(HICON icon) : icon(icon), fuLoad(LR_DEFAULTCOLOR)
		{
			dcassert(icon);
		}

		HIconWrapper(const HIconWrapper&) = delete;
		HIconWrapper& operator= (const HIconWrapper&) = delete;

		HIconWrapper(HIconWrapper &&src): icon(src.icon), fuLoad(src.fuLoad)
		{
			src.icon = NULL;
		}

		HIconWrapper& operator= (HIconWrapper &&src)
		{
			icon = src.icon;
			fuLoad = src.fuLoad;
			src.icon = NULL;
			return *this;
		}
		
		~HIconWrapper();
		operator HICON() const { return icon; }
		HICON detach()
		{
			HICON result = icon;
			icon = NULL;
			return result;
		}
		
	private:
		static HICON load(WORD id, int cx, int cy, UINT fuLoad);
		
	private:
		HICON icon;
		UINT fuLoad;
};

#endif //_H_ICON_WRAPPER_H_
