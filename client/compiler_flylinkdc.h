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

#ifndef DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H
#define DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H

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

//#define FLYLINKDC_USE_TORRENT

#ifdef _WIN32
#define ENABLE_WEB_SERVER
#endif

// #define FLYLINKDC_USE_DNS
#define FLYLINKDC_USE_DROP_SLOW
#define FLYLINKDC_USE_STATS_FRAME
//#define FLYLINKDC_USE_ASK_SLOT // отключаем автопопрошайку
//#define FLYLINKDC_USE_VACUUM

#define FLYLINKDC_USE_DOS_GUARD // Включаем защиту от DoS атаки старых версий - http://www.flylinkdc.ru/2011/01/flylinkdc-dos.html
#define FLYLINKDC_USE_IPFILTER
#define FLYLINKDC_USE_LASTIP_AND_USER_RATIO
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
# define FLYLINKDC_USE_COLUMN_RATIO
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

#define IRAINMAN_ENABLE_WHOIS
#define IRAINMAN_ENABLE_MORE_CLIENT_COMMAND
//#define IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
//#define IRAINMAN_INCLUDE_USER_CHECK // - Не понял нахрена оно нужно. если юзеров 100 тыщ то что будет?
#define IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
#define IRAINMAN_USE_BB_CODES // BB codes support http://ru.wikipedia.org/wiki/BbCode
//#define IRAINMAN_ENABLE_AUTO_BAN
#define IRAINMAN_INCLUDE_SMILE // Disable this to cut all smile support from code.
// #define IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
#define IRAINMAN_ENABLE_OP_VIP_MODE
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
# define IRAINMAN_ENABLE_OP_VIP_MODE_ON_NMDC
#endif
//#define IRAINMAN_THEME_MANAGER_LISTENER_ENABLE
//#define IRAINMAN_DISALLOWED_BAN_MSG
#ifndef IRAINMAN_DISALLOWED_BAN_MSG
// #define SMT_ENABLE_FEATURE_BAN_MSG // please DON'T enable this!
#endif
#define IRAINMAN_USE_HIDDEN_USERS // http://adc.sourceforge.net/ADC-EXT.html#_hidden_status_for_client_type
//#endif

#define IRAINMAN_USE_SPIN_LOCK

//#define FLYLINKDC_USE_VIEW_AS_TEXT_OPTION

#define USE_APPDATA

// [+] SSA - новый алгоритм поиска имени файла и title для WinAmp
#define SSA_NEW_WINAMP_PROC_FOR_TITLE_AND_FILENAME
// [+] SSA - отображать пусто в magnet, если не найден файл в шаре
// #define SSA_DONT_SHOW_MAGNET_ON_NO_FILE_IN_SHARE
#define SSA_IPGRANT_FEATURE // [+] SSA additional slots for special IP's
// TODO
//#define SSA_SHELL_INTEGRATION

#define SCALOLAZ_PROPPAGE_TRANSPARENCY
#define SCALOLAZ_CHAT_REFFERING_TO_NICK

#ifdef IRAINMAN_INCLUDE_SMILE
# define IRAINMAN_INCLUDE_GDI_OLE 1
#endif

#if defined(IRAINMAN_INCLUDE_GDI_OLE)
# define IRAINMAN_INCLUDE_GDI_INIT
#endif

#ifdef FLYLINKDC_BETA
#define FLYLINKDC_COLLECT_UNKNOWN_TAG
#ifdef _DEBUG
#define FLYLINKDC_COLLECT_UNKNOWN_FEATURES
#endif
#endif

#ifdef _DEBUG
#define DEBUG_USER_CONNECTION
#define LOCK_DEBUG
#endif

// TODO: remove it from release after testing
#define DEBUG_GDI_IMAGE

#define FLYLINKDC_SUPPORT_HUBTOPIC
#define FLYLINKDC_USE_DDOS_DETECT

#define FLYLINKDC_USE_EXT_JSON
#define FLYLINKDC_USE_SOCKET_COUNTER

#define HAVE_NATPMP_H

#if defined _POSIX_SOURCE || defined _GNU_SOURCE
#define HAVE_TIME_R
#endif

#define USE_QUEUE_RWLOCK

// Make sure we're using the templates from algorithm...
#ifdef min
# undef min
#endif
#ifdef max
# undef max
#endif

#endif // DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H
