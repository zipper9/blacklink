#include "stdafx.h"
#include "ImageLists.h"
#include "MainFrm.h"
#include "ResourceLoader.h"
#include "RegKey.h"
#include "resource.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include "../client/Tag16.h"
#include <shellapi.h>

FileImage g_fileImage;
UserImage g_userImage;
UserStateImage g_userStateImage;
GenderImage g_genderImage;
FlagImage g_flagImage;
TransferTreeImage g_TransferTreeImage;
VideoImage g_videoImage;
HubImage g_hubImage;
FileListImage g_fileListImage;
OtherImage g_otherImage;
FavUserImage g_favUserImage;
EditorImage g_editorImage;
TransfersImage g_transfersImage;
TransferArrowsImage g_transferArrowsImage;
NavigationImage g_navigationImage;
FilterImage g_filterImage;
IconBitmaps g_iconBitmaps;

// It may be useful to show a special virus icon for files like "* dvdrip.exe", "*.jpg.exe", etc
// Disabled for now.
string FileImage::getVirusIconIndex(const string& aFileName, int& p_icon_index)
{
	p_icon_index = 0;
	auto x = Text::toLower(Util::getFileExtWithoutDot(aFileName)); //TODO часто зовем
#if 0
	if (x.compare(0, 3, "exe", 3) == 0)
	{
		// Проверка на двойные расширения
		string xx = Util::getFileName(aFileName);
		xx = Text::toLower(Util::getFileDoubleExtWithoutDot(xx));
		bool is_virus = (aFileName.size() > 13 && stricmp(aFileName.c_str() + aFileName.size() - 11, " dvdrip.exe") == 0);
		if (!xx.empty() || is_virus)
		{
			if (is_virus || CFlyServerConfig::isVirusExt(xx))
			{
				static int g_virus_exe_icon_index = 0;
				if (!g_virus_exe_icon_index)
				{
					images.AddIcon(WinUtil::g_hThermometerIcon);
					imageCount++;
					g_virus_exe_icon_index = imageCount - 1;
				}
				p_icon_index = g_virus_exe_icon_index;
				return x;
			}
			// Проверим медиа-расширение.exe
			const auto i = xx.rfind('.');
			dcassert(i != string::npos);
			if (i != string::npos)
			{
				const auto base_x = xx.substr(0, i);
				if (CFlyServerConfig::isMediainfoExt(base_x))
				{
					static int g_media_virus_exe_icon_index = 0;
					if (!g_media_virus_exe_icon_index)
					{
						images.AddIcon(WinUtil::g_hMedicalIcon);
						imageCount++;
						g_media_virus_exe_icon_index = imageCount - 1;
					}
					p_icon_index = g_media_virus_exe_icon_index; // g_virus_exe_icon_index;
					return x;
				}
			}
		}
	}
#endif
	return x;
}

