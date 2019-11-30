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

#ifndef DCPLUSPLUS_CLIENT_SETTINGS_MANAGER_H
#define DCPLUSPLUS_CLIENT_SETTINGS_MANAGER_H

#include "Util.h"
#include "Speaker.h"
#include "Singleton.h"
#include "..\boost\boost\logic\tribool.hpp"

#define MAX_SOCKET_BUFFER_SIZE (64 * 1024)

STANDARD_EXCEPTION(SearchTypeException);

class SimpleXML;

class SettingsManagerListener
{
	public:
		virtual ~SettingsManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Load;
		typedef X<1> Repaint;
		
		virtual void on(Load, SimpleXML&) { }
		virtual void on(Repaint) { }
};

class SettingsManager : public Singleton<SettingsManager>, public Speaker<SettingsManagerListener>
{
	public:
		typedef boost::unordered_map<string, StringList> SearchTypes;
		typedef SearchTypes::iterator SearchTypesIter;
		
		static StringList g_connectionSpeeds;
		static boost::logic::tribool g_upnpUDPSearchLevel;
		static boost::logic::tribool g_upnpTorrentLevel;
		static boost::logic::tribool g_upnpTCPLevel;
		static boost::logic::tribool g_upnpTLSLevel;
		
		static void upnpPortLevelInit()
		{
			g_upnpUDPSearchLevel = boost::logic::indeterminate;
			g_upnpTorrentLevel = boost::logic::indeterminate;
			g_upnpTCPLevel = boost::logic::indeterminate;
			g_upnpTLSLevel = boost::logic::indeterminate;
		}
		
		static string g_UDPTestExternalIP;
		
		enum StrSetting
		{
			STR_FIRST,
			CONFIG_VERSION = STR_FIRST,

			// Language & encoding
			LANGUAGE_FILE,
			DEFAULT_CODEPAGE,
			TIME_STAMPS_FORMAT,
			
			// User settings
			NICK,
			PRIVATE_ID,
			UPLOAD_SPEED,
			DESCRIPTION,
			EMAIL,
			CLIENT_ID,
#ifdef FLYLINKDC_USE_LOCATION_DIALOG // TODO: Remove
			LOCATION_COUNTRY,
			LOCATION_CITY,
			LOCATION_ISP,
#endif

			// Network settings
			BIND_ADDRESS,
			EXTERNAL_IP,
			MAPPER,
			SOCKS_SERVER, SOCKS_USER, SOCKS_PASSWORD,
			HTTP_PROXY, // Unused

			// Directories
			DOWNLOAD_DIRECTORY, 
			TEMP_DOWNLOAD_DIRECTORY,
			DCLST_DIRECTORY,

			// Sharing
			SKIPLIST_SHARE,
		                  
			// Private messages
			PM_PASSWORD,
			PM_PASSWORD_HINT,
			PM_PASSWORD_OK_HINT,

			// Auto ban
			DONT_BAN_PATTERN,

			// Auto priority
			AUTO_PRIORITY_PATTERNS,

			// URLs
			HUBLIST_SERVERS,
			URL_GET_IP,
			URL_IPTRUST,

			// TLS settings
			TLS_PRIVATE_KEY_FILE, TLS_CERTIFICATE_FILE, TLS_TRUSTED_CERTIFICATES_PATH,

			// Message templates
			DEFAULT_AWAY_MESSAGE, 
			SECONDARY_AWAY_MESSAGE,
			BAN_MESSAGE,
			ASK_SLOT_MESSAGE, // Unused
			RATIO_MESSAGE,
			WMLINK_TEMPLATE,

			// Web server
			WEBSERVER_BIND_ADDRESS,			
			WEBSERVER_USER, WEBSERVER_PASS, 
			WEBSERVER_POWER_USER, WEBSERVER_POWER_PASS,

			// Logging
			LOG_DIRECTORY,
			LOG_FILE_DOWNLOAD,
			LOG_FILE_UPLOAD, 
			LOG_FILE_MAIN_CHAT,
			LOG_FILE_PRIVATE_CHAT,
			LOG_FILE_STATUS,
			LOG_FILE_WEBSERVER,
			LOG_FILE_CUSTOM_LOCATION,
			LOG_FILE_SYSTEM,
			LOG_FILE_SQLITE_TRACE,
			LOG_FILE_DDOS_TRACE,
			LOG_FILE_TORRENT_TRACE,
			LOG_FILE_PSR_TRACE,
			LOG_FILE_FLOOD_TRACE,
			LOG_FILE_TCP_MESSAGES,
			LOG_FILE_UDP_PACKETS,
			LOG_FORMAT_DOWNLOAD,
			LOG_FORMAT_UPLOAD,
			LOG_FORMAT_MAIN_CHAT,
			LOG_FORMAT_PRIVATE_CHAT,
			LOG_FORMAT_STATUS,
			LOG_FORMAT_WEBSERVER,
			LOG_FORMAT_CUSTOM_LOCATION,
			LOG_FORMAT_SYSTEM,
			LOG_FORMAT_SQLITE_TRACE,
			LOG_FORMAT_DDOS_TRACE,
			LOG_FORMAT_TORRENT_TRACE,
			LOG_FORMAT_PSR_TRACE,
			LOG_FORMAT_FLOOD_TRACE,
			LOG_FORMAT_TCP_MESSAGES,
			LOG_FORMAT_UDP_PACKETS,

