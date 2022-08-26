#include "stdafx.h"
#include "ThemeUtil.h"
#include "../client/SettingsManager.h"
#include "../client/SimpleXML.h"
#include "../client/LogManager.h"

#define RANGE(first, last) { SettingsManager::first, SettingsManager::last }

static const struct
{
	int idFirst;
	int idLast;
} themeAttrib[] =
{
	RANGE(TEXT_FONT, TEXT_FONT),
	RANGE(BACKGROUND_COLOR, ERROR_COLOR),
	RANGE(DOWNLOAD_BAR_COLOR, FILE_QUEUED_COLOR),
	RANGE(TEXT_GENERAL_BACK_COLOR, TEXT_ENEMY_ITALIC),
	RANGE(RESERVED_SLOT_COLOR, BAD_FILELIST_COLOR),
	RANGE(PROGRESS_TEXT_COLOR_DOWN, PROGRESSBAR_ODC_BUMPED),
	RANGE(MENUBAR_TWO_COLORS, MENUBAR_BUMPED),
	RANGE(BOLD_MSG_AUTHOR, BOLD_MSG_AUTHOR)
};

#undef RANGE

static bool isThemeAttribute(int id)
{
	for (int i = 0; i < _countof(themeAttrib); ++i)
		if (id >= themeAttrib[i].idFirst && id <= themeAttrib[i].idLast) return true;
	return false;
}

void Util::importDcTheme(const tstring& file, SettingsStore& ss)
{
#ifdef _DEBUG
	LogManager::message("Loading theme: " + Text::fromT(file));
#endif
	SimpleXML xml;
	xml.fromXML(File(file, File::READ, File::OPEN).read());
	xml.stepIn();
	if (xml.findChild(("Settings")))
	{
		xml.stepIn();
		while (xml.getNextChild())
		{
			const string& name = xml.getChildTag();
			int id = SettingsManager::getIdByName(name);
			if (id >= 0 && isThemeAttribute(id))
			{
				if (SettingsManager::isIntSetting(id))
					ss.setIntValue(id, Util::toInt(xml.getChildData()));
				else
					ss.setStrValue(id, xml.getChildData());
			}
			else
				LogManager::message("Unknown theme attribute: " + name, false);
		}
	}
}

void Util::exportDcTheme(const tstring& file, const SettingsStore* ss)
{
	static const string type("type");
	static const string stringType("string");
	static const string intType("int");

	SimpleXML xml;
	xml.addTag("DCPlusPlus");
	xml.stepIn();
	xml.addTag("Settings");
	xml.stepIn();

	for (int i = 0; i < _countof(themeAttrib); ++i)
	{
		int idFirst = themeAttrib[i].idFirst;
		int idLast = themeAttrib[i].idLast;
		for (int id = idFirst; id <= idLast; ++id)
		{
			string tag = SettingsManager::getNameById(id);
			if (SettingsManager::isIntSetting(id))
			{
				int value;
				if (!(ss && ss->getIntValue(id, value)))
					value = SettingsManager::get((SettingsManager::IntSetting) id);
				xml.addTag(tag, value);
				xml.addChildAttrib(type, intType);
			}
			else
			{
				string value;
				if (!(ss && ss->getStrValue(id, value)))
					value = SettingsManager::get((SettingsManager::StrSetting) id);
				xml.addTag(tag, value);
				xml.addChildAttrib(type, stringType);
			}
		}
	}

	File f(file, File::WRITE, File::CREATE | File::TRUNCATE);
	BufferedOutputStream<false> os(&f, 32 * 1024);
	os.write(SimpleXML::utf8Header);
	xml.toXML(&os);
	os.flushBuffers(true);
}

void Util::getDefaultTheme(SettingsStore& ss)
{
	for (int i = 0; i < _countof(themeAttrib); ++i)
	{
		int idFirst = themeAttrib[i].idFirst;
		int idLast = themeAttrib[i].idLast;
		for (int id = idFirst; id <= idLast; ++id)
		{
			if (SettingsManager::isIntSetting(id))
				ss.setIntValue(id, SettingsManager::getDefault((SettingsManager::IntSetting) id));
			else
				ss.setStrValue(id, SettingsManager::getDefault((SettingsManager::StrSetting) id));
		}
	}
}
