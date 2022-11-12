/*
 * Copyright (C) 2006-2013 Crise, crise<at>mail.berlios.de
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

#ifndef RESOURCE_LOADER_H
#define RESOURCE_LOADER_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>
#include <atlimage.h>

#define USE_THEME_MANAGER

#ifdef USE_THEME_MANAGER
#include "ThemeManager.h"
#endif

class CImageEx : public CImage
{
	public:
		CImageEx(): buffer(nullptr) {}
		explicit CImageEx(LPCTSTR fileName) noexcept : buffer(nullptr)
		{
			Load(fileName);
		}
		CImageEx(UINT id, LPCTSTR pType = RT_RCDATA, HMODULE hInst =
#if defined(USE_THEME_MANAGER)
		             ThemeManager::getResourceLibInstance()
#else
		             nullptr
#endif
		        ) noexcept :
			buffer(nullptr)
		{
			LoadFromResource(id, pType, hInst);
		}
		CImageEx(UINT id, UINT type, HMODULE hInst =
#if defined(USE_THEME_MANAGER)
		             ThemeManager::getResourceLibInstance()
#else
		             nullptr
#endif
		        ) noexcept :
			buffer(nullptr)
		{
			LoadFromResource(id, MAKEINTRESOURCE(type), hInst);
		}
		
		~CImageEx()
		{
			Destroy();
		}

		CImageEx(const CImageEx&) = delete;
		CImageEx& operator= (const CImageEx&) = delete;
		CImageEx& operator= (CImageEx&&);

		bool LoadFromResourcePNG(UINT id) noexcept;
		bool LoadFromResource(UINT id, LPCTSTR pType = RT_RCDATA, HMODULE hInst =
#if defined(USE_THEME_MANAGER)
		                          ThemeManager::getResourceLibInstance()
#else
		                          nullptr
#endif
		                     ) noexcept;
		void Destroy() noexcept;
		
	private:
		HGLOBAL buffer;
};

class ResourceLoader
{
	public:
		static int LoadImageList(LPCTSTR fileName, CImageList& imgList, int cx, int cy);
		static int LoadImageList(UINT id, CImageList& imgList, int cx, int cy);
};

#endif // RESOURCE_LOADER_H