int FileImage::getIconIndex(const string& fileName)
{
	int iconIndex = 0;
	string x = getVirusIconIndex(fileName, iconIndex);
	if (iconIndex)
		return iconIndex;
	if (useSystemIcons && !x.empty())
	{
		ASSERT_MAIN_THREAD();
		auto j = iconCache.find(x);
		if (j != iconCache.end())
			return j->second;
		tstring ext = Text::toT(x);
		ext.insert(0, _T("."), 1);
		WinUtil::RegKey key;
		if (key.open(HKEY_CLASSES_ROOT, ext.c_str(), KEY_QUERY_VALUE))
		{
			key.close();
			tstring file = _T("x") + ext;
			SHFILEINFO fi = {};
			if (SHGetFileInfo(file.c_str(), FILE_ATTRIBUTE_NORMAL, &fi, sizeof(fi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
			{
				images.AddIcon(fi.hIcon);
				::DestroyIcon(fi.hIcon);
				iconCache[x] = imageCount;
#ifdef DEBUG_IMAGE_LISTS
				LogManager::message("Adding file type '" + x + "' to image list, index = " + Util::toString(imageCount), false);
#endif
				return imageCount++;
			}
		}
		return DIR_FILE;
	}
	return DIR_FILE;
}

void FileImage::init()
{
#ifdef _DEBUG
	dcassert(imageCount == -1);
#endif
	ResourceLoader::LoadImageList(IDR_FOLDERS, images, 16, 16);
	imageCount = DIR_IMAGE_LAST;
	dcassert(images.GetImageCount() == DIR_IMAGE_LAST);
	updateSettings();
}

void FileImage::updateSettings()
{
	const auto ss = SettingsManager::instance.getUiSettings();
	useSystemIcons = ss->getBool(Conf::USE_SYSTEM_ICONS);
}

void UserStateImage::init()
{
	ResourceLoader::LoadImageList(IDR_STATE_USERS, images, 16, 16);
}

void GenderImage::init()
{
	ResourceLoader::LoadImageList(IDR_GENDER_USERS, images, 16, 16);
}

void UserImage::init()
{
	const auto ss = SettingsManager::instance.getUiSettings();
	const string& userListImage = ss->getString(Conf::USERLIST_IMAGE);
	if (userListImage.empty())
		ResourceLoader::LoadImageList(IDR_USERS, images, 16, 16);
	else
		ResourceLoader::LoadImageList(Text::toT(userListImage).c_str(), images, 16, 16);
}

void VideoImage::init()
{
	ResourceLoader::LoadImageList(IDR_MEDIAFILES, images, 16, 16);
}

void TransferTreeImage::init()
{
	if (imageCount == 0)
	{
		imageCount = ResourceLoader::LoadImageList(IDR_TRANSFER_TREE, images, 16, 16);
		dcassert(imageCount);
	}
}

static bool checkPath(string& path)
{
	FileAttributes attr;
	if (File::getAttributes(path, attr) && attr.isDirectory())
	{
		path += PATH_SEPARATOR;
		return true;
	}
	path.clear();
	return false;
}

void FlagImage::init()
{
	string s = Util::getConfigPath() + "Flags";
	if (!checkPath(s))
	{
		s = Util::getDataPath() + "Flags";
		checkPath(s);
	}
	flagsPath = Text::toT(s);

	s = Util::getConfigPath() + "CustomLocations";
	if (!checkPath(s))
	{
		s = Util::getDataPath() + "CustomLocations";
		checkPath(s);
	}
	customLocationsPath = Text::toT(s);
}

bool FlagImage::drawCountry(HDC dc, uint16_t countryCode, const POINT& pt)
{
	if (flagsPath.empty()) return false;
	HBITMAP bmp = nullptr;
	auto i = bitmaps.find(countryCode);
	if (i == bitmaps.end())
	{
		if (countryCode != TAG('z', 'z'))
		{
			tstring imagePath = flagsPath + Text::toT(tagToString(countryCode)) + _T(".bmp");
			bmp = (HBITMAP) ::LoadImage(NULL, imagePath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
			bitmaps.insert(make_pair(countryCode, bmp));
		}
		else
		{
			CImageEx img;
			if (img.LoadFromResource(IDR_BAD_FLAG, _T("PNG")))
				bmp = img.Detach();
		}
	}
	else
		bmp = i->second;
	if (!bmp) return false;
	if (!memDC) memDC = CreateCompatibleDC(dc);
	HGDIOBJ oldBmp = SelectObject(memDC, bmp);
	BitBlt(dc, pt.x, pt.y, FLAG_IMAGE_WIDTH, FLAG_IMAGE_HEIGHT, memDC, 0, 0, SRCCOPY);
	SelectObject(memDC, oldBmp);
	return true;
}

bool FlagImage::drawLocation(HDC dc, int locationImage, const POINT& pt)
{
	if (customLocationsPath.empty()) return false;
	uint32_t index = (uint32_t) locationImage << 16;
	auto i = bitmaps.find(index);
	HBITMAP bmp;
	if (i == bitmaps.end())
	{
		tstring imagePath = customLocationsPath + Util::toStringT(locationImage) + _T(".bmp");
		bmp = (HBITMAP) ::LoadImage(NULL, imagePath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		bitmaps.insert(make_pair(index, bmp));
	}
	else
		bmp = i->second;
	if (!bmp) return false;
	if (!memDC) memDC = CreateCompatibleDC(dc);
	HGDIOBJ oldBmp = SelectObject(memDC, bmp);
	BitBlt(dc, pt.x, pt.y, FLAG_IMAGE_WIDTH, FLAG_IMAGE_HEIGHT, memDC, 0, 0, SRCCOPY);
	SelectObject(memDC, oldBmp);
	return true;
}

FlagImage::~FlagImage()
{
	if (memDC) DeleteDC(memDC);
#ifdef _DEBUG
	for (auto& i : bitmaps)
	{
		HBITMAP bmp = i.second;
		if (bmp) DeleteObject(bmp);
	}
#endif
}

int VideoImage::getMediaVideoIcon(unsigned x_size, unsigned y_size)
{
	if (y_size > x_size)
		std::swap(x_size, y_size);
	if (x_size >= 1920 && y_size >= 1080)
		return 0; //Full HD
	if (x_size >= 1280 && y_size >= 720)
		return 1; //HD
	if (x_size >= 640 && y_size >= 480)
		return 2; //SD
	if (x_size >= 320 && y_size >= 200)
		return 3; //Mobile or VHS
	return -1;
}

void HubImage::init()
{
	ResourceLoader::LoadImageList(IDR_HUB_ICONS, images, 16, 16);
}

void FileListImage::init()
{
	ResourceLoader::LoadImageList(IDR_FILELIST_ICONS, images, 16, 16);
}

void OtherImage::init()
{
	ResourceLoader::LoadImageList(IDR_OTHER_ICONS, images, 16, 16);
}

void FavUserImage::init()
{
	ResourceLoader::LoadImageList(IDR_FAV_USERS_STATES, images, 16, 16);
}

void EditorImage::init()
{
	ResourceLoader::LoadImageList(IDR_EDITOR_ICONS, images, 16, 16);
}

void TransfersImage::init()
{
	ResourceLoader::LoadImageList(IDR_TRANSFERS, images, 16, 16);
}

void TransferArrowsImage::init()
{
	ResourceLoader::LoadImageList(IDR_ARROWS, images, 16, 16);
}

void NavigationImage::init()
{
	ResourceLoader::LoadImageList(IDR_NAVIGATION, images, 16, 16);
}

void FilterImage::init()
{
	ResourceLoader::LoadImageList(IDR_FILTER, images, 16, 16);
}

static HBITMAP createBitmapFromImageList(HIMAGELIST imageList, int iconSize, int index, HDC hDCSource, HDC hDCTarget)
{
	BITMAPINFO bi = { sizeof(BITMAPINFOHEADER) };
	bi.bmiHeader.biWidth = iconSize;
	bi.bmiHeader.biHeight = iconSize;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	HBITMAP hBitmap = ::CreateDIBSection(hDCSource, &bi, DIB_RGB_COLORS, nullptr, nullptr, 0);
	dcassert(hBitmap);

	if (hBitmap)
	{
		HGDIOBJ oldBitmap = SelectObject(hDCTarget, hBitmap);
		IMAGELISTDRAWPARAMS dp = { sizeof(IMAGELISTDRAWPARAMS) };
		dp.himl = imageList;
		dp.i = index;
		dp.hdcDst = hDCTarget;
		dp.fStyle = ILD_TRANSPARENT;
		dp.fState = ILS_ALPHA;
		dp.Frame = 255;
		ImageList_DrawIndirect(&dp);
		SelectObject(hDCTarget, oldBitmap);
	}
	return hBitmap;
}

static HBITMAP createBitmapFromDIB(int iconSize, const BITMAP& info, const RECT& rc)
{
	if (rc.top < 0 || rc.top >= info.bmHeight || rc.bottom - rc.top < iconSize) return nullptr;
	int srcStride = info.bmWidth * 4;
	int destStride = iconSize * 4;
	if (destStride > srcStride) return nullptr;

	uint8_t* imgDest = nullptr;
	BITMAPINFO bi = { sizeof(BITMAPINFOHEADER) };
	bi.bmiHeader.biWidth = iconSize;
	bi.bmiHeader.biHeight = -iconSize;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	HBITMAP hBitmap = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, (void**) &imgDest, nullptr, 0);
	dcassert(hBitmap);

	const uint8_t* pIn = (const uint8_t *) info.bmBits + (info.bmHeight - 1 - rc.top) * srcStride + rc.left * 4;
	uint8_t* pOut = imgDest;
	for (int y = 0; y < iconSize; ++y)
	{
		memcpy(pOut, pIn, destStride);
		pIn -= srcStride;
		pOut += destStride;
	}
	return hBitmap;
}

static HBITMAP createBitmapFromIcon(HICON hIcon, int iconSize, HDC hDCSource, HDC hDCTarget)
{
	BITMAPINFO bi = { sizeof(BITMAPINFOHEADER) };
	bi.bmiHeader.biWidth = iconSize;
	bi.bmiHeader.biHeight = iconSize;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	HBITMAP hBitmap = ::CreateDIBSection(hDCSource, &bi, DIB_RGB_COLORS, nullptr, nullptr, 0);
	dcassert(hBitmap);

	if (hBitmap)
	{
		HGDIOBJ oldBitmap = SelectObject(hDCTarget, hBitmap);
		DrawIconEx(hDCTarget, 0, 0, hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
		SelectObject(hDCTarget, oldBitmap);
	}
	return hBitmap;
}

void IconBitmaps::init(int index, int source, int id)
{
	data[index].source = source;
	data[index].id = id;
}

IconBitmaps::IconBitmaps()
{
	memset(data, 0, sizeof(data));
	init(INTERNET_HUBS,           SOURCE_MAIN,     0);
	init(RECONNECT,               SOURCE_MAIN,     1);
	init(FAVORITES,               SOURCE_MAIN,     3);
	init(FAVORITE_USERS,          SOURCE_MAIN,     4);
	init(RECENT_HUBS,             SOURCE_MAIN,     5);
	init(DOWNLOAD_QUEUE,          SOURCE_MAIN,     6);
	init(FINISHED_DOWNLOADS,      SOURCE_MAIN,     7);
	init(UPLOAD_QUEUE,            SOURCE_MAIN,     8);
	init(FINISHED_UPLOADS,        SOURCE_MAIN,     9);
	init(SEARCH,                  SOURCE_MAIN,     10);
	init(ADL_SEARCH,              SOURCE_MAIN,     11);
	init(SEARCH_SPY,              SOURCE_MAIN,     12);
	init(NETWORK_STATISTICS,      SOURCE_MAIN,     13);
	init(SETTINGS,                SOURCE_MAIN,     15);
	init(NOTEPAD,                 SOURCE_MAIN,     16);
	init(SHUTDOWN,                SOURCE_MAIN,     18);
	init(DOWNLOADS_DIR,           SOURCE_MAIN,     22);
	init(REFRESH_SHARE,           SOURCE_MAIN,     23);
	init(QUICK_CONNECT,           SOURCE_MAIN,     25);
	init(RESTORE_CONN,            SOURCE_MAIN,     27);
	init(CDM_DEBUG,               SOURCE_MAIN,     30);
	init(TTH,                     SOURCE_MAIN,     36);
	init(HASH_PROGRESS,           SOURCE_MAIN,     39);
	init(HELP,                    SOURCE_MAIN,     41);
	init(DHT,                     SOURCE_MAIN,     44);
	init(ABOUT,                   SOURCE_MAIN,     45);
	init(FAVORITE_DIRS,           SOURCE_SETTINGS, 5);
	init(PREVIEW,                 SOURCE_SETTINGS, 6);
	init(PRIORITY,                SOURCE_SETTINGS, 8);
	init(CONTACT_LIST,            SOURCE_SETTINGS, 15);
	init(LOGS,                    SOURCE_SETTINGS, 24);
	init(COMMANDS,                SOURCE_SETTINGS, 25);
	init(LIMIT,                   SOURCE_SETTINGS, 26);
	init(WALL,                    SOURCE_SETTINGS, 32);
	init(WEB_SEARCH,              SOURCE_SETTINGS, 35);
	init(MESSAGES,                SOURCE_SETTINGS, 39);
	init(HUB_ONLINE,              SOURCE_HUB,      0);
	init(HUB_OFFLINE,             SOURCE_HUB,      1);
	init(HUB_MODE_ACTIVE,         SOURCE_HUB,      2);
	init(HUB_MODE_PASSIVE,        SOURCE_HUB,      3);
	init(HUB_MODE_OFFLINE,        SOURCE_HUB,      4);
	init(HUB_SWITCH,              SOURCE_HUB,      5);
	init(FILELIST,                SOURCE_FILELIST, 0);
	init(FILELIST_OFFLINE,        SOURCE_FILELIST, 1);
	init(FILELIST_SEARCH,         SOURCE_FILELIST, 2);
	init(FILELIST_SEARCH_OFFLINE, SOURCE_FILELIST, 3);
	init(USER,                    SOURCE_FAVUSERS, 0);
	init(USER_OFFLINE,            SOURCE_FAVUSERS, 2);
	init(BANNED_USER,             SOURCE_FAVUSERS, 3);
	init(DOWNLOAD,                SOURCE_ARROWS,   0);
	init(FAVORITE,                SOURCE_OTHER,    0);
	init(INFORMATION,             SOURCE_OTHER,    1);
	init(QUESTION,                SOURCE_OTHER,    2);
	init(EXCLAMATION,             SOURCE_OTHER,    3);
	init(WARNING,                 SOURCE_OTHER,    4);
	init(STATUS_SUCCESS,          SOURCE_OTHER,    5);
	init(STATUS_FAILURE,          SOURCE_OTHER,    6);
	init(STATUS_PAUSE,            SOURCE_OTHER,    7);
	init(STATUS_ONLINE,           SOURCE_OTHER,    8);
	init(STATUS_OFFLINE,          SOURCE_OTHER,    9);
	init(MOVE_UP,                 SOURCE_OTHER,    10);
	init(MOVE_DOWN,               SOURCE_OTHER,    11);
	init(ADD,                     SOURCE_OTHER,    12);
	init(REMOVE,                  SOURCE_OTHER,    13);
	init(PROPERTIES,              SOURCE_OTHER,    14);
	init(COPY_TO_CLIPBOARD,       SOURCE_OTHER,    15);
	init(ADD_HUB,                 SOURCE_OTHER,    16);
	init(REMOVE_HUB,              SOURCE_OTHER,    17);
	init(GOTO_HUB,                SOURCE_OTHER,    18);
	init(ADD_USER,                SOURCE_OTHER,    19);
	init(REMOVE_USER,             SOURCE_OTHER,    20);
	init(GOTO_USER,               SOURCE_OTHER,    21);
	init(CHAT_PROHIBIT,           SOURCE_OTHER,    22);
	init(CHAT_ALLOW,              SOURCE_OTHER,    23);
	init(DISCONNECT,              SOURCE_OTHER,    24);
	init(MOVE,                    SOURCE_OTHER,    25);
	init(RENAME,                  SOURCE_OTHER,    26);
	init(SELECTION,               SOURCE_OTHER,    27);
	init(ERASE,                   SOURCE_OTHER,    28);
	init(PAUSE,                   SOURCE_OTHER,    29);
	init(CLEAR,                   SOURCE_OTHER,    30);
	init(HAMMER,                  SOURCE_OTHER,    31);
	init(FINGER,                  SOURCE_OTHER,    32);
	init(KEY,                     SOURCE_OTHER,    33);
	init(GOTO_FILELIST,           SOURCE_OTHER,    35);
	init(FOLDER,                  SOURCE_FILES,    0);
	init(EDITOR_SEND,             SOURCE_EDITOR,   0);
	init(EDITOR_MULTILINE,        SOURCE_EDITOR,   1);
	init(EDITOR_EMOTICON,         SOURCE_EDITOR,   2);
	init(EDITOR_TRANSCODE,        SOURCE_EDITOR,   3);
	init(EDITOR_BOLD,             SOURCE_EDITOR,   4);
	init(EDITOR_ITALIC,           SOURCE_EDITOR,   5);
	init(EDITOR_UNDERLINE,        SOURCE_EDITOR,   6);
	init(EDITOR_STRIKE,           SOURCE_EDITOR,   7);
	init(EDITOR_COLOR,            SOURCE_EDITOR,   8);
	init(EDITOR_LINK,             SOURCE_EDITOR,   9);
	init(EDITOR_FIND,             SOURCE_EDITOR,   10);
	init(FILTER,                  SOURCE_FILTER,   0);
	init(CLEAR_SEARCH,            SOURCE_FILTER,   1);
	init(PM,                      SOURCE_ICON,     IDR_TRAY_AND_TASKBAR_PM);
	init(MAGNET,                  SOURCE_ICON,     IDR_MAGNET);
	init(DCLST,                   SOURCE_ICON,     IDR_DCLST);
	init(PADLOCK_CLOSED,          SOURCE_ICON,     IDR_PADLOCK_CLOSED);
	init(PADLOCK_OPEN,            SOURCE_ICON,     IDR_PADLOCK_OPEN);
}

bool IconBitmaps::loadIcon(int index, int size)
{
	if (data[index].icon[size]) return true;
	int iconSize = (size + 1)*16;
	HIconWrapper wrapper(data[index].id, iconSize, iconSize);
	HICON hIcon = wrapper.detach();
	data[index].icon[size] = hIcon;
	return hIcon != nullptr;
}

HIMAGELIST IconBitmaps::getImageList(MainFrame* mainFrame, int type, int size)
{
	switch (type)
	{
		case SOURCE_MAIN:
			return size == 0 ? mainFrame->getSmallToolbarImages() : mainFrame->getToolbarImages();
		case SOURCE_HUB:
			return g_hubImage.getIconList();
		case SOURCE_FILELIST:
			return g_fileListImage.getIconList();
		case SOURCE_FILES:
			return g_fileImage.getIconList();
		case SOURCE_SETTINGS:
			return mainFrame->getSettingsImages();
		case SOURCE_FAVUSERS:
			return g_favUserImage.getIconList();
		case SOURCE_OTHER:
			return g_otherImage.getIconList();
		case SOURCE_ARROWS:
			return g_transferArrowsImage.getIconList();
		case SOURCE_EDITOR:
			return g_editorImage.getIconList();
		case SOURCE_FILTER:
			return g_filterImage.getIconList();
	}
	return nullptr;
}

HBITMAP IconBitmaps::getBitmap(int index, int size)
{
	dcassert(index >= 0 && index < MAX_BITMAPS);
	dcassert(size == 0 || size == 1);

	if (data[index].bitmap[size]) return data[index].bitmap[size];
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (!mainFrame) return nullptr;
	if (data[index].source == SOURCE_ICON)
	{
		if (!loadIcon(index, size)) return nullptr;
		HDC hdc = GetDC(mainFrame->m_hWnd);
		if (!hdc) return nullptr;
		HDC hdcTemp = CreateCompatibleDC(hdc);
		int iconSize = (size + 1)*16;
		data[index].bitmap[size] = createBitmapFromIcon(data[index].icon[size], iconSize, hdc, hdcTemp);
		ReleaseDC(mainFrame->m_hWnd, hdc);
		DeleteDC(hdcTemp);
	}
	else
	{
		HIMAGELIST imageList = getImageList(mainFrame, data[index].source, size);
		IMAGEINFO info;
		if (!ImageList_GetImageInfo(imageList, data[index].id, &info)) return nullptr;

		BITMAP bi;
		HBITMAP hBitmap;
		if (!GetObject(info.hbmImage, sizeof(bi), &bi)) return nullptr;

		int iconSize = (size + 1) * 16;
		if (bi.bmBitsPixel == 32 && bi.bmPlanes == 1 && bi.bmWidth > 0 && bi.bmHeight > 0)
			hBitmap = createBitmapFromDIB(iconSize, bi, info.rcImage);
		else
		{
			HDC hdc = GetDC(mainFrame->m_hWnd);
			if (!hdc) return nullptr;
			HDC hdcTemp = CreateCompatibleDC(hdc);
			if (!hdcTemp) return nullptr;
			hBitmap = createBitmapFromImageList(imageList, iconSize, data[index].id, hdc, hdcTemp);
			ReleaseDC(mainFrame->m_hWnd, hdc);
			DeleteDC(hdcTemp);
		}
		data[index].bitmap[size] = hBitmap;
	}
	return data[index].bitmap[size];
}

HICON IconBitmaps::getIcon(int index, int size)
{
	dcassert(index >= 0 && index < MAX_BITMAPS);
	dcassert(size == 0 || size == 1);

	if (data[index].icon[size]) return data[index].icon[size];
	MainFrame* mainFrame = MainFrame::getMainFrame();
	if (!mainFrame) return nullptr;
	if (data[index].source == SOURCE_ICON)
	{
		loadIcon(index, size);
	}
	else
	{
		HIMAGELIST imageList = getImageList(mainFrame, data[index].source, size);
		data[index].icon[size] = ImageList_GetIcon(imageList, data[index].id, ILD_NORMAL);
	}
	return data[index].icon[size];
}

#ifdef _DEBUG
IconBitmaps::~IconBitmaps()
{
	for (Image& image : data)
	{
		if (image.bitmap[0]) DeleteObject(image.bitmap[0]);
		if (image.bitmap[1]) DeleteObject(image.bitmap[1]);
	}
}
#endif