			// User configurable commands
			RAW1_TEXT,
			RAW2_TEXT,
			RAW3_TEXT,
			RAW4_TEXT,
			RAW5_TEXT,
		                  
			// Players formats
			WINAMP_FORMAT, WMP_FORMAT, ITUNES_FORMAT, MPLAYERC_FORMAT, JETAUDIO_FORMAT, QCDQMP_FORMAT,

			// Font
			TEXT_FONT,
			
			// Toolbar settings
			TOOLBAR, TOOLBARIMAGE, TOOLBARHOTIMAGE,
			WINAMPTOOLBAR,
			
			// Popup settings
			POPUP_FONT,
			POPUP_TITLE_FONT,
			POPUP_IMAGE_FILE,

			// Sounds
			SOUND_BEEPFILE, SOUND_BEGINFILE, SOUND_FINISHFILE, SOUND_SOURCEFILE,
			SOUND_UPLOADFILE, SOUND_FAKERFILE, SOUND_CHATNAMEFILE, SOUND_TTH,
			SOUND_HUBCON, SOUND_HUBDISCON, SOUND_FAVUSER, SOUND_FAVUSER_OFFLINE,
			SOUND_TYPING_NOTIFY, SOUND_SEARCHSPY,

			// Themes and custom images
			USERLIST_IMAGE,
			THEME_MANAGER_THEME_DLL_NAME,
			THEME_MANAGER_SOUNDS_THEME_NAME,
			EMOTICONS_FILE,

			// Password
			PASSWORD,

			// Frames UI state
			TRANSFER_FRAME_ORDER, TRANSFER_FRAME_WIDTHS, TRANSFER_FRAME_VISIBLE,
			HUB_FRAME_ORDER, HUB_FRAME_WIDTHS, HUB_FRAME_VISIBLE,
			SEARCH_FRAME_ORDER, SEARCH_FRAME_WIDTHS, SEARCH_FRAME_VISIBLE,
			DIRLIST_FRAME_ORDER, DIRLIST_FRAME_WIDTHS, DIRLIST_FRAME_VISIBLE,
			FAVORITES_FRAME_ORDER, FAVORITES_FRAME_WIDTHS, FAVORITES_FRAME_VISIBLE,
			QUEUE_FRAME_ORDER, QUEUE_FRAME_WIDTHS, QUEUE_FRAME_VISIBLE,
			PUBLIC_HUBS_FRAME_ORDER, PUBLIC_HUBS_FRAME_WIDTHS, PUBLIC_HUBS_FRAME_VISIBLE,
			USERS_FRAME_ORDER, USERS_FRAME_WIDTHS, USERS_FRAME_VISIBLE,
			FINISHED_DL_FRAME_ORDER, FINISHED_DL_FRAME_WIDTHS, FINISHED_DL_FRAME_VISIBLE,
			FINISHED_UL_FRAME_WIDTHS, FINISHED_UL_FRAME_ORDER, FINISHED_UL_FRAME_VISIBLE,
			UPLOAD_QUEUE_FRAME_ORDER, UPLOAD_QUEUE_FRAME_WIDTHS, UPLOAD_QUEUE_FRAME_VISIBLE,
			RECENTS_FRAME_ORDER, RECENTS_FRAME_WIDTHS, RECENTS_FRAME_VISIBLE, 
			ADLSEARCH_FRAME_ORDER, ADLSEARCH_FRAME_WIDTHS, ADLSEARCH_FRAME_VISIBLE,
			SPY_FRAME_WIDTHS, SPY_FRAME_ORDER, SPY_FRAME_VISIBLE,

			// Recents
			SAVED_SEARCH_SIZE,
			KICK_MSG_RECENT_01, KICK_MSG_RECENT_02, KICK_MSG_RECENT_03, KICK_MSG_RECENT_04, KICK_MSG_RECENT_05,
			KICK_MSG_RECENT_06, KICK_MSG_RECENT_07, KICK_MSG_RECENT_08, KICK_MSG_RECENT_09, KICK_MSG_RECENT_10,
			KICK_MSG_RECENT_11, KICK_MSG_RECENT_12, KICK_MSG_RECENT_13, KICK_MSG_RECENT_14, KICK_MSG_RECENT_15,
			KICK_MSG_RECENT_16, KICK_MSG_RECENT_17, KICK_MSG_RECENT_18, KICK_MSG_RECENT_19, KICK_MSG_RECENT_20,
			
