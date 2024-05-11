#include "stdinc.h"
#include "AppPaths.h"
#include "PathUtil.h"
#include "Util.h" // validateFileName
#include "SimpleXML.h"
#include "FormatUtil.h"
#include "ParamExpander.h"
#include "Random.h"
#include "LogManager.h"
#include "SysVersion.h"
#include "version.h"

#ifdef _WIN32
#include "CompatibilityManager.h"
#include <shlobj.h>
#endif

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

// In local mode, all config and temp files are kept in the same dir as the executable
static bool localMode;

string Util::paths[Util::PATH_LAST];
string Util::sysPaths[Util::SYS_PATH_LAST];

#ifdef _WIN32
tstring Util::getModuleFileName()
{
	static tstring moduleFileName;
	if (moduleFileName.empty())
	{
		TCHAR buf[MAX_PATH];
		DWORD len = GetModuleFileName(NULL, buf, MAX_PATH);
		moduleFileName.assign(buf, len);
	}
	return moduleFileName;
}
#else
string Util::getModuleFileName()
{
	string result;
#ifdef __FreeBSD__
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	char buf[PATH_MAX];
	size_t cb = sizeof(buf);
	if (!sysctl(mib, 4, buf, &cb, NULL, 0)) result = buf;
#else
	char link[64];
	sprintf(link, "/proc/%u/exe", (unsigned) getpid());
	char buf[PATH_MAX];
	ssize_t size = readlink(link, buf, sizeof(buf));
	if (size > 0) result.assign(buf, size);
#endif
	return result;
}
#endif

bool Util::locatedInSysPath(const string& path)
{
#ifdef _WIN32
	// don't share Windows directory
	return Util::locatedInSysPath(Util::WINDOWS, path) ||
	       Util::locatedInSysPath(Util::APPDATA, path) ||
	       Util::locatedInSysPath(Util::LOCAL_APPDATA, path) ||
	       Util::locatedInSysPath(Util::PROGRAM_FILES, path) ||
	       Util::locatedInSysPath(Util::PROGRAM_FILESX86, path);
#else
	return false;
#endif
}

bool Util::locatedInSysPath(Util::SysPaths sysPath, const string& currentPath)
{
	const string& path = sysPaths[sysPath];
	return !path.empty() && currentPath.length() >= path.length() && strnicmp(currentPath, path, path.size()) == 0;
}

void Util::initProfileConfig()
{
#ifdef _WIN32
	paths[PATH_USER_CONFIG] = getSysPath(APPDATA) + APPNAME PATH_SEPARATOR_STR;
#else
	const char* home = getenv("HOME");
	if (!home) home = "/tmp";
	paths[PATH_USER_CONFIG] = home;
	paths[PATH_USER_CONFIG] += PATH_SEPARATOR_STR "." APPNAME_LC PATH_SEPARATOR_STR;
#endif
}

static const string configFiles[] =
{
	"ADLSearch.xml",
	"DCPlusPlus.xml",
	"Favorites.xml",
	"IPTrust.ini",
#ifdef SSA_IPGRANT_FEATURE
	"IPGrant.ini",
#endif
	"IPGuard.ini",
	"Queue.xml",
	"DHT.xml"
};

static void copySettings(const string& sourcePath, const string& destPath)
{
	File::ensureDirectory(destPath);
	for (size_t i = 0; i < _countof(configFiles); ++i)
	{
		string sourceFile = sourcePath + configFiles[i];
		string destFile = destPath + configFiles[i];
		if (!File::isExist(destFile) && File::isExist(sourceFile))
		{
			if (!File::copyFile(sourceFile, destFile))
				LogManager::message("Error copying " + sourceFile + " to " + destFile + ": " + Util::translateError());
		}
	}
}

void Util::moveSettings()
{
	copySettings(paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR, paths[PATH_USER_CONFIG]);
}

void Util::backupSettings()
{
	copySettings(getConfigPath(),
		Util::formatDateTime(getConfigPath() + "Backup" PATH_SEPARATOR_STR "%Y-%m-%d" PATH_SEPARATOR_STR, time(nullptr)));
}

