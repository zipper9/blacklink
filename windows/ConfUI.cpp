#include "stdafx.h"
#include "ConfUI.h"
#include "SettingsManager.h"
#include "AppPaths.h"
#include "SysVersion.h"
#include "KnownClients.h"
#include "../client/Text.h"
#include "../client/ConfCore.h"

#ifndef CW_USEDEFAULT
#define CW_USEDEFAULT ((int) 0x80000000)
#endif

#ifndef SW_SHOWNORMAL
#define SW_SHOWNORMAL 1
#endif

#ifndef RGB
#define RGB(r, g, b) ((uint32_t)((r) & 0xFF) | (uint32_t)(((g) & 0xFF)<<8) | (uint32_t)(((b) & 0xFF)<<16))
#endif

static BaseSettingsImpl::MinMaxValidator<int> validatePos(1, INT_MAX);
static BaseSettingsImpl::MinMaxValidator<int> validateNonNeg(0, INT_MAX);
static BaseSettingsImpl::MinMaxValidator<int> validateTabRows(1, 20);
static BaseSettingsImpl::MinMaxValidator<int> validateTabSize(7, 80);
static BaseSettingsImpl::MinMaxValidator<int> validateSearchHistory(10, 80);
static BaseSettingsImpl::MinMaxValidator<int> validatePopupTime(1, 15);
static BaseSettingsImpl::MinMaxValidator<int> validatePopupMaxLength(3, 512);
static BaseSettingsImpl::MinMaxValidator<int> validatePopupWidth(80, 599);
static BaseSettingsImpl::MinMaxValidator<int> validatePopupHeight(50, 299);
static BaseSettingsImpl::MinMaxValidator<int> validatePopupTransparency(50, 255);
static BaseSettingsImpl::MinMaxValidator<int> validateMediaPlayer(0, Conf::NumPlayers-1);
static BaseSettingsImpl::MinMaxValidator<int> validateSplitter(80, 8000);
static BaseSettingsImpl::MinMaxValidator<int> validateShutdownAction(0, 5);
static BaseSettingsImpl::MinMaxValidator<int> validateShutdownTimeout(1, 3600);

static const int toolbarSizes[] = { 24, 16 };
static BaseSettingsImpl::ListValidator<int> validateToolbarSize(toolbarSizes, _countof(toolbarSizes));

