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

#ifndef DCPLUSPLUS_DCPP_COMPILER_H
#define DCPLUSPLUS_DCPP_COMPILER_H

#if !defined(_DEBUG) && !defined(NDEBUG)
#error define NDEBUG in Release configurations
#endif

#ifdef __clang__

#elif defined(__GNUC__)

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 5)
#error GCC 4.5 is required
#endif

#elif defined(_MSC_VER)

#if _MSC_VER < 1900 || (_MSC_VER == 1900 && _MSC_FULL_VER < 190023918)
#error Visual Studio 2015 Update 2 is required
#endif

#ifndef _DEBUG
#define _ITERATOR_DEBUG_LEVEL 0 // VC++ 2010
#define BOOST_DISABLE_ASSERTS 1
#define _HAS_ITERATOR_DEBUGGING 0
#else
#define _ITERATOR_DEBUG_LEVEL 2 // VC++ 2010 
#define _HAS_ITERATOR_DEBUGGING 1
#endif

//disable the deprecated warnings for the CRT functions.
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#define _ATL_SECURE_NO_DEPRECATE 1

#ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif

#pragma warning(disable: 4996)
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4244) // 'argument' : conversion from 'int' to 'unsigned short', possible loss of data
#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4456) // declaration of 'lock' hides previous local declaration
#pragma warning(disable: 4458) // C4458: declaration of 'nativeImage' hides class member

#ifdef _WIN64
#pragma warning(disable: 4267) // conversion from 'xxx' to 'yyy', possible loss of data
#endif

#if _MSC_VER == 1900
#pragma warning(disable: 4592) // 'trustedKeyprint' : symbol will be dynamically initialized(implementation limitation)
#endif

#else

#error No supported compiler found

#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#define _ULL(x) x##ull
#define I64_FMT "%I64d"
#define U64_FMT "%I64u"

#elif defined(SIZEOF_LONG) && SIZEOF_LONG == 8
#define _ULL(x) x##ul
#define I64_FMT "%ld"
#define U64_FMT "%lu"

#else
#define _ULL(x) x##ull
#define I64_FMT "%lld"
#define U64_FMT "%llu"
#endif

#ifndef SIZEOF_WCHAR
#ifdef _WIN32
#define SIZEOF_WCHAR 2
#else
#define SIZEOF_WCHAR 4
#endif
#endif

#if defined _POSIX_SOURCE || defined _GNU_SOURCE || defined _POSIX_C_SOURCE || defined _XOPEN_SOURCE || defined _BSD_SOURCE
#define HAVE_TIME_R
#define HAVE_STRERROR_R
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifndef BOOST_NO_RTTI
#define BOOST_NO_RTTI
#endif

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifdef _WIN32
#ifndef BOOST_USE_WINDOWS_H
#define BOOST_USE_WINDOWS_H
#endif
#endif

#include "FeatureDef.h"

#endif // DCPLUSPLUS_DCPP_COMPILER_H
