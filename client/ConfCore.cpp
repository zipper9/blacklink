#include "stdinc.h"
#include <boost/algorithm/string.hpp>

#include "ConfCore.h"
#include "SettingsManager.h"
#include "ResourceManager.h"
#include "LogManager.h"
#include "StrUtil.h"
#include "Util.h"
#include "AppPaths.h"
#include "Path.h"
#include "Random.h"
#include "Base32.h"
#include "CID.h"

static const string DEFAULT_LANG_FILE = "en-US.xml";

static const char URL_GET_IP_DEFAULT[]  = "http://checkip.dyndns.com";
static const char URL_GET_IP6_DEFAULT[] = "http://checkipv6.dynu.com";
static const char URL_GEOIP_DEFAULT[]   = "http://geoip.airdcpp.net";

static const char HUBLIST_SERVERS_DEFAULT[] =
	"https://www.te-home.net/?do=hublist&get=hublist.xml.bz2;"
	"https://dcnf.github.io/Hublist/hublist.xml.bz2;"
	"https://dchublist.biz/?do=hublist&get=hublist.xml.bz2;"
	"https://dchublist.org/hublist.xml.bz2;"
	"https://dchublist.ru/hublist.xml.bz2;"
	"https://dchublists.com/?do=hublist&get=hublist.xml.bz2";

static const char COMPRESSED_FILES_DEFAULT[] = "*.bz2;*.zip;*.rar;*.7z;*.gz;*.mp3;*.ogg;*.flac;*.ape;*.mp4;*.mkv;*.jpg;*.jpeg;*.gif;*.png;*.docx;*.xlsx";
static const char WANT_END_FILES_DEFAULT[] = "*.mov;*.mp4;*.3gp;*.3g2";

static BaseSettingsImpl::MinMaxValidator<int> validatePos(1, INT_MAX);
static BaseSettingsImpl::MinMaxValidator<int> validateNonNeg(0, INT_MAX);
static BaseSettingsImpl::MinMaxValidator<int> validateBufSize(64, 8192);
static BaseSettingsImpl::MinMaxValidator<int> validateIncoming(Conf::INCOMING_DIRECT, Conf::INCOMING_FIREWALL_PASSIVE);
static BaseSettingsImpl::MinMaxValidator<int> validateGender(0, 4);
static BaseSettingsImpl::MinMaxValidator<int> validateSlots(0, 500);
static BaseSettingsImpl::MinMaxValidator<int> validateDownloadSlots(0, 100);
static BaseSettingsImpl::MinMaxValidator<int> validateMinislotSize(16, INT_MAX);
static BaseSettingsImpl::MinMaxValidator<int> validateSegments(1, 200);
static BaseSettingsImpl::MinMaxValidator<int> validateUserCheckBatch(5, 50);
static BaseSettingsImpl::MinMaxValidator<int> validateSqliteJournalMode(0, 3);
static BaseSettingsImpl::MinMaxValidator<int> validateDbFinishedBatch(0, 2000);
static BaseSettingsImpl::MinMaxValidator<int> validatePort(1, 65535);
static BaseSettingsImpl::MinMaxValidatorWithZero<int> validateListeningPort(1024, 65535);
static BaseSettingsImpl::MinMaxValidator<int> validateHighPort(1024, 65535);
static BaseSettingsImpl::MinMaxValidator<int> validateHour(0, 23);
static BaseSettingsImpl::MinMaxValidator<int> validateSockBuf(0, Conf::MAX_SOCKET_BUFFER_SIZE);
static BaseSettingsImpl::MinMaxValidatorWithZero<int> validateMaxChunkSize(64*1024, INT_MAX);
static BaseSettingsImpl::MinMaxValidator<int> validateAutoSearchTime(1, 60);
static BaseSettingsImpl::MinMaxValidatorWithDef<int> validateSearchInterval(2, 120, 10);
static BaseSettingsImpl::MinMaxValidator<int> validateMyInfoDelay(0, 180);
static BaseSettingsImpl::MinMaxValidatorWithZero<int> validateSpeedLimit(32, INT_MAX);
static BaseSettingsImpl::MinMaxValidator<int> validatePerUserLimit(0, 10240);
static BaseSettingsImpl::MinMaxValidator<int> validateKeepListDays(0, 9999);
static BaseSettingsImpl::MinMaxValidator<int> validateLogLines(0, 999);
static BaseSettingsImpl::MinMaxValidator<int> validateCCPMIdleTimeout(0, 30);
static BaseSettingsImpl::MinMaxValidatorWithZero<int> validateIpUpdateInterval(30, 86400);
static BaseSettingsImpl::StringSizeValidator validateNick(49);
static BaseSettingsImpl::StringSizeValidator validateDescription(100);
static BaseSettingsImpl::StringSizeValidator validateEmail(64);

struct LogFileTlsCertValidator : public BaseSettingsImpl::Validator<string>
{
	bool checkValue(const string& val, int flags) const noexcept override
	{
		return val.find("%[kp]") != string::npos || val.find("%[KP]") != string::npos;
	}
	void fixValue(string& val) const noexcept override
	{
		if (!checkValue(val, 0)) val.clear();
	}
};

static LogFileTlsCertValidator validateLogFileTlsCert;

