#include "stdafx.h"
#include "ImageLists.h"
#include "MainFrm.h"
#include "ResourceLoader.h"
#include "resource.h"
#include "../client/CompatibilityManager.h"
#include "../client/Tag16.h"
#include <shellapi.h>

FileImage g_fileImage;
UserImage g_userImage;
UserStateImage g_userStateImage;
GenderImage g_genderImage;
FlagImage g_flagImage;
TransferTreeImage g_TransferTreeImage;
VideoImage g_videoImage;
FavImage g_favImage;
FavUserImage g_favUserImage;
EditorImage g_editorImage;
TransfersImage g_transfersImage;
TransferArrowsImage g_transferArrowsImage;
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

int FileImage::getIconIndex(const string& aFileName)
{
	int iconIndex = 0;
	string x = getVirusIconIndex(aFileName, iconIndex);
	if (iconIndex)
		return iconIndex;
	if (BOOLSETTING(USE_SYSTEM_ICONS))
	{
		if (!x.empty())
		{
			const auto j = m_iconCache.find(x);
			if (j != m_iconCache.end())
				return j->second;
		}
		x = string("x.") + x;
		SHFILEINFO fi = { 0 };
		if (SHGetFileInfo(Text::toT(x).c_str(), FILE_ATTRIBUTE_NORMAL, &fi, sizeof(fi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
		{
			images.AddIcon(fi.hIcon);
			::DestroyIcon(fi.hIcon);
			m_iconCache[x] = imageCount;
			return imageCount++;
		}
		else
		{
			return DIR_FILE;
		}
	}
	else
	{
		return DIR_FILE;
	}
}

void FileImage::init()
{
#ifdef _DEBUG
	dcassert(imageCount == -1);
#endif
	/** @todo fix this so that the system icon is used for dirs as well (we need
	to mask it so that incomplete folders appear correct */
#if 0
	if (BOOLSETTING(USE_SYSTEM_ICONS))
	{
		SHFILEINFO fi = {0};
		g_fileImages.Create(16, 16, ILC_COLOR32 | ILC_MASK, 16, 16);
		::SHGetFileInfo(_T("."), FILE_ATTRIBUTE_DIRECTORY, &fi, sizeof(fi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
		g_fileImages.AddIcon(fi.hIcon);
		g_fileImages.AddIcon(ic);
		::DestroyIcon(fi.hIcon);
	}
	else
	{
		ResourceLoader::LoadImageList(IDR_FOLDERS, fileImages, 16, 16);
	}
#endif
	ResourceLoader::LoadImageList(IDR_FOLDERS, images, 16, 16);
	imageCount = DIR_IMAGE_LAST;
	dcassert(images.GetImageCount() == DIR_IMAGE_LAST);
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
	if (SETTING(USERLIST_IMAGE).empty())
	{
		ResourceLoader::LoadImageList(IDR_USERS, images, 16, 16);
	}
	else
	{
		ResourceLoader::LoadImageList(Text::toT(SETTING(USERLIST_IMAGE)).c_str(), images, 16, 16);
	}
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
			ExCImage img;
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
	if (x_size >= 200 && x_size <= 640 && y_size >= 140 && y_size <= 480)
		return 3; //Mobile or VHS
	if (x_size >= 640 && x_size <= 1080 && y_size >= 240 && y_size <= 720)
		return 2; //SD
	if (x_size >= 1080 && x_size <= 1920 && y_size >= 500 && y_size <= 1080)
		return 1; //HD
	if (x_size >= 1920 && y_size >= 1080)
		return 0; //Full HD
	return -1;
}

void FavImage::init()
{
	ResourceLoader::LoadImageList(IDR_FAVORITE, images, 16, 16);
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

static HBITMAP createBitmapFromImageList(HIMAGELIST imageList, int iconSize, int index, HDC hDCSource, HDC hDCTarget)
{
	BITMAPINFO bi = { sizeof(BITMAPINFOHEADER) };
	bi.bmiHeader.biWidth = iconSize;
	bi.bmiHeader.biHeight = iconSize;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	HBITMAP hBitmap = ::CreateDIBSection(hDCSource, &bi, DIB_RGB_COLORS, nullptr, nullptr, 0);
	ATLASSERT(hBitmap);

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

static HBITMAP createBitmapFromIcon(HICON hIcon, int iconSize, HDC hDCSource, HDC hDCTarget)
{
	BITMAPINFO bi = { sizeof(BITMAPINFOHEADER) };
	bi.bmiHeader.biWidth = iconSize;
	bi.bmiHeader.biHeight = iconSize;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	HBITMAP hBitmap = ::CreateDIBSection(hDCSource, &bi, DIB_RGB_COLORS, nullptr, nullptr, 0);
	ATLASSERT(hBitmap);

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
	init(INTERNET_HUBS,      SOURCE_MAIN,     0);
	init(RECONNECT,          SOURCE_MAIN,     1);
	init(FAVORITES,          SOURCE_MAIN,     3);
	init(FAVORITE_USERS,     SOURCE_MAIN,     4);
	init(RECENT_HUBS,        SOURCE_MAIN,     5);
	init(DOWNLOAD_QUEUE,     SOURCE_MAIN,     6);
	init(FINISHED_DOWNLOADS, SOURCE_MAIN,     7);
	init(UPLOAD_QUEUE,       SOURCE_MAIN,     8);
	init(FINISHED_UPLOADS,   SOURCE_MAIN,     9);
	init(SEARCH,             SOURCE_MAIN,     10);
	init(ADL_SEARCH,         SOURCE_MAIN,     11);
	init(SEARCH_SPY,         SOURCE_MAIN,     12);
	init(NETWORK_STATISTICS, SOURCE_MAIN,     13);
	init(FILELIST,           SOURCE_MAIN,     14);
	init(SETTINGS,           SOURCE_MAIN,     15);
	init(NOTEPAD,            SOURCE_MAIN,     16);
	init(SHUTDOWN,           SOURCE_MAIN,     18);
	init(INTERNET,           SOURCE_MAIN,     20);
	init(DOWNLOADS_DIR,      SOURCE_MAIN,     22);
	init(REFRESH_SHARE,      SOURCE_MAIN,     23);
	init(QUICK_CONNECT,      SOURCE_MAIN,     25);
	init(RESTORE_CONN,       SOURCE_MAIN,     27);
	init(CDM_DEBUG,          SOURCE_MAIN,     30);
	init(TTH,                SOURCE_MAIN,     36);
	init(HASH_PROGRESS,      SOURCE_MAIN,     39);
	init(HELP,               SOURCE_MAIN,     41);
	init(DHT,                SOURCE_MAIN,     44);
	init(ABOUT,              SOURCE_MAIN,     45);
	init(FAVORITE_DIRS,      SOURCE_SETTINGS, 5);
	init(PREVIEW,            SOURCE_SETTINGS, 6);
	init(PRIORITY,           SOURCE_SETTINGS, 8);
	init(CONTACT_LIST,       SOURCE_SETTINGS, 15);
	init(LOGS,               SOURCE_SETTINGS, 24);
	init(COMMANDS,           SOURCE_SETTINGS, 25);
	init(LIMIT,              SOURCE_SETTINGS, 26);
	init(BANNED_USER,        SOURCE_SETTINGS, 27);
	init(DCLST,              SOURCE_SETTINGS, 36);
	init(MESSAGES,           SOURCE_SETTINGS, 39);
	init(FAVORITE,           SOURCE_FAVORITE, 0);
	init(DOWNLOAD,           SOURCE_ARROWS,   0);
	init(EDITOR_SEND,        SOURCE_EDITOR,   0);
	init(EDITOR_MULTILINE,   SOURCE_EDITOR,   1);
	init(EDITOR_EMOTICON,    SOURCE_EDITOR,   2);
	init(EDITOR_TRANSCODE,   SOURCE_EDITOR,   3);
	init(EDITOR_BOLD,        SOURCE_EDITOR,   4);
	init(EDITOR_ITALIC,      SOURCE_EDITOR,   5);
	init(EDITOR_UNDERLINE,   SOURCE_EDITOR,   6);
	init(EDITOR_STRIKE,      SOURCE_EDITOR,   7);
	init(EDITOR_COLOR,       SOURCE_EDITOR,   8);
	init(PM,                 SOURCE_ICON,     IDR_TRAY_AND_TASKBAR_PM);
	init(USER,               SOURCE_ICON,     IDR_PRIVATE);
	init(HUB_ONLINE,         SOURCE_ICON,     IDR_HUB);
	init(HUB_OFFLINE,        SOURCE_ICON,     IDR_HUB_OFF);
	init(FILELIST_OFFLINE,   SOURCE_ICON,     IDR_FILE_LIST_OFFLINE);
	init(MAGNET,             SOURCE_ICON,     IDR_MAGNET);
	init(CLEAR,              SOURCE_ICON,     IDR_PURGE);
	init(PAUSE,              SOURCE_ICON,     IDR_PAUSE);
	init(EXCLAMATION,        SOURCE_ICON,     IDR_ERROR);
	init(PADLOCK_CLOSED,     SOURCE_ICON,     IDR_PADLOCK_CLOSED);
	init(PADLOCK_OPEN,       SOURCE_ICON,     IDR_PADLOCK_OPEN);
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
		case SOURCE_SETTINGS:
			return mainFrame->getSettingsImages();
		case SOURCE_FAVORITE:
			return g_favImage.getIconList();
		case SOURCE_ARROWS:
			return g_transferArrowsImage.getIconList();
		case SOURCE_EDITOR:
			return g_editorImage.getIconList();
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
		HDC hdc = GetDC(mainFrame->m_hWnd);
		if (!hdc) return nullptr;
		HDC hdcTemp = CreateCompatibleDC(hdc);
		int iconSize = (size + 1)*16;
		data[index].bitmap[size] = createBitmapFromImageList(imageList, iconSize, data[index].id, hdc, hdcTemp);
		ReleaseDC(mainFrame->m_hWnd, hdc);
		DeleteDC(hdcTemp);
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
