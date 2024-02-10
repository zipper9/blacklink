#ifndef APP_PATHS_H_
#define APP_PATHS_H_

#include "typedefs.h"
#include "debug.h"

namespace Util
{
	enum Paths
	{
		// Global configuration
		PATH_GLOBAL_CONFIG,
		// Per-user configuration (queue, favorites, ...)
		PATH_USER_CONFIG,
		// Per-user local data (cache, temp files, ...)
		PATH_USER_LOCAL,
		// Web server files
		PATH_WEB_SERVER,
		// Default download location
		PATH_DOWNLOADS,
		// Default file list location
		PATH_FILE_LISTS,
		// Default hub list cache
		PATH_HUB_LISTS,
		// Where the notepad file is stored
		PATH_NOTEPAD,
		// Folder with emoticon packs
		PATH_EMOPACKS,
		// Languages files location
		PATH_LANGUAGES,
		// Themes and resources
		PATH_THEMES,
		// Executable path
		PATH_EXE,
		// Sounds path
		PATH_SOUNDS,
		// Default download directory for HttpClient
		PATH_HTTP_DOWNLOADS,
		// Plugin DLLs
		PATH_PLUGINS,
		PATH_LAST
	};

	enum SysPaths
	{
#ifdef _WIN32
		WINDOWS,
		PROGRAM_FILESX86,
		PROGRAM_FILES,
		APPDATA,
		LOCAL_APPDATA,
		COMMON_APPDATA,
		PERSONAL,
#endif
		SYS_PATH_LAST
	};

	extern string paths[PATH_LAST];
	extern string sysPaths[SYS_PATH_LAST];

	void initAppPaths();
	bool isLocalMode();

	// Path of temporary storage
	string getTempPath();

#ifdef _WIN32
	tstring getModuleFileName();
#else
	string getModuleFileName();
#endif

#ifdef _WIN32
	string getSystemDownloadsPath(const string& def);
#endif

	// Migrate from pre-localmode config location
	void migrate(const string& file) noexcept;

	// Path of program configuration files
	inline const string& getPath(Paths path)
	{
		dcassert(!paths[path].empty());
		return paths[path];
	}

	// Path of system folder
	inline const string& getSysPath(SysPaths path)
	{
		dcassert(!sysPaths[path].empty());
		return sysPaths[path];
	}

	inline const string& getListPath() { return getPath(PATH_FILE_LISTS); }
	inline const string& getHubListsPath() { return getPath(PATH_HUB_LISTS); }
	inline const string& getNotepadFile() { return getPath(PATH_NOTEPAD); }
	inline const string& getConfigPath() { return getPath(PATH_USER_CONFIG); }
	inline const string& getDataPath() { return getPath(PATH_GLOBAL_CONFIG); }
	inline const string& getLanguagesPath() { return getPath(PATH_LANGUAGES); }
	inline const string& getLocalPath() { return getPath(PATH_USER_LOCAL); }
	inline const string& getWebServerPath() { return getPath(PATH_WEB_SERVER); }
	inline const string& getDownloadsPath() { return getPath(PATH_DOWNLOADS); }
	inline const string& getEmoPacksPath() { return getPath(PATH_EMOPACKS); }
	inline const string& getThemesPath() { return getPath(PATH_THEMES); }
	inline const string& getExePath() { return getPath(PATH_EXE); }
	inline const string& getSoundsPath() { return getPath(PATH_SOUNDS); }
	inline const string& getHttpDownloadsPath() { return getPath(PATH_HTTP_DOWNLOADS); }
	inline const string& getPluginsPath() { return getPath(PATH_PLUGINS); }

	void loadBootConfig();
	bool locatedInSysPath(SysPaths sysPath, const string& path);
	bool locatedInSysPath(const string& path);
	void initProfileConfig();
	void moveSettings();
	void backupSettings();
}

#endif // APP_PATHS_H_
