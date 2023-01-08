#ifndef THEME_UTIL_H_
#define THEME_UTIL_H_

#include "../client/typedefs.h"
#include "SettingsStore.h"

namespace Util
{
	enum
	{
		ATTRIB_TYPE_NONE,
		ATTRIB_TYPE_CHAT,
		ATTRIB_TYPE_OTHER
	};

	int getThemeAttribType(int id);
	void importDcTheme(const tstring& file, SettingsStore& ss);
	void exportDcTheme(const tstring& file, const SettingsStore* ss);
	void getDefaultTheme(SettingsStore& ss);
}

#endif // THEME_UTIL_H_
