#ifndef CONF_CORE_H_
#define CONF_CORE_H_

namespace Conf
{
	enum
	{
		INCOMING_DIRECT,
		INCOMING_FIREWALL_UPNP,
		INCOMING_FIREWALL_NAT,
		INCOMING_FIREWALL_PASSIVE
	};

	enum
	{
		OUTGOING_DIRECT,
		OUTGOING_SOCKS5
	};

	enum
	{
		BIND_OPTION_NO_FALLBACK = 1,
		BIND_OPTION_USE_DEV = 2
	};

	enum
	{
		TE_ACTION_ASK,
		TE_ACTION_REPLACE,
		TE_ACTION_RENAME,
		TE_ACTION_SKIP
	};

	enum
	{
		MEDIA_INFO_OPTION_ENABLE = 1,
		MEDIA_INFO_OPTION_SCAN_AUDIO = 2,
		MEDIA_INFO_OPTION_SCAN_VIDEO = 4
	};

	static const int MAX_SOCKET_BUFFER_SIZE = 64 * 1024;

	enum
	{
		CONFIG_VERSION = 1,

		// Language & encoding
		// strings
		LANGUAGE_FILE,
		DEFAULT_CODEPAGE,
		TIME_STAMPS_FORMAT,

		// User settings
		// strings
		NICK,
		PRIVATE_ID,
		UPLOAD_SPEED,
		DESCRIPTION,
		EMAIL,
		CLIENT_ID,
		DHT_KEY,
		// ints
		GENDER,
		OVERRIDE_CLIENT_ID,
		ADD_TO_DESCRIPTION,
		ADD_DESCRIPTION_SLOTS,
		ADD_DESCRIPTION_LIMIT,
		AUTO_CHANGE_NICK,

		// Network settings
		// strings
		BIND_ADDRESS,
		BIND_ADDRESS6,
		BIND_DEVICE,
		BIND_DEVICE6,
		EXTERNAL_IP,
		EXTERNAL_IP6,
		MAPPER,
		MAPPER6,
		SOCKS_SERVER,
		SOCKS_USER,
		SOCKS_PASSWORD,
		HTTP_PROXY,
		HTTP_USER_AGENT,
		// ints
		ENABLE_IP6,
		TCP_PORT,
		UDP_PORT, 
		TLS_PORT,
		USE_TLS,
		INCOMING_CONNECTIONS,
		INCOMING_CONNECTIONS6,
		OUTGOING_CONNECTIONS,
		AUTO_DETECT_CONNECTION,
		AUTO_DETECT_CONNECTION6,
		ALLOW_NAT_TRAVERSAL,
		IPUPDATE,
		WAN_IP_MANUAL,
		WAN_IP_MANUAL6,
		BIND_OPTIONS,
		BIND_OPTIONS6,
		IPUPDATE_INTERVAL, // Unused, visible in UI
		NO_IP_OVERRIDE,
		NO_IP_OVERRIDE6,
		AUTO_TEST_PORTS,
		SOCKS_PORT,
		SOCKS_RESOLVE,
		USE_DHT,
		USE_HTTP_PROXY,

		// Directories
		// strings
		DOWNLOAD_DIRECTORY, 
		TEMP_DOWNLOAD_DIRECTORY,

		// Sharing
		// strings
		SKIPLIST_SHARE,
		// ints
		AUTO_REFRESH_TIME,
		AUTO_REFRESH_ON_STARTUP,
		SHARE_HIDDEN,
		SHARE_SYSTEM,
		SHARE_VIRTUAL,
		MAX_HASH_SPEED,
		SAVE_TTH_IN_NTFS_FILESTREAM,
		SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM,
		FAST_HASH,
		MEDIA_INFO_OPTIONS,
		MEDIA_INFO_FORCE_UPDATE,

		// File lists
		// ints
		FILELIST_INCLUDE_UPLOAD_COUNT,
		FILELIST_INCLUDE_TIMESTAMP,

		// Uploads
		// strings
		COMPRESSED_FILES,

		// Slots
		// ints
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

		// Downloads & Queue
		// strings
		WANT_END_FILES,
		// ints
		DOWNLOAD_SLOTS,
		FILE_SLOTS,
		EXTRA_DOWNLOAD_SLOTS,
		MAX_DOWNLOAD_SPEED,
		BUFFER_SIZE_FOR_DOWNLOADS,
		ENABLE_MULTI_CHUNK,
		MIN_MULTI_CHUNK_SIZE, // Unused, visible in UI
		MAX_CHUNK_SIZE,
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
		DONT_DL_ALREADY_SHARED, // Unused
		KEEP_LISTS_DAYS,
		TARGET_EXISTS_ACTION,
		SKIP_EXISTING,
		COPY_EXISTING_MAX_SIZE,
		USE_MEMORY_MAPPED_FILES,
		AUTO_MATCH_DOWNLOADED_LISTS,

