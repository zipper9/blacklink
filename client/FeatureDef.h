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

#define USE_QUEUE_RWLOCK

#endif // FEATURE_DEF_H_
