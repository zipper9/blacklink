#ifndef IMAGE_LISTS_H
#define IMAGE_LISTS_H

#include "../client/typedefs.h"
#include "../client/IPInfo.h"
#include <boost/unordered_map.hpp>
#include <atlctrls.h>

class BaseImageList
{
	public:
		CImageList& getIconList()
		{
			return images;
		}
		void Draw(HDC hDC, int nImage, POINT pt)
		{
			images.Draw(hDC, nImage, pt, LVSIL_SMALL);
		}
		void uninit()
		{
			images.Destroy();
		}

	protected:
		CImageList images;
};

struct FileImage : public BaseImageList
{
	public:
		enum TypeDirectoryImages
		{
			DIR_ICON,
			DIR_MASKED,
			DIR_FILE,
			DIR_DSLCT,
			DIR_DVD,
			DIR_BD,
			DIR_TORRENT,
			DIR_IMAGE_LAST
		};
		
		int getIconIndex(const string& aFileName);
		string getVirusIconIndex(const string& aFileName, int& p_icon_index);
		
		static bool isBdFolder(const string& nameDir)
		{
			// Папка содержащая подпапки VIDEO_TS или AUDIO_TS, является DVD папкой
			static const string g_bdmvDir = "BDMV";
			return nameDir == g_bdmvDir;
		}
		
		static bool isDvdFolder(const string& nameDir)
		{
			// Папка содержащая подпапки VIDEO_TS или AUDIO_TS, является DVD папкой
			static const string g_audioTsDir = "AUDIO_TS";
			static const string g_videoTsDir = "VIDEO_TS";
			return nameDir == g_audioTsDir  || nameDir == g_videoTsDir;
		}
		
		// Метод по названию файла определяет, является ли файл частью dvd
		static bool isDvdFile(const string& nameFile)
		{
			// Имена файлов dvd удовлетворяют правилу (8 символов – название файла, 3 – его расширение)
			if (nameFile.length() == 12)
			{
				static const string video_ts = "VIDEO_TS";
				if (nameFile.compare(0, video_ts.size(), video_ts) == 0) // имя файла
				{
					static const string bup = "BUP";
					static const string ifo = "IFO";
					static const string vob = "VOB";
					
					if (nameFile.compare(9, 3, bup) == 0 || nameFile.compare(9, 3, ifo) == 0 || nameFile.compare(9, 3, vob) == 0) // расширение файла
					{
						return true;
					}
					
					// Разбираем имя вида "VTS_03_1"
					static const string vts = "VTS";
					if (nameFile.compare(0, 3, vts) == 0 && isdigit(nameFile[4]) && isdigit(nameFile[5]) && isdigit(nameFile[7]))
					{
						return true;
					}
				}
			}
			
			return false;
		}
		
		FileImage()
#ifdef _DEBUG
			: imageCount(-1)
#endif
		{
		}
		void init();
	
	private:
		int imageCount;
		boost::unordered_map<string, int> m_iconCache;
};

extern FileImage g_fileImage;

class UserImage : public BaseImageList
{
	public:
		void init();
		void reinit() // User Icon Begin / User Icon End
		{
			uninit();
			init();
		}
};

class UserStateImage : public BaseImageList
{
	public:
		void init();
};

class TrackerImage : public BaseImageList
{
	public:
		void init();
};

class GenderImage : public BaseImageList
{
	public:
		void init();
};

class FavImage : public BaseImageList
{
	public:
		void init();
};

extern UserImage g_userImage;
extern UserStateImage g_userStateImage;
extern TrackerImage g_trackerImage;
extern GenderImage g_genderImage;
extern FavImage g_favImage;

class TransferTreeImage : public BaseImageList
{
	public:
		int imageCount;
		TransferTreeImage() : imageCount(0) {}
		void init();
};

extern TransferTreeImage g_TransferTreeImage;

class FlagImage : public BaseImageList
{
	public:
		FlagImage() : memDC(NULL) {}
		~FlagImage();
		void init();
		void drawCountry(HDC dc, const IPInfo& ipInfo, const POINT& pt)
		{
			Draw(dc, ipInfo.countryImage, pt);
		}
		bool drawLocation(HDC dc, const IPInfo& ipInfo, const POINT& pt);

	private:
		boost::unordered_map<int, HBITMAP> bitmaps;
		tstring path;
		HDC memDC;
};

extern FlagImage g_flagImage;

class VideoImage : public BaseImageList
{
	public:
		void init();
		static int getMediaVideoIcon(unsigned x_size, unsigned y_size);
};

extern VideoImage g_videoImage;

class IconBitmaps
{
	public:
		enum
		{
			INTERNET_HUBS,
			RECONNECT,
			FAVORITES,
			FAVORITE_USERS,
			RECENT_HUBS,
			DOWNLOAD_QUEUE,
			FINISHED_DOWNLOADS,
			UPLOAD_QUEUE,
			FINISHED_UPLOADS,
			SEARCH,
			ADL_SEARCH,
			SEARCH_SPY,
			NETWORK_STATISTICS,
			FILELIST,
			SETTINGS,
			NOTEPAD,
			SHUTDOWN,
			INTERNET,
			DOWNLOADS_DIR,
			REFRESH_SHARE,
			QUICK_CONNECT,
			RESTORE_CONN,
			CDM_DEBUG,
			TTH,
			HASH_PROGRESS,
			FAVORITE_DIRS,
			PREVIEW,
			PRIORITY,
			CONTACT_LIST,
			LOGS,
			COMMANDS,
			LIMIT,
			BANNED_USER,
			DCLST,
			MESSAGES,
			FAVORITE,
			PM,
			USER,
			HUB_ONLINE,
			HUB_OFFLINE,
			FILELIST_OFFLINE,
			MAGNET,
			CLEAR,
			MAX_BITMAPS
		};

	private:
		struct Image
		{
			HBITMAP bitmap[2]; // 0 - small, 1 - large
			HICON icon[2];
			int source;
			int id;
		};

		enum
		{
			SOURCE_ICON,
			SOURCE_MAIN,
			SOURCE_SETTINGS,
			SOURCE_FAVORITE
		};

		Image data[MAX_BITMAPS];
		void init(int index, int source, int id);
		bool loadIcon(int index, int source);

	public:
		//void init(HDC hdc, HIMAGELIST toolbarImages, HIMAGELIST settingsImages);
		IconBitmaps();
#ifdef _DEBUG
		~IconBitmaps();
#endif
		HBITMAP getBitmap(int index, int size);
		HICON getIcon(int index, int size);
};

extern IconBitmaps g_iconBitmaps;

#endif /* IMAGE_LISTS_H */