		// Throttling
		// ints
		THROTTLE_ENABLE,
		MAX_UPLOAD_SPEED_LIMIT_NORMAL,
		MAX_UPLOAD_SPEED_LIMIT_TIME,
		MAX_DOWNLOAD_SPEED_LIMIT_NORMAL,
		MAX_DOWNLOAD_SPEED_LIMIT_TIME,
		TIME_DEPENDENT_THROTTLE,
		BANDWIDTH_LIMIT_START,
		BANDWIDTH_LIMIT_END,
		PER_USER_UPLOAD_SPEED_LIMIT,

		// User checking
		// ints
		CHECK_USERS_NMDC,
		CHECK_USERS_ADC,
		USER_CHECK_BATCH,

		// Slow sources auto disconnect
		// ints
		ENABLE_AUTO_DISCONNECT,
		AUTO_DISCONNECT_SPEED,
		AUTO_DISCONNECT_FILE_SPEED,
		AUTO_DISCONNECT_TIME,
		AUTO_DISCONNECT_MIN_FILE_SIZE,
		AUTO_DISCONNECT_REMOVE_SPEED,
		AUTO_DISCONNECT_MULTISOURCE_ONLY,

		// Chat
		// ints
		SUPPRESS_MAIN_CHAT,
		IP_IN_CHAT,
		COUNTRY_IN_CHAT,
		ISP_IN_CHAT,

		// Private messages
		// strings
		PM_PASSWORD,
		PM_PASSWORD_HINT,
		PM_PASSWORD_OK_HINT,
		// ints
		PROTECT_PRIVATE,
		PROTECT_PRIVATE_RND,
		PROTECT_PRIVATE_SAY,
		PM_LOG_LINES,
		IGNORE_ME,
		SUPPRESS_PMS,
		LOG_IF_SUPPRESS_PMS,
		IGNORE_HUB_PMS,
		IGNORE_BOT_PMS,

		// Auto priority
		// strings
		AUTO_PRIORITY_PATTERNS,
		// ints
		AUTO_PRIORITY_USE_PATTERNS,
		AUTO_PRIORITY_PATTERNS_PRIO,
		AUTO_PRIORITY_USE_SIZE,
		AUTO_PRIORITY_SMALL_SIZE,
		AUTO_PRIORITY_SMALL_SIZE_PRIO,

		// Max finished items
		// ints
		MAX_FINISHED_DOWNLOADS,
		MAX_FINISHED_UPLOADS,

		// URLs
		// strings
		HUBLIST_SERVERS,
		URL_PORT_TEST,
		URL_GET_IP,
		URL_GET_IP6,
		URL_DHT_BOOTSTRAP,
		URL_GEOIP,

		// TLS settings
		// strings
		TLS_PRIVATE_KEY_FILE,
		TLS_CERTIFICATE_FILE,
		TLS_TRUSTED_CERTIFICATES_PATH,
		// ints
		ALLOW_UNTRUSTED_HUBS,
		ALLOW_UNTRUSTED_CLIENTS,

		// Protocol options
		// strings
		NMDC_FEATURES_CC,
		ADC_FEATURES_CC,
		// ints
		SOCKET_IN_BUFFER,
		SOCKET_OUT_BUFFER,
		COMPRESS_TRANSFERS,
		MAX_COMPRESSION,
		SEND_BLOOM,
		SEND_EXT_JSON,
		SEND_DB_PARAM,
		SEND_QP_PARAM,
		USE_SALT_PASS,
		USE_BOT_LIST,
		USE_MCTO,
		USE_CCPM,
		USE_CPMI,
		CCPM_AUTO_START,
		CCPM_IDLE_TIMEOUT,
		USE_TTH_LIST,
		USE_DI_PARAM,
		USE_SUDP,
		MAX_COMMAND_LENGTH, 
		HUB_USER_COMMANDS,
		MAX_HUB_USER_COMMANDS,
		MYINFO_DELAY,
		NMDC_ENCODING_FROM_DOMAIN,

		// Malicious IP detection
		// ints
		ENABLE_IPGUARD,
		ENABLE_P2P_GUARD,
		ENABLE_IPTRUST,
		IPGUARD_DEFAULT_DENY,
		P2P_GUARD_LOAD_INI,
		P2P_GUARD_BLOCK,