			STR_LAST
		};
		                
		enum IntSetting
		{
			INT_FIRST = STR_LAST + 1,
			
			// User settings
			GENDER = INT_FIRST,
			OVERRIDE_CLIENT_ID,
			ADD_TO_DESCRIPTION,
			ADD_DESCRIPTION_SLOTS,
			ADD_DESCRIPTION_LIMIT,			

			// Network settings (Ints)
			TCP_PORT,
			UDP_PORT, 
			TLS_PORT,
			USE_TLS,
			DHT_PORT,
			INCOMING_CONNECTIONS,
			AUTO_PASSIVE_INCOMING_CONNECTIONS,
			FORCE_PASSIVE_INCOMING_CONNECTIONS,
			OUTGOING_CONNECTIONS,
			AUTO_DETECT_CONNECTION,
#ifdef RIP_USE_CONNECTION_AUTODETECT
			INCOMING_AUTODETECT_FLAG,
#endif
			ALLOW_NAT_TRAVERSAL,
			IPUPDATE,
			WAN_IP_MANUAL,
			IPUPDATE_INTERVAL,
			NO_IP_OVERRIDE, // Unused, visible in UI
			SOCKS_PORT, SOCKS_RESOLVE,

			// Slots & policy
			SLOTS,
			AUTO_KICK,
			AUTO_KICK_NO_FAVS,
			MINISLOT_SIZE,
			EXTRA_SLOTS,
			HUB_SLOTS,
			EXTRA_SLOT_BY_IP,
			EXTRA_SLOT_TO_DL,
			EXTRA_PARTIAL_SLOTS,
			AUTO_SLOTS,
			AUTO_SLOT_MIN_UL_SPEED,
			SEND_SLOTGRANT_MSG,

			// Protocol options
			SOCKET_IN_BUFFER,
			SOCKET_OUT_BUFFER,
			COMPRESS_TRANSFERS,
			MAX_COMPRESSION,
			SEND_BLOOM,
			MAX_COMMAND_LENGTH, 
			HUB_USER_COMMANDS,
			PSR_DELAY, // Unused, visible in UI
			
			// Sharing (Ints)
			AUTO_REFRESH_TIME,
			SHARE_HIDDEN,
			SHARE_SYSTEM,
			SHARE_VIRTUAL,
			MAX_HASH_SPEED,
			SAVE_TTH_IN_NTFS_FILESTREAM,
			SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM,
			FAST_HASH,
			FILESHARE_INC_FILELIST,     // Unused, visible in UI
			FILESHARE_REINDEX_ON_START, // Unused, visible in UI
			FILELIST_INCLUDE_HIT,

			// Downloads & Queue
			DOWNLOAD_SLOTS,
			FILE_SLOTS,
			EXTRA_DOWNLOAD_SLOTS,
			MAX_DOWNLOAD_SPEED,
			BUFFER_SIZE_FOR_DOWNLOADS,
			ENABLE_MULTI_CHUNK,   // Unused, visible in UI
			MIN_MULTI_CHUNK_SIZE, // Unused, visible in UI
			OVERLAP_CHUNKS,
			DOWNCONN_PER_SEC,
			AUTO_SEARCH,
			AUTO_SEARCH_TIME,
			AUTO_SEARCH_LIMIT, // Unused, visible in UI
			AUTO_SEARCH_DL_LIST, 
			AUTO_SEARCH_MAX_SOURCES,
			REPORT_ALTERNATES,
			DONT_BEGIN_SEGMENT,
			DONT_BEGIN_SEGMENT_SPEED,
			SEGMENTS_MANUAL,
			NUMBER_OF_SEGMENTS,
			SKIP_ZERO_BYTE,
			SKIP_ALREADY_DOWNLOADED_FILES,
			DONT_DL_ALREADY_SHARED,
			DONT_DL_PREVIOUSLY_BEEN_IN_SHARE,
			KEEP_LISTS,
			TARGET_EXISTS_ACTION,
			NEVER_REPLACE_TARGET,

			// Slow sources auto disconnect
			ENABLE_AUTO_DISCONNECT,
			AUTO_DISCONNECT_SPEED,
			AUTO_DISCONNECT_FILE_SPEED,
			AUTO_DISCONNECT_TIME,
			AUTO_DISCONNECT_MIN_FILE_SIZE,
			AUTO_DISCONNECT_REMOVE_SPEED,
			AUTO_DISCONNECT_MULTISOURCE_ONLY,