void Util::initAppPaths()
{
#ifdef _WIN32
	paths[PATH_EXE] = Util::getFilePath(Text::fromT(Util::getModuleFileName()));

	TCHAR buf[MAX_PATH];
#define SYS_WIN_PATH_INIT(path) \
	if(::SHGetFolderPath(NULL, CSIDL_##path, NULL, SHGFP_TYPE_CURRENT, buf) == S_OK) \
	{ \
		sysPaths[path] = Text::fromT(buf) + PATH_SEPARATOR; \
	}

	//LogManager::message("Sysytem Path: " + sysPaths[path]);
	//LogManager::message("Error SHGetFolderPath: GetLastError() = " + Util::toString(GetLastError()));

	SYS_WIN_PATH_INIT(WINDOWS);
	SYS_WIN_PATH_INIT(PROGRAM_FILESX86);
	SYS_WIN_PATH_INIT(PROGRAM_FILES);
	if (SysVersion::isWow64())
	{
		// Correct PF path on a 64-bit system running 32-bit version.
		const char* PFW6432 = getenv("ProgramW6432");
		if (PFW6432)
			sysPaths[PROGRAM_FILES] = string(PFW6432) + PATH_SEPARATOR;
	}
	SYS_WIN_PATH_INIT(APPDATA);
	SYS_WIN_PATH_INIT(LOCAL_APPDATA);
	SYS_WIN_PATH_INIT(COMMON_APPDATA);
	SYS_WIN_PATH_INIT(PERSONAL);
#undef SYS_WIN_PATH_INIT

#else
	paths[PATH_EXE] = Util::getFilePath(Util::getModuleFileName());
#endif

	paths[PATH_GLOBAL_CONFIG] = paths[PATH_EXE];
	loadBootConfig();

	if (localMode && paths[PATH_USER_CONFIG].empty())
		paths[PATH_USER_CONFIG] = paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR;

#ifdef USE_APPDATA
#ifdef _WIN32
	if (paths[PATH_USER_CONFIG].empty() &&
	    (File::isExist(paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR "DCPlusPlus.xml") ||
	    !(locatedInSysPath(PROGRAM_FILES, paths[PATH_EXE]) || locatedInSysPath(PROGRAM_FILESX86, paths[PATH_EXE]))))
	{
		// Check if settings directory is writable
		paths[PATH_USER_CONFIG] = paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
		const auto tempFile = paths[PATH_USER_CONFIG] + ".test-writable-" + Util::toString(Util::rand()) + ".tmp";
		try
		{
			File f(tempFile, File::WRITE, File::CREATE | File::TRUNCATE);
			f.close();
			File::deleteFile(tempFile);
			localMode = true;
		}
		catch (const FileException&)
		{
			auto error = GetLastError();
			if (error == ERROR_ACCESS_DENIED)
			{
				initProfileConfig();
				moveSettings();
			}
		}
	}
	else
#endif
	{
		initProfileConfig();
	}
#else // USE_APPDATA
	paths[PATH_USER_CONFIG] = paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
#endif //USE_APPDATA
	paths[PATH_LANGUAGES] = paths[PATH_GLOBAL_CONFIG] + "Lang" PATH_SEPARATOR_STR;
	paths[PATH_THEMES] = paths[PATH_GLOBAL_CONFIG] + "Themes" PATH_SEPARATOR_STR;
	paths[PATH_SOUNDS] = paths[PATH_GLOBAL_CONFIG] + "Sounds" PATH_SEPARATOR_STR;

	if (!File::isAbsolute(paths[PATH_USER_CONFIG]))
	{
		paths[PATH_USER_CONFIG] = paths[PATH_GLOBAL_CONFIG] + paths[PATH_USER_CONFIG];
	}

	paths[PATH_USER_CONFIG] = validateFileName(paths[PATH_USER_CONFIG]);
	paths[PATH_USER_LOCAL] = paths[PATH_USER_CONFIG];

#ifdef _WIN32
	paths[PATH_DOWNLOADS] = localMode ? paths[PATH_USER_CONFIG] + "Downloads" PATH_SEPARATOR_STR : getSystemDownloadsPath(CompatibilityManager::getDefaultPath());
#else
	paths[PATH_DOWNLOADS] = paths[PATH_USER_CONFIG] + "Downloads" PATH_SEPARATOR_STR;
#endif
	paths[PATH_WEB_SERVER] = paths[PATH_EXE] + "WebServer" PATH_SEPARATOR_STR;

	paths[PATH_FILE_LISTS] = paths[PATH_USER_LOCAL] + "FileLists" PATH_SEPARATOR_STR;
	paths[PATH_HUB_LISTS] = paths[PATH_USER_LOCAL] + "HubLists" PATH_SEPARATOR_STR;
	paths[PATH_HTTP_DOWNLOADS] = paths[PATH_USER_LOCAL] + "HttpDownloads" PATH_SEPARATOR_STR;
	paths[PATH_NOTEPAD] = paths[PATH_USER_CONFIG] + "Notepad.txt";
	paths[PATH_EMOPACKS] = paths[PATH_GLOBAL_CONFIG] + "EmoPacks" PATH_SEPARATOR_STR;
	paths[PATH_PLUGINS] = paths[PATH_GLOBAL_CONFIG] + "Plugins" PATH_SEPARATOR_STR COMPILED_ARCH_STRING PATH_SEPARATOR_STR;

	for (int i = 0; i < PATH_LAST; ++i)
		paths[i].shrink_to_fit();

	for (int i = 0; i < SYS_PATH_LAST; ++i)
		sysPaths[i].shrink_to_fit();

	File::ensureDirectory(paths[PATH_USER_CONFIG]);
	File::ensureDirectory(getTempPath());
}

bool Util::isLocalMode()
{
	return localMode;
}

void Util::migrate(const string& file) noexcept
{
	if (localMode)
		return;
	if (File::getSize(file) != -1)
		return;
	string fname = getFileName(file);
	string old = paths[PATH_GLOBAL_CONFIG] + "Settings\\" + fname;
	if (File::getSize(old) == -1)
		return;
	LogManager::message("Util::migrate old = " + old + " new = " + file, false);
	File::renameFile(old, file);
}

void Util::loadBootConfig()
{
	// Load boot settings
	try
	{
		SimpleXML boot;
		boot.fromXML(File(getPath(PATH_GLOBAL_CONFIG) + "dcppboot.xml", File::READ, File::OPEN).read());
		boot.stepIn();

		if (boot.findChild("LocalMode"))
		{
			localMode = boot.getChildData() != "0";
		}
		boot.resetCurrentChild();
		if (boot.findChild("ConfigPath"))
		{
			StringMap params;
#ifdef _WIN32
			string s = getSysPath(APPDATA);
			removePathSeparator(s);
			params["APPDATA"] = std::move(s);

			s = getSysPath(LOCAL_APPDATA);
			removePathSeparator(s);
			params["LOCAL_APPDATA"] = std::move(s);

			s = getSysPath(COMMON_APPDATA);
			removePathSeparator(s);
			params["COMMON_APPDATA"] = std::move(s);

			s = getSysPath(PERSONAL);
			removePathSeparator(s);
			params["PERSONAL"] = std::move(s);

			s = getSysPath(PROGRAM_FILESX86);
			removePathSeparator(s);
			params["PROGRAM_FILESX86"] = std::move(s);

			s = getSysPath(PROGRAM_FILES);
			removePathSeparator(s);
			params["PROGRAM_FILES"] = std::move(s);
#endif
			paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			appendPathSeparator(paths[PATH_USER_CONFIG]);
		}
	}
	catch (const Exception&)
	{
	}
}

#ifdef _WIN32
string Util::getSystemDownloadsPath(const string& def)
{
	typedef HRESULT(WINAPI * _SHGetKnownFolderPath)(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);
	static HMODULE shell32 = nullptr;
	if (!shell32)
	{
		shell32 = ::LoadLibrary(_T("Shell32.dll"));
		if (shell32)
		{
			_SHGetKnownFolderPath getKnownFolderPath = (_SHGetKnownFolderPath)::GetProcAddress(shell32, "SHGetKnownFolderPath");
			if (getKnownFolderPath)
			{
				PWSTR path = nullptr;
				// Defined in KnownFolders.h.
				static const GUID downloads = {0x374de290, 0x123f, 0x4565, {0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b}};
				if (getKnownFolderPath(downloads, 0, NULL, &path) == S_OK)
				{
					const string ret = Text::wideToUtf8(path) + "\\";
					::CoTaskMemFree(path);
					return ret;
				}
			}
			::FreeLibrary(shell32);
		}
	}

	return def + "Downloads\\";
}
#endif

string Util::getTempPath()
{
#ifdef _WIN32
	WCHAR buf[MAX_PATH + 1];
	DWORD size = GetTempPathW(MAX_PATH + 1, buf);
	string tmp;
	return Text::wideToUtf8(buf, static_cast<size_t>(size), tmp);
#else
	return "/tmp/";
#endif
}
