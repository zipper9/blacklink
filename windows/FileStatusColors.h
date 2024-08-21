#ifndef FILE_STATUS_COLORS_H_
#define FILE_STATUS_COLORS_H_

#include "../client/SettingsManager.h"
#include "ConfUI.h"
#include "ColorUtil.h"

struct FileStatusColors
{
	enum
	{
		SHARED,
		DOWNLOADED,
		CANCELED,
		FOUND,
		MAX_COLORS
	};

	COLORREF bgNormal[MAX_COLORS];
	COLORREF fgNormal[MAX_COLORS];
	COLORREF fgInQueue;

	void get()
	{
		auto ss = SettingsManager::instance.getUiSettings();
		bgNormal[SHARED] = ss->getInt(Conf::FILE_SHARED_COLOR);
		bgNormal[DOWNLOADED] = ss->getInt(Conf::FILE_DOWNLOADED_COLOR);
		bgNormal[CANCELED] = ss->getInt(Conf::FILE_CANCELED_COLOR);
		bgNormal[FOUND] = ss->getInt(Conf::FILE_FOUND_COLOR);
		fgInQueue = ss->getInt(Conf::FILE_QUEUED_COLOR);
		for (int i = 0; i < MAX_COLORS; ++i)
			fgNormal[i] = ColorUtil::textFromBackground(bgNormal[i]);
	}

	bool compare(const FileStatusColors& rhs) const
	{
		for (int i = 0; i < MAX_COLORS; ++i)
			if (bgNormal[i] != rhs.bgNormal[i]) return false;
		return fgInQueue == rhs.fgInQueue;
	}
};

struct FileStatusColorsEx : public FileStatusColors
{
	COLORREF bgLighter[MAX_COLORS];
	COLORREF fgLighter[MAX_COLORS];

	void get()
	{
		FileStatusColors::get();
		for (int i = 0; i < MAX_COLORS; ++i)
		{
			bgLighter[i] = ColorUtil::lighter(bgNormal[i]);
			fgLighter[i] = ColorUtil::textFromBackground(bgLighter[i]);
		}
	}
};

#endif