struct NoSpaceValidator : public BaseSettingsImpl::Validator<string>
{
	bool checkValue(const string& val, int flags) const noexcept override
	{
		return val.find(' ') == string::npos;
	}
	void fixValue(string& val) const noexcept override
	{
		val.erase(std::remove(val.begin(), val.end(), ' '), val.end());
	}
};

static NoSpaceValidator noSpaceValidator;

struct TrimSpaceValidator : public BaseSettingsImpl::Validator<string>
{
	bool checkValue(const string& val, int flags) const noexcept override
	{
		if (val.empty()) return true;
		return !Util::isWhiteSpace(val[0]) && !Util::isWhiteSpace(val.back());
	}
	void fixValue(string& val) const noexcept override
	{
		boost::algorithm::trim(val);
	}
};

static TrimSpaceValidator trimSpaceValidator;

#if 0
struct CIDValidator : public BaseSettingsImpl::Validator<string>
{
		bool checkValue(const string& val, int flags) const noexcept override
		{
			if (val.length() != 39) return false;
			CID cid;
			bool error;
			Util::fromBase32(val.c_str(), cid.writableData(), CID::SIZE, &error);
			return !error && !cid.isZero();
		}
		void fixValue(string& val) const noexcept override
		{
			if (!checkValue(val, 0))
			{
				CID cid;
				cid.regenerate();
				val = cid.toBase32();
			}
		}
};

static CIDValidator validateCID;
#endif