			// Private messages (Ints)
			PROTECT_PRIVATE,
			PROTECT_PRIVATE_RND,
			PROTECT_PRIVATE_SAY,
			PM_LOG_LINES,
			IGNORE_ME,
			SUPPRESS_PMS,
			LOG_IF_SUPPRESS_PMS,
			IGNORE_HUB_PMS,
			IGNORE_BOT_PMS,
			
			// Max finished items
			MAX_FINISHED_DOWNLOADS,
			MAX_FINISHED_UPLOADS,

			// Throttling
			THROTTLE_ENABLE,
			MAX_UPLOAD_SPEED_LIMIT_NORMAL,
			MAX_UPLOAD_SPEED_LIMIT_TIME,
			MAX_DOWNLOAD_SPEED_LIMIT_NORMAL,
			MAX_DOWNLOAD_SPEED_LIMIT_TIME,
			TIME_DEPENDENT_THROTTLE,
			BANDWIDTH_LIMIT_START,
			BANDWIDTH_LIMIT_END,			
			
			// Auto ban (Ints)
			ENABLE_AUTO_BAN,
			AUTOBAN_FAKE_SHARE_PERCENT,
			AUTOBAN_MAX_DISCONNECTS,
			AUTOBAN_MAX_TIMEOUTS,
			AUTOBAN_SLOTS_MIN,
			AUTOBAN_SLOTS_MAX,
			AUTOBAN_SHARE,
			AUTOBAN_LIMIT,
			AUTOBAN_MSG_PERIOD,
			AUTOBAN_STEALTH,
			AUTOBAN_SEND_PM,
			AUTOBAN_CMD_DISCONNECTS,
			AUTOBAN_CMD_TIMEOUTS,
			AUTOBAN_CMD_FAKESHARE,
			AUTOBAN_FL_LEN_MISMATCH, // Unused, visible in UI
			AUTOBAN_FL_TOO_SMALL,    // Unused, visible in UI
			AUTOBAN_FL_UNAVAILABLE,
			DONT_BAN_FAVS,
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
			DONT_BAN_OP,
#endif
#ifdef IRAINMAN_INCLUDE_USER_CHECK
			CHECK_NEW_USERS,
#endif

			// Auto priority (Ints)
			AUTO_PRIORITY_USE_PATTERNS,
			AUTO_PRIORITY_PATTERNS_PRIO,
			AUTO_PRIORITY_USE_SIZE,
			AUTO_PRIORITY_SMALL_SIZE,
			AUTO_PRIORITY_SMALL_SIZE_PRIO,

			// Malicious IP detection
			ENABLE_IPGUARD,
			ENABLE_P2P_GUARD,
			ENABLE_IPTRUST,
			IPGUARD_DEFAULT_DENY,
			
			// Search
			SEARCH_PASSIVE,
			MIN_SEARCH_INTERVAL,
			MIN_SEARCH_INTERVAL_PASSIVE,
			
			// Away settings
			AWAY,
			AUTO_AWAY,
			ENABLE_SECONDARY_AWAY,
			SECONDARY_AWAY_START,
			SECONDARY_AWAY_END,
			
			// TLS settings (Ints)
			ALLOW_UNTRUSTED_HUBS,
			ALLOW_UNTRUSTED_CLIENTS,

			// Torrents
			USE_TORRENT_SEARCH,
			USE_TORRENT_RSS,

			// DNS lookup <Not implemented>
#ifdef FLYLINKDC_USE_DNS
			NSLOOKUP_MODE,
			NSLOOKUP_DELAY,
#endif
			
			// Database
			DB_LOG_FINISHED_DOWNLOADS,
			DB_LOG_FINISHED_UPLOADS,
			ENABLE_LAST_IP_AND_MESSAGE_COUNTER,
			ENABLE_RATIO_USER_LIST,
			SQLITE_USE_JOURNAL_MEMORY,

			// Web server (Ints)
			WEBSERVER,
			WEBSERVER_PORT,
			WEBSERVER_SEARCHSIZE,
			WEBSERVER_SEARCHPAGESIZE,
			WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR,
			WEBSERVER_ALLOW_UPNP,		
			
			// Logging (Ints)
			LOG_DOWNLOADS,
			LOG_UPLOADS,
			LOG_MAIN_CHAT,
			LOG_PRIVATE_CHAT,
			LOG_STATUS_MESSAGES,
			LOG_WEBSERVER,
			LOG_CUSTOM_LOCATION,
			LOG_SYSTEM,
			LOG_SQLITE_TRACE,
			LOG_DDOS_TRACE,
			LOG_TORRENT_TRACE,
			LOG_PSR_TRACE,
			LOG_FLOOD_TRACE,
			LOG_FILELIST_TRANSFERS,
			LOG_TCP_MESSAGES,
			LOG_UDP_PACKETS,

