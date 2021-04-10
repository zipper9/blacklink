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

#ifndef DCPLUSPLUS_DCPP_SINGLETON_H
#define DCPLUSPLUS_DCPP_SINGLETON_H

#include "typedefs.h"

template<typename T>
class Singleton
{
	public:
		explicit Singleton() { }
		virtual ~Singleton() { }

		Singleton(const Singleton&) = delete;
		Singleton& operator= (const Singleton&) = delete;
		
		static bool isValidInstance()
		{
			return instance != nullptr;
		}
		static T* getInstance()
		{
#ifdef _DEBUG
			if (!isValidInstance())
			{
				showErrorMessage("instance is null");
				dcassert(0);
			}
#endif
			return instance;
		}
		
		static void newInstance()
		{
#ifdef _DEBUG
			if (isValidInstance())
			{
				showErrorMessage("instance already created");
				dcassert(0);
			}
#endif
			delete instance;
			instance = new T();
		}
		
		static void deleteInstance()
		{
#ifdef _DEBUG
			if (!isValidInstance())
			{
				showErrorMessage("instance already deleted");
				dcassert(0);
			}
#endif
			delete instance;
			instance = nullptr;
		}
	protected:
		static T* instance;
#ifdef _DEBUG
		static void showErrorMessage(const char* message)
		{
#ifdef _WIN32
			::MessageBoxA(nullptr, typeid(T).name(), message, MB_OK | MB_ICONERROR);
#else
			fprintf(stderr, "%s: %s\n", typeid(T).name(), message);
#endif
		}
#endif
};

template<class T> T* Singleton<T>::instance = nullptr;

#endif // DCPLUSPLUS_DCPP_SINGLETON_H