void Conf::initCoreSettings()
{
	auto s = SettingsManager::instance.getCoreSettings();

	// Language & encoding
	s->addString(LANGUAGE_FILE, "LanguageFile", DEFAULT_LANG_FILE);
	s->addString(DEFAULT_CODEPAGE, "DefaultCodepage");
	s->addString(TIME_STAMPS_FORMAT, "TimeStampsFormat", "%X");

	// User settings
	s->addString(NICK, "Nick", Util::emptyString, 0, &validateNick);
	s->addString(PRIVATE_ID, "CID");
	s->addString(UPLOAD_SPEED, "UploadSpeed", "50");
	s->addString(DESCRIPTION, "Description", Util::emptyString, 0, &validateDescription);
	s->addString(EMAIL, "EMail", Util::emptyString, 0, &validateEmail);
	s->addString(CLIENT_ID, "ClientID");
	s->addString(DHT_KEY, "DHTKey");
	s->addInt(GENDER, "Gender", 0, 0, &validateGender);
	s->addBool(OVERRIDE_CLIENT_ID, "OverrideClientID");
	s->addBool(ADD_TO_DESCRIPTION, "ExtDescription");
	s->addBool(ADD_DESCRIPTION_SLOTS, "ExtDescriptionSlots");
	s->addBool(ADD_DESCRIPTION_LIMIT, "ExtDescriptionLimit");
	s->addBool(AUTO_CHANGE_NICK, "AutoChangeNick");

	// Network settings
	s->addString(BIND_ADDRESS, "BindAddress", "0.0.0.0", Settings::FLAG_FIX_VALUE, &trimSpaceValidator);
	s->addString(BIND_ADDRESS6, "BindAddress6", Util::emptyString, Settings::FLAG_FIX_VALUE, &trimSpaceValidator);
	s->addString(BIND_DEVICE, "BindDevice");
	s->addString(BIND_DEVICE6, "BindDevice6");
	s->addString(EXTERNAL_IP, "ExternalIp");
	s->addString(EXTERNAL_IP6, "ExternalIp6");
	s->addString(MAPPER, "Mapper");
	s->addString(MAPPER6, "Mapper6");
	s->addString(SOCKS_SERVER, "SocksServer");
	s->addString(SOCKS_USER, "SocksUser");
	s->addString(SOCKS_PASSWORD, "SocksPassword");
	s->addString(HTTP_PROXY, "HttpProxy");
	s->addString(HTTP_USER_AGENT, "HttpUserAgent");
	s->addBool(ENABLE_IP6, "EnableIP6");
	s->addInt(TCP_PORT, "InPort", 0, 0, &validateListeningPort);
	s->addInt(UDP_PORT, "UDPPort", 0, 0, &validateListeningPort);
	s->addInt(TLS_PORT, "TLSPort", 0, 0, &validateListeningPort);
	s->addBool(USE_TLS, "UseTLS", true);
	s->addInt(INCOMING_CONNECTIONS, "IncomingConnections", INCOMING_FIREWALL_UPNP, 0, &validateIncoming);
	s->addInt(INCOMING_CONNECTIONS6, "IncomingConnections6", INCOMING_DIRECT, 0, &validateIncoming);
	s->addInt(OUTGOING_CONNECTIONS, "OutgoingConnections", OUTGOING_DIRECT);
	s->addBool(AUTO_DETECT_CONNECTION, "AutoDetectIncomingConnection", true);
	s->addBool(AUTO_DETECT_CONNECTION6, "AutoDetectIncomingConnection6", true);
	s->addBool(ALLOW_NAT_TRAVERSAL, "AllowNATTraversal", true);
	s->addBool(IPUPDATE, "AutoUpdateIP");
	s->addBool(WAN_IP_MANUAL, "WANIPManual");
	s->addBool(WAN_IP_MANUAL6, "WANIPManual6");
	s->addInt(BIND_OPTIONS, "BindOptions");
	s->addInt(BIND_OPTIONS6, "BindOptions6");
	s->addInt(IPUPDATE_INTERVAL, "AutoUpdateIPInterval", 0, 0, &validateIpUpdateInterval);
	s->addBool(NO_IP_OVERRIDE, "NoIPOverride");
	s->addBool(NO_IP_OVERRIDE6, "NoIPOverride6");
	s->addBool(AUTO_TEST_PORTS, "AutoTestPorts", true);
	s->addInt(SOCKS_PORT, "SocksPort", 1080, 0, &validatePort);
	s->addBool(SOCKS_RESOLVE, "SocksResolve", true);
	s->addBool(USE_DHT, "UseDHT");
	s->addBool(USE_HTTP_PROXY, "UseHTTPProxy");

	// Directories
	s->addString(DOWNLOAD_DIRECTORY, "DownloadDirectory", Util::getDownloadsPath());
	s->addString(TEMP_DOWNLOAD_DIRECTORY, "TempDownloadDirectory");

	// Sharing
	s->addString(SKIPLIST_SHARE, "SkiplistShare", "*.dctmp;*.!ut", Settings::FLAG_FIX_VALUE, &noSpaceValidator);
	s->addInt(AUTO_REFRESH_TIME, "AutoRefreshTime", 60);
	s->addBool(AUTO_REFRESH_ON_STARTUP, "AutoRefreshOnStartup", true);
	s->addBool(SHARE_HIDDEN, "ShareHidden");
	s->addBool(SHARE_SYSTEM, "ShareSystem");
	s->addBool(SHARE_VIRTUAL, "ShareVirtual", true);
	s->addInt(MAX_HASH_SPEED, "MaxHashSpeed");
	s->addBool(SAVE_TTH_IN_NTFS_FILESTREAM, "SaveTthInNtfsFilestream", true);
	s->addInt(SET_MIN_LENGTH_TTH_IN_NTFS_FILESTREAM, "SetMinLengthTthInNtfsFilestream", 16);
	s->addBool(FAST_HASH, "FastHash", true);
	s->addInt(MEDIA_INFO_OPTIONS, "MediaInfoOptions");
	s->addBool(MEDIA_INFO_FORCE_UPDATE, "MediaInfoForceUpdate");

	// File lists
	s->addBool(FILELIST_INCLUDE_UPLOAD_COUNT, "FileListUseUploadCount", true);
	s->addBool(FILELIST_INCLUDE_TIMESTAMP, "FileListUseTS", true);

	// Uploads
	s->addString(COMPRESSED_FILES, "CompressedFiles", COMPRESSED_FILES_DEFAULT);

	// Slots
	s->addInt(SLOTS, "Slots", 15, 0, &validateSlots);
	s->addBool(AUTO_KICK, "AutoKick");
	s->addBool(AUTO_KICK_NO_FAVS, "AutoKickNoFavs");
	s->addInt(MINISLOT_SIZE, "MinislotSize", 64, 0, &validateMinislotSize);
	s->addInt(EXTRA_SLOTS, "ExtraSlots", 5, 0, &validateNonNeg);
	s->addInt(HUB_SLOTS, "HubSlots", 1, 0, &validateNonNeg);
	s->addBool(EXTRA_SLOT_BY_IP, "ExtraSlotByIP");
	s->addBool(EXTRA_SLOT_TO_DL, "ExtraSlotToDl", true);
	s->addInt(EXTRA_PARTIAL_SLOTS, "ExtraPartialSlots", 1, 0, &validateNonNeg);
	s->addInt(AUTO_SLOTS, "AutoSlot", 5, 0, &validateNonNeg);
	s->addInt(AUTO_SLOT_MIN_UL_SPEED, "AutoSlotMinULSpeed");
	s->addBool(SEND_SLOTGRANT_MSG, "SendSlotGrantMsg");

	// Downloads & Queue
	s->addString(WANT_END_FILES, "WantEndFiles", WANT_END_FILES_DEFAULT, Settings::FLAG_FIX_VALUE, &noSpaceValidator);
	s->addInt(DOWNLOAD_SLOTS, "DownloadSlots", 0, 0, &validateDownloadSlots);
	s->addInt(FILE_SLOTS, "FileSlots", 0, 0, &validateDownloadSlots);
	s->addInt(EXTRA_DOWNLOAD_SLOTS, "ExtraDownloadSlots", 3, 0, &validateDownloadSlots);
	s->addInt(MAX_DOWNLOAD_SPEED, "MaxDownloadSpeed");
	s->addInt(BUFFER_SIZE_FOR_DOWNLOADS, "BufferSizeForDownloads", 1024, 0, &validateBufSize);
	s->addBool(ENABLE_MULTI_CHUNK, "EnableMultiChunk", true);
	s->addInt(MIN_MULTI_CHUNK_SIZE, "MinMultiChunkSize", 2);
	s->addInt(MAX_CHUNK_SIZE, "MaxChunkSize", 0, 0, &validateMaxChunkSize);
	s->addBool(OVERLAP_CHUNKS, "OverlapChunks", true);
	s->addInt(DOWNCONN_PER_SEC, "DownConnPerSec", 2);
	s->addBool(AUTO_SEARCH, "AutoSearch", true);
	s->addInt(AUTO_SEARCH_TIME, "AutoSearchTime", 1, 0, &validateAutoSearchTime);
	s->addInt(AUTO_SEARCH_LIMIT, "AutoSearchLimit", 15, 0, &validatePos);
	s->addBool(AUTO_SEARCH_DL_LIST, "AutoSearchAutoMatch");
	s->addInt(AUTO_SEARCH_MAX_SOURCES, "AutoSearchMaxSources", 5);
	s->addBool(REPORT_ALTERNATES, "ReportFoundAlternates", true);
	s->addBool(DONT_BEGIN_SEGMENT, "DontBeginSegment");
	s->addInt(DONT_BEGIN_SEGMENT_SPEED, "DontBeginSegmentSpeed", 1024);
	s->addBool(SEGMENTS_MANUAL, "SegmentsManual", true);
	s->addInt(NUMBER_OF_SEGMENTS, "NumberOfSegments", 50, 0, &validateSegments);
	s->addBool(SKIP_ZERO_BYTE, "SkipZeroByte");
	s->addBool(DONT_DL_ALREADY_SHARED, "DontDownloadAlreadyShared");
	s->addInt(KEEP_LISTS_DAYS, "KeepListsDays", 30, 0, &validateKeepListDays);
	s->addInt(TARGET_EXISTS_ACTION, "TargetExistsAction", TE_ACTION_ASK);
	s->addBool(SKIP_EXISTING, "SkipExisting", true);
	s->addInt(COPY_EXISTING_MAX_SIZE, "CopyExistingMaxSize", 100);
	s->addBool(USE_MEMORY_MAPPED_FILES, "UseMemoryMappedFiles", true);
	s->addBool(AUTO_MATCH_DOWNLOADED_LISTS, "AutoMatchDownloadedLists", true);

	// Throttling
	s->addBool(THROTTLE_ENABLE, "ThrottleEnable");
	s->addInt(MAX_UPLOAD_SPEED_LIMIT_NORMAL, "UploadLimitNormal", 0, 0, &validateSpeedLimit);
	s->addInt(MAX_UPLOAD_SPEED_LIMIT_TIME, "UploadLimitTime", 0, 0, &validateSpeedLimit);
	s->addInt(MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, "DownloadLimitNormal", 0, 0, &validateSpeedLimit);
	s->addInt(MAX_DOWNLOAD_SPEED_LIMIT_TIME, "DownloadLimitTime", 0, 0, &validateSpeedLimit);
	s->addBool(TIME_DEPENDENT_THROTTLE, "TimeThrottle");
	s->addInt(BANDWIDTH_LIMIT_START, "TimeLimitStart", 0, 0, &validateHour);
	s->addInt(BANDWIDTH_LIMIT_END, "TimeLimitEnd", 0, 0, &validateHour);
	s->addInt(PER_USER_UPLOAD_SPEED_LIMIT, "PerUserUploadLimit", 0, 0, &validatePerUserLimit);

	// User checking
	s->addBool(CHECK_USERS_NMDC, "CheckUsersNmdc");
	s->addBool(CHECK_USERS_ADC, "CheckUsersAdc");
	s->addInt(USER_CHECK_BATCH, "UserCheckBatch", 5, 0, &validateUserCheckBatch);

	// Slow sources auto disconnect
	s->addBool(ENABLE_AUTO_DISCONNECT, "AutoDisconnectEnable");
	s->addInt(AUTO_DISCONNECT_SPEED, "AutoDisconnectSpeed", 5);
	s->addInt(AUTO_DISCONNECT_FILE_SPEED, "AutoDisconnectFileSpeed", 10);
	s->addInt(AUTO_DISCONNECT_TIME, "AutoDisconnectTime", 20);
	s->addInt(AUTO_DISCONNECT_MIN_FILE_SIZE, "AutoDisconnectMinFileSize", 10);
	s->addInt(AUTO_DISCONNECT_REMOVE_SPEED, "AutoDisconnectRemoveSpeed", 2);
	s->addBool(AUTO_DISCONNECT_MULTISOURCE_ONLY, "DropMultiSourceOnly", true);

	// Chat
	s->addBool(SUPPRESS_MAIN_CHAT, "SuppressMainChat");
	s->addBool(IP_IN_CHAT, "IpInChat");
	s->addBool(COUNTRY_IN_CHAT, "CountryInChat");
	s->addBool(ISP_IN_CHAT, "ISPInChat");

	// Private messages
	s->addString(PM_PASSWORD, "Password");
	s->addString(PM_PASSWORD_HINT, "PasswordHint");
	s->addString(PM_PASSWORD_OK_HINT, "PasswordOkHint");
	s->addBool(PROTECT_PRIVATE, "ProtectPrivate");
	s->addBool(PROTECT_PRIVATE_RND, "ProtectPrivateRnd", true);
	s->addBool(PROTECT_PRIVATE_SAY, "ProtectPrivateSay");
	s->addInt(PM_LOG_LINES, "PMLogLines", 50, 0, &validateLogLines);
	s->addBool(IGNORE_ME, "IgnoreMe");
	s->addBool(SUPPRESS_PMS, "SuppressPms");
	s->addBool(LOG_IF_SUPPRESS_PMS, "LogIfSuppressPms", true);
	s->addBool(IGNORE_HUB_PMS, "IgnoreHubPms");
	s->addBool(IGNORE_BOT_PMS, "IgnoreBotPms");

	// Auto priority
	s->addString(AUTO_PRIORITY_PATTERNS, "AutoPriorityPatterns", "*.sfv;*.nfo;*sample*;*cover*;*.pls;*.m3u", Settings::FLAG_FIX_VALUE, &noSpaceValidator);
	s->addBool(AUTO_PRIORITY_USE_PATTERNS, "AutoPriorityUsePatterns", true);
	s->addInt(AUTO_PRIORITY_PATTERNS_PRIO, "AutoPriorityPatternsPrio", 6); // Higher
	s->addBool(AUTO_PRIORITY_USE_SIZE, "AutoPriorityUseSize", true);
	s->addInt(AUTO_PRIORITY_SMALL_SIZE, "AutoPrioritySmallSize", 64);
	s->addInt(AUTO_PRIORITY_SMALL_SIZE_PRIO, "AutoPrioritySmallSizePrio", 6); // Higher

	// Max finished items
	s->addInt(MAX_FINISHED_DOWNLOADS, "MaxFinishedDownloads", 1000, 0, &validateNonNeg);
	s->addInt(MAX_FINISHED_UPLOADS, "MaxFinishedUploads", 1000, 0, &validateNonNeg);

	// URLs
	s->addString(HUBLIST_SERVERS, "HublistServers", HUBLIST_SERVERS_DEFAULT);
	s->addString(URL_PORT_TEST, "UrlPortTest", "http://media.fly-server.ru:37015/fly-test-port", Settings::FLAG_FIX_VALUE, &trimSpaceValidator);
	s->addString(URL_GET_IP, "UrlGetIp", URL_GET_IP_DEFAULT, Settings::FLAG_FIX_VALUE, &trimSpaceValidator);
	s->addString(URL_GET_IP6, "UrlGetIp6", URL_GET_IP6_DEFAULT, Settings::FLAG_FIX_VALUE, &trimSpaceValidator);
	s->addString(URL_DHT_BOOTSTRAP, "UrlDHTBootstrap", "http://strongdc.sourceforge.net/bootstrap/", Settings::FLAG_FIX_VALUE, &trimSpaceValidator);
	s->addString(URL_GEOIP, "UrlGeoIP", URL_GEOIP_DEFAULT, Settings::FLAG_FIX_VALUE, &trimSpaceValidator);

	// TLS settings
	const string tlsPath = Util::getConfigPath() + "Certificates" PATH_SEPARATOR_STR;
	s->addString(TLS_PRIVATE_KEY_FILE, "TLSPrivateKeyFile", tlsPath + "client.key", Settings::FLAG_CONVERT_PATH);
	s->addString(TLS_CERTIFICATE_FILE, "TLSCertificateFile", tlsPath + "client.crt", Settings::FLAG_CONVERT_PATH);
	s->addString(TLS_TRUSTED_CERTIFICATES_PATH, "TLSTrustedCertificatesPath", tlsPath, Settings::FLAG_CONVERT_PATH);
	s->addBool(ALLOW_UNTRUSTED_HUBS, "AllowUntrustedHubs", true);
	s->addBool(ALLOW_UNTRUSTED_CLIENTS, "AllowUntrustedClients", true);

	// Protocol options
	s->addString(NMDC_FEATURES_CC, "NMDCFeaturesCC");
	s->addString(ADC_FEATURES_CC, "ADCFeaturesCC");
	s->addInt(SOCKET_IN_BUFFER, "SocketInBuffer2", MAX_SOCKET_BUFFER_SIZE, 0, &validateSockBuf);
	s->addInt(SOCKET_OUT_BUFFER, "SocketOutBuffer2", MAX_SOCKET_BUFFER_SIZE, 0, &validateSockBuf);
	s->addBool(COMPRESS_TRANSFERS, "CompressTransfers", true);
	s->addInt(MAX_COMPRESSION, "MaxCompression", 9);
	s->addBool(SEND_BLOOM, "SendBloom", true);
	s->addBool(SEND_EXT_JSON, "SendExtJSON", true);
	s->addBool(SEND_DB_PARAM, "SendDBParam", true);
	s->addBool(SEND_QP_PARAM, "SendQPParam", true);
	s->addBool(USE_SALT_PASS, "UseSaltPass", true);
	s->addBool(USE_BOT_LIST, "UseBotList", true);
	s->addBool(USE_MCTO, "UseMCTo");
	s->addBool(USE_CCPM, "UseCCPM", true);
	s->addBool(USE_CPMI, "UseCPMI", true);
	s->addBool(CCPM_AUTO_START, "CCPMAutoStart");
	s->addInt(CCPM_IDLE_TIMEOUT, "CCPMIdleTimeout", 10, 0, &validateCCPMIdleTimeout);
	s->addBool(USE_TTH_LIST, "UseTL", true);
	s->addBool(USE_DI_PARAM, "UseDIParam");
	s->addBool(USE_SUDP, "UseSUDP");
	s->addInt(MAX_COMMAND_LENGTH, "MaxCommandLength", 16 * 1024 * 1024);
	s->addBool(HUB_USER_COMMANDS, "HubUserCommands", true);
	s->addInt(MAX_HUB_USER_COMMANDS, "MaxHubUserCommands", 500, 0, &validateNonNeg);
	s->addInt(MYINFO_DELAY, "MyInfoDelay", 35, 0, &validateMyInfoDelay);
	s->addBool(NMDC_ENCODING_FROM_DOMAIN, "NMDCEncodingFromDomain", true);

	// Malicious IP detection
	s->addBool(ENABLE_IPGUARD, "EnableIpGuard");
	s->addBool(ENABLE_P2P_GUARD, "EnableP2PGuard");
	s->addBool(ENABLE_IPTRUST, "EnableIpTrust", true);
	s->addBool(IPGUARD_DEFAULT_DENY, "IpGuardDefaultDeny");
	s->addBool(P2P_GUARD_LOAD_INI, "P2PGuardLoadIni");
	s->addBool(P2P_GUARD_BLOCK, "P2PGuardBlock");

	// Anti-flood
	s->addInt(ANTIFLOOD_MIN_REQ_COUNT, "AntiFloodMinReqCount", 15);
	s->addInt(ANTIFLOOD_MAX_REQ_PER_MIN, "AntiFloodMaxReqPerMin", 12);
	s->addInt(ANTIFLOOD_BAN_TIME, "AntiFloodBanTime", 3600);

	// Search
	s->addBool(SEARCH_PASSIVE, "SearchPassiveAlways");
	s->addInt(MIN_SEARCH_INTERVAL, "MinimumSearchInterval", 10, 0, &validateSearchInterval);
	s->addInt(MIN_SEARCH_INTERVAL_PASSIVE, "MinimumSearchIntervalPassive", 10, 0, &validateSearchInterval);
	s->addBool(INCOMING_SEARCH_TTH_ONLY, "IncomingSearchTTHOnly");
	s->addBool(INCOMING_SEARCH_IGNORE_BOTS, "IncomingSearchIgnoreBots");
	s->addBool(INCOMING_SEARCH_IGNORE_PASSIVE, "IncomingSearchIgnorePassive");
	s->addBool(ADLS_BREAK_ON_FIRST, "AdlsBreakOnFirst");

	// Away settings
	s->addString(DEFAULT_AWAY_MESSAGE, "DefaultAwayMessage");
	s->addString(SECONDARY_AWAY_MESSAGE, "SecondaryAwayMsg");
	s->addBool(AWAY, "Away");
	s->addBool(ENABLE_SECONDARY_AWAY, "AwayThrottle");
	s->addInt(SECONDARY_AWAY_START, "AwayStart");
	s->addInt(SECONDARY_AWAY_END, "AwayEnd");

	// Database
	s->addInt(DB_LOG_FINISHED_DOWNLOADS, "DbLogFinishedDownloads", 365, 0, &validateNonNeg);
	s->addInt(DB_LOG_FINISHED_UPLOADS, "DbLogFinishedUploads", 365, 0, &validateNonNeg);
	s->addBool(ENABLE_LAST_IP_AND_MESSAGE_COUNTER, "EnableLastIP", true);
	s->addBool(ENABLE_RATIO_USER_LIST, "EnableRatioUserList", true);
	s->addBool(ENABLE_UPLOAD_COUNTER, "EnableUploadCounter", true);
	s->addInt(SQLITE_JOURNAL_MODE, "SQLiteJournalMode", 0, 0, &validateSqliteJournalMode);
	s->addInt(DB_FINISHED_BATCH, "DbFinishedBatch", 300, 0, &validateDbFinishedBatch);
	s->addBool(GEOIP_AUTO_UPDATE, "GeoIPAutoUpdate", true);
	s->addInt(GEOIP_CHECK_HOURS, "GeoIPCheckHours", 30, 0, &validatePos);
	s->addBool(USE_CUSTOM_LOCATIONS, "UseCustomLocations", true);

	// Web server
	s->addString(WEBSERVER_BIND_ADDRESS, "WebServerBindAddress", "0.0.0.0", Settings::FLAG_FIX_VALUE, &trimSpaceValidator);
	s->addString(WEBSERVER_USER, "WebServerUser", "user");
	s->addString(WEBSERVER_PASS, "WebServerPass");
	s->addString(WEBSERVER_POWER_USER, "WebServerPowerUser", "admin");
	s->addString(WEBSERVER_POWER_PASS, "WebServerPowerPass");
	s->addBool(ENABLE_WEBSERVER, "WebServer");
	s->addInt(WEBSERVER_PORT, "WebServerPort", 0, 0, &validateHighPort);

	// Message templates
	s->addString(ASK_SLOT_MESSAGE, "SlotAskMessage");
	s->addString(RATIO_MESSAGE, "RatioTemplate", "/me ratio: %[ratio] (Uploaded: %[up] | Downloaded: %[down])");
	s->addString(WMLINK_TEMPLATE, "WebMagnetTemplate", "<a href=\"%[magnet]\" title=\"%[name]\" target=\"_blank\">%[name] (%[size])</a>");

	// Hub frame
	s->addString(HUB_FRAME_ORDER, "HubFrameOrder");
	s->addString(HUB_FRAME_WIDTHS, "HubFrameWidths");
	s->addString(HUB_FRAME_VISIBLE, "HubFrameVisible", "1,1,0,1,1,1,1,1,1,1,1,1,1,1");
	s->addBool(FILTER_MESSAGES, "FilterMessages", true);

	// Logging
	const string logDir = Util::getLocalPath() + "Logs" PATH_SEPARATOR_STR;
	const string simpleLogFormat = "[%Y-%m-%d %H:%M:%S] %[message]";
	const string chatLogFormat = "[%Y-%m-%d %H:%M:%S%[extra]] %[message]";
	s->addString(LOG_DIRECTORY, "LogDir", logDir, Settings::FLAG_CONVERT_PATH);	
	s->addString(LOG_FILE_DOWNLOAD, "LogFileDownload", "Downloads.log");
	s->addString(LOG_FILE_UPLOAD, "LogFileUpload", "Uploads.log");
	s->addString(LOG_FILE_MAIN_CHAT, "LogFileMainChat", "%Y-%m" PATH_SEPARATOR_STR "%[hubURL].log");
	s->addString(LOG_FILE_PRIVATE_CHAT, "LogFilePrivateChat", "PM" PATH_SEPARATOR_STR "%Y-%m" PATH_SEPARATOR_STR "%[userNI]-%[hubURL].log");
	s->addString(LOG_FILE_STATUS, "LogFileStatus", "%Y-%m" PATH_SEPARATOR_STR  "%[hubURL]_status.log");
	s->addString(LOG_FILE_WEBSERVER, "LogFileWebServer", "WebServer.log");
	s->addString(LOG_FILE_SYSTEM, "LogFileSystem", "System.log");
	s->addString(LOG_FILE_SQLITE_TRACE, "LogFileSQLiteTrace", "SQLTrace.log");
	s->addString(LOG_FILE_TORRENT_TRACE, "LogFileTorrentTrace", "torrent.log");
	s->addString(LOG_FILE_SEARCH_TRACE, "LogFileSearchTrace", "Found.log");
	s->addString(LOG_FILE_DHT_TRACE, "LogFileDHTTrace", "DHT.log");
	s->addString(LOG_FILE_PSR_TRACE, "LogFilePSRTrace", "PSR.log");
	s->addString(LOG_FILE_FLOOD_TRACE, "LogFileFloodTrace", "flood.log");
	s->addString(LOG_FILE_TCP_MESSAGES, "LogFileCMDDebugTrace", "Trace" PATH_SEPARATOR_STR "%[ip]" PATH_SEPARATOR_STR "%[ipPort].log");
	s->addString(LOG_FILE_UDP_PACKETS, "LogFileUDPDebugTrace", "Trace" PATH_SEPARATOR_STR "UDP-Packets.log");
	s->addString(LOG_FILE_TLS_CERT, "LogFileTLSCert", "Trace" PATH_SEPARATOR_STR "%[ip]" PATH_SEPARATOR_STR "%[kp].cer", 0, &validateLogFileTlsCert);
	s->addString(LOG_FORMAT_DOWNLOAD, "LogFormatPostDownload", "%Y-%m-%d %H:%M:%S: %[target] %[@DownloadedFrom] %[userNI] (%[userCID]), %[fileSI] (%[fileSIchunk]), %[speed], %[time]");
	s->addString(LOG_FORMAT_UPLOAD, "LogFormatPostUpload", "%Y-%m-%d %H:%M:%S %[source] %[@UploadedTo] %[userNI] (%[userCID]), %[fileSI] (%[fileSIchunk]), %[speed], %[time]");
	s->addString(LOG_FORMAT_MAIN_CHAT, "LogFormatMainChat", chatLogFormat);
	s->addString(LOG_FORMAT_PRIVATE_CHAT, "LogFormatPrivateChat", chatLogFormat);
	s->addString(LOG_FORMAT_STATUS, "LogFormatStatus", simpleLogFormat);
	s->addString(LOG_FORMAT_WEBSERVER, "LogFormatWebServer", simpleLogFormat);
	s->addString(LOG_FORMAT_SYSTEM, "LogFormatSystem" ,simpleLogFormat);
	s->addString(LOG_FORMAT_SQLITE_TRACE, "LogFormatSQLiteTrace", "[%Y-%m-%d %H:%M:%S] (%[thread_id]) %[sql]");
	s->addString(LOG_FORMAT_TORRENT_TRACE, "LogFormatTorrentTrace", simpleLogFormat);
	s->addString(LOG_FORMAT_SEARCH_TRACE, "LogFormatSearchTrace", simpleLogFormat);
	s->addString(LOG_FORMAT_DHT_TRACE, "LogFormatDHTTrace", simpleLogFormat);
	s->addString(LOG_FORMAT_PSR_TRACE, "LogFormatPSRTrace", simpleLogFormat);
	s->addString(LOG_FORMAT_FLOOD_TRACE, "LogFormatFloodTrace", simpleLogFormat);
	s->addString(LOG_FORMAT_TCP_MESSAGES, "LogFormatCMDDebugTrace", simpleLogFormat);
	s->addString(LOG_FORMAT_UDP_PACKETS, "LogFormatUDPDebugTrace", simpleLogFormat);
	s->addBool(LOG_DOWNLOADS, "LogDownloads");
	s->addBool(LOG_UPLOADS, "LogUploads");
	s->addBool(LOG_MAIN_CHAT, "LogMainChat", true);
	s->addBool(LOG_PRIVATE_CHAT, "LogPrivateChat", true);
	s->addBool(LOG_STATUS_MESSAGES, "LogStatusMessages");
	s->addBool(LOG_WEBSERVER, "WebServerLog");
	s->addBool(LOG_SYSTEM, "LogSystem");
	s->addBool(LOG_SQLITE_TRACE, "LogSQLiteTrace");
	s->addBool(LOG_TORRENT_TRACE, "LogTorrentTrace");
	s->addBool(LOG_SEARCH_TRACE, "LogSearchTrace");
	s->addBool(LOG_DHT_TRACE, "LogDHTTrace");
	s->addBool(LOG_PSR_TRACE, "LogPSRTrace");
	s->addBool(LOG_FLOOD_TRACE, "LogFloodTrace");
	s->addBool(LOG_FILELIST_TRANSFERS, "LogFilelistTransfers");
	s->addBool(LOG_TCP_MESSAGES, "LogCMDDebugTrace");
	s->addBool(LOG_UDP_PACKETS, "LogUDPDebugTrace");
	s->addBool(LOG_SOCKET_INFO, "LogSocketInfo");
	s->addBool(LOG_TLS_CERTIFICATES, "LogTLSCertificates");
}