			// Startup & shutdown
			STARTUP_BACKUP,
			SHUTDOWN_ACTION,
			SHUTDOWN_TIMEOUT,
			REGISTER_URL_HANDLER,
			REGISTER_MAGNET_HANDLER,
			REGISTER_DCLST_HANDLER,

			// Colors & text styles
			BACKGROUND_COLOR,
			TEXT_COLOR,
			ERROR_COLOR,
			TEXT_GENERAL_BACK_COLOR, TEXT_GENERAL_FORE_COLOR, TEXT_GENERAL_BOLD, TEXT_GENERAL_ITALIC,
			TEXT_MYOWN_BACK_COLOR, TEXT_MYOWN_FORE_COLOR, TEXT_MYOWN_BOLD, TEXT_MYOWN_ITALIC,
			TEXT_PRIVATE_BACK_COLOR, TEXT_PRIVATE_FORE_COLOR, TEXT_PRIVATE_BOLD, TEXT_PRIVATE_ITALIC,
			TEXT_SYSTEM_BACK_COLOR, TEXT_SYSTEM_FORE_COLOR, TEXT_SYSTEM_BOLD, TEXT_SYSTEM_ITALIC,
			TEXT_SERVER_BACK_COLOR, TEXT_SERVER_FORE_COLOR, TEXT_SERVER_BOLD, TEXT_SERVER_ITALIC,
			TEXT_TIMESTAMP_BACK_COLOR, TEXT_TIMESTAMP_FORE_COLOR, TEXT_TIMESTAMP_BOLD, TEXT_TIMESTAMP_ITALIC,
			TEXT_MYNICK_BACK_COLOR, TEXT_MYNICK_FORE_COLOR, TEXT_MYNICK_BOLD, TEXT_MYNICK_ITALIC,
			TEXT_FAV_BACK_COLOR, TEXT_FAV_FORE_COLOR, TEXT_FAV_BOLD, TEXT_FAV_ITALIC,
			TEXT_OP_BACK_COLOR, TEXT_OP_FORE_COLOR, TEXT_OP_BOLD, TEXT_OP_ITALIC,
			TEXT_URL_BACK_COLOR, TEXT_URL_FORE_COLOR, TEXT_URL_BOLD, TEXT_URL_ITALIC,
			TEXT_ENEMY_BACK_COLOR, TEXT_ENEMY_FORE_COLOR, TEXT_ENEMY_BOLD, TEXT_ENEMY_ITALIC,

			// User list colors
			RESERVED_SLOT_COLOR,
			IGNORED_COLOR,
			FAVORITE_COLOR,
			NORMAL_COLOR,
			FIREBALL_COLOR,
			SERVER_COLOR,
			PASSIVE_COLOR,
			OP_COLOR,
			CHECKED_COLOR,
			BAD_CLIENT_COLOR,
			BAD_FILELIST_COLOR, 

			// Other colors
			DOWNLOAD_BAR_COLOR,
			UPLOAD_BAR_COLOR, 
			PROGRESS_BACK_COLOR,
			PROGRESS_COMPRESS_COLOR,
			PROGRESS_SEGMENT_COLOR,
			COLOR_RUNNING,
			COLOR_RUNNING_COMPLETED,
			COLOR_DOWNLOADED,
			BAN_COLOR,
			DUPE_COLOR, 
#ifdef SCALOLAZ_USE_COLOR_HUB_IN_FAV
			HUB_IN_FAV_BK_COLOR,
			HUB_IN_FAV_CONNECT_BK_COLOR,
#endif

			// Assorted UI settings (Ints)
			SHOW_GRIDLINES,
			SHOW_INFOTIPS,
			USE_SYSTEM_ICONS,
			USE_EXPLORER_THEME,
			USE_12_HOUR_FORMAT,
			MDI_MAXIMIZED,
			TOGGLE_ACTIVE_WINDOW,
			POPUNDER_PM,
			POPUNDER_FILELIST,

			// Tab settings
			TABS_POS, MAX_TAB_ROWS, TAB_SIZE, TABS_SHOW_INFOTIPS,
			TABS_CLOSEBUTTONS, TABS_BOLD, NON_HUBS_FRONT,			
			BOLD_FINISHED_DOWNLOADS, BOLD_FINISHED_UPLOADS, BOLD_QUEUE,
			BOLD_HUB, BOLD_PM, BOLD_SEARCH, BOLD_WAITING_USERS,

			// Toolbar settings (Ints)
			LOCK_TOOLBARS,
			TB_IMAGE_SIZE,
			TB_IMAGE_SIZE_HOT,
			SHOW_PLAYER_CONTROLS,
			
			// Menu settings
			MENUBAR_TWO_COLORS, MENUBAR_LEFT_COLOR, MENUBAR_RIGHT_COLOR, MENUBAR_BUMPED,
			UC_SUBMENU,

