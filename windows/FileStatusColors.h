#ifndef FILE_STATUS_COLORS_H_
#define FILE_STATUS_COLORS_H_

#include "../client/SettingsManager.h"

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
		bgNormal[SHARED] = SETTING(FILE_SHARED_COLOR);
		bgNormal[DOWNLOADED] = SETTING(FILE_DOWNLOADED_COLOR);
		bgNormal[CANCELED] = SETTING(FILE_CANCELED_COLOR);
		bgNormal[FOUND] = SETTING(FILE_FOUND_COLOR);
		fgInQueue = SETTING(FILE_QUEUED_COLOR);
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