void Conf::initUiSettings()
{
	auto s = SettingsManager::instance.getUiSettings();

	// Startup & shutdown
	bool registerHandlers = !Util::isLocalMode();
	s->addBool(STARTUP_BACKUP, "StartupBackup");
	s->addInt(SHUTDOWN_ACTION, "ShutdownAction", 0, 0, &validateShutdownAction);
	s->addInt(SHUTDOWN_TIMEOUT, "ShutdownTimeout", 150, 0, &validateShutdownTimeout);
	s->addBool(REGISTER_URL_HANDLER, "UrlHandler", registerHandlers);
	s->addBool(REGISTER_MAGNET_HANDLER, "MagnetRegister", registerHandlers);
	s->addBool(REGISTER_DCLST_HANDLER, "DclstRegister", registerHandlers);
	s->addBool(DETECT_PREVIEW_APPS, "DetectPreviewApps", true);

	// Themes and fonts
	s->addString(TEXT_FONT, "TextFont");
	s->addString(COLOR_THEME, "ColorTheme");
	s->addString(USERLIST_IMAGE, "UserListImage");
	s->addString(THEME_MANAGER_THEME_DLL_NAME, "ThemeDLLName");
	s->addString(THEME_MANAGER_SOUNDS_THEME_NAME, "ThemeManagerSoundsThemeName");
	s->addString(EMOTICONS_FILE, "EmoticonsFile", "Kolobok");
	s->addString(ADDITIONAL_EMOTICONS, "AdditionalEmoticons", "FlylinkSmilesInternational;FlylinkSmiles");
	s->addBool(COLOR_THEME_MODIFIED, "ColorThemeModified");

	// Colors & text styles
	s->addInt(BACKGROUND_COLOR, "BackgroundColor", RGB(255, 255, 255));
	s->addInt(TEXT_COLOR, "TextColor", RGB(0, 0, 0));
	s->addInt(ERROR_COLOR, "ErrorColor", RGB(255, 0, 0));
	s->addInt(TEXT_GENERAL_BACK_COLOR, "TextGeneralBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_GENERAL_FORE_COLOR, "TextGeneralForeColor", RGB(0, 0, 0));
	s->addBool(TEXT_GENERAL_BOLD, "TextGeneralBold");
	s->addBool(TEXT_GENERAL_ITALIC, "TextGeneralItalic");
	s->addInt(TEXT_MYOWN_BACK_COLOR, "TextMyOwnBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_MYOWN_FORE_COLOR, "TextMyOwnForeColor", RGB(67, 98, 154));
	s->addBool(TEXT_MYOWN_BOLD, "TextMyOwnBold");
	s->addBool(TEXT_MYOWN_ITALIC, "TextMyOwnItalic");
	s->addInt(TEXT_PRIVATE_BACK_COLOR, "TextPrivateBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_PRIVATE_FORE_COLOR, "TextPrivateForeColor", RGB(0, 0, 0));
	s->addBool(TEXT_PRIVATE_BOLD, "TextPrivateBold");
	s->addBool(TEXT_PRIVATE_ITALIC, "TextPrivateItalic");
	s->addInt(TEXT_SYSTEM_BACK_COLOR, "TextSystemBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_SYSTEM_FORE_COLOR, "TextSystemForeColor", RGB(164, 0, 128));
	s->addBool(TEXT_SYSTEM_BOLD, "TextSystemBold", true);
	s->addBool(TEXT_SYSTEM_ITALIC, "TextSystemItalic");
	s->addInt(TEXT_SERVER_BACK_COLOR, "TextServerBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_SERVER_FORE_COLOR, "TextServerForeColor", RGB(192, 0, 138));
	s->addBool(TEXT_SERVER_BOLD, "TextServerBold", true);
	s->addBool(TEXT_SERVER_ITALIC, "TextServerItalic");
	s->addInt(TEXT_TIMESTAMP_BACK_COLOR, "TextTimestampBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_TIMESTAMP_FORE_COLOR, "TextTimestampForeColor", RGB(0, 91, 182));
	s->addBool(TEXT_TIMESTAMP_BOLD, "TextTimestampBold");
	s->addBool(TEXT_TIMESTAMP_ITALIC, "TextTimestampItalic");
	s->addInt(TEXT_MYNICK_BACK_COLOR, "TextMyNickBackColor", RGB(240, 255, 240));
	s->addInt(TEXT_MYNICK_FORE_COLOR, "TextMyNickForeColor", RGB(0, 170, 0));
	s->addBool(TEXT_MYNICK_BOLD, "TextMyNickBold", true);
	s->addBool(TEXT_MYNICK_ITALIC, "TextMyNickItalic");
	s->addInt(TEXT_NORMAL_BACK_COLOR, "TextNormBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_NORMAL_FORE_COLOR, "TextNormForeColor", RGB(80, 80, 80));
	s->addBool(TEXT_NORMAL_BOLD, "TextNormBold");
	s->addBool(TEXT_NORMAL_ITALIC, "TextNormItalic");
	s->addInt(TEXT_FAV_BACK_COLOR, "TextFavBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_FAV_FORE_COLOR, "TextFavForeColor", RGB(0, 128, 255));
	s->addBool(TEXT_FAV_BOLD, "TextFavBold", true);
	s->addBool(TEXT_FAV_ITALIC, "TextFavItalic");
	s->addInt(TEXT_OP_BACK_COLOR, "TextOPBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_OP_FORE_COLOR, "TextOPForeColor", RGB(0, 128, 64));
	s->addBool(TEXT_OP_BOLD, "TextOPBold", true);
	s->addBool(TEXT_OP_ITALIC, "TextOPItalic");
	s->addInt(TEXT_URL_BACK_COLOR, "TextURLBackColor", RGB(255, 255, 255));
	s->addInt(TEXT_URL_FORE_COLOR, "TextURLForeColor", RGB(0, 102, 204));
	s->addBool(TEXT_URL_BOLD, "TextURLBold");
	s->addBool(TEXT_URL_ITALIC, "TextURLItalic");
	s->addInt(TEXT_ENEMY_BACK_COLOR, "TextEnemyBackColor", RGB(244, 244, 244));
	s->addInt(TEXT_ENEMY_FORE_COLOR, "TextEnemyForeColor", RGB(255, 128, 64));
	s->addBool(TEXT_ENEMY_BOLD, "TextEnemyBold");
	s->addBool(TEXT_ENEMY_ITALIC, "TextEnemyItalic");

	// User list colors
	s->addInt(RESERVED_SLOT_COLOR, "ReservedSlotColor", RGB(255, 0, 128));
	s->addInt(IGNORED_COLOR, "IgnoredColor", RGB(128, 128, 128));
	s->addInt(FAVORITE_COLOR, "FavoriteColor", RGB(0, 128, 255));
	s->addInt(FAV_BANNED_COLOR, "FavBannedColor", RGB(255, 128, 64));
	s->addInt(NORMAL_COLOR, "NormalColor", RGB(0, 0, 0));
	s->addInt(FIREBALL_COLOR, "FireballColor", RGB(0, 0, 0));
	s->addInt(SERVER_COLOR, "ServerColor", RGB(0, 0, 0));
	s->addInt(PASSIVE_COLOR, "PassiveColor", RGB(67, 98, 154));
	s->addInt(OP_COLOR, "OpColor", RGB(0, 128, 64));
	s->addInt(CHECKED_COLOR, "CheckedColor", RGB(104, 32, 238));
	s->addInt(CHECKED_FAIL_COLOR, "CheckedFailColor", RGB(204, 0, 0));

	// Other colors
	s->addInt(DOWNLOAD_BAR_COLOR, "DownloadBarColor", RGB(0x36, 0xDB, 0x24));
	s->addInt(UPLOAD_BAR_COLOR, "UploadBarColor", RGB(0x00, 0xAB, 0xFD));
	s->addInt(PROGRESS_BACK_COLOR, "ProgressBackColor", RGB(0xCC, 0xCC, 0xCC));
	s->addInt(PROGRESS_SEGMENT_COLOR, "ProgressSegmentColor", RGB(0x52, 0xB9, 0x44));
	s->addInt(COLOR_RUNNING, "ColorRunning", RGB(84, 130, 252));
	s->addInt(COLOR_RUNNING_COMPLETED, "ColorRunning2", RGB(255, 255, 0));
	s->addInt(COLOR_DOWNLOADED, "ColorDownloaded", RGB(0, 255, 0));
	s->addInt(BAN_COLOR, "BanColor", RGB(116, 154, 179));
	s->addInt(FILE_SHARED_COLOR, "FileSharedColor", RGB(114, 219, 139));
	s->addInt(FILE_DOWNLOADED_COLOR, "FileDownloadedColor", RGB(145, 194, 196));
	s->addInt(FILE_CANCELED_COLOR, "FileCanceledColor", RGB(210, 168, 211));
	s->addInt(FILE_FOUND_COLOR, "FileFoundColor", RGB(255, 255, 0));
	s->addInt(FILE_QUEUED_COLOR, "FileQueuedColor", RGB(186, 0, 42));
	s->addInt(TABS_INACTIVE_BACKGROUND_COLOR, "TabsInactiveBgColor", RGB(220, 220, 220));
	s->addInt(TABS_ACTIVE_BACKGROUND_COLOR, "TabsActiveBgColor", RGB(255, 255, 255));
	s->addInt(TABS_INACTIVE_TEXT_COLOR, "TabsInactiveTextColor", RGB(82, 82, 82));
	s->addInt(TABS_ACTIVE_TEXT_COLOR, "TabsActiveTextColor", RGB(0, 0, 0));
	s->addInt(TABS_OFFLINE_BACKGROUND_COLOR, "TabsOfflineBgColor", RGB(244, 166, 166));
	s->addInt(TABS_OFFLINE_ACTIVE_BACKGROUND_COLOR, "TabsOfflineActiveBgColor", RGB(255, 183, 183));
	s->addInt(TABS_UPDATED_BACKGROUND_COLOR, "TabsUpdatedBgColor", RGB(217, 234, 247));
	s->addInt(TABS_BORDER_COLOR, "TabsBorderColor", RGB(157, 157, 161));

	// File lists
	s->addBool(FILELIST_SHOW_SHARED, "FileListShowShared", true);
	s->addBool(FILELIST_SHOW_DOWNLOADED, "FileListShowDownloaded", true);
	s->addBool(FILELIST_SHOW_CANCELED, "FileListShowCanceled", true);
	s->addBool(FILELIST_SHOW_MY_UPLOADS, "FileListShowMyUploads", true);

	// Assorted UI settings
	s->addBool(APP_DPI_AWARE, "AppDpiAware");
	s->addBool(SHOW_GRIDLINES, "ShowGrid");
	s->addBool(SHOW_INFOTIPS, "ShowInfoTips", true);
	s->addBool(USE_SYSTEM_ICONS, "UseSystemIcons", true);
	s->addBool(MDI_MAXIMIZED, "MDIMaxmimized", true);
	s->addBool(TOGGLE_ACTIVE_WINDOW, "ToggleActiveWindow", true);
	s->addBool(POPUNDER_PM, "PopunderPm");
	s->addBool(POPUNDER_FILELIST, "PopunderFilelist");

	// Tab settings
	s->addInt(TABS_POS, "TabsPos", TABS_TOP);
	s->addInt(MAX_TAB_ROWS, "MaxTabRows", 7, 0, &validateTabRows);
	s->addInt(TAB_SIZE, "TabSize", 30, 0, &validateTabSize);
	s->addBool(TABS_SHOW_INFOTIPS, "TabsShowInfoTips", true);
	s->addBool(TABS_CLOSEBUTTONS, "UseTabsCloseButton", true);
	s->addBool(TABS_BOLD, "TabsBold");
	s->addBool(NON_HUBS_FRONT, "NonHubsFront");
	s->addBool(BOLD_FINISHED_DOWNLOADS, "BoldFinishedDownloads", true);
	s->addBool(BOLD_FINISHED_UPLOADS, "BoldFinishedUploads", true);
	s->addBool(BOLD_QUEUE, "BoldQueue", true);
	s->addBool(BOLD_HUB, "BoldHub", true);
	s->addBool(BOLD_PM, "BoldPm", true);
	s->addBool(BOLD_SEARCH, "BoldSearch", true);
	s->addBool(BOLD_WAITING_USERS, "BoldWaitingUsers", true);
	s->addBool(HUB_URL_IN_TITLE, "HubUrlInTitle");

	// Toolbar settings
	s->addString(TOOLBAR, "Toolbar", "1,27,-1,0,25,5,3,4,-1,6,7,8,9,22,-1,10,-1,14,23,-1,15,17,-1,19,21,29,24,28,20");
	s->addString(WINAMPTOOLBAR, "WinampToolBar", "0,-1,1,2,3,4,5,6,7,8");
	s->addBool(LOCK_TOOLBARS, "LockToolbars");
	s->addInt(TB_IMAGE_SIZE, "TbImageSize", 24, 0, &validateToolbarSize);
	s->addBool(SHOW_PLAYER_CONTROLS, "ShowWinampControl");

	// Menu settings
#ifdef _WIN32
	bool useFlatMenuHeader = SysVersion::isOsWin8Plus();
	uint32_t menuHeaderColor = SysVersion::isOsWin11Plus() ? RGB(185, 220, 255) : RGB(0, 128, 255);
#else
	bool useFlatMenuHeader = false;
	uint32_t menuHeaderColor = RGB(0, 128, 255);
#endif
	s->addBool(USE_CUSTOM_MENU, "UseCustomMenu", true);
	s->addBool(MENUBAR_TWO_COLORS, "MenubarTwoColors", !useFlatMenuHeader);
	s->addInt(MENUBAR_LEFT_COLOR, "MenubarLeftColor", menuHeaderColor);
	s->addInt(MENUBAR_RIGHT_COLOR, "MenubarRightColor", RGB(168, 211, 255));
	s->addBool(MENUBAR_BUMPED, "MenubarBumped", !useFlatMenuHeader);
	s->addBool(UC_SUBMENU, "UcSubMenu", true);

	// Progressbar settings
	s->addBool(SHOW_PROGRESS_BARS, "ShowProgressBars", true);
	s->addInt(PROGRESS_TEXT_COLOR_DOWN, "ProgressTextDown", RGB(0, 0, 0));
	s->addInt(PROGRESS_TEXT_COLOR_UP, "ProgressTextUp", RGB(0, 0, 0));
	s->addBool(PROGRESS_OVERRIDE_COLORS, "ProgressOverrideColors", true);
	s->addInt(PROGRESS_3DDEPTH, "Progress3DDepth", 3);
	s->addBool(PROGRESS_OVERRIDE_COLORS2, "ProgressOverrideColors2");
	s->addBool(PROGRESSBAR_ODC_STYLE, "ProgressbaroDCStyle");
	s->addBool(PROGRESSBAR_ODC_BUMPED, "OdcStyleBumped", true);
	s->addBool(STEALTHY_STYLE_ICO, "StealthyStyleIco", true);
	s->addBool(STEALTHY_STYLE_ICO_SPEEDIGNORE, "StealthyStyleIcoSpeedIgnore");
	s->addInt(TOP_DL_SPEED, "TopDownSpeed", 512);
	s->addInt(TOP_UL_SPEED, "TopUpSpeed", 512);

	// Media Player message formats
	s->addString(WINAMP_FORMAT, "WinampFormat", "+me is listening to '%[artist] - %[track] - %[title]' (%[percent] of %[length], %[bitrate], Winamp %[version]) %[magnet]");
	s->addString(WMP_FORMAT, "WMPFormat", "+me is listening to '%[title]' (%[bitrate], Windows Media Player %[version]) %[magnet]");
	s->addString(ITUNES_FORMAT, "iTunesFormat", "+me is listening to '%[artist] - %[title]' (%[percent] of %[length], %[bitrate], iTunes %[version]) %[magnet]");
	s->addString(MPLAYERC_FORMAT, "MPCFormat", "+me is listening to '%[title]' (Media Player Classic) %[magnet]");
	s->addString(JETAUDIO_FORMAT, "JetAudioFormat", "+me is listening to '%[artist] - %[title]' (%[percent], JetAudio %[version]) %[magnet]");
	s->addString(QCDQMP_FORMAT, "QcdQmpFormat", "+me is listening to '%[title]' (%[bitrate], %[sample]) (%[elapsed] %[bar] %[length]) %[magnet]");

	// Open at startup
	s->addBool(OPEN_RECENT_HUBS, "OpenRecentHubs", true);
	s->addBool(OPEN_PUBLIC_HUBS, "OpenPublic", true);
	s->addBool(OPEN_FAVORITE_HUBS, "OpenFavoriteHubs");
	s->addBool(OPEN_FAVORITE_USERS, "OpenFavoriteUsers");
	s->addBool(OPEN_QUEUE, "OpenQueue");
	s->addBool(OPEN_FINISHED_DOWNLOADS, "OpenFinishedDownloads");
	s->addBool(OPEN_FINISHED_UPLOADS, "OpenFinishedUploads");
	s->addBool(OPEN_SEARCH_SPY, "OpenSearchSpy");
	s->addBool(OPEN_NETWORK_STATISTICS, "OpenNetworkStatistics");
	s->addBool(OPEN_NOTEPAD, "OpenNotepad");
	s->addBool(OPEN_WAITING_USERS, "OpenWaitingUsers");
	s->addBool(OPEN_CDMDEBUG, "OpenCdmDebug");
	s->addBool(OPEN_DHT, "OpenDHT");
	s->addBool(OPEN_ADLSEARCH, "OpenADLSearch");

	// Click actions
	s->addInt(USERLIST_DBLCLICK, "UserListDoubleClick");
	s->addInt(TRANSFERLIST_DBLCLICK, "TransferListDoubleClick");
	s->addInt(CHAT_DBLCLICK, "ChatDoubleClick", 1);
	s->addInt(FAVUSERLIST_DBLCLICK, "FavUserDblClick");
	s->addBool(MAGNET_ASK, "MagnetAsk", true);
	s->addInt(MAGNET_ACTION, "MagnetAction", MAGNET_ACTION_SEARCH);
	s->addBool(SHARED_MAGNET_ASK, "SharedMagnetAsk", true);
	s->addInt(SHARED_MAGNET_ACTION, "SharedMagnetAction", MAGNET_ACTION_SEARCH);
	s->addBool(DCLST_ASK, "DCLSTAsk", true);
	s->addInt(DCLST_ACTION, "DCLSTAction", MAGNET_ACTION_SEARCH);

	// Window behavior
	s->addBool(TOPMOST, "Topmost");
	s->addBool(MINIMIZE_TRAY, "MinimizeToTray");
	s->addBool(MINIMIZE_ON_STARTUP, "MinimizeOnStartup");
	s->addBool(MINIMIZE_ON_CLOSE, "MinimizeOnClose");
	s->addBool(SHOW_CURRENT_SPEED_IN_TITLE, "ShowCurrentSpeedInTitle");

	// Confirmations
	s->addBool(CONFIRM_EXIT, "ConfirmExit", true);
	s->addBool(CONFIRM_DELETE, "ConfirmDelete", true);
	s->addBool(CONFIRM_HUB_REMOVAL, "ConfirmHubRemoval", true);
	s->addBool(CONFIRM_HUBGROUP_REMOVAL, "ConfirmHubgroupRemoval", true);
	s->addBool(CONFIRM_USER_REMOVAL, "ConfirmUserRemoval", true);
	s->addBool(CONFIRM_FINISHED_REMOVAL, "ConfirmFinishedRemoval", true);
	s->addBool(CONFIRM_SHARE_FROM_SHELL, "ConfirmShareFromShell", true);
	s->addBool(CONFIRM_CLEAR_SEARCH_HISTORY, "ConfirmClearSearchHistory", true);
	s->addBool(CONFIRM_ADLS_REMOVAL, "ConfirmAdlsRemoval", true);

	// Password
	s->addString(PASSWORD, "AuthPass");
	s->addBool(PROTECT_TRAY, "ProtectTray");
	s->addBool(PROTECT_START, "ProtectStart");
	s->addBool(PROTECT_CLOSE, "ProtectClose");

	// Hub frame
	s->addBool(AUTO_FOLLOW, "AutoFollow", true);
	s->addBool(SHOW_JOINS, "ShowJoins");
	s->addBool(FAV_SHOW_JOINS, "FavShowJoins", true);
	s->addBool(PROMPT_HUB_PASSWORD, "PromptPassword", true);
	s->addBool(ENABLE_COUNTRY_FLAG, "EnableCountryFlag", true);
	s->addBool(SHOW_CHECKED_USERS, "ShowCheckedUsers", true);
	s->addInt(HUB_POSITION, "HubPosition", POS_RIGHT);
	s->addBool(SORT_FAVUSERS_FIRST, "SortFavUsersFirst");
	s->addBool(FILTER_ENTER, "FilterEnter");
	s->addBool(JOIN_OPEN_NEW_WINDOW, "OpenNewWindow");
	s->addInt(USER_THRESHOLD, "HubThreshold", 1000, 0, &validateNonNeg);
	s->addBool(POPUP_PMS_HUB, "PopupHubPms", true);
	s->addBool(POPUP_PMS_BOT, "PopupBotPms", true);
	s->addBool(POPUP_PMS_OTHER, "PopupPMs", true);

	// Chat frame
	s->addInt(CHAT_BUFFER_SIZE, "ChatBufferSize", 25000);
	s->addBool(CHAT_TIME_STAMPS, "ChatTimeStamps", true);
	s->addBool(BOLD_MSG_AUTHOR, "BoldMsgAuthor", true);
	s->addBool(CHAT_PANEL_SHOW_INFOTIPS, "ChatShowInfoTips", true);
	s->addBool(SHOW_EMOTICONS_BTN, "ShowEmoticonsButton", true);
	s->addBool(CHAT_ANIM_SMILES, "ChatAnimSmiles", true);
	s->addBool(SMILE_SELECT_WND_ANIM_SMILES, "SmileSelectWndAnimSmiles", true);
	s->addBool(SHOW_SEND_MESSAGE_BUTTON, "ShowSendMessageButton", true);
#ifdef BL_UI_FEATURE_BB_CODES
	s->addBool(FORMAT_BB_CODES, "FormatBBCode", true);
	s->addBool(FORMAT_BB_CODES_COLORS, "FormatBBCodeColors", true);
	s->addBool(SHOW_BBCODE_PANEL, "ShowBBCodePanel", true);
#endif
	s->addBool(SHOW_MULTI_CHAT_BTN, "ShowMultiChatButton", true);
	s->addBool(SHOW_TRANSCODE_BTN, "ShowTranscodeButton");
	s->addBool(SHOW_LINK_BTN, "ShowLinkButton", true);
	s->addBool(SHOW_FIND_BTN, "ShowFindButton", true);
	s->addBool(CHAT_REFFERING_TO_NICK, "ChatRefferingToNick", true);
	s->addBool(STATUS_IN_CHAT, "StatusInChat", true);
	s->addBool(DISPLAY_CHEATS_IN_MAIN_CHAT, "DisplayCheatsInMainChat", true);
	s->addBool(USE_CTRL_FOR_LINE_HISTORY, "UseCTRLForLineHistory");
	s->addBool(MULTILINE_CHAT_INPUT, "MultilineChatInput", true);
	s->addBool(MULTILINE_CHAT_INPUT_BY_CTRL_ENTER, "MultilineChatInputCtrlEnter", true);
#ifdef BL_UI_FEATURE_BB_CODES
	s->addBool(FORMAT_BOT_MESSAGE, "FormatBotMessage", true);
#endif
	s->addBool(SEND_UNKNOWN_COMMANDS, "SendUnknownCommands");

	// Search frame
	s->addBool(SAVE_SEARCH_SETTINGS, "SaveSearchSettings");
	s->addBool(FORGET_SEARCH_REQUEST, "ForgetSearchRequest");
	s->addInt(SEARCH_HISTORY, "SearchHistory", 30, 0, &validateSearchHistory);
	s->addBool(CLEAR_SEARCH, "ClearSearch", true);
	s->addBool(USE_SEARCH_GROUP_TREE_SETTINGS, "UseSearchGroupTreeSettings", true);
	s->addBool(ONLY_FREE_SLOTS, "OnlyFreeSlots");

	// Users frame
	s->addInt(SHOW_IGNORED_USERS, "ShowIgnoredUsers", -1);

	// Queue frame
	s->addBool(QUEUE_FRAME_SHOW_TREE, "UploadQueueFrameShowTree", true);

	// Finished frame
	s->addBool(SHOW_SHELL_MENU, "ShowShellMenu", true);

	// Search Spy frame
	s->addBool(SPY_FRAME_IGNORE_TTH_SEARCHES, "SpyFrameIgnoreTthSearches");
	s->addBool(SHOW_SEEKERS_IN_SPY_FRAME, "ShowSeekersInSpyFrame", true);
	s->addBool(LOG_SEEKERS_IN_SPY_FRAME, "LogSeekersInSpyFrame");

	// Transfers
	s->addBool(TRANSFERS_ONLY_ACTIVE_UPLOADS, "TransfersOnlyActiveUploads");

	// Settings dialog
	s->addBool(REMEMBER_SETTINGS_PAGE, "RememberSettingsPage", true);
	s->addInt(SETTINGS_PAGE, "SettingsPage");
	s->addBool(USE_OLD_SHARING_UI, "UseOldSharingUI");

	// Media player
	s->addInt(MEDIA_PLAYER, "MediaPlayer", 0, 0, &validateMediaPlayer);
	s->addBool(USE_MAGNETS_IN_PLAYERS_SPAM, "UseMagnetsInPlayerSpam", true);
	s->addBool(USE_BITRATE_FIX_FOR_SPAM, "UseBitrateFixForSpam");

	// Popup settings
	s->addString(POPUP_FONT, "PopupFont");
	s->addString(POPUP_TITLE_FONT, "PopupTitleFont");
	s->addString(POPUP_IMAGE_FILE, "PopupImageFile");
	s->addBool(POPUPS_DISABLED, "PopupsDisabled");
	s->addInt(POPUP_TYPE, "PopupType");
	s->addInt(POPUP_TIME, "PopupTime", 5, 0, &validatePopupTime);
	s->addInt(POPUP_WIDTH, "PopupW", 200, 0, &validatePopupWidth);
	s->addInt(POPUP_HEIGHT, "PopupH", 90, 0, &validatePopupHeight);
	s->addInt(POPUP_TRANSPARENCY, "PopupTransp", 200, 0, &validatePopupTransparency);
	s->addInt(POPUP_MAX_LENGTH, "PopupMaxLength", 120, 0, &validatePopupMaxLength);
	s->addInt(POPUP_BACKCOLOR, "PopupBackColor", RGB(58, 122, 180));
	s->addInt(POPUP_TEXTCOLOR, "PopupTextColor", RGB(255, 255, 255));
	s->addInt(POPUP_TITLE_TEXTCOLOR, "PopupTitleTextColor", RGB(255, 255, 255));
	s->addInt(POPUP_IMAGE, "PopupImage");
	s->addInt(POPUP_COLORS, "PopupColors");
	s->addBool(POPUP_ON_HUB_CONNECTED, "PopupHubConnected");
	s->addBool(POPUP_ON_HUB_DISCONNECTED, "PopupHubDisconnected");
	s->addBool(POPUP_ON_FAVORITE_CONNECTED, "PopupFavoriteConnected", true);
	s->addBool(POPUP_ON_FAVORITE_DISCONNECTED, "PopupFavoriteDisconnected", true);
	s->addBool(POPUP_ON_CHEATING_USER, "PopupCheatingUser", true);
	s->addBool(POPUP_ON_CHAT_LINE, "PopupChatLine");
	s->addBool(POPUP_ON_DOWNLOAD_STARTED, "PopupDownloadStart");
	s->addBool(POPUP_ON_DOWNLOAD_FAILED, "PopupDownloadFailed");
	s->addBool(POPUP_ON_DOWNLOAD_FINISHED, "PopupDownloadFinished", true);
	s->addBool(POPUP_ON_UPLOAD_FINISHED, "PopupUploadFinished");
	s->addBool(POPUP_ON_PM, "PopupPm");
	s->addBool(POPUP_ON_NEW_PM, "PopupNewPM", true);
	s->addBool(POPUP_ON_SEARCH_SPY, "PopupSearchSpy");
	s->addBool(POPUP_ON_FOLDER_SHARED, "PopupNewFolderShare", true);
	s->addBool(POPUP_PM_PREVIEW, "PreviewPm", true);
	s->addBool(POPUP_ONLY_WHEN_AWAY, "PopupAway");
	s->addBool(POPUP_ONLY_WHEN_MINIMIZED, "PopupMinimized", true);

	// Sounds
	s->addString(SOUND_BEEPFILE, "SoundBeepFile", Util::getSoundsPath() + "PrivateMessage.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_BEGINFILE, "SoundBeginFile", Util::getSoundsPath() + "DownloadBegins.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_FINISHFILE, "SoundFinishedFile", Util::getSoundsPath() + "DownloadFinished.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_SOURCEFILE, "SoundSourceFile", Util::getSoundsPath() + "AltSourceAdded.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_UPLOADFILE, "SoundUploadFile", Util::getSoundsPath() + "UploadFinished.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_FAKERFILE, "SoundFakerFile", Util::getSoundsPath() + "FakerFound.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_CHATNAMEFILE, "SoundChatNameFile", Util::getSoundsPath() + "MyNickInMainChat.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_TTH, "SoundTTH", Util::getSoundsPath() + "FileCorrupted.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_HUBCON, "SoundHubConnected", Util::getSoundsPath() + "HubConnected.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_HUBDISCON, "SoundHubDisconnected", Util::getSoundsPath() + "HubDisconnected.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_FAVUSER, "SoundFavUserOnline", Util::getSoundsPath() + "FavUser.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_FAVUSER_OFFLINE, "SoundFavUserOffline", Util::getSoundsPath() + "FavUserDisconnected.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_TYPING_NOTIFY, "SoundTypingNotify", Util::getSoundsPath() + "TypingNotify.wav", Settings::FLAG_CONVERT_PATH);
	s->addString(SOUND_SEARCHSPY, "SoundSearchSpy", Util::getSoundsPath() + "SearchSpy.wav", Settings::FLAG_CONVERT_PATH);
	s->addBool(SOUNDS_DISABLED, "SoundsDisabled", true);
	s->addBool(PRIVATE_MESSAGE_BEEP, "PrivateMessageBeep");
	s->addBool(PRIVATE_MESSAGE_BEEP_OPEN, "PrivateMessageBeepOpen");

	// Frames UI state
	s->addString(TRANSFER_FRAME_ORDER, "TransferFrameOrder");
	s->addString(TRANSFER_FRAME_WIDTHS, "TransferFrameWidths");
	s->addString(TRANSFER_FRAME_VISIBLE, "TransferFrameVisible");
	/*
	s->addString(HUB_FRAME_ORDER, "HubFrameOrder");
	s->addString(HUB_FRAME_WIDTHS, "HubFrameWidths");
	s->addString(HUB_FRAME_VISIBLE, "HubFrameVisible", "1,1,0,1,1,1,1,1,1,1,1,1,1,1");
	*/
	s->addString(SEARCH_FRAME_ORDER, "SearchFrameOrder");
	s->addString(SEARCH_FRAME_WIDTHS, "SearchFrameWidths");
	s->addString(SEARCH_FRAME_VISIBLE, "SearchFrameVisible");
	s->addString(DIRLIST_FRAME_ORDER, "DirectoryListingFrameOrder");
	s->addString(DIRLIST_FRAME_WIDTHS, "DirectoryListingFrameWidths");
	s->addString(DIRLIST_FRAME_VISIBLE, "DirectoryListingFrameVisible", "1,1,1,1,1");
	s->addString(FAVORITES_FRAME_ORDER, "FavoritesFrameOrder");
	s->addString(FAVORITES_FRAME_WIDTHS, "FavoritesFrameWidths");
	s->addString(FAVORITES_FRAME_VISIBLE, "FavoritesFrameVisible");
	s->addString(QUEUE_FRAME_ORDER, "QueueFrameOrder");
	s->addString(QUEUE_FRAME_WIDTHS, "QueueFrameWidths");
	s->addString(QUEUE_FRAME_VISIBLE, "QueueFrameVisible");
	s->addString(PUBLIC_HUBS_FRAME_ORDER, "PublicHubsFrameOrder");
	s->addString(PUBLIC_HUBS_FRAME_WIDTHS, "PublicHubsFrameWidths");
	s->addString(PUBLIC_HUBS_FRAME_VISIBLE, "PublicHubsFrameVisible");
	s->addString(USERS_FRAME_ORDER, "UsersFrameOrder");
	s->addString(USERS_FRAME_WIDTHS, "UsersFrameWidths");
	s->addString(USERS_FRAME_VISIBLE, "UsersFrameVisible");
	s->addString(FINISHED_DL_FRAME_ORDER, "FinishedDLFrameOrder");
	s->addString(FINISHED_DL_FRAME_WIDTHS, "FinishedDLFrameWidths");
	s->addString(FINISHED_DL_FRAME_VISIBLE, "FinishedDLFrameVisible", "1,1,1,1,1,1,1,1");
	s->addString(FINISHED_UL_FRAME_WIDTHS, "FinishedULFrameWidths");
	s->addString(FINISHED_UL_FRAME_ORDER, "FinishedULFrameOrder");
	s->addString(FINISHED_UL_FRAME_VISIBLE, "FinishedULFrameVisible", "1,1,1,1,1,1,1");
	s->addString(UPLOAD_QUEUE_FRAME_ORDER, "UploadQueueFrameOrder");
	s->addString(UPLOAD_QUEUE_FRAME_WIDTHS, "UploadQueueFrameWidths");
	s->addString(UPLOAD_QUEUE_FRAME_VISIBLE, "UploadQueueFrameVisible");
	s->addString(RECENTS_FRAME_ORDER, "RecentFrameOrder");
	s->addString(RECENTS_FRAME_WIDTHS, "RecentFrameWidths");
	s->addString(RECENTS_FRAME_VISIBLE, "RecentFrameVisible");
	s->addString(ADLSEARCH_FRAME_ORDER, "ADLSearchFrameOrder");
	s->addString(ADLSEARCH_FRAME_WIDTHS, "ADLSearchFrameWidths");
	s->addString(ADLSEARCH_FRAME_VISIBLE, "ADLSearchFrameVisible");
	s->addString(SPY_FRAME_WIDTHS, "SpyFrameWidths");
	s->addString(SPY_FRAME_ORDER, "SpyFrameOrder");
	s->addString(SPY_FRAME_VISIBLE, "SpyFrameVisible");
	s->addInt(TRANSFER_FRAME_SORT, "TransferFrameSort", 3); // COLUMN_FILENAME
	s->addInt(TRANSFER_FRAME_SPLIT, "TransferFrameSplit", 8000);
	s->addInt(HUB_FRAME_SORT, "HubFrameSort", 1); // COLUMN_NICK
	s->addInt(SEARCH_FRAME_SORT, "SearchFrameSort", -3); // COLUMN_HITS, descending
	s->addInt(DIRLIST_FRAME_SORT, "DirectoryListingFrameSort", 1); // COLUMN_FILENAME
	s->addInt(DIRLIST_FRAME_SPLIT, "DirectoryListingFrameSplit", 2500, 0, &validateSplitter);
	s->addInt(FAVORITES_FRAME_SORT, "FavoritesFrameSort");
	s->addInt(QUEUE_FRAME_SORT, "QueueFrameSort");
	s->addInt(QUEUE_FRAME_SPLIT, "QueueFrameSplit", 2500, 0, &validateSplitter);
	s->addInt(PUBLIC_HUBS_FRAME_SORT, "PublicHubsFrameSort", -3); // COLUMN_USERS, descending
	s->addInt(USERS_FRAME_SORT, "UsersFrameSort");
	s->addInt(USERS_FRAME_SPLIT, "UsersFrameSplit", 8000);
	s->addInt(FINISHED_DL_FRAME_SORT, "FinishedDLFrameSort");
	s->addInt(FINISHED_UL_FRAME_SORT, "FinishedULFrameSort");
	s->addInt(UPLOAD_QUEUE_FRAME_SORT, "UploadQueueFrameSort");
	s->addInt(UPLOAD_QUEUE_FRAME_SPLIT, "UploadQueueFrameSplit", 2500, 0, &validateSplitter);
	s->addInt(RECENTS_FRAME_SORT, "RecentFrameSort");
	s->addInt(ADLSEARCH_FRAME_SORT, "ADLSearchFrameSort");
	s->addInt(SPY_FRAME_SORT, "SpyFrameSort", 4); // COLUMN_TIME

	// View visibility
	s->addBool(SHOW_STATUSBAR, "ShowStatusbar", true);
	s->addBool(SHOW_TOOLBAR, "ShowToolbar", true);
	s->addBool(SHOW_TRANSFERVIEW, "ShowTransferView", true);
	s->addBool(SHOW_QUICK_SEARCH, "ShowQSearch", true);

	// Main window size & position
	s->addInt(MAIN_WINDOW_STATE, "MainWindowState", SW_SHOWNORMAL);
	s->addInt(MAIN_WINDOW_SIZE_X, "MainWindowSizeX", CW_USEDEFAULT);
	s->addInt(MAIN_WINDOW_SIZE_Y, "MainWindowSizeY", CW_USEDEFAULT);
	s->addInt(MAIN_WINDOW_POS_X, "MainWindowPosX", CW_USEDEFAULT);
	s->addInt(MAIN_WINDOW_POS_Y, "MainWindowPosY", CW_USEDEFAULT);

	// Other settings, mostly useless
	s->addBool(AUTO_AWAY, "AutoAway");
	s->addBool(REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY, "ReduceProcessPriorityIfMinimized");
	s->addBool(DCLST_CREATE_IN_SAME_FOLDER, "DclstCreateInSameFolder", true);
	s->addBool(DCLST_INCLUDESELF, "DclstIncludeSelf", true);
	s->addBool(SEARCH_MAGNET_SOURCES, "SearchMagnetSources", true);
	s->addString(DCLST_DIRECTORY, "DclstFolder");

	// Recents
	s->addString(SAVED_SEARCH_SIZE, "SavedSearchSize");
	s->addString(KICK_MSG_RECENT_01, "KickMsgRecent01");
	s->addString(KICK_MSG_RECENT_02, "KickMsgRecent02");
	s->addString(KICK_MSG_RECENT_03, "KickMsgRecent03");
	s->addString(KICK_MSG_RECENT_04, "KickMsgRecent04");
	s->addString(KICK_MSG_RECENT_05, "KickMsgRecent05");
	s->addString(KICK_MSG_RECENT_06, "KickMsgRecent06");
	s->addString(KICK_MSG_RECENT_07, "KickMsgRecent07");
	s->addString(KICK_MSG_RECENT_08, "KickMsgRecent08");
	s->addString(KICK_MSG_RECENT_09, "KickMsgRecent09");
	s->addString(KICK_MSG_RECENT_10, "KickMsgRecent10");
	s->addString(KICK_MSG_RECENT_11, "KickMsgRecent11");
	s->addString(KICK_MSG_RECENT_12, "KickMsgRecent12");
	s->addString(KICK_MSG_RECENT_13, "KickMsgRecent13");
	s->addString(KICK_MSG_RECENT_14, "KickMsgRecent14");
	s->addString(KICK_MSG_RECENT_15, "KickMsgRecent15");
	s->addString(KICK_MSG_RECENT_16, "KickMsgRecent16");
	s->addString(KICK_MSG_RECENT_17, "KickMsgRecent17");
	s->addString(KICK_MSG_RECENT_18, "KickMsgRecent18");
	s->addString(KICK_MSG_RECENT_19, "KickMsgRecent19");
	s->addString(KICK_MSG_RECENT_20, "KickMsgRecent20");
	s->addInt(SAVED_SEARCH_TYPE, "SavedSearchType");
	s->addInt(SAVED_SEARCH_SIZEMODE, "SavedSearchSizeMode", 2);
	s->addInt(SAVED_SEARCH_MODE, "SavedSearchMode", 1);
}

void Conf::updateUiSettingsDefaults()
{
	auto s = SettingsManager::instance.getCoreSettings();
	s->setStringDefault(Conf::HTTP_USER_AGENT, KnownClients::userAgents[0]);
}

void Conf::processUiSettings()
{
	auto s = SettingsManager::instance.getUiSettings();
	string theme = s->getString(Conf::THEME_MANAGER_THEME_DLL_NAME);
	bool updateTheme = false;
	if (Text::isAsciiSuffix2<string>(theme, ".dll"))
	{
		theme.erase(theme.length() - 4);
		updateTheme = true;
	}
	if (Text::isAsciiSuffix2<string>(theme, "_x64"))
	{
		theme.erase(theme.length() - 4);
		updateTheme = true;
	}
	if (updateTheme) s->setString(Conf::THEME_MANAGER_THEME_DLL_NAME, theme);
}