		// Anti-flood
		// ints
		ANTIFLOOD_MIN_REQ_COUNT,
		ANTIFLOOD_MAX_REQ_PER_MIN,
		ANTIFLOOD_BAN_TIME,

		// Search
		// ints
		SEARCH_PASSIVE,
		MIN_SEARCH_INTERVAL,
		MIN_SEARCH_INTERVAL_PASSIVE,
		INCOMING_SEARCH_TTH_ONLY,
		INCOMING_SEARCH_IGNORE_BOTS,
		INCOMING_SEARCH_IGNORE_PASSIVE,
		ADLS_BREAK_ON_FIRST,			

		// Away settings
		// strings
		DEFAULT_AWAY_MESSAGE, 
		SECONDARY_AWAY_MESSAGE,
		// ints
		AWAY,
		ENABLE_SECONDARY_AWAY,
		SECONDARY_AWAY_START,
		SECONDARY_AWAY_END,

		// Database
		// ints
		DB_LOG_FINISHED_DOWNLOADS,
		DB_LOG_FINISHED_UPLOADS,
		ENABLE_LAST_IP_AND_MESSAGE_COUNTER,
		ENABLE_RATIO_USER_LIST,
		ENABLE_UPLOAD_COUNTER,
		SQLITE_JOURNAL_MODE,
		DB_FINISHED_BATCH,
		GEOIP_AUTO_UPDATE,
		GEOIP_CHECK_HOURS,
		USE_CUSTOM_LOCATIONS,

		// Web server
		// strings
		WEBSERVER_BIND_ADDRESS,
		WEBSERVER_USER,
		WEBSERVER_PASS,
		WEBSERVER_POWER_USER,
		WEBSERVER_POWER_PASS,
		// ints
		ENABLE_WEBSERVER,
		WEBSERVER_PORT,

		// Message templates
		// strings
		ASK_SLOT_MESSAGE, // Unused
		RATIO_MESSAGE,
		WMLINK_TEMPLATE,

		// Hub frame
		HUB_FRAME_ORDER,
		HUB_FRAME_WIDTHS,
		HUB_FRAME_VISIBLE,
		FILTER_MESSAGES,

		// Logging
		// strings
		LOG_DIRECTORY,
		LOG_FILE_DOWNLOAD,
		LOG_FILE_UPLOAD, 
		LOG_FILE_MAIN_CHAT,
		LOG_FILE_PRIVATE_CHAT,
		LOG_FILE_STATUS,
		LOG_FILE_WEBSERVER,
		LOG_FILE_SYSTEM,
		LOG_FILE_SQLITE_TRACE,
		LOG_FILE_TORRENT_TRACE,
		LOG_FILE_SEARCH_TRACE,
		LOG_FILE_DHT_TRACE,
		LOG_FILE_PSR_TRACE,
		LOG_FILE_FLOOD_TRACE,
		LOG_FILE_TCP_MESSAGES,
		LOG_FILE_UDP_PACKETS,
		LOG_FILE_TLS_CERT,
		LOG_FORMAT_DOWNLOAD,
		LOG_FORMAT_UPLOAD,
		LOG_FORMAT_MAIN_CHAT,
		LOG_FORMAT_PRIVATE_CHAT,
		LOG_FORMAT_STATUS,
		LOG_FORMAT_WEBSERVER,
		LOG_FORMAT_SYSTEM,
		LOG_FORMAT_SQLITE_TRACE,
		LOG_FORMAT_TORRENT_TRACE,
		LOG_FORMAT_SEARCH_TRACE,
		LOG_FORMAT_DHT_TRACE,
		LOG_FORMAT_PSR_TRACE,
		LOG_FORMAT_FLOOD_TRACE,
		LOG_FORMAT_TCP_MESSAGES,
		LOG_FORMAT_UDP_PACKETS,
		// ints
		LOG_DOWNLOADS,
		LOG_UPLOADS,
		LOG_MAIN_CHAT,
		LOG_PRIVATE_CHAT,
		LOG_STATUS_MESSAGES,
		LOG_WEBSERVER,
		LOG_SYSTEM,
		LOG_SQLITE_TRACE,
		LOG_TORRENT_TRACE,
		LOG_SEARCH_TRACE,
		LOG_DHT_TRACE,
		LOG_PSR_TRACE,
		LOG_FLOOD_TRACE,
		LOG_FILELIST_TRANSFERS,
		LOG_TCP_MESSAGES,
		LOG_UDP_PACKETS,
		LOG_SOCKET_INFO,
		LOG_TLS_CERTIFICATES
	};

	void initCoreSettings();
	void updateCoreSettingsDefaults();
	void processCoreSettings();
}

#endif // CONF_CORE_H_
