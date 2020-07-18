#ifndef IMAGE_LISTS_H
#define IMAGE_LISTS_H

#include "../client/typedefs.h"
#include "../client/IPInfo.h"
#include <atlctrls.h>

class BaseImageList
{
	public:
		CImageList& getIconList()
		{
			return m_images;
		}
		void Draw(HDC hDC, int nImage, POINT pt)
		{
			m_images.Draw(hDC, nImage, pt, LVSIL_SMALL);
		}
		void uninit()
		{
			m_images.Destroy();
		}

	protected:
		CImageList m_images;
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
			: m_imageCount(-1)
#endif
		{
		}
		void init();
	
	private:
		int m_imageCount;
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

extern UserImage g_userImage;
extern UserStateImage g_userStateImage;
extern TrackerImage g_trackerImage;
extern GenderImage g_genderImage;

class TransferTreeImage : public BaseImageList
{
	public:
		uint8_t m_flagImageCount;
		TransferTreeImage() : m_flagImageCount(0)
		{
		}
		void init();
};

extern TransferTreeImage g_TransferTreeImage;

class FlagImage : public BaseImageList
{
	public:
		uint8_t m_flagImageCount;
		FlagImage() : m_flagImageCount(0)
		{
		}
		void init();
		using BaseImageList::Draw;
		void DrawCountry(HDC dc, const IPInfo& ipInfo, const POINT& pt)
		{
			Draw(dc, ipInfo.countryImage, pt);
		}
		void DrawLocation(HDC dc, const IPInfo& ipInfo, const POINT& pt)
		{
			Draw(dc, ipInfo.locationImage + m_flagImageCount, pt);
		}
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
			BITMAP_RECONNECT,
			BITMAP_SEARCH,
			BITMAP_PM,
			BITMAP_FILELIST,
			BITMAP_DOWNLOAD,
			BITMAP_UPLOAD,
			BITMAP_PRIORITY,
			BITMAP_LIMIT,
			BITMAP_PREVIEW,
			BITMAP_COMMANDS,
			BITMAP_CONTACT_LIST,
			MAX_BITMAPS
		};

		HBITMAP bitmaps[MAX_BITMAPS];

		void init(HDC hdc, HIMAGELIST toolbarImages, HIMAGELIST settingsImages);
#ifdef _DEBUG
		~IconBitmaps();
#endif
};

extern IconBitmaps g_iconBitmaps;

#endif /* IMAGE_LISTS_H */
