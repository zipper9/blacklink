#include "stdinc.h"
#include "MediaInfoLib.h"
#include "AppPaths.h"
#include "Text.h"

MediaInfoLib MediaInfoLib::instance;

#ifdef _WIN32
#define MEDIA_INFO_LIB _T("MediaInfo.dll")
#else
#define MEDIA_INFO_LIB _T("MediaInfo.so")
#endif

void MediaInfoLib::clearPointers() noexcept
{
#ifdef MEDIA_INFO_UNICODE
	pMediaInfo_New = nullptr;
	pMediaInfo_Delete = nullptr;
	pMediaInfo_Open = nullptr;
	pMediaInfo_Close = nullptr;
	pMediaInfo_Count_Get = nullptr;
	pMediaInfo_Get = nullptr;
	pMediaInfo_Inform = nullptr;
	pMediaInfo_Option = nullptr;
#else
	pMediaInfoA_New = nullptr;
	pMediaInfoA_Delete = nullptr;
	pMediaInfoA_Open = nullptr;
	pMediaInfoA_Close = nullptr;
	pMediaInfoA_Count_Get = nullptr;
	pMediaInfoA_Get = nullptr;
	pMediaInfoA_Inform = nullptr;
	pMediaInfoA_Option = nullptr;
#endif
}

#define RESOLVE(f) p ## f = (fn ## f) resolve(#f); if (!p ## f) return false;

bool MediaInfoLib::resolveSymbols() noexcept
{
#ifdef MEDIA_INFO_UNICODE
	RESOLVE(MediaInfo_New);
	RESOLVE(MediaInfo_Delete);
	RESOLVE(MediaInfo_Open);
	RESOLVE(MediaInfo_Close);
	RESOLVE(MediaInfo_Count_Get);
	RESOLVE(MediaInfo_Get);
	RESOLVE(MediaInfo_Inform);
	RESOLVE(MediaInfo_Option);
#else
	RESOLVE(MediaInfoA_New);
	RESOLVE(MediaInfoA_Delete);
	RESOLVE(MediaInfoA_Open);
	RESOLVE(MediaInfoA_Close);
	RESOLVE(MediaInfoA_Count_Get);
	RESOLVE(MediaInfoA_Get);
	RESOLVE(MediaInfoA_Inform);
	RESOLVE(MediaInfoA_Option);
#endif
	return true;
}

void MediaInfoLib::init()
{
	if (isOpen() || initialized) return;
	initialized = true;
	if (!open(getLibraryPath())) return;
	if (!resolveSymbols())
	{
		close();
		clearPointers();
	}
}

void MediaInfoLib::uninit()
{
	close();
	clearPointers();
	initialized = false;
}

tstring MediaInfoLib::getLibraryPath()
{
	tstring ts = Text::toT(Util::getPluginsPath());
	ts += MEDIA_INFO_LIB;
	return ts;
}

tstring MediaInfoLib::getLibraryVersion()
{
	tstring result;
#ifdef MEDIA_INFO_UNICODE
	if (!pMediaInfo_Option) return result;
	const wchar_t* version = pMediaInfo_Option(nullptr, L"Info_Version", L"");
	if (version) result = version;
#else
	if (!pMediaInfoA_Option) return result;
	const char* version = pMediaInfoA_Option(nullptr, "Info_Version", "");
	if (version) result = Text::toT(version);
#endif
	return result;
}