			// Progressbar settings
			SHOW_PROGRESS_BARS,
			PROGRESS_TEXT_COLOR_DOWN, PROGRESS_TEXT_COLOR_UP, 
			PROGRESS_OVERRIDE_COLORS, PROGRESS_3DDEPTH, PROGRESS_OVERRIDE_COLORS2,
			PROGRESSBAR_ODC_STYLE, PROGRESSBAR_ODC_BUMPED,
			STEALTHY_STYLE, STEALTHY_STYLE_ICO, STEALTHY_STYLE_ICO_SPEEDIGNORE,
			TOP_DL_SPEED,
			TOP_UL_SPEED,
			UL_COLOR_DEPENDS_ON_SLOTS,

			// Popup settings (Ints)
			POPUPS_DISABLED,
			POPUP_TYPE,
			POPUP_TIME,
			POPUP_WIDTH,
			POPUP_HEIGHT,
			POPUP_TRANSPARENCY,
			POPUP_MAX_LENGTH,
			POPUP_BACKCOLOR,
			POPUP_TEXTCOLOR,
			POPUP_TITLE_TEXTCOLOR,
			POPUP_IMAGE,
			POPUP_COLORS, 
			POPUP_ON_HUB_CONNECTED, POPUP_ON_HUB_DISCONNECTED,
			POPUP_ON_FAVORITE_CONNECTED, POPUP_ON_FAVORITE_DISCONNECTED,
			POPUP_ON_CHEATING_USER, POPUP_ON_CHAT_LINE,
			POPUP_ON_DOWNLOAD_STARTED, POPUP_ON_DOWNLOAD_FAILED, POPUP_ON_DOWNLOAD_FINISHED,
			POPUP_ON_UPLOAD_FINISHED,
			POPUP_ON_PM, POPUP_ON_NEW_PM,
			POPUP_ON_SEARCH_SPY,
			POPUP_ON_FOLDER_SHARED,
			POPUP_PM_PREVIEW,
			POPUP_ONLY_WHEN_AWAY,
			POPUP_ONLY_WHEN_MINIMIZED, 

			// Sound settings (Int)
			SOUNDS_DISABLED,
			PRIVATE_MESSAGE_BEEP,
			PRIVATE_MESSAGE_BEEP_OPEN,

			// Open on startup
			OPEN_RECENT_HUBS, OPEN_PUBLIC_HUBS, OPEN_FAVORITE_HUBS, OPEN_FAVORITE_USERS,
			OPEN_QUEUE, OPEN_FINISHED_DOWNLOADS, OPEN_FINISHED_UPLOADS,
			OPEN_SEARCH_SPY, OPEN_NETWORK_STATISTICS, OPEN_NOTEPAD,
			OPEN_WAITING_USERS, OPEN_CDMDEBUG,
		
			// Click actions
			USERLIST_DBLCLICK,
			TRANSFERLIST_DBLCLICK,
			CHAT_DBLCLICK,
			FAVUSERLIST_DBLCLICK,
			MAGNET_ASK,
			MAGNET_ACTION,
			DCLST_ASK,
			DCLST_ACTION,

			// Window behavior
			TOPMOST,
			MINIMIZE_TRAY,
			MINIMIZE_ON_STARTUP, 
			MINIMIZE_ON_CLOSE,
			SHOW_CURRENT_SPEED_IN_TITLE,

			// Confirmations
			CONFIRM_EXIT,
			CONFIRM_DELETE,
			CONFIRM_HUB_REMOVAL,
			CONFIRM_HUBGROUP_REMOVAL,
			CONFIRM_USER_REMOVAL, 			
			CONFIRM_SHARE_FROM_SHELL,
	
			// Password (Ints)
			PROTECT_TRAY, PROTECT_START, PROTECT_CLOSE,
			
			// ADLSearch
			ADLS_BREAK_ON_FIRST,			

			// Media player
			MEDIA_PLAYER,
			USE_MAGNETS_IN_PLAYERS_SPAM,
			USE_BITRATE_FIX_FOR_SPAM,
			
			// Other settings, mostly useless
			REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY,
			MIN_MEDIAINFO_SIZE,
			DCLST_CREATE_IN_SAME_FOLDER,
			DCLST_INCLUDESELF,
			REPORT_TO_USER_IF_OUTDATED_OS_DETECTED,
			
			// View visibility
			SHOW_STATUSBAR, SHOW_TOOLBAR, SHOW_TRANSFERVIEW, SHOW_TRANSFERVIEW_TOOLBAR, SHOW_QUICK_SEARCH,
			
