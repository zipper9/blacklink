#ifndef MEDIA_INFO_LIB_H_
#define MEDIA_INFO_LIB_H_

#include "DynamicLibrary.h"

#ifdef _WIN32
#define MEDIA_INFO_API __stdcall
#else
#define MEDIA_INFO_API
#endif

#ifdef _UNICODE
#define MEDIA_INFO_UNICODE
#endif

class MediaInfoLib : public DynamicLibrary
{
	public:
		// Stream types
		enum
		{
			Stream_General,
			Stream_Video,
			Stream_Audio,
			Stream_Text,
			Stream_Other,
			Stream_Image,
			Stream_Menu,
			Stream_Max
		};

		// Kinds of info
		enum
		{
			Info_Name,
			Info_Text,
			Info_Measure,
			Info_Options,
			Info_Name_Text,
			Info_Measure_Text,
			Info_Info,
			Info_HowTo,
			Info_Max
		};

#ifdef MEDIA_INFO_UNICODE
		typedef wchar_t MediaInfoChar;

		typedef void* (MEDIA_INFO_API *fnMediaInfo_New)();
		typedef void (MEDIA_INFO_API *fnMediaInfo_Delete)(void* handle);
		typedef size_t (MEDIA_INFO_API *fnMediaInfo_Open)(void* handle, const wchar_t* path);
		typedef void (MEDIA_INFO_API *fnMediaInfo_Close)(void* handle);
		typedef const size_t (MEDIA_INFO_API *fnMediaInfo_Count_Get)(void* handle, int StreamKind, size_t StreamNumber);
		typedef const wchar_t* (MEDIA_INFO_API *fnMediaInfo_Get)(void* handle, int StreamKind, size_t StreamNumber, const wchar_t* Parameter, int InfoKind, int SearchKind);
		typedef const wchar_t* (MEDIA_INFO_API *fnMediaInfo_Inform)(void*, size_t);
		typedef const wchar_t* (MEDIA_INFO_API *fnMediaInfo_Option)(void*, const wchar_t* param, const wchar_t* value);

		fnMediaInfo_New pMediaInfo_New;
		fnMediaInfo_Delete pMediaInfo_Delete;
		fnMediaInfo_Open pMediaInfo_Open;
		fnMediaInfo_Close pMediaInfo_Close;
		fnMediaInfo_Count_Get pMediaInfo_Count_Get;
		fnMediaInfo_Get pMediaInfo_Get;
		fnMediaInfo_Inform pMediaInfo_Inform;
		fnMediaInfo_Option pMediaInfo_Option;
#else
		typedef char MediaInfoChar;

		typedef void* (MEDIA_INFO_API *fnMediaInfoA_New)();
		typedef void (MEDIA_INFO_API *fnMediaInfoA_Delete)(void* handle);
		typedef size_t (MEDIA_INFO_API *fnMediaInfoA_Open)(void* handle, const char* path);
		typedef void (MEDIA_INFO_API *fnMediaInfoA_Close)(void* handle);
		typedef const size_t (MEDIA_INFO_API *fnMediaInfoA_Count_Get)(void* handle, int StreamKind, size_t StreamNumber);
		typedef const char* (MEDIA_INFO_API *fnMediaInfoA_Get)(void* handle, int StreamKind, size_t StreamNumber, const char* Parameter, int InfoKind, int SearchKind);
		typedef const char* (MEDIA_INFO_API *fnMediaInfoA_Inform)(void*, size_t);
		typedef const char* (MEDIA_INFO_API *fnMediaInfoA_Option)(void*, const char* param, const char* value);

		fnMediaInfoA_New pMediaInfoA_New;
		fnMediaInfoA_Delete pMediaInfoA_Delete;
		fnMediaInfoA_Open pMediaInfoA_Open;
		fnMediaInfoA_Close pMediaInfoA_Close;
		fnMediaInfoA_Count_Get pMediaInfoA_Count_Get;
		fnMediaInfoA_Get pMediaInfoA_Get;
		fnMediaInfoA_Inform pMediaInfoA_Inform;
		fnMediaInfoA_Option pMediaInfoA_Option;
#endif

		MediaInfoLib() : initialized(false) { clearPointers(); }
		~MediaInfoLib() { uninit(); }

		void init();
		void uninit();
		static tstring getLibraryPath();
		tstring getLibraryVersion();
		bool isError() const { return !isOpen() && initialized; }

		static MediaInfoLib instance;

	private:
		void clearPointers() noexcept;
		bool resolveSymbols() noexcept;

		bool initialized;
};

#endif // MEDIA_INFO_LIB_H_
