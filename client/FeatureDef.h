/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef FEATURE_DEF_H_
#define FEATURE_DEF_H_

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

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4244) // 'argument' : conversion from 'int' to 'unsigned short', possible loss of data
#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4456) // declaration of 'l_lock' hides previous local declaration

#ifdef _WIN64
# pragma warning(disable: 4267) // conversion from 'xxx' to 'yyy', possible loss of data
#endif

#if _MSC_VER == 1900
#if _MSC_FULL_VER < 190023918
#error Visual Studio 2015 Update 2 is required
// https://www.visualstudio.com/en-us/news/vs2015-update2-vs.aspx
#endif

# pragma warning(disable: 4592) // 'trustedKeyprint' : symbol will be dynamically initialized(implementation limitation)

// https://connect.microsoft.com/VisualStudio/feedback/details/1892487/code-generated-by-msvc-doesnt-operate-atomically-on-std-atomic-t-object-when-sizeof-t-n-alignof-t-n-n-2-4-8
// Enable a bugfix in VS2015 update 2, remove in the next version of VS2015
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#endif

#if _MSC_VER >= 1900
# pragma warning(disable: 4458) // C4458: declaration of 'nativeImage' hides class member
#endif

#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE 1

#endif

#ifdef _DEBUG
#define BL_FEATURE_COLLECT_UNKNOWN_TAGS
#define BL_FEATURE_COLLECT_UNKNOWN_FEATURES
#endif

#define BL_FEATURE_IP_DATABASE
#define BL_FEATURE_DROP_SLOW_SOURCES
#define BL_FEATURE_WEB_SERVER
#define BL_FEATURE_NMDC_EXT_JSON
#define BL_FEATURE_IPFILTER

#define FLYLINKDC_USE_STATS_FRAME
//#define FLYLINKDC_USE_ASK_SLOT // отключаем автопопрошайку

#ifdef BL_FEATURE_IP_DATABASE
# define FLYLINKDC_USE_COLUMN_RATIO
#endif

#define BL_UI_FEATURE_BB_CODES
#define BL_UI_FEATURE_EMOTICONS
#undef  BL_UI_FEATURE_VIEW_AS_TEXT

#define IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
// #define IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
#define IRAINMAN_USE_HIDDEN_USERS // http://adc.sourceforge.net/ADC-EXT.html#_hidden_status_for_client_type

#define USE_SPIN_LOCK
#define USE_APPDATA

// [+] SSA - новый алгоритм поиска имени файла и title для WinAmp
#define SSA_NEW_WINAMP_PROC_FOR_TITLE_AND_FILENAME
// [+] SSA - отображать пусто в magnet, если не найден файл в шаре
// #define SSA_DONT_SHOW_MAGNET_ON_NO_FILE_IN_SHARE
#define SSA_IPGRANT_FEATURE // [+] SSA additional slots for special IP's
// TODO
//#define SSA_SHELL_INTEGRATION

#ifdef BL_UI_FEATURE_EMOTICONS
# define IRAINMAN_INCLUDE_GDI_OLE 1
#endif

#if defined(IRAINMAN_INCLUDE_GDI_OLE)
# define IRAINMAN_INCLUDE_GDI_INIT
#endif

#ifdef _DEBUG
#define DEBUG_USER_CONNECTION
#define LOCK_DEBUG
#endif

// TODO: remove it from release after testing
#define DEBUG_GDI_IMAGE

#define FLYLINKDC_SUPPORT_HUBTOPIC

#define FLYLINKDC_USE_SOCKET_COUNTER

#define HAVE_NATPMP_H
#define HAVE_OPENSSL

#if defined _POSIX_SOURCE || defined _GNU_SOURCE || defined _POSIX_C_SOURCE || defined _XOPEN_SOURCE || defined _BSD_SOURCE
#define HAVE_TIME_R
#define HAVE_STRERROR_R
#endif

#define USE_QUEUE_RWLOCK

// Make sure we're using the templates from algorithm...
#ifdef min
# undef min
#endif
#ifdef max
# undef max
#endif

#endif // FEATURE_DEF_H_