			// Frames UI state (Ints)
			TRANSFER_FRAME_SORT, TRANSFER_FRAME_SPLIT,
			HUB_FRAME_SORT,
			SEARCH_FRAME_SORT,
			DIRLIST_FRAME_SORT, DIRLIST_FRAME_SPLIT,
			FAVORITES_FRAME_SORT,
			QUEUE_FRAME_SORT, QUEUE_FRAME_SPLIT,
			PUBLIC_HUBS_FRAME_SORT,
			USERS_FRAME_SORT, USERS_FRAME_SPLIT,
			FINISHED_DL_FRAME_SORT,
			FINISHED_UL_FRAME_SORT,
			UPLOAD_QUEUE_FRAME_SORT, UPLOAD_QUEUE_FRAME_SPLIT,
			RECENTS_FRAME_SORT,
			ADLSEARCH_FRAME_SORT,
			SPY_FRAME_SORT,

			// Hub frame
			AUTO_FOLLOW,
			SHOW_JOINS,
			FAV_SHOW_JOINS,
			PROMPT_HUB_PASSWORD,
			FILTER_MESSAGES,
#ifdef SCALOLAZ_HUB_MODE
			ENABLE_HUBMODE_PIC,
#endif
			ENABLE_COUNTRY_FLAG,
#ifdef IRAINMAN_INCLUDE_USER_CHECK
			SHOW_SHARE_CHECKED_USERS,
#endif
			HUB_POSITION,
			SORT_FAVUSERS_FIRST,
			FILTER_ENTER,
			JOIN_OPEN_NEW_WINDOW,
			SHOW_FULL_HUB_INFO_ON_TAB,
			USER_THERSHOLD,
			STRIP_TOPIC,
			POPUP_PMS_HUB,
			POPUP_PMS_BOT,
			POPUP_PMS_OTHER,
			
			// Chat frame
			CHAT_BUFFER_SIZE,
			CHAT_TIME_STAMPS,
			BOLD_MSG_AUTHOR,
			CHAT_PANEL_SHOW_INFOTIPS,
			SHOW_EMOTICONS_BTN,
			CHAT_ANIM_SMILES,
			SMILE_SELECT_WND_ANIM_SMILES,
			SHOW_SEND_MESSAGE_BUTTON,
#ifdef IRAINMAN_USE_BB_CODES
			FORMAT_BB_CODES,
			FORMAT_BB_CODES_COLORS,
#endif
			SHOW_BBCODE_PANEL,
			SHOW_MULTI_CHAT_BTN,
#ifdef SCALOLAZ_CHAT_REFFERING_TO_NICK
			CHAT_REFFERING_TO_NICK,
#endif
			IP_IN_CHAT,
			COUNTRY_IN_CHAT,
			ISP_IN_CHAT, 
			STATUS_IN_CHAT,
			DISPLAY_CHEATS_IN_MAIN_CHAT,
			USE_CTRL_FOR_LINE_HISTORY,
			MULTILINE_CHAT_INPUT,
			MULTILINE_CHAT_INPUT_BY_CTRL_ENTER,
			FORMAT_BOT_MESSAGE,
			SEND_UNKNOWN_COMMANDS,
			SUPPRESS_MAIN_CHAT,
			
			// Search frame
			SAVE_SEARCH_SETTINGS,
			FORGET_SEARCH_REQUEST,
			SEARCH_HISTORY,
			CLEAR_SEARCH,
			USE_SEARCH_GROUP_TREE_SETTINGS,
			SEARCH_DETECT_TTH, // Unused
			ONLY_FREE_SLOTS,

			// Queue frame
			QUEUE_FRAME_SHOW_TREE,
			
			// Upload queue frame
			UPLOAD_QUEUE_FRAME_SHOW_TREE,

			// Finished frame
			SHOW_SHELL_MENU,
			
			// Search Spy frame
			SPY_FRAME_IGNORE_TTH_SEARCHES,
			SHOW_SEEKERS_IN_SPY_FRAME,
			LOG_SEEKERS_IN_SPY_FRAME,

			// Settings dialog
			REMEMBER_SETTINGS_PAGE,
			SETTINGS_PAGE,
			SETTINGS_WINDOW_TRANSP,
			SETTINGS_WINDOW_COLORIZE,
			USE_OLD_SHARING_UI,

			// Main window size & position
			MAIN_WINDOW_STATE,
			MAIN_WINDOW_SIZE_X, MAIN_WINDOW_SIZE_Y, MAIN_WINDOW_POS_X, MAIN_WINDOW_POS_Y,

			// Recents (Ints)
			SAVED_SEARCH_TYPE, SAVED_SEARCH_SIZEMODE, SAVED_SEARCH_MODE,

			INT_LAST,
			SETTINGS_LAST = INT_LAST
		};
		                
		enum
		{
			INCOMING_DIRECT, INCOMING_FIREWALL_UPNP, INCOMING_FIREWALL_NAT,
			INCOMING_FIREWALL_PASSIVE
		};

