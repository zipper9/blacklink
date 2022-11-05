#ifndef IMAGE_LISTS_H
#define IMAGE_LISTS_H

#include "../client/typedefs.h"
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
			DIR_DCLST,
			DIR_DVD,
			DIR_BD,
			DIR_TORRENT,
			DIR_IMAGE_LAST
		};

		int getIconIndex(const string& aFileName);
		string getVirusIconIndex(const string& aFileName, int& p_icon_index);

		static bool isBdFolder(const string& nameDir)
		{
			// Check for BDMV folder
			static const string bdmvDir = "BDMV";
			return nameDir == bdmvDir;
		}

		static bool isDvdFolder(const string& nameDir)
		{
			// Check for VIDEO_TS or AUDIO_TS
			static const string audioTsDir = "AUDIO_TS";
			static const string videoTsDir = "VIDEO_TS";
			return nameDir == audioTsDir  || nameDir == videoTsDir;
		}

		static bool isDvdFile(const string& nameFile)
		{
			if (nameFile.length() == 12)
			{
				static const string video_ts = "VIDEO_TS";
				if (nameFile.compare(0, video_ts.size(), video_ts) == 0)
				{
					static const string bup = "BUP";
					static const string ifo = "IFO";
					static const string vob = "VOB";
					
					if (nameFile.compare(9, 3, bup) == 0 || nameFile.compare(9, 3, ifo) == 0 || nameFile.compare(9, 3, vob) == 0) // נאסרטנוםטו פאיכא
					{
						return true;
					}
					
					// Check for names like "VTS_03_1"
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

class GenderImage : public BaseImageList
{
	public:
		void init();
};

class HubImage : public BaseImageList
{
	public:
		void init();
};

class FileListImage : public BaseImageList
{
	public:
		void init();
};

class OtherImage : public BaseImageList
{
	public:
		void init();
};

class FavUserImage : public BaseImageList
{
	public:
		void init();
};

class EditorImage : public BaseImageList
{
	public:
		void init();
};

class TransfersImage : public BaseImageList
{
	public:
		void init();
};

class TransferArrowsImage : public BaseImageList
{
	public:
		void init();
};

extern UserImage g_userImage;
extern UserStateImage g_userStateImage;
extern GenderImage g_genderImage;
extern HubImage g_hubImage;
extern FileListImage g_fileListImage;
extern OtherImage g_otherImage;
extern FavUserImage g_favUserImage;
extern EditorImage g_editorImage;
extern TransfersImage g_transfersImage;
extern TransferArrowsImage g_transferArrowsImage;

class TransferTreeImage : public BaseImageList
{
	public:
		int imageCount;
		TransferTreeImage() : imageCount(0) {}
		void init();
};

extern TransferTreeImage g_TransferTreeImage;

class FlagImage
{
	public:
		static const int FLAG_IMAGE_WIDTH  = 25;
		static const int FLAG_IMAGE_HEIGHT = 16;

		FlagImage() : memDC(NULL) {}
		~FlagImage();
		void init();
		bool drawCountry(HDC dc, uint16_t countryCode, const POINT& pt);
		bool drawLocation(HDC dc, int locationImage, const POINT& pt);

	private:
		boost::unordered_map<uint32_t, HBITMAP> bitmaps;
		tstring customLocationsPath;
		tstring flagsPath;
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
			HELP,
			DHT,
			ABOUT,
			FAVORITE_DIRS,
			PREVIEW,
			PRIORITY,
			CONTACT_LIST,
			LOGS,
			COMMANDS,
			LIMIT,
			WALL,
			MESSAGES,
			HUB_ONLINE,
			HUB_OFFLINE,
			HUB_MODE_ACTIVE,
			HUB_MODE_PASSIVE,
			HUB_MODE_OFFLINE,
			HUB_SWITCH,
			FILELIST,
			FILELIST_OFFLINE,
			FILELIST_SEARCH,
			FILELIST_SEARCH_OFFLINE,
			USER,
			USER_OFFLINE,
			BANNED_USER,
			DOWNLOAD,
			FAVORITE,
			INFORMATION,
			QUESTION,
			EXCLAMATION,
			WARNING,
			STATUS_SUCCESS,
			STATUS_FAILURE,
			STATUS_PAUSE,
			STATUS_ONLINE,
			STATUS_OFFLINE,
			MOVE_UP,
			MOVE_DOWN,
			ADD,
			REMOVE,
			PROPERTIES,
			COPY_TO_CLIPBOARD,
			ADD_HUB,
			REMOVE_HUB,
			GOTO_HUB,
			ADD_USER,
			REMOVE_USER,
			GOTO_USER,
			CHAT_PROHIBIT,
			CHAT_ALLOW,
			DISCONNECT,
			MOVE,
			RENAME,
			SELECTION,
			ERASE,
			PAUSE,
			CLEAR,
			HAMMER,
			FINGER,
			KEY,
			EDITOR_SEND,
			EDITOR_MULTILINE,
			EDITOR_EMOTICON,
			EDITOR_TRANSCODE,
			EDITOR_BOLD,
			EDITOR_ITALIC,
			EDITOR_UNDERLINE,
			EDITOR_STRIKE,
			EDITOR_COLOR,
			PM,
			MAGNET,
			DCLST,
			PADLOCK_CLOSED,
			PADLOCK_OPEN,
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
			SOURCE_HUB,
			SOURCE_FILELIST,
			SOURCE_SETTINGS,
			SOURCE_FAVUSERS,
			SOURCE_OTHER,
			SOURCE_ARROWS,
			SOURCE_EDITOR
		};

		Image data[MAX_BITMAPS];
		void init(int index, int source, int id);
		bool loadIcon(int index, int source);
		static HIMAGELIST getImageList(class MainFrame* mainFrame, int type, int size);

	public:
		IconBitmaps();
#ifdef _DEBUG
		~IconBitmaps();
#endif
		HBITMAP getBitmap(int index, int size);
		HICON getIcon(int index, int size);
};

extern IconBitmaps g_iconBitmaps;

#endif /* IMAGE_LISTS_H */
