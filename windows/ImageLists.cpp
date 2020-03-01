#include "stdafx.h"
#include "ImageLists.h"
#include "ResourceLoader.h"
#include "resource.h"
#include "../client/CompatibilityManager.h"
#include <Shellapi.h>

FileImage g_fileImage;
UserImage g_userImage;
UserStateImage g_userStateImage;
TrackerImage g_trackerImage;
GenderImage g_genderImage;
FlagImage g_flagImage;
TransferTreeImage g_TransferTreeImage;
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
VideoImage g_videoImage;
#endif

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
					m_images.AddIcon(WinUtil::g_hThermometerIcon);
					m_imageCount++;
					g_virus_exe_icon_index = m_imageCount - 1;
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
						m_images.AddIcon(WinUtil::g_hMedicalIcon);
						m_imageCount++;
						g_media_virus_exe_icon_index = m_imageCount - 1;
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
//#ifndef _DEBUG
#if 1
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
			m_images.AddIcon(fi.hIcon);
			::DestroyIcon(fi.hIcon);
			m_iconCache[x] = m_imageCount;
			return m_imageCount++;
		}
		else
		{
			return DIR_FILE;
		}
#else
		return DIR_FILE;
#endif
	}
	else
	{
		return DIR_FILE;
	}
}

void FileImage::init()
{
#ifdef _DEBUG
	dcassert(m_imageCount == -1);
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
	ResourceLoader::LoadImageList(IDR_FOLDERS, m_images, 16, 16);
	m_imageCount = DIR_IMAGE_LAST;
	dcassert(m_images.GetImageCount() == DIR_IMAGE_LAST);
}

void UserStateImage::init()
{
	ResourceLoader::LoadImageList(IDR_STATE_USERS, m_images, 16, 16);
}

void TrackerImage::init()
{
	ResourceLoader::LoadImageList(IDR_TRACKER_IMAGES, m_images, 16, 16);
}

void GenderImage::init()
{
	ResourceLoader::LoadImageList(IDR_GENDER_USERS, m_images, 16, 16);
}

void UserImage::init()
{
	if (SETTING(USERLIST_IMAGE).empty())
	{
		ResourceLoader::LoadImageList(IDR_USERS, m_images, 16, 16);
	}
	else
	{
		ResourceLoader::LoadImageList(Text::toT(SETTING(USERLIST_IMAGE)).c_str(), m_images, 16, 16);
	}
}

void VideoImage::init()
{
	ResourceLoader::LoadImageList(IDR_MEDIAFILES, m_images, 16, 16);
}

void TransferTreeImage::init()
{
	if (m_flagImageCount == 0)
	{
		m_flagImageCount = ResourceLoader::LoadImageList(IDR_TRANSFER_TREE, m_images, 16, 16);
		dcassert(m_flagImageCount);
	}
}

void FlagImage::init()
{
	//const int l_count_before = m_images.GetImageCount();
	m_flagImageCount = ResourceLoader::LoadImageList(IDR_FLAGS, m_images, 25, 16);
	dcassert(m_flagImageCount);
	dcassert(m_images.GetImageCount() <= 255); // Чтобы не превысить 8 бит
	// !SMT!-IP
	//m_imageCount = m_images.GetImageCount();
	if (!CompatibilityManager::isWine()) //[+]PPA под линуксом пока падаем http://flylinkdc.blogspot.com/2010/08/customlocationsbmp-wine.html
	{
		CBitmap UserLocations;
		if (UserLocations.m_hBitmap = (HBITMAP)::LoadImage(NULL, Text::toT(Util::getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                                                                       true
#endif
		                                                                   ) + "CustomLocations.bmp").c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE))
			m_images.Add(UserLocations, RGB(77, 17, 77));
	}
}

#ifdef SCALOLAZ_MEDIAVIDEO_ICO
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
#endif