		enum { OUTGOING_DIRECT, OUTGOING_SOCKS5 };
		
		enum { MAGNET_AUTO_SEARCH, MAGNET_AUTO_DOWNLOAD, MAGNET_AUTO_DOWNLOAD_AND_OPEN };

		enum { ON_DOWNLOAD_ASK, /*FlylinkDC Team TODO ON_DOWNLOAD_EXIST_FILE_TO_NEW_DEST,*/  ON_DOWNLOAD_REPLACE, ON_DOWNLOAD_RENAME, ON_DOWNLOAD_SKIP };
		
		enum { POS_LEFT, POS_RIGHT };
		
		enum { TABS_TOP, TABS_BOTTOM };
		
		enum PlayerSelected
		{
			WinAmp,
			WinMediaPlayer,
			iTunes,
			WinMediaPlayerClassic,
			JetAudio,
			QCDQMP,
			PlayersCount,
			UnknownPlayer = PlayersCount
		};
		
		static const string& get(StrSetting key, const bool useDefault = true);
		static int get(IntSetting key, const bool useDefault = true);
		static bool getBool(IntSetting key, const bool useDefault = true)
		{
			return get(key, useDefault) != 0;
		}
		// [!] IRainman all set function return status: true is value automatically corrected, or false if not.
		static bool set(StrSetting key, const string& value);
		static bool set(IntSetting key, int value);
		static bool set(IntSetting key, const std::string& value);
		static bool set(IntSetting key, bool value)
		{
			return set(key, int(value));
		}
		
		static bool isDefault(int aSet)
		{
			return !isSet[aSet];
		}
		
		static bool LoadLanguage();

		static void importDcTheme(const tstring& file, const bool asDefault = false);
		static void exportDcTheme(const tstring& file);

		static void unset(size_t key)
		{
			isSet[key] = false;
		}

		void load()
		{
			Util::migrate(getConfigFile());
			load(getConfigFile());
		}

		void save()
		{
			save(getConfigFile());
		}
		
		void load(const string& aFileName);
		void save(const string& aFileName);
		void setDefaults();
		void loadOtherSettings();
		static void generateNewTCPPort();
		static void generateNewUDPPort();
		
		// Search types
		static void validateSearchTypeName(const string& name);
		
		void setSearchTypeDefaults();
		void addSearchType(const string& name, const StringList& extensions, bool validated = false);
		void delSearchType(const string& name);
		void renameSearchType(const string& oldName, const string& newName);
		void modSearchType(const string& name, const StringList& extensions);
		
		static const SearchTypes& getSearchTypes()
		{
			return g_searchTypes;
		}
		static const StringList& getExtensions(const string& name);
		
		static int getNewPortValue(int oldPortValue);
		static string getSoundFilename(const SettingsManager::StrSetting sound);
		static bool getBeepEnabled(const SettingsManager::IntSetting sound);
		static bool getPopupEnabled(const SettingsManager::IntSetting popup);

	private:
		friend class Singleton<SettingsManager>;
		SettingsManager();
	
		static string strSettings[STR_LAST - STR_FIRST];
		static int    intSettings[INT_LAST - INT_FIRST];
		static string strDefaults[STR_LAST - STR_FIRST];
		static int    intDefaults[INT_LAST - INT_FIRST];
		static bool isSet[SETTINGS_LAST];

		// Search types
		static SearchTypes g_searchTypes; // name, extlist
		
		static SearchTypesIter getSearchType(const string& name);
		
		static string getConfigFile()
		{
			return Util::getConfigPath() + "DCPlusPlus.xml";
		}

		static void setDefault(StrSetting key, const string& value)
		{
			strDefaults[key - STR_FIRST] = value;
		}
		
		static void setDefault(IntSetting key, int value)
		{
			intDefaults[key - INT_FIRST] = value;
		}
		
		static void setDefault(IntSetting key, const string& value)
		{
			intDefaults[key - INT_FIRST] = Util::toInt(value);
		}		
};

// Shorthand accessor macros
#define SETTING(k) SettingsManager::get(SettingsManager::k, true)
#define BOOLSETTING(k) SettingsManager::getBool(SettingsManager::k, true)

#define SET_SETTING(k, v) SettingsManager::set(SettingsManager::k, v)
#define SOUND_SETTING(k) SettingsManager::getSoundFilename(SettingsManager::k)
#define SOUND_BEEP_BOOLSETTING(k) SettingsManager::getBeepEnabled(SettingsManager::k)
#define POPUP_ENABLED(k) SettingsManager::getPopupEnabled(SettingsManager::k)

#define SPLIT_SETTING_AND_LOWER(key) Util::splitSettingAndLower(SETTING(key))

#endif // !defined(SETTINGS_MANAGER_H)