void Conf::updateCoreSettingsDefaults()
{
	auto s = SettingsManager::instance.getCoreSettings();
	s->setStringDefault(DEFAULT_AWAY_MESSAGE, STRING(DEFAULT_AWAY_MESSAGE));
	s->setStringDefault(ASK_SLOT_MESSAGE, STRING(ASK_SLOT_TEMPLATE));
	s->setStringDefault(PM_PASSWORD, Util::getRandomNick());
	s->setStringDefault(PM_PASSWORD_HINT, STRING(DEF_PASSWORD_HINT));
	s->setStringDefault(PM_PASSWORD_OK_HINT, STRING(DEF_PASSWORD_OK_HINT));
}

static bool parseCIDString(const string& val, CID& cid)
{
	if (val.length() != 39) return false;
	bool error;
	Util::fromBase32(val.c_str(), cid.writableData(), CID::SIZE, &error);
	return !error && !cid.isZero();
}

static int generateRandomPort(const int* exclude, int excludeCount)
{
	int value;
	for (;;)
	{
		value = Util::rand(49152, 65536);
		bool excluded = false;
		for (int i = 0; i < excludeCount && !excluded; ++i)
			if (value == exclude[i])
				excluded = true;
		if (!excluded) break;
	}
	return value;
}

void Conf::processCoreSettings()
{
	auto s = SettingsManager::instance.getCoreSettings();
	s->lockWrite();

	int excludePorts[3];
	int excludeCount = 0;

	int portTCP = s->getInt(TCP_PORT);
	if (portTCP) excludePorts[excludeCount++] = portTCP;

	int portTLS = s->getInt(TLS_PORT);
	if (portTLS) excludePorts[excludeCount++] = portTLS;

	int portWeb = s->getInt(WEBSERVER_PORT);
	if (portWeb) excludePorts[excludeCount++] = portWeb;
	
	if (!portTCP)
	{
		portTCP = generateRandomPort(excludePorts, excludeCount);
		s->setInt(TCP_PORT, portTCP);
		excludePorts[excludeCount++] = portTCP;
	}
	if (!portTLS)
	{
		portTLS = generateRandomPort(excludePorts, excludeCount);
		s->setInt(TLS_PORT, portTLS);
		excludePorts[excludeCount++] = portTLS;
	}
	if (!portWeb)
	{
		portWeb = generateRandomPort(excludePorts, excludeCount);
		s->setInt(WEBSERVER_PORT, portWeb);
	}
	if (!s->getInt(UDP_PORT))
	{
		int port = generateRandomPort(excludePorts, 0);
		s->setInt(UDP_PORT, port);
	}

	CID cid;
	if (!parseCIDString(s->getString(PRIVATE_ID), cid))
	{
		cid.regenerate();
		s->setString(PRIVATE_ID, cid.toBase32());
	}
	if (!parseCIDString(s->getString(DHT_KEY), cid))
	{
		cid.regenerate();
		s->setString(DHT_KEY, cid.toBase32());
	}

	if (s->getString(WEBSERVER_PASS).empty())
		s->setString(WEBSERVER_PASS, Util::getRandomPassword());
	if (s->getString(WEBSERVER_POWER_PASS).empty())
		s->setString(WEBSERVER_POWER_PASS, Util::getRandomPassword());

	if (s->getBool(Conf::PROTECT_PRIVATE_RND))
		s->setString(Conf::PM_PASSWORD, Util::getRandomNick());

#ifdef _WIN32
	static const string templatePMFolder = "PM\\%B - %Y\\";
	const string& val = s->getString(Conf::LOG_FILE_PRIVATE_CHAT);
	if (val.find(templatePMFolder) != string::npos)
	{
		string newVal = val;
		boost::replace_all(newVal, templatePMFolder, "PM\\%Y-%m\\");
		s->setString(Conf::LOG_FILE_PRIVATE_CHAT, newVal);
	}
#endif

	s->unlockWrite();
	LogManager::updateSettings();
}
